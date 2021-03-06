/*******************************************************************************
* sd_fat32.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Header file for the FAT32 filesystem code.  User functions are at
*              the bottom.
*******************************************************************************/

#ifndef SD_FAT32_H
#define SD_FAT32_H

// NOTE: avr-gcc uses little-endian byte ordering
// This is helpful, since the MBR and filesystem are also little-endian

#include "sd_highlevel.h"
#include "fat32_filenames.h"

enum fat32_error_codes
{  // these codes are numbered after the card error codes in sd_highlevel.h
    ERROR_MBR = NUM_CARD_ERROR_CODES, // error reading MBR
    ERROR_NO_FAT32,                   // no FAT32 filesystem found
    ERROR_FAT32_VOLUME_ID,            // unexpected values in the FAT32 volume ID
    ERROR_FAT32_INIT,                 // FAT32 filesystem has not been mounted
    ERROR_FAT32_CLUSTER_LOOKUP,       // bad value encountered when looking up a FAT32 entry
    ERROR_FAT32_AT_ROOT,              // tried to pop directory when in the root dir
    ERROR_FAT32_NOT_FOUND,            // the file or directory was not found
    ERROR_FAT32_NOT_DIR,              // tried to call push on a file
    ERROR_FAT32_END_OF_DIR,           // tried to read a dir entry after reaching the end
    ERROR_FAT32_NOT_FILE,             // tried to open a directory as a file
    ERROR_FAT32_NOT_OPEN,             // tried to read/write/seek when no file is open
    ERROR_FAT32_TOO_FAR,              // tried to read/seek beyond the file's length
    ERROR_FAT32_ALREADY_EXISTS,       // tried to create a file or dir that already exists
    ERROR_FAT32_FILE_READ_ONLY,       // tried to write to a file that was opened read-only
    ERROR_FAT32_FULL,                 // no free space left on the card
    ERROR_FAT32_INVALID_NAME,         // gave a function a bad name (too long, invalid characters...)
    ERROR_FAT32_NOT_EMPTY,            // tried to call rmdir on an non-empty directory
    ERROR_FAT32_ALREADY_OPEN,         // tried to open a file that is already open
    ERROR_FAT32_TOO_MANY_FILES,       // maximum number of files already open
    ERROR_FAT32_BAD_FILE_ID,          // tried to use a file id higher than the maximum allowed
    NUM_FAT32_ERROR_CODES
};

extern uint8_t error_code;

#if defined(ATMEGA168)
#define MAX_FILES 2
#elif (defined(ATMEGA328) || defined(M2))
#define MAX_FILES 8
#elif (defined(M4))
#define MAX_FILES 32
#else
#error Unknown target
#endif

#define MBR_END_SIGNATURE   (0xaa55)
#define FAT32_END_SIGNATURE (0xaa55)

#define FAT32_END_OF_CHAIN ((uint32_t)0xffffffff)

#pragma pack(1)

typedef struct partition_entry
{
    uint8_t  boot_flag;           // ignore
    uint8_t  chs_begin[3];        // ignore
    uint8_t  type_code;           // should be 0x0b or 0x0c for FAT32
    uint8_t  chs_end[3];          // ignore
    uint32_t start_sector;
    uint32_t number_of_sectors;
} partition_entry;

typedef struct mbr_block
{
    uint8_t boot_code[446];
    partition_entry partition[4];
    uint16_t signature;
} mbr_block;

