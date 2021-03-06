/*******************************************************************************
* sd_highlevel.c
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: High-level reading/writing functionality for SD cards, which
*              allows partial block reading/writing and also includes caching,
*              to avoid hitting the SD card on every data access.
*******************************************************************************/

#include "sd_highlevel.h"
#include "debug.h"

uint8_t error_code;



bool initialized = false;

enum spi_speed current_speed;
crc_option current_crc_type;

bool drop_speed (void)
{
    #ifdef HIGHLEVEL_DEBUG
    debug ("Reducing SPI speed: ");
    #endif
    
    switch (current_speed)
    {
        case SPI_HIGH_SPEED:
            current_speed = SPI_MED_SPEED;
            break;
        case SPI_MED_SPEED:
            current_speed = SPI_LOW_SPEED;
            break;
        case SPI_LOW_SPEED:
            current_speed = SPI_MIN_SPEED;
            break;
        case SPI_MIN_SPEED:
        case SPI_INIT_SPEED:
            return false;
    }
    
    start_spi (current_speed);
    return true;
}


// if a read or write fails due to an unknown error, try reducing the speed
// and re-initializing the card
bool error_recovery (void)
{
    #ifdef HIGHLEVEL_DEBUG
    debug ("ERROR RECOVERY\n");
    #endif
    
    attempt_resync();
    if (!drop_speed())
        return false;
    attempt_resync();
    
    enum spi_speed lower_speed = current_speed;
    
    if (!init_card (current_crc_type))
    {
        #ifdef HIGHLEVEL_DEBUG
        debug ("RECOVERY FAILED\n");
        #endif
        return false;
    }
    
    attempt_resync();
    
    current_speed = lower_speed;
    
    #ifdef HIGHLEVEL_DEBUG
    debug ("RECOVERY OK\n");
    #endif
    
    return true;
}


bool init_card (crc_option crc_type)
{
    uint8_t test = 0;
    
    init_cache();
    
    current_crc_type = crc_type;
    
    start_spi (SPI_INIT_SPEED);
    
    if (reset_card() != SPI_OK)
    {
        error_code = ERROR_RESET;
        return false;
    }
    
    if (crc_type == USE_CRC)
    {
        if (enable_crc() != SPI_OK)
        {
            error_code = ERROR_ENABLE_CRC;
            return false;
        }
    }
    
    if (initialize_card() != SPI_OK)
    {
        error_code = ERROR_INIT;
        return false;
    }
    
    if (set_block_length (512) != SPI_OK)
    {
        error_code = ERROR_BLOCK_LENGTH;
        return false;
    }
    
    initialized = true;
    
    // start at the minimum speed and check if this card actually supports CRC
    start_spi (SPI_MIN_SPEED);
    if (crc_type == USE_CRC)
    {
        #ifdef HIGHLEVEL_DEBUG
        debug ("CRC test\n");
        #endif
        
        if (!read_partial_block (0, 0, &test, 1))
        {
            if (error_code == ERROR_CRC && last_crc == 0xffff)
            {  // this card isn't giving us valid CRCs after all
                error_code = ERROR_ENABLE_CRC;
            }
            
            // Either it doesn't support CRC when we want it to, or
            // it doesn't even get up to 1MHz.  Either way, FAIL.
            return false;
        }
        
        #ifdef HIGHLEVEL_DEBUG
        debug ("CRC OK\n");
        #endif
    }
    
    // bump the speed up to max
    current_speed = SPI_HIGH_SPEED;
    start_spi (SPI_HIGH_SPEED);
    
    error_code = ERROR_NONE;
    return true;
}



