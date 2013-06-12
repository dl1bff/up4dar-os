/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)
Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
#include "dhcp.h"
#include "up_dstar/settings.h"

// #include "dns_cache.h"

#define NTP_PORT                 123
#define NTP_PACKET_LENGTH        48

#define LOCAL_PORT               udp_socket_ports[UDP_SOCKET_NTP]
#define ETHERNET_PAYLOAD_OFFSET  42


static char ntp_state;
static int ntp_timer;
static char ntp_retry_counter;

#define NTP_STATE_IDLE				0
#define NTP_STATE_DNS_REQ_SENT		1
#define NTP_STATE_NTP_REQ_SENT		10


#define TIMER_SECONDS(a)	((a)*2)


void ntp_handle_packet(const uint8_t* data, int length, const uint8_t* address)
{
	if (length >= NTP_PACKET_LENGTH)
	{
		if (data[1] == 0) // KISS OF DEATH packet (see RFC4330)
		{
			ntp_timer = TIMER_SECONDS(3600); // try again in one hour
		}
		else
		{		  
			uint32_t time = (data[40] << 24) | (data[41] << 16) | (data[42] << 8) | data[43];
			rtclock_set_time(time);
			
			ntp_timer = TIMER_SECONDS(4 * 3600); // next update in 4 hours
		}			
		
		ntp_state = NTP_STATE_IDLE;
		LOCAL_PORT = 0; // close socket
	}  
  
}


static unsigned char ntp_server_address[4];

static void query_time(void)
{
  eth_txmem_t* packet = udp4_get_packet_mem(NTP_PACKET_LENGTH, LOCAL_PORT, NTP_PORT, ntp_server_address);

  if (packet == NULL)
    return;

  uint8_t* data = packet->data + ETHERNET_PAYLOAD_OFFSET;
  memset(data, 0, NTP_PACKET_LENGTH);
  data[0] = (4 << 3) | 3; // LI=0, VN=4 (version 4), Mode=3 (client)    (see RFC4330)

  udp4_calc_chksum_and_send(packet, ntp_server_address);
}

/*
void update_time()
{
  uint8_t address[4];
  if (!dns_cache_get_address(DNS_CACHE_SLOT_NTP, address))
    return;


  LOCAL_PORT = udp_get_new_srcport();
  for (size_t index = 0; index < SHOT_COUNT; index ++)
    query_time(address);
}
*/




void ntp_service(void)
{
	if (ntp_timer > 0)
	{
		ntp_timer --;
		return;
	}
	
	switch(ntp_state)
	{
		case NTP_STATE_IDLE:
			
			if (dhcp_is_ready() && SETTING_BOOL(B_ENABLE_NTP))
			{
				
				if (memcmp(ipv4_ntp, ipv4_zero_addr, sizeof(ipv4_ntp)) == 0)
				{	// no DHCP NTP address or no fixed NTP address
				
					ntp_state = NTP_STATE_DNS_REQ_SENT;
					ntp_timer = TIMER_SECONDS(5);
					// send DNS request for 0.up4dar.pool.ntp.org
				}
				else
				{
					ntp_state = NTP_STATE_NTP_REQ_SENT;
					ntp_timer = TIMER_SECONDS(2);
					ntp_retry_counter = 4;
					memcpy(ntp_server_address, ipv4_ntp, sizeof(ntp_server_address));
					LOCAL_PORT = udp_get_new_srcport();
					query_time();
				}
			}
			else
			{
				ntp_timer = TIMER_SECONDS(3); // wait for DHCP to get ready
			}				
			break;
			
		case NTP_STATE_DNS_REQ_SENT:
			// TODO: implement DNS request
			ntp_state = NTP_STATE_IDLE;
			ntp_timer = TIMER_SECONDS(120);
			break;
			
		case NTP_STATE_NTP_REQ_SENT:
			if (ntp_retry_counter > 0)
			{
				ntp_retry_counter --;
				
				ntp_timer = TIMER_SECONDS(2);
				query_time();
			}
			else
			{  // no answer, try again in 2 minutes
				ntp_state = NTP_STATE_IDLE;
				ntp_timer = TIMER_SECONDS(120);
				LOCAL_PORT = 0; // close socket
			}
			break;
			
	}
}

void ntp_init()
{
  // dns_cache_set_slot(DNS_CACHE_SLOT_NTP, "0.up4dar.pool.ntp.org", update_time);
  
  ntp_state = NTP_STATE_IDLE;
  ntp_timer	= TIMER_SECONDS(5);  // start a request in 5 seconds
}
