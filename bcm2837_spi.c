/*
 *  Code based on https://www.airspayce.com/mikem/bcm2835/index.html
 *  GPL Version 3, see https://www.gnu.org/licenses/gpl-3.0.html
 *  Copyright (C) 2011-2013 Mike McCauley
 */

#include "bcm2837_spi.h"
// #include "bcm2837_gpio.h"

#include <utils/arith.h>
#include <stdio.h>

/*
 * Default for RasPi3 is 400 MHz, but for some reason we force it to 250 MHz
 * with "core_freq=250" in config.txt
 */
#define BCM2837_CORE_CLK_HZ     (250 * 1000 * 1000)


static volatile uint32_t* bcm2837_spi0  = NULL;

/* SPI bit order. BCM2837 SPI0 only supports MSBFIRST, so we instead
 * have a software based bit reversal, based on a contribution by Damiano Benedetti
 */
static uint8_t bcm2837_spi_bit_order = BCM2837_SPI_BIT_ORDER_MSBFIRST;

static uint8_t bcm2837_byte_reverse_table[] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*---------------HELPER FUNCTIONS------------------*/

static uint8_t bcm2837_correct_order(uint8_t b)
{
    if (bcm2837_spi_bit_order == BCM2837_SPI_BIT_ORDER_LSBFIRST)
    {
        return bcm2837_byte_reverse_table[b];
    }
    else
    {
        return b;
    }
}

#include <platsupport/gpio.h>
#include <platsupport/plat/gpio.h>

/*! Base Address of the GPIO registers */
#define BCM2837_GPIO_BASE               0x200000
/*! Base Address of the SPI0 registers */
#define BCM2837_SPI0_BASE               0x204000

gpio_sys_t          gpio_sys;
gpio_t              gpio;

uint32_t bcm2837_peri_read(volatile uint32_t* paddr)
{
    uint32_t ret;
    __sync_synchronize();
    ret = *paddr;
    __sync_synchronize();
    return ret;
}

uint32_t bcm2837_peri_read_nb(volatile uint32_t* paddr)
{
    return *paddr;
}

void bcm2837_peri_write(volatile uint32_t* paddr, uint32_t value)
{
    __sync_synchronize();
    *paddr = value;
    __sync_synchronize();
}

void bcm2837_peri_write_nb(volatile uint32_t* paddr, uint32_t value)
{
    *paddr = value;
}

void bcm2837_peri_set_bits(volatile uint32_t* paddr, uint32_t value,
                           uint32_t mask)
{
    uint32_t v = bcm2837_peri_read(paddr);
    v = (v & ~mask) | (value & mask);
    bcm2837_peri_write(paddr, v);
}

/*--------------SPI INTERFACE------------------------------*/

int bcm2837_spi_begin(void* vaddr, ps_io_ops_t *io_ops)
{
    volatile uint32_t* paddr;

    int ret = gpio_sys_init(io_ops,&gpio_sys);
    if (ret)
    {
        printf("gpio_sys_init() failed: rslt = %i", ret);
        return -1;
    }
    // bcm2837_gpio_init(vaddr);
    bcm2837_spi0 = (uint32_t*)(vaddr + (BCM2837_SPI0_BASE - BCM2837_GPIO_BASE));

    /* Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them */
    for (size_t i = 0; i < 5; i++)
    {
        gpio_sys.init(&gpio_sys, 7 + i, 0, &gpio);
        bcm2837_gpio_fsel(&gpio, BCM2837_GPIO_FSEL_ALT0);
    }

    // bcm2837_gpio_fsel(RPI_GPIO_P1_26, BCM2837_GPIO_FSEL_ALT0); /* CE1 */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_24, BCM2837_GPIO_FSEL_ALT0); /* CE0 */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_21, BCM2837_GPIO_FSEL_ALT0); /* MISO */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_19, BCM2837_GPIO_FSEL_ALT0); /* MOSI */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_23, BCM2837_GPIO_FSEL_ALT0); /* CLK */

    /* Set the SPI CS register to the some sensible defaults */
    paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    bcm2837_peri_write(paddr, 0); /* All 0s */

    /* Clear TX and RX fifos */
    bcm2837_peri_write_nb(paddr, BCM2837_SPI0_CS_CLEAR);

    return 1; // OK
}

void bcm2837_spi_end(void)
{
    for (size_t i = 0; i < 5; i++)
    {
        gpio_sys.init(&gpio_sys, 7 + i, 0, &gpio);
        bcm2837_gpio_fsel(&gpio, BCM2837_GPIO_FSEL_INPT);
    }
    // /* Set all the SPI0 pins back to input */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_26, BCM2837_GPIO_FSEL_INPT); /* CE1 */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_24, BCM2837_GPIO_FSEL_INPT); /* CE0 */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_21, BCM2837_GPIO_FSEL_INPT); /* MISO */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_19, BCM2837_GPIO_FSEL_INPT); /* MOSI */
    // bcm2837_gpio_fsel(RPI_GPIO_P1_23, BCM2837_GPIO_FSEL_INPT); /* CLK */
}

void bcm2837_spi_setBitOrder(uint8_t order)
{
    bcm2837_spi_bit_order = order;
}

void bcm2837_spi_setDataMode(uint8_t mode)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    /* Mask in the CPO and CPHA bits of CS */
    bcm2837_peri_set_bits(paddr, mode << 2,
                          BCM2837_SPI0_CS_CPOL | BCM2837_SPI0_CS_CPHA);
}


/* defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
*/
void bcm2837_spi_setClockDivider(uint16_t divider)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CLK / 4;
    bcm2837_peri_write(paddr, divider);
}


void bcm2837_spi_chipSelect(uint8_t cs)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    /* Mask in the CS bits of CS */
    bcm2837_peri_set_bits(paddr, cs, BCM2837_SPI0_CS_CS);
}

void bcm2837_spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    uint8_t shift = 21 + cs;
    /* Mask in the appropriate CSPOLn bit */
    bcm2837_peri_set_bits(paddr, active << shift, 1 << shift);
}


/* Writes (and reads) a single byte to SPI */
uint8_t bcm2837_spi_transfer(uint8_t value)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    volatile uint32_t* fifo = bcm2837_spi0 + BCM2837_SPI0_FIFO / 4;
    uint32_t ret;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    // Clear TX and RX fifos
    */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_CLEAR, BCM2837_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_TA, BCM2837_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_TXD))
        ;

    /* Write to FIFO, no barrier */
    bcm2837_peri_write_nb(fifo, bcm2837_correct_order(value));

    /* Wait for DONE to be set */
    while (!(bcm2837_peri_read_nb(paddr) & BCM2837_SPI0_CS_DONE))
        ;

    /* Read any byte that was sent back by the slave while we sere sending to it */
    ret = bcm2837_correct_order(bcm2837_peri_read_nb(fifo));

    /* Set TA = 0, and also set the barrier */
    bcm2837_peri_set_bits(paddr, 0, BCM2837_SPI0_CS_TA);

    return ret;
}

/* Writes (and reads) an number of bytes to SPI */
void bcm2837_spi_transfernb(char* tbuf, char* rbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    volatile uint32_t* fifo = bcm2837_spi0 + BCM2837_SPI0_FIFO / 4;
    uint32_t TXCnt = 0;
    uint32_t RXCnt = 0;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    */

    /* Clear TX and RX fifos */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_CLEAR, BCM2837_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_TA, BCM2837_SPI0_CS_TA);

    /* Use the FIFO's to reduce the interbyte times */
    while ((TXCnt < len) || (RXCnt < len))
    {
        /* TX fifo not full, so add some more bytes */
        while (((bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_TXD)) && (TXCnt < len ))
        {
            bcm2837_peri_write_nb(fifo, bcm2837_correct_order(tbuf[TXCnt]));
            TXCnt++;
        }
        /* Rx fifo not empty, so get the next received bytes */
        while (((bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_RXD)) && ( RXCnt < len ))
        {
            rbuf[RXCnt] = bcm2837_correct_order(bcm2837_peri_read_nb(fifo));
            RXCnt++;
        }
    }
    /* Wait for DONE to be set */
    while (!(bcm2837_peri_read_nb(paddr) & BCM2837_SPI0_CS_DONE))
        ;

    /* Set TA = 0, and also set the barrier */
    bcm2837_peri_set_bits(paddr, 0, BCM2837_SPI0_CS_TA);
}

/* Writes an number of bytes to SPI */
void bcm2837_spi_writenb(const char* tbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    volatile uint32_t* fifo = bcm2837_spi0 + BCM2837_SPI0_FIFO / 4;
    uint32_t i;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    // Answer: an ISR is required to issue the required memory barriers.
    */

    /* Clear TX and RX fifos */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_CLEAR, BCM2837_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_TA, BCM2837_SPI0_CS_TA);

    for (i = 0; i < len; i++)
    {
        /* Maybe wait for TXD */
        while (!(bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_TXD))
            ;

        /* Write to FIFO, no barrier */
        bcm2837_peri_write_nb(fifo, bcm2837_correct_order(tbuf[i]));

        /* Read from FIFO to prevent stalling */
        while (bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_RXD)
        {
            (void) bcm2837_peri_read_nb(fifo);
        }
    }

    /* Wait for DONE to be set */
    while (!(bcm2837_peri_read_nb(paddr) & BCM2837_SPI0_CS_DONE))
    {
        while (bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_RXD)
        {
            (void) bcm2837_peri_read_nb(fifo);
        }
    };

    /* Set TA = 0, and also set the barrier */
    bcm2837_peri_set_bits(paddr, 0, BCM2837_SPI0_CS_TA);
}

/* Writes (and reads) an number of bytes to SPI
// Read bytes are copied over onto the transmit buffer
*/
void bcm2837_spi_transfern(char* buf, uint32_t len)
{
    bcm2837_spi_transfernb(buf, buf, len);
}

void bcm2837_spi_write(uint16_t data)
{
    volatile uint32_t* paddr = bcm2837_spi0 + BCM2837_SPI0_CS / 4;
    volatile uint32_t* fifo = bcm2837_spi0 + BCM2837_SPI0_FIFO / 4;

    /* Clear TX and RX fifos */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_CLEAR, BCM2837_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2837_peri_set_bits(paddr, BCM2837_SPI0_CS_TA, BCM2837_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm2837_peri_read(paddr) & BCM2837_SPI0_CS_TXD))
        ;

    /* Write to FIFO */
    bcm2837_peri_write_nb(fifo,  (uint32_t) data >> 8);
    bcm2837_peri_write_nb(fifo,  data & 0xFF);


    /* Wait for DONE to be set */
    while (!(bcm2837_peri_read_nb(paddr) & BCM2837_SPI0_CS_DONE))
        ;

    /* Set TA = 0, and also set the barrier */
    bcm2837_peri_set_bits(paddr, 0, BCM2837_SPI0_CS_TA);
}
