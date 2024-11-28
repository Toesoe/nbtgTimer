add_library(stm32g0xx_sys INTERFACE)

target_include_directories(stm32g0xx_sys INTERFACE
    .
    CMSIS/Core/Include
    CMSIS/Device
)