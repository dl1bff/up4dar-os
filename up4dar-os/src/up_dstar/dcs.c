
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
#include "up_net/dns.h"

#include "up_app/a_lib_internal.h"
#include "up_app/a_lib.h"
#include "sw_update.h"

static const char dcs_html_info[] = "<table border=\"0\" width=\"95%\"><tr>"

                              "<td width=\"4%\"><img border=\"0\" src=\"up4dar_dcs.jpg\"></td>"

                              "<td width=\"96%\">"

                              "<font size=\"1\">Universal Platform for Digital Amateur Radio</font></br>"

                              "<font size=\"2\"><b>www.UP4DAR.de</b>&nbsp;</font>"
							  
							  "<font size=\"1\">Version: X.0.00.00 </font>"
		 
                              "</td>"

                              "</tr></table>";



static int dcs_udp_local_port;

static int dcs_state;

#define DCS_KEEPALIVE_TIMEOUT  100
#define DCS_CONNECT_REQ_TIMEOUT  6
#define DCS_CONNECT_RETRIES		  3
#define DCS_DISCONNECT_REQ_TIMEOUT  6
#define DCS_DISCONNECT_RETRIES	  3
#define DCS_DNS_TIMEOUT			 2
#define DCS_DNS_RETRIES		  3

static int dcs_timeout_timer;
static int dcs_retry_counter;

#define DCS_DISCONNECTED		1
#define DCS_CONNECT_REQ_SENT	2
#define DCS_CONNECTED			3
#define DCS_DISCONNECT_REQ_SENT	4
#define DCS_DNS_REQ_SENT		5
#define DCS_DNS_REQ				6


static const char * const dcs_state_text[7] =
{
	"            ",
	"disconnected",
	"conn request",
	"connected   ",
	"disc request",
	"DNS request ",
	"DNS request "
};	

static int current_module;
static int current_server;



#define NUM_SERVERS 30


void dcs_init(void)
{

	dcs_state = DCS_DISCONNECTED;	
	
	current_module = 'C';
	current_server = 1;  // DCS001
}

void dcs_get_current_reflector_name (char * s)
{
	memcpy(s, "DCS", 3);
	vdisp_i2s(s + 3, 3, 10, 1, current_server);
	s[6] = ' ';
	s[7] = current_module;
}



static uint8_t dcs_server_ipaddr[4];

static void dcs_link_to (int module);

static char dcs_server_dns_name[25]; // dns name of reflector e.g. "dcs001.xreflector.net"

static void dcs_set_dns_name(void)
{
	memcpy(dcs_server_dns_name, "dcs", 3);
	vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
	memcpy(dcs_server_dns_name+6, ".xreflector.net", 16);
}


