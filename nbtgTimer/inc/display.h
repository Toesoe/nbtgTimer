/**
 * @file  display.h
 * @brief 128x64 oled display routines
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(void);

void ssd1306_Reset(void);
void ssd1306_WriteSingle(uint8_t);
void ssd1306_WriteMulti(uint8_t *, size_t);
void ssd1306_delayMs(uint32_t);

#ifdef __cplusplus
}
#endif
#endif //!_DISPLAY_H_