/*******************************************************************************
* m_microsd.c
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Contains the functions needed to communicate with the mMicroSD
*              peripheral over I2C from the M4.
*******************************************************************************/

#include "m_microsd.h"
#include "mBus.h"

extern CPAL_InitTypeDef      mBusStruct;
extern CPAL_TransferTypeDef  mBusTx;
extern CPAL_TransferTypeDef  mBusRx;

#define I2C_ADDR_WRITE ((0x5D) << 1)
#define I2C_ADDR_READ  (((0x5D) << 1) | 1)

#define TWI_BUFFER_LEN 257

#define MAX_RESPONSE_RETRIES        1000
#define MS_BETWEEN_RESPONSE_RETRIES 1

#define nop()  __asm__ __volatile__("nop")

#ifndef false
#define false ((bool)0)
#endif

#ifndef true
#define true ((bool)1)
#endif

//unsigned char twi_start(unsigned char address, unsigned char readwrite);
//unsigned char twi_send_byte(unsigned char byte);
//void twi_stop(void);


// convert file names from their representation on disk
// eg., "TEST    TXT" becomes "TEST.TXT"
void filename_fs_to_8_3 (const char *input_name,
                         char *output_name)
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    bool wrote_dot = false;
    
    while (in_index < 11 && out_index < 11)
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

void filename_8_3_to_fs (const char *input_name,
                         char *output_name)
{
    uint8_t in_index = 0;
    uint8_t out_index = 0;
    
    char in_char;
    
    while (out_index < 11)
    {
        if (input_name[in_index] == '\0')
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


typedef enum m_microsd_command_type
{
    M_SD_INIT = 0,
    M_SD_SHUTDOWN,
    M_SD_GET_SIZE,
    M_SD_OBJECT_EXISTS,
    M_SD_GET_FIRST_ENTRY,
    M_SD_GET_NEXT_ENTRY,
    M_SD_PUSH,
    M_SD_POP,
    M_SD_MKDIR,
    M_SD_RMDIR,
    M_SD_DELETE,
    M_SD_OPEN_FILE,
    M_SD_CLOSE_FILE,
    M_SD_SEEK,
    M_SD_GET_SEEK,
    M_SD_READ_FILE,
    M_SD_WRITE_FILE,
    
    M_SD_NONE = 255
} m_microsd_command_type;

typedef struct i2c_command
{
    uint8_t command;
    uint8_t data_length;
    uint8_t data[TWI_BUFFER_LEN];
} i2c_command;

typedef struct i2c_response
{
    uint8_t response_code;
    uint8_t data_length;
    uint8_t data[TWI_BUFFER_LEN];
} i2c_response;

union m_sd_transmission
{
    i2c_command  order;
    i2c_response response;
} transmission;

m_sd_errors m_sd_error_code;


static bool i2c_write (void)
{
    if (CPAL_I2C_Write (&mBusStruct) == CPAL_PASS)
    {
        // wait for the data to be shifted out
        while (mBusStruct.CPAL_State != CPAL_STATE_READY &&
               mBusStruct.CPAL_State != CPAL_STATE_ERROR)
        {}
        
        if (mBusStruct.CPAL_State == CPAL_STATE_ERROR)
        {  // if there was a transfer error
            mBusStruct.CPAL_State = CPAL_STATE_READY;
            mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
            
            m_sd_error_code = ERROR_I2C_COMMAND;
            return false;
        }
    }
    else
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    return true;
}

static bool i2c_read (void)
{
    if (CPAL_I2C_Read (&mBusStruct) == CPAL_PASS)
    {
        // wait for the data to be shifted out
        while (mBusStruct.CPAL_State != CPAL_STATE_READY &&
               mBusStruct.CPAL_State != CPAL_STATE_ERROR)
        {}
        
        if (mBusStruct.CPAL_State == CPAL_STATE_ERROR ||
            mBusStruct.wCPAL_DevError != CPAL_I2C_ERR_NONE)
        {  // if there was a transfer error
            mBusStruct.CPAL_State = CPAL_STATE_READY;
            
            m_sd_error_code = ERROR_I2C_COMMAND;
            return false;
        }
    }
    else
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    if (mBusGetLastError() != CPAL_I2C_ERR_NONE)
        return false;
    
    return true;
}

static bool send_order (void)
{
    if (mBusStruct.CPAL_State != CPAL_STATE_READY)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    // send command type, data length, and data all in one go
    mBusStruct.wCPAL_Options = CPAL_OPT_NO_MEM_ADDR;
    mBusStruct.pCPAL_TransferTx = &mBusTx; 
    mBusStruct.pCPAL_TransferTx->wNumData = 2 + transmission.order.data_length;
    mBusStruct.pCPAL_TransferTx->pbBuffer = (uint8_t*)&transmission.order;
    mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)I2C_ADDR_WRITE;
    
    if (!i2c_write())
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    m_sd_error_code = ERROR_NONE;
    return true;
}

static bool receive_response (void)
{
    uint16_t retries = 0;
    
retry:
    if (retries > MAX_RESPONSE_RETRIES)
    {
        m_sd_error_code = ERROR_I2C_RESPONSE_TIMEOUT;
        return false;
    }
    
    if (mBusStruct.CPAL_State != CPAL_STATE_READY ||
        mBusStruct.wCPAL_DevError != CPAL_I2C_ERR_NONE ||
        mBusGetLastError() != CPAL_I2C_ERR_NONE)
    {
        retries++;
        mWaitms (MS_BETWEEN_RESPONSE_RETRIES);
        goto retry;
    }
    
    // receive command code and data length
    mBusStruct.wCPAL_Options = CPAL_OPT_NO_MEM_ADDR;
    mBusStruct.pCPAL_TransferRx = &mBusRx; 
    mBusStruct.pCPAL_TransferRx->wNumData = 2;
    mBusStruct.pCPAL_TransferRx->pbBuffer = (uint8_t*)&transmission.response;
    mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)I2C_ADDR_READ;
    
    if (!i2c_read())
    {
        retries++;
        mWaitms (MS_BETWEEN_RESPONSE_RETRIES);
        goto retry;
    }
    
    // get data
    if (transmission.response.data_length > 0)
    {
        mBusStruct.wCPAL_Options = CPAL_OPT_NO_MEM_ADDR;
        mBusStruct.pCPAL_TransferRx = &mBusRx; 
        mBusStruct.pCPAL_TransferRx->wNumData = 2 + transmission.response.data_length;
        mBusStruct.pCPAL_TransferRx->pbBuffer = (uint8_t*)&transmission.response;
        mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)I2C_ADDR_READ;
        
        if (!i2c_read())
        {
            m_sd_error_code = ERROR_I2C_COMMAND;
            return false;
        }
    }
    
    m_sd_error_code = ERROR_NONE;
    return true;
}





