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



extern int memcmp(const void *, const void *, size_t );


static void start_system_software(void)
{
	
		asm volatile (
		"movh r0, 0x8000  \n"
		"orl  r0, 0x4000  \n"
		"mov  pc, r0"
		);  // jump to 0x80004000
		
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
		char maturity = 'N';
		
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
			maturity = 'B';
			break;
			case 0x40:
			maturity = 'E';
			break;
		}
		
		buf[0] = image;
		buf[1] = '-';
		disp_i2s(buf + 2, 1, 10, 1, version_info[1]);
		buf[3] = '.';
		disp_i2s(buf + 4, 2, 10, 1, version_info[2]);
		buf[6] = '.';
		disp_i2s(buf + 7, 2, 10, 1, version_info[3]);
		buf[9] = '-';
		buf[10] = maturity;
		buf[11] = 0;
}


static int do_system_update = 0;
// static int system_update_counter;
static int num_update_blocks = 0;
static int idle_counter = 0;
static int counter = 0;

#define BOOTLOADER2_PIN  AVR32_PIN_PA18


static void idle_proc(void)
{
	// int i = gpio_get_pin_value( BOOTLOADER2_PIN );
	
	
	char buf[6];
	
	
	if (idle_counter == 0)
	{
		idle_counter = 7000;
		
		disp_i2s(buf, 5, 10, 0, counter);
		disp_prints_xy(0, 0, 16, DISP_FONT_6x8, 0, buf);
		
		counter++;
		
		if (counter > 30)
		{
			start_system_software();
		}
	}
	
	
	delay_nop();
	
	idle_counter --;
	
}


#define SHA1SUM_SIZE		20

struct staging_area_info
{
	unsigned char version_info[4];
	unsigned char num_blocks_hi;
	unsigned char num_blocks_lo;
	unsigned char sha1sum[SHA1SUM_SIZE];
};



#define FLASH_BLOCK_SIZE	512
#define SYSTEM_PROGRAM_START_ADDRESS		((unsigned char *) 0x80004000)
#define UPDATE_PROGRAM_START_ADDRESS		((unsigned char *) 0x80002000)
#define STAGING_AREA_ADDRESS				((unsigned char *) 0x80042000)
#define STAGING_AREA_MAX_BLOCKS		495
#define SOFTWARE_VERSION_IMAGE_OFFSET	4
#define STAGING_AREA_INFO_ADDRESS		((struct staging_area_info *) (STAGING_AREA_ADDRESS + (STAGING_AREA_MAX_BLOCKS * FLASH_BLOCK_SIZE)))




static SHA1Context ctx1;


static int check_pending_system_update(void)
{
	struct staging_area_info * info = STAGING_AREA_INFO_ADDRESS;
	
	num_update_blocks = (info->num_blocks_hi << 8) | info->num_blocks_lo;
	
	if ((num_update_blocks < 1) || (num_update_blocks > 495)) // too small or too big
	{
		return 0;
	}
	
	char buf[20];
	
	version2string(buf, info->version_info);
	
	if (buf[0] != 'S') // not a system image in the staging area
	{
		return 0;
	}
	
	SHA1Reset(&ctx1);
	SHA1Input(&ctx1, STAGING_AREA_ADDRESS, num_update_blocks * FLASH_BLOCK_SIZE);
	SHA1Result(&ctx1);
	
	if (memcmp(ctx1.Message_Digest, info->sha1sum, SHA1SUM_SIZE) != 0) // checksum not correct
	{
		return 0;
	}
	
	if (memcmp(SYSTEM_PROGRAM_START_ADDRESS, STAGING_AREA_ADDRESS,
		 num_update_blocks * FLASH_BLOCK_SIZE) == 0) // already copied
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
	counter = 0;	
	// delay_nop();
	
	do_system_update = check_pending_system_update();
	
	
	if ((gpio_get_pin_value( BOOTLOADER2_PIN ) != 0)  // key not pressed
		&& (do_system_update == 0)) // no system update necessary
	{
		start_system_software();
	}
	
	
	board_init();
	
	disp_init();

	// Insert application code here, after the board has been initialized.
	
	
	
	char buf[20];
	
	version2string(buf, UPDATE_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
	
	disp_prints_xy(0, 0, 0, DISP_FONT_6x8, 0, buf);
	
	version2string(buf, SYSTEM_PROGRAM_START_ADDRESS + SOFTWARE_VERSION_IMAGE_OFFSET);
	
	disp_prints_xy(0, 0, 8, DISP_FONT_6x8, 0, buf);
	
	
	
	disp_main_loop( idle_proc );
}
