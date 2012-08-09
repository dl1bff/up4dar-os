/*

Copyright (C) 2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * disp.h
 *
 * Created: 09.08.2012 11:55:10
 *  Author: mdirska
 */ 


#ifndef DISP_H_
#define DISP_H_



struct disp_font
{
	unsigned char * data;
	int width;
	int height;
};

void disp_init(void);
void disp_get_pixel ( int layer, int x, int y, unsigned char blob[8]);
void disp_set_pixel ( int layer, int x, int y, int disp_inverse, unsigned char data, int numbits );
void disp_printc_xy ( int layer, int x, int y, struct disp_font * font, int disp_inverse, unsigned char c);
void disp_prints_xy ( int layer, int x, int y, struct disp_font * font, int disp_inverse, const char * s );
void disp_clear_rect(int layer, int x, int y, int width, int height);
void disp_i2s (char * buf, int size, int base, int leading_zero, unsigned int n);
void disp_main_loop( void (*idle_proc) (void) );



extern struct disp_font disp_fonts[];

#define DISP_FONT_6x8		(disp_fonts + 0)

#endif /* DISP_H_ */
