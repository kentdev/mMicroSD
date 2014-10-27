#include "mGeneral.h"
#include "mBus.h"
#include "mUSB.h"
#include "m_microsd.h"

void filename_8_3_to_fs (const char *input_name,
                         char *output_name);

void error (void)
{
    fflush (stdout);
    
    for (;;)
    {
        mRedTOGGLE;
        mWaitms (250);
    }
}

static void search (const char *name)
{
    bool is_dir;
    if (m_sd_object_exists (name, &is_dir))
    {
        printf ("%s found: ", name);
        
        if (is_dir)
            printf ("directory\n");
        else
            printf ("file\n");
    }
    else
    {
        if (m_sd_error_code == ERROR_NONE)
            printf ("%s not found\n");
        else
            printf ("Error when searching for %s: %d\n", name, (int)m_sd_error_code);
    }
}

void main (void)
{
    mInit();
    mBusInit();
	mUSBInit();
    
    mRedON;
    mWaitms (1500);
    
    printf ("Beginning\n");
    
    if (!m_sd_init())
    {
        printf ("Error initializing mMicroSD: %d\n", (int)m_sd_error_code);
        error();
    }
    printf ("Init OK\n");
    
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
        printf ("Error pushing into testdir: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Pushed into testdir\n");
    }
    
    uint8_t file_id = 0;
    uint32_t seek_pos;
    if (!m_sd_get_seek_pos (file_id, &seek_pos))
    {
        printf ("Error getting seek pos: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seek pos: %u\n", seek_pos);
    }
    
    if (!m_sd_open_file ("python.txt", READ_FILE, &file_id))
    {
        printf ("Error opening python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Opened python.txt\n");
    }
    
    if (!m_sd_get_seek_pos (file_id, &seek_pos))
    {
        printf ("Error getting seek pos: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seek pos: %u\n", seek_pos);
    }
    
    char buffer[21];
    if (!m_sd_read_file (file_id, 21, buffer))
    {
        printf ("Error reading python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        for (uint8_t i = 0; i < 21; i++)
            printf ("%c", buffer[i]);
        printf ("\n");
    }
    
    if (!m_sd_get_seek_pos (file_id, &seek_pos))
    {
        printf ("Error getting seek pos: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seek pos: %u\n", seek_pos);
    }
    
    if (!m_sd_close_file (file_id))
    {
        printf ("Error closing python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Closed python.txt\n");
    }
    
    if (!m_sd_open_file ("python.txt", READ_FILE, &file_id))
    {
        printf ("Error opening python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Opened python.txt\n");
    }
    
    if (!m_sd_get_seek_pos (file_id, &seek_pos))
    {
        printf ("Error getting seek pos: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seek pos: %u\n", seek_pos);
    }
    
    if (!m_sd_seek (file_id, 10))
    {
        printf ("Error seeking to offset 10: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seeked to offset 10\n");
    }
    
    if (!m_sd_get_seek_pos (file_id, &seek_pos))
    {
        printf ("Error getting seek pos: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Seek pos: %u\n", seek_pos);
    }
    
    if (!m_sd_read_file (file_id, 21, buffer))
    {
        printf ("Error reading python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        for (uint8_t i = 0; i < 21; i++)
            printf ("%c", buffer[i]);
        printf ("\n");
    }
    
    if (!m_sd_close_file (file_id))
    {
        printf ("Error closing python.txt: %d\n", (int)m_sd_error_code);
    }
    else
    {
        printf ("Closed python.txt\n");
    }
    */
    
    
    // test mkdir, get_size, pushing, opening, closing, reading, writing, and seeking
    #define COPY_BUFFER_LEN 64
    char copy_buffer[COPY_BUFFER_LEN];
    
    if (!m_sd_push ("testdir"))
        printf ("Error pushing into testdir: %d\n", (int)m_sd_error_code);
    
    if (!m_sd_mkdir ("blahblah"))
        printf ("Error creating blahblah directory: %d\n", (int)m_sd_error_code);
    
    uint32_t file_size;
    if (!m_sd_get_size ("python.txt", &file_size))
        printf ("Error getting size of python.txt: %d\n", (int)m_sd_error_code);
    else
        printf ("Size of python.txt: %lu\n", file_size);
    
    uint8_t input_file_id;
    uint8_t output_file_1_id;
    uint8_t output_file_2_id;
    
    if (!m_sd_open_file ("python.txt", READ_FILE, &input_file_id))
        printf ("Error opening python.txt: %d\n", (int)m_sd_error_code);
    
    if (!m_sd_push ("blahblah"))
        printf ("Error pushing into blahblah: %d\n", (int)m_sd_error_code);
    
    if (!m_sd_open_file ("somecopy.txt", CREATE_FILE, &output_file_1_id))
        printf ("Error opening somecopy.txt: %d\n", (int)m_sd_error_code);
    
    if (!m_sd_pop())
        printf ("Error popping from blahblah: %d\n", (int)m_sd_error_code);
    
    if (!m_sd_mkdir ("copy2loc"))
        printf ("Error creating copy2loc dir: %d\n", (int)m_sd_error_code);
    
    uint32_t bytes_copied = 0;
    while (bytes_copied < file_size)
    {
        uint32_t bytes_to_copy = file_size - bytes_copied;
        if (bytes_to_copy > COPY_BUFFER_LEN)
            bytes_to_copy = COPY_BUFFER_LEN;
        
        if (!m_sd_seek (input_file_id, bytes_copied))
            printf ("Error seeking in python.txt: %d\n", (int)m_sd_error_code);
        
        if (!m_sd_read_file (input_file_id, bytes_to_copy, copy_buffer))
            printf ("Error reading python.txt: %d\n", (int)m_sd_error_code);
        
        
        
        if (!m_sd_push ("copy2loc"))
            printf ("Error pushing into copy2loc: %d\n", (int)m_sd_error_code);
        
        const open_option open_type = (bytes_copied == 0) ? CREATE_FILE : APPEND_FILE;
        if (!m_sd_open_file ("copy2.txt", open_type, &output_file_2_id))
            printf ("Error opening copy2.txt: %d\n", (int)m_sd_error_code);
        
        if (!m_sd_write_file (output_file_1_id, bytes_to_copy, copy_buffer))
            printf ("Error writing to somecopy.txt: %d\n", (int)m_sd_error_code);
        
        if (!m_sd_write_file (output_file_2_id, bytes_to_copy, copy_buffer))
            printf ("Error writing to copy2.txt: %d\n", (int)m_sd_error_code);
        
        if (!m_sd_close_file (output_file_2_id))
            printf ("Error closing copy2.txt: %d\n", (int)m_sd_error_code);
        
        if (!m_sd_pop ())
            printf ("Error popping from copy2loc: %d\n", (int)m_sd_error_code);
        
        bytes_copied += bytes_to_copy;
    }
    printf ("\nDONE\n");
    
    if (!m_sd_close_file (output_file_1_id))
        printf ("Error closing somecopy.txt: %d\n", (int)m_sd_error_code);
    
    
    
    if (!m_sd_shutdown())
    {
        printf ("Error shutting down mMicroSD: %d\n", (int)m_sd_error_code);
        error();
    }
    printf ("Shutdown OK\n");
    
    mRedOFF;
    
    for (;;)
    {
        mGreenTOGGLE;
        mWaitms (250);
    }
}
