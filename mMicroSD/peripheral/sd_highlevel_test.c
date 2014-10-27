#include "sd_highlevel.h"
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

void error_print (enum card_error_codes err)
{
    m_usb_tx_string ("ERROR ");
    m_usb_tx_uint ((uint16_t) err);
    m_usb_tx_string ("\n");
}

int main (void)
{
    m_disableJTAG();
    m_clockdivide (1);  // set clock to 8MHz
    
    m_usb_init();
    
    m_red (ON);
    m_wait (1500);
    
    if (!init_card (true))
    {
        error_print (error_code);
        error();
    }
    m_red (OFF);
    
    
    uint8_t read_buffer[25];
    uint8_t write_buffer[8] = {'A','A','B','C','C','B','A','A'};
    
    m_usb_tx_string ("Partial read (24 bytes, offset 1): ");
    if (!read_partial_block (1, 1, read_buffer, 24))
    {
        error_print (error_code);
        error();
    }
    read_buffer[24] = '\0';
    
    for (uint8_t i = 0; i < 25; i++)
    {
        m_usb_tx_char (read_buffer[i]);
        m_usb_tx_string (" ");
    }
    m_usb_tx_string ("\n");
    
    m_usb_tx_string ("Partial write (8 bytes, offset 4) of A A B C C B A A\n");
    if (!write_partial_block (1, 4, write_buffer, 8))
    {
        error_print (error_code);
        error();
    }
    
    m_usb_tx_string ("Partial read (24 bytes, offset 2): ");
    if (!read_partial_block (1, 2, read_buffer, 24))
    {
        error_print (error_code);
        error();
    }
    read_buffer[24] = '\0';
    
    for (uint8_t i = 0; i < 25; i++)
    {
        m_usb_tx_char (read_buffer[i]);
        m_usb_tx_string (" ");
    }
    m_usb_tx_string ("\n");
    
    
    // reset the block for next time
    char fill[33] = "abcdefghijklmnopqrstuvwxyz123456";
    int j = 0;
    for (int i = 0; i < 512; i++)
    {
        block[i] = fill[j++];
        if (j == 32)
            j = 0;
    }
    
    if (write_block (1) != OK)
    {
        m_usb_tx_string ("Error writing block\n");
        error();
    }
    
    
    for (;;)
    {
        m_green (TOGGLE);
        m_wait (125);
    }
    
    return 0;
}
