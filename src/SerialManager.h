/*
 * SerialManaerger.h
 *
 *  Created on: Apr 11, 2018
 *      Author: Ian
 */
#ifndef SERIAL_H
#define SERIAL_H

#include <string>
#include <fstream>
#include <termios.h>

using namespace std;

class SerialManager {

public:
    SerialManager( string serialFilename, unsigned long baudRate, int messageBytes);
    bool Init();
    string Read();
    bool Write( int num );
    bool Close();

private:
    bool setBaudRate(struct termios options);
    string m_serialFilename;
    unsigned long m_baudRate;
    int m_fd;
    int m_messageBytes;
};


#endif // SERIAL_H