typedef struct volume_id
{
    uint8_t  jump_instruction[3];
    char     oem_id[8];
    uint16_t bytes_per_sector;      // default is 512
    uint8_t  sectors_per_cluster;   // power of 2 between 1 and 128
    uint16_t reserved_sectors;
    uint8_t  number_of_fats;        // must be 2
    uint16_t fat16_root_entries;    // must be 0
    uint16_t fat16_sectors;         // must be 0
    uint8_t  media_type;
    uint16_t fat16_sectors_per_fat; // must be 0
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;        // sectors before the start of the partition, ignored
    uint32_t fat32_sectors;
    uint32_t fat32_sectors_per_fat;
    uint16_t filler;                // flags to indicate which FAT is used, usually left empty
    uint16_t fat32_version;
    uint32_t root_cluster;          // usually 2
    uint16_t fs_info_sector;        // sector number of the FS info sector, usually 1
    uint16_t backup_boot_sector;    // usually 6
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  extended_boot_sig;     // 0x29 indicates that the next 3 fields exist
    uint32_t volume_serial_number;
    char     volume_label[11];
    char     system_id[8];          // must be "FAT32   "
    uint8_t  bootstrap_code[420];
    uint16_t signature;             // must be FAT32_END_SIGNATURE
} volume_id;

typedef struct fs_info_block
{
	uint32_t lead_signature; // must be 0x41615252
	uint8_t  reserved[480];
	uint32_t structure_signature; // must be 0x61417272
	uint32_t free_cluster_count; // number of free clusters, or -1 if unknown
	uint32_t next_free_cluster;
	uint8_t  reserved2[12];
	uint16_t filler;
	uint16_t boot_signature;  // must be FAT32_END_SIGNATURE
} fs_info_block;
#define FAT32_FREE_CLUSTER_COUNT_OFFSET (4 + 480 + 4)

typedef struct dir_entry
{
    char name[11];
    uint8_t attrib;
    uint8_t filler[8];
    uint16_t first_cluster_high;
    uint8_t filler2[4];
    uint16_t first_cluster_low;
    uint32_t file_size;
} dir_entry;

// possible flags for dir_entry_condensed
#define ENTRY_IS_DIR    1
#define ENTRY_IS_HIDDEN 2
#define ENTRY_IS_EMPTY  4

typedef struct dir_entry_condensed
{
    char name[11];
    uint8_t flags;
    uint32_t first_cluster;  // first cluster of the file/dir
    uint32_t file_size;
} dir_entry_condensed;

typedef enum traverse_option
{
    READ_DIR_START,
    READ_DIR_NEXT,
    ADD_ENTRY,
    REMOVE_ENTRY,
    UPDATE_ENTRY
} traverse_option;

typedef enum open_option
{
    READ_FILE = 0,
    APPEND_FILE,
    CREATE_FILE
} open_option;

typedef struct opened_file
{
    bool open;
    open_option access_type;
    
    uint32_t directory_starting_cluster;
    char name_on_fs[11];
    
    uint32_t first_cluster;
    
    uint32_t seek_offset;
    
    uint32_t current_cluster;
    uint8_t  sector_in_cluster;
    uint16_t offset_in_sector;
    
    uint32_t size;
} opened_file;

#pragma pack()

extern opened_file files[MAX_FILES];


// internal functions:

bool verify_name (const char *name,
                  bool is_dir);

bool fs_filenames_match (const char a[11], const char b[11]);

// get the next cluster from the current cluster
//static bool sd_fat32_cluster_lookup (const uint32_t from_cluster,
//                                     uint32_t *to_cluster);

// look in the FAT for the next unused cluster
//static bool sd_fat32_next_empty_cluster (const uint32_t from_cluster,
//                                         uint32_t *empty_cluster);

// set an entry in the FAT
bool sd_fat32_set_cluster (const uint32_t from_cluster,
                           const uint32_t to_cluster);

// add a cluster to a cluster chain
// (where cluster_in_chain can be any cluster in the cluster chain)
bool sd_fat32_append_cluster (const uint32_t cluster_in_chain,
                              uint32_t *added_cluster);

// destroy the cluster chain of a file or directory
// MAKE SURE DIRECTORIES ARE EMPTY BEFORE CALLING THIS ON THEM!
// this does NOT remove the object from its directory: the idea is to call
// sd_fat32_traverse_directory with REMOVE_OBJECT on an object with a valid
// name, then call this with the updated object to remove the cluster chain
bool sd_fat32_clear_clusters (dir_entry_condensed *object);


