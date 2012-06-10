/*

Copyright (C) 2011,2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * rx_dstar_crc_header.h
 *
 * Created: 17.04.2011 11:14:18
 *  Author: mdirska
 */ 


#ifndef RX_DSTAR_CRC_HEADER_H_
#define RX_DSTAR_CRC_HEADER_H_


unsigned short rx_dstar_crc_header(const unsigned char* header);
unsigned short rx_dstar_crc_data(const unsigned char* data, int num);

#endif /* RX_DSTAR_CRC_HEADER_H_ */
