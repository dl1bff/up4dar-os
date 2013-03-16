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

#define SLOW_DATA_CHUNK_SIZE     5

#define APRS_SEND_ONLY_PORT      8080
#define ETHERNET_PAYLOAD_OFFSET  42

#pragma mark APRS and D-PRS packet building functions

static xSemaphoreHandle lock;

static unsigned long validity1 = 0;  // Position data validity time
static unsigned long validity2 = 0;  // Altitude data validity time

static int64_t altitude = UNDEFINED_ALTITUDE;
static char buffer[APRS_BUFFER_SIZE];

static char* pointer1 = NULL;  // Pointer to TNC-2 packet header
static char* pointer2 = NULL;  // Pointer to APRS packet data
static char* terminator = NULL;

static char* reader = NULL;

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

void prepare_packet()
{
  // Fill D-PRS header
  memcpy(buffer, "$$CRCxxxx,", DPRS_SIGN_LENGTH);
  pointer1 = buffer + DPRS_SIGN_LENGTH;
  // Fill TNC-2 header
  pointer2 = pointer1;
  pointer2 += build_aprs_call(pointer2);
  memcpy(pointer2, ">APD4XX,DSTAR*:", 15);
  pointer2[5] = (software_version[1] % 10) + '0';
  pointer2[6] = (software_version[2] % 24) + 'A';
  pointer2 += 15;
  terminator = pointer2;
}

void update_packet(const char** parameters)
{
  size_t length = build_position_report(pointer2, parameters);
  // Fill D-PRS/APRS-IS terminator and Slow Data filler
  terminator = pointer2 + length;
  memcpy(terminator, "\r\n", 2);
  terminator += 2;
  memset(terminator, 0x66, 5);
  // Update CRC in D-PRS header
  length = terminator - pointer1 - 1; // From TNC-2 header to CR
  uint16_t sum = rx_dstar_crc_data(pointer1, length);
  vdisp_i2s(buffer + 5, 4, 16, 1, sum);
  buffer[DPRS_SIGN_LENGTH - 1] = ',';
  // Reset D-PRS reader position
  reader = buffer;
}

int has_packet_data()
{
  return
    (validity1 > the_clock) &&
    (pointer2 < terminator);
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
    update_packet(parameters);
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
  size_t count = 0;
  if (xSemaphoreTake(lock, portTICK_RATE_MS * 2) == pdTRUE)
  {
    if (has_packet_data() == 0)
    {
      xSemaphoreGive(lock);
      return;
    }

    memcpy(data, reader, SLOW_DATA_CHUNK_SIZE);

    count = terminator - reader;
    if (count > SLOW_DATA_CHUNK_SIZE)
      count = SLOW_DATA_CHUNK_SIZE;

    reader += SLOW_DATA_CHUNK_SIZE;
    if (reader >= terminator)
      reader = buffer;

    xSemaphoreGive(lock);
  }
  return count;
}

#pragma mark APRS-IS reporting

void calculate_aprs_password(char* buffer)
{
  uint8_t hash[] = { 0x73, 0xe2 };

  for (size_t index = 0; (index < CALLSIGN_LENGTH) && (settings.s.my_callsign[index] > ' '); index ++)
    hash[index & 1] ^= settings.s.my_callsign[index];

  uint16_t code = ((hash[0] << 8) | hash[1]) & 0x7fff;
  vdisp_i2s(buffer, 5, 10, 0, code);
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

    uint8_t* pointer = packet->data + ETHERNET_PAYLOAD_OFFSET;

    memset(pointer, 0, APRS_IS_BUFFER_SIZE);

    memcpy(pointer, "user ", 5);
    pointer += 5;
    pointer += build_aprs_call(pointer);

    memcpy(pointer, " pass ", 6);
    calculate_aprs_password(pointer + 6);
    pointer += 11;

    memcpy(pointer, " vers UP4DAR X.0.00.00  \r\n", 26);
    version2string(pointer + 13, software_version);
    pointer[23] = ' ';
    pointer += 26;

    size_t length = terminator - pointer1;
    memcpy(pointer, pointer1, length);
    pointer += length;

    udp4_calc_chksum_and_send(packet, address);
    xSemaphoreGive(lock);
  }
}

#pragma mark Routine

void aprs_reset()
{
  reader = buffer;
  if (SETTING_CHAR(C_DPRS_ENABLED) > 0)
    send_network_report();
}

void aprs_activate_beacon()
{
  if (SETTING_CHAR(C_APRS_BEACON) == 0)
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
  prepare_packet();
  port = udp_get_new_srcport();
  lock = xSemaphoreCreateMutex();
  dns_cache_set_slot(DNS_CACHE_SLOT_APRS, "aprs.dstar.su", aprs_activate_beacon);
}