/*
 * LoggerController.cpp
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */

#include "LoggerController.h"
#include "../MZNT_http/http-client.h"
#include "system_defines.h"

#include <iostream>
#include <string>

using namespace std;

LoggerController::LoggerController(string nodeName, string ifFilename, string config_filename,
								   unsigned long numUploadBytes, unsigned long numPreBytes,
								   unsigned long numPostBytes,
								   unsigned long uploadRefactorySec,
								   string http_host, int http_port,
								   string mqtt_host, int mqtt_port) : m_hwMgr(ifFilename, config_filename)
{
	m_nodeName = nodeName;
	m_ifFilename = ifFilename;

	m_mqtt_port = mqtt_port;
	m_http_port = mqtt_port;

	m_http_host = http_host;
	m_mqtt_host = "tcp://" + mqtt_host + ":" + to_string(mqtt_port);

	m_numPreBytes = (numPreBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;
	m_numPostBytes = (numPostBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;
	m_uploading = false;

	m_uploadRefactory = uploadRefactorySec;
	m_numUpdateBytes = (numUploadBytes / BUFFER_BYTESIZE) * BUFFER_BYTESIZE;

	m_numEvents = 0;
	m_numUpdates = 0;
}

LoggerController::~LoggerController()
{
	curl_destroy();
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

	if (!m_mqttCtlr.Publish("info", info_msg.c_str(), 2))
	{
		cerr << "Failed to publish message." << endl;
	}

	// Initialize HTTP upload.
	if (curl_init("gnssfast.colorado.edu", 1337) < 0)
	{
		cerr << "Failed to initialize curl." << endl;
	}

	if (!m_hwMgr.Start())
	{
		cerr << "HW init failed." << endl;
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
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies:
// returns: True on success
//-----------------------------------------------------------------------
bool LoggerController::Update()
{
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
		string remoteLocation = "/" + m_nodeName;
		if (msgPayload == "start")
		{
			remoteLocation += ("/event" + to_string(m_numEvents) + ".bin");
			m_numEvents++;
		}
		else
		{
			remoteLocation += ("/update" + to_string(m_numUpdates) + ".bin");
			m_numUpdates++;
		}
		int result = asynch_send(m_ifFilename.c_str(), offset, totalSize, remoteLocation.c_str());
		if (result < 0)
		{
			cerr << "Failed to start upload: " << endl;
		} else {
			cout << "Asynch upload started." << endl;
		}
	}

	return 0;
}
