#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>

#include "sd_fat32.h"

#define I2C_ADDR (0x5D)
#define TWI_BUFFER_LEN 257

// This code is designed to run on the ATmega168 or ATmega328, not the M2

#define set(reg,bit)		reg |= (1<<(bit))
#define clear(reg,bit)		reg &= ~(1<<(bit))
#define toggle(reg,bit)		reg ^= (1<<(bit))
#define check(reg,bit)		(bool)(reg & (1<<(bit)))


// I2C slave status codes:
// transmit:
#define TX_ADDR_ACK          0xA8  // received our address + read bit and ACK'd
#define TX_ADDR_ACK_ARB_LOST 0xB0  // received our address + read bit and ACK'd, also master arbitration was lost
#define TX_BYTE_ACK          0xB8  // sent the byte in TWDR and got an ACK
#define TX_BYTE_NACK         0xC0  // sent the byte in TWDR and got a NACK
#define TX_FINAL_BYTE_ACK    0xC8  // sent the last byte in TWDR and got an ACK

// receive:
#define RX_ADDR_ACK          0x60  // received our address + write bit and ACK'd
#define RX_ADDR_ACK_ARB_LOST 0x68  // received our address + write bit and ACK'd, also master arbitration was lost
#define RX_GEN_ACK           0x70  // received general call address and ACK'd
#define RX_GEN_ACK_ARB_LOST  0x78  // received general call address and ACK'd, also master arbitration was lost
#define RX_ADDR_DATA_ACK     0x80  // received data on our address and ACK'd
#define RX_ADDR_DATA_NACK    0x88  // received data on our address and NACK'd
#define RX_GEN_DATA_ACK      0x90  // received data on general call address and ACK'd
#define RX_GEN_DATA_NACK     0x98  // received data on general call address and NACK'd
#define RX_STOP_RESTART      0xA0  // received a STOP or repeated START condition

// other:
#define I2C_UNDEFINED        0xF8  // no state information available
#define I2C_ERROR            0x00  // an error occurred on the I2C bus


enum i2c_error_codes
{  // these codes are numbered after the fat32 error codes in sd_fat32.h
    ERROR_I2C_COMMAND = NUM_FAT32_ERROR_CODES, // error receiving I2C command
    NUM_I2C_ERROR_CODES
};


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

// we can reuse the same buffer for sending and receiving data, since
// we will only be doing one of them at a time
struct
{
    uint8_t code;
    uint8_t data_length;
    uint8_t data[TWI_BUFFER_LEN];
} transmission;

volatile bool new_order;
volatile uint8_t TWCR_state;


#ifdef TEST_FAT32

// TEST: Copy testdir/python.txt into both
//       testdir/created/py_copy.txt and testdir/created/py_copy2.txt

static void error (void)
{
    for (;;)
    {
        toggle (PORTB, 0);
        _delay_ms (125);
    }
}

#define COPY_BUFFER_LEN 80
uint8_t copy_buffer[COPY_BUFFER_LEN];

static void test_copy (void)
{
    if (!sd_fat32_init())
        error();
    
    if (!sd_fat32_push ("testdir"))
        error();
    
    uint32_t file_size;
    if (!sd_fat32_get_size ("python.txt", &file_size))
        error();
    
    uint32_t bytes_copied = 0;
    
    while (bytes_copied < file_size)
    {
        uint32_t bytes_to_copy = file_size - bytes_copied;
        if (bytes_to_copy > COPY_BUFFER_LEN)
            bytes_to_copy = COPY_BUFFER_LEN;
        
        if (!sd_fat32_open_file ("python.txt", READ_FILE))
            error();
        
        if (!sd_fat32_seek (bytes_copied))
            error();
        
        if (!sd_fat32_read_file (bytes_to_copy, copy_buffer))
            error();
        
        if (!sd_fat32_push ("created"))
            error();
        
        const open_option open_type = (bytes_copied == 0) ? CREATE_FILE : APPEND_FILE;
        if (!sd_fat32_open_file ("py_copy.txt", open_type))
            error();
        
        if (!sd_fat32_write_file (bytes_to_copy, copy_buffer))
            error();
        
        if (!sd_fat32_open_file ("py_copy2.txt", open_type))
            error();
        
        if (!sd_fat32_write_file (bytes_to_copy, copy_buffer))
            error();
        
        if (!sd_fat32_pop ())
            error();
        
        bytes_copied += bytes_to_copy;
    }
    
    if (!sd_fat32_shutdown())
        error();
}

#endif





#define TWI_ACK()    {(TWCR = _BV (TWIE) | _BV (TWINT) | _BV (TWEN) | _BV (TWEA)); TWCR_state = TWCR;}
#define TWI_NACK()   {(TWCR = _BV (TWIE) | _BV (TWINT) | _BV (TWEN)); TWCR_state = TWCR;}

void process_order (void);