void dcs_service (void)
{
	if (dcs_timeout_timer > 0)
	{
		dcs_timeout_timer --;	
	}
	
	
	vd_prints_xy( VDISP_REF_LAYER, 20, 36, VDISP_FONT_6x8, 0, 
		  dcs_state_text[  (dcs_mode != 0) ? dcs_state : 0 ]);
	
	switch (dcs_state)
	{
		case DCS_CONNECTED:
			if (dcs_timeout_timer == 0)
			{
				dcs_state = DCS_DISCONNECTED;
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
					dcs_state = DCS_DISCONNECTED;
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
			if (dns_get_lock()) // resolver not busy
			{
				if (dns_req_A(dcs_server_dns_name) == 0) // OK
				{
					dcs_state = DCS_DNS_REQ_SENT;
				}
				else
				{
					dcs_state = DCS_DISCONNECTED; // problem within resolver
					dns_release_lock();
				}
			}			
			else if (dcs_timeout_timer == 0)
			{
				if (dcs_retry_counter > 0)
				{
					dcs_retry_counter --;
				}
				
				if (dcs_retry_counter == 0)
				{
					dcs_state = DCS_DISCONNECTED;
					dns_release_lock();
				}
				else
				{
					dcs_timeout_timer = DCS_DNS_TIMEOUT;
				}
			}
			break;
			
		case DCS_DNS_REQ_SENT:
			if (dns_result_available()) // resolver is ready
			{
				if (dns_get_A_addr(dcs_server_ipaddr) < 0) // DNS didn't work
				{
					dcs_state = DCS_DISCONNECTED;
				}
				else
				{
					dcs_udp_local_port = udp_get_new_srcport();
					
					udp_socket_ports[UDP_SOCKET_DCS] = dcs_udp_local_port;
					
					dcs_link_to(current_module);
					
					dcs_state = DCS_CONNECT_REQ_SENT;
					dcs_retry_counter = DCS_CONNECT_RETRIES;
					dcs_timeout_timer =  DCS_CONNECT_REQ_TIMEOUT;
				}
				
				dns_release_lock();
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
			dcs_retry_counter = DCS_DNS_RETRIES;
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
		
		
	}
}

static void dcs_keepalive_response (void);


void dcs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	if (memcmp(ipv4_src_addr, dcs_server_ipaddr, sizeof ipv4_addr) != 0)
	{
		// packet is not from the currently selected server
		return;
	}
	
	if (data_len >= 100) // voice packet
	{
		if (memcmp(data, "0001", 4) == 0)  // first four bytes "0001"
		{
			if (data[14] == current_module) // filter out the right channel
			{
				dstarProcessDCSPacket( data );
			}
		}
	}
	else if (data_len == 14) // connect response packet
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
	else if (data_len == 9)  // keep alive packet (old version)
	{
		if (dcs_state == DCS_CONNECTED)
		{
			dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
			dcs_keepalive_response();
		}			
	}
	else if (data_len == 22)  // keep alive packet (new version)
	{
		if (dcs_state == DCS_CONNECTED)
		{
			dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
			dcs_keepalive_response();
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

void dcs_select_reflector (int server_num, char module)
{
	if (dcs_state != DCS_DISCONNECTED)  // only when disconnected
		return;
		
	current_server = server_num;
	current_module = module;
	
}



#define DCS_UDP_PORT 30051

#define DCS_VOICE_FRAME_SIZE  (100)


static eth_txmem_t * dcs_get_packet_mem (int udp_size)
{
	return udp4_get_packet_mem( udp_size, dcs_udp_local_port, DCS_UDP_PORT,
		dcs_server_ipaddr );
	
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

#define DCS_REGISTER_MODULE  'D'

// #define DCS_CONNECT_FRAME_SIZE  19
#define DCS_CONNECT_FRAME_SIZE  519

static void dcs_link_to (int module)
{
	eth_txmem_t * packet = dcs_get_packet_mem( DCS_CONNECT_FRAME_SIZE );
	
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
	d[8] = DCS_REGISTER_MODULE; // my repeater module
	d[9] = module; // module to link to
	d[10] = 0;
	
	dcs_get_current_reflector_name(buf);
	
	memcpy(d + 11, buf, 7);
	
	// d[18] = ' ';
	
	d[18] = '@';
	// memcpy(d + 19, dcs_html_info, sizeof dcs_html_info);
	infocpy(d + 19);
	
	dcs_calc_chksum_and_send( packet, DCS_CONNECT_FRAME_SIZE );
}


#define DCS_KEEPALIVE_RESP_FRAME_SIZE  17

static void dcs_keepalive_response (void)
{
	eth_txmem_t * packet = dcs_get_packet_mem( DCS_KEEPALIVE_RESP_FRAME_SIZE );
	
	if (packet == NULL)
	{
		return;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	memcpy (d, settings.s.my_callsign, 7);
	
	d[7] = DCS_REGISTER_MODULE;
	d[8] = 0;
	
	char buf[8];
	dcs_get_current_reflector_name(buf);
	
	memcpy(d + 9, buf, 8);
	
	dcs_calc_chksum_and_send( packet, DCS_KEEPALIVE_RESP_FRAME_SIZE );
}





static int slow_data_count = 0;
static uint8_t slow_data[5];

void send_dcs (int session_id, int last_frame, char dcs_frame_counter)
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
	memcpy (d + 15, settings.s.my_callsign, 7);
	d[22] = DCS_REGISTER_MODULE;
	memcpy(d + 23, "CQCQCQ  ", 8); 
	memcpy (d + 31, settings.s.my_callsign, 8);
	memcpy (d + 39, settings.s.my_ext, 4);
	
	d[43] = (session_id >> 8) & 0xFF;
	d[44] = session_id & 0xFF;
	
	d[45] = dcs_frame_counter | ((last_frame != 0) ? 0x40 : 0);
	
	memcpy (d + 46, dcs_ambe_data, sizeof dcs_ambe_data);
	
	d[58] = dcs_tx_counter & 0xFF;
	d[59] = (dcs_tx_counter >> 8) & 0xFF;
	d[60] = (dcs_tx_counter >> 16) & 0xFF;
	
	d[61] = 0x01; // Frame Format version low
	// d[62] = 0x00; // Frame Format version high
	d[63] = 0x21; // Language Set 0x21
	
	memcpy (d + 64, settings.s.txmsg, 20);
	
	if (last_frame != 0)
	{
		d[55] = 0x55;
		d[56] = 0x55;
		d[57] = 0x55;
	}
	else
	{
		if (dcs_frame_counter == 0) // sync
		{
			d[55] = 0x55;
			d[56] = 0x2d;
			d[57] = 0x16;
		}
		else
		{
			
			if ((dcs_frame_counter >= 1) && (dcs_frame_counter <= 8)
				&& (dcs_tx_counter < 20))  // send tx_msg only in first frame
			{
				int i = (dcs_frame_counter - 1) >> 1;
				if (dcs_frame_counter & 1)
				{
					d[55] = (0x40 + i) ^ 0x70;
					d[56] = settings.s.txmsg[ i * 5 + 0 ] ^ 0x4F;
					d[57] = settings.s.txmsg[ i * 5 + 1 ] ^ 0x93;
				}
				else
				{
					d[55] = settings.s.txmsg[ i * 5 + 2 ] ^ 0x70;
					d[56] = settings.s.txmsg[ i * 5 + 3 ] ^ 0x4F;
					d[57] = settings.s.txmsg[ i * 5 + 4 ] ^ 0x93;
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
						d[55] = 0x16;  // NOP
						d[56] = 0x29;
						d[57] = 0xf5;
					}
					else
					{
						d[55] = (0x30 + slow_data_count) ^ 0x70;
						d[56] = slow_data[ 0 ] ^ 0x4F;
						d[57] = slow_data[ 1 ] ^ 0x93;
					}
				}
				else
				{
					if (slow_data_count <= 2)
					{
						d[55] = 0x16;  // NOP
						d[56] = 0x29;
						d[57] = 0xf5;
					}
					else
					{
						d[55] = slow_data[ 2 ] ^ 0x70;
						d[56] = slow_data[ 3 ] ^ 0x4F;
						d[57] = slow_data[ 4 ] ^ 0x93;
					}
				}
				
			}
			
		
		}
	}
	
	
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

