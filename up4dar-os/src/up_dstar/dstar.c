/*

Copyright (C) 2011,2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * dstar.c
 *
 * Created: 03.04.2011 11:22:04
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "dstar.h"

#include <string.h>

#include "rx_dstar_crc_header.h"

#include "vdisp.h"
#include "rtclock.h"
#include "phycomm.h"

#include "ambe.h"

#include "up_io/eth_txmem.h"

#include "up_net/snmp_data.h"
#include "up_net/snmp.h"
#include "settings.h"


static xQueueHandle dstarQueue;
static xQueueHandle snmpReqQueue;

static U32 qTimeout = 0;

static struct dstarPacket dp;

static int diagram_displayed;
static int repeater_msg;

// static int hub_min = 3000;
// static int hub_max = 0;

#define PPM_BUFSIZE 10
int ppm_buf[PPM_BUFSIZE];
int ppm_ptr;
int ppm_display_active;

static void mkPrintableString (char * data, int len)
{
	int i;
	for (i=0; i < len; i++)
	{
		if ((data[i] < 32) || (data[i] > 126))
		{
			data[i] = ' ';
		}
	}
	data[len] = 0;
}


static char buf[40];


static void printHeader( int ypos, unsigned char crc_result, const unsigned char * header_data )
{
	
	memcpy(buf, header_data + 19, 8);
	mkPrintableString(buf,8);
	
	
	if ((crc_result == 0) && (ypos == 5))
	{
		if ((header_data[0] & 0x07) == 0)
		{
			// vdisp_clear_rect (0, 48, 128, 16);
			vdisp_prints_xy(0, 48, VDISP_FONT_6x8, 0, "UR:");
			vdisp_prints_xy(18, 48, VDISP_FONT_6x8, 0, buf);
			
			rtclock_disp_xy(70, 48, 2, 0);
		}
		else
		{
			// vdisp_load_buf();
			repeater_msg = 1;
			vdisp_prints_xy(80, 16, VDISP_FONT_6x8, 0, buf);
		}
		
	}
	else if ((crc_result == 0) && (ypos == 9))
	{
		if (diagram_displayed != 0)
		{
			vdisp_clear_rect (0, 0, 128, 36);
			diagram_displayed = 0;
		}
		vdisp_prints_xy(74, 18, VDISP_FONT_4x6, 0, "UR:");
		vdisp_prints_xy(86, 18, VDISP_FONT_4x6, 0, buf);
	}
	
	
	memcpy(buf, header_data + 27, 8);
	mkPrintableString(buf,8);
	
	
	if ((crc_result == 0) && (ypos == 5))
	{
		if ((header_data[0] & 0x07) == 0)
		{
			vdisp_prints_xy(0, 36, VDISP_FONT_8x12, 0, "RX:");
			vdisp_prints_xy(24, 36, VDISP_FONT_8x12, 0, buf);
		}
		else
		{
			vdisp_prints_xy(80, 8, VDISP_FONT_6x8, 0, buf);
		}
	}
	else if ((crc_result == 0) && (ypos == 9))
	{
		vdisp_prints_xy(0, 18, VDISP_FONT_4x6, 0, "RX:");
		vdisp_prints_xy(12, 18, VDISP_FONT_4x6, 0, buf);
	}
	
	
	memcpy(buf, header_data + 35, 4);
	mkPrintableString(buf,4);
	
	
	if ((crc_result == 0) && (ypos == 5))
	{
		if ((header_data[0] & 0x07) == 0)
		{
			vdisp_prints_xy(88, 36, VDISP_FONT_8x12, 0, "/");
			vdisp_prints_xy(96, 36, VDISP_FONT_8x12, 0, buf);
		}
		
	}
	else if ((crc_result == 0) && (ypos == 9))
	{
		vdisp_prints_xy(44, 18, VDISP_FONT_4x6, 0, "/");
		vdisp_prints_xy(48, 18, VDISP_FONT_4x6, 0, buf);
	}
	
	memcpy(buf, header_data + 11, 8);
	mkPrintableString(buf,8);
	
	if ((crc_result == 0) && (ypos == 9))
	{
		vdisp_prints_xy(0, 9, VDISP_FONT_6x8, 0, "R1:");
		vdisp_prints_xy(18, 9, VDISP_FONT_6x8, 0, buf);
	}
	
	memcpy(buf, header_data + 3, 8);
	mkPrintableString(buf,8);
	
	if ((crc_result == 0) && (ypos == 9))
	{
		vdisp_prints_xy(74, 11, VDISP_FONT_4x6, 0, "RPT2:");
		vdisp_prints_xy(94, 11, VDISP_FONT_4x6, 0, buf);
	}
}


static unsigned char sdHeaderBuf[41];
static unsigned char sdHeaderPos = 0;

static unsigned char sdTypeFlag = 0;
static unsigned char sdData[6];  // +1 because of mkPrintableString


static unsigned char checkSDHeaderCRC(void) 
{
	unsigned short sum = rx_dstar_crc_header(sdHeaderBuf);
	
	if (sdHeaderBuf[39] != (sum & 0xFF))
	{
		return 1;		
	}
	
	if (sdHeaderBuf[40] != ((sum >> 8) & 0xFF))
	{
		return 1;		
	}
	
	return 0;
}

static void processSDHeader( unsigned char len )
{
	int i;
	
	if ((len < 1) || (len > 5))
	{
		sdHeaderPos = 0;  // reset
		return;
	}
	
	if ((sdHeaderPos + len) > (sizeof sdHeaderBuf))
	{
		sdHeaderPos = 0;  // reset
		return;
	}
	
	for (i=0; i < len; i++)
	{
		sdHeaderBuf[sdHeaderPos + i] = sdData[i];
	}
	
	sdHeaderPos += len;
	
	if (sdHeaderPos == 41)
	{
		unsigned char res = checkSDHeaderCRC();
		
		printHeader(9, res, sdHeaderBuf);
		sdHeaderPos = 0;
	}
	
	if (len < 5)  // last frame always shorter than 5
	{
		sdHeaderPos = 0;
	}
}



static void processSlowData( unsigned char sdPos, const unsigned char * sd )
{	
	if (sdPos & 1)
	{	
		sdTypeFlag = sd[0];
		sdData[0] = sd[1];
		sdData[1] = sd[2];
	}
	else
	{	
		sdData[2] = sd[0];
		sdData[3] = sd[1];
		sdData[4] = sd[2];
		
		unsigned char len;
		
		len = sdTypeFlag & 0x07;  // ignore Bit 3
		
		if (len > 5) // invalid length
		{
			len = 0;  
		}
		
		switch (sdTypeFlag & 0xF0)
		{
			case 0x50:
				processSDHeader(len);
				break;
				
			case 0x40:
				if ((sdTypeFlag & 0x0C) == 0)
				{
					mkPrintableString((char *) sdData, 5);
					
					vdisp_prints_xy( ((sdTypeFlag & 0x03) * 30), 56, VDISP_FONT_6x8, 0, (char *) sdData );
				}
				break;
		}
	}
}

static U32 voicePackets = 0;
static U32 syncPackets = 0;


static unsigned char dState = 0;

#define FRAMESYNC_LEN	90

static unsigned char frameSync[FRAMESYNC_LEN];


static void dstarStateChange(unsigned char n)
{
	int i;
	// int lastValue = 128;
	
	switch(n)
	{

		case 2:
			voicePackets = 0;
			syncPackets = 0;
			sdHeaderPos = 0;
			
			// vdisp_save_buf();
			vd_copy_screen(VDISP_SAVE_LAYER, VDISP_MAIN_LAYER, 36, 64);
			
			vdisp_clear_rect (0, 0, 128, 64);
			vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 1, "  0s" );
			repeater_msg = 0;
			ppm_ptr = 0;
			
			for (i=0; i < PPM_BUFSIZE ; i++)
			{
				ppm_buf[i] = 4;
			}
			
			ppm_display_active = 0;
			break;
			
		case 1:
		
			if (repeater_msg == 0)
			{				
				int secs = voicePackets / 50;
				
				if (secs > 0)
				{
					char buf[4];
					vdisp_i2s(buf, 3, 10, 0, secs);
					vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 0, buf );
					vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, 0, "s" );
				}
				else
				{
					vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 0, "    " );
				}
				
				vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 0, "    " );
			}
			else
			{
				// vdisp_load_buf();
				
				vd_copy_screen(VDISP_MAIN_LAYER, VDISP_SAVE_LAYER, 36, 64);
			}

			
			
			
			/*
			
			for (i=0; i < FRAMESYNC_LEN; i++)
			{
				int k = frameSync[i];
				
				if (k < 40)
				{
					k = 40;
				}
				
				if (k >= 216)
				{
					k = 215;
				}

				
					
				
				lastValue = k;
			}  */
			break;
			
		case 10:
		    vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 1, "WAIT" );
			//vdisp_prints_xy( 36,0, VDISP_FONT_6x8, 0, " " );
			break;
		
		case 4:
		    vdisp_prints_xy( 0,0, VDISP_FONT_6x8, 1, " TX " );
			//vdisp_prints_xy( 36,0, VDISP_FONT_5x8, 0, " " );
			break;
	}
	
	dState = n;
}


