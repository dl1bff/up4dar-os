/*

Copyright (C) 2011,2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * ipv4.c
 *
 * Created: 05.02.2012 14:23:02
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <asf.h>

#include "board.h"
#include "gpio.h"
#include "gcc_builtin.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"

#include "ipneigh.h"
#include "ipv4.h"

#include "up_dstar/dstar.h"

#include "snmp.h"
#include "up_dstar/dcs.h"
#include "up_net/dhcp.h"
#include "dns.h"

#include "up_dstar/vdisp.h"
#include "up_crypto/up_crypto.h"
#include "ntp.h"


unsigned char ipv4_addr[4];

unsigned char ipv4_netmask[4];

unsigned char ipv4_gw[4];

unsigned char ipv4_ntp[4];

unsigned char ipv4_dns_pri[4];
unsigned char ipv4_dns_sec[4];

const uint8_t ipv4_zero_addr[4] = { 0, 0, 0, 0 };

static const uint8_t echo_reply_header[38] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // dest
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // src
	0x08, 0x00, // IP
	0x45, 0x00, 0x00, 0x00,   // IPv4, 20 Bytes Header
	0x00, 0x00, 0x40, 0x00,  // don't fragment
	0x80, 0x01, 0x00, 0x00,  // ICMP, TTL=128
	0x00, 0x00, 0x00, 0x00,  // source
	0x00, 0x00, 0x00, 0x00,  // destination
	0x00, 0x00, 0x00, 0x00   // Type 0 (echo reply) Code 0
};	


static int ipv4_header_checksum( const uint8_t * p, int header_len )
{
	int sum = 0;
	int i;
	
	for (i=0; i < (header_len >> 1); i++)
	{
		if (i != 5)  // skip checksum field
		{
			sum += ((unsigned short *) p) [i];
		}
	}
	
	while ((sum >> 16) != 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	return ( ~sum ) & 0xFFFF;
}

	
static void icmpv4_send_echo_reply (const uint8_t * p, int len, const uint8_t * ipv4_header)
{
	eth_txmem_t * packet = eth_txmem_get(len + 20 + 14); // get buffer for reply
	
	if (packet == NULL) // nomem
		return;
	
	uint8_t * echo_reply_buf = packet->data;
	
	memcpy (echo_reply_buf, echo_reply_header, sizeof echo_reply_header);
	
	eth_set_src_mac_and_type(echo_reply_buf, 0x0800); // type = IP
	
	memcpy(echo_reply_buf + 26, ipv4_addr, sizeof ipv4_addr); // src IP
	memcpy(echo_reply_buf + 30, ipv4_header + 12, sizeof ipv4_addr); // dest IP
	
	int total_length = len + 20;
	
	((unsigned short *) (echo_reply_buf + 14)) [1] = total_length;
	
	
	((unsigned short *) (echo_reply_buf + 14)) [5] = ipv4_header_checksum(echo_reply_buf + 14, 20);
	
	memcpy(echo_reply_buf + 38, p + 4, len - 4);  // ping daten kopieren ohne type und chksum
	
	int i;
	int sum = 0;
	
	for (i=0; i < (len >> 1); i++)
	{
		if (i != 1)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (echo_reply_buf + 34)) [i];
		}
	}
	
	if ((len & 0x01) != 0)  // ungerade Anzahl bytes
	{
		sum += (echo_reply_buf + 34)[len -1] << 8;  // letztes byte mit 0 als padding
	}
	
	while ((sum >> 16) != 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	sum = ( ~sum ) & 0xFFFF;
	
	((unsigned short *) (echo_reply_buf + 34)) [1] = sum;
	
	
	ip_addr_t  tmp_addr;
	memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
	memcpy(&tmp_addr.ipv4.addr, ipv4_header + 12 , sizeof ipv4_addr);  // antwort an diese adresse
	
	ipneigh_send_packet(&tmp_addr, packet);
		
	eth_counter ++;
}	
	
	

static void icmpv4_input (const uint8_t * p, int len, const uint8_t * ipv4_header)
{
	int sum = 0;
	int i;
	
	for (i=0; i < (len >> 1); i++)
	{
		if (i != 1)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) p) [i];
		}
	}
	
	if ((len & 0x01) != 0)  // ungerade Anzahl bytes
	{
		sum += p[len -1] << 8;  // letztes byte mit 0 als padding
	}
	
	while ((sum >> 16) != 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	sum = ( ~sum ) & 0xFFFF;
		
	if (sum != ((unsigned short *) p) [1])  // checksumme falsch
		return;
	
	
	
	switch (p[0])
	{
		case 8:  // echo request
			icmpv4_send_echo_reply ( p, len, ipv4_header);
			break;
	}

}	


void ipv4_udp_prepare_packet( eth_txmem_t * packet, const uint8_t * dest_ipv4_addr, int udp_data_length,
	int udp_src_port, int udp_dest_port )
{
	uint8_t * p = packet->data;
	
	memset(p + 14, 0, 20 ); // fill IP header with zeros
	
	eth_set_src_mac_and_type(p, 0x0800); // IP packet
	
	unsigned short r = crypto_get_random_15bit();
	
	p[14] = 0x45; // IPv4, 20 Bytes Header
	p[18] = r & 0xFF;
	p[19] = r >> 7;
	p[20] = 0x40; // don't fragment
	p[22] = 128;  // TTL=128
	p[23] = 17;  // next header -> UDP
	
	memcpy(p + 26, ipv4_addr, sizeof ipv4_addr); // src IP
	memcpy(p + 30, dest_ipv4_addr, sizeof ipv4_addr); // dest IP
	
	int total_length = udp_data_length + 8 + 20;
	
	((unsigned short *) (p + 14)) [1] = total_length;
	
	
	((unsigned short *) (p + 14)) [5] = ipv4_header_checksum(p+14, 20);
	
	((unsigned short *) (p + 14)) [10] = udp_src_port & 0xFFFF; 
	((unsigned short *) (p + 14)) [11] = udp_dest_port & 0xFFFF;
	((unsigned short *) (p + 14)) [12] = udp_data_length + 8;
    //	((unsigned short *) (p + 14)) [13] = 0;  // chksum
	
}


static void snmp_send_reply (const uint8_t * p, int len, const uint8_t * ipv4_header)
{
	int data_length = 0;
	
	eth_txmem_t * packet = snmp_process_request( p + 8, len, & data_length );
	
	if (packet == NULL)  // something went wrong
		return; 
	
	if (data_length <= 0)  // error occurred
	{
		eth_txmem_free(packet);
		return; 
	}		
		
	
	int src_port = (p[2] << 8) | p[3];
	int dest_port = (p[0] << 8) | p[1];
	
	
	ipv4_udp_prepare_packet(packet, ipv4_header + 12, data_length, src_port, dest_port);
	
	udp4_calc_chksum_and_send(packet,  ipv4_header + 12);
	
}


static int udp4_header_checksum( const uint8_t * p)
{
	int sum = 0;
	int i;

	for (i=6; i < 10; i++) // dest+src IP addr
	{
		sum += ((unsigned short *) (p)) [i];
	}

	sum += 17;

	int udp_length = ((unsigned short *) (p)) [12];
	sum += udp_length;



	for (i=0; i < (udp_length >> 1); i++)
	{
		if (i != 3) // skip checksum field
		{
			sum += ((unsigned short *) (p + 20))[i];
		}
	}

	if ((udp_length & 1) == 1) // odd number of bytes
	{
		sum += ((unsigned char * )( p + 20)) [udp_length-1] << 8;
	}

	while ((sum >> 16) != 0)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	sum = ( ~sum ) & 0xFFFF;

	if (sum == 0)
	{
		sum = 0xFFFF;
	}
	
	return sum;
}

unsigned short udp_socket_ports[NUM_UDP_SOCKETS] = { 68, 161, 0, 0, 0 };
	

int udp_get_new_srcport(void)
{
	int p;
	
	while (1)
	{
		p = crypto_get_random_16bit();
		
		if (p < 1024)
		{
			p += 11000;
		}
	
		int already_in_use = 0;
		int i;
		for (i=0; i < NUM_UDP_SOCKETS; i++)
		{
			if (p == udp_socket_ports[i])
			{
				already_in_use = 1;
			}
		}
		
		if (already_in_use == 0)
			break;
	}
	
	return p;
}	
	
	
static void udp_input (const uint8_t * p, int len, const uint8_t * ipv4_header)
{
	// int src_port = (p[0] << 8) | p[1];
	int dest_port = (p[2] << 8) | p[3];
	int udp_length = (p[4] << 8) | p[5];
	
	if (udp_length > len)  // length invalid
	   return;
	
	if (udp_length < 8)  // UDP header has at least 8 bytes
	   return;
	   
	int checksum = (p[6] << 8) | p[7];
	
	if (checksum != 0)
	{
		if (checksum != udp4_header_checksum(ipv4_header))
			return;
	}	
	
	if (dest_port == 0)  // 0 is a special value (socket not connected)
		return ;	
	
	int i;
	
	for (i=0; i < NUM_UDP_SOCKETS; i++)
	{
		if (dest_port == udp_socket_ports[i])
		{
			switch (i)
			{
			case UDP_SOCKET_SNMP:
				snmp_send_reply ( p, udp_length - 8, ipv4_header );
				break;
				
			case UDP_SOCKET_DHCP:
				if (dhcp_is_ready() == 0)  // if DHCP is not completed yet
				{
					dhcp_input_packet( p + 8, udp_length - 8 );
				}
				break;
				
			case UDP_SOCKET_DCS:
				dcs_input_packet( p + 8, udp_length - 8, ipv4_header + 12 /* src addr */);
				break;
				
			case UDP_SOCKET_DNS:
				dns_input_packet( p + 8, udp_length - 8, ipv4_header + 12 /* src addr */);
				break;
			
			case UDP_SOCKET_NTP:
				ntp_handle_packet( p + 8, udp_length - 8, ipv4_header + 12 /* src addr */);
				break;
			}
			
			return;
		}		 
	}		
	   
	
}	
	
	


