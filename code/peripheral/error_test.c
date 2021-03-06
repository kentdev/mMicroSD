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
    m_clockdivide (1);
    m_usb_init();
    
    m_red (ON);
    m_wait (1500);  // wait a bit to give us time to start reading from USB on the computer side
    
    m_usb_tx_string ("Beginning test\n");
    
    uint8_t cluster = 0;
    for (cluster = 0; cluster < 10; cluster++)
    {
        uint16_t to_cluster = (uint16_t)12345;
        
        m_usb_tx_string ("Set cluster ");
        m_usb_tx_uint (to_cluster);
        m_usb_tx_string ("\n");
        
        uint16_t test[4] = {(uint16_t)1, (uint16_t)2, (uint16_t)3, (uint16_t)4};
        
        // the output never reaches this point, though eventually the red LED
        // turns off and the green LED begins blinking, as if it reached the
        // end of the code below
        
        /* also, if to_cluster and test are set to volatile, or to different types
           like uint32_t, the values are printed out mangled:
           
           For example, with
           volatile uint32_t to_cluster = (uint32_t)12345;
           volatile uint16_t test[4] = {(uint16_t)1, (uint16_t)2, (uint16_t)3, (uint16_t)4};
           
           the output is:
           
           
            Beginning test

            Set cluster 12345

            Writing: 32264 1 16 0 

            Set cluster 0

            Writing: 8705 2 17 0 

            Set cluster 0

            Writing: 769 3 17 0 

            Set cluster 0

            Writing: 1 4 17 0 

            Set cluster 0

            Writing: 1 5 17 0 

            Set cluster 0

            Writing: 1 6 17 0 

            Set cluster 0

            Writing: 1 7 17 0 

            Set cluster 0

            Writing: 1 8 17 0 

            Set cluster 0

            Writing: 32257 9 17 0 

            Set cluster 0

            Writing: 4353 10 17 0 

            DONE

        */
        
        
        m_usb_tx_string ("Writing: ");
        uint8_t i = 0;
        for (i = 0; i < 4; i++)
        {
            m_usb_tx_uint (test[i]);
            m_usb_tx_string (" ");
        }
        m_usb_tx_string ("\n");
    }
    
    m_usb_tx_string ("DONE\n");
    
    m_red (OFF);
    for (;;)
    {
        m_green (TOGGLE);
        m_wait (125);
    }
    
    return 0;
}

