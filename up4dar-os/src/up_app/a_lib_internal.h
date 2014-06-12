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
 * a_lib_internal.h
 *
 * Created: 08.07.2012 14:31:42
 *  Author: mdirska
 */ 


#ifndef A_LIB_INTERNAL_H_
#define A_LIB_INTERNAL_H_


#define A_KEY_BUTTON_APP_MANAGER  0


void a_dispatch_key_event( int layer_num, int key_num, int key_event );

void a_run_app ( void (*app_main) (void * app_context));

void a_app_manager_init(void);

void a_app_manager_service(void);

extern char dcs_mode;
extern char hotspot_mode;
extern char repeater_mode;
extern char parrot_mode;


#endif /* A_LIB_INTERNAL_H_ */