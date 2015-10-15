PROG=goodfetuino
CFILES = uart.c goodfet/jtag.c goodfet/jtag430.c goodfet/jtag430x2.c

MCU=-mmcu=atmega328p
CPU_SPEED=-DF_CPU=16000000UL

PORT=/dev/ttyUSB0

OBJS=$(CFILES:.c=.o)
LIBS=-Wl,-Map=$(PROG).map

CC=avr-gcc
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump

DEFS=$(CPU_SPEED)  #-DUART_TX_BUFFER_SIZE=16
    
CFLAGS=-Os -Wall $(MCU) $(DEFS) \
    -Igoodfet \
    -Wl,--gc-sections -ffunction-sections -fdata-sections -mcall-prologues -Wl,--relax

all: $(PROG).hex

$(PROG).hex : $(PROG).elf
	avr-size $(PROG).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@ 
	$(OBJDUMP) -h -S $<  > $(PROG).lst

$(PROG).elf: $(PROG).c $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $< $(CFLAGS) $(INCLUDE) -c -o $@

clean:
	rm -f $(PROG).hex $(PROG).elf $(PROG).map $(PROG).lst *.o *~ goodfet/*.o

upload:
	avrdude -p m328p -c arduino -P $(PORT) -b 57600 -U flash:w:$(PROG).hex


