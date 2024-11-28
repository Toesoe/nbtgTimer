/**
 * @file uart.c
 *
 * @brief uart functionality
 * 
 * TODO: verify ISR execution time with dual ifs
 * TODO: add DMA when the above TODO is done
 * TODO: timeouts on transmission (when TXNE stays high e.g. data is not flushed out)
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "uart.h"

#include <stm32g0xx_ll_bus.h>
#include <stm32g0xx_ll_gpio.h>
#include <stm32g0xx_ll_usart.h>
#include <stm32g0xx_ll_rcc.h>
#include <stm32g0xx_ll_dma.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

static QueueHandle_t const *pConsoleRXQueue = NULL;

static USART_TypeDef *g_pConsoleUsart = NULL;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================


//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief initialize UART peripheral
 * 
 * @param pPeripheral USART peripheral (USART1/USART2/USART3)
 * @param baudrate baudrate to use (modbus recommended 9600, console 115200)
 * 
 * @note USARTs have their RX channel disabled on startup to prevent firing RX interrupts
 */
void initUsart(USART_TypeDef *pPeripheral, uint32_t baudrate)
{
    if (pPeripheral == USART1)
    {
        NVIC_EnableIRQ(USART1_IRQn);
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
        LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_SYSCLK);
    }
    else if (pPeripheral == USART2)
    {
        NVIC_EnableIRQ(USART2_IRQn);
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
        LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_SYSCLK);
    }

    LL_USART_SetTransferDirection(pPeripheral, LL_USART_DIRECTION_TX_RX);
    LL_USART_ConfigCharacter(pPeripheral, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1); // 8 bits, no parity, 1 start & stop bit
    LL_USART_SetBaudRate(pPeripheral, SystemCoreClock, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, baudrate);

    LL_USART_Enable(pPeripheral);

    // wait for usart3 to come up, tx + rx enable ack flags should be set
    while ((!(LL_USART_IsActiveFlag_TEACK(pPeripheral))) || (!(LL_USART_IsActiveFlag_REACK(pPeripheral))));

    LL_USART_DisableDirectionRx(pPeripheral);

    LL_USART_ClearFlag_ORE(pPeripheral);
    LL_USART_EnableIT_RXNE_RXFNE(pPeripheral);
    LL_USART_EnableIT_ERROR(pPeripheral);

    LL_USART_SetTXFIFOThreshold(pPeripheral, LL_USART_FIFOTHRESHOLD_1_8);
    LL_USART_SetRXFIFOThreshold(pPeripheral, LL_USART_FIFOTHRESHOLD_1_8);

    LL_USART_DisableFIFO(pPeripheral);
}

void toggleUsartRX(bool enable)
{
    enable ? LL_USART_EnableDirectionRx(g_pConsoleUsart) : LL_USART_DisableDirectionRx(g_pConsoleUsart);
    while (g_pConsoleUsart->ISR & USART_ISR_RXNE_RXFNE) { (void)g_pConsoleUsart->RDR; } // flush FIFO
}

/**
 * @brief blocking sendchar function for console; waits for TXE flag to be set
 * 
 * @param c char to send
 */
void consolePutchar(char c)
{
    while (!LL_USART_IsActiveFlag_TXE_TXFNF(g_pConsoleUsart)) { /* infinite wait */ }
    LL_USART_TransmitData8(g_pConsoleUsart, c);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes" // ignored: no proto, but spawned in startup file

/**
 * @brief ST-named irq handler for usart1. posts current char directly onto queue via ISR-safe routine
 * TODO: handle IRQ errors
 */
__attribute__((interrupt)) void USART1_IRQHandler(void)
{
    uint8_t val = 0;
    LL_USART_ClearFlag_FE(USART1);
    LL_USART_ClearFlag_ORE(USART1);
    LL_USART_ClearFlag_NE(USART1);
    if (LL_USART_IsActiveFlag_RXNE_RXFNE(USART1))
    {
        val = LL_USART_ReceiveData8(USART1); // inlined
        xQueueSendToBackFromISR(*pConsoleRXQueue, &val, NULL);
    }
}

#pragma GCC diagnostic pop

//=====================================================================================================================
// Statics
//=====================================================================================================================