static void print_diagram(int mean_value)
{
	int i;
	
	for (i=0; i < FRAMESYNC_LEN; i++)
	{
		int d = (mean_value - frameSync[i]) * SETTING_SHORT(S_PHY_RXDEVFACTOR) / 70;
					
		if ((d > -20) && (d < 20))
		{
			vdisp_set_pixel(38 + i, d + 20, 0, 1, 1);
			if ((i & 0x01) == 1)
			{
				vdisp_set_pixel(38 + i, 20, 0, 1, 1);
			}
		}
	}			
				
	char buf[5];
			
	vdisp_i2s(buf, 3, 10, 0, mean_value);
	vdisp_prints_xy( 116, 14, VDISP_FONT_4x6, 0, buf );
	
}


void dstarResetCounters(void)
{
	voicePackets = 0;
	syncPackets = 0;
}

void dstarChangeMode(int m)
{
	char buf[7] = { 0x00, 0x10, 0x02, 0xD3, m, 0x10, 0x03 };
		
	phyCommSend( buf, sizeof buf );
	
	dstarResetCounters();
}


#define CPU_ID_LEN 15

static char sysinfo[70];
static int sysinfo_len = 0;


int snmp_get_phy_cpuid ( int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	if (maxlen < CPU_ID_LEN)
			return 1;
			
	if (sysinfo_len <= CPU_ID_LEN )
	{
		memset (res, 0, CPU_ID_LEN);
	}
	else
	{
		memcpy(res, sysinfo + (sysinfo_len - CPU_ID_LEN), CPU_ID_LEN);
	}
	
	*res_len = CPU_ID_LEN;
	return 0;
}

