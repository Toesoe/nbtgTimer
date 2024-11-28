add_library(freertos_config INTERFACE)

target_include_directories(freertos_config INTERFACE
    .
)

target_link_libraries(freertos_config INTERFACE stm32g0xx_sys)

set(FREERTOS_PORT GCC_ARM_CM0 CACHE STRING "")