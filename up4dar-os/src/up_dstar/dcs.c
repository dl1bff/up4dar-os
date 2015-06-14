
/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

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
 * dcs.c
 *
 * Created: 12.05.2012 18:53:46
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include "gcc_builtin.h"

#include "dstar.h"
#include "up_io/eth_txmem.h"

#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/snmp.h"

#include "vdisp.h"
#include "up_io/eth.h"

#include "up_dstar/gps.h"

#include "dcs.h"
#include "up_dstar/settings.h"

#include "up_crypto/up_crypto.h"
#include "up_net/dns2.h"

#include "up_app/a_lib_internal.h"
#include "up_app/a_lib.h"
#include "sw_update.h"
#include "rx_dstar_crc_header.h"



char repeater_callsign[CALLSIGN_LENGTH];

static const char dcs_html_info[] = "<table border=\"0\" width=\"95%\"><tr>"

                              "<td width=\"4%\"><img border=\"0\" src=\"up4dar_dcs.jpg\"></td>"

                              "<td width=\"96%\">"

                              "<font size=\"1\">Universal Platform for Digital Amateur Radio</font></br>"

                              "<font size=\"2\"><b><a href=\"http://www.UP4DAR.de\">www.UP4DAR.de</a></b>&nbsp;</font>"
							  
							  //"<font size=\"1\">Version: X.0.00.00 </font>"
		 
                              "</td>"

                              "</tr></table>";




static int dcs_udp_local_port;

#define DCS_KEEPALIVE_TIMEOUT  100
#define DCS_CONNECT_REQ_TIMEOUT  6
#define DCS_CONNECT_RETRIES		  3
#define DCS_DISCONNECT_REQ_TIMEOUT  6
#define DCS_DISCONNECT_RETRIES	  3
#define DCS_DNS_TIMEOUT			 2
#define DCS_DNS_RETRIES		  3
#define DCS_DNS_INITIAL_RETRIES		  15

static int dcs_timeout_timer;
static int dcs_retry_counter;

#define DCS_DISCONNECTED		1
#define DCS_CONNECT_REQ_SENT	2
#define DCS_CONNECTED			3
#define DCS_DISCONNECT_REQ_SENT	4
#define DCS_DNS_REQ_SENT		5
#define DCS_DNS_REQ				6
#define DCS_WAIT				7

static int dcs_state;
static int dcs_state_history = DCS_DISCONNECTED;

#define DEXTRA_UDP_PORT            30001
#define DEXTRA_CONNECT_SIZE        11
#define DEXTRA_CONNECT_REPLY_SIZE  14
#define DEXTRA_KEEPALIVE_SIZE      9
#define DEXTRA_KEEPALIVE_V2_SIZE   10
#define DEXTRA_RADIO_HEADER_SIZE   56
#define DEXTRA_VOICE_FRAME_SIZE    27



static const char * const dcs_state_text[8] =
{
	"            ",
	"disconnected",
	"conn request",
	"connected   ",
	"disc request",
	"DNS request ",
	"DNS request ",
	"waiting     "
};	

static char current_module;
static short current_server;
static char current_server_type;



#define NUM_SERVERS 30


void dcs_init(void)
{

	dcs_state = DCS_DISCONNECTED;	
	
	current_module = 'C';
	current_server = 1;  // DCS001
	current_server_type = SERVER_TYPE_DCS; // DCS
}

void dcs_get_current_reflector_name (char * s)
{
	switch(current_server_type)
	{
		case SERVER_TYPE_TST:
			memcpy(s, "TST", 3);
			break;
		case SERVER_TYPE_DEXTRA:
			memcpy(s, "XRF", 3);
			break;
		default:
		case SERVER_TYPE_DCS:
			memcpy(s, "DCS", 3);
			break;
	}
	
	vdisp_i2s(s + 3, 3, 10, 1, current_server);
	s[6] = ' ';
	s[7] = current_module;
}

void dcs_get_current_statustext (char * s)
{
	memcpy(s, dcs_state_text[  (dcs_mode != 0) ? dcs_state : 0 ], 12);
	 // text is 12 bytes long
}


