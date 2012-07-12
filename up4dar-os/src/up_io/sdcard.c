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
 * sdcard.c
 *
 * Created: 12.07.2012 08:22:57
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <avr32/io.h>
#include "board.h"
#include "gpio.h"
#include "gcc_builtin.h"
#include "up_dstar/vdisp.h"
#include "up_dstar/audio_q.h"

#include "sdcard.h"

#define SD_BUF_LEN	900

static uint8_t sdTxBuf[SD_BUF_LEN];
static uint8_t sdRxBuf[SD_BUF_LEN];


static int sd_send_cmd( int cmd, uint32_t arg, uint32_t * data )
{

#define CMD_BYTES	20

	int tx_bytes = CMD_BYTES;
	
	if (cmd == 17) // read single block
	{
		tx_bytes = SD_BUF_LEN;
	}

	memset(sdTxBuf, 0xFF, tx_bytes);
	
	sdTxBuf[0] = 0x40 | (cmd & 0x3F);
	sdTxBuf[1] = (arg >> 24) & 0xFF;
	sdTxBuf[2] = (arg >> 16) & 0xFF;
	sdTxBuf[3] = (arg >>  8) & 0xFF;
	sdTxBuf[4] = (arg >>  0) & 0xFF;
	
	int i;
	uint8_t crc = 0;
	
	for (i=0; i<5; i++) 
	{
		uint8_t d = sdTxBuf[i];
		int j;
		
		for (j=0; j<8; j++)
		{
			crc <<= 1;
			if (((d & 0x80)^(crc & 0x80)) != 0)
			{
				crc ^= 0x09;
			}
			d <<= 1;
		}   
	}

	crc = (crc<<1) | 1;
	
	sdTxBuf[5] = crc;
	


	AVR32_PDCA.channel[5].mar = (unsigned long) sdRxBuf;
	AVR32_PDCA.channel[5].tcr = tx_bytes ;
	AVR32_PDCA.channel[4].mar = (unsigned long) sdTxBuf;
	AVR32_PDCA.channel[4].tcr = tx_bytes ; // send buffer

	while (AVR32_PDCA.channel[4].ISR.trc == 0)
	{
		vTaskDelay(1);
	}
	
	for (i=0; i < tx_bytes; i++)
	{
		if (sdRxBuf[i] != 0xFF)
		{
			if (data != NULL)
			{
				if (cmd != 17) // read single block
				{				
				  *data = (sdRxBuf[i+1] << 24)
					| (sdRxBuf[i+2] << 16)
					| (sdRxBuf[i+3] << 8)
					| (sdRxBuf[i+4] );
				}
				else
				{
					*data = i+1;
				}					
			}
			return sdRxBuf[i];
		}
	}
	
	return -1;
}


static uint16_t tmp_buf[AUDIO_Q_TRANSFERLEN];

static audio_q_t * audio_tx_q;

