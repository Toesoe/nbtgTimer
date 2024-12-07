/**
 * @file i2c.c
 *
 * @brief i2c functionality
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "i2c.h"

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_i2c.h>
#include <stm32g0xx_ll_dma.h>
#include <stm32g0xx_ll_rcc.h>
#include <stm32g0xx_ll_utils.h>
#include <stm32g0xx_ll_system.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define GET_BIT_POS(num) (((num) == 0) ? 0 : __builtin_ctzl(num)) // builtin_ctzl counts trailing zeroes

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

static i2cDMACompleteCb g_fnCbTxComplete = NULL;
static i2cDMAErrorCb g_fnCbTxError = NULL;

static I2C_TypeDef *g_pI2CDisp = NULL;

static uint8_t g_dispI2CAddr = 0;
static uint32_t g_dispFramebufSize = 0;
static uint32_t g_dispFramebufAddr = 0;

static uint32_t g_curFramebufSize = 0;
static uint32_t g_curFramebufAddr = 0;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief Initializes an I2C peripheral
 *
 * Initializes an I2C peripheral given its timing and whether it runs at 1MHz.
 *
 * @param[in] pI2CPeriph Pointer to the I2C peripheral to be initialized, either I2C1 or I2C2.
 * @param[in] timing The value to be written to the I2C_TIMINGR register.
 * @param[in] isDisplay Whether the I2C peripheral is used for display. If true, fast mode plus + DMA is enabled.
 */
void initI2C(I2C_TypeDef *pI2CPeriph, uint32_t timing, bool isDisplay)
{
    if (pI2CPeriph == I2C1)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
    }
    else if (pI2CPeriph == I2C2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
    }

    LL_I2C_InitTypeDef I2C_InitStruct = {0};

    I2C_InitStruct.PeripheralMode  = LL_I2C_MODE_I2C;
    I2C_InitStruct.Timing          = timing;
    I2C_InitStruct.AnalogFilter    = LL_I2C_ANALOGFILTER_ENABLE;
    I2C_InitStruct.DigitalFilter   = 0;
    I2C_InitStruct.OwnAddress1     = 0;
    I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
    I2C_InitStruct.OwnAddrSize     = LL_I2C_OWNADDRESS1_7BIT;
    LL_I2C_Init(pI2CPeriph, &I2C_InitStruct);
    LL_I2C_EnableAutoEndMode(pI2CPeriph);
    LL_I2C_SetOwnAddress2(pI2CPeriph, 0, LL_I2C_OWNADDRESS2_NOMASK);
    LL_I2C_DisableOwnAddress2(pI2CPeriph);
    LL_I2C_DisableGeneralCall(pI2CPeriph);
    LL_I2C_EnableClockStretching(pI2CPeriph);

    if (isDisplay)
    {
        g_pI2CDisp = pI2CPeriph;
        LL_SYSCFG_EnableFastModePlus(pI2CPeriph == I2C1 ? LL_SYSCFG_I2C_FASTMODEPLUS_I2C1 : LL_SYSCFG_I2C_FASTMODEPLUS_I2C2);
        LL_I2C_EnableDMAReq_TX(pI2CPeriph);
    }

    LL_I2C_Enable(pI2CPeriph);
}

void I2CTransmit(I2C_TypeDef *pI2CPeriph, uint8_t addr, uint8_t *data, size_t len)
{
    pI2CPeriph->TXDR = *data++; // fix for erratum 2.8.6
    LL_I2C_HandleTransfer(pI2CPeriph, addr, LL_I2C_ADDRSLAVE_7BIT, len, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);

    while (!LL_I2C_IsActiveFlag_TC(pI2CPeriph))
    {
        while (!LL_I2C_IsActiveFlag_TXE(pI2CPeriph)) {}
        LL_I2C_TransmitData8(pI2CPeriph, *data++);
    }

    while (!LL_I2C_IsActiveFlag_STOP(pI2CPeriph)){}
    LL_I2C_ClearFlag_TC(pI2CPeriph);
    LL_I2C_ClearFlag_STOP(pI2CPeriph);
}

/**
 * @brief Initializes the DMA for I2C display transfer.
 *
 * Configures the DMA settings to enable data transfer from memory to the 
 * I2C peripheral for display purposes. Sets up interrupt handlers for 
 * transfer completion and error scenarios.
 *
 * @param[in] framebufferSize The size of the framebuffer to be transferred.
 * @param[in] framebufferAddress The memory address of the framebuffer.
 * @param[in] cbTxComplete Callback function to be called upon transfer completion.
 * @param[in] cbTxError Callback function to be called upon transfer error.
 */
void initDisplayI2CDMA(uint8_t dispAddr, size_t framebufferSize, uint32_t framebufferAddress, i2cDMACompleteCb cbTxComplete, i2cDMAErrorCb cbTxError)
{
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_I2C2_TX);
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

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, framebufferSize);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1, framebufferAddress, LL_I2C_DMA_GetRegAddr(g_pI2CDisp, LL_I2C_DMA_REG_DATA_TRANSMIT), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, framebufferAddress);

    g_fnCbTxComplete = cbTxComplete;
    g_fnCbTxError = cbTxError;
    g_dispI2CAddr = dispAddr;
    g_dispFramebufSize = framebufferSize;
    g_dispFramebufAddr = framebufferAddress;
    g_curFramebufAddr = framebufferAddress;
}

void i2cTransferDMA(void)
{
    g_curFramebufSize = g_dispFramebufSize - 255;
    g_curFramebufAddr = g_dispFramebufAddr + 255;

    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, g_dispFramebufAddr);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 255);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_I2C_HandleTransfer(g_pI2CDisp, g_dispI2CAddr, LL_I2C_ADDRSLAVE_7BIT, 255, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);
}

__attribute__((interrupt)) void DMA1_Channel1_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC1(DMA1))
    {
        LL_DMA_ClearFlag_TC1(DMA1);

        if (g_curFramebufSize > 0)
        {
            uint16_t current_chunk_size = (g_curFramebufSize > 255) ? 255 : g_curFramebufSize;
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
            LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, g_curFramebufAddr);
            LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, current_chunk_size);
            LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

            LL_I2C_HandleTransfer(g_pI2CDisp, g_dispI2CAddr, LL_I2C_ADDRSLAVE_7BIT, current_chunk_size, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_RESTART_7BIT_WRITE);

            g_curFramebufSize -= current_chunk_size;
            g_curFramebufAddr += current_chunk_size;
        }
        else
        {
            LL_I2C_GenerateStopCondition(g_pI2CDisp);
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
            if (g_fnCbTxComplete != NULL) g_fnCbTxComplete();
        }
    }
    else if (LL_DMA_IsActiveFlag_TE1(DMA1))
    {
        LL_DMA_ClearFlag_TE1(DMA1);
        LL_I2C_ClearFlag_STOP(g_pI2CDisp);
        if (g_fnCbTxError != NULL) g_fnCbTxError();
    }
}