static uint8_t dcs_server_ipaddr[4];

static void dcs_link_to (char module);

static char dcs_server_dns_name[40]; // dns name of reflector e.g. "dcs001.xreflector.net"
static int dns_handle;

static void dcs_set_dns_name(void)
{
	switch(current_server_type)
	{
		case SERVER_TYPE_TST:
			memcpy(dcs_server_dns_name, "tst", 3);
			vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
			if (SETTING_BOOL(B_ENABLE_ALT_DNS))
			{
				memcpy(dcs_server_dns_name+6, ".reflector.hamnet.up4dar.de", 28);
			}
			else
			{
				memcpy(dcs_server_dns_name+6, ".reflector.up4dar.de", 21);
			}
			break;
			
		default:
		case SERVER_TYPE_DCS:
			memcpy(dcs_server_dns_name, "dcs", 3);
			vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
			if (SETTING_BOOL(B_ENABLE_ALT_DNS))
			{
				memcpy(dcs_server_dns_name+6, ".reflector.hamnet.up4dar.de", 28);
			}
			else
			{
				memcpy(dcs_server_dns_name+6, ".xreflector.net", 16);
			}
			break;
		case SERVER_TYPE_DEXTRA:
			memcpy(dcs_server_dns_name, "xrf", 3);
			vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
			
			if ((current_server >= 230) && (current_server < 270))
			{
				memcpy(dcs_server_dns_name+6, ".dstar.su", 10);
			}
			else if (SETTING_BOOL(B_ENABLE_ALT_DNS))
			{
				memcpy(dcs_server_dns_name+6, ".reflector.hamnet.up4dar.de", 28);
			}
			else
			{
				memcpy(dcs_server_dns_name+6, ".reflector.up4dar.de", 21);
			}
			break;
	}			
			
}


void dcs_service (void)
{
	if (dcs_timeout_timer > 0)
	{
		dcs_timeout_timer --;	
	}
	
	
	vd_prints_xy( VDISP_REF_LAYER, 20, 48, VDISP_FONT_6x8, 0, 
		  dcs_state_text[  (dcs_mode != 0) ? dcs_state : 0 ]);
		  
	switch (dcs_state)
	{
		case DCS_CONNECTED:
			if (dcs_timeout_timer == 0)
			{
				dcs_timeout_timer = 2; // 1 second
				dcs_state = DCS_WAIT;
				udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "NOWD");
			}
			break;
		
		case DCS_CONNECT_REQ_SENT:
			if (dcs_timeout_timer == 0)
			{
				if (dcs_retry_counter > 0)
				{
					dcs_retry_counter --;
				}
				
				if (dcs_retry_counter == 0)
				{
					dcs_timeout_timer = 20; // 10 seconds
					dcs_state = DCS_WAIT;
					udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
					vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "RQTO");
				}
				else
				{
					dcs_link_to(current_module);
					dcs_timeout_timer = DCS_CONNECT_REQ_TIMEOUT;
				}
			}
			break;
		
		case DCS_DISCONNECT_REQ_SENT:
			if (dcs_timeout_timer == 0)
			{
				if (dcs_retry_counter > 0)
				{
					dcs_retry_counter --;
				}
				
				if (dcs_retry_counter == 0)
				{
					dcs_state = DCS_DISCONNECTED;
					udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
				}
				else
				{
					dcs_link_to(' ');
					dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
				}
			}
			break;
			
		case DCS_DNS_REQ:
		
			dns_handle = dns2_req_A(dcs_server_dns_name);
			
			if (dns_handle >= 0) // resolver not busy
			{
				dcs_state = DCS_DNS_REQ_SENT;
			}			
			else if (dcs_timeout_timer == 0)
			{
				if (dcs_retry_counter > 0)
				{
					dcs_retry_counter --;
				}
				
				if (dcs_retry_counter == 0)
				{
					dcs_timeout_timer = 30; // 15 seconds
					dcs_state = DCS_WAIT;
				}
				else
				{
					dcs_timeout_timer = DCS_DNS_TIMEOUT;
				}
			}
			break;
			
		case DCS_DNS_REQ_SENT:
			if (dns2_result_available(dns_handle)) // resolver is ready
			{
				uint8_t * addrptr;
				if (dns2_get_A_addr(dns_handle, &addrptr) <= 0) // DNS didn't work
				{
					dcs_timeout_timer = 30; // 15 seconds
					dcs_state = DCS_WAIT;
				}
				else
				{
					memcpy (dcs_server_ipaddr, addrptr, 4); // use first address of DNS result
					dcs_udp_local_port = (current_server_type == SERVER_TYPE_DEXTRA) ? DEXTRA_UDP_PORT : udp_get_new_srcport();
					
					udp_socket_ports[UDP_SOCKET_DCS] = dcs_udp_local_port;
					
					dcs_link_to(current_module);
					
					dcs_state = DCS_CONNECT_REQ_SENT;
					dcs_retry_counter = DCS_CONNECT_RETRIES;
					dcs_timeout_timer =  DCS_CONNECT_REQ_TIMEOUT;
				}
				
				dns2_free(dns_handle);
			}
			break;
			
		case DCS_WAIT:
			if (dcs_timeout_timer == 0)
			{
				dcs_retry_counter = DCS_DNS_INITIAL_RETRIES;
				dcs_timeout_timer = DCS_DNS_TIMEOUT;
				dcs_state = DCS_DNS_REQ;
			}				
			break;
	}
}




