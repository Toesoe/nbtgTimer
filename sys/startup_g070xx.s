/**
 * 32G0 startup file, THUMB2 asm
 * clean commented version adapted from ST's cube code (BSD 3-clause)
 */

.syntax unified             @ use unified ARM syntax
.cpu    cortex-m0plus
.fpu    softvfp
.thumb

.global g_pfnVectors        @ global ptr to vector table
.global Default_Handler     @ default interrupt handler

.word   _estack             @ highest stack address (start)
.word   _sidata             @ start address for data section
.word   _sdata              @ data start
.word   _edata              @ data end
.word   _sbss               @ bss start
.word   _ebss               @ bss end

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr     r0, =_estack
    mov     sp, r0          @ set sp

@ initialize data segment with values from flash. data size is (_sdata - _edata), in flash at _sidata
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
    movs    r3, #0          @ counter register

CopyData:
    ldr     r4, [r2, r3]    @ copy word at &(_sidata + counter) to R4  (FLASH -> REG)
    str     r4, [r0, r3]    @ copy word from R4 to &(_sdata + counter) (REG -> RAM)
    adds    r3, r3, #4      @ set R3 to R3 + 4
    adds    r4, r0, r3      @ set R4 to R0 + R3 (&_sdata + counter): address to write next value in RAM to
    cmp     r4, r1          @ compare R4 to _edata: if equal, we're done
    bcc     CopyData

@ zero BSS region
    ldr     r2, =_sbss
    ldr     r4, =_ebss
    movs    r3, #0
    b       ZeroBSS

FillZeroBSS:
    str     r3, [r2]        @ write #0 from R3 to addr in R2
    adds    r2, r2, #4      @ increment counter

ZeroBSS:
    cmp     r2, r4          @ reached the end?
    bcc     FillZeroBSS     @ nope

InitFunctions:
    bl SystemInit           @ set clocks, VTOR
    bl main                 @ we branch with link so if main returns we end up in an infinite loop
LoopForever:
    b LoopForever

.size Reset_Handler, .-Reset_Handler

@ default interrupt handler, branches to infinite loop above
.section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b LoopForever

.size Default_Handler, .-Default_Handler

/******************************************************************************
*
* The minimal vector table for a Cortex M0.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
.section .isr_vector,"a",%progbits
.type   g_pfnVectors, %object

g_pfnVectors:
.word   _estack
.word   Reset_Handler
.word   NMI_Handler
.word   HardFault_Handler
.word   0
.word   0
.word   0
.word   0
.word   0
.word   0
.word   0
.word   SVC_Handler
.word   0
.word   0
.word   PendSV_Handler
.word   SysTick_Handler
.word   WWDG_IRQHandler                   /* Window WatchDog              */
.word   0                                /* reserved                     */
.word   RTC_TAMP_IRQHandler               /* RTC through the EXTI line    */
.word   FLASH_IRQHandler                  /* FLASH                        */
.word   RCC_IRQHandler                    /* RCC                          */
.word   EXTI0_1_IRQHandler                /* EXTI Line 0 and 1            */
.word   EXTI2_3_IRQHandler                /* EXTI Line 2 and 3            */
.word   EXTI4_15_IRQHandler               /* EXTI Line 4 to 15            */
.word   0                                 /* reserved                     */
.word   DMA1_Channel1_IRQHandler          /* DMA1 Channel 1               */
.word   DMA1_Channel2_3_IRQHandler        /* DMA1 Channel 2 and Channel 3 */
.word   DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler /* DMA1 Channel 4 to Channel 7, DMAMUX1 overrun */
.word   ADC1_IRQHandler                   /* ADC1                         */
.word   TIM1_BRK_UP_TRG_COM_IRQHandler    /* TIM1 Break, Update, Trigger and Commutation */
.word   TIM1_CC_IRQHandler                /* TIM1 Capture Compare         */
.word   0                                 /* reserved                     */
.word   TIM3_IRQHandler                   /* TIM3                         */
.word   TIM6_IRQHandler                   /* TIM6                         */
.word   TIM7_IRQHandler                   /* TIM7                         */
.word   TIM14_IRQHandler                  /* TIM14                        */
.word   TIM15_IRQHandler                  /* TIM15                        */
.word   TIM16_IRQHandler                  /* TIM16                        */
.word   TIM17_IRQHandler                  /* TIM17                        */
.word   I2C1_IRQHandler                   /* I2C1                         */
.word   I2C2_IRQHandler                   /* I2C2                         */
.word   SPI1_IRQHandler                   /* SPI1                         */
.word   SPI2_IRQHandler                   /* SPI2                         */
.word   USART1_IRQHandler                 /* USART1                       */
.word   USART2_IRQHandler                 /* USART2                       */
.word   USART3_4_IRQHandler               /* USART3, USART4               */

