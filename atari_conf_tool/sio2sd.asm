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
	opt r+ l+

; some zero-page variables
	org $80

scrptr	.ds 2
dataptr	.ds 2
iocbch	.ds 1
ncnt	.ds 1
nend	.ds 1
;ccnt	.ds 1
diskno	.ds 1
devid	.ds 1
chk	.ds 1
status	.ds 1

; default devid can be modified by AVR
	org $2b00
defaultdevid	dta 0

	org $2c00
fnt	.ds $400

empty	.ds 48
hline	.ds 48

offdisk	.ds 39

dscr	.ds 48*8
escr	.ds 48*19

ddata	.ds 54*8
edata	.ds 54*19

ch23	dta $00,$6c,$fe,$6c,$6c,$fe,$6c,$00
ch2a	dta $00,$6c,$38,$fe,$38,$6c,$00,$00
ch60	dta $00,$30,$18,$00,$00,$00,$00,$00
ch7b	dta $18,$30,$30,$60,$30,$30,$18,$00
ch7d	dta $60,$30,$30,$18,$30,$30,$60,$00
ch7e	dta $00,$00,$76,$dc,$00,$00,$00,$00
ch7f	dta $fe,$c6,$c6,$c6,$c6,$c6,$fe,$00

helpdl	:5 dta $70
	dta $4e,a(hline)
	dta $30
	dta $47,a(helpscr)
	dta $30
	dta $4e,a(hline)
	dta $70
	dta $4e,a(hline)
	dta $30
	dta $46,a(helpscr+24)
	:8 dta $30,6
	dta $30
	dta $4e,a(hline)
	dta $41,a(helpdl)

helpscr	dta d'   SIO2SD HELP SCREEN   '*
	dta d'  SH+1-4'*,d' CHOOSE SIO2SD  '
	dta d'     1-8'*,d' CHOOSE DRIVE   '
	dta d'     A-S'*,d' GO ENTRY       '
	dta d'       U'*,d' UPPER DIR      '
	dta d'       -'*,d' DRIVE OFF      '
	dta d'   SPACE'*,d' NEXT PAGE      '
	dta d'       V'*,d' SWAP D1/D2     '
	dta d'       X'*,d' REBOOT         '
	dta d'       Z'*,d' EXIT           '

dl	dta $0
	dta $4e,a(hline),$4f,a(empty)
	dta $42,a(title)
	dta $4f,a(empty),$4e,a(hline)
	dta $0
	dta $4e,a(hline),$4f,a(empty)
	dta $42,a(dscr)
	:7 dta 2
	dta $4f,a(empty),$4e,a(hline)
	dta $0
	dta $4e,a(hline),$4f,a(empty)
	dta $42,a(escr)
	:18 dta $2
	dta $4f,a(empty),$4e,a(hline)
	dta $41,a(dl)

title	dta c'       SIO2SD CONFIG (DEV:0) - Press HELP       '
titledevid	equ title+26
;title	dta c'                  SIO2SD CONFIG                 '
;	      012345678901234567890123456789012345678
;offdisk	dta c' - OFF -                               '
off_disk_txt	dta c'- OFF -'
no_card_txt	dta c'NO CARD'
sio_error_txt	dta c'SIO ERROR'

siov	equ $e459
ciov	equ $e456
kdev	dta c'K:',$9b

freechannel	ldx #$10
nextchannel	lda $340,x
	cmp #$ff
	beq foundchannel
	txa
	add #$10
	tax
	bpl nextchannel
	ldx #$70	;not found, so close #7 and use it
	mva #$0c $342,x
	jsr ciov
	ldx #$70
foundchannel	stx iocbch
	rts

opendev	ldx iocbch
	mva #$03 $342,x	; open
	mwa #kdev $344,x
	mva #4 $34a,x
	mva #0 $34b,x
	jmp ciov 

getch	ldx iocbch
	mva #$07 $342,x	; get char
	jmp ciov

closedev	ldx iocbch
	mva #$0c $342,x
	jmp ciov

gohelp	mwa #helpdl $230
	mwa #$08 710
	mva #$e0 $2f4
;	mva #0 732
help_loop	lda $d209
	cmp #17
	bne help_exit
	lda $d20f
	and #4
	beq help_loop
;help_loop	lda 732
;	cmp #17
;	beq help_exit
;	lda $2fc
;	cmp #255
;	beq help_loop
;	jsr getch ; get key
help_exit;	mva #0 732
	mwa #dl $230
	mwa #$80 710
	mva #>fnt $2f4
	rts

showdiskno	mwa #dscr scrptr
	ldx #1
	ldy #3
shdisk	txa
	cmp diskno
	bne shdiskneg
	ora #128
shdiskneg	add #'0'
	sta (scrptr),y
	iny
	lda #'.'
	sta (scrptr),y
	adw scrptr #47
	inx
	cpx #9
	bne shdisk
	rts

sio2sd	mva #$70 $300
	lda devid
	add #3
	sta $301
	mva #$7F $306
	mva #$0 $307
	jmp siov

