cmake_minimum_required(VERSION 3.13)

# ── Target board: Raspberry Pi Pico 2 (RP2350) ──
set(PICO_BOARD pico2)

# Pull in Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(SUSFsensors C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(SUSFsensors
    SUSFsensors.c
)

target_link_libraries(SUSFsensors
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_timer
    m
)

# Enable USB serial (stdout -> USB CDC at 115200) and disable UART
pico_enable_stdio_usb(SUSFsensors 1)
pico_enable_stdio_uart(SUSFsensors 0)

# Generate UF2 / map / dis files
pico_add_extra_outputs(SUSFsensors)