void dcs_on(void)
{
	
	switch (dcs_state)
	{
		
		case DCS_DISCONNECTED:
			dcs_set_dns_name();
			
			dcs_state = DCS_DNS_REQ;
			dcs_retry_counter = DCS_DNS_INITIAL_RETRIES;
			dcs_timeout_timer = DCS_DNS_TIMEOUT;
			
			break;
	}
}


void dcs_off(void)
{
	
	switch (dcs_state)
	{
		case DCS_CONNECTED:
		
		
			dcs_link_to(' ');
			
			dcs_state = DCS_DISCONNECT_REQ_SENT;
			dcs_retry_counter = DCS_DISCONNECT_RETRIES;
			dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
			break;
		
		default:
		
			dcs_state = DCS_DISCONNECTED;
			udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
			break;
	}
}

void dcs_home(void)
{
	dcs_over();

	settings_get_home_ref();

	dcs_select_reflector((int)SETTING_SHORT(S_REF_SERVER_NUM), (char)SETTING_CHAR(C_REF_MODULE_CHAR), (char)SETTING_CHAR(C_REF_TYPE));

	if (SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) == 1)
	{
		dcs_set_dns_name();
			
		dcs_state = DCS_DNS_REQ;
		dcs_retry_counter = DCS_DNS_INITIAL_RETRIES;
		dcs_timeout_timer = DCS_DNS_TIMEOUT;
	}
}

void dcs_over(void)
{
	if (dcs_state == DCS_CONNECTED)
	{
		dcs_link_to(' ');
		dcs_state = DCS_DISCONNECTED;
		dcs_state_history = DCS_DISCONNECTED;
	}
	else
	{
		dcs_state = DCS_DISCONNECTED;
		udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
	}
}

bool dcs_changed(void)
{
	if (dstarPhyRX()) return false;
	
	if (dcs_mode != 0 && dcs_state_history != dcs_state && (dcs_state == DCS_CONNECTED || dcs_state == DCS_DISCONNECTED))
	{
		dcs_state_history = dcs_state;
		
		return true;
	}
	
	return false;
}


static void dcs_keepalive_response (int request_size);


