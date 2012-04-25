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

#include "vdisp.h"

#include "up_io/eth.h"
#include "up_io/wm8510.h"

#include "gcc_builtin.h"

#include "up_dstar/audio_q.h"





#define BUF_SIZE 64

static uint32_t out_buf1[BUF_SIZE];
static uint32_t out_buf2[BUF_SIZE];
static uint32_t in_buf1[BUF_SIZE];
static uint32_t in_buf2[BUF_SIZE];

static unsigned short * sound_buf;

#define SOUND_BUF_SIZE 160
static int sound_buf2_ptr = 0;

static unsigned short sound_buf2[SOUND_BUF_SIZE];

static uint32_t counter = 0;


static audio_q_t * audio_output_q;
static audio_q_t * audio_input_q;




static unsigned short int chan_tx_data[24] =
  {
	0x13ec,
	0x0000,

	0x1030,
	0x4000,
	0x0000,
	0x0000,
	0x0048,

	0x0000,
	0x0000,
	0x0000,

	0xFAFF,  //  FF = no DTMF
	// 0x008C,  // DTMF A

	// 0x8004,  // encoder
	0x8000,  // decoder


	0x9e8d,  // silence data (normal)
	0x3288,
	0x261a,
	0x3f61,
	0xe800,
	0x0000,
	
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000
  };

static int chan_tx_state = 0;


static int16_t abuf[AUDIO_Q_TRANSFERLEN];
static int16_t abuf_ptr;

static int16_t encbuf[AUDIO_Q_TRANSFERLEN];


unsigned char tmp_ambe_buf[9000];
int tmp_ambe_write_buf_ptr = 0;
int tmp_ambe_read_buf_ptr = 0;
int ambe_encode = 0;
static int ambe_encoder_state = 0;
static int ambe_encoder_counter = 0;

static int silence_counter = 0;

static void set_silence(void)
{
	tmp_ambe_buf[0] = 0x9e;
	tmp_ambe_buf[1] = 0x8d; 
	tmp_ambe_buf[2] = 0x32;
	tmp_ambe_buf[3] = 0x88;
	tmp_ambe_buf[4] = 0x26;
	tmp_ambe_buf[5] = 0x1a;
	tmp_ambe_buf[6] = 0x3f;
	tmp_ambe_buf[7] = 0x61;
	tmp_ambe_buf[8] = 0xe8;
	
	tmp_ambe_write_buf_ptr = 9;
}



static void put_sound_data( unsigned short d )
{
	char buf[10];
	
	sound_buf2[ sound_buf2_ptr ] = d;
	
	sound_buf2_ptr++;
	
	if (sound_buf2_ptr >= SOUND_BUF_SIZE)
	{
		memcpy( sound_buf, sound_buf2, sizeof sound_buf2 );
		sound_buf2_ptr = 0;
		eth_send_vdisp_frame();
		
		chan_tx_state = 1; // send new request to AMBE
		
		counter ++;
		
		// vdisp_i2s(buf, 8, 10, 0, counter);
		
		extern int audio_error;
		
		vdisp_i2s(buf, 8, 10, 0, audio_error);
		vdisp_prints_xy( 60, 56, VDISP_FONT_6x8, 0, buf );
		
		extern int audio_max;
		
		vdisp_i2s(buf, 8, 10, 0, audio_max);
		vdisp_prints_xy( 60, 48, VDISP_FONT_6x8, 0, buf );
		
	}
}


