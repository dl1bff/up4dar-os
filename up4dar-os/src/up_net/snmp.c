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
 * snmp.c
 *
 * Created: 03.05.2012 11:04:43
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include "up_dstar/vdisp.h"
#include "snmp.h"

#include "snmp_data.h"

#include "gcc_builtin.h"


#define BER_INTEGER			0x02
#define BER_OCTETSTRING		0x04
#define BER_NULL			0x05
#define BER_OID				0x06

#define BER_SEQUENCE		0x30

#define BER_SNMP_GET		0xA0
#define BER_SNMP_GETNEXT	0xA1
#define BER_SNMP_RESPONSE	0xA2
#define BER_SNMP_SET		0xA3




static char tmp_callsign[8] = "DL1BFF  ";


int snmp_encode_int ( int32_t value, uint8_t * res, int * res_len, int maxlen )
{
	uint32_t mask = 0xFFFFFF80;
	int len = 1;
	uint32_t v = (uint32_t) value;
	
	while (len < 4)
	{
		if ((v & mask) == ((value < 0) ? mask : 0))
		   break;
		len ++;
		mask = mask << 8;
	}
	
	if (len > maxlen)
		return 1;
		
	*res_len = len;
	
	int i;
	
	for (i=0; i < len; i++)
	{
		res[len - 1 - i] = v & 0x00FF;
		v = v >> 8;
	}
	
	return 0;
}


static int set_callsign (int32_t arg, const uint8_t * req, int req_len)
{
	if (req_len > (sizeof tmp_callsign))
		return 1;
		
	memset(tmp_callsign, ' ', (sizeof tmp_callsign));
	memcpy(tmp_callsign, req, req_len);
	return 0;
}

static int get_callsign (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	memcpy(res, tmp_callsign, sizeof tmp_callsign);
	*res_len = sizeof tmp_callsign;
	return 0;
}


static int test_return_string (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	memcpy(res, "TESTxTEST.TEST.", 15);
	res[4] = 0x30 | arg;
	*res_len = 15;
	return 0;
}

static int test_return_integer (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	return snmp_encode_int( arg, res, res_len, maxlen );
}


// static const unsigned char * id_data = (unsigned char *) 0x80800204;

static int get_cpu_id (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	if (maxlen < 15)
		return 1;
		
	memcpy(res, (unsigned char *) 0x80800204, 15);
	*res_len = 15;
	return 0;
}


static const struct snmp_table_struct {
	const char * oid;
	char valueType;
	int (* getter) (int32_t arg, uint8_t * res, int * res_len, int maxlen);
	int (* setter) (int32_t arg, const uint8_t * req, int req_len);
	int32_t arg;
} snmp_table[] =
{  // this table must be sorted (oid string)
	{ "110",	BER_OCTETSTRING,	test_return_string,		0			, 1},
	{ "120",	BER_OCTETSTRING,	get_cpu_id,		0			, 0},
		
		
	{ "14111",	BER_INTEGER,		test_return_integer,		0		, 1},
	{ "14112",	BER_INTEGER,		test_return_integer,		0		, 2},
	{ "14113",	BER_INTEGER,		test_return_integer,		0		, 3},
		
	{ "14121",	BER_OCTETSTRING,	test_return_string,			0		, 1},
	{ "14122",	BER_OCTETSTRING,	test_return_string,			0		, 5},
	{ "14123",	BER_OCTETSTRING,	test_return_string,			0		, 6},
	
		
	{ "14131",	BER_INTEGER,		test_return_integer,		0		, 10},
	{ "14132",	BER_INTEGER,		test_return_integer,		0		, 10},
	{ "14133",	BER_INTEGER,		test_return_integer,		0		, -10},
		
	{ "14141",	BER_INTEGER,		test_return_integer,		0		, 100},
	{ "14142",	BER_INTEGER,		test_return_integer,		0		, 200},
	{ "14143",	BER_INTEGER,		test_return_integer,		0		, 439462500},
		
	{ "210",	BER_OCTETSTRING,	snmp_get_phy_sysinfo,		0			, 0},
	{ "220",	BER_OCTETSTRING,	snmp_get_phy_cpuid,		0			, 0},
	{ "230", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 1},
	{ "240", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 2},
	{ "250", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 3},
	{ "260", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 4},
	{ "270", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 5},
	{ "280", 	BER_INTEGER,	snmp_get_phy_sysparam,	snmp_set_phy_sysparam	, 6},
			
	{ "30",    BER_OCTETSTRING,	get_callsign,   set_callsign,		 0},
	{ "40", 	BER_INTEGER,	snmp_get_voltage,			0		, 0}
	
};	


 // OID root  1.3.6.1.3.5573.1