static const char no_conn[] = "no connection to PHY";

int snmp_get_phy_sysinfo ( int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	if (sysinfo_len <= CPU_ID_LEN )
	{
		if ((sizeof no_conn) >= maxlen)
			return 1;
		memcpy (res, no_conn, (sizeof no_conn) - 1);
		*res_len = (sizeof no_conn) - 1;
	}
	else
	{
		if ((sysinfo_len - CPU_ID_LEN) > maxlen)
			return 1;
		memcpy(res, sysinfo, sysinfo_len - CPU_ID_LEN);
		*res_len = sysinfo_len - CPU_ID_LEN;
	}
	
	return 0;
}


int snmp_get_phy_sysparam ( int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	struct snmpReq rq;
	char buf[2];
	
	while (xQueueReceive( snmpReqQueue, &rq, 0))  // flush Q
	 ;
	
	buf[0] = 0x41; // request parameter
	buf[1] = arg;
	phyCommSendCmd(buf, 2);
	
	if (xQueueReceive( snmpReqQueue, &rq, 200)) // wait max 200ms
	{
		if (rq.param == -1)  // 0xD4  cmd_execution
		{
			if (rq.data != 1)  // not successful
				return 1; // error
		}
	}
	
	if (xQueueReceive( snmpReqQueue, &rq, 200)) // wait max 200ms
	{
		if (rq.param == arg)  // param matches requested param
		{
			return snmp_encode_int( rq.data, res, res_len, maxlen );
		}
	}
	
	return 1; // timeout
}


