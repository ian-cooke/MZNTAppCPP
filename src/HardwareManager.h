/*
 * HardwareManager.h
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */

#ifndef SRC_HARDWAREMANAGER_H_
#define SRC_HARDWAREMANAGER_H_

#include <queue>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include "ThreadSafeQueue.h"

using namespace std;

class HardwareManager {
public:
	HardwareManager( string if_outFilename, string conf_filename );
	virtual ~HardwareManager();

	bool Start();

	bool Stop();

	void *HWThread(void);
	void *WriterThread(void);


	// This is for launching the HW Thread within the context of this class.
	static void *HWThread_Helper(void *context) {
		if(!context){
			return NULL;
		}

		return ((HardwareManager*)context)->HWThread();
	}

	static void *WriterThread_Helper(void *context){
		if(!context){
			return NULL;
		}
		return ((HardwareManager*) context)->WriterThread();
	}

	unsigned long GetBytesWritten() { return m_bytesWritten; }
	bool InitializeHardware();


private:

	bool ConfigureNT1065(bool verbose);

	bool ParseNTConfig(  volatile uint32_t * SpiWrBuffer);

	bool ConfigureFPGASettings();

	bool EnableAXI_Interrupts();

	unsigned long *mmapDMABuffer( size_t length, unsigned long *phys_address );
	unsigned long *mmapDDR( size_t length, off_t offset, volatile unsigned long **pageStart );

	string m_IF_baseName;
	const char *m_AGC_fileName;
	bool m_counterOn;
	bool m_resamplingOn;
	uint32_t m_resampThreshold;
	bool m_initialized;
	int m_numErrors;

	pthread_t m_hwThread;
	pthread_t m_writerThread;
	bool m_stopSignal;
	bool m_breakFileSignal;

	unsigned long m_bytesWritten;

	string m_conf_filename;

	//std::queue <char*> m_dataQueue;
	ThreadSafeQueue m_dataQueue;

	volatile unsigned long *m_ddrBase;
	volatile unsigned long *m_ddrBaseOff;
	volatile unsigned long *m_cdmaBase;
	volatile unsigned long *m_cdmaBaseOff;
	volatile unsigned long *m_intrBase;
	volatile unsigned long *m_intrBaseOff;
	unsigned long m_paddr_dmaTarget;
};

#endif /* SRC_HARDWAREMANAGER_H_ */
