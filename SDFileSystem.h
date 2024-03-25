/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SDFILESYSTEM_H
#define SDFILESYSTEM_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "SDStructs.h"

/**
 * @brief Initializes disk.
 * 
 * @details Initializes SD card and sets block length.
 * 
 * @param spi   spi driver struct
 * @param hal   hardware abstraction layer for sd card
 * @param cfg   configuration struct for sd card
 * 
 * @return      Returns error code
 */
int disk_initialize(
    spisd_t* spi, 
    const spisd_hal_t* hal, 
    const spisd_config_t* cfg
);

/**
 * @brief Reads a number of blocks from SD card.
 * 
 * @details Reads a number of blocks from SD card starting from a certain block
 *          and writing data into the buffer.
 * 
 * @param spi           spi driver object
 * @param buffer        buffer is filled with read data
 * @param block_number  starting block from where to read data
 * @param count         number of blocks to read
 * 
 * @return              returns error code
 */
int disk_read(
    spisd_t* spi, 
    uint8_t* buffer, 
    off_t block_number, 
    off_t count
);

/**
 * @brief Writes a number of blocks to SD card.
 * 
 * @details Writes a number of blocks to SD card starting from a certain block
 *          by using data from buffer.
 * 
 * @param spi           spi driver object
 * @param buffer        holds data that is written to disk
 * @param block_number  starting block from where to write data
 * @param count         number of blocks to write 
 * 
 * @return              returns error code
 */
int disk_write(
    spisd_t* spi, 
    const uint8_t* buffer, 
    off_t block_number, 
    off_t count
);

/**
 * @brief Returns capacity of disk.
 * 
 * @param spi   spi driver object
 */
off_t disk_capacity(
    spisd_t* spi
);

/**
 * @brief Returns number of sectors on disk.
 */
off_t disk_sectors();

/**
 * @brief Returns block size.
 */
off_t disk_block_size();

int disk_sync();
int disk_status();

#endif