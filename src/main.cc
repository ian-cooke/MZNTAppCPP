/*
 * main.c
 * Entry point for MZNT App
 * Christopher Bate
 * C++ Re-work of original C code, March 2018
 *
 */
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <chrono>
#include "LoggerController.h"

using namespace std;

int main(int argc, char **argv)
{
	// Initialize our default options.
	string if_file("/mnt/IF.bin");
	string agc_file("/mnt/AGC");
	string config_file = "/etc/ConfigSet10.txt";
	string node_name("mz0");
	unsigned int resamp_threshold = 0x04000000;
	unsigned long runtime = 3600;
	bool counter = false;
	bool resampling = true;
	int agc_alert_thresh = 9;
	unsigned int upload_refactory = 120; 
	string http_host("gnssfast.colorado.edu");
	string mqtt_host("gnssfast.colorado.edu");
	int http_port = 1337;
	int mqtt_port = 5554;
	int result;
	unsigned long update_byte_size = 16000000*8;
	unsigned long num_pre_bytes = 16000000*8;
	unsigned long num_post_bytes = 16000000*8;

	// Parse the command line options.
	while ((result = getopt(argc, argv, "o:s:c:t:anh:q:i:e:N:u:m:p:P:")) > 0)
	{
		switch (result)
		{
		case 'o':
			if_file = optarg;
			break;
		case 's':
			runtime = strtod(optarg, NULL);
			break;
		case 'c':
			config_file = optarg;
			break;
		case 't':
			resamp_threshold = (unsigned int)strtol(optarg, NULL, 0);
			break;
		case 'a':
			counter = true;
			break;
		case 'n':
			resampling = false;
			break;
		case 'h':
			http_host = optarg;
			break;
		case 'q':
			agc_file = optarg;
			break;
		case 'i':
			agc_alert_thresh = atoi(optarg);
			break;
		case 'e':
			upload_refactory = (unsigned int)atoi(optarg);
			break;
		case 'N':
			node_name = optarg;
			break;
		case 'u':
			update_byte_size = atoi(optarg)*1000000;
			break;
		case 'm':
			mqtt_host = optarg;
			break;
		case 'p':
			http_port = atoi(optarg);
			break;
		case 'P':
			mqtt_port = atoi(optarg);
			break;
		}
	}

	// Display the options
	cout << "IF File: " << if_file << endl << "AGC File: " << agc_file <<endl;
	cout << "Resampling: " << resampling << endl << "Config: " << config_file <<endl;
	cout << "Runtime: " << runtime <<endl;
	cout << "Resamp threshold: " << resamp_threshold <<endl;
	cout << "HTTP Host: " << http_host << " at port: " << http_port << endl;
	cout << "MQTT Host: " << mqtt_host << " at port: " << mqtt_port << endl;
	cout << "Upload Refactory Time: " << (int) upload_refactory << endl;
	cout << "Node name: " << node_name << endl; 
	cout << "AGC Alert threshold: " << agc_alert_thresh << endl;
	cout << "Counter on: " << counter << endl;
	cout << "Normal update size: " << (update_byte_size/1000000) << " mb " <<endl;
	cout << "Event (pre) data size: " << (num_pre_bytes/1000000) << " mb" << endl;
	//cout << "Event (post) data size: " << 
 	
	LoggerController logger(node_name, if_file,agc_file, config_file, update_byte_size, 
							num_pre_bytes,num_post_bytes, upload_refactory,
							http_host, http_port, mqtt_host, mqtt_port );

	if (!logger.Start())
	{
		cout << "Error starting." << endl;
		return -1;
	}

	// Main loop.
	auto startTime = chrono::steady_clock::now();
	while(1){
		auto currTime = chrono::steady_clock::now();
		chrono::duration<double> elapsed = currTime - startTime;

		if(elapsed.count() > runtime){
			cout << "Stopping. " << elapsed.count() << " sec elapsed." << endl;
			break;
		}

		logger.Update(elapsed.count());
	}

	if (!logger.Stop())
	{
		cout << "Error stopping." << endl;
		return -1;
	}

	cout << "Goodbye." << endl;

	return 0;
}
