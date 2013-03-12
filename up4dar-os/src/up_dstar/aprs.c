#include "aprs.h"

#include "semphr.h"

#include "settings.h"
#include "sw_update.h"
#include "rx_dstar_crc_header.h"
#include "vdisp.h"
#include "up_io/eth.h"
#include "up_io/eth_txmem.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/dhcp.h"
#include "up_net/dns_cache.h"

#define UNDEFINED_ALTITUDE       INT64_MIN

#define APRS_BUFFER_SIZE         400
#define APRS_POSITION_LENGTH     34
#define DPRS_SIGN_LENGTH         10

#define SLOW_DATA_CHUNK_SIZE     5

#define DNS_CACHE_SLOT_APRS      1
#define APRS_SEND_ONLY_PORT      8080
#define ETHERNET_PAYLOAD_OFFSET  42

#pragma mark APRS and D-PRS packet building functions

int64_t altitude = UNDEFINED_ALTITUDE;

xSemaphoreHandle lock;
char buffer[APRS_BUFFER_SIZE];

char* pointer1 = NULL;  // Pointer to TNC-2 packet header
char* pointer2 = NULL;  // Pointer to APRS packet data
char* terminator = NULL;

char* reader = NULL;

int port;
char password[6];

#pragma mark Generic packet building functions

void handle_position_fix_data(const char** parameters)
{
  altitude = (*parameters[9] != 0) ? 
    ((atoi(parameters[9]) * 26444) >> 13) :
    UNDEFINED_ALTITUDE;
}

