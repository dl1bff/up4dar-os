/*

Copyright (C) 2011,2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * vdisp.h
 *
 * Created: 01.06.2011 08:16:50
 *  Author: mdirska
 */ 


#ifndef VDISP_H_
#define VDISP_H_


struct vdisp_font
  {
	 unsigned char * data;
	 int width;
	 int height;
  };
  
  
void vdisp_init ( void );

void vdisp_prints_xy ( int x, int y, struct vdisp_font * font, int disp_inverse, const char * s );
void vdisp_clear_rect(int x, int y, int width, int height);
void vdisp_printc_xy ( int x, int y, struct vdisp_font * font, int disp_inverse, unsigned char c);
void vdisp_set_pixel ( int x, int y, int disp_inverse, unsigned char data, int numbits );
// void vdisp_save_buf(void);
// void vdisp_load_buf(void);
void vdisp_i2s (char * buf, int size, int base, int leading_zero, unsigned int n);

void vdisp_get_pixel ( int x, int y, unsigned char blob[8]);
void vd_get_pixel ( int layer, int x, int y, unsigned char blob[8]);
void vd_set_pixel ( int layer, int x, int y, int disp_inverse, unsigned char data, int numbits );
void vd_printc_xy ( int layer, int x, int y, struct vdisp_font * font, int disp_inverse, unsigned char c);
void vd_prints_xy ( int layer, int x, int y, struct vdisp_font * font, int disp_inverse, const char * s );
void vd_prints_xy_inverse ( int layer, int x, int y, struct vdisp_font * font, int disp_inverse, const char * s );
void vd_clear_rect(int layer, int x, int y, int width, int height);
int vd_new_screen (void);
void vd_copy_screen (int dst, int src, int y_from, int y_to);
extern struct vdisp_font vdisp_fonts[];



#define VDISP_FONT_4x6		(vdisp_fonts + 0)
#define VDISP_FONT_5x8		(vdisp_fonts + 1)
#define VDISP_FONT_6x8		(vdisp_fonts + 2)
#define VDISP_FONT_8x12		(vdisp_fonts + 3)

#define VDISP_CURRENT_LAYER -1

#define VDISP_MAIN_LAYER 0
#define VDISP_GPS_LAYER 1
#define VDISP_REF_LAYER 2
#define VDISP_DEBUG_LAYER 3
#define VDISP_SAVE_LAYER 4
#define VDISP_AUDIO_LAYER 5
#define VDISP_DVSET_LAYER 6
#define VDISP_RMUSET_LAYER 7

#endif /* VDISP_H_ */
