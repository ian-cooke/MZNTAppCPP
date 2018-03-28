/*
 * main.c
 * Entry point for MZNT App
 * Christopher Bate
 * C++ Re-work of original C code, March 2018
 *
 */
#include <iostream>

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

	return 0;
}
