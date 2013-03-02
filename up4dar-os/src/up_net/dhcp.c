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
 * dhcp.c
 *
 * Created: 13.05.2012 05:19:42
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "gcc_builtin.h"
#include <asf.h>

#include "board.h"
#include "gpio.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "ipneigh.h"
#include "ipv4.h"

#include "dhcp.h"

#include "up_dstar/vdisp.h"
#include "up_crypto/up_crypto.h"

#include "up_dstar/vdisp.h"

static int dhcp_state;


#define DHCP_NO_LINK	1
#define DHCP_DISCOVER	2
#define DHCP_LINK_LOST	3
#define DHCP_REQUEST_SENT	4
#define DHCP_REQUEST_RESENT	5
#define DHCP_REBIND_REQ_SENT	6

#define DHCP_READY		10




#define DHCP_TIMEOUT_TIMER_MAX		86400 
    // 1 day
	
#define DHCP_TIMEOUT_TIMER_MIN		10
    // 10 seconds
	
#define DHCP_REQ_TIMEOUT_TIMER 8



static int dhcp_timer;

static unsigned int dhcp_xid = 0;

static int dhcp_T1;



typedef struct bootp_header
{
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	
	uint16_t xid_hi; // caution: memory is probably not 32bit aligned!
	uint16_t xid_lo; // therefore use two 16bit values
	uint16_t secs;
	uint16_t flags;
	
	uint8_t ciaddr[4];
	uint8_t yiaddr[4];
	uint8_t siaddr[4];
	uint8_t giaddr[4];
	
	uint8_t chaddr[16];
	
	uint8_t sname[64];
	
	uint8_t file[128];
	
	uint8_t magic_cookie[4];
		
} bootp_header_t;


static const uint8_t dhcp_magic_cookie[4] = { 99, 130, 83, 99 };


void dhcp_init(void)
{
	dhcp_state = DHCP_NO_LINK;
	dhcp_timer = 0;
	dhcp_T1 = 900; // 15 minutes if not overwritten by DHCPOFFER
}



static const uint8_t dhcp_frame_header[] =
	{	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // broadcast address
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // source will be added later
		0x08, 0x00,  // IPv4
		0x45, 0x00, // v4, DSCP
		0x00, 0x00, // ip length (will be set later)
		0x01, 0x01, // ID
		0x40, 0x00,  // DF don't fragment, offset = 0
		0x40, // TTL
		0x11, // UDP = 17
		0x00, 0x00,  // header chksum (will be calculated later)
		0, 0, 0, 0,  // source 0.0.0.0
		255, 255, 255, 255,  // destination
		0x00, 0x44,  // source port, client bootp 68
		0x00, 0x43,  // destination port, server bootp 67
		0x00, 0x00,    //   UDP length (will be set later)
		0x00, 0x00  // UDP chksum (0 = no checksum)	
};

static eth_txmem_t * dhcp_get_packet_mem (int udp_size)
{
	eth_txmem_t * packet = eth_txmem_get( udp_size + 8 + 20 + 14 );
	
	if (packet == NULL)
	{
		return NULL;
	}
	
	memcpy (packet->data, dhcp_frame_header, sizeof dhcp_frame_header);
		
	eth_set_src_mac_and_type( packet->data, 0x0800 );
	
	
	
	uint8_t * d = packet->data + (8 + 20 + 14);
	
	memset ( d, 0, udp_size);
	
	// memcpy(packet->data + 26, ipv4_addr, sizeof ipv4_addr); // src IP
	// memcpy(packet->data + 30, servers[current_server].ipv4_a, sizeof ipv4_addr); // dest IP
	
	
	
	bootp_header_t * m = (bootp_header_t *) d;
	
	m->op = 1; // BOOTREQUEST
	m->htype = 1; // ethernet
	m->hlen = 6;
	
	memcpy (m->chaddr, mac_addr, sizeof mac_addr);
	
	m->xid_hi = dhcp_xid >> 16;
	m->xid_lo = dhcp_xid & 0xFFFF;
	
	memcpy (m->magic_cookie, dhcp_magic_cookie, sizeof dhcp_magic_cookie);
	
	return packet;
}