size_t build_alitude_extension(char* buffer)
{
  if (altitude != UNDEFINED_ALTITUDE)
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

size_t build_position_report(char* buffer, const char** parameters)
{
  size_t length = APRS_POSITION_LENGTH;
  // Position report with time (no messaging capability)
  buffer[0] = '/';
  // Time
  memcpy(buffer + 1, parameters[1], 6);
  buffer[7] = 'z';
  // Latitude
  memcpy(buffer + 8, parameters[3], 7);
  buffer[15] = *parameters[4];
  // Symbol table / Overlay
  buffer[16] = settings.s.aprs_symbol[0];
  // Longitude
  memcpy(buffer + 17, parameters[5], 8);
  buffer[25] = *parameters[6];
  // Symbol code
  buffer[26] = settings.s.aprs_symbol[1];
  // Course
  memcpy(buffer + 27, parameters[8], 3);
  buffer[30] = '/';
  // Speed
  memcpy(buffer + 27, parameters[7], 3);
  // Alittude (optional)
  length += build_alitude_extension(buffer + 34);
  // Comment
  memcpy(buffer + length, settings.s.dprs_msg, DPRS_MSG_LENGTH);
  length += DPRS_MSG_LENGTH;
  
  return length;
}

size_t build_aprs_call(char* buffer)
{
  size_t number;
  for (number = 0; (number < CALLSIGN_LENGTH) && (settings.s.my_callsign[number] > ' '); number ++)
    buffer[number] = settings.s.my_callsign[number];
  if ((settings.s.aprs_ssid > 0) && (settings.s.aprs_ssid < 10))
  {
    buffer[number] = '-';
    buffer[number + 1] = settings.s.aprs_ssid + 0x30;
    number += 2;
  }
  if ((settings.s.aprs_ssid >= 10) && (settings.s.aprs_ssid <= 15))
  {
    buffer[number] = '-';
    buffer[number + 1] = '1';
    buffer[number + 2] = settings.s.aprs_ssid + 0x26;
    number += 3;
  }
  return number;
}

#pragma mark Buffered packet building functions

void prepare_packet()
{
  // Fill D-PRS header
  memcpy(buffer, "$$CRCxxxx,", DPRS_SIGN_LENGTH);
  pointer1 = buffer + DPRS_SIGN_LENGTH;
  // Fill TNC-2 header
  pointer2 = pointer1;
  pointer2 += build_aprs_call(pointer2);
  memcpy(pointer2, ">APD4XX,DSTAR*:", 15);
  pointer2[5] = '0' + software_version[1] & 0x0f;
  pointer2[6] = 'A' + (software_version[2] / 10) & 0x0f;
  pointer2 += 15;
  terminator = pointer2;
}

void update_packet(const char** parameters)
{
  size_t length = build_position_report(pointer2, parameters);
  // Update CRC at D-PRS header
  uint16_t sum = rx_dstar_crc_data(buffer + DPRS_SIGN_LENGTH, length);
  vdisp_i2s(buffer + 5, 4, 16, 1, sum);
  // Fill packet terminator
  terminator = pointer2 + length;
  memcpy(terminator, "\r\n", 2);
  terminator += 2;
  memset(terminator, 0x66, 5);
}

#pragma mark APRS-IS functions

void calculate_aprs_password()
{
  uint16_t hash = 0x73e2;
  for (size_t index = 0; (index < CALLSIGN_LENGTH) && (settings.s.my_callsign[index] > ' ') && (settings.s.my_callsign[index + 1] > ' '); index += 2)
    hash ^= (settings.s.my_callsign[index] << 8) | settings.s.my_callsign[index + 1];
  hash &= 0x7fff;
  vdisp_i2s(password, 6, 10, 0, hash);
}

void send_network_report()
{
  ip_addr_t address;
  if ((dhcp_is_ready() != 0) && 
      (dns_cache_get_address(DNS_CACHE_SLOT_APRS, &address) != 0) &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    eth_txmem_t * packet = udp4_get_packet_mem(APRS_BUFFER_SIZE, port, APRS_SEND_ONLY_PORT, address.ipv4.addr);

    if (packet == NULL)
    {
      xSemaphoreGive(lock);
      return;
    }

    uint8_t* pointer = packet->data + ETHERNET_PAYLOAD_OFFSET;

    memcpy(pointer, "user ", 5);
    pointer += build_aprs_call(pointer + 5);
    memcpy(pointer, " pass ", 6);
    memcpy(pointer + 6, password, 6);
    memcpy(pointer + 12, " vers UP4DAR X.0.00.00 \r\n", 25);
    version2string(pointer + 25, software_version);
    pointer[35] = ' ';
    pointer += 37;

    size_t length = terminator - pointer1;
    memcpy(pointer, pointer1, length);
    pointer += length;

    packet->tx_size = pointer - packet->data; // Is it correct?

    udp4_calc_chksum_and_send(packet, address.ipv4.addr);
    xSemaphoreGive(lock);
  }
}

#pragma mark Common exported functions

void aprs_process_gps_data(const char** parameters)
{
  if ((memcmp(parameters[0], "GPGGA", 6) == 0) &&
      (*parameters[4] != '0') &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    handle_position_fix_data(parameters);
    xSemaphoreGive(lock);
    return;
  }
  if ((memcmp(parameters[0], "GPRMC", 6) == 0) &&
      (*parameters[2] == 'A') &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    update_packet(parameters);
    xSemaphoreGive(lock);
    return;
  }
}

size_t aprs_get_slow_data(uint8_t* data)
{
  size_t count = 0;
  if (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE)
  {
    memcpy(data, reader, SLOW_DATA_CHUNK_SIZE);

    count = terminator - reader;
    if (count > SLOW_DATA_CHUNK_SIZE)
      count = SLOW_DATA_CHUNK_SIZE;

    reader += SLOW_DATA_CHUNK_SIZE;
    if (reader > terminator)
      reader = buffer;

    xSemaphoreGive(lock);
  }
  return count;
}

#pragma mark APRS-IS Task

void handle_dns_cache_event()
{

}

void aprs_init()
{
  reader = buffer;
  prepare_packet();
  calculate_aprs_password();
  port = udp_get_new_srcport();
  dns_cache_set_slot(DNS_CACHE_SLOT_APRS, "rotate.aprs.net", handle_dns_cache_event);
}