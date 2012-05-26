
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
 * gps.c
 *
 * Created: 26.05.2012 15:18:49
 *  Author: mdirska
 */ 



#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include "gcc_builtin.h"
#include "vdisp.h"

#include "up_io\serial.h"

#include "gps.h"


#define VDISP_GPS_LAYER 1

static xComPortHandle gpsSerialHandle;

#define MAXLINELEN 100
static char input_line[MAXLINELEN];

// static int  pos = 0;


static int hex_get (uint8_t d)
{
	if ((d >= '0') && (d <= '9'))
	{
		return d - '0';
	}

	if ((d >= 'A') && (d <= 'F'))
	{
		return d - 'A' + 10;
	}

	return -1;
}

static int gps_chksum_ok ( const char * s )
{
	uint8_t chksum = 0;
	uint8_t chk;

	while (*s)
	{
		if (*s == '*')
		{
			int k;


			k = hex_get (s[1]);

			if (k < 0)
				return 0;

			chk = k << 4;

			k = hex_get (s[2]);

			if (k < 0)
				return 0;

			chk = chk | k;

			if (s[3] != 0)
				return 0;

			if (chk == chksum)
				return 1;

			return 0;
		}

		chksum = chksum ^ (*s);

		s++;
	}

	return 0;
}


#define NO_SAT  -1

struct gps_satellites
{
	int sat_id; // NO_SAT if satellite info is not available
	int elevation;
	int azimuth;
	int snr;
};

typedef struct gps_satellites gps_satellites_t;

#define MAX_SATELLITES 16

static gps_satellites_t sats[MAX_SATELLITES];


static void gps_init_satlist(void)
{
	int i;
	
	for (i=0; i < MAX_SATELLITES; i++)
	{
		sats[i].sat_id = NO_SAT;
	}
}	

#define MAX_NMEA_PARAMS 25

static char * nmea_params[MAX_NMEA_PARAMS];


static int get_nmea_num(int param)
{
	int v = 0;
	char * s = nmea_params[param];
	
	while (*s)
	{
		v *= 10;
		v += (*s) & 0x0F;
		s++;
	}
	
	return v;
}


#define GPGSA_NUM_DATA 12

unsigned short gpgsa_data[GPGSA_NUM_DATA];

static void recv_gpgsa(void)
{
	int i;
	
	for (i=0; i < GPGSA_NUM_DATA; i++)
	{
		if ( (*(nmea_params[ 3 + i ])) == 0) // empty parameter
		{
			gpgsa_data[i] = NO_SAT;
		}
		else
		{
			gpgsa_data[i] = get_nmea_num( 3 + i );
		}			
	}
}


static char buf[10];

static void recv_gpgsv(int num_sats)
{
	int total = get_nmea_num(1);
	int msgnum = get_nmea_num(2);
	
	if (msgnum == 1) // first msg
	{
		gps_init_satlist();  // clear list
	}
	
	
	
	
	int i;
	
	for (i=0; i < num_sats; i++)
	{
		int s_ptr = i + ((msgnum - 1) << 2);
		
		if (s_ptr < MAX_SATELLITES)
		{
			if ( (*(nmea_params[ 4 + i * 4 ])) == 0) // empty parameter
			{
				sats[s_ptr].sat_id = NO_SAT;
			}
			else
			{
				sats[s_ptr].sat_id = get_nmea_num( 4 + i * 4 );
				sats[s_ptr].elevation = get_nmea_num( 5 + i * 4 );
				sats[s_ptr].azimuth = get_nmea_num( 6 + i * 4 );
				sats[s_ptr].snr = get_nmea_num( 7 + i * 4 );
			}
			
		}
	}

	if (msgnum == total) // last record, print it
	{
		vd_clear_rect (VDISP_GPS_LAYER, 0, 0, 128, 64);
		
		for (i=0; i < MAX_SATELLITES; i++)
		{
			if (sats[i].sat_id != NO_SAT)
			{
				int x = (i & 0x01) ? 64 : 0;
				int y = ( i >> 1 ) * 6;
				
				int j;
				
				int used_in_fix = 0;
				
				for (j=0; j < GPGSA_NUM_DATA; j++)
				{
					if (sats[i].sat_id == gpgsa_data[j])
					{
						used_in_fix = 1;
					}
				}
				
				vdisp_i2s(buf, 3, 10, 1, sats[i].sat_id);
				vd_prints_xy(VDISP_GPS_LAYER, x +  0, y, VDISP_FONT_4x6, used_in_fix, buf);
			
				vdisp_i2s(buf, 2, 10, 1, sats[i].elevation);
				vd_prints_xy(VDISP_GPS_LAYER, x + 16, y, VDISP_FONT_4x6, 0, buf);	
				
				vdisp_i2s(buf, 3, 10, 1, sats[i].azimuth);
				vd_prints_xy(VDISP_GPS_LAYER, x + 28, y, VDISP_FONT_4x6, 0, buf);
				
				vdisp_i2s(buf, 2, 10, 1, sats[i].snr);
				vd_prints_xy(VDISP_GPS_LAYER, x + 44, y, VDISP_FONT_4x6, 0, buf);	
			}
		}
	}
	
	
	
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(1));
	vd_prints_xy(VDISP_GPS_LAYER, 0, 58, VDISP_FONT_4x6, 0, buf);
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(2));
	vd_prints_xy(VDISP_GPS_LAYER, 12, 58, VDISP_FONT_4x6, 0, buf);
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(3));
	vd_prints_xy(VDISP_GPS_LAYER, 24, 58, VDISP_FONT_4x6, 0, buf);
}


