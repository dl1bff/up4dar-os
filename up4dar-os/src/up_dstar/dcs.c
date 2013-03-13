
/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include "gcc_builtin.h"

#include "up_io/eth.h"
#include "up_io/eth_txmem.h"

#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/snmp.h"
#include "up_net/dns.h"

#include "up_crypto/up_crypto.h"

#include "up_app/a_lib_internal.h"
#include "up_app/a_lib.h"

#include "rx_dstar_crc_header.h"
#include "slow_data.h"
#include "sw_update.h"
#include "settings.h"
#include "vdisp.h"
#include "dstar.h"
#include "dcs.h"

#define DCS_KEEPALIVE_TIMEOUT       100
#define DCS_CONNECT_REQ_TIMEOUT     6
#define DCS_CONNECT_RETRIES         3
#define DCS_DISCONNECT_REQ_TIMEOUT  6
#define DCS_DISCONNECT_RETRIES      3
#define DCS_DNS_TIMEOUT             2
#define DCS_DNS_RETRIES             3
#define DCS_DNS_INITIAL_RETRIES     15

#define DCS_DISCONNECTED            1
#define DCS_CONNECT_REQ_SENT        2
#define DCS_CONNECTED               3
#define DCS_DISCONNECT_REQ_SENT     4
#define DCS_DNS_REQ_SENT            5
#define DCS_DNS_REQ                 6
#define DCS_WAIT                    7

#define DCS_REGISTER_MODULE         'D'

#define DCS_UDP_PORT                30051
#define DCS_VOICE_FRAME_SIZE        100
#define DCS_CONNECT_SIZE            519
#define DCS_CONNECT_REPLY_SIZE      14
#define DCS_KEEPALIVE_SIZE          22
#define DCS_KEEPALIVE_REPLY_SIZE    17

#define DEXTRA_UDP_PORT             30001
#define DEXTRA_CONNECT_SIZE         11
#define DEXTRA_CONNECT_REPLY_SIZE   14
#define DEXTRA_KEEPALIVE_SIZE       9
#define DEXTRA_KEEPALIVE_V2_SIZE    10
#define DEXTRA_RADIO_HEADER_SIZE    56
#define DEXTRA_VOICE_FRAME_SIZE     27

#define SERVER_TYPE_DCS             0
#define SERVER_TYPE_TST             1
#define SERVER_TYPE_DEXTRA          2

#define ETHERNET_PAYLOAD_OFFSET     42  // Skip IP + UDP headers

const char dcs_html_info[] =
  "<table border=\"0\" width=\"95%\"><tr>"
  "<td width=\"4%\"><img border=\"0\" src=\"up4dar_dcs.jpg\"></td>"
  "<td width=\"96%\">"
  "<font size=\"1\">Universal Platform for Digital Amateur Radio</font></br>"
  "<font size=\"2\"><b>www.UP4DAR.de</b>&nbsp;</font>"
  "<font size=\"1\">Version: X.0.00.00  </font>"
  "</td>"
  "</tr></table>";

const char* const dcs_state_text[] =
{
  "            ",
  "disconnected",
  "conn request",
  "connected   ",
  "disc request",
  "DNS request ",
  "DNS request ",
  "waiting     "
};

const char* const reflector_prefixes[] =
{
  "DCS",
  "TST",
  "XRF"
};

int dcs_state;
int dcs_timeout_timer;
int dcs_retry_counter;

char current_module;
char current_server_type;
short current_server;

uint8_t dcs_server_ipaddr[4];
char dcs_server_dns_name[25]; // dns name of reflector e.g. "dcs001.xreflector.net"

int dcs_udp_local_port;

uint8_t dcs_ambe_data[9];
int dcs_tx_counter = 0;

void dcs_link_to(char module);
void dcs_keepalive_response(int request_size);

void dcs_init()
{
  dcs_state = DCS_DISCONNECTED;  

  current_module = 'C';
  current_server = 1;  // DCS001
  current_server_type = SERVER_TYPE_DCS; // DCS
}

