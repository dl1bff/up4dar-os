
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

static const char dcs_html_info[] = "<table border=\"0\" width=\"95%\"><tr>"
				   "<td width=\"4%\"><img border=\"0\" src=dongle.jpg></td>"
				   "<td width=\"96%\"><font size=\"2\">"
				   "<b>UP4DAR</b>"
				   "</font></td>"
				   "</tr></table>";


#define UDP_MIN_PORT 11000
#define UDP_MAX_PORT 11050

int dcs_udp_local_port;

static int dcs_state;

#define DCS_KEEPALIVE_TIMEOUT  60
#define DCS_CONNECT_REQ_TIMEOUT  6
#define DCS_DISCONNECT_REQ_TIMEOUT  6

static int dcs_timeout_timer;
static int dcs_retry_counter;

#define DCS_DISCONNECTED		1
#define DCS_CONNECT_REQ_SENT	2
#define DCS_CONNECTED			3
#define DCS_DISCONNECT_REQ_SENT	4


static int current_module;
static int current_server;

#define NUM_SERVERS 2

const static struct dcs_servers
 {
	char name[7];
	uint8_t ipv4_a[4];
 } servers[NUM_SERVERS] = {
  	 {  "DCS001",   { 87, 106, 3, 249 } },
	 {  "DCS002",   { 87, 106, 48, 7 } }   	 
};

void dcs_init(void)
{
	dcs_udp_local_port = UDP_MIN_PORT;

	dcs_state = DCS_DISCONNECTED;	
	
	current_module = 'C';
	current_server = 0;
}




static void dcs_link_to (int module);





void dcs_service (void)
{
	if (dcs_timeout_timer > 0)
	{
		dcs_timeout_timer --;	
	}
	
	switch (dcs_state)
	{
		case DCS_CONNECTED:
			if (dcs_timeout_timer == 0)
			{
				dcs_state = DCS_DISCONNECTED;
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
				}
				else
				{
					dcs_link_to(' ');
					dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
				}
			}
			break;
	}
}




void dcs_on_off (void)
{
	switch (dcs_state)
	{
		case DCS_CONNECTED:
			dcs_link_to(' ');
			
			dcs_state = DCS_DISCONNECT_REQ_SENT;
			dcs_retry_counter = 3;
			dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
			break;
		
		case DCS_DISCONNECTED:
		
			dcs_udp_local_port ++;
			
			if (dcs_udp_local_port > UDP_MAX_PORT)
			{
				dcs_udp_local_port = UDP_MIN_PORT;
			}
			
			dcs_link_to(current_module);
			
			dcs_state = DCS_CONNECT_REQ_SENT;
			dcs_retry_counter = 3;
			dcs_timeout_timer = DCS_CONNECT_REQ_TIMEOUT;
			break;
	}
}

static void dcs_keepalive_response (void);


void dcs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	if (memcmp(ipv4_src_addr, servers[current_server].ipv4_a, sizeof ipv4_addr) != 0)
	{
		// packet is not from the currently selected server
		return;
	}
	
	if (data_len >= 100) // voice packet
	{
		if (memcmp(data, "0001", 4) == 0)  // first four bytes "0001"
		{
			dstarProcessDCSPacket( data );
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
			}
		}
		else if (dcs_state == DCS_DISCONNECT_REQ_SENT)
		{
			if (data[9] == ' ')
			{
				dcs_state = DCS_DISCONNECTED;
			}
		}
	}
	else if (data_len == 9)  // keep alive packet
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

void dcs_get_current_reflector_name (char * s)
{
	memcpy(s, servers[current_server].name, 6);
	s[6] = ' ';
	s[7] = current_module;
}




uint8_t dcs_ambe_data[9];

static int dcs_tx_counter = 0;
static int dcs_frame_counter = 0;


void dcs_reset_tx_counters(void)
{
	dcs_frame_counter = 0;
	dcs_tx_counter = 0;
}

