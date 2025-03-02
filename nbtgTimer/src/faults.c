/**
 * @file faults.c
 *
 * @brief faulthandlers
 */

#include "faults.h"

#include <FreeRTOS.h>
#include <stm32g0xx.h>
#include <task.h>

#include "board.h"

//=====================================================================================================================
// Typedefs
//=====================================================================================================================
/** @brief struct which corresponds to the order in which the Cortex core's register values are dumped onto the stack
 *         in case of a fault. can be used to quickly evaluate the system context at the time of the fault
 */
typedef struct __attribute__((packed)) ContextStateFrame
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t return_address;
    uint32_t xpsr;
} sContextStateFrame;

void faultHandler(const sContextStateFrame *); // proto to placate the compiler
void unwindStack(void);

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief fault handler
 *
 * @param pFrame current stack frame. this works because R0 contains the value the stack pointer had before the fault
 * occured and we look from the stack downwards to obtain the context frame
 */
void faultHandler(const sContextStateFrame *pFrame)
{
    do
    {
        asm volatile("bkpt");
    }
    while (0);

    // we break here; use GDB to analyze pFrame.
}

/**
 * @brief a smidge of Thumb16 assembler to check which stack pointer was in use. will branch unconditionally to the
 * fault handler
 *
 */
void unwindStack(void)
{
    // check whether MSP or PSP was in use, push value to R0
    // note: we're in Thumb16, so we can't TST on LR or immediates, we need an unshifted reg
    asm volatile("mov r0, lr \n"
                 "ldr r4, =4 \n"
                 "tst r0, r4 \n"
                 "beq use_msp \n" // Branch if equal (bit 2 of LR is clear); MSP was in use
                 "mrs r0, psp \n" // Move Process Stack Pointer to R0
                 "b faultHandler \n"
                 "use_msp: \n"
                 "mrs r0, msp \n" // Move Main Stack Pointer to R0
                 "b faultHandler \n");
}

/**
 * @brief handle hardfaults
 *
 */
void HardFault_Handler(void)
{
    unwindStack();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes" // ignored: protos are in FreeRTOS

void vAssertCalled(unsigned long ulLine, const char *const pcFileName)
{
    volatile uint32_t  ulSetToNonZeroInDebuggerToContinue = 0;

#ifdef DEBUG
    taskENTER_CRITICAL();

    /* You can step out of this function to debug the assertion by using
    the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
    value. */
    while (ulSetToNonZeroInDebuggerToContinue == 0)
    {
        asm("bkpt 1");
    }

    taskEXIT_CRITICAL();
#endif

    NVIC_SystemReset();
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    volatile uint32_t  ulSetToNonZeroInDebuggerToContinue = 0;

#ifdef DEBUG
    taskENTER_CRITICAL();

    /* You can step out of this function to debug the assertion by using
    the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
    value. */
    while (ulSetToNonZeroInDebuggerToContinue == 0)
    {
        asm("bkpt 1");
    }

    taskEXIT_CRITICAL();
#endif
    NVIC_SystemReset();
}

#pragma GCC diagnostic pop