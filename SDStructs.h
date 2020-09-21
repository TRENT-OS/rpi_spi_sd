#ifndef SDSTRUCT_h
#define SDSTRUCT_h

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief SD configuration struct.
 */
typedef struct spisd_config_s
{
    uint32_t init_sck;
    uint32_t transfer_sck;
    //uint32_t block_size;
    //uint32_t sectors;
    //uint32_t capacity;
} spisd_config_t;

struct spisd_s;

/**
 * @brief Defines the hardware abstraction layer for the spi sd driver.
 */
typedef struct spisd_hal_s
{
    /**
     * @brief SPI transfer function.
     * 
     * @param spi       spi driver object
     * @param tx_data   byte that should be sent over spi
     * 
     * @return          returns byte that was read via spi
     */
    uint8_t (*_spi_transfer)(struct spisd_s* spi, uint8_t tx_data);

    /**
     * @brief SPI chip select function.
     * 
     * @details When cs is set to 1, then the respective device is selected over spi.
     * 
     * @param spi   spi driver object
     * @param cs    chip select
     */ 
    void (*_spi_cs)(struct spisd_s* spi, uint8_t cs);

    /**
     * @brief SPI wait function.
     * 
     * @details The function will stall all communication with SPI for ms milliseconds.
     * 
     * @param spi   spi driver object
     * @param ms    number of milliseconds to wait
     */
    void (*_spi_wait)(struct spisd_s* spi, uint32_t ms);
} spisd_hal_t;

/**
 * @brief The spi sd driver struct.
 */
typedef struct spisd_s
{
    // physical spi SD config
    const spisd_config_t* cfg;
    // HAL config
    const spisd_hal_t* hal;
} spisd_t;

#endif