static portTASK_FUNCTION( ambeTask, pvParameters )
{
	gpio_set_pin_low(AVR32_PIN_PB20); // RESETN
	
	AVR32_SPI0.mr = 0x110F0007;  // PCSDEC PS MSTR  17 clocks delay  -> exactly 8kHz!!
	
	AVR32_SPI0.csr0 = 0x00001E84; //  16 Bit,  SCBR=30, CSNAAT, DLYBCT=0
	AVR32_SPI0.csr1 = 0x00001E84; //  16 Bit,  SCBR=30, CSNAAT, DLYBCT=0
	AVR32_SPI0.csr2 = 0x00001E84; //  16 Bit,  SCBR=30, CSNAAT, DLYBCT=0
	AVR32_SPI0.csr3 = 0x00001E84; //  16 Bit,  SCBR=30, CSNAAT, DLYBCT=0
	
	AVR32_SPI0.CR.spien = 1;

#define AMBE_CS_IDLE	0x000B0000
#define AMBE_CS_CODEC	0x000D0000
#define AMBE_CS_CHAN	0x000E0000

	AVR32_SPI0.tdr = AMBE_CS_IDLE;


	
	int i;
	for (i=0; i < BUF_SIZE; i++)
	{
		switch (i & 0x03)
		{
			case 0:
				out_buf1[i] = AMBE_CS_CODEC;
				out_buf2[i] = AMBE_CS_CODEC;
				break;
				
			default:
				out_buf1[i] = AMBE_CS_IDLE;
				out_buf2[i] = AMBE_CS_IDLE;
				break;
		}
		
		in_buf1[i] = 0;
		in_buf2[i] = 0;
	}
	
	AVR32_PDCA.channel[0].mr = AVR32_PDCA_WORD; // 32 bit transfer
	AVR32_PDCA.channel[0].psr = AVR32_PDCA_PID_SPI0_TX; // select peripherial
	AVR32_PDCA.channel[0].mar = (unsigned long) out_buf1;
	AVR32_PDCA.channel[0].tcr = BUF_SIZE ; 
	
	AVR32_PDCA.channel[1].mr = AVR32_PDCA_WORD; // 32 bit transfer
	AVR32_PDCA.channel[1].psr = AVR32_PDCA_PID_SPI0_RX; // select peripherial
	AVR32_PDCA.channel[1].mar = (unsigned long) in_buf1;
	AVR32_PDCA.channel[1].tcr = BUF_SIZE ; 
	
	vTaskDelay(1);
	gpio_set_pin_high(AVR32_PIN_PB20);


	vTaskDelay(100);
	
	AVR32_PDCA.channel[1].cr = 1; // rx enable
	AVR32_PDCA.channel[0].cr = 1; // tx enable
	
	
	char buf_ready_rx = 1;
	char buf_ready_tx = 1;

/*	
	int tx_ptr = 0;
	extern unsigned char Sprache[];
	#define	Anzahl_SprachBytes	(7830 + 200 * 9)
*/

	for( ;; )
	{
		vTaskDelay(1);
		
		if ( (AVR32_PDCA.channel[0].tcrr == 0)
			&& (AVR32_PDCA.channel[0].tcr < BUF_SIZE))
		{
			switch (buf_ready_tx)
			{
				case 1:
					AVR32_PDCA.channel[0].marr = (unsigned long) out_buf2;
					buf_ready_tx = 2;
					break;
				case 2:
					AVR32_PDCA.channel[0].marr = (unsigned long) out_buf1;
					buf_ready_tx = 1;
					break;
			}
			AVR32_PDCA.channel[0].tcrr = BUF_SIZE;
			
			uint32_t * b = out_buf1;
			
			if (buf_ready_tx == 2)
			{
				b = out_buf2;
			}
			
			for (i=2; i < BUF_SIZE; i+=4)  // CHAN part of the buffer
			{
				switch (chan_tx_state)
				{
					case 1:
						b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) ];
						break;
					case 2:
						if ((i >> 2) < 8)
						{
							b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) + 16 ];
						}
						else
						{
							b[i] = AMBE_CS_IDLE;
						}
						break;
					default:
						b[i] = AMBE_CS_IDLE;
				}
			}
			
			audio_q_get (audio_input_q, encbuf);
			
			for (i=0; i < BUF_SIZE; i+=4)  // CODEC part of the buffer
			{
				b[i] = AMBE_CS_CODEC | ((unsigned short ) encbuf[ (i >> 2) ]);
			}				
			
			switch (chan_tx_state)
			{
				case 1:
					chan_tx_state = 2;
					break;
				case 2:					
					chan_tx_state = 0;
					
					if (ambe_encode == 1)
					{
						chan_tx_data[11] = 0x8007; // encoder
					}
					else
					{
						chan_tx_data[11] = 0x8003; // decoder,  do not set rate again
						
						chan_tx_data[12] =	(tmp_ambe_buf[tmp_ambe_read_buf_ptr + 0] << 8) |
											 tmp_ambe_buf[tmp_ambe_read_buf_ptr + 1];
						chan_tx_data[13] =	(tmp_ambe_buf[tmp_ambe_read_buf_ptr + 2] << 8) |
											 tmp_ambe_buf[tmp_ambe_read_buf_ptr + 3];					 
						chan_tx_data[14] =	(tmp_ambe_buf[tmp_ambe_read_buf_ptr + 4] << 8) |
											 tmp_ambe_buf[tmp_ambe_read_buf_ptr + 5];
						chan_tx_data[15] =	(tmp_ambe_buf[tmp_ambe_read_buf_ptr + 6] << 8) |
											 tmp_ambe_buf[tmp_ambe_read_buf_ptr + 7];
						chan_tx_data[16] =	(tmp_ambe_buf[tmp_ambe_read_buf_ptr + 8] << 8);
						
						tmp_ambe_read_buf_ptr += 9;
						
						
						
						if ((tmp_ambe_read_buf_ptr >= ((sizeof tmp_ambe_buf) - 9))
							 || (tmp_ambe_read_buf_ptr >= tmp_ambe_write_buf_ptr))
						{
							tmp_ambe_read_buf_ptr = 0;
							set_silence();
						}
						else
						{
							silence_counter = 200;
						}
					}
					
					/*
					
					chan_tx_data[12] = (Sprache[tx_ptr + 0] << 8) | Sprache[tx_ptr + 1];
					chan_tx_data[13] = (Sprache[tx_ptr + 2] << 8) | Sprache[tx_ptr + 3];
					chan_tx_data[14] = (Sprache[tx_ptr + 4] << 8) | Sprache[tx_ptr + 5];
					chan_tx_data[15] = (Sprache[tx_ptr + 6] << 8) | Sprache[tx_ptr + 7];
					chan_tx_data[16] = (Sprache[tx_ptr + 8] << 8) ;
					
					tx_ptr += 9;
					if (tx_ptr >= Anzahl_SprachBytes)
					{
						tx_ptr = 0;
					}
					
					*/
					break;
			}
		}
		
		if ( (AVR32_PDCA.channel[1].tcrr == 0)
			&& (AVR32_PDCA.channel[1].tcr < BUF_SIZE))
		{
			switch (buf_ready_rx)
			{
				case 1:
					AVR32_PDCA.channel[1].marr = (unsigned long) in_buf2;
					buf_ready_rx = 2;
					break;
				case 2:
					AVR32_PDCA.channel[1].marr = (unsigned long) in_buf1;
					buf_ready_rx = 1;
					break;
			}
			AVR32_PDCA.channel[1].tcrr = BUF_SIZE;
			
			uint32_t * b = in_buf1;
			
			if (buf_ready_rx == 2)
			{
				b = in_buf2;
			}
			
			for (i=0; i < BUF_SIZE; i++)
			{
				if ((b[i] & 0x000F0000) == AMBE_CS_CODEC)
				{
					put_sound_data(b[i] & 0x0000FFFF);
					// wm8510_put_audio_sample( (int16_t) (b[i] & 0x0000FFFF));
					
					abuf[abuf_ptr] = (int16_t) (b[i] & 0x0000FFFF);
					abuf_ptr ++;
					if (abuf_ptr >= AUDIO_Q_TRANSFERLEN)
					{
						abuf_ptr = 0;
						
						if (silence_counter > 0)
						{
							silence_counter --;
							audio_q_put( audio_output_q, abuf );
						}													
					}
				}
				else if ((b[i] & 0x000F0000) == AMBE_CS_CHAN)
				{
					if (ambe_encode == 1)
					{
					switch (ambe_encoder_state)
					{
					case 0:
						if ((b[i] & 0x0000FFFF) == 0x13ec)
						{
							ambe_encoder_state = 1;
							ambe_encoder_counter = 0;
						}
						break;
						
					case 1:
						ambe_encoder_counter ++;
						switch (ambe_encoder_counter)
						{
							case 12:
							case 13:
							case 14:
							case 15:
								tmp_ambe_buf[tmp_ambe_write_buf_ptr] = (b[i] & 0x0000FF00) >> 8;
								tmp_ambe_buf[tmp_ambe_write_buf_ptr+1] = (b[i] & 0x000000FF);
								tmp_ambe_write_buf_ptr += 2;
								break;
							
							case 16:
								tmp_ambe_buf[tmp_ambe_write_buf_ptr] = (b[i] & 0x0000FF00) >> 8;
								tmp_ambe_write_buf_ptr++;
								break;
							case 23:
								ambe_encoder_state = 0;
								break;
						}
						break;
					}
					
					if (tmp_ambe_write_buf_ptr >= ((sizeof tmp_ambe_buf) - 9))
					{
						ambe_stop_encode();
					}
					
					} // if ambe_encode					
					
				}
			}
			
			
		}
		
		 
		// AVR32_SPI0.tdr = 0x000A0000;
	}
} 


void ambe_start_encode(void)
{
	if (ambe_encode == 0)
	{
		ambe_encoder_state = 0;
		ambe_encoder_counter = 0;
		set_silence();
		ambe_encode = 1;
	}
}

void ambe_stop_encode(void)
{
	if (ambe_encode != 0)
	{
		tmp_ambe_read_buf_ptr = 0;
		ambe_encode = 0;
	}
}	



void ambeInit( unsigned char * pixelBuf, audio_q_t * decoded_audio, audio_q_t * input_audio )
{
	sound_buf = (unsigned short *)  (pixelBuf + 1024);

	audio_output_q = decoded_audio;
	audio_input_q = input_audio;
	abuf_ptr = 0;
	
	ambe_encode = 0;
	set_silence();
	
	xTaskCreate( ambeTask, ( signed char * ) "AMBE", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 2 , ( xTaskHandle * ) NULL );

}