void dcs_select_reflector (int go_up)
{
	if (dcs_state != DCS_DISCONNECTED)  // only when disconnected
		return;
		
	if (go_up != 0)
	{
		current_module++;
		if (current_module > 'Z')
		{
			current_module = 'A';
			current_server ++;
			
			if (current_server >= NUM_SERVERS)
			{
				current_server = 0;
			}
		}
	}
	else
	{
		current_module --;
		if (current_module < 'A')
		{
			current_module = 'Z';
			current_server --;
			
			if (current_server < 0)
			{
				current_server = NUM_SERVERS - 1;
			}
		}
	}
}




static const uint8_t dcs_frame_header[] =
	{	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xDE, 0x1B, 0xFF, 0x00, 0x00, 0x01,
		0x08, 0x00,  // IPv4
		0x45, 0x00, // v4, DSCP
		0x00, 0x00, // ip length (will be set later)
		0x01, 0x01, // ID
		0x40, 0x00,  // DF don't fragment, offset = 0
		0x40, // TTL
		0x11, // UDP = 17
		0x00, 0x00,  // header chksum (will be calculated later)
		192, 168, 1, 33,  // source
		192, 168, 1, 255,  // destination
		0xb0, 0xb0,  // source port
		0x40, 0x01,  // destination port 16385
		0x00, 0x00,    //   UDP length (will be set later)
		0x00, 0x00  // UDP chksum (0 = no checksum)	
};

#define DCS_UDP_PORT 30051

#define DCS_VOICE_FRAME_SIZE  (100)


static eth_txmem_t * dcs_get_packet_mem (int udp_size)
{
	eth_txmem_t * packet = eth_txmem_get( UDP_PACKET_SIZE(udp_size) );
	
	if (packet == NULL)
	{
		vdisp_prints_xy( 40, 56, VDISP_FONT_6x8, 0, "NOMEM" );
		return NULL;
	}
	
	memset ( packet->data + (8 + 20 + 14), 0, udp_size);
	
	memcpy (packet->data, dcs_frame_header, sizeof dcs_frame_header);
		
	eth_set_src_mac_and_type( packet->data, 0x0800 );
	
	memcpy(packet->data + 26, ipv4_addr, sizeof ipv4_addr); // src IP
	memcpy(packet->data + 30, servers[current_server].ipv4_a, sizeof ipv4_addr); // dest IP
	
	packet->data[34] = dcs_udp_local_port >> 8;   // UDP source port
	packet->data[35] = dcs_udp_local_port & 0xFF;
	
	packet->data[36] = DCS_UDP_PORT >> 8;
	packet->data[37] = DCS_UDP_PORT & 0xFF;
	
	return packet;
}

static void dcs_calc_chksum_and_send (eth_txmem_t * packet, int udp_size)
{
	uint8_t * dcs_frame = packet->data;
	
	int udp_length = udp_size + 8;  // include UDP header
	
	((unsigned short *) (dcs_frame + 14)) [1] = udp_length + 20; // IP len
	
	((unsigned short *) (dcs_frame + 14)) [12] = udp_length; // UDP len
	
	int sum = 0;
	int i;
	
	for (i=0; i < 10; i++) // 20 Byte Header
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (dcs_frame + 14)) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
	((unsigned short *) (dcs_frame + 14)) [5] = sum; // checksumme setzen
	
	
	ip_addr_t  tmp_addr;
	
	
	if (ipv4_get_neigh_addr(&tmp_addr, servers[current_server].ipv4_a ) != 0)  // get addr of neighbor
	{
		// neighbor could not be set
		eth_txmem_free(packet); // throw away packet
	}
	else
	{
		ipneigh_send_packet (&tmp_addr, packet);
	}
		
}


#define DCS_CONNECT_FRAME_SIZE  19