check	mva #$ff chk
	mva #0 $41
	mva #0 $302	; cmd
	mva #$40 $303	; input
	mwa #chk $304
	mwa #1 $308	; 1 byte
	jsr sio2sd
	mva #8 $41
	rts

get_disks	mva #1 $302	; cmd
	mva #$40 $303	; input
	mwa #ddata $304
	mwa #54*4 $308
	mva #1 $30A
	mva #4 $30B
	jsr sio2sd
	mva #1 $302
	mva #$40 $303
	mwa #ddata+54*4 $304
	mwa #54*4 $308
	mva #5 $30A
	mva #4 $30B
	jmp sio2sd

set_disk	mva #2 $302
	mva #$80 $303
	mwa dataptr $304
	mwa #54 $308
	mva diskno $30A
	mva #1 $30B
	jmp sio2sd

off_disk	mva #3 $302	; cmd
	mva #0 $303	; no transfer
	mva diskno $30A
	mva #1 $30B
	jmp sio2sd

get_next	mwa #edata dataptr
	mwa #escr+6 scrptr
	mva #0 ncnt
	mva #0 nend
get_next_loop	lda nend
	bne get_next_empty
	lda ncnt
	and #3
	bne get_next_testentry
	mva #4 $302
	mva #$40 $303
	mwa dataptr $304
	lda ncnt
	cmp #16
	beq get_next_lastget
	mwa #54*4 $308
	mva #4 $30A
	bne get_next_sio
get_next_lastget	mwa #54*3 $308
	mva #3 $30A
get_next_sio	mva #0 $30B
	jsr sio2sd
get_next_testentry	ldy #0
	lda (dataptr),y
	bne get_next_gotdata
	mva #1 nend
	bne get_next_gotdata
get_next_empty	ldy #53
	lda #0
get_next_empty_nch	sta (dataptr),y
	dey
	bpl get_next_empty_nch
get_next_gotdata	ldy #39
	lda (dataptr),y
	beq get_next_emptyline
	cmp #1
	beq get_next_negative
	dey
get_next_pos_nch	lda (dataptr),y
	sta (scrptr),y
	dey
	bpl get_next_pos_nch
	bmi get_next_finalize
get_next_negative	dey
get_next_neg_nch	lda (dataptr),y
	ora #128
	sta (scrptr),y
	dey
	bpl get_next_neg_nch
	bmi get_next_finalize
get_next_emptyline	dey
	lda #$20
get_next_el_nch	sta (scrptr),y
	dey
	bpl get_next_el_nch
get_next_finalize	inc ncnt
	adw dataptr #54
	adw scrptr #48
	lda ncnt
	cmp #19
	jne get_next_loop
	rts

enter_dir	mva #5 $302
	mva #$80 $303
	mwa dataptr $304
	mwa #54 $308
	mva #0 $30A
	mva #0 $30B
	jmp sio2sd

dir_up	mva #6 $302
	mva #0 $303
	mva #0 $30A
	mva #0 $30B
	jmp sio2sd

show_disks	mwa #ddata dataptr
	mwa #dscr+6 scrptr
	ldx #8
show_next_disk	ldy #0
	lda (dataptr),y
	beq show_off_disk
	ldy #38
show_name_nextch	lda (dataptr),y
	sta (scrptr),y
	dey
	bpl show_name_nextch
	bmi show_disks_endloop
show_off_disk	ldy #38
show_off_nextch	lda offdisk,y
	sta (scrptr),y
	dey
	bpl show_off_nextch
show_disks_endloop	adw scrptr #48
	adw dataptr #54
	dex
	bne show_next_disk
	rts

clr_screen	mwa #dscr+6 scrptr
	ldx #8+19
clr_next_line	ldy #38
	lda #$20
clr_next_char	sta (scrptr),y
	dey
	bpl clr_next_char
	adw scrptr #48
	dex
	bne clr_next_line	
	rts

sio_error_msg	jsr clr_screen
	mwa #escr+4*48+20 scrptr
	mwa #sio_error_txt dataptr
	ldy #8
	jmp print_msg

no_card_msg	jsr clr_screen
	mwa #escr+4*48+21 scrptr
	mwa #no_card_txt dataptr
	ldy #6
;	jmp print_msg

print_msg	lda (dataptr),y
	sta (scrptr),y
	dey
	bpl print_msg
	rts

main
	ldy #0
fntcpy	lda #0
	sta fnt,y
	lda $e000,y
	sta fnt+$100,y
	lda $e100,y
	sta fnt+$200,y
	lda $e300,y
	sta fnt+$300,y
	iny
	bne fntcpy

	ldy #7
chcpy	lda ch23,y
	sta fnt+($23*8),y
	lda ch2a,y
	sta fnt+($2a*8),y
	lda ch60,y
	sta fnt+($60*8),y
	lda ch7b,y
	sta fnt+($7b*8),y
	lda ch7d,y
	sta fnt+($7d*8),y
	lda ch7e,y
	sta fnt+($7e*8),y
	lda ch7f,y
	sta fnt+($7f*8),y
	dey
	bpl chcpy

	ldy #47
