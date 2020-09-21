/* Copyright (C) 2020, HENSOLDT Cyber GmbH */
/**
 * @file
 * @brief   SPI LED driver.
 */
#include "OS_Error.h"
#include "LibDebug/Debug.h"
#include "OS_Dataport.h"
#include "TimeServer.h"

#include "bcm2837_spi.h"
#include "SDFileSystem.h"
//#include "SPI_MSD_Driver.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <camkes.h>

#ifndef _SPISD_ERR_BASE
#define _SPISD_ERR_BASE            (-24000)
#endif

#define SPISD_OK                   (0)
#define SPISD_ERR_INTERNAL         (_SPISD_ERR_BASE - 1)
#define SPISD_ERR_BAD_STATE        (_SPISD_ERR_BASE - 2)
#define SPISD_ERR_HW_BUSY          (_SPISD_ERR_BASE - 3)
#define SPISD_ERR_BUSY             (_SPISD_ERR_BASE - 4)
#define SPISD_ERR_ERASE_UNALIGNED  (_SPISD_ERR_BASE - 5)
#define SPISD_ERR_BAD_CONFIG       (_SPISD_ERR_BASE - 6)i

/**
 * @brief organizational data for spi led driver.
 */
static struct
{
    bool           init_ok;
    spisd_t        spi_sd_ctx;
    OS_Dataport_t  port_storage;
} ctx =
{
    .port_storage  = OS_DATAPORT_ASSIGN(storage_port),
    .init_ok       = false,
};

static const if_OS_Timer_t timer =
    IF_OS_TIMER_ASSIGN(
        timeServer_rpc,
        timeServer_notify);

static
bool
isValidMSDArea(
    spisd_t* spi,
    off_t const offset,
    off_t const size)
{
    off_t const end = offset + size;
    return ( (offset >= 0) && (size >= 0) && (end >= offset) && (end <= disk_capacity(&(ctx.spi_sd_ctx))) );
}

static 
__attribute__((__nonnull__))
uint8_t
impl_spi_transfer(
    spisd_t* spi,
    uint8_t tx_data)
{
    return bcm2837_spi_transfer(tx_data);
}

static
__attribute__((__nonnull__))
void
impl_spi_cs(
    spisd_t* spi,
    uint8_t cs)
{
    bcm2837_spi_chipSelect(cs ? BCM2837_SPI_CS0 : BCM2837_SPI_CS2);
    return;
}

static
__attribute__((__nonnull__))
void
impl_spi_wait(
    spisd_t* spi,
    uint32_t ms)
{
    // TimeServer.h provides this helper function, it contains the hard-coded
    // assumption that the RPC interface is "timeServer_rpc"
    TimeServer_sleep(&timer, TimeServer_PRECISION_MSEC, ms);
}

void post_init(void)
{
    Debug_LOG_INFO("BCM2837_SPI_MSD init");

    // initialize BCM2837 SPI library
    if (!bcm2837_spi_begin(regBase))
    {
        Debug_LOG_ERROR("bcm2837_spi_begin() failed");
        return;
    }

    bcm2837_spi_setBitOrder(BCM2837_SPI_BIT_ORDER_MSBFIRST);
    bcm2837_spi_setDataMode(BCM2837_SPI_MODE0);
    bcm2837_spi_chipSelect(BCM2837_SPI_CS0);
    bcm2837_spi_setChipSelectPolarity(BCM2837_SPI_CS0, 0);

    static const spisd_config_t spisd_config =
    {
        // initial clock speed must be between 100 and 400 kHz for SD card initialization
        // 250MHz / 2048 = 122.0703125 kHz
        .init_sck = BCM2837_SPI_CLOCK_DIVIDER_2048,
        // Note: The highest SPI clock rate is 20 MHz for MMC and 25 MHz for SD 
        // 250MHz / 16 = 15.625 MHz
        .transfer_sck = BCM2837_SPI_CLOCK_DIVIDER_16,
        // Standard capacity cards have variable data block sizes, whereas High
        // Capacity cards fix the size of data block to 512 bytes. Therefore
        // just always use the Standard Capacity cards with a block size of 512 bytes.
        // This is set with CMD16.
        //.block_size = 512
    };

    // set initialization clock speed before SD card initialization    
    bcm2837_spi_setClockDivider(spisd_config.init_sck); 

    // initialize MSD_SPI library
    static const spisd_hal_t hal =
    {
        ._spi_transfer = impl_spi_transfer,
        ._spi_cs    = impl_spi_cs,
        ._spi_wait  = impl_spi_wait  
    };

    disk_initialize(&ctx.spi_sd_ctx, &hal, &spisd_config);

    if ( (NULL == ctx.spi_sd_ctx.hal) )
    {
        Debug_LOG_ERROR("disk_initialize() failed");
        return;
    }

    // set transfer clock speed after SD card initialization 
    bcm2837_spi_setClockDivider(spisd_config.transfer_sck); 

    ctx.init_ok = true;

    Debug_LOG_INFO("BCM2837_SPI_MSD done");
}


