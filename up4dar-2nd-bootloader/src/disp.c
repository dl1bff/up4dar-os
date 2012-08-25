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
 * disp.c
 *
 * Created: 09.08.2012 11:53:42
 *  Author: mdirska
 */ 

#include <asf.h>

#include "disp.h"

#include "dispfont.h"

#define MAX_NUM_SCREEN 1

static unsigned char * pixelbuf[MAX_NUM_SCREEN];

static unsigned char buf_layer0[1024];

struct disp_font disp_fonts[1] =
{
	{ (unsigned char *) dispfont6x8, 6, 8 }
};


extern unsigned char software_version[];

void disp_init(void)
{
	pixelbuf[0] = buf_layer0;
	
	disp_prints_xy(0, 0, 0, DISP_FONT_6x8, 0, "V: ");
	
	char buf[20];
	
	char image = '?';
	char maturity = 'N';
	
	switch(software_version[0] & 0x0F)
	{
		case 1: 
			image = 'P'; // PHY image
			break;
		case 2:
			image = 'U'; // Updater image
			break;
		case 3:
			image = 'S'; // System image
			break;
	}
	
	switch(software_version[0] & 0xC0)
	{
		case 0x80:
			maturity = 'B';
			break;
		case 0x40:
			maturity = 'E';
			break;
	}
	
	buf[0] = image;
	buf[1] = '-';
	disp_i2s(buf + 2, 1, 10, 1, software_version[1]);
	buf[3] = '.';
	disp_i2s(buf + 4, 2, 10, 1, software_version[2]);
	buf[6] = '.';
	disp_i2s(buf + 7, 2, 10, 1, software_version[3]);
	buf[9] = '-';	
	buf[10] = maturity;
	buf[11] = 0;
	
	disp_prints_xy(0, 18, 0, DISP_FONT_6x8, 0, buf);
	
	disp_printc_xy(0, 122, 14, DISP_FONT_6x8, 0, 30);
	disp_printc_xy(0, 122, 42, DISP_FONT_6x8, 0, 31);
}

void disp_get_pixel ( int layer, int x, int y, unsigned char blob[8])
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

void disp_set_pixel ( int layer, int x, int y, int disp_inverse, unsigned char data, int numbits )
{
	int i;
	
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

void disp_printc_xy ( int layer, int x, int y, struct disp_font * font, int disp_inverse, unsigned char c)
{
	int i;
	
	for (i=0; i < font->height; i++)
	{
		disp_set_pixel ( layer, x, y + i, disp_inverse, font->data[ c * font->height + i ], font->width );
	}
}

void disp_prints_xy ( int layer, int x, int y, struct disp_font * font, int disp_inverse, const char * s )
{
	int xx = x;
	
	while ( *s != 0 )
	{
		disp_printc_xy( layer, xx, y, font, disp_inverse, ((int) *s) & 0xFF );
		s++;
		xx += font->width;
	}
}

void disp_clear_rect(int layer, int x, int y, int width, int height)
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
			disp_set_pixel( layer, j, i, 0, 0, bits );
			k -= bits;
			j += bits;
		}
	}
}

void disp_i2s (char * buf, int size, int base, int leading_zero, unsigned int n)
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


#define LCD_PIN_RES		AVR32_PIN_PA02
#define LCD_PIN_E		AVR32_PIN_PB12
#define LCD_PIN_CS1		AVR32_PIN_PB13
#define LCD_PIN_CS2		AVR32_PIN_PB14
#define LCD_PIN_DI		AVR32_PIN_PB21
#define LCD_PIN_RW		AVR32_PIN_PB22



static void lcd_send( void (*idle_proc) (void), int linksrechts, int rs, int data )
{
	// uint32_t d = data << 24;
	
	uint32_t d = 0;
	int i;
	
	if (rs != 0)
	{
		gpio_set_pin_high(LCD_PIN_DI);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_DI);
	}
	
	if (linksrechts == 1)
	{
		gpio_set_pin_high(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_high(LCD_PIN_CS2);
	}
	
	for (i=0; i < 8; i++)
	{
		if ((data & 1) != 0)
		{
			d |= (1 << 23);
		}
		
		d = d << 1;
		data = data >> 1;
	}
	
	
	gpio_set_group_high(1 /* PORT B */, d);
	gpio_set_group_low(1 /* PORT B */, d ^ 0xFF000000);
	
	
	gpio_set_pin_high(LCD_PIN_E);
	
	idle_proc();
	
	gpio_set_pin_low(LCD_PIN_E);
	
	
	if (linksrechts == 1)
	{
		gpio_set_pin_low(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_CS2);
	}
	
	idle_proc();
}



void disp_main_loop( void (*idle_proc) (void) )
{
	gpio_set_pin_low(LCD_PIN_RES);
	gpio_set_pin_low(LCD_PIN_E);
	gpio_set_pin_low(LCD_PIN_CS1);
	gpio_set_pin_low(LCD_PIN_CS2);
	gpio_set_pin_low(LCD_PIN_RW);
	
	// gpio_set_pin_high(AVR32_PIN_PB19); // kontrast
	// gpio_set_pin_high(AVR32_PIN_PB18); // backlight
	
	int i;
	
	for (i=0; i < 3000; i++)
	{
		idle_proc();
	}
	
	gpio_set_pin_high(LCD_PIN_RES);
	
	for (i=0; i < 1000; i++)
	{
		idle_proc();
	}
	
	lcd_send(idle_proc, 1, 0, 0x3f);
	lcd_send(idle_proc, 2, 0, 0x3f);
	
	
	unsigned char blob[8];
	
	for(;;)
	{
		int x,y,i;
		
		for (x=0; x < 16; x++)
		{
			for (y=0; y < 8; y++)
			{
				
				int r = ((x >= 8) ? 2 : 1);
				lcd_send(idle_proc, r, 0, 0x40 | ((x & 0x07) << 3));
				lcd_send(idle_proc, r, 0, 0xB8 | (y & 0x07));

				disp_get_pixel( 0, x << 3, y << 3, blob );
				
				int mask = 0x80;
				
				for (i=0; i < 8; i++)
				{
					int m = 1;
					int d = 0;
					int j;
					
					for (j=0; j < 8; j++)
					{
						if ((blob[j] & mask) != 0)
						{
							d |= m;
						}
						m = m << 1;
					}
					
					lcd_send(idle_proc, r, 1, d);

					mask = mask >> 1;
				}
			}
		}
	}
	
}
