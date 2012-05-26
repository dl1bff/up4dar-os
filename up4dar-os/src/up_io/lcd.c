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
 * lcd.c
 *
 * Created: 26.05.2012 16:38:39
 *  Author: mdirska
 */ 




#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <asf.h>

#include "board.h"
#include "gpio.h"

#include "up_dstar/vdisp.h"

#include "lcd.h"


#define LCD_PIN_RES		AVR32_PIN_PA02
#define LCD_PIN_E		AVR32_PIN_PB12
#define LCD_PIN_CS1		AVR32_PIN_PB13
#define LCD_PIN_CS2		AVR32_PIN_PB14
#define LCD_PIN_DI		AVR32_PIN_PB21
#define LCD_PIN_RW		AVR32_PIN_PB22



static void lcd_send(int linksrechts, int rs, int data)
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
	
	taskYIELD();
	
	gpio_set_pin_low(LCD_PIN_E);
	
	
	if (linksrechts == 1)
	{
		gpio_set_pin_low(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_CS2);
	}
	
	taskYIELD();
}

static int current_layer = 0;

void lcd_show_layer (int layer)
{
	current_layer = layer;
}

static void vLCDTask( void *pvParameters )
{
	gpio_set_pin_low(LCD_PIN_RES);
	gpio_set_pin_low(LCD_PIN_E);
	gpio_set_pin_low(LCD_PIN_CS1);
	gpio_set_pin_low(LCD_PIN_CS2);
	gpio_set_pin_low(LCD_PIN_RW);
	
	gpio_set_pin_high(AVR32_PIN_PB19); // kontrast
	gpio_set_pin_high(AVR32_PIN_PB18); // backlight
	
	vTaskDelay( 30 );
	
	gpio_set_pin_high(LCD_PIN_RES);
	
	vTaskDelay( 10 );
	
	lcd_send(1, 0, 0x3f);
	lcd_send(2, 0, 0x3f);
	
	
	unsigned char blob[8];
	
	for(;;)
	{
		int x,y,i;
		
		for (x=0; x < 16; x++)
		{
			for (y=0; y < 8; y++)
			{
				
				int r = ((x >= 8) ? 2 : 1);
				lcd_send(r, 0, 0x40 | ((x & 0x07) << 3));
				lcd_send(r, 0, 0xB8 | (y & 0x07));

				vd_get_pixel( current_layer, x << 3, y << 3, blob );
				
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
					
					lcd_send(r, 1, d);

					mask = mask >> 1;					
				}
			}
		}
		
		vTaskDelay( 5 );
	}
	
}



void lcd_init(void)
{
	xTaskCreate( vLCDTask, (signed char *) "LCD", 300, ( void * ) 0,  (tskIDLE_PRIORITY + 1), ( xTaskHandle * ) NULL );
	
}	