void dcs_get_current_reflector_name(char* buffer)
{
  memcpy(buffer, reflector_prefixes[current_server_type], 3);
  vdisp_i2s(buffer + 3, 3, 10, 1, current_server);
  buffer[6] = ' ';
  buffer[7] = current_module;
}

void dcs_set_dns_name()
{
  switch(current_server_type)
  {
    case SERVER_TYPE_TST:
      memcpy(dcs_server_dns_name, "tst", 3);
      vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
      memcpy(dcs_server_dns_name + 6, ".mdx.de", 8);
      break;

    case SERVER_TYPE_DCS:
      memcpy(dcs_server_dns_name, "dcs", 3);
      vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
      if ((current_server >= 100) && (current_server < 200))
      {
        memcpy(dcs_server_dns_name + 6, ".mdx.de", 8);
        break;
      }
      memcpy(dcs_server_dns_name + 6, ".xreflector.net", 16);
      break;

    case SERVER_TYPE_DEXTRA:
      memcpy(dcs_server_dns_name, "xrf", 3);
      vdisp_i2s(dcs_server_dns_name + 3, 3, 10, 1, current_server);
      if ((current_server >= 200) && (current_server < 300))
      {
        memcpy(dcs_server_dns_name + 6, ".dstar.su", 10);
        break;
      }
      memcpy(dcs_server_dns_name + 6, ".reflector.ircddb.net", 22);
      break;
  }
}

void dcs_set_source_port()
{
  dcs_udp_local_port = (current_server_type == SERVER_TYPE_DEXTRA) ? DEXTRA_UDP_PORT : udp_get_new_srcport();
  udp_socket_ports[UDP_SOCKET_DCS] = dcs_udp_local_port;
}

void dcs_service()
{
  if (dcs_timeout_timer > 0)
    dcs_timeout_timer --;

  vd_prints_xy(VDISP_REF_LAYER, 20, 36, VDISP_FONT_6x8, 0, dcs_state_text[  (dcs_mode != 0) ? dcs_state : 0 ]);

  switch (dcs_state)
  {
    case DCS_CONNECTED:
      if (dcs_timeout_timer > 0)
        break;

      dcs_timeout_timer = 2; // 1 second
      dcs_state = DCS_WAIT;
      udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
      vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "NOWD");
      break;

    case DCS_CONNECT_REQ_SENT:
      if (dcs_timeout_timer > 0)
        break;

      if (dcs_retry_counter > 0)
        dcs_retry_counter --;

      if (dcs_retry_counter > 0)
      {
        dcs_link_to(current_module);
        dcs_timeout_timer = DCS_CONNECT_REQ_TIMEOUT;
        break;
      }

      dcs_timeout_timer = 20; // 10 seconds
      dcs_state = DCS_WAIT;
      udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
      vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "RQTO");
      break;

    case DCS_DISCONNECT_REQ_SENT:
      if (dcs_timeout_timer > 0)
        break;

      if (dcs_retry_counter > 0)
        dcs_retry_counter --;

      if (dcs_retry_counter > 0)
      {
        dcs_link_to(' ');
        dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
        break;
      }

      dcs_state = DCS_DISCONNECTED;
      udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
      break;

    case DCS_DNS_REQ:
      if (!dns_get_lock()) // resolver busy
      {
        if (dcs_timeout_timer > 0)
          break;

        if (dcs_retry_counter > 0)
          dcs_retry_counter --;

        if (dcs_retry_counter > 0)
        {
          dcs_timeout_timer = DCS_DNS_TIMEOUT;
          break;  
        }

        dns_release_lock();
        dcs_timeout_timer = 30; // 15 seconds
        dcs_state = DCS_WAIT;
        break;
      }

      if (dns_req_A(dcs_server_dns_name) != 0)
      {
        dns_release_lock();
        dcs_timeout_timer = 10; // 5 seconds
        dcs_state = DCS_WAIT;
        break;
      }

      dcs_state = DCS_DNS_REQ_SENT;
      break;

    case DCS_DNS_REQ_SENT:
      if (!dns_result_available()) // resolver is busy
        break;

      if (dns_get_A_addr(dcs_server_ipaddr) < 0) // DNS didn't work
      {
        dns_release_lock();
        dcs_timeout_timer = 30; // 15 seconds
        dcs_state = DCS_WAIT;
        break;
      }

      dns_release_lock();

      dcs_set_source_port();
      dcs_link_to(current_module);

      dcs_state = DCS_CONNECT_REQ_SENT;
      dcs_retry_counter = DCS_CONNECT_RETRIES;
      dcs_timeout_timer =  DCS_CONNECT_REQ_TIMEOUT;
      break;

    case DCS_WAIT:
      if (dcs_timeout_timer > 0)
        break;

      dcs_retry_counter = DCS_DNS_INITIAL_RETRIES;
      dcs_timeout_timer = DCS_DNS_TIMEOUT;
      dcs_state = DCS_DNS_REQ;
      break;
  }
}

