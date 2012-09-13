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
 * lldp.c
 *
 * Created: 10.05.2012 12:50:11
 *  Author: mdirska
 */ 


/* 
	LLDP frames
	
	IEEE 802.1AB-2009
	
*/

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "gcc_builtin.h"

#include "up_io/eth_txmem.h"
#include "up_io/eth.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/snmp.h"

#include "up_dstar/settings.h"


#include "lldp.h"


const uint8_t lldp_frame[] =
{
	0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E,  // dest  "nearest bridge" 01-80-C2-00-00-0E 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // src
	0x00, 0x00, // type
	
	0x02, 0x07, // chassis ID, ethernet MAC
	  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	
	0x04, 0x07, // port ID, ethernet MAC
	  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	
	0x06, 0x02, 0x00, 0x0A,  // TTL 10 seconds
	
	0x0C,    6 , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // system description
	
	0x10,   12 , 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,  // management address
	  0x01, 0x00, 0x00, 0x00, 0x00,		// interface number
	  0x00,  // OID  
	
	0x00, 0x00  // END
};


#define UP4DAR_UDP_IDENT_PORT  45233


void lldp_send (void)
{
	eth_txmem_t * packet = eth_txmem_get( sizeof lldp_frame );
	
	if (packet == NULL) // nomem
		return;
		
	memcpy(packet->data, lldp_frame, sizeof lldp_frame);
	
	memcpy(packet->data + 17, mac_addr, 6); // 6 byte mac address
	memcpy(packet->data + 26, mac_addr, 6); // 6 byte mac address
	
	memcpy(packet->data + 38, "UP4DAR", 6);
	
	memcpy(packet->data + 48, ipv4_addr, 4);
	
	eth_set_src_mac_and_type(packet->data, 0x88cc);
	
	eth_txmem_send(packet);
	
	// UDP ident
	
	uint8_t dest_ipv4_addr[4];
	int i;
	
	for (i=0; i < 4; i++)
	{
		dest_ipv4_addr[i] = ipv4_addr[i] | (ipv4_netmask[i] ^ 0xFF);  // ipv4 subnet broadcast
	}
	
	packet = udp4_get_packet_mem(8, UP4DAR_UDP_IDENT_PORT, UP4DAR_UDP_IDENT_PORT, dest_ipv4_addr);
	// packet = eth_txmem_get( UDP_PACKET_SIZE(8) );
	
	if (packet == NULL) // nomem
		return;
	
	memcpy(packet->data + UDP_PACKET_SIZE(0), settings.s.my_callsign, 8);
	
	udp4_calc_chksum_and_send(packet, NULL); // send broadcast
	
}