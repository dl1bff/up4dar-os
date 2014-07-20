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
#include "up_dstar/ambe_q.h"
#include "settings.h"
#include "up_io/serial2.h"
#include "fixpoint_math.h"



#define BUF_SIZE  (AUDIO_Q_TRANSFERLEN * 4)

static uint32_t out_buf1[BUF_SIZE];
static uint32_t out_buf2[BUF_SIZE];
static uint32_t in_buf1[BUF_SIZE];
static uint32_t in_buf2[BUF_SIZE];

// static unsigned short * sound_buf;

#define SOUND_BUF_SIZE 160
static int sound_buf2_ptr = 0;

static unsigned short sound_buf2[SOUND_BUF_SIZE];

static uint32_t counter = 0;


static audio_q_t * audio_output_q;
static audio_q_t * audio_input_q;

static ambe_q_t ambe_output_q;
static ambe_q_t * ambe_input_q;

// our special LFI indicator:  {0x9E, 0x8D, 0x36, 0x98, 0x66, 0x1E, 0x3F, 0x23, 0xE4}
static const uint8_t ambe_lfi_data_sd[36] = 
  {0xF0, 0x0F, 0xFF, 0xF0, 0xF0, 0x00, 0xFF, 0x0F,
	  0x00, 0xFF,
	  0x0F, 0xF0, 0xF0, 0x0F, 0xF0, 0x00, 0x0F, 0xF0,
	  0x0F, 0xF0, 0x00, 0x0F, 0xFF, 0xF0, 0x00, 0xFF,
	  0xFF, 0xFF,
	  0x00, 0xF0, 0x00, 0xFF, 0xFF, 0xF0, 0x0F, 0x00};  // SD version of LFI indicator
	  

static unsigned short int chan_tx_data[60] =
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
	0x0000,
	
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
		
	0x0000,  
	0x0000,
	0x0000,
	0x0000,
	0x0000,
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


unsigned char tmp_ambe_buf[AMBE_Q_DATASIZE];

int input_counter = 0;

int ambe_encode = 0;
static int ambe_encoder_state = 0;
static int ambe_encoder_counter = 0;

static int silence_counter = 0;


#define AUTOMUTE_VALUE	120

static int automute = 0;
static int ref_timer = 0;
bool ref_timer_break = false;

void ambe_set_automute(int enable)
{
	if (enable != 0)
	{
		automute = AUTOMUTE_VALUE;
	}
	else
	{
		automute = 0;
	}
}

int ambe_get_automute(void)
{
	return automute;
}

int ambe_get_ref_timer(void)
{
	return ref_timer;
}

void ambe_set_ref_timer(int enable)
{
	if (enable != 0)
	{
		if (SETTING_CHAR(C_REF_TIMER) == 0)
			ref_timer = -1;
		else if (SETTING_CHAR(C_REF_TIMER) == 1)
			ref_timer = (5 * 60) * 2;
		else if (SETTING_CHAR(C_REF_TIMER) == 2)
			ref_timer = (10 * 60) * 2;
		else if (SETTING_CHAR(C_REF_TIMER) == 3)
			ref_timer = (15 * 60) * 2;
		else if (SETTING_CHAR(C_REF_TIMER) == 4)
			ref_timer = (20 * 60) * 2;
		else if (SETTING_CHAR(C_REF_TIMER) == 5)
			ref_timer = (30 * 60) * 2;
		else if (SETTING_CHAR(C_REF_TIMER) == 6)
			ref_timer = (40 * 60) * 2;
	}
	else
	{
		ref_timer = -1;
	}
}

void ambe_ref_timer_break(int enable)
{
	if ((enable != 0) && (ref_timer > 0) && (!ref_timer_break))
	{
		ref_timer_break = true;
		ambe_set_ref_timer(0);
	}
	else if (ref_timer_break)
	{
		ref_timer_break = false;
	
		ambe_set_ref_timer(1);
	}
}

void ambe_service(void)
{
	if (automute > 0)
	{
		automute --;
	}
	
	if (ref_timer > 0)
	{
		ref_timer --;
	}
}

