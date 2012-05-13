
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


static void expand_to_sd_data( uint8_t * sd_data, const uint8_t * inp_data)
{
	int i;
	for (i=0; i < AMBE_Q_DATASIZE; i++)
	{
		uint8_t d = inp_data[i];
		sd_data[(i << 2) + 0] = ((d & 0x80) ? 0xF0 : 0) | ((d & 0x40) ? 0x0F : 0);
		sd_data[(i << 2) + 1] = ((d & 0x20) ? 0xF0 : 0) | ((d & 0x10) ? 0x0F : 0);
		sd_data[(i << 2) + 2] = ((d & 0x08) ? 0xF0 : 0) | ((d & 0x04) ? 0x0F : 0);
		sd_data[(i << 2) + 3] = ((d & 0x02) ? 0xF0 : 0) | ((d & 0x01) ? 0x0F : 0);
	}
}

static void reduce_sd_data( uint8_t * data, const uint8_t * sd_data)
{
	int i;
	for (i=0; i < AMBE_Q_DATASIZE; i++)
	{
		data[i] =
			 (sd_data[(i << 2) + 0] & 0x80)       |  ((sd_data[(i << 2) + 0] & 0x08) << 3) |
			((sd_data[(i << 2) + 1] & 0x80) >> 2) |  ((sd_data[(i << 2) + 1] & 0x08) << 1) |
			((sd_data[(i << 2) + 2] & 0x80) >> 4) |  ((sd_data[(i << 2) + 2] & 0x08) >> 1) |
			((sd_data[(i << 2) + 3] & 0x80) >> 6) |  ((sd_data[(i << 2) + 3] & 0x08) >> 3);
	}
}




int ambe_q_flush (ambe_q_t * a, int read_fast)
{

	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		a->count = 0;
		a->in_ptr = 0;
		a->out_ptr = 0;
		a->state = (read_fast != 0) ? 1 : 0;
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
	
	ambe_q_flush( a, 0 );
}


int ambe_q_put_sd (ambe_q_t * a,  const uint8_t * data)
{
	int ret = 0;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if ((AMBE_Q_BUFLEN - a->count) >= AMBE_Q_DATASIZE_SD) // there is space in the buffer
		{
			memcpy (a->buf + a->in_ptr, data, AMBE_Q_DATASIZE_SD);
			
			a->in_ptr += AMBE_Q_DATASIZE_SD;
			
			if (a->in_ptr >= AMBE_Q_BUFLEN)
			{
				a->in_ptr = 0;
			}
			
			a->count += AMBE_Q_DATASIZE_SD;
			
			if (a->count >= (AMBE_Q_DATASIZE_SD * 3)) // if 3 ambe records are in memory...
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

int ambe_q_put (ambe_q_t * a,  const uint8_t * data)
{
	uint8_t buf[AMBE_Q_DATASIZE_SD];
	expand_to_sd_data(buf, data);
	return ambe_q_put_sd( a, buf );
}


int ambe_q_get_sd (ambe_q_t * a,  uint8_t * data )
{
	int ret = 0;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if ((a->count > 0) && (a->state == 1)) // there is data in the buffer
		{
			memcpy (data, a->buf + a->out_ptr, AMBE_Q_DATASIZE_SD);
			
			a->out_ptr += AMBE_Q_DATASIZE_SD;
			
			if (a->out_ptr >= AMBE_Q_BUFLEN)
			{
				a->out_ptr = 0;
			}
			
			a->count -= AMBE_Q_DATASIZE_SD;
			
			if (a->count <= 0)
			{
				a->state = 0;
			}
		}		
		else
		{
			expand_to_sd_data( data, silence_data );
			ret = 1;
		}				
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		
		expand_to_sd_data( data, silence_data );
		ret = 1;
	}
	
	return ret;
}

int ambe_q_get (ambe_q_t * a,  uint8_t * data)
{
	uint8_t buf[AMBE_Q_DATASIZE_SD];
	int ret = ambe_q_get_sd( a, buf );
	reduce_sd_data( data, buf );
	return ret;
}