void dcs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	if (memcmp(ipv4_src_addr, dcs_server_ipaddr, sizeof ipv4_addr) != 0)
	{
		// packet is not from the currently selected server
		return;
	}
	
	if ((data_len >= 100) || (data_len == DEXTRA_RADIO_HEADER_SIZE) || (data_len == DEXTRA_VOICE_FRAME_SIZE)) // voice packet
	{
		if (memcmp(data, "0001", 4) == 0)  // first four bytes "0001"
		{
			if (data[14] == current_module) // filter out the right channel
			{
				dstarProcessDCSPacket( data );
			}
		}
		if (memcmp(data, "DSVT", 4) == 0)
		{
			dstarProcessDExtraPacket(data);
		}
	}
	else if ((data_len == 14) || (data_len == DEXTRA_CONNECT_REPLY_SIZE)) // connect response packet
	{
		if (dcs_state == DCS_CONNECT_REQ_SENT)
		{
			if ((data[9] == current_module) &&
			  (memcmp(data + 10, "ACK", 3) == 0))
			{
				dcs_state = DCS_CONNECTED;
				dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "ACK ");
				
				a_app_manager_select_first(); // switch to main screen
			}
			else
			{
				dcs_state = DCS_DISCONNECTED;
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "NACK");
			} 
		}
		else if (dcs_state == DCS_DISCONNECT_REQ_SENT)
		{
			if (data[9] == ' ')
			{
				dcs_state = DCS_DISCONNECTED;
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "DISC");
			}
		}
	}
/*	else if (data_len == 9)  // keep alive packet (old version)
	{
		if (dcs_state == DCS_CONNECTED)
		{
			dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
			dcs_keepalive_response();
		}			
	}  */
	else if ((data_len == 22) || (data_len == DEXTRA_KEEPALIVE_SIZE) || (data_len == DEXTRA_KEEPALIVE_V2_SIZE))  // keep alive packet (new version)
	{
		if (dcs_state == DCS_CONNECTED)
		{
			dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
			dcs_keepalive_response(data_len);
		}
	}
}

int dcs_is_connected (void)
{
	if ((dcs_state == DCS_CONNECTED) || (dcs_state == DCS_DISCONNECT_REQ_SENT))
		return 1;
		
	return 0;
}

uint8_t dcs_ambe_data[9];

static int dcs_tx_counter = 0;
// static int dcs_frame_counter = 0;


void dcs_reset_tx_counters(void)
{
	// dcs_frame_counter = 0;
	dcs_tx_counter = 0;
}

void dcs_select_reflector (short server_num, char module, char server_type)
{
	if (dcs_state != DCS_DISCONNECTED)  // only when disconnected
		return;

	current_server_type = server_type;
	current_server = server_num;
	current_module = module;
	
	set_ref_params(server_num, module, server_type);
	
	return;
}



#define DCS_UDP_PORT 30051

#define DCS_VOICE_FRAME_SIZE  (100)


static eth_txmem_t * dcs_get_packet_mem (int udp_size)
{
	short port = (current_server_type != SERVER_TYPE_DEXTRA) ? DCS_UDP_PORT : DEXTRA_UDP_PORT;
	return udp4_get_packet_mem( udp_size, dcs_udp_local_port, port, dcs_server_ipaddr );
	
}

static void dcs_calc_chksum_and_send (eth_txmem_t * packet, int udp_size)
{
	udp4_calc_chksum_and_send(packet, dcs_server_ipaddr);
	
		
}

static void infocpy ( uint8_t * mem )
{
	char buf[11];
	
	memcpy(mem, dcs_html_info, sizeof dcs_html_info);
			
	version2string(buf, software_version); // get current software version
			
	int i;
			
	for (i=0; i < (500 - strlen(buf)); i++)
	{
		if (mem[i] == 'X')  // look for 'X'
		{
			memcpy(mem + i, buf, strlen(buf));
			// replace  X.0.00.00  with current software version
			break;
		}
	}
}

// #define DCS_CONNECT_FRAME_SIZE	19
#define DCS_CONNECT_FRAME_SIZE		519

