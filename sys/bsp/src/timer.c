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

typedef struct
{
    fnTimCallback fnCb;
    void *        pUserCtx;
} STimerIRQCallback_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static STimerIRQCallback_t g_framerateCallback = {0};
static STimerIRQCallback_t g_enlargerCallback = {0};

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
        LL_TIM_EnableCounter(pTimerDef->pHWTimer);
    }
    else if (pTimerDef->pHWTimer == TIM14)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM14);
        LL_TIM_SetPrescaler(pTimerDef->pHWTimer, __LL_TIM_CALC_PSC(SystemCoreClock, pTimerDef->period));
        LL_TIM_SetAutoReload(TIM14, __LL_TIM_CALC_ARR(SystemCoreClock, LL_TIM_GetPrescaler(TIM14), 33)); // 30hz tick for a 30fps refresh
        LL_TIM_EnableIT_UPDATE(TIM14);
        NVIC_SetPriority(TIM14_IRQn, 0);
        LL_TIM_EnableCounter(pTimerDef->pHWTimer);
    }
    else if (pTimerDef->pHWTimer == TIM15)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM15);
        LL_TIM_SetPrescaler(pTimerDef->pHWTimer, __LL_TIM_CALC_PSC(SystemCoreClock, pTimerDef->period));
        LL_TIM_SetAutoReload(TIM14, __LL_TIM_CALC_ARR(SystemCoreClock, LL_TIM_GetPrescaler(TIM14), 100)); // 10hz for .1 second resolution
        LL_TIM_EnableIT_UPDATE(TIM15);
        NVIC_SetPriority(TIM15_IRQn, 0);
    }
}

/**
 * @brief start enlarger timer
 * 
 * @param duration in milliseconds, for lamp to remain on
 */
void startEnlargerTimer(uint32_t duration)
{
    LL_TIM_SetAutoReload(TIM15, __LL_TIM_CALC_ARR(SystemCoreClock, LL_TIM_GetPrescaler(TIM15), duration));
    LL_TIM_EnableCounter(TIM15);
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

void registerTimerCallback(ETimerType_t timerType, fnTimCallback fnCb, void *pUserData)
{
    switch(timerType)
    {
        case TIMER_ENLARGER_LAMP_ENABLE:
        {
            g_enlargerCallback.fnCb = fnCb;
            g_enlargerCallback.pUserCtx = pUserData;
            break;
        }
        case TIMER_FRAMERATE:
        {
            g_framerateCallback.fnCb = fnCb;
            g_framerateCallback.pUserCtx = pUserData;
            break;
        }
        default:
        break;
    }
}

void TIM14_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM14))
    {
        LL_TIM_ClearFlag_UPDATE(TIM14);
        if (g_framerateCallback.fnCb) g_framerateCallback.fnCb(g_framerateCallback.pUserCtx);
    }
}

void TIM15_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM15))
    {
        LL_TIM_ClearFlag_UPDATE(TIM15);
        if (g_enlargerCallback.fnCb) g_enlargerCallback.fnCb(g_enlargerCallback.pUserCtx);
    }
}

//=====================================================================================================================
// Statics
//=====================================================================================================================