
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
 * txtask.c
 *
 * Created: 30.03.2013 06:47:04
 *  Author: mdirska
 */ 


#include <asf.h>

#include "board.h"
#include "gpio.h"
#include "power_clocks_lib.h"


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "vdisp.h"
#include "ambe.h"
#include "gcc_builtin.h"
#include "dcs.h"
#include "ambe_q.h"
#include "rtclock.h"
#include "up_io\wm8510.h"
#include "dstar.h"
#include "up_net\snmp_data.h"
#include "gps.h"
#include "phycomm.h"
#include "settings.h"
#include "up_app\a_lib.h"
#include "txtask.h"
#include "up_app\a_lib_internal.h"
#include "up_crypto\up_crypto.h"



static ambe_q_t * microphone;




void set_phy_parameters(void)
{
	uint8_t value;
	
	value = SETTING_SHORT(S_PHY_TXDELAY) & 0xFF;
	snmp_set_phy_sysparam(1, &value, 1);
	value = SETTING_CHAR(C_PHY_TXGAIN) & 0xFF;
	snmp_set_phy_sysparam(2, &value, 1);
	value = SETTING_CHAR(C_PHY_RXINV) & 0xFF;
	snmp_set_phy_sysparam(3, &value, 1);
	value = SETTING_CHAR(C_PHY_TXDCSHIFT) & 0xFF;
	snmp_set_phy_sysparam(4, &value, 1);
	
	snmp_set_phy_sysparam_raw(5, (uint8_t *) repeater_callsign, CALLSIGN_LENGTH);
	
	/* 
	value = SETTING_SHORT(S_PHY_MATFST) & 0xFF;
	snmp_set_phy_sysparam(5, &value, 1);
	value = SETTING_SHORT(S_PHY_LENGTHOFVW) & 0xFF;
	snmp_set_phy_sysparam(6, &value, 1);
	*/
}

#define DLE 0x10
#define STX 0x02
#define ETX 0x03

static void send_cmd(const char* Befehl, const short size){
	const char* nachricht = Befehl;
	
	char  buf[100];
	char  zeichen;
	short ind = 2;
	
	
	buf[0] = DLE;
	buf[1] = STX;
	
	for (int i=0; i<size; ++i){
		zeichen = *nachricht++;
		if (zeichen == DLE){
			buf[ind++] = DLE;
			buf[ind++] = DLE;
		} else {
			buf[ind++] = zeichen;
		}
	}
	
	buf[ind++] = DLE;
	buf[ind++] = ETX;
	
	phyCommSend(buf, ind);
}



static const char tx_on[1] = {0x10};
static char header[40];

static char send_voice[11];
static char send_data [ 4];

// static const char YOUR[9] = "CQCQCQ  ";
// static const char RPT2[9] = "DB0DF  G";
// static const char RPT1[9] = "DB0DF  B";
// static const char MY1[9]  = "DL1BFF  ";
// static const char MY2[5]  = "    ";

// static int phy_frame_counter = 0;
static int txmsg_counter = 0;

static const char direct_callsign[8] = "DIRECT  ";

static const uint8_t end_sequence_v[9] = {
	0x55, 0x55, 0x55, 0x55, 0xc8, 0x7a, 0x00, 0x00, 0x00
};

static const uint8_t end_sequence_d[3] = {
	0x55 ^ 0x70,
	0x55 ^ 0x4F,
	0x55 ^ 0x93
};

