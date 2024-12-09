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

static i2cStatusCallback g_fnI2cDMACallback = NULL;
static i2cStatusCallback g_fnI2cRegularCallback = NULL;

static SI2CTransfer_t *g_pCurrentTransfer_I2C1 = NULL;
static SI2CTransfer_t *g_pCurrentTransfer_I2C2 = NULL;

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
void i2cInit(I2C_TypeDef *pI2CPeriph, uint32_t timing, bool isDisplay)
{
    if (pI2CPeriph == I2C1)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
        NVIC_SetPriority(I2C1_IRQn, 0);
        NVIC_EnableIRQ(I2C1_IRQn);
    }
    else if (pI2CPeriph == I2C2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
        NVIC_SetPriority(I2C2_IRQn, 0);
        NVIC_EnableIRQ(I2C2_IRQn);
    }

    LL_I2C_Disable(pI2CPeriph);
    LL_I2C_SetTiming(pI2CPeriph, timing);

    if (isDisplay)
    {
        LL_SYSCFG_EnableFastModePlus(pI2CPeriph == I2C1 ? LL_SYSCFG_I2C_FASTMODEPLUS_I2C1 : LL_SYSCFG_I2C_FASTMODEPLUS_I2C2);
        LL_I2C_EnableDMAReq_TX(pI2CPeriph);
    }

    LL_I2C_Enable(pI2CPeriph);

    LL_I2C_EnableIT_TX(pI2CPeriph);
    LL_I2C_EnableIT_RX(pI2CPeriph);
    LL_I2C_EnableIT_NACK(pI2CPeriph);
    LL_I2C_EnableIT_ERR(pI2CPeriph);
    LL_I2C_EnableIT_STOP(pI2CPeriph);
    LL_I2C_EnableIT_TC(pI2CPeriph);
}

void i2cRegisterCallback(i2cStatusCallback callback)
{
    g_fnI2cRegularCallback = callback;
}

void i2cTransmit(I2C_TypeDef *pI2CPeriph, SI2CTransfer_t *pI2CTransferCtx)
{
    pI2CPeriph->TXDR = *(pI2CTransferCtx->pBuffer++); // fix for erratum 2.8.6
    pI2CTransferCtx->transferred = 0;

    if (pI2CPeriph == I2C1) { g_pCurrentTransfer_I2C1 = pI2CTransferCtx; }
    else { LL_I2C_DisableDMAReq_TX(I2C2); g_pCurrentTransfer_I2C2 = pI2CTransferCtx; }

    uint16_t chunkSize = (pI2CTransferCtx->len > 255) ? 255 : pI2CTransferCtx->len;

    LL_I2C_HandleTransfer(pI2CPeriph, pI2CTransferCtx->address, LL_I2C_ADDRSLAVE_7BIT, chunkSize, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);
}

/**
 * @brief Initializes the DMA for I2C display transfer.
 *
 * Configures the DMA settings to enable data transfer from memory to the 
 * I2C peripheral for display purposes. Sets up interrupt handlers for 
 * transfer completion and error scenarios.
 *
 * @param[in] dispAddr The I2C address of the display controller.
 * @param[in] dmaStatusCb callback for dma complete or error
 *
 */
void i2cInitDisplayDMA(i2cStatusCallback dmaStatusCb)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
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

    g_fnI2cDMACallback = dmaStatusCb;
}

/**
 * @brief start DMA transaction to display
 * 
 */
void i2cTransferDisplayDMA(SI2CTransfer_t *pDisplayDMATransferCtx)
{
    g_pCurrentTransfer_I2C2 = pDisplayDMATransferCtx;
    g_pCurrentTransfer_I2C2->transferred = 0;

    // LL_I2C_DisableIT_TX(I2C2);
    // LL_I2C_DisableIT_RX(I2C2);
    // LL_I2C_DisableIT_NACK(I2C2);
    // LL_I2C_DisableIT_ERR(I2C2);
    // LL_I2C_DisableIT_STOP(I2C2);
    // LL_I2C_DisableIT_TC(I2C2);

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
      (uint32_t)pDisplayDMATransferCtx->pBuffer,
      (uint32_t)LL_I2C_DMA_GetRegAddr(I2C2, LL_I2C_DMA_REG_DATA_TRANSMIT),
      LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1)
    );

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 255);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_I2C_HandleTransfer(I2C2, g_pCurrentTransfer_I2C2->address, LL_I2C_ADDRSLAVE_7BIT, 255, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);
}

