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

#include "disp.h"
#include "sha1.h"

#include "serial.h"

#include "flashc.h"

extern int memcmp(const void *, const void *, size_t );
extern int memcpy(void *, const void *, size_t );
extern int memset(void *, int, size_t );



static void start_system_software(void)
{
		
		asm volatile (
		"movh r0, 0x8000  \n"
		"orl  r0, 0x5000  \n"
		"mov  pc, r0"
		);  // jump to 0x80005000
		
}



static void delay_nop(void)
{
	int i;
	
	for (i=0; i < 1000; i++)
	{
		asm volatile ("nop");
	}
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
		disp_i2s(buf + 2, 1, 10, 1, version_info[1]);
		buf[3] = '.';
		disp_i2s(buf + 4, 2, 10, 1, version_info[2]);
		buf[6] = '.';
		disp_i2s(buf + 7, 2, 10, 1, version_info[3]);
		buf[9] = maturity;
		buf[10] = 0;
}


#define COMMAND_TIMEOUT 59
#define COMMAND_TIMEOUT_PHY 9

static int do_system_update = 0;
static int system_update_counter = -1;
static int num_update_blocks = 0;
static int idle_counter = 0;
static int timeout_counter = COMMAND_TIMEOUT;

static int link_to_phy_mode = 0;

#define BOOTLOADER2_PIN  AVR32_PIN_PA18

static int rx_escape = 0;  

static int input_ptr = -1; // waiting for STX

#define INPUT_DATA_BUFLEN	550
static unsigned char input_data[INPUT_DATA_BUFLEN];



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





static SHA1Context ctx1;


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

static int checksum_is_correct(int image_len )
{



	SHA1Reset(&ctx1);
	SHA1Input(&ctx1, STAGING_AREA_ADDRESS, image_len);
	SHA1Result(&ctx1);

	int i;
	unsigned char sha1_buf_1[20];
	

	for (i=0; i < 5; i++)
	{
		unsigned int d = ctx1.Message_Digest[i];


		sha1_buf_1[i*4 + 0] = ((d >> 24) & 0xFF);
		sha1_buf_1[i*4 + 1] = ((d >> 16) & 0xFF);
		sha1_buf_1[i*4 + 2] = ((d >>  8) & 0xFF);
		sha1_buf_1[i*4 + 3] = ((d      ) & 0xFF);
	}



	unsigned char sha1_buf_2[20];
	int count = 0;
	int nibble = 0;

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

				if (count >= 20)
				  break;
			}
		}
	}



	if (count == 20)
	{
		return ( memcmp(sha1_buf_1, sha1_buf_2, 20) == 0);
	}

	return 0;
}


static void send_byte (int d)
{
	if (d == 0x10)
	{
		serial_putc(0, d);
	}
	serial_putc(0, d);
}


static void send_cmd (int cmd, int len, const unsigned char * d)
{
	serial_putc(0, 0x10);
	serial_putc(0, 0x02);

	send_byte(cmd);

	int i;
	
	for (i=0; i < len; i++)
	{
		send_byte(d[i]);
	}

	serial_putc(0, 0x10);
	serial_putc(0, 0x03);
}

static unsigned char version_info[69];

