/**
 * @file spi.c
 *
 * @brief spi functionality
 * 
 * NOTE: only SPI1 is currently supported. if more buses should be supported this driver will need a rework
 * TODO: add timeouts (RTOS-proof)
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "spi.h"
#include "gpio.h"

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_dma.h>
#include <stm32g0xx_ll_spi.h>
#include <stm32g0xx_ll_rcc.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define CRC16_POLY_CCITT  (0x1021)     // CCITT, used by ADE9178

#define SPI_DUMMY_BYTE 0x00

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SPI_TypeDef *g_pSPIPeripheral = NULL;
static spiStatusCallback g_fnSpiDMACallback = NULL;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

static uint8_t spiRxTx(uint8_t);

//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief initialize SPI bus
 * 
 * @param pSPIPeripheral the SPI bus to use
 */
void spiInit(SPI_TypeDef *pSPIPeripheral)
{
    LL_SPI_InitTypeDef SPI_InitStruct = {0};

    if (pSPIPeripheral == SPI1)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
        NVIC_EnableIRQ(SPI1_IRQn);
    }
    else if (pSPIPeripheral == SPI2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);
        NVIC_EnableIRQ(SPI2_IRQn);
    }

    // init SPI, mode 3 (CPOL=1, CPHA=1)
    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode              = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity     = LL_SPI_POLARITY_HIGH;
    SPI_InitStruct.ClockPhase        = LL_SPI_PHASE_2EDGE;
    SPI_InitStruct.NSS               = LL_SPI_NSS_SOFT;
    SPI_InitStruct.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV32;
    SPI_InitStruct.BitOrder          = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly           = CRC16_POLY_CCITT;

    LL_SPI_Init(pSPIPeripheral, &SPI_InitStruct);
    LL_SPI_SetCRCWidth(pSPIPeripheral, LL_SPI_CRC_16BIT);
    LL_SPI_DisableNSSPulseMgt(pSPIPeripheral);
    LL_SPI_SetStandard(pSPIPeripheral, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_Enable(pSPIPeripheral);

    g_pSPIPeripheral = pSPIPeripheral;
}

/**
 * @brief write data over SPI bus
 * 
 * @param pData pointer to uint8 array of data
 * @param count number of bytes to write
 * 
 * @note will transmit hw generated CRC after pData, if enabled
 */
void spiWriteData(const uint8_t *pData, size_t count)
{
    while (count--)
    {
        (void)spiRxTx(*pData++);
    }

    while (LL_SPI_IsActiveFlag_BSY(g_pSPIPeripheral));
}

/**
 * @brief read data from SPI bus
 * 
 * @param pData pointer to destination array
 * @param count number of bytes to read
 */
void spiReadData(uint8_t *pData, size_t count)
{
    while (count--)
    {
        *pData++ = spiRxTx(SPI_DUMMY_BYTE);
    }

    while (LL_SPI_IsActiveFlag_BSY(g_pSPIPeripheral));
}

void spiSendCommand(const uint8_t *pData, size_t count)
{
    toggleDisplayDataCommand(true);
    spiWriteData(pData, count);
    toggleDisplayDataCommand(false);
}

void spiInitDisplayDMA(spiStatusCallback dmaStatusCb)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_SPI2_TX);
    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_BYTE);

    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);

    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    g_fnSpiDMACallback = dmaStatusCb;
}


//=====================================================================================================================
// Statics
//=====================================================================================================================

/**
 * @brief transmit + receive a byte
 * 
 * @param tx byte to transmit (can be 0x00 for none, only read)
 * @return uint8_t read byte
 */
static uint8_t spiRxTx(uint8_t tx)
{
    while (!LL_SPI_IsActiveFlag_TXE(g_pSPIPeripheral)) { /* infinite wait */ }
    LL_SPI_TransmitData8(g_pSPIPeripheral, tx);
    while (!LL_SPI_IsActiveFlag_RXNE(g_pSPIPeripheral)) { /* infinite wait */ }
    return LL_SPI_ReceiveData8(g_pSPIPeripheral);
}