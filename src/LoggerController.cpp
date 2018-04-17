/*
 * LoggerController.cpp
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */

#include "LoggerController.h"
#include "HTTPClient.h"
#include "system_defines.h"
#include "HardwareManager.h"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdlib.h> // for abs
#include <ctime>

using namespace std;

LoggerController::LoggerController(string nodeName, string ifFilename, string agcFilename, string config_filename,
								   unsigned long numUploadBytes, unsigned long numPreBytes,
								   unsigned long numPostBytes,
								   unsigned long uploadRefactorySec,
								   string http_host, int http_port,
								   string mqtt_host, int mqtt_port, bool counterOn, bool resampling,
								   int agc_threshold, int agc_updateHz, bool splitWrite,
								   unsigned int resamp_threshold) : m_hwMgr(ifFilename, config_filename, counterOn, resampling, splitWrite, resamp_threshold)
{
	m_nodeName = nodeName;
	m_ifFilename = ifFilename;
	m_agcFilename = agcFilename;

	m_mqtt_port = mqtt_port;
	m_http_port = http_port;

	m_http_host = http_host;
	m_mqtt_host = "tcp://" + mqtt_host + ":" + to_string(mqtt_port);

	m_numPreBytes = (numPreBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;
	m_numPostBytes = (numPostBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;
	m_ephSize = 120000000;
	//m_uploading = false;
	m_stopCond = false;

	m_uploadRefactory = uploadRefactorySec;
	m_numUpdateBytes = (numUploadBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;

	m_numEvents = 0;
	m_numUpdates = 0;
	m_bytesWrittenAGC = 0;
	m_agcUpdateRate = 1.0 / (double)agc_updateHz;
	m_agcThreshold = agc_threshold;
	m_profilePoint = chrono::steady_clock::now();
}

LoggerController::~LoggerController()
{
}

//-----------------------------------------------------------------------
// Start
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies:
// returns: True on success
//-----------------------------------------------------------------------
bool LoggerController::Start()
{
	// Init hardware.
	if (!m_hwMgr.InitializeHardware())
	{
		cout << "Failed to initialize hardware." << endl;
		return false;
	}

	// Set the mqtt hw handle
	HTTPClient::ms_hwHandle = &m_hwMgr;
	UploadTask::ms_hwHandle = &m_hwMgr;

	// Start the MQTT subsystem.
	if (!m_mqttCtlr.Start(m_mqtt_host.c_str(), m_nodeName.c_str(), "mznt", "rj39SZSz", this))
	{
		cerr << "Failed to start MQTT controller" << endl;
	}
	else
	{
		cout << "MQTT controller started." << endl;
	}

	if (!m_mqttCtlr.Subscribe("alert"))
	{
		cerr << "Failed to subscribe to alert topic" << endl;
	}

	string info_msg = m_nodeName + "-ONLINE";

	// Tell the leader we are online.
	if (!m_mqttCtlr.Publish("leader", info_msg.c_str(), 2))
	{
		cerr << "Failed to publish message." << endl;
	}

	// Initialize HTTP upload.
	if (m_httpClient.isInitialized() == false)
	{
		cerr << "Failed to initialize curl." << endl;
	}

	// Open AGC log file and overwrite what is there.
	// Each update we make to AGC log file will open/close the file.
	m_agcLogFile.open(m_agcFilename.c_str(), ios::out | ios::binary);
	m_agcLogFile.close();

	if (!m_hwMgr.Start())
	{
		cerr << "HW init failed." << endl;
	}

	// Get the current time and sync time.
	auto currentTime = m_hwMgr.GetTimeStart();
	time_t currTT = chrono::system_clock::to_time_t(currentTime);

	// Display the current time.
	chrono::milliseconds ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	cout << "Current time: " << ctime(&currTT) << endl;

	// Initialize the agc data.
	AGCLogData initialData[4];
	m_hwMgr.GetAGCData(initialData);
	for (int i = 0; i < 4; i++)
	{
		m_agcLast[i] = initialData[i].ifAGC;
	}

	return true;
}

//-----------------------------------------------------------------------
// Start
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies:
// returns: True on success
//-----------------------------------------------------------------------
bool LoggerController::Stop()
{
	cout << "Called stop." << endl;

	m_hwMgr.Stop();

	cout << "HW Mgr exiting." << endl;

	return true;
}

//-----------------------------------------------------------------------
// Update
// Requires: Performs regular updates of logger controller.
//
// Modifies:
// returns: True on success
//-----------------------------------------------------------------------
bool LoggerController::Update(double elapsed)
{
	// Check time.
	static double lastTime = 0.0;
	static int numLastErrors = 0;
	static AGCTransmit agcTransmit;

	// Check for errors.
	if (m_hwMgr.GetNumErrors() > numLastErrors)
	{
		// Publish the error.
		m_mqttCtlr.Publish("errors", m_nodeName + " had a HW error.", 0);
		// Reset errors.
		//m_hwMgr.ResetNumErrors();
		numLastErrors = m_hwMgr.GetNumErrors();
	}

	if ((elapsed - lastTime) >= m_agcUpdateRate)
	{
		lastTime = elapsed;

		AGCLogData data[4];
		m_agcLogFile.open(m_agcFilename.c_str(), ios::out | ios::binary | ios::app);
		if (!m_agcLogFile.is_open())
		{
			cerr << "Failed to open AGC logfile." << endl;
			return false;
		}

		m_hwMgr.GetAGCData(data);
		m_agcLogFile.write(reinterpret_cast<const char *>(data), streamsize(sizeof(AGCLogData) * 4));
		m_agcLogFile.close();

		// Update the agc data and send MQTT if necessary
		for (int i = 0; i < 4; i++)
		{
			// Check for an event.
			if (abs(m_agcLast[i] - data[i].ifAGC) > m_agcThreshold)
			{
				auto eventTime = chrono::system_clock::now();
				time_t eventTT = chrono::system_clock::to_time_t(eventTime);
				cout << "Time: " << std::ctime(&eventTT) << endl;
				if (m_agcLast[i] < data[i].ifAGC)
				{
					cout << "AGC on channel " << i << " went HIGH" << endl;
					m_mqttCtlr.Publish("leader", "HIGH-" + std::to_string(i), 0);
				}
				else
				{
					cout << "AGC on channel " << i << " went LOW" << endl;
					m_mqttCtlr.Publish("leader", "LOW-" + std::to_string(i), 0);
				}
			}

			agcTransmit.ifAGC[i] = data[i].ifAGC;
			m_agcLast[i] = data[i].ifAGC;
		}

		// Transmit AGC over MQTT.
		// DataRate: 64+32bytes*5bytes/sec
		agcTransmit.ms_truncated = data[0].ms_truncated;

		//cout << "Updated AGC logfile."<<endl;
		m_bytesWrittenAGC += sizeof(AGCLogData) * 4;

		// Publish.
		unsigned char b_data[sizeof(AGCTransmit)];
		string topic = m_nodeName + "-agc";
		//cout << "Size of AGCTransmit: " << dec << sizeof(AGCTransmit) << endl;
		memcpy(b_data, (unsigned char *)&agcTransmit, sizeof(AGCTransmit));
		m_mqttCtlr.Publish(topic, b_data, sizeof(AGCTransmit), 0);
	}

	return true;
}

//-----------------------------------------------------------------------
// Start
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies:
// returns: True on success
//-----------------------------------------------------------------------
int LoggerController::MQTT_Callback(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
	string msgPayload((char *)message->payload, message->payloadlen);
	cout << "Msg Recv: " << msgPayload << endl;
	vector<string> payloadTokens;
	stringstream tokenStream(msgPayload);
	string token;

	// Tokenize the incoming payload
	while (getline(tokenStream, token, '-'))
	{
		payloadTokens.push_back(token);
	}

	if (payloadTokens[0] == "event")
	{
		if (payloadTokens[1] == "start")
		{
			unsigned short channel = stoi(payloadTokens[2]);
			unsigned int eventNum = stoi(payloadTokens[3]);
			cout << "Event alert recv ch " << channel << " enum " << eventNum << endl;

			// target filename.
			string remoteFilename = m_http_host + "/" + m_nodeName + "/" + "event" + payloadTokens[3];
			string ifExt = remoteFilename + ".IF.CH_" + payloadTokens[2];
			string agcExt = remoteFilename + ".AGC";

			// Calculate offset and total size. All positions should be multiples of CHANBUF_SIZE
			string localIF1 = m_hwMgr.GetIFFilename() + ".CH_" + payloadTokens[2];
			string localIF2;
			// We need 8sec ~42MB of the "before data".
			unsigned long currFilePos = m_hwMgr.GetCurrFileOffset(); // This is the current offset in each file.
			cout << "Curr file offset: " << dec << currFilePos << endl;
			unsigned long offset1 = 0;
			unsigned long offset2 = 0;
			unsigned long totalSize1 = 1;
			unsigned long totalSize2 = 1;
			bool twoIFuploads = false;

			// Wait until we have enough "post data"
			while (m_hwMgr.GetCurrFileOffset() < (currFilePos + m_numPostBytes))
			{
				//Busy wait.
				// Break if we already made a new file. Means we were at the edge in the first place.
				if (m_hwMgr.GetCurrFileOffset() < currFilePos)
				{
					break;
				}
			}

			// Scenaro 1: We have enough byes to upload.
			if (currFilePos >= m_numPreBytes)
			{
				cout << "Scenario1" << endl;
				offset1 = currFilePos - m_numPreBytes;
				totalSize1 = m_numPreBytes + m_numPostBytes;
			}
			else
			{ // Scenario 2: We need 2 files.
				if (m_hwMgr.GetFileHistorySize() >= 2)
				{
					twoIFuploads = true;
					cout << "Scenario2" << endl;

					offset1 = 0;
					totalSize1 = currFilePos + m_numPostBytes;

					localIF2 = m_hwMgr.GetLastIFFilename() + ".CH_" + payloadTokens[2];
					offset2 = m_hwMgr.GetMaxSingleFileSize() - m_numPreBytes + currFilePos;
					totalSize2 = m_numPreBytes - totalSize1;
				}
				else
				{
					cout << "Scenario 3" << endl;
					twoIFuploads = false;
					offset1 = 0;
					totalSize1 = currFilePos + m_numPreBytes;
				}
			}

			// Sanity check.
			if (totalSize1 > (m_hwMgr.GetMaxSingleFileSize() - offset1))
			{
				totalSize1 = m_hwMgr.GetMaxSingleFileSize() - offset1;
			}
			if (twoIFuploads && (totalSize2 > (m_hwMgr.GetMaxSingleFileSize() - offset2)))
			{
				totalSize2 = m_hwMgr.GetMaxSingleFileSize() - offset2;
			}

			// Perform the upload.
			if (!m_httpClient.upload(ifExt, localIF1.c_str(), m_http_port, offset1, totalSize1, true, true))
			{
				cerr << "Error uploading IF data!" << endl;
			}

			if (twoIFuploads && (!m_httpClient.upload(ifExt + ".0", localIF2.c_str(), m_http_port, offset2, totalSize2, true, true)))
			{
				cerr << "Error uploading the first part of the IF data." << endl;
			}

			if (!m_httpClient.upload(agcExt, m_agcFilename, m_http_port, 0, m_bytesWrittenAGC, true, false))
			{
				cerr << "Error uploading AGC." << endl;
			}
		}
	}

	// Check if this is an alert.
	if (payloadTokens[0] == "update" || payloadTokens[0] == "eph")
	{
		if(payloadTokens[0] == "eph" && payloadTokens[2] != m_nodeName){
			return 0;
		}

		cout << "Alert Received." << endl;
		unsigned long currentPos = m_hwMgr.GetCurrFileOffset();
		unsigned long totalSize = 1;
		unsigned long offset = 0;

		// Set the params.
		if(payloadTokens[0] == "update")
		{
			totalSize = m_numUpdateBytes;
		}
		else {
			totalSize = m_ephSize;
		}

		offset = currentPos;
		// target filename.
		string remoteFilename = m_http_host + "/" + m_nodeName + "/";

		if(payloadTokens[0] == "update"){
			remoteFilename = remoteFilename + "update" + payloadTokens[2];
		} else {
			remoteFilename = remoteFilename + "eph";
		}

		string ifExt = remoteFilename + ".IFCH_" + payloadTokens[1];
		string agcExt = remoteFilename + ".AGC";
		
		string localIF = m_hwMgr.GetIFFilename() + ".CH_" + payloadTokens[1];

		cout << "Upload filename: " << localIF << endl;

		string statusMsg = "status_update-" + m_nodeName + "-recvupdate";
		m_mqttCtlr.Publish("leader",statusMsg,0);

		// Wait for enough bytes.
		while (m_hwMgr.GetCurrFileOffset() < currentPos + totalSize)
		{
			if (m_hwMgr.GetCurrFileOffset() < currentPos)
			{
				cout << "Upload edge case." << endl;
				totalSize = m_hwMgr.GetMaxSingleFileSize() - currentPos;
				break;
			}
		}

		statusMsg = "status_update-" + m_nodeName + "-startupload";
		m_mqttCtlr.Publish("leader",statusMsg,0);

		// Perform upload.
		if (!m_httpClient.upload(ifExt, localIF.c_str(), m_http_port, offset, totalSize, true, true))
		{
			cerr << "Error uploading IF data!" << endl;
			statusMsg = "status_update-" + m_nodeName + "-uploadfail_if";
			m_mqttCtlr.Publish("leader",statusMsg,0);
		}
		if (!m_httpClient.upload(agcExt, m_agcFilename, m_http_port, 0, m_bytesWrittenAGC, true, false))
		{
			cerr << "Error uploading AGC." << endl;
			statusMsg = "status_update-" + m_nodeName + "-uploadfail_agc";
			m_mqttCtlr.Publish("leader",statusMsg,0);
		}
	}

	if (msgPayload == "pause")
	{
		m_hwMgr.SetWrite(false);
		//m_hwMgr.RequestIFWriteBreak();
	}

	if (msgPayload == "resume")
	{
		m_hwMgr.SetWrite(true);
	}

	if (msgPayload == "halt")
	{
		m_stopCond = true;
	}

	if (msgPayload == "profile_start")
	{
		m_profilePoint = chrono::steady_clock::now();

		m_mqttCtlr.Publish("alert", "profile_response", 0);
	}

	if (msgPayload == "profile_end")
	{
		auto profileEnd = chrono::steady_clock::now();
		chrono::duration<double> timeDiff = profileEnd - m_profilePoint;
		cout << "Round trip MQTT msg took: " << timeDiff.count() << endl;
	}


	//---------------------------------------------------------------------------------------
	// Depracted Msgs
	//---------------------------------------------------------------------------------------
	/*if(payloadTokens[0] == "full_pos")
	{
		// Convert time to ms

		unsigned long currPos = m_hwMgr.GetBytesWritten();
		cout << "Current position " << dec << currPos << endl;
		// Upload this data.
		// Upload Pre + Post chunk.
		string remoteLocation = m_http_host + "/" + m_nodeName + ".full40.IF.bin";
		unsigned long upSize = 640000000;

		while ((m_hwMgr.GetBytesWritten() - currPos) < upSize)
		{
					// Busy wait.
		}
		string chfn = m_hwMgr.GetIFFilename() + ".CH_0";
		if(!m_httpClient.upload(remoteLocation,chfn.c_str(), m_http_port, currPos, upSize, true, true))
		{
			cerr << "Error uploading IF data!" << endl;
		}
	}

	if(payloadTokens[0] == "time_check_start")
	{
		// Get current time in ms since start
		chrono::milliseconds ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
		unsigned long long mscount = ms.count();
		cout << "Num msec since epoch: " << dec << mscount << endl;
		m_mqttCtlr.Publish("leader","time_check_response-"+to_string(mscount),0);
	}

	if(payloadTokens[0] == "time_check_end")
	{
		// Convert time to ms
		unsigned long long msCenter = stoull(payloadTokens[1]);
		cout << "Got request for data centered at: " << msCenter << endl;

		// Find which block offset from start of file this is.
		chrono::milliseconds msStart = chrono::duration_cast<chrono::milliseconds>(m_hwMgr.GetTimeStart().time_since_epoch());
		unsigned long long timeDiff = msCenter -msStart.count();

		cout << "The time offset from start is: " << timeDiff << " ms from start" << endl;

		unsigned long blockOffset = (unsigned long)(timeDiff/m_hwMgr.GetMSPerBlock());
		blockOffset = blockOffset * BUFFER_BYTESIZE;

		cout << "The block offset is: " << blockOffset << " bytes from start" << endl;

		// Upload this data.
		// Upload Pre + Post chunk.
		string remoteLocation = m_http_host + "/" + m_nodeName + ".timecheck.IF.bin";
		unsigned long upSize = 128000000;

		while ((m_hwMgr.GetBytesWritten() - blockOffset) < upSize)
		{
			// Busy wait.
		}
		string chfn = m_hwMgr.GetIFFilename() + ".CH_0";
		if(!m_httpClient.upload(remoteLocation,chfn.c_str(), m_http_port, blockOffset, upSize, true, true))
		{
			cerr << "Error uploading IF data!" << endl;
		}
	}*/

	return 0;
}