//------------------------------------------------------------------------------



//-----------------------------------------------
// Startup and shutdown:

// mount the microSD card's FAT32 filesystem
bool m_sd_init (void)
{
    mBusInit();
    
    transmission.order.command = M_SD_INIT;
    transmission.order.data_length = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}

// flush any pending writes and unmount the filesystem
bool m_sd_shutdown (void)
{
    transmission.order.command = M_SD_SHUTDOWN;
    transmission.order.data_length = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


//-----------------------------------------------
// File and directory information:


// get the size of a file in the current directory
bool m_sd_get_size (const char *name,
                    uint32_t *size)
{
    uint16_t len = strlen (name);
    
    if (len > 12)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_GET_SIZE;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    if (m_sd_error_code != ERROR_NONE)
    {
        return false;
    }
    
    if (transmission.response.data_length != 4)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    const uint32_t *size_ptr = ((uint32_t*)transmission.response.data);
    *size = *size_ptr;
    return true;
}


// search the current directory
// if the target is found, *is_directory will be set accordingly
bool m_sd_object_exists (const char *name,
                         bool *is_directory)
{
    uint16_t len = strlen (name);
    
    if (len > 12)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_OBJECT_EXISTS;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    if (m_sd_error_code != ERROR_NONE)
    {
        return false;
    }
    
    if (transmission.response.data_length != 2)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    if (transmission.response.data[0])
    {
        *is_directory = (bool)transmission.response.data[1];
        return true;
    }
    return false;
}


// iterate through the current directory, reading the names of its objects
// returns false when the end of the directory has been reached
bool m_sd_get_dir_entry_first (char name[12])
{
    transmission.order.command = M_SD_GET_FIRST_ENTRY;
    transmission.order.data_length = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    if (m_sd_error_code != ERROR_NONE)
    {
        return false;
    }
    
    if (transmission.response.data_length == 0)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    const bool retval = (bool)transmission.response.data[0];
    
    if (retval)
    {
        filename_fs_to_8_3 ((char*)&(transmission.response.data[1]),
                            name);
    }
    
    return retval;
}


bool m_sd_get_dir_entry_next  (char name[12])
{
    transmission.order.command = M_SD_GET_NEXT_ENTRY;
    transmission.order.data_length = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    if (m_sd_error_code != ERROR_NONE)
    {
        return false;
    }
    
    if (transmission.response.data_length == 0)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    const bool retval = (bool)transmission.response.data[0];
    
    if (retval)
    {
        filename_fs_to_8_3 ((char*)&(transmission.response.data[1]),
                            name);
    }
    
    return retval;
}


//-----------------------------------------------
// Directory traversal and modification:

// enter the directory with the given name
bool m_sd_push (const char *name)
{
    uint16_t len = strlen (name);
    
    if (len > 8)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_PUSH;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}

// go up one level in the directory path
bool m_sd_pop (void)
{
    transmission.order.command = M_SD_POP;
    transmission.order.data_length = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}

// create a subdirectory in the current directory
bool m_sd_mkdir (const char *name)
{
    uint16_t len = strlen (name);
    
    if (len > 8)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_MKDIR;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


// remove an empty directory from the current directory
bool m_sd_rmdir (const char *name)
{
    uint16_t len = strlen (name);
    
    if (len > 8)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_RMDIR;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


// delete a file from the current directory
// if you delete an open file, the file will be closed first
bool m_sd_delete (const char *name)
{
    uint16_t len = strlen (name);
    
    if (len > 12)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_DELETE;
    transmission.order.data_length = (uint8_t)len + 1;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i] = (uint8_t)name[i];
    transmission.order.data[i] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


//-----------------------------------------------
// File access and modification:

// open a file in the current directory
// actions are:
//   READ_FILE:   open an existing file as read-only
//   APPEND_FILE: open an existing file as read-write and seek to the end
//   CREATE_FILE: create a file, deleting any previously existing file
bool m_sd_open_file (const char *name,
                     open_option action,
                     uint8_t *file_id)
{
    uint16_t len = strlen (name);
    
    if (len > 12)
    {
        m_sd_error_code = ERROR_FAT32_INVALID_NAME;
        return false;
    }
    
    transmission.order.command = M_SD_OPEN_FILE;
    transmission.order.data_length = 1 + (uint8_t)len + 1;
    
    transmission.order.data[0] = (uint8_t)action;
    
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        transmission.order.data[i + 1] = (uint8_t)name[i];
    transmission.order.data[i + 1] = 0;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    if (m_sd_error_code != ERROR_NONE)
        return false;
    
    if (transmission.response.data_length != 1)
        return false;
    
    *file_id = transmission.response.data[0];
    return true;
}

// close the opened file
// does nothing if no file is open
bool m_sd_close_file (uint8_t file_id)
{
    transmission.order.command = M_SD_CLOSE_FILE;
    transmission.order.data_length = 1;
    transmission.order.data[0] = file_id;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


// seek to a location within the opened file
// an offset of FILE_END_POS will seek to the end of the file
bool m_sd_seek (uint8_t file_id,
                uint32_t offset)
{
    transmission.order.command = M_SD_SEEK;
    transmission.order.data_length = 5;
    
    transmission.order.data[0] = file_id;
    uint32_t *offset_ptr = ((uint32_t*)&transmission.order.data[1]);
    *offset_ptr = offset;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}

// get the seek position in the opened file
bool m_sd_get_seek_pos (uint8_t file_id,
                        uint32_t *offset)
{
    transmission.order.command = M_SD_GET_SEEK;
    transmission.order.data_length = 1;
    transmission.order.data[0] = file_id;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    
    if (m_sd_error_code != ERROR_NONE)
        return false;
    
    if (transmission.response.data_length != 4)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    const uint32_t *offset_ptr = ((uint32_t*)transmission.response.data);
    *offset = *offset_ptr;
    
    return true;
}

// read from the current location in the file
// updates the seek position
//
// if the length of the read would go beyond the end of the file, an
// error is returned and nothing is read
bool m_sd_read_file (uint8_t file_id,
                     uint32_t length,
                     uint8_t *buffer)
{
    transmission.order.command = M_SD_READ_FILE;
    transmission.order.data_length = 2;
    
    if (length > TWI_BUFFER_LEN - 1)
    {
        m_sd_error_code = ERROR_I2C_MESSAGE_TOO_LONG;
        return false;
    }
    
    transmission.order.data[0] = file_id;
    transmission.order.data[1] = length;
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    
    if (m_sd_error_code != ERROR_NONE)
        return false;
    
    if (transmission.response.data_length != length)
    {
        m_sd_error_code = ERROR_I2C_COMMAND;
        return false;
    }
    
    for (uint8_t i = 0; i < length; i++)
        buffer[i] = transmission.response.data[i];
    
    return true;
}


// write to the current location in the file
// updates the seek position
bool m_sd_write_file (uint8_t file_id,
                      uint32_t length,
                      uint8_t *buffer)
{
    transmission.order.command = M_SD_WRITE_FILE;
    
    if (length > TWI_BUFFER_LEN - 1)
    {
        m_sd_error_code = ERROR_I2C_MESSAGE_TOO_LONG;
        return false;
    }
    
    if (length == 0)
        return true;
    
    transmission.order.data_length = length + 1;
    transmission.order.data[0] = file_id;
    
    for (uint8_t i = 0; i < length; i++)
        transmission.response.data[i + 1] = (uint8_t)buffer[i];
    
    if (!send_order())
        return false;
    
    if (!receive_response())
        return false;
    
    m_sd_error_code = transmission.response.response_code;
    return (m_sd_error_code == ERROR_NONE);
}


