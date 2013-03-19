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

#include "aprs.h"

#include "queue.h"
#include "semphr.h"

#include "settings.h"
#include "sw_update.h"
#include "rx_dstar_crc_header.h"
#include "vdisp.h"
#include "rtclock.h"
#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/dhcp.h"
#include "up_net/dns_cache.h"

#include "up_sys/timer.h"

#define UNDEFINED_ALTITUDE       INT64_MIN
#define DATA_VALIDITY_INTERVAL   600

#define APRS_BUFFER_SIZE         (100 + DPRS_MSG_LENGTH)
#define APRS_IS_BUFFER_SIZE      (64 + APRS_BUFFER_SIZE)
#define APRS_POSITION_LENGTH     34
#define DPRS_SIGN_LENGTH         10

#define CRC_POSITION             5
#define TNC2_POSITION            DPRS_SIGN_LENGTH

#define SLOW_DATA_CHUNK_SIZE     5

#define APRS_SEND_ONLY_PORT      8080
#define ETHERNET_PAYLOAD_OFFSET  42

#define OPERATIONAL_BUFFER       0
#define OUTGOING_BUFFER          1

#define BUFFER_COUNT             2

struct buffer
{
  int version;
  int length;
  char data[APRS_BUFFER_SIZE];
};

#pragma mark APRS and D-PRS packet building functions

static xSemaphoreHandle lock;

static unsigned long validity1 = 0;  // Position data validity time
static unsigned long validity2 = 0;  // Altitude data validity time

static int64_t altitude = UNDEFINED_ALTITUDE;
static struct buffer buffers[BUFFER_COUNT];

static int position = 0;

static int port;

static const char* const symbols[] =
  {
    "/[", // Jogger
    "/>", // Car
    "/-", // House
    "/s", // Boat
    "/b", // Bicycle
    "/v"  // Van
  };

#pragma mark APRS packet building

size_t build_altitude_extension(char* buffer)
{
  if ((altitude != UNDEFINED_ALTITUDE) && (validity2 > the_clock))
  {
    if (altitude >= 0)
    {
      memcpy(buffer, "/A=", 3);
      vdisp_i2s(buffer + 3, 6, 10, 1, (unsigned int) altitude);
    }
    else
    {
      memcpy(buffer, "/A=-", 4);
      vdisp_i2s(buffer + 4, 5, 10, 1, (unsigned int) - altitude);
    }
    return 9;
  }
  return 0;
}

void copy_extension(char* buffer, const char* parameter)
{
  memset(buffer, '0', 3);
  char* delimiter = strstr(parameter, ".");
  if ((delimiter != NULL) && (delimiter <= (parameter + 3)))
  {
    char* position1 = buffer + 2;
    char* position2 = delimiter - 1;
    do
    {
      *position1 = *position2;
      position1 --;
      position2 --;
    }
    while ((position1 >= buffer) && (position2 >= parameter));
  }
}

size_t build_position_report(char* buffer, const char** parameters)
{
  size_t length = APRS_POSITION_LENGTH;
  const char* symbol = symbols[SETTING_CHAR(C_DPRS_SYMBOL)];
  // Position report with time (no messaging capability)
  buffer[0] = '/';
  // Time
  memcpy(buffer + 1, parameters[1], 6);
  buffer[7] = 'z';
  // Latitude
  memcpy(buffer + 8, parameters[3], 7);
  buffer[15] = *parameters[4];
  // Symbol table / Overlay
  buffer[16] = symbol[0];
  // Longitude
  memcpy(buffer + 17, parameters[5], 8);
  buffer[25] = *parameters[6];
  // Symbol code
  buffer[26] = symbol[1];
  // Course
  copy_extension(buffer + 27, parameters[8]);
  buffer[30] = '/';
  // Speed
  copy_extension(buffer + 31, parameters[7]);
  // Altitude (optional)
  length += build_altitude_extension(buffer + 34);
  // Commentary
  memcpy(buffer + length, settings.s.dprs_msg, DPRS_MSG_LENGTH);
  length += DPRS_MSG_LENGTH;

  return length;
}

