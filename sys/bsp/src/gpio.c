/**
 * @file gpio.c
 *
 * @brief generic gpio functionality
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "gpio.h"

#include <stdbool.h>
#include <stddef.h>

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_exti.h>
#include <stm32g0xx_ll_utils.h>
#include <stm32g0xx_ll_gpio.h>

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

static STimerPeriphPinDef_t *g_pCurrentPeriphPinDefs = NULL;
static STimerGenericPinDef_t *g_pCurrentGenericPinDefs = NULL;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

static void initGPIO_RS232(SUSARTPinDef_t *);
static void initGPIO_I2C(SI2CPinDef_t *);
static void initGPIO_SPI(SSPIPinDef_t *);
static void initGPIO_Generic(SGenericGPIOPin_t *);

//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief initialize GPIO pins
 */
void initGPIO_peripherals(STimerPeriphPinDef_t *pPinDefs)
{
    g_pCurrentPeriphPinDefs = pPinDefs;

    initGPIO_I2C(g_pCurrentPeriphPinDefs->pI2cDispPinDef);
    initGPIO_I2C(g_pCurrentPeriphPinDefs->pI2cEepromPinDef);
    initGPIO_SPI(g_pCurrentPeriphPinDefs->pSpiDisplayDef);
}


void initGPIO_generic(STimerGenericPinDef_t *pPinDefs)
{
    g_pCurrentGenericPinDefs = pPinDefs;

    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton10SecPlus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton10SecMinus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton1SecPlus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton1SecMinus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton100MsecPlus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButton100MsecMinus);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButtonToggleLamp);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButtonStartTimer);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pButtonMode);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pPinOptocoupler);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pFootswitchDetect);
    initGPIO_Generic(g_pCurrentGenericPinDefs->pFootswitchInput);
}

void toggleEepromWP(bool disableWP)
{
    disableWP ? LL_GPIO_ResetOutputPin(g_pCurrentPeriphPinDefs->pI2cEepromPinDef->wpPin.port,
                                    g_pCurrentPeriphPinDefs->pI2cEepromPinDef->wpPin.pin) :
               LL_GPIO_SetOutputPin(g_pCurrentPeriphPinDefs->pI2cEepromPinDef->wpPin.port,
                                      g_pCurrentPeriphPinDefs->pI2cEepromPinDef->wpPin.pin);
}

void toggleOptocoupler(bool enableOutput)
{
    enableOutput ? LL_GPIO_SetOutputPin(g_pCurrentGenericPinDefs->pPinOptocoupler->pinPort.port,
                                        g_pCurrentGenericPinDefs->pPinOptocoupler->pinPort.pin) :
                   LL_GPIO_ResetOutputPin(g_pCurrentGenericPinDefs->pPinOptocoupler->pinPort.port,
                                          g_pCurrentGenericPinDefs->pPinOptocoupler->pinPort.pin);
}

void toggleDisplayDataCommand(bool isCommand)
{
    isCommand ? LL_GPIO_ResetOutputPin(g_pCurrentPeriphPinDefs->pSpiDisplayDef->dcPin.port,
                                    g_pCurrentPeriphPinDefs->pSpiDisplayDef->dcPin.pin) :
                LL_GPIO_SetOutputPin(g_pCurrentPeriphPinDefs->pSpiDisplayDef->dcPin.port,
                                    g_pCurrentPeriphPinDefs->pSpiDisplayDef->dcPin.pin);
}

//=====================================================================================================================
// Statics
//=====================================================================================================================

/**
 * @brief initialize UART
 */
static void initGPIO_RS232(SUSARTPinDef_t *pUSARTDef)
{
    LL_GPIO_InitTypeDef consoleGpio = {
        .Pin = pUSARTDef->txPin.pin,
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Speed = LL_GPIO_SPEED_FREQ_HIGH,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_NO, // HW pullup
        .Alternate = pUSARTDef->pinAFMode,
    };

    LL_GPIO_Init(pUSARTDef->txPin.port, &consoleGpio);

    consoleGpio.Pin = pUSARTDef->rxPin.pin;
    LL_GPIO_Init(pUSARTDef->rxPin.port, &consoleGpio);
}

/**
 * @brief initialize I2C GPIOs
 * 
 * TODO: doublecheck if necessary
 */
static void initGPIO_I2C(SI2CPinDef_t *pI2CDef)
{
    LL_GPIO_InitTypeDef i2cGpio = {
        .Pin = pI2CDef->sclPin.pin,
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH,
        .OutputType = LL_GPIO_OUTPUT_OPENDRAIN,
        .Pull = LL_GPIO_PULL_NO, // HW pullup
        .Alternate = pI2CDef->pinAFMode,
    };

    LL_GPIO_Init(pI2CDef->sclPin.port, &i2cGpio);

    i2cGpio.Pin = pI2CDef->sdaPin.pin;
    LL_GPIO_Init(pI2CDef->sdaPin.port, &i2cGpio);

    LL_GPIO_SetPinPull(pI2CDef->sdaPin.port, pI2CDef->sdaPin.pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(pI2CDef->sclPin.port, pI2CDef->sclPin.pin, LL_GPIO_PULL_NO);

    if (pI2CDef->wpPin.pin != 0)
    {
        i2cGpio.Mode = LL_GPIO_MODE_OUTPUT;
        i2cGpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
        i2cGpio.Pin = pI2CDef->wpPin.pin;
        LL_GPIO_Init(pI2CDef->wpPin.port, &i2cGpio);
    }
}

/**
 * @brief initialize SPI GPIOs
 *
 */
static void initGPIO_SPI(SSPIPinDef_t *pSPIDef)
{
    LL_GPIO_InitTypeDef spiGpio = {
        .Pin = pSPIDef->sckPin.pin,
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Speed = LL_GPIO_SPEED_FREQ_HIGH,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_NO, // HW pullup
        .Alternate = pSPIDef->pinAFMode,
    };

    LL_GPIO_Init(pSPIDef->sckPin.port, &spiGpio);

    spiGpio.Pin = pSPIDef->misoPin.pin;
    LL_GPIO_Init(pSPIDef->misoPin.port, &spiGpio);

    spiGpio.Pin = pSPIDef->mosiPin.pin;
    LL_GPIO_Init(pSPIDef->mosiPin.port, &spiGpio);

    spiGpio.Pin = pSPIDef->csPin.pin;
    spiGpio.Mode = LL_GPIO_MODE_OUTPUT; // no AF: CS is done in SW
    LL_GPIO_Init(pSPIDef->csPin.port, &spiGpio);

    spiGpio.Pin = pSPIDef->dcPin.pin;
    LL_GPIO_Init(pSPIDef->dcPin.port, &spiGpio);
}

static void initGPIO_Generic(SGenericGPIOPin_t *pGenericPinDef)
{
    LL_GPIO_InitTypeDef gpio = {
        .Pin = pGenericPinDef->pinPort.pin,
        .Mode = pGenericPinDef->isOutput,
        .Speed = LL_GPIO_SPEED_FREQ_HIGH,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
        .Pull = LL_GPIO_PULL_NO,
    };

    LL_GPIO_Init(pGenericPinDef->pinPort.port, &gpio);
}