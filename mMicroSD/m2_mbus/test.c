#include "m_general.h"
#include "m_bus.h"
#include "m_usb.h"
#include "m_microsd.h"

void filename_8_3_to_fs (const char *input_name,
                         char *output_name);

void error (void)
{
    for (;;)
    {
        m_red (TOGGLE);
        m_wait (250);
    }
}

static void print_name (const char *name)
{
    for (uint8_t i = 0; i < 255 && name[i] != '\0'; i++)
        m_usb_tx_char (name[i]);
}

static void search (const char *name)
{
    bool is_dir;
    if (m_sd_object_exists (name, &is_dir))
    {
        print_name (name);
        
        m_usb_tx_string (" found: ");
        if (is_dir)
            m_usb_tx_string ("directory\n");
        else
            m_usb_tx_string ("file\n");
    }
    else
    {
        if (m_sd_error_code == ERROR_NONE)
        {
            print_name (name);
            m_usb_tx_string (" not found\n");
        }
        else
        {
            m_usb_tx_string ("Error when searching for ");
            print_name (name);
            m_usb_tx_string (": ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
    }
}

void main (void)
{
    m_clockdivide (0);
    
    m_usb_init();
    m_red (ON);
    m_wait (1000);
    
    if (!m_sd_init())
    {
        m_usb_tx_string ("Error initializing mMicroSD: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
        error();
    }
    m_usb_tx_string ("Init OK\n");
    
    /*
    // test of m_sd_get_size
    uint32_t size;
    if (!m_sd_get_size ("test60.txt", &size))
    {
        m_usb_tx_string ("Error getting size of test60.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Size of test60.txt: ");
        m_usb_tx_ulong (size);
        m_usb_tx_string ("\n");
    }
    */
    
    /*
    // test of m_sd_object_exists
    search ("test53.txt");
    search ("test?.abc");
    search ("testdir");
    search ("nonexist.ent");
    */
    
    /*
    // test of m_sd_get_dir_entry_first and m_sd_get_dir_entry_next
    char name[12];
    uint16_t num_entries = 0;
    
    if (m_sd_get_dir_entry_first (name))
    {
        num_entries++;
        print_name (name);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Error getting first directory entry: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    while (m_sd_get_dir_entry_next (name))
    {
        num_entries++;
        print_name (name);
        m_usb_tx_string ("\n");
    }
    
    if (m_sd_error_code != ERROR_NONE)
    {
        m_usb_tx_string ("Error getting directory entry: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Finished reading directory entries\n");
    }
    
    m_usb_tx_string ("Total entries: ");
    m_usb_tx_uint (num_entries);
    m_usb_tx_string ("\n");
    */
    
    /*
    // test of m_sd_push, m_sd_object_exists, and m_sd_pop
    search ("test53.txt");
    search ("python.txt");
    search ("created");
    
    m_usb_tx_string ("PUSH\n");
    
    if (!m_sd_push ("testdir"))
    {
        m_usb_tx_string ("Error pushing into testdir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    search ("test53.txt");
    search ("python.txt");
    search ("created");
    
    m_usb_tx_string ("POP\n");
    
    if (!m_sd_pop())
    {
        m_usb_tx_string ("Error popping: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    search ("test53.txt");
    search ("python.txt");
    search ("created");
    
    m_usb_tx_string ("DONE\n");
    */
    
    /*
    // test of m_sd_mkdir
    if (!m_sd_mkdir ("newdir"))
    {
        m_usb_tx_string ("Error creating newdir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    */
    
    /*
    // test of m_sd_rmdir
    if (!m_sd_rmdir ("newdir"))
    {
        m_usb_tx_string ("Error removing newdir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_rmdir ("emptydir"))
    {
        m_usb_tx_string ("Error removing emptydir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    */
    
    /*
    // test of m_sd_delete
    if (!m_sd_delete ("nonexist"))
    {
        m_usb_tx_string ("Error deleting nonexist: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_delete ("blah"))
    {
        m_usb_tx_string ("Error deleting blah: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    */
    
    /*
    // test of m_sd_open_file and m_sd_close_file
    if (!m_sd_open_file ("nonexist.ent", READ_FILE))
    {
        m_usb_tx_string ("Error opening nonexist.ent: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_close_file())
    {
        m_usb_tx_string ("Error closing file: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_open_file ("test53.txt", APPEND_FILE))
    {
        m_usb_tx_string ("Error opening test53.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_close_file())
    {
        m_usb_tx_string ("Error closing test53.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_open_file ("testblah.txt", CREATE_FILE))
    {
        m_usb_tx_string ("Error opening testblah.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_close_file())
    {
        m_usb_tx_string ("Error closing testblah.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    */
    
    /*
    // test of m_sd_push, m_sd_open_file, m_sd_seek, m_sd_get_seek_pos, m_sd_read_file, and m_sd_close_file
    if (!m_sd_push ("testdir"))
    {
        m_usb_tx_string ("Error pushing into testdir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Pushed into testdir\n");
    }
    
    uint32_t seek_pos;
    if (!m_sd_get_seek_pos (&seek_pos))
    {
        m_usb_tx_string ("Error getting seek pos: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seek pos: ");
        m_usb_tx_ulong (seek_pos);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_open_file ("python.txt", READ_FILE))
    {
        m_usb_tx_string ("Error opening python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Opened python.txt\n");
    }
    
    if (!m_sd_get_seek_pos (&seek_pos))
    {
        m_usb_tx_string ("Error getting seek pos: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seek pos: ");
        m_usb_tx_ulong (seek_pos);
        m_usb_tx_string ("\n");
    }
    
    char buffer[21];
    if (!m_sd_read_file (21, buffer))
    {
        m_usb_tx_string ("Error reading python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        for (uint8_t i = 0; i < 21; i++)
            m_usb_tx_char (buffer[i]);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_get_seek_pos (&seek_pos))
    {
        m_usb_tx_string ("Error getting seek pos: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seek pos: ");
        m_usb_tx_ulong (seek_pos);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_close_file())
    {
        m_usb_tx_string ("Error closing python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Closed python.txt\n");
    }
    
    if (!m_sd_open_file ("python.txt", READ_FILE))
    {
        m_usb_tx_string ("Error opening python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Opened python.txt\n");
    }
    
    if (!m_sd_get_seek_pos (&seek_pos))
    {
        m_usb_tx_string ("Error getting seek pos: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seek pos: ");
        m_usb_tx_ulong (seek_pos);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_seek (10))
    {
        m_usb_tx_string ("Error seeking to offset 10: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seeked to offset 10\n");
    }
    
    if (!m_sd_get_seek_pos (&seek_pos))
    {
        m_usb_tx_string ("Error getting seek pos: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Seek pos: ");
        m_usb_tx_ulong (seek_pos);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_read_file (21, buffer))
    {
        m_usb_tx_string ("Error reading python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        for (uint8_t i = 0; i < 21; i++)
            m_usb_tx_char (buffer[i]);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_close_file())
    {
        m_usb_tx_string ("Error closing python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    else
    {
        m_usb_tx_string ("Closed python.txt\n");
    }
    */
    
    /*
    // test mkdir, get_size, pushing, opening, closing, reading, writing, and seeking
    #define COPY_BUFFER_LEN 64
    char copy_buffer[COPY_BUFFER_LEN];
    
    if (!m_sd_push ("testdir"))
    {
        m_usb_tx_string ("Error pushing into testdir: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    if (!m_sd_mkdir ("blahblah"))
    {
        m_usb_tx_string ("Error creating blahblah directory: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    uint32_t file_size;
    if (!m_sd_get_size ("python.txt", &file_size))
    {
        m_usb_tx_string ("Error getting size of python.txt: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
    }
    
    uint32_t bytes_copied = 0;
    
    while (bytes_copied < file_size)
    {
        uint32_t bytes_to_copy = file_size - bytes_copied;
        if (bytes_to_copy > COPY_BUFFER_LEN)
            bytes_to_copy = COPY_BUFFER_LEN;
        
        if (!m_sd_open_file ("python.txt", READ_FILE))
        {
            m_usb_tx_string ("Error opening python.txt: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        if (!m_sd_seek (bytes_copied))
        {
            m_usb_tx_string ("Error seeking in python.txt: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        if (!m_sd_read_file (bytes_to_copy, copy_buffer))
        {
            m_usb_tx_string ("Error reading python.txt: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        if (!m_sd_push ("blahblah"))
        {
            m_usb_tx_string ("Error pushing into blahblah: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        const open_option open_type = (bytes_copied == 0) ? CREATE_FILE : APPEND_FILE;
        if (!m_sd_open_file ("somecopy.txt", open_type))
        {
            m_usb_tx_string ("Error opening somecopy.txt: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        if (!m_sd_write_file (bytes_to_copy, copy_buffer))
        {
            m_usb_tx_string ("Error writing to somecopy.txt: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        if (!m_sd_pop ())
        {
            m_usb_tx_string ("Error popping from blahblah: ");
            m_usb_tx_int ((int)m_sd_error_code);
            m_usb_tx_string ("\n");
        }
        
        bytes_copied += bytes_to_copy;
    }
    m_usb_tx_string ("\nDONE\n");
    */
    
    if (!m_sd_shutdown())
    {
        m_usb_tx_string ("Error shutting down mMicroSD: ");
        m_usb_tx_int ((int)m_sd_error_code);
        m_usb_tx_string ("\n");
        error();
    }
    m_usb_tx_string ("Shutdown OK\n");
    
    m_red (OFF);
    for (;;)
    {
    }
}