static void dcs_link_to (char module)
{
	int size = (current_server_type == SERVER_TYPE_DEXTRA) ? DEXTRA_CONNECT_SIZE : DCS_CONNECT_FRAME_SIZE;
	eth_txmem_t * packet = dcs_get_packet_mem(size);
	
	if (packet == NULL)
	{
		return;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	memcpy (d, settings.s.my_callsign, 7);
	
	char buf[8];
	
	memcpy (buf, settings.s.my_callsign, 7);
	buf[7] = 0;
	vd_prints_xy(VDISP_DEBUG_LAYER, 86, 0, VDISP_FONT_6x8, 1, buf);
	vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "    ");
	
	d[7] = ' ';
	d[8] = settings.s.my_callsign[7];
	if ((d[8] < 'A') || (d[8] > 'Z'))
	{
		d[8] = SETTING_CHAR(C_REF_SOURCE_MODULE_CHAR); // my repeater module
	}
	d[9] = module; // module to link to
	d[10] = 0;

	if (current_server_type != SERVER_TYPE_DEXTRA)
	{
		dcs_get_current_reflector_name(buf);
		memcpy(d + 11, buf, 7);
		d[18] = ' ';
		d[18] = '@';
		memcpy(d + 19, dcs_html_info, sizeof dcs_html_info);
		infocpy(d + 19);
	}

	dcs_calc_chksum_and_send(packet, size);
}


#define DCS_KEEPALIVE_RESP_FRAME_SIZE		17

static void dcs_keepalive_response(int request_size)
{
	int size = (current_server_type == SERVER_TYPE_DEXTRA) ? request_size : DCS_KEEPALIVE_RESP_FRAME_SIZE;

	eth_txmem_t * packet = dcs_get_packet_mem(size);
	
	if (packet == NULL)
		return;
	
	uint8_t* d = packet->data + 42; // skip ip+udp header
	
	memcpy (d, settings.s.my_callsign, 7);


	if (current_server_type == SERVER_TYPE_DEXTRA)
	{
		switch (request_size)
		{
			case DEXTRA_KEEPALIVE_SIZE:
				d[7] = ' ';
				d[8] = 0;
				break;
			
			case DEXTRA_KEEPALIVE_V2_SIZE:
				d[7] = settings.s.my_callsign[7];
				if ((d[7] < 'A') || (d[7] > 'Z'))
				{
					d[7] = SETTING_CHAR(C_REF_SOURCE_MODULE_CHAR); // my repeater module
				}
				d[8] = current_module;
				d[9] = 0;
				break;
		}				
	}
	else
	{
		d[7] = settings.s.my_callsign[7];
		if ((d[7] < 'A') || (d[7] > 'Z'))
		{
			d[7] = SETTING_CHAR(C_REF_SOURCE_MODULE_CHAR); // my repeater module
		}
		d[8] = 0;
		dcs_get_current_reflector_name((char *) (d + 9));
	}

	dcs_calc_chksum_and_send(packet, size);
}


static int slow_data_count = 0;
static uint8_t slow_data[5];

static void build_slow_data(uint8_t * d, int last_frame, char dcs_frame_counter)
{
	if (last_frame != 0)
	{
		d[0] = 0x55;
		d[1] = 0x55;
		d[2] = 0x55;
	}
	else
	{
		if (dcs_frame_counter == 0) // sync
		{
			d[0] = 0x55;
			d[1] = 0x2d;
			d[2] = 0x16;
		}
		else
		{

			if ((dcs_frame_counter >= 1) && (dcs_frame_counter <= 8)
				&& (dcs_tx_counter < 20))  // send tx_msg only in first frame
			{
				int i = (dcs_frame_counter - 1) >> 1;
				if (dcs_frame_counter & 1)
				{
					d[0] = (0x40 + i) ^ 0x70;
					d[1] = settings.s.txmsg[ i * 5 + 0 ] ^ 0x4F;
					d[2] = settings.s.txmsg[ i * 5 + 1 ] ^ 0x93;
				}
				else
				{
					d[0] = settings.s.txmsg[ i * 5 + 2 ] ^ 0x70;
					d[1] = settings.s.txmsg[ i * 5 + 3 ] ^ 0x4F;
					d[2] = settings.s.txmsg[ i * 5 + 4 ] ^ 0x93;
				}
			}
			else
			{
				if (dcs_frame_counter & 1)
				{
					slow_data_count = gps_get_slow_data(slow_data);
					// slow_data_count = 0;
					if (slow_data_count == 0)
					{
						d[0] = 0x16;  // NOP
						d[1] = 0x29;
						d[2] = 0xf5;
					}
					else
					{
						d[0] = (0x30 + slow_data_count) ^ 0x70;
						d[1] = slow_data[ 0 ] ^ 0x4F;
						d[2] = slow_data[ 1 ] ^ 0x93;
					}
				}
				else
				{
					if (slow_data_count <= 2)
					{
						d[0] = 0x16;  // NOP
						d[1] = 0x29;
						d[2] = 0xf5;
					}
					else
					{
						d[0] = slow_data[ 2 ] ^ 0x70;
						d[1] = slow_data[ 3 ] ^ 0x4F;
						d[2] = slow_data[ 4 ] ^ 0x93;
					}
				}

			}


		}
	}
}

