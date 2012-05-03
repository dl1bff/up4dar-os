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
 * snmp.h
 *
 * Created: 03.05.2012 11:04:56
 *  Author: mdirska
 */ 


#ifndef SNMP_H_
#define SNMP_H_


int snmp_process_request( const uint8_t * req, int req_len, uint8_t * response );


#endif /* SNMP_H_ */
