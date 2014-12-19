/*******************************************************************************
* sd_fat32.c
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: FAT32 filesystem code for SD cards (and anything else, really,
*              if the functions in sd_highlevel.c and sd_lowlevel.c were
*              replicated for another device)
*******************************************************************************/

#include "sd_fat32.h"

#if defined(FAT32_DEBUG) || defined (FREE_RAM)
#ifndef M4
#include "m_usb.h"
#endif
#endif

#ifdef FREE_RAM
void free_ram (void)
{
  extern int __heap_start, *__brkval; 
  int v; 
  m_usb_tx_string ("Free RAM: ");
  m_usb_tx_uint( (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval) );
  m_usb_tx_char ('\n');
}
#endif

bool fat32_initialized = false;

uint32_t fat32_partition_start_sector;  // start of FAT32 partition
uint32_t fat32_start_sector;  // start, after accounting for reserved/hidden sectors
uint32_t fat32_number_of_sectors;

uint8_t  fat32_sectors_per_cluster;
uint8_t  fat32_number_of_fats;
uint32_t fat32_sectors_per_fat;
uint32_t fat32_cluster_start_sector;
uint32_t fat32_root_first_cluster;

uint32_t fat32_fs_info_sector;
uint32_t fat32_starting_free_cluster_count;
uint32_t fat32_free_cluster_count;

uint32_t current_dir_cluster;

opened_file files[MAX_FILES];