size_t build_aprs_call(char* buffer)
{
  size_t number;
  for (number = 0; (number < CALLSIGN_LENGTH) && (settings.s.my_callsign[number] > ' '); number ++)
    buffer[number] = settings.s.my_callsign[number];
  if ((SETTING_CHAR(C_APRS_SSID) > 0) && (SETTING_CHAR(C_APRS_SSID) < 10))
  {
    buffer[number] = '-';
    buffer[number + 1] = SETTING_CHAR(C_APRS_SSID) + 0x30;
    number += 2;
  }
  if ((SETTING_CHAR(C_APRS_SSID) >= 10) && (SETTING_CHAR(C_APRS_SSID) <= 15))
  {
    buffer[number] = '-';
    buffer[number + 1] = '1';
    buffer[number + 2] = SETTING_CHAR(C_APRS_SSID) + 0x26;
    number += 3;
  }
  return number;
}

#pragma mark Buffered operations

void build_packet(const char** parameters)
{
  char* data = buffers[OPERATIONAL_BUFFER].data;
  // Fill D-PRS header
  memcpy(data, "$$CRCxxxx,", DPRS_SIGN_LENGTH);
  data += DPRS_SIGN_LENGTH;
  // Fill TNC-2 header
  data += build_aprs_call(data);
  memcpy(data, ">APD4XX,DSTAR*:", 15);
  data[5] = (software_version[1] % 10) + '0';
  data[6] = (software_version[2] % 24) + 'A';
  data += 15;
  // Fill APRS payload
  data += build_position_report(data, parameters);
  // Fill D-PRS/APRS-IS terminator and Slow Data filler
  memcpy(data, "\r\n", 2);
  data += 2;
  memset(data, 0x66, 5);
  // Update buffer attributes
  int length = data - buffers[OPERATIONAL_BUFFER].data;
  buffers[OPERATIONAL_BUFFER].length = length;
  buffers[OPERATIONAL_BUFFER].version ++;
  // Update CRC in D-PRS header
  data = buffers[OPERATIONAL_BUFFER].data;
  length -= TNC2_POSITION + 1; // Length from TNC-2 header to CR
  uint16_t sum = rx_dstar_crc_data(data + TNC2_POSITION, length);
  vdisp_i2s(data + CRC_POSITION, 4, 16, 1, sum);
  data[DPRS_SIGN_LENGTH - 1] = ',';
}

int has_packet_data()
{
  return
    (validity1 > the_clock) &&
    (buffers[OPERATIONAL_BUFFER].length > 0);
}

#pragma mark GPS data handling

int parse_digits(const char* data, size_t length)
{
  int outcome = 0;
  for (size_t index = 0; index < length; index ++)
    outcome = outcome * 10 + data[index] - 0x30;
  return outcome;
}

long parse_time(const char* time)
{
  if (*time != 0)
    return
      parse_digits(time, 2) * 3600 +
      parse_digits(time + 2, 2) * 60 +
      parse_digits(time + 4, 2);
  return 0;
}

void process_position_fix_data(const char** parameters)
{
  altitude = (*parameters[9] != 0) ? 
    ((atoi(parameters[9]) * 26444) >> 13) :
    UNDEFINED_ALTITUDE;

  long time = parse_time(parameters[1]);
  if (the_clock < time)
    rtclock_set_time(time);
}

void aprs_process_gps_data(const char** parameters, size_t count)
{
  if ((count >= 12) &&
      (memcmp(parameters[0], "GPRMC", 6) == 0) &&
      (*parameters[2] == 'A') &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    build_packet(parameters);
    validity1 = the_clock + DATA_VALIDITY_INTERVAL;
    xSemaphoreGive(lock);
    return;
  }
  if ((count >= 15) &&
      (memcmp(parameters[0], "GPGGA", 6) == 0) &&
      (*parameters[6] != '0') &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    process_position_fix_data(parameters);
    validity2 = the_clock + DATA_VALIDITY_INTERVAL;
    xSemaphoreGive(lock);
    return;
  }
}

#pragma mark DV-A reporting

