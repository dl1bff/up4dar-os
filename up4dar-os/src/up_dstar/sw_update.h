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
 * sw_update.h
 *
 * Created: 24.10.2012 18:13:11
 *  Author: mdirska
 */ 


#ifndef SW_UPDATE_H_
#define SW_UPDATE_H_

extern unsigned char software_version[];

int sw_update_pending(void);
void sw_update_init(xQueueHandle dq );
void version2string (char * buf, const unsigned char * version_info);

#endif /* SW_UPDATE_H_ */
