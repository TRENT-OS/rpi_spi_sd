/*
 *  Code based on https://www.airspayce.com/mikem/bcm2835/index.html
 *  GPL Version 3, see https://www.gnu.org/licenses/gpl-3.0.html and COPYING
 *  Copyright (C) 2011-2013 Mike McCauley
 */

#pragma once

#include <stdint.h>

/* Defines for SPI
   GPIO register offsets from BCM2837_SPI0_BASE.
   Offsets into the SPI Peripheral block in bytes per 10.5 SPI Register Map
*/
#define BCM2837_SPI0_CS 0x0000   /*!< SPI Master Control and Status */
#define BCM2837_SPI0_FIFO 0x0004 /*!< SPI Master TX and RX FIFOs */
#define BCM2837_SPI0_CLK 0x0008  /*!< SPI Master Clock Divider */
#define BCM2837_SPI0_DLEN 0x000c /*!< SPI Master Data Length */
#define BCM2837_SPI0_LTOH 0x0010 /*!< SPI LOSSI mode TOH */
#define BCM2837_SPI0_DC 0x0014   /*!< SPI DMA DREQ Controls */

/* Register masks for SPI0_CS */
#define BCM2837_SPI0_CS_LEN_LONG 0x02000000 /*!< Enable Long data word in Lossi mode if DMA_LEN is set */
#define BCM2837_SPI0_CS_DMA_LEN 0x01000000  /*!< Enable DMA mode in Lossi mode */
#define BCM2837_SPI0_CS_CSPOL2 0x00800000   /*!< Chip Select 2 Polarity */
#define BCM2837_SPI0_CS_CSPOL1 0x00400000   /*!< Chip Select 1 Polarity */
#define BCM2837_SPI0_CS_CSPOL0 0x00200000   /*!< Chip Select 0 Polarity */
#define BCM2837_SPI0_CS_RXF 0x00100000      /*!< RXF - RX FIFO Full */
#define BCM2837_SPI0_CS_RXR 0x00080000      /*!< RXR RX FIFO needs Reading (full) */
#define BCM2837_SPI0_CS_TXD 0x00040000      /*!< TXD TX FIFO can accept Data */
#define BCM2837_SPI0_CS_RXD 0x00020000      /*!< RXD RX FIFO contains Data */
#define BCM2837_SPI0_CS_DONE 0x00010000     /*!< Done transfer Done */
#define BCM2837_SPI0_CS_TE_EN 0x00008000    /*!< Unused */
#define BCM2837_SPI0_CS_LMONO 0x00004000    /*!< Unused */
#define BCM2837_SPI0_CS_LEN 0x00002000      /*!< LEN LoSSI enable */
#define BCM2837_SPI0_CS_REN 0x00001000      /*!< REN Read Enable */
#define BCM2837_SPI0_CS_ADCS 0x00000800     /*!< ADCS Automatically Deassert Chip Select */
#define BCM2837_SPI0_CS_INTR 0x00000400     /*!< INTR Interrupt on RXR */
#define BCM2837_SPI0_CS_INTD 0x00000200     /*!< INTD Interrupt on Done */
#define BCM2837_SPI0_CS_DMAEN 0x00000100    /*!< DMAEN DMA Enable */
#define BCM2837_SPI0_CS_TA 0x00000080       /*!< Transfer Active */
#define BCM2837_SPI0_CS_CSPOL 0x00000040    /*!< Chip Select Polarity */
#define BCM2837_SPI0_CS_CLEAR 0x00000030    /*!< Clear FIFO Clear RX and TX */
#define BCM2837_SPI0_CS_CLEAR_RX 0x00000020 /*!< Clear FIFO Clear RX  */
#define BCM2837_SPI0_CS_CLEAR_TX 0x00000010 /*!< Clear FIFO Clear TX  */
#define BCM2837_SPI0_CS_CPOL 0x00000008     /*!< Clock Polarity */
#define BCM2837_SPI0_CS_CPHA 0x00000004     /*!< Clock Phase */
#define BCM2837_SPI0_CS_CS 0x00000003       /*!< Chip Select */

/*! \brief bcm2837SPIBitOrder SPI Bit order
  Specifies the SPI data bit ordering for bcm2837_spi_setBitOrder()
*/
typedef enum
{
    BCM2837_SPI_BIT_ORDER_LSBFIRST = 0, /*!< LSB First */
    BCM2837_SPI_BIT_ORDER_MSBFIRST = 1  /*!< MSB First */
} bcm2837SPIBitOrder;