static void dcs_link_to (int module)
{
	eth_txmem_t * packet = dcs_get_packet_mem( DCS_CONNECT_FRAME_SIZE );
	
	if (packet == NULL)
	{
		return;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	memcpy (d, settings.s.my_callsign, 7);
	
	d[7] = ' ';
	d[8] = 'B'; // my repeater module B
	d[9] = module; // module to link to
	d[10] = 0;
	
	char buf[8];
	dcs_get_current_reflector_name(buf);
	
	memcpy(d + 11, buf, 7);
	
	d[18] = ' ';
	
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
	
	d[7] = ' ';
	d[8] = 0;
	
	char buf[8];
	dcs_get_current_reflector_name(buf);
	
	memcpy(d + 9, buf, 7);
	
	d[16] = ' ';
	
	dcs_calc_chksum_and_send( packet, DCS_KEEPALIVE_RESP_FRAME_SIZE );
}


static int slow_data_count = 0;
static uint8_t slow_data[5];

void send_dcs (int session_id, int last_frame)
{
	if (dcs_state == DCS_CONNECTED)  // only send voice if connected
	{
		
	int frame_size = DCS_VOICE_FRAME_SIZE;
	
	if ((dcs_tx_counter & 0x7F) == 0x03)  // approx every 2 seconds
	{
		frame_size += 500; // send HTML info
	}		
		
	eth_txmem_t * packet = dcs_get_packet_mem( frame_size );
	
	if (packet == NULL)
	{
		return;
	}
	
	
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	if (frame_size > DCS_VOICE_FRAME_SIZE) // send HTML info
	{
		memcpy(d + 100, dcs_html_info, sizeof dcs_html_info);
	}
	
	memcpy(d, "0001", 4);
	
	d[4] = 0;  // flags
	d[5] = 0;
	d[6] = 0;
	
	char buf[8];
	
	dcs_get_current_reflector_name( buf );
	
	memcpy (d + 7, buf, 8);
	memcpy (d + 15, buf, 8);
	memcpy(d + 23, "CQCQCQ  ", 8); 
	memcpy (d + 31, settings.s.my_callsign, 8);
	memcpy (d + 39, "    ", 4);
	
	d[43] = (session_id >> 8) & 0xFF;
	d[44] = session_id & 0xFF;
	
	d[45] = dcs_frame_counter | ((last_frame != 0) ? 0x40 : 0);
	
	memcpy (d + 46, dcs_ambe_data, sizeof dcs_ambe_data);
	
	d[58] = dcs_tx_counter & 0xFF;
	d[59] = (dcs_tx_counter >> 8) & 0xFF;
	d[60] = (dcs_tx_counter >> 16) & 0xFF;
	
	d[61] = 0x01;
	
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
			extern const char dstar_tx_msg[20];
			
			if ((dcs_frame_counter >= 1) && (dcs_frame_counter <= 8)
				&& (dcs_tx_counter < 20))  // send tx_msg only in first frame
			{
				int i = (dcs_frame_counter - 1) >> 1;
				if (dcs_frame_counter & 1)
				{
					d[55] = (0x40 + i) ^ 0x70;
					d[56] = dstar_tx_msg[ i * 5 + 0 ] ^ 0x4F;
					d[57] = dstar_tx_msg[ i * 5 + 1 ] ^ 0x93;
				}
				else
				{
					d[55] = dstar_tx_msg[ i * 5 + 2 ] ^ 0x70;
					d[56] = dstar_tx_msg[ i * 5 + 3 ] ^ 0x4F;
					d[57] = dstar_tx_msg[ i * 5 + 4 ] ^ 0x93;
				}
			}
			else
			{
				if (dcs_frame_counter & 1)
				{
					// slow_data_count = gps_get_slow_data(slow_data);
					slow_data_count = 0;
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
	
	dcs_frame_counter ++;
	
	if (dcs_frame_counter >= 21)
	{
		dcs_frame_counter = 0;
	}
	
	dcs_tx_counter ++;
	
	int secs = dcs_tx_counter / 50;
				
	if ((last_frame != 0) || ((dcs_tx_counter & 0x0F) == 0x01)) // show seconds on every 16th call
	{
		char buf[4];
		vdisp_i2s(buf, 3, 10, 0, secs);
		vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, last_frame ? 0 : 1, buf );
		vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, last_frame ? 0 : 1, "s" );
	}
	
}

