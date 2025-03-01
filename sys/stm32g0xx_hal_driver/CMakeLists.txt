add_library(stm32g0xx_ll
    Src/stm32g0xx_ll_adc.c
    Src/stm32g0xx_ll_comp.c
    Src/stm32g0xx_ll_crc.c
    Src/stm32g0xx_ll_crs.c
    Src/stm32g0xx_ll_dac.c
    Src/stm32g0xx_ll_dma.c
    Src/stm32g0xx_ll_exti.c
    Src/stm32g0xx_ll_gpio.c
    Src/stm32g0xx_ll_i2c.c
    Src/stm32g0xx_ll_lptim.c
    Src/stm32g0xx_ll_lpuart.c
    Src/stm32g0xx_ll_pwr.c
    Src/stm32g0xx_ll_rcc.c
    Src/stm32g0xx_ll_rng.c
    Src/stm32g0xx_ll_rtc.c
    Src/stm32g0xx_ll_spi.c
    Src/stm32g0xx_ll_tim.c
    Src/stm32g0xx_ll_ucpd.c
    Src/stm32g0xx_ll_usart.c
    Src/stm32g0xx_ll_utils.c
)

target_include_directories(stm32g0xx_ll PUBLIC
    Inc
    ../stm32g0xx_sys/CMSIS/Device
)

# don't enforce warnings on external libraries
target_compile_options(stm32g0xx_ll PRIVATE
    -w  # -w disables all gcc warnings
)

target_link_libraries(stm32g0xx_ll PUBLIC
    stm32g0xx_sys # stm32g0xx.h
)