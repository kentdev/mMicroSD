/*******************************************************************************
* sd_lowlevel.c
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Low-level code for reading from and writing to an SD card over
*              SPI.  The code is designed to be run from either an ATmega168, an
*              ATmega328, an M2, or an M4.  In the case of the M4, it uses SPI1,
*              and B1 is used as the chip select pin (though this can be changed
*              in the defines below).
*******************************************************************************/

#include "sd_lowlevel.h"
#include "debug.h"

uint32_t total_block_accesses = 0;


#if defined(M2)
// running on the M2

#define POWER_REDUCTION_REGISTER PRR0

#define P_SS   PORTB0
#define P_SCLK PORTB1
#define P_MOSI PORTB2
#define P_MISO PORTB3

#define SS_HIGH() set   (PORTB, P_SS)
#define SS_LOW()  clear (PORTB, P_SS)

#elif (defined(ATMEGA168) || defined(ATMEGA328))
// running on the ATmega168/328

#define POWER_REDUCTION_REGISTER PRR

#define P_SS   PORTB2
#define P_SCLK PORTB5
#define P_MOSI PORTB3
#define P_MISO PORTB4

#define SS_HIGH() set   (PORTB, P_SS)
#define SS_LOW()  clear (PORTB, P_SS)

#elif defined(M4)
// running on the M4

// M4 pins (SPI1):
//    SS: (arbitrary) B1
//    SCLK: B3
//    MOSI: B4
//    MISO: B5

// to change the SS pin, change these 4 defines
#define SS_GPIO_PIN  GPIO_Pin_1
#define SS_GPIO_PORT GPIOB
#define SS_HIGH()    (GPIOB->BSRR = GPIO_BSRR_BS_1)
#define SS_LOW()     (GPIOB->BSRR = GPIO_BSRR_BR_1)

#else
#error No idea what board this code is being compiled for
#endif





#define CMD_RESET          0
#define CMD_INIT           1
#define CMD_CHECK_VOLTAGE  8
#define CMD_BLOCK_LENGTH   16
#define CMD_READ_BLOCK     17
#define CMD_WRITE_BLOCK    24
#define CMD_SD_INIT        41
#define CMD_APP_CMD        55
#define CMD_READ_OCR       58
#define CMD_CRC_ON_OFF     59

bool is_sdhc = false;
bool crc_enabled = false;

uint16_t last_crc = 0;  // the most recently received CRC value from the card
// (needed because the card could be returning all 0xFFFFs instead of actual CRCs)

uint16_t block_length = 0;



#ifndef M4
// AVR code

void write_SPI_byte (uint8_t byte)
{
    SPDR = byte;  // send the byte to the output register
    while (!check (SPSR, SPIF));  // wait for it to be written out
    clear (SPSR, SPIF);
}

uint8_t read_SPI_byte()
{
    SPDR = 0xff;  // send dummy data
    while (!check (SPSR, SPIF));
    clear (SPSR, SPIF);
    return SPDR;
}

#else
// M4 code

inline uint8_t read_from_FIFO (void)
{
    uint32_t spi_base = (uint32_t)SPI1;
    spi_base += 0x0C;
    
    return *(volatile uint8_t *)spi_base;
}

inline void write_to_FIFO (const uint8_t byte)
{
    uint32_t spi_base = (uint32_t)SPI1;
    spi_base += 0x0C;
    
    *(volatile uint8_t *)spi_base = byte;
}

void write_SPI_byte (uint8_t byte)
{
    volatile SPI_TypeDef *spi = SPI1;
    
    read_from_FIFO();  // ensure that the RXNE flag is cleared
    
    write_to_FIFO (byte);  // send the byte to the TX FIFO
    
    while (!(spi->SR & SPI_SR_TXE));  // wait for the data to be placed into the TX FIFO
    while (!(spi->SR & SPI_SR_RXNE));  // wait for the response to be shifted in
    
    byte = read_from_FIFO();  // read in the (unused) response, to get it out of the RX FIFO
    //printf ("w:%x\n", byte);
    
    while (spi->SR & SPI_SR_BSY);  // wait for the SPI bus to finish working
    mWaitus (1);
}