initspacers	lda #0
	sta empty,y
	lda #$55
	sta hline,y
	dey
	bpl initspacers

	ldy #38
	lda #0
clroffdisk	sta offdisk,y
	dey
	bpl clroffdisk
	ldy #6
cpyoffdisk	lda off_disk_txt,y
	sta offdisk+1,y
	dey
	bpl cpyoffdisk

	jsr freechannel
	jsr opendev

	mwa #dl $230
	mva #>fnt $2f4
	mva #1 $26f
	mva #212 $d000
	mva #3 $d008
	mva #$ff $d00d
	mva #%00100011 $22F
	mva #$0 704
	mva #$0F 708
	mva #$0A 709
	mva #$80 710
	mva #0 712
;	jsr setint

	mva #1 diskno
	jsr showdiskno

	mwa #escr scrptr
	ldx #'A'
	ldy #3
pkeys	txa
	sta (scrptr),y
	iny
	lda #'.'
	sta (scrptr),y
;	dey
;	adw scrptr #48
	adw scrptr #47
	inx
	cpx #'T'
	bne pkeys

	mva defaultdevid devid

newdevid
	add #'0'
	sta titledevid

	jsr check
	lda chk
	cmp #$ff
	bne firstcheck_no_error
	sta status
	jsr sio_error_msg
	jmp loop
firstcheck_no_error	cmp #0
	bne firstcheck_have_card
	sta status
	jsr no_card_msg
	jmp loop
firstcheck_have_card
	mva #1 status
	jsr get_disks
	jsr show_disks
	jsr get_next
preloop	mva #0 732

loop	lda $d209
	cmp #17
	bne nohelp
	lda $d20f
	and #4
	bne nohelp
;loop	lda 732
;	cmp #17
;	bne nohelp
	jsr gohelp
	jmp loop

nohelp	lda $2fc
	cmp #255
	jeq loop_nokey
; key pressed
	jsr getch	; get key
	cmp #'A'
	bmi nocapitalalpha
	cmp #'Z'+1
	bpl nocapitalalpha
	add #'a'-'A'
nocapitalalpha
;	sta title+3

	cmp #'z'
	bne noexit
	jsr closedev
	mva #'E' kdev	; reset screen
	jsr opendev
	jsr closedev
	rts
noexit
	cmp #'x'
	bne noreboot
	mva #1 580
	jsr $e477	
noreboot
	cmp #'!'
	bmi nodevid
	cmp #'%'
	bpl nodevid
	sub #'!'
	sta devid
	jmp newdevid
nodevid
	cmp #'1'
	bmi nodigit
	cmp #'9'
	bpl nodigit
	sub #'0'
	sta diskno
	jsr showdiskno
nodigit
	pha
	lda status
	cmp #1
	beq keys_status_ok
	pla
	jmp loop
keys_status_ok	pla
	cmp #'-'
	bne nooffdisk
	jsr off_disk
	jmp loop
nooffdisk
	cmp #'u'
	bne nodirup
	jsr dir_up
	jsr get_next
	jmp loop
nodirup
	cmp #'v'
	bne noswap
	lda diskno
	pha
	mwa #ddata dataptr
	mva #2 diskno
	ldy #39
	lda (dataptr),y
	beq swapoff1
	jsr set_disk
	jmp swapnext
swapoff1	jsr off_disk
swapnext	mwa #ddata+54 dataptr
	mva #1 diskno
	ldy #39
	lda (dataptr),y
	beq swapoff2
	jsr set_disk
	jmp swapend
swapoff2	jsr off_disk
swapend	pla
	sta diskno
	jmp loop
noswap
	cmp #' '
	bne nogetnext
	jsr get_next
	jmp loop
nogetnext
	cmp #'a'
	bmi noalpha
	cmp #'t'
	bpl noalpha
	sub #'a'
	tax
	mwa #edata dataptr
loop_alpha_next	cpx #0
	beq loop_alpha_ptrok
	dex
	adw dataptr #54
	jmp loop_alpha_next
loop_alpha_ptrok
	ldy #39
	lda (dataptr),y
	jeq loop
	cmp #1
	beq loop_enter_dir
	jsr set_disk
	jmp loop
loop_enter_dir	jsr enter_dir
	jsr get_next
noalpha
	jmp loop

loop_nokey	lda $14
	and #$0f
	jne loop
	jsr check
	lda chk
	cmp #$ff
	bne loop_check_no_error
	cmp status
	jeq loop
	sta status
	jsr sio_error_msg
	jmp loop
loop_check_no_error	cmp #0
	bne loop_check_have_card
	cmp status
	jeq loop
	sta status
	jsr no_card_msg
	jmp loop
loop_check_have_card
	cmp #1
	bne loop_refresh_disks
	lda status
	cmp #1
	jeq loop
loop_refresh_disks
	mva #1 status
	jsr get_disks
	jsr show_disks
	lda chk
	cmp #2
	jne loop
	jsr get_next
	jmp loop

	run main
