/*
 * SerialManager.cpp
 *
 *  Created on: Apr 11, 2018
 *      Author: Ian
 */
#include "SerialManager.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

SerialManager::SerialManager(string serialFilename, unsigned long baudRate, int messageBytes) {
	m_serialFilename = serialFilename;
	m_baudRate = baudRate;
	m_messageBytes = messageBytes;
}

bool SerialManager::Init() {

	// Open the port
	m_fd = open(m_serialFilename.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
	if (m_fd == -1) {
		perror("Cannot open file");
		return false;
	} else {
		fcntl(m_fd, F_SETFL, 0);
	}

	// Port Settings
	struct termios options;	// Get the Current options for the port
	tcgetattr(m_fd, &options);

	// Set the baud rate
	bool baudSet = setBaudRate(options);
	if (!baudSet) {
		return false;
	}
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= (CLOCAL | CREAD); // Enable the receiver and set local mode…
	options.c_cflag |= CS8;
	tcsetattr(m_fd, TCSANOW, &options);	// Set the new options for the port…

	return true;
}

string SerialManager::Read() {
	// Parse Mavlink protocol
	char buff[m_messageBytes];
	int rd;
	rd = read(rd, buff, m_messageBytes);
	if (rd == -1) {
		perror("Cannot open file");
		return "NULL";
	}
	return string(buff);

}

bool SerialManager::Write(int num) {
	char a = '0' + num;
	ssize_t n = write(m_fd, &a, 1);

	if (n < 0) {
		return false;
	}
	return true;
}

bool SerialManager::Close() {
	int s = close(m_fd);
	if (s < 0) {
		return false;
	}
	return true;
}

bool SerialManager::setBaudRate(struct termios options) {
	bool success = true;
	switch (m_baudRate) {
		case 4800: cfsetispeed(&options, B4800);
				   cfsetospeed(&options, B4800);
				   break;
		case 9600: cfsetispeed(&options, B9600);
				   cfsetospeed(&options, B9600);
				   break;
		case 19200: cfsetispeed(&options, B19200);
					cfsetospeed(&options, B19200);
					break;
		case 38400: cfsetispeed(&options, B38400);
					cfsetospeed(&options, B38400);
					break;
		case 57600: cfsetispeed(&options, B57600);
					cfsetospeed(&options, B57600);
					break;
		case 115200: cfsetispeed(&options, B115200);
					 cfsetospeed(&options, B115200);
					 break;
		default: success = false;
	}
	return success;
}
