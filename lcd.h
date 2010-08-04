/*
 * SIO2SD
 *
 * Copyright (C) 2005-2010 Jakub Kruszona-Zawadzki
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LCD_H_
#define _LCD_H_

#include <inttypes.h>

#ifndef FIRMWARE
void lcd_init(void);
void lcd_clear_line(uint8_t lno);
void lcd_put_line(uint8_t lno,const uint8_t *buff);
void lcd_put_line_p(uint8_t lno,const uint8_t *buff);
#else
void lcd_send_err(void);
void lcd_send_ok(void);
#endif
void lcd_set_cursor(void);
void lcd_next_hash(void);

#endif
