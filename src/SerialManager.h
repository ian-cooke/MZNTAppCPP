#ifndef SERIAL_H
#define SERIAL_H

#include <string>
#include <fstream>

using namespace std;

class SerialManager {
public:
    SerialManager( string serialFilename, unsigned long baudRate ) 
    {
        m_baudRate = baudRate;
        m_serialFilename = serialFilename;
    }

    bool Init() {   
        m_file.open( m_serialFilename.c_str(), ios::in | ios::out );

        return true;
    }

    string Read() {
        string result; 

        m_file >> result;

        return result;
    }

    bool Write( int num ) {
        m_file << num;
        return true;
    }

    bool Close() {
        m_file.close();
        return true;
    }


private:
    string m_serialFilename;
    fstream m_file;
    unsigned long m_baudRate;
};


#endif