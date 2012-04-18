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

#include "ambe.h"




static portTASK_FUNCTION( ambeTask, pvParameters )
{
	for( ;; )
	{
		vTaskDelay(10);
		
	}
} 






void ambeInit( void )
{
	
	xTaskCreate( ambeTask, ( signed char * ) "AMBE", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 1 , ( xTaskHandle * ) NULL );

}
