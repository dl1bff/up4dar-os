
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
#include "gpio.h"
#include "up_dstar/urcall.h"
#include "ambe_fec.h"
#include "up_dstar/slowdata.h"
#include "ccs.h"

static ambe_q_t * microphone;
static uint8_t rx_data[3];
static uint8_t rx_voice[9];
static uint8_t rx_header[39];
static uint8_t header_crc_result;

#define MAX_DTMF_COMMAND  6
static char dtmf_cmd_string[MAX_DTMF_COMMAND + 1];
static uint8_t dtmf_cmd_ptr = 0;
#define MAX_DTMF_HISTORY  5
static uint8_t dtmf_history[MAX_DTMF_HISTORY];
#define DTMF_THRESHOLD  3
#define DTMF_HISTOGRAM_SIZE  17
static uint8_t dtmf_histogram[DTMF_HISTOGRAM_SIZE];
static uint8_t dtmf_counter = 0;
#define DTMF_DETECT_TIME 250
static uint8_t dtmf_tone_detected = 0;
static int send_as_broadcast = 1;
static int header_reason = 1;
static int suppress_user_feedback = 0;


void set_phy_parameters(void)
{
	uint8_t value;
	char buf[9];
	
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
	
	buf[0] = SET_RMU;
	buf[1] = SETTING_CHAR(C_RMU_ENABLED) == 1 ? 0x01 : 0x02;
	
	phyCommSendCmd(buf, 2);
	
	int qrgRX = 0;
	int qrgTX = 0;
	
	for (int i=0; i < QRG_LENGTH; i++)
	{
		qrgRX = (qrgRX * 10) + (settings.s.qrg_rx[i] & 0x0F);
	}
	
	for (int i=0; i < QRG_LENGTH; i++)
	{
		qrgTX = (qrgTX * 10) + (settings.s.qrg_tx[i] & 0x0F);
	}

	buf[0] = SET_QRG;
	buf[1] = ((unsigned int)qrgRX >> 24) & 0xFF;
	buf[2] = ((unsigned int)qrgRX >> 16) & 0xFF;
	buf[3] = ((unsigned int)qrgRX >> 8) & 0xFF;
	buf[4] = ((unsigned int)qrgRX >> 0) & 0xFF;
	buf[5] = ((unsigned int)qrgTX >> 24) & 0xFF;
	buf[6] = ((unsigned int)qrgTX >> 16) & 0xFF;
	buf[7] = ((unsigned int)qrgTX >> 8) & 0xFF;
	buf[8] = ((unsigned int)qrgTX >> 0) & 0xFF;
	
	phyCommSendCmd(buf, 9);
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
static const char feedback_tx_on[1] = {0x12};

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
				
	char* urcall = getURCALL();
				
	
	
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
		header[20+i] = urcall[i];
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

static void phy_send_response(int send_as_broadcast, int header_reason, uint8_t * rx_header)
{
	char txmsg[20];

	// Schalte UP4DAR auf Senden um
	if (repeater_mode)
		send_cmd(feedback_tx_on, 1);
	else
		send_cmd(tx_on, 1);
	
	// Bereite den Header vor
	header[0] = 0x20;
	header[1] = 0x01;				// Das steht in den Bestaetigungsdurchgaengen von ICOM-Repeater. Evtl. benoetigt man das nicht.
	header[2] = 0x00;				// "2nd control byte"
	header[3] = 0x00;				// "3rd control byte"
	
	memcpy(header+4, repeater_callsign, CALLSIGN_LENGTH);			// RPT2 (Ausstieg)
	
	memcpy(header+12, settings.s.my_callsign, CALLSIGN_LENGTH-1);		// RPT1 (Einstieg)
	
	header[19] = 0x47;													// Trage "G" als RPT1 Modul ein

	if (send_as_broadcast == 1) // Status via broadcast
	{
		memcpy(header+20, "CQCQCQ  ", CALLSIGN_LENGTH);					// YOUR
	}
	else // Personal feedback
	{
		memcpy(header+20, rx_header+27, CALLSIGN_LENGTH);				// YOUR <== MY
	}
	
	memcpy(header+28, settings.s.my_callsign, CALLSIGN_LENGTH-1);		// MY1
	
	header[35] = 0x47;													// Trage "G" als MY-Modul ein

	if (hotspot_mode)
	{
		memcpy(header+36, "SPOT", 4);									// MY2 = SPOT
	}
	else
	{
		memcpy(header+36, "RPTR", 4);									// MY2 = RPTR
	}
	
	// Sende den Headerbefehl
	send_cmd(header, 40);
	
	send_voice[0] = 0x21;
	send_voice[1] = 0x01;
	memcpy(send_voice+2, ambe_silence_data, 9);
		
	if (header_reason == 1)
	{
		// bereite Tx-Message vor
		if (dstarFeedbackHeader() == 0)
		{
			// ===========11111111112222222222==
			memcpy(txmsg,"Header CRC is OK.   ", 20);
		}
		else if (dstarFeedbackHeader() == 1)
		{
			// ===========11111111112222222222==
			memcpy(txmsg,"Header CRC is wrong!", 20);
		}
		else if ((dstarFeedbackHeader() == 2) || (dstarFeedbackHeader() == 3))
		{
			// ===========11111111112222222222==
			memcpy(txmsg,"TermFlag missing!   ", 20);
		}
	}
	else
	{
		dcs_get_current_reflector_name(txmsg);
	
		txmsg[6] = txmsg[7];
		txmsg[7] = ' ';
	
		dcs_get_current_statustext(txmsg + 8);
	}
	
	send_data[0] = 0x22;
	
	send_cmd(send_voice, 11);
	
	for (int i=0; i<4; ++i)
	{
		send_cmd(send_voice, 11);
		send_data[1] = 0x40 + i;
		memcpy(send_data+2, txmsg+5*i, 2);
		send_cmd(send_data, 4);
		
		send_cmd(send_voice, 11);
		memcpy(send_data+1, txmsg+5*i+2, 3);
		send_cmd(send_data, 4);
	}
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

	
static const gpio_map_t ext_gpio_map =
{
	{ AVR32_PIN_PA24, GPIO_DIR_OUTPUT | GPIO_INIT_LOW }	// GPIO0 on extension header
	
};

void send_dcs_state(void)
{
	vTaskDelay(500); //statt 950
	phy_send_response(send_as_broadcast, 0, rx_header);
	send_as_broadcast = 1;
}

void send_feedback(void)
{
	if (suppress_user_feedback == 0)
	{
		vTaskDelay(100); // wait 100ms
		if (dstarPhyRX()) return;	// Der laufende Durchgang ist noch nicht zu Ende.
		vTaskDelay(300); // wait 300ms
	
		phy_send_response(0, header_reason, rx_header);
		header_reason = 1;
	
		if (hotspot_mode)
		vTaskDelay((SETTING_SHORT(S_PHY_TXDELAY)*27+5600)>>4);
	}
	else
	{
		suppress_user_feedback = 0;
	}
}


static void dtmf_decode_init(void)
{
	dtmf_tone_detected = 0;
	strncpy(dtmf_cmd_string, "      ", MAX_DTMF_COMMAND);
	dtmf_cmd_ptr = 0;
	memset(dtmf_history, 0, MAX_DTMF_HISTORY);
	dtmf_counter = 0;
}


static int dtmf_decode(const uint8_t * ambe_data)
{
	int i;
	int dtmf_code = 0;
	
	if (dtmf_counter < DTMF_DETECT_TIME)
	{
		dtmf_counter ++;
	
		dtmf_code = ambe_get_dtmf_code(ambe_data);
	
		dtmf_history[dtmf_counter % MAX_DTMF_HISTORY] = dtmf_code;
	
		memset(dtmf_histogram, 0, DTMF_HISTOGRAM_SIZE);
	
		for (i=0; i < MAX_DTMF_HISTORY; i++)
		{
			dtmf_histogram[dtmf_history[i]] ++;
		}
	
		
		for (i=0; i < DTMF_HISTOGRAM_SIZE; i++)
		{
			if (dtmf_histogram[i] >= DTMF_THRESHOLD)
			{
				if (i != dtmf_tone_detected)
				{
					dtmf_tone_detected = i;
					
					if ((i > 0) && (dtmf_cmd_ptr < MAX_DTMF_COMMAND))
					{
						dtmf_cmd_string[dtmf_cmd_ptr] = dtmf_code_to_char(dtmf_tone_detected);
						dtmf_cmd_ptr++;
						dtmf_cmd_string[dtmf_cmd_ptr] = 0;
					}
				}
				
				break;
			}
		}
	
		vd_prints_xy(VDISP_AUDIO_LAYER, 0, 48, VDISP_FONT_6x8, 0, dtmf_cmd_string);
	}
	
	return dtmf_code;
}


static void parse_number(const char * s, int len, int * num, char * dcs_room)
{
	int n = 0;
	int i;
	
	for (i=0; i < len; i++)
	{
		if ((s[i] >= '0') && (s[i] <= '9'))
		{
			n = n*10 + (s[i] & 0x0F);
		}
		else
		{
			return; // not a number
		}
	}
	
	if (dcs_room != 0)
	{
		int room = n % 100; // last to chars: room number
		n /= 100;
		if ((room >= 1) && (room <= 26))
		{
			*dcs_room = 64 + room; // ASCII  A..Z
		}
		else
		{
			return; // not a valid dcs format
		}
	}
	
	if (n >= 1000) // only three digit reflector numbers
	{
		return; 
	}
	
	*num = n;
}

static void dtmf_cmd_exec(void)
{
	if (dtmf_cmd_string[0] != 32) // a DTMF string was received
	{
		int len = strlen(dtmf_cmd_string);
		char last_char = dtmf_cmd_string[len - 1];
		
		if (dtmf_cmd_string[0] == '#') // unlink
		{
			ambe_set_ref_timer(1);
			dcs_off();
			
			suppress_user_feedback = 1;
			if (header_crc_result == DSTAR_HEADER_OK)
			{
				send_as_broadcast = 0;
			}
		}
		else if (dtmf_cmd_string[0] == '0') // Statusrückmeldung wie I
		{
			header_reason = 0;
			if (header_crc_result == DSTAR_HEADER_OK)
			{
				send_as_broadcast = 0;
			}
	    }
		else if (dtmf_cmd_string[0] == 'D')
		{
			int reflector = -1;
			char room_letter = 'A';
			
			if ((last_char >= 'A') && (last_char <= 'D'))
			{
				parse_number (dtmf_cmd_string+1, len - 2, &reflector, 0);
				room_letter = last_char;
			}
			else
			{
				parse_number (dtmf_cmd_string+1, len - 1, &reflector, &room_letter);
			}
			
			if (reflector >= 0)
			{
				dcs_over();
				ambe_set_ref_timer(1);
				dcs_select_reflector(reflector, room_letter, SERVER_TYPE_DCS);
				dcs_on();
				
				suppress_user_feedback = 1;
				if (header_crc_result == DSTAR_HEADER_OK)
				{
					send_as_broadcast = 0;
				}
			}
		}
		else if ((dtmf_cmd_string[0] >= '1') && (dtmf_cmd_string[0] <= '9') &&
					(last_char >= 'A') && (last_char <= 'D'))
		{
			int reflector = -1;
			parse_number (dtmf_cmd_string, len - 1, &reflector, 0);
			
			if (reflector >= 0)
			{
				dcs_over();
				ambe_set_ref_timer(1);
				dcs_select_reflector(reflector, last_char, SERVER_TYPE_DEXTRA);
				dcs_on();
				
				suppress_user_feedback = 1;
				if (header_crc_result == DSTAR_HEADER_OK)
				{
					send_as_broadcast = 0;
				}
			}
		}
	}	
}

static void vTXTask( void *pvParameters )
{
	int tx_state = 0;
	
	int session_id = 0;
	
	int i;
	
	gpio_enable_gpio( ext_gpio_map, sizeof( ext_gpio_map ) / sizeof( ext_gpio_map[0] ) );
	
	for (i=0; i < (sizeof( ext_gpio_map ) / sizeof( ext_gpio_map[0] )); i++)
	{
		gpio_configure_pin( ext_gpio_map[i].pin, ext_gpio_map[i].function);
	}
	
	
	vTaskDelay(1000); // 1secs
	
	dstarChangeMode(1);
	set_phy_parameters();
	dstarChangeMode(2);
	
	vdisp_clear_rect(0,0,128,64);
	gps_reset_slow_data();
	
#define PTT_CONDITION  (((gpio_get_pin_value(AVR32_PIN_PA28) == 0) || (software_ptt == 1)) && (!parrot_mode))

	
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
			tx_info_off();
			ambe_ref_timer_break(0);
			
			if (PTT_CONDITION  // PTT pressed
			 && (memcmp(settings.s.my_callsign, "NOCALL  ", CALLSIGN_LENGTH) != 0))
			{
				
				ccs_send_info((uint8_t *) settings.s.my_callsign, (uint8_t *) settings.s.my_ext, 1);
				
				ambe_set_automute(0); // switch off automute
				tx_state = 1;
				ambe_start_encode();
						
				if (!dcs_mode || hotspot_mode || repeater_mode)
				{
					phy_start_tx();
				}
				
				frame_counter = 20; // counter 0..20,  first "send_dcs" with 0
				tx_counter = 0;
				
				dtmf_decode_init();
									
				vTaskDelay(80); // pre-buffer audio while PHY sends header
				dcs_reset_tx_counters();
				
				ambe_q_flush(microphone, 1);
				vTaskDelay(45); // pre-buffer one AMBE frame
				
				session_id = crypto_get_random_16bit();
				
				tx_min_count = 8; // send at least 8 AMBE frames+data (full TX Message)
				
				vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 1, " TX " );
				
				rtclock_reset_tx_ticks();
				curr_tx_ticks = 0;
				
				header_crc_result = 0;
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
					
					// vTaskDelay(200); // fill rx buffer
					
					vTaskDelay(40);
					
					dstar_get_header(rx_source, &header_crc_result, rx_header);
					
					if ((rx_source == SOURCE_PHY) && (header_crc_result == DSTAR_HEADER_OK))
					{
						ccs_send_info(rx_header + 27, rx_header + 35, 0); // mycall  and mycall_ext
					}
					
					rtclock_disp_xy(84, 0, 2, 1);
					
					rtclock_reset_tx_ticks();
					curr_tx_ticks = 0;
					session_id ++;
					dcs_reset_tx_counters();
					tx_counter = 0;
					
					tx_state = 10;
					last_rx_source = rx_source;
					
					
					dtmf_decode_init();
					
					if (hotspot_mode || repeater_mode)
					{
						if (last_rx_source == SOURCE_NET) // rx comes over the internet
						{
							phy_start_tx_hotspot(header_crc_result, rx_header);
						
							send_phy_hotspot ( frame_counter, rx_data, rx_voice );
						}
						else if (last_rx_source == SOURCE_PHY) // rx comes over PHY
						{

							/* 
							buf[0] = rx_header[24];
							buf[1] = rx_header[25];
							buf[2] = rx_header[26];
							buf[3] = 0;
							
							vd_prints_xy(VDISP_DEBUG_LAYER, 108, 22, VDISP_FONT_4x6, 0, buf );
							*/
							
							// vd_prints_xy(VDISP_DEBUG_LAYER, 108, 22, VDISP_FONT_4x6, 0, "ON " );
							
							if ((header_crc_result == DSTAR_HEADER_OK) &&
								((rx_header[26] == 'I') ||
								(rx_header[26] == 'U') ||
								(rx_header[26] == 'L') ||
								(((repeater_mode &&
								(rx_header[10] != 0x47)) ||
								((hotspot_mode) &&
								(rx_header[0] & 0x40))) &&
								((rx_header[0] & 0x08) == 0))))
							{
								tx_state = 11; // don't forward transmission
																
								if ( (rx_header[26] == 'U') || (rx_header[26] == 'L')  )	// Bei diesen Faellen wird der Feeback ueber eine Aenderung im DCS-Client angestossen
								{
									suppress_user_feedback	= 1;
									send_as_broadcast		= 0;
								}
								
								if (rx_header[26] == 'I')
								{
									header_reason		= 0;
								}
								else if (rx_header[26] == 'U')
								{
									ambe_set_ref_timer(1);
									dcs_off();
								}
								else if ((rx_header[26] == 'L') && (rx_header[25] >= 'A') && (rx_header[25] <= 'Z'))
								{
									int n = 0;
									int i;
									
									for (i=22; i < 25; i++)
									{
										if ((rx_header[i] >= '0') && (rx_header[i] <= '9'))
										{
											n = n*10 + (rx_header[i] & 0x0F);
										}
										else
										{
											n = -1;
											break;
										}
									}
									
									if (n >= 0)
									{
										if (memcmp("DCS", rx_header+19, 3) == 0)
										{
											dcs_over();
											ambe_set_ref_timer(1);
											dcs_select_reflector(n, rx_header[25], SERVER_TYPE_DCS);
											dcs_on();
										}
										else if (memcmp("XRF", rx_header+19, 3) == 0)
										{
											dcs_over();
											ambe_set_ref_timer(1);
											dcs_select_reflector(n, rx_header[25], SERVER_TYPE_DEXTRA);
											dcs_on();
										}
										else if (memcmp("XLX", rx_header+19, 3) == 0)
										{
											dcs_over();
											ambe_set_ref_timer(1);
											dcs_select_reflector(n, rx_header[25], SERVER_TYPE_XLX);
											dcs_on();
										}
									}									
								}
							}
							else
							{								
								send_dcs_hotspot(  session_id, 0, frame_counter, rx_data, rx_voice, 
									header_crc_result, rx_header );
							}									
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
			tx_info_on();
			ambe_ref_timer_break(1);
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
					if (!dcs_mode || hotspot_mode || repeater_mode)
					{
						send_phy ( dcs_ambe_data, frame_counter );
					}
					
					
					
					if (dcs_mode)
					{
						if (dtmf_decode(dcs_ambe_data) != 0)
						{
							memcpy(dcs_ambe_data, ambe_silence_data, 9); // silence DTMF on DCS connection
						}
						
						if (dcs_is_connected())
						{
							send_dcs(  session_id, 0, frame_counter ); // send normal frame
						}
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
				
				dtmf_cmd_exec();
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
						dtmf_cmd_exec();
						
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
				// only detect DTMF from PHY source
				if (last_rx_source == SOURCE_PHY)
				{
					if (dtmf_decode(rx_voice) != 0)
					{
						memcpy(rx_voice, ambe_silence_data, 9); // silence DTMF on DCS connection
					}
					
					if (hotspot_mode || repeater_mode)
					{
						slowdata_analyze_stream();
					}
				}
				
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
				
				
				/*
				uint32_t data1;
				uint32_t data2;
				char buf[7];
				
				
				ambe_fec_decode_first_block(rx_voice, &data1, &data2);
				
				vdisp_i2s(buf, 6, 16, 1, data1);
				vd_prints_xy(VDISP_AUDIO_LAYER, 69, 48, VDISP_FONT_6x8, 0, buf);
				vdisp_i2s(buf, 6, 16, 1, data2);
				vd_prints_xy(VDISP_AUDIO_LAYER, 69, 56, VDISP_FONT_6x8, 0, buf);
				*/
				
				
				
				
					
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
			
		case 11: // dummy receive for command-transmission (link, unlink etc.)
			if (last_rx_source != rx_q_process(&frame_counter,rx_data,rx_voice))
			{
				tx_state = 0;
			
				//if (hotspot_mode || repeater_mode)
				//{
					//if (last_rx_source == SOURCE_PHY) // rx comes over PHY
					//{
						//// vd_prints_xy(VDISP_DEBUG_LAYER, 108, 22, VDISP_FONT_4x6, 0, "OFF" );
						//
						//vTaskDelay(250); // wait before sending ACK
						//phy_send_response( rx_header );
					//}
				//}
			}
			else
			{
				if (hotspot_mode || repeater_mode)
				{
					slowdata_analyze_stream();
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
		
		if (tx_state == 10)
		{
			gpio_set_pin_high(AVR32_PIN_PA24);
		}
		else
		{
			gpio_set_pin_low(AVR32_PIN_PA24);
		}
		
	}		
	
}	







void txtask_init( ambe_q_t * mic )
{
	microphone = mic;
	
	memcpy (repeater_callsign, settings.s.my_callsign, CALLSIGN_LENGTH);
	if ((repeater_callsign[7] < 'A') || (repeater_callsign[7] > 'E'))
	{
		repeater_callsign[7] = DEFAULT_REPEATER_MODULE_CHAR; // my repeater module
	}
	xTaskCreate( vTXTask, (signed char *) "TX", 300, ( void * ) 0, tskIDLE_PRIORITY + 1, ( xTaskHandle * ) NULL );
	
}