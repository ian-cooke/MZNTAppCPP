#include "HTTPClient.h"
#include <curl/curl.h>
#include <pthread.h>
#include <iostream>
#include <fstream>

// Initialize statics
HardwareManager *HTTPClient::ms_hwHandle = NULL;
HardwareManager *UploadTask::ms_hwHandle = NULL;

HTTPClient::HTTPClient() : m_threadPool(5)
{
    curl_global_init(CURL_GLOBAL_ALL);

    m_initialized = true;
}


HTTPClient::~HTTPClient()
{
    curl_global_cleanup();
}

bool HTTPClient::upload( string address, string local_filename, int port, unsigned long offset, unsigned long length, bool block, bool cancelCurr)
{
    // Don't allow outstanding tasks.
    if(!m_threadPool.AllThreadsOccupied()) {
        m_threadPool.AddTask( new UploadTask(local_filename, offset, length, address, port));
        return true;
    } 
    else 
    {
        cerr<< "MAX UPLOADS, ignoring" << endl;
        return false;
    }

    return true;
}


void UploadTask::Run()
{
    CURL *curl = curl_easy_init();

    if(curl)
    {
         /* upload to this place */
        curl_easy_setopt(curl, CURLOPT_PORT, m_port);

        /* tell it to "upload" to the URL */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);

        /* enable verbose for easier tracing */
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        curl_easy_setopt(curl, CURLOPT_URL, m_fullRemotePath.c_str() );
        cout << "Setting upload size to " << dec << (curl_off_t)m_upParams.uploadSize<< endl;
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)m_upParams.uploadSize );
        
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, UploadTask::read_callback);

        // Limit the upload speed so we don't kill the HDD speed
        //curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, 1000000);
        //curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
        m_upParams.infile.open(m_localFilename.c_str(), ifstream::in | ifstream::binary );
        
        if(!m_upParams.infile.is_open()) {
            cerr<<"Could not open file for reading." <<endl;
        } else {
        	cout << "Seeking to: " << m_upParams.offset << endl;
            m_upParams.infile.seekg(m_upParams.offset);
            curl_easy_setopt(curl, CURLOPT_READDATA, &m_upParams);
            CURLcode res = curl_easy_perform(curl);

            m_upParams.infile.close();
            
            if(res!=CURLE_OK) {
                cerr << "CURL upload failed " << curl_easy_strerror(res) << endl;
            }
        }

        curl_easy_cleanup(curl);
    } 
    else 
    {
        cerr << "Error, could not create easy curl." << endl;
    }
    

    // These tasks are allocated dynamically so we have to delete ourselves.
    delete this;
}
