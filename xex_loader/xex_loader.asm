;
; SIO2SD
;
; Copyright (C) 2005-2010 Jakub Kruszona-Zawadzki
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

; vim:ts=20:sts=20:sw=20
	opt l+ h-

; some zero-page variables
	org $1f
secsize:
	.ds 1

	org $43

block_start:
	.ds 2
block_end:
	.ds 2
sectors:
	.ds 2
lastsize:
	.ds 1
;next_sector	.ds 2

; default devid can be modified by AVR
	org $700

start:
	dta $00,$03
	dta <start,>start
	dta <run,>run

run:
	ldy #79
print:
	lda text,Y
	bmi print_zero
	cmp #32
	bcs print_nozero
print_zero:
	lda #00
	beq print_store
print_nozero:
	cmp #96
	bcs print_store
	sub #32
print_store:
	sta ($58),Y	; SAVMSC - first byte of the screen memory
	dey
	bpl print

	lda fsize
	and #$7F
	sta lastsize
	lda fsize
	rol
	lda fsize+1
	rol
	sta sectors
	lda fsize+2
	rol
	sta sectors+1

	ldx #$80
	stx secsize

read_block_header:
	jsr getbyte
	sta block_start
	jsr getbyte
	sta block_start+1
	and block_start
	cmp #$FF
	beq read_block_header
	jsr getbyte
	sta block_end
	jsr getbyte
	sta block_end+1

change_byte:
	lda #0			; this 0 is incremented
	bne not_first_block
	inc change_byte+1	; increment 0 above (and then we are only once here)
	lda block_start
	sta $2e0
	lda block_start+1
	sta $2e1

not_first_block:
; set INITAD to rts
	lda #<anyrts
	sta $2e2
	lda #>anyrts
	sta $2e3

	bne get_block_byte	; short JMP - bra

next_block_byte:
	inw block_start

get_block_byte:
	jsr getbyte
	ldy #0
	sta (block_start),Y
	lda block_start
	cmp block_end
	bne next_block_byte
	lda block_start+1
	cmp block_end+1
	bne next_block_byte

	txa
	pha
	jsr do_initad
	pla
	tax

	bne read_block_header ; 	bra - always true

run_prog:
;	ldy #1
;	sty $09
;reboot_init:
;	lda $2e0,Y
;	sta $a,Y
;	sta $c,Y
;	sty $244
;	dey
;	bpl reboot_init
	jmp ($2e0)

error:
	jmp $E474	; WARMSV

; X regiser must be preserved
getbyte:
	cpx secsize
	beq read_next_sector
rbyte:
	lda sector_buffer,X
	inx
anyrts:
	rts

read_next_sector:
	txa	; x is equal to secsize - if secsize is less than $80 (positive) then it was the last sector
	bpl run_prog

	ldx #11
copysio:
	lda siodata,X
	sta $300,X
	dex
	bpl copysio
	jsr $E459	; SIOV
	bmi error

	inc siodata+10
	bne no_high_inc
	inc siodata+11
no_high_inc:
	lda sectors
	bne only_low_dec
	dec sectors+1
only_low_dec:
	dec sectors

	lda sectors+1
	bpl not_last_sector
	lda lastsize
	sta secsize
	beq run_prog
not_last_sector:
	ldx #$00
	beq rbyte

do_initad:
	jmp ($2e2)

siodata:
	dta $31,$01,$52,$40,<sector_buffer,>sector_buffer,$1F,$00,$80,$00,$04,$00

	org $800
text	.ds 80
fsize	.ds 4

	org $800
sector_buffer
	.ds 128
