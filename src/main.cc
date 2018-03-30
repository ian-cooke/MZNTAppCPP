/*
 * main.c
 * Entry point for MZNT App
 * Christopher Bate
 * C++ Re-work of original C code, March 2018
 *
 */
#include <iostream>
#include <unistd.h>
#include "HardwareManager.h"

using namespace std;

int main()
{
	HardwareManager hwMgr;

	if(! hwMgr.InitializeHardware("/etc/ConfigSet10.txt") ){
		cerr<<"Hardware Initialization failed." <<endl;
		return -1;
	} else {
		cout << "Hardware initialization successful." << endl;
	}

	if(! hwMgr.Begin() ){
		cerr<<"HW Thread init failed." << endl;
	}

	sleep(5);

	hwMgr.Stop();

	cout << "Exiting." << endl;

	return 0;
}
