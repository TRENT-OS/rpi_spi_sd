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
/* Introduction
 * ------------
 * SD and MMC cards support a number of interfaces, but common to them all
 * is one based on SPI. This is the one I'm implmenting because it means
 * it is much more portable even though not so performant, and we already
 * have the mbed SPI Interface!
 *
 * The main reference I'm using is Chapter 7, "SPI Mode" of:
 *  http://www.sdcard.org/developers/tech/sdcard/pls/Simplified_Physical_Layer_Spec.pdf
 *
 * SPI Startup
 * -----------
 * The SD card powers up in SD mode. The SPI interface mode is selected by
 * asserting CS low and sending the reset command (CMD0). The card will
 * respond with a (R1) response.
 *
 * CMD8 is optionally sent to determine the voltage range supported, and
 * indirectly determine whether it is a version 1.x SD/non-SD card or
 * version 2.x. I'll just ignore this for now.
 *
 * ACMD41 is repeatedly issued to initialise the card, until "in idle"
 * (bit 0) of the R1 response goes to '0', indicating it is initialised.
 *
 * You should also indicate whether the host supports High Capicity cards,
 * and check whether the card is high capacity - i'll also ignore this
 *
 * SPI Protocol
 * ------------
 * The SD SPI protocol is based on transactions made up of 8-bit words, with
 * the host starting every bus transaction by asserting the CS signal low. The
 * card always responds to commands, data blocks and errors.
 *
 * The protocol supports a CRC, but by default it is off (except for the
 * first reset CMD0, where the CRC can just be pre-calculated, and CMD8)
 * I'll leave the CRC off I think!
 *
 * Standard capacity cards have variable data block sizes, whereas High
 * Capacity cards fix the size of data block to 512 bytes. I'll therefore
 * just always use the Standard Capacity cards with a block size of 512 bytes.
 * This is set with CMD16.
 *
 * You can read and write single blocks (CMD17, CMD25) or multiple blocks
 * (CMD18, CMD25). For simplicity, I'll just use single block accesses. When
 * the card gets a read command, it responds with a response token, and then
 * a data token or an error.
 *
 * SPI Command Format
 * ------------------
 * Commands are 6-bytes long, containing the command, 32-bit argument, and CRC.
 *
 * +---------------+------------+------------+-----------+----------+--------------+
 * | 01 | cmd[5:0] | arg[31:24] | arg[23:16] | arg[15:8] | arg[7:0] | crc[6:0] | 1 |
 * +---------------+------------+------------+-----------+----------+--------------+
 *
 * As I'm not using CRC, I can fix that byte to what is needed for CMD0 (0x95)
 *
 * All Application Specific commands shall be preceded with APP_CMD (CMD55).
 *
 * SPI Response Format
 * -------------------
 * The main response format (R1) is a status byte (normally zero). Key flags:
 *  idle - 1 if the card is in an idle state/initialising
 *  cmd  - 1 if an illegal command code was detected
 *
 *    +-------------------------------------------------+
 * R1 | 0 | arg | addr | seq | crc | cmd | erase | idle |
 *    +-------------------------------------------------+
 *
 * R1b is the same, except it is followed by a busy signal (zeros) until
 * the first non-zero byte when it is ready again.
 *
 * Data Response Token
 * -------------------
 * Every data block written to the card is acknowledged by a byte
 * response token
 *
 * +----------------------+
 * | xxx | 0 | status | 1 |
 * +----------------------+
 *              010 - OK!
 *              101 - CRC Error
 *              110 - Write Error
 *
 * Single Block Read and Write
 * ---------------------------
 *
 * Block transfers have a byte header, followed by the data, followed
 * by a 16-bit CRC. In our case, the data will always be 512 bytes.
 *
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 * | 0xFE | data[0] | data[1] |        | data[n] | crc[15:8] | crc[7:0] |
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 */
#include "SDFileSystem.h"
#include "SDCard.h"

static int _cdv;
static int _is_initialized;
static off_t _sectors;
static const int BLOCK_SIZE = 512;

uint8_t _spi_read_write(spisd_t* spi, uint8_t data)
{
  return spi->hal->_spi_transfer(spi,data);
}

