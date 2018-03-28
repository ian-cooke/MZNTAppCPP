#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#define CDMA_BASE_ADDRESS     0x4E200000
#define AXI_INTC_BASE_ADDRESS 0x41800000
#define GPIO_DATA_OFFSET      0x00000000
#define GPIO_BIT0_MASK      0x00000001
#define GPIO_BIT1_MASK      0x00000002

#define AXI_GPIO0_BASE_ADDRESS 0x41200000 /**< Register for threshold */
#define AXI_GPIO0_REG1_OFFSET 0x00000000 /**< Threshold register; 0 - 1 bit Nonzero means 2 bit */
#define AXI_GPIO0_REG2_OFFSET 0x00000002 /**< Bit0: 0 -resampling 1- no resampling; Bit1: 0 - RF, 1- Counter, 64 bit offset*/

#define DDR_BASE_WRITE_ADDRESS    0x08000000

#define XAXICDMA_CR_OFFSET    	0x00000000  /**< Control register */
#define XAXICDMA_SR_OFFSET    	0x00000004  /**< Status register */
#define XAXICDMA_CDESC_OFFSET 	0x00000008  /**< Current descriptor pointer */
#define XAXICDMA_TDESC_OFFSET	0x00000010  /**< Tail descriptor pointer */
#define XAXICDMA_SRCADDR_OFFSET 0x00000018  /**< Source address register */
#define XAXICDMA_DSTADDR_OFFSET 0x00000020  /**< Destination address register */
#define XAXICDMA_BTT_OFFSET     0x00000028  /**< Bytes to transfer */

#define XAXICDMA_CR_RESET_MASK	0x00000004 /**< Reset DMA engine */
#define XAXICDMA_CR_SGMODE_MASK	0x00000008 /**< Scatter gather mode */

#define XAXICDMA_XR_IRQ_IOC_MASK	0x00001000 /**< Completion interrupt */
#define XAXICDMA_XR_IRQ_DELAY_MASK	0x00002000 /**< Delay interrupt */
#define XAXICDMA_XR_IRQ_ERROR_MASK	0x00004000 /**< Error interrupt */
#define XAXICDMA_XR_IRQ_ALL_MASK	0x00007000 /**< All interrupts */
#define XAXICDMA_XR_IRQ_SIMPLE_ALL_MASK	0x00005000 /**< All interrupts for
						     simple only mode */
#define XAXICDMA_SR_IDLE_MASK         0x00000002  /**< DMA channel idle */
#define XAXICDMA_SR_SGINCLD_MASK      0x00000008  /**< Hybrid build */
#define XAXICDMA_SR_ERR_INTERNAL_MASK 0x00000010  /**< Datamover internal err */
#define XAXICDMA_SR_ERR_SLAVE_MASK    0x00000020  /**< Datamover slave err */
#define XAXICDMA_SR_ERR_DECODE_MASK   0x00000040  /**< Datamover decode err */
#define XAXICDMA_SR_ERR_SG_INT_MASK   0x00000100  /**< SG internal err */
#define XAXICDMA_SR_ERR_SG_SLV_MASK   0x00000200  /**< SG slave err */
#define XAXICDMA_SR_ERR_SG_DEC_MASK   0x00000400  /**< SG decode err */
#define XAXICDMA_SR_ERR_ALL_MASK      0x00000770  /**< All errors */

#define BUFFER_BYTESIZE 262144	// Length of the buffers for DMA transfer

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

//#define DDR_Q_SIZE 1900
#define DDR_Q_SIZE 300
#define DDR_MAP_SIZE_Q DDR_Q_SIZE*BUFFER_BYTESIZE /* = 40 * 256k ~10.5mb */

#define BRAM_BASE_ADDRESS    0xC0000000
#define BRAM_MAP_MASK (BRAM_MAP_SIZE - 1)

#define OUTFNAME "/mnt/IFdata.bin"
#define OUTDATATYPE unsigned char
#define OUTDATASIZE sizeof(OUTDATATYPE)