bool verify_name (const char *name,
                  bool is_dir)
{
    uint8_t i = 0;
    while (name[i] != '\0')
    {
        if (i > ((is_dir) ? 7 : 11))
        {
            error_code = ERROR_FAT32_INVALID_NAME;
            return false;
        }
        
        if ((name[i] >= 'a' && name[i] <= 'z') ||
            (name[i] >= 'A' && name[i] <= 'Z') ||
            (name[i] >= '0' && name[i] <= '9') ||
             name[i] == '!' || name[i] == '#' ||
             name[i] == '$' || name[i] == '%' ||
             name[i] == '&' || name[i] == '(' ||
             name[i] == ')' || name[i] == '-' ||
             name[i] == '@' || name[i] == '^' ||
             name[i] == '_' || name[i] == '`' ||
             name[i] == '{' || name[i] == '}' ||
             name[i] == '~' || name[i] == ' ' ||
             name[i] == '.' || name[i] > 127)
        {
            i++;
        }
        else
        {
            error_code = ERROR_FAT32_INVALID_NAME;
            return false;
        }
    }
    
    if (i == 0)
    {
        error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    error_code = ERROR_NONE;
    return true;
}


// check if this entry indicates the end of a cluster chain
static inline bool end_of_chain (const uint32_t cluster_num)
{
    // the end of the chain is indicated by bits 1-27 all being set
    // (the last 4 bits are undefined and 0x?ffffff* indicates reserved,
    // bad, or end-of-chain: treat them all as end-of-chain)
    //
    // cluster numbers under 2 are also invalid and will be treated as
    // end-of-chain markers
    return (cluster_num < 2) || ((cluster_num & (uint32_t)0x0ffffff0) == (uint32_t)0x0ffffff0);
}

// get a sector number from a cluster number
static inline uint32_t cluster_to_sector (const uint32_t cluster_num)
{
    return fat32_cluster_start_sector + (cluster_num - 2) * fat32_sectors_per_cluster;
}

// get the sector of the FAT that contains this cluster
static inline uint32_t fat_cluster_sector (const uint32_t cluster_num)
{
    return fat32_start_sector + (cluster_num >> 7);
}

// get the byte offset in the FAT sector of this cluster
static inline uint32_t fat_cluster_sector_offset (const uint32_t cluster_num)
{
    return (cluster_num % (uint32_t)128) * (uint32_t)4;
}

// look in the FAT for the cluster following this one
static bool sd_fat32_cluster_lookup (const uint32_t from_cluster,
                                     uint32_t *to_cluster)
{
    // each FAT sector contains 128 4-byte cluster entries
    const uint32_t fat_entry_sector = fat_cluster_sector (from_cluster);
    const uint32_t fat_entry_offset = fat_cluster_sector_offset (from_cluster);
    
    if (end_of_chain (from_cluster))
    {  // if the cluster we were given was invalid
        *to_cluster = FAT32_END_OF_CHAIN;
        error_code = ERROR_FAT32_CLUSTER_LOOKUP;
        return false;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    #ifdef FAT32_DEBUG
    bool retval = read_partial_block (fat_entry_sector,
                                      fat_entry_offset,
                                      (uint8_t*)to_cluster,
                                      4);
    
    /*m_usb_tx_string ("Cluster chain: ");
    m_usb_tx_ulong (from_cluster);
    m_usb_tx_string (" -> ");
    m_usb_tx_ulong (*to_cluster);
    m_usb_tx_string ("\n");*/
    
    return retval;
    #else
    return read_partial_block (fat_entry_sector,
                               fat_entry_offset,
                               (uint8_t*)to_cluster,
                               4);
    #endif
}


// look in the FAT for the next unused cluster
static bool sd_fat32_next_empty_cluster (const uint32_t from_cluster,
                                         uint32_t *empty_cluster)
{
    const uint32_t final_cluster =
        (fat32_number_of_sectors - fat32_cluster_start_sector) /
        fat32_sectors_per_cluster;
    
    *empty_cluster = from_cluster;
    uint32_t cluster_value = FAT32_END_OF_CHAIN;  // dummy value
    
    uint32_t fat_entry_sector = fat32_start_sector + (from_cluster >> 7);
    uint32_t fat_entry_offset = (from_cluster % 128) * 4;
    
    while (cluster_value != 0)
    {
        (*empty_cluster)++;
        
        if (*empty_cluster >= final_cluster)
        {
            *empty_cluster = 3;  // first cluster of root dir is 2
            fat_entry_sector = fat32_start_sector;
            fat_entry_offset = 12;
        }
        
        if (*empty_cluster == from_cluster)
        {  // we wrapped around all the way to where we began and didn't find anything
            error_code = ERROR_FAT32_FULL;
            return false;
        }
        
        fat_entry_offset += 4;
        if (fat_entry_offset >= 512)
        {
            fat_entry_offset = 0;
            fat_entry_sector++;
        }
        
        if (!read_partial_block (fat_entry_sector,
                                 fat_entry_offset,
                                 (uint8_t*)&cluster_value,
                                 4))
        {
            return false;
        }
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Next empty cluster: ");
    m_usb_tx_ulong (*empty_cluster);
    m_usb_tx_char ('\n');
    #else
    printf ("Next empty cluster: %04x\n", *empty_cluster);
    #endif
    #endif
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return true;
}


// set an entry in the FAT
bool sd_fat32_set_cluster (const uint32_t from_cluster,
                           const uint32_t to_cluster)
{
    uint32_t target_sector = fat_cluster_sector (from_cluster);
    const uint32_t sector_offset = fat_cluster_sector_offset (from_cluster);
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Set cluster ");
    m_usb_tx_ulong (from_cluster);
    m_usb_tx_string (" (block ");
    m_usb_tx_ulong (target_sector);
    m_usb_tx_string (", offset ");
    m_usb_tx_ulong (sector_offset);
    m_usb_tx_string (") to ");
    m_usb_tx_ulong (to_cluster);
    m_usb_tx_char ('\n');
    #else
    printf ("Set cluster %lu (block %lu, offset %lu) to %lu\n", from_cluster, target_sector, sector_offset, to_cluster);
    #endif
    #endif
    
    // need to set the entry in all FATs
    for (uint8_t fat_index = 0; fat_index < fat32_number_of_fats; fat_index++)
    {
        if (!write_partial_block (target_sector,
                                  sector_offset,
                                  (uint8_t*)&to_cluster,
                                  4))
        {
            return false;
        }
        
        target_sector += fat32_sectors_per_fat;
    }
    
    
    return true;
}


// add a cluster to a cluster chain
// (where cluster_in_chain can be any cluster in the cluster chain)
bool sd_fat32_append_cluster (const uint32_t cluster_in_chain,
                              uint32_t *added_cluster)
{
    uint32_t final_cluster = cluster_in_chain;
    uint32_t next_cluster;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Append cluster to chain that includes ");
    m_usb_tx_ulong (cluster_in_chain);
    m_usb_tx_char ('\n');
    #else
    printf ("Append cluster to chain that includes %lu\n", cluster_in_chain);
    #endif
    #endif
    
    if (end_of_chain (cluster_in_chain))
        return false;  // return false if we were given a bad starting cluster
    
    if (!sd_fat32_cluster_lookup (final_cluster, &next_cluster))
        return false;
    
    while (!end_of_chain (next_cluster))
    {
        final_cluster = next_cluster;
        
        if (!sd_fat32_cluster_lookup (final_cluster, &next_cluster))
            return false;
    }
    
    // final_cluster is now the number of the last cluster in the object's chain
    
    // find the next empty cluster
    if (!sd_fat32_next_empty_cluster (final_cluster, added_cluster))
        return false;
    
    // set that cluster as the new end of the chain
    if (!sd_fat32_set_cluster (*added_cluster, FAT32_END_OF_CHAIN))
        return false;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    if (fat32_free_cluster_count != (uint32_t)0xffffffff)
    {  // if the FS supports free cluster count
        fat32_free_cluster_count--;
    }
    
    // make the no-longer-final cluster to point to the no-longer-empty cluster
    return sd_fat32_set_cluster (final_cluster, *added_cluster);
}



// length must divide 512 evenly
bool sd_fat32_fill_sector (const uint32_t sector_num,
                           const uint8_t *pattern,
                           const uint16_t length)
{
    for (uint16_t i = 0; i < 512; i += length)
    {
        if (!write_partial_block (sector_num, i, pattern, length))
            return false;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return true;
}


// destroy the cluster chain of a file or directory
// MAKE SURE DIRECTORIES ARE EMPTY BEFORE CALLING THIS ON THEM!
// this does NOT remove the object from its directory: the idea is to call
// sd_fat32_traverse_directory with REMOVE_OBJECT on an object with a valid
// name, then call this with the updated object to remove the cluster chain
bool sd_fat32_free_clusters (dir_entry_condensed *object)
{
    uint32_t current_cluster = object->first_cluster;
    uint32_t next_cluster;
    
    if (end_of_chain (current_cluster))
    {  // if the first cluster was invalid, the file was empty
        return true;
    }
    
    while (1)
    {
        if (!sd_fat32_cluster_lookup (current_cluster, &next_cluster))
            return false;
        
        if (!sd_fat32_set_cluster (current_cluster, 0))
            return false;
        
        if (fat32_free_cluster_count != (uint32_t)0xffffffff)
        {  // if the FS supports free cluster count
            fat32_free_cluster_count++;
        }
        
        if (end_of_chain (next_cluster))
            return true;
        
        current_cluster = next_cluster;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return true;
}


bool fs_filenames_match (const char a[11], const char b[11])
{
    for (uint8_t i = 0; i < 11; i++)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}


// read the mbr and locate the first FAT32 partition we find
bool init_mbr (void)
{
    //mbr_block mbr;
    // mbr_block is too large to read and copy
    // instead, read to a dummy variable, then grab directly from the cached sector
    
    uint8_t dummy;
    if (!read_partial_block (0, 0, &dummy, 1))
        return false;
    
    cached_sector *sector = cache_lookup (0);
    if (sector == END_OF_CHAIN)
    {  // the sector should be in the cache now
        error_code = ERROR_CACHE_FAILURE;
        return false;
    }
    mbr_block *mbr = (mbr_block*)sector->data;
    
    if (mbr->signature != MBR_END_SIGNATURE)
    {
        error_code = ERROR_MBR;
        return false;
    }
    
    fat32_partition_start_sector = 0;
    fat32_start_sector = 0;
    fat32_number_of_sectors = 0;
    
    // open the first FAT32 partition found
    for (uint8_t i = 0; i < 4; i++)
    {
        /*
        #ifdef FAT32_DEBUG
        m_usb_tx_string ("partition type: ");
        m_usb_tx_hex (mbr.partition[i].type_code);
        m_usb_tx_string ("\n");
        #endif
        */
        
        if (mbr->partition[i].type_code == 0x0b || mbr->partition[i].type_code == 0x0c)
        {  // found a FAT32 partition
            fat32_partition_start_sector = mbr->partition[i].start_sector;
            fat32_start_sector           = mbr->partition[i].start_sector;
            fat32_number_of_sectors      = mbr->partition[i].number_of_sectors;
            break;
        }
    }
    
    if (fat32_start_sector == 0 || fat32_number_of_sectors == 0)
    {  // couldn't find a FAT32 partition in the partition table
        error_code = ERROR_NO_FAT32;
        return false;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// read info from the first sector of the filesystem
bool extract_fs_info (void)
{
    //volume_id volume;
    // volume_id is too large to read and copy
    // instead, read to a dummy variable, then grab directly from block[]
    
    uint8_t dummy;
    if (!read_partial_block (fat32_partition_start_sector,
                             0,
                             &dummy,
                             1))
    {
        return false;
    }
    
    cached_sector *sector = cache_lookup (fat32_partition_start_sector);
    if (sector == END_OF_CHAIN)
    {  // the sector should be in the cache now
        error_code = ERROR_CACHE_FAILURE;
        return false;
    }
    volume_id *volume = (volume_id*)sector->data;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("start sector: ");
    m_usb_tx_ulong (fat32_partition_start_sector);
    m_usb_tx_string ("\nnumber of sectors: ");
    m_usb_tx_ulong (fat32_number_of_sectors);
    m_usb_tx_string ("\nhidden sectors: ");
    m_usb_tx_ulong (volume->hidden_sectors);
    m_usb_tx_string ("\nreserved sectors: ");
    m_usb_tx_ulong (volume->reserved_sectors);
    m_usb_tx_string ("\nsectors per FAT: ");
    m_usb_tx_ulong (volume->fat32_sectors_per_fat);
    m_usb_tx_string ("\nnumber of FATs: ");
    m_usb_tx_ulong (volume->number_of_fats);
    m_usb_tx_string ("\n");
    #else
    printf ("start sector: %lu\n", fat32_partition_start_sector);
    printf ("number of sectors: %lu\n", fat32_number_of_sectors);
    printf ("hidden sectors: %lu\n", volume->hidden_sectors);
    printf ("reserved sectors: %lu\n", volume->reserved_sectors);
    printf ("sectors per FAT: %lu\n", volume->fat32_sectors_per_fat);
    printf ("number of FATs: %lu\n", volume->number_of_fats);
    #endif
    #endif
    
    // check for expected values
    if (volume->bytes_per_sector      != 512 ||
        volume->number_of_fats        !=   2 ||
        volume->fat16_root_entries    !=   0 ||
        volume->fat16_sectors         !=   0 ||
        volume->fat16_sectors_per_fat !=   0 ||
        volume->system_id[0]          != 'F' ||
        volume->system_id[1]          != 'A' ||
        volume->system_id[2]          != 'T' ||
        volume->system_id[3]          != '3' ||
        volume->system_id[4]          != '2' ||
        volume->signature != FAT32_END_SIGNATURE)
    {
        error_code = ERROR_FAT32_VOLUME_ID;
        return false;
    }
    
    // adjust the start sector to account for the hidden and reserved sectors
    fat32_start_sector += volume->hidden_sectors + volume->reserved_sectors;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("new start sector: ");
    m_usb_tx_ulong (fat32_start_sector);
    m_usb_tx_string ("\nnumber of sectors from MBR = ");
    m_usb_tx_long (fat32_number_of_sectors);
    m_usb_tx_string (", from volume = ");
    m_usb_tx_ulong (volume->fat32_sectors);
    m_usb_tx_string ("\n");
    #else
    printf ("new start sector: %lu\n", fat32_start_sector);
    printf ("number of sectors from MBR = %ld, from volume = %lu\n", fat32_number_of_sectors, volume->fat32_sectors);
    #endif
    #endif
    
    // (use volume->fat32_sectors instead of the number of sectors we got from the MBR)
    fat32_number_of_sectors = volume->fat32_sectors;
    
    // extract all the information we need
    fat32_sectors_per_cluster = volume->sectors_per_cluster;
    fat32_cluster_start_sector = fat32_start_sector +
                                 (volume->fat32_sectors_per_fat *
                                  volume->number_of_fats);
    fat32_root_first_cluster = volume->root_cluster;
    fat32_sectors_per_fat = volume->fat32_sectors_per_fat;
    fat32_number_of_fats = volume->number_of_fats;
    
    if (volume->fs_info_sector == (uint16_t)0 || volume->fs_info_sector == (uint16_t)0xffff)
    {  // free cluster count is unsupported
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("FAT32 cluster count unsupported, FS info sector is ");
        m_usb_tx_hex (volume->fs_info_sector);
        m_usb_tx_string ("\n");
        #else
        printf ("FAT32 cluster count unsupported, FS info sector is %04x\n", volume->fs_info_sector);
        #endif
        #endif
        
        fat32_fs_info_sector = 0;
        fat32_starting_free_cluster_count = (uint32_t)0xffffffff;
        fat32_free_cluster_count = (uint32_t)0xffffffff;
    }
    else
    {
        fat32_fs_info_sector = fat32_partition_start_sector + (uint32_t)volume->fs_info_sector;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("FAT32 info sector: sector ");
        m_usb_tx_ulong ((uint32_t)volume->fs_info_sector);
        m_usb_tx_string ("\n");
        #else
        printf ("FAT32 info sector: sector %lu + %lu = %lu\n", fat32_partition_start_sector, (uint32_t)volume->fs_info_sector, fat32_fs_info_sector);
        #endif
        #endif
        
        // the FS info block is also too large to copy, so read that directly too
        if (!read_partial_block (fat32_fs_info_sector,
                                 0,
                                 &dummy,
                                 1))
        {
            return false;
        }
        
        cached_sector *sector = cache_lookup (fat32_fs_info_sector);
        if (sector == END_OF_CHAIN)
        {  // the sector should be in the cache now
            error_code = ERROR_CACHE_FAILURE;
            return false;
        }
        fs_info_block *fs_info = (fs_info_block*)sector->data;
        
        // make sure this really is the FS info block by checking its signatures
        if (fs_info->lead_signature      != (uint32_t)0x41615252 ||
            fs_info->structure_signature != (uint32_t)0x61417272 ||
            fs_info->boot_signature      != FAT32_END_SIGNATURE)
        {  // invalid FS info signature, mark the free cluster count as unused
            #ifdef FAT32_DEBUG
            #ifndef M4
            m_usb_tx_string ("Bad FS info signatures:\n\t");
            m_usb_tx_ulong (fs_info->lead_signature);
            m_usb_tx_string (" vs expected ");
            m_usb_tx_ulong ((uint32_t)0x41615252);
            m_usb_tx_string ("\n\t");
            m_usb_tx_ulong (fs_info->structure_signature);
            m_usb_tx_string (" vs expected ");
            m_usb_tx_ulong ((uint32_t)0x61417272);
            m_usb_tx_string ("\n\t");
            m_usb_tx_hex (fs_info->boot_signature);
            m_usb_tx_string (" vs expected ");
            m_usb_tx_hex (FAT32_END_SIGNATURE);
            #else
            printf ("Bad FS info signatures:\n\t%lu vs expected %lu", fs_info->lead_signature, (uint32_t)0x41615252);
            printf ("\n\t%lu vs expected %lu", fs_info->structure_signature, (uint32_t)0x61417272);
            printf ("\n\t%04x vs expected %04x\n", fs_info->boot_signature, FAT32_END_SIGNATURE);
            #endif
            #endif
            
            fat32_fs_info_sector = 0;
            fat32_starting_free_cluster_count = (uint32_t)0xffffffff;
            fat32_free_cluster_count = (uint32_t)0xffffffff;
        }
        else
        {  // valid FS info signatures, retrieve the current free cluster count
            fat32_free_cluster_count = fs_info->free_cluster_count;
            fat32_starting_free_cluster_count = fat32_free_cluster_count;
            
            #ifdef FAT32_DEBUG
            #ifndef M4
            m_usb_tx_string ("Free clusters: ");
            m_usb_tx_ulong (fat32_free_cluster_count);
            m_usb_tx_string ("\n");
            #else
            printf ("Free clusters %lu\n", fat32_free_cluster_count);
            #endif
            #endif
        }
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("sectors per cluster: ");
    m_usb_tx_uint (fat32_sectors_per_cluster);
    m_usb_tx_string ("\nsector of first cluster: ");
    m_usb_tx_ulong (fat32_cluster_start_sector);
    m_usb_tx_string ("\nfirst cluster of root dir: ");
    m_usb_tx_ulong (fat32_root_first_cluster);
    m_usb_tx_string ("\n");
    #else
    printf ("sectors per cluster: %u\n", fat32_sectors_per_cluster);
    printf ("sector of first cluster: %lu\n", fat32_cluster_start_sector);
    printf ("first cluster of root dir: %lu\n", fat32_root_first_cluster);
    #endif
    #endif
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// mount the filesystem
bool sd_fat32_init (void)
{
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("initializing FAT32\n");
    #else
    printf ("initializing FAT32\n");
    #endif
    #endif
    
    fat32_initialized = false;
    if (!init_card (USE_CRC))
        return false;  // error_code will have been set in init_card
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Card OK\n");
    #else
    printf ("Card OK\n");
    #endif
    #endif
    
    if (!init_mbr())
        return false;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("MBR OK\n");
    #else
    printf ("MBR OK\n");
    #endif
    #endif
    
    if (!extract_fs_info())
        return false;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("FAT32 FS OK\n");
    #else
    printf ("FAT32 FS OK\n");
    #endif
    #endif
    
    fat32_initialized = true;
    
    for (uint8_t i = 0; i < MAX_FILES; i++)
    {
        opened_file *file = &(files[i]);
        
        file->open = false;
        file->directory_starting_cluster = 0;
        file->access_type = READ_FILE;
        for (uint8_t i = 0; i < 11; i++)
            file->name_on_fs[i] = ' ';
        
        file->first_cluster = 0;
        file->seek_offset = 0;
        file->current_cluster = 0;
        file->sector_in_cluster = 0;
        file->offset_in_sector = 0;
        file->size = 0;
    }
    
    current_dir_cluster = fat32_root_first_cluster;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// unmount the filesystem
bool sd_fat32_shutdown (void)
{
    // close any opened files
    for (uint8_t i = 0; i < MAX_FILES; i++)
    {
        if (!sd_fat32_close_file (i))
            return false;
    }
    
    if (fat32_free_cluster_count != (uint32_t)0xffffffff &&
        fat32_fs_info_sector != 0)
    {  // if the FS supports a free cluster count
        if (fat32_starting_free_cluster_count != fat32_free_cluster_count)
        {  // if the value has changed
            #ifdef FAT32_DEBUG
            #ifndef M4
            m_usb_tx_string ("Updating free cluster count: ");
            m_usb_tx_ulong (fat32_free_cluster_count);
            m_usb_tx_string ("\n");
            #else
            printf ("Updating free cluster count: %lu\n", fat32_free_cluster_count);
            #endif
            #endif
            
            if (!write_partial_block (fat32_fs_info_sector,
                                      FAT32_FREE_CLUSTER_COUNT_OFFSET,
                                      (uint8_t*)&fat32_free_cluster_count,
                                      4))
            {
                return false;
            }
        }
        #ifdef FAT32_DEBUG
        else
        {
            #ifndef M4
            m_usb_tx_string ("Free cluster count unchanged: ");
            m_usb_tx_ulong (fat32_free_cluster_count);
            m_usb_tx_string ("\n");
            #else
            printf ("Free cluster count unchanged: %lu\n", fat32_free_cluster_count);
            #endif
        }
        #endif
    }
    #ifdef FAT32_DEBUG
    else
    {
        #ifndef M4
        m_usb_tx_string ("Free cluster count unsupported\n");
        #else
        printf ("Free cluster count unsupported\n");
        #endif
    }
    #endif
    
    // write out all modified cached sectors
    if (!flush_cache())
        return false;
    
    fat32_initialized = false;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    // send the card a bunch of cycles to let it finish up anything it needs
    attempt_resync();
    
    // reset the card
    if (reset_card() != SPI_OK)
        return false;
    
    return true;
}




// begin or continue reading directory entries, or add/remove/update an entry
// return value:
//   read:
//     true: the buffer has been filled with an entry's info
//     false: all entries have been read, or error
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
                                  traverse_option action)
{
    static uint32_t current_cluster = 0;
    static uint32_t current_sector = 0;
    static uint16_t sector_offset = 0;
    
    uint32_t new_cluster = 0;
    const uint8_t fillvalue[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0};
    
    if (action != READ_DIR_NEXT)
    {  // start at the beginning of the entry list
        current_cluster = current_dir_cluster;
        current_sector = cluster_to_sector (current_cluster);
        sector_offset = 0;
    }
    
    if (current_sector == 0)
    {  // this should only happen if READ_DIR_NEXT is used when it shouldn't be
        error_code = ERROR_FAT32_END_OF_DIR;
        return false;
    }
    
    dir_entry entry_to_add;
    uint32_t entry_add_sector = (uint32_t)0xffffffff;
    uint16_t entry_add_offset = 0xffff;
    if (action == ADD_ENTRY)  // if we need to add an entry
    {  // convert the dir_entry_condensed into a regular dir_entry
        for (uint8_t i = 0; i < 11; i++)
            entry_to_add.name[i] = buffer->name[i];
        
        // filler values for file entries
        // these values found by looking at files created with Linux's FAT32 driver
        const uint8_t filler1[8] = {0x20, 0x00, 0x64, 0xa5, 0x7c, 0x64, 0x42, 0x92};
        const uint8_t filler2[4] = {0xa5, 0x7c, 0x64, 0x42};
        
        memset (entry_to_add.filler,  filler1, 8);
        memset (entry_to_add.filler2, filler2, 4);
        
        entry_to_add.attrib = 0;
        if (buffer->flags & ENTRY_IS_DIR)
            entry_to_add.attrib |= 0b00010000;
        if (buffer->flags & ENTRY_IS_HIDDEN)
            entry_to_add.attrib |= 0x01;
        
        entry_to_add.first_cluster_high = (uint16_t)((buffer->first_cluster >> 16) & (uint32_t)0x0000ffff);
        entry_to_add.first_cluster_low  = (uint16_t)(buffer->first_cluster & (uint32_t)0x0000ffff);
        
        entry_to_add.file_size = buffer->file_size;
    }
    
    dir_entry entry;
    bool extend_and_end = false;
    
    while (1)
    {  // continue reading entries until we find what want, or reach the end
        if ( !read_partial_block (current_sector,
                                  sector_offset,
                                  (uint8_t*)&entry,
                                  sizeof (dir_entry)) )
        {
            #ifdef FAT32_DEBUG
            #ifndef M4
            m_usb_tx_string ("Error reading entry: ");
            m_usb_tx_uint (error_code);
            m_usb_tx_string ("\n");
            #else
            printf ("Error reading entry: %u\n", error_code);
            #endif
            #endif
            return false;
        }
        
        if (entry.name[0] == (char)0xe5)  // unused entry
        {
            if (action == ADD_ENTRY)
            {  // if we're trying to add an entry
                // replace this unused entry with the new one
                return write_partial_block (current_sector,
                                            sector_offset,
                                            (uint8_t*)&entry_to_add,
                                            sizeof (dir_entry));
            }
        }
        
        if (entry.name[0] == (char)0x00)  // end of directory list
        {
            if (action == ADD_ENTRY)
            {
                // mark where the entry should be added
                entry_add_sector = current_sector;
                entry_add_offset = sector_offset;
                
                // then flag that a new end-of-dir entry must be created
                extend_and_end = true;
                
                // (don't write the entry immediately; make sure we can
                // extend the end-of-dir marker first so bad things don't
                // happen in situations where free space is low)
            }
            else if (action == REMOVE_ENTRY || action == UPDATE_ENTRY)
            {  // we reached the end of the directory without finding the entry to remove
                error_code = ERROR_FAT32_END_OF_DIR;
                return false;
            }
            else  // reading directory entries
            {  // indicate that we've reached the end
                error_code = ERROR_NONE;
                return false;
            }
        }
        
        if (action == REMOVE_ENTRY || action == UPDATE_ENTRY)
        {  // if we want to remove or update an entry, check the name
            if (fs_filenames_match (buffer->name, entry.name))
            {  // we found the entry
                if (action == REMOVE_ENTRY)
                {
                    // change the first character of the entry to make it indicate "empty"
                    entry.name[0] = (uint8_t)0xe5;
                    
                    // gather information about the file getting removed
                    buffer->first_cluster = (uint32_t)entry.first_cluster_high;
                    buffer->first_cluster <<= 16;
                    buffer->first_cluster |= (uint32_t)entry.first_cluster_low & (uint32_t)0x0000ffff;
                    buffer->file_size = entry.file_size;
                    
                    if (!write_partial_block (current_sector,
                                              sector_offset,
                                              (uint8_t*)&entry,
                                              sizeof (dir_entry)) )
                    {
                        return false;
                    }
                }
                else
                {  // update the entry's starting cluster and file size
                    entry.file_size = buffer->file_size;
                    entry.first_cluster_high = (uint16_t)((buffer->first_cluster >> 16) & (uint32_t)0x0000ffff);
                    entry.first_cluster_low  = (uint16_t)(buffer->first_cluster & (uint32_t)0x0000ffff);
                    
                    if (!write_partial_block (current_sector,
                                              sector_offset,
                                              (uint8_t*)&entry,
                                              sizeof (dir_entry)) )
                    {
                        return false;
                    }
                }
                
                #ifdef FREE_RAM
                free_ram();
                #endif
                
                // finished removing or updating
                error_code = ERROR_NONE;
                return true;
            }
        }
        
        sector_offset += sizeof (dir_entry);  // increment the offset
        if (sector_offset >= 512)
        {  // if we hit the end of the sector
            current_sector++;
            sector_offset = 0;
            
            if (current_sector % fat32_sectors_per_cluster == 0)
            {  // if we reached the end of the cluster
                new_cluster = 0;
                
                if (!sd_fat32_cluster_lookup (current_cluster, &new_cluster))
                {  // hit an error trying to read from the FAT
                    return false;
                }
                
                if (end_of_chain (new_cluster))
                {  // this is where the cluster chain stops
                    if (extend_and_end)
                    {  // if we need to append a new end-of-dir marker
                        // add a new cluster to the chain
                        #ifdef FAT32_DEBUG
                        #ifndef M4
                        m_usb_tx_string ("appending new cluster\n");
                        #else
                        printf ("appending new cluster\n");
                        #endif
                        #endif
                        
                        if (!sd_fat32_append_cluster (current_cluster, &new_cluster))
                            return false;
                        
                        current_cluster = new_cluster;
                        current_sector = cluster_to_sector (current_cluster);
                        
                        #ifdef FAT32_DEBUG
                        #ifndef M4
                        m_usb_tx_string ("New final cluster: ");
                        m_usb_tx_ulong (new_cluster);
                        m_usb_tx_char ('\n');
                        #else
                        printf ("New final cluster: %lu\n", new_cluster);
                        #endif
                        #endif
                        
                        // fill the new cluster with zeroes
                        for (uint32_t n = 0; n < fat32_sectors_per_cluster; n++)
                        {
                            if (!sd_fat32_fill_sector (cluster_to_sector (new_cluster) + n,
                                                       fillvalue,
                                                       16))
                                return false;
                        }
                    }
                    else
                    {  // if reading, treat this as an end-of-dir marker
                        error_code = ERROR_FAT32_END_OF_DIR;
                        return false;
                    }
                }
                else
                {
                    current_cluster = new_cluster;
                    current_sector = cluster_to_sector (current_cluster);
                }
            }
        }
        
        if (extend_and_end)
        {
            // write the end-of-dir marker to the current location
            entry.name[0] = 0;
            if (!write_partial_block (current_sector,
                                      sector_offset,
                                      (uint8_t*)&entry,
                                      sizeof (dir_entry)) )
            {
                return false;
            }
            
            // then write the new entry to its marked location
            if (!write_partial_block (entry_add_sector,
                                      entry_add_offset,
                                      (uint8_t*)&entry_to_add,
                                      sizeof (dir_entry)) )
            {
                return false;
            }
            
            #ifdef FREE_RAM
            free_ram();
            #endif
            
            // we've added the new entry and the new end
            return true;
        }
        
        // -----
        // if we reached this point, we've read a valid entry
        // -----
        
        if (action == ADD_ENTRY || action == REMOVE_ENTRY || action == UPDATE_ENTRY)
        {  // if we want to add/remove/update something, skip entries here
            continue;
        }
        else
        {  // if we're just reading entries
            // copy the relevant data into the buffer and return
            for (uint8_t i = 0; i < 11; i++)
                buffer->name[i] = entry.name[i];
            
            buffer->flags = 0;
            if (entry.name[0] == (char)0xe5)  // unused entry
                buffer->flags |= ENTRY_IS_EMPTY;
            if (entry.attrib & 0x01)  // hidden entry (typically part of a long file name)
                buffer->flags |= ENTRY_IS_HIDDEN;
            if (entry.attrib & 0b00010000)  // entry is a directory
                buffer->flags |= ENTRY_IS_DIR;
                
            buffer->first_cluster = (uint32_t)entry.first_cluster_high;
            buffer->first_cluster <<= 16;
            buffer->first_cluster |= entry.first_cluster_low;
            buffer->file_size = entry.file_size;
            
            #ifdef FREE_RAM
            free_ram();
            #endif
            
            error_code = ERROR_NONE;
            return true;
        }
    }
    
    error_code = ERROR_FAT32_END_OF_DIR;
    return false;  // we should never get here
}


// convert file names from their representation on disk
// eg., "TEST    TXT" becomes "TEST.TXT"
void filename_fs_to_8_3 (const char *input_name,
                         char output_name[13])
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    bool wrote_dot = false;
    
    while (in_index < 11 && out_index < 12)
    {
        if (in_index < 8)
        {
            if (input_name[in_index] == ' ')
            {
                in_index = 8;  // skip to the last 3 chars
                continue;
            }
            else
            {
                output_name[out_index++] = input_name[in_index];
            }
        }
        else
        {  // extension characters
            if (input_name[in_index] != ' ')
            {
                if (!wrote_dot)
                {
                    output_name[out_index++] = '.';
                    wrote_dot = true;
                }
                output_name[out_index++] = input_name[in_index];
            }
        }
        
        in_index++;
    }
    
    output_name[out_index] = '\0';
}


// convert file names into their representation on disk
// eg., "test.txt" becomes "TEST    TXT"
void filename_8_3_to_fs (const char *input_name,
                         char *output_name)
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    char in_char;
    
    while (out_index < 11)
    {
        if (in_index >= 12 || input_name[in_index] == '\0')
        {
            break;
        }
        else if (input_name[in_index] == '.')
        {  // encountered a period
            while (out_index < 8)
            {  // fill the output with spaces until the 3-char extension
                output_name[out_index] = ' ';
                out_index++;
            }
            
            in_index++;
        }
        else
        {
            in_char = input_name[in_index];
            
            // convert to uppercase
            if (in_char >= (char)0x61 && in_char <= (char)0x7a)
				in_char -= 0x20;
            
            output_name[out_index] = in_char;
            
            in_index++;
            out_index++;
        }
    }
    
    while (out_index < 11)
    {
        output_name[out_index] = ' ';
        out_index++;
    }
}


// search the current directory for a file or directory
bool sd_fat32_search_dir (const char *search_name,
                          bool name_already_converted,
                          dir_entry_condensed *result)
{
    char name_8_3[11];
    
    if (name_already_converted)
    {
        for (uint8_t i = 0; i < 11; i++)
            name_8_3[i] = search_name[i];
    }
    else
    {
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("filename to search for: '");
        for (uint8_t i = 0; i < 12; i++)
        {
            if (search_name[i] == '\0')
                break;
            m_usb_tx_char (search_name[i]);
        }
        m_usb_tx_string ("'\n");
        #else
        printf ("filename to search for: '");
        for (uint8_t i = 0; i < 12; i++)
        {
            if (search_name[i] == '\0')
                break;
            printf ("%c", search_name[i]);
        }
        printf ("'\n");
        #endif
        #endif
        
        if (search_name[0] == '.' && search_name[1] == '\0')
        {
            name_8_3[0] = '.';
            for (uint8_t i = 1; i < 11; i++)
                name_8_3[i] = ' ';
        }
        else if (search_name[0] == '.' && search_name[1] == '.' && search_name[2] == '\0')
        {
            name_8_3[0] = '.';
            name_8_3[1] = '.';
            for (uint8_t i = 2; i < 11; i++)
                name_8_3[i] = ' ';
        }
        else
        {
            filename_8_3_to_fs (search_name, name_8_3);
        }
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("search for filename on disk: '");
    for (uint8_t i = 0; i < 11; i++)
        m_usb_tx_char (name_8_3[i]);
    m_usb_tx_string ("'\n");
    #else
    printf ("search for filename on disk: '");
    for (uint8_t i = 0; i < 11; i++)
        printf ("%c", name_8_3[i]);
    printf ("'\n");
    #endif
    #endif
    
    bool read_result = sd_fat32_traverse_directory (result, READ_DIR_START);
    while (read_result)
    {
        // check this entry's name to see if it matches what we're looking for
        // if it matches, great; if not, check the next entry
        if (fs_filenames_match (result->name, name_8_3))
        {
            #ifdef FREE_RAM
            free_ram();
            #endif
            
            error_code = ERROR_NONE;
            return true;
        }
        else
        {
            read_result = sd_fat32_traverse_directory (result, READ_DIR_NEXT);
        }
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    // out of entries to look through, or low-level error
    if (error_code == ERROR_FAT32_END_OF_DIR)
        error_code = ERROR_FAT32_NOT_FOUND;
    return false;
}


// enter a directory
bool sd_fat32_push (const char *name)
{
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, true))
        return false;
    
    dir_entry_condensed result;
    if (!sd_fat32_search_dir (name, false, &result))
    {
        error_code = ERROR_FAT32_NOT_FOUND;
        return false;
    }
    
    if (!(result.flags & ENTRY_IS_DIR))
    {
        error_code = ERROR_FAT32_NOT_DIR;
        return false;
    }
    
    if (result.first_cluster == 0)
    {  // sometimes ".." in a subdirectory of root points to 0, instead of to root's first cluster
        result.first_cluster = fat32_root_first_cluster;
    }
    
    current_dir_cluster = result.first_cluster;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// leave the current directory and go up one level
bool sd_fat32_pop (void)
{
    if (current_dir_cluster == fat32_root_first_cluster)
    {
        error_code = ERROR_FAT32_AT_ROOT;
        return false;
    }
    
    return sd_fat32_push ("..");
}


// get the size of a file in a directory
bool sd_fat32_get_size (const char *name,
                        uint32_t *size)
{
    dir_entry_condensed entry;
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, false))
        return false;
    
    if (!sd_fat32_search_dir (name, false, &entry))
    {
        error_code = ERROR_FAT32_NOT_FOUND;
        return false;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    *size = entry.file_size;
    error_code = ERROR_NONE;
    return true;
}


// search the current directory
// if the target is found, *is_directory will be set accordingly
bool sd_fat32_object_exists (const char *name,
                             bool *is_directory)
{
    dir_entry_condensed entry;
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, false))
        return false;
    
    if (!sd_fat32_search_dir (name, false, &entry))
        return false;
    
    *is_directory = (bool)(entry.flags & ENTRY_IS_DIR);
    error_code = ERROR_NONE;
    return true;
}


// iterate through the current directory, reading the names of its objects
// returns false when the end of the directory has been reached
bool sd_fat32_get_dir_entry_first (char name[13])
{
    dir_entry_condensed entry;
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    name[0] = '\0';  // make the name empty to start
    
    if (!sd_fat32_traverse_directory (&entry, READ_DIR_START))
        return false;
    
    while (entry.flags & (ENTRY_IS_EMPTY | ENTRY_IS_HIDDEN))
    {  // if this entry is empty or hidden, keep reading
        if (!sd_fat32_traverse_directory (&entry, READ_DIR_NEXT))
            return false;
    }
    
    filename_fs_to_8_3 (entry.name, name);
    
    error_code = ERROR_NONE;
    return true;
}

bool sd_fat32_get_dir_entry_next  (char name[13])
{
    dir_entry_condensed entry;
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    name[0] = '\0';  // make the name empty to start
    
    if (!sd_fat32_traverse_directory (&entry, READ_DIR_NEXT))
        return false;
    
    while (entry.flags & (ENTRY_IS_EMPTY | ENTRY_IS_HIDDEN))
    {  // if this entry is empty or hidden, keep reading
        if (!sd_fat32_traverse_directory (&entry, READ_DIR_NEXT))
            return false;
    }
    
    filename_fs_to_8_3 (entry.name, name);
    
    error_code = ERROR_NONE;
    return true;
}


// create an object in the current directory
bool sd_fat32_add_object (dir_entry_condensed *object)
{
    dir_entry_condensed new_obj;
    uint32_t new_cluster;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    // filler values for directory entries
    // these values found by looking at directories created with Linux's FAT32 driver
    const uint8_t filler1[8] = {0x00, 0x00, 0x43, 0x8d, 0x6b, 0x42, 0x6b, 0x42};
    const uint8_t filler2[4] = {0x43, 0x8d, 0x6b, 0x42};
    
    // make sure an object of this name doesn't already exist
    if (sd_fat32_search_dir (object->name, true, &new_obj))
    {
        error_code = ERROR_FAT32_ALREADY_EXISTS;
        return false;
    }
    else if (error_code != ERROR_NONE && error_code != ERROR_FAT32_END_OF_DIR)
    {  // make sure the search function didn't return false for some other reason
        return false;
    }
    
    if (object->flags & ENTRY_IS_DIR)
    {  // if the new object is a directory
        
        // find an empty cluster
        if (!sd_fat32_next_empty_cluster (current_dir_cluster, &new_cluster))
            return false;
        
        // set that cluster as the end of the cluster chain
        if (!sd_fat32_set_cluster (new_cluster, FAT32_END_OF_CHAIN))
            return false;
        
        // make the entry contain its starting cluster
        object->first_cluster = new_cluster;
        
        // directly write entries for ".", "..", and the end-of-dir marker:
        
        dir_entry entry;
        
        
        // fill the entire cluster with zeroes:
        
        // fill the entry with zeroes
        for (uint8_t i = 0; i < sizeof (dir_entry); i++)
            ((uint8_t*)&entry)[i] = 0;
        
        // fill all the sectors in the cluster with the same zero'd entry
        for (uint32_t n = 0; n < (uint32_t)fat32_sectors_per_cluster; n++)
        {
            if (!sd_fat32_fill_sector (cluster_to_sector (new_cluster) + n,
                                       (uint8_t*)&entry,
                                       sizeof (dir_entry)) )
            {
                return false;
            }
        }
        
        
        // first entry: .
                
        // set the name to ".           "
        entry.name[0] = '.';
        for (uint8_t i = 1; i < 11; i++)
            entry.name[i] = ' ';
        
        entry.attrib = 0b00010000;  // set as a directory
        
        for (uint8_t i = 0; i < 8; i++)
            entry.filler[i] = filler1[i];
        
        // . points to itself
        entry.first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xffff);
        entry.first_cluster_low  = (uint16_t)(new_cluster & 0xffff);
        
        for (uint8_t i = 0; i < 4; i++)
            entry.filler2[i] = filler2[i];
        
        entry.file_size = 0;
        
        if (!write_partial_block (cluster_to_sector (new_cluster),
                                  0,
                                  (uint8_t*)&entry,
                                  sizeof (dir_entry)) )
        {
            return false;
        }
        
        
        // next entry: ..
        
        // just update the changed fields
        entry.name[1] = '.';
        
        // ".." points to containing directory
        // if the containing directory is root, ".." should point to cluster 0
        uint32_t dir_cluster = current_dir_cluster;
        if (dir_cluster == fat32_root_first_cluster)
            dir_cluster = 0;
        
        entry.first_cluster_high = (uint16_t)((dir_cluster >> 16) & 0xffff);
        entry.first_cluster_low  = (uint16_t)(dir_cluster & 0xffff);
        
        if (!write_partial_block (cluster_to_sector (new_cluster),
                                  sizeof (dir_entry),
                                  (uint8_t*)&entry,
                                  sizeof (dir_entry)) )
        {
            return false;
        }
    }
    else
    {  // created an empty file: no clusters are allocated
        object->first_cluster = 0;  // in this case, starting cluster must be 0
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    // add the entry to the current directory
    return sd_fat32_traverse_directory (object, ADD_ENTRY);
}


// open a file in the current directory
bool sd_fat32_open_file (const char *name,
                         const open_option action,
                         uint8_t *file_id)
{
    dir_entry_condensed entry;
    opened_file *file;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, false))
        return false;
    
    // check if the file is open already
    for (uint8_t i = 0; i < MAX_FILES; i++)
    {
        if (files[i].open)
        {
            if (files[i].directory_starting_cluster == current_dir_cluster)
            {
                char fs_name[11];
                filename_8_3_to_fs (name, fs_name);
                
                if (fs_filenames_match (files[i].name_on_fs, fs_name))
                {
                    error_code = ERROR_FAT32_ALREADY_OPEN;
                    return false;
                }
            }
        }
    }
    
    // check for an empty file id
    for (*file_id = 0; *file_id < MAX_FILES; (*file_id)++)
    {
        if (!files[*file_id].open)
            break;
    }
    
    if (*file_id == MAX_FILES)
    {  // no open file slots left
        error_code = ERROR_FAT32_TOO_MANY_FILES;
        return false;
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Opening file in slot ");
    m_usb_tx_uint ((uint16_t)*file_id);
    m_usb_tx_string ("\n");
    #else
    printf ("Opening file in slot %u\n", (uint16_t)*file_id);
    #endif
    #endif
    
    
    if (sd_fat32_search_dir (name, false, &entry))
    {  // the file exists
        if (action == CREATE_FILE)
        {  // delete the existing file
            #ifdef FAT32_DEBUG
            #ifndef M4
            m_usb_tx_string ("File exists, deleting");
            m_usb_tx_char ('\n');
            #else
            printf ("File exists, deleting\n");
            #endif
            #endif
            
            sd_fat32_delete (name);
        }
    }
    else
    {  // if the search failed
        if (error_code != ERROR_NONE && error_code != ERROR_FAT32_END_OF_DIR)
            return false;  // search failed due to a low-level error
        
        // otherwise, the search returned false because it didn't find anything
        if (action == READ_FILE || action == APPEND_FILE)
        {  // if we want to read or append to an existing file
            error_code = ERROR_FAT32_NOT_FOUND;
            return false;
        }
    }
    
    if (action == CREATE_FILE)
    {
        // fill in entry info
        filename_8_3_to_fs (name, entry.name);
        entry.flags = 0;
        entry.file_size = 0;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("Creating file '");
        for (uint8_t i = 0; i < 11; i++)
            m_usb_tx_char (entry.name[i]);
        m_usb_tx_char ('\'');
        m_usb_tx_char ('\n');
        #else
        printf ("Creating file '");
        for (uint8_t i = 0; i < 11; i++)
            printf ("%c", entry.name[i]);
        printf ("'\n");
        #endif
        #endif
        
        // add the entry to the current directory
        if (!sd_fat32_add_object (&entry))
            return false;
    }
    else if (entry.flags & ENTRY_IS_DIR)
    {
        error_code = ERROR_FAT32_NOT_FILE;
        return false;
    }
    
    file = &(files[*file_id]);
    file->open = true;
    file->access_type = action;
    file->directory_starting_cluster = current_dir_cluster;
    filename_8_3_to_fs (name, file->name_on_fs);
    file->first_cluster = entry.first_cluster;
    file->seek_offset = 0;
    file->current_cluster = entry.first_cluster;
    file->sector_in_cluster = 0;
    file->offset_in_sector = 0;
    file->size = entry.file_size;
    
    if (action == APPEND_FILE)
    {
        if (!sd_fat32_seek (*file_id, FILE_END_POS))
        {  // error seeking to the end of the file
            sd_fat32_close_file (*file_id);
            return false;
        }
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// close the file
// does nothing if the file is not open
bool sd_fat32_close_file (uint8_t file_id)
{
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    opened_file *file = &files[file_id];
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (file_id >= MAX_FILES)
    {
        error_code = ERROR_FAT32_BAD_FILE_ID;
        return false;
    }
    
    if (!file->open)
    {
        error_code = ERROR_NONE;
        return true;
    }
    
    bool result = true;
    uint32_t temp_dir_cluster;
    
    if (file->access_type != READ_FILE)
    {  // if we were modifying the file
        // update the size of the file in the directory entry
        dir_entry_condensed entry;
        
        for (uint8_t i = 0; i < 11; i++)
            entry.name[i] = file->name_on_fs[i];
        entry.first_cluster = file->first_cluster;
        entry.file_size = file->size;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("Updating entry: first cluster = ");
        m_usb_tx_ulong (entry.first_cluster);
        m_usb_tx_string (", file size = ");
        m_usb_tx_ulong (entry.file_size);
        m_usb_tx_string ("\n");
        #else
        printf ("Updating entry: first cluster = %lu, file size = %lu\n", entry.first_cluster, entry.file_size);
        #endif
        #endif
        
        // temporarily jump back into whatever directory the file is in
        temp_dir_cluster = current_dir_cluster;
        current_dir_cluster = file->directory_starting_cluster;
        
        // if the update fails, continue closing the file anyway
        result = sd_fat32_traverse_directory (&entry, UPDATE_ENTRY);
        
        #ifdef FAT32_DEBUG
        if (!result)
        {
            #ifndef M4
            m_usb_tx_string ("UPDATE FAILED: ");
            m_usb_tx_uint (error_code);
            m_usb_tx_char ('\n');
            #else
            printf ("UPDATE FAILED: %u\n", error_code);
            #endif
        }
        #endif
        
        // jump back to the directory we're supposed to be in
        current_dir_cluster = temp_dir_cluster;
    }
    
    // clear the file details
    for (uint8_t i = 0; i < 11; i++)
        file->name_on_fs[i] = ' ';
    
    file->open = false;
    file->access_type = READ_FILE;
    file->directory_starting_cluster = 0;
    file->first_cluster = 0;
    file->seek_offset = 0;
    file->current_cluster = 0;
    file->sector_in_cluster = 0;
    file->offset_in_sector = 0;
    file->size = 0;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return result;
}


// seek to an offset in the opened file
bool sd_fat32_seek (uint8_t file_id,
                    uint32_t offset)
{
    uint32_t new_cluster;
    opened_file *file = &(files[file_id]);
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (file_id >= MAX_FILES)
    {
        error_code = ERROR_FAT32_BAD_FILE_ID;
        return false;
    }
    
    if (!file->open)
    {
        error_code = ERROR_FAT32_NOT_OPEN;
        return false;
    }
    
    if (offset == FILE_END_POS)
    {  // seek to end
        offset = file->size;
    }
    
    if (offset > file->size)
    {
        error_code = ERROR_FAT32_TOO_FAR;
        return false;
    }
    
    file->sector_in_cluster = 0;
    file->current_cluster = file->first_cluster;
    
    file->seek_offset = offset;
    
    while (offset >= 512)
    {
        offset -= 512;
        file->sector_in_cluster++;
        
        if (file->sector_in_cluster >= fat32_sectors_per_cluster)
        {
            if (!sd_fat32_cluster_lookup (file->current_cluster,
                                          &new_cluster))
            {  // this will only return false if a low-level error occurred
                return false;
            }
            
            file->current_cluster = new_cluster;
            file->sector_in_cluster = 0;
        }
    }
    
    file->offset_in_sector = offset;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}

// get the seek position in the current file
bool sd_fat32_get_seek_pos (uint8_t file_id, uint32_t *offset)
{
    error_code = ERROR_NONE;
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (file_id >= MAX_FILES)
    {
        error_code = ERROR_FAT32_BAD_FILE_ID;
        return false;
    }
    
    if (!files[file_id].open)
    {
        error_code = ERROR_FAT32_NOT_OPEN;
        return false;
    }
    
    *offset = files[file_id].seek_offset;
    return true;
}


// read data from the current position in the file
// and update the seek position
bool sd_fat32_read_file (uint8_t file_id,
                         uint32_t length,
                         uint8_t *buffer)
{
    uint16_t length_to_read;
    uint32_t new_cluster;
    
    opened_file *file = &(files[file_id]);
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (file_id >= MAX_FILES)
    {
        error_code = ERROR_FAT32_BAD_FILE_ID;
        return false;
    }
    
    if (!file->open)
    {
        error_code = ERROR_FAT32_NOT_OPEN;
        return false;
    }
    
    if (file->seek_offset + length > file->size)
    {
        error_code = ERROR_FAT32_TOO_FAR;
        return false;
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Reading ");
    m_usb_tx_ulong (length);
    m_usb_tx_string (" bytes from file id ");
    m_usb_tx_uint ((uint16_t)file_id);
    m_usb_tx_string ("\n");
    #else
    printf ("Reading %lu bytes from file id %u\n", length, (uint16_t)file_id);
    #endif
    #endif
    
    // read to the end of the sector while the read length would put us past the sector
    while ((uint32_t)file->offset_in_sector + length >= 512)
    {
        length_to_read = 512 - file->offset_in_sector;
        
        if (end_of_chain (file->current_cluster))
        {  // if the current cluster isn't actually allocated to this file
            error_code = ERROR_FAT32_TOO_FAR;
            return false;
        }
        
        if (!read_partial_block ( cluster_to_sector (file->current_cluster) +
                                      file->sector_in_cluster,
                                  file->offset_in_sector,
                                  buffer,
                                  length_to_read))
        {  // this will only return false if a low-level error occurred
            return false;
        }
        
        buffer += length_to_read;
        length -= length_to_read;
        
        file->offset_in_sector = 0;
        file->sector_in_cluster++;
        
        file->seek_offset += length_to_read;
        
        if (file->sector_in_cluster >= fat32_sectors_per_cluster)
        {
            if (!sd_fat32_cluster_lookup (file->current_cluster,
                                          &new_cluster))
            {  // this will only return false if a low-level error occurred
                return false;
            }
            
            file->current_cluster = new_cluster;
            file->sector_in_cluster = 0;
        }
    }
    
    // read any remaining bytes from the current sector
    if (length > 0)
    {
        if (end_of_chain (file->current_cluster))
        {  // if the current cluster isn't actually allocated to this file
            error_code = ERROR_FAT32_TOO_FAR;
            return false;
        }
        
        if (!read_partial_block ( cluster_to_sector (file->current_cluster) +
                                      file->sector_in_cluster,
                                  file->offset_in_sector,
                                  buffer,
                                  length))
        {  // this will only return false if a low-level error occurred
            return false;
        }
        
        file->offset_in_sector += length;
        file->seek_offset += length;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// create a subdirectory in the current directory
bool sd_fat32_mkdir (const char *name)
{
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, true))
        return false;
    
    // create a new directory entry
    dir_entry_condensed new_dir;
    
    // set all of its info, except its actual location
    filename_8_3_to_fs (name, new_dir.name);
    new_dir.flags = ENTRY_IS_DIR;
    new_dir.file_size = 0;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return sd_fat32_add_object (&new_dir);
}


// remove an empty directory from the current directory
bool sd_fat32_rmdir (const char *name)
{
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (!verify_name (name, true))
        return false;
    
    // enter the directory and look at its entries
    if (!sd_fat32_push (name))
        return false;
    
    dir_entry_condensed entry;
    
    bool retval = sd_fat32_traverse_directory (&entry, READ_DIR_START);
    
    while (retval)
    {
        if (entry.name[0] == '.')
        {  // if the entry we read was "." or "..", they don't count, keep reading
            retval = sd_fat32_traverse_directory (&entry, READ_DIR_NEXT);
        }
        else
        {  // an entry exists in the directory, we can't rmdir
            sd_fat32_pop();
            error_code = ERROR_FAT32_NOT_EMPTY;
            return false;
        }
    }
    
    // traverse_directory returned false, make sure it wasn't due to low-level error
    if (error_code != ERROR_NONE)
        return false;
    
    // the target directory has been verified to be empty, pop back out
    if (!sd_fat32_pop())
        return false;
    
    filename_8_3_to_fs (name, entry.name);
    
    if (!sd_fat32_traverse_directory (&entry, REMOVE_ENTRY))
        return false;
    
    error_code = ERROR_NONE;
    return true;
}


// add a cluster to a file
bool extend_file_clusters (opened_file *file)
{
    uint32_t new_cluster;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("EXTEND FILE: First cluster is ");
    m_usb_tx_ulong (file->first_cluster);
    m_usb_tx_string (", current is ");
    m_usb_tx_ulong (file->current_cluster);
    m_usb_tx_string ("\n");
    #else
    printf ("EXTEND FILE: First cluster is %lu, current is %lu\n", file->first_cluster, file->current_cluster);
    #endif
    #endif
    
    if (end_of_chain (file->first_cluster))
    {  // if the file doesn't have any clusters allocated to it yet
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("Search starts at ");
        m_usb_tx_ulong (file->directory_starting_cluster);
        m_usb_tx_char ('\n');
        #else
        printf ("Search starts at %lu\n", file->directory_starting_cluster);
        #endif
        #endif
        
        // find an empty cluster
        if (!sd_fat32_next_empty_cluster (file->directory_starting_cluster, &new_cluster))
            return false;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("  Next empty cluster: ");
        m_usb_tx_ulong (new_cluster);
        m_usb_tx_char ('\n');
        #else
        printf ("  Next empty cluster: %lu\n", new_cluster);
        #endif
        #endif
        
        // mark the cluster as the final cluster in its chain
        if (!sd_fat32_set_cluster (new_cluster, FAT32_END_OF_CHAIN))
            return false;
        
        // set it as the file's first cluster
        file->first_cluster = new_cluster;
        file->current_cluster = new_cluster;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("first cluster is now ");
        m_usb_tx_ulong (file->first_cluster);
        m_usb_tx_string ("\n");
        #else
        printf ("first cluster is now %lu\n", file->first_cluster);
        #endif
        #endif
    }
    else
    {  // if the file has a first cluster, and just needs to be extended
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("Adding cluster to chain... ");
        #else
        printf ("Adding cluster to chain... ");
        #endif
        #endif
        
        if (!sd_fat32_append_cluster (file->first_cluster, &file->current_cluster))
            return false;
        
        #ifdef FAT32_DEBUG
        #ifndef M4
        m_usb_tx_string ("added cluster ");
        m_usb_tx_ulong (file->current_cluster);
        m_usb_tx_string ("\n");
        #else
        printf ("added cluster %lu\n", file->current_cluster);
        #endif
        #endif
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return true;
}


bool sd_fat32_write_file (uint8_t file_id,
                          uint32_t length,
                          uint8_t *buffer)
{
    uint16_t length_to_write;
    uint32_t new_cluster;
    
    opened_file *file = &(files[file_id]);
    
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    if (file_id >= MAX_FILES)
    {
        error_code = ERROR_FAT32_BAD_FILE_ID;
        return false;
    }
    
    if (!file->open)
    {
        error_code = ERROR_FAT32_NOT_OPEN;
        return false;
    }
    
    if (file->access_type == READ_FILE)
    {
        error_code = ERROR_FAT32_FILE_READ_ONLY;
        return false;
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Writing ");
    m_usb_tx_ulong (length);
    m_usb_tx_string (" bytes to file id ");
    m_usb_tx_uint ((uint16_t)file_id);
    m_usb_tx_string ("\n");
    #else
    printf ("Writing %lu bytes to file id %u\n", length, (uint16_t)file_id);
    #endif
    #endif
    
    // read to the end of the sector while the read length would put us past the sector
    while ((uint32_t)file->offset_in_sector + length >= 512)
    {
        length_to_write = 512 - file->offset_in_sector;
        
        if (end_of_chain (file->current_cluster))
        {  // if the current cluster isn't actually allocated to this file
            if (!extend_file_clusters (file))
                return false;
        }
        
        if (!write_partial_block ( cluster_to_sector (file->current_cluster) +
                                       file->sector_in_cluster,
                                   file->offset_in_sector,
                                   buffer,
                                   length_to_write))
        {  // this will only return false if a low-level error occurred
            return false;
        }
        
        buffer += length_to_write;
        length -= length_to_write;
        
        file->offset_in_sector = 0;
        file->sector_in_cluster++;
        
        file->seek_offset += length_to_write;
        if (file->seek_offset > file->size)
            file->size = file->seek_offset;
        
        if (file->sector_in_cluster >= fat32_sectors_per_cluster)
        {
            if (!sd_fat32_cluster_lookup (file->current_cluster,
                                          &new_cluster))
            {  // this will only return false if a low-level error occurred
                return false;
            }
            
            if (end_of_chain (new_cluster))
            {  // hit the end of the allocated clusters, need to add another
                if (!extend_file_clusters (file))
                    return false;
            }
            else
            {  // the file already has a cluster after this one
                file->current_cluster = new_cluster;
            }
            
            file->sector_in_cluster = 0;
        }
    }
    
    // write any remaining bytes to the current sector
    if (length > 0)
    {
        if (end_of_chain (file->current_cluster))
        {  // if the current cluster isn't actually allocated to this file
            if (!extend_file_clusters (file))
                return false;
        }
        
        if (!write_partial_block ( cluster_to_sector (file->current_cluster) +
                                       file->sector_in_cluster,
                                   file->offset_in_sector,
                                   buffer,
                                   length))
        {  // this will only return false if a low-level error occurred
            return false;
        }
        
        file->offset_in_sector += length;
        file->seek_offset += length;
        
        if (file->seek_offset > file->size)
            file->size = file->seek_offset;
    }
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    error_code = ERROR_NONE;
    return true;
}


// delete a file from the current directory
// if you delete an open file, the file will be closed first
bool sd_fat32_delete (const char *name)
{
    if (!fat32_initialized)
    {
        error_code = ERROR_FAT32_INIT;
        return false;
    }
    
    // construct a minimal entry for what we're trying to remove
    dir_entry_condensed to_remove;
    
    if (!verify_name (name, false))
        return false;
    
    // minimal entry consists of just a name at first
    filename_8_3_to_fs (name, to_remove.name);
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Removing entry\n");
    #else
    printf ("Removing entry\n");
    #endif
    #endif
    
    for (uint8_t i = 0; i < MAX_FILES; i++)
    {
        if (files[i].open && files[i].directory_starting_cluster == current_dir_cluster)
        {  // there's a file open and in the current directory
            // make sure the open file isn't the one we're trying to delete
            if (fs_filenames_match (to_remove.name, files[i].name_on_fs))
            {  // if it is, close it first
                #ifdef FAT32_DEBUG
                #ifndef M4
                m_usb_tx_string ("File is open, closing\n");
                #else
                printf ("File is open, closing\n");
                #endif
                #endif
                
                if (!sd_fat32_close_file (i))
                    return false;
            }
        }
    }
    
    if (!sd_fat32_traverse_directory (&to_remove, REMOVE_ENTRY))
    {
        if (error_code == ERROR_FAT32_END_OF_DIR)
            error_code = ERROR_FAT32_NOT_FOUND;
        return false;
    }
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Entry removed, clearing clusters starting with ");
    m_usb_tx_ulong (to_remove.first_cluster);
    m_usb_tx_string ("\n");
    #else
    printf ("Entry removed, clearing clusters starting with %lu\n", to_remove.first_cluster);
    #endif
    #endif
    
    // if the entry was successfully removed from the current directory, it
    // will now contain a valid first_cluster number
    if (!sd_fat32_free_clusters (&to_remove))
        return false;
    
    #ifdef FAT32_DEBUG
    #ifndef M4
    m_usb_tx_string ("Clusters cleared\n");
    #else
    printf ("Clusters cleared\n");
    #endif
    #endif
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    return true;
}



