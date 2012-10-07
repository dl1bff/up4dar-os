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
 * dns.h
 *
 * Created: 13.09.2012 09:00:11
 *  Author: mdirska
 */ 


#ifndef DNS_H_
#define DNS_H_


void dns_init(void);
void dns_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr);
int dns_get_lock(void);
void dns_release_lock(void);
int dns_req_A (const char * name);
int dns_result_available(void);
int dns_get_A_addr ( uint8_t * v4addr);

#endif /* DNS_H_ */
