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

#ifndef TIMER_H
#define TIMER_H

#include "FreeRTOS.h"
#include "gcc_builtin.h"

#define TIMER_SLOT_DNS_CACHE    0
#define TIMER_SLOT_APRS_BEACON  1

#define TIMER_SLOT_COUNT        2

typedef void (*timer_handler)(int slot);

void timer_set_slot(int slot, int interval, timer_handler callback);

void timer_init();

#endif