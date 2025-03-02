/**
 * @file timer.c
 *
 * @brief timer functionality
 * 
 * TODO: just TIM1 for now: add possible timers as enum
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "timer.h"

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_tim.h>
#include <stm32g0xx_ll_rcc.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

static displayFramerateTimerCallback g_fnFramerateCallback = NULL;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief initialize a specific timer
 * 
 */
void initTimer(STimerDef_t const *pTimerDef)
{
    if (pTimerDef->pHWTimer == TIM1)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
        LL_TIM_SetPrescaler(pTimerDef->pHWTimer, __LL_TIM_CALC_PSC(SystemCoreClock, pTimerDef->period));
        TIM1->ARR = 0xFFFFFFFF;
    }
    else if (pTimerDef->pHWTimer == TIM14)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM14);
        LL_TIM_SetPrescaler(pTimerDef->pHWTimer, __LL_TIM_CALC_PSC(SystemCoreClock, pTimerDef->period));
        LL_TIM_SetAutoReload(TIM14, __LL_TIM_CALC_ARR(SystemCoreClock, LL_TIM_GetPrescaler(TIM14), 33)); // 30fps tick
        LL_TIM_EnableIT_UPDATE(TIM14);
        NVIC_SetPriority(TIM14_IRQn, 0);
    }

    
    LL_TIM_EnableCounter(pTimerDef->pHWTimer);
}


/**
 * @brief get value of counter register on specific timer
 * @param pTimer timer to check
 * 
 * @return uint32_t value
 */
uint32_t timerGetValue(STimerDef_t const *pTimer)
{
    return pTimer->pHWTimer->CNT;
}

/**
 * @brief blocking delay timer
 * 
 * @param pTimer timer to use
 * @param delay amount of TIMER_UNITS to delay, depends on selected timer
 */
void timerDelay(STimerDef_t const *pTimer, const uint32_t delay)
{
    uint32_t start = pTimer->pHWTimer->CNT;

    while ((pTimer->pHWTimer->CNT - start) < delay);
}

/**
 * @brief freertos runtime stats timer, 1MHz
 * 
 * @note uses TIM17: don't use for other timers
 * 
 */
void initRtosTimer(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM17);

    LL_TIM_SetPrescaler(TIM17, __LL_TIM_CALC_PSC(SystemCoreClock, 1000000));
    TIM17->ARR = 0xFFFFFFFF;
    LL_TIM_EnableCounter(TIM17);
}

/**
 * @brief fetch timer value for freertos runtime stats 
 * 
 * @return uint32_t timer value
 */
uint32_t rtosTimerGetValue(void)
{
    return TIM17->CNT;
}

void registerDisplayFramerateTimerCallback(displayFramerateTimerCallback fnCb)
{
    if (fnCb == NULL) return;
    g_fnFramerateCallback = fnCb;
    NVIC_EnableIRQ(TIM14_IRQn);
}


void TIM14_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM14))
    {
        LL_TIM_ClearFlag_UPDATE(TIM14);
        g_fnFramerateCallback();
    }
}

//=====================================================================================================================
// Statics
//=====================================================================================================================