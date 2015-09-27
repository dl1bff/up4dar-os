/*

Copyright (C) 2015   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * dns2.c
 *
 * Created: 19.01.2014 17:26:45
 *  Author: mdirska
 */ 


#include <asf.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"



#include "up_dstar/vdisp.h"

#include "dns2.h"
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


/*
static int dns_udp_local_port = 0;
static int dns_req_id = 0;
static int dns_req_type = 0;



static int dns_state = DNS_STATE_READY;
static int dns_timeout = 0;
static int dns_retry = 0;
static int dns_current_server = 0;



static int dns_reqname_len;
static char dns_reqname[DNS_REQNAME_SIZE];

static uint8_t dns_result_ipv4_addr[4];

*/

#define DNS_REQNAME_SIZE  60

struct dns2_cache
{
	uint16_t udp_local_port;
	uint16_t req_id;
	uint16_t ttl;
	uint8_t req_type;
	uint8_t timeout;
	uint8_t retry;
	uint8_t primary_server; 
	uint8_t state;
	
	uint8_t link;
	
	uint8_t reqname_len;
	uint8_t reqname[DNS_REQNAME_SIZE];
	
	uint8_t result_data_len;
	uint8_t result_data[DNS_REQNAME_SIZE];
};


#define DNS_NUMBER_OF_ENTRIES  10
static struct dns2_cache * dc;



/*
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

*/


static int dns2_parse_domain(int handle, const char * name)
{
	const uint8_t * p = (uint8_t *) name;
	struct dns2_cache * cur = dc + handle;
	
	cur->reqname_len = 0;
	
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
				cur->reqname[len_pos] = 0;
				data_pos++;
			}
			cur->reqname[data_pos] = *p;
			data_pos++;
			cur->reqname[len_pos]++;
		}
		p++;
		
		if (data_pos >= (DNS_REQNAME_SIZE -1))
		{
			return -1; // name too long
		}
	}
	
	cur->reqname[data_pos] = 0;
	cur->reqname_len = data_pos+1;
	return 0;
}

static const uint8_t tst002addr[4] = { 192, 168, 1, 210 };
	


static int dns2_name_cmp(const uint8_t * name1, const uint8_t * packet, int packet_len, const uint8_t * name2)
{
	int ptr1 = 0;
	int ptr2 = 0;
	
	int max_pointer_count = 5; // max pointers in name
	
	while(1)
	{
		if ((name1[ptr1] & 0xC0) == 0xC0) // pointer to a different location in the packet
		{
			if (packet == NULL)  // no pointers possible without the packet pointer
				break;
			
			max_pointer_count --;
			
			if (max_pointer_count <= 0)
				break;  // too many pointers in name (possible loop)
				
			ptr1 = ((name1[ptr1] & 0x3F) << 8) | name1[ptr1 + 1];
			
			if (ptr1 >= packet_len) // new pointer outside of packet
				break;
			
			name1 = packet;
			
			continue;
		}
		
		if (name1[ptr1] != name2[ptr2]) // the length of the label is not the same -> name not equal
			break;
			
		if (name1[ptr1] == 0) // end of labels -> every label was equal -> match!
			return 0;
			
#define TOLOWER(c)  ((((c)>='A') && ((c)<='Z')) ? ((c) | 0x20) : (c))

		for (int i=1; i <= name1[ptr1]; i++)
		{
			if (TOLOWER(name1[ptr1 + i]) != TOLOWER(name2[ptr2 + i]))
				return -1; // character in label is not equal
		}
		
		ptr1 += name1[ptr1] + 1;
		ptr2 += name2[ptr2] + 1;
		
		if (ptr2 > 256) // much too big, something is wrong, stop here
			break;
	}
	
	return -1;
}


