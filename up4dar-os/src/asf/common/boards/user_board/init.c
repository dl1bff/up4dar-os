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


void board_init(void)
{
	
	pcl_freq_param_t freq = { configCPU_CLOCK_HZ, configPBA_CLOCK_HZ, FOSC0, OSC0_STARTUP };
		
	pcl_configure_clocks ( &freq );
	
	
	// EVK1105   LED Pins
	gpio_set_gpio_pin(AVR32_PIN_PB27);
	gpio_set_pin_low(AVR32_PIN_PB27);
	
	gpio_set_gpio_pin(AVR32_PIN_PB28);
	gpio_set_pin_low(AVR32_PIN_PB28);
	
	
}
