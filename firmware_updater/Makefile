MCU = atmega32
#AVRDUDE = avrdude -P usb -c avrisp2 -p $(MCU)

AVRCC = avr-gcc
SHOWSIZE = avr-size
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
OBJS = lcd.o mmc.o main.o

AVRCFLAGS=-g -mmcu=$(MCU) -W -Wall -Wshadow -Wstrict-prototypes -Os -mcall-prologues -mno-interrupts -DFIRMWARE -DF_CPU=14318180ULL

#-------------------
all: sdfirmware.hex sdfirmware.bin sdfirmware.dasm
	$(SHOWSIZE) sdfirmware.out
#-------------------
sdfirmware.dasm : sdfirmware.out
	$(OBJDUMP) -D sdfirmware.out > sdfirmware.dasm
sdfirmware.hex : sdfirmware.out 
	$(OBJCOPY) -S -R .eeprom -O ihex sdfirmware.out sdfirmware.hex 
sdfirmware.bin : sdfirmware.out 
	$(OBJCOPY) -S -R .eeprom -O binary --gap-fill 0xFF --pad-to 0x8000 sdfirmware.out sdfirmware.bin 
sdfirmware.out : $(OBJS)
	$(AVRCC) $(AVRCFLAGS) -o sdfirmware.out -Wl,-Map,sdfirmware.map,--section-start=.text=0x7800 $(OBJS)

#-------------------
lcd.o: ../lcd.c ../delay.h ../cbisbi.h
	$(AVRCC) -c $(AVRCFLAGS) -I.. ../lcd.c -o lcd.o
mmc.o: ../mmc.c ../mmc.h ../delay.h ../cbisbi.h
	$(AVRCC) -c $(AVRCFLAGS) -I.. ../mmc.c -o mmc.o
main.o: main.c ../delay.h ../mmc.h ../lcd.h
	$(AVRCC) -c $(AVRCFLAGS) -I.. main.c -o main.o

#-------------------
clean:
	rm -f *.o *.map *.out
#-------------------
distclean: clean
	rm -f *.dasm *.hex *.bin
