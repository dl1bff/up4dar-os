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
 * slowdata.h
 *
 * Created: 11.06.2015 14:44:55
 *  Author: mdirska
 */ 


#ifndef SLOWDATA_H_
#define SLOWDATA_H_

void slowdata_data_input( unsigned char * data, unsigned char len );
void slowdataInit(void);
void slowdata_analyze_stream(void);

extern char * slowDataGPSA;

#endif /* SLOWDATA_H_ */
