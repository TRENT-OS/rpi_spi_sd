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
function(DeclareCAmkESComponent_RPI_SPI_MSD name)

    cmake_parse_arguments(
        PARSE_ARGV
        1
        RPI_SPI_MSD
        ""
        ""
        "LIBS"
    )

    DeclareCAmkESComponent(
        ${name}
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RPI_SPI_SD.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bcm2837_gpio.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bcm2837_spi.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SDFileSystem.c
            # ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SPI_MSD_Driver.c
        C_FLAGS
            -Wall
            -Werror
        LIBS
            os_core_api
            os_libs
            ${RPI_SPI_MSD_LIBS}
    )

endfunction()