static void send_dcs_private (int session_id, int last_frame, char dcs_frame_counter)
{
	if (dcs_state == DCS_CONNECTED)  // only send voice if connected
	{
		
	int frame_size = DCS_VOICE_FRAME_SIZE;
	
	/*
	if ((dcs_tx_counter & 0x7F) == 0x03)  // approx every 2 seconds
	{
		frame_size += 500; // send HTML info
	}
	*/		
		
	eth_txmem_t * packet = dcs_get_packet_mem( frame_size );
	
	if (packet == NULL)
	{
		return;
	}
	
	
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	/*
	if (frame_size > DCS_VOICE_FRAME_SIZE) // send HTML info
	{
		infocpy(d + DCS_VOICE_FRAME_SIZE);

	}
	*/
	
	memcpy(d, "0001", 4);
	
	d[4] = 0;  // flags
	d[5] = 0;
	d[6] = 0;
	
	
	char buf[8];
	
	dcs_get_current_reflector_name( buf );
	
	memcpy (d + 7, buf, 8);
	//memcpy (d + 15, buf, 8);
	memcpy (d + 15, settings.s.my_callsign, 8);
	if ((d[22] < 'A') || (d[22] > 'Z'))
	{
		d[22] = SETTING_CHAR(C_REF_SOURCE_MODULE_CHAR); // my repeater module
	}
	memcpy(d + 23, "CQCQCQ  ", 8); 
	memcpy (d + 31, settings.s.my_callsign, 8);
	memcpy (d + 39, settings.s.my_ext, 4);
	
	d[43] = (session_id >> 8) & 0xFF;
	d[44] = session_id & 0xFF;
	
	d[45] = dcs_frame_counter | ((last_frame == 1) ? 0x40 : 0);
	
	memcpy (d + 46, dcs_ambe_data, sizeof dcs_ambe_data);
	
	d[58] = dcs_tx_counter & 0xFF;
	d[59] = (dcs_tx_counter >> 8) & 0xFF;
	d[60] = (dcs_tx_counter >> 16) & 0xFF;
	
	d[61] = 0x01; // Frame Format version low
	// d[62] = 0x00; // Frame Format version high
	d[63] = 0x21; // Language Set 0x21
	
	memcpy (d + 64, settings.s.txmsg, 20);

    build_slow_data(d + 55, last_frame, dcs_frame_counter);
	
	dcs_calc_chksum_and_send( packet, frame_size );
	
	} // if (dcs_state == DCS_CONNECTED)
	
	/*
	dcs_frame_counter ++;
	
	if (dcs_frame_counter >= 21)
	{
		dcs_frame_counter = 0;
	}
	*/
	
	dcs_tx_counter ++;
	
	/*
	int secs = dcs_tx_counter / 50;
				
	if ((last_frame != 0) || ((dcs_tx_counter & 0x0F) == 0x01)) // show seconds on every 16th call
	{
		char buf[4];
		vdisp_i2s(buf, 3, 10, 0, secs);
		vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, last_frame ? 0 : 1, buf );
		vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, last_frame ? 0 : 1, "s" );
	}
	*/
}

