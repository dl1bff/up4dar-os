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
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
#include <asf.h>


#define BOOTLOADER2_PIN  AVR32_PIN_PA18

static void delay_nop(void)
{
	int i;
	
	for (i=0; i < 1000; i++)
	{
		asm volatile ("nop");
	}
}

int main (void)
{
	gpio_enable_gpio_pin(BOOTLOADER2_PIN);
	gpio_configure_pin(BOOTLOADER2_PIN, GPIO_DIR_INPUT | GPIO_PULL_UP);
	
	delay_nop();
	
	if (gpio_get_pin_value( BOOTLOADER2_PIN ) != 0)  // key not pressed
	{
		asm volatile (
			"movh r0, 0x8000  \n"
			"orl  r0, 0x4000  \n"
			"mov  pc, r0"
			);  // jump to 0x80004000 
	}
	
	
	board_init();

	// Insert application code here, after the board has been initialized.
	
	while (1)
	{
		
			
		delay_nop();
	
	}	
}
