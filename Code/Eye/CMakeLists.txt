cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(sarcasmos_eye C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(EYE_ASSET_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/generated/eye_assets.hpp)
add_custom_command(
    OUTPUT ${EYE_ASSET_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/asset_tool.py compile
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/asset_tool.py
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/eye_assets.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling eye animation assets"
)
add_custom_target(generate_eye_assets DEPENDS ${EYE_ASSET_HEADER})

add_executable(sarcasmos_eye src/main.cpp src/gc9a01.cpp)
add_dependencies(sarcasmos_eye generate_eye_assets)
target_include_directories(sarcasmos_eye PRIVATE include ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED DEVICE_ROLE)
    set(DEVICE_ROLE 0)
endif()
if(NOT DEFINED I2C_ADDRESS)
    set(I2C_ADDRESS 0x30)
endif()
option(ANIMATION_AUTOPLAY "Cycle through implemented eye animations" OFF)
target_compile_definitions(sarcasmos_eye PRIVATE DEVICE_ROLE=${DEVICE_ROLE} I2C_ADDRESS=${I2C_ADDRESS})
if(ANIMATION_AUTOPLAY)
    target_compile_definitions(sarcasmos_eye PRIVATE ANIMATION_AUTOPLAY=1)
endif()
target_link_libraries(sarcasmos_eye pico_stdlib hardware_i2c hardware_spi hardware_pwm hardware_watchdog)
pico_enable_stdio_uart(sarcasmos_eye 1)
pico_enable_stdio_usb(sarcasmos_eye 0)
pico_add_extra_outputs(sarcasmos_eye)

add_executable(sarcasmos_eye_display_test src/display_test.cpp src/gc9a01.cpp)
target_include_directories(sarcasmos_eye_display_test PRIVATE include)
target_link_libraries(sarcasmos_eye_display_test
    pico_stdlib
    hardware_pwm
    hardware_spi
    hardware_watchdog
)
pico_enable_stdio_uart(sarcasmos_eye_display_test 1)
pico_enable_stdio_usb(sarcasmos_eye_display_test 0)
pico_add_extra_outputs(sarcasmos_eye_display_test)
