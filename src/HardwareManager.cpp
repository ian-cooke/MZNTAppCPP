/*
 * HardwareManager.cpp
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */
//#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <string.h>
#include <sys/mman.h>
#include <queue>
#include <iostream>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <chrono>

#include "HardwareManager.h"
#include "system_defines.h"
#include "ThreadSafeQueue.h"

using namespace std;

HardwareManager::HardwareManager(string if_outFilename, string conf_filename, bool counterOn) : m_dataQueue(300)
{
	// Initialize all member variables to default values.
	m_counterOn = counterOn;
	m_resamplingOn = true;
	m_resampThreshold = 0x04000000;
	m_initialized = false;

	m_AGC_fileName = "/mnt/AGC.bin";
	m_IF_baseName = if_outFilename;

	m_stopSignal = false;
	m_breakFileSignal = false;

	m_ddrBase = m_ddrBaseOff = NULL;
	m_cdmaBase = m_cdmaBaseOff = NULL;
	m_intrBase = m_intrBaseOff = NULL;
	m_paddr_dmaTarget = 0;
	m_spiWrite = m_spiRead = m_spiBase = NULL;

	m_bytesWritten = 0;
	m_numErrors = 0;
	m_writeOn = true;

	m_conf_filename = conf_filename;
}

HardwareManager::~HardwareManager()
{
	// Deallocate the mmaps.
	if (m_ddrBaseOff)
	{
		cout << "Releasing DDR mmap." << endl;
		if (munmap((void *)m_ddrBaseOff, DDR_MAP_SIZE_Q) == -1)
		{
			cerr << "Failed to release mmap to DDR" << endl;
		}
	}

	if (m_cdmaBase)
	{
		cout << "Releasing CDMA mmap." << endl;
		if (munmap((void *)m_ddrBase, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to DDR" << endl;
		}
	}

	if (m_intrBase)
	{
		cout << "Releasing interrupt mmap." << endl;
		if (munmap((void *)m_ddrBase, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to DDR" << endl;
		}
	}

	if (m_spiBase)
	{
		cout << "Releasing SPI base" << endl;
		if (munmap((void *)m_spiBase, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to SPI base" << endl;
		}
	}

	if (m_spiRead)
	{
		cout << "Releasing SPI base" << endl;
		if (munmap((void *)m_spiRead, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to SPI base" << endl;
		}
	}
	if (m_spiWrite)
	{
		cout << "Releasing SPI write" << endl;
		if (munmap((void *)m_spiWrite, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to SPI write" << endl;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
// HW Thread
// requires class initialized
// actions: DMA Management thread loop
// Modifies: m_dataQueue
//-------------------------------------------------------------------------------------------------------------
void *HardwareManager::HWThread()
{
	cout << "In HW thread." << endl;

	bool firstInterrupt = true;
	unsigned long bufferNum = 0;
	volatile unsigned long regValue;
	unsigned long queuePos = DDR_BASE_WRITE_ADDRESS;
	unsigned long queueIndex = 0;
	unsigned long loopCnt = 0;

	while (!m_stopSignal)
	{
		// Busy poll the interrupt.
		do
		{
			//cout << "waiting for interrupt." << endl;
			regValue = *(volatile unsigned long *)((char *)m_intrBaseOff + AXIINTC_ISR_OFFSET);
			usleep(10);
			if (m_stopSignal)
			{
				break;
			}
		} while (!(regValue & BUFINT01_ISR_MASK));

		if (m_stopSignal)
		{
			break;
		}

		// cout << "Conducting transfer." << endl;

		/* Choose which BRAM to tranfer data from */
		if (regValue & BUFINT0_ISR_MASK)
		{
			bufferNum = 0;
			// Acknowledge the interrupt
			*(volatile unsigned long *)((char *)m_intrBaseOff + AXIINTC_IAR_OFFSET) = BUFINT0_ISR_MASK;
		}
		if (regValue & BUFINT1_ISR_MASK)
		{
			bufferNum = 1;
			// Acknowledge the interrupt
			*(volatile unsigned long *)((char *)m_intrBaseOff + AXIINTC_IAR_OFFSET) = BUFINT1_ISR_MASK;
		}

		// Checks for double condition.
		// REQUIREMENT: BUSY POLLING AND ACK INTR SHOULD NEVER RESULT IN THIS CONDITION
		if (BUFINT01_ISR_MASK == (regValue & BUFINT01_ISR_MASK))
		{
			// Both the interrupts have come.. Unexpected except for the first time
			// Acknowledge the interrupt
			*(volatile unsigned long *)((char *)m_intrBaseOff + AXIINTC_IAR_OFFSET) = BUFINT01_ISR_MASK;
			if (firstInterrupt == true)
			{
				firstInterrupt = false;
				continue;
			}
			else
			{
				cerr << "ERROR: HW INTR MISSED." << endl;
				m_numErrors++;
				continue;
				//break;
			}
		}

		// Reset the DMA
		do
		{
			*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET) = XAXICDMA_CR_RESET_MASK;
			/* If the reset bit is still high, then reset is not done */
			regValue = *(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET);
		} while (regValue & XAXICDMA_CR_RESET_MASK);

		// Enable DMA interrupt
		regValue = *(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET);
		regValue = (regValue | XAXICDMA_XR_IRQ_ALL_MASK);
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET) = regValue;

		// Checking for the DMA to go idle
		regValue = *(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_SR_OFFSET);

		if (!(regValue & XAXICDMA_SR_IDLE_MASK))
		{
			cerr << "GT: BUS IS BUSY Error Condition" << endl;
			break;
		}

		// Ensure we don't overwrite the DDR Buffer.
		while (m_dataQueue.size() == DDR_Q_SIZE-2)
		{
			cerr << "Queue full, wait for transfer." << endl;
			//m_numErrors++;
		}

		// Set the source address - this is one of two buffers (hence the bufferNum*BUFFER_BYTESIZE)
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_SRCADDR_OFFSET) = BRAM_BASE_ADDRESS + bufferNum * BUFFER_BYTESIZE;

		// Set the destination address. This is based on the physical address provided by the dmaTarget.
		queuePos = m_paddr_dmaTarget + queueIndex * (unsigned long)BUFFER_BYTESIZE;
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_DSTADDR_OFFSET) = queuePos; //if_data;

		// Write number of bytes to transfer (this initiates the DMA)
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_BTT_OFFSET) = BUFFER_BYTESIZE;

		/*** Wait for the DMA transfer Status ***/
		do
		{
			regValue = *(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_SR_OFFSET);
		} while (!(regValue & XAXICDMA_SR_IDLE_MASK));

		/*** Print DMA transfer status ***/
		if ((regValue & XAXICDMA_XR_IRQ_IOC_MASK))
		{
			//printf("Transfer Completed \n\r ");
		}
		else if ((regValue & XAXICDMA_XR_IRQ_DELAY_MASK))
		{
			cerr << "GT: IRQ Delay Interrupt" << endl;
			m_numErrors++;
			continue;
			//break;
		}
		else if ((regValue & XAXICDMA_XR_IRQ_ERROR_MASK))
		{
			cerr << "GT: Transfer Error Interrupt" << endl;
			m_numErrors++;
			continue;
			//break;
		}

		// Add the pointer to the queue.
		m_dataQueue.push(((char *)m_ddrBaseOff + queueIndex * BUFFER_BYTESIZE));

		// Inc queue index
		queueIndex = (queueIndex + 1) % (DDR_Q_SIZE-2);

		/*if(loopCnt%200 == 0) {
			cout <<"OK" << endl;
		}*/

		loopCnt++;
	}

	cout << "Exiting." << endl;
	cout << "Number of intr serviced: " << (int)loopCnt << endl;
	cout << "Error cnt: " << (int)m_numErrors << endl;
	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
// Writerthread
// requires: class initialized
// actions: Performs continuous polling of shared data queue and writing to file.
// Modifies: m_dataQueue
// returns: true on success, false otherwise.
//-------------------------------------------------------------------------------------------------------------
void *HardwareManager::WriterThread()
{
	cout << "In writer thread." << endl;

	int res = 0;
	struct sched_param params;
	params.sched_priority = 90;

	// Get the curent limit
	struct rlimit limits;
	getrlimit(RLIMIT_RTPRIO, &limits);

	printf("This threads RT PRIO Cur: %ld, Max: %ld\n", limits.rlim_cur, limits.rlim_max);

	res = pthread_setschedparam(pthread_self(), SCHED_RR, &params);

	if (res != 0)
	{
		printf("WARNING: Failed to set thread RT policy and priority.\n");
		printf("Errno: %d\n", errno);
		return NULL;
	}

	// Open the IF data file.
	/*FILE *writeFile = fopen(m_IF_baseName, "wb");
	if (writeFile == NULL)
	{
		cerr << "Could not open the IF data output file." << endl;
		return NULL;
	}*/

	ofstream outputFile;

	// The first open will overwrite any file with this name.
	outputFile.open(m_IF_baseName.c_str(), ios::out | ios::binary);
	static unsigned long lastBytesWritten = 0;
	static unsigned int fileNo = 1;
	unsigned int blockSize = 1 * BUFFER_BYTESIZE;
	static string lastFilename = m_IF_baseName;

	// Set the contiguous buffer we have in RAM to be a DMA target buffer
	// The underlying UDMABUF driver will keep this portion of ram syncd with the cache
	// Before we read from it.
	SetSyncDir();
	SetSyncSize(blockSize);
	auto startTime = chrono::steady_clock::now();
	while (!m_stopSignal)
	{
		if(m_breakFileSignal) {
			// Close the file.
			outputFile.close();
			string newName = m_IF_baseName + "." + to_string(fileNo);
			cout << "Opening new file: " + newName << endl;
			outputFile.open(newName.c_str(), ios::out | ios::binary );
			fileNo++;
			m_breakFileSignal = false;

			// Push the last filename and update.
			// push here
			lastFilename = newName;

			// reset bytes written.
			m_bytesWritten = 0;
			lastBytesWritten = 0;
		}

		// Get the first data.
		char *nextData = m_dataQueue.pop_front();

		// If we get null data stop.
		// This is a stop signal.
		if (nextData == NULL)
		{
			break;
		}

		// Sync all the cache's before we read.
		SetSyncOffset((unsigned long)nextData - (unsigned long)m_ddrBaseOff);
		SyncForCPU();
		if(m_writeOn){
			outputFile.write(nextData, streamsize(blockSize));
			m_bytesWritten += blockSize;
		}

		auto currTime = chrono::steady_clock::now();

		chrono::duration<double> elapsed = currTime - startTime;

		unsigned long bytesDiff = (m_bytesWritten - lastBytesWritten) / 1e6;

		if ((bytesDiff) >= 100)
		{
			cout << "Writer thread average speed: " << ((double)bytesDiff) / (elapsed.count()) << " MB per second." << endl;
			lastBytesWritten = m_bytesWritten;
			startTime = chrono::steady_clock::now();
		}
	}

	//fclose(writeFile);
	outputFile.close();

	cout << "Writer thread exiting." << endl;

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
// GetAGCData
// requires: dataArray is an array of 4 AGCLogData ready to be filled
// actions: Reads AGC data into dataArray
// Modifies: dataArray
// returns: true on success, false otherwise.
//
//-------------------------------------------------------------------------------------------------------------
bool HardwareManager::GetAGCData(AGCLogData dataArray[4])
{
	if (!m_initialized)
	{
		return false;
	}

	// This buffer holds our SPI register values
	volatile uint32_t inputBuffer[49] __attribute__((aligned(64)));

	int channel = 0;
	for (channel = 0; channel < 4; channel++)
	{
		// Wait for not busy
		// This writes to register 5 of the NT1065 over spi via FPGA
		// The Bits of reg are as follows:
		/*0 - temp meas sys execute
			 * 1 - temp meas mode 0 - single 1 -continuous
			 * 2-3 unused
			 *  4-5 channel to be monitored
			 *  6-7 unused
			 */
		*(m_spiWrite + 5) = ((channel << 4) | (0x0 << 1) | (0x1 << 0)) | (0x05 << 8);

		while (*m_spiBase & 0x00000008)
		{
			// Busy wait.
		}

		// Give write instruction instruction
		*m_spiBase = 0x00000002;
		usleep(10);
		*m_spiBase = 0x00000000;

		// Wait for not busy
		while (*m_spiBase & 0x00000008)
		{
			/// Busy wait.
		}

		// Give read instruction
		*m_spiBase = 0x00000004;
		usleep(10);
		*m_spiBase = 0x00000000;

		// Wait for not busy
		while (*m_spiBase & 0x00000008)
		{
			/// Busy wait.
		}

		// Read in the values.
		for (int iter = 0; iter < 11; iter++)
		{
			inputBuffer[iter] = 0x000000FF & *(m_spiRead + iter);
		}

		// Add the values to the data.
		dataArray[channel].ms_truncated = GetBytesWritten();
		dataArray[channel].status = inputBuffer[5];
		dataArray[channel].upperT = inputBuffer[7];
		dataArray[channel].lowerT = inputBuffer[8];
		dataArray[channel].rfAGC = inputBuffer[9];
		dataArray[channel].ifAGC = inputBuffer[10] & 0x1F;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
// InitializeHardware
// requires: Class initialized.
// actions: Initializes the NT and 1065
// Modifies:
// returns: true on success, false otherwise.
//
//-------------------------------------------------------------------------------------------------------------
bool HardwareManager::InitializeHardware()
{
	// Uploads hex settings to NT1065 chip via SPI module located on FPGA
	if (!ConfigureNT1065(false))
	{
		cerr << "Failed to init NT1065 through SPI." << endl;
		return false;
	}

	// Configures resampling, counter
	if (!ConfigureFPGASettings())
	{
		cerr << "Failed to set FPGA settings." << endl;
		return false;
	}

	//MMAP the interrupt controller
	m_intrBaseOff = mmapDDR(MAP_SIZE, AXI_INTC_BASE_ADDRESS, &m_intrBase);
	if (m_intrBaseOff == NULL)
	{
		return false;
	}

	// MMAP the CDMA
	m_cdmaBaseOff = mmapDDR(MAP_SIZE, CDMA_BASE_ADDRESS, &m_cdmaBase);
	if (m_cdmaBaseOff == NULL)
	{
		return false;
	}

	// Check the DMA Mode and switch it to simple mode
	unsigned long regValue;
	regValue = *(unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET);
	if ((regValue & XAXICDMA_CR_SGMODE_MASK))
	{
		regValue = (regValue & (~XAXICDMA_CR_SGMODE_MASK));
		*(unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_CR_OFFSET) = regValue;
	}

	// MMAP the SPI Peripherals
	m_spiWrite = mmapDDR(MAP_SIZE, XPAR_NT1065_SPI_WR_SPI_MEM_CTRL_S_AXI_BASEADDR, NULL);
	if (m_spiWrite == NULL)
	{
		return false;
	}

	m_spiBase = mmapDDR(MAP_SIZE, XPAR_NT1065_SPI_NT1065_SPI_DRIVER_V1_0_0_BASEADDR, NULL);
	if (m_spiBase == NULL)
	{
		return false;
	}

	m_spiRead = mmapDDR(MAP_SIZE, XPAR_NT1065_SPI_RD_SPI_MEM_CTRL_S_AXI_BASEADDR, NULL);
	if (m_spiRead == NULL)
	{
		return false;
	}
	// MMAP the DDR
	// Memory map the DDR to the allocated kernel module
	// WRITER and gen thread share this address, so w e can't move this initialization to the gen thread?
	m_ddrBaseOff = mmapDMABuffer(DDR_MAP_SIZE_Q, &m_paddr_dmaTarget);
	if (m_ddrBaseOff == NULL)
	{
		return false;
	}

	cout << "Physical Address target: " << hex << (unsigned long)m_paddr_dmaTarget << endl;

	// This turns on interrupts from the FPGA
	if (!EnableAXI_Interrupts())
	{
		cerr << "Failed to turn on AXI interrupts." << endl;
		return false;
	}

	m_initialized = true;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
// Begin
// requires initialization complete
// actions: Starts the DMA management thread
// Modifies: thread
// returns: true on success, false otherwise.
//-------------------------------------------------------------------------------------------------------------
bool HardwareManager::Start()
{

	pthread_create(&m_hwThread, NULL, HardwareManager::HWThread_Helper, (void *)this);

	pthread_create(&m_writerThread, NULL, HardwareManager::WriterThread_Helper, (void *)this);

	cout << "Created HW and Writer thread." << endl;

	return true;
}

bool HardwareManager::Stop()
{
	cout << "Stoping thread." << endl;
	m_stopSignal = true;

	// Join the hardware thread first.
	// This could fail if the writer thread is not pulling data
	// out of the queue (hw thread is sleeping)
	while (m_dataQueue.size() >= 300)
	{
		m_dataQueue.pop_front();
	}

	cout << "Waiting for HW thread." << endl;

	pthread_join(m_hwThread, NULL);

	cout << "HW Thread stopped." << endl;

	// Join the writer thread.
	// Push NULL to to tell it no more data is coming.
	m_dataQueue.push(NULL);
	pthread_join(m_writerThread, NULL);

	cout << " Writer thread stopped." << endl;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
// parse_conf
// requires m_conf_filename to be set
// actions: fills the SpiWrBuffer with required information based on the
//					file conf_file
// Modifies: SpiWrBuffer
// returns: 0 on success, -1 otherwise.
//
// This legacy C function written by Sean should be re-done using C++ strings
//-------------------------------------------------------------------------------------------------------------
bool HardwareManager::ParseNTConfig(volatile uint32_t *SpiWrBuffer)
{
	// require our configuration filename to be set.
	if (m_conf_filename == "")
	{
		return false;
	}

	char *line = NULL;
	char *line2 = NULL;
	char *val;
	char *reg;
	char *end;
	size_t len = 0;
	ssize_t read;
	uint8_t regnum, valnum;
	uint32_t i = 0;

	FILE *conf_file;
	conf_file = fopen(m_conf_filename.c_str(), "rb");
	if (conf_file == NULL)
	{
		fprintf(stderr, "Failed to open configuration file %s\n", m_conf_filename.c_str());
		return false;
	}

	while ((read = getline(&line, &len, conf_file)) != -1)
	{
		if (line[0] == ';')
		{
			continue; //Comments have a ; at the beginning of the line
		}
		line2 = line;
		reg = strsep(&line2, "\t ");
		regnum = strtol(&reg[3], &end, 10); //Format is REGXX where XX is our register number
		val = strsep(&line2, "\t ");

		end = NULL;
		valnum = strtol(val, &end, 16);
		if (i > 40)
		{
			fprintf(stderr, "SPI buffer overfilled, rework this\n");
			break;
		}
		SpiWrBuffer[i] = 0 | regnum << 8 | valnum; // For the SPI write buffer the values should be 0000XXYY Where XX is the register # and YY is the new value
		i++;
	}

	free(line);

	fclose(conf_file);

	return true;
}

//-----------------------------------------------------------------------------
// configure_nt1065
// Requires: m_conf_filename
// Requires: well formed conf_filename for configuration file
// You can pass 0 or 1 to "verbose", it just changes the amount of printf's
// Modifies: Writes via SPI to the NT1065
// Returns: true on success
//-----------------------------------------------------------------------------
bool HardwareManager::ConfigureNT1065(bool verbose)
{
	if (m_conf_filename == "")
	{
		return false;
	}

	int memfd;
	volatile uint32_t *mapped_base, *mapped_spi_base;
	off_t dev_base = XPAR_NT1065_SPI_NT1065_SPI_DRIVER_V1_0_0_BASEADDR;
	uint32_t RegValue;

	int memfd_1;
	volatile uint32_t *mapped_base_1, *mapped_spi_write;
	off_t dev_base_1 = XPAR_NT1065_SPI_WR_SPI_MEM_CTRL_S_AXI_BASEADDR;

	int memfd_2;
	volatile uint32_t *mapped_base_2, *mapped_spi_read;
	off_t dev_base_2 = XPAR_NT1065_SPI_RD_SPI_MEM_CTRL_S_AXI_BASEADDR;

	int k;

	volatile uint32_t SpiWrBuffer[41] __attribute__((aligned(64)));
	volatile uint32_t SpiRdBuffer[49] __attribute__((aligned(64)));
	uint8_t reg;
	uint8_t val;
	/****************/
	/*** Mappings ***/
	/****************/
	/* Map: mapped_spi_write */
	/**************************/
	memfd_1 = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd_1 == -1)
	{
		printf("Can't open /dev/mem.\n");
		exit(0);
	}
	mapped_base_1 = (uint32_t *)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd_1, dev_base_1 & ~MAP_MASK);
	if (mapped_base_1 == (void *)-1)
	{
		printf("Can't map the memory(spi_base_1) to user space.\n");
		exit(0);
	}
	mapped_spi_write = mapped_base_1 + (dev_base_1 & MAP_MASK);

	/* Map: mapped_dev_base */
	/************************/
	memfd = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd == -1)
	{
		printf("Can't open /dev/mem.\n");
		exit(0);
	}
	mapped_base = (uint32_t *)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, dev_base & ~MAP_MASK);
	if (mapped_base == (void *)-1)
	{
		printf("Can't map the memory(spi_base) to user space.\n");
		exit(0);
	}
	mapped_spi_base = mapped_base + (dev_base & MAP_MASK);

	/* Map: mapped_spi_read */
	/**************************/
	memfd_2 = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd_2 == -1)
	{
		printf("Can't open /dev/mem.\n");
		return false;
	}

	mapped_base_2 = (uint32_t *)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd_2, dev_base_2 & ~MAP_MASK);
	if (mapped_base_2 == (void *)-1)
	{
		printf("Can't map the memory(spi_base_2) to user space.\n");
		return false;
	}
	mapped_spi_read = mapped_base_2 + (dev_base_2 & MAP_MASK);

	if (ParseNTConfig(SpiWrBuffer) == false)
	{
		return false;
	}
	// Copy from PS memory to SPI driver's memory
	for (k = 0; k < 41; k++)
	{
		*(mapped_spi_write + k) = SpiWrBuffer[k];
	}

	//Test Write
	// Check Busy and wait till busy goes low
	do
	{
		RegValue = *(volatile uint32_t *)mapped_spi_base;
	} while (0x00000008 & RegValue);

	// Give Write instruction
	*mapped_spi_base = 0x00000002;
	usleep(100);
	printf("---Programming RF---\n");
	*mapped_spi_base = 0x00000000;

	// Wait till Busy goes low
	do
	{
		RegValue = *(volatile uint32_t *)mapped_spi_base;
	} while (0x00000008 & RegValue);

	// Check Busy and wait till busy goes low
	do
	{
		RegValue = *(volatile uint32_t *)mapped_spi_base;
	} while (0x00000008 & RegValue);

	*mapped_spi_base = 0x00000004;
	printf("---Reading RF---\n");
	usleep(100);
	*mapped_spi_base = 0x00000000;

	// Wait till Busy goes low
	do
	{
		RegValue = *(volatile uint32_t *)mapped_spi_base;
	} while (0x00000008 & RegValue);

	// Copy the read data
	for (k = 0; k < 49; k++)
	{
		SpiRdBuffer[k] = 0x000000FF & *(mapped_spi_read + k);
	}
	// Check the read data
	for (k = 0; k < 41; k++)
	{
		reg = (SpiWrBuffer[k] & 0x0000FF00) >> 8;
		val = (SpiWrBuffer[k] & 0x000000FF);
		if (verbose)
		{
			fprintf(stderr, "WR reg %d, val %d || ", reg, val);
			fprintf(stderr, "RD reg %d, val %d\n", reg, SpiRdBuffer[reg]);
		}
		if (val != SpiRdBuffer[reg])
		{
			fprintf(stderr, "Warning: register %d on NT1065 initialization differ: wr: %d, rd: %d\n", reg, val, SpiRdBuffer[reg]);
		}
	}
	/******************/
	/*** Unmappings ***/
	/******************/

	/* Unmap: mapped_spi_write */
	/****************************/
	if (munmap((void *)mapped_base_1, MAP_SIZE) == -1)
	{
		printf("Can't unmap SPI memory buffer.\n");
		return false;
	}

	close(memfd_1);

	/* Unmap: mapped_base_2 */
	/************************/
	if (munmap((void *)mapped_base_2, MAP_SIZE) == -1)
	{
		printf("Can't unmap SPI memory buffer.\n");
		return false;
	}

	close(memfd_2);

	/* Unmap: mapped_base */
	/**********************/
	if (munmap((void *)mapped_base, MAP_SIZE) == -1)
	{
		printf("Can't unmap SPI memory buffer.\n");
		return false;
	}
	close(memfd);

	return true;
}

//-----------------------------------------------------------------------------
// configure_fpga
// Configures some FPGA settings via GPIO0
// Requires: threshold is between 0 and 0x05000000
// resampling: 1 means resampled, 0 means not resampled.
// Modifies: GPIO
// Returns: 0 on success, -1 otherwise.
//-----------------------------------------------------------------------------
bool HardwareManager::ConfigureFPGASettings()
{
	int32_t memfd;
	volatile uint32_t *mapped_base, *mapped_ctr_base;
	off_t dev_base = AXI_GPIO0_BASE_ADDRESS;

	memfd = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd == -1)
	{
		printf("FAIL: Can't open /dev/mem.\n");
		return false;
	}
	mapped_base =
		(uint32_t *)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd,
						 dev_base & ~MAP_MASK);
	if (mapped_base == (void *)-1)
	{
		printf("FAIL: Can't map the memory(spi_base) to user space.\n");
		return false;
	}
	mapped_ctr_base = mapped_base + (dev_base & MAP_MASK);

	*((mapped_ctr_base + AXI_GPIO0_REG1_OFFSET)) = m_resampThreshold;

	if (m_counterOn == true)
	{
		printf("Setting counter to ON.\n");
		if (m_resamplingOn == true)
		{
			*((mapped_ctr_base + AXI_GPIO0_REG2_OFFSET)) = 0x00000002; // Turn on counter mode w/o resampling 0b10
		}
		else
		{
			*((mapped_ctr_base + AXI_GPIO0_REG2_OFFSET)) = 0x00000003; // Turn on counter mode w/ resambling 0b11
		}
	}
	else
	{
		if (m_resamplingOn == true)
		{
			*((mapped_ctr_base + AXI_GPIO0_REG2_OFFSET)) = 0x00000000; //Ensure that the counter is off w/o resampling 0b00
		}
		else
		{
			*((mapped_ctr_base + AXI_GPIO0_REG2_OFFSET)) = 0x00000001; // Ensure counter off w/ resampling 0b01
		}
	}

	if (munmap((void *)mapped_base, MAP_SIZE) == -1)
	{
		printf("FAIL: Can't unmap memory from user space.\n");
		return false;
	}
	close(memfd);
	//printf("Finished configuring the FPGA\n");
	fflush(stdout);

	return true;
}

//-----------------------------------------------------------------------------
// mmapDDR
// Requries: length is a page multiple, offset is the addrss you want mapped.
// Modifies: stores start of mapped page in pageStart (so you can unmap later)
// Returns: pointer to actual start of where you wanted (offset), NULL if failed
//-----------------------------------------------------------------------------
unsigned long *HardwareManager::mmapDDR(size_t length, off_t offset, volatile unsigned long **pageStart)
{
	//void *mapped_base;
	volatile unsigned long *mapped_offset;
	int ddrFile;
	void *base;

	/* Map: mapped_dev_base */
	/************************/
	ddrFile = open("/dev/mem", O_RDWR | O_SYNC);
	if (ddrFile == -1)
	{
		//printf("Can't open /dev/mem.\n");
		//exit(0);
		//UniError("Failed to open /dev/mem.");
		return NULL;
	}
	base = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED,
				ddrFile, offset & ~(length - 1));

	if (base == MAP_FAILED)
	{
		//printf("Can't map the memory(dev_base) to user space.\n");
		//exit(0);
		printf("Paged start is %p \n", *pageStart);
		//UniError("mmap failed.");
		return NULL;
	}
	mapped_offset = (unsigned long *)(((char *)base) + (offset & (length - 1)));

	if (pageStart)
	{
		*pageStart = (unsigned long *)base;
	}

	// NOTE: CLOSING THE FILE DESCRIPTOR DOES NOT UNMAP THE REGION!
	close(ddrFile);

	return (unsigned long *)mapped_offset;
}

