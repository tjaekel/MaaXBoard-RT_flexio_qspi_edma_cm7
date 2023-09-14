/*
 * SDK example
 * modified: tjaekel, 09/05/2013
 * target: MaaxBoard-RT
 */

#include "fsl_debug_console.h"
#include "fsl_lpspi.h"
#include "fsl_flexio_spi_edma.h"
#include "pin_mux.h"
#include "board.h"
#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
#include "fsl_dmamux.h"
#endif

#include "fsl_common.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*
 * ATTENTION:
 * the debug UART is LPUART6:
 * J1, pin 16 : RxD
 * J1, pin 18 : TxD
 */
/*Master related*/
#define WORD_SIZE_32					/* 8bit or 32bit transfer */
/* it must be set for FLEXIO_QSPI! we configure 4 clock cycles on SCLK! */
#define TRANSFER_SIZE     (32U*4U)    	/*! Transfer dataSize : here multiples of four */
#define TRANSFER_BAUDRATE 10000000U 	/*! Transfer baudrate: 30 MHz is the max., faster is wrong waveform! or
 	 	 	 	 	 	 	 	 	 	    change to 32bit mode: OK with 60 MHz on SPI, on QSPI: max.30 MHz! */
/* it works also for the SPI slave!
 * It depends on the "clock_config.c": CLOCK_Root_Flexio2
 * if you change to 125 MHz: FLEXIO2 uses 120 MHz as max. possible - it results in 60 MHz but not clean waveform!
 * change to 32bit mode and 60 MHz is OK (SCLK: 55.6/62.5 MHz)
 */

#define MASTER_FLEXIO_SPI_BASEADDR (FLEXIO2)
#define FLEXIO_SPI_SOUT_PIN        24U				//MOSI is D0
//24 - consecutive ("parallel") but LPSPI2			//J1, pin 8  : D0
//25												//J1, pin 10 : D1
//26												//J1, pin 29 : D2
//27												//J1, pin 31 : D3
#define FLEXIO_SPI_SIN_PIN         16U				//J1, pin 32	MISO
#define FLEXIO_SPI_CLK_PIN         19U				//J1, pin 13	SCLK
#define FLEXIO_SPI_PCS_PIN         13U				//J1, pin 11	PCS

#define MASTER_FLEXIO_SPI_IRQ FLEXIO2_IRQn

#define MASTER_FLEXIO_SPI_CLOCK_FREQUENCY (CLOCK_GetFreqFromObs(CCM_OBS_FLEXIO2_CLK_ROOT))

#define FLEXIO_DMA_REQUEST_SOURCE_BASE        (kDmaRequestMuxFlexIO2Request0Request1)
#define EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR    DMAMUX0
#define EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR DMA0
#define FLEXIO_SPI_TX_DMA_LPSPI_CHANNEL       (0U)
#define FLEXIO_SPI_RX_DMA_LPSPI_CHANNEL       (1U)
#define FLEXIO_TX_SHIFTER_INDEX               0U
#define FLEXIO_RX_SHIFTER_INDEX               4U
#define EXAMPLE_TX_DMA_SOURCE                 (kDmaRequestMuxFlexIO2Request0Request1)
#define EXAMPLE_RX_DMA_SOURCE                 (kDmaRequestMuxFlexIO2Request2Request3)

/*Slave related*/
#define SLAVE_LPSPI_BASEADDR   (LPSPI2)
#define SLAVE_LPSPI_IRQ_HANDLE (LPSPI2_DriverIRQHandler)
#define SLAVE_LPSPI_IRQN       (LPSPI2_IRQn)

#define SLAVE_LPSPI_PCS_FOR_INIT     (kLPSPI_Pcs0)
#define SLAVE_LPSPI_PCS_FOR_TRANSFER (kLPSPI_SlavePcs0)

/* pins LPSPI2, slave: */
/* SOUT_PIN:	J1, pin 29	MISO
 * SIN_PIN :	J1, pin 31	MOSI
 * CLK_PIN :	J1, pin 08	SCLK
 * PCS_PIN :	J1, pin 10	PCS
 *
 * connect with wires:
 * Master       Slave
 * J1, pin 33 - J1, pin 31 = MOSI - MOSI
 * J1, pin 13 - J1, pin 08 = SCLK
 * J1, pin 11 - J1, pin 10 = PCS
 * J1, pin 32 - J1, pin 29 = MISO - MISO
 */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* LPSPI user callback */
void FLEXIO_SPI_MasterUserCallback(FLEXIO_SPI_Type *base,
                                   flexio_spi_master_edma_handle_t *handle,
                                   status_t status,
                                   void *userData);
void LPSPI_SlaveUserCallback(LPSPI_Type *base, lpspi_slave_handle_t *handle, status_t status, void *userData);

/*******************************************************************************
 * Variables
 ******************************************************************************/
