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

#include <asf.h>
#include <board.h>
#include <conf_board.h>

#include "gpio.h"
#include "power_clocks_lib.h"

#include "FreeRTOS.h"




static const gpio_map_t lcd_gpio_map =
{
	{ AVR32_PIN_PA02, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// RES
	{ AVR32_PIN_PB12, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // E
	{ AVR32_PIN_PB13, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// CS1
	{ AVR32_PIN_PB14, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // CS2
	{ AVR32_PIN_PB21, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D/I
	{ AVR32_PIN_PB22, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // R/W	
	{ AVR32_PIN_PB24, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D0
	{ AVR32_PIN_PB25, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D1	
	{ AVR32_PIN_PB26, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D2
	{ AVR32_PIN_PB27, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D3	
	{ AVR32_PIN_PB28, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D4
	{ AVR32_PIN_PB29, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D5	
	{ AVR32_PIN_PB30, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D6
	{ AVR32_PIN_PB31, GPIO_DIR_OUTPUT | GPIO_INIT_LOW }   // D7
};

static const gpio_map_t lcd_pwm_gpio_map =
{
	{ AVR32_PWM_6_PIN, AVR32_PWM_6_FUNCTION },  // LCD_Backlight
	{ AVR32_PWM_0_PIN, AVR32_PWM_0_FUNCTION }    // LCD_PWM_Kontrast
};


static const gpio_map_t switch_gpio_map =
{
	{ AVR32_PIN_PA18, GPIO_DIR_INPUT | GPIO_PULL_UP },	// SW1
	{ AVR32_PIN_PA19, GPIO_DIR_INPUT | GPIO_PULL_UP },  // SW2
	{ AVR32_PIN_PA20, GPIO_DIR_INPUT | GPIO_PULL_UP },	// SW3
	{ AVR32_PIN_PA21, GPIO_DIR_INPUT | GPIO_PULL_UP },  // SW4
	{ AVR32_PIN_PA22, GPIO_DIR_INPUT | GPIO_PULL_UP },	// SW5
	{ AVR32_PIN_PA23, GPIO_DIR_INPUT | GPIO_PULL_UP }   // SW6
};
	
void board_init(void)
{
	int i;
	
	pcl_freq_param_t freq = { configCPU_CLOCK_HZ, configPBA_CLOCK_HZ, FOSC0, OSC0_STARTUP };
		
	pcl_configure_clocks ( &freq );
	
	
	/*
	// EVK1105   LED Pins
	gpio_set_gpio_pin(AVR32_PIN_PB27);
	gpio_set_pin_low(AVR32_PIN_PB27);
	
	gpio_set_gpio_pin(AVR32_PIN_PB28);
	gpio_set_pin_low(AVR32_PIN_PB28);
	
	*/
	
	// LCD display
	
	gpio_enable_gpio( lcd_gpio_map, sizeof( lcd_gpio_map ) / sizeof( lcd_gpio_map[0] ) );
	
	for (i=0; i < (sizeof( lcd_gpio_map ) / sizeof( lcd_gpio_map[0] )); i++)
	{
		gpio_configure_pin( lcd_gpio_map[i].pin, lcd_gpio_map[i].function);
	}
	
	
	gpio_enable_module( lcd_pwm_gpio_map, sizeof( lcd_pwm_gpio_map ) / sizeof( lcd_pwm_gpio_map[0] ) );
	
	
	AVR32_PWM.channel[6].CMR.cpre = 4;
	AVR32_PWM.channel[6].cprd = 1000;
	AVR32_PWM.channel[6].cdty = 200;
	
	AVR32_PWM.ENA.chid6 = 1;
	
	AVR32_PWM.channel[0].CMR.cpre = 3;
	AVR32_PWM.channel[0].cprd = 1000;
	AVR32_PWM.channel[0].cdty = 520;
	
	AVR32_PWM.ENA.chid0 = 1;
	
	
	

	// Tasten

	gpio_enable_gpio( switch_gpio_map, sizeof( switch_gpio_map ) / sizeof( switch_gpio_map[0] ) );
		
	for (i=0; i < (sizeof( switch_gpio_map ) / sizeof( switch_gpio_map[0] )); i++)
	{
		gpio_configure_pin( switch_gpio_map[i].pin, switch_gpio_map[i].function);
	}
}