static const uint8_t up4dar_oid[] = { 0x2b, 0x06, 0x01, 0x03, 0xab, 0x45, 0x01 };
	

static int parse_len;
static const uint8_t * parse_ptr;

static uint8_t tmp_oid[15];

#define MAX_RESULT_LEN	70
#define MAX_REQ_PARAM		5

static struct snmp_result_struct {
	int oid_index;
	uint8_t result_buf[MAX_RESULT_LEN];
	int result_len;
} snmp_result[MAX_REQ_PARAM];



static int get_length( int * ret_len_len )
{
	int len = parse_ptr[1];
	int len_len = 1;
	
	if (len > 0x7F)
	{
		switch(parse_ptr[1] & 0x7F)
		{
		case 1:
			len = parse_ptr[2];
			len_len = 2;
			break;
		case 2:
			len = (parse_ptr[2] << 8) | parse_ptr[3];
			len_len = 3;
			break;
		default:  // length field much to long
			len = -1; // error code
			break;
		}
	}
	
	if (ret_len_len != 0)
	{
		*ret_len_len = len_len;
	}
	
	return len;
}


static int check_type (int datatype)
{
	
	if (datatype != parse_ptr[0])  // type not correct
		return 1; 
		
	int len_len;
	
	int len = get_length( & len_len); 
	
	if (len < 0)   // invalid length field
		return 1;
		
	if ((len + 1 + len_len) > parse_len)  // length incorrect
		return 1;
		
	return 0;
}

static int get_integer(int * result)
{
	if (BER_INTEGER != parse_ptr[0])  // type not correct
		return 1; 
		
	int len_len;
	
	int len = get_length( & len_len); 
	
	if (len < 0)   // invalid length field
		return 1;
		
	if ((len + 1 + len_len) > parse_len)  // length incorrect
		return 1;	
	
	*result = 0;
	
	int i;
	
	for (i=0; i < len; i++)
	{
		*result = ((*result) << 8) | parse_ptr[ 1 + i + len_len  ];
	}
	
	return 0;
}

static int get_octetstring(int valType, uint8_t * res, int * res_len, int maxlen)
{
	if (parse_ptr[0] != valType)  //  type not correct
		return 1;
		
	int len_len;
	
	int len = get_length( & len_len); 
	
	if (len < 0)   // invalid length field
		return 1;
		
	if ((len + 1 + len_len) > parse_len)  // length incorrect
		return 1;	
		
	if (len > maxlen)
		return 1;  // too long
		
	memcpy(res, parse_ptr + 1 + len_len, len);
	
	*res_len = len;
	
	return 0;
}


static void enter_sequence(void)
{
	int len_len;
	
	int len = get_length( & len_len); 
	
	if (len < 0)   // invalid length field
		return;
		
	parse_len -= 1 + len_len;
	parse_ptr += 1 + len_len;
}

static void ber_skip(void)
{
	int len_len;
	
	int len = get_length( & len_len); 
	
	if (len < 0)   // invalid length field
		return;
		
	parse_len -= 1 + len_len + len;
	parse_ptr += 1 + len_len + len;
}




static int get_oid_len(int oid_index)
{
	const char * s = snmp_table[oid_index].oid;
	
	int len = 0;
	
	while ((*s) != 0)
	{
		s++;
		len ++;
	}		
	
	return len + (sizeof up4dar_oid);
}


static void oid_copy (uint8_t * dest, int oid_index)
{
	uint8_t * d = dest;
	memcpy (d, up4dar_oid, sizeof up4dar_oid);
	d += sizeof up4dar_oid;
	
	const char * s = snmp_table[oid_index].oid;
	while ((*s) != 0)
	{
		if (((*s) >= '0') && ((*s) <= '9'))
		{
			(*d) = (*s) & 0x0F;  // numbers
		}
		else
		{
			(*d) = ((*s) & 0x1F) + 9;  // letters (A = 10, B = 11, ...)
		}
		
		s++;
		d++;
	}	
}

static uint8_t tmp2_oid[15];

