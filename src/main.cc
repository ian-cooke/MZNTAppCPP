/*
 * main.c
 * Entry point for MZNT App
 * Christopher Bate
 * C++ Re-work of original C code, March 2018
 *
 */
#include <iostream>
#include <unistd.h>
#include "LoggerController.h"

using namespace std;

int main()
{
	LoggerController logger("mz1", "/mnt/mz1.IF.bin");

	if(!logger.Start()) {
		cout << "Starting." << endl;
	}

	sleep(60);

	if(!logger.Stop()) {
		cout << "Stopping." << endl;
	}

	return 0;
}
