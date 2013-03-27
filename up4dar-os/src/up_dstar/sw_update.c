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
 * sw_update.c
 *
 * Created: 24.10.2012 18:12:51
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include "gcc_builtin.h"


#include "vdisp.h"

#include "phycomm.h"


#include "settings.h"

#include "sw_update.h"
#include "flashc.h"

#include "up_net/snmp_data.h"

#include "up_crypto/sha1.h"
#include "dstar.h"


#include "software_version.h"


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



static int hex_value(int ch)
{
	if ((ch >= '0') && (ch <= '9'))
	{
		return ch - 48;
	}
	else if ((ch >= 'A') && (ch <= 'F'))
	{
		return (ch - 65) + 10;
	}
	else if ((ch >= 'a') && (ch <= 'f'))
	{
		return (ch - 97) + 10;
	}

	return -1;
}

static SHA1Context ctx;

static void calc_sha1 (int num_blocks, unsigned char * res_sum)
{
	
	
	unsigned char * fw_buf = STAGING_AREA_ADDRESS;

	int image_len = num_blocks * FLASH_BLOCK_SIZE;

	SHA1Reset(&ctx);
	SHA1Input(&ctx, fw_buf, image_len);
	SHA1Result(&ctx);

	int i;


	for (i=0; i < 5; i++)
	{
		unsigned int d = ctx.Message_Digest[i];

		res_sum[i*4 + 0] = ((d >> 24) & 0xFF);
		res_sum[i*4 + 1] = ((d >> 16) & 0xFF);
		res_sum[i*4 + 2] = ((d >>  8) & 0xFF);
		res_sum[i*4 + 3] = ((d      ) & 0xFF);
	}

}


static unsigned char sha1_buf_1[SHA1SUM_SIZE];
static unsigned char sha1_buf_2[SHA1SUM_SIZE];

static int checksum_is_correct(int last_block_number)
{
	int image_len = last_block_number * FLASH_BLOCK_SIZE;
	
	
	
	calc_sha1(last_block_number, sha1_buf_1);
	
	
	int count = 0;
	int nibble = 0;

	int i;
	for (i=0; i < 80; i++)
	{
		int v = hex_value( STAGING_AREA_ADDRESS[image_len + i]);

		if (v >= 0)
		{
			if (nibble == 0)
			{
				sha1_buf_2[count] = v << 4;
				nibble = 1;
			}
			else
			{
				sha1_buf_2[count] |= v;
				nibble = 0;
				count ++;

				if (count >= SHA1SUM_SIZE)
				  break;
			}
		}
	}


	if (count == SHA1SUM_SIZE)
	{
		return ( memcmp(sha1_buf_1, sha1_buf_2, SHA1SUM_SIZE) == 0);
	}

	return 0;
}



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

int snmp_get_sw_version (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	return snmp_encode_int(
	software_version[1] * 10000 +
	software_version[2] * 100 +
	software_version[3], res, res_len, maxlen );
}



#define PHY_VERSION_STRING_LEN 54

// static int ypos = 0;

static int parse_version_string (const char * s, const char * prefix,
	unsigned char * version_numbers, int num_numbers )
{
	int i;
	if (num_numbers >= 5)
	{
		return 0;
	}

	unsigned char numbers_seen[4];

	for (i=0; i < num_numbers; i++)
	{
		version_numbers[i] = 0;
		numbers_seen[i] = 0;
	}

	int number_ptr = 0;

	const char * ptr = strstr(s, prefix);
	
	

	if (ptr != NULL)
	{
		// vdisp_prints_xy(0, ypos, VDISP_FONT_4x6, 0, ptr);
		// ypos += 6;
		
		ptr += strlen(prefix); // go to start of numbers

		while (1)
		{
			switch (*ptr)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					numbers_seen[number_ptr] = 1;
					version_numbers[number_ptr] =
							version_numbers[number_ptr] * 10 + ((*ptr) & 0x0F);
					break;
				case '.':
					if (numbers_seen[number_ptr] == 0)
					{
						return 0; // no numbers before dot
					}
					number_ptr ++;
					if (number_ptr >= num_numbers)
					{
						return 0;  // too many dots
					}
					break;
				default:
					if (numbers_seen[number_ptr] == 0)
					{
						return 0; // no numbers after last dot
					}
					if ((number_ptr + 1) == num_numbers)
					{
						return 1; // right number of dots
					}
					return 0;
			}
			ptr ++;
		}
	}

	return 0;
}



static unsigned char hw_version[2];

static struct staging_area_info tmp_info;

