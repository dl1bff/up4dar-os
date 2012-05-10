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

unsigned char ipv4_addr[4] = { 192, 168, 1, 33 };

unsigned char ipv4_netmask[4] = { 255, 255, 255, 0 };

unsigned char ipv4_gw[4] = { 192, 168, 1, 1 };

unsigned char dcs_relay_host[4] = { 192, 168, 1, 55 };


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
	
	int sum = 0;
	int i;
	
	for (i=0; i < 10; i++) // 20 Byte Header
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (echo_reply_buf + 14)) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
	((unsigned short *) (echo_reply_buf + 14)) [5] = sum; // checksumme setzen
	
	memcpy(echo_reply_buf + 38, p + 4, len - 4);  // ping daten kopieren ohne type und chksum
	
	sum = 0;
	
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
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
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
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
		
	if (sum != ((unsigned short *) p) [1])  // checksumme falsch
		return;
	
	
	
	switch (p[0])
	{
		case 8:  // echo request
			icmpv4_send_echo_reply ( p, len, ipv4_header);
			break;
	}

}	


static const unsigned char snmp_reply_header [] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // dest
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // src
	0x08, 0x00, // IP
	0x45, 0x00, 0x00, 0x00,   // IPv4, 20 Bytes Header
	0x00, 0x00, 0x40, 0x00,  // don't fragment
	0x80, 0x11, 0x00, 0x00,  // UDP, TTL=128
	0x00, 0x00, 0x00, 0x00,  // source
	0x00, 0x00, 0x00, 0x00  // destination
};	


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
		
	uint8_t * snmp_reply_buf = packet->data;
	
	memcpy(snmp_reply_buf, snmp_reply_header, sizeof snmp_reply_header);
	
	eth_set_src_mac_and_type(snmp_reply_buf, 0x0800); // IP packet
	
	memcpy(snmp_reply_buf + 26, ipv4_addr, sizeof ipv4_addr); // src IP
	memcpy(snmp_reply_buf + 30, ipv4_header + 12, sizeof ipv4_addr); // dest IP
	
	
	int total_length = data_length + 8 + 20;
	
	((unsigned short *) (snmp_reply_buf + 14)) [1] = total_length;
	
	int sum = 0;
	int i;
	
	for (i=0; i < 10; i++) // 20 Byte Header
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (snmp_reply_buf + 14)) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
	((unsigned short *) (snmp_reply_buf + 14)) [5] = sum; // checksumme setzen
	
	// UDP
	
	int src_port = (p[0] << 8) | p[1];
	int dest_port = (p[2] << 8) | p[3];
	
	((unsigned short *) (snmp_reply_buf + 14)) [10] = dest_port & 0xFFFF;
	((unsigned short *) (snmp_reply_buf + 14)) [11] = src_port & 0xFFFF;
	((unsigned short *) (snmp_reply_buf + 14)) [12] = data_length + 8;
	((unsigned short *) (snmp_reply_buf + 14)) [13] = 0;  // chksum
	
	
	ip_addr_t  tmp_addr;
	memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
	memcpy(&tmp_addr.ipv4.addr, ipv4_header + 12 , sizeof ipv4_addr);  // antwort an diese adresse
	
	ipneigh_send_packet (&tmp_addr, packet);
	
	// (snmp_reply_buf, total_length + 14);
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
	   
	switch(dest_port)
	{
	case 5555:
	
		if (udp_length >= (8+100)) // accept packets to port 5555 with at least 100 data bytes
		{
			if (memcmp(p + 8, "0001", 4) == 0)  // first four bytes "0001"
			{
				dstarProcessDCSPacket( p + 8 );
			}
		}
		break;
		
	case 161: // SNMP
		snmp_send_reply ( p, udp_length - 8, ipv4_header );
		break;
	}
}	
	
	

void ipv4_input (const uint8_t * p, int len, const uint8_t * eth_header)
{
	
	if (memcmp(p+16, ipv4_addr, sizeof ipv4_addr) != 0)  // paket ist nicht fuer mich
		return;
	
	if ((p[0] & 0xF0) != 0x40)   // IP version not 4
		return;
		
	int header_len = (p[0] & 0x0F) << 2;
	
	if (header_len < 20)  // IP Header hat mindestens 20 bytes
		return;
	
	int total_len = (p[2] << 8) | p[3];
	
	if ((total_len < header_len) || (total_len > len))  // Laenge passt nicht
		return;
		
	int sum = 0;
	int i;
	
	for (i=0; i < (header_len >> 1); i++)
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) p) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
		
	if (sum != ((unsigned short *) p) [5])  // checksumme falsch
		return;
		
	if (((p[6] & 0x3F) != 0) || (p[7] != 0)) // fragment bit & fragment offset != 0
		return;
		
	ip_addr_t  tmp_addr;
	memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
	memcpy(&tmp_addr.ipv4.addr, p+12, sizeof ipv4_addr);
	ipneigh_rx( &tmp_addr, (mac_addr_t *) (eth_header + 6), 0);  // put source into neighbor list
			
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