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
#include "HTTPClient.h"
#include <string>
#include <fstream>

using namespace std;

class LoggerController : public MQTTMsgHandler {
public:
    LoggerController( string nodeName, string ifFilename, string agcFilename,
                      string config_filename, unsigned long numUpdateBytes, unsigned long numPreBytes,
                      unsigned long numPostBytes, unsigned long uploadRefactorySec,
                      string http_host, int http_port,
                      string mqtt_host, int mqtt_port, bool counterOn );
    ~LoggerController();

    int MQTT_Callback( void * context, char *topicName, int topicLen, MQTTClient_message *message );

    bool Start();
    bool Stop();

    bool Update(double elapsed);

    bool GetErrorState() { return m_hwMgr.GetErrorState(); }

private:
    MQTTController m_mqttCtlr;
    HardwareManager m_hwMgr;
    HTTPClient m_httpClient;
    ofstream m_agcLogFile;
    string m_nodeName;
    string m_ifFilename;
    string m_agcFilename;
    string m_http_host;
    string m_mqtt_host;
    int m_http_port;
    int m_mqtt_port;
    bool m_uploading;
    int m_agcRate;
    int m_agcThreshold;
    double m_agcUpdateRate;
    unsigned long m_numEvents;
    unsigned long m_numUpdates;
    unsigned long m_numPreBytes;
    unsigned long m_numUpdateBytes;
    unsigned long m_numPostBytes;
    unsigned long m_uploadRefactory;
    unsigned long m_bytesWrittenAGC;
};

#endif
