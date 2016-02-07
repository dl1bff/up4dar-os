/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)
Copyright (C) 2015   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
#include "up_app/a_lib_internal.h"
#include "up_net/ipneigh.h"
#include "up_net/ipv4.h"
#include "up_net/dhcp.h"
#include "up_net/dns2.h"
#include "software_version.h"

/*
#define UNDEFINED_ALTITUDE       INT64_MIN
#define DATA_VALIDITY_INTERVAL   600

#define APRS_BUFFER_SIZE         (100 + DPRS_MSG_LENGTH)
#define APRS_IS_BUFFER_SIZE      (64 + APRS_BUFFER_SIZE)
#define APRS_POSITION_LENGTH     34
#define DPRS_SIGN_LENGTH         10

#define CRC_POSITION             5
#define TNC2_POSITION            DPRS_SIGN_LENGTH

#define SLOW_DATA_CHUNK_SIZE     5

*/
#define APRS_SEND_ONLY_PORT      8080


/*
#define ETHERNET_PAYLOAD_OFFSET  42

#define OPERATIONAL_BUFFER       0
#define OUTGOING_BUFFER          1

#define BUFFER_COUNT             2

// Aux. functions
const uint8_t* dns_cache_aprs(void);

struct buffer
{
  int version;
  int length;
  char data[APRS_BUFFER_SIZE];
};

// #pragma mark APRS and D-PRS packet building functions

static xSemaphoreHandle lock;

static unsigned long validity1 = 0;  // Position data validity time
static unsigned long validity2 = 0;  // Altitude data validity time

static int64_t altitude = UNDEFINED_ALTITUDE;
static struct buffer buffers[BUFFER_COUNT];

static int position = 0;
*/

static int aprs_local_port;

/*
static const char* const symbols[] =
  {
    "/[", // Jogger
    "/>", // Car
    "/-", // House
    "/s", // Boat
    "/b", // Bicycle
    "/v"  // Van
  };

*/

// #pragma mark APRS packet building

/*
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

*/

/*
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

*/

size_t build_aprs_call(char* buffer)
{
  size_t number;
  for (number = 0; (number < (CALLSIGN_LENGTH - 1)) && (settings.s.my_callsign[number] > ' '); number ++)
  {
    buffer[number] = settings.s.my_callsign[number];
  }
  
  if ((number < (CALLSIGN_LENGTH - 1)) && (settings.s.my_callsign[7] > ' '))
  {
	  buffer[number++] = '-';
	  buffer[number++] = settings.s.my_callsign[7];  
  }
  
  return number;
}

// #pragma mark Buffered operations

/*
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
  uint16_t sum = rx_dstar_crc_data((unsigned char *) (data + TNC2_POSITION), length);
  vdisp_i2s(data + CRC_POSITION, 4, 16, 1, sum);
  data[DPRS_SIGN_LENGTH - 1] = ',';
}

int has_packet_data(void)
{
  return
    (validity1 > the_clock) &&
    (buffers[OPERATIONAL_BUFFER].length > 0);
}
*/


// #pragma mark GPS data handling

/*
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
*/

/*
void process_position_fix_data(const char** parameters)
{
  altitude = (*parameters[9] != 0) ? 
    ((atoi(parameters[9]) * 26444) >> 13) :
    UNDEFINED_ALTITUDE;

  long time = parse_time(parameters[1]);
  if (the_clock < time)
    rtclock_set_time(time);
}

*/

