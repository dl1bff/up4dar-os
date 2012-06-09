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
 * eth.h
 *
 * Created: 28.05.2011 18:12:55
 *  Author: mdirska
 */ 


#ifndef ETH_H_
#define ETH_H_


// void eth_init(void);
// void eth_init(unsigned char ** p);
void eth_init(void);
// void eth_send_frame (void);
// void eth_send_vdisp_frame (void);


void eth_send_raw ( unsigned char * b, int len );

void eth_rx (void);

void eth_set_src_mac_and_type(uint8_t * data, uint16_t ethType);

extern U32 eth_counter;
extern U32 eth_counter2;
extern U32 eth_counter3;

extern unsigned char mac_addr[6];

#endif /* ETH_H_ */
