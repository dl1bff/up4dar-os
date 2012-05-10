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
 * ipv4.h
 *
 * Created: 05.02.2012 14:23:29
 *  Author: mdirska
 */ 


#ifndef IPV4_H_
#define IPV4_H_


extern unsigned char ipv4_addr[4];

extern unsigned char ipv4_netmask[4];

extern unsigned char ipv4_gw[4];

extern unsigned char dcs_relay_host[4];

void ipv4_input (const uint8_t * p, int len, const uint8_t * eth_header);


#endif /* IPV4_H_ */