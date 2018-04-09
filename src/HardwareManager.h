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

//#pragma pack(push, 1)
typedef struct AGCLogData
{
	uint64_t ms_truncated; // ~49 days to overflow
	uint8_t chnm;		   // Channel number info
	uint8_t status;		   // Several status registers (Reg 6)
	uint8_t upperT;		   //Higher order temperature bits plus a status (Reg 7)
	uint8_t lowerT;		   //Lower order temperature bits (Reg 8)
	uint8_t rfAGC;		   //RF agc status and value (Reg 9)
	uint8_t ifAGC;		   //IF Agc value (Reg 10)
} AGCLogData;

class HardwareManager
{
  public:
	HardwareManager(string if_outFilename, string conf_filename, bool counterOn);
	virtual ~HardwareManager();

	bool Start();

	bool Stop();

	bool GetAGCData( AGCLogData dataArray[4] );

	void *HWThread(void);
	void *WriterThread(void);

	// This is for launching the HW Thread within the context of this class.
	static void *HWThread_Helper(void *context)
	{
		if (!context)
		{
			return NULL;
		}

		return ((HardwareManager *)context)->HWThread();
	}

	static void *WriterThread_Helper(void *context)
	{
		if (!context)
		{
			return NULL;
		}
		return ((HardwareManager *)context)->WriterThread();
	}

	unsigned long GetBytesWritten() { return m_bytesWritten; }
	bool InitializeHardware();

	bool GetErrorState() { return (m_numErrors > 0); }

  private:
	bool ConfigureNT1065(bool verbose);

	bool ParseNTConfig(volatile uint32_t *SpiWrBuffer);

	bool ConfigureFPGASettings();

	bool EnableAXI_Interrupts();

	unsigned long *mmapDMABuffer(size_t length, unsigned long *phys_address);
	unsigned long *mmapDDR(size_t length, off_t offset, volatile unsigned long **pageStart);

	// User DMA buf management
	void SetSyncOffset(unsigned long offset);
	void SetSyncSize(unsigned int size);
	void SetSyncDir();
	bool SyncForCPU();

	string m_IF_baseName;
	string m_udmaFilename;
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

	/* MMAPED FPGA and DDR Peripherals */
	/* These are virtual addresses (MMAP)*/
	volatile unsigned long *m_ddrBase; // DDR CDMA Target (Virtual)
	volatile unsigned long *m_ddrBaseOff;
	volatile unsigned long *m_cdmaBase; // In FPGA
	volatile unsigned long *m_cdmaBaseOff;
	volatile unsigned long *m_intrBase; // In FPGA
	volatile unsigned long *m_intrBaseOff;
	volatile unsigned long *m_spiBase; // In FPGA
	volatile unsigned long *m_spiRead;
	volatile unsigned long *m_spiWrite;

	unsigned long m_paddr_dmaTarget; // Physical location of DDR CDMA target
};

#endif /* SRC_HARDWAREMANAGER_H_ */
