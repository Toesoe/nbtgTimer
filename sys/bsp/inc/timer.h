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

typedef enum
{
    TIMER_SYS_DELAY,
    TIMER_FRAMERATE,
    TIMER_ENLARGER_LAMP_ENABLE,
} ETimerType_t;

typedef void (*fnTimCallback)(void *userCtx);

/** @brief struct that defines a timer */
typedef struct
{
    ETimerType_t timerType;
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

void startEnlargerTimer(uint32_t);
uint32_t timerGetValue(STimerDef_t const *);

// freertos system timers
void initRtosTimer(void);
uint32_t rtosTimerGetValue(void);

void registerTimerCallback(ETimerType_t, fnTimCallback, void *);

#ifdef __cplusplus
}
#endif
#endif //!_TIMER_H_