/*
 * MQTTController.h
 *
 *  Created on: April 2018
 *      Author: chris
 */
#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H

#include <string>
#include <iostream>
#include <MQTTClient.h>
\
using namespace std;

class MQTTMsgHandler
{
public:
	virtual int MQTT_Callback( void * context, char *topicName, int topicLen, MQTTClient_message *message ) = 0;
};

class MQTTController {
public:
    MQTTController();
    ~MQTTController();

    bool Start(const char *hostAddress, const char *clientID,
               const char *username, const char *passwd, MQTTMsgHandler *context);

    bool Connect();

    bool Publish( string topic, string message, int qos );
    bool Subscribe( string topic );

private:
    string m_brokerAddress;
    string m_username;
    string m_password;
    string m_clientID;
    
    static int MA_Callback(void * context, char *topicName, int topicLen, MQTTClient_message *message) {
    	cout<<"MSG RECV." << endl;
        (static_cast<MQTTMsgHandler*>(context))->MQTT_Callback(context, topicName, topicLen, message);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    bool m_connected;

    MQTTClient m_paho_client;
    MQTTClient_connectOptions m_conn_opts;
};

#endif
