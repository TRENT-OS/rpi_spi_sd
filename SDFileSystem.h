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