/*
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
  else if ((count >= 15) &&
      (memcmp(parameters[0], "GPGGA", 6) == 0) &&
      (*parameters[6] != '0') &&
      (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
    process_position_fix_data(parameters);
    validity2 = the_clock + DATA_VALIDITY_INTERVAL;
    xSemaphoreGive(lock);
    return;
  }
  else if ((count >= 15) &&
	  (memcmp(parameters[0], "$CRC", 4) == 0) &&
	  (*parameters[6] != '0') &&
	  (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE))
  {
	  process_position_fix_data(parameters);
	  validity2 = the_clock + DATA_VALIDITY_INTERVAL;
	  xSemaphoreGive(lock);
	  return;
  }
}
*/
// #pragma mark DV-A reporting
/*
uint8_t aprs_get_slow_data(uint8_t* data)
{
  int count = 0;
  
  
  if (xSemaphoreTake(lock, portTICK_RATE_MS * 2) == pdTRUE)
  {
    if (has_packet_data() == 0)
    {
      xSemaphoreGive(lock);
      return count;
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
*/
// #pragma mark APRS-IS reporting

void calculate_aprs_password(char* password)
{
  uint8_t hash[] = { 0x73, 0xe2 };

  for (size_t index = 0; (index < CALLSIGN_LENGTH) && (settings.s.my_callsign[index] > ' '); index ++)
    hash[index & 1] ^= settings.s.my_callsign[index];

  uint16_t code = ((hash[0] << 8) | hash[1]) & 0x7fff;
  vdisp_i2s(password, 5, 10, 1, code);
  
  /*
  // Patch to the original Routine of DL1BFF
  // in order TO HAVE leading zeros!
  for (int i=0; (i<5) && (password[i] == 0x20); ++i)
  {
	  password[i] = '0';
  }
  */
}


static uint8_t cached_aprs_ipv4addr[4] = { 0, 0, 0, 0 };


static int dns_cache_aprs(uint8_t * addr)
{
	int handle = dns2_req_A("aprs.dstar.su");
	
	if (handle < 0) // request was not accepted
	{
		return -1;
	}
	
	/*
	char buf[5];
	
	vdisp_i2s(buf,3,10,1,handle);
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 0, VDISP_FONT_6x8, 0, buf);
	
	static int counter1;
	static int counter2;
	
	counter1++;
	
	vdisp_i2s(buf,3,10,1,counter1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 8, VDISP_FONT_6x8, 0, buf);
	
	*/
	
	if (dns2_result_available(handle))
	{
		uint8_t * ipv4_bytes;
		int result = dns2_get_A_addr(handle, &ipv4_bytes);
		
		if (result >= 1)
		{
			memcpy(cached_aprs_ipv4addr, ipv4_bytes, 4);
		}
	}
	
	dns2_free(handle);
	
	if (memcmp(cached_aprs_ipv4addr, ipv4_zero_addr, sizeof ipv4_addr) == 0)
	{
		return -1;
	}
	
	memcpy(addr, cached_aprs_ipv4addr, 4);

	/*
	counter2++;
	vdisp_i2s(buf,3,10,1,counter2);
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 16, VDISP_FONT_6x8, 0, buf);
	*/
	
	return 0;
} 


void aprs_send_beacon(void)
{
	uint16_t udp_payload_size = 0;
	
	uint8_t aprs_call[8];
	uint8_t aprs_call_size = build_aprs_call((char *)aprs_call);
	
	// gruener Bereich (APRS-IS-Header)
	udp_payload_size += 5 + aprs_call_size + 6 + 5 + 26;
	
	// TCP2-Payload
	udp_payload_size += aprs_call_size + 22 + 6 + 32 + 9;
	
	uint8_t ipv4_aprs_addr[4];
	if (dns_cache_aprs(ipv4_aprs_addr) != 0)
	{
		return;  // DNS not in cache... try next time
	}
	
	eth_txmem_t * packet = udp4_get_packet_mem(udp_payload_size, aprs_local_port, APRS_SEND_ONLY_PORT, ipv4_aprs_addr);
	
	//memcpy(packet->data + 42, data, udp_payload_size*sizeof(uint8_t));
	uint8_t* p = packet->data + 42;
		
	memcpy(p, "user ", 5);
	p += 5;
		
	memcpy(p, aprs_call, aprs_call_size);	//build_aprs_call(data);
	p += aprs_call_size;
		
	memcpy(p, " pass ", 6);
	p += 6;
		
	calculate_aprs_password((char *) p);
	p += 5;
		
	memcpy(p, " vers UP4DAR " SWVER_STRING " \r\n", 16 + strlen(SWVER_STRING));
	p += 16 + strlen(SWVER_STRING);
		
	memcpy(p, aprs_call, aprs_call_size);	//build_aprs_call(data);
	p += aprs_call_size;
		
	//memcpy(p, ">APD401,TCPIP*:", 15);
	//p += 15;
		
	memcpy(p, ">APD401,qAR,DL2MRB-B:/", 22);
	p += 22;
		
	// HHMMSS = Zulu time
	rtclock_get_time((char *) p);
	p += 6;
		
	memcpy(p, "h4803.63ND01137.34E&UP4DAR based", 32);
	p += 32;
		
	if (hotspot_mode)
	{
		memcpy(p, " Hotspot ", 9);
		p += 9;
	}
	else if (repeater_mode)
	{
		memcpy(p, " Repeater", 9);
		p += 9;
	}
		
	udp4_calc_chksum_and_send(packet, ipv4_aprs_addr);
}


