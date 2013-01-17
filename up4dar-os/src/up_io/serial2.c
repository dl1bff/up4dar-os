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


unsigned long serialRXError = 0;
unsigned long serialRXOK = 0;


static void vUSART0_ISR( void );
static void vUSART1_ISR( void );
static void vUSART2_ISR( void );


#define NUM_USART 3

#define USART_BUFLEN	100

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
	int irq;
	void (* intrHandler) (void);
	// xQueueHandle xRxedChars;
	// xQueueHandle xCharsForTx;
	struct usartBuffer rx;
	struct usartBuffer tx;
} usarts[NUM_USART] =
{
	{ &AVR32_USART0,  AVR32_USART0_IRQ,  vUSART0_ISR,  {0, 0, { 0 }},  {0, 0, { 0 }} },
	{ &AVR32_USART1,  AVR32_USART1_IRQ,  vUSART1_ISR,  {0, 0, { 0 }},  {0, 0, { 0 }} },
	{ &AVR32_USART2,  AVR32_USART2_IRQ,  vUSART2_ISR,  {0, 0, { 0 }},  {0, 0, { 0 }} }
};



// -------------

#if __GNUC__
	__attribute__((noinline))
#elif __ICCAVR32__
	#pragma optimize = no_inline
#endif

static portBASE_TYPE prvUSART_ISR_NonNakedBehaviour( int usartNum )
{
	/* Now we can declare the local variables. */
	portCHAR     cChar;
	// portBASE_TYPE     xHigherPriorityTaskWoken = pdFALSE;
	unsigned portLONG     ulStatus;
	unsigned portLONG     ulMaskReg;
	volatile avr32_usart_t  *usart = usarts[usartNum].usart;
	portBASE_TYPE retstatus;

	/* What caused the interrupt? */
	ulStatus = usart->csr; 
	ulMaskReg = usart->imr;

	if (ulStatus & ulMaskReg & AVR32_USART_CSR_TXRDY_MASK)
	{
		/* The interrupt was caused by the THR becoming empty.  Are there any
		more characters to transmit?
		Because FreeRTOS is not supposed to run with nested interrupts, put all OS
		calls in a critical section . */
		// portENTER_CRITICAL();
		//	retstatus = xQueueReceiveFromISR( usarts[usartNum].xCharsForTx, &cChar, &xHigherPriorityTaskWoken );
		// portEXIT_CRITICAL();
		
		
		retstatus = get_q (& usarts[usartNum].tx, &cChar);

		if (retstatus == 1)
		{
			/* A character was retrieved from the queue so can be sent to the
			 THR now. */
			usart->thr = cChar;
		}
		else
		{
			/* Queue empty, nothing to send so turn off the Tx interrupt. */
			usart->idr = AVR32_USART_IDR_TXRDY_MASK;
		}
	}

	if (ulStatus & AVR32_USART_CSR_RXRDY_MASK)
	{
		/* The interrupt was caused by the receiver getting data. */
		cChar = usart->rhr; //TODO

		/* Because FreeRTOS is not supposed to run with nested interrupts, put all OS
		calls in a critical section . */
		/*
		portENTER_CRITICAL();
			retstatus = xQueueSendFromISR(usarts[usartNum].xRxedChars, &cChar, &xHigherPriorityTaskWoken);
		portEXIT_CRITICAL();
		*/
	
		retstatus = put_q(& usarts[usartNum].rx, cChar);
		
		
		if (retstatus == 1)
		{
			serialRXOK ++;
		}
		else
		{
			serialRXError ++;
		}
	}
	
	
	if (ulStatus & (AVR32_USART_CSR_OVRE_MASK | AVR32_USART_CSR_FRAME_MASK ))
	{
			serialRXError ++;	
			usart->cr = AVR32_USART_CR_RSTSTA_MASK;
	}
	
	
	// xSerialRXError += usart->NER.nb_errors;

	/* The return value will be used by portEXIT_SWITCHING_ISR() to know if it
	should perform a vTaskSwitchContext(). */
	// return ( xHigherPriorityTaskWoken );
	
	return pdFALSE; // do not switch context
}

/*-----------------------------------------------------------*/

/*
 * USART interrupt service routine.
 */
#if __GNUC__
	__attribute__((__naked__))
#elif __ICCAVR32__
	#pragma shadow_registers = full   // Naked.
#endif

static void vUSART0_ISR( void )
{
	/* This ISR can cause a context switch, so the first statement must be a
	call to the portENTER_SWITCHING_ISR() macro.  This must be BEFORE any
	variable declarations. */
	portENTER_SWITCHING_ISR();

	prvUSART_ISR_NonNakedBehaviour(0);

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR();
}



/*-----------------------------------------------------------*/


/*
 * USART interrupt service routine.
 */
#if __GNUC__
	__attribute__((__naked__))
#elif __ICCAVR32__
	#pragma shadow_registers = full   // Naked.
#endif

static void vUSART1_ISR( void )
{
	/* This ISR can cause a context switch, so the first statement must be a
	call to the portENTER_SWITCHING_ISR() macro.  This must be BEFORE any
	variable declarations. */
	portENTER_SWITCHING_ISR();

	prvUSART_ISR_NonNakedBehaviour(1);

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR();
}
/*-----------------------------------------------------------*/

/*
 * USART interrupt service routine.
 */
#if __GNUC__
	__attribute__((__naked__))
#elif __ICCAVR32__
	#pragma shadow_registers = full   // Naked.
#endif

static void vUSART2_ISR( void )
{
	/* This ISR can cause a context switch, so the first statement must be a
	call to the portENTER_SWITCHING_ISR() macro.  This must be BEFORE any
	variable declarations. */
	portENTER_SWITCHING_ISR();

	prvUSART_ISR_NonNakedBehaviour(2);

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR();
}
/*-----------------------------------------------------------*/



// -------------




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
		portENTER_CRITICAL();
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


			/* Register the USART interrupt handler to the interrupt controller and
			 enable the USART interrupt. */
			INTC_register_interrupt((__int_handler) usarts[usartNum].intrHandler, 
					usarts[usartNum].irq, AVR32_INTC_INT1);

			/* Enable USART interrupt sources (but not Tx for now)... */
			usart->ier = AVR32_USART_IER_RXRDY_MASK;

			/* Enable receiver and transmitter... */
			usart->cr = AVR32_USART_CR_TXEN_MASK | AVR32_USART_CR_RXEN_MASK;
			
		}
		portEXIT_CRITICAL();
		
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
	
	portENTER_CRITICAL();
	
	{
		usart->idr = AVR32_USART_IDR_RXRDY_MASK | AVR32_USART_IDR_TXRDY_MASK;
		usart->cr = AVR32_USART_CR_TXDIS_MASK | AVR32_USART_CR_RXDIS_MASK;
	}
	
	portEXIT_CRITICAL();
	
	return 0;
}


int serial_putc ( int usartNum, char cOutChar );

int serial_getc ( int usartNum, char * cOutChar );
int serial_rx_char_available (int usartNum);



/*

int serial_puts (int usartNum, const char * s)
{
	const char * p = s;
	while (*p != 0)
	{
		int res = serial_putc(usartNum, *p);
		
		if (res == pdFAIL) // queue is full
		{
			return -1;
		}
		p++;
	}
	return 0;
}

*/