void main (void)
{
    // set the clock divider to 1, for 8 MHz
    CLKPR = (1<<CLKPCE);
    CLKPR = 0;
    
    
    #ifdef TEST_FAT32
    // test the FAT32 interface and then busy-loop
    // LED on: still working
    // LED off: done
    // LED blinking: error
    set (DDRB, 0);
    set (PORTB, 0);
    test_copy();
    clear (PORTB, 0);
    for (;;);
    #endif
    
    
    // set up I2C:
    
    // I2C frequency = CPU clock / (16 + (2 * TWBR * prescaler))
    // clock is 8 MHz and we want a 400 kHz frequency: prescaler = 1, TWBR = 2
    
    // set prescaler to 1 and TWBR to 2
    clear (TWSR, TWPS0);
    clear (TWSR, TWPS1);
    TWBR = 2;
    
    new_order = false;
    
    // set slave address, and don't listen to the general call address
    TWAR = (I2C_ADDR << 1) & 0xfe;
    
    // enable I2C, I2C interrupt and interrupt flag, and respond with ACK
    TWI_ACK();
    
    sei();  // enable interrupts
    
    
    
    set (DDRB, 0);
    
    for (uint8_t i = 0; i < 10; i++)
    {
        toggle (PORTB, 0);
        _delay_ms (25);
    }
    
    set (PORTB, 0);
    for (;;)
    {
        // process any commands received over I2C
        
        if (new_order)
        {  // there's a new order ready for processing
            // I2C should be NACKing everything at this point
            
            process_order();
            set (PORTB, 0);
            
            new_order = false;
            TWI_ACK();
        }
    }
}


void process_order (void)
{
    if (transmission.data_length > TWI_BUFFER_LEN)
    {
        transmission.code = ERROR_I2C_COMMAND;
        transmission.data_length = 0;
        return;
    }
    
    switch (transmission.code)
    {
        case M_SD_INIT:
            sd_fat32_init();
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_SHUTDOWN:
            sd_fat32_shutdown();
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_GET_SIZE:
            {
                if (transmission.data_length == 0)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                if (transmission.data_length > 12)
                {
                    transmission.code = ERROR_FAT32_INVALID_NAME;
                    transmission.data_length = 0;
                    return;
                }
                
                char filename[13];
                for (uint8_t i = 0; i < 12; i++)
                    filename[i] = (char)transmission.data[i];
                filename[12] = '\0';
                
                sd_fat32_get_size (filename,
                                   (uint32_t *)transmission.data);
                transmission.code = error_code;
                transmission.data_length = 4;
            }
            break;
        
        case M_SD_OBJECT_EXISTS:
            {
                if (transmission.data_length == 0)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                char filename[13];
                for (uint8_t i = 0; i < 12; i++)
                    filename[i] = (char)transmission.data[i];
                filename[12] = '\0';
                
                bool retval = sd_fat32_object_exists (filename,
                                                      (bool *)&(transmission.data[1]));
                transmission.code = error_code;
                transmission.data[0] = (uint8_t)retval;
                transmission.data_length = 2;
            }
            break;
        
        case M_SD_GET_FIRST_ENTRY:
            {
                bool retval = sd_fat32_get_dir_entry_first ((char *)&transmission.data[1]);
                transmission.data[0] = (uint8_t)retval;
                transmission.code = error_code;
                transmission.data_length = 12;
            }
            break;
        
        case M_SD_GET_NEXT_ENTRY:
            {
                bool retval = sd_fat32_get_dir_entry_next ((char *)&transmission.data[1]);
                transmission.data[0] = (uint8_t)retval;
                transmission.code = error_code;
                transmission.data_length = 12;
            }
            break;
        
        case M_SD_PUSH:
            if (transmission.data_length == 0)
            {
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                return;
            }
            
            sd_fat32_push ((char *)transmission.data);
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_POP:
            sd_fat32_pop();
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_MKDIR:
            if (transmission.data_length == 0)
            {
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                return;
            }
            
            sd_fat32_mkdir ((char *)transmission.data);
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_RMDIR:
            if (transmission.data_length == 0)
            {
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                return;
            }
            
            sd_fat32_rmdir ((char *)transmission.data);
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_DELETE:
            if (transmission.data_length == 0)
            {
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                return;
            }
            
            sd_fat32_delete ((char *)transmission.data);
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_OPEN_FILE:
            {
                if (transmission.data[transmission.data_length - 1] != '\0')
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                if (transmission.data[0] > 2)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                uint8_t file_id;
                sd_fat32_open_file ((char *)&(transmission.data[1]),
                                    transmission.data[0],
                                    &file_id);
                
                transmission.code = error_code;
                transmission.data_length = 1;
                transmission.data[0] = file_id;
            }
            break;
        
        case M_SD_CLOSE_FILE:
            if (transmission.data_length != 1)
            {
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                return;
            }
            
            sd_fat32_close_file (transmission.data[0]);
            transmission.code = error_code;
            transmission.data_length = 0;
            break;
        
        case M_SD_SEEK:
            {
                if (transmission.data_length != 5)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                const uint32_t* seek_ptr = (uint32_t*)&(transmission.data[1]);
                sd_fat32_seek (transmission.data[0],
                               *seek_ptr);
                
                transmission.code = error_code;
                transmission.data_length = 0;
            }
            break;
        
        case M_SD_GET_SEEK:
            {
                if (transmission.data_length != 1)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                const uint8_t file_id = transmission.data[0];
                
                uint32_t *data_ptr = (uint32_t*)transmission.data;
                if (!sd_fat32_get_seek_pos (file_id, data_ptr))
                {
                    transmission.code = error_code;
                    transmission.data_length = 0;
                    return;
                }
                
                transmission.code = error_code;
                transmission.data_length = 4;
            }
            break;
        
        case M_SD_READ_FILE:
            {
                if (transmission.data_length != 2)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                const uint8_t file_id = transmission.data[0];
                const uint8_t length = transmission.data[1];
                
                sd_fat32_read_file (file_id,
                                    (uint32_t)length,
                                    (uint8_t*)transmission.data);
                transmission.code = error_code;
                
                if (error_code == ERROR_NONE)
                    transmission.data_length = length;
                else
                    transmission.data_length = 0;
            }
            break;
        
        case M_SD_WRITE_FILE:
            {
                if (transmission.data_length < 2)
                {
                    transmission.code = ERROR_I2C_COMMAND;
                    transmission.data_length = 0;
                    return;
                }
                
                const uint8_t file_id = transmission.data[0];
                
                sd_fat32_write_file (file_id,
                                     (uint32_t)transmission.data_length - 1,
                                     (uint8_t*)&transmission.data[1]);
                transmission.code = error_code;
                transmission.data_length = 0;
            }
            break;
        
        default:
            transmission.code = ERROR_I2C_COMMAND;
            transmission.data_length = 0;
            break;
    }
}