//------------------------------------------------------------------------------
// This is a CAmkES RPC interface handler. It's guaranteed that "written"
// never points to NULL.
OS_Error_t
__attribute__((__nonnull__))
storage_rpc_write(
    off_t offset,
    size_t  size,
    size_t* written)
{
    // set defaults
    *written = 0;

    Debug_LOG_DEBUG(
        "SPI write: offset %jd (0x%jx), size %zu (0x%zx)",
        offset, offset, size, size);

    if (!ctx.init_ok)
    {
        Debug_LOG_ERROR("initialization failed, fail call %s()", __func__);
        return OS_ERROR_INVALID_STATE;
    }

    size_t dataport_size = OS_Dataport_getSize(ctx.port_storage);
    if (size > dataport_size)
    {
        // the client did a bogus request, it knows the data port size and
        // never ask for more data
        Debug_LOG_ERROR(
            "size %zu exceeds dataport size %zu",
            size,
            dataport_size );

        return OS_ERROR_INVALID_PARAMETER;
    }

    if (!isValidMSDArea(&(ctx.spi_sd_ctx), offset, size))
    {
        Debug_LOG_ERROR(
            "write area at offset %jd with size %zu out of bounds",
            offset, size);
        return OS_ERROR_OUT_OF_BOUNDS;
    }

    const void* buffer = OS_Dataport_getBuf(ctx.port_storage);

    int ret = 0;
    int bytesWritten = 0;
    if (size > 0)
    {
        uint8_t block[disk_block_size()];
        memset(block,0,disk_block_size());

        uint32_t sector = offset / disk_block_size();
        //read first block, adjust according bytes and write block back
        ret = disk_read(&(ctx.spi_sd_ctx),block,sector,1);
        if (ret != 0){
            Debug_LOG_ERROR( "disk_read() failed => SPISD_write() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                offset, offset, bytesWritten, bytesWritten, ret);
            return OS_ERROR_GENERIC;
        }
        size_t nbr_of_bytes = size <= (disk_block_size() - (offset - sector * disk_block_size())) ? size : (disk_block_size() - (offset - sector * disk_block_size()));
        memcpy(block + (offset - sector * disk_block_size()),buffer,nbr_of_bytes);
        ret = disk_write(&(ctx.spi_sd_ctx),block,sector,1);
        if (ret != 0){
            Debug_LOG_ERROR( "disk_write() failed => SPISD_write() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                offset, offset, bytesWritten, bytesWritten, ret);
            return OS_ERROR_GENERIC;
        }

        bytesWritten += nbr_of_bytes;
        size -= nbr_of_bytes;
        buffer += nbr_of_bytes;

        //write the remaining blocks
        while (size > disk_block_size())
        {
            memcpy(block,buffer,disk_block_size());
            ret = disk_write(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_write() failed => SPISD_write() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesWritten, bytesWritten, ret);
                return OS_ERROR_GENERIC;
            }
            bytesWritten += disk_block_size();
            size -= disk_block_size();
            buffer += disk_block_size();
        }

        if (size > 0)
        {
            //read last block, adjust according bytes and write block back
            ret = disk_read(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_read() failed => SPISD_write() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesWritten, bytesWritten, ret);
                return OS_ERROR_GENERIC;
            }
            memcpy(block,buffer,size);
            ret = disk_write(&(ctx.spi_sd_ctx),block,sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_write() failed => SPISD_write() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesWritten, bytesWritten, ret);
                return OS_ERROR_GENERIC;
            }
            bytesWritten += size;
            size -= size;
        }
    }

    *written = bytesWritten;

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
// This is a CAmkES RPC interface handler. It's guaranteed that "read"
// never points to NULL.
OS_Error_t
__attribute__((__nonnull__))
storage_rpc_read(
    off_t offset,
    size_t  size,
    size_t* read)
{
    // set defaults
    *read = 0;

    Debug_LOG_DEBUG(
        "SPI read: offset %jd (0x%jx), size %zu (0x%zx)",
        offset, offset, size, size);

    if (!ctx.init_ok)
    {
        Debug_LOG_ERROR("initialization failed, fail call %s()", __func__);
        return OS_ERROR_INVALID_STATE;
    }

    size_t dataport_size = OS_Dataport_getSize(ctx.port_storage);
    if (size > dataport_size)
    {
        // the client did a bogus request, it knows the data port size and
        // never ask for more data
        Debug_LOG_ERROR(
            "size %zu exceeds dataport size %zu",
            size,
            dataport_size );

        return OS_ERROR_INVALID_PARAMETER;
    }

    if (!isValidMSDArea(&(ctx.spi_sd_ctx), offset, size))
    {
        Debug_LOG_ERROR(
            "read area at offset %jd with size %zu out of bounds",
            offset, size);

        return OS_ERROR_OUT_OF_BOUNDS;
    }

    int ret = 0;
    int bytesRead = 0;

    if (size > 0)
    {
        uint8_t block[disk_block_size()];
        memset(block,0,disk_block_size());
        
        uint32_t sector = offset / disk_block_size();
        
        //read first block and copy according bytes to dataport
        ret = disk_read(&(ctx.spi_sd_ctx),block,sector,1);
        if (ret != 0){
            Debug_LOG_ERROR( "disk_read() failed => SPISD_read() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                offset, offset, bytesRead, bytesRead, ret);
            return OS_ERROR_GENERIC;
        }
        size_t nbr_of_bytes = size <= (disk_block_size() - (offset - sector * disk_block_size())) ? size : (disk_block_size() - (offset - sector * disk_block_size()));
        memcpy(OS_Dataport_getBuf(ctx.port_storage),block + (offset - sector * disk_block_size()),nbr_of_bytes);
   
        bytesRead += nbr_of_bytes;
        size -= nbr_of_bytes;
    
        //read the remaining blocks and copy into dataport 
        while (size > disk_block_size())
        {
            ret = disk_read(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_read() failed => SPISD_read() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesRead, bytesRead, ret);
                return OS_ERROR_GENERIC;
            }
            memcpy(OS_Dataport_getBuf(ctx.port_storage) + bytesRead,block,disk_block_size());
            bytesRead += disk_block_size();
            size -= disk_block_size();
        }

        if (size > 0)
        {
            //read last block and copy according bytes into the dataport
            ret = disk_read(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_read() failed => SPISD_read() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesRead, bytesRead, ret);
                return OS_ERROR_GENERIC;
            }
            memcpy(OS_Dataport_getBuf(ctx.port_storage) + bytesRead,block,size);
            bytesRead += size;
            size -= size;
        }
    }
    
    *read = bytesRead;

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
// This is a CAmkES RPC interface handler. It's guaranteed that "erased"
// never points to NULL.
OS_Error_t
__attribute__((__nonnull__))
storage_rpc_erase(
    off_t offset,
    off_t size,
    off_t* erased)
{
    // set defaults
    *erased = 0;

    Debug_LOG_DEBUG(
        "SPI erase: offset %jd (0x%jx), size %jd (0x%jx)",
        offset, offset, size, size);

    if (!ctx.init_ok)
    {
        Debug_LOG_ERROR("initialization failed, fail call %s()", __func__);
        return OS_ERROR_INVALID_STATE;
    }
    
    size_t dataport_size = OS_Dataport_getSize(ctx.port_storage);
    if (size > dataport_size)
    {
        // the client did a bogus request, it knows the data port size and
        // never ask for more data
        Debug_LOG_ERROR(
            "size %lld exceeds dataport size %zu",
            size,
            dataport_size );

        return OS_ERROR_INVALID_PARAMETER;
    }

    if (!isValidMSDArea(&(ctx.spi_sd_ctx), offset, size))
    {
        Debug_LOG_ERROR(
            "erase area at offset %jd with size %jd out of bounds",
            offset, size);
        return OS_ERROR_OUT_OF_BOUNDS;
    }

    int ret = 0;
    size_t bytesErased = 0;

    if(size > 0){
        uint8_t block[disk_block_size()];
        memset(block,0,disk_block_size());
        
        uint32_t sector = offset / disk_block_size();
        //read first block, adjust according bytes and erase block
        ret = disk_read(&(ctx.spi_sd_ctx),block,sector,1);
        if (ret != 0){
            Debug_LOG_ERROR( "disk_read() failed => SPISD_erase() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                offset, offset, bytesErased, bytesErased, ret);
            return OS_ERROR_GENERIC;
        }
        size_t nbr_of_bytes = size <= (disk_block_size() - (offset - sector * disk_block_size())) ? size : (disk_block_size() - (offset - sector * disk_block_size()));
        memset(block + (offset - sector * disk_block_size()),0xFF,nbr_of_bytes);
        ret = disk_write(&(ctx.spi_sd_ctx),block,sector,1);
        if (ret != 0){
            Debug_LOG_ERROR( "disk_write() failed => SPISD_erase() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                offset, offset, bytesErased, bytesErased, ret);
            return OS_ERROR_GENERIC;
        }
        
        bytesErased += nbr_of_bytes;
        size -= nbr_of_bytes;
        
        //erase the remaining blocks
        while (size > disk_block_size())
        {
            memset(block,0xFF,disk_block_size());
            ret = disk_write(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_write() failed => SPISD_erase() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesErased, bytesErased, ret);
                return OS_ERROR_GENERIC;
            }
            bytesErased += disk_block_size();
            size -= disk_block_size();
        }

        if (size > 0)
        {
            //read last block, adjust according bytes and erase block
            ret = disk_read(&(ctx.spi_sd_ctx),block,++sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_read() failed => SPISD_erase() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesErased, bytesErased, ret);
                return OS_ERROR_GENERIC;
            }
            memset(block,0xFF,size);
            ret = disk_write(&(ctx.spi_sd_ctx),block,sector,1);
            if (ret != 0){
                Debug_LOG_ERROR( "disk_write() failed => SPISD_erase() failed, offset %jd (0x%jx), size %zu (0x%zx), code %d",
                                 offset, offset, bytesErased, bytesErased, ret);
                return OS_ERROR_GENERIC;
            }
            bytesErased += size;
            size -= size;
        }
    }
    
    *erased = bytesErased;

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
// This is a CAmkES RPC interface handler. It's guaranteed that "size"
// never points to NULL.
OS_Error_t
__attribute__((__nonnull__))
storage_rpc_getSize(
    off_t* size)
{
    if (!ctx.init_ok)
    {
        Debug_LOG_ERROR("initialization failed, fail call %s()", __func__);
        return OS_ERROR_INVALID_STATE;
    }

    *size = disk_capacity(&(ctx.spi_sd_ctx));

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
// This is a CAmkES RPC interface handler. It's guaranteed that "flags"
// never points to NULL.
OS_Error_t
__attribute__((__nonnull__))
storage_rpc_getState(
    uint32_t* flags)
{
    if (!ctx.init_ok)
    {
        Debug_LOG_ERROR("initialization failed, fail call %s()", __func__);
        return OS_ERROR_INVALID_STATE;
    }

    *flags = disk_status();
    return OS_SUCCESS;
}