uint8_t read_SPI_byte()
{
    volatile SPI_TypeDef *spi = SPI1;
    
    read_from_FIFO();  // ensure that the RXNE flag is cleared
    
    write_to_FIFO (0xff);  // write a dummy byte
    
    while (!(spi->SR & SPI_SR_TXE));  // wait for the data to be placed into the TX FIFO
    while (!(spi->SR & SPI_SR_RXNE));  // wait for the response to be shifted in
    
    uint8_t byte = read_from_FIFO();  // read the response
    //printf ("r:%x\n", byte);
    
    while (spi->SR & SPI_SR_BSY);  // wait for the SPI bus to finish working
    mWaitus (1);
    
    return byte;
}

#endif



void attempt_resync (void)
{
    SS_HIGH();
    read_SPI_byte();
    SS_LOW();
    uint16_t byte_counter = 0;
    while (read_SPI_byte() != (uint8_t)0xff || byte_counter < 65535)
        byte_counter++;
    SS_HIGH();
}

uint8_t send_SD_command (uint8_t command, const uint32_t data)
{
    uint8_t message[6];
    message[0] = 0b01000000 | command;
    message[1] = (data >> 24) & 0xff;
    message[2] = (data >> 16) & 0xff;
    message[3] = (data >> 8)  & 0xff;
    message[4] =  data        & 0xff;
    
    if (command == CMD_RESET)
        message[5] = 0x95;  // need a valid CRC when saying "go to SPI mode"
    else if (command == CMD_CHECK_VOLTAGE)
        message[5] = 0x87;  // also need a valid CRC for the voltage check command
    else if (command == CMD_CRC_ON_OFF || crc_enabled)
        message[5] = (getCRC (message, 5) << 1) | 0x01;
    else
        message[5] = 0xff;  // dummy CRC byte
    
    
    SS_HIGH();
    write_SPI_byte (0xff);  // give the card some breathing room between commands
    SS_LOW();
    
    for (uint8_t i = 0; i < 6; i++)
        write_SPI_byte (message[i]);
    
    // need to send an additional 8 clock cycles after a command
    // but give it 10 bytes' worth to see if it sends a response
    uint8_t response = 0xff;
    for (uint8_t i = 0; i < 10 && response == 0xff; i++)
        response = read_SPI_byte();
    
    SS_HIGH();
    
    return response;
}

ret reset_card (void)
{
    crc_enabled = false;
    uint8_t response;
    
    // give the SD card 80 clock cycles to start up (with SS and MOSI high)
    SS_HIGH();
    
    for (uint8_t i = 0; i < 10; i++)
        read_SPI_byte();
    
    // try several times before giving up
    for (uint16_t i = 0; i < RESET_TRIES_BEFORE_ERROR; i++)
    {
        response = send_SD_command (CMD_RESET, 0);
        
        if (response == 1)  // got the expected response
            return SPI_OK;
        
        // give it a few clock cycles
        for (uint8_t rest = 0; rest < 5; rest++)
            write_SPI_byte (0xff);
    }
    
    return SPI_ERROR;  // failure
}

ret enable_crc (void)
{
    const uint8_t response = send_SD_command (CMD_CRC_ON_OFF, 1);
    
    if (response == 0 || response == 1)
    {
        crc_enabled = true;
        return SPI_OK;
    }
    
    return SPI_ERROR;
}

