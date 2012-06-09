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
 * eth.c
 *
 * Created: 28.05.2011 18:12:19
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include <asf.h>

#include "board.h"
#include "gpio.h"

#include "eth_txmem.h"

#include "up_net/ipneigh.h"
#include "eth.h"

#include "up_net/ipv4.h"

#include "gcc_builtin.h"

#include "up_net/arp.h"


U32 eth_counter = 0;
U32 eth_counter2 = 0;
U32 eth_counter3 = 0;

int eth_ptr = 0;


/*

static unsigned char vdisp_frame[1024 + 42 + 320] =
	{	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xDE, 0x1B, 0xFF, 0x00, 0x00, 0x01,
		0x08, 0x00,  // IPv4
		0x45, 0x00, // v4, DSCP
		0x00, 0x00, // ip length (will be set later)
		0x01, 0x01, // ID
		0x40, 0x00,  // DF don't fragment, offset = 0
		0x40, // TTL
		0x11, // UDP = 17
		0x00, 0x00,  // header chksum (will be calculated later)
		192, 168, 1, 33,  // source
		192, 168, 1, 255,  // destination
		0xb0, 0xb0,  // source port
		0xb0, 0xb0,  // destination port
		0x00, 0x00,    //   UDP length (will be set later)
		0x00, 0x00,  // UDP chksum (0 = no checksum)
		
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

*/


#define RECV_BUF_COUNT  64
#define RECV_BUF_SIZE	128

static unsigned char rx_mem[(RECV_BUF_COUNT * RECV_BUF_SIZE) + 1540];  // 64 * 128  + 1 Frame

static unsigned long rx_buffer_q[RECV_BUF_COUNT * 2];

unsigned char mac_addr[6] = { 0xDE, 0x1B, 0xFF, 0x00, 0x00, 0x00 };
	



//void eth_init(unsigned char ** p)
void eth_init(void)
{	
	
	// Ethernet MAC address
	memcpy(mac_addr + 3, (unsigned char *) 0x80800204, 3); // first 3 bytes of CPU id
									// are the last 3 bytes of the MAC address
	
	AVR32_MACB.NCFGR.spd = 1;  // 100MBit/s

	AVR32_MACB.NCFGR.fd = 1; // full duplex
	AVR32_MACB.NCFGR.clk = 2; // MCK / 32 -> 1.875 MHz
	
	AVR32_MACB.USRIO.rmii = 0; // RMII
	
	// tx_buffer_q[0] = (unsigned long) & vdisp_frame;
	// tx_buffer_q[1] = (sizeof vdisp_frame) | 0x40008000; // wrap bit, last buffer
	
	// AVR32_MACB.tbqp = (unsigned long) & tx_buffer_q;
	
	AVR32_MACB.ncr = 0x0018; // TE + MDE
	
	/*
	if (p != 0)
	{
		*p = vdisp_frame + 42;
	}  */
	
	unsigned long rx_addr = ((unsigned long) & rx_mem) & 0xFFFFFFFC; 
	
	if (rx_addr & 0x04)
	{
		rx_addr += 4;  // align to 8 byte boundary
	}
	
	int i;
	
	for (i=0; i < RECV_BUF_COUNT; i++)
	{
		rx_buffer_q[ (i << 1) ] = rx_addr;
		rx_addr += RECV_BUF_SIZE;
	}
	
	rx_buffer_q[ ((i - 1) << 1) ] |= 0x02; // wrap bit
	
	
	
	AVR32_MACB.rbqp = (unsigned long) & rx_buffer_q;
	
	AVR32_MACB.NCFGR.drfcs = 1;  // don't copy FCS to memory
	
	AVR32_MACB.sa1b = mac_addr[0] |
						(mac_addr[1] << 8) |
						(mac_addr[2] << 16) |
						(mac_addr[3] << 24);
	AVR32_MACB.sa1t	= mac_addr[4] |
						(mac_addr[5] << 8);
						
	AVR32_MACB.NCR.re = 1;  // receive enable

	eth_ptr = 0;
	
	/*
	// 2kHz tone:
	for (i=(1024 + 42); i < (sizeof vdisp_frame); i+=8)
	{
		vdisp_frame[i+0] = 0;
		vdisp_frame[i+1] = 0;
				
		vdisp_frame[i+2] = 10;
		vdisp_frame[i+3] = 0;
				
		vdisp_frame[i+4] = 0;
		vdisp_frame[i+5] = 0;
				
		vdisp_frame[i+6] = 245;
		vdisp_frame[i+7] = 0;
	}
	
	// vdisp_frame
	
	int udp_length = 1024 + 320 + 8;
	
	((unsigned short *) (vdisp_frame + 14)) [1] = udp_length + 20; // IP len
	
	((unsigned short *) (vdisp_frame + 14)) [12] = udp_length; // UDP len
	
	int sum = 0;
	
	for (i=0; i < 10; i++) // 20 Byte Header
	{
		if (i != 5)  // das checksum-feld weglassen
		{
			sum += ((unsigned short *) (vdisp_frame + 14)) [i];
		}
	}
	
	sum = (~ ((sum & 0xFFFF)+(sum >> 16))) & 0xFFFF;
	
	((unsigned short *) (vdisp_frame + 14)) [5] = sum; // checksumme setzen
	
	
	*/
	
}


