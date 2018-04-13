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
								   int agc_threshold, int agc_updateHz, bool splitWrite) : m_hwMgr(ifFilename, config_filename, counterOn, resampling, splitWrite)
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
	m_uploading = false;
	m_stopCond = false;

	m_uploadRefactory = uploadRefactorySec;
	m_numUpdateBytes = (numUploadBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;

	m_numEvents = 0;
	m_numUpdates = 0;
	m_bytesWrittenAGC = 0;
	m_agcUpdateRate = 1.0/(double)agc_updateHz;
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
		return false;
	}

	// Set the mqtt hw handle
	HTTPClient::ms_hwHandle = &m_hwMgr;

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
	m_agcLogFile.open(m_agcFilename.c_str(), ios::out | ios::binary );
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
	unsigned long long mscount = ms.count();
	cout << "Num msec since epoch: " << dec << mscount << endl;

	// Initialize the agc data.
	AGCLogData initialData[4];
	m_hwMgr.GetAGCData( initialData );
	for(int i=0; i < 4; i++){
		m_agcLast[i] = initialData[i];
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

	// Check for errors.
	if(m_hwMgr.GetNumErrors() > numLastErrors) {
		// Publish the error.
		m_mqttCtlr.Publish( "errors", m_nodeName + " had a HW error.", 0);
		// Reset errors.
		//m_hwMgr.ResetNumErrors();
		numLastErrors = m_hwMgr.GetNumErrors();
	}

	if((elapsed-lastTime) >= m_agcUpdateRate) {
		lastTime = elapsed;

		AGCLogData data[4];
		m_agcLogFile.open(m_agcFilename.c_str(), ios::out | ios::binary  | ios::app );	
		if(!m_agcLogFile.is_open()){
			cerr << "Failed to open AGC logfile." << endl;
			return false;
		}

		m_hwMgr.GetAGCData( data );
		m_agcLogFile.write(reinterpret_cast<const char*>(data), streamsize(sizeof(AGCLogData)*4));
		m_agcLogFile.close();

		// Update the agc data and send MQTT if necessary
		for( int i = 0; i < 4; i++ ) {
			if( abs(m_agcLast[i].ifAGC - data[i].ifAGC) > m_agcThreshold ) {
				auto eventTime = chrono::system_clock::now();
				time_t eventTT = chrono::system_clock::to_time_t(eventTime);
				cout << "Time: " << std::ctime(&eventTT) << endl;
				if(m_agcLast[i].ifAGC < data[i].ifAGC) {
					cout << "AGC on channel " << i << " went HIGH at "  << endl;
					m_mqttCtlr.Publish( "leader" , "HIGH-" + std::to_string(i),0);
				} else {
					cout << "AGC on channel " << i << " went LOW at" << endl;
					m_mqttCtlr.Publish( "leader" , "LOW-" + std::to_string(i),0);
				}
			}
			
			m_agcLast[i] = data[i];
 		}

		//cout << "Updated AGC logfile."<<endl;
		m_bytesWrittenAGC += sizeof(AGCLogData)*4;

		// Publish.
		string topic = m_nodeName + "-agc";
		//m_mqttCtlr.Publish(topic, "keepalive",0);
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
	while(getline(tokenStream,token,'-'))
	{
		payloadTokens.push_back(token);
	}

	// Check if this is an alert.
	if ((msgPayload == "start") || (msgPayload == "update"))
	{
		unsigned long currentPos = m_hwMgr.GetBytesWritten();
		cout << "Alert Received." << endl;
		unsigned long totalSize = 0;
		unsigned long offset = 0;
		unsigned long numBytesRequired = m_numUpdateBytes;

		// If this is an event, wait until we have enough "post" data.
		if (msgPayload == "start")
		{
			numBytesRequired = m_numPreBytes;

			// Wait until we have at least pre+post bytes.
			cout << "Waiting for enough data." << endl;
			while ((m_hwMgr.GetBytesWritten() - currentPos) < m_numPreBytes)
			{
				// Busy wait.
			}

			// Increase total transfer size by num pre_bytes.
			totalSize += m_numPreBytes;
		}

		cout << "Current position: " << fixed << currentPos << " bytes." << endl;

		// Check if our current position is less than the number of "preBytes"
		if (currentPos < numBytesRequired)
		{
			// We don't have enough bytes, so our offset will be zero.
			totalSize += currentPos;
			offset = 0;
		}
		else
		{
			// we have enough bytes, so calculate offset
			totalSize += numBytesRequired;
			offset = (currentPos - numBytesRequired);
		}

		cout << "Starting upload of size " << fixed << (int)totalSize << " at offset " << fixed << (int)offset << endl;

		// Upload Pre + Post chunk.
		string remoteLocation = m_http_host + "/" + m_nodeName;
		string remoteLocationAGC = m_http_host+"/" + m_nodeName;
		if (msgPayload == "start")
		{
			remoteLocation += ("/event" + to_string(m_numEvents) + ".IF.bin");
			remoteLocationAGC += ("/event" + to_string(m_numEvents) + ".AGC.bin");
			m_numEvents++;
		}
		else
		{
			remoteLocation += ("/update" + to_string(m_numUpdates) + ".IF.bin");
			remoteLocationAGC += ("/update" + to_string(m_numUpdates) + ".AGC.bin");
			m_numUpdates++;
		}

		if(!m_httpClient.upload(remoteLocation,m_hwMgr.GetIFFilename().c_str(), m_http_port, offset, totalSize, true, true))
		{
			cerr << "Error uploading IF data!" << endl;
		}

		if(!m_httpClient.upload(remoteLocationAGC, m_agcFilename, m_http_port, 0, m_bytesWrittenAGC, true, false)) {
			cerr << "Error uploading AGC." << endl;
		}
	}

	if( msgPayload == "pause") {
		m_hwMgr.SetWrite(false);
		m_hwMgr.RequestIFWriteBreak();
	}
	
	if(msgPayload == "resume") {
		m_hwMgr.SetWrite(true);
	}

	if(msgPayload == "halt") {
		m_stopCond = true;
	}

	if(msgPayload == "profile_start") {
		m_profilePoint = chrono::steady_clock::now();

		m_mqttCtlr.Publish("alert", "profile_response", 0);
	}

	if(msgPayload == "profile_end") {
		auto profileEnd = chrono::steady_clock::now();
		chrono::duration<double> timeDiff = profileEnd - m_profilePoint;
		cout << "Round trip MQTT msg took: " << timeDiff.count() << endl; 
	}

	if(payloadTokens[0] == "full_pos")
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
	}

	return 0;
}