ISR (TWI_vect)
{
    static uint8_t *transmission_ptr = (uint8_t *)&transmission;
    static bool receiving = false;
    
    const uint8_t status = TWSR & 0b11111000;
    
    switch (status)
    {
        // sending response:
        case TX_ADDR_ACK:
        case TX_ADDR_ACK_ARB_LOST:
            // master wants to read response data
            transmission_ptr = (uint8_t *)&transmission;
            TWDR = *transmission_ptr++;
            TWI_ACK();
            break;
        
        case TX_BYTE_ACK:
            // continuing to send data
            TWDR = *transmission_ptr++;
            
            if ( (transmission_ptr >= ((uint8_t *)&transmission) + 2 &&
                  transmission_ptr >= ((uint8_t *)&transmission) + 2 + transmission.data_length) ||
                 transmission_ptr >= ((uint8_t *)&transmission) + 2 + TWI_BUFFER_LEN)
            {  // if we've reached the end of our response
                transmission_ptr = (uint8_t *)&transmission;  // reset buffer to default address
            }
            TWI_ACK();
            break;
        
        case TX_BYTE_NACK:
            // master says that this was the final byte it will read
            transmission_ptr = (uint8_t *)&transmission;  // reset buffer to default address
            TWI_ACK();
            break;
        
        
        // receiving order:
        case RX_ADDR_ACK:
        case RX_ADDR_ACK_ARB_LOST:
            // beginning to receive an order
            receiving = true;
            
            transmission.code = M_SD_NONE;
            transmission.data_length = 0;
            
            transmission_ptr = (uint8_t *)&transmission;
            TWI_ACK();
            break;
        
        case RX_ADDR_DATA_ACK:
            *transmission_ptr++ = TWDR;
            
            if ( (transmission_ptr >= ((uint8_t*)&transmission) + 2 &&
                  transmission_ptr >= ((uint8_t*)&transmission) + 2 + transmission.data_length) ||
                transmission_ptr >= ((uint8_t*)&transmission) + 2 + TWI_BUFFER_LEN)
            {
                TWI_NACK();  // don't accept any new commands until processing is complete
                
                // if we've read all the data the master said it would send, or if we're
                // about to overflow our buffer
                clear (PORTB, 0);
                receiving = false;
                new_order = true;
                
                /*
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                */
                
                transmission_ptr = (uint8_t *)&transmission;  // reset buffer to default address
                
            }
            else
            {
                TWI_ACK();  // continue receiving data
            }
            break;
        
        
        
        
        // encountered a stop or restart
        case RX_STOP_RESTART:
        // encountered an error, unknown state, or otherwise:    
        case TX_FINAL_BYTE_ACK:
        case RX_ADDR_DATA_NACK:
        case RX_GEN_ACK:
        case RX_GEN_ACK_ARB_LOST:
        case RX_GEN_DATA_ACK:
        case RX_GEN_DATA_NACK:
        case I2C_UNDEFINED:
        case I2C_ERROR:
        default:
            if (new_order)
            {  // if we already have a command to be processed
                TWI_NACK();
            }
            else if (receiving)
            {  // if we got a stop or restart while receiving a command
                // treat it as the end of the command and begin processing
                clear (PORTB, 0);
                receiving = false;
                new_order = true;
                
                /*
                transmission.code = ERROR_I2C_COMMAND;
                transmission.data_length = 0;
                */
                
                transmission_ptr = (uint8_t *)&transmission;  // reset buffer to default address
                TWI_NACK();
            }
            else
            {
                transmission_ptr = (uint8_t *)&transmission;
                receiving = false;
                TWI_ACK();
            }
            break;
    }
}