/*
 *  mmapDAMBuf
 *  This mmaps the contiguous user dma buffer created at startup.
 *  it returns the virtual address for the memory map.
 *  it stores the physical address in the pointer phys_address
 *  Note: when calling this funciton, phys_address should just be a reference
 *  to an unsigned long, not a reference to a pointer. The point is to store the physical
 *  address of the buffer as a number to be used by the Generate thread to control the CDMA destination.
 */
unsigned long *HardwareManager::mmapDMABuffer(size_t length, unsigned long *phys_address)
{
	//void *mapped_base;
	volatile unsigned long *mapped_offset;
	int ddrFile, statFile;

	/* Map: mapped_dev_base */
	/************************/
	ddrFile = open("/dev/udmabuf0", O_RDWR);
	if (ddrFile == -1)
	{
		return NULL;
	}
	mapped_offset = (unsigned long *)mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, ddrFile, 0);

	if (mapped_offset == MAP_FAILED)
	{
		return NULL;
	}
	// NOTE: CLOSING THE FILE DESCRIPTOR DOES NOT UNMAP THE REGION!
	close(ddrFile);

	// We need to get the phyical address of the dma buffer too.
	char attr[1024];
	statFile = open("/sys/class/udmabuf/udmabuf0/phys_addr", O_RDONLY);
	if (statFile == -1)
	{
		return NULL;
	}
	read(statFile, attr, 1024);
	sscanf(attr, "%x", (unsigned int *)phys_address);
	close(statFile);

	return (unsigned long *)mapped_offset;
}