// reads an entire block into the cache
// intended for use only by read_partial_block and write_partial_block
bool read_whole_block (const uint32_t block_number)
{
    uint8_t crc_retries = CRC_RETRIES;
    uint8_t timeout_retries = TIMEOUT_RETRIES;
    uint8_t unknown_retries = UNKNOWN_RETRIES;    
    
    ret status;
    
    cached_sector *sector = cache_lookup (block_number);
    
    if (sector != END_OF_CHAIN)
    {  // we already have this sector in the cache
        // move it to the head of the cache chain, to mark it as most recently accessed
        if (!move_to_head (sector))
            return false;
        
        error_code = ERROR_NONE;
        return true;
    }
    
    #ifdef LOWLEVEL_DEBUG
    debug ("cache miss for ");
    debugulong (block_number);
    debug ("\n");
    #endif
    
    // if the sector is not cached, remove the oldest cached sector
    sector = remove_least_used();
    
    if (sector->block_number != INVALID_SECTOR && sector->modified)
    {  // if the oldest cached sector was valid and modified, write it out
        #ifdef HIGHLEVEL_DEBUG
        debug ("loading block ");
        debugulong (block_number);
        debug (" forced commit of cached block ");
        debugulong (sector->block_number);
        debug ("\n");
        #endif
        
        if (!write_to_card (sector))
        {  // write failed, re-add this sector to the cache chain and fail
            add_as_head (sector);
            return false;
        }
    }
    
    // use the sector to store the block to be read, and make it the new head
    sector->block_number = block_number;
    sector->modified = false;
    add_as_head (sector);
    
    #ifdef LOWLEVEL_DEBUG
    debug ("reading ");
    debugulong (block_number);
    debug (" from SD card\n");
    #endif
    
read:
    status = read_block (block_number, sector->data);
    
    switch (status)
    {
        case SPI_OK:
            break;
        case SPI_BAD_CRC:
            if (crc_retries > 0)
            {
                crc_retries--;
                goto read;
            }
            else
            {
                // give the sector a bad sector number so it isn't treated as valid data
                sector->block_number = INVALID_SECTOR;
                
                error_code = ERROR_CRC;
                return false;
            }
            break;
        case SPI_TIMEOUT:
            if (timeout_retries > 0)
            {
                timeout_retries--;
                goto read;
            }
            else
            {
                // give the sector a bad sector number so it isn't treated as valid data
                sector->block_number = INVALID_SECTOR;
                
                error_code = ERROR_TIMEOUT;
                return false;
            }
            break;
        default:
            if (unknown_retries > 0)
            {
                unknown_retries--;
                goto read;
            }
            else
            {
                if (error_recovery())
                {  // if we were able to lower the speed and re-initialize the card
                    unknown_retries = UNKNOWN_RETRIES;
                    goto read;
                }
                else
                {
                    // give the sector a bad sector number so it isn't treated as valid data
                    sector->block_number = INVALID_SECTOR;
                    
                    error_code = ERROR_UNKNOWN;
                    return false;
                }
            }
            break;
    }
    
    error_code = ERROR_NONE;
    return true;
}




// reads data, blocks are 512 bytes long
//
// a recently read block is cached, so reading different parts of the
// same block over multiple calls doesn't have much of a performance penalty
bool read_partial_block (const uint32_t block_number,
                         const uint16_t offset,
                         uint8_t *buffer,
                         const uint16_t length)
{
    if (!initialized)
    {
        error_code = ERROR_CARD_UNINIT;
        return false;
    }
    
    if (buffer == 0)
    {
        error_code = ERROR_NULL_BUFFER;
        return false;
    }
    
    if (offset + length > 512)
    {
        error_code = ERROR_TOO_FAR;
        return false;
    }
    
    if (!read_whole_block (block_number))
        return false;
    
    cached_sector *sector = cache_lookup (block_number);
    if (sector == END_OF_CHAIN)
    {  // the sector should be in the cache now
        error_code = ERROR_CACHE_FAILURE;
        return false;
    }
    
    // copy the relevant bytes from the block to the buffer
    for (uint16_t i = 0; i < length; i++)
        buffer[i] = sector->data[offset + i];
    
    error_code = ERROR_NONE;
    return true;
}


// reads a block's CRC value without storing block data
// (this function is not affected by caching)
bool read_block_crc (const uint32_t block_number,
                     uint16_t *crc)
{
    if (!initialized)
    {
        error_code = ERROR_CARD_UNINIT;
        return false;
    }
    
    uint8_t crc_retries = CRC_RETRIES;
    uint8_t timeout_retries = TIMEOUT_RETRIES;
    uint8_t unknown_retries = UNKNOWN_RETRIES;    
    
    ret status;
read:
    status = read_block_crc_only (block_number, crc);
    
    switch (status)
    {
        case SPI_OK:
            break;
        case SPI_BAD_CRC:
            if (crc_retries > 0)
            {
                crc_retries--;
                goto read;
            }
            else
            {
                error_code = ERROR_CRC;
                return false;
            }
            break;
        case SPI_TIMEOUT:
            if (timeout_retries > 0)
            {
                timeout_retries--;
                goto read;
            }
            else
            {
                error_code = ERROR_TIMEOUT;
                return false;
            }
            break;
        default:
            if (unknown_retries > 0)
            {
                unknown_retries--;
                goto read;
            }
            else
            {
                if (error_recovery())
                {  // if we were able to lower the speed and re-initialize the card
                    unknown_retries = UNKNOWN_RETRIES;
                    goto read;
                }
                else
                {
                    error_code = ERROR_UNKNOWN;
                    return false;
                }
            }
            break;
    }
    
    error_code = ERROR_NONE;
    return true;
}





