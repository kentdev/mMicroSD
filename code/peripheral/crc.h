/*******************************************************************************
* crc.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: CRC checksum code used by sd_lowlevel.c, though I can't really
*              claim to be the author here, since the code is just two open
*              source CRC functions, which I modified for the AVR and STM32.
*******************************************************************************/

#ifndef CRC_H
#define CRC_H

#ifndef M4
// AVR code
#include "m_general.h"
#else
// M4 code
#include "mGeneral.h"
#endif

// 16-bit CRC for data blocks
uint16_t crc16_ccitt (uint8_t *data, const uint16_t length);
void crc16_by_byte (uint16_t *crc, uint8_t byte);

// 7-bit CRC for commands
uint8_t getCRC (const uint8_t message[], uint8_t length);


#endif