static void free_buffer (int start_buf, int end_buf)
{
	int i = start_buf;
	
	while(i != end_buf)
	{
		rx_buffer_q[ (i << 1) ] &= (~ 0x01);  //freigeben
		
		i ++;
		
		if (i == end_buf)
			break;   // der (end_buf == RECV_BUF_COUNT)-Fall
		
		if (i >= RECV_BUF_COUNT)
		{
			i = 0;
		}
	}
}


void eth_send_raw ( unsigned char * b, int len )
{
	// TODO:  wait if a transmission is currently active
	
	//AVR32_MACB.tbqp = (unsigned long) & tx_buffer_q;
	
	/*
	tx_buffer_q[0] = (unsigned long) b;
	tx_buffer_q[1] = ((unsigned long) len) | 0x40008000; // wrap bit, last buffer
	AVR32_MACB.NCR.tstart = 1; // und los!
	*/
}	


void eth_set_src_mac_and_type(uint8_t * data, uint16_t ethType)
{
	memcpy(data + 6, mac_addr, sizeof mac_addr);  // source MAC
	
	data[12] = ethType >> 8;
	data[13] = ethType & 0xFF;
}



static void process_frame (unsigned char * p, int len)
{
	//eth_counter ++;
	
	switch (((unsigned short *)p)[6])
	{
		case 0x86dd: // IPv6
			break;
		case 0x0800: // IPv4
			ipv4_input(p+14, len - 14, p);
			break;
		case 0x0806: // ARP
			if (len >= 42)
			{
				arp_process_packet(p);
			}
			break;
	}
}