void dcs_on()
{
  if (dcs_state == DCS_DISCONNECTED)
  {
    dcs_set_dns_name();

    dcs_state = DCS_DNS_REQ;
    dcs_retry_counter = DCS_DNS_INITIAL_RETRIES;
    dcs_timeout_timer = DCS_DNS_TIMEOUT;
  }
}

void dcs_off()
{
  switch (dcs_state)
  {
    case DCS_CONNECTED:
      dcs_link_to(' ');

      dcs_state = DCS_DISCONNECT_REQ_SENT;
      dcs_retry_counter = DCS_DISCONNECT_RETRIES;
      dcs_timeout_timer = DCS_DISCONNECT_REQ_TIMEOUT;
      break;

    default:
      dcs_state = DCS_DISCONNECTED;
      udp_socket_ports[UDP_SOCKET_DCS] = 0; // stop receiving frames
      break;
  }
}

void dcs_input_packet(const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
  // packet is not from the currently selected server
  if (memcmp(ipv4_src_addr, dcs_server_ipaddr, sizeof ipv4_addr) != 0)
    return;

  switch (data_len)
  {
    case DCS_VOICE_FRAME_SIZE:
      if ((memcmp(data, "0001", 4) == 0) &&
          (data[14] == current_module)) // filter out the right channel
        dstarProcessDCSPacket(data);
      break;

    case DEXTRA_RADIO_HEADER_SIZE:
    case DEXTRA_VOICE_FRAME_SIZE: // voice packet
      if (memcmp(data, "DSVT", 4) == 0)
        dstarProcessDExtraPacket(data);
      break;

    case DCS_CONNECT_REPLY_SIZE:
    // case DEXTRA_CONNECT_REPLY_SIZE:
      if (dcs_state == DCS_CONNECT_REQ_SENT)
      {
        if (memcmp(data + 10, "ACK", 3) != 0)
        {
          dcs_state = DCS_DISCONNECTED;
          vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "NACK");
          break;
        }
        if (data[9] == current_module)
        {
          dcs_state = DCS_CONNECTED;
          dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
          vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "ACK ");
          a_app_manager_select_first(); // switch to main screen
          break;
        }
        if (data[9] == ' ')
        {
          dcs_state = DCS_DISCONNECTED;
          vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "DISC");
          break;
        }
      }
      break;

    case DCS_KEEPALIVE_SIZE:
    case DEXTRA_KEEPALIVE_SIZE:
    case DEXTRA_KEEPALIVE_V2_SIZE:
      if (dcs_state == DCS_CONNECTED)
      {
        dcs_timeout_timer = DCS_KEEPALIVE_TIMEOUT;
        dcs_keepalive_response(data_len);
      }
      break;
  }
}

int dcs_is_connected()
{
  return ((dcs_state == DCS_CONNECTED) || (dcs_state == DCS_DISCONNECT_REQ_SENT)) ? 1 : 0;
}

void dcs_reset_tx_counters(void)
{
  dcs_tx_counter = 0;
}

void dcs_select_reflector(short server_num, char module, char server_type)
{
  if (dcs_state == DCS_DISCONNECTED)  // only when disconnected
  {
    current_server_type = server_type;
    current_server = server_num;
    current_module = module;
  }
}