static void phy_start_tx(void)
{

	// Schalte UP4DAR auf Senden um
	
    send_cmd(tx_on, 1);
	
	// Bereite einen Header vor
	
	header[0] = 0x20;
	header[1] = (SETTING_CHAR(C_DV_DIRECT) == 1) ? 0 :	// "1st control byte"
				  (1 << 6)	// Setze den Repeater-Flag
				;
				
	
	
	header[2] = 0x0;				// "2nd control byte"
	header[3] = 0x0;				// "3rd control byte"
	
	for (short i=0; i<CALLSIGN_LENGTH; ++i){
		if (SETTING_CHAR(C_DV_DIRECT) == 1)
		{
			header[4+i] = direct_callsign[i];
		}
		else
		{
			header[4+i] = settings.s.rpt2[ ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH) + i];
		}		
	}
	
	for (short i=0; i<CALLSIGN_LENGTH; ++i){
		if (SETTING_CHAR(C_DV_DIRECT) == 1)
		{
			header[12+i] = direct_callsign[i];
		}
		else
		{
			header[12+i] = settings.s.rpt1[ ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH) + i];
		}
	}
	
	for (short i=0; i<CALLSIGN_LENGTH; ++i){
		header[20+i] = settings.s.urcall[ ((SETTING_CHAR(C_DV_USE_URCALL_SETTING  ) - 1)*CALLSIGN_LENGTH) + i];
	}
	
	for (short i=0; i<CALLSIGN_LENGTH; ++i){
		header[28+i] = settings.s.my_callsign[i];
	}
	
	for (short i=0; i<CALLSIGN_EXT_LENGTH; ++i){
		header[36+i] = settings.s.my_ext[i];
	}
	
	// Bis zu 70ms kann man sich Zeit lassen, bevor die Header-Daten uebergeben werden.
	// Die genaue Wartezeit ist natruerlich von TX-DELAY abhaengig.
	//usleep(70000);
	
	// vTaskDelay (50); // 50ms
	
	send_cmd(header, 40);
	
	// phy_frame_counter = 0;
	txmsg_counter = 0;
}

//phy_start_tx_net(header_crc_result, header);
static void phy_start_tx_hotspot(uint8_t crc_result, uint8_t * rx_header)
{

	// Schalte UP4DAR auf Senden um
	
	send_cmd(tx_on, 1);
	
	// Bereite einen Header vor
	
	header[0] = 0x20;  // send header command
	header[1] = 0x00;
	header[2] = 0x00;				// "2nd control byte"
	header[3] = 0x00;				// "3rd control byte"
	
	// RPT1 and RPT2
	for (short i=0; i<CALLSIGN_LENGTH; i++) 
	{
		header[4+i] = settings.s.my_callsign[i];
	}
	
	for (short i=0; i<CALLSIGN_LENGTH; i++)
	{
		header[12+i] = settings.s.my_callsign[i];
	}
	
	
	for (short i=0; i<CALLSIGN_LENGTH; ++i){
		header[20+i] = "CQCQCQ  "[i];
	}
	
	if (crc_result == DSTAR_HEADER_OK)
	{
		// URCALL is original URCALL from RX
		for (short i=0; i<CALLSIGN_LENGTH; ++i){
			header[28+i] = rx_header[27+i];
		}
		
		for (short i=0; i<CALLSIGN_EXT_LENGTH; ++i){
			header[36+i] = rx_header[35+i];
		}
	}
	else
	{
		for (short i=0; i<CALLSIGN_LENGTH; ++i){
			header[28+i] = ' ';
		}
		
		for (short i=0; i<CALLSIGN_EXT_LENGTH; ++i){
			header[36+i] = ' ';
		}
	}
	
	send_cmd(header, 40);
}


static int slow_data_count;
static uint8_t slow_data[5];

// const char dstar_tx_msg[20] = "Michael, Berlin, D23";
// --------------------------- 12345678901234567890

