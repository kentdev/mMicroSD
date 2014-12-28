#------------------------------------------------------------------------------
# MAEVARM M4 STM32F373 
# version: 1.0
# date: February 24, 2013
# author: Neel Shah (neels@seas.upenn.edu)
# description: Makefile for compilation of Library Files
#              Auto detection of OS, tested with OSX, Ubuntu and Windows
#			   Auto find and addition of library source files for compilation 
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# --> DON'T EDIT THIS FILE <--  
#------------------------------------------------------------------------------

BUILDDIR = obj
ARMDIR   = arm
CPALDIR  = cpal
DSPDIR   = dsp
INCDIR   = inc
STDIR    = st
USBDIR   = usb

ARCH = arm-none-eabi
CC   = $(ARCH)-gcc
AR   = $(ARCH)-ar

ifeq ($(OS),Windows_NT)
	UNAME := Windows
	SHELL=c:/windows/system32/cmd.exe
else
    UNAME := $(shell uname -s)
endif

CFLAGS  = -g -Os -Wall -std=c99
CFLAGS += -mlittle-endian -mthumb -mcpu=cortex-m4 
CFLAGS += -mfpu=fpv4-sp-d16 -mfloat-abi=hard 
CFLAGS += -ffunction-sections -fdata-sections 
CFLAGS += -ffreestanding -nostdlib -fno-strict-aliasing
CFLAGS += -I$(ARMDIR) -I$(CPALDIR)/$(INCDIR) -I$(STDIR)/$(INCDIR)
CFLAGS += -I$(USBDIR)/$(INCDIR) -Wno-unused-variable
CFLAGS += -DARM_MATH_CM4 -D'__FPU_PRESENT=1'
#CFLAGS += -DUSE_STDPERIPH_DRIVER
#CFLAGS += -mthumb-interwork

_SRCS  = $(wildcard $(CPALDIR)/*/*.c)
_SRCS += $(wildcard $(DSPDIR)/*/*.c)
_SRCS += $(wildcard $(STDIR)/*/*.c)
_SRCS += $(wildcard $(USBDIR)/*/*.c)
SRCS   = $(notdir $(_SRCS)) 

OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS))

ifeq ($(UNAME),Windows)
OBJSW = $(patsubst %.c,%.o,$(SRCS))
endif

.PHONY: $(BUILDDIR)/libstm32f37x.a

all: begin $(BUILDDIR)/libstm32f37x.a

$(BUILDDIR)/%.o: $(CPALDIR)/*/%.c
	@$(CC) $(CFLAGS) -c -o $@ $^
$(BUILDDIR)/%.o: $(DSPDIR)/*/%.c
	@$(CC) $(CFLAGS) -c -o $@ $^
$(BUILDDIR)/%.o: $(STDIR)/*/%.c
	@$(CC) $(CFLAGS) -c -o $@ $^
$(BUILDDIR)/%.o: $(USBDIR)/*/%.c
	@$(CC) $(CFLAGS) -c -o $@ $^

$(BUILDDIR)/libstm32f37x.a: $(OBJS)
	@$(AR) -rcs $@ $(OBJS)

begin:
ifeq ($(UNAME),Windows)
	@If Not Exist "$(CURDIR)/$(BUILDDIR)" (mkdir "$(CURDIR)/$(BUILDDIR)")
else
	@mkdir -p $(BUILDDIR)
endif
	
clean:
ifeq ($(UNAME),Windows)
	@erase $(OBJSW) libstm32f37x.a /s
else
	@rm -f $(OBJS) $(BUILDDIR)/libstm32f37x.a
endif