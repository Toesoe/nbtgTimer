add_library(bsp
    src/i2c.c
    src/gpio.c
    src/timer.c
    #src/uart.c
    src/board.c
)

target_include_directories(bsp PUBLIC
    inc
)

target_link_libraries(bsp PUBLIC
    stm32g0xx_ll
    stm32g0xx_sys
    freertos_kernel
)