static void send_phy ( const unsigned char * d, char phy_frame_counter )
{
	send_voice[0] = 0x21;
	send_voice[1] = 0x01;
	
	for (short k=0; k<9; ++k)
	{
		send_voice[2+k] = d[k];
	}
			
	send_cmd(send_voice, 11);

	if (phy_frame_counter > 0)
	{
		send_data[0] = 0x22;
		
		if ((txmsg_counter == 0) && (phy_frame_counter >= 1) && (phy_frame_counter <= 8))
		{
			int i = (phy_frame_counter - 1) >> 1;
			if (phy_frame_counter & 1)
			{
				send_data[1] = 0x40 + i;
				send_data[2] = settings.s.txmsg[ i * 5 + 0 ];
				send_data[3] = settings.s.txmsg[ i * 5 + 1 ];
			}
			else
			{
				send_data[1] = settings.s.txmsg[ i * 5 + 2 ];
				send_data[2] = settings.s.txmsg[ i * 5 + 3 ];
				send_data[3] = settings.s.txmsg[ i * 5 + 4 ];
			}
		}
		else
		{
				if (phy_frame_counter & 1)
				{
					slow_data_count = gps_get_slow_data(slow_data);
					
					if (slow_data_count == 0)
					{
						send_data[1] = 0x66;
						send_data[2] = 0x66;
						send_data[3] = 0x66;
					}
					else
					{
						send_data[1] = (0x30 + slow_data_count);
						send_data[2] = slow_data[ 0 ];
						send_data[3] = slow_data[ 1 ];
					}
				}
				else
				{
					if (slow_data_count <= 2)
					{
						send_data[1] = 0x66;
						send_data[2] = 0x66;
						send_data[3] = 0x66;
					}
					else
					{
						send_data[1] = slow_data[ 2 ];
						send_data[2] = slow_data[ 3 ];
						send_data[3] = slow_data[ 4 ];
					}
				}
			
		}
		
		send_cmd(send_data, 4);
	}
	
	// phy_frame_counter++;
	
	if (phy_frame_counter >= 20)
	{
		// phy_frame_counter = 0;
		txmsg_counter ++;
		if (txmsg_counter >= 60)
		{
			txmsg_counter = 0;
		}
	}		
}	


// send_phy_net ( frame_counter, rx_data, rx_voice );

static void send_phy_hotspot( uint8_t frame_counter, uint8_t * rx_data, uint8_t * rx_voice )
{
	send_voice[0] = 0x21; // send voice data
	send_voice[1] = 0x01;

	memcpy (send_voice + 2, rx_voice, 9);
	
	send_cmd(send_voice, 11);

	if (frame_counter > 0)
	{
		send_data[0] = 0x22;
		
		memcpy (send_data + 1, rx_data, 3);
		
		send_cmd(send_data, 4);
	}
}			

	

static uint8_t rx_data[3];
static uint8_t rx_voice[9];
static uint8_t rx_header[39];
static uint8_t header_crc_result;

