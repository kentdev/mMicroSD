# Makefile for the mMicroSD M2 test

DEVICE     = atmega32u4
CLOCK      = 16000000

CFLAGS = -Wall -funsigned-bitfields -ffreestanding -mcall-prologues -fpack-struct -fshort-enums -std=gnu99 -Os -DM2

FILES = m_bus.c m_usb.c m_microsd.c test.c

DEFINES = -DF_CPU=$(CLOCK) -DF_CLOCK=$(CLOCK)

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

flash: all
	dfu-programmer atmega32u4 erase
	dfu-programmer atmega32u4 flash main.hex

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

