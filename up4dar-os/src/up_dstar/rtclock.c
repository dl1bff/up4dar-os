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
 * rtclock.c
 *
 * Created: 11.06.2011 12:52:33
 *  Author: mdirska
 */ 



#include "rtclock.h"

#include "FreeRTOS.h"
#include "vdisp.h"

static unsigned long the_clock;
static unsigned short rtclock_ticks;

static long tx_ticks;
static long rx_ticks;

void vApplicationTickHook( void )
{
	rtclock_ticks ++;
	tx_ticks ++;
	rx_ticks ++;
	
	if (rtclock_ticks >= configTICK_RATE_HZ)
	{
		rtclock_ticks = 0;
		the_clock ++;
	}
}

unsigned long rtclock_get_ticks( void )
{
	return (the_clock * configTICK_RATE_HZ) + rtclock_ticks;
}


long rtclock_get_tx_ticks( void )
{
	return tx_ticks;
}

void rtclock_reset_tx_ticks( void )
{
	tx_ticks = 0;
}

long rtclock_get_rx_ticks( void )
{
	return rx_ticks;
}

void rtclock_reset_rx_ticks( void )
{
	rx_ticks = 0;
}

void rtclock_disp_xy(int x, int y, int dots, int display_seconds)
{
	unsigned int m = the_clock / 60;
	
	unsigned int minutes = m % 60;
	
	unsigned int h = m / 60;
	
	unsigned int hours = h % 24;
	
	char buf[3];
	
	vdisp_i2s(buf, 2, 10, 1, hours);
	vdisp_prints_xy(x, y, VDISP_FONT_6x8, 0, buf);

	vdisp_set_pixel(x + 12, y + 0, 0, 0, 4);
	vdisp_set_pixel(x + 12, y + 1, 0, dots, 4);
	vdisp_set_pixel(x + 12, y + 2, 0, 0, 4);
	vdisp_set_pixel(x + 12, y + 3, 0, 0, 4);
	vdisp_set_pixel(x + 12, y + 4, 0, 0, 4);
	vdisp_set_pixel(x + 12, y + 5, 0, dots, 4);
	vdisp_set_pixel(x + 12, y + 6, 0, 0, 4);
	vdisp_set_pixel(x + 12, y + 7, 0, 0, 4);
	
	vdisp_i2s(buf, 2, 10, 1, minutes);
	vdisp_prints_xy(x + 16, y, VDISP_FONT_6x8, 0, buf);
	
	if (display_seconds != 0)
	{
		vdisp_set_pixel(x + 28, y + 0, 0, 0, 4);
		vdisp_set_pixel(x + 28, y + 1, 0, dots, 4);
		vdisp_set_pixel(x + 28, y + 2, 0, 0, 4);
		vdisp_set_pixel(x + 28, y + 3, 0, 0, 4);
		vdisp_set_pixel(x + 28, y + 4, 0, 0, 4);
		vdisp_set_pixel(x + 28, y + 5, 0, dots, 4);
		vdisp_set_pixel(x + 28, y + 6, 0, 0, 4);
		vdisp_set_pixel(x + 28, y + 7, 0, 0, 4);
	
		unsigned int seconds = the_clock % 60;
		
		vdisp_i2s(buf, 2, 10, 1, seconds);
		vdisp_prints_xy(x + 32, y, VDISP_FONT_6x8, 0, buf);
	}
}


void rtclock_set_time(unsigned long time)
{
	the_clock = time;
}