int8_t voltage_command (void)
{
    uint8_t voltage_response = send_SD_command (CMD_CHECK_VOLTAGE, 0x1aa);
    
    if (voltage_response == 1)
    {  // the card returned OK, and will send an additional 4 bytes
        SS_LOW();
        read_SPI_byte();
        read_SPI_byte();
        uint8_t byte1 = read_SPI_byte();
        uint8_t byte2 = read_SPI_byte();
        SS_HIGH();
        
        if (byte1 != 0x01 || byte2 != 0xaa)
        {  // invalid data returned
            return -1;  // bad response: we can't use the card at all in this case
        }
        return 1;  // indicate SDHC card
    }
    else
    {  // the card returned "invalid command"
        SS_LOW();
        while (voltage_response != 0xff)
        {  // it might return a bunch of nonsense, wait until it's done
            voltage_response = read_SPI_byte();
        }
        SS_HIGH();
        
        return 0;  // indicate SD card
    }
}

void check_sdhc_blocksize (void)
{  // cards that respond to an SDHC init might still use SD block addressing
    if (send_SD_command (CMD_READ_OCR, 0) != 0)  // card responded with an error
        is_sdhc = false;
    
    // if no error, the card sends 4 more bytes
    
    SS_LOW();
    
    // the second most significant bit of the first byte indicates addressing type
    if ((read_SPI_byte() & 0x40) == 0)
        is_sdhc = false;
    
    // read the other, unused, bytes
    read_SPI_byte();
    read_SPI_byte();
    read_SPI_byte();
    
    SS_HIGH();
    
    #ifdef LOWLEVEL_DEBUG
    if (is_sdhc)
        debug ("Card uses SDHC addressing\n");
    else
        debug ("Card uses SD addressing\n");
    #endif
}

ret initialize_card (void)
{
    uint8_t response;
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Initializing\n");
    #endif
    
    int8_t voltage_response = voltage_command();
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Voltage response: ");
    debugint (voltage_response);
    debug ("\n");
    #endif
    
    if (voltage_response == -1)
    {  // bad voltage response, don't use the card
        return SPI_ERROR;
    }
    
    if (voltage_response == 1)
    {  // SDHC card
        is_sdhc = true;
        
        #ifdef LOWLEVEL_DEBUG
        debug ("SDHC init\n");
        #endif
        for (uint16_t tries = 0; tries < INIT_TRIES_BEFORE_ERROR; tries++)
        {
            send_SD_command (CMD_APP_CMD, 0);  // prepare for special init command
            
            // send init command with the HCS flag (indicates host SDHC support)
            response = send_SD_command (CMD_SD_INIT, 0x40000000);
            
            if (response == 0)  // success
            {
                #ifdef LOWLEVEL_DEBUG
                debug ("success\n");
                #endif
                
                check_sdhc_blocksize();  // check whether SDHC addressing is actually used
                
                return SPI_OK;
            }
            
            if (response == 1)  // card is still busy
                continue;
            
            // any other response: encountered an error
            #ifdef LOWLEVEL_DEBUG
            debug ("error\n");
            #endif
            break;
        }
    }
    
    
    // if we reached this point, the card is non-SDHC
    #ifdef LOWLEVEL_DEBUG
    debug ("SD init\n");
    #endif
    uint16_t tries = 0;
    while (1)
    {
        tries++;
        
        if (tries > INIT_TRIES_BEFORE_ERROR)
            return SPI_TIMEOUT;
        
        send_SD_command (CMD_APP_CMD, 0);  // prepare for special init command
        response = send_SD_command (CMD_SD_INIT, 0);  // send init command
        
        if (response == 0)  // success
            return SPI_OK;
        
        if (response == 1)  // card is still busy
            continue;
        
        // any other response: error
        // try CMD_INIT without the prepended CMD_ACMD
        tries /= 2;  // give it a few more tries to work with
        break;
    }
    
    while (1)
    {  // ACMD41 failed, try regular CMD1 initialization
        tries++;
        
        if (tries > INIT_TRIES_BEFORE_ERROR)
            return SPI_TIMEOUT;
        
        response = send_SD_command (CMD_INIT, 0);
        
        if (response == 0)  // success
            return SPI_OK;
        
        if (response == 1)  // card is still busy
            continue;
        
        // any other response: error
        return SPI_ERROR;
    }
    
    return SPI_OK;
}