static void dhcp_calc_chksum_and_send (eth_txmem_t * packet, int udp_size)
{
	uint8_t * dhcp_frame = packet->data;
	
	int udp_length = udp_size + 8;  // include UDP header
	
	((unsigned short *) (dhcp_frame + 14)) [1] = udp_length + 20; // IP len
	
	((unsigned short *) (dhcp_frame + 14)) [12] = udp_length; // UDP len
	
	int sum = 0;
	int i;
	
	for (i=0; i < 10; i++) // 20 Byte Header
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (dhcp_frame + 14)) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
	((unsigned short *) (dhcp_frame + 14)) [5] = sum; // checksumme setzen
	
	/*
	ip_addr_t  tmp_addr;
	
	
	if (ipv4_get_neigh_addr(&tmp_addr, servers[current_server].ipv4_a ) != 0)  // get addr of neighbor
	{
		// neighbor could not be set
		eth_txmem_free(packet); // throw away packet
	}
	else
	{  
		
		ipneigh_send_packet (&tmp_addr, packet);
	 }  */
		
	eth_txmem_send(packet);
	
}



static const uint8_t dhcp_discover_packet[] =
	{	53, 0x01, 0x01,   // DHCP Message Type DHCPDISCOVER
		55, 0x04, 1, 3, 5, 42, // Request Parameter List: netmask, router, DNS, NTP
		0xFF  // END
	};	
	

	

static void dhcp_send_discover(void)
{
	// vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, "DHCP");
	
	int pkt_size = (sizeof (bootp_header_t)) + (sizeof dhcp_discover_packet);
	
	
	eth_txmem_t * packet = dhcp_get_packet_mem( pkt_size );
	
	
	if (packet == NULL)
		return; // nomem
		
	uint8_t * d = packet->data + ( 14 + 20 + 8 + (sizeof (bootp_header_t)));
	
	memcpy (d, dhcp_discover_packet, sizeof dhcp_discover_packet);

	dhcp_calc_chksum_and_send(packet, pkt_size );
	
}


static uint8_t dhcp_request_packet[] =
	{
		53, 0x01, 0x03, // DHCP Message Type DHCPREQUEST
		50, 0x04, 0,0,0,0,  // requested IP address
		54, 0x04, 0,0,0,0,  // server identifier
		55, 0x04, 0x01, 0x03, 0x06, 42, // Request Parameter List: netmask, router, DNS, NTP
		0xFF  // END
	};
	
static uint8_t dhcp_rebind_packet[] =
{
	53, 0x01, 0x03, // DHCP Message Type DHCPREQUEST
	54, 0x04, 0,0,0,0,  // server identifier
	55, 0x04, 0x01, 0x03, 0x06, 42, // Request Parameter List: netmask, router, DNS, NTP
	0xFF  // END
};

static void dhcp_send_request(int rebind)
{
	int pkt_size = (sizeof (bootp_header_t)) + 
	   ( (rebind == 0) ?  (sizeof dhcp_request_packet) : (sizeof dhcp_rebind_packet)  );
	
	
	
	eth_txmem_t * packet = dhcp_get_packet_mem( pkt_size );
	
	
	if (packet == NULL)
		return; // nomem
		
	uint8_t * d = packet->data + ( 14 + 20 + 8  );
	
	if (rebind == 0)
	{
		memcpy (d + (sizeof (bootp_header_t)), dhcp_request_packet, sizeof dhcp_request_packet);
	
	}
	else
	{	
		bootp_header_t * m = (bootp_header_t *) d;
		
		memcpy (dhcp_rebind_packet + 5, dhcp_request_packet + 11, 4); // server ID
		
		memcpy (d + (sizeof (bootp_header_t)), dhcp_rebind_packet, sizeof dhcp_rebind_packet );
		
		memcpy(m->ciaddr, ipv4_addr, sizeof ipv4_addr); // current src IP
		
		memcpy(packet->data + 26, ipv4_addr, sizeof ipv4_addr); // current src IP
	}
	
	
	

	dhcp_calc_chksum_and_send(packet, pkt_size );
}


static void dhcp_get_new_xid(void)
{
	crypto_get_random_bytes((unsigned char *)& dhcp_xid, sizeof dhcp_xid);
}

void dhcp_set_link_state (int link_up)
{
	if (link_up != 0)
	{
		if (dhcp_state == DHCP_NO_LINK)
		{
			dhcp_get_new_xid();
			
			dhcp_state = DHCP_DISCOVER;
			dhcp_timer = 1;
		}
	}
	else
	{
		if (dhcp_state != DHCP_NO_LINK)
		{
			dhcp_state = DHCP_LINK_LOST;
			dhcp_timer = 1;
			
		}
	}
}


static void clear_dhcp_debug(void)
{
	vd_clear_rect(VDISP_DEBUG_LAYER, 0, 6, 80, 42);
}


