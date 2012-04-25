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
 * audio_q.h
 *
 * Created: 25.04.2012 10:14:39
 *  Author: mdirska
 */ 


#ifndef AUDIO_Q_H_
#define AUDIO_Q_H_


#include <FreeRTOS.h>

#include "semphr.h"

#define AUDIO_Q_TRANSFERLEN 16
#define AUDIO_Q_BUFLEN  (AUDIO_Q_TRANSFERLEN * 4)

struct audio_q {
	short buf[AUDIO_Q_BUFLEN];
	short in_ptr;
	short out_ptr;
	short count;
	xSemaphoreHandle mutex;
};

typedef struct audio_q audio_q_t;

void audio_q_initialize (audio_q_t * a);
void audio_q_put (audio_q_t * a, const short * samples);
void audio_q_get (audio_q_t * a, short * samples);

#endif /* AUDIO_Q_H_ */