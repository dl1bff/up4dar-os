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
 * a_lib.h
 *
 * Created: 08.07.2012 12:33:52
 *  Author: mdirska
 */ 


#ifndef A_LIB_H_
#define A_LIB_H_

#define A_KEY_BUTTON_1		1
#define A_KEY_BUTTON_2		2
#define A_KEY_BUTTON_3		3
#define A_KEY_BUTTON_UP		4
#define A_KEY_BUTTON_DOWN	5

#define A_KEY_PRESSED		1
#define A_KEY_RELEASED		2
#define A_KEY_HOLD_500MS	3
#define A_KEY_REPEAT		4
#define A_KEY_HOLD_2S		5


void a_set_app_name ( void * app_context, const char * app_name);
void a_set_key_event_handler ( void * app_context, void (*key_event_handler) (void * a, int key_num, int key_event));
void a_set_private_data ( void * app_context, void * priv );
void * a_get_private_data ( void * app_context );
void * a_malloc ( void * app_context, int num_bytes );


#endif /* A_LIB_H_ */
