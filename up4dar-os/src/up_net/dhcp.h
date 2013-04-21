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
 * dhcp.h
 *
 * Created: 13.05.2012 05:20:10
 *  Author: mdirska
 */ 


#ifndef DHCP_H_
#define DHCP_H_






int dhcp_is_ready(void);

void dhcp_init(int fixed_address);
void dhcp_set_link_state (int link_up);
void dhcp_service(void);
void dhcp_input_packet (const uint8_t * data, int data_len);


#endif /* DHCP_H_ */