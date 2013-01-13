
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

#include "fixpoint_math.h"

#include "gps.h"
#include "settings.h"




static xComPortHandle gpsSerialHandle;

#define MAXLINELEN 100
static char input_line[MAXLINELEN];


#define FIXDATA_MAXLEN 13
static char fix_data[4][FIXDATA_MAXLEN];


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

static unsigned short gpgsa_data[GPGSA_NUM_DATA];

static int gps_fix = 0;
static int gpgga_fix_info = 0;
static int gprmc_fix_mode = 0;
static int gprmc_status = 0;

static void recv_gpgsa(void)
{
	int i;
	
	gps_fix = get_nmea_num( 2 );
	
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
	
	
	
	int j;
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
		
		
		if (gps_fix >= 2)
		{
			buf[0] = 0x30 + gps_fix;
			buf[1] = 0;
			
			vd_prints_xy(VDISP_GPS_LAYER, 62, 46, VDISP_FONT_4x6, 0, buf);
			vd_prints_xy(VDISP_GPS_LAYER, 66, 46, VDISP_FONT_4x6, 0, "D FIX");
		}
		else
		{
			vd_prints_xy(VDISP_GPS_LAYER, 62, 46, VDISP_FONT_4x6, 0, "NO FIX");
		}
		
		/*
		if (gpgga_fix_info > 0)
		{
			buf[0] = 0x30 + gpgga_fix_info;
			buf[1] = 0;
			
			vd_prints_xy(VDISP_GPS_LAYER, 0, 56, VDISP_FONT_6x8, 0, buf);
		}
		else
		{
			vd_prints_xy(VDISP_GPS_LAYER, 0, 56, VDISP_FONT_6x8, 0, "-");
		}
		
		if (gprmc_fix_mode > 0)
		{
			buf[0] = gprmc_fix_mode;
			buf[1] = 0;
			
			vd_prints_xy(VDISP_GPS_LAYER, 6, 0, VDISP_FONT_6x8, 0, buf);
		}
		else
		{
			vd_prints_xy(VDISP_GPS_LAYER, 6, 0, VDISP_FONT_6x8, 0, "-");
		}
		
		if (gprmc_status > 0)
		{
			buf[0] = gprmc_status;
			buf[1] = 0;
			
			vd_prints_xy(VDISP_GPS_LAYER, 12, 0, VDISP_FONT_6x8, 0, buf);
		}
		else
		{
			vd_prints_xy(VDISP_GPS_LAYER, 12, 0, VDISP_FONT_6x8, 0, "-");
		}
		*/
		
		/*
		for (i=0; i < 360; i+= 45)
		{
			vd_set_pixel(VDISP_GPS_LAYER, 32 + (fixpoint_sin(i) / 700),
				32 + (fixpoint_cos(i) / 700), 0, 1, 1);
		}
		*/
		
		for (i=0; i < 360; i+= 10)
		{
			vd_set_pixel(VDISP_GPS_LAYER, 32 + (fixpoint_sin(i) / 357),
				32 + (fixpoint_cos(i) / 357), 0, 1, 1);
		}			
				
		vd_prints_xy(VDISP_GPS_LAYER, 0, 28, VDISP_FONT_6x8, 0, "W");
		vd_prints_xy(VDISP_GPS_LAYER, 58, 28, VDISP_FONT_6x8, 0, "E");
		vd_prints_xy(VDISP_GPS_LAYER, 29, 0, VDISP_FONT_6x8, 0, "N");
		vd_prints_xy(VDISP_GPS_LAYER, 29, 56, VDISP_FONT_6x8, 0, "S");
		
		
		for (i=0; i < MAX_SATELLITES; i++)
		{
			if (sats[i].sat_id != NO_SAT)
			{
				int x = 120 - (i % 6) * 10;
				int y = (i / 6) * 21;
				
				
				
				int used_in_fix = 0;
				
				for (j=0; j < GPGSA_NUM_DATA; j++)
				{
					if (sats[i].sat_id == gpgsa_data[j])
					{
						used_in_fix = 1;
					}
				}
				
				if (sats[i].elevation > 0)
				{
					int xx = 28 + fixpoint_cos(sats[i].elevation) * fixpoint_sin(sats[i].azimuth) / 3570000;
					int yy = 29 - fixpoint_cos(sats[i].elevation) * fixpoint_cos(sats[i].azimuth) / 3570000;
					
					vdisp_i2s(buf, 2, 10, 1, sats[i].sat_id);
					vd_prints_xy(VDISP_GPS_LAYER, xx, yy+1, VDISP_FONT_4x6, used_in_fix, buf);
					
					for (j=0; j < 8; j++)
					{
						vd_set_pixel(VDISP_GPS_LAYER, xx + j, yy, 0, used_in_fix, 1);
					}
					
					for (j=0; j < 7; j++)
					{
						vd_set_pixel(VDISP_GPS_LAYER, xx + 8, yy + j, 0, used_in_fix, 1);
					}
				}
				
				vdisp_i2s(buf, 2, 10, 1, sats[i].sat_id);
				vd_prints_xy(VDISP_GPS_LAYER, x, y + 14, VDISP_FONT_4x6, 0, buf);
				
				int v = fixpoint_sin(sats[i].snr) / 833;
				
				vd_set_pixel(VDISP_GPS_LAYER, x+1, y + 12, 0, 0x7F, 7);
				
				for (j=1; j < v; j++)
				{
					vd_set_pixel(VDISP_GPS_LAYER, x+1, y + 12 - j, 0,
						used_in_fix ? 0x7F : 0x41 , 7);
				}
				
				vd_set_pixel(VDISP_GPS_LAYER, x+1, y + 12 - v, 0, 0x7F, 7);
				
				/*
				vdisp_i2s(buf, 3, 10, 1, sats[i].sat_id);
				vd_prints_xy(VDISP_GPS_LAYER, x +  0, y, VDISP_FONT_4x6, used_in_fix, buf);
			
				vdisp_i2s(buf, 2, 10, 1, sats[i].elevation);
				vd_prints_xy(VDISP_GPS_LAYER, x + 16, y, VDISP_FONT_4x6, 0, buf);	
				
				vdisp_i2s(buf, 3, 10, 1, sats[i].azimuth);
				vd_prints_xy(VDISP_GPS_LAYER, x + 28, y, VDISP_FONT_4x6, 0, buf);
				
				vdisp_i2s(buf, 2, 10, 1, sats[i].snr);
				vd_prints_xy(VDISP_GPS_LAYER, x + 44, y, VDISP_FONT_4x6, 0, buf);	
				
				*/
			}
		}
		
		
	}
	
	
	for (i=0; i < 2; i++)
	{
		vd_prints_xy(VDISP_GPS_LAYER, 56, 52 + (i*6), VDISP_FONT_4x6, 0, fix_data[i]);
	}	
	
	
	/*
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(1));
	vd_prints_xy(VDISP_GPS_LAYER, 0, 58, VDISP_FONT_4x6, 0, buf);
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(2));
	vd_prints_xy(VDISP_GPS_LAYER, 12, 58, VDISP_FONT_4x6, 0, buf);
	vdisp_i2s(buf, 2, 10, 1, get_nmea_num(3));
	vd_prints_xy(VDISP_GPS_LAYER, 24, 58, VDISP_FONT_4x6, 0, buf);
	*/
}



