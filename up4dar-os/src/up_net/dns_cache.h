/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DNS_CACHE_H
#define DNS_CACHE_H

#include "FreeRTOS.h"
#include "gcc_builtin.h"
#include "up_io/eth_txmem.h"
#include "up_net/ipneigh.h"

#define DNS_CACHE_SLOT_NTP    0
#define DNS_CACHE_SLOT_APRS   1

#define DNS_CACHE_SLOT_COUNT  2

typedef void (*dns_cache_handler)(int slot);

void dns_cache_set_slot(int slot, const char* host, dns_cache_handler callback);
int dns_cache_get_address(int slot, uint8_t* address);

void dns_cache_init();

#endif