
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
 * gps.h
 *
 * Created: 26.05.2012 15:19:04
 *  Author: mdirska
 */ 


#ifndef GPS_H_
#define GPS_H_


void gps_init(void);
int gps_get_slow_data(uint8_t * slow_data);
void gps_reset_slow_data(void);

#endif /* GPS_H_ */