static eth_txmem_t * dcs_get_packet_mem(int udp_size)
{
  short port = (current_server_type != SERVER_TYPE_DEXTRA) ? DCS_UDP_PORT : DEXTRA_UDP_PORT;
  return udp4_get_packet_mem(udp_size, dcs_udp_local_port, port, dcs_server_ipaddr);
}

static void dcs_calc_chksum_and_send(eth_txmem_t* packet)
{
  udp4_calc_chksum_and_send(packet, dcs_server_ipaddr);
}

void copy_html_info(char* buffer)
{
  memcpy(buffer, dcs_html_info, sizeof(dcs_html_info));

  for (size_t index = 0; index < sizeof(dcs_html_info) - 11; index ++)
  {
    if (buffer[index] == 'X')  // look for 'X'
    {
      // replace  X.0.00.00  with current software version
      version2string(buffer + index, software_version);
      buffer[index + 10] = ' ';
      break;
    }
  }
}

void dcs_link_to(char module)
{
  int size = (current_server_type == SERVER_TYPE_DEXTRA) ? DEXTRA_CONNECT_SIZE : DCS_CONNECT_SIZE;
  eth_txmem_t * packet = dcs_get_packet_mem(size);

  if (packet == NULL)
    return;

  char buf[8];
  memcpy (buf, settings.s.my_callsign, 7);
  buf[7] = 0;
  vd_prints_xy(VDISP_DEBUG_LAYER, 86, 0, VDISP_FONT_6x8, 1, buf);
  vd_prints_xy(VDISP_DEBUG_LAYER, 104, 8, VDISP_FONT_6x8, 0, "    ");

  uint8_t* d = packet->data + ETHERNET_PAYLOAD_OFFSET;

  memcpy(d, settings.s.my_callsign, 7);
  d[7] = ' ';
  d[8] = DCS_REGISTER_MODULE; // my repeater module
  d[9] = module; // module to link to
  d[10] = 0;

  if (current_server_type != SERVER_TYPE_DEXTRA)
  {
    dcs_get_current_reflector_name(d + 11);
    d[18] = '@';
    copy_html_info(d + 19);
  }

  dcs_calc_chksum_and_send(packet);
}

void dcs_keepalive_response(int request_size)
{
  int size = (current_server_type == SERVER_TYPE_DEXTRA) ? request_size : DCS_KEEPALIVE_REPLY_SIZE;

  eth_txmem_t * packet = dcs_get_packet_mem(size);

  if (packet == NULL)
    return;

  uint8_t* d = packet->data + ETHERNET_PAYLOAD_OFFSET;

  memcpy (d, settings.s.my_callsign, 7);

  switch (request_size)
  {
    case DEXTRA_KEEPALIVE_SIZE:
      d[7] = ' ';
      d[8] = 0;
      break;

    case DEXTRA_KEEPALIVE_V2_SIZE:
      d[7] = DCS_REGISTER_MODULE;
      d[8] = current_module;
      d[9] = 0;
      break;

    case DCS_KEEPALIVE_SIZE:
      d[7] = DCS_REGISTER_MODULE;
      d[8] = 0;
      dcs_get_current_reflector_name(d + 9);
      break;
  }

  dcs_calc_chksum_and_send(packet);
}

void send_xcs(int session_id, char last_frame, char frame_counter)
{
  eth_txmem_t * packet = dcs_get_packet_mem(DCS_VOICE_FRAME_SIZE);

  if (packet == NULL)
    return;

  uint8_t * d = packet->data + ETHERNET_PAYLOAD_OFFSET;

  memcpy(d, "0001", 4);

  // Flags
  d[4] = 0;
  d[5] = 0;
  d[6] = 0;

  dcs_get_current_reflector_name(d + 7);
  memcpy (d + 15, settings.s.my_callsign, 7);
  d[22] = DCS_REGISTER_MODULE;
  memcpy(d + 23, "CQCQCQ  ", 8);
  memcpy (d + 31, settings.s.my_callsign, 8);
  memcpy (d + 39, settings.s.my_ext, 4);

  d[43] = (session_id >> 8) & 0xFF;
  d[44] = session_id & 0xFF;

  d[45] = frame_counter | (last_frame << 6);

  memcpy(d + 46, dcs_ambe_data, sizeof(dcs_ambe_data));
  build_slow_data(d + 55, last_frame, frame_counter, dcs_tx_counter);

  d[58] = dcs_tx_counter & 0xFF;
  d[59] = (dcs_tx_counter >> 8) & 0xFF;
  d[60] = (dcs_tx_counter >> 16) & 0xFF;

  d[61] = 0x01; // Frame Format version low
  d[62] = 0x00; // Frame Format version high
  d[63] = 0x21; // Language Set 0x21

  memcpy(d + 64, settings.s.txmsg, 20);

  dcs_calc_chksum_and_send(packet);

  dcs_tx_counter ++;
}

