#include "sd_fat32.h"
#include "m_general.h"
#include "m_usb.h"

void error (void)
{
    for (;;)
    {
        m_red (TOGGLE);
        m_wait (125);
    }
}

void error_print (uint8_t code)
{
    m_usb_tx_string ("ERROR ");
    m_usb_tx_uint ((uint16_t) code);
    m_usb_tx_string ("\n");
}



#define COPY_BUFFER_LEN 80
uint8_t copy_buffer[COPY_BUFFER_LEN];


#ifdef FREE_RAM
void free_ram (void);
#endif

uint8_t i = 0;

int main (void)
{
    m_clockdivide (1);  // set clock to 8MHz
    
    m_usb_init();
    
    m_red (ON);
    m_wait (1500);
    
    if (!sd_fat32_init())
    {
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("Beginning test\n");
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    if (!sd_fat32_push ("testdir"))
    {
        m_usb_tx_string ("Couldn't enter testdir, ");
        error_print (error_code);
        error();
    }
    
    uint32_t file_size;
    if (!sd_fat32_get_size ("python.txt", &file_size))
    {
        m_usb_tx_string ("Couldn't get size of python.txt, ");
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("Size of python.txt: ");
    m_usb_tx_ulong (file_size);
    m_usb_tx_string ("\n");
    
    uint32_t bytes_copied = 0;
    
    while (bytes_copied < file_size)
    {
        uint32_t bytes_to_copy = file_size - bytes_copied;
        if (bytes_to_copy > COPY_BUFFER_LEN)
            bytes_to_copy = COPY_BUFFER_LEN;
        
        if (!sd_fat32_open_file ("python.txt", READ_FILE))
        {
            m_usb_tx_string ("Couldn't open python.txt for offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        m_usb_tx_string ("\nOffset ");
        m_usb_tx_ulong (bytes_copied);
        m_usb_tx_char ('\n');
        if (!sd_fat32_seek (bytes_copied))
        {
            m_usb_tx_string ("Couldn't seek python.txt to offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_read_file (bytes_to_copy, copy_buffer))
        {
            m_usb_tx_string ("Couldn't read python.txt, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        for (uint32_t z = 0; z < bytes_to_copy; z++)
            m_usb_tx_char (copy_buffer[z]);
        
        /*if (!sd_fat32_close_file())
        {
            m_usb_tx_string ("Couldn't close python.txt, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }*/
        
        if (!sd_fat32_push ("created"))
        {
            m_usb_tx_string ("Couldn't enter created, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        const open_option open_type = (bytes_copied == 0) ? CREATE_FILE : APPEND_FILE;
        if (!sd_fat32_open_file ("py_copy.txt", open_type))
        {
            m_usb_tx_string ("Couldn't open py_copy.txt for offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_write_file (bytes_to_copy, copy_buffer))
        {
            m_usb_tx_string ("Couldn't write py_copy.txt, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        /*if (!sd_fat32_close_file())
        {
            m_usb_tx_string ("Couldn't close py_copy.txt, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }*/
        
        if (!sd_fat32_open_file ("py_copy2.txt", open_type))
        {
            m_usb_tx_string ("Couldn't open py_copy2.txt for offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_write_file (bytes_to_copy, copy_buffer))
        {
            m_usb_tx_string ("Couldn't write py_copy2.txt, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_pop ())
        {
            m_usb_tx_string ("Couldn't pop from created, offset ");
            m_usb_tx_ulong (bytes_copied);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        bytes_copied += bytes_to_copy;
    }
    
    m_usb_tx_string ("\nCopy completed\n");
    
    /*
    if (!sd_fat32_push ("created"))
    {
        m_usb_tx_string ("Couldn't enter created, ");
        error_print (error_code);
        error();
    }
    */
    
    /*for (uint8_t i = 0; i < 20; i++)
    {
        char name[11] = "folder_a";
        name[7] = 'a' + i;
        
        if (!sd_fat32_mkdir (name))
        {
            m_usb_tx_string ("Couldn't create ");
            for (uint8_t n = 0; n < 8; n++)
                m_usb_tx_char (name[n]);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
    }*/
    
    
    
    /*if (!sd_fat32_delete ("newdir"))
    {
        m_usb_tx_string ("Couldn't delete newdir, ");
        error_print (error_code);
        error();
    }*/
    
    /*
    if (!sd_fat32_open_file ("testfile", CREATE_FILE))
    {
        m_usb_tx_string ("Couldn't open testfile, time 1, ");
        error_print (error_code);
        error();
    }
    
    if (!sd_fat32_write (testdata_len, (uint8_t*)testdata))
    {
        m_usb_tx_string ("Couldn't write to testfile, time 1, ");
        error_print (error_code);
        error();
    }
    
    if (!sd_fat32_close_file ())
    {
        m_usb_tx_string ("Couldn't close testfile the first time, ");
        error_print (error_code);
        error();
    }
    
    if (!sd_fat32_open_file ("testfile", APPEND_FILE))
    {
        m_usb_tx_string ("Couldn't open testfile, ");
        error_print (error_code);
        error();
    }
    
    for (i = 0; i < 120; i++)
    {
        m_usb_tx_string ("testfile, loop ");
        m_usb_tx_uint (i);
        m_usb_tx_string ("\n");
        
        if (!sd_fat32_write (5, (uint8_t*)"Loop "))
        {
            m_usb_tx_string ("Couldn't write to testfile, loop header ");
            m_usb_tx_uint (i);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        char var[5];
        itoa ((int)i, var, 10);
        int len = 0;
        while (len < 5)
        {
            if (!var[len])
                break;
            len++;
        }
        
        if (!sd_fat32_write (len, (uint8_t*)var))
        {
            m_usb_tx_string ("Couldn't write to testfile, loop header num ");
            m_usb_tx_uint (i);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_write (1, (uint8_t*)"\n"))
        {
            m_usb_tx_string ("Couldn't write to testfile, loop header end ");
            m_usb_tx_uint (i);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
        
        if (!sd_fat32_write (testdata_len, (uint8_t*)testdata))
        {
            m_usb_tx_string ("Couldn't write to testfile, loop ");
            m_usb_tx_uint (i);
            m_usb_tx_string (", ");
            error_print (error_code);
            error();
        }
    }
    
    if (!sd_fat32_write (5, (uint8_t*)" DONE"))
    {
        m_usb_tx_string ("Couldn't write 'done', ");
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("Done writing, closing\n");
    
    if (!sd_fat32_close_file ())
    {
        m_usb_tx_string ("Couldn't close testfile, ");
        error_print (error_code);
    }
    
    
    if (!sd_fat32_pop())
    {
        m_usb_tx_string ("Couldn't pop from testdir, ");
        error_print (error_code);
    }
    
    
    m_usb_tx_string ("All actions completed\n");
    */
    
    /*
    dir_entry_condensed entry;
    bool continue_reading = sd_fat32_traverse_directory (&entry, READ_DIR_START);
    
    m_usb_tx_string ("Root entries:\n");
    
    while (continue_reading)
    {
        if (!(entry.flags & (ENTRY_IS_HIDDEN | ENTRY_IS_EMPTY)))
        {
            m_usb_tx_string ("\t'");
            for (uint8_t i = 0; i < 11; i++)
            {
                if (entry.name[i] == 0)
                    break;
                
                m_usb_tx_char (entry.name[i]);
            }
            m_usb_tx_string ("'");
            if (entry.flags & ENTRY_IS_DIR)
                m_usb_tx_string (" (DIRECTORY)");
            m_usb_tx_string ("\n");
        }
        
        continue_reading = sd_fat32_traverse_directory (&entry, READ_DIR_NEXT);
    }
    
    if (!sd_fat32_push ("testdir"))
    {
        m_usb_tx_string ("Couldn't enter testdir, ");
        error_print (error_code);
    }
    else
    {
        if (sd_fat32_search_dir ("python.txt", &entry))
        {
            m_usb_tx_string ("Found python.txt at cluster ");
            m_usb_tx_ulong (entry.first_cluster);
            m_usb_tx_string (", size = ");
            m_usb_tx_ulong (entry.file_size);
            m_usb_tx_string ("\n");
        }
        else
        {
            m_usb_tx_string ("Unable to find python.txt\n");
        }
    }
    
    if (!sd_fat32_pop())
    {
        m_usb_tx_string ("Couldn't pop from testdir, ");
        error_print (error_code);
    }
    else
    {
        if (sd_fat32_search_dir ("test6.txt", &entry))
        {
            m_usb_tx_string ("Found test6.txt at cluster ");
            m_usb_tx_ulong (entry.first_cluster);
            m_usb_tx_string (", size = ");
            m_usb_tx_ulong (entry.file_size);
            m_usb_tx_string ("\n");
        }
        else
        {
            m_usb_tx_string ("Unable to find test6.txt\n");
        }
    }
    
    
    if (!sd_fat32_push ("testdir"))
    {
        m_usb_tx_string ("Couldn't enter testdir, ");
        error_print (error_code);
        error();
    }
    
    if (!sd_fat32_open_file ("python.txt"))
    {
        m_usb_tx_string ("Couldn't open python.txt, ");
        error_print (error_code);
        error();
    }
    
    uint32_t file_size;
    
    if (!sd_fat32_get_size ("python.txt", &file_size))
    {
        m_usb_tx_string ("Couldn't get size of python.txt, ");
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("File size: ");
    m_usb_tx_ulong (file_size);
    m_usb_tx_string ("\n");
    
    uint32_t offset = 16;
    
    if (!sd_fat32_seek (offset))
    {
        m_usb_tx_string ("Couldn't seek to offset, ");
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("Contents:\n'");
    
    while (offset < file_size)
    {
        uint32_t to_read = file_size - offset;
        if (to_read > 1000)
            to_read = 1000;
        
        uint8_t buffer[1000];
        if (!sd_fat32_read (to_read, buffer))
        {
            m_usb_tx_string ("Error reading python.txt, ");
            error_print (error_code);
            error();
        }
        
        for (uint16_t i = 0; i < to_read; i++)
            m_usb_tx_char (buffer[i]);
        
        offset += to_read;
    }
    
    m_usb_tx_string ("'\n(EOF)\n");
    */
    
    /*
    if (!sd_fat32_mkdir ("created"))
    {
        m_usb_tx_string ("Error creating created, ");
        error_print (error_code);
        error();
    }
    */
    
    m_red (OFF);
    
    if (!sd_fat32_shutdown())
    {
        m_usb_tx_string ("Error during shutdown, ");
        error_print (error_code);
        error();
    }
    
    #ifdef LOWLEVEL_DEBUG
    m_usb_tx_string ("\n\nTotal block accesses: ");
    m_usb_tx_ulong (total_block_accesses);
    m_usb_tx_string ("\n");
    #endif
    
    for (;;)
    {
        m_green (TOGGLE);
        m_wait (125);
    }
    
    return 0;
}

