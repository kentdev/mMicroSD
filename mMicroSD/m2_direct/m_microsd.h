/*******************************************************************************
* m_microsd.h
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Just a set of defines, to use the same API for direct access to
*              the SD card as access over I2C.
*******************************************************************************/

#ifndef M_MICROSD_H
#define M_MICROSD_H

#include "m_general.h"
#include "m_bus.h"

#include "m_usb.h"

#include "sd_fat32.h"

#define m_sd_init                sd_fat32_init
#define m_sd_shutdown            sd_fat32_shutdown

#define m_sd_get_size            sd_fat32_get_size
#define m_sd_object_exists       sd_fat32_object_exists
#define m_sd_get_dir_entry_first sd_fat32_get_dir_entry_first
#define m_sd_get_dir_entry_next  sd_fat32_get_dir_entry_next

#define m_sd_push                sd_fat32_push
#define m_sd_pop                 sd_fat32_pop
#define m_sd_mkdir               sd_fat32_mkdir
#define m_sd_rmdir               sd_fat32_rmdir
#define m_sd_delete              sd_fat32_delete

#define m_sd_open_file           sd_fat32_open_file
#define m_sd_close_file          sd_fat32_close_file
#define m_sd_seek                sd_fat32_seek
#define m_sd_get_seek_pos        sd_fat32_get_seek_pos
#define m_sd_read_file           sd_fat32_read_file
#define m_sd_write_file          sd_fat32_write_file

#endif