ret set_block_length (const uint16_t new_block_length)
{
    if (new_block_length > MAX_BLOCK_LENGTH)
        return SPI_ERROR;
    
    const uint8_t response = send_SD_command (CMD_BLOCK_LENGTH, new_block_length);
    
    if (response == 0)
    {
        block_length = new_block_length;
        return SPI_OK;
    }
    return SPI_ERROR;
}

ret read_block (const uint32_t block_number, uint8_t *block)
{
    // SD cards take the direct address of the block
    // SDHC cards take the block number
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    total_block_accesses++;
    
    #ifdef LOWLEVEL_DEBUG
    if (is_sdhc)
        debug ("SDHC");
    else
        debug ("SD");
    
    debug (" read: block number = ");
    debuguint (block_number);
    debugchar ('\n');
    #endif
    
    uint8_t response = send_SD_command (CMD_READ_BLOCK, (is_sdhc) ? block_number : block_number * (uint32_t)512);
    uint16_t emptyBytes = 0;
    
    uint16_t crc = 0;
    uint16_t sent_crc;
    
    SS_LOW();
    while (response == 0xff ||
           response == 0x00)
    {  // waiting for a response
        emptyBytes++;
        
        if (emptyBytes >= READ_BLOCK_TIMEOUT_BYTES)  // we've waited long enough
        {
            #ifdef LOWLEVEL_DEBUG
            debug ("Error: timeout\n");
            #endif
            return SPI_TIMEOUT;  // give up
        }
        
        response = read_SPI_byte();
    }
    SS_HIGH();
    
    if (response != 0xfe)
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Error: response = ");
        debughex (response & (uint16_t)0xff);
        debug ("\n");
        #endif
        return SPI_ERROR;  // the result was an error byte, not a data token
    }
    
    // all clear, begin reading
    SS_LOW();
    
    for (uint16_t i = 0; i < block_length; i++)
        block[i] = read_SPI_byte();
    
    if (!crc_enabled)
    {
        read_SPI_byte();  // 16-bit CRC
        read_SPI_byte();
        
        SS_HIGH();
    }
    else
    {
        sent_crc = read_SPI_byte();
        sent_crc <<= 8;
        sent_crc |= read_SPI_byte();
        
        last_crc = sent_crc;
        crc = crc16_ccitt ((uint8_t*)block, block_length);
        
        SS_HIGH();
        
        #ifdef LOWLEVEL_DEBUG
        debug ("Calculated hex: ");
        debughex (crc);
        debug ("\nReceived hex:   ");
        debughex (sent_crc);
        debug ("\n");
        #endif
        
        if (crc != sent_crc)
        {
            return SPI_BAD_CRC;
        }
    }
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Read complete\n");
    #endif
    
    return SPI_OK;
}

