/*

Copyright (C) 2015   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * ccs.c
 *
 * Created: 03.09.2015 09:01:03
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"

#include "task.h"

#include "gcc_builtin.h"

#include "dstar.h"
#include "up_io/eth_txmem.h"

#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/snmp.h"

#include "vdisp.h"
#include "up_io/eth.h"

#include "dcs.h"
#include "up_dstar/settings.h"

#include "up_crypto/up_crypto.h"
#include "up_net/dns2.h"

#include "up_app/a_lib_internal.h"
#include "up_app/a_lib.h"
#include "sw_update.h"
#include "rx_dstar_crc_header.h"


#include "ccs.h"



#define CCS_DISCONNECTED		1
#define CCS_CONNECT_REQ_SENT	2
#define CCS_CONNECTED			3
#define CCS_DISCONNECT_REQ_SENT	4
#define CCS_DNS_REQ_SENT		5
#define CCS_DNS_REQ				6
#define CCS_WAIT				7
#define CCS_DISCONNECT_REQ		8

static uint8_t ccs_state;
static uint8_t ccs_timeout_timer;
static uint8_t ccs_retry_counter;
static int dns_handle;
// static uint8_t ccs_current_server;
/* static const char * ccs_server_dns_name[2] = {
		"CCS702.xreflector.net",
		"CCS704.xreflector.net"
}; */

static char ccs_server_dns_name[23]; // dns name of CCS server e.g. "ccs702.xreflector.net"

static uint8_t ccs_server_ipaddr[4];

static char ccs_rx_callsign[8];
static char ccs_rx_callsign_ext[4];
static char ccs_rx_reflector[8];


#define CCS_KEEPALIVE_TIMEOUT  100
#define CCS_CONNECT_REQ_TIMEOUT  6
#define CCS_CONNECT_RETRIES		  3
#define CCS_DISCONNECT_REQ_TIMEOUT  6
#define CCS_DISCONNECT_RETRIES	  3
#define CCS_DNS_TIMEOUT			 2
#define CCS_DNS_RETRIES		  3
#define CCS_DNS_INITIAL_RETRIES		  15
#define CCS_FAILURE_TIMEOUT		30


#define CCS_KEEPALIVE_RESP_FRAME_SIZE   25
#define CCS_CONNECT_FRAME_SIZE   39
#define CCS_DISCONNECT_FRAME_SIZE 19
#define CCS_INFO_FRAME_SIZE   100

#define CCS_UDP_PORT	30062


//                                   1234567890123456789012345678901234567890
static const char * connect_frame = "XX0XXX  BA@JO62QM @UP4DAR-CCS7-Client  "; // len 39 Bytes

static const char info_frame_template[CCS_INFO_FRAME_SIZE] = {
	'0', '0', '0', '1', 0,0,0,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // reflector
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // this repeater callsign
	'C', 'Q', 'C', 'Q', 'C', 'Q', 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // mycall
	0x20, 0x20, 0x20, 0x20,							// mycall_ext
	0x00, 0x00,					// stream id
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   // 45-60  0x00
	0x01,  // 61
	0x00,  // 62
	0x21,  // 63
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  // 64-71
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  // 72-79
	0x20, 0x20, 0x20, 0x20,							 // 80-83
	0,0,0,0, 0,0,0,0, 0,							 // 84-92
	0x30,  // data source: RF
	0, 0, 0, 0, 0, 0							// 94-99
};

static void ccs_send_connect(void);
static void ccs_send_disconnect(void);
static void ccs_send_info_frame(void);

static void ccs_set_dns_name(void)
{
	memcpy(ccs_server_dns_name, "CCS702.xreflector.net", 22);
	
	if (repeater_mode)
	{
		memcpy(ccs_server_dns_name, "CCS704", 6);
	}
}

void ccs_init(void)
{
	ccs_state = CCS_DISCONNECTED;
	ccs_timeout_timer = 0;
	// ccs_current_server = 0;
	ccs_rx_callsign[0] = 0;
	ccs_set_dns_name();
}

const char * ccs_current_servername(void)
{
	return ccs_server_dns_name;
}


/*
static void ccs_next_server(void)
{
	int num_servers = (sizeof ccs_server_dns_name) / (sizeof (const char *));
	
	ccs_current_server ++;
	
	if (ccs_current_server >= num_servers)
	{
		ccs_current_server = 0;
	}
}

*/

