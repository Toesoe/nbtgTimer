/**
 * @file i2c.c
 *
 * @brief i2c functionality for nbtgTimer
 *
 *
 * TODO: timeouts!
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "i2c.h"

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_i2c.h>
#include <stm32g0xx_ll_rcc.h>
#include <stm32g0xx_ll_utils.h>

#include "FreeRTOS.h"
#include "task.h"

#include "timer.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define I2C_TIMEOUT_US    500

// timings only valid for pclk == 16MHz. when upping clocks use CubeMX to regenerate these values
#define I2C_TIMING_100KHZ 0x00503D5A
#define I2C_TIMING_400KHZ 0x00300617
#define I2C_TIMING_1MHZ   0x00200205

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
 * @brief initialize the I2C peripheral
 * 
 */
void I2C_Init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
    LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);

    // disable I2C
    I2C1->CR1 &= ~I2C_CR1_PE;

    // disable ANF
    I2C1->CR1 |= I2C_CR1_ANFOFF;

    // set timing, calculated for sysclk == 16MHz
    I2C1->TIMINGR = I2C_TIMING_400KHZ;

    // enable peripheral
    I2C1->CR1 |= I2C_CR1_PE;
}

/**
 * @brief send a start condition
 * 
 */
void I2C_Start(void)
{
    // set start bit
    I2C1->CR2 |= I2C_CR2_START;

    // wait for bus to become busy
    while (!(I2C1->ISR & I2C_ISR_BUSY));
}

/**
 * @brief write a single byte to the I2C peripheral
 * 
 * @param data byte to write
 */
void I2C_Write(uint8_t data)
{
    while (!(I2C1->ISR & I2C_ISR_TXE))
        ; // wait for TXE bit to set
    I2C1->TXDR = data;
}

/**
 * @brief setup a transfer on the I2C bus
 * 
 * @param slaveAddr slave address to read from/write to
 * @param count     amount of bytes to transfer
 * @param isRead    set to true when reading, false when writing
 */
void I2C_SetupTransfer(uint8_t slaveAddr, uint8_t count, bool isRead)
{
    I2C1->CR2 = slaveAddr;                   //  set address
    if (isRead) I2C1->CR2 |= I2C_CR2_RD_WRN; // set r/w flag
    I2C1->CR2 &= ~I2C_CR2_AUTOEND;

    uint32_t nbytes = (count << 16);
    I2C1->CR2 |= nbytes; // nbytes is from bit 16->23
}

/**
 * @brief send a stop condition
 * 
 */
void I2C_Stop(void)
{
    I2C1->CR2 |= I2C_CR2_STOP;
}

/**
 * @brief write multiple bytes to the I2C peripheral
 * 
 * @param pData    pointer to buffer containing bytes to write
 * @param size     amount of bytes to write TODO: support more than 255
 * @param autoEnd  if true, will generate a stop condition after (size) bytes
 */
void I2C_WriteMulti(const uint8_t *pData, uint8_t size, bool autoEnd)
{
    /**** STEPS FOLLOWED  ************
    1. Wait for the TXE (bit 7 in SR1) to set. This indicates that the DR is empty
    2. Keep Sending DATA to the DR Register after performing the check if the TXE bit is set
    3. Once the DATA transfer is complete, Wait for the BTF (bit 2 in SR1) to set. This indicates the end of LAST DATA
    transmission
    */

    if (autoEnd)
    {
        I2C1->CR2 |= I2C_CR2_AUTOEND;
    }

    while (!(I2C1->ISR & I2C_ISR_TXE))
        ; // wait for TXE bit to set

    while (size)
    {
        while (!(I2C1->ISR & I2C_ISR_TXE))
            ;                           // wait for TXE bit to set
        I2C1->TXDR = (uint32_t)*pData++; // send data
        size--;
    }
}

/**
 * @brief read bytes from the I2C peripheral
 * 
 * @param pBuffer buffer to read bytes to
 * @param size    amount of bytes to read TODO: support more than 255
 */
void I2C_Read(uint8_t *pBuffer, uint8_t size)
{
    int remaining = size;

    I2C1->ISR &= ~I2C_ISR_NACKF; // clear the NACK bit

    if (size == 1)
    {
        while (!(I2C1->ISR & I2C_ISR_RXNE))
            ; // wait for rxne

        pBuffer[size - remaining] = (uint8_t)I2C1->RXDR;
    }

    else
    {
        while (remaining > 2)
        {
            while (!(I2C1->ISR & I2C_ISR_RXNE))
                ; // wait for rxne

            pBuffer[size - remaining] = (uint8_t)I2C1->RXDR;

            remaining--;
        }

        while (!(I2C1->ISR & I2C_ISR_RXNE))
            ;
        pBuffer[size - remaining] = (uint8_t)I2C1->RXDR;

        remaining--;

        while (!(I2C1->ISR & I2C_ISR_RXNE))
            ;
        pBuffer[size - remaining] = (uint8_t)I2C1->RXDR;
    }
}

/**
 * @brief find a specific byte over an I2C device
 * @note  utilizes RELOAD=1 to find a specific byte. does not require I2C_SetupTransfer
 *
 * @param slaveAddr  slave address to read from
 * @param byteToFind the byte in question to find
 * @param maxSize    maximum amount of bytes to check
 */
size_t I2C_FindFirstOccurrence(uint8_t slaveAddr, uint8_t byteToFind, size_t maxSize)
{
    bool   isFound = false;
    size_t cnt     = 0;

    I2C1->CR2      = slaveAddr;  //  set address
    I2C1->CR2 |= I2C_CR2_RD_WRN; // switch to READ

    I2C1->CR2 |= (uint32_t)(255 << 16); // nbytes is from bit 16->23

    I2C1->CR2 &= ~I2C_CR2_AUTOEND; // no auto-end
    I2C1->CR2 |= I2C_CR2_RELOAD;   // set reload mode

    I2C_Start();

    while (!isFound && (cnt != maxSize))
    {
        while (!(I2C1->ISR & I2C_ISR_RXNE))
            ; // wait for rxne

        if (I2C1->RXDR == byteToFind)
        {
            isFound = true;
        }
        else
        {
            cnt++;
        }

        if (I2C1->ISR & I2C_ISR_TCR)
        {
            // transmission complete reload. reset transfer
            I2C1->CR2 |= (uint32_t)(255 << 16); // set new number of bytes
        }
    }
    I2C1->CR2 &= ~I2C_CR2_RELOAD; // clear reload bit
    I2C_Stop();

    return cnt;
}