static void process_packet (int len)
{
	
	switch (input_data[0])
	{
		case 0x01: // info request
			send_cmd(0x01, sizeof version_info, version_info);
			break;
			
		case 0xEF: // switch to PHY
			link_to_phy_mode = 1;
			serial_init(1, 115200);
			break;
			
		case 0xE1: // start upload
			send_cmd(0xE4, 0, 0);
			system_update_counter = 0;
			break;
			
		case 0xE2: // fw data
			if ((len == (FLASH_BLOCK_SIZE + 1)) && (system_update_counter >= 0))
			{
				flashc_memcpy(STAGING_AREA_ADDRESS + (system_update_counter * FLASH_BLOCK_SIZE),
					input_data + 1, FLASH_BLOCK_SIZE, true);
				
				system_update_counter++;
				
				if (system_update_counter < STAGING_AREA_MAX_BLOCKS)
				{
					unsigned char d = 0x01;
					send_cmd(0xD4, 1, &d); // cmd exec OK
				}
				else
				{
					system_update_counter = -1; // too much data sent
				}
			}
			break;
			
		case 0xE3: // end of upload
			if (system_update_counter > 10) // some minimum size
			{
				if (checksum_is_correct((system_update_counter - 1) * FLASH_BLOCK_SIZE))
				{
					struct staging_area_info tmp_info;
					
					memcpy (tmp_info.version_info, STAGING_AREA_ADDRESS + 
						SOFTWARE_VERSION_IMAGE_OFFSET, sizeof tmp_info.version_info); // copy version info
					int num_blocks = system_update_counter - 1;
					tmp_info.num_blocks_hi = num_blocks >> 8;
					tmp_info.num_blocks_lo = num_blocks & 0xFF;
										
					int i;
					for (i=0; i < 5; i++)
					{
						unsigned int d = ctx1.Message_Digest[i]; // the checksum was 
													// calculated by function "checksum_is_correct" above

						tmp_info.sha1sum[i*4 + 0] = ((d >> 24) & 0xFF);
						tmp_info.sha1sum[i*4 + 1] = ((d >> 16) & 0xFF);
						tmp_info.sha1sum[i*4 + 2] = ((d >>  8) & 0xFF);
						tmp_info.sha1sum[i*4 + 3] = ((d      ) & 0xFF);
					}
					
					flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & tmp_info, sizeof tmp_info, true);
					
					send_cmd(0x01, sizeof version_info, version_info);
					timeout_counter = 3;
				}
				else
				{
					unsigned char d = 0x03;
					send_cmd(0xD4, 1, &d); // cmd exec: syntax error
				}					
			}
			system_update_counter = -1;
			break;
			
	}
}


static void input_rx_byte(unsigned char d)
{
	if (rx_escape)
	{
		rx_escape = 0;
		
		switch (d)
		{
		case 0x10:
			if (input_ptr >= 0)
			{
				input_data[input_ptr] = d;
				input_ptr++;
			}
			break;
			
		case 0x02: // STX
			input_ptr = 0;
			break;
			
		case 0x03:  //ETX
			if (input_ptr > 0)
			{
				process_packet(input_ptr);
			}				
			input_ptr = -1;
			break;
			
		default: // illegal character
			input_ptr = -1;
		}
	}
	else
	{
		if (d == 0x10)
		{
			rx_escape = 1;
		}
		else
		{
			if (input_ptr >= 0)
			{
				input_data[input_ptr] = d;
				input_ptr++;
			}
		}	
	}
	
	if (input_ptr >= INPUT_DATA_BUFLEN)
	{
		input_ptr = -1;
	}
}



static void idle_proc(void)
{
	// int i = gpio_get_pin_value( BOOTLOADER2_PIN );
	
	
	char buf[7];
	
	
	if (link_to_phy_mode == 1)
	{
		if (serial_rx_char_available(1) != 0)
		{		
			if (serial_getc(1, buf) == 1)
			{
				serial_putc(0, buf[0]);
				timeout_counter = COMMAND_TIMEOUT_PHY;
			}
		}			
	}
	
	
	if (serial_rx_char_available(0) != 0)
	{
		if (serial_getc(0, buf) == 1)
		{
			timeout_counter = (link_to_phy_mode == 1) ? COMMAND_TIMEOUT_PHY : COMMAND_TIMEOUT;
			
			if (link_to_phy_mode == 1)
			{
				serial_putc(1, buf[0]);
			}
			else
			{
				if (do_system_update == 0)
				{
					input_rx_byte(buf[0]);
				}
			}
		}
	}		
	
	
	if (idle_counter == 0)
	{
		idle_counter = 7000;
		
		disp_i2s(buf, 2, 10, 0, timeout_counter);
		disp_prints_xy(0, 116, 0, DISP_FONT_6x8, 0, buf);
		
		
		if (do_system_update == 0)
		{
			disp_i2s(buf, 6, 10, 0, serialRXOK);
			disp_prints_xy(0, 48, 48, DISP_FONT_6x8, 0, buf);
			
			disp_i2s(buf, 6, 10, 0, serialRXError);
			disp_prints_xy(0, 48, 56, DISP_FONT_6x8, 0, buf);
			
			disp_prints_xy(0, 0, 24, DISP_FONT_6x8, 0, (link_to_phy_mode == 1) ? "RS232 -> PHY   " : "RS232 -> SYSTEM");
		}
		
		if (timeout_counter < 0)
		{
			
			serial_stop(0);
			serial_stop(1);
			
			AVR32_WDT.ctrl = 0x55001001; // turn on watchdog
			AVR32_WDT.ctrl = 0xAA001001;
			
			while(1)  // do nothing until watchdog does a reset
			{
			}				
			
		}
		
		timeout_counter --;
	}
	
	
	delay_nop();
	
	idle_counter --;
	
}