#ifdef WORD_SIZE_32
AT_NONCACHEABLE_SECTION_INIT(uint32_t masterRxData[TRANSFER_SIZE]) = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint32_t masterTxData[TRANSFER_SIZE]) = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint32_t slaveRxData[TRANSFER_SIZE])  = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint32_t slaveTxData[TRANSFER_SIZE])  = {0U};
#else
AT_NONCACHEABLE_SECTION_INIT(uint8_t masterRxData[TRANSFER_SIZE]) = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint8_t masterTxData[TRANSFER_SIZE]) = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint8_t slaveRxData[TRANSFER_SIZE])  = {0U};
AT_NONCACHEABLE_SECTION_INIT(uint8_t slaveTxData[TRANSFER_SIZE])  = {0U};
#endif

FLEXIO_SPI_Type spiDev;
flexio_spi_master_edma_handle_t g_m_handle;
lpspi_slave_handle_t g_s_handle;

edma_handle_t txHandle;
edma_handle_t rxHandle;

volatile bool isSlaveTransferCompleted  = false;
volatile bool isMasterTransferCompleted = false;

/*******************************************************************************
 * Code
 ******************************************************************************/

void FLEXIO_SPI_MasterUserCallback(FLEXIO_SPI_Type *base,
                                   flexio_spi_master_edma_handle_t *handle,
                                   status_t status,
                                   void *userData)
{
    isMasterTransferCompleted = true;
}

void LPSPI_SlaveUserCallback(LPSPI_Type *base, lpspi_slave_handle_t *handle, status_t status, void *userData)
{
    if (status == kStatus_Success)
    {
        __NOP();
    }

    if (status == kStatus_LPSPI_Error)
    {
        __NOP();
    }

    isSlaveTransferCompleted = true;

    PRINTF("This is LPSPI slave call back.\r\n");
}

