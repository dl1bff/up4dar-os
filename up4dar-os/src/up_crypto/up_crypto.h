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
 * up_crypto.h
 *
 * Created: 19.08.2012 10:28:56
 *  Author: mdirska
 */ 


#ifndef UP_CRYPTO_H_
#define UP_CRYPTO_H_



int crypto_get_random_bytes (unsigned char * dest, int num_bytes);
int crypto_get_random_15bit(void);
int crypto_get_random_16bit(void);

#endif /* UP_CRYPTO_H_ */