void ipv4_input (const uint8_t * p, int len, const uint8_t * eth_header)
{
	if (dhcp_is_ready() != 0)  // dhcp completed
	{
		if (memcmp(p+16, ipv4_addr, sizeof ipv4_addr) != 0)  // then: only allow packets
						// for my ip address
			return;
	}
	
	
	if ((p[0] & 0xF0) != 0x40)   // IP version not 4
		return;
		
	int header_len = (p[0] & 0x0F) << 2;
	
	if (header_len < 20)  // IP Header hat mindestens 20 bytes
		return;
	
	int total_len = (p[2] << 8) | p[3];
	
	if ((total_len < header_len) || (total_len > len))  // Laenge passt nicht
		return;
		
	
		
	if (ipv4_header_checksum(p, header_len) != ((unsigned short *) p) [5])  // checksumme falsch
		return;
		
	if (((p[6] & 0x3F) != 0) || (p[7] != 0)) // fragment bit & fragment offset != 0
		return;
		
		
	if (ipv4_addr_is_local(p + 12))
	{
		ip_addr_t  tmp_addr;
		memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
		memcpy(&tmp_addr.ipv4.addr, p+12, sizeof ipv4_addr);
		ipneigh_rx( &tmp_addr, (mac_addr_t *) (eth_header + 6), 0);  // put source into neighbor list
	}
			
	switch (p[9])  // protocol
	{
		case 1:
			icmpv4_input(p + header_len, total_len - header_len, p);
			break;
		case 17: // UDP
			udp_input(p + header_len, total_len - header_len, p);
			break;
	}
	
}


