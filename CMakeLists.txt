# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(james C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(rocket rocket.cpp)
add_executable(logdump logdump.cpp)

pico_set_program_name(rocket "james-rocket")
pico_set_program_version(rocket "1.0")
pico_set_program_name(logdump "james-logdump")
pico_set_program_version(logdump "1.0")


pico_enable_stdio_uart(rocket 0)
pico_enable_stdio_usb(rocket 1)
pico_enable_stdio_uart(logdump 0)
pico_enable_stdio_usb(logdump 1)

add_library(pico_servo servo.h servo.cpp)
add_library(pico_ADXL345 ADXL345_RP2040/ADXL345.h ADXL345_RP2040/ADXL345.cpp)
add_library(littlefs littlefs/lfs.c littlefs/lfs.h littlefs/lfs_util.c littlefs/lfs_util.h)

target_link_libraries(pico_servo pico_stdlib hardware_pwm)
target_link_libraries(pico_ADXL345 pico_stdlib hardware_i2c)

# Add any user requested libraries
target_link_libraries(rocket
        pico_stdlib
        hardware_i2c
        hardware_pwm
        pico_servo
        pico_ADXL345
        littlefs
)

target_link_libraries(logdump
        pico_stdlib
        littlefs
)

pico_add_extra_outputs(rocket)
pico_add_extra_outputs(logdump)
