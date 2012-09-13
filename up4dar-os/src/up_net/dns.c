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
 * dns.c
 *
 * Created: 13.09.2012 08:59:57
 *  Author: mdirska
 */ 

#include <asf.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"



#include "up_dstar/vdisp.h"

#include "dns.h"
#include "up_crypto/up_crypto.h"



int dns_udp_local_port;

void dns_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	
}


static void vDNSTask( void *pvParameters )
{
	int counter = 0;
	char tmp_buf[10];
	
	while(1)
	{
		dns_udp_local_port = 50000 + (crypto_get_random_15bit() & 0xFFF);
		
		
		/*
		counter ++;
		
		vdisp_i2s( tmp_buf, 5, 10, 0, counter);
		
		vdisp_prints_xy( 0, 48, VDISP_FONT_6x8, 1, tmp_buf );
		*/
		
		vTaskDelay(3000);
	}
}	

void dns_init(void)
{
	dns_udp_local_port = 0;
	
	xTaskCreate( vDNSTask, (signed char *) "DNS", 200, ( void * ) 0, ( tskIDLE_PRIORITY + 1 ), ( xTaskHandle * ) NULL );

}

