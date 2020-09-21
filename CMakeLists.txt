#
# RasPi3 (BCM2837) SPI LED driver
#
# Copyright (C) 2020, Hensoldt Cyber GmbH
#

cmake_minimum_required(VERSION 3.17)

#-------------------------------------------------------------------------------
# Declare RasPi3 (BCM2837) SPI MSD Driver CAmkES Component
#       TODO
#
# Parameters:
#
#   <name>
#     required, component instance name
#
#   LIBS <lib_1> <lib_2> ...
#     additional libraries. At least a TimeServer client library is required
#
function(RPi_SPI_SD_DeclareCAmkESComponent name)

    cmake_parse_arguments(
        PARSE_ARGV
        1
        RPi_SPI_SD
        ""
        ""
        "LIBS"
    )

    DeclareCAmkESComponent(
        ${name}
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RPi_SPI_SD.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bcm2837_gpio.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bcm2837_spi.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SDFileSystem.c
        C_FLAGS
            -Wall
            -Werror
        LIBS
            os_core_api
            lib_debug
            ${RPi_SPI_SD_LIBS}
    )

endfunction()
