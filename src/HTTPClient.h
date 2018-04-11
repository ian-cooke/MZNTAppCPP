#ifndef HTTP_C_H
#define HTTP_C_H

#include <string>
#include <unistd.h>
#include <fstream>

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

  private:
    void *easy_worker(void *parameters);
    static size_t read_callback(char *buffer, size_t size, size_t nitems, void *instream)
    {
        ifstream *infile = (ifstream *)instream;
        infile->read(buffer, size * nitems);
        //cout << "Expected to read " << size*nitems << endl;
        //cout << "Read " << bytesRead << endl;
        return infile->gcount();
    }

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