void send_dextra_header(int session_id)
{
  eth_txmem_t* packet = dcs_get_packet_mem(DEXTRA_RADIO_HEADER_SIZE);

  if (packet == NULL)
    return;

  uint8_t* d = packet->data + ETHERNET_PAYLOAD_OFFSET;

  // DSVT Header, 8 bytes
  memcpy(d, "DSVT", 4);
  d[4] = 0x10;  // Type = DSVT_TYPE_RADIO_HEADER
  d[5] = 0x00;
  d[6] = 0x00;
  d[7] = 0x00;

  // Trunk header
  d[8] = 0x20; // Type = RP2C_TYPE_DIGITAL_VOICE
  d[9] = 0x00; // Repeater 2
  d[10] = 0x00; // Repeater 1
  d[11] = 0x00; // Tarminal

  d[12] = (session_id >> 8) & 0xff;
  d[13] = session_id & 0xff;

  d[14] = 0x80; // Sequence = RP2C_NUMBER_RADIO_HEADER

  // Radio Header
  d[15] = 0;
  d[16] = 0;
  d[17] = 0;

  dcs_get_current_reflector_name(d + 18);
  memcpy(d + 26, d + 18, 7);
  d[33] = 'G';
  memcpy(d + 34, "CQCQCQ  ", 8); 
  memcpy(d + 42, settings.s.my_callsign, 8);
  memcpy(d + 50, settings.s.my_ext, 4);

  unsigned short sum = rx_dstar_crc_header(d + 15);
  d[54] = sum & 0xff;
  d[55] = (sum >> 8) & 0xff;

  dcs_calc_chksum_and_send(packet);
}

void send_dextra_frame(int session_id, char last_frame, char frame_counter)
{
  eth_txmem_t * packet = dcs_get_packet_mem(DEXTRA_VOICE_FRAME_SIZE);

  if (packet == NULL)
    return;

  uint8_t* d = packet->data + ETHERNET_PAYLOAD_OFFSET;

  // DSVT Header, 8 bytes
  memcpy(d, "DSVT", 4);
  d[4] = 0x20; // Type = DSVT_TYPE_DIGITAL_VOICE
  d[5] = 0x00;
  d[6] = 0x00;
  d[7] = 0x00;

  // Trunk header
  d[8] = 0x20; // Type = RP2C_TYPE_DIGITAL_VOICE
  d[9] = 0x00; // Repeater 2
  d[10] = 0x00; // Repeater 1
  d[11] = 0x00; // Tarminal

  d[12] = (session_id >> 8) & 0xff;
  d[13] = session_id & 0xff;

  d[14] = frame_counter | (last_frame << 6);

  memcpy(d + 15, dcs_ambe_data, sizeof(dcs_ambe_data));
  build_slow_data(d + 24, last_frame, frame_counter, dcs_tx_counter);

  dcs_calc_chksum_and_send(packet);

  dcs_tx_counter ++;
}

void send_dcs(int session_id, int last_frame, char frame_counter)
{
  if (dcs_state == DCS_CONNECTED)
  {
    switch (current_server_type)
    {
      case SERVER_TYPE_DEXTRA:
        if (frame_counter == 0)
          send_dextra_header(session_id);
        send_dextra_frame(session_id, last_frame, frame_counter);
        break;

      case SERVER_TYPE_DCS:
      case SERVER_TYPE_TST:
        send_xcs(session_id, last_frame, frame_counter);
        break;
    }
  }
}
