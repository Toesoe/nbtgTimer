/**
 * @file i2c.h
 *
 * @brief i2c routines
 */

#ifndef _I2C_H_
#define _I2C_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "stm32g070xx.h"

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef void (*i2cStatusCallback)(bool);

typedef struct
{
    uint8_t address;
    uint8_t *pBuffer;
    size_t len;
    size_t transferred;
} SI2CTransfer_t;

//=====================================================================================================================
// Defines
//=====================================================================================================================

// only valid for 64MHz PCLK + analog filter enable!!
#define I2C_100KHZ (0x10B17DB5)
#define I2C_400KHZ (0x00C12166)
#define I2C_1MHZ (0x00910B1C)

//=====================================================================================================================
// Functions
//=====================================================================================================================

void i2cInit(I2C_TypeDef *, uint32_t, bool);
void i2cRegisterCallback(i2cStatusCallback);

void i2cTransmit(I2C_TypeDef *, SI2CTransfer_t *);

void i2cInitDisplayDMA(i2cStatusCallback);
void i2cTransferDisplayDMA(SI2CTransfer_t *);

#ifdef __cplusplus
}
#endif
#endif //!_I2C_H_