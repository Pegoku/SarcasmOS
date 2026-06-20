cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(sarcasmos_mouth C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(sarcasmos_mouth src/main.cpp)
target_include_directories(sarcasmos_mouth PRIVATE include)
if(NOT DEFINED I2C_ADDRESS)
    set(I2C_ADDRESS 0x32)
endif()
target_compile_definitions(sarcasmos_mouth PRIVATE I2C_ADDRESS=${I2C_ADDRESS})
target_link_libraries(sarcasmos_mouth pico_stdlib hardware_i2c hardware_watchdog)
pico_enable_stdio_uart(sarcasmos_mouth 1)
pico_enable_stdio_usb(sarcasmos_mouth 0)
pico_add_extra_outputs(sarcasmos_mouth)
