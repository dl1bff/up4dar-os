#include "audio_q.h"
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
 * audio_q.c
 *
 * Created: 25.04.2012 10:14:57
 *  Author: mdirska
 */ 

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#include "audio_q.h"


void audio_q_initialize (audio_q_t * a)
{
	a->mutex = xSemaphoreCreateMutex();
	a->count = 0;
	a->in_ptr = 0;
	a->out_ptr = 0;
}


int audio_error = 0;

int audio_max = 0;


void audio_q_put (audio_q_t * a,  const short * samples)
{
	int i;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if ((AUDIO_Q_BUFLEN - a->count) >= AUDIO_Q_TRANSFERLEN) // there is space in the buffer
		{
			for (i=0; i < AUDIO_Q_TRANSFERLEN; i++)
			{
				if (samples[i] > audio_max)
				{
					audio_max = samples[i];
				}
				
				a->buf[a->in_ptr] = samples[i];
				a->in_ptr ++;
				if (a->in_ptr >= AUDIO_Q_BUFLEN)
				{
					a->in_ptr = 0;
				}
			}
			a->count += AUDIO_Q_TRANSFERLEN;
			
			if (a->count < ((AUDIO_Q_TRANSFERLEN*2) -2) )  // add one sample
			{
				a->buf[a->in_ptr] = samples[AUDIO_Q_TRANSFERLEN -1];
				a->in_ptr ++;
				if (a->in_ptr >= AUDIO_Q_BUFLEN)
				{
					a->in_ptr = 0;
				}
				a->count ++;
				
				audio_error ++;
			}
			else if (a->count > (AUDIO_Q_BUFLEN - AUDIO_Q_TRANSFERLEN + 2)) // delete last sample
			{
				a->in_ptr --;
				if (a->in_ptr < 0)
				{
					a->in_ptr = AUDIO_Q_BUFLEN -1;
				}
				a->count --;
				
				audio_error ++;
			}
		}
		else
		{
				audio_error ++;
		}			
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		
	}
}



static void fill_with_zeros (short * samples)
{
	int i;
	
	for (i=0; i < AUDIO_Q_TRANSFERLEN; i++)
	{
		samples[i] = 0;
	}
}



void audio_q_get (audio_q_t * a,  short * samples)
{
	int i;
	
	if( xSemaphoreTake( a->mutex, 0 ) == pdTRUE )  // get Mutex, don't wait
    {
		if (a->count >= AUDIO_Q_TRANSFERLEN) // there is data in the buffer
		{
			for (i=0; i < AUDIO_Q_TRANSFERLEN; i++)
			{
				samples[i] = a->buf[a->out_ptr];
				a->out_ptr ++;
				if (a->out_ptr >= AUDIO_Q_BUFLEN)
				{
					a->out_ptr = 0;
				}
			}
			a->count -= AUDIO_Q_TRANSFERLEN;
		}		
		else
		{
			fill_with_zeros(samples);
			
		}				
        xSemaphoreGive( a->mutex );
    }
	else
	{
		// should not happen: could not get Mutex
		fill_with_zeros(samples);
	
	}
}