int initialise_card(spisd_t* spi, const spisd_hal_t* hal, const spisd_config_t* cfg) {
    memset(spi, 0, sizeof(spisd_t));
    spi->cfg = cfg;
    spi->hal = hal;

    // Set to SCK for initialisation, and clock card with cs = 1
    //spi->hal->_spi_set_frequency(spi,_init_sck);
    spi->hal->_spi_cs(spi,0);
    for (int i = 0; i < 16; i++) {
        _spi_read_write(spi,0xFF);
    }

    // send CMD0, should return with all zeros except IDLE STATE set (bit 0)
    // You need to repeat cmd0 multiple times, it is possible SD card is sending garbage the first few times: https://electronics.stackexchange.com/questions/77417/what-is-the-correct-command-sequence-for-microsd-card-initialization-in-spi/238217
    _cmd(spi,0,0);
    _cmd(spi,0,0);
    _cmd(spi,0,0);
    _cmd(spi,0,0);
    _cmd(spi,0,0);
    if (_cmd(spi, 0, 0) != R1_IDLE_STATE) {
        printf("No disk, or could not put SD card in to SPI idle state\n");
        return SDCARD_FAIL;
    }

    // send CMD8 to determine whther it is ver 2.x
    int r = _cmd8(spi);
    if (r == R1_IDLE_STATE) {
        return initialise_card_v2(spi);
    } else if (r == (R1_IDLE_STATE | R1_ILLEGAL_COMMAND)) {
        return initialise_card_v1(spi);
    } else {
        printf("Not in idle state after sending CMD8 (not an SD card?)\n");
        return SDCARD_FAIL;
    }
}

int initialise_card_v1(spisd_t* spi) {
    for (int i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        _cmd(spi, 55, 0);
        if (_cmd(spi, 41, 0) == 0) {
            _cdv = BLOCK_SIZE;
            printf("\n\rInit: SEDCARD_V1\n\r");
            return SDCARD_V1;
        }
    }

    printf("Timeout waiting for v1.x card\n");
    return SDCARD_FAIL;
}

int initialise_card_v2(spisd_t* spi) {
    for (int i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        spi->hal->_spi_wait(spi,50);
        _cmd58(spi);
        _cmd(spi, 55, 0);
        if (_cmd(spi, 41, 0x40000000) == 0) {
            _cmd58(spi);
            printf("\n\rInit: SDCARD_V2\n\r");
            _cdv = 1;
            return SDCARD_V2;
        }
    }

    printf("Timeout waiting for v2.x card\n");
    return SDCARD_FAIL;
}

int disk_initialize(spisd_t* spi, const spisd_hal_t* hal, const spisd_config_t* cfg) {
    _is_initialized = initialise_card(spi, hal, cfg);
    if (_is_initialized == 0) {
        printf("Fail to initialize card\n");
        return 1;
    }
    printf("init card = %d\n", _is_initialized);
    _sectors = _sd_sectors(spi);

    // Set block length to BLOCK_SIZE (max is 512) (CMD16)
    if (_cmd(spi, 16, BLOCK_SIZE) != 0) {
        printf("Set %d-byte block timed out\n",BLOCK_SIZE);
        return 1;
    }

    // Set SCK for data transfer
    //spi->hal->_spi_set_frequency(spi,_transfer_sck);
    return 0;
}

int disk_write(spisd_t* spi, const uint8_t* buffer, off_t block_number, off_t count) {
    if (!_is_initialized) {
        return -1;
    }
    
    for (uint32_t b = block_number; b < block_number + count; b++) {
        // set write address for single block (CMD24)
        if (_cmd(spi, 24, b * _cdv) != 0) {
            return 1;
        }
        
        // send the data block
        _write(spi, buffer, BLOCK_SIZE);
        buffer += BLOCK_SIZE;
    }
    
    return 0;
}

int disk_read(spisd_t* spi, uint8_t* buffer, off_t block_number, off_t count) {
    if (!_is_initialized) {
        return -1;
    }
    
    for (uint32_t b = block_number; b < block_number + count; b++) {
        // set read address for single block (CMD17)
        if (_cmd(spi, 17, b * _cdv) != 0) {
            return 1;
        }
        
        // receive the data
        _read(spi, buffer, BLOCK_SIZE);
        buffer += BLOCK_SIZE;
    }

    return 0;
}

