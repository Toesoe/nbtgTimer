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
 * @param[in] is1MHz Whether the I2C peripheral runs at 1MHz. If true, fast mode plus is enabled.
 */
void initI2C(I2C_TypeDef *pI2CPeriph, uint32_t timing, bool is1MHz)
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

    if (is1MHz)
    {
        LL_SYSCFG_EnableFastModePlus(pI2CPeriph == I2C1 ? LL_SYSCFG_I2C_FASTMODEPLUS_I2C1 : LL_SYSCFG_I2C_FASTMODEPLUS_I2C2);
    }
}

/**
 * @brief Initializes the DMA for I2C2 (display) transmit transfers.
 *
 * Initializes the DMA for I2C2 (display) transmit transfers.
 *
 * @note This function must be called before the display is initialized.
 */
void initDisplayI2CDMA(void)
{
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_I2C2_TX);
    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_BYTE);
}