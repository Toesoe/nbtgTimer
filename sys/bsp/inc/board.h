/**
 * @file board.h
 *
 * @brief board-specific functionality
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "i2c.h"
#include "gpio.h"
#include "timer.h"

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initBoard(void);

#ifdef __cplusplus
}
#endif
#endif //!_BOARD_H_