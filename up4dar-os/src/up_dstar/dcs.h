
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
 * dcs.h
 *
 * Created: 12.05.2012 18:54:24
 *  Author: mdirska
 */ 


#ifndef DCS_H_
#define DCS_H_

#define SERVER_TYPE_DCS		0
#define SERVER_TYPE_TST		1
#define SERVER_TYPE_DEXTRA	2

extern uint8_t dcs_ambe_data[9];

extern char repeater_callsign[];

void dcs_init(void);
void dcs_service (void);
void dcs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr);
void dcs_off (void);
void dcs_over(void);
bool dcs_changed(void);

void send_dcs (int session_id, int last_frame, char dcs_frame_counter);
void dcs_get_current_reflector_name (char * s);
int dcs_is_connected (void);
void dcs_reset_tx_counters(void);

void dcs_select_reflector (short server_num, char module, char server_type);
void dcs_on(void);

void dcs_home(void);


void send_dcs_hotspot (int session_id, int last_frame, uint8_t frame_counter, const uint8_t * rx_data, const uint8_t * rx_voice, uint8_t crc_result, const uint8_t * rx_header);
void dcs_get_current_statustext (char * s);
#endif /* DCS_H_ */