static void gps_parse_nmea(void)
{
	int ptr = 1;
	int num_params = 0;
	
	int state = 0;
	
	while (input_line[ptr] != '*') // 
	{
		if (state == 0)
		{
			state = 1;
				
			nmea_params[num_params] = input_line + ptr;
			num_params ++;
			if (num_params >= MAX_NMEA_PARAMS)
			{
				num_params --;
			}
		}
		
		if (input_line[ptr] == ',')
		{
			input_line[ptr] = 0; // end of string
			
			state = 0;
		}
		
		ptr ++;
	}
	
	input_line[ptr] = 0; // end of string
	
	if (state == 0) // last parameter is empty
	{
		nmea_params[num_params] = input_line + ptr;
		num_params ++;
	}
	
	/*
	vd_prints_xy(VDISP_GPS_LAYER, 0, pos, VDISP_FONT_4x6, 0, nmea_params[0]);
	
	char buf[4];
	vdisp_i2s(buf, 3, 10, 0, num_params);
	vd_prints_xy(VDISP_GPS_LAYER, 30, pos, VDISP_FONT_4x6, 0, buf);
	
	pos += 6;
	
	if (pos >= 63)
	{
		pos = 0;
	}
	
	*/
	
	if (memcmp(nmea_params[0], "GPGSV", 6) == 0)
	{
		
		
		if (num_params == 20)
		{
			recv_gpgsv(4);
		}
		else if (num_params == 16)
		{
			recv_gpgsv(3);
		}
		else if (num_params == 12)
		{
			recv_gpgsv(2);
		}
		else if (num_params == 8)
		{
			recv_gpgsv(1);
		}
		
		/*
		{
			vdisp_i2s(buf, 3, 10, 0, num_params);
			vd_prints_xy(VDISP_GPS_LAYER, 64, 58, VDISP_FONT_4x6, 0, buf);
		}
		*/
	}
	else if (memcmp(nmea_params[0], "GPGSA", 6) == 0)
	{
		if (num_params == 18)
		{
			recv_gpgsa();
		}			
	}		
}


static void vGPSTask( void *pvParameters )
{
	int input_ptr = 0;
	
	for (;;)
	{
		if (xSerialGetChar(gpsSerialHandle, (signed char *) input_line + input_ptr, 1000) == pdTRUE)  // one second timeout
		{
			
			if (input_line[input_ptr] < 32) // probably CR or LF
			{
				input_line[input_ptr] = 0; // end of string
				
				if ((input_line[0] == '$') && (gps_chksum_ok(input_line + 1) != 0) )
				{
					gps_parse_nmea();
				}
				input_ptr = 0;
			}
			else
			{
				input_ptr ++;
				if (input_ptr >= MAXLINELEN)
				{
					input_ptr --; // don't go over the end of the buffer
				}
			}
		}
		else // timeout
		{
			input_ptr = 0;
		}
		
	}
	
}


void gps_init(void)
{
	
	gps_init_satlist();
	
	vd_clear_rect (VDISP_GPS_LAYER, 0, 0, 128, 64);
	
	gpsSerialHandle = xSerialPortInitMinimal( 0, 4800, 10 );
	
	if (gpsSerialHandle < 0)
	{
		// TODO: error handling
		
		return;
	}
	
	xTaskCreate( vGPSTask, (signed char *) "GPS", configMINIMAL_STACK_SIZE, ( void * ) 0,
		 ( tskIDLE_PRIORITY + 1 ), ( xTaskHandle * ) NULL );
	
}