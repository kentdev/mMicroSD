/*******************************************************************************
* sd_highlevel.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: High-level reading/writing functionality for SD cards, which
*              allows partial block reading/writing and also includes caching,
*              to avoid hitting the SD card on every data access.
*******************************************************************************/

#ifndef SD_HIGHLEVEL_H
#define SD_HIGHLEVEL_H

#include "sd_lowlevel.h"

#define CRC_RETRIES     8
#define TIMEOUT_RETRIES 5
#define UNKNOWN_RETRIES 2


//------------------------------------------------------------------------------
// Sector caching

#if !defined(CACHED_SECTORS) || CACHED_SECTORS < 1

#if defined(ATMEGA168)
#define CACHED_SECTORS 1
#elif (defined(ATMEGA328) || defined(M2))
#define CACHED_SECTORS 2
#elif (defined(M4))
#define CACHED_SECTORS 8
#else
#error Unknown target
#endif

#endif

#if CACHED_SECTORS <= 0
#error Must have at least one cached sector
#endif

typedef struct cached_sector
{
    uint32_t block_number;
    bool     modified;
    uint8_t  data[512];
    struct cached_sector *next;
} cached_sector;

#define INVALID_SECTOR ((uint32_t)0xffffffff)
#define END_OF_CHAIN ((cached_sector*)0)

extern cached_sector cache[CACHED_SECTORS];
extern cached_sector *head;

// initialize the cache chain, each node containing block 0xffffffff
void init_cache (void);

// find a node in the cache chain
cached_sector *cache_lookup (uint32_t block_number);

// remove a node from the chain and make it the new head
bool move_to_head (cached_sector *node);

// remove and return the last node of the chain
cached_sector *remove_least_used (void);

// add a new node as the head of the chain
void add_as_head (cached_sector *node);

// write out any modified cached sectors, then re-initialize the cache chain
bool flush_cache (void);

// end of sector caching
//------------------------------------------------------------------------------




enum card_error_codes
{
    ERROR_NONE = 0,      // no error
    ERROR_RESET = 1,     // error resetting the card
    ERROR_ENABLE_CRC,    // error enabling CRC
    ERROR_INIT,          // error initializing the card
    ERROR_BLOCK_LENGTH,  // error setting the block length
    ERROR_CARD_UNINIT,   // the card has not been initialized yet
    ERROR_NULL_BUFFER,   // the read/write function was given a null buffer
    ERROR_TOO_FAR,       // tried to read or write beyond the block length (512 bytes)
    ERROR_TIMEOUT,       // timeout when reading/writing to the card
    ERROR_CRC,           // too many CRC errors when reading/writing
    ERROR_CACHE_FAILURE, // an error occurred in the block caching system
    ERROR_UNKNOWN,       // some other error
    NUM_CARD_ERROR_CODES
};

// error_code contains the relevant error code if any of the functions return false
extern uint8_t error_code;


typedef enum crc_option
{
    USE_CRC,
    //USE_CRC_WORKAROUND,
    NO_CRC
} crc_option;

bool init_card (crc_option crc_type);

bool error_recovery (void);

// blocks are 512 bytes long


// reads data
//
// a recently read block is cached, so reading different parts of the
// same block over multiple calls doesn't have much of a performance penalty
bool read_partial_block (const uint32_t block_number,
                         const uint16_t offset,
                         uint8_t *buffer,
                         const uint16_t length);

// reads the CRC value of a block, without storing any block data
// (this function is not affected by caching)
bool read_block_crc (const uint32_t block_number,
                     uint16_t *crc);


// writes data to a block
//
// the write might not happen immediately, so that several small writes
// can be grouped into a single larger write
bool write_partial_block (const uint32_t block_number,
                          const uint16_t offset,
                          const uint8_t *buffer,
                          const uint16_t length);


// Write a modified cached sector to the SD card
bool write_to_card (cached_sector *sector);

#endif

