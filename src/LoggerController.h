/*
 * LoggerController.h
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */
#ifndef LOGGER_CONTROLLER_H
#define LOGGER_CONTROLLER_H

#include "HardwareManager.h"
#include "MQTTController.h"
#include <string>

using namespace std;

class LoggerController : public MQTTMsgHandler {
public:
    LoggerController( string nodeName, string ifFilename );
    ~LoggerController();

    int MQTT_Callback( void * context, char *topicName, int topicLen, MQTTClient_message *message );

    bool Start();
    bool Stop();

    bool Update();

private:
    MQTTController m_mqttCtlr;
    HardwareManager m_hwMgr;
    string m_nodeName;
    string m_ifFilename;
    bool m_uploading;
    unsigned long m_numEvents;
    unsigned long m_numPreBytes;
    unsigned long m_numPostBytes;
};

#endif