static int check_pending_system_update(void)
{
	struct staging_area_info * info = STAGING_AREA_INFO_ADDRESS;
	
	num_update_blocks = (info->num_blocks_hi << 8) | info->num_blocks_lo;
	
	if ((num_update_blocks < 1) || (num_update_blocks > STAGING_AREA_MAX_BLOCKS)) // too small or too big
	{
		return 0;
	}
	
	char buf[20];
	
	version2string(buf, info->version_info);
	
	if (buf[0] != 'S') // not a system image in the staging area
	{
		return 0;
	}
	
	return 1;
}

int main (void)
{
	AVR32_WDT.ctrl = 0x55001000; // turn off watchdog
	AVR32_WDT.ctrl = 0xAA001000;
	gpio_enable_gpio_pin(BOOTLOADER2_PIN);
	gpio_configure_pin(BOOTLOADER2_PIN, GPIO_DIR_INPUT | GPIO_PULL_UP);


	idle_counter = 0;
	
	
	do_system_update = check_pending_system_update();
	
	
	if ((gpio_get_pin_value( BOOTLOADER2_PIN ) != 0)  // key not pressed
		&& (do_system_update == 0)) // no system update necessary
	{
		start_system_software();
	}
	
	
	board_init();
	
	disp_init();
	
	serial_init(0, 115200);
	
	
	char buf[20];
	
	version2string(buf, UPDATE_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
	
	disp_prints_xy(0, 0, 0, DISP_FONT_6x8, 0, buf);
	
	memset(version_info, 0x20, sizeof version_info); // fill with spaces
	memcpy(version_info + ((sizeof version_info) - 15), (unsigned char *) 0x80800204, 15);
				// CPU serial number
	
	const char * p = "UP4DAR Updater ";
	char * p2 = (char *) version_info;
	
	while(*p) // copy string
	{
		*p2 = *p;
		p++;
		p2++;
	}
	
	p = buf;
	
	while(*p) // copy version number
	{
		*p2 = *p;
		p++;
		p2++;
	}
	
	version2string(buf, SYSTEM_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
	
	if (buf[0] == 'S')
	{
		disp_prints_xy(0, 0, 8, DISP_FONT_6x8, 0, buf);
	}		
	
	if (do_system_update != 0)
	{	
		disp_prints_xy(0, 0, 48, DISP_FONT_6x8, 0, "New System Image:");
	
		SHA1Reset(&ctx1);
		SHA1Input(&ctx1, STAGING_AREA_ADDRESS, num_update_blocks * FLASH_BLOCK_SIZE);
		SHA1Result(&ctx1);
	
		if (memcmp(ctx1.Message_Digest, STAGING_AREA_INFO_ADDRESS->sha1sum, SHA1SUM_SIZE) != 0) // checksum not correct
		{
			disp_prints_xy(0, 0, 48, DISP_FONT_6x8, 0, "Checksum not correct!");
		}
		else
		{
			disp_prints_xy(0, 0, 48, DISP_FONT_6x8, 0, "New System Image:");
			
			flashc_memcpy(SYSTEM_PROGRAM_START_ADDRESS, STAGING_AREA_ADDRESS,
				num_update_blocks * FLASH_BLOCK_SIZE, true);
			
			version2string(buf, SYSTEM_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
			disp_prints_xy(0, 0, 56, DISP_FONT_6x8, 0, buf);
		}
			
		unsigned char d = 0;
		flashc_memcpy(STAGING_AREA_INFO_ADDRESS, & d, 1, true); // erase info
		
		timeout_counter = 5;
	}
	else
	{
		disp_prints_xy(0, 0, 32, DISP_FONT_6x8, 0, "(115200 Baud 8N1)");
		disp_prints_xy(0, 0, 48, DISP_FONT_6x8, 0, "RxOK:");
		disp_prints_xy(0, 0, 56, DISP_FONT_6x8, 0, "RxError:");
	}
	
	disp_main_loop( idle_proc );
}