int dns2_req_A (const char * name)
{
	
	int i;
	int handle = -1;
	int link_zero_slot = -1;
	int ttl_min = 65535;
	
	for (i=0; i < DNS_NUMBER_OF_ENTRIES; i++)
	{
		struct dns2_cache * cur = dc + i;
		
		if (cur->reqname_len == 0)
		{
			handle = i; // free slot
			break;
		}
		
		if (cur->link == 0) // slot currently not in use
		{
			if (cur->ttl < ttl_min)
			{
				ttl_min = cur->ttl;
				link_zero_slot = i; // unused slot with the lowest TTL
			}
		}
	}
	
	if (handle < 0)
	{
		if (link_zero_slot < 0) // could not find slot that is not in use
			return -1; // no free slots
	
		handle = link_zero_slot; // use entry with smallest ttl left	
	}
	
	if (dns2_parse_domain(handle, name) != 0)
	{
		return -1;  // something wrong with the requested name
	}
	
	struct dns2_cache * cur = dc + handle;
	
	for (i=0; i < DNS_NUMBER_OF_ENTRIES; i++)
	{		
		if (i == handle) // skip the newly created entry
			continue;
		
		struct dns2_cache * cached = dc + i;
		
		if (cached->reqname_len > 0) // entry is not empty
		{
			if ((cached->ttl > 0) && (dns2_name_cmp(cur->reqname, 0, 0, cached->reqname) == 0))
			{  // same name requested and some TTL left
				cached->link ++;
				cur->reqname_len = 0; // free the newly created handle
				return i; // return the cached handle 
			}
		}
	}
	
	if (strcmp(name, "tst002.reflector.up4dar.de") == 0)
	{
		memcpy (cur->result_data, tst002addr, sizeof tst002addr);
		cur->result_data_len = sizeof tst002addr;
		cur->state = DNS_STATE_RESULT_OK;
		cur->ttl = 5;
		cur->link = 1;
		return handle;
	}
		
	if (memcmp(ipv4_dns_pri, ipv4_zero_addr, sizeof ipv4_addr) == 0)
	{  // no primary DNS server
		return -1;
	}
		
	cur->retry = DNS_REQ_RETRY;
	cur->timeout = 0;
	cur->primary_server = 1; // begin with primary server
	cur->link = 1; // one client links to this
	cur->state = DNS_STATE_REQ_A;
	cur->udp_local_port = 0; // select new port
	
	vd_prints_xy(VDISP_DEBUG_LAYER, 0, 48, VDISP_FONT_4x6, 0, "AREQ");
	vd_prints_xy(VDISP_DEBUG_LAYER, 20, 48, VDISP_FONT_4x6, 0, name);
	vd_clear_rect(VDISP_DEBUG_LAYER, 0, 54, 80, 6);
	return handle; // OK
}



int dns2_result_available(int handle)
{
	struct dns2_cache * cur = dc + handle;
	
	if (
		(cur->state == DNS_STATE_RESULT_ERR) ||
		(cur->state == DNS_STATE_RESULT_OK) ||
		(cur->state == DNS_STATE_RESULT_TIMEOUT)  )
	{
		return 1;
	}
	
	return 0;
}




int dns2_get_A_addr ( int handle, uint8_t ** v4addr)
{
	struct dns2_cache * cur = dc + handle;
	
	if (cur->state != DNS_STATE_RESULT_OK)
	{
		return -1;
	}
	
	*v4addr = cur->result_data;
	
	return cur->result_data_len / 4; // number of IPv4 addresses
}


/*
static int dns_udp_local_port = 0;
static int dns_req_id = 0;
static int dns_req_type = 0;

*/

#define DNS_QTYPE_A		1
#define DNS_QTYPE_CNAME	5
#define DNS_QTYPE_SRV	33

#define DNS_UDP_PORT  53

