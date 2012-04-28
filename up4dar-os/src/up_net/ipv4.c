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



#include <asf.h>

#include "board.h"
#include "gpio.h"
#include "gcc_builtin.h"

#include "up_io/eth.h"

#include "ipneigh.h"
#include "ipv4.h"

#include "up_dstar/ambe.h"


unsigned char ipv4_addr[4] = { 192, 168, 1, 33 };
	
static unsigned char echo_reply_buf[1520] = {
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
	
	
static void icmpv4_send_echo_reply (unsigned char * p, int len, unsigned char * ipv4_header)
{
	ip_addr_t  tmp_addr;
	memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
	memcpy(&tmp_addr.ipv4.addr, ipv4_header + 12 , sizeof ipv4_addr);  // antwort an diese adresse
	
	if (ipneigh_get(&tmp_addr, (mac_addr_t *) echo_reply_buf) != 0)
		return;   // ethernet-adresse war nicht in der neighbor list
		
	memcpy(echo_reply_buf + 6, mac_addr, sizeof (mac_addr_t));  // absender MAC
	
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
	
	eth_send_raw (echo_reply_buf, len + 20 + 14);
		
	eth_counter ++;
}	
	
	

static void icmpv4_input (unsigned char * p, int len, unsigned char * ipv4_header)
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
	
	
	
static void udp_input (unsigned char * p, int len)
{
	// int src_port = (p[0] << 8) | p[1];
	int dest_port = (p[2] << 8) | p[3];
	int udp_length = (p[4] << 8) | p[5];
	
	if (udp_length > len)  // length invalid
	   return;
	
	if (udp_length < 8)  // UDP header has at least 8 bytes
	   return;
	   
	if (dest_port == 5555)
	{
		if (udp_length >= (8+9)) // accept packets to port 5555 with at least 9 data bytes
		{
			ambe_input_data( p + 8 );
		}
	}
}	
	
	

void ipv4_input (unsigned char * p, int len)
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
		
	switch (p[9])  // protocol
	{
		case 1:
			icmpv4_input(p + header_len, total_len - header_len, p);
			break;
		case 17: // UDP
			udp_input(p + header_len, total_len - header_len);
			break;
	}
	
}