int ipv4_addr_is_local ( const uint8_t * ipv4_a )
{
	int i;
	
	for (i=0; i < 4; i++)
	{
		uint8_t net_addr = ipv4_addr[i]  &  ipv4_netmask[i];
		
		if ((ipv4_a[i] & ipv4_netmask[i]) != net_addr)
		{
			return 0;
		}
	}
	
	return 1;
}


int ipv4_get_neigh_addr( ip_addr_t * addr, const uint8_t * ipv4_dest )
{
	memset(addr->ipv4.zero, 0, sizeof addr->ipv4.zero);
	
	if (ipv4_addr_is_local( ipv4_dest ) != 0)
	{
		// destination is on local subnet -> next hop is destination
		memcpy(addr->ipv4.addr, ipv4_dest, sizeof ipv4_gw);
	}
	else
	{
		// destination is not on local subnet -> next hop is gateway
		if (memcmp(ipv4_gw, ipv4_zero_addr, sizeof ipv4_zero_addr) == 0) // no gateway addr
		{
			return -1; // error: no gateway, no neighbour
		}
		
		memcpy(addr->ipv4.addr, ipv4_gw, sizeof ipv4_gw);
	}
	
	return 0;
}


void ipv4_print_ip_addr(int y, const char * desc, const uint8_t * ip)
{
	char buf[4];
	
	vd_prints_xy(VDISP_DEBUG_LAYER, 0, y, VDISP_FONT_4x6, 0, desc);
	vd_prints_xy(VDISP_DEBUG_LAYER, 20, y, VDISP_FONT_4x6, 0, "___.___.___.___");
	
	int i;
	
	for (i=0; i < 4; i++)
	{
		vdisp_i2s(buf, 3, 10, 1, ip[i]);
		vd_prints_xy(VDISP_DEBUG_LAYER, 20 + (i*4*4), y, VDISP_FONT_4x6, 0, buf);
	}
	
}



