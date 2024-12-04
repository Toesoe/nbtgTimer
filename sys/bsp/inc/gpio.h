/**
 * @file gpio.h
 *
 * @brief generic gpio functionality
 */

#ifndef _GPIO_H_
#define _GPIO_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>

#include "stm32g070xx.h"

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief basic GPIO pin/port definition */
typedef struct
{
    uint32_t pin;
    GPIO_TypeDef *port;
} SGPIOPin_t;

/** @brief GPIO pin/port definition with input/output flag */
typedef struct
{
    SGPIOPin_t pinPort;
    bool isOutput;
} SGenericGPIOPin_t;

/** @brief EXTI mapped GPIO pin/port definition */
typedef struct
{
    SGPIOPin_t gpio;
    uint32_t extiLine;
    uint32_t extiPort;
} SEXTIGPIOType_t;

/** @brief pin definitions for SPI */
typedef struct
{
    SPI_TypeDef *pPeripheral;
    SGPIOPin_t   csPin;
    SGPIOPin_t   sckPin;
    SGPIOPin_t   misoPin;
    SGPIOPin_t   mosiPin;
    uint32_t     pinAFMode;
} SSPIPinDef_t;

/** @brief pin definitions for I2C  */
typedef struct
{
    I2C_TypeDef *pPeripheral;
    SGPIOPin_t   sdaPin;
    SGPIOPin_t   sclPin;
    SGPIOPin_t   wpPin; // optional WP pin for EEPROMs
    uint32_t     pinAFMode;
} SI2CPinDef_t;

/** @brief pin definitions for USART */
typedef struct
{
    USART_TypeDef *pPeripheral;
    SGPIOPin_t     txPin;
    SGPIOPin_t     rxPin;
    SGPIOPin_t     dePin; // driver enable, if applicable
    uint32_t       pinAFMode;
} SUSARTPinDef_t;

/** @brief struct to specify peripheral pin definitions */
typedef struct
{
    // I2C
    SI2CPinDef_t *pI2cEepromPinDef;
    SI2CPinDef_t *pI2cDispPinDef;
} STimerPeriphPinDef_t;

/** @brief struct to specify generic pin definitions */
typedef struct
{
    SGenericGPIOPin_t *pButton10SecPlus;
    SGenericGPIOPin_t *pButton10SecMinus;
    SGenericGPIOPin_t *pButton1SecPlus;
    SGenericGPIOPin_t *pButton1SecMinus;
    SGenericGPIOPin_t *pButton100MsecPlus;
    SGenericGPIOPin_t *pButton100MsecMinus;
    SGenericGPIOPin_t *pButtonToggleLamp;
    SGenericGPIOPin_t *pButtonStartTimer;
    SGenericGPIOPin_t *pButtonMode;

    SGenericGPIOPin_t *pPinOptocoupler;

    SGenericGPIOPin_t *pFootswitchDetect;
    SGenericGPIOPin_t *pFootswitchInput;
} STimerGenericPinDef_t;

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initGPIO_peripherals(STimerPeriphPinDef_t *);
void initGPIO_generic(SGenericGPIOPin_t *);

void toggleEepromWP(bool);
void toggleOptocoupler(bool);

#ifdef __cplusplus
}
#endif
#endif //!_GPIO_H_