static int dns2_send_req( int handle, uint8_t req_type)
{
	struct dns2_cache * cur = dc + handle;
	
	if (cur->udp_local_port == 0)
	{
		cur->udp_local_port = udp_get_new_srcport();
		
		// udp_socket_ports[UDP_SOCKET_DNS] = dns_udp_local_port;
		
		cur->req_id = crypto_get_random_16bit();
	}
	
	int udp_size = 12 + cur->reqname_len + 4;  // dns header + domain + type + class
	
	uint8_t * dest_addr = (cur->primary_server != 0) ? ipv4_dns_pri : ipv4_dns_sec;
	
	eth_txmem_t * packet =  udp4_get_packet_mem( udp_size, cur->udp_local_port,
		 DNS_UDP_PORT, dest_addr );
	
	
	if (packet == NULL)
	{
		return -1;
	}
	
	uint8_t * d = packet->data + 42; // skip ip+udp header
	
	
	d[0] = cur->req_id >> 8;
	d[1] = cur->req_id & 0xFF;
	
	d[2] = 0x01;  // request, recursion desired
	d[3] = 0x00;  // Z = 0
	
	d[4] = 0; d[5] = 1; // one question
	d[6] = 0; d[7] = 0; // no answer RR
	d[8] = 0; d[9] = 0; // no authority RR
	d[10] = 0; d[11] = 0; // no additional RR
	
	memcpy (d + 12, cur->reqname, cur->reqname_len); // requested domain
	
	d += cur->reqname_len + 12;
	
	cur->req_type = req_type; // remember, which type of request was sent
	
	d[0] = 0;  d[1] = req_type;  // QTYPE
	d[2] = 0;  d[3] = 1;		// QCLASS IN
	
	udp4_calc_chksum_and_send(packet, dest_addr );
	
	cur->timeout = DNS_REQ_TIMEOUT;  // wait for response
	
	return 0;
}


int dns2_find_dns_port( uint16_t port )
{
	int i;
	
	for (i=0; i < DNS_NUMBER_OF_ENTRIES; i++)
	{
		struct dns2_cache * cur = dc + i;
		
		if ((cur->reqname_len > 0) && 
			(cur->state == DNS_STATE_REQ_A) &&
			(cur->udp_local_port == port))
		{
			return i;
		}
	}
	
	return -1; // not found
}


void dns2_free(int handle)
{
	struct dns2_cache * cur = dc + handle;
	if (cur->link > 0)
	{
		cur->link --;
	}
}


static int dns2_name_len(const uint8_t * name)
{
	int ptr = 0;
	
	while (name[ptr] != 0) // name ends with 0 length byte..
	{
		if ((name[ptr] & 0xC0) == 0xC0) // ... or pointer
		{
			ptr ++; // pointer has two bytes
			break;
		}
		
		int len = (name[ptr] & 0x3F);
		
		ptr += 1 + len; // go to next label
		
		if (ptr > 256)  // name is too long, stop here
			break;
	}
	
	return ptr + 1;  // +1 for the last byte (0)
}

#define DNS_TTL_COUNTER_TIMER	3000
#define DNS_WAIT_TIME_PER_SLOT  3


void dns2_input_packet ( int handle, const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr)
{
	struct dns2_cache * cur = dc + handle;
	uint8_t * server_addr = (cur->primary_server != 0) ? ipv4_dns_pri : ipv4_dns_sec;
	
	
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
	
	if ( rx_dns_req_id != cur->req_id)
		return; // req id not correct
		
	if ((rx_flags & 0x8000) != 0x8000) // is not a response
		return;
	
	if (rx_question_num != 1) // unexpected size of question section
		return;
	
	if (dns2_name_cmp(data + 12, data, data_len, cur->reqname) != 0)
		return;  // not the requested name
	
	const uint8_t * d = data + (12 + dns2_name_len(data + 12));
	
	if ((d[0] != 0) || (d[1] != cur->req_type) || (d[2] != 0) || (d[3] != 1))
		return;  // wrong type or class
		
	// if ((cur->state != DNS_STATE_REQ_A) && (cur->state != DNS_STATE_REQ_SRV))
	if (cur->state != DNS_STATE_REQ_A)
		return; // wrong state
		
	cur->ttl = 0; // if an error occurs now: cleanup this cache entry quickly
		
	if ((rx_flags & 0x0F) != 0) // return code not 0
	{
		cur->state = DNS_STATE_RESULT_ERR;
		return;
	}
	
	if (rx_answer_num < 1) // at least one answer must be present
	{
		cur->state = DNS_STATE_RESULT_ERR;
		return;
	}
	
	d += 4; // skip question QTYPE and QCLASS
	// const uint8_t * d_end = data + data_len;
	
	
	
	// for now: expect first answer to be the requested RR
	// doesn't work with CNAME   -- FIXIT!!
	
	if (dns2_name_cmp(d, data, data_len, cur->reqname) != 0)
	{
		cur->state = DNS_STATE_RESULT_ERR;
		return;  // not the requested name
	}
	
	d += dns2_name_len(d);
	
	// vdisp_prints_xy( 24, 48, VDISP_FONT_6x8, 1, "DNS" );
	
	
	
	if ((d[0] != 0) || (d[1] != DNS_QTYPE_A) || (d[2] != 0) || (d[3] != 1) /* CLASS IN */
	  || (d[8] != 0) || (d[9] != 4) /* size of IPv4 address */ )
	{
		cur->state = DNS_STATE_RESULT_ERR;
		return;  // wrong type or class, wrong length of address
	}
	
	uint32_t ttl = (d[4] << 24) | (d[5] << 16) | (d[6] << 8) | d[7];
	 
	if (ttl < 20)
	{
		ttl = 20; // at least 20 seconds
	}
	
	if (ttl > 14400)  // 4 hours
	{
		ttl = 14400;
	}
	
	cur->ttl = ttl / (DNS_TTL_COUNTER_TIMER / 1000); // current ttl counter has tick rate of 3 seconds
	
	memcpy (cur->result_data, d + 10, sizeof ipv4_addr);
	cur->result_data_len = sizeof ipv4_addr;
	
	// vdisp_prints_xy( 0, 48, VDISP_FONT_6x8, 1, "DNS1" );
	
	cur->state = DNS_STATE_RESULT_OK;
	
}