/* these offsets are in bytes*/
#define AXIINTC_ISR_OFFSET    	0x00000000  /**< Interrupt Status Register */
#define AXIINTC_IPR_OFFSET    	0x00000004  /**< Interrupt Pending Register */
#define AXIINTC_IER_OFFSET    	0x00000008  /**< Interrupt Enable Register */
#define AXIINTC_IAR_OFFSET    	0x0000000C  /**< Interrupt Acknowledge Register */
#define AXIINTC_SIE_OFFSET 		0x00000010  /**< Set Interrupt Enable Register */
#define AXIINTC_CIE_OFFSET 		0x00000014  /**< Clear Interrupt Enable Register */
#define AXIINTC_IVR_OFFSET 		0x00000018  /**<  Interrupt Vector Register */
#define AXIINTC_MER_OFFSET 		0x0000001C  /**< Master Enable Register */
#define AXIINTC_IMR_OFFSET 		0x00000020  /**< Interrupt Mode Register */
#define AXIINTC_ILR_OFFSET 		0x00000024  /**< Interrupt Level Register */

#define BUFINT0_ISR_MASK			0x00000001
#define BUFINT0_IER_MASK			0x00000001
#define BUFINT0_SIE_MASK			0x00000001
#define BUFINT1_ISR_MASK			0x00000002
#define BUFINT1_IER_MASK			0x00000002
#define BUFINT1_SIE_MASK			0x00000002

#define BUFINT01_ISR_MASK			0x00000003
#define BUFINT01_IER_MASK			0x00000003
#define BUFINT01_SIE_MASK			0x00000003

#define BUFINT_MER_MASK             0x00000003

#define MPCORE_BASE_ADDRESS			0xF8F00000
#define GIC_CNTR_OFFSET     		0x00000100
#define GIC_CNTR_EN_NS_MASK			0x00000002
#define GIC_CNTR_EN_S_MASK			0x00000001

#define GIC_DIST_EN_OFFSET     		0x00001000	// Write 0x00000003 to enable non-secure interrupts
#define GIC_DIST_EN_NSINT_MASK   	0x00000002	// program 1 to enable
#define GIC_DIST_EN_SINT_MASK   	0x00000001	// program 1 to enable

#define GIC_DIST_TYPE_OFFSET     	0x00001004	// Read only
#define GIC_DIST_EN_NSINT_MASK   	0x00000002
#define GIC_DIST_EN_SINT_MASK   	0x00000001

#define GIC_SPI_CPU_OFFSET			0X0000183C	// ICDIPTR15
#define GIC_SPI_CPU_IID62_MASK		0x00030000	// program 3 to target both processors
#define GIC_SPI_CPU_IID61_MASK		0x00000300	// program 3 to target both processors

#define GIC_INT_CFG_OFFSET			0X00001C0C	//ICDICFR3
#define GIC_INT_CFG_IID62_MASK		0x30000000	// program 3 to configure rising edge
#define GIC_INT_CFG_IID61_MASK		0x0C000000	// program 3 to configure rising edge

#define GIC_SPI_STAT_OFFSET			0X00001D04	//SPI_Status_0 read only
#define GIC_SPI_IID62_MASK			0x40000000
#define GIC_INT_IID61_MASK			0x20000000


/* Definitions for peripheral NT1065_SPI_RD_SPI_MEM_CTRL */
#define XPAR_NT1065_SPI_RD_SPI_MEM_CTRL_S_AXI_BASEADDR 0x40000000
#define XPAR_NT1065_SPI_RD_SPI_MEM_CTRL_S_AXI_HIGHADDR 0x40000FFF

/* Definitions for peripheral NT1065_SPI_WR_SPI_MEM_CTRL */
#define XPAR_NT1065_SPI_WR_SPI_MEM_CTRL_S_AXI_BASEADDR 0x40010000
#define XPAR_NT1065_SPI_WR_SPI_MEM_CTRL_S_AXI_HIGHADDR 0x42000FFF

/* Definitions for peripheral NT1065_SPI_NT1065_SPI_DRIVER_V1_0_0 */
#define XPAR_NT1065_SPI_NT1065_SPI_DRIVER_V1_0_0_BASEADDR 0x43C00000
#define XPAR_NT1065_SPI_NT1065_SPI_DRIVER_V1_0_0_HIGHADDR 0x43C00FFF

#endif

