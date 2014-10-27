#*******************************************************************************
# mMicroSD makefile
# version: 1.0
# date: April 16, 2013
# author: Kent deVillafranca (kent@kentdev.net)
# description: Makefile for the mMicroSD peripheral (I2C slave which accesses a
#              microSD card over SPI).
#******************************************************************************/

# device is:
#   atmega32u4 for the M2
#   atmega168, atmega328, or atmega328p for the chip on the final board
#   the M4 is not supported for use as an I2C slave peripheral, because why would you do that?
DEVICE     = atmega328
CLOCK      = 8000000

ifeq (atmega168,$(DEVICE))
    AVRDUDE_MCU := m168
    DEVICEDEF := -DATMEGA168
endif
ifeq (atmega328,$(DEVICE))
    AVRDUDE_MCU := m328
    DEVICEDEF := -DATMEGA328
endif
ifeq (atmega328p,$(DEVICE))
    AVRDUDE_MCU := m328p
    DEVICEDEF := -DATMEGA328
endif
ifeq (atmega32u4,$(DEVICE))
    DEVICEDEF := -DM2
endif


CFLAGS = -Wall -funsigned-bitfields -ffreestanding -mcall-prologues -fshort-enums -std=gnu99 -Os

FILES = crc.c sd_lowlevel.c sd_highlevel.c sd_highlevel_cache.c sd_fat32.c m_microsd.c

DEFINES = -DF_CPU=$(CLOCK) -DF_CLOCK=$(CLOCK) $(DEVICEDEF)

# Additional defines:
#   -DLOWLEVEL_DEBUG     Print out debugging information for low-level card operations
#   -DHIGHLEVEL_DEBUG    Print out debugging information for high-level card operations
#   -DFAT32_DEBUG        Print out debugging information for filesystem operations
#   -DVERIFY_WRITE       Re-read blocks that have just been written to ensure that the new data
#                        has actually been written (apparently checking that the card doesn't
#                        return a CRC error isn't enough; I've encountered at least one card
#                        that would occasionally report that a block had been successfully
#                        written when it really hadn't)
#   -DFREE_RAM           Periodically print out how much unused RAM is left
#
# All defines except for VERIFY_WRITE make the code pull in the m_usb library, and
# won't work on the ATmega168 or ATmega328

DEFINES += -DVERIFY_WRITE

USE_USB = 
ifneq (,$(findstring DLOWLEVEL_DEBUG,$(DEFINES)))
    USE_USB = YES
endif
ifneq (,$(findstring DHIGHLEVEL_DEBUG,$(DEFINES)))
    USE_USB = YES
endif
ifneq (,$(findstring DFAT32_DEBUG,$(DEFINES)))
    USE_USB = YES
endif
ifneq (,$(findstring DFREE_RAM,$(DEFINES)))
    USE_USB = YES
endif

ifneq (,$(USE_USB))
FILES := m_bus.c m_usb.c $(FILES)
ifneq (atmega32u4,$(DEVICE))
$(error Cannot use USB debugging unless programming on the M2)
endif
endif

COMPILE = avr-gcc $(CFLAGS) $(DEFINES) -mmcu=$(DEVICE)
LINK = $(COMPILE)

# symbolic targets: 
all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

install: flash 

info: all
	avr-size -C --mcu=$(DEVICE) main.elf

ifeq (atmega32u4,$(DEVICE))
flash: all
	dfu-programmer atmega32u4 erase
	dfu-programmer atmega32u4 flash main.hex
else
flash: all
	avrdude -c avrispmkii -p $(AVRDUDE_MCU) -P usb -e -U flash:w:main.hex
endif

clean:
	rm -f main.hex main.elf $(MAIN) $(CHILDREN)

# file targets:
main.elf: $(FILES)
	$(COMPILE) $^ -o $@

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c