static void vDNSTask( void *pvParameters )
{
	uint16_t ttl_check_counter = 0;
	
	while(1)
	{
		int i;
		
		if (ttl_check_counter > 0)
		{
			ttl_check_counter --;
		}
		
		char entry_counter[2];
		entry_counter[0] = 0x30;
		entry_counter[1] = 0;
		
		for (i=0; i < DNS_NUMBER_OF_ENTRIES; i++)
		{
			vTaskDelay(DNS_WAIT_TIME_PER_SLOT); // give time to other threads
			
			struct dns2_cache * cur = dc + i;
			
			if (cur->reqname_len == 0) // skip inactive slots
				continue; 
			
			entry_counter[0] ++;
			
			if (cur->timeout > 0)
			{
				cur->timeout --;
			}
		
			switch (cur->state)
			{
			case DNS_STATE_REQ_A:
				if (cur->timeout == 0)
				{
					if (cur->retry > 0)
					{
						cur->retry --;
					}
				
					if (cur->retry == 0)
					{
						if (cur->primary_server != 0)
						{
							if (memcmp(ipv4_dns_sec, ipv4_zero_addr, sizeof ipv4_addr) == 0)
							{  // no secondary DNS server
								cur->state = DNS_STATE_RESULT_TIMEOUT;
							}
							else
							{
								cur->primary_server = 0; // try the secondary server
								cur->udp_local_port = 0; // with new port number
								cur->retry = DNS_REQ_RETRY;
							}
						}
						else
						{
							cur->state = DNS_STATE_RESULT_TIMEOUT;
						}
					}
					else
					{
						if (dns2_send_req(i, DNS_QTYPE_A) != 0)  // send failed
						{
							cur->state = DNS_STATE_RESULT_ERR;
						}
					}
				}
				break;
			
			}
			
			if (ttl_check_counter == 0)
			{
				if (cur->ttl > 0)
				{
					cur->ttl --;
				}
				
				if ((cur->ttl == 0) && (cur->link == 0)) // TTL expired and entry unused
				{
					cur->reqname_len = 0; // free entry
				}
			}
			
			
		} // for
		
		if (ttl_check_counter == 0)
		{
			ttl_check_counter = (DNS_TTL_COUNTER_TIMER / DNS_WAIT_TIME_PER_SLOT) / DNS_NUMBER_OF_ENTRIES; // decrease TTL every 3 seconds
		}
		
		// vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 54, VDISP_FONT_4x6, 0, entry_counter);
		
	} // while(1)
}	




void dns2_init(void)
{
	
	dc = (struct dns2_cache *) pvPortMalloc ( DNS_NUMBER_OF_ENTRIES * (sizeof (struct dns2_cache)));
	
	memset(dc, 0, DNS_NUMBER_OF_ENTRIES * (sizeof (struct dns2_cache))); // clear cache memory
	
	xTaskCreate( vDNSTask, (signed char *) "DNS2", 400, ( void * ) 0, ( tskIDLE_PRIORITY + 1 ), ( xTaskHandle * ) NULL );

}

