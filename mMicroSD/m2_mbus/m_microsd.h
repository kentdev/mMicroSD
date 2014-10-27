/*******************************************************************************
* m_microsd.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Contains the functions needed to communicate with the mMicroSD
*              peripheral over I2C from the M2.
*******************************************************************************/

#ifndef M_MICROSD_H
#define M_MICROSD_H

#include "m_general.h"
#include "m_bus.h"

#include "m_usb.h"

#define FILE_END_POS ((uint32_t)0xffffffff)

typedef enum m_sd_errors
{
    ERROR_NONE = 0,     // no error
    
    // card-level
    ERROR_RESET = 1,             // error resetting the card
    ERROR_ENABLE_CRC,            // error enabling CRC
    ERROR_INIT,                  // error initializing the card
    ERROR_BLOCK_LENGTH,          // error setting the block length
    ERROR_CARD_UNINIT,           // the card has not been initialized yet
    ERROR_NULL_BUFFER,           // the read/write function was given a null buffer
    ERROR_TOO_FAR,               // tried to read or write beyond the block length (512 bytes)
    ERROR_TIMEOUT,               // timeout when reading/writing to the card
    ERROR_CRC,                   // too many CRC errors when reading/writing
    ERROR_CACHE_FAILURE,         // an error occurred in the block caching system
    ERROR_UNKNOWN,               // some other error
    
    // filesystem-level
    ERROR_MBR,                   // error reading MBR
    ERROR_NO_FAT32,              // no FAT32 filesystem found
    ERROR_FAT32_VOLUME_ID,       // unexpected values in the FAT32 volume ID
    ERROR_FAT32_INIT,            // FAT32 filesystem has not been mounted
    ERROR_FAT32_CLUSTER_LOOKUP,  // bad value encountered when looking up a FAT32 entry
    ERROR_FAT32_AT_ROOT,         // tried to pop directory when in the root dir
    ERROR_FAT32_NOT_FOUND,       // the file or directory was not found
    ERROR_FAT32_NOT_DIR,         // tried to call push on a file
    ERROR_FAT32_END_OF_DIR,      // tried to read a dir entry after reaching the end
    ERROR_FAT32_NOT_FILE,        // tried to open a directory as a file
    ERROR_FAT32_NOT_OPEN,        // tried to read/write/seek when no file is open
    ERROR_FAT32_TOO_FAR,         // tried to read/seek beyond the file's length
    ERROR_FAT32_ALREADY_EXISTS,  // tried to create a file or dir that already exists
    ERROR_FAT32_FILE_READ_ONLY,  // tried to write to a file that was opened read-only
    ERROR_FAT32_FULL,            // no free space left on the card
    ERROR_FAT32_INVALID_NAME,    // gave a function a bad name (too long, invalid characters...)
    ERROR_FAT32_NOT_EMPTY,       // tried to call rmdir on an non-empty directory
    ERROR_FAT32_ALREADY_OPEN,    // tried to open a file that is already open
    ERROR_FAT32_TOO_MANY_FILES,  // maximum number of files already open
    ERROR_FAT32_BAD_FILE_ID,     // tried to use a file id higher than the maximum allowed
    
    // mBus-level
    ERROR_I2C_COMMAND,           // error communicating over I2C
    ERROR_I2C_RESPONSE_TIMEOUT,  // timed out waiting for a response from the mMicroSD
    ERROR_I2C_MESSAGE_TOO_LONG   // tried to read or write too much data at a time
} m_sd_errors;


typedef enum open_option
{
    READ_FILE = 0,
    APPEND_FILE,
    CREATE_FILE
} open_option;

// convert file names from their representation on disk
// eg., "TEST    TXT" becomes "TEST.TXT"
void filename_fs_to_8_3 (const char *input_name,
                         char *output_name);

extern m_sd_errors m_sd_error_code;

//==============================================================================
//=============================== USER FUNCTIONS ===============================
//==============================================================================
// Note: Long file names are NOT supported!
//       8.3 characters for files and 8 characters for directories MAX!


//-----------------------------------------------
// Startup and shutdown:

// mount the microSD card's FAT32 filesystem
bool m_sd_init (void);

// flush any pending writes and unmount the filesystem
bool m_sd_shutdown (void);


//-----------------------------------------------
// File and directory information:

// get the size of a file in the current directory
bool m_sd_get_size (const char *name,
                    uint32_t *size);

// search the current directory
// if the target is found, *is_directory will be set accordingly
bool m_sd_object_exists (const char *name,
                         bool *is_directory);

// iterate through the current directory, reading the names of its objects
// returns false when the end of the directory has been reached
bool m_sd_get_dir_entry_first (char name[12]);
bool m_sd_get_dir_entry_next  (char name[12]);


//-----------------------------------------------
// Directory traversal and modification:

// enter the directory with the given name
bool m_sd_push (const char *name);

// go up one level in the directory path
bool m_sd_pop (void);

// create a subdirectory in the current directory
bool m_sd_mkdir (const char *name);

// remove an empty subdirectory from the current directory
bool m_sd_rmdir (const char *name);

// delete a file from the current directory
// if you delete an open file, the file will be closed first
bool m_sd_delete (const char *name);


//-----------------------------------------------
// File access and modification:

// open a file in the current directory
// actions are:
//   READ_FILE:   open an existing file as read-only
//   APPEND_FILE: open an existing file as read-write and seek to the end
//   CREATE_FILE: create a file, deleting any previously existing file
//
// you cannot open a file that is already opened elsewhere
bool m_sd_open_file (const char *name,
                     const open_option action,
                     uint8_t *file_id);

// close the file
// does nothing if the file is not open
bool m_sd_close_file (uint8_t file_id);

// seek to a location within the opened file
// an offset of FILE_END_POS will seek to the end of the file
bool m_sd_seek (uint8_t file_id,
                uint32_t offset);

// get the seek position in the opened file
bool m_sd_get_seek_pos (uint8_t file_id,
                        uint32_t *offset);

// read from the current location in the file
// updates the seek position
//
// if the length of the read would go beyond the end of the file, an
// error is returned and nothing is read
bool m_sd_read_file (uint8_t file_id,
                     uint32_t length,
                     uint8_t *buffer);

// write to the current location in the file
// updates the seek position
bool m_sd_write_file (uint8_t file_id,
                      uint32_t length,
                      uint8_t *buffer);

#endif

