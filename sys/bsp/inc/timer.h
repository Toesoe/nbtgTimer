/**
 * @file timer.h
 *
 * @brief timer routines
 */

#ifndef _TIMER_H_
#define _TIMER_H_

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

/** @brief struct that defines a timer */
typedef struct
{
    TIM_TypeDef *pHWTimer; //< the hardware timer used for this timerdef
    uint32_t     period;   //< the interval period in Hz (1000 = second timer, etc)
} STimerDef_t;

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initTimer(STimerDef_t const *);
void timerDelay(STimerDef_t const *, const uint32_t);
uint32_t timerGetValue(STimerDef_t const *);

// freertos system timers
void initRtosTimer(void);
uint32_t rtosTimerGetValue(void);

#ifdef __cplusplus
}
#endif
#endif //!_TIMER_H_