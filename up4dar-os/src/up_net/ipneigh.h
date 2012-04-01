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
 * ipneigh.h
 *
 * Created: 05.02.2012 14:30:11
 *  Author: mdirska
 */ 


#ifndef IPNEIGH_H_
#define IPNEIGH_H_

struct ipneigh_ipv4
{
	unsigned char zero[12];
	unsigned char addr[4];
};

struct ipneigh_ipv6
{
	unsigned char addr[16];
};


union ip_addr_u {
	struct ipneigh_ipv4 ipv4;
	struct ipneigh_ipv6 ipv6;
};

typedef union ip_addr_u ip_addr_t;

struct ipneigh_mac_addr
{
	unsigned char addr[6];
};

typedef struct ipneigh_mac_addr mac_addr_t;

enum ipneigh_state
{
	INCOMPLETE,
	REACHABLE,
	STALE,
	DELAY,
	PROBE
};

void ipneigh_init (void);

void ipneigh_rx ( const ip_addr_t * a, const mac_addr_t * m, int solicited );

int ipneigh_get ( const ip_addr_t * a, mac_addr_t * m );

#endif /* IPNEIGH_H_ */