void dhcp_service(void)
{
	if (dhcp_timer > 0)
	{
		dhcp_timer --;
		
		
	}
	
	// vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, "DHC1");
	
	char buf[7];
	
	vdisp_i2s(buf, 6, 10, 0, dhcp_timer);
	vd_prints_xy(VDISP_DEBUG_LAYER, 0, 0, VDISP_FONT_4x6, 0, "DHCP Timer");
	vd_prints_xy(VDISP_DEBUG_LAYER, 44, 0, VDISP_FONT_4x6, 0, buf);
	
	switch (dhcp_state)
	{
		case DHCP_DISCOVER:
			if (dhcp_timer == 0)
			{
				clear_dhcp_debug();
				dhcp_send_discover();
				dhcp_timer = DHCP_REQ_TIMEOUT_TIMER;
			}
			break;
			
		case DHCP_LINK_LOST:
			if (dhcp_timer == 0)
			{
				clear_dhcp_debug();
				ipv4_init(); // reset ipv4 address
				dhcp_state = DHCP_NO_LINK;
			}
			break;
		case DHCP_REQUEST_SENT:
			if (dhcp_timer == 0)
			{
				dhcp_send_request(0); // send request one more time
				dhcp_state = DHCP_REQUEST_RESENT;
				dhcp_timer = DHCP_REQ_TIMEOUT_TIMER;
			}
			break;
		case DHCP_REQUEST_RESENT:
			if (dhcp_timer == 0)
			{
				// no ACK, start again
				dhcp_get_new_xid();
				
				dhcp_state = DHCP_DISCOVER;
				dhcp_timer = 1;
			}
			break;
		case DHCP_READY:
			if (dhcp_timer == 0)  // now rebind old address
			{
				dhcp_get_new_xid();
				
				dhcp_send_request(1);
			
				dhcp_state = DHCP_REBIND_REQ_SENT;
				dhcp_timer = DHCP_REQ_TIMEOUT_TIMER;
			}
			break;
			
		case DHCP_REBIND_REQ_SENT:
			if (dhcp_timer == 0)
			{
				dhcp_send_request(1); // send request one more time
				dhcp_state = DHCP_REQUEST_RESENT;
				dhcp_timer = DHCP_REQ_TIMEOUT_TIMER;
			}
			break;
	}
	
}


int dhcp_is_ready(void)
{
	return dhcp_state == DHCP_READY;
}





#define RECEIVED_OFFER  1
#define RECEIVED_ACK	2


