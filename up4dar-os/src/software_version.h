/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * software_version.h
 *
 * Created: 25.08.2012 18:50:52
 *  Author: mdirska
 */ 


#ifndef SOFTWARE_VERSION_H_
#define SOFTWARE_VERSION_H_

#define SOFTWARE_IMAGE_PHY			1
#define SOFTWARE_IMAGE_UPDATER		2
#define SOFTWARE_IMAGE_SYSTEM(a)		(3 | ((a) << 2))

#define SOFTWARE_MATURITY_NORMAL		0x00
#define SOFTWARE_MATURITY_BETA			0x80
#define SOFTWARE_MATURITY_EXPERIMENTAL	0x40

#define SOFTWARE_IMAGE_SYSTEM_LETTERS		"SRQONMLKJIHGEDBA"


#define SWVER_BYTE0		(SOFTWARE_IMAGE_SYSTEM(0) | SOFTWARE_MATURITY_EXPERIMENTAL)
//#define SWVER_BYTE0		(SOFTWARE_IMAGE_SYSTEM(0) | SOFTWARE_MATURITY_NORMAL)
//#define SWVER_BYTE0		(SOFTWARE_IMAGE_SYSTEM(5) | SOFTWARE_MATURITY_BETA )
#define SWVER_BYTE1		1
#define SWVER_BYTE2		1
#define SWVER_BYTE3		41

#define SWVER_STRING "S.1.01.41e"

#endif /* SOFTWARE_VERSION_H_ */