static int find_oid (const uint8_t * oid, int oid_len, int is_getnext)
{
	if (oid_len <= (sizeof up4dar_oid))
	{
		return  is_getnext ? 0 : -1;  // oid not found
			// return first entry on GETNEXT, error otherwise
	}
	
	if (memcmp(oid, up4dar_oid, sizeof up4dar_oid) != 0)
	{
		return  is_getnext ? 0 : -1;  // prefix not correct
			// return first entry on GETNEXT, error otherwise
	}
	
	const uint8_t * o = oid + (sizeof up4dar_oid);
	int o_len = oid_len - (sizeof up4dar_oid);
	
	int i;
	
	for (i=0; i < ((sizeof snmp_table) / (sizeof (struct snmp_table_struct))); i++)
	{
		memset(tmp2_oid, 0, sizeof tmp2_oid);
		
		int k_len = get_oid_len(i) - (sizeof up4dar_oid);
		oid_copy (tmp2_oid, i);
	
		int cmp = memcmp(tmp2_oid + (sizeof up4dar_oid), o, o_len);
		
		if (cmp == 0)
		{
			if (is_getnext && (o_len == k_len))  // exact match
			{
				i++;  // then take the next one
				if (i >= ((sizeof snmp_table) / (sizeof (struct snmp_table_struct))))
				{
					i = -1; // that was the last one, return err after the last one
				}
			}
			return i;
		}
		else if (cmp > 0)
		{
			return is_getnext ? i : -1; // not found in sorted list
		}
		
	}
	
	return  -1;  // prefix not found
			// return first entry on GETNEXT, error otherwise
}


static uint8_t community_string[30];
static int community_string_len;

static int req_type_pos;
static int error_byte_pos;
static int error_index_byte_pos;


static int error_msg (int err_code, int err_idx, uint8_t * response, const uint8_t * req, int req_len)
{
	memcpy(response, req, req_len);
	response[req_type_pos] = BER_SNMP_RESPONSE;
	response[error_byte_pos] = err_code;
	response[error_index_byte_pos] = err_idx;
	return req_len;
}	

// static char buf[10];

