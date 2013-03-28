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
 * vdisp.c
 *
 * Created: 01.06.2011 08:16:32
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"

#include "vdisp.h"

#include "vdispfont.h"
#include "up_io\lcd.h"



#define MAX_NUM_SCREEN 20

static unsigned char * pixelbuf[MAX_NUM_SCREEN];

// static unsigned char pixelbuf2[1024];

static uint8_t num_screen;


void vdisp_init ( void )
{
	num_screen = 0;
}

int vd_new_screen (void)
{
	if (num_screen >= MAX_NUM_SCREEN)
	{
		return -1;
	}
	
	uint8_t * b = pvPortMalloc( 1024 );
	
	if (b == NULL)
	{
		return -1;
	}
	
	pixelbuf[num_screen] = b;
	
	vd_clear_rect(num_screen, 0,0, 128, 64);
	
	num_screen ++;
	
	return (num_screen - 1);
}


struct vdisp_font vdisp_fonts[4] =
  {
	  { (unsigned char *) vdispfont4x6, 4, 6 },
	  { (unsigned char *) vdispfont5x8, 5, 8 },			  
	  { (unsigned char *) vdispfont6x8, 6, 8 },
	  { (unsigned char *) vdispfont8x12, 8, 12 }
  };




void vdisp_get_pixel ( int x, int y, unsigned char blob[8])
{
	vd_get_pixel(VDISP_MAIN_LAYER, x, y, blob);
}

void vd_get_pixel ( int layer, int x, int y, unsigned char blob[8])
{
	int i;
	int xb = x >> 3;
	
	if (xb >= 16)
		return;
		
	if (y > 56)
		return;
		
	for (i=0; i < 8; i++)
	{
		blob[i] = pixelbuf[layer] [((y + i) << 4) + xb];	
	}
}


void vdisp_set_pixel ( int x, int y, int disp_inverse, unsigned char data, int numbits )
{
	vd_set_pixel(VDISP_MAIN_LAYER, x, y, disp_inverse, data, numbits);
}

void vd_set_pixel ( int layer, int x, int y, int disp_inverse, unsigned char data, int numbits )
{
	int i;
	
	if (layer == lcd_current_layer)
	{
		lcd_update_screen = 1;
	}
	
	unsigned short b = data;
	unsigned short m = 1;
	
	for (i=1; i < numbits; i++)
	{
		m = (m << 1) | 1;
	}
	
	int xb = x >> 3;
	int xa = x & 0x07;
	
	if (xb >= 16)
		return;
		
	if (y >= 64)
		return;
	
	b = b << (16 - numbits - xa);
	m = m << (16 - numbits - xa);
	
	if (disp_inverse != 0)
	{
		b = b ^ m;
	}
	
	pixelbuf[layer] [(y << 4) + xb] &=  ~(m >> 8);
	pixelbuf[layer] [(y << 4) + xb] |=  (b >> 8);
	
	if (((m & 0xff) != 0) && (xb < 15))
	{
		pixelbuf[layer] [(y << 4) + xb + 1] &=  ~(m & 0xff);
		pixelbuf[layer] [(y << 4) + xb + 1] |=  (b & 0xff);
	}
}

void vdisp_printc_xy ( int x, int y, struct vdisp_font * font, int disp_inverse, unsigned char c)
{
	int i;
	
	for (i=0; i < font->height; i++)
	{
		vdisp_set_pixel ( x, y + i, disp_inverse, font->data[ c * font->height + i ], font->width );
	}
}

void vd_printc_xy ( int layer, int x, int y, struct vdisp_font * font, int disp_inverse, unsigned char c)
{
	int i;
	
	for (i=0; i < font->height; i++)
	{
		vd_set_pixel ( layer, x, y + i, disp_inverse, font->data[ c * font->height + i ], font->width );
	}
}

void vdisp_prints_xy ( int x, int y, struct vdisp_font * font, int disp_inverse, const char * s )
{
	int xx = x;
	
	while ( *s != 0 )	
	{
		vdisp_printc_xy( xx, y, font, disp_inverse, ((int) *s) & 0xFF );
		s++;
		xx += font->width;
	}
}

void vd_prints_xy ( int layer, int x, int y, struct vdisp_font * font, int disp_inverse, const char * s )
{
	int xx = x;
	
	while ( *s != 0 )	
	{
		vd_printc_xy( layer, xx, y, font, disp_inverse, ((int) *s) & 0xFF );
		s++;
		xx += font->width;
	}
}

void vdisp_clear_rect(int x, int y, int width, int height)
{
	vd_clear_rect(VDISP_MAIN_LAYER, x, y, width, height);
}

void vd_clear_rect(int layer, int x, int y, int width, int height)
{
	int i;
	int j;
	int k;
	
	for (i=y; i < (y + height); i++)
	{
		k = width;
		j = x;
		
		while (k > 0)
		{
			int bits = (width > 8) ? 8 : width;
			vd_set_pixel( layer, j, i, 0, 0, bits );
			k -= bits;
			j += bits;
		}	
	}
}

void vd_copy_screen (int dst, int src, int y_from, int y_to)
{
	int i;
	
	for (i=y_from*16; i < y_to*16; i++)
	{
		pixelbuf[dst][i] = pixelbuf[src][i];		
	}
}

/*
void vdisp_save_buf(void)
{
	int i;
	
	for (i=36*16; i < 64*16; i++)
	{
		pixelbuf2[i] = pixelbuf[0][i];		
	}
}

void vdisp_load_buf(void)
{
	int i;
	
	for (i=36*16; i < 64*16; i++)
	{
		pixelbuf[0][i] = pixelbuf2[i];		
	}
}
*/

void vdisp_i2s (char * buf, int size, int base, int leading_zero, unsigned int n)
{
	buf[size] = 0;
	
	int i;
	
	for (i=(size-1); i >= 0; i--)
	{
		char c = n % base;
		
		if (c > 9)
		{
			c += 7;
		}
		
		buf[i] = 48 + c;
		n /= base;	
	}
	
	if (leading_zero == 0)
	{
		for (i=0; i < size; i++)
		{
			if ( (buf[i] == '0') && (buf[i+1] != 0))
			{
				buf[i] = ' ';
			}
			else
				break;
		}
	}
}