// length must divide 512 evenly
bool sd_fat32_fill_sector (const uint32_t sector_num,
                           const uint8_t *pattern,
                           const uint16_t length);

// begin or continue reading directory entries, or add/remove/update an entry
// return value:
//   read:
//     true: the buffer has been filled with an entry's info
//     false: all entries have been read
//   add:
//     true: the entry has been added, and buffer's entry_* variables have been updated
//     false: error adding the entry
//   remove:
//     true: the entry has been removed, and buffer has had its first_cluster
//           and file_size variables updated
//     false: couldn't find the entry or error removing entry
//   update:
//     true: the entry's first_cluster and file_size have been updated
//     false: couldn't find the entry or error modifying entry
//
// when reading: if you change directories, be sure to call this with READ_DIR_START,
// or it will keep reading from the old directory
bool sd_fat32_traverse_directory (dir_entry_condensed *buffer,
                                  traverse_option action);

// search the current directory for an object with the given name
// if the object was found, it fills in result and returns true
// returns false if nothing was found, and the contents of result are undefined
bool sd_fat32_search_dir (const char *name,
                          bool name_already_converted,
                          dir_entry_condensed *result);

// create an object in the current directory
bool sd_fat32_add_object (dir_entry_condensed *object);

// add a cluster to a file
bool extend_file_clusters (opened_file *file);






//==============================================================================
//=============================== USER FUNCTIONS ===============================
//==============================================================================
// Note: Long file names are NOT supported!
//       8.3 characters for files and 8 characters for directories MAX!


//-----------------------------------------------
// Startup and shutdown:

// mount the filesystem
bool sd_fat32_init (void);

// flush any pending writes and unmount the filesystem
bool sd_fat32_shutdown (void);


//-----------------------------------------------
// File and directory information:

// get the size of a file in the current directory
bool sd_fat32_get_size (const char *name,
                        uint32_t *size);

// search the current directory
// if the target is found, *is_directory will be set accordingly
bool sd_fat32_object_exists (const char *name,
                             bool *is_directory);

// iterate through the current directory, reading the names of its objects
// returns false when the end of the directory has been reached
bool sd_fat32_get_dir_entry_first (char name[13]);
bool sd_fat32_get_dir_entry_next  (char name[13]);


//-----------------------------------------------
// Directory traversal and modification:

// enter the directory with the given name
bool sd_fat32_push (const char *name);

// go up one level in the directory path
bool sd_fat32_pop (void);

// create a subdirectory in the current directory
bool sd_fat32_mkdir (const char *name);

// remove an empty directory from the current directory
bool sd_fat32_rmdir (const char *name);

// delete a file from the current directory
// if you delete an open file, the file will be closed first
bool sd_fat32_delete (const char *name);


//-----------------------------------------------
// File access and modification:

// open a file in the current directory
// actions are:
//   READ_FILE:   open an existing file as read-only
//   APPEND_FILE: open an existing file as read-write and seek to the end
//   CREATE_FILE: create a file, deleting any previously existing file
//
// you cannot open a file that is already opened elsewhere
bool sd_fat32_open_file (const char *name,
                         const open_option action,
                         uint8_t *file_id);

// close the file
// does nothing if the file is not open
bool sd_fat32_close_file (uint8_t file_id);

// seek to a location within the opened file
// an offset of FILE_END_POS will seek to the end of the file
#define FILE_END_POS ((uint32_t)0xffffffff)
bool sd_fat32_seek (uint8_t file_id,
                    uint32_t offset);

// get the seek position in the current file
bool sd_fat32_get_seek_pos (uint8_t file_id,
                            uint32_t *offset);

// read from the current location in the file
// updates the seek position
//
// if the length of the read would go beyond the end of the file, an
// error is returned and nothing is read
bool sd_fat32_read_file (uint8_t file_id,
                         uint32_t length,
                         uint8_t *buffer);

// write to the current location in the file
// updates the seek position
bool sd_fat32_write_file (uint8_t file_id,
                          uint32_t length,
                          uint8_t *buffer);



#endif