/*! \brief SPI Data mode
  Specify the SPI data mode to be passed to bcm2837_spi_setDataMode()
*/
typedef enum
{
    BCM2837_SPI_MODE0 = 0, /*!< CPOL = 0, CPHA = 0 */
    BCM2837_SPI_MODE1 = 1, /*!< CPOL = 0, CPHA = 1 */
    BCM2837_SPI_MODE2 = 2, /*!< CPOL = 1, CPHA = 0 */
    BCM2837_SPI_MODE3 = 3  /*!< CPOL = 1, CPHA = 1 */
} bcm2837SPIMode;

/*! \brief bcm2837SPIChipSelect
  Specify the SPI chip select pin(s)
*/
typedef enum
{
    BCM2837_SPI_CS0 = 0,    /*!< Chip Select 0 */
    BCM2837_SPI_CS1 = 1,    /*!< Chip Select 1 */
    BCM2837_SPI_CS2 = 2,    /*!< Chip Select 2 (ie pins CS1 and CS2 are asserted) */
    BCM2837_SPI_CS_NONE = 3 /*!< No CS, control it yourself */
} bcm2837SPIChipSelect;

/*! \brief bcm2837SPIClockDivider
  Specifies the divider used to generate the SPI clock from the system clock.
  Figures below give the divider, clock period and clock frequency. Clock
  divided is based on nominal core clock rate of 250MHz on RPi1 and RPi2, and
  400MHz on RPi3. It is reported that (contrary to the documentation) any even
  divider may used. The frequencies shown for each divider have been confirmed
  by measurement on RPi1 and RPi2. The system clock frequency on RPi3 is
  different, so the frequency you get from a given divider will be different.
  See comments in 'SPI Pins' for information about reliable SPI speeds.
  Note: it is possible to change the core clock rate of the RPi 3 back to
  250MHz, by putting
  \code
  core_freq=250
  \endcode
  in the config.txt
*/
typedef enum
{
    BCM2837_SPI_CLOCK_DIVIDER_65536 = 0,     /*!< 65536 = 3.814697260kHz on Rpi2, 6.1035156kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_32768 = 32768, /*!< 32768 = 7.629394531kHz on Rpi2, 12.20703125kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_16384 = 16384, /*!< 16384 = 15.25878906kHz on Rpi2, 24.4140625kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_8192 = 8192,   /*!< 8192 = 30.51757813kHz on Rpi2, 48.828125kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_4096 = 4096,   /*!< 4096 = 61.03515625kHz on Rpi2, 97.65625kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_2048 = 2048,   /*!< 2048 = 122.0703125kHz on Rpi2, 195.3125kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_1024 = 1024,   /*!< 1024 = 244.140625kHz on Rpi2, 390.625kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_512 = 512,     /*!< 512 = 488.28125kHz on Rpi2, 781.25kHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_256 = 256,     /*!< 256 = 976.5625kHz on Rpi2, 1.5625MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_128 = 128,     /*!< 128 = 1.953125MHz on Rpi2, 3.125MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_64 = 64,       /*!< 64 = 3.90625MHz on Rpi2, 6.250MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_32 = 32,       /*!< 32 = 7.8125MHz on Rpi2, 12.5MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_16 = 16,       /*!< 16 = 15.625MHz on Rpi2, 25MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_8 = 8,         /*!< 8 = 31.25MHz on Rpi2, 50MHz on RPI3 */
    BCM2837_SPI_CLOCK_DIVIDER_4 = 4,         /*!< 4 = 62.5MHz on Rpi2, 100MHz on RPI3. Dont expect this speed to work reliably. */
    BCM2837_SPI_CLOCK_DIVIDER_2 = 2,         /*!< 2 = 125MHz on Rpi2, 200MHz on RPI3, fastest you can get. Dont expect this speed to work reliably.*/
    BCM2837_SPI_CLOCK_DIVIDER_1 = 1          /*!< 1 = 3.814697260kHz on Rpi2, 6.1035156kHz on RPI3, same as 0/65536 */
} bcm2837SPIClockDivider;

/*! Start SPI operations.
      Forces RPi SPI0 pins P1-19 (MOSI), P1-21 (MISO), P1-23 (CLK), P1-24 (CE0) and P1-26 (CE1)
      to alternate function ALT0, which enables those pins for SPI interface.
      You should call bcm2837_spi_end() when all SPI funcitons are complete to return the pins to
      their default functions.
      \param[in] vaddr Virtual address (in the user space) of the begining of the memory mapped GPIO registers,
      \sa  bcm2837_spi_end()
      \return 1 if successful, 0 otherwise (perhaps because you are not running as root)
    */
