#ifndef HTTP_C_H
#define HTTP_C_H

#include <string>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "system_defines.h"
#include "HardwareManager.h"

using namespace std;

class HTTPClient
{
  public:
    HTTPClient();
    ~HTTPClient();

    bool upload(string address, string local_filename, int port,
                unsigned long offset, unsigned long length, bool block, bool cancelCurr);

    static void *UploadHelper(void *context)
    {
        if (!context)
        {
            return NULL;
        }
        return (((HTTPClient *)context)->easy_worker(NULL));
    }

    bool isInitialized()
    {
    	return m_initialized;
    }

    static HardwareManager *ms_hwHandle;

  private:
    void *easy_worker(void *parameters);
    static size_t read_callback(char *buffer, size_t size, size_t nitems, void *instream)
    {
    	size_t readSize = size*nitems;
    	if(readSize+ms_total > ms_uploadSize) {
    		readSize = ms_uploadSize-ms_total;
    	}
    	if(ms_hwHandle){
    		while(ms_hwHandle->GetDataQueueSize() > (DDR_Q_SIZE/2))
    		{
    			// Busy wait.
    		}
    	}
    	//cout << "Reading " << dec<< nitems << " of size " << dec << size << endl;
        ifstream *infile = (ifstream *)instream;
        infile->read(buffer, readSize);
        //cout << "Expected to read " << size*nitems << endl;
        //cout << "Read " << infile->gcount() << endl;
        ms_total += infile->gcount();
        //cout << "Total " << total << endl;
        return infile->gcount();
    }

    static size_t ms_uploadSize;
    static size_t ms_total;
    bool m_initialized;
    bool m_uploading;

    unsigned long m_uploadSize;
    unsigned long m_uploadOffset;
    unsigned long m_bytesUploaded;

    string m_localFilename;
    string m_remoteAddress;

    int m_port;
};

#endif