int disk_status() {
    // FATFileSystem::disk_status() returns 0 when initialized
    if (_is_initialized) {
        return 0;
    } else {
        return 1;
    }
}


// PRIVATE FUNCTIONS
int _cmd(spisd_t* spi, int cmd, int arg) {
    spi->hal->_spi_cs(spi,1);

    // send a command
    _spi_read_write(spi, 0x40 | cmd);
    _spi_read_write(spi, arg >> 24);
    _spi_read_write(spi, arg >> 16);
    _spi_read_write(spi, arg >> 8);
    _spi_read_write(spi, arg >> 0);
    _spi_read_write(spi, 0x95);

    // wait for the repsonse (response[7] == 0)
    for (int i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = _spi_read_write(spi, 0xFF);
        if (!(response & 0x80)) {
            spi->hal->_spi_cs(spi,0);
            _spi_read_write(spi,0xFF);
            return response;
        }
    }
    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi, 0xFF);
    return -1; // timeout
}

int _cmdx(spisd_t* spi, int cmd, int arg) {
    spi->hal->_spi_cs(spi,1);

    // send a command
    _spi_read_write(spi, 0x40 | cmd);
    _spi_read_write(spi, arg >> 24);
    _spi_read_write(spi, arg >> 16);
    _spi_read_write(spi, arg >> 8);
    _spi_read_write(spi, arg >> 0);
    _spi_read_write(spi, 0x95);

    // wait for the repsonse (response[7] == 0)
    for (int i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = _spi_read_write(spi, 0xFF);
        if (!(response & 0x80)) {
            return response;
        }
    }
    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi,0xFF);
    return -1; // timeout
}


int _cmd58(spisd_t* spi) {
    spi->hal->_spi_cs(spi,1);
    int arg = 0;

    // send a command
    _spi_read_write(spi, 0x40 | 58);
    _spi_read_write(spi, arg >> 24);
    _spi_read_write(spi, arg >> 16);
    _spi_read_write(spi, arg >> 8);
    _spi_read_write(spi, arg >> 0);
    _spi_read_write(spi, 0x95);

    // wait for the repsonse (response[7] == 0)
    for (int i = 0; i < SD_COMMAND_TIMEOUT; i++) {
        int response = _spi_read_write(spi, 0xFF);
        if (!(response & 0x80)) {
            int ocr = _spi_read_write(spi,0xFF) << 24;
            ocr |= _spi_read_write(spi,0xFF) << 16;
            ocr |= _spi_read_write(spi,0xFF) << 8;
            ocr |= _spi_read_write(spi,0xFF) << 0;
            spi->hal->_spi_cs(spi,0);
            _spi_read_write(spi,0xFF);
            return response;
        }
    }
    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi,0xFF);
    return -1; // timeout
}

int _cmd8(spisd_t* spi) {
    spi->hal->_spi_cs(spi,1);

    // send a command
    _spi_read_write(spi,0x40 | 8);
    _spi_read_write(spi,0x00);
    _spi_read_write(spi,0x00);
    _spi_read_write(spi,0x01);
    _spi_read_write(spi,0xAA);
    _spi_read_write(spi,0x87);

    // wait for the repsonse (response[7] == 0)
    for (int i = 0; i < SD_COMMAND_TIMEOUT * 1000; i++) {
        char response[5];
        response[0] = _spi_read_write(spi,0xFF);
        if (!(response[0] & 0x80)) {
            for (int j = 1; j < 5; j++) {
                response[i] = _spi_read_write(spi,0xFF);
            }
            spi->hal->_spi_cs(spi,0);
            _spi_read_write(spi,0xFF);
            return response[0];
        }
    }
    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi,0xFF);
    return -1; // timeout
}

int _read(spisd_t* spi, uint8_t *buffer, off_t length) {
    spi->hal->_spi_cs(spi,1);

    // read until start byte (0xFF)
    while (_spi_read_write(spi,0xFF) != 0xFE);

    // read data
    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = _spi_read_write(spi,0xFF);
    }
    _spi_read_write(spi,0xFF); // checksum
    _spi_read_write(spi,0xFF);

    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi,0xFF);
    return 0;
}

