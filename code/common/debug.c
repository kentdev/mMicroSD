#include "debug.h"

#if defined(ATMEGA168) || defined(ATMEGA328)
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <stdlib.h>

inline void send_char (const char c)
{
    while (!(UCSR0A & (1 << UDRE0)));  // wait for the transmit buffer to be empty
    UDR0 = c;  // send the current byte
}

void debugUART_str (const char *str)
{
    // send debugging strings out the ATmega168/328's UART port
    char c = pgm_read_byte (str++);
    
    while (c)  // loop until we hit the string's terminator
    {
        send_char (c);
        c = pgm_read_byte (str++);  // get the next byte in the string
    }
}

void debugUART_char (const char c)
{
    send_char (c);
}

inline char hex (const char nybble)
{
    if (nybble < 10)
        return '0' + nybble;
    else
        return 'A' + (nybble - 10);
}

void debugUART_hexchar (const char c)
{
    send_char (hex (c >> 4));
    send_char (hex (c & 0x0f));
}

void debugUART_hex (const uint16_t h)
{
    debugUART_hexchar (h >> 8);
    debugUART_hexchar (h & 0xff);
}

void debugUART_int (const int16_t i)
{
    char string[7] = {0,0,0,0,0,0,0};
    
	itoa (i, string, 10);
	
	for (int i = 0; i < 7; i++)
	{
        if (string[i])
            send_char (string[i]);
        else
            break;
    }
}

void debugUART_uint (const uint16_t i)
{
    char string[6] = {0,0,0,0,0,0};
    
	itoa (i, string, 10);
	
	for (int i = 0; i < 6; i++)
	{
        if (string[i])
            send_char (string[i]);
        else
            break;
    }
}

void debugUART_long (const int32_t l)
{
    char string[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	
	ltoa (l, string, 10);
	
	for (int i = 0; i < 11; i++)
	{
        if (string[i])
            send_char (string[i]);
        else
            break;
	}
}

void debugUART_ulong (const uint32_t l)
{
    char string[11] = {0,0,0,0,0,0,0,0,0,0,0};
	
	ltoa (l, string, 10);
	
	for (int i = 0; i < 11; i++)
	{
        if (string[i])
            send_char (string[i]);
        else
            break;
	}
}



#endif

void init_debug (void)
{
    #if defined(ATMEGA168) || defined(ATMEGA328)
    // set up UART TX (PD1)
    UBRR0 = 51;  // 9600bps at 8MHz clock, 19200bps at 16MHz
    UCSR0B = (1 << TXEN0);  // enable TX
    UCSR0C = (1 << UCSZ00) | (1 << UCSZ01);  // async UART, 8 data bits, 1 stop bit, no parity
    #endif
}

