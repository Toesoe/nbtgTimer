/**
 * @file spi.h
 *
 * @brief spi functionality
 */

 #ifndef _SPI_H_
 #define _SPI_H_
 
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
 
 typedef void (*spiStatusCallback)(bool);

 typedef struct
{
    uint8_t *pBuffer;
    size_t len;
    size_t transferred;
} SSPITransfer_t;

 //=====================================================================================================================
 // Defines
 //=====================================================================================================================
 
 //=====================================================================================================================
 // Functions
 //=====================================================================================================================
 
 void spiInit(SPI_TypeDef *);
 
 void spiWriteData(const uint8_t *, size_t);
 void spiReadData(uint8_t *, size_t);
 void spiSendCommand(const uint8_t *, size_t);

 void spiInitDisplayDMA(spiStatusCallback);
 void spiTransferBlockDMA(SSPITransfer_t *);
 
 #ifdef __cplusplus
 }
 #endif
 #endif //!_SPI_H_