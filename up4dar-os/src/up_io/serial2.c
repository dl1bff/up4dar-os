/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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

/*! \file *********************************************************************
 *
 * \brief FreeRTOS Serial Port management example for AVR32 UC3.
 *
 * - Compiler:           IAR EWAVR32 and GNU GCC for AVR32
 * - Supported devices:  All AVR32 devices can be used.
 * - AppNote:
 *
 * \author               Atmel Corporation: http://www.atmel.com \n
 *                       Support and FAQ: http://support.atmel.no/
 *
 *****************************************************************************/

/* Copyright (c) 2007, Atmel Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of ATMEL may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
 * SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



/*
 * serial2.c
 *
 * Created: 17.01.2013 17:01:36
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "serial2.h"
#include <avr32/io.h>
#include "board.h"
#include "gpio.h"


int serial_rx_error = 0;
int serial_rx_ok = 0;


#define NUM_USART 2

#define USART_BUFLEN	200



struct usartBuffer
{
	int input_ptr;
	int output_ptr;
	char buf[USART_BUFLEN];
};



static int put_q( struct usartBuffer * q, char c)
{
	int next_ptr = q->input_ptr;
	
	next_ptr ++;
	
	if (next_ptr >= USART_BUFLEN)
	{
		next_ptr = 0;
	}
	
	if (next_ptr == q->output_ptr) // queue is full
	{
		return 0;
	}
	
	q->buf[ q->input_ptr ] = c;
	q->input_ptr = next_ptr;
	
	return 1;
}

static int get_q( struct usartBuffer * q, char * c)
{
	if (q->input_ptr == q->output_ptr)
	{
		return 0; // queue empty
	}
	
	int next_ptr = q->output_ptr;
	
	next_ptr ++;
	
	if (next_ptr >= USART_BUFLEN)
	{
		next_ptr = 0;
	}
	
	*c = q->buf[ q->output_ptr ];
	q->output_ptr = next_ptr;
	
	return 1;
}



static struct usartParams
{
	volatile avr32_usart_t * usart;
	uint8_t dma_pid_rx;
	uint8_t dma_pid_tx;
	uint8_t dma_channel_rx;
	uint8_t dma_channel_tx;
	int tx_ptr_sent;
	struct usartBuffer rx;
	struct usartBuffer tx;
} usarts[NUM_USART] =
{
	  { &AVR32_USART0, AVR32_PDCA_PID_USART0_RX, AVR32_PDCA_PID_USART0_TX, 6, 7, 0,
		   {0, 0, { 0 }},  {0, 0, { 0 }} }
	 ,{ &AVR32_USART1, AVR32_PDCA_PID_USART1_RX, AVR32_PDCA_PID_USART1_TX, 8, 9, 0,
		   {0, 0, { 0 }},  {0, 0, { 0 }} }
};




int serial_init ( int usartNum, int baudrate )
{
	if ((usartNum < 0) || (usartNum >= NUM_USART))
	{
	  return -1;  // error
	}
	
	volatile avr32_usart_t  *usart = usarts[usartNum].usart;
	
	int cd; /* USART Clock Divider. */


	/* Configure USART. */
	if(   ( baudrate > 0 ) )
	{
		// portENTER_CRITICAL();
		{
			/**
			** Reset USART.
			**/
			/* Disable all USART interrupt sources to begin... */
			usart->idr = 0xFFFFFFFF;

			/* Reset mode and other registers that could cause unpredictable
			 behaviour after reset */
			usart->mr = 0; /* Reset Mode register. */
			usart->rtor = 0; /* Reset Receiver Time-out register. */
			usart->ttgr = 0; /* Reset Transmitter Timeguard register. */

			/* Shutdown RX and TX, reset status bits, reset iterations in CSR, reset NACK
			 and turn off DTR and RTS */
			usart->cr = AVR32_USART_CR_RSTRX_MASK   |
					   AVR32_USART_CR_RSTTX_MASK   |
					   AVR32_USART_CR_RXDIS_MASK   |
					   AVR32_USART_CR_TXDIS_MASK   |
					   AVR32_USART_CR_RSTSTA_MASK  |
					   AVR32_USART_CR_RSTIT_MASK   |
					   AVR32_USART_CR_RSTNACK_MASK |
					   AVR32_USART_CR_DTRDIS_MASK  |
					   AVR32_USART_CR_RTSDIS_MASK;
					   
				   
			/**
			** Configure USART.
			**/

			/* Set the USART baudrate to be as close as possible to the wanted baudrate. */
			/*
			*             ** BAUDRATE CALCULATION **
			*
			*                 Selected Clock                       Selected Clock
			*     baudrate = ----------------   or     baudrate = ----------------
			*                    16 x CD                              8 x CD
			*
			*       (with 16x oversampling)              (with 8x oversampling)
			*/
			
			if( baudrate > 300 )
			{
				/* Use 8x oversampling */
				usart->mr |= (1<<AVR32_USART_MR_OVER_OFFSET);
				cd = configPBA_CLOCK_HZ / (baudrate);
				
				int fp = cd & 0x07; // fractional baudrate
				
				cd = cd >> 3; // divide by 8

				if( cd < 2 )
				{
					return -1;  // error
				}

				usart->brgr = (cd << AVR32_USART_BRGR_CD_OFFSET) | (fp << AVR32_USART_BRGR_FP_OFFSET);
			}
			else
			{
				return -1;
			}
			
			

			/* Set the USART Mode register: Mode=Normal(0), Clk selection=MCK(0),
			CHRL=8BIT,  SYNC=0(asynchronous), PAR=None, NBSTOP=0 (1 Stop bit), CHMODE=0, MSBF=0,
			MODE9=0, CKLO=0, OVER(previously done when setting the baudrate),
			other fields not used in this mode. */
			usart->mr |= ((8-5) << AVR32_USART_MR_CHRL_OFFSET  ) |
					(   4  << AVR32_USART_MR_PAR_OFFSET   ) |
					(   0  << AVR32_USART_MR_NBSTOP_OFFSET);

			/* Write the Transmit Timeguard Register */
			usart->ttgr = 0;

			
			uint8_t dma_channel = usarts[usartNum].dma_channel_rx;
			
			AVR32_PDCA.channel[dma_channel].psr   =  usarts[usartNum].dma_pid_rx    << AVR32_PDCA_PID_OFFSET;
			AVR32_PDCA.channel[dma_channel].mr    = AVR32_PDCA_MR_SIZE_BYTE  << AVR32_PDCA_MR_SIZE_OFFSET;
			
			AVR32_PDCA.channel[dma_channel].mar   = (unsigned long) usarts[usartNum].rx.buf;
			AVR32_PDCA.channel[dma_channel].marr  = (unsigned long) usarts[usartNum].rx.buf;
			
			AVR32_PDCA.channel[dma_channel].tcr   = (USART_BUFLEN & AVR32_PDCA_TCR_TCV_MASK)   << AVR32_PDCA_TCR_TCV_OFFSET;
			AVR32_PDCA.channel[dma_channel].tcrr  = (USART_BUFLEN & AVR32_PDCA_TCRR_TCRV_MASK) << AVR32_PDCA_TCRR_TCRV_OFFSET;
			
			
			dma_channel = usarts[usartNum].dma_channel_tx;
			
			AVR32_PDCA.channel[dma_channel].psr   =  usarts[usartNum].dma_pid_tx    << AVR32_PDCA_PID_OFFSET;
			AVR32_PDCA.channel[dma_channel].mr    = AVR32_PDCA_MR_SIZE_BYTE  << AVR32_PDCA_MR_SIZE_OFFSET;
			

			/* Enable receiver and transmitter... */
			usart->cr = AVR32_USART_CR_TXEN_MASK | AVR32_USART_CR_RXEN_MASK;
			
			// enable DMA
			AVR32_PDCA.channel[usarts[usartNum].dma_channel_rx].cr    = AVR32_PDCA_CR_TEN_MASK;
			AVR32_PDCA.channel[usarts[usartNum].dma_channel_tx].cr    = AVR32_PDCA_CR_TEN_MASK;
		}
		// portEXIT_CRITICAL();
		
	}
	else
	{
		return -1;  // error
	}

	return 0;  
}



