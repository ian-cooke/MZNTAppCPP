#ifndef HTTP_C_H
#define HTTP_C_H

#include <string>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "system_defines.h"
#include "HardwareManager.h"
#include "ThreadPool.h"

using namespace std;

class UploadTask : public Task
{
  public:
    UploadTask( string filename, unsigned long offset, unsigned long size,
          string fullRemotePath, int port)
    {
        m_localFilename = filename;
        m_upParams.offset = offset;
        m_upParams.uploadSize = size;
        m_upParams.total = 0;
        m_port = port;
        m_fullRemotePath = fullRemotePath;
        cout << "Created new UploadTask for: " << m_fullRemotePath << endl;
    }

    ~UploadTask()
    {
        cout << "UploadTask exiting" << endl;
    }

    static HardwareManager *ms_hwHandle;

    void Run();

    void ShowTask()
    {
        cout << "UploadTask: " << m_fullRemotePath << endl;
    }

  private:
    static size_t read_callback(char *buffer, size_t size, size_t nitems, void *instream)
    {
        // Get the parameters
        size_t readSize = size * nitems;
        upload_params *upParams = (upload_params*)instream;

        if (readSize + upParams->total > upParams->uploadSize)
        {
            readSize = upParams->uploadSize - upParams->total;
        }

        if (ms_hwHandle)
        {
            while (ms_hwHandle->GetDataQueueSize() > (DDR_Q_SIZE / 2))
            {
                // Busy wait.
            }
        }
        //cout << "Reading " << dec<< nitems << " of size " << dec << size << endl;
        upParams->infile.read(buffer, readSize);
        //cout << "Expected to read " << size*nitems << endl;
        //cout << "Read " << infile->gcount() << endl;
        upParams->total += upParams->infile.gcount();
        //cout << "Total " << total << endl;
        return upParams->infile.gcount();
    }

    struct upload_params
    {
        unsigned long offset;
        unsigned long uploadSize;
        unsigned long total;
        ifstream infile;
    };
    int m_port;
    string m_localFilename;
    string m_fullRemotePath;
    upload_params m_upParams;
};

class HTTPClient
{
  public:
    HTTPClient();
    ~HTTPClient();

    bool upload(string address, string local_filename, int port,
                unsigned long offset, unsigned long length, bool block, bool cancelCurr);

    bool isInitialized()
    {
        return m_initialized;
    }

    static HardwareManager *ms_hwHandle;

  private:
    bool m_initialized;

    ThreadPool m_threadPool;
};

#endif
