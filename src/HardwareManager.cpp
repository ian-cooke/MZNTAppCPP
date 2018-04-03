/*
 * HardwareManager.cpp
 *
 *  Created on: Mar 14, 2018
 *      Author: chris
 */
#include <cstdio>
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

#include "HardwareManager.h"
#include "system_defines.h"
#include "ThreadSafeQueue.h"

using namespace std;

HardwareManager::HardwareManager( string if_outFilename ) : m_dataQueue(300)
{
	// Initialize all member variables to default values.
	m_conf_filename = "/etc/ConfigSet10.txt";
	m_counterOn = false;
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

	m_bytesWritten = 0;
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
	unsigned long errorCnt = 0;

	while (!m_stopSignal)
	{
		// Busy poll the interrupt.
		do
		{
			regValue = *(volatile unsigned long *)((char *)m_intrBaseOff + AXIINTC_ISR_OFFSET);
			usleep(10);
		} while (!(regValue & BUFINT01_ISR_MASK));

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
				errorCnt++;
				//continue;
				break;
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
			printf("GT: BUS IS BUSY Error Condition \n\r");
			break;
		}

		// Ensure we don't overwrite the DDR Buffer.
		while (m_dataQueue.size() == 300)
		{
			cout << "Queue full, wait for transfer." << endl;
		}

		// Set the source address - this is one of two buffers (hence the bufferNum*BUFFER_BYTESIZE)
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_SRCADDR_OFFSET) = BRAM_BASE_ADDRESS + bufferNum * BUFFER_BYTESIZE;

		// Set the destination address. This is based on the physical address provided by the dmaTarget.
		queuePos = (unsigned long)m_ddrBaseOff + queueIndex * (unsigned long)BUFFER_BYTESIZE;
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_DSTADDR_OFFSET) = queuePos; //if_data;

		// Write number of bytes to transfer (this initiates the DMA)
		*(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_BTT_OFFSET) = BUFFER_BYTESIZE;

		/*** Wait for the DMA transfer Status ***/
		do
		{
			regValue = *(volatile unsigned long *)((char *)m_cdmaBaseOff + XAXICDMA_SR_OFFSET);
		} while (!(regValue & XAXICDMA_SR_IDLE_MASK));

		// Add the pointer to the queue.
		m_dataQueue.push(((char *)m_ddrBaseOff + queueIndex * BUFFER_BYTESIZE));

		// Inc queue index
		queueIndex = (queueIndex + 1) % DDR_Q_SIZE;
	}

	cout << "Exiting." << endl;
	cout << "Error cnt: " << errorCnt << endl;
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

	// Open the IF data file.
	/*FILE *writeFile = fopen(m_IF_baseName, "wb");
	if (writeFile == NULL)
	{
		cerr << "Could not open the IF data output file." << endl;
		return NULL;
	}*/

	ofstream outputFile;
	outputFile.open( m_IF_baseName.c_str(), ios::out | ios::binary );

	while (!m_stopSignal)
	{

		// Get the first data.
		char *nextData = m_dataQueue.pop_front();

		// If we get null data stop.
		// This is a stop signal.
		if (nextData == NULL)
		{
			break;
		}

		outputFile.write(nextData, BUFFER_BYTESIZE);
		m_bytesWritten += BUFFER_BYTESIZE;
	}

	//fclose(writeFile);
	outputFile.close();

	cout << "Writer thread exiting." << endl;

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
// parse_conf
// requires conf_filename is a valid string path to config file
// actions: Initializes the NT and 1065
// Modifies: SpiWrBuffer
// returns: true on success, false otherwise.
//
//-------------------------------------------------------------------------------------------------------------
bool HardwareManager::InitializeHardware(const char *conf_filename)
{
	m_conf_filename = conf_filename;

	// Uploads hex settings to NT1065 chip via SPI module located on FPGA
	if (!ConfigureNT1065(false))
	{
		return false;
	}

	// Configures resampling, counter
	if (!ConfigureFPGASettings())
	{
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

	// MMAP the DDR
	// Memory map the DDR to the allocated kernel module
	// WRITER and gen thread share this address, so w e can't move this initialization to the gen thread?
	m_ddrBaseOff = mmapDMABuffer(DDR_MAP_SIZE_Q, &m_paddr_dmaTarget);
	if (m_ddrBaseOff == NULL)
	{
		return false;
	}

	// This turns on interrupts from the FPGA
	if (!EnableAXI_Interrupts())
	{
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
bool HardwareManager::Begin()
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
	// out of the queue.
	if (m_dataQueue.size() == 300)
	{
		m_dataQueue.pop_front();
	}
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
	if (!m_conf_filename)
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
	conf_file = fopen(m_conf_filename, "rb");
	if (conf_file == NULL)
	{
		fprintf(stderr, "Failed to open configuration file %s\n", m_conf_filename);
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
	if (!m_conf_filename)
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
		exit(0);
	}

	mapped_base_2 = (uint32_t *)mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd_2, dev_base_2 & ~MAP_MASK);
	if (mapped_base_2 == (void *)-1)
	{
		printf("Can't map the memory(spi_base_2) to user space.\n");
		exit(0);
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
	usleep(10);
	//printf("---Programming RF---\n");
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
	//printf("---Reading RF---\n");
	usleep(10);
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
		return -1;
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
		if (m_resamplingOn == false)
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
		}
	}

	cout << "AXI Interrupts Initialized." << endl;

	return true;
}