uint8_t aprs_get_slow_data(uint8_t* data)
{
  int count = 0;
  if (xSemaphoreTake(lock, portTICK_RATE_MS * 2) == pdTRUE)
  {
    if (has_packet_data() == 0)
    {
      xSemaphoreGive(lock);
      return;
    }

    if ((position == 0) &&
        (buffers[OUTGOING_BUFFER].version != buffers[OPERATIONAL_BUFFER].version))
      memcpy(&buffers[OUTGOING_BUFFER], &buffers[OPERATIONAL_BUFFER], sizeof(struct buffer));

    memcpy(data, buffers[OUTGOING_BUFFER].data + position, SLOW_DATA_CHUNK_SIZE);

    count = buffers[OUTGOING_BUFFER].length;
    if (count > SLOW_DATA_CHUNK_SIZE)
      count = SLOW_DATA_CHUNK_SIZE;

    position += SLOW_DATA_CHUNK_SIZE;
    if (position >= buffers[OUTGOING_BUFFER].length)
      position = 0;

    xSemaphoreGive(lock);
  }
  return count;
}

#pragma mark APRS-IS reporting

void calculate_aprs_password(char* password)
{
  uint8_t hash[] = { 0x73, 0xe2 };

  for (size_t index = 0; (index < CALLSIGN_LENGTH) && (settings.s.my_callsign[index] > ' '); index ++)
    hash[index & 1] ^= settings.s.my_callsign[index];

  uint16_t code = ((hash[0] << 8) | hash[1]) & 0x7fff;
  vdisp_i2s(password, 5, 10, 0, code);
}

void send_network_report()
{
  uint8_t address[4];
  if ((dhcp_is_ready() != 0) && 
      (dns_cache_get_address(DNS_CACHE_SLOT_APRS, address) != 0) &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    if (has_packet_data() == 0)
    {
      xSemaphoreGive(lock);
      return;
    }

    eth_txmem_t * packet = udp4_get_packet_mem(APRS_IS_BUFFER_SIZE, port, APRS_SEND_ONLY_PORT, address);

    if (packet == NULL)
    {
      xSemaphoreGive(lock);
      return;
    }

    uint8_t* data = packet->data + ETHERNET_PAYLOAD_OFFSET;

    memset(data, 0, APRS_IS_BUFFER_SIZE);

    memcpy(data, "user ", 5);
    data += 5;
    data += build_aprs_call(data);

    memcpy(data, " pass ", 6);
    calculate_aprs_password(data + 6);
    data += 11;

    memcpy(data, " vers UP4DAR X.0.00.00  \r\n", 26);
    version2string(data + 13, software_version);
    data[23] = ' ';
    data += 26;

    memcpy(data, 
      buffers[OPERATIONAL_BUFFER].data + DPRS_SIGN_LENGTH,
      buffers[OPERATIONAL_BUFFER].length - DPRS_SIGN_LENGTH);

    udp4_calc_chksum_and_send(packet, address);
    xSemaphoreGive(lock);
  }
}

#pragma mark Routine

void aprs_reset()
{
  if (SETTING_CHAR(C_DPRS_ENABLED) != 0)
    send_network_report();
  position = 0;
}

void aprs_activate_beacon()
{
  if ((SETTING_CHAR(C_APRS_BEACON) == 0) ||
      (SETTING_CHAR(C_DPRS_ENABLED) != 0))
  {
    timer_set_slot(TIMER_SLOT_APRS_BEACON, 0, NULL);
    return;
  }

  int interval = SETTING_CHAR(C_APRS_BEACON) * 60 * 1000;
  timer_set_slot(TIMER_SLOT_APRS_BEACON, interval, send_network_report);
  send_network_report();
}

void aprs_init()
{
  port = udp_get_new_srcport();
  lock = xSemaphoreCreateMutex();

  for (int index = 0; index < BUFFER_COUNT; index ++)
  {
    buffers[index].version = 0;
    buffers[index].length = 0;
  }

  dns_cache_set_slot(DNS_CACHE_SLOT_APRS, "aprs.dstar.su", aprs_activate_beacon);
}