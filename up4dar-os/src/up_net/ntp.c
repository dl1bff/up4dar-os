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

#pragma pack(push, 1)

struct ntp_packet {
  uint8_t mode;
  uint8_t stratum;
  char poll;
  char precision;
  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t reference_identifier;
  uint32_t reference_timestamp_secs;
  uint32_t reference_timestamp_fraq;
  uint32_t originate_timestamp_secs;
  uint32_t originate_timestamp_fraq;
  uint32_t receive_timestamp_seqs;
  uint32_t receive_timestamp_fraq;
  uint32_t transmit_timestamp_secs;
  uint32_t transmit_timestamp_fraq;
};

#pragma pack(pop)

#define NTP_PORT                 123
#define LOCAL_PORT               udp_socket_ports[UDP_SOCKET_NTP]
#define ETHERNET_PAYLOAD_OFFSET  42

#define SHOT_COUNT               4

uint32_t htonl(uint32_t value)
{
  union presentation
  {
    uint32_t value;
    uint8_t octets[4];
  };
  union presentation outcome;
  outcome.octets[0] = (value >> 24) & 0xff;
  outcome.octets[1] = (value >> 16) & 0xff;
  outcome.octets[2] = (value >>  8) & 0xff;
  outcome.octets[3] = (value >>  0) & 0xff;
  return outcome.value;
};

#define ntohl(value)  htonl((value))

void ntp_handle_packet(const uint8_t* buffer, int length, const uint8_t* address)
{
  if (length == sizeof(struct ntp_packet))
  {
    struct ntp_packet* data = (struct ntp_packet*)buffer;
    the_clock = ntohl(data->transmit_timestamp_secs);
  }
}

void query_time()
{
  ip_addr_t address;

  if (!dns_cache_get_address(DNS_CACHE_SLOT_NTP, &address))
    return;

  eth_txmem_t* packet = udp4_get_packet_mem(sizeof(struct ntp_packet), LOCAL_PORT, NTP_PORT, address.ipv4.addr);

  if (packet == NULL)
    return;

  struct ntp_packet* data = (struct ntp_packet*)(packet->data + ETHERNET_PAYLOAD_OFFSET);

  memset(data, 0, sizeof(struct ntp_packet));
  data->mode = (4 << 3) | 3;

  udp4_calc_chksum_and_send(packet, address.ipv4.addr);
}

void make_shot()
{
  for (size_t index = 0; index < SHOT_COUNT; index ++)
    query_time();
}

void ntp_init()
{
  LOCAL_PORT = udp_get_new_srcport();
  dns_cache_set_slot(DNS_CACHE_SLOT_NTP, "pool.ntp.org", make_shot);
}
