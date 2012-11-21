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

#include "up_dstar/audio_q.h"

#include "up_dstar/fixpoint_math.h"
#include "up_dstar/settings.h"



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


static char spkr_volume = 0;

int wm8510_get_spkr_volume (void)
{
	return spkr_volume;
}

void wm8510_set_spkr_volume (int vol)
{
	if ((vol >= -57) && (vol <= 6))
	{
		spkr_volume = vol;
		send_wm8510(  54, (spkr_volume + 57) & 0x3F );
	}
}

static int beep_counter = 0;
static int beep_phase = 0;
static int beep_phase_incr = 0;
static int beep_volume = 0;
static int beep_fade_in = 0;


static int chip_init(void)
{
	beep_counter = 0;
	
	AVR32_TWI.cr = 0x24; // MSEN + SVDIS
	AVR32_TWI.mmr =  0x001A0100;    // DADR= 0011010   , One-byte internal device address, MREAD = 0
	
	if (send_wm8510(  0, 0x000) != 0) goto error;  // reset
	
	if (send_wm8510(  49, 0x003) != 0) goto error;  // TSDEN, VROI (charge output C slowly)
	
	if (send_wm8510(  3, 0x00D) != 0) goto error;  // Power Management 3:  MONOMIXEN, SPKMIXEN, DACEN
	
	if (send_wm8510(  2, 0x015) != 0) goto error;  // Power Management 2:  BOOSTEN, INPPGAEN, ADCEN
	
	if (send_wm8510(  14, 0x1E8) != 0) goto error;  // enable ADCOSR128, enable HPF  327Hz
	
	//if (send_wm8510(  33, 0x071) != 0) goto error;  // ALC hold 170ms, target level -27dBFS
	if (send_wm8510(  33, 0x077) != 0) goto error;  // ALC hold 170ms, target level -18dBFS

	if (send_wm8510(  34, 0x052) != 0) goto error;  // ALC attack 0.4ms, decay 13ms
 	
	if (send_wm8510(  10, 0x048) != 0) goto error;  // DACMU, DACOSR128
		
	if (send_wm8510(  1, 0x01B) != 0) goto error;  // Power Management 1:  MICBEN, BIASEN, VMIDSEL=5kohm
	//if (send_wm8510(  1, 0x00B) != 0) goto error;  // Power Management 1:  BIASEN, VMIDSEL=5kohm
	
	if (send_wm8510(  4, 0x018) != 0) goto error;  // Audio Interface: DSP/PCM mode, 16 bit
	
	vTaskDelay(300);
	
	if (send_wm8510(  1, 0x01F) != 0) goto error;  // Power Management 1:  MICBEN, BIASEN, BUFIOEN, VMIDSEL=5kohm
	//if (send_wm8510(  1, 0x00F) != 0) goto error;  // Power Management 1:   BIASEN, BUFIOEN, VMIDSEL=5kohm
	
	vTaskDelay(300);
	
	if (send_wm8510(  6, 0x0C0) != 0) goto error;  // Clock: MCLK / 8
	
	if (send_wm8510(  7, 0x00A) != 0) goto error;  // Sample rate 8kHz
	
	if (send_wm8510(  3, 0x0ED) != 0) goto error;  // Power Management 3:  MONOEN, MONOMIXEN, SPKNEN, SPKPEN, SPKMIXEN, DACEN
	
	vTaskDelay(100);
	
	if (send_wm8510(  10, 0x008) != 0) goto error;  // DACMU off, DACOSR128
	
	if (send_wm8510(  50, 0x001) != 0) goto error;  // DAC to Speaker
	
	if (send_wm8510(  32, 0x138) != 0) goto error;  // enable ALC
	
	wm8510_set_spkr_volume (SETTING_CHAR(C_SPKR_VOLUME));
	// if (send_wm8510(  54, (spkr_volume + 57) & 0x3F ) != 0 ) goto error; // speaker volume
	
	return 0;
	
	error:
		return 1;
}


static audio_q_t * audio_tx_q;
static audio_q_t * audio_rx_q;

// #define BUF_SIZE   (AUDIO_Q_TRANSFERLEN * 2)
#define BUF_SIZE   (AUDIO_Q_TRANSFERLEN)
static int16_t tx_buf0[BUF_SIZE];
static int16_t tx_buf1[BUF_SIZE];
static int16_t rx_buf0[BUF_SIZE];
static int16_t rx_buf1[BUF_SIZE];
static int16_t * tx_buf[2];
static int16_t * rx_buf[2];

static int curr_tx_buf = 0;
static int curr_rx_buf = 0;