int _write(spisd_t* spi, const uint8_t* buffer, off_t length) {
    spi->hal->_spi_cs(spi,1);

    // indicate start of block
    _spi_read_write(spi,0xFE);

    // write the data
    for (uint32_t i = 0; i < length; i++) {
        _spi_read_write(spi,buffer[i]);
    }

    // write the checksum
    _spi_read_write(spi,0xFF);
    _spi_read_write(spi,0xFF);

    // check the response token
    if ((_spi_read_write(spi,0xFF) & 0x1F) != 0x05) {
        spi->hal->_spi_cs(spi,0);
        _spi_read_write(spi,0xFF);
        return 1;
    }

    // wait for write to finish
    while (_spi_read_write(spi,0xFF) == 0);

    spi->hal->_spi_cs(spi,0);
    _spi_read_write(spi,0xFF);
    return 0;
}

off_t _sd_sectors(spisd_t* spi) {
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr, capacity;
    off_t hc_c_size;
    off_t blocks;

    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (_cmdx(spi, 9, 0) != 0) {
        printf("Didn't get a response from the disk\n");
        return 0;
    }

    uint8_t csd[16];
    if (_read(spi, csd, 16) != 0) {
        printf("Couldn't read csd response from disk\n");
        return 0;
    }

    // CSD = Card Specific Data
    // csd_structure : csd[127:126]
    // c_size        : csd[73:62]
    // c_size_mult   : csd[49:47]
    // read_bl_len   : csd[83:80] - the *maximum* read block length

    int csd_structure = ext_bits(csd, 127, 126);

    switch (csd_structure) {
        case 0:
            _cdv = BLOCK_SIZE;
            c_size = ext_bits(csd, 73, 62);
            c_size_mult = ext_bits(csd, 49, 47);
            read_bl_len = ext_bits(csd, 83, 80);

            block_len = 1 << read_bl_len;
            mult = 1 << (c_size_mult + 2);
            blocknr = (c_size + 1) * mult;
            capacity = blocknr * block_len;
            blocks = capacity / BLOCK_SIZE;
            printf("\n\rSDCard\n\rc_size: %d \n\rcapacity: %d \n\rsectors: %lld\n\r", c_size, capacity, blocks);
            break;

        case 1:
            _cdv = 1;
            hc_c_size = ext_bits(csd, 69, 48);
            blocks = (hc_c_size+1)*1024;
            printf("\n\rSDHC Card \n\rhc_c_size: %lld\n\rcapacity: %lld \n\rsectors: %lld\n\r", hc_c_size, blocks*BLOCK_SIZE, blocks);
            break;

        default:
            printf("CSD struct unsupported\r\n");
            return 0;
    };
    return blocks;
}

off_t disk_block_size(){
    return BLOCK_SIZE;
}

off_t disk_sectors() { 
    return _sectors; 
}

int disk_sync() { 
    return 0; 
}

off_t disk_capacity(spisd_t* spi){
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr, capacity;

    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (_cmdx(spi, 9, 0) != 0) {
        printf("Didn't get a response from the disk\n");
        return 0;
    }

    uint8_t csd[16];
    if (_read(spi, csd, 16) != 0) {
        printf("Couldn't read csd response from disk\n");
        return 0;
    }

    // CSD = Card Specific Data
    // csd_structure : csd[127:126]
    // c_size        : csd[73:62]
    // c_size_mult   : csd[49:47]
    // read_bl_len   : csd[83:80] - the *maximum* read block length

    int csd_structure = ext_bits(csd, 127, 126);

    switch (csd_structure) {
        case 0:
            c_size = ext_bits(csd, 73, 62);
            c_size_mult = ext_bits(csd, 49, 47);
            read_bl_len = ext_bits(csd, 83, 80);

            block_len = 1 << read_bl_len;
            mult = 1 << (c_size_mult + 2);
            blocknr = (c_size + 1) * mult;
            capacity = blocknr * block_len;
            return capacity;
            break;

        case 1:
            return _sectors*BLOCK_SIZE;
            break;

        default:
            printf("CSD struct unsupported\r\n");
            return 0;
    };
}