static int parse_dhcp_options(const uint8_t * data, int data_len, const bootp_header_t * m )
{
	int bytes_remaining = data_len;
	const uint8_t * p = data;
	int res = 0;
	
	
	while (bytes_remaining > 0)
	{
		int option_type = p[0];
		int option_len = p[1];
		
		if (option_type == 0) // special NOP option
		{
			p += 1;
			bytes_remaining -= 1;
			continue;
		}
		
		switch (option_type)
		{
			case 53: // message type
				if (p[2] == 2) // DHCPOFFER
				{
					memcpy(dhcp_request_packet + 5, m->yiaddr, 4); // offered address
					res = RECEIVED_OFFER;						
				}
				else if (p[2] == 5) // ACK
				{
					res = RECEIVED_ACK;
					memcpy(ipv4_addr, m->yiaddr, 4); // set offered address	
					ipv4_print_ip_addr(36, "MYIP", ipv4_addr);			
				}
				break;
			case 54: // server id
				if (option_len == 4)
				{
					memcpy(dhcp_request_packet + 11, p+2, 4); // server id
					ipv4_print_ip_addr(6, "SRV", p+2);
				}
				break;
			case 3: // router option
				if (res == RECEIVED_ACK) // assumption: message type comes first!
				{
					if (option_len >= 4)
					{
						// use only first entry
						memcpy(ipv4_gw, p+2, 4);
						ipv4_print_ip_addr(12, "GW", p+2);
					}						
				}
				break;
			case 1: // netmask
				if (res == RECEIVED_ACK) // assumption: message type comes first!
				{
					if (option_len == 4)
					{
						memcpy(ipv4_netmask, p+2, 4);
						ipv4_print_ip_addr(18, "MASK", p+2);
					}					
				}
				break;
			case 42: // NTP server
				if (res == RECEIVED_ACK) // assumption: message type comes first!
				{
					if (option_len >= 4)
					{
						// use only first entry
						memcpy(ipv4_ntp, p+2, 4);
						ipv4_print_ip_addr(42, "NTP", p+2);
					}
				}
				break;
			case 6: // DNS
				if (res == RECEIVED_ACK) // assumption: message type comes first!
				{
					if (option_len >= 4)
					{
						memcpy(ipv4_dns_pri, p+2, 4); // primary DNS
						
						if (option_len >= 8)
						{
							memcpy(ipv4_dns_sec, p+2+4, 4); // secondary DNS
						}
						else
						{
							memset(ipv4_dns_sec, 0, 4);
						}					
						
						ipv4_print_ip_addr(24, "DNS1", ipv4_dns_pri);	
						ipv4_print_ip_addr(30, "DNS2", ipv4_dns_sec);	
					}
				}
				break;
				
			/*	
			case 51: // lease time
				if (option_len == 4)
				{
					char buf[5];
					
					int 
					vdisp_i2s(buf, 3, 10, 0, p[2]);
					vd_prints_xy(VDISP_DEBUG_LAYER, 0, 8, VDISP_FONT_6x8, 0, buf);
					
					vdisp_i2s(buf, 3, 10, 0, p[3]);
					vd_prints_xy(VDISP_DEBUG_LAYER, 24, 8, VDISP_FONT_6x8, 0, buf);
					
					vdisp_i2s(buf, 3, 10, 0, p[4]);
					vd_prints_xy(VDISP_DEBUG_LAYER, 48, 8, VDISP_FONT_6x8, 0, buf);
					
					vdisp_i2s(buf, 3, 10, 0, p[5]);
					vd_prints_xy(VDISP_DEBUG_LAYER, 72, 8, VDISP_FONT_6x8, 0, buf);
				} 
				break;
				*/
			
			case 58: // timer T1
				if (option_len == 4)
				{
					dhcp_T1 = (p[2] << 24) | (p[3] << 16) | (p[4] << 8) | p[5];
					
					/* char buf[12];
										
					vdisp_i2s(buf, 11, 10, 0, dhcp_T1);
					vd_prints_xy(VDISP_DEBUG_LAYER, 0, 8, VDISP_FONT_6x8, 0, buf); */
					
				}
				break;
			
			/*
			case 59: // Timer T2
			if (option_len == 4)
			{
				char buf[5];
				
				vdisp_i2s(buf, 3, 10, 0, p[2]);
				vd_prints_xy(VDISP_DEBUG_LAYER, 0, 24, VDISP_FONT_6x8, 0, buf);
				
				vdisp_i2s(buf, 3, 10, 0, p[3]);
				vd_prints_xy(VDISP_DEBUG_LAYER, 24, 24, VDISP_FONT_6x8, 0, buf);
				
				vdisp_i2s(buf, 3, 10, 0, p[4]);
				vd_prints_xy(VDISP_DEBUG_LAYER, 48, 24, VDISP_FONT_6x8, 0, buf);
				
				vdisp_i2s(buf, 3, 10, 0, p[5]);
				vd_prints_xy(VDISP_DEBUG_LAYER, 72, 24, VDISP_FONT_6x8, 0, buf);
			}
			break;
			*/
			case 255:
				return res;
		}
		
		p += 2 + option_len;
		bytes_remaining -= 2 + option_len;
	}
	
	return res;
}

void dhcp_input_packet (const uint8_t * data, int data_len)
{
	
	
	
	if (data_len <= (sizeof (bootp_header_t)))
		return; // packet too short
	
	bootp_header_t * m = (bootp_header_t *) data;
	
	if (m->op != 2) // BOOTREPLY
		return;
	
	if (m->xid_hi != (dhcp_xid >> 16))
		return;
		
	if (m->xid_lo != (dhcp_xid & 0xFFFF))
		return;
	
	if (memcmp (m->magic_cookie, dhcp_magic_cookie, sizeof dhcp_magic_cookie) != 0)
		return;
		
 //  vdisp_prints_xy(0, 56, VDISP_FONT_6x8, 0, "DHCP!");
	
	if (dhcp_state != DHCP_READY) // only change settings when DHCP is not ready yet
	{
		switch (parse_dhcp_options(data + (sizeof (bootp_header_t)), data_len - (sizeof (bootp_header_t)), m))
		{
			case RECEIVED_OFFER:
				if (dhcp_state == DHCP_DISCOVER)
				{
					dhcp_send_request(0);  // send request to the server which sent the first reply
				}
				dhcp_state = DHCP_REQUEST_SENT;
				dhcp_timer = DHCP_REQ_TIMEOUT_TIMER;
				break;
			
			case RECEIVED_ACK:
				dhcp_state = DHCP_READY;
				
				if (dhcp_T1 < DHCP_TIMEOUT_TIMER_MIN)
				{
					dhcp_timer = 2 * DHCP_TIMEOUT_TIMER_MIN;
				}
				else if (dhcp_T1 > DHCP_TIMEOUT_TIMER_MAX)
				{
					dhcp_timer = 2 * DHCP_TIMEOUT_TIMER_MAX;
				}
				else
				{
					dhcp_timer = 2 * dhcp_T1;
				}
				break;
			
		}
	}	
	
}
