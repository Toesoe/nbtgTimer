cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(TARGET_CPU "cortex-m0plus")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ${TARGET_CPU})
set(CMAKE_GENERATOR Ninja)

if (NOT DEFINED ENV{NBTG_TIMER_TOOLCHAIN})
    if (CMAKE_HOST_UNIX)
        set(TOOLCHAIN_PATH /opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin)
    endif()

    if (CMAKE_HOST_WIN32)
        set(TOOLCHAIN_PATH C:/SysGCC/arm-eabi/bin)
    endif()

    message(WARNING "NBTG_TIMER_TOOLCHAIN environment variable is not set. Using default path ${TOOLCHAIN_PATH}")
else()
    message(STATUS "Using toolchain located at $ENV{NBTG_TIMER_TOOLCHAIN}.")
    file(TO_CMAKE_PATH $ENV{NBTG_TIMER_TOOLCHAIN} TOOLCHAIN_PATH)
endif()

if (CMAKE_HOST_WIN32)
    set(TOOL_SUFFIX .exe)
    set(CMAKE_COLOR_MAKEFILE OFF)
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # we need gnu ext for asm calls
# set(CMAKE_VERBOSE_MAKEFILE ON) # enable this if you want to debug make issues
set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER_WORKS TRUE) # prevent errors on initial test compile

set(CMAKE_C_COMPILER        ${TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOL_SUFFIX} CACHE PATH "" FORCE)
set(CMAKE_CXX_COMPILER      ${TOOLCHAIN_PATH}/arm-none-eabi-g++${TOOL_SUFFIX} CACHE PATH "" FORCE)
set(CMAKE_ASM_COMPILER      ${TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOL_SUFFIX} CACHE PATH "" FORCE)
set(CMAKE_SIZE_UTIL         ${TOOLCHAIN_PATH}/arm-none-eabi-size${TOOL_SUFFIX})
set(CMAKE_OBJCOPY           ${TOOLCHAIN_PATH}/arm-none-eabi-objcopy${TOOL_SUFFIX})

set(COMMON_FLAGS      "-mcpu=${TARGET_CPU} -mthumb -fmax-errors=5 -msoft-float -mfloat-abi=soft -MD")
set(WARN_FLAGS        "-Wall -Wextra -Wpointer-arith -Wformat -Wno-unused-local-typedefs -Wno-unused-parameter -Wfloat-equal \
                       -Wshadow -Wwrite-strings -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes")
set(C_CXX_FLAGS       "-ffunction-sections -fdata-sections")

set(CMAKE_C_FLAGS_INIT          "${COMMON_FLAGS} ${C_CXX_FLAGS} ${WARN_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_INIT        "${COMMON_FLAGS} -x assembler-with-cpp"        CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -nostdlib -nostartfiles -mthumb -msoft-float -lc -lm"    CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_EXPORT_COMPILE_COMMANDS             ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES    ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES   ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS     ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES  ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS   ON)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE           ON)

option(BUILD_DOC   "Build documentation" ON)
option(BUILD_TESTS "Build test programs" OFF)

set(DISABLE_LIB_TESTS ON)