// writes data to a block
//
// the write might not happen immediately, so that several small writes
// can be grouped into a single larger write
bool write_partial_block (const uint32_t block_number,
                          const uint16_t offset,
                          const uint8_t *buffer,
                          const uint16_t length)
{
    if (!initialized)
    {
        error_code = ERROR_CARD_UNINIT;
        return false;
    }
    
    if (buffer == 0)
    {
        error_code = ERROR_NULL_BUFFER;
        return false;
    }
    
    if (offset + length > 512)
    {
        #ifdef HIGHLEVEL_DEBUG
        debug ("TOO FAR: offset = ");
        debuguint (offset);
        debug (", length = ");
        debuguint (length);
        debug ("\n");
        #endif
        
        error_code = ERROR_TOO_FAR;
        return false;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    cached_sector *sector = cache_lookup (block_number);
    
    if (sector == END_OF_CHAIN)
    {  // if we don't have this sector in the cache
        if (!read_whole_block (block_number))  // read it in
            return false;
        
        sector = cache_lookup (block_number);
        if (sector == END_OF_CHAIN)
        {  // the sector should be in the cache now
            error_code = ERROR_CACHE_FAILURE;
            return false;
        }
    }
    
    // modify the block using the input buffer
    #ifdef HIGHLEVEL_DEBUG
    debug ("Writing to ");
    debugulong (block_number);
    debug (", offset ");
    debughex (offset);
    debug (": ");
    #endif
    
    for (uint16_t i = 0; i < length; i++)
    {
        sector->data[offset + i] = buffer[i];
        
        #ifdef HIGHLEVEL_DEBUG
        debughexchar (sector->data[offset + i]);
        debug (" ");
        #endif
    }
    #ifdef HIGHLEVEL_DEBUG
    debug ("\n");
    #endif
    
    sector->modified = true;
    
    return true;
}


// Write a modified cached sector to the SD card
bool write_to_card (cached_sector *sector)
{
    // ignore attempts to write to an invalid sector number
    // this usually happens when flushing if the cache has not been fully used
    if (sector->block_number == INVALID_SECTOR)
    {
        error_code = ERROR_NONE;
        return true;
    }
    
    if (!sector->modified)
    {
        #ifdef HIGHLEVEL_DEBUG
        debug ("Block ");
        debugulong (sector->block_number);
        debug (" unmodified, not writing\n");
        #endif
        
        error_code = ERROR_NONE;
        return true;
    }
    
    #ifdef HIGHLEVEL_DEBUG
    debug ("Committing write to block ");
    debugulong (sector->block_number);
    debug ("\n");
    #endif
    
    uint8_t crc_retries = CRC_RETRIES;
    uint8_t timeout_retries = TIMEOUT_RETRIES;
    uint8_t unknown_retries = UNKNOWN_RETRIES;
    
    // write the whole block to the card
    ret status;
write:
    status = write_block (sector->block_number, sector->data);
    
    switch (status)
    {
        case SPI_OK:
            break;
        case SPI_BAD_CRC:
            if (crc_retries > 0)
            {
                crc_retries--;
                goto write;
            }
            else
            {
                error_code = ERROR_CRC;
                return false;
            }
            break;
        case SPI_TIMEOUT:
            if (timeout_retries > 0)
            {
                timeout_retries--;
                goto write;
            }
            else
            {
                error_code = ERROR_TIMEOUT;
                return false;
            }
            break;
        default:
            if (unknown_retries > 0)
            {
                unknown_retries--;
                goto write;
            }
            else
            {
                if (error_recovery())
                {  // if we were able to lower the speed and re-initialize the card
                    unknown_retries = UNKNOWN_RETRIES;
                    goto write;
                }
                else
                {
                    error_code = ERROR_UNKNOWN;
                    return false;
                }
            }
            break;
    }
    
    #ifdef VERIFY_WRITE
    #ifdef HIGHLEVEL_DEBUG
    debug ("WRITE VERIFICATION\n");
    #endif
    
    uint16_t written_crc = crc16_ccitt (sector->data, block_length);
    uint16_t returned_crc;
    if (!read_block_crc (sector->block_number, &returned_crc))
        return false;
    
    if (written_crc != returned_crc)
    {
        #ifdef HIGHLEVEL_DEBUG
        debug ("\nWRITE VERIFICATION FAILED\n");
        debug ("Write CRC: ");
        debughex (written_crc);
        debug (", read CRC: ");
        debughex (returned_crc);
        debug ("\n");
        #endif
        
        if (crc_retries > 0)
        {
            crc_retries--;
            goto write;
        }
        else
        {
            error_code = ERROR_CRC;
            return false;
        }
    }
    
    #ifdef HIGHLEVEL_DEBUG
    debug ("\nWRITE VERIFICATION OK\n");
    #endif
    
    #endif
    
    
    sector->modified = false;
    
    error_code = ERROR_NONE;
    return true;
}