__attribute__((interrupt)) void DMA1_Channel1_IRQHandler(void)
{ 
    if (LL_DMA_IsActiveFlag_TC1(DMA1))
    {
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
        LL_DMA_ClearFlag_TC1(DMA1);

        // get previous transfer amount
        g_pCurrentTransfer_I2C2->transferred += (uint8_t)((I2C2->CR2 & I2C_CR2_NBYTES) >> I2C_CR2_NBYTES_Pos);

        if (g_pCurrentTransfer_I2C2->transferred < g_pCurrentTransfer_I2C2->len)
        {
            uint16_t chunkSize = ((g_pCurrentTransfer_I2C2->len - g_pCurrentTransfer_I2C2->transferred) > 255) ? 255 : (g_pCurrentTransfer_I2C2->len - g_pCurrentTransfer_I2C2->transferred);

            LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
                (uint32_t)(((uint32_t)g_pCurrentTransfer_I2C2->pBuffer) + g_pCurrentTransfer_I2C2->transferred),
                (uint32_t)LL_I2C_DMA_GetRegAddr(I2C2, LL_I2C_DMA_REG_DATA_TRANSMIT),
                LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1)
            );

            LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, chunkSize);

            LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

            LL_I2C_HandleTransfer(I2C2, g_pCurrentTransfer_I2C2->address, LL_I2C_ADDRSLAVE_7BIT, chunkSize, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);
        }
        else
        {
            if (g_fnI2cDMACallback != NULL) g_fnI2cDMACallback(true);
        }
    }
    else if (LL_DMA_IsActiveFlag_TE1(DMA1))
    {
        LL_DMA_ClearFlag_TE1(DMA1);
        LL_I2C_ClearFlag_STOP(I2C2);
        if (g_fnI2cDMACallback != NULL) g_fnI2cDMACallback(false);
    }
}

__attribute((interrupt)) void I2C1_IRQHandler(void)
{
  /* Check RXNE flag value in ISR register */
    if (LL_I2C_IsActiveFlag_TXIS(I2C1))
    {
        g_pCurrentTransfer_I2C1->transferred++;
        LL_I2C_TransmitData8(I2C1, *(g_pCurrentTransfer_I2C1->pBuffer++));
    }
    /* Check STOP flag value in ISR register */
    else if (LL_I2C_IsActiveFlag_STOP(I2C1))
    {
        /* End of Transfer */
        LL_I2C_ClearFlag_STOP(I2C1);
        if (g_fnI2cRegularCallback != NULL) g_fnI2cRegularCallback(true);
    }
    else
    {
        if (g_fnI2cRegularCallback != NULL) g_fnI2cRegularCallback(false);
    }
}

__attribute((interrupt)) void I2C2_IRQHandler(void)
{
    if (LL_I2C_IsActiveFlag_NACK(I2C2))
    {
        LL_I2C_ClearFlag_NACK(I2C2);
    }

    else if (LL_I2C_IsActiveFlag_TXIS(I2C2))
    {
        g_pCurrentTransfer_I2C2->transferred++;
        LL_I2C_TransmitData8(I2C2, *g_pCurrentTransfer_I2C2->pBuffer++);
    }

    else if (LL_I2C_IsActiveFlag_STOP(I2C2))
    {
        LL_I2C_ClearFlag_STOP(I2C2);

        if (g_pCurrentTransfer_I2C2->transferred < g_pCurrentTransfer_I2C2->len)
        {
            uint16_t chunkSize = ((g_pCurrentTransfer_I2C2->len - g_pCurrentTransfer_I2C2->transferred) > 255) ? 255 : (g_pCurrentTransfer_I2C2->len - g_pCurrentTransfer_I2C2->transferred);

            LL_I2C_HandleTransfer(I2C2, g_pCurrentTransfer_I2C2->address, LL_I2C_ADDRSLAVE_7BIT, chunkSize, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_RESTART_7BIT_WRITE);
        }
        else
        {
            if (g_fnI2cRegularCallback != NULL) g_fnI2cRegularCallback(true);
        }
    }
    else
    {
        if (g_fnI2cRegularCallback != NULL) g_fnI2cRegularCallback(false);
    }
}