int snmp_process_request( const uint8_t * req, int req_len, uint8_t * response )
{
	parse_len = req_len;
	parse_ptr = req;
	
	int value;
	
	
	if (check_type(BER_SEQUENCE) != 0)  return 0; // first element must be SEQUENCE
	
	enter_sequence();
	
	if (get_integer(&value) != 0)  return 0; // snmp version
	
	if (value != 0)  return 0; // 0 == SMNPv1
	
	ber_skip();
	
	if (get_octetstring(BER_OCTETSTRING, community_string,
	      &community_string_len, sizeof community_string) != 0)  return 0; // snmp community
	
	ber_skip();
	
	req_type_pos = (parse_ptr - req); // position of the byte where the request type is
	int request_type = parse_ptr[0]; // SNMP request type
	
	if (
		(request_type != BER_SNMP_GET) &&
		(request_type != BER_SNMP_GETNEXT) &&
		(request_type != BER_SNMP_SET)
			) return 0;  // wrong type
			
	if (check_type(request_type) != 0)  return 0;
	
	enter_sequence();
	
	int request_id;
	
	if (get_integer(&request_id) != 0)  return 0;
	
	ber_skip();
	
	if (check_type(BER_INTEGER) != 0)  return 0; // error
	
	int tmp_len_len;
	
	int tmp_len = get_length(& tmp_len_len);
	
	error_byte_pos = (parse_ptr - req) + tmp_len + tmp_len_len; // position of
													// the byte where the error code is
	
	ber_skip();
	
	if (check_type(BER_INTEGER) != 0)  return 0; // error index
	
	tmp_len = get_length(& tmp_len_len);
	
	error_index_byte_pos = (parse_ptr - req) + tmp_len + tmp_len_len; // position of
									// the byte where the error index code is
	
	ber_skip();
	
	if (check_type(BER_SEQUENCE) != 0)  return 0; // varbind list
	
	enter_sequence();
	
	int param_pos = 0;
	
	while (parse_len > 0)
	{
		if (check_type(BER_SEQUENCE) != 0)  return 0; // varbind
		
		enter_sequence();
		
		int oid_len;
		
		if (get_octetstring(BER_OID, tmp_oid, & oid_len, sizeof tmp_oid) != 0)  return 0; // oid
		
		int oid_index = find_oid(tmp_oid, oid_len, (request_type == BER_SNMP_GETNEXT) ? 1 : 0);
		
		if (oid_index < 0)  return error_msg(0x02, param_pos + 1, response, req, req_len);  // not found
		
		ber_skip();
		
		
		if (request_type == BER_SNMP_SET)
		{	
			if ( parse_ptr[0] != snmp_table[oid_index].valueType )
			  return error_msg(0x03, param_pos + 1, response, req, req_len); // wrong data type
			  
			if ( snmp_table[oid_index].setter == 0 )
			  return error_msg(0x04, param_pos + 1, response, req, req_len); // read-only parameter
			  
			int data_len = get_length(& tmp_len);
			
			if ( snmp_table[oid_index].setter(snmp_table[oid_index].arg, parse_ptr + 1 + tmp_len, data_len) != 0)
			  return error_msg(0x05, param_pos + 1, response, req, req_len); // general error
		}
		
		snmp_result[param_pos].oid_index = oid_index;
		
		if ( snmp_table[oid_index].getter == 0)
			return error_msg(0x05, param_pos + 1, response, req, req_len); // general error
		
		if ( snmp_table[oid_index].getter(snmp_table[oid_index].arg, 
				snmp_result[param_pos].result_buf , &snmp_result[param_pos].result_len, MAX_RESULT_LEN ) != 0)
			  return error_msg(0x05, param_pos + 1, response, req, req_len); // general error
		
		
		ber_skip();
		param_pos ++;
		
		if (param_pos > MAX_REQ_PARAM)  // more parameters than we have memory for
			return error_msg(0x01, param_pos, response, req, req_len); // "response message too large"
	}
	
	
	tmp_len = 0;
	
	int i;
	
	for (i=0; i < param_pos; i++)
	{
		tmp_len += get_oid_len(snmp_result[i].oid_index);
		tmp_len += snmp_result[i].result_len;
		tmp_len += 4; // type and length for value (2 byte length field)
		tmp_len += 2; // type and length for oid
		tmp_len += 4; // varbind sequence (2 byte length field)
	}
	
	tmp_len += 4; // varbind list sequence (2 byte length field)
	tmp_len += 3; // error index
	tmp_len += 3; // error
	tmp_len += 6; // request_id ( 4 byte integer )
	tmp_len += 4; // getResponse (2 byte length field)
	tmp_len += 2 + community_string_len; // communitystring, length < 127
	tmp_len += 3; // snmp_version
	
	value = tmp_len + 4; // the UDP packet length
	
	uint8_t * resp = response;
	
	resp[0] = BER_SEQUENCE;
	resp[1] = 0x82;
	resp[2] = (tmp_len >> 8) & 0xFF;
	resp[3] = tmp_len & 0xFF;
	
	resp += 4;
	
	resp[0] = BER_INTEGER;
	resp[1] = 1;
	resp[2] = 0;  // version = SNMPv1
	
	resp += 3;
	
	resp[0] = BER_OCTETSTRING;
	resp[1] = community_string_len;
	
	memcpy(resp + 2, community_string, community_string_len);
	
	resp += 2 + community_string_len;
	
	tmp_len -= 3 + 2 + community_string_len + 4;
	
	resp[0] = BER_SNMP_RESPONSE;
	resp[1] = 0x82;
	resp[2] = (tmp_len >> 8) & 0xFF;
	resp[3] = tmp_len & 0xFF;
	
	resp += 4;
	
	resp[0] = BER_INTEGER;
	resp[1] = 4;
	resp[2] = (request_id >> 24) & 0xFF;
	resp[3] = (request_id >> 16) & 0xFF;
	resp[4] = (request_id >> 8) & 0xFF;
	resp[5] = request_id & 0xFF;
	
	resp += 6;
	
	resp[0] = BER_INTEGER;
	resp[1] = 1;
	resp[2] = 0;  // error = 0
	
	resp += 3;
	
	resp[0] = BER_INTEGER;
	resp[1] = 1;
	resp[2] = 0;  // error index = 0
	
	resp += 3;
	
	tmp_len -= 6 + 3 + 3 + 4;
	
	resp[0] = BER_SEQUENCE;
	resp[1] = 0x82;
	resp[2] = (tmp_len >> 8) & 0xFF;
	resp[3] = tmp_len & 0xFF;
	
	resp += 4;
	
	for (i=0; i < param_pos; i++)
	{
		int oid_len = get_oid_len(snmp_result[i].oid_index);
		
		int t = snmp_result[i].result_len;
		t += 4; // type and length for value (2 byte length field)
		t += oid_len; // oid length
		t += 2; // type and length for oid
		
		
		resp[0] = BER_SEQUENCE;
		resp[1] = 0x82;
		resp[2] = (t >> 8) & 0xFF;
		resp[3] = t & 0xFF;
		
		resp += 4;
		
		resp[0] = BER_OID;
		resp[1] = oid_len;
		
		oid_copy(resp + 2, snmp_result[i].oid_index);
		
		resp += 2 + oid_len;
		
		resp[0] = snmp_table[snmp_result[i].oid_index].valueType;
		resp[1] = 0x82;
		resp[2] = (snmp_result[i].result_len >> 8) & 0xFF;
		resp[3] = snmp_result[i].result_len & 0xFF;
		
		memcpy(resp + 4, snmp_result[i].result_buf, snmp_result[i].result_len);
		
		resp += 4 + snmp_result[i].result_len;
		
	}
	
	
	// vdisp_i2s(buf, 8, 10, 0, tmp_len);
	// vdisp_prints_xy( 0, 56, VDISP_FONT_6x8, 0, buf );
	
	return value;
}