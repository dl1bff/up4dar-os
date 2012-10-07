/*

Copyright (C) 2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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



/*
 * dns.c
 *
 * Created: 13.09.2012 08:59:57
 *  Author: mdirska
 */ 

#include <asf.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"



#include "up_dstar/vdisp.h"

#include "dns.h"
#include "up_crypto/up_crypto.h"
#include "up_io/eth_txmem.h"
#include "ipneigh.h"
#include "ipv4.h"

#include "gcc_builtin.h"




#define DNS_STATE_READY		0
#define DNS_STATE_LOCKED	1
#define DNS_STATE_REQ_A		2
#define DNS_STATE_REQ_SRV	3
#define DNS_STATE_RESULT_OK		4
#define DNS_STATE_RESULT_ERR	5
#define DNS_STATE_RESULT_TIMEOUT  6

#define DNS_REQ_RETRY		4
#define DNS_REQ_TIMEOUT		100

static int dns_state = DNS_STATE_READY;
static int dns_timeout = 0;
static int dns_retry = 0;
static int dns_current_server = 0;

int dns_get_lock(void)
{
	if (dns_state == DNS_STATE_READY)
	{
		dns_state = DNS_STATE_LOCKED;
		return 1;
	}
	
	return 0;
}

void dns_release_lock(void)
{
	dns_state = DNS_STATE_READY;
}


#define DNS_REQNAME_SIZE  50

static int dns_reqname_len;
static char dns_reqname[DNS_REQNAME_SIZE];

static int dns_parse_domain(const char * name)
{
	const char * p = name;
	
	dns_reqname_len = 0;
	
	int state = 0;
	int len_pos = 0;
	int data_pos = 0;
	
	while (*p)
	{
		if ((*p) == '.')
		{
			state = 0;
		}
		else
		{
			if (state == 0)
			{
				state = 1;
				len_pos = data_pos;
				dns_reqname[len_pos] = 0;
				data_pos++;
			}
			dns_reqname[data_pos] = *p;
			data_pos++;
			dns_reqname[len_pos]++;
		}
		p++;
		
		if (data_pos >= (DNS_REQNAME_SIZE -1))
		{
			return -1; // name too long
		}
	}
	
	dns_reqname[data_pos] = 0;
	dns_reqname_len = data_pos+1;
	return 0;
}


int dns_req_A (const char * name)
{
	if (dns_state == DNS_STATE_LOCKED)
	{
		if (dns_parse_domain(name) != 0)
		{
			return -1;  // something wrong with the requested name
		}
		
		if (memcmp(ipv4_dns_pri, ipv4_zero_addr, sizeof ipv4_addr) == 0)
		{  // no primary DNS server
			return -1;
		}
		
		dns_retry = DNS_REQ_RETRY;
		dns_timeout = 0;
		dns_current_server = 0;
		dns_state = DNS_STATE_REQ_A;
		return 0; // OK
	}
	
	return -1;  // wrong state
}



int dns_result_available(void)
{
	if (
		(dns_state == DNS_STATE_RESULT_ERR) ||
		(dns_state == DNS_STATE_RESULT_OK) ||
		(dns_state == DNS_STATE_RESULT_TIMEOUT)  )
	{
		return 1;
	}
	
	return 0;
}


static uint8_t dns_result_ipv4_addr[4];

int dns_get_A_addr ( uint8_t * v4addr)
{
	if (dns_state != DNS_STATE_RESULT_OK)
	{
		return -1;
	}
	
	memcpy (v4addr, dns_result_ipv4_addr, sizeof ipv4_addr);
	return 0;
}


static int dns_udp_local_port = 0;
static int dns_req_id = 0;
static int dns_req_type = 0;

#define DNS_QTYPE_A		1
#define DNS_QTYPE_SRV	33

#define DNS_UDP_PORT  53

