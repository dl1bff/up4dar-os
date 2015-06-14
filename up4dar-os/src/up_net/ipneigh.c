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
 * ipneigh.c
 *
 * Created: 05.02.2012 14:29:55
 *  Author: mdirska
 */ 



#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <asf.h>

#include "board.h"
#include "gpio.h"

#include "up_dstar/vdisp.h"


#include "up_io/eth.h"
#include "up_io/eth_txmem.h"

#include "ipneigh.h"
#include "up_net/arp.h"

#include "gcc_builtin.h"

struct ipneigh_list
{
	ip_addr_t		ip_addr;
	mac_addr_t		mac_addr;
	ipneigh_state_t	state;
	eth_txmem_t *  pending_packet;
	int		retry_counter;
	int		timer;
};


#define NEIGH_LIST_LEN	6

static struct ipneigh_list neighbors[NEIGH_LIST_LEN];

static ip_addr_t zero_address;

void ipneigh_init(void)
{
	memset(neighbors, 0, sizeof neighbors);
	memset(&zero_address, 0, sizeof zero_address);	
}

#define REACHABLE_TIMER 40

#define PROBE_TIMER 3
#define PROBE_RETRY 4

#define INCOMPLETE_TIMER 3
#define INCOMPLETE_RETRY 4


static void ipneigh_send_nd ( const ip_addr_t * a, int unicast, const mac_addr_t * m)
{
	// vdisp_prints_xy( 0, 56, VDISP_FONT_6x8, 0, "ND  " );
	
	if (memcmp(a, &zero_address, sizeof (zero_address.ipv4.zero) ) == 0) // is IPv4 address
	{
		arp_send_request (a, unicast, m);
	}
}


void ipneigh_service(void)
{
	int i;
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)) != 0) // entry not empty
		{
			switch (neighbors[i].state)
			{
				case REACHABLE:
					neighbors[i].timer --;
						
					if (neighbors[i].timer <= 0)
					{
						neighbors[i].state = PROBE;
						neighbors[i].timer = PROBE_TIMER;
						neighbors[i].retry_counter = PROBE_RETRY;
						neighbors[i].pending_packet = NULL;
					}
				
					break;
					
				case INCOMPLETE:
					neighbors[i].timer --;
						
					if (neighbors[i].timer <= 0)
					{
						neighbors[i].retry_counter --;
						
						if (neighbors[i].retry_counter <= 0)
						{
							memcpy(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)); // delete this entry
							
							if (neighbors[i].pending_packet != NULL)
							{
								eth_txmem_free(neighbors[i].pending_packet);
								neighbors[i].pending_packet = NULL;
							}								
						}
						else
						{
							ipneigh_send_nd (&neighbors[i].ip_addr, 0, 0);
							neighbors[i].timer = INCOMPLETE_TIMER;
						}						
					}
					break;
					
				case PROBE:
					neighbors[i].timer --;
						
					if (neighbors[i].timer <= 0)
					{
						neighbors[i].retry_counter --;
						
						if (neighbors[i].retry_counter <= 0)
						{
							neighbors[i].state = INCOMPLETE;
							neighbors[i].timer = INCOMPLETE_TIMER;
							neighbors[i].retry_counter = INCOMPLETE_RETRY;
							neighbors[i].pending_packet = NULL;				
						}
						else
						{
							ipneigh_send_nd (&neighbors[i].ip_addr, 1,
							    &neighbors[i].mac_addr );
							neighbors[i].timer = PROBE_TIMER;
						}						
					}
					break;
				
				case STALE:
				case DELAY:
						// do nothing
					break;
			} // switch
		} // if: entry not empty
	} // for	
}