static char rcvd_chksum1;
static char rcvd_chksum2;


static int slow_data_state = 0;
static const char * slow_data_ptr = 0;
/*
static const char * gps_id = "DL1BFF  ,BN  *5B             \r\n";
*/
char gps_id[32];
// ---------------------------12345678901234567890123456789
// gps_id is always 29 bytes long

#define DSTAR_GPS_MAXLINELEN 100

static char gprmc_data[DSTAR_GPS_MAXLINELEN];
static char gpgga_data[DSTAR_GPS_MAXLINELEN];


static void copy_dstar_gps_line (char * dest, int num_fields)
{
	int ptr = 1;
	dest[0] = '$';
	
	int i;
	for (i=0; i < num_fields; i++)
	{
		const char * p = nmea_params[i];
		
		while (*p)
		{
			dest[ptr] = *p;
			ptr ++;
			p++;
			
			if (ptr >= (DSTAR_GPS_MAXLINELEN - 5))  // line too long!
			{
				dest[DSTAR_GPS_MAXLINELEN - 5] = 0;
				return;
			}
		}
		 
		dest[ptr] = ',';
		ptr ++;
			
		if (ptr >= (DSTAR_GPS_MAXLINELEN - 5))  // line too long!
		{
			dest[DSTAR_GPS_MAXLINELEN - 5] = 0;
			return;
		}
	}
	
	// the last comma is modified to an asterisk with chksum afterwards
	
	dest[ptr - 1] = '*';
	dest[ptr] = rcvd_chksum1;
	dest[ptr + 1] = rcvd_chksum2;
	dest[ptr + 2] = 13; // CR
	dest[ptr + 3] = 10; // LF
	dest[ptr + 4] = 0; // end of string
	
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
	
	rcvd_chksum1 = input_line[ptr + 1];
	rcvd_chksum2 = input_line[ptr + 2];
	
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
	else if (memcmp(nmea_params[0], "GPRMC", 6) == 0)
	{
		if (num_params == 13)
		{
			gprmc_fix_mode = nmea_params[12][0];
			gprmc_status = nmea_params[2][0];
		
		/*
			int i;	
			for (i = 0; i < 3; i++)
			{
				memcpy(fix_data[i], nmea_params[1 + (i*2)], FIXDATA_MAXLEN);
				fix_data[i][FIXDATA_MAXLEN - 1] = 0;
			}
			*/
			
			if (slow_data_state == 0)
			{
				copy_dstar_gps_line(gprmc_data, 13);
				if (gpgga_data[0] != 0) // gpgga_data is not empty
				{
					slow_data_ptr = gpgga_data;
					slow_data_state = 1;
				}
			}				
		}			
	}
	else if (memcmp(nmea_params[0], "GPGGA", 6) == 0)
	{
		if (num_params == 15)
		{
			// memcpy(fix_data[0], nmea_params[1], FIXDATA_MAXLEN);
			// fix_data[0][FIXDATA_MAXLEN - 1] = 0;
			memcpy(fix_data[0], nmea_params[2], FIXDATA_MAXLEN);
			fix_data[1][FIXDATA_MAXLEN - 1] = 0;
			memcpy(fix_data[1], nmea_params[4], FIXDATA_MAXLEN);
			fix_data[2][FIXDATA_MAXLEN - 1] = 0;
				
			gpgga_fix_info = get_nmea_num(6);
			
			if (slow_data_state == 0)
			{							
				copy_dstar_gps_line(gpgga_data, 15);
				if (gprmc_data[0] != 0) // gprmc_data is not empty
				{
					slow_data_ptr = gpgga_data;
					slow_data_state = 1;
				}
			}			
		}			
	}
	/*
	else if (memcmp(nmea_params[0], "GPZDA", 6) == 0)
	{
		if (num_params == 7)
		{
			memcpy(fix_data[3], nmea_params[1], FIXDATA_MAXLEN);
			fix_data[3][FIXDATA_MAXLEN - 1] = 0;
		}	
	} */		
}


// static int chcount = 0;

static int copy_slow_data( int * bytes_copied, uint8_t * slow_data)
{
	int count = 0;
	
	while ((*slow_data_ptr) && (count < 5))
	{
		slow_data[count] = (*slow_data_ptr);
		
		/* vdisp_printc_xy((chcount % 32) * 4, 8 + (chcount / 32) * 6,VDISP_FONT_4x6, 0, slow_data[count]);
		chcount ++;
		if (chcount >= (9 * 32))
		{
			chcount = 0;
		} */
		
		
		count ++;
		slow_data_ptr ++;
	}
	
	*bytes_copied = count;
	
	return ((*slow_data_ptr) == 0) ? 1 : 0;  // return 1 if at end of the string
}


void gps_reset_slow_data(void)
{
	gprmc_data[0] = 0;  // empty string, to be filled by NMEA data
	gpgga_data[0] = 0;
	slow_data_state = 0;
}

static const char * const dprs_symbol[6] =
  {
	  "HS  ", // Jogger
	  "MV  ", // Car
	  "BN  ", // House
	  "PY  ", // Boat
	  "LB  ", // Bicycle
	  "LV  "  // Van
  };
  

static int hex_char (int i)
{
	if (i < 10)
	{
		return 0x30 | i;
	}
	
	return 'A' + i - 10;
}


int gps_get_slow_data(uint8_t * slow_data)
{
	int ret_val = 0;
	
	slow_data[0] = 0x66; // NOP data
	slow_data[1] = 0x66;
	
	if (SETTING_CHAR(C_DPRS_ENABLED) != 1)
	{
		return 0;
	}
	
	// vdisp_printc_xy(0,0,VDISP_FONT_6x8, 0, 0x30 + slow_data_state);
	
	switch (slow_data_state)
	{
		case 0:  // no data to send
			break;
			
		case 1: // send GPGGA 
			if (slow_data_ptr == 0)
			{
				slow_data_state = 4; // end
			}
			else
			{
				if (copy_slow_data(&ret_val, slow_data) != 0) // if all bytes are copied
				{
					slow_data_state = 2;
					slow_data_ptr = gprmc_data;
				}
			}
			break;
		
		case 2: // send GPRMC
			if (slow_data_ptr == 0)
			{
				slow_data_state = 4; // end
			}
			else
			{
				if (copy_slow_data(&ret_val, slow_data) != 0) // if all bytes are copied
				{
					memcpy (gps_id, settings.s.my_callsign, CALLSIGN_LENGTH);
					gps_id[8] = ',';
					memcpy (gps_id + 9, dprs_symbol[SETTING_CHAR(C_DPRS_SYMBOL)], 4);
					
					memset (gps_id + 13, ' ', 16); // spaces
					gps_id[29] = 13;
					gps_id[30] = 10;
					gps_id[31] = 0;
					
					memcpy (gps_id + 13, settings.s.dprs_msg, 13);
					
					int i = 26;
					while (i > 13)
					{
						if (gps_id[i-1] != ' ') // space
							break;
						
						i--;
					}
					
					int chksum = 0;
					
					int j;
					
					for (j=0; j < i; j++)
					{
						chksum = chksum ^ gps_id[j];
					}
					
					gps_id[i] = '*';
					gps_id[i+1] = hex_char(chksum >> 4);
					gps_id[i+2] = hex_char(chksum & 0x0F);
					
					slow_data_state = 3;
					slow_data_ptr = gps_id;
				}
			}
			break;
			
		case 3: // send gps_id
			if (slow_data_ptr == 0)
			{
				slow_data_state = 4; // end
			}
			else
			{
				if (copy_slow_data(&ret_val, slow_data) != 0) // if all bytes are copied
				{
					slow_data_state = 4;
					slow_data_ptr = 0;
				}
			}
			break;
			
		case 4:
			gps_reset_slow_data();
			break;
	}
	
	return ret_val;
}

static void vGPSTask( void *pvParameters )
{
	int input_ptr = 0;
	
/*
#define FIXINTERVAL "$PMTK300,1000,0,0,0,0*1C\r\n"	
#define WAAS_SET1 "$PMTK313,1*2E\r\n"
#define WAAS_SET2 "$PMTK301,2*2E\r\n"
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, FIXINTERVAL);
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, FIXINTERVAL);
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, WAAS_SET1);
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, WAAS_SET2);
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, WAAS_SET1);

	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, "$PMTK104*37\r\n");   // full reset
	
	*/

	
	
/*	vSerialPutString(gpsSerialHandle, "$PMTK313,0*2F\r\n"); // disable SBAS
	
	vTaskDelay(1000);
	
	vSerialPutString(gpsSerialHandle, "$PMTK301,0*2C\r\n"); // disable WAAS
	
	vTaskDelay(1000); */
	
	// vSerialPutString(gpsSerialHandle, "$PMTK101*32\r\n");   // hot reset
	
	//vSerialPutString(gpsSerialHandle, "$PMTK104*37\r\n");   // full reset
	
	// vTaskDelay(1000);
	// vSerialPutString(gpsSerialHandle, "$PMTK313,0*2F\r\n"); // disable SBAS satellite search
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK301,0*2C\r\n"); // disable WAAS
	// vTaskDelay(1000);
	// vSerialPutString(gpsSerialHandle, "$PMTK313,1*2E\r\n"); // enable SBAS satellite search
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK301,2*2E\r\n"); // enable WAAS
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK397,0*23\r\n"); // disable speed threshold
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK220,2000*1C\r\n");   // data every 2 seconds
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK300,2000,0,0,0,0*1F\r\n");   // data every 2 seconds
	// vTaskDelay(1000); 
	// vSerialPutString(gpsSerialHandle, "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0*29\r\n"); 
	
	/*
	 vTaskDelay(1000);
	 vSerialPutString(gpsSerialHandle, "$PMTK104*37\r\n");   // cold reset
	 vTaskDelay(1000); 
	 vSerialPutString(gpsSerialHandle, "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0*29\r\n");
	*/
	 vTaskDelay(1000);
	 // vSerialPutString(gpsSerialHandle, "$PMTK102*31\r\n");   // warm reset
	 vSerialPutString(gpsSerialHandle, "$PMTK301,0*2C\r\n"); // disable WAAS
	 //vSerialPutString(gpsSerialHandle, "$PMTK301,2*2E\r\n"); // enable WAAS
	 // vSerialPutString(gpsSerialHandle, "$PMTK313,0*2F\r\n"); // disable SBAS satellite search
	 // vSerialPutString(gpsSerialHandle, "$PMTK397,0*23\r\n"); // disable speed threshold
	 
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


void gps_init(int comPortHandle)
{
	
	gps_init_satlist();
	gps_reset_slow_data();
	
	vd_clear_rect (VDISP_GPS_LAYER, 0, 0, 128, 64);
	
	// gpsSerialHandle = xSerialPortInitMinimal( 0, 4800, 10 );
	
	gpsSerialHandle = comPortHandle;
	
	if (gpsSerialHandle < 0)
	{
		// TODO: error handling
		
		return;
	}
	
	xTaskCreate( vGPSTask, (signed char *) "GPS", configMINIMAL_STACK_SIZE, ( void * ) 0,
		 ( tskIDLE_PRIORITY + 1 ), ( xTaskHandle * ) NULL );
	
}