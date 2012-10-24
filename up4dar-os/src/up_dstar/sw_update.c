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
 * sw_update.c
 *
 * Created: 24.10.2012 18:12:51
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <string.h>


#include "vdisp.h"

#include "phycomm.h"


#include "settings.h"

#include "sw_update.h"


static xQueueHandle dstarQueue;



int sw_update_pending(void)
{
	return 0;
}



static void vUpdateTask( void *pvParameters )
{
	int counter = 0;
	
	while (counter < 10)
	{
		char buf[10];
		
		vTaskDelay(1000);
		
		vdisp_i2s(buf, 5, 10, 1, counter);
		vdisp_prints_xy(0,0, VDISP_FONT_6x8, 1, buf);
		
		counter++;
	}
	
	// enable watchdog -> reset in one second
	AVR32_WDT.ctrl = 0x55001001;
	AVR32_WDT.ctrl = 0xAA001001;
	
	while(1)
	{
		vTaskDelay(1000);
	}
}	


void sw_update_init(xQueueHandle dq )
{
	dstarQueue = dq;
	
	xTaskCreate( vUpdateTask, (signed char *) "Update", 300, ( void * ) 0,  (tskIDLE_PRIORITY + 1), ( xTaskHandle * ) NULL );
	
}