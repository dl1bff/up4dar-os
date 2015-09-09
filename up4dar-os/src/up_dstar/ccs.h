/*

Copyright (C) 2015   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * ccs.h
 *
 * Created: 03.09.2015 09:01:20
 *  Author: mdirska
 */ 


#ifndef CCS_H_
#define CCS_H_


void ccs_init(void);
void ccs_service (void);
const char * ccs_current_servername(void);
int ccs_is_connected (void);
void ccs_input_packet ( const uint8_t * data, int data_len, const uint8_t * ipv4_src_addr);
void ccs_start (void);
void ccs_stop(void);



void ccs_send_info(const uint8_t * mycall, const uint8_t * mycall_ext, int remove_module_char);
#endif /* CCS_H_ */