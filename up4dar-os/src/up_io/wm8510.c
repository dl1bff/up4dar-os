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
 * wm8510.c
 *
 * Created: 22.04.2012 14:49:32
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"
#include "task.h"

#include "gpio.h"

#include "wm8510.h"

#include "up_dstar/vdisp.h"

#include "gcc_builtin.h"



static int send_wm8510 (int reg, int value)
{
	AVR32_TWI.iadr = (reg << 1) | (value >> 8);
		
	AVR32_TWI.thr = value & 0xFF;
	
	vTaskDelay(1);
	
	avr32_twi_sr_t sr = AVR32_TWI.SR;
	
	if (sr.nack || !sr.txcomp)  // no ACK received or TX not complete
	{
		return 1;
	}
	
	return 0;
}



static int chip_init(void)
{
	AVR32_TWI.cr = 0x24; // MSEN + SVDIS
	AVR32_TWI.mmr =  0x001A0100;    // DADR= 0011010   , One-byte internal device address, MREAD = 0
	
	if (send_wm8510(  0, 0x000) != 0) goto error;  // reset
	
	if (send_wm8510(  49, 0x003) != 0) goto error;  // TSDEN, VROI (charge output C slowly)
	
	if (send_wm8510(  3, 0x005) != 0) goto error;  // Power Management 3:  SPKMIXEN, DACEN
	
	if (send_wm8510(  10, 0x040) != 0) goto error;  // DACMU
		
	if (send_wm8510(  1, 0x00B) != 0) goto error;  // Power Management 1:  BIASEN, VMIDSEL=5kohm
	
	if (send_wm8510(  4, 0x018) != 0) goto error;  // Audio Interface: DSP/PCM mode, 16 bit
	
	vTaskDelay(300);
	
	if (send_wm8510(  1, 0x00F) != 0) goto error;  // Power Management 1:  BIASEN, BUFIOEN, VMIDSEL=5kohm
	
	vTaskDelay(300);
	
	if (send_wm8510(  6, 0x0C0) != 0) goto error;  // Clock: MCLK / 8
	
	if (send_wm8510(  7, 0x00A) != 0) goto error;  // Sample rate 8kHz
	
	if (send_wm8510(  3, 0x065) != 0) goto error;  // Power Management 3:  SPKNEN, SPKPEN, SPKMIXEN, DACEN
	
	vTaskDelay(100);
	
	if (send_wm8510(  10, 0x000) != 0) goto error;  // DACMU off
	
	return 0;
	
	error:
		return 1;
}


#define BUF_SIZE 64
static int16_t audio_buf[BUF_SIZE];
static int buf_ptr = 0;


void wm8510_put_audio_sample (int16_t d)
{
	audio_buf[buf_ptr] = d;
	buf_ptr++;
	if (buf_ptr >= BUF_SIZE)
	{
		buf_ptr = 0;
	}
}

static portTASK_FUNCTION( wm8510Task, pvParameters )
{
	int audio_state = 0;
	int i;
	
	for (i=0; i < BUF_SIZE; i+=4) // 2kHz tone
	{
		audio_buf[i+0] = 0;
		audio_buf[i+1] = 100;
		audio_buf[i+2] = 0;
		audio_buf[i+3] = -100;  
	}
	
	
	for(;;)
	{
		
		switch (audio_state)
		{
		case 0:
			vTaskDelay(1000);
			if (chip_init() == 0) // init successful
			{
				audio_state = 1;
				vdisp_prints_xy(0, 40, VDISP_FONT_6x8, 0, "OK ");
				
				AVR32_PDCA.channel[2].mr = AVR32_PDCA_HALF_WORD; // 16 bit transfer
				AVR32_PDCA.channel[2].psr = AVR32_PDCA_PID_SSC_TX; // select peripherial
				AVR32_PDCA.channel[2].mar = (unsigned long) audio_buf;
				AVR32_PDCA.channel[2].tcr = BUF_SIZE ; 
				
				AVR32_SSC.cr = 0x0100;  // enable TX
				AVR32_PDCA.channel[2].cr = 1; // tx DMA enable
				
			}
			else
			{
				vdisp_prints_xy(0, 40, VDISP_FONT_6x8, 0, "ERR");
			}
			break;
			
		case 1:
			vTaskDelay(1);
			if ( (AVR32_PDCA.channel[2].tcrr == 0)
				&& (AVR32_PDCA.channel[2].tcr < BUF_SIZE))
			{
				AVR32_PDCA.channel[2].marr = (unsigned long) audio_buf;
				AVR32_PDCA.channel[2].tcrr = BUF_SIZE;	
			}			
			break;
		}

	}
}	





void wm8510Init(void)
{
	
	xTaskCreate( wm8510Task, ( signed char * ) "WM8510", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 2 , ( xTaskHandle * ) NULL );
	
}