static char vbuf[PHY_VERSION_STRING_LEN + 1];


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
		
		int i;

		for (i=0; i < PHY_VERSION_STRING_LEN; i++)
		{
			vbuf[i] = STAGING_AREA_ADDRESS[FLASH_BLOCK_SIZE - 64 + i];
		}

		vbuf[PHY_VERSION_STRING_LEN] = 0;
		
		

		
		
		if (checksum_is_correct( block_number ))
		{
			
			memcpy (tmp_info.version_info, STAGING_AREA_ADDRESS +
				SOFTWARE_VERSION_IMAGE_OFFSET, sizeof tmp_info.version_info); // copy version info
			
			tmp_info.num_blocks_hi = block_number >> 8;
			tmp_info.num_blocks_lo = block_number & 0xFF;
			
			calc_sha1 ( block_number, tmp_info.sha1sum );
			
			flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & tmp_info, sizeof tmp_info, true);
		}
		else if (parse_version_string(vbuf, "HW-Ver: ", hw_version, 2)
			&& parse_version_string(vbuf, "SW-Ver: ", tmp_info.version_info + 1, 3)
			&& (hw_version[0] == 1) && (hw_version[1] == 1))
		{
			
			tmp_info.version_info[0] = SOFTWARE_IMAGE_PHY;
			
			block_number++; // last block is part of the firmware
			
			tmp_info.num_blocks_hi = block_number >> 8;
			tmp_info.num_blocks_lo = block_number & 0xFF;
			
			calc_sha1 ( block_number, tmp_info.sha1sum );
			
			flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & tmp_info, sizeof tmp_info, true);
		}
		
		
	}
	
	
	return 0;
}



void version2string (char * buf, const unsigned char * version_info)
{
	char image = '?';
	char maturity = 0;
	
	switch(version_info[0] & 0x03)
	{
		case SOFTWARE_IMAGE_PHY:
			image = 'P'; // PHY image
			break;
		case SOFTWARE_IMAGE_UPDATER:
			image = 'U'; // Updater image
			break;
		case SOFTWARE_IMAGE_SYSTEM(0):
			image = SOFTWARE_IMAGE_SYSTEM_LETTERS[ (version_info[0] & 0x3C) >> 2 ];
			break;
	}
	
	switch(version_info[0] & 0xC0)
	{
		case SOFTWARE_MATURITY_BETA:
			maturity = 'b'; // beta
			break;
		case SOFTWARE_MATURITY_EXPERIMENTAL:
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


static void send_cmd_phy (int cmd, int len, const unsigned char * data)
{
	char buf[530];
	
	if (len < (sizeof buf))
	{
		memcpy(buf + 1, data, len);
		buf[0] = cmd;
		phyCommSendCmd(buf, len + 1);
	}
}


static void send_cmd_with_arg1(int cmd, int arg1)
{
	unsigned char c;

	c = arg1;

	send_cmd_phy(cmd, 1, &c);
}

static void send_cmd_without_arg(int cmd)
{
	send_cmd_phy(cmd, 0, 0);
}


static struct dstarPacket dp;
static xQueueHandle dstarQueue;


static unsigned char update_state = 0;
static int fw_send_counter = 0;

static int processPacket(void)
{
	char buf[20];
	
	
	switch(dp.cmdByte)
	{
		case 0x01:
			
			if (dp.dataLen >= 69)
			{
				memcpy (vbuf, dp.data, PHY_VERSION_STRING_LEN );
				vbuf[PHY_VERSION_STRING_LEN] = 0;
				
				unsigned char p_ver_buf[4];
				p_ver_buf[0] = SOFTWARE_IMAGE_PHY;

				if (update_state == 0)
				{
					if (parse_version_string(vbuf, "SW-Ver: ", p_ver_buf+1, 3) != 0)
					{
						vdisp_prints_xy(0, 16, VDISP_FONT_6x8, 0, "Old Firmware:");
						version2string(buf, p_ver_buf);
						vdisp_prints_xy(12, 24, VDISP_FONT_6x8, 0, buf);
						
						send_cmd_with_arg1(0xD3, 0x01); // switch to service mode
						update_state = 1;
					}
					else
					{
						vdisp_prints_xy(0, 16, VDISP_FONT_6x8, 0, "PHY Bootloader");
						send_cmd_without_arg(0xE1); // switch to flash mode
						update_state = 2;
					}
				}
				else if (update_state == 4)  // info msg after restart of new firmware
				{
					if (parse_version_string(vbuf, "SW-Ver: ", p_ver_buf+1, 3) != 0)
					{
						version2string(buf, p_ver_buf);
						vdisp_prints_xy(12, 40, VDISP_FONT_6x8, 0, buf);
						return 1; // update was successful
					}						
				}								
			}
			break;
			
		case 0xD4: // cmd exec
			if (dp.dataLen >= 1)
			{
				if ((update_state == 1) && (dp.data[0] == 2)) // switch to service mode failed:
							// UP4DAR already in service mode
				{
					send_cmd_without_arg(0xE1); // switch to flash mode
					update_state = 2;
					break;
				}

				if (dp.data[0] != 1) // unexpected result
				{
					vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, "ERROR 1");
					// after this: wait for timeout
					break;
				}

				if (update_state == 3)
				{
					fw_send_counter ++;

					if (fw_send_counter >= num_update_blocks)
					{
						send_cmd_without_arg (0xE3);  // end of flash
						update_state = 4;  // wait for version_info
					}
					else
					{
						vdisp_i2s(buf, 3, 10, 1, fw_send_counter+1);
						vdisp_prints_xy(12, 40, VDISP_FONT_6x8, 0, buf);
						
						send_cmd_phy( 0xE2, FLASH_BLOCK_SIZE, STAGING_AREA_ADDRESS +
							(fw_send_counter * FLASH_BLOCK_SIZE));
					}
				}
			}
			break;
			
		case 0xD1: // mode info
			if (dp.dataLen >= 2)
			{
				if ((dp.data[0] != 1) && (dp.data[1] != 0)) // unexpected result
				{
					vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, "ERROR 2");
					// after this: wait for timeout
					break;
				}

				if (update_state == 1)
				{
					send_cmd_without_arg(0xE1); // switch to flash mode
					update_state = 2;
				}
			}
			break;
			
		case 0xE4: // update mode
			if (update_state == 2)
			{
				update_state = 3;
				
				vdisp_prints_xy(12, 40, VDISP_FONT_6x8, 0, "001/");
				vdisp_i2s(buf, 3, 10, 1, num_update_blocks);
				vdisp_prints_xy(36, 40, VDISP_FONT_6x8, 0, buf);
				
				send_cmd_phy( 0xE2, FLASH_BLOCK_SIZE, STAGING_AREA_ADDRESS);
				fw_send_counter = 0;
			}
			break;
	}
	
	return 0;
}


