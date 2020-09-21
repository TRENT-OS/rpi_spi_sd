#ifndef SDCARD_H
#define SDCARD_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "SDStructs.h"

#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

// Types
//  - v1.x Standard Capacity
//  - v2.x Standard Capacity
//  - v2.x High Capacity
//  - Not recognised as an SD Card
#define SDCARD_FAIL 0
#define SDCARD_V1   1
#define SDCARD_V2   2
#define SDCARD_V2HC 3

#define SD_COMMAND_TIMEOUT 5000
#define SD_DBG             0

/**
 * @brief SPI read write operation.
 *
 * @param spi   spi driver object
 * @param data  byte to write to sd card
 * 
 * @return      returns byte read from sd card
 */
uint8_t _spi_read_write(
    spisd_t* spi, 
    uint8_t data
);

/**
 * @brief cmd
 * 
 * @param spi   spi driver object
 * @param cmd   command index
 * @param arg   argument 
 * 
 * @return      returns error code
 */
int _cmd(
    spisd_t* spi, 
    int cmd, 
    int arg
);

/**
 * @brief cmdx
 * 
 * @param spi   spi driver object
 * @param cmd   command index
 * @param arg   argument
 * 
 * @return      returns error code
 */
int _cmdx(
    spisd_t* spi, 
    int cmd, 
    int arg
);

/**
 * @brief cmd8
 * 
 * @param spi   spi driver object
 * 
 * @returns     returns error code
 */
int _cmd8(
    spisd_t* spi
);

/**
 * @brief cmd58
 * 
 * @param spi   spi driver object
 * 
 * @return      returns error code
 */
int _cmd58(
    spisd_t* spi
);

/**
 * @brief Initializes SD card.
 * 
 * @param spi   spi driver object
 * @param hal   hardware abstration layer for sd card
 * @param cfg   configuration object for sd card
 * 
 * @return      returns error code
 */
int initialise_card(
    spisd_t* spi, 
    const spisd_hal_t* hal, 
    const spisd_config_t* cfg
);

/**
 * @brief Initializes SD card type 1.
 * 
 * @param spi   spi driver object
 * 
 * @return      returns error code
 */
int initialise_card_v1(
    spisd_t* spi
);

/**
 * @brief Initializes SD card type 2.
 * 
 * @param spi   spi driver object
 * 
 * @return      returns error code
 */
int initialise_card_v2(
    spisd_t* spi
);

/**
 * @brief Reads a certain number of bytes into the buffer.
 * 
 * @param spi       spi driver object
 * @param buffer    read data will be stored in buffer
 * @param length    number of bytes to read
 * 
 * @return          returns error code 
 */
int _read(
    spisd_t* spi, 
    uint8_t * buffer, 
    off_t length
);

/**
 * @brief Writes a certain number of bytes from buffer.
 * 
 * @param spi       spi driver object
 * @param buffer    holds data that will be written to sd card
 * @param length    number of bytes to write
 * 
 * @return          returns error code
 */
int _write(
    spisd_t* spi, 
    const uint8_t *buffer, 
    off_t length
);

/**
 * @brief Calculates the number of sectors.
 * 
 * @param spi   spi driver object
 * 
 * @return      returns the number of sectors on the sd card
 */
off_t _sd_sectors(
    spisd_t* spi
);

/**
 * @brief Extracts bits of data register.
 *
 * @details Extracts certain range of bits of data register based on msb and lsb number.
 * 
 * @param data  data where certain bits are extracted
 * @param msb   most significant bit to return
 * @param lsb   least significant bit to return
 * 
 * @return      returns integer value of extracted bits.
 */
static uint32_t ext_bits(unsigned char *data, int msb, int lsb) {
    uint32_t bits = 0;
    uint32_t size = 1 + msb - lsb;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t position = lsb + i;
        uint32_t byte = 15 - (position >> 3);
        uint32_t bit = position & 0x7;
        uint32_t value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}

#endif