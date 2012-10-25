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
#include "flashc.h"

#include "up_net/snmp_data.h"



static xQueueHandle dstarQueue;




#define FLASH_BLOCK_SIZE	512
#define SYSTEM_PROGRAM_START_ADDRESS		((unsigned char *) 0x80005000)
#define UPDATE_PROGRAM_START_ADDRESS		((unsigned char *) 0x80002000)
#define STAGING_AREA_ADDRESS				((unsigned char *) 0x80042800)
#define STAGING_AREA_MAX_BLOCKS		487
#define SOFTWARE_VERSION_IMAGE_OFFSET	4
#define STAGING_AREA_INFO_ADDRESS		((struct staging_area_info *) (STAGING_AREA_ADDRESS + (STAGING_AREA_MAX_BLOCKS * FLASH_BLOCK_SIZE)))


#define SHA1SUM_SIZE		20

struct staging_area_info
{
	unsigned char version_info[4];
	unsigned char num_blocks_hi;
	unsigned char num_blocks_lo;
	unsigned char sha1sum[SHA1SUM_SIZE];
};



int snmp_get_sw_update (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	if (maxlen < (sizeof (struct staging_area_info)))
	{
		return 1; // result memory too small
	}
	
	memcpy(res, STAGING_AREA_INFO_ADDRESS, sizeof (struct staging_area_info));
	*res_len = sizeof (struct staging_area_info);
	return 0;
}




int snmp_set_sw_update (int32_t arg, const uint8_t * req, int req_len)
{
	unsigned short block_number;
	
	if (req_len != (FLASH_BLOCK_SIZE + 2))
	{
		return 1; // unexpected length
	}
	
	block_number = (req[0] << 8) | req[1];
	
	int last_block = 0;
	
	if ((block_number & 0x8000) != 0)
	{
		last_block = 1;
	}
	
	block_number &= 0x7FFF;
	
	if (block_number >= STAGING_AREA_MAX_BLOCKS)
	{
		return 1; // illegal block position
	}
	
	flashc_memcpy(STAGING_AREA_ADDRESS + (block_number * FLASH_BLOCK_SIZE),
		req + 2, FLASH_BLOCK_SIZE, true);
	
	
	if (last_block != 0)
	{
		// TODO here
		// calculation of SHA1 here and write valid info to STAGING_AREA_INFO_ADDRESS
		
		unsigned char d = 0xF0;
		flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & d, 1, true); // erase info
	}
	
	
	return 0;
}



static void version2string (char * buf, const unsigned char * version_info)
{
	char image = '?';
	char maturity = 0;
	
	switch(version_info[0] & 0x0F)
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
	
	switch(version_info[0] & 0xC0)
	{
		case 0x80:
			maturity = 'b'; // beta
			break;
		case 0x40:
			maturity = 'e'; // experimental
			break;
	}
	
	buf[0] = image;
	buf[1] = '.';
	vdisp_i2s(buf + 2, 1, 10, 1, version_info[1]);
	buf[3] = '.';
	vdisp_i2s(buf + 4, 2, 10, 1, version_info[2]);
	buf[6] = '.';
	vdisp_i2s(buf + 7, 2, 10, 1, version_info[3]);
	buf[9] = maturity;
	buf[10] = 0;
}


static unsigned short num_update_blocks = 0;

int sw_update_pending(void)
{
		struct staging_area_info * info = STAGING_AREA_INFO_ADDRESS;
		
		num_update_blocks = (info->num_blocks_hi << 8) | info->num_blocks_lo;
		
		if ((num_update_blocks < 1) || (num_update_blocks > STAGING_AREA_MAX_BLOCKS)) // too small or too big
		{
			return 0;
		}
		
		char buf[20];
		
		version2string(buf, info->version_info);
		
		if ((buf[0] != 'P') && (buf[0] != 'U')) // not a PHY or Updater image in the staging area
		{
			return 0;
		}
		
		return 1;
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
	
	unsigned char d = 0;
	flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & d, 1, true); // erase info
	
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