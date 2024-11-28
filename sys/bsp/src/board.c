/**
 * @file board.c
 *
 * @brief board-specific functionality for nbtgTimer
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "board.h"

#include "stm32g070xx.h"

#include <stm32g0xx_ll_rcc.h>
#include <stm32g0xx_ll_exti.h>
#include <stm32g0xx_ll_system.h>
#include <stm32g0xx_ll_tim.h>
#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_utils.h>
#include <stm32g0xx_ll_adc.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_wwdg.h>

#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SYS_CLK_FREQ_HZ (64000000)

STimerDef_t timer1 = {TIM1, 1000};
STimerDef_t timer14 = {TIM14, 100};

static void initSysclock(void);

void initBoard(void)
{
#ifdef DEBUG
    DBG->APBFZ1 |= DBG_APB_FZ1_DBG_IWDG_STOP;
    DBG->APBFZ1 |= DBG_APB_FZ1_DBG_WWDG_STOP;
#endif
    // reset peripherals, init flash + systick
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    // disable internal pullup on dead battery pins of UCPD periph
    LL_SYSCFG_DisableDBATT(LL_SYSCFG_UCPD1_STROBE | LL_SYSCFG_UCPD2_STROBE);

    initSysclock();

    initTimer(&timer1);
    initTimer(&timer14);
    I2C_Init();
}

//=====================================================================================================================
// Statics
//=====================================================================================================================

/**
 * @brief basic sysclock init + prescalers
 * 
 * @note  system is clocked at 16MHz; this includes all periphs on APB/AHB
 * 
 */
static void initSysclock(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
    LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1);

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    // clock PLL at 64MHz: (16MHz / 1) * (8 / 2)
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1);

    // sysclk uses HSI: max = 16MHz. if you want to go higher use main PLL
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);

    LL_Init1msTick(SYS_CLK_FREQ_HZ);
    LL_SetSystemCoreClock(SYS_CLK_FREQ_HZ);
}