.size   g_pfnVectors, .-g_pfnVectors

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/

.weak      NMI_Handler
.thumb_set NMI_Handler,Default_Handler

.weak      HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler

.weak      SVC_Handler
.thumb_set SVC_Handler,Default_Handler

.weak      PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler

.weak      SysTick_Handler
.thumb_set SysTick_Handler,Default_Handler

.weak      WWDG_IRQHandler
.thumb_set WWDG_IRQHandler,Default_Handler

.weak      RTC_TAMP_IRQHandler
.thumb_set RTC_TAMP_IRQHandler,Default_Handler

.weak      FLASH_IRQHandler
.thumb_set FLASH_IRQHandler,Default_Handler

.weak      RCC_IRQHandler
.thumb_set RCC_IRQHandler,Default_Handler

.weak      EXTI0_1_IRQHandler
.thumb_set EXTI0_1_IRQHandler,Default_Handler

.weak      EXTI2_3_IRQHandler
.thumb_set EXTI2_3_IRQHandler,Default_Handler

.weak      EXTI4_15_IRQHandler
.thumb_set EXTI4_15_IRQHandler,Default_Handler

.weak      DMA1_Channel1_IRQHandler
.thumb_set DMA1_Channel1_IRQHandler,Default_Handler

.weak      DMA1_Channel2_3_IRQHandler
.thumb_set DMA1_Channel2_3_IRQHandler,Default_Handler

.weak      DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler
.thumb_set DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler,Default_Handler

.weak      ADC1_IRQHandler
.thumb_set ADC1_IRQHandler,Default_Handler

.weak      TIM1_BRK_UP_TRG_COM_IRQHandler
.thumb_set TIM1_BRK_UP_TRG_COM_IRQHandler,Default_Handler

.weak      TIM1_CC_IRQHandler
.thumb_set TIM1_CC_IRQHandler,Default_Handler

.weak      TIM3_IRQHandler
.thumb_set TIM3_IRQHandler,Default_Handler

.weak      TIM6_IRQHandler
.thumb_set TIM6_IRQHandler,Default_Handler

.weak      TIM7_IRQHandler
.thumb_set TIM7_IRQHandler,Default_Handler

.weak      TIM14_IRQHandler
.thumb_set TIM14_IRQHandler,Default_Handler

.weak      TIM15_IRQHandler
.thumb_set TIM15_IRQHandler,Default_Handler

.weak      TIM16_IRQHandler
.thumb_set TIM16_IRQHandler,Default_Handler

.weak      TIM17_IRQHandler
.thumb_set TIM17_IRQHandler,Default_Handler

.weak      I2C1_IRQHandler
.thumb_set I2C1_IRQHandler,Default_Handler

.weak      I2C2_IRQHandler
.thumb_set I2C2_IRQHandler,Default_Handler

.weak      SPI1_IRQHandler
.thumb_set SPI1_IRQHandler,Default_Handler

.weak      SPI2_IRQHandler
.thumb_set SPI2_IRQHandler,Default_Handler

.weak      USART1_IRQHandler
.thumb_set USART1_IRQHandler,Default_Handler

.weak      USART2_IRQHandler
.thumb_set USART2_IRQHandler,Default_Handler

.weak      USART3_4_IRQHandler
.thumb_set USART3_4_IRQHandler,Default_Handler
