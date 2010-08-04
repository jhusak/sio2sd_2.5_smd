all: sio2sd.xex.h sio2sd.xex pajero_sio2sd.xex.h

../tohex/tohex: ../tohex/tohex.c
	$(MAKE) -C ../tohex

sio2sd.xex.h:sio2sd.xex ../tohex/tohex
	../tohex/tohex < sio2sd.xex > sio2sd.xex.h

pajero_sio2sd.xex.h:pajero_sio2sd.xex ../tohex/tohex
	../tohex/tohex < pajero_sio2sd.xex > pajero_sio2sd.xex.h

sio2sd.xex:sio2sd.asm
	mads -o:sio2sd.xex sio2sd.asm

clean:
	rm -f sio2sd.xex

distclean:clean
	rm -f pajero_sio2sd.xex.h sio2sd.xex.h sio2sd.lst
