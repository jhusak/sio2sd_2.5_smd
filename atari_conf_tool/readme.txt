
SIO2SD commands:

device  command  aux    direction    length   description
 0x72      0      0   SIO2SD->ATARI     1        check
 0x72      1     dno  SIO2SD->ATARI    39+N      get_disk
 0x72      2     dno  ATARI->SIO2SD    39+N      set_disk
 0x72      3     dno      none          0        off_disk
 0x72      4      0   SIO2SD->ATARI    39+N      get_next_entry
 0x72      5      0   ATARI->SIO2SD    39+N      enter_directory
 0x72      6      0       none          0        dir_up

check:
  returns one byte:
  0 - no SD/MMC card, bad format etc.
  1 - ready to use, card not changed
  2 - ready to use, card was changed

get_disk:
  returns info about disk "dno". 1<=dno<=4

set_disk:
  set disk "dno" as ... (bytes form get_disk or get_next_entry)

off_disk:
  turns disk "dno" off

get_next_entry:
  returns info about next entry in current directory. last entry returns zeros

enter_directory:
  enter into given directory

dir_up:
  leaves directory