void ccs_service (void)
{
	if (ccs_timeout_timer > 0)
	{
		ccs_timeout_timer --;
	}
	
	
	switch (ccs_state)
	{
		case CCS_CONNECTED:
		if (ccs_timeout_timer == 0)
		{
			ccs_timeout_timer = 2; // 1 second
			ccs_state = CCS_WAIT;
			udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
			vd_prints_xy(VDISP_DEBUG_LAYER, 104, 16, VDISP_FONT_6x8, 0, "NOWD");
		}
		else
		{
			if (ccs_rx_callsign[0] != 0)
			{
				ccs_send_info_frame();
				ccs_rx_callsign[0] = 0;
			}
		}
		break;
		
		
		
		case CCS_CONNECT_REQ_SENT:
		if (ccs_timeout_timer == 0)
		{
			if (ccs_retry_counter > 0)
			{
				ccs_retry_counter --;
			}
			
			if (ccs_retry_counter == 0)
			{
				ccs_timeout_timer = CCS_FAILURE_TIMEOUT;
				ccs_state = CCS_WAIT;
				// ccs_next_server();
				udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 16, VDISP_FONT_6x8, 0, "RQTO");
			}
			else
			{
				ccs_send_connect();
				ccs_timeout_timer = CCS_CONNECT_REQ_TIMEOUT;
			}
		}
		break;
		
		case CCS_DISCONNECT_REQ:
			ccs_send_disconnect();
			ccs_timeout_timer = CCS_DISCONNECT_REQ_TIMEOUT;
			ccs_state = CCS_DISCONNECT_REQ_SENT;
			ccs_retry_counter = CCS_DISCONNECT_RETRIES;
		
		break;
		
		
		case CCS_DISCONNECT_REQ_SENT:
		if (ccs_timeout_timer == 0)
		{
			if (ccs_retry_counter > 0)
			{
				ccs_retry_counter --;
			}
			
			if (ccs_retry_counter == 0)
			{
				ccs_state = CCS_DISCONNECTED;
				udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
			}
			else
			{
				ccs_send_disconnect();
				ccs_timeout_timer = CCS_DISCONNECT_REQ_TIMEOUT;
			}
		}
		break;
		
		case CCS_DNS_REQ:
		
		dns_handle = dns2_req_A(ccs_current_servername());
		
		if (dns_handle >= 0) // resolver not busy
		{
			ccs_state = CCS_DNS_REQ_SENT;
		}
		else if (ccs_timeout_timer == 0)
		{
			if (ccs_retry_counter > 0)
			{
				ccs_retry_counter --;
			}
			
			if (ccs_retry_counter == 0)
			{
				ccs_timeout_timer = CCS_FAILURE_TIMEOUT;
				ccs_state = CCS_WAIT;
				// ccs_next_server();
			}
			else
			{
				ccs_timeout_timer = CCS_DNS_TIMEOUT;
			}
		}
		break;
		
		case CCS_DNS_REQ_SENT:
		if (dns2_result_available(dns_handle)) // resolver is ready
		{
			uint8_t * addrptr;
			if (dns2_get_A_addr(dns_handle, &addrptr) <= 0) // DNS didn't work
			{
				ccs_timeout_timer = CCS_FAILURE_TIMEOUT;
				ccs_state = CCS_WAIT;
				// ccs_next_server();
			}
			else
			{
				memcpy (ccs_server_ipaddr, addrptr, 4); // use first address of DNS result
				
				udp_socket_ports[UDP_SOCKET_CCS] = udp_get_new_srcport();
				
				ccs_send_connect();
				
				ccs_state = CCS_CONNECT_REQ_SENT;
				ccs_retry_counter = CCS_CONNECT_RETRIES;
				ccs_timeout_timer =  CCS_CONNECT_REQ_TIMEOUT;
			}
			
			dns2_free(dns_handle);
		}
		break;
		
		case CCS_WAIT:
		if (ccs_timeout_timer == 0)
		{
			ccs_retry_counter = CCS_DNS_INITIAL_RETRIES;
			ccs_timeout_timer = CCS_DNS_TIMEOUT;
			ccs_state = CCS_DNS_REQ;
		}
		break;
	}
}



static void ccs_send_connect(void)
{
	
		eth_txmem_t * packet = udp4_get_packet_mem( CCS_CONNECT_FRAME_SIZE,
				 udp_socket_ports[UDP_SOCKET_CCS], CCS_UDP_PORT, ccs_server_ipaddr );
		
		if (packet == NULL)
		return;
		
		uint8_t* d = packet->data + 42; // skip ip+udp header
		
		memcpy (d, connect_frame, CCS_CONNECT_FRAME_SIZE);
		memcpy (d, repeater_callsign, 7);

		d[8] = repeater_callsign[7];
		
		udp4_calc_chksum_and_send(packet, ccs_server_ipaddr);
}



static void ccs_send_info_frame(void)
{
	
	eth_txmem_t * packet = udp4_get_packet_mem( CCS_INFO_FRAME_SIZE,
	udp_socket_ports[UDP_SOCKET_CCS], CCS_UDP_PORT, ccs_server_ipaddr );
	
	if (packet == NULL)
	return;
	
	uint8_t* d = packet->data + 42; // skip ip+udp header
	
	memcpy (d, info_frame_template, CCS_INFO_FRAME_SIZE);

	memcpy (d + 7, ccs_rx_reflector, 8);	
	memcpy (d + 15, repeater_callsign, 8);
	memcpy (d + 31, ccs_rx_callsign, 8);
	memcpy (d + 39, ccs_rx_callsign_ext, 4);
	
	udp4_calc_chksum_and_send(packet, ccs_server_ipaddr);
}


