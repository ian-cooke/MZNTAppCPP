#include "HTTPClient.h"
#include <curl/curl.h>
#include <pthread.h>
#include <iostream>
#include <fstream>

HTTPClient::HTTPClient() 
{
    m_uploading = false;

    curl_global_init(CURL_GLOBAL_ALL);

    m_initialized = true;

    m_bytesUploaded = 0;

    m_port = 1337;
}


HTTPClient::~HTTPClient()
{
    curl_global_cleanup();
}


bool HTTPClient::upload( string address, string local_filename, int port, unsigned long offset, unsigned long length, bool block)
{
    if(!m_uploading) {
        m_uploading = true;
        m_uploadOffset = offset;
        m_uploadSize = length;
        m_localFilename = local_filename;
        m_remoteAddress = address;
        m_port = port;
        pthread_t id;
         // Create thread
        pthread_create(&id, NULL, HTTPClient::UploadHelper, (void*)this);

        if(block){
            pthread_join(id, NULL);
            return true;
        }

        return true;
    } 
    else 
    {
        cerr<< "Error, tried to do multiple uploads" << endl;
        return false;
    }

    return true;
}


void* HTTPClient::easy_worker(void* parameters)
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
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        curl_easy_setopt(curl, CURLOPT_URL, m_remoteAddress.c_str() );

        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)m_uploadSize );
        /* set where to read from */
        
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

        ifstream infile;

        infile.open(m_localFilename.c_str(), ifstream::in | ifstream::binary );
        
        if(!infile.is_open()) {
            cerr<<"Could not open file for reading." <<endl;
        } else {
            infile.seekg(m_uploadOffset);
            curl_easy_setopt(curl, CURLOPT_READDATA, &infile);
            CURLcode res = curl_easy_perform(curl);

            infile.close();

            cout << "Closed file." << endl;
            //cout << "Bytes read: " << m_bytesUploaded <<endl;

            if(res!=CURLE_OK) {
                cerr << "CURL upload failed " << curl_easy_strerror(res) << endl;
            }
        }

        curl_easy_cleanup(curl);
        m_uploading = false;
        m_bytesUploaded = 0;
    } 
    else 
    {
        cerr << "Error, could not create easy curl." << endl;
        m_uploading = false;
    }

    return NULL;
}