int serial_stop ( int usartNum )
{
	if ((usartNum < 0) || (usartNum >= NUM_USART))
	{
		return -1;  // error
	}
	
	volatile avr32_usart_t  *usart = usarts[usartNum].usart;
	
	usart->cr = AVR32_USART_CR_TXDIS_MASK | AVR32_USART_CR_RXDIS_MASK;
	
	return 0;
}


int serial_timeout_error = 0;
int serial_putc_q_full = 0;


int serial_putc ( int usartNum, char cOutChar )
{
	struct usartBuffer * q = & usarts[usartNum].tx;

	if (put_q(q, cOutChar) != 1)
	{
		serial_putc_q_full++;
		return 0; // queue is full
	}

	return 1;
}





int serial_rx_char_available (int usartNum)
{
	struct usartBuffer * rx_q = & usarts[usartNum].rx;
	uint8_t dma_channel = usarts[usartNum].dma_channel_rx;
	
	rx_q->input_ptr = AVR32_PDCA.channel[dma_channel].mar 
						- (unsigned long) usarts[usartNum].rx.buf;
	
	if (AVR32_PDCA.channel[dma_channel].ISR.trc != 0) // transfer complete -> should not happen
	{
		// start again
		
		rx_q->input_ptr = 0;
		rx_q->output_ptr = 0;
		
		AVR32_PDCA.channel[dma_channel].marr  = (unsigned long) rx_q->buf;
		AVR32_PDCA.channel[dma_channel].mar  = (unsigned long) rx_q->buf;
		AVR32_PDCA.channel[dma_channel].tcrr  = (USART_BUFLEN & AVR32_PDCA_TCRR_TCRV_MASK) << AVR32_PDCA_TCRR_TCRV_OFFSET;
		AVR32_PDCA.channel[dma_channel].tcr  = (USART_BUFLEN & AVR32_PDCA_TCR_TCV_MASK) << AVR32_PDCA_TCR_TCV_OFFSET;
		
		serial_rx_error ++;
	}								
	else if (AVR32_PDCA.channel[dma_channel].ISR.rcz != 0)
	{
		AVR32_PDCA.channel[dma_channel].marr  = (unsigned long) rx_q->buf;
		AVR32_PDCA.channel[dma_channel].tcrr  = (USART_BUFLEN & AVR32_PDCA_TCRR_TCRV_MASK) << AVR32_PDCA_TCRR_TCRV_OFFSET;
		
		serial_rx_ok ++;
	}
	
	struct usartBuffer * tx_q = & usarts[usartNum].tx;
	dma_channel = usarts[usartNum].dma_channel_tx;
	
	if (usarts[usartNum].tx_ptr_sent != tx_q->output_ptr) // transmission in progress
	{
		if (AVR32_PDCA.channel[dma_channel].ISR.trc != 0) // tx inactive
		{
			tx_q->output_ptr = usarts[usartNum].tx_ptr_sent; // everything transmitted
		}			
	}
	
	if (usarts[usartNum].tx_ptr_sent == tx_q->output_ptr) // transmission not in progress
	{
		if (tx_q->input_ptr > tx_q->output_ptr)
		{
			int len = tx_q->input_ptr - tx_q->output_ptr;
		
			AVR32_PDCA.channel[dma_channel].mar  = (unsigned long) (tx_q->buf + tx_q->output_ptr);
			AVR32_PDCA.channel[dma_channel].tcr  = (len & AVR32_PDCA_TCR_TCV_MASK) << AVR32_PDCA_TCR_TCV_OFFSET;
			usarts[usartNum].tx_ptr_sent = tx_q->input_ptr;
		}
		else if (tx_q->input_ptr < tx_q->output_ptr)
		{
			int len = USART_BUFLEN - tx_q->output_ptr;
		
			AVR32_PDCA.channel[dma_channel].mar  = (unsigned long) (tx_q->buf + tx_q->output_ptr);
			AVR32_PDCA.channel[dma_channel].tcr  = (len & AVR32_PDCA_TCR_TCV_MASK) << AVR32_PDCA_TCR_TCV_OFFSET;
			AVR32_PDCA.channel[dma_channel].marr  = (unsigned long) tx_q->buf;
			AVR32_PDCA.channel[dma_channel].tcrr  = (tx_q->input_ptr & AVR32_PDCA_TCRR_TCRV_MASK) << AVR32_PDCA_TCRR_TCRV_OFFSET;
			usarts[usartNum].tx_ptr_sent = tx_q->input_ptr;
		}
	}

	return rx_q->input_ptr != rx_q->output_ptr;
}



void serial_putc_tmo (int comPort, char c, short timeout)
{
	short i = timeout;
	
	while (i > 0)
	{
		if (serial_putc(comPort, c) == 1)
			break;
		i--;
		serial_rx_char_available(comPort); // fill the TX buffer
		vTaskDelay(1);
	}
	
	if (i <= 0)
	{
		serial_timeout_error ++;
	}
}

int serial_getc ( int usartNum, char * cOutChar )
{
	struct usartBuffer * q = & usarts[usartNum].rx;
	
	return get_q(q, cOutChar);
}


