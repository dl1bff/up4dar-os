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



#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "phycomm.h"

#include "up_io/serial2.h"

#include "compiler.h"


#include "up_dstar/dstar.h"



#define mainCOM_TEST_PRIORITY     ( tskIDLE_PRIORITY + 1 )




static int xPort;

#define comRX_BLOCK_TIME			( ( portTickType ) 100 )
#define comSTACK_SIZE				configMINIMAL_STACK_SIZE


extern U32 counter;


#define RXSTATE_IDLE				0
#define RXSTATE_RECEIVING_CMD		1
#define RXSTATE_RECEIVING_DATA		2

static char rxState = RXSTATE_IDLE;
static char rxEscCharReceived = FALSE;


#define HEX_RECEIVE_IDLE		0
#define HEX_RECEIVE_FIRSTCHAR	1
#define HEX_RECEIVE_SECONDCHAR	2

/*
static char hexRxState = HEX_RECEIVE_IDLE;
static unsigned char hexData;
*/

extern U32 errorCounter;


static struct dstarPacket dp;

static xQueueHandle dstarQueue;





static void rxByte2 ( unsigned char d )
{
	switch (rxState)
	{
		case RXSTATE_IDLE:
			errorCounter ++;
			break;
			
		case RXSTATE_RECEIVING_CMD:
			dp.cmdByte = d;
			dp.dataLen = 0;
			rxState = RXSTATE_RECEIVING_DATA;
			break;
			
		case RXSTATE_RECEIVING_DATA:
			if (dp.dataLen < (sizeof dp.data) )
			{
				dp.data[dp.dataLen] = d;
				dp.dataLen ++;
			}
			else
			{
				errorCounter ++;
				rxState = RXSTATE_IDLE;
			}
			break;
	}
}	


static void rxByte ( unsigned char d )
{
	if (rxEscCharReceived == TRUE)
	{
		rxEscCharReceived = FALSE;
		
		switch (d)
		{
			case 0x10:
				rxByte2(d);
				break;
				
			case 0x02:
				if (rxState != RXSTATE_IDLE)
				{
					errorCounter ++;
				}
				rxState = RXSTATE_RECEIVING_CMD;
				break;
				
			case 0x03:
				if (rxState == RXSTATE_IDLE)
				{
					errorCounter ++;
				}
				else
				{
					counter ++;
					
					if (! (xQueueSend( dstarQueue, &dp, 0) == pdTRUE))
					{
						errorCounter ++;
					}
				}
				rxState = RXSTATE_IDLE;
				break;
				
			default:
				errorCounter ++;
				rxState = RXSTATE_IDLE;
				break;
		}
	}
	else
	{
		if (d == 0x10)
		{
			rxEscCharReceived = TRUE;
		}
		else
		{
			rxByte2(d);
		}
	}
}

/*

static unsigned char convHex(char c)
{
	unsigned char n = c & 0x0F;
	
	if (c > 64)
	{
		n += 9;
	}
	
	return n;
}


static void hexRx( char c ) 
{
	if ( ((c >= '0') && (c <= '9') )
		|| ((c >= 'A') && (c <= 'F')) )
	{
		switch(hexRxState)
		{
			case HEX_RECEIVE_IDLE:
				hexData = convHex(c) << 4;
				hexRxState = HEX_RECEIVE_FIRSTCHAR;
				break;
			case HEX_RECEIVE_FIRSTCHAR:
				hexData |= convHex(c);
				hexRxState = HEX_RECEIVE_SECONDCHAR;
				break;
			default:
				hexRxState = HEX_RECEIVE_IDLE;  // error
				errorCounter ++;
				break;
		}
	}
	else if ((c == 10) || (c == 13) || (c == 32))
	{
		switch(hexRxState)
		{
			case HEX_RECEIVE_SECONDCHAR:
				rxByte(hexData);
				hexRxState = HEX_RECEIVE_IDLE;
				break;
			default:
				hexRxState = HEX_RECEIVE_IDLE;
				break;
		}
	}
	else
	{
		hexRxState = HEX_RECEIVE_IDLE;  // error
		errorCounter ++;
	}
	
}
*/

static portTASK_FUNCTION( vComRxTask, pvParameters )
{
	char cByteRxed;
	short timeout_counter = 0;

	for( ;; )
	{
		if (serial_rx_char_available(xPort))
		{
			timeout_counter = 0;
			
			while (serial_getc(xPort, &cByteRxed) == 1)
			{
				rxByte(cByteRxed);
			}
		}
		else // no chars available
		{
			vTaskDelay(1);
			
			timeout_counter ++;
			if (timeout_counter >= 1000)  // one second
			{
				rxState = RXSTATE_IDLE;
				timeout_counter = 0;
			}
			
		}
		
	}
} 



void phyCommSend (char * buf, int len)
{
	int i;
	
	for (i=0; i < len; i++)
	{
		serial_putc_tmo( xPort, buf[i], 500 );  // half a second...
	}
}



#define DLE 0x10
#define STX 0x02
#define ETX 0x03

void phyCommSendCmd (const char * cmd, int len)
{
	const char * p = cmd;
	int i;
	
	serial_putc_tmo( xPort, DLE, 50 );
	serial_putc_tmo( xPort, STX, 50 );
	
	for (i=0; i < len; i++)
	{
		char d = *p;
		
		if (d == DLE)
		{
			serial_putc_tmo( xPort, DLE, 50 );
			serial_putc_tmo( xPort, DLE, 50 );
		}
		else
		{
			serial_putc_tmo( xPort, d, 50 );
		}
		p++;
	}
	
	serial_putc_tmo( xPort, DLE, 50 );
	serial_putc_tmo( xPort, ETX, 50 );
}


void phyCommInit( xQueueHandle dq, int comPortHandle )
{
	dstarQueue = dq;
	
	// xPort = xSerialPortInitMinimal( 1, 115200, 20 );
	
	xPort = comPortHandle;
	
	xTaskCreate( vComRxTask, ( signed char * ) "COMRx", comSTACK_SIZE, NULL, mainCOM_TEST_PRIORITY, ( xTaskHandle * ) NULL );
}
