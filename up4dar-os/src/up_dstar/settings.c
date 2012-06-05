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
 * settings.c
 *
 * Created: 04.06.2012 17:53:43
 *  Author: mdirska
 */ 



#include "FreeRTOS.h"

#include "gcc_builtin.h"

#include "settings.h"


settings_t settings;

const limits_t long_values_limits[NUM_LONG_VALUES] = {
	{  0,  0,  0  }	
};

const limits_t short_values_limits[NUM_SHORT_VALUES] = {
	// #define S_STANDBY_BEEP_FREQUENCY		0
	{  200,		3000,		1000  },
	// #define S_STANDBY_BEEP_DURATION		1
	{  20,		500,		100  },
	// #define S_PTT_BEEP_FREQUENCY			2
	{  200,		3000,		600  },
	// #define S_PTT_BEEP_DURATION			3
	{  20,		500,		100  }
};

const limits_t char_values_limits[NUM_CHAR_VALUES] = {
	// #define C_STANDBY_BEEP_VOLUME		0
	{  0,		100,		10  },
	// #define C_PTT_BEEP_VOLUME			1
	{  0,		100,		50  }	
};


static uint32_t calc_chksum(void)
{
	uint32_t  v = 0x837454A1;
	
	int i;
	
	for (i=0; i < 127; i++)
	{
		v ^= settings.settings_words[i];
	}
	
	return v;
}


void settings_init(void)
{
	memcpy(& settings, (const void *) AVR32_FLASHC_USER_PAGE, 512);
	
	uint32_t chk = calc_chksum();
	
	if (chk != settings.settings_words[127])  // checksum wrong, set default values
	{
		int i;
		
		for (i=0; i < NUM_LONG_VALUES; i++)
		{
			settings.s.long_values[i] = long_values_limits[i].init_value;
		}
		
		for (i=0; i < NUM_SHORT_VALUES; i++)
		{
			settings.s.short_values[i] = short_values_limits[i].init_value;
		}
		
		for (i=0; i < NUM_CHAR_VALUES; i++)
		{
			settings.s.char_values[i] = char_values_limits[i].init_value;
		}
	}
}