void ipv4_init(void)
{
	ipv4_addr[0] = 169;
	ipv4_addr[1] = 254;
	ipv4_addr[2] = mac_addr[4];
	ipv4_addr[3] = mac_addr[5];
	
	ipv4_netmask[0] = 255;
	ipv4_netmask[1] = 255;
	ipv4_netmask[2] = 0;
	ipv4_netmask[3] = 0;
	
	memcpy(ipv4_gw, ipv4_zero_addr, sizeof ipv4_zero_addr); // no gateway
	memcpy(ipv4_dns_pri, ipv4_zero_addr, sizeof ipv4_zero_addr); // no primary dns
	memcpy(ipv4_dns_sec, ipv4_zero_addr, sizeof ipv4_zero_addr); // no secondary dns
	memcpy(ipv4_ntp, ipv4_zero_addr, sizeof ipv4_zero_addr); // no NTP server
	
	ipneigh_init(); // delete neighbor cache
}



eth_txmem_t * udp4_get_packet_mem (int udp_size, int src_port, int dest_port, const uint8_t * ipv4_dest_addr)
{
	eth_txmem_t * packet = eth_txmem_get( UDP_PACKET_SIZE(udp_size) );
	
	if (packet == NULL)
	{
		vdisp_prints_xy( 40, 56, VDISP_FONT_6x8, 0, "NOMEM" );
		return NULL;
	}
	
	ipv4_udp_prepare_packet( packet, ipv4_dest_addr, udp_size, src_port, dest_port);
	
	return packet;
}


void udp4_calc_chksum_and_send (eth_txmem_t * packet, const uint8_t * ipv4_dest_addr)
{
	
	uint8_t * p = packet->data;

	
	((unsigned short *) (p + 14)) [13] = udp4_header_checksum(p + 14);
	
	
	if (ipv4_dest_addr == NULL)
	{
		memset(packet->data, 0xFF, 6); // broadcast address
		eth_txmem_send(packet);
	}
	else
	{
		ip_addr_t  tmp_addr;
			
		if (ipv4_get_neigh_addr(&tmp_addr, ipv4_dest_addr ) != 0)  // get addr of neighbor
		{
			// neighbor could not be set - no gateway!
			eth_txmem_free(packet); // throw away packet
		}
		else
		{
			ipneigh_send_packet (&tmp_addr, packet);
		}
	}
	
}
