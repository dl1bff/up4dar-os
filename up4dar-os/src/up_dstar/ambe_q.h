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
 * ambe_q.h
 *
 * Created: 28.04.2012 17:14:47
 *  Author: mdirska
 */ 


#ifndef AMBE_Q_H_
#define AMBE_Q_H_



#include <FreeRTOS.h>

#include "semphr.h"

#define AMBE_Q_DATASIZE  9

#define AMBE_Q_DATASIZE_SD  (AMBE_Q_DATASIZE * 4)

#define AMBE_Q_BUFLEN  (AMBE_Q_DATASIZE_SD * 50)

struct ambe_q {
	uint8_t buf[AMBE_Q_BUFLEN];
	short in_ptr;
	short out_ptr;
	short count;
	short state;
	xSemaphoreHandle mutex;
};

typedef struct ambe_q ambe_q_t;

void ambe_q_initialize (ambe_q_t * a);
int ambe_q_put (ambe_q_t * a, const uint8_t * data);
int ambe_q_get (ambe_q_t * a, uint8_t * data);

int ambe_q_flush (ambe_q_t * a, int read_fast);
int ambe_q_put_sd (ambe_q_t * a, const uint8_t * data);
int ambe_q_get_sd (ambe_q_t * a, uint8_t * data );

#endif /* AMBE_Q_H_ */