static void send_dextra_header(int session_id, const char * mycall, const char * mycallext)
{
  eth_txmem_t* packet = dcs_get_packet_mem(DEXTRA_RADIO_HEADER_SIZE);

  if (packet == NULL)
    return;

  uint8_t* d = packet->data + 42; // skip ip+udp header

  memcpy(d, "DSVT", 4);

  d[4]  = 0x10;
  d[5]  = 0x00;
  d[6]  = 0x00;
  d[7]  = 0x00;
  d[8]  = 0x20;
  d[9]  = 0x00;
  d[10] = 0x01;

  d[11] = 0x00;

  d[12] = (session_id >> 8) & 0xFF;
  d[13] = session_id & 0xFF;
        
  d[14] = 0x80;

  // Flags
  d[15] = 0;
  d[16] = 0;
  d[17] = 0;

  char reflector[8];
  dcs_get_current_reflector_name(reflector);

  memcpy(d + 18, reflector, 8);
  // d[25] = 'G';
  memcpy(d + 26, reflector, 8);
  d[33] = 'G';
  memcpy(d + 34, "CQCQCQ  ", 8); 
  memcpy(d + 42, mycall, 8);
  memcpy(d + 50, mycallext, 4);
  
  unsigned short sum = rx_dstar_crc_header(d + 15);
  d[54] = sum & 0xFF;
  d[55] = sum >> 8;

  dcs_calc_chksum_and_send(packet, DEXTRA_RADIO_HEADER_SIZE);
}

static void send_dextra_frame(int session_id, int last_frame, char dcs_frame_counter)
{
  eth_txmem_t * packet = dcs_get_packet_mem(DEXTRA_VOICE_FRAME_SIZE);

  if (packet == NULL)
    return;

  uint8_t* d = packet->data + 42; // skip ip+udp header

  memcpy(d, "DSVT", 4);

  d[4] = 0x20;
  d[5] = 0x00;
  d[6] = 0x00;
  d[7] = 0x00;
  d[8] = 0x20;
  d[9] = 0x00;
  d[10] = 0x01;
  d[11] = 0x00;

  d[12] = (session_id >> 8) & 0xFF;
  d[13] = session_id & 0xFF;

  d[14] = dcs_frame_counter | ((last_frame != 0) ? 0x40 : 0);

  memcpy(d + 15, dcs_ambe_data, sizeof(dcs_ambe_data));
  build_slow_data(d + 24, last_frame, dcs_frame_counter);

  dcs_calc_chksum_and_send(packet, DEXTRA_VOICE_FRAME_SIZE);

  dcs_tx_counter ++;
}

void send_dcs(int session_id, int last_frame, char dcs_frame_counter)
{
  if ((current_server_type == SERVER_TYPE_DEXTRA) &&
      (dcs_state == DCS_CONNECTED))
  {
    if (dcs_frame_counter == 0)
	{
      send_dextra_header(session_id, settings.s.my_callsign, settings.s.my_ext);
	}	  
    send_dextra_frame(session_id, last_frame, dcs_frame_counter);
    return;
  }
  send_dcs_private(session_id, last_frame, dcs_frame_counter);
}



