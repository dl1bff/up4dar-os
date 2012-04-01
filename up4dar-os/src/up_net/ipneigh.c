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




#include <asf.h>

#include "board.h"
#include "gpio.h"


#include "ipneigh.h"

#include "gcc_builtin.h"

struct ipneigh_list
{
	ip_addr_t		ip_addr;
	mac_addr_t		mac_addr;
	enum ipneigh_state	state;
};


#define NEIGH_LIST_LEN	10

static struct ipneigh_list neighbors[NEIGH_LIST_LEN];

static ip_addr_t zero_address;

void ipneigh_init(void)
{
	memset(neighbors, 0, sizeof neighbors);
	memset(&zero_address, 0, sizeof zero_address);	
}


void ipneigh_rx ( const ip_addr_t * a, const mac_addr_t * m, int solicited )
{
	int i;
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, a, sizeof (ip_addr_t)) == 0)
		{
			if (solicited != 0)
			{
				memcpy(&neighbors[i].mac_addr, m, sizeof (mac_addr_t));
				neighbors[i].state = REACHABLE;
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
			return;
		}
	}
	// Fehlerbehandlung: kein freier Platz in der Neighbor-Tabelle gefunden
}

int ipneigh_get ( const ip_addr_t * a, mac_addr_t * m )
{
	int i;
	
	for (i=0; i < NEIGH_LIST_LEN; i++)
	{
		if (memcmp(&neighbors[i].ip_addr, a, sizeof (ip_addr_t)) == 0)
		{
			memcpy(m, &neighbors[i].mac_addr, sizeof (mac_addr_t));
			return 0;
		}
	}
	
	return -1;  // not found
}
