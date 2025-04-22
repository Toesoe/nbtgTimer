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
#include "timer.h"

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

#define SYS_CLK_FREQ_HZ (SystemCoreClock)

//=====================================================================================================================
// Globals
//=====================================================================================================================

STimerDef_t delayTimer     = {TIMER_SYS_DELAY, TIM1, 1000, nullptr, nullptr};
STimerDef_t framerateTimer = {TIMER_FRAMERATE, TIM14, 1000, nullptr, nullptr};
STimerDef_t enlargerTimer  = {TIMER_ENLARGER_LAMP_ENABLE, TIM15, 1000, nullptr, nullptr};

// I2C:                      periph, SDA                     , SCL                     , WP                      , AFMODE
SI2CPinDef_t g_R1_eepromI2C = {I2C1, { LL_GPIO_PIN_7, GPIOB }, { LL_GPIO_PIN_6, GPIOB }, { LL_GPIO_PIN_5, GPIOB }, LL_GPIO_AF_6 };
SI2CPinDef_t g_R1_dispI2C   = {I2C2, { LL_GPIO_PIN_11, GPIOB }, { LL_GPIO_PIN_10, GPIOB }, {0, 0}                , LL_GPIO_AF_6 }; // OPT1 and OPT2 on schematic

// SPI:                      periph,  CS,                       SCLK,                    , MISO                     ,  MOSI                    , D/C                      ,  RST                     , AFMODE
SSPIPinDef_t g_R1_dispSPI   = {SPI2, { LL_GPIO_PIN_12, GPIOB }, { LL_GPIO_PIN_13, GPIOB }, { LL_GPIO_PIN_14, GPIOB }, { LL_GPIO_PIN_15, GPIOB }, { LL_GPIO_PIN_11, GPIOB }, { LL_GPIO_PIN_10, GPIOB }, LL_GPIO_AF_0};

// Generic
SGenericGPIOPin_t    g_R1_button10SecPlus       = {{ LL_GPIO_PIN_7, GPIOA }, false }; // SW4
SGenericGPIOPin_t    g_R1_button10SecMinus      = {{ LL_GPIO_PIN_6, GPIOA }, false }; // SW7
SGenericGPIOPin_t    g_R1_button1SecPlus        = {{ LL_GPIO_PIN_5, GPIOA }, false }; // SW5
SGenericGPIOPin_t    g_R1_button1SecMinus       = {{ LL_GPIO_PIN_4, GPIOA }, false }; // SW8
SGenericGPIOPin_t    g_R1_button100MsecPlus     = {{ LL_GPIO_PIN_3, GPIOA }, false }; // SW6
SGenericGPIOPin_t    g_R1_button100MsecMinus    = {{ LL_GPIO_PIN_2, GPIOA }, false }; // SW9
SGenericGPIOPin_t    g_R1_buttonToggleLamp      = {{ LL_GPIO_PIN_2, GPIOC }, false }; // SW1
SGenericGPIOPin_t    g_R1_buttonStartTimer      = {{ LL_GPIO_PIN_3, GPIOC }, false }; // SW2
SGenericGPIOPin_t    g_R1_buttonMode            = {{ LL_GPIO_PIN_1, GPIOC }, false }; // SW3
SGenericGPIOPin_t    g_R1_pinOptocoupler        = {{ LL_GPIO_PIN_2, GPIOB }, true };
SGenericGPIOPin_t    g_R1_footswitchDetect      = {{ LL_GPIO_PIN_1, GPIOB }, false };
SGenericGPIOPin_t    g_R1_footswitchInput       = {{ LL_GPIO_PIN_0, GPIOB }, false };
SGenericGPIOPin_t    g_R1_pinBuzzer             = {{ LL_GPIO_PIN_8, GPIOB }, true };  // SW18

STimerPeriphPinDef_t g_timerRev1PeriphPins      = {
         &g_R1_eepromI2C,
         &g_R1_dispI2C,
         &g_R1_dispSPI
};

STimerGenericPinDef_t g_timerRev1GenericPins = {
    &g_R1_button10SecPlus,   &g_R1_button10SecMinus,   &g_R1_button1SecPlus,   &g_R1_button1SecMinus,
    &g_R1_button100MsecPlus, &g_R1_button100MsecMinus, &g_R1_buttonToggleLamp, &g_R1_buttonStartTimer,
    &g_R1_buttonMode,        &g_R1_pinOptocoupler,     &g_R1_footswitchDetect, &g_R1_footswitchInput
};

//=====================================================================================================================
// Protos
//=====================================================================================================================

static void initSysclock(void);

//=====================================================================================================================
// Functions
//=====================================================================================================================


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

    NVIC_SetPriority(SysTick_IRQn, 3);
    initSysclock();

    initTimer(&delayTimer);

    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);

    initGPIO_peripherals(&g_timerRev1PeriphPins);
    initGPIO_generic(&g_timerRev1GenericPins);

    i2cInit(g_R1_eepromI2C.pPeripheral, I2C_100KHZ, false);
    //i2cInit(g_R1_dispI2C.pPeripheral, I2C_1MHZ, true);
    spiInit(g_R1_dispSPI.pPeripheral);

    initTimer(&framerateTimer);
    initTimer(&enlargerTimer);
}

void hwDelayMs(uint32_t ms)
{
    timerDelay(&delayTimer, ms);
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
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    // clock PLL at 64MHz: (16MHz / 1) * (16 / 2)
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    LL_RCC_PLL_EnableDomain_SYS();
    while (LL_RCC_PLL_IsReady() != 1);

    // sysclk uses HSI: max = 16MHz. if you want to go higher use main PLL
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    LL_Init1msTick(SYS_CLK_FREQ_HZ);
    LL_SetSystemCoreClock(SYS_CLK_FREQ_HZ);
}