static void ccs_send_disconnect(void)
{
	
	eth_txmem_t * packet = udp4_get_packet_mem( CCS_DISCONNECT_FRAME_SIZE,
			 udp_socket_ports[UDP_SOCKET_CCS], CCS_UDP_PORT, ccs_server_ipaddr );
	
	if (packet == NULL)
	return;
	
	uint8_t* d = packet->data + 42; // skip ip+udp header
	
	memset(d, 0x20, CCS_DISCONNECT_FRAME_SIZE); // fill with ASCII spaces
	
	memcpy (d, repeater_callsign, 7);

	d[8] = repeater_callsign[7];

	udp4_calc_chksum_and_send(packet, ccs_server_ipaddr);
	
}

static void ccs_keepalive_response(void)
{
	
	eth_txmem_t * packet = udp4_get_packet_mem( CCS_KEEPALIVE_RESP_FRAME_SIZE,
			 udp_socket_ports[UDP_SOCKET_CCS], CCS_UDP_PORT, ccs_server_ipaddr );
		
	if (packet == NULL)
	return;
		
	uint8_t* d = packet->data + 42; // skip ip+udp header
		
	memset(d, 0x20, CCS_KEEPALIVE_RESP_FRAME_SIZE); // fill with ASCII spaces
	
	memcpy (d, repeater_callsign, 7);

	memcpy (d + 8, repeater_callsign, 7);

	udp4_calc_chksum_and_send(packet, ccs_server_ipaddr);
	
}


void ccs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	if (memcmp(ipv4_src_addr, ccs_server_ipaddr, sizeof ipv4_addr) != 0)
	{
		// packet is not from the currently selected server
		return;
	}
	
	
	if (data_len == 14) // connect response packet
	{
		if (ccs_state == CCS_CONNECT_REQ_SENT)
		{
			if ((data[8] == repeater_callsign[7]) &&
			  (memcmp(data + 10, "ACK", 3) == 0))
			{
				ccs_rx_callsign[0] = 0;
				ccs_state = CCS_CONNECTED;
				ccs_timeout_timer = CCS_KEEPALIVE_TIMEOUT;
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 16, VDISP_FONT_6x8, 0, "ACK ");
			}
			else
			{
				ccs_state = CCS_DISCONNECTED;
				udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 16, VDISP_FONT_6x8, 0, "NACK");
			} 
		}
		else if (ccs_state == CCS_DISCONNECT_REQ_SENT)
		{
			if (data[9] == ' ')
			{
				ccs_state = CCS_DISCONNECTED;
				udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
				vd_prints_xy(VDISP_DEBUG_LAYER, 104, 16, VDISP_FONT_6x8, 0, "DISC");
			}
		}
	}
	else if (data_len == CCS_KEEPALIVE_RESP_FRAME_SIZE)  // keep alive packet 
	{
		if (ccs_state == CCS_CONNECTED)
		{
			ccs_timeout_timer = CCS_KEEPALIVE_TIMEOUT;
			ccs_keepalive_response();
		}
	}
}

int ccs_is_connected (void)
{
	if ((ccs_state == CCS_CONNECTED) || (ccs_state == CCS_DISCONNECT_REQ_SENT))
		return 1;
		
	return 0;
}

void ccs_start (void)
{
	if (ccs_state == CCS_DISCONNECTED)
	{
		char buf[8];
		
		dcs_get_current_reflector_name(buf);
		
		if (memcmp(buf, "DCS", 3) == 0)
		{
			ccs_set_dns_name();
			ccs_state = CCS_DNS_REQ;
		}
	}
}

void ccs_stop(void)
{
	if (ccs_state == CCS_CONNECTED)
	{
		ccs_state = CCS_DISCONNECT_REQ;
	}
	else
	{
		udp_socket_ports[UDP_SOCKET_CCS] = 0; // stop receiving frames
		ccs_state = CCS_DISCONNECTED;
	}
}



void ccs_send_info(const uint8_t * mycall, const uint8_t * mycall_ext, int remove_module_char)
{
	if (!ccs_is_connected())
		return;
		
	if (dcs_is_connected())
	{
		dcs_get_current_reflector_name(ccs_rx_reflector);
		if (memcmp(ccs_rx_reflector, "DCS", 3) != 0)
		{
			memset(ccs_rx_reflector, 0x20, 8);
		}
	}
	else
	{
		memset(ccs_rx_reflector, 0x20, 8);
	}
	
	memcpy(ccs_rx_callsign, mycall, 8);
	if (remove_module_char != 0)
	{
		ccs_rx_callsign[7] = 0x20;
	}
	memcpy(ccs_rx_callsign_ext, mycall_ext, 4);
}