int main(void)
{
    BOARD_ConfigMPU();
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("FLEXIO Master EDMA - QSPI Master demo\r\n");
    PRINTF("This example use one FLEXIO QSPI as Master on MaaxBoard-RT.\r\n");
    PRINTF("Please make sure you make the correct wire connections. The connection is:\r\n");
    PRINTF("FLEXIO_QSPI_master \r\n");
    PRINTF(" CLK  J1,p13\r\n");
    PRINTF(" PCS  J1,p11\r\n");
    PRINTF(" D0   J1,p8\r\n");
    PRINTF(" D1   J1,p10\r\n");
    PRINTF(" D2   J1,p29\r\n");
    PRINTF(" D3   J1,p31\r\n");

    uint32_t errorCount;
    uint32_t i;
    flexio_spi_master_config_t masterConfig;
    lpspi_slave_config_t slaveConfig;
    flexio_spi_transfer_t masterXfer;
    lpspi_transfer_t slaveXfer;

    edma_config_t config;

    /* Master config */
    FLEXIO_SPI_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = TRANSFER_BAUDRATE;
#ifdef WORD_SIZE_32
    masterConfig.dataMode = kFLEXIO_SPI_32BitMode;
#else
    masterConfig.dataMode = kFLEXIO_SPI_8BitMode;
#endif

    spiDev.flexioBase      = MASTER_FLEXIO_SPI_BASEADDR;
    spiDev.SDOPinIndex     = FLEXIO_SPI_SOUT_PIN;
    spiDev.SDIPinIndex     = FLEXIO_SPI_SIN_PIN;
    spiDev.SCKPinIndex     = FLEXIO_SPI_CLK_PIN;
    spiDev.CSnPinIndex     = FLEXIO_SPI_PCS_PIN;
    spiDev.shifterIndex[0] = FLEXIO_TX_SHIFTER_INDEX;
    spiDev.shifterIndex[1] = FLEXIO_RX_SHIFTER_INDEX;
    spiDev.timerIndex[0]   = 0U;
    spiDev.timerIndex[1]   = 1U;

    FLEXIO_SPI_MasterInit(&spiDev, &masterConfig, MASTER_FLEXIO_SPI_CLOCK_FREQUENCY);

    /*Slave config*/
#ifdef WORD_SIZE_32
    slaveConfig.bitsPerFrame = 32;
#else
    slaveConfig.bitsPerFrame = 8;
#endif
    slaveConfig.cpol         = kLPSPI_ClockPolarityActiveHigh;
    slaveConfig.cpha         = kLPSPI_ClockPhaseFirstEdge;
    slaveConfig.direction    = kLPSPI_MsbFirst;

    slaveConfig.whichPcs           = SLAVE_LPSPI_PCS_FOR_INIT;
    slaveConfig.pcsActiveHighOrLow = kLPSPI_PcsActiveLow;

    slaveConfig.pinCfg        = kLPSPI_SdiInSdoOut;
    slaveConfig.dataOutConfig = kLpspiDataOutRetained;

    LPSPI_SlaveInit(SLAVE_LPSPI_BASEADDR, &slaveConfig);

    /* Set up the transfer data */
    for (i = 0U; i < TRANSFER_SIZE; i++)
    {
        masterTxData[i] = 0xAAAAAA00 + i;		//(i + 0x11) % 256U;
        masterRxData[i] = 0U;

        slaveTxData[i] = ~masterTxData[i];
        slaveRxData[i] = 0U;
    }

    isSlaveTransferCompleted = false;
    /* Set up slave first */
    LPSPI_SlaveTransferCreateHandle(SLAVE_LPSPI_BASEADDR, &g_s_handle, LPSPI_SlaveUserCallback, NULL);

    /*Set slave transfer ready to receive/send data*/
    slaveXfer.txData      = (uint8_t *)slaveTxData;
    slaveXfer.rxData      = (uint8_t *)slaveRxData;
    slaveXfer.dataSize    = TRANSFER_SIZE;
    slaveXfer.configFlags = SLAVE_LPSPI_PCS_FOR_TRANSFER | kLPSPI_SlaveByteSwap;

    LPSPI_SlaveTransferNonBlocking(SLAVE_LPSPI_BASEADDR, &g_s_handle, &slaveXfer);

#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
    /*Init DMAMUX */
    DMAMUX_Init(EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR);
    /* Set channel for FLEXIO */
    DMAMUX_SetSource(EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR, FLEXIO_SPI_TX_DMA_LPSPI_CHANNEL, EXAMPLE_TX_DMA_SOURCE);
    ////DMAMUX_SetSource(EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR, FLEXIO_SPI_RX_DMA_LPSPI_CHANNEL, EXAMPLE_RX_DMA_SOURCE);
    DMAMUX_EnableChannel(EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR, FLEXIO_SPI_TX_DMA_LPSPI_CHANNEL);
    ////DMAMUX_EnableChannel(EXAMPLE_FLEXIO_SPI_DMAMUX_BASEADDR, FLEXIO_SPI_RX_DMA_LPSPI_CHANNEL);
#endif

    /* Init the EDMA module */
    EDMA_GetDefaultConfig(&config);
    EDMA_Init(EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR, &config);
    EDMA_CreateHandle(&txHandle, EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR, FLEXIO_SPI_TX_DMA_LPSPI_CHANNEL);
    ////EDMA_CreateHandle(&rxHandle, EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR, FLEXIO_SPI_RX_DMA_LPSPI_CHANNEL);

#if defined(FSL_FEATURE_EDMA_HAS_CHANNEL_MUX) && FSL_FEATURE_EDMA_HAS_CHANNEL_MUX
    EDMA_SetChannelMux(EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR, FLEXIO_SPI_TX_DMA_LPSPI_CHANNEL, EXAMPLE_TX_DMA_SOURCE);
    ////EDMA_SetChannelMux(EXAMPLE_FLEXIO_SPI_DMA_LPSPI_BASEADDR, FLEXIO_SPI_RX_DMA_LPSPI_CHANNEL, EXAMPLE_RX_DMA_SOURCE);
#endif

    /* Set up master transfer */
    FLEXIO_SPI_MasterTransferCreateHandleEDMA(&spiDev, &g_m_handle, FLEXIO_SPI_MasterUserCallback, NULL, &txHandle,
                                              &rxHandle);

    /*Start master transfer*/
    masterXfer.txData   = (uint8_t *)masterTxData;
    masterXfer.rxData   = (uint8_t *)masterRxData;
    masterXfer.dataSize = TRANSFER_SIZE;
#ifdef WORD_SIZE_32
    masterXfer.flags    = kFLEXIO_SPI_csContinuous | kFLEXIO_SPI_32bitLsb;		//kFLEXIO_SPI_8bitMsb;
#else
    masterXfer.flags    = kFLEXIO_SPI_csContinuous;
#endif

    isMasterTransferCompleted = false;
    FLEXIO_SPI_MasterTransferEDMA(&spiDev, &g_m_handle, &masterXfer);

    /* Wait slave received all data. */
    while (!(isSlaveTransferCompleted && isMasterTransferCompleted))
    {
    }

    errorCount = 0U;
    for (i = 0U; i < TRANSFER_SIZE; i++)
    {
        if (masterTxData[i] != slaveRxData[i])
        {
            errorCount++;
            PRINTF("1: %x | %x\r\n", masterTxData[i], slaveRxData[i]);
        }

        if (slaveTxData[i] != masterRxData[i])
        {
            errorCount++;
            PRINTF("2: %x | %x\r\n", slaveTxData[i], ~masterRxData[i]);
        }
    }
    if (errorCount == 0U)
    {
        PRINTF("FLEXIO SPI master <-> LPSPI slave transfer all data matched!\r\n");
    }
    else
    {
        PRINTF("Error occurred in FLEXIO SPI master <-> LPSPI slave transfer!\r\n");
    }

    FLEXIO_SPI_MasterDeinit(&spiDev);
    LPSPI_Deinit(SLAVE_LPSPI_BASEADDR);

    PRINTF("\r\nEnd of example.\r\n");

    while (1)
    {
    }
}
