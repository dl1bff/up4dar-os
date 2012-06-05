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
 * settings.h
 *
 * Created: 04.06.2012 17:53:57
 *  Author: mdirska
 */ 


#ifndef SETTINGS_H_
#define SETTINGS_H_


#define NUM_LONG_VALUES		30
#define NUM_SHORT_VALUES	30
#define NUM_CHAR_VALUES		30


typedef union settings_union
{
	uint32_t settings_words[128];
	
	struct {
		int32_t long_values[30];
		int16_t short_values[30];
		int8_t char_values[30];
		char my_callsign[8];
	} s;
	
} settings_t;


typedef struct {
	int32_t min_value;
	int32_t max_value;
	int32_t init_value;
} limits_t;



extern settings_t settings;

extern const limits_t long_values_limits[NUM_LONG_VALUES];
extern const limits_t short_values_limits[NUM_LONG_VALUES];
extern const limits_t char_values_limits[NUM_LONG_VALUES];

// LONG values




// SHORT values
#define S_STANDBY_BEEP_FREQUENCY	0
#define S_STANDBY_BEEP_DURATION		1
#define S_PTT_BEEP_FREQUENCY		2
#define S_PTT_BEEP_DURATION			3


// CHAR values
#define C_STANDBY_BEEP_VOLUME		0
#define C_PTT_BEEP_VOLUME			1




#define SETTING_LONG(a) (settings.s.long_values[a])
#define SETTING_SHORT(a) (settings.s.short_values[a])
#define SETTING_CHAR(a) (settings.s.char_values[a])

void settings_init(void);


#endif /* SETTINGS_H_ */