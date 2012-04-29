
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
 * ambe_q.c
 *
 * Created: 28.04.2012 17:14:25
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include "gcc_builtin.h"

#include "ambe_q.h"



static const uint8_t silence_data[AMBE_Q_DATASIZE] =
 { 0x9e, 0x8d, 0x32, 0x88, 0x26, 0x1a, 0x3f, 0x61, 0xe8 };



int ambe_q_flush (ambe_q_t * a)
{

	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		a->count = 0;
		a->in_ptr = 0;
		a->out_ptr = 0;
		a->state = 0;
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		return 1;
	}
	
	return 0;
}


void ambe_q_initialize( ambe_q_t * a )
{
	a->mutex = xSemaphoreCreateMutex();
	
	ambe_q_flush( a );
}


int ambe_q_put (ambe_q_t * a,  const uint8_t * data)
{
	int ret = 0;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if ((AMBE_Q_BUFLEN - a->count) >= AMBE_Q_DATASIZE) // there is space in the buffer
		{
			memcpy (a->buf + a->in_ptr, data, AMBE_Q_DATASIZE);
			
			a->in_ptr += AMBE_Q_DATASIZE;
			
			if (a->in_ptr >= AMBE_Q_BUFLEN)
			{
				a->in_ptr = 0;
			}
			
			a->count += AMBE_Q_DATASIZE;
			
			if (a->count >= (AMBE_Q_DATASIZE * 3)) // if 3 ambe records are in memory...
			{
				a->state = 1;  // ... start delivering via get method
			}
		}
		else
		{
			ret = 1; // buffer is full
		}			
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		ret = 1;
	}
	
	return ret;
}



int ambe_q_get (ambe_q_t * a,  uint8_t * data )
{
	int ret = 0;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if ((a->count > 0) && (a->state == 1)) // there is data in the buffer
		{
			memcpy (data, a->buf + a->out_ptr, AMBE_Q_DATASIZE);
			
			a->out_ptr += AMBE_Q_DATASIZE;
			
			if (a->out_ptr >= AMBE_Q_BUFLEN)
			{
				a->out_ptr = 0;
			}
			
			a->count -= AMBE_Q_DATASIZE;
			
			if (a->count <= 0)
			{
				a->state = 0;
			}
		}		
		else
		{
			memcpy ( data, silence_data, AMBE_Q_DATASIZE );
			ret = 1;
		}				
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		
		memcpy ( data, silence_data, AMBE_Q_DATASIZE );
		ret = 1;
	}
	
	return ret;
}

