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
 * ambe.h
 *
 * Created: 18.04.2012 16:40:26
 *  Author: mdirska
 */ 


#ifndef AMBE_H_
#define AMBE_H_

#include "up_dstar/audio_q.h"
#include "up_dstar/ambe_q.h"

void ambe_start_encode(void);
void ambe_stop_encode(void);


void ambe_input_data( const uint8_t * d);
void ambe_input_data_sd( const uint8_t * d);
void ambe_init( audio_q_t * decoded_audio, audio_q_t * input_audio, ambe_q_t * microphone );
void ambe_set_automute(int enable);
void ambe_set_autoaprs(int enable);
int ambe_get_automute(void);
int ambe_get_autoaprs(void);
int ambe_get_ref_timer(void);
void ambe_set_ref_timer(int enable);
void ambe_ref_timer_break(int enable);
void ambe_service(void);
#endif /* AMBE_H_ */