static void send_dcs_hotspot_dcs (int session_id, int last_frame, uint8_t frame_counter,
	const uint8_t * rx_data, const uint8_t * rx_voice, const uint8_t * rx_header)
{
	int frame_size = DCS_VOICE_FRAME_SIZE;
	
	eth_txmem_t * packet = dcs_get_packet_mem( frame_size );
	
	if (packet == NULL)
	{
		return;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	memcpy(d, "0001", 4);
	
	d[4] = 0;  // flags
	d[5] = 0;
	d[6] = 0;
	
	char buf[8];
	
	dcs_get_current_reflector_name( buf );
	
	memcpy (d + 7, buf, 8);
	//memcpy (d + 15, buf, 8);
	memcpy (d + 15, settings.s.my_callsign, 8);
	if ((d[22] < 'A') || (d[22] > 'Z'))
	{
		d[22] = SETTING_CHAR(C_REF_SOURCE_MODULE_CHAR); // my repeater module
	}
	memcpy(d + 23, "CQCQCQ  ", 8);
	memcpy (d + 31, rx_header + 27, 8);
	memcpy (d + 39, rx_header + 35, 4);
	
	d[43] = (session_id >> 8) & 0xFF;
	d[44] = session_id & 0xFF;
	
	d[45] = frame_counter | ((last_frame != 0) ? 0x40 : 0);
	
	memcpy (d + 46, rx_voice, 9);
	
	d[58] = dcs_tx_counter & 0xFF;
	d[59] = (dcs_tx_counter >> 8) & 0xFF;
	d[60] = (dcs_tx_counter >> 16) & 0xFF;
	
	d[61] = 0x01; // Frame Format version low
	// d[62] = 0x00; // Frame Format version high
	d[63] = 0x21; // Language Set 0x21
	
	// memcpy (d + 64, settings.s.txmsg, 20);

	//build_slow_data(d + 55, last_frame, dcs_frame_counter);
	
	d[55] = rx_data[ 0 ] ^ 0x70;
	d[56] = rx_data[ 1 ] ^ 0x4F;
	d[57] = rx_data[ 2 ] ^ 0x93;
	
	
	dcs_calc_chksum_and_send( packet, frame_size );
	
}

static void send_dcs_hotspot_dextra (int session_id, int last_frame, uint8_t frame_counter,
	const uint8_t * rx_data, const uint8_t * rx_voice, const uint8_t * rx_header)
{
	eth_txmem_t * packet = dcs_get_packet_mem(DEXTRA_VOICE_FRAME_SIZE);

	if (packet == NULL)
	return;

	uint8_t* d = packet->data + 42; // skip ip+udp header

	memcpy(d, "DSVT", 4);

	d[4] = 0x20;
	d[5] = 0x00;
	d[6] = 0x00;
	d[7] = 0x00;
	d[8] = 0x20;
	d[9] = 0x00;
	d[10] = 0x01;
	d[11] = 0x00;

	d[12] = (session_id >> 8) & 0xFF;
	d[13] = session_id & 0xFF;

	d[14] = frame_counter | ((last_frame == 1) ? 0x40 : 0);

	//memcpy(d + 15, dcs_ambe_data, sizeof(dcs_ambe_data));
	//build_slow_data(d + 24, last_frame, dcs_frame_counter);
	
	memcpy (d+15, rx_voice, 9);
	
	d[24] = rx_data[ 0 ] ^ 0x70;
	d[25] = rx_data[ 1 ] ^ 0x4F;
	d[26] = rx_data[ 2 ] ^ 0x93;

	dcs_calc_chksum_and_send(packet, DEXTRA_VOICE_FRAME_SIZE);

}

// send_dcs_hotspot(  session_id, frame_counter, rx_data, rx_voice, rx_header );
void send_dcs_hotspot (int session_id, int last_frame, uint8_t frame_counter,
	const uint8_t * rx_data, const uint8_t * rx_voice, uint8_t crc_result, const uint8_t * rx_header)
{
	if ( (  hotspot_mode &&
			(dcs_state == DCS_CONNECTED) && // only send if connected
			(crc_result == DSTAR_HEADER_OK) &&  // last received header was OK
			(rx_header[0] == 0x00) &&   // no repeater flag in header
			(memcmp("DIRECT  ", rx_header + 3, 8) == 0) &&  // RPT1 and RPT2 contain "DIRECT  "
			(memcmp("DIRECT  ", rx_header + 11, 8) == 0) ) 
			||
		 ( repeater_mode &&
		   (dcs_state == DCS_CONNECTED) && // only send if connected
		   (crc_result == DSTAR_HEADER_OK) &&  // last received header was OK
		   (rx_header[0] == 0x40) &&   // repeater flag in header
		   (memcmp(repeater_callsign, rx_header + 11, 8) == 0)  // RPT1 repeater callsign
		    )
		 )
	{	
		if (current_server_type == SERVER_TYPE_DEXTRA)
		{
			if (frame_counter == 0)
			{
				send_dextra_header(session_id, (char *) rx_header + 27, (char * ) rx_header + 35);
			}
			send_dcs_hotspot_dextra(session_id, last_frame, frame_counter, rx_data, rx_voice, rx_header);
		}
		else
		{
			send_dcs_hotspot_dcs(session_id, last_frame, frame_counter, rx_data, rx_voice, rx_header);
		}
	}
	
	dcs_tx_counter ++;
}