void eth_rx (void)
{
	if (AVR32_MACB.RSR.rec == 1) 
	{
		AVR32_MACB.RSR.rec = 1;  // bit loeschen
		
	
		
		while(1)
		{
			
		
		int count = RECV_BUF_COUNT;
		
		while (1)
		{
			
			while ((rx_buffer_q[ (eth_ptr << 1) ] & 0x01) == 0) // fertigen buffer suchen
			{
			
				eth_ptr ++;
				if (eth_ptr >= RECV_BUF_COUNT)
				{
					eth_ptr = 0;
				}
			
			
				count --;
				if (count <= 0)  // genau RECV_BUF_COUNT pruefen, sonst ist man
				   // bei einem leeren buffer nicht genau an der stelle wo dann der
				   // naechste startblock reingeschrieben wuerde. die naechste
				   // if anweisung macht dann das paket kaputt.
				{
					// einmal alles durchgesucht, keinen fertigen buffer gefunden!
				
					return;
				//	eth_counter ++;  // mitzaehlen, wie oft das passiert
				//	return;
				}
			}
		
			if ((rx_buffer_q[ (eth_ptr << 1) +1 ] & 0x4000) == 0) // not start buffer
			{	
			// der gefundene buffer ist kein start buffer!
				rx_buffer_q[ (eth_ptr << 1) ] &= (~ 0x01);  //freigeben und weitersuchen
				
				eth_counter3 ++;
			}
			else
				break;
		}		
				
		/*
		if ((rx_buffer_q[ (eth_ptr << 1) ] & 0x01) == 0)  // buffer nicht fertig -> break
			break;
		*/
		
		
		
		int start_buffer = eth_ptr;
		count = 13; // irgendwo in den naechsten 13 buffern muss der stop-buffer sein
		
		int rx_error = 0;
		
		while ((rx_buffer_q[ (eth_ptr << 1) +1 ] & 0x8000) == 0)
		{
			eth_ptr ++;
			if (eth_ptr >= RECV_BUF_COUNT)
			{
				eth_ptr = 0;
			}
				
			count --;
			if (count < 0)
			{
				// keinen stop buffer gefunden
				eth_counter2 ++;  // mitzaehlen, wie oft das passiert
				// free_buffer(0, RECV_BUF_COUNT);
				
				free_buffer(start_buffer, eth_ptr); // alles bis hier hin freigeben
				rx_error = 1;
				break;
			}
			
			if ((rx_buffer_q[ (eth_ptr << 1) ] & 0x01) == 0)
			{
				// dieser buffer ist gar nicht gefuellt, der frame war vielleicht
				// noch gar nicht fertig empfangen
				eth_ptr = start_buffer;  // zurueck auf den letzten start_buffer
				
				
				eth_counter2 ++;  // mitzaehlen, wie oft das passiert
				return;
			}
			
			if ((rx_buffer_q[ (eth_ptr << 1) +1 ] & 0x4000) != 0) // wir suchen einen stopbuffer
			{
				// dieser buffer ist ein anderer start-buffer, das kann nur passieren,
				// wenn ein empfangsvorgang abgebrochen wurde -> vorhergehende
				// buffer freigeben
				free_buffer(start_buffer, eth_ptr);
				
				start_buffer = eth_ptr; // an dieser stelle weiter nach stop-buffer suchen
				count = 13;
				
				eth_counter3 ++;
			}
		}
		
		if (rx_error != 0)
		  continue;  
			
		// hier ist jetzt ein kompletter frame: 
		//   start_buffer  anfang
		//   eth_ptr    ende
		
		int packet_len = rx_buffer_q[(eth_ptr << 1) +1] & 0x7FF;
		
		if (eth_ptr < start_buffer)  // buffer wrap trat auf
		{
			unsigned char * s_buf = ((unsigned char *) (rx_buffer_q[0] & (~0x07)));
			memcpy ( s_buf + (RECV_BUF_COUNT * RECV_BUF_SIZE), s_buf, packet_len );
			// Daten vom Anfang an's Ende kopieren, damit der Frame in einem Stueck ist
		}			
		
		// Frame bearbeiten
		
		process_frame ((unsigned char *) (rx_buffer_q[start_buffer << 1] & 0xFFFFFFFC),
		   packet_len);
		
		//eth_counter ++;
		//
		
		eth_ptr ++;
		if (eth_ptr >= RECV_BUF_COUNT)
		{
			eth_ptr = 0;
		}
		
		free_buffer(start_buffer, eth_ptr);
		
		
		} // while(1)		
		
	}

	
	
}

/*

void eth_send_vdisp_frame (void)
{

  eth_send_raw( vdisp_frame, sizeof vdisp_frame );  

}

*/

