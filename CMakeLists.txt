cmake_minimum_required(VERSION 3.16.0)

cmake_policy(SET CMP0135 NEW)

project(nbtgTimer
    DESCRIPTION "nbtgTimer firmware"
    LANGUAGES C CXX ASM
    VERSION "0.0.1"
)

set(TARGET_FILE "nbtgTimer")
add_executable(${TARGET_FILE}.elf)

if (NOT BUILD_TESTS)
    message("\n----------------------------------------------------")
    message("        Building nbtgTimer firmware version ${PROJECT_VERSION}.")
    message("             Compiler version: ${CMAKE_C_COMPILER_VERSION}")
    message("----------------------------------------------------\n")
endif()

set(FW_ELF_FILE   "${TARGET_FILE}.elf")
set(FW_HEX_FILE   "${TARGET_FILE}.hex")
set(FW_MAP_FILE   "${TARGET_FILE}.map")

set(CMAKE_C_FLAGS_DEBUG     "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_CXX_FLAGS_DEBUG   "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_C_FLAGS_RELEASE   "-O2 -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fstack-protector -s")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fstack-protector -s")

# cascading definitions
add_compile_definitions(
    _GNU_SOURCE
    STM32G070xx
    USE_FULL_LL_DRIVER
    FW_VERSION=\"${CMAKE_PROJECT_VERSION}\"
    $<$<CONFIG:Debug>:DEBUG>
)

add_subdirectory(nbtgTimer)
add_subdirectory(sys)
add_subdirectory(lib)

target_sources(${FW_ELF_FILE} PRIVATE
    ${CMAKE_SOURCE_DIR}/sys/stm32g0xx_sys/CMSIS/Device/system_stm32g0xx.c
    ${CMAKE_SOURCE_DIR}/sys/stm32g0xx_sys/startup_g070xx.s
)

target_link_libraries(${FW_ELF_FILE} PRIVATE
    stm32g0xx_sys
    stm32g0xx_ll
    freertos_kernel
    freertos_config
    bsp
    #tim_drv
)

target_link_options(${FW_ELF_FILE} PRIVATE
    -Wl,-Map=${FW_MAP_FILE},--cref
    -T${CMAKE_CURRENT_SOURCE_DIR}/sys/stm32g0xx_sys/nbtgTimer.ld
)

# Don't enforce warnings on external libraries
target_compile_options(freertos_kernel PRIVATE
    -w  # -w disables all gcc warnings
)

add_custom_command(TARGET ${FW_ELF_FILE} POST_BUILD
    COMMAND ${CMAKE_SIZE_UTIL} ${FW_ELF_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${FW_ELF_FILE} ${FW_HEX_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (BUILD_DOC)

    find_package(Doxygen)

    if (DOXYGEN_FOUND)
        # Set input and output files
        set(DOXYGEN_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/Doxygen)

        message("Doxygen build for ${PROJECT_NAME} started")

        add_custom_target(kwh100docs ALL
            COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else (DOXYGEN_FOUND)
        message("Doxygen needs to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)

endif()