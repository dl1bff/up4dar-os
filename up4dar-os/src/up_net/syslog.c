/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

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

#include "syslog.h"

#include "FreeRTOS.h"
#include "gcc_builtin.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/dhcp.h"

#include "up_dstar/vdisp.h"

#define ETHERNET_PAYLOAD_OFFSET  42
#define SYSLOG_UDP_PORT          514

#define PRIORITY_POSITION        1
#define ADDRESS_POSITION         9

union _IPAddress
{
  uint8_t b[4];
  uint32_t s_addr;
};

typedef union _IPAddress IPAddress;

const char* const template = "<000>1 - 000.000.000.000 UP4DAR - - - - ";

void syslog(char facility, char severity, const char* message, int length)
{
  if (dhcp_is_ready() == 0)
    return;

  IPAddress address;

  // For the first time we use local network broadcast as destination address
  address.s_addr = ((IPAddress*)ipv4_addr)->s_addr | ~ ((IPAddress*)ipv4_netmask)->s_addr;

  size_t size = length + sizeof(template);

  short port = udp_get_new_srcport();
  eth_txmem_t* packet = udp4_get_packet_mem(size, port, SYSLOG_UDP_PORT, address.b);

  if (packet == NULL)
    return;

  uint8_t* buffer = packet->data + ETHERNET_PAYLOAD_OFFSET;
  memcpy(buffer, template, sizeof(template));
  memcpy(buffer + sizeof(template), message, length);

  unsigned int priority = (facility << 3) | (severity & 7);
  vdisp_i2s(buffer + PRIORITY_POSITION, 3, 10, 1, priority);

  for (size_t index = 0; index < sizeof(ipv4_addr); index ++)
    vdisp_i2s(buffer + ADDRESS_POSITION + index * 4, 3, 10, 1, ipv4_addr[index]);

  udp4_calc_chksum_and_send(packet, address.b);
};
