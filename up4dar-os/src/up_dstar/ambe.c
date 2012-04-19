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
 * ambe.c
 *
 * Created: 18.04.2012 16:40:39
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "task.h"

#include "gpio.h"

#include "ambe.h"




static portTASK_FUNCTION( ambeTask, pvParameters )
{
	AVR32_SPI0.mr = 0x030F0007;  // PCSDEC PS MSTR   3 clocks delay
	
	AVR32_SPI0.csr0 = 0x03034084; // CSNAAT, 16 Bit,  SCBR=64
	AVR32_SPI0.csr1 = 0x03034084; // CSNAAT, 16 Bit,  SCBR=64
	AVR32_SPI0.csr2 = 0x03034084; // CSNAAT, 16 Bit,  SCBR=64
	AVR32_SPI0.csr3 = 0x03034084; // CSNAAT, 16 Bit,  SCBR=64
	
	AVR32_SPI0.CR.spien = 1;
	
	gpio_set_pin_low(AVR32_PIN_PB20); // RESETN
	vTaskDelay(1);
	gpio_set_pin_high(AVR32_PIN_PB20);
	
	for( ;; )
	{
		vTaskDelay(20);
		
		AVR32_SPI0.tdr = 0x000A0000;
	}
} 






void ambeInit( void )
{
	
	xTaskCreate( ambeTask, ( signed char * ) "AMBE", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 2 , ( xTaskHandle * ) NULL );

}
