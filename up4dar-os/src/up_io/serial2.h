/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


/*
 * serial2.h
 *
 * Created: 17.01.2013 17:02:35
 *  Author: mdirska
 */ 


#ifndef SERIAL2_H_
#define SERIAL2_H_

extern int serial_timeout_error;
extern int serial_putc_q_full;
extern int serial_rx_error;
extern int serial_rx_ok;

int serial_init ( int usartNum, int baudrate );
int serial_putc ( int usartNum, char cOutChar );
int serial_stop ( int usartNum );
int serial_getc ( int usartNum, char * cOutChar );
int serial_rx_char_available (int usartNum);
void serial_putc_tmo (int comPort, char c, short timeout);

// int serial_puts (int usartNum, const char * s);

#endif /* SERIAL2_H_ */