void ipneigh_rx ( const ip_addr_t * a, const mac_addr_t * m, int solicited )
{
	int i;
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, a, sizeof (ip_addr_t)) == 0)
		{
			if ((solicited != 0) && 
			  ((neighbors[i].state == INCOMPLETE) || (neighbors[i].state == PROBE)))
			{
				memcpy(&neighbors[i].mac_addr, m, sizeof (mac_addr_t));
				neighbors[i].state = REACHABLE;
				neighbors[i].timer = REACHABLE_TIMER;
				neighbors[i].retry_counter = 0;
				
				if (neighbors[i].pending_packet != NULL)
				{
					memcpy(neighbors[i].pending_packet->data, m, sizeof (mac_addr_t));
						// fill in dest MAC addr
					eth_txmem_send(neighbors[i].pending_packet);
					neighbors[i].pending_packet = NULL;
				}
			}
			return;
		}
	}
	
	// nicht gefunden, neuen Eintrag machen
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)) == 0)
		{
			memcpy(&neighbors[i].ip_addr, a, sizeof (ip_addr_t));
			memcpy(&neighbors[i].mac_addr, m, sizeof (mac_addr_t));
			
			neighbors[i].state = STALE;
			neighbors[i].timer = 0;
			neighbors[i].retry_counter = 0;
			neighbors[i].pending_packet = NULL;
			return;
		}
	}
	// Fehlerbehandlung: kein freier Platz in der Neighbor-Tabelle gefunden -> ersten STALE
	// EIntrag nehmen
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if ( (memcmp(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)) != 0) &&
		     (neighbors[i].state == STALE) )
		{
			memcpy(&neighbors[i].ip_addr, a, sizeof (ip_addr_t));
			memcpy(&neighbors[i].mac_addr, m, sizeof (mac_addr_t));
			
			neighbors[i].state = STALE;
			neighbors[i].timer = 0;
			neighbors[i].retry_counter = 0;
			neighbors[i].pending_packet = NULL;
			return;
		}
	}
	
	// Fehler: no space in neighbor list
}


#define NEIGH_FOUND			1
#define NEIGH_INCOMPLETE	2
#define NEIGH_ERROR			3

static int ipneigh_get ( const ip_addr_t * a, mac_addr_t * m, eth_txmem_t * packet)
{
	int i;
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, a, sizeof (ip_addr_t)) == 0)
		{
			if (neighbors[i].state == INCOMPLETE)
			{
				// ND going on
				// vdisp_prints_xy( 30, 56, VDISP_FONT_6x8, 0, "INCOM" );
				
				eth_txmem_free(packet);  // packet could not be sent, free mem
				return NEIGH_INCOMPLETE;
			}			
			
			memcpy(m, &neighbors[i].mac_addr, sizeof (mac_addr_t));  // return MAC addr	
			
			if (neighbors[i].state == STALE)
			{
				neighbors[i].state = PROBE;
				neighbors[i].timer = PROBE_TIMER;
				neighbors[i].retry_counter = PROBE_RETRY;
				neighbors[i].pending_packet = NULL;	
			}
			
			return NEIGH_FOUND;
		}
	}
	
	// nicht gefunden, neuen Eintrag machen
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)) == 0)
		{
			memcpy(&neighbors[i].ip_addr, a, sizeof (ip_addr_t));
			memset(&neighbors[i].mac_addr, 0, sizeof (mac_addr_t));
			
			neighbors[i].state = INCOMPLETE;
			neighbors[i].timer = INCOMPLETE_TIMER;
			neighbors[i].retry_counter = INCOMPLETE_RETRY;
			neighbors[i].pending_packet = packet;
			ipneigh_send_nd (&neighbors[i].ip_addr, 0, 0);
			return NEIGH_INCOMPLETE;
		}
	}
	
	// kein freier Platz in der Neighbor-Tabelle gefunden -> ersten STALE
	// EIntrag nehmen
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if ( (memcmp(&neighbors[i].ip_addr, &zero_address, sizeof (ip_addr_t)) != 0) &&
		     (neighbors[i].state == STALE) )
		{
			memcpy(&neighbors[i].ip_addr, a, sizeof (ip_addr_t));
			memset(&neighbors[i].mac_addr, 0, sizeof (mac_addr_t));
			
			neighbors[i].state = INCOMPLETE;
			neighbors[i].timer = INCOMPLETE_TIMER;
			neighbors[i].retry_counter = INCOMPLETE_RETRY;
			neighbors[i].pending_packet = packet;
			ipneigh_send_nd (&neighbors[i].ip_addr, 0, 0);
			return NEIGH_INCOMPLETE;
		}
	}
	
	return NEIGH_ERROR;  // no space in neighbor table
}



void ipneigh_send_packet ( const ip_addr_t * a, eth_txmem_t * packet )
{
	int res = ipneigh_get( a, (mac_addr_t *) packet->data, packet );  // try to set dest MAC addr
	
	if (res == NEIGH_FOUND)
	{
		eth_txmem_send(packet); // MAC addr is set, send packet
	}
	else if (res == NEIGH_ERROR)
	{
		eth_txmem_free(packet);  // packet could not be sent, free mem
	}
	/* else if (res == NEIGH_INCOMPLETE)
	{
		vdisp_prints_xy( 0, 56, VDISP_FONT_6x8, 0, "INCOM" );
	} */
	
	// otherwise: wait for ND answer
}

