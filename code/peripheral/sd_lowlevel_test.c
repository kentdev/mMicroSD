#include "sd_lowlevel.h"
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

int main (void)
{
    m_disableJTAG();
    m_clockdivide (1);  // set clock to 8MHz
    
    start_spi (SPI_LOW_SPEED);
    
    m_usb_init();
    m_red (ON);
    m_wait (1500);
    m_red (OFF);
    
    m_green (ON);
    m_wait (1);  // wait 1ms after power-up
    
    m_green (OFF);
    
    m_red (ON);
    
    m_usb_tx_string ("Resetting SD card...\r\n");
    if (reset_card() != OK)
    {
        m_usb_tx_string ("Error resetting SD card\r\n");
        error();
    }
    
    m_usb_tx_string ("Enabling CRC...\r\n");
    if (enable_crc() != OK)
    {
        m_usb_tx_string ("Error enabling CRC\r\n");
    }
    
    m_usb_tx_string ("Initializing SD card...\r\n");
    if (initialize_card() != OK)
    {
        m_usb_tx_string ("Error initializing SD card\r\n");
        error();
    }
    
    start_spi (SPI_HIGH_SPEED);
    
    m_usb_tx_string ("Setting block length...\r\n");
    if (set_block_length (512) != OK)
    {
        m_usb_tx_string ("Error sending block length command\r\n");
        error();
    }
    
    m_red (OFF);
    
    
    
    
    if (read_block (0) != OK)
    {
        m_usb_tx_string ("Error reading block\r\n");
        error();
    }
    
    /*m_usb_tx_string ("Data:\r\n");
    m_usb_tx_hex (0);
    m_usb_tx_string ("\t\t");
    for (uint16_t i = 0; i < 512; i++)
    {
        m_usb_tx_hex (block[i]);
        m_usb_tx_string ("\t");
        if ((i + 1) % 8 == 0 && i + 1 < 512)
        {
            m_usb_tx_string ("\r\n");
            m_usb_tx_hex (i + 1);
            m_usb_tx_string ("\t\t");
        }
    }
    m_usb_tx_string ("\r\n");*/
    
    
    char fill[33] = "abcdefghijklmnopqrstuvwxyz123456";
    int j = 0;
    for (int i = 0; i < 512; i++)
    {
        block[i] = fill[j++];
        if (j == 32)
            j = 0;
    }
    
    if (write_block (0) != OK)
    {
        m_usb_tx_string ("Error writing block\r\n");
        error();
    }
    
    if (read_block (0) != OK)
    {
        m_usb_tx_string ("Error reading block\r\n");
        error();
    }
    
    m_usb_tx_string ("Data:\r\n");
    m_usb_tx_hex (0);
    m_usb_tx_string ("\t\t");
    for (uint16_t i = 0; i < 512; i++)
    {
        m_usb_tx_hex (block[i]);
        m_usb_tx_string ("\t");
        if ((i + 1) % 8 == 0 && i + 1 < 512)
        {
            m_usb_tx_string ("\r\n");
            m_usb_tx_hex (i + 1);
            m_usb_tx_string ("\t\t");
        }
    }
    m_usb_tx_string ("\r\n");
    
    for (;;)
    {
        m_green (TOGGLE);
        m_wait (125);
    }
    
    return 0;
}