//-----------------------------------------------------------------------------
// Initializes the Interrupts
// Requires: /dev/mem be accessible to current process
// - verbose != 0 if you want debug output
// Modifies:
// - Send commands to the interrupt controller via /dev/mem mmap
// Returns: 0 on successful initialization, -1 otherwise
//-----------------------------------------------------------------------------
bool HardwareManager::EnableAXI_Interrupts()
{
	//int res;
	volatile unsigned long *pageStart;
	volatile unsigned long *intOffset;

	intOffset = (unsigned long *)mmapDDR(MAP_SIZE, AXI_INTC_BASE_ADDRESS, &pageStart);
	if (intOffset == NULL)
	{
		cerr << "Failed to memeory map for interrupt initialization" << endl;
		return false;
	}

	//RegValue = BUFINT01_SIE_MASK;
	*(unsigned long *)((char *)intOffset + AXIINTC_SIE_OFFSET) = BUFINT01_SIE_MASK;

	//RegValue = BUFINT_MER_MASK;
	*(unsigned long *)((char *)intOffset + AXIINTC_MER_OFFSET) = BUFINT_MER_MASK;

	if (pageStart)
	{
		if (munmap((void *)pageStart, MAP_SIZE) == -1)
		{
			cerr << "Failed to release mmap to DDR" << endl;
			return false;
		}
	}

	cout << "AXI Interrupts Initialized." << endl;

	return true;
}

/*
 * These are helper functions for the UDMABUF management
 * They are used in the writer for ensuring cache is synchronized.
 */
void HardwareManager::SetSyncOffset(unsigned long offset)
{
	static int fd = 0;
	static char attr[1024];
	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_offset", O_WRONLY)) != -1)
	{
		sprintf(attr, "%lu", offset);
		write(fd, attr, strlen(attr));
		close(fd);
	}
}

void HardwareManager::SetSyncSize(unsigned int size)
{
	static int fd = 0;
	static char attr[1024];
	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_size", O_WRONLY)) != -1)
	{
		sprintf(attr, "%u", size);
		write(fd, attr, strlen(attr));
		close(fd);
	}
}

void HardwareManager::SetSyncDir()
{
	static int fd = 0;
	static char attr[1024];
	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_direction", O_WRONLY)) != -1)
	{
		sprintf(attr, "%d", 2);
		write(fd, attr, strlen(attr));
		close(fd);
	}
}

bool HardwareManager::SyncForCPU()
{
	static int fd = 0;
	string one = "1";
	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_for_cpu", O_WRONLY)) != -1)
	{
		write(fd, one.c_str(), one.length());
		close(fd);
		return true;
	}
	else
	{
		return false;
	}
}