void aprs_send_user_report(uint8_t * gps_a_data, uint16_t gps_a_len)
{
	uint16_t udp_payload_size = 0;
	
	uint8_t aprs_call[8];
	uint8_t aprs_call_size = build_aprs_call((char *) aprs_call);
	
	// gruener Bereich (APRS-IS-Header)
	udp_payload_size += 5 + aprs_call_size + 6 + 5 + 26;
	
	// TNC2-Payload
	udp_payload_size += gps_a_len;

	uint8_t  ipv4_aprs_addr[4];
	if (dns_cache_aprs(ipv4_aprs_addr) != 0)
	{
		return;  // DNS not in cache... try next time
	}
	
	eth_txmem_t * packet = udp4_get_packet_mem(udp_payload_size, aprs_local_port, APRS_SEND_ONLY_PORT, ipv4_aprs_addr);
	
	uint8_t* p = packet->data + 42;
				
	memcpy(p, "user ", 5);
	p += 5;
		
	memcpy(p, aprs_call, aprs_call_size);	//build_aprs_call(data);
	p += aprs_call_size;
		
	memcpy(p, " pass ", 6);
	p += 6;
		
	calculate_aprs_password((char *) p);
	p += 5;
		
	memcpy(p, " vers UP4DAR " SWVER_STRING " \r\n", 16 + strlen(SWVER_STRING));
	p += 16 + strlen(SWVER_STRING);
		
	memcpy(p, gps_a_data, gps_a_len);
		
	/*
	// ==============================================================================
	memcpy(p, "DL3OCK-7", 8);
	p += 8;
		
	memcpy(p, ">API282,WIDE1-1,DSTAR*,qAR,DL2MRB-B:", 36);
	p += 36;
		
	// HHMMSSh = Zulu time
	memcpy(p, "/163400h4803.70N/01137.36E>027/000/Denis", 40);
	p += 40;
	// ==============================================================================
	
	*/
	
	udp4_calc_chksum_and_send(packet, ipv4_aprs_addr);
}

// #pragma mark Routine

/*
void aprs_reset(void)
{
  if (SETTING_CHAR(C_DPRS_ENABLED) != 0)
    send_aprs_udp_report();
  position = 0;
}


void aprs_handle_cache_event(void)
{
  //if (timer_get_timeout(TIMER_SLOT_APRS_BEACON) == 0)
  //  aprs_activate_beacon();
}

*/

void aprs_init(void)
{
  aprs_local_port = udp_get_new_srcport();
  
  /*
  lock = xSemaphoreCreateMutex();

  for (int index = 0; index < BUFFER_COUNT; index ++)
  {
    buffers[index].version = 0;
    buffers[index].length = 0;
  }
*/

  //dns_cache_set_slot(DNS_CACHE_SLOT_APRS, "aprs.dstar.su", aprs_handle_cache_event);
  
  
}