static void vSDCardTask( void *pvParameters )
{
	AVR32_USART3.brgr = 8; // 2 MHz   (65.536 MHz / 4 / 4)  SCK
	
	AVR32_USART3.mr = 0x000409CE;
	   // CLKO, CPOL=0, no parity, CPHA=1,
	   //   CHRL=8, USCLKS=CLK_USART, Mode=SPI Master
	   
	
	AVR32_PDCA.channel[4].mr = AVR32_PDCA_BYTE; // 8 bit transfer
	AVR32_PDCA.channel[4].psr = AVR32_PDCA_PID_USART3_TX; // select peripherial
	// AVR32_PDCA.channel[4].mar = (unsigned long) sdTxBuf;
	// AVR32_PDCA.channel[4].tcr = SD_BUF_LEN ;
	
	
	AVR32_PDCA.channel[5].mr = AVR32_PDCA_BYTE; // 8 bit transfer
	AVR32_PDCA.channel[5].psr = AVR32_PDCA_PID_USART3_RX; // select peripherial
	/*
	AVR32_PDCA.channel[5].mar = (unsigned long) sdRxBuf;
	AVR32_PDCA.channel[5].tcr = SD_BUF_LEN ;
	*/
	AVR32_USART3.cr = 0x50; // RXEN + TXEN
	
	AVR32_PDCA.channel[5].cr = 1; // rx DMA enable
	AVR32_PDCA.channel[4].cr = 1; // tx DMA enable  
	
	memset(sdRxBuf, 0xFF, SD_BUF_LEN);
	memset(sdTxBuf, 0xFF, SD_BUF_LEN);
	
	vTaskDelay(1000);
	
	// send lots of clock pulses without CS
	
	AVR32_PDCA.channel[5].mar = (unsigned long) sdRxBuf;
	AVR32_PDCA.channel[5].tcr = SD_BUF_LEN ;
	AVR32_PDCA.channel[4].mar = (unsigned long) sdTxBuf;
	AVR32_PDCA.channel[4].tcr = SD_BUF_LEN ;
	
	gpio_set_pin_low(AVR32_PIN_PA04); // activate CS
	
	int state = 0;
	int acmd41 = 0;
	int ccs = 0;
	uint32_t ocr = 0;
	uint32_t cmd8data = 0;
	uint32_t ptr = 0;
	int blk_num = 20000;
	int i;
	
	for (;;)
	{
		int res = -1;
		
		switch(state)
		{
			case 0: // reset
				vTaskDelay(1000);
				res = sd_send_cmd(0, 0, 0);
				
				if (res == 1) // idle
				{
					state = 10; // initialize card
				}				
				break;
				
			case 10:
				vTaskDelay(100);
				res = sd_send_cmd(8, 0x01AA, &cmd8data);
				
				if (res < 0) // 8 not recognized
				{
					state = 2; // try with cmd1
				}
				
				if ((cmd8data & 0x0FFF) == 0x01AA)
				{
					state = 1;
				}
				break;
				
			case 1: // init card with ACMD41
				vTaskDelay(100);
				res = sd_send_cmd(55, 0, 0);
				
				if (res < 0) // 55 not recognized
				{
					state = 2; // try with cmd1
				}
				
				res = sd_send_cmd(41, 0x40FF8000, 0); // HCS set
				
				if (res < 0) // 41 not recognized
				{
					state = 2; // try with cmd1
				}
				
				if (res == 0) // ready
				{
					state = 3; // set block size
					acmd41 = 1;
				}
				break;
				
			case 2: // init card cmd1
				vTaskDelay(100);
				res = sd_send_cmd(1, 0, 0);
				
				if (res == 0) // ready
				{
					state = 3; // set block size
					acmd41 = 0;
				}
				break;
				
			case 3: // set block size 512
				
				res = sd_send_cmd(16, 512, 0);
			
				if (res == 0) // ready
				{
					state = 4; // get OCR
				}
				else
				{
					vTaskDelay(100);
				}
				break;
			
			case 4: // get OCR
				res = sd_send_cmd(58, 0, &ocr);
				
				if (res < 0) // no response
				{
					state = 5;
				}
			
				if (res == 0) // ready
				{
					ccs = (ocr & 0xC0000000) == 0xC0000000 ? 1 : 0;
					state = 5; // get block
				}
				else
				{
					vTaskDelay(100);
				}
				break;
				
			case 5:
			
				// vTaskDelay(5);
				
				res = sd_send_cmd( 17, (ccs == 0) ? (blk_num << 9) : blk_num, &ptr);
				
				blk_num ++;
				
				for (; ptr < SD_BUF_LEN; ptr++)
				{
					if (sdRxBuf[ptr] == 0xFE) // data token
					{
						int j;
						
						for (j=0; j < 8; j++)
						{
							for (i=0; i < AUDIO_Q_TRANSFERLEN; i++)
							{
								tmp_buf[i] = (sdRxBuf[ptr + 2 + ((i + (j*AUDIO_Q_TRANSFERLEN)) <<1)] << 8) |
									sdRxBuf[ptr + 1 + ((i + (j*AUDIO_Q_TRANSFERLEN)) <<1)];
							}
							
							audio_q_put( audio_tx_q, (const short *) tmp_buf);
							
							vTaskDelay(3);
						}
												
						
						
						break;
					}						
						
				}	
				
				for (; ptr < SD_BUF_LEN; ptr++)
				{
					if (sdRxBuf[ptr] == 0xFE) // data token
					break;
				}
				break;
		}
		
		
		
		char buf[9];
			
		vdisp_i2s(buf, 4, 10, 0, state);
		vdisp_prints_xy(0, 8, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 4, 10, 0, acmd41);
		vdisp_prints_xy(0, 16, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 4, 10, 0, ccs);
		vdisp_prints_xy(0, 24, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 8, 16, 1, ocr);
		vdisp_prints_xy(0, 32, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 4, 10, 0, res);
		vdisp_prints_xy(0, 40, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 4, 10, 0, ptr);
		vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, buf);
		
		vdisp_i2s(buf, 8, 10, 0, blk_num);
		vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, buf);
		
	}
}

void sdcard_init (audio_q_t * tx)
{
	audio_tx_q = tx;
	
	xTaskCreate( vSDCardTask, (signed char *) "SDCARD", 200, ( void * ) 0,  (tskIDLE_PRIORITY + 1), ( xTaskHandle * ) NULL );
	
}