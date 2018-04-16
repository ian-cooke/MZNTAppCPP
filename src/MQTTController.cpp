/*
* MQTTController.cpp
* Chris
* April, 2018
*/

#include "MQTTController.h"
#include <MQTTClient.h>

#include <iostream>

using namespace std;

//-----------------------------------------------------------------------
// Constructor
// Requires:
// Modifies: 
// returns: NA
//-----------------------------------------------------------------------
MQTTController::MQTTController()
{
    m_conn_opts = MQTTClient_connectOptions_initializer;
    m_connected = false;
}

//-----------------------------------------------------------------------
// Deconstructor
// Requires:
// Modifies: 
// returns: NA
//-----------------------------------------------------------------------
MQTTController::~MQTTController() 
{
    if(m_connected) {
        MQTTClient_disconnect(m_paho_client, 1000);
    }

    MQTTClient_destroy(&m_paho_client);
}

//-----------------------------------------------------------------------
// Start
// Requires: ma is a function with type MQTTClient_messageArrived
// Modifies: 
// returns: True on success
//-----------------------------------------------------------------------
bool MQTTController::Start(const char *hostAddress, const char *clientID, 
            const char *username, const char *passwd, MQTTMsgHandler *context)
{
    int result;

    m_brokerAddress = hostAddress;
    m_username = username;
    m_password = passwd;
    m_clientID = clientID;

    m_conn_opts.username = m_username.c_str();
    m_conn_opts.password = m_password.c_str();
    m_conn_opts.keepAliveInterval = 20;
    m_conn_opts.cleansession = 1;

    result = MQTTClient_create(&m_paho_client, m_brokerAddress.c_str(),m_clientID.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if( result != MQTTCLIENT_SUCCESS ){
        cerr << "MQTTClient failed to be created." << endl;
        return false;
    }

    result = MQTTClient_setCallbacks( m_paho_client, (void*)context, NULL, MQTTController::MA_Callback, NULL );

    if( result != MQTTCLIENT_SUCCESS) {
        cerr << "Failed to set callback for MQTT Client!" << endl;
        return false;
    }

    // Attempt the connection.
    if( !Connect() ){
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------
// connect
// Requires: Class initialized (Start called)
// Modifies: 
// returns: True on success
//-----------------------------------------------------------------------
bool MQTTController::Connect() 
{
    int result;

    result = MQTTClient_connect(m_paho_client, &m_conn_opts);
    if(result != MQTTCLIENT_SUCCESS){
        cerr << "MQTTClient failed to connect with error code "<< result << endl;
        return false;
    } else {
        cout << "MQTTClient connected successfully." << endl;
    }

    m_connected = true;

    return true;
}

//-----------------------------------------------------------------------
// connect
// Requires: Class initialized (Start called)
// Modifies: 
// returns: True on success
//-----------------------------------------------------------------------
bool MQTTController::Subscribe( string topic )
{
    int result;
    result = MQTTClient_subscribe( m_paho_client, topic.c_str(), 0);
    if(result != MQTTCLIENT_SUCCESS ) {
        cerr << "Failed to subscribe to " << topic << endl;
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------
// Publish
// Requires: Class initialized (Start called), connected
// Modifies: 
// returns: True on success
//-----------------------------------------------------------------------
bool MQTTController::Publish( string topic, string message, int qos )
{
    int result;
    result = MQTTClient_publish( m_paho_client, topic.c_str(), message.length(), (void*)message.c_str(), qos, 0, NULL);
    if( result != MQTTCLIENT_SUCCESS ){
        return false;
    }

    return true;
}
bool MQTTController::Publish( string topic, unsigned char *data, size_t length, int qos )
{
    int result;
    result = MQTTClient_publish( m_paho_client, topic.c_str(), length, (void*)data, qos, 0, NULL);
    if( result != MQTTCLIENT_SUCCESS ){
        return false;
    }

    return true;
}