/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ntp.h"

#include "up_dstar/rtclock.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "ipneigh.h"
#include "ipv4.h"

#include "dns_cache.h"


#define NTP_PORT                 123
#define NTP_PACKET_LENGTH        48

#define LOCAL_PORT               udp_socket_ports[UDP_SOCKET_NTP]
#define ETHERNET_PAYLOAD_OFFSET  42

#define SHOT_COUNT               4

void ntp_handle_packet(const uint8_t* data, int length, const uint8_t* address)
{
  if (length == NTP_PACKET_LENGTH)
  {
    uint32_t time = (data[40] << 24) | (data[41] << 16) | (data[42] << 8) | data[43];
    rtclock_set_time(time);
  }  
}

void query_time(uint8_t* address)
{
  eth_txmem_t* packet = udp4_get_packet_mem(NTP_PACKET_LENGTH, LOCAL_PORT, NTP_PORT, address);

  if (packet == NULL)
    return;

  uint8_t* data = packet->data + ETHERNET_PAYLOAD_OFFSET;
  memset(data, 0, NTP_PACKET_LENGTH);
  data[0] = (4 << 3) | 3;

  udp4_calc_chksum_and_send(packet, address);
}

void make_shot()
{
  uint8_t address[4];
  if (!dns_cache_get_address(DNS_CACHE_SLOT_NTP, address))
    return;

  LOCAL_PORT = udp_get_new_srcport();
  for (size_t index = 0; index < SHOT_COUNT; index ++)
    query_time(address);
}

void ntp_init()
{
  dns_cache_set_slot(DNS_CACHE_SLOT_NTP, "0.up4dar.pool.ntp.org", make_shot);
}
