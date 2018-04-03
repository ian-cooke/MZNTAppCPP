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

LoggerController::LoggerController( string nodeName, string ifFilename ) : m_hwMgr(ifFilename)
{
	m_nodeName = nodeName;
	m_ifFilename = ifFilename;

	curl_init("gnssfast.coloardo.edu", 1337);

	m_numPreBytes = BUFFER_BYTESIZE*245;
	m_numPostBytes = BUFFER_BYTESIZE*100;
	m_uploading = false;

	m_numEvents = 0;
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
    if(! m_hwMgr.InitializeHardware("/etc/ConfigSet10.txt") ){
		cerr<<"Hardware Initialization failed." <<endl;
		return -1;
	} else {
		cout << "Hardware initialization successful." << endl;
	}

	// Start the MQTT subsystem.
	m_mqttCtlr.Start("tcp://gnssfast.colorado.edu:5554",m_nodeName.c_str(),"mznt","rj39SZSz", this);

	if(!m_mqttCtlr.Subscribe("alert")){
		cerr << "Failed to subscribe to alert topic" << endl;
	}

	string info_msg = m_nodeName + "-ONLINE";

	if(!m_mqttCtlr.Publish("info", info_msg.c_str(),2)) {
		cerr << "Failed to publish message." << endl;
	}

	// Initialize HTTP upload.
	curl_init("gnssfast.colorado.edu", 1337);

    if(! m_hwMgr.Begin() ){
	    cerr<<"HW Thread init failed." << endl;
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
bool LoggerController::Update(){
	return true;
}

//-----------------------------------------------------------------------
// Start
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies: 
// returns: True on success
//-----------------------------------------------------------------------
int LoggerController::MQTT_Callback( void * context, char *topicName, int topicLen, MQTTClient_message *message ) {
	string msgPayload( (char*)message->payload, message->payloadlen);
	cout << "Msg Recv: " << msgPayload << endl;

	// Check if this is an alert.
	if(msgPayload == "start") {
		if(!m_uploading){
			m_uploading = true;
			cout << "Alert Received." << endl;

			unsigned long currentPos = m_hwMgr.GetBytesWritten();

			cout << "Current position: " << m_hwMgr.GetBytesWritten() << " bytes." << endl;

			// Wait until we have at least pre+post bytes.
			cout << "Waiting for enough data." << endl;
			while( (m_hwMgr.GetBytesWritten()-currentPos) < m_numPostBytes ) {
				// Busy wait.
			}

			unsigned long totalSize = m_numPostBytes;
			unsigned long offset = 0;
			if(currentPos < m_numPreBytes) {
				totalSize += currentPos;
			} else {
				totalSize += m_numPreBytes;
				offset += (currentPos-m_numPreBytes);
			}

			cout << "Starting upload of size " << totalSize << " at offset " << offset << endl;

			// Upload Pre + Post chunk.
			string remoteLocation = "/" + m_nodeName + "/event" + to_string(m_numEvents) + ".bin";
			int result = asynch_send( m_ifFilename.c_str(), offset, totalSize, remoteLocation.c_str() );
			if( result < 0 ) {
				cerr << "Failed to start upload: " << endl;
			}

			m_uploading = false;
		}
	}

	return 0;
}

