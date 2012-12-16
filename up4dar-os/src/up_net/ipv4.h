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

extern unsigned char ipv4_ntp[4];

extern unsigned char ipv4_dns_pri[4];
extern unsigned char ipv4_dns_sec[4];

extern const uint8_t ipv4_zero_addr[4];

#define UDP_PACKET_SIZE(a) (14 + 20 + 8 + (a))

#define NUM_UDP_SOCKETS   4

extern unsigned short udp_socket_ports[NUM_UDP_SOCKETS];

#define UDP_SOCKET_DHCP		0
#define UDP_SOCKET_SNMP		1
#define UDP_SOCKET_DNS		2
#define UDP_SOCKET_DCS		3



void ipv4_input (const uint8_t * p, int len, const uint8_t * eth_header);

int ipv4_get_neigh_addr( ip_addr_t * addr, const uint8_t * ipv4_dest );
int ipv4_addr_is_local ( const uint8_t * ipv4_a );
void ipv4_init(void);
void ipv4_udp_prepare_packet( eth_txmem_t * packet, const uint8_t * dest_ipv4_addr, int udp_data_length, int udp_src_port, int udp_dest_port );

eth_txmem_t * udp4_get_packet_mem (int udp_size, int src_port, int dest_port, const uint8_t * ipv4_dest_addr);

void udp4_calc_chksum_and_send (eth_txmem_t * packet, const uint8_t * ipv4_dest_addr);
int udp_get_new_srcport(void);

void ipv4_print_ip_addr(int y, const char * desc, const uint8_t * ip);

#endif /* IPV4_H_ */