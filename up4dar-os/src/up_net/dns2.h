/*

Copyright (C) 2014   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * dns2.h
 *
 * Created: 19.01.2014 17:22:07
 *  Author: mdirska
 */ 


#ifndef DNS2_H_
#define DNS2_H_

void dns2_init(void);

void dns2_input_packet ( int handle, const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr);

int dns2_find_dns_port( uint16_t port );

int dns2_req_A (const char * name);
int dns2_result_available( int handle );
int dns2_get_A_addr ( int handle, uint8_t ** v4addr);

void dns2_free( int handle );


#endif /* DNS2_H_ */