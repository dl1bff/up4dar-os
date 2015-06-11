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
 * rtclock.h
 *
 * Created: 11.06.2011 12:53:12
 *  Author: mdirska
 */ 


#ifndef RTCLOCK_H_
#define RTCLOCK_H_



#define RTCLOCK_INCR_TICK


void vApplicationTickHook( void );


void rtclock_disp_xy(int x, int y, int dots, int display_seconds);
const char* rtclock_get_time( void );
unsigned long rtclock_get_ticks( void );
long rtclock_get_tx_ticks( void );
void rtclock_reset_tx_ticks( void );
long rtclock_get_rx_ticks( void );
void rtclock_reset_rx_ticks( void );
extern unsigned long volatile the_clock;
void rtclock_set_time(unsigned long time);
#endif /* RTCLOCK_H_ */