static int do_phy_update (void)
{
	unsigned char qTimeout = 0;
	
	update_state = 0;
	
	send_cmd_without_arg(0x01);
	
	for( ;; )
	{
		if( xQueueReceive( dstarQueue, &dp, 500 ) )
		{
			if (processPacket() != 0) // end of flashing procedure
				return 1;
			qTimeout = 0;
		}
		else
		{
			// timeout
			qTimeout ++;
			
			if (qTimeout > 10)
				return 0;
		}
		
	}
}




static void vUpdateTask( void *pvParameters )
{
	SHA1Context ctx1;
	
	vTaskDelay(1000);
	
	vdisp_clear_rect(0,0, 128, 64);
	
	char buf[20];
	struct staging_area_info * info = STAGING_AREA_INFO_ADDRESS;
	
	version2string(buf, info->version_info);
	
	vdisp_prints_xy(0, 0, VDISP_FONT_6x8, 0, "New Firmware:");
	vdisp_prints_xy(12, 8, VDISP_FONT_6x8, 0, buf);
	
	SHA1Reset(&ctx1);
	SHA1Input(&ctx1, STAGING_AREA_ADDRESS, num_update_blocks * FLASH_BLOCK_SIZE);
	  // num_update_blocks  set in sw_update_pending
	SHA1Result(&ctx1);
	
	if (memcmp(ctx1.Message_Digest, STAGING_AREA_INFO_ADDRESS->sha1sum, SHA1SUM_SIZE) != 0) // checksum not correct
	{
		vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, "Checksum not correct!");
	}
	else
	{
		if (buf[0] == 'U')
		{
			if (num_update_blocks > 24) // image too large (must be <= 12kByte)
			{
				vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, "File too big!");
			}
			else
			{
				flashc_memcpy(UPDATE_PROGRAM_START_ADDRESS, STAGING_AREA_ADDRESS,
					num_update_blocks * FLASH_BLOCK_SIZE, true);
				vdisp_prints_xy(0, 38, VDISP_FONT_6x8, 0, "Update was successful!");
				version2string(buf, UPDATE_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
				vdisp_prints_xy(12, 48, VDISP_FONT_6x8, 0, buf);
				
			}
		}
		else if (buf[0] == 'P')
		{
			if (do_phy_update() != 0)
			{
				vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, "Update was successful!");
			}
			else
			{
				vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, "UPDATE ERROR");
			}
		}
		
	}
	
	vTaskDelay(2500);
	
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
	
	xTaskCreate( vUpdateTask, (signed char *) "Update", 1500, ( void * ) 0,  (tskIDLE_PRIORITY + 1), ( xTaskHandle * ) NULL );
	
}