extern int bcm2837_spi_begin(void* vaddr);

/*! End SPI operations.
      SPI0 pins P1-19 (MOSI), P1-21 (MISO), P1-23 (CLK), P1-24 (CE0) and P1-26 (CE1)
      are returned to their default INPUT behaviour.
    */
extern void bcm2837_spi_end(void);

/*! Sets the SPI bit order
      Set the bit order to be used for transmit and receive. The bcm2837 SPI0 only supports BCM2837_SPI_BIT_ORDER_MSB,
      so if you select BCM2837_SPI_BIT_ORDER_LSB, the bytes will be reversed in software.
      The library defaults to BCM2837_SPI_BIT_ORDER_MSB.
      \param[in] order The desired bit order, one of BCM2837_SPI_BIT_ORDER_*,
      see \ref bcm2837SPIBitOrder
    */
extern void bcm2837_spi_setBitOrder(uint8_t order);

/*! Sets the SPI clock divider and therefore the
      SPI clock speed.
      \param[in] divider The desired SPI clock divider, one of BCM2837_SPI_CLOCK_DIVIDER_*,
      see \ref bcm2837SPIClockDivider
    */
extern void bcm2837_spi_setClockDivider(uint16_t divider);

/*! Sets the SPI clock divider by converting the speed parameter to
      the equivalent SPI clock divider. ( see \sa bcm2837_spi_setClockDivider)
      \param[in] speed_hz The desired SPI clock speed in Hz
    */
extern void bcm2837_spi_set_speed_hz(uint32_t speed_hz);

/*! Sets the SPI data mode
      Sets the clock polariy and phase
      \param[in] mode The desired data mode, one of BCM2837_SPI_MODE*,
      see \ref bcm2837SPIMode
    */
extern void bcm2837_spi_setDataMode(uint8_t mode);

/*! Sets the chip select pin(s)
      When an bcm2837_spi_transfer() is made, the selected pin(s) will be asserted during the
      transfer.
      \param[in] cs Specifies the CS pins(s) that are used to activate the desired slave.
      One of BCM2837_SPI_CS*, see \ref bcm2837SPIChipSelect
    */
extern void bcm2837_spi_chipSelect(uint8_t cs);

/*! Sets the chip select pin polarity for a given pin
      When an bcm2837_spi_transfer() occurs, the currently selected chip select pin(s)
      will be asserted to the
      value given by active. When transfers are not happening, the chip select pin(s)
      return to the complement (inactive) value.
      \param[in] cs The chip select pin to affect
      \param[in] active Whether the chip select pin is to be active HIGH
    */
extern void bcm2837_spi_setChipSelectPolarity(uint8_t cs, uint8_t active);

/*! Transfers one byte to and from the currently selected SPI slave.
      Asserts the currently selected CS pins (as previously set by bcm2837_spi_chipSelect)
      during the transfer.
      Clocks the 8 bit value out on MOSI, and simultaneously clocks in data from MISO.
      Returns the read data byte from the slave.
      Uses polled transfer as per section 10.6.1 of the BCM 2835 ARM Peripherls manual
      \param[in] value The 8 bit data byte to write to MOSI
      \return The 8 bit byte simultaneously read from  MISO
      \sa bcm2837_spi_transfern()
    */
extern uint8_t bcm2837_spi_transfer(uint8_t value);

/*! Transfers any number of bytes to and from the currently selected SPI slave.
      Asserts the currently selected CS pins (as previously set by bcm2837_spi_chipSelect)
      during the transfer.
      Clocks the len 8 bit bytes out on MOSI, and simultaneously clocks in data from MISO.
      The data read read from the slave is placed into rbuf. rbuf must be at least len bytes long
      Uses polled transfer as per section 10.6.1 of the BCM 2835 ARM Peripherls manual
      \param[in] tbuf Buffer of bytes to send.
      \param[out] rbuf Received bytes will by put in this buffer
      \param[in] len Number of bytes in the tbuf buffer, and the number of bytes to send/received
      \sa bcm2837_spi_transfer()
    */
extern void bcm2837_spi_transfernb(char* tbuf, char* rbuf, uint32_t len);

