#include "m_general.h"
#include "m_usb.h"


void free_ram (void)
{  // found this online
  extern int __heap_start, *__brkval; 
  int v; 
  m_usb_tx_hex ((int)&v);
  m_usb_tx_string (", free RAM: ");
  m_usb_tx_int( (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval) );
  m_usb_tx_string ("\n");
}


uint16_t i;
void begin (void)
{
    volatile uint16_t sacrifice[1200];
    
    uint16_t to_cluster = (uint16_t)12345;
    uint16_t test1 = 1;
    uint16_t test2 = 2;
    uint16_t test3 = 3;
    uint16_t test4 = 4;
    
    
    
    m_clockdivide (1);
    m_usb_init();
    
    m_red (ON);
    m_wait (1000);  // wait a bit to give us time to start reading from USB on the computer side
    
    free_ram();
    
    m_usb_tx_string ("RAMEND = ");
    m_usb_tx_uint (RAMEND);
    m_usb_tx_string (", FLASHEND = ");
    m_usb_tx_uint (FLASHEND);
    m_usb_tx_string ("\n");
    
    m_usb_tx_string ("io.h: ");
    m_usb_tx_string (_AVR_IOXXX_H_);
    m_usb_tx_string ("\n");
    
    m_usb_tx_string ("Beginning test\n");
    
    uint8_t cluster = 1;
    
    
    free_ram();
    
    m_usb_tx_string ("sacrifice addrs:\n");
    for (i = 0; i < 1200; i++)
    {
        sacrifice[i] = i;
        m_usb_tx_hex (&(sacrifice[i]));
        m_usb_tx_char (' ');
    }
    m_usb_tx_string ("\n");
    
    m_usb_tx_int ((int)&(sacrifice[1199]) - (int)&(sacrifice[0]));
    m_usb_tx_string ("\n");
    
    m_usb_tx_uint (test1);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test2);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test3);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test4);
    m_usb_tx_string (" ");
    
    m_usb_tx_string ("\n");
    
    m_usb_tx_string ("Set ");
    m_usb_tx_uint (to_cluster);
    m_usb_tx_string (", ");
    m_usb_tx_int ((int)(test3));
    m_usb_tx_string ("\n");
    
    m_wait (100);
    
    m_usb_tx_string ("Addrs: ");
    m_usb_tx_hex (&test1);
    m_usb_tx_string (", ");
    m_usb_tx_hex ((&test2));
    m_usb_tx_string (", ");
    m_usb_tx_hex ((&test3));
    m_usb_tx_string (", ");
    m_usb_tx_hex ((&test4));
    m_usb_tx_string ("\n");
    
    m_usb_tx_uint (test1);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test2);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test3);
    m_usb_tx_string (" ");
    m_usb_tx_uint (test4);
    m_usb_tx_string (" ");
    
    m_usb_tx_string ("\n");
    
    

    
    m_usb_tx_string ("DONE\n");
    
    m_red (OFF);
    for (;;)
    {
        m_green (TOGGLE);
        m_wait (125);
    }
}

void main (void)
{
    begin();
}

