#ifndef DETAILED_ERRORS_H
#define DETAILED_ERRORS_H

#ifdef DEBUG
    #if defined(ATMEGA168) || defined(ATMEGA328)
        #include <avr/io.h>
        void debugUART_str (const char *str);
        void debugUART_char (const char c);
        void debugUART_hexchar (const char c);
        void debugUART_hex (const uint16_t h);
        void debugUART_int (const int16_t i);
        void debugUART_uint (const uint16_t i);
        void debugUART_long (const int32_t l);
        void debugUART_ulong (const uint32_t lu);
        #define debug(s)        debugUART_str(PSTR(s))
        #define debugchar(c)    debugUART_char(c)
        #define debughexchar(c) debugUART_hexchar(c)
        #define debughex(h)     debugUART_hex(h)
        #define debugint(i)     debugUART_int(i)
        #define debuguint(u)    debugUART_uint(u)
        #define debuglong(l)    debugUART_long(l)
        #define debugulong(l)   debugUART_ulong(l)
    #elif defined(M2)
        #include "m_usb.h"
        #define debug(s)        m_usb_tx_string(s)
        #define debugchar(c)    m_usb_tx_char(c)
        #define debughexchar(c) m_usb_tx_hexchar(c)
        #define debughex(h)     m_usb_tx_hex(h)
        #define debugint(i)     m_usb_tx_int(i)
        #define debuguint(u)    m_usb_tx_uint(u)
        #define debuglong(l)    m_usb_tx_long(l)
        #define debugulong(l)   m_usb_tx_ulong(l)
    #elif defined(M4)
        #define debug(s)        printf(s)
        #define debugchar(c)    printf("%d", c)
        #define debughexchar(c) printf("%x", c)
        #define debughex(h)     printf("%x", h)
        #define debugint(i)     printf("%d", i)
        #define debuguint(u)    printf("%u", u)
        #define debuglong(l)    printf("%ld", l)
        #define debugulong(l)   printf("%lu", l)
    #else
        #error Unknown target device
    #endif
#else
    #define debug(s)
    #define debugchar(c)
    #define debughexchar(c)
    #define debughex(h)
    #define debugint(i)
    #define debuguint(u)
    #define debuglong(l)
    #define debugulong(l)
#endif

void init_debug (void);

#endif
