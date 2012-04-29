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
 * dstar.h
 *
 * Created: 03.04.2011 11:21:51
 *  Author: mdirska
 */ 


#ifndef DSTAR_H_
#define DSTAR_H_


struct dstarPacket
{
	unsigned char cmdByte;
	unsigned char dataLen;
	unsigned char data[100];
};


void dstarInit(xQueueHandle dstarQueue);

void dstarChangeMode(int m);

void dstarResetCounters(void);

void dstarProcessDCSPacket( const uint8_t * data );

#endif /* DSTAR_H_ */