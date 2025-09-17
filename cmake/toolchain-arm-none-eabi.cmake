cmake_minimum_required(VERSION 3.18)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Allow caller to override toolchain root; default to repo-provided GCC
if(NOT DEFINED GNU_ARM_TOOLCHAIN_DIR)
  set(GNU_ARM_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/gcc")
endif()

# Compilers
set(CMAKE_C_COMPILER   "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc.exe" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc.exe" CACHE FILEPATH "")

# Binutils
find_program(ARM_OBJCOPY NAMES arm-none-eabi-objcopy.exe arm-none-eabi-objcopy objcopy.exe
             HINTS "${GNU_ARM_TOOLCHAIN_DIR}/bin" "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin")
if(NOT ARM_OBJCOPY AND EXISTS "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-objcopy.exe")
  set(ARM_OBJCOPY "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-objcopy.exe")
elseif(NOT ARM_OBJCOPY AND EXISTS "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin/objcopy.exe")
  set(ARM_OBJCOPY "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin/objcopy.exe")
endif()
find_program(ARM_SIZE NAMES arm-none-eabi-size.exe arm-none-eabi-size size.exe
             HINTS "${GNU_ARM_TOOLCHAIN_DIR}/bin" "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin")
if(NOT ARM_SIZE AND EXISTS "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-size.exe")
  set(ARM_SIZE "${GNU_ARM_TOOLCHAIN_DIR}/bin/arm-none-eabi-size.exe")
elseif(NOT ARM_SIZE AND EXISTS "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin/size.exe")
  set(ARM_SIZE "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin/size.exe")
endif()
find_program(ARM_OBJDUMP NAMES arm-none-eabi-objdump.exe arm-none-eabi-objdump objdump.exe
             HINTS "${GNU_ARM_TOOLCHAIN_DIR}/bin" "${GNU_ARM_TOOLCHAIN_DIR}/arm-none-eabi/bin")

set(CMAKE_OBJCOPY "${ARM_OBJCOPY}" CACHE FILEPATH "")
set(CMAKE_SIZE    "${ARM_SIZE}" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${ARM_OBJDUMP}" CACHE FILEPATH "")

# Common flags
set(COMMON_CPU_FLAGS "-mcpu=arm926ej-s -marm -msoft-float")
set(COMMON_C_FLAGS   "${COMMON_CPU_FLAGS} -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -std=gnu11")
set(COMMON_ASM_FLAGS "${COMMON_CPU_FLAGS}")

set(CMAKE_C_FLAGS_INIT "${COMMON_C_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${COMMON_ASM_FLAGS}")

set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
