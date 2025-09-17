cmake_minimum_required(VERSION 3.18)
project(ultra700 C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories (place artifacts relative to project root)
set(OUT_DIR "${CMAKE_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${OUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}")

# Common include directories
set(PROJ_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
include_directories(
  ${PROJ_SRC_DIR}
  ${PROJ_SRC_DIR}/Lib
  ${PROJ_SRC_DIR}/Lib/USB
  ${PROJ_SRC_DIR}/Lib/MT6261
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers
  ${PROJ_SRC_DIR}/Application
  ${PROJ_SRC_DIR}/Application/Drivers
  ${PROJ_SRC_DIR}/System
  ${PROJ_SRC_DIR}/GUI
  ${PROJ_SRC_DIR}/GUI/Resource
  ${PROJ_SRC_DIR}/Bootloader
)

enable_language(ASM)

# Helper to collect sources by folder
function(collect_sources out_var)
  set(sources ${ARGN})
  set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# Startup assembly common for both
set(STARTUP "${PROJ_SRC_DIR}/startup_MT6261x.S")

# Minimal system sources used by bootloader
set(SYS_BOOT_MIN_SRCS
  ${PROJ_SRC_DIR}/System/asmutils.s
  ${PROJ_SRC_DIR}/System/exchandlers.s
  ${PROJ_SRC_DIR}/System/debug.c
  ${PROJ_SRC_DIR}/System/utils.c
)

# Full system sources used by payload
set(SYS_FULL_SRCS
  ${PROJ_SRC_DIR}/System/asmutils.s
  ${PROJ_SRC_DIR}/System/exchandlers.s
  ${PROJ_SRC_DIR}/System/crc.c
  ${PROJ_SRC_DIR}/System/debug.c
  ${PROJ_SRC_DIR}/System/dlist.c
  ${PROJ_SRC_DIR}/System/evmngr.c
  ${PROJ_SRC_DIR}/System/fscache.c
  ${PROJ_SRC_DIR}/System/init.c
  ${PROJ_SRC_DIR}/System/lrtimer.c
  ${PROJ_SRC_DIR}/System/memory.c
  ${PROJ_SRC_DIR}/System/pmngr.c
  ${PROJ_SRC_DIR}/System/resource.c
  ${PROJ_SRC_DIR}/System/ringbuf.c
  ${PROJ_SRC_DIR}/System/sf.c
  ${PROJ_SRC_DIR}/System/tlsf.c
  ${PROJ_SRC_DIR}/System/utils.c
)

# Driver sources for bootloader (minimal)
set(DRIVER_BOOT_MIN_SRCS
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/emi.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/gpio.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/nvic.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/pll.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/rgu.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/tcmcache.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/usart.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/ustimer.c
)

# Full driver sources for payload
set(DRIVER_FULL_SRCS
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/auxadc.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/afe.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/emi.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/gpio.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/gpt.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/lcdif.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/msdc.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/nvic.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/pctl.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/pll.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/pmu.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/pwm.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/rgu.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/rtc.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/sfi.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/spi.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/tcmcache.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/usart.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/usb.c
  ${PROJ_SRC_DIR}/Lib/MT6261/Drivers/ustimer.c
  ${PROJ_SRC_DIR}/Lib/USB/usb9.c
)

# GUI sources (optional minimal set; include if present)
set(GUI_SRCS
  ${PROJ_SRC_DIR}/GUI/gdi.c
  ${PROJ_SRC_DIR}/GUI/gdifont.c
  ${PROJ_SRC_DIR}/GUI/gdiutils.c
  ${PROJ_SRC_DIR}/GUI/gui.c
  ${PROJ_SRC_DIR}/GUI/guibutton.c
  ${PROJ_SRC_DIR}/GUI/guilabel.c
  ${PROJ_SRC_DIR}/GUI/guiobject.c
  ${PROJ_SRC_DIR}/GUI/guiwin.c
)

set(APP_HEADER_SRC   ${PROJ_SRC_DIR}/Lib/appheader.c)
set(BOOT_HEADER_SRC  ${PROJ_SRC_DIR}/Lib/bootheader.c)

# Bootloader target
set(BOOT_SRCS
  ${STARTUP}
  ${PROJ_SRC_DIR}/Bootloader/DZ09_boot.c
  ${PROJ_SRC_DIR}/Bootloader/sha-1.c
  ${BOOT_HEADER_SRC}
  ${SYS_BOOT_MIN_SRCS}
  ${DRIVER_BOOT_MIN_SRCS}
)

add_executable(bootloader.elf ${BOOT_SRCS})
target_compile_definitions(bootloader.elf PRIVATE TARGET_BOOTLOADER)
set(BOOT_LD "${CMAKE_SOURCE_DIR}/MT6261A_boot.ld")
target_link_options(bootloader.elf PRIVATE
  -T${BOOT_LD}
  -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
  -Wl,--gc-sections -Wl,-Map=${OUT_DIR}/bootloader.map -Wl,-e,__reset_handler -nostartfiles -static
)
target_link_libraries(bootloader.elf PRIVATE c gcc nosys)

# Payload (system) target
set(PAYLOAD_SRCS
  ${STARTUP}
  ${PROJ_SRC_DIR}/System/DZ09.c
  ${APP_HEADER_SRC}
  ${SYS_FULL_SRCS}
  ${DRIVER_FULL_SRCS}
  ${GUI_SRCS}
  ${PROJ_SRC_DIR}/Application/appinit.c
  ${PROJ_SRC_DIR}/Application/audio_note.c
  ${PROJ_SRC_DIR}/Application/Drivers/backlight.c
  ${PROJ_SRC_DIR}/Application/Drivers/delay.c
  ${PROJ_SRC_DIR}/Application/Drivers/dflib.c
  ${PROJ_SRC_DIR}/Application/Drivers/ft6236.c
  ${PROJ_SRC_DIR}/Application/Drivers/ili9341.c
  ${PROJ_SRC_DIR}/Application/Drivers/simple_keypad.c
  ${PROJ_SRC_DIR}/Application/Drivers/single_led.c
  ${PROJ_SRC_DIR}/Application/Drivers/sdcard.c
  ${PROJ_SRC_DIR}/Application/pcm_player.c
  ${PROJ_SRC_DIR}/Application/Drivers/usbdevice_cdc.c
)

add_executable(payload.elf ${PAYLOAD_SRCS})
target_compile_definitions(payload.elf PRIVATE TARGET_SYSTEM)
set(PAYLOAD_LD "${CMAKE_SOURCE_DIR}/MT6261A.ld")
target_link_options(payload.elf PRIVATE
  -T${PAYLOAD_LD}
  -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
  -Wl,--gc-sections -Wl,-Map=${OUT_DIR}/payload.map -Wl,-e,__reset_handler -nostartfiles -static
)
target_link_libraries(payload.elf PRIVATE c gcc nosys)
target_link_libraries(payload.elf PRIVATE "${PROJ_SRC_DIR}/GUI/Resource/fontlib.a")

# Common warnings and tuning
foreach(tgt bootloader.elf payload.elf)
  target_compile_options(${tgt} PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
  )
  # Size after link
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${tgt}>
    COMMENT "Size of $<TARGET_FILE_NAME:${tgt}>"
  )
endforeach()

# Favor size for bootloader to fit TCM
target_compile_options(bootloader.elf PRIVATE -Os)

# Convert to BIN/HEX and sign using tools/mtk_sign.exe if available
set(SIGN_TOOL "${CMAKE_SOURCE_DIR}/tools/mtk_sign.exe")
set(MTK_SIGN_ARGS "" CACHE STRING "Additional arguments for mtk_sign.exe")
if(EXISTS "${SIGN_TOOL}")
  set(SIGN_CMD "${SIGN_TOOL}")
else()
  set(SIGN_CMD "")
endif()

function(add_artifacts tgt stem)
  set(elf "$<TARGET_FILE:${tgt}>")
  set(bin "${OUT_DIR}/${stem}.bin")
  set(hex "${OUT_DIR}/${stem}.hex")
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${elf} ${bin}
    COMMAND ${CMAKE_OBJCOPY} -O ihex   ${elf} ${hex}
    COMMENT "Generating ${stem}.bin and ${stem}.hex"
  )
  if(SIGN_CMD)
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND "${SIGN_CMD}" ${MTK_SIGN_ARGS} ${bin}
      COMMENT "Signing ${stem}.bin"
    )
  endif()
endfunction()

add_artifacts(bootloader.elf bootloader)
add_artifacts(payload.elf    payload)

message(STATUS "Build outputs in: ${OUT_DIR}")