static portTASK_FUNCTION( wm8510Task, pvParameters )
{
	int audio_state = 0;
	
	for(;;)
	{
		
		switch (audio_state)
		{
		case 0:
			vTaskDelay(1000);
			if (chip_init() == 0) // init successful
			{
				audio_state = 1;
				// vdisp_prints_xy(0, 40, VDISP_FONT_6x8, 0, "OK ");
				
				AVR32_PDCA.channel[2].mr = AVR32_PDCA_HALF_WORD; // 16 bit transfer
				AVR32_PDCA.channel[2].psr = AVR32_PDCA_PID_SSC_TX; // select peripherial
				AVR32_PDCA.channel[2].mar = (unsigned long) tx_buf[curr_tx_buf];
				AVR32_PDCA.channel[2].tcr = BUF_SIZE ; 
				
				audio_q_get( audio_tx_q, tx_buf[curr_tx_buf]); // first half
				// audio_q_get( audio_tx_q, tx_buf[curr_tx_buf] + AUDIO_Q_TRANSFERLEN);  // second half
				
				AVR32_PDCA.channel[3].mr = AVR32_PDCA_HALF_WORD; // 16 bit transfer
				AVR32_PDCA.channel[3].psr = AVR32_PDCA_PID_SSC_RX; // select peripherial
				AVR32_PDCA.channel[3].mar = (unsigned long) rx_buf[curr_rx_buf];
				AVR32_PDCA.channel[3].tcr = BUF_SIZE ; 
				
				AVR32_SSC.cr = 0x0101;  // enable TX + RX
				AVR32_PDCA.channel[3].cr = 1; // rx DMA enable
				AVR32_PDCA.channel[2].cr = 1; // tx DMA enable
			}
			else
			{
				// vdisp_prints_xy(0, 40, VDISP_FONT_6x8, 0, "ERR");
			}
			break;
			
		case 1:
			vTaskDelay(1);
			if ( (AVR32_PDCA.channel[2].tcrr == 0)
				&& (AVR32_PDCA.channel[2].tcr < BUF_SIZE))
			{
				curr_tx_buf ^= 1; // toggle buffer
				AVR32_PDCA.channel[2].marr = (unsigned long) tx_buf[curr_tx_buf];
				AVR32_PDCA.channel[2].tcrr = BUF_SIZE;
				
				audio_q_get( audio_tx_q, tx_buf[curr_tx_buf]); // first half
				// audio_q_get( audio_tx_q, tx_buf[curr_tx_buf] + AUDIO_Q_TRANSFERLEN); // second half
				
				if (beep_counter > 0)
				{
					int i;
					
					for (i=0; i < BUF_SIZE; i++)
					{
						int vol = beep_volume;
						
						if (beep_counter == 1) // at the end of the beep
						{
							vol = (BUF_SIZE - i) * beep_volume / BUF_SIZE; // fade out
						}
						else if (beep_fade_in != 0) // start of beep
						{
							beep_fade_in = 0;
							vol = (i + 1) * beep_volume / BUF_SIZE; // fade in
						}
						
						tx_buf[curr_tx_buf][i] = (tx_buf[curr_tx_buf][i] / 2) + 
							(vol * fixpoint_sin(beep_phase)) / 100;
						beep_phase += beep_phase_incr;
						if (beep_phase >= 360)
						{
							beep_phase -= 360;
						}
					}						
					
					beep_counter --;	
				}
				
			}			
			if ( (AVR32_PDCA.channel[3].tcrr == 0)
				&& (AVR32_PDCA.channel[3].tcr < BUF_SIZE))
			{
				curr_rx_buf ^= 1; // toggle buffer
				AVR32_PDCA.channel[3].marr = (unsigned long) rx_buf[curr_rx_buf];
				AVR32_PDCA.channel[3].tcrr = BUF_SIZE;
				
				// if (gpio_get_pin_value(AVR32_PIN_PA28) == 0)
				// {
				
				audio_q_put( audio_rx_q, rx_buf[curr_rx_buf]); // first half
				// audio_q_put( audio_rx_q, rx_buf[curr_rx_buf] + AUDIO_Q_TRANSFERLEN); // second half
				
				// }					
			}			
			break;
		}

	}
}	


#define SAMPLE_RATE 8000

void wm8510_beep(int duration_ms, int frequency_hz, int volume_percent)
{
	if ((duration_ms <= 0) || (volume_percent <= 0))
		return;

	beep_counter = duration_ms  *  SAMPLE_RATE / ( configTICK_RATE_HZ * BUF_SIZE ) ;
	
	beep_volume = volume_percent;
	
	if (beep_volume < 0)
	{
		beep_volume = 0;
	}
	
	if (beep_volume > 100)
	{
		beep_volume = 100;
	}
	
	beep_phase = 0;
	
	beep_phase_incr = (360 * frequency_hz) / SAMPLE_RATE;
	
	beep_fade_in = 1;
}



void wm8510Init( audio_q_t * tx, audio_q_t * rx )
{
	tx_buf[0] = tx_buf0;
	tx_buf[1] = tx_buf1;
	
	curr_tx_buf = 0;
	
	rx_buf[0] = rx_buf0;
	rx_buf[1] = rx_buf1;
	
	curr_rx_buf = 0;
	
	audio_tx_q = tx;
	audio_rx_q = rx;
	
	xTaskCreate( wm8510Task, ( signed char * ) "WM8510", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 2 , ( xTaskHandle * ) NULL );
	
}