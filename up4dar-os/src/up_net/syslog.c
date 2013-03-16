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
#include "queue.h"
#include "gcc_builtin.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/dhcp.h"

#include "up_dstar/vdisp.h"
#include "up_dstar/sw_update.h"

#define ETHERNET_PAYLOAD_OFFSET  42
#define SYSLOG_UDP_PORT          514

#define PRIORITY_POSITION        1
#define ADDRESS_POSITION         9
#define TEMPLATE_LENGTH          (sizeof(template) - 1)

const char template[] = "<000>1 - 000.000.000.000 UP4DAR - - - - ";

void syslog(char facility, char severity, const char* message)
{
  if ((facility == LOG_DEBUG) && ((software_version[0] & 0xc0) == 0))
    return;

  if (dhcp_is_ready() == 0)
    return;

  if (memcmp(ipv4_syslog, ipv4_zero_addr, sizeof(ipv4_zero_addr)) == 0)
    return;

  size_t length = strlen(message);
  size_t size = length + TEMPLATE_LENGTH;

  short port = udp_get_new_srcport();
  eth_txmem_t* packet = udp4_get_packet_mem(size, port, SYSLOG_UDP_PORT, ipv4_syslog);

  if (packet == NULL)
    return;

  uint8_t* buffer = packet->data + ETHERNET_PAYLOAD_OFFSET;
  memcpy(buffer, template, sizeof(template));
  memcpy(buffer + TEMPLATE_LENGTH, message, length);

  unsigned int priority = (facility << 3) | (severity & 7);
  vdisp_i2s(buffer + PRIORITY_POSITION, 3, 10, 1, priority);
  buffer[PRIORITY_POSITION + 3] = '>';

  for (size_t index = 0; index < sizeof(ipv4_addr); index ++)
  {
    vdisp_i2s(buffer + ADDRESS_POSITION + index * 4, 3, 10, 1, ipv4_addr[index]);
    buffer[ADDRESS_POSITION + index * 4 + 3] = '.';
  }
  buffer[ADDRESS_POSITION + 16] = ' ';

  udp4_calc_chksum_and_send(packet, ipv4_syslog);
};