/*! Transfers any number of bytes to and from the currently selected SPI slave
      using bcm2837_spi_transfernb.
      The returned data from the slave replaces the transmitted data in the buffer.
      \param[in,out] buf Buffer of bytes to send. Received bytes will replace the contents
      \param[in] len Number of bytes int eh buffer, and the number of bytes to send/received
      \sa bcm2837_spi_transfer()
    */
extern void bcm2837_spi_transfern(char* buf, uint32_t len);

/*! Transfers any number of bytes to the currently selected SPI slave.
      Asserts the currently selected CS pins (as previously set by bcm2837_spi_chipSelect)
      during the transfer.
      \param[in] buf Buffer of bytes to send.
      \param[in] len Number of bytes in the buf buffer, and the number of bytes to send
    */
extern void bcm2837_spi_writenb(const char* buf, uint32_t len);

/*! Transfers half-word to and from the currently selected SPI slave.
      Asserts the currently selected CS pins (as previously set by bcm2837_spi_chipSelect)
      during the transfer.
      Clocks the 8 bit value out on MOSI, and simultaneously clocks in data from MISO.
      Returns the read data byte from the slave.
      Uses polled transfer as per section 10.6.1 of the BCM 2835 ARM Peripherls manual
      \param[in] data The 8 bit data byte to write to MOSI
      \sa bcm2837_spi_writenb()
    */
extern void bcm2837_spi_write(uint16_t data);

/*! Start AUX SPI operations.
      Forces RPi AUX SPI pins P1-36 (MOSI), P1-38 (MISO), P1-40 (CLK) and P1-36 (CE2)
      to alternate function ALT4, which enables those pins for SPI interface.
      \return 1 if successful, 0 otherwise (perhaps because you are not running as root)
    */
extern int bcm2837_aux_spi_begin(void);

/*! End AUX SPI operations.
       SPI1 pins P1-36 (MOSI), P1-38 (MISO), P1-40 (CLK) and P1-36 (CE2)
       are returned to their default INPUT behaviour.
     */
extern void bcm2837_aux_spi_end(void);

/*! Sets the AUX SPI clock divider and therefore the AUX SPI clock speed.
      \param[in] divider The desired AUX SPI clock divider.
    */
extern void bcm2837_aux_spi_setClockDivider(uint16_t divider);

/*!
     * Calculates the input for \sa bcm2837_aux_spi_setClockDivider
     * @param speed_hz A value between \sa BCM2837_AUX_SPI_CLOCK_MIN and \sa BCM2837_AUX_SPI_CLOCK_MAX
     * @return Input for \sa bcm2837_aux_spi_setClockDivider
     */
extern uint16_t bcm2837_aux_spi_CalcClockDivider(uint32_t speed_hz);

/*! Transfers half-word to and from the AUX SPI slave.
      Asserts the currently selected CS pins during the transfer.
      \param[in] data The 8 bit data byte to write to MOSI
      \return The 8 bit byte simultaneously read from  MISO
      \sa bcm2837_spi_transfern()
    */
extern void bcm2837_aux_spi_write(uint16_t data);

/*! Transfers any number of bytes to the AUX SPI slave.
      Asserts the CE2 pin during the transfer.
      \param[in] buf Buffer of bytes to send.
      \param[in] len Number of bytes in the tbuf buffer, and the number of bytes to send
    */
extern void bcm2837_aux_spi_writenb(const char* buf, uint32_t len);

/*! Transfers any number of bytes to and from the AUX SPI slave
      using bcm2837_aux_spi_transfernb.
      The returned data from the slave replaces the transmitted data in the buffer.
      \param[in,out] buf Buffer of bytes to send. Received bytes will replace the contents
      \param[in] len Number of bytes int eh buffer, and the number of bytes to send/received
      \sa bcm2837_aux_spi_transfer()
    */
extern void bcm2837_aux_spi_transfern(char* buf, uint32_t len);

/*! Transfers any number of bytes to and from the AUX SPI slave.
      Asserts the CE2 pin during the transfer.
      Clocks the len 8 bit bytes out on MOSI, and simultaneously clocks in data from MISO.
      The data read read from the slave is placed into rbuf. rbuf must be at least len bytes long
      \param[in] tbuf Buffer of bytes to send.
      \param[out] rbuf Received bytes will by put in this buffer
      \param[in] len Number of bytes in the tbuf buffer, and the number of bytes to send/received
    */
extern void bcm2837_aux_spi_transfernb(const char* tbuf, char* rbuf,
                                       uint32_t len);
