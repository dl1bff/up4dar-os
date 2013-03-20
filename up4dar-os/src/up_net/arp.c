/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * arp.c
 *
 * Created: 10.05.2012 16:32:04
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <asf.h>

#include "board.h"
#include "gpio.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"


#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"

#include "up_net/arp.h"

#include "gcc_builtin.h"




static const unsigned char arp_header[6] = {0x00, 0x01, 0x08, 0x00, 6, 4};
	
	
#define ARP_PACKET_SIZE  42


void arp_send_request (const ip_addr_t * a, int unicast, const mac_addr_t * m)
{
	eth_txmem_t * t = eth_txmem_get(ARP_PACKET_SIZE); // get buffer
	
	if (t == NULL)
		return;
		
	uint8_t * arp_frame = t->data;
			
	int i;
	
	for (i=0; i < ARP_PACKET_SIZE; i++ )
	{
		arp_frame[i] = 0;
	}
		
	if (unicast != 0)
	{
		memcpy(arp_frame + 0, m, sizeof mac_addr); // dest MAC
	}	
	else
	{
		memset(arp_frame + 0, 0xFF, sizeof mac_addr); // dest MAC = broadcast address
	}
	
		
	eth_set_src_mac_and_type(arp_frame, 0x0806);
			
	memcpy(arp_frame + 14, arp_header, sizeof arp_header);
	arp_frame[20] = 0x00;
	arp_frame[21] = 0x01; // request
	memcpy(arp_frame + 22, mac_addr, sizeof mac_addr); // sender MAC
	memcpy(arp_frame + 28, ipv4_addr, sizeof ipv4_addr); // sender IP
	
	if (unicast != 0)
	{
		memcpy(arp_frame + 32, m, sizeof mac_addr); // dest MAC
	}
	else
	{
		memset(arp_frame + 32, 0, sizeof mac_addr); // target MAC
	}		
	
	memcpy(arp_frame + 38, &a->ipv4.addr, sizeof ipv4_addr); // target IP

	eth_txmem_send( t );
}


void arp_process_packet(uint8_t * raw_packet)
{
	
	if (memcmp(raw_packet+14, arp_header, sizeof arp_header) != 0) // header not correct
		return; 
	
	uint8_t * p = raw_packet + 20;
	
	switch (((unsigned short *)p)[0])
	{
	case 1: // request
	
		if (memcmp(p+18, ipv4_addr, sizeof ipv4_addr) == 0) // my IP
		{
			int i;
			
			eth_txmem_t * t = eth_txmem_get(ARP_PACKET_SIZE); // get buffer
			
			if (t == NULL)
				break;
			
			uint8_t * arp_frame = t->data;
			
			for (i=0; i < ARP_PACKET_SIZE; i++ )
			{
				arp_frame[i] = 0;
			}
			
			memcpy(arp_frame + 0, p+2, sizeof mac_addr); // dest MAC
			
			eth_set_src_mac_and_type(arp_frame, 0x0806);
			
			memcpy(arp_frame + 14, arp_header, sizeof arp_header);
			arp_frame[20] = 0x00;
			arp_frame[21] = 0x02; // reply
			memcpy(arp_frame + 22, mac_addr, sizeof mac_addr); // sender MAC
			memcpy(arp_frame + 28, ipv4_addr, sizeof ipv4_addr); // sender IP
			memcpy(arp_frame + 32, p+2, sizeof mac_addr); // target MAC
			memcpy(arp_frame + 38, p+8, sizeof ipv4_addr); // target IP
			
			if (ipv4_addr_is_local(p + 8))
			{
				ip_addr_t  tmp_addr;
				memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
				memcpy(&tmp_addr.ipv4.addr, p+8, sizeof ipv4_addr);
				ipneigh_rx( &tmp_addr, (mac_addr_t *) (p+2), 0);  // put into neighbor list
			}				
			
			eth_txmem_send( t );
		}
		break;
		
	case 2: // reply
		if (memcmp(p+18, ipv4_addr, sizeof ipv4_addr) == 0)  // is it for me?
		{
			if (ipv4_addr_is_local(p + 8))
			{
				ip_addr_t  tmp_addr;
				memset(&tmp_addr.ipv4.zero, 0, sizeof tmp_addr.ipv4.zero);
				memcpy(&tmp_addr.ipv4.addr, p+8, sizeof ipv4_addr);
				ipneigh_rx( &tmp_addr, (mac_addr_t *) (p+2), 1);  // solicited response
			}				
		}
		break;
	}		
		
}