static int dns_send_req(int req_type)
{
	if (dns_udp_local_port == 0)
	{
		dns_udp_local_port = udp_get_new_srcport();
		
		udp_socket_ports[UDP_SOCKET_DNS] = dns_udp_local_port;
		
		dns_req_id = crypto_get_random_16bit();
	}
	
	int udp_size = 12 + dns_reqname_len + 4;  // dns header + domain + type + class
	
	uint8_t * dest_addr = (dns_current_server == 0) ? ipv4_dns_pri : ipv4_dns_sec;
	
	eth_txmem_t * packet =  udp4_get_packet_mem( udp_size, dns_udp_local_port,
		 DNS_UDP_PORT, dest_addr );
	
	
	if (packet == NULL)
	{
		return -1;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	
	d[0] = dns_req_id >> 8;
	d[1] = dns_req_id & 0xFF;
	
	d[2] = 0x01;  // request, recursion desired
	d[3] = 0x10;  // non-auth data acceptable
	
	d[4] = 0; d[5] = 1; // one question
	d[6] = 0; d[7] = 0; // no answer RR
	d[8] = 0; d[9] = 0; // no authority RR
	d[10] = 0; d[11] = 0; // no additional RR
	
	memcpy (d + 12, dns_reqname, dns_reqname_len); // requested domain
	
	d += dns_reqname_len + 12;
	
	dns_req_type = req_type; // remember, which type of request was sent
	
	d[0] = 0;  d[1] = req_type;  // QTYPE
	d[2] = 0;  d[3] = 1;		// QCLASS IN
	
	udp4_calc_chksum_and_send(packet, dest_addr );
	
	dns_timeout = DNS_REQ_TIMEOUT;  // wait for response
	
	return 0;
}




void dns_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	uint8_t * server_addr = (dns_current_server == 0) ? ipv4_dns_pri : ipv4_dns_sec;
	
	if (memcmp(ipv4_src_addr, server_addr, sizeof ipv4_addr) != 0)
	{
		// packet is not from the currently selected server
		return;
	}
	
	if ( data_len < 12 ) // packet too small
	{
		return;
	}
	
	int rx_dns_req_id = (data[0] << 8) | data[1];
	int rx_flags = (data[2] << 8) | data[3];
	int rx_question_num = (data[4] << 8) | data[5];
	int rx_answer_num = (data[6] << 8) | data[7];
//	int rx_authority_num = (data[8] << 8) | data[9];
//	int rx_additional_num = (data[10] << 8) | data[11];
	
	if ( rx_dns_req_id != dns_req_id)
		return; // req id not correct
		
	if ((rx_flags & 0x8000) != 0x8000) // is not a response
		return;
	
	if (rx_question_num != 1) // unexpected size of question section
		return;
	
	if (memcmp(data + 12, dns_reqname, dns_reqname_len) != 0)
		return;  // not the requested name
	
	const uint8_t * d = data + (12 + dns_reqname_len);
	
	if ((d[0] != 0) || (d[1] != dns_req_type) || (d[2] != 0) || (d[3] != 1))
		return;  // wrong type or class
		
	if ((dns_state != DNS_STATE_REQ_A) && (dns_state != DNS_STATE_REQ_SRV))
		return; // wrong state
		
	if ((rx_flags & 0x0F) != 0) // return code not 0
	{
		dns_state = DNS_STATE_RESULT_ERR;
		return;
	}
	
	if (rx_answer_num < 1) // at least one answer must be present
	{
		dns_state = DNS_STATE_RESULT_ERR;
		return;
	}
	
	d += 4; // skip question QTYPE and QCLASS
	// const uint8_t * d_end = data + data_len;
	
	
	
	// for now: expect first answer to be the requested RR
	// doesn't work with CNAME   -- FIXIT!!
		
	if ((d[0] == 0xC0) && (d[1] == 0x0C)) // domain compression
	{
		d += 2;
	}
	else
	{
		if (memcmp(d, dns_reqname, dns_reqname_len) != 0)
		{
			dns_state = DNS_STATE_RESULT_ERR;
			return;  // not the requested name 
		}
		
		// TODO: handle domain compression correctly  -- FIXIT!!
		
		d += dns_reqname_len;
	}
	
	
	// vdisp_prints_xy( 24, 48, VDISP_FONT_6x8, 1, "DNS" );
	
	
	
	if ((d[0] != 0) || (d[1] != dns_req_type) || (d[2] != 0) || (d[3] != 1)
	  || (d[8] != 0) || (d[9] != 4) )
	{
		dns_state = DNS_STATE_RESULT_ERR;
		return;  // wrong type or class, wrong length of address
	}		
	
	memcpy (dns_result_ipv4_addr, d + 10, sizeof ipv4_addr);
	
	// vdisp_prints_xy( 0, 48, VDISP_FONT_6x8, 1, "DNS1" );
	
	dns_state = DNS_STATE_RESULT_OK;
}



static void vDNSTask( void *pvParameters )
{
	
	while(1)
	{
		
		if (dns_timeout > 0)
		{
			dns_timeout --;
		}
		
		switch (dns_state)
		{
		case DNS_STATE_REQ_A:
			if (dns_timeout == 0)
			{
				if (dns_retry > 0)
				{
					dns_retry --;
				}
				
				if (dns_retry == 0)
				{
					if (dns_current_server == 0)
					{
						if (memcmp(ipv4_dns_sec, ipv4_zero_addr, sizeof ipv4_addr) == 0)
						{  // no secondary DNS server
							dns_state = DNS_STATE_RESULT_TIMEOUT;
						}
						else
						{
							dns_current_server = 1; // try the secondary server
							dns_udp_local_port = 0; // with new port number
							dns_retry = DNS_REQ_RETRY;
						}
					}
					else
					{
						dns_state = DNS_STATE_RESULT_TIMEOUT;
					}
				}
				else
				{
					if (dns_send_req(DNS_QTYPE_A) != 0)  // send failed
					{
						dns_state = DNS_STATE_RESULT_ERR;
					}
				}
			}
			break;
			
		case DNS_STATE_READY:
			dns_udp_local_port = 0;
			break;
		}
		
		
		vTaskDelay(20);
	}
}	


/*
		counter ++;
		
		vdisp_i2s( tmp_buf, 5, 10, 0, counter);
		
		vdisp_prints_xy( 0, 48, VDISP_FONT_6x8, 1, tmp_buf );
		*/

void dns_init(void)
{
	dns_udp_local_port = 0;
	
	xTaskCreate( vDNSTask, (signed char *) "DNS", 200, ( void * ) 0, ( tskIDLE_PRIORITY + 1 ), ( xTaskHandle * ) NULL );

}

