/*******************************************************************************
* sd_lowlevel.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Low-level code for reading from and writing to an SD card over
*              SPI.  The code is designed to be run from either an ATmega168, an
*              ATmega328, an M2, or an M4.  In the case of the M4, it uses SPI1,
*              and B1 is used as the chip select pin (though this can be changed
*              in the defines in sd_lowlevel.c).
*******************************************************************************/

#ifndef SD_LOWLEVEL_H
#define SD_LOWLEVEL_H

#if defined(M2) || defined(ATMEGA168) || defined(ATMEGA328)
// AVR code
#include "m_general.h"
#elif defined(M4)
// M4 code
#include "mGeneral.h"
#else
#error "Unknown device, use -DM2, -DATMEGA168, -DATMEGA328, or -DM4 in your makefile"
#endif

#ifndef bool
#include <stdbool.h>
#endif

#include "crc.h"

// the ONLY block length for SDHC and SDXC cards is 512 bytes
#define MAX_BLOCK_LENGTH  512ul

#define RESET_TRIES_BEFORE_ERROR 10
#define INIT_TRIES_BEFORE_ERROR  10000ul
#define READ_BLOCK_TIMEOUT_BYTES 65534ul


// sd_lowlevel on an ATmega or the M2 requires either an 8MHz or 16MHz system clock
// other clock speeds might work, but will put the SD card SPI init speed out of spec
//
// sd_lowlevel on the M4 assumes a 72MHz clock speed

extern uint32_t total_block_accesses;

#ifdef FREE_RAM
void free_ram (void);
#endif

enum spi_speed
{
    SPI_INIT_SPEED,
    SPI_MIN_SPEED,
    SPI_LOW_SPEED,
    SPI_MED_SPEED,
    SPI_HIGH_SPEED
};

typedef enum ret_val
{
    SPI_OK,
    SPI_BAD_CRC,
    SPI_TIMEOUT,
    SPI_ERROR
} ret;

// sets up the output pins and registers
// can be called multiple times (eg, low speed at first, then high speed after initialization)
void start_spi (enum spi_speed speed);

void attempt_resync (void);

ret reset_card (void);  // the first thing that needs to be called after start_spi
ret initialize_card (void);

// enabling CRC must be done between reset_card and initialize_card
// CRC is disabled by default
ret enable_crc (void);

extern uint16_t last_crc;  // the most recently received CRC value from the card
// (needed because the card could be returning all 0xFFFFs instead of actual CRCs)

extern uint16_t block_length;  // max of MAX_BLOCK_LENGTH, initialized to 0
ret set_block_length (const uint16_t block_length);

ret read_block (const uint32_t block_number, uint8_t *block);
ret read_block_crc_only (const uint32_t block_number, uint16_t *crc);
ret write_block (const uint32_t block_number, uint8_t *block);

#endif

