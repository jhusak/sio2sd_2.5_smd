all: xex_loader.bin.h

../tohex/tohex: ../tohex/tohex.c
	$(MAKE) -C ../tohex

xex_loader.bin.h:xex_loader.bin ../tohex/tohex
	../tohex/tohex < xex_loader.bin > xex_loader.bin.h

xex_loader.bin:xex_loader.asm
	mads -o:xex_loader.bin xex_loader.asm

clean:
	rm -f xex_loader.bin

distclean:clean
	rm -f xex_loader.bin.h xex_loader.lst
