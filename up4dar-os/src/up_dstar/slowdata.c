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
 * slowdata.c
 *
 * Created: 11.06.2015 14:44:30
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "slowdata.h"
#include "vdisp.h"
#include "rx_dstar_crc_header.h"
#include "gcc_builtin.h"
#include "aprs.h"



#define SLOWDATA_FIFO_BYTES  15

static unsigned char slowDataFIFO[SLOWDATA_FIFO_BYTES];
static short slowDataFIFOinPtr;
static short slowDataFIFOoutPtr;

#define SLOWDATA_GPSA_BUFLEN  100

static char * slowDataGPSA;
static short slowDataGPSA_ptr;
static short slowDataGPSA_state;

static void slowdata_add_byte(unsigned char d)
{
	short tmp_ptr = slowDataFIFOinPtr;
	
	tmp_ptr ++;
	
	if (tmp_ptr >= SLOWDATA_FIFO_BYTES)
	{
		tmp_ptr = 0;
	}
	
	if (tmp_ptr == slowDataFIFOoutPtr) // buffer is full
	{
		return;
	}
	
	slowDataFIFO[slowDataFIFOinPtr] = d;
	slowDataFIFOinPtr = tmp_ptr;
}


void slowdata_data_input( unsigned char * data, unsigned char len )
{
	int i;
	
	for (i=0; i < len; i++)
	{
		slowdata_add_byte(data[i]);
	}
}


static char crc[5];

void slowdata_analyze_stream(void)
{
	while (slowDataFIFOoutPtr != slowDataFIFOinPtr)
	{
		char d = (char) slowDataFIFO[slowDataFIFOoutPtr];
		
		slowDataFIFOoutPtr++;
		
		if (slowDataFIFOoutPtr >= SLOWDATA_FIFO_BYTES)
		{
			slowDataFIFOoutPtr = 0;
		}
		
		switch (slowDataGPSA_state)
		{
			case 0:
				if (d == '$')
				{
					slowDataGPSA_state = 1;
				}
				break;
			case 1:
				if (d == '$')
				{
					slowDataGPSA_state = 2;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 2:
				if (d == 'C')
				{
					slowDataGPSA_state = 3;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 3:
				if (d == 'R')
				{
					slowDataGPSA_state = 4;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 4:
				if (d == 'C')
				{
					slowDataGPSA_state = 5;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 5:
				if (d > ' ')
				{
					slowDataGPSA_state = 6;
					crc[0] = d;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 6:
				if (d > ' ')
				{
					slowDataGPSA_state = 7;
					crc[1] = d;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 7:
				if (d > ' ')
				{
					slowDataGPSA_state = 8;
					crc[2] = d;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 8:
				if (d > ' ')
				{
					slowDataGPSA_state = 9;
					crc[3] = d;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 9:
				if (d == ',')
				{
					slowDataGPSA_state = 10;
					slowDataGPSA_ptr = 0;
				}
				else
				{
					slowDataGPSA_state = 0;
				}
				break;
			case 10:
				if (d >= ' ') // printable character
				{
					if (slowDataGPSA_ptr >= SLOWDATA_GPSA_BUFLEN) // buffer full, line too long
					{
						slowDataGPSA_state = 0;
					}
					
					slowDataGPSA[slowDataGPSA_ptr] = d;
					slowDataGPSA_ptr ++;
				}
				else  //  everything else including CR
				{
					slowDataGPSA_state = 0;
					
					if (slowDataGPSA_ptr < SLOWDATA_GPSA_BUFLEN) // buffer not full
					{
						slowDataGPSA[slowDataGPSA_ptr] = d; // put last character at the end
						slowDataGPSA_ptr ++;
						
						// crc[4] = 0;
						// vd_prints_xy(VDISP_NODEINFO_LAYER, 80, 16, VDISP_FONT_6x8, 0, crc);
						
						unsigned short sum = rx_dstar_crc_data((unsigned char *) slowDataGPSA, slowDataGPSA_ptr);
						char buf[5];
						vdisp_i2s(buf, 4, 16, 1, sum);
						
						// buf[4] = 0;
						// vd_prints_xy(VDISP_NODEINFO_LAYER, 80, 24, VDISP_FONT_6x8, 0, buf);
						
						if (memcmp(crc, buf, 4) == 0)
						{
							aprs_send_user_report((unsigned char *) slowDataGPSA, slowDataGPSA_ptr -1); // send GPS-A data without trailing CR
						}
					}
				}
				break;
		} // switch
	} // while
}


void slowdataInit(void)
{
	
	slowDataGPSA = (char *) pvPortMalloc (SLOWDATA_GPSA_BUFLEN);
	
	slowDataGPSA_ptr = 0;
	slowDataGPSA_state = 0;
	
	slowDataFIFOinPtr = 0;
	slowDataFIFOoutPtr = 0;
	
}