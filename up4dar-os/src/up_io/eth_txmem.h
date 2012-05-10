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
 * eth_txmem.h
 *
 * Created: 10.05.2012 09:09:22
 *  Author: mdirska
 */ 


#ifndef ETH_TXMEM_H_
#define ETH_TXMEM_H_

typedef struct eth_txmem
{
	uint8_t * data;
	uint16_t tx_size;
	uint16_t state;
	
} eth_txmem_t;


int eth_txmem_init(void);
eth_txmem_t * eth_txmem_get (int size);
int eth_txmem_send (eth_txmem_t * packet);
void eth_txmem_flush_q (void);
void eth_txmem_free (eth_txmem_t * packet);


#endif /* ETH_TXMEM_H_ */