static void put_sound_data( unsigned short d )
{
	// char buf[10];
	
	sound_buf2[ sound_buf2_ptr ] = d;
	
	sound_buf2_ptr++;
	
	if (sound_buf2_ptr >= SOUND_BUF_SIZE)
	{
		// memcpy( sound_buf, sound_buf2, sizeof sound_buf2 );
		sound_buf2_ptr = 0;
		
		//eth_send_vdisp_frame();
		
		chan_tx_state = 1; // send new request to AMBE
		
		counter ++;
		
		// vdisp_i2s(buf, 8, 10, 0, counter);
		
		/*
		extern int audio_error;
		
		vdisp_i2s(buf, 8, 10, 0, audio_error);
		vdisp_prints_xy( 60, 56, VDISP_FONT_6x8, 0, buf );
		
		extern int audio_max;
		
		vdisp_i2s(buf, 8, 10, 0, audio_max);
		vdisp_prints_xy( 60, 48, VDISP_FONT_6x8, 0, buf );
		*/
		
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
	

	int audio_meter_sample_counter = 0;
#define AUDIO_METER_NUM_SAMPLES 160
	
	int audio_meter_square_sum = 0;
	
	char audio_meter_max_value = 0;
	char audio_meter_hold_timer = 0;

	char audio_clip = 0;

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
					/*
					case 1:
						b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) ];
						break;
					case 2:
						b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) + 16 ];
						break;
					case 3:
						b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) + 32 ];
						break;
					case 4:
						if ((i >> 2) < 12)
						{
							b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) + 48 ];
						}
						else
						{
							b[i] = AMBE_CS_IDLE;
						}
						break;
						*/
					case 1:
						b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) ];
						break;
					case 2:
						if ((i >> 2) < 28)
						{
							b[i] = AMBE_CS_CHAN | chan_tx_data[ (i >> 2) + 32 ];
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
				short sample = encbuf[ (i >> 2) ];
				
			
				audio_meter_square_sum += (sample*sample) >> 7;
				
				audio_meter_sample_counter++;
				
				if (audio_meter_sample_counter >= AUDIO_METER_NUM_SAMPLES)
				{
					audio_meter_sample_counter = 0;
					
					//char buf[5];
					
					unsigned int v = (unsigned int) (-1 * fixpoint_milliBel( audio_meter_square_sum));
					
					//vdisp_i2s(buf, 4, 10, 1, v);
					//vdisp_prints_xy(0, 32, VDISP_FONT_6x8, 0, buf);
					//serial_puts(0, buf);
					//serial_puts(0, "\n");
					
					v /= 100;
					
					if (v < audio_meter_max_value)
					{
						
						audio_meter_max_value = v;
						audio_meter_hold_timer = 50;
					}
					
					char buf[4];
					vdisp_i2s(buf+1, 2, 10, 1, audio_meter_max_value);
					buf[0] = '-';
					vd_prints_xy(VDISP_AUDIO_LAYER, 69, 25, VDISP_FONT_6x8, audio_clip, buf);
					
					for (i=0; i < 104; i+=8)
					{
						int j;
						int tmp_byte = 0;
						
						#define BIT7SET 0x80
						
						for (j=0; j < 8; j++)
						{
							int pixel = ((i+j) > v) ? BIT7SET : 0;
							
							if ((i+j) == audio_meter_max_value)
							{
								pixel = BIT7SET;
							}
							
							tmp_byte = (tmp_byte >> 1) | pixel;
						}
						
						#undef BIT7SET
						
						vd_set_pixel(VDISP_AUDIO_LAYER, 105-i, 36, 0, tmp_byte, 8);
						vd_set_pixel(VDISP_AUDIO_LAYER, 105-i, 37, 0, tmp_byte, 8);
						vd_set_pixel(VDISP_AUDIO_LAYER, 105-i, 38, 0, tmp_byte, 8);
					}
					
					if (audio_meter_hold_timer > 0)
					{
						audio_meter_hold_timer --;
					}
					
					if (audio_meter_hold_timer == 0)
					{
						audio_meter_max_value = 99;
						audio_clip = 0;
					}
					
					audio_meter_square_sum = 0;
				}


				/*
				if (sample > 3276)
				{
					sample = 3276;
					audio_clip = 1;
				}
				else if (sample < -3276)
				{
					sample = -3276;
					audio_clip = 1;
				}
				
				b[i] = AMBE_CS_CODEC | ((unsigned short ) (sample * 10)); // x10 = 20dB Gain
				*/
				
				if (sample > 8191)
				{
					sample = 8191;
					audio_clip = 1;
				}
				else if (sample < -8191)
				{
					sample = -8191;
					audio_clip = 1;
				}
				
				b[i] = AMBE_CS_CODEC | ((unsigned short ) (sample * 4)); // x4 = 12dB Gain
			}				
			
			switch (chan_tx_state)
			{
				case 1:
					chan_tx_state = 2;
					break;
				case 2:
					chan_tx_state = 3;
					break;
				case 3:
					chan_tx_state = 4;
					break;
				case 4:					
					chan_tx_state = 0;
					
					if (ambe_encode == 1)
					{
						chan_tx_data[11] = 0x8007; // encoder
						
						for (i=12; i < 60; i++)
						{
							chan_tx_data[i] = 0x0000;
						}
					}
					else
					{
												
						if (ambe_q_get_sd (& ambe_output_q, (uint8_t *) (chan_tx_data + 12)) == 0)
						{
							 // if buffer not empty set silence_counter
							silence_counter = 10;  // output audio for another 10 * AUDIO_Q_TRANSFERLEN samples
													// after queue is empty
						}
						
						if (silence_counter == 0) // no AMBE data received for some time
						{
							chan_tx_data[11] = 0x800b; // decoder,  do not set rate again, enable standard sleep mode
						}
						else
						{
							chan_tx_data[11] = 0x8003; // decoder,  do not set rate again							
						}							
						
						if (memcmp((chan_tx_data + 12), ambe_lfi_data_sd, sizeof ambe_lfi_data_sd) == 0)
						{
							chan_tx_data[1] = 0x0080; // set lost frame indicator bit
						}
						else
						{
							chan_tx_data[1] = 0x0000; // clear lost frame indicator bit
						}							
					}
					
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
					
					abuf[abuf_ptr] = (int16_t) (b[i] & 0x0000FFFF);
					abuf_ptr ++;
					if (abuf_ptr >= AUDIO_Q_TRANSFERLEN)
					{
						abuf_ptr = 0;
						
						if (silence_counter > 0)
						{
							silence_counter --;
							
							if (automute == 0)
							{
								audio_q_put( audio_output_q, abuf );
								
								if (silence_counter == 0)
								{
									wm8510_beep(
										SETTING_SHORT(S_STANDBY_BEEP_DURATION),
										SETTING_SHORT(S_STANDBY_BEEP_FREQUENCY),
										SETTING_CHAR(C_STANDBY_BEEP_VOLUME)
									);
								}
							}
							else
							{  // automute running, extend mute if silence_counter > 0
								automute = AUTOMUTE_VALUE;
							}
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
								tmp_ambe_buf[(ambe_encoder_counter - 12) << 1] = (b[i] & 0x0000FF00) >> 8;
								tmp_ambe_buf[((ambe_encoder_counter - 12) << 1) + 1] = (b[i] & 0x000000FF);
								break;
							
							case 16:
								tmp_ambe_buf[8] = (b[i] & 0x0000FF00) >> 8;
								if (ambe_q_put(ambe_input_q, tmp_ambe_buf) != 0) // queue full
								{
									ambe_stop_encode();
								}
								break;
							case 59:
								ambe_encoder_state = 0;
								break;
						}
						break;
					}
					
					} // if ambe_encode					
					
				}
			}
			
			
		}
		
	}
} 


