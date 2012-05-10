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
 * arp.h
 *
 * Created: 10.05.2012 16:32:38
 *  Author: mdirska
 */ 


#ifndef ARP_H_
#define ARP_H_







void arp_process_packet(uint8_t * raw_packet);

void arp_send_request (const ip_addr_t * a, int unicast, const mac_addr_t * m);

#endif /* ARP_H_ */