static void vTXTask( void *pvParameters )
{
	int tx_state = 0;
	
	int session_id = 0;
	
	
	vTaskDelay(1000); // 1secs
	
	dstarChangeMode(1);
	set_phy_parameters();
	dstarChangeMode(2);
	
	vdisp_clear_rect(0,0,128,64);
	gps_reset_slow_data();
	
#define PTT_CONDITION  ((gpio_get_pin_value(AVR32_PIN_PA28) == 0) || (software_ptt == 1))

	
	short tx_min_count = 0;
	uint8_t frame_counter = 0;
	short secs = 0;
	short tx_counter = 0;
	char buf[6];
	long curr_tx_ticks = 0;
	int last_rx_source = 0;
	
	for(;;)
	{
		switch(tx_state)
		{
		case 0:  // PTT off
			if (PTT_CONDITION  // PTT pressed
			 && (memcmp(settings.s.my_callsign, "NOCALL  ", CALLSIGN_LENGTH) != 0))
			{
				ambe_set_automute(0); // switch off automute
				tx_state = 1;
				ambe_start_encode();
						
				if (!dcs_mode || hotspot_mode || repeater_mode)
				{
					phy_start_tx();
				}
				
				frame_counter = 20; // counter 0..20,  first "send_dcs" with 0
				tx_counter = 0;
									
				vTaskDelay(80); // pre-buffer audio while PHY sends header
				dcs_reset_tx_counters();
				
				ambe_q_flush(microphone, 1);
				vTaskDelay(45); // pre-buffer one AMBE frame
				
				session_id = crypto_get_random_16bit();
				
				tx_min_count = 8; // send at least 8 AMBE frames+data (full TX Message)
				
				vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 1, " TX " );
				
				rtclock_reset_tx_ticks();
				curr_tx_ticks = 0;
			}
			else
			{
				int rx_source = rx_q_process(&frame_counter,rx_data,rx_voice);
				
				if (rx_source != 0) // receiving starts
				{
					vd_copy_screen(VDISP_SAVE_LAYER, VDISP_MAIN_LAYER, 36, 64);
					vdisp_clear_rect (0, 0, 128, 64);
					
					if (rx_source == SOURCE_PHY)
					{
						dstar_print_diagram();
					}
					
					vTaskDelay(200); // fill rx buffer
					
					dstar_get_header(rx_source, &header_crc_result, rx_header);
					
					rtclock_disp_xy(84, 0, 2, 1);
					
					rtclock_reset_tx_ticks();
					curr_tx_ticks = 0;
					session_id ++;
					dcs_reset_tx_counters();
					tx_counter = 0;
					
					tx_state = 10;
					last_rx_source = rx_source;
					
					
					
					if (hotspot_mode || repeater_mode)
					{
						if (last_rx_source == SOURCE_NET) // rx comes over the internet
						{
							phy_start_tx_hotspot(header_crc_result, rx_header);
						
							send_phy_hotspot ( frame_counter, rx_data, rx_voice );
						}
						else if (last_rx_source == SOURCE_PHY) // rx comes over PHY
						{
							send_dcs_hotspot(  session_id, 0, frame_counter, rx_data, rx_voice, 
									header_crc_result, rx_header );
						}						
					}
					
					
				}
				else
				{
					vTaskDelay(5); // watch for PTT or incoming traffic every 5ms
				}

			}
			break;
			
		case 1:  // PTT on
			if ((!PTT_CONDITION) && (tx_min_count <= 0))  // PTT released
			{
				tx_state = 2;
				ambe_stop_encode();
			}
			else
			{
				if (tx_min_count > 0)
				{
					tx_min_count--;
				}
				
				if (ambe_q_get(microphone, dcs_ambe_data) != 0) // queue unexpectedly empty
				{
					ambe_stop_encode();
					if (dcs_mode && dcs_is_connected())
					{
						memcpy(dcs_ambe_data, ambe_silence_data, 9);
						send_dcs( session_id, 2, frame_counter); // send 0x55 0x55 0x55 in data part
						frame_counter ++;
						
						if (frame_counter >= 21)
						{
							frame_counter = 0;
						}
						memcpy(dcs_ambe_data, end_sequence_v, 9);
						send_dcs( session_id, 1, frame_counter); // send end frame
					}
					
					tx_state = 3; // wait for PTT release
				}					
				else
				{
					if (dcs_mode && dcs_is_connected())
					{
						send_dcs(  session_id, 0, frame_counter ); // send normal frame
					}
					
					if (!dcs_mode || hotspot_mode || repeater_mode)
					{
						send_phy ( dcs_ambe_data, frame_counter );
					}		
					
					curr_tx_ticks += 20; // send AMBE data every 20ms			
					
					long tdiff = curr_tx_ticks - rtclock_get_tx_ticks();
					
					if (tdiff > 0)
					{
						if (tdiff > 500)
						{
							tdiff = 500;
						}
						
						vTaskDelay(tdiff); 
					}
					
				}
			}
			break;
				
		case 2: // PTT off, drain microphone data
			if (ambe_q_get(microphone, dcs_ambe_data) != 0) // queue empty
			{
				if (dcs_mode && dcs_is_connected())
				{
					memcpy(dcs_ambe_data, ambe_silence_data, 9);
					send_dcs( session_id, 2, frame_counter); // send 0x55 0x55 0x55 in data part
					frame_counter ++;
					
					if (frame_counter >= 21)
					{
						frame_counter = 0;
					}
					memcpy(dcs_ambe_data, end_sequence_v, 9);
					send_dcs( session_id, 1, frame_counter); // send end frame
				}
				
				tx_state = 3; // wait for PTT
			}
			else
			{
				if (dcs_mode && dcs_is_connected())
				{
					send_dcs(  session_id, 0, frame_counter ); // send normal frame
				}
				
				if (!dcs_mode || hotspot_mode || repeater_mode)
				{
					send_phy ( dcs_ambe_data, frame_counter );
				}					
				vTaskDelay(20); // wait 20ms
			}
			break;
		
		case 3: // PTT on, wait for release
			if (!PTT_CONDITION)  // PTT released
			{
				tx_state = 0;
				wm8510_beep(
					SETTING_SHORT(S_PTT_BEEP_DURATION),
					SETTING_SHORT(S_PTT_BEEP_FREQUENCY),
					SETTING_CHAR(C_PTT_BEEP_VOLUME)
					);
					
				vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 0, "    " );
				vdisp_i2s(buf, 3, 10, 0, secs);
				vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 0, buf );
				vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, 0, "s" );
			}
			else
			{
				vTaskDelay(10); // watch for PTT every 10ms
			}
			break;
			
			
		case 10:
		
			if (last_rx_source != rx_q_process(&frame_counter,rx_data,rx_voice))
			{
				tx_state = 0;
				
				if (hotspot_mode || repeater_mode)
				{
					if (last_rx_source == SOURCE_PHY) // rx comes over PHY
					{
						if (dcs_is_connected())
						{
							frame_counter ++;  // frame_counter was not set by rx_q_process, increment here
							
							if (frame_counter >= 21)
							{
								frame_counter = 0;
							}
							
							send_dcs_hotspot(  session_id, 0, frame_counter, end_sequence_d, ambe_silence_data, 
								header_crc_result, rx_header );
							
							frame_counter ++;
							
							if (frame_counter >= 21)
							{
								frame_counter = 0;
							}
							
							send_dcs_hotspot(  session_id, 1, frame_counter, end_sequence_d, end_sequence_v, 
								header_crc_result, rx_header );
							
						}
					}
				}
			}
			else
			{
				if (hotspot_mode || repeater_mode)
				{
					if (last_rx_source == SOURCE_NET) // rx comes over the internet
					{	
						send_phy_hotspot ( frame_counter, rx_data, rx_voice );
					}
					else if (last_rx_source == SOURCE_PHY) // rx comes over PHY
					{
						send_dcs_hotspot(  session_id, 0, frame_counter, rx_data, rx_voice, 
								header_crc_result, rx_header );
					}
				}			
					
				curr_tx_ticks += 20; // rx/tx AMBE data every 20ms
				
				long tdiff = curr_tx_ticks - rtclock_get_tx_ticks();
				
				if (tdiff > 0)
				{
					if (tdiff > 500)
					{
						tdiff = 500;
					}
					
					vTaskDelay(tdiff);
				}
			}
			break;
		}
		
		vdisp_i2s( buf, 5, 10, 0, tx_state );
		vd_prints_xy(VDISP_DEBUG_LAYER, 108, 28, VDISP_FONT_4x6, 0, buf );
		
		if ((tx_state > 0) && (tx_state < 10)) // increment frame_counter only for PTT+microphone usage
		{
			frame_counter ++;
			
			if (frame_counter >= 21)
			{
				frame_counter = 0;
			}
			
			tx_counter++;
			
			secs = tx_counter / 50;
				
			if ((tx_counter & 0x0F) == 0x01) // show seconds on every 16th frame
			{
				vdisp_i2s(buf, 3, 10, 0, secs);
				vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 1, buf );
				vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, 1, "s" );
			}
			
			
		}
	}		
	
}	







void txtask_init( ambe_q_t * mic )
{
	microphone = mic;
	
	memcpy (repeater_callsign, settings.s.my_callsign, CALLSIGN_LENGTH);
	if ((repeater_callsign[7] < 'A') || (repeater_callsign[7] > 'Z'))
	{
		repeater_callsign[7] = 'B'; // my repeater module
	}
	xTaskCreate( vTXTask, (signed char *) "TX", 300, ( void * ) 0, tskIDLE_PRIORITY + 1, ( xTaskHandle * ) NULL );
	
}