ret read_block_crc_only (const uint32_t block_number, uint16_t *crc)
{
    // read the CRC for the block, but don't actually store any block data
    
    // SD cards take the direct address of the block
    // SDHC cards take the block number
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    total_block_accesses++;
    
    #ifdef LOWLEVEL_DEBUG
    if (is_sdhc)
        debug ("SDHC");
    else
        debug ("SD");
    
    debug (" read crc: block number = ");
    debuguint (block_number);
    debug ("\n");
    #endif
    
    uint8_t response = send_SD_command (CMD_READ_BLOCK, (is_sdhc) ? block_number : block_number * (uint32_t)512);
    uint16_t emptyBytes = 0;
    
    uint16_t sent_crc;
    
    SS_LOW();
    while (response == 0xff ||
           response == 0x00)
    {  // waiting for a response
        emptyBytes++;
        
        if (emptyBytes >= READ_BLOCK_TIMEOUT_BYTES)  // we've waited long enough
        {
            #ifdef LOWLEVEL_DEBUG
            debug ("Error: timeout\n");
            #endif
            return SPI_TIMEOUT;  // give up
        }
        
        response = read_SPI_byte();
    }
    SS_HIGH();
    
    if (response != 0xfe)
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Error: response = ");
        debughex (response & (uint16_t)0xff);
        debug ("\n");
        #endif
        return SPI_ERROR;  // the result was an error byte, not a data token
    }
    
    // all clear, begin reading
    SS_LOW();
    
    *crc = 0;
    for (uint16_t i = 0; i < block_length; i++)
    {
        uint8_t byte = read_SPI_byte();
        crc16_by_byte (crc, byte);
    }
    
    if (!crc_enabled)
    {
        read_SPI_byte();  // 16-bit CRC
        read_SPI_byte();
        
        SS_HIGH();
    }
    else
    {
        sent_crc = read_SPI_byte();
        sent_crc <<= 8;
        sent_crc |= read_SPI_byte();
        
        SS_HIGH();
        
        #ifdef LOWLEVEL_DEBUG
        debug ("Calculated hex: ");
        debughex (*crc);
        debug ("\nReceived hex:   ");
        debughex (sent_crc);
        debug ("\n");
        #endif
        
        if (*crc != sent_crc)
        {
            return SPI_BAD_CRC;
        }
    }
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Read complete\n");
    #endif
    
    return SPI_OK;
}

ret write_block (const uint32_t block_number, uint8_t *block)
{
    uint16_t crc;
    
    #ifdef FREE_RAM
    free_ram();
    #endif
    
    total_block_accesses++;
    
    #ifdef LOWLEVEL_DEBUG
    if (is_sdhc)
        debug ("SDHC");
    else
        debug ("SD");
    
    debug (" write, block ");
    debuguint (block_number);
    debug (", data = \n");
    
    for (uint16_t i = 0; i < 512; i++)
    {
        if (i % 16 == 0)
            debug ("\n");
        debughexchar (block[i]);
        debug (" ");
    }
    debug ("\n");
    #endif
    
    if (crc_enabled)
    {
        crc = crc16_ccitt ((uint8_t*)block, block_length);
        
        #ifdef LOWLEVEL_DEBUG
        debug ("Sent CRC: ");
        debughex (crc);
        debug ("\n");
        #endif
    }
    else
    {
        crc = 0xffff;
    }
    
    // SD cards take the direct address of the block
    // SDHC cards take the block number
    uint8_t response = send_SD_command (CMD_WRITE_BLOCK, (is_sdhc) ? block_number : block_number * 512);
    if (response != 0)
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Error: response = ");
        debughex (response & (uint16_t)0xff);
        debug ("\n");
        #endif
        return SPI_ERROR;
    }
    
    SS_LOW();
    read_SPI_byte();
    
    // send the start data token
    write_SPI_byte (0xfe);
    
    // send the data
    for (uint16_t i = 0; i < block_length; i++)
        write_SPI_byte (block[i]);
    
    // send 16-bit CRC
    write_SPI_byte ((crc >> 8) & 0xff);
    write_SPI_byte (crc & 0xff);
    
    // read data response
    response = read_SPI_byte() & 0b00001111;
    
    // continue reading until SD card stops sending a busy signal
    while (read_SPI_byte() != 0xff);
    
    SS_HIGH();
    
    if (response == 0b1101)  // write error
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Write error\n");
        #endif
        return SPI_ERROR;
    }
    if (response == 0b1011)  // CRC error
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Write error (bad CRC)\n");
        #endif
        return SPI_BAD_CRC;
    }
    if (response == 0b0101)  // data accepted
    {
        #ifdef LOWLEVEL_DEBUG
        debug ("Write complete\n");
        #endif
        return SPI_OK;
    }
    
    // the response should be one of those three, so we should never get here
    
    // continue reading until SD card stops sending a busy signal
    SS_LOW();
    while (read_SPI_byte() != 0xff);
    SS_HIGH();
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Write error (unexpected answer)\n");
    #endif
    return SPI_ERROR;
}

void start_spi (enum spi_speed speed)
{
    #ifndef M4
    // AVR
    
    clear (POWER_REDUCTION_REGISTER, PRSPI);  // disable SPI power reduction
    
    set   (DDRB, P_SS);    // set B0 (SS) as output
    set   (DDRB, P_SCLK);  // set B1 (SCLK) as output
    set   (DDRB, P_MOSI);  // set B2 (MOSI) as output
    clear (DDRB, P_MISO);  // set B3 (MISO) as input
    
    clear (PORTB, P_MISO); // disable pull-up resistor on MISO
    set   (PORTB, P_MOSI); // initialize MOSI high
    clear (PORTB, P_SCLK); // initialize SCLK low
    SS_HIGH();             // initialize SS high
    
    switch (speed)
    {
        case SPI_INIT_SPEED:
            // set SPI clock to /64, SPI2X disabled
            // SPI clock is 125kHz at 8MHz (ATmega168/328) or 250kHz at 16Mhz
            // (valid clock speeds for the initial SPI setup are 100-400kHz)
            clear (SPCR, SPR0);
            set   (SPCR, SPR1);
            clear (SPSR, SPI2X);
            break;
        case SPI_MIN_SPEED:
            // set SPI to the lowest speed we deem acceptable
            // SPI clock divider at /16, SPI2x disabled
            // final speed: 500kHz at 8Mhz or 1MHz at 16Mhz
            set   (SPCR, SPR0);
            clear (SPCR, SPR1);
            clear (SPSR, SPI2X);
            break;
        case SPI_LOW_SPEED:
            // set SPI to a low speed: SPI clock divider at /16, SPI2X enabled
            // final speed: 1MHz or 2MHz
            set   (SPCR, SPR0);
            clear (SPCR, SPR1);
            set   (SPSR, SPI2X);
            break;
        case SPI_MED_SPEED:
            // set SPI to a medium speed: SPI clock divider at /4, SPI2X disabled
            // final speed: 2MHz or 4MHz
            clear (SPCR, SPR0);
            clear (SPCR, SPR1);
            clear (SPSR, SPI2X);
            break;
        case SPI_HIGH_SPEED:
            // set SPI to max speed: SPI clock divider at /4, SPI2X enabled
            // final speed: 4MHz or 8MHz
            clear (SPCR, SPR0);
            clear (SPCR, SPR1);
            set   (SPSR, SPI2X);
            break;
    }
    
    set (SPCR, SPE);   // enable SPI
    set (SPCR, MSTR);  // set SPI to master mode
    
    #else
    // M4
    
    static bool pins_initialized = false;
    if (!pins_initialized)
    {  // set up the SPI IO pins
        GPIO_InitTypeDef GPIO_InitStruct;
        
        #ifdef LOWLEVEL_DEBUG
        printf ("Setting up SPI pins and timer\n");
        #endif
        
        // M4 pins (SPI1):
        //    SS: (arbitrary, set in defines at the start of this file)
        //    SCLK: B3
        //    MOSI: B4
        //    MISO: B5
        
        // set SS as general-purpose push-pull output
        GPIO_InitStruct.GPIO_Pin  = SS_GPIO_PIN;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
        GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
      	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_2;
        GPIO_Init (SS_GPIO_PORT, &GPIO_InitStruct);
        
        // set SCLK, MOSI, and MISO as alternate-function pins
        GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
        GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
      	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_2;
        GPIO_Init (GPIOB, &GPIO_InitStruct);
        
        // set up alternate functions
        //   SCLK on B3: AF5
        //   MISO on B4: AF5
        //   MOSI on B5: AF5
        GPIO_PinAFConfig (GPIOB, GPIO_PinSource3, GPIO_AF_5);
        GPIO_PinAFConfig (GPIOB, GPIO_PinSource4, GPIO_AF_5);
        GPIO_PinAFConfig (GPIOB, GPIO_PinSource5, GPIO_AF_5);
        
        // enable the SPI clock
        RCC_APB2PeriphClockCmd (RCC_APB2Periph_SPI1, ENABLE);
        
        SS_HIGH();                     // SS high
        GPIOB->BSRR = GPIO_BSRR_BR_3;  // SCLK low
        GPIOB->BSRR = GPIO_BSRR_BS_5;  // MOSI high
        
        pins_initialized = true;
    }
    
    
    // first make sure SPI is disabled:
    //
    // Wait until FTLVL[1:0] = 00 (no more data to transmit)
    // Wait until BSY=0 (the last data frame is processed)
    // Read data until FRLVL[1:0] = 00 (read all the received data)
    // Disable the SPI (SPE=0).
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Disabling SPI in preparation for speed change\n");
    #endif
    
    while (SPI1->SR & SPI_SR_FTLVL != 0);
    while (SPI1->SR & SPI_SR_BSY != 0);
    while (SPI1->SR & SPI_SR_FRLVL != 0)
        read_from_FIFO();
    SPI1->CR1 &= ~SPI_CR1_SPE;
    
    #ifdef LOWLEVEL_DEBUG
    debug ("Enabling SPI at speed type %d\n", (int)speed);
    #endif
    
    // CR2 16-bit control register: defaults except for...
    //    FRXTH: Set to 1, to generate RXNE events when FIFO contains 8 bits or more
    //    DS (4 bits): Set to 0111, for 8-bit transfers
    SPI1->CR2 = SPI_CR2_FRXTH | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0;
    
    // CR1 16-bit control register: defaults except for...
    //    SSM: Set to 1, so the external NSS pin is ignored
    //    SSI: Set to 1, to indicate that NSS is forced high
    //    SPE: Set to 1, to enable SPI
    //    BR (3 bits): Controls baud rate
    //        000 = PCLK / 2   = 32MHz, probably too fast for cheap SD cards
    //        001 = PCLK / 4   = 16MHz
    //        010 = PCLK / 8   = 8MHz, the fastest we can measure on the Saleae logic analyzer
    //        011 = PCLK / 16  = 4MHz
    //        100 = PCLK / 32  = 2MHz
    //        101 = PCLK / 64  = 1MHz
    //        110 = PCLK / 128 = 512kHz
    //        111 = PCLK / 256 = 256kHz
    //    MSTR: Set to 1
    uint16_t baudRate;
    switch (speed)
    {
        default:
        case SPI_INIT_SPEED:
            // set SPI clock to 256KHz
            baudRate = SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0;
            break;
        case SPI_MIN_SPEED:
            // set SPI clock to 1MHz
            baudRate = SPI_CR1_BR_2 | SPI_CR1_BR_0;
            break;
        case SPI_LOW_SPEED:
            // set SPI to a low speed: 2MHz
            baudRate = SPI_CR1_BR_2;
            break;
        case SPI_MED_SPEED:
            // set SPI to a medium speed: 8MHz
            baudRate = SPI_CR1_BR_1;
            break;
        case SPI_HIGH_SPEED:
            // set SPI to high speed: 16MHz
            baudRate = SPI_CR1_BR_0;
            break;
    }
    
    SPI1->CR1  = SPI_CR1_SSM | SPI_CR1_SSI;  // set the internal NSS signal high first
    SPI1->CR1 |= SPI_CR1_SPE | baudRate | SPI_CR1_MSTR;
    
    #ifdef LOWLEVEL_DEBUG
    debug ("SPI enabled\n");
    #endif
    
    #endif
}