void ambe_start_encode(void)
{
	if (ambe_encode == 0)
	{
		ambe_encoder_state = 0;
		ambe_encoder_counter = 0;
		ambe_encode = 1;
		// vdisp_prints_xy( 0, 0, VDISP_FONT_6x8, 1, " TX " );
	}
}

void ambe_stop_encode(void)
{
	if (ambe_encode != 0)
	{
		ambe_encode = 0;
		// vdisp_prints_xy( 0, 0, VDISP_FONT_6x8, 0, "    " );
	}
}




void ambe_input_data( const uint8_t * d)
{
	ambe_q_put ( & ambe_output_q, d );
}

void ambe_input_data_sd( const uint8_t * d)
{
	ambe_q_put_sd ( & ambe_output_q, d );
	// ambe_output_q.state = 1; // "fast" option
}

void ambe_init( audio_q_t * decoded_audio, audio_q_t * input_audio,
		ambe_q_t * microphone )
{
	// sound_buf = (unsigned short *)  (pixelBuf + 1024);

	audio_output_q = decoded_audio;
	audio_input_q = input_audio;
	abuf_ptr = 0;
	
	ambe_q_initialize( & ambe_output_q );
	
	ambe_input_q = microphone;
	
	ambe_encode = 0;
	
	xTaskCreate( ambeTask, ( signed char * ) "AMBE", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 2 , ( xTaskHandle * ) NULL );

}
