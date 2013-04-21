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



static int lcd_recv(int linksrechts, int rs)
{
	uint32_t data;
	
	int i;
	int d;
	
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
	
	gpio_set_pin_high(LCD_PIN_RW);
	
	gpio_configure_group(1 /* PORT B */, 0xFF000000, GPIO_DIR_INPUT | GPIO_PULL_UP);
		// data pins are now input pins with pull-up
	
	gpio_set_pin_high(LCD_PIN_E);
	
	taskYIELD();
	
	data = AVR32_GPIO.port[1 /* PORT B */].pvr;  // read PORT B
	
	gpio_set_pin_low(LCD_PIN_E);
	
	gpio_set_pin_low(LCD_PIN_RW);
	
	if (linksrechts == 1)
	{
		gpio_set_pin_low(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_CS2);
	}
	
	d = 0;
	
	for (i=0; i < 8; i++)
	{
		d = d << 1;
		
		if ((data & (1 << 24)) != 0)
		{
			d |= 1;
		}
		
		data = data >> 1;
	}
	
	gpio_configure_group(1 /* PORT B */, 0xFF000000, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
		// data pins are now output pins
	
	taskYIELD();
	
	return d;
}

#define LCD_CHECK_INTERVAL	110

char lcd_current_layer = 0;
char lcd_update_screen = 1;
char lcd_init_screen_counter = LCD_CHECK_INTERVAL;

static uint8_t display_layer[128];

void lcd_show_layer (int layer)
{
	lcd_current_layer = layer;
	
	int i;
	
	for (i=0; i < 128; i++)
	{
		display_layer[i] = lcd_current_layer;
	}
	
	lcd_update_screen = 1;
}

// static const uint8_t help_lines[2] = { 1, 2 };

void lcd_show_help_layer(int help_layer)
{
	if (help_layer == 0) // turn off help_layer
	{
		lcd_show_layer(lcd_current_layer);
		return;
	}
	
	int i;
	/* int j;
	
	for (j=0; j < ((sizeof help_lines) / (sizeof help_lines[0])); j++)
	{		
		for (i=help_lines[j]*16 + 11; i < ((help_lines[j]+1)*16); i++ )
		{
			display_layer[i] = help_layer;
		}
	} */
	
	for (i=0; i < 6; i++)
	{
		display_layer[i] = help_layer;
	}	
	
	for (i=7*16; i < 128; i++)
	{
		display_layer[i] = help_layer;
	}
	
	lcd_update_screen = 1;
}

void lcd_set_backlight (int v)
{
	AVR32_PWM.channel[6].cdty = 1000 - (v * 10);  // v = 0..100
}

void lcd_set_contrast (int v)
{
	AVR32_PWM.channel[0].cdty = v * 10;  // v = 0..100
}


static void lcd_reset (void)
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
	
	lcd_send(1, 0, 0x3f); // switch display on
	lcd_send(2, 0, 0x3f);
}

static void vLCDTask( void *pvParameters )
{
	
	lcd_reset();
	
	unsigned char blob[8];
	
	for(;;)
	{
		int x,y,i;
		
		if (lcd_update_screen != 0)
		{
		lcd_update_screen = 0;	
		
		for (x=0; x < 16; x++)
		{
			for (y=0; y < 8; y++)
			{
				
				int r = ((x >= 8) ? 2 : 1);
				lcd_send(r, 0, 0x40 | ((x & 0x07) << 3));
				lcd_send(r, 0, 0xB8 | (y & 0x07));

				vd_get_pixel( display_layer[(y << 4) | x], x << 3, y << 3, blob );
				
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
		} // if (lcd_update_screen != 0)		
		
		vTaskDelay( 5 );
		
		lcd_init_screen_counter --;
		
		if (lcd_init_screen_counter <= 0)
		{
			lcd_init_screen_counter = LCD_CHECK_INTERVAL;
			
			/* char buf[3];
			
			vdisp_i2s(buf, 2, 16, 1, lcd_recv(1, 0));
			vd_prints_xy(VDISP_DEBUG_LAYER, 0, 56, VDISP_FONT_6x8, 0, buf);
			*/
			
			if ((lcd_recv(1, 0) != 0x00) || (lcd_recv(2, 0) != 0x00))
			{
				lcd_reset();
			}
		}
		
	}
	
}



void lcd_init(void)
{
	xTaskCreate( vLCDTask, (signed char *) "LCD", 300, ( void * ) 0,  (tskIDLE_PRIORITY + 1), ( xTaskHandle * ) NULL );
	
}	