int snmp_set_phy_sysparam (int32_t arg, const uint8_t * req, int req_len)
{
	struct snmpReq rq;
	char buf[3];
	
	while (xQueueReceive( snmpReqQueue, &rq, 0))  // flush Q
	 ;
	
	buf[0] = 0x42; // set parameter
	buf[1] = arg;
	buf[2] = req[ req_len - 1 ]; // LSByte
	
	phyCommSendCmd(buf, 3);
	
	if (xQueueReceive( snmpReqQueue, &rq, 200)) // wait max 200ms
	{
		if (rq.param == -1)  // 0xD4  cmd_execution
		{
			if (rq.data == 1)  // successful
				return 0;
		}
	}
	
	return 1; // timeout or error
}


static void processPacket(void)
{
	
	
	
	switch(dp.cmdByte)
	{
		case 0x01:
			
			vdisp_clear_rect (0, 0, 128, 64);
			// dstarChangeMode(4);
			if (dp.dataLen <= (sizeof sysinfo))
			{
				memcpy(sysinfo, dp.data, dp.dataLen);
				sysinfo_len = dp.dataLen;
				
#define QRG_TX  430375000
#define QRG_RX  430375000

				char buf[12];
				
				buf[0] = 0x44;
				buf[1] = (QRG_RX >> 24) & 0xFF;
				buf[2] = (QRG_RX >> 16) & 0xFF;
				buf[3] = (QRG_RX >> 8) & 0xFF;
				buf[4] = (QRG_RX >> 0) & 0xFF;
				buf[5] = (QRG_TX >> 24) & 0xFF;
				buf[6] = (QRG_TX >> 16) & 0xFF;
				buf[7] = (QRG_TX >> 8) & 0xFF;
				buf[8] = (QRG_TX >> 0) & 0xFF;
 				
				phyCommSendCmd(buf, 9);
			}
			
			
			break;
			
		case 0x30:
			if (dp.dataLen == 40)
			{
				
				printHeader (5, dp.data[0], dp.data + 1); 
				
			}				
			break;
			
		case 0x31:
			if (dp.dataLen >= 10)
			{
				voicePackets ++;
				
				{
					char buf[4];
					int secs = voicePackets / 50;
					
					if (secs > 0)
					{
						vdisp_i2s(buf, 3, 10, 0, secs);
						vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, 1, buf );
						vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, 1, "s" );
					}
				}
				
				if ( (dp.data[0] == 0x20) && (dp.dataLen >= 37) )
				{
					ambe_input_data_sd( dp.data + 1 );
				}				
			}				
			break;
		case 0x32:
			syncPackets ++;
			
			if (dp.dataLen == 1)
			{
				ppm_buf[ppm_ptr] = dp.data[0];
				
				ppm_ptr++;
				
				if (ppm_ptr >= PPM_BUFSIZE)
				{
					ppm_ptr = 0;
					ppm_display_active = 1;
				}
				
				if (ppm_display_active != 0)
				{
					int i;
					char buf[5];
					int v;
					int minus = 0;
					
					for (i=0, v=0; i < PPM_BUFSIZE ; i++)
					{
						v += ppm_buf[i];
					}
					
					v = (v * 84) / PPM_BUFSIZE;
					v -= 7 * 84;
					// v -= 28;
					if (v < 0)
					{
						minus = 1;
						v = -v;
					}
					vdisp_i2s(buf, 3, 10, 1, v);
					vdisp_prints_xy( 5, 0, VDISP_FONT_6x8, 0, buf );
					vdisp_prints_xy( 0, 0, VDISP_FONT_6x8, 0, minus ? "-" : "+" );
					// vdisp_prints_xy( 20, 0, VDISP_FONT_5x8, 0, "ppm" );
				}
			}				
			break;
		case 0x33:
			if (dp.dataLen == 4)
			{
				processSlowData(dp.data[0], dp.data + 1);
			}
			break;
		case 0x34:
			break;
			
		case 0x35:
			if (dp.dataLen == (FRAMESYNC_LEN + 2))
			{
				
				if (dState != 2)
				{
					dstarStateChange(2);
				}
				
				int i;
				
				for (i=0; i < FRAMESYNC_LEN; i++)
				{
					frameSync[i] = dp.data[i+2];
				}			
				
				print_diagram(dp.data[0]);	
				
				char buf[5];
				int hub = dp.data[1] * SETTING_SHORT(S_PHY_RXDEVFACTOR);
				
				vdisp_i2s(buf, 4, 10, 0, hub);
				vdisp_prints_xy( 0, 10, VDISP_FONT_6x8, 0, buf );
				vdisp_prints_xy( 24, 10, VDISP_FONT_6x8, 0, "Hz" );
				vdisp_prints_xy( 0, 18, VDISP_FONT_6x8, 0, "Hub" );
				/*
				if (hub > hub_max)
				{
					hub_max = hub;
				}
				
				if (hub < hub_min)
				{
					hub_min = hub;
				}
				*/
				diagram_displayed = 1;
				
			} 
			
			break;
			
		case 0x40:
			if (dp.dataLen == 2)
			{
				struct snmpReq sr;
				sr.param = dp.data[0];
				switch (sr.param)
				{
					case 2:
					case 4:
						sr.data = ((int8_t *) dp.data)[1]; // signed byte
						break;
					default:
						sr.data = dp.data[1]; // unsigned byte
						break;
				}
				
				xQueueSend ( snmpReqQueue, & sr, 0 );
			}
			break;
			
		case 0xD1:
			if (dp.dataLen >= 2)
			{
				
				if (dState != dp.data[1])
				{
					dstarStateChange(dp.data[1]);
				}
			}
			break;
			
		case 0xD4:
			if (dp.dataLen == 1)
			{
				struct snmpReq sr;
				sr.param = -1;
				sr.data = dp.data[0];
				
				xQueueSend ( snmpReqQueue, & sr, 0 );
			}
			break;
	}	
}


static portTASK_FUNCTION( dstarRXTask, pvParameters )
{
	for( ;; )
	{
		if( xQueueReceive( dstarQueue, &dp, 500 ) )
		{
			processPacket();
		}
		else
		{
			// timeout
			qTimeout ++;
			// dispPrintDecimalXY(1, 8, qTimeout);
			
			dstarStateChange(0);
		}
		
	}
} 


static U32 dcs_last_session = 0;


void dstarProcessDCSPacket( const uint8_t * data )
{
	uint8_t buf[4];
	
	
	int dcs_session = data[43] | (data[44] << 8);
	
	if (dcs_session != dcs_last_session)
	{
		dcs_last_session = dcs_session;
		
		sdHeaderPos = 0;
		
		vdisp_clear_rect (0, 0, 128, 64);
		printHeader (5, 0, data + 4 );
		rtclock_disp_xy(84, 0, 2, 1);
	}
	
	int dcs_packets = data[58] | (data[59] << 8) | (data[60] << 16);
	
	int secs = dcs_packets / 50;
			
	int last_frame = 0;
	
		
	if ((data[45] & 0x40) != 0)
	{
		last_frame = 1;
	}			
	else
	{
		ambe_input_data( data + 46);
		
		buf[0] = 0x70 ^ data[55];
		buf[1] = 0x4F ^ data[56];
		buf[2] = 0x93 ^ data[57];
	
		processSlowData( data[45], buf);
	}		
			
	if (secs > 0)
	{
		vdisp_i2s((char *)buf, 3, 10, 0, secs);
		vdisp_prints_xy( 104, 48, VDISP_FONT_6x8, (last_frame == 0) ? 1 : 0, (char *) buf );
		vdisp_prints_xy( 122, 48, VDISP_FONT_6x8, (last_frame == 0) ? 1 : 0, "s" );
	}	
	
}



void dstarInit( xQueueHandle dq )
{
	dstarQueue = dq;
	
	snmpReqQueue = xQueueCreate( 3, sizeof (struct snmpReq) );
	
	xTaskCreate( dstarRXTask, ( signed char * ) "DstarRx", configMINIMAL_STACK_SIZE, NULL,
		 tskIDLE_PRIORITY + 1 , ( xTaskHandle * ) NULL );

}
