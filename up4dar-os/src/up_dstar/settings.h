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


#define SETTINGS_VERSION	0x0001

#define NUM_LONG_VALUES		10
#define NUM_SHORT_VALUES	30
#define NUM_CHAR_VALUES		30

#define CALLSIGN_LENGTH		8
#define CALLSIGN_EXT_LENGTH		4
#define DPRS_MSG_LENGTH		13
#define TXMSG_LENGTH		20

#define NUM_RPT_SETTINGS	5
#define NUM_URCALL_SETTINGS		10


typedef union settings_union
{
	uint32_t settings_words[128];
	
	struct {
		int32_t long_values[NUM_LONG_VALUES];
		int16_t short_values[NUM_SHORT_VALUES];
		int8_t char_values[NUM_CHAR_VALUES];
		char my_callsign[CALLSIGN_LENGTH];
		char rpt1[CALLSIGN_LENGTH * NUM_RPT_SETTINGS];
		char rpt2[CALLSIGN_LENGTH * NUM_RPT_SETTINGS];
		char urcall[CALLSIGN_LENGTH * NUM_URCALL_SETTINGS];
		char my_ext[CALLSIGN_EXT_LENGTH];
		char txmsg[TXMSG_LENGTH];
		char dprs_msg[DPRS_MSG_LENGTH];
	} s;
	
} settings_t;


typedef struct {
	int32_t min_value;
	int32_t max_value;
	int32_t init_value;
} limits_t;



extern settings_t settings;

extern const limits_t long_values_limits[NUM_LONG_VALUES];
extern const limits_t short_values_limits[NUM_SHORT_VALUES];
extern const limits_t char_values_limits[NUM_CHAR_VALUES];

// LONG values




// SHORT values
#define S_STANDBY_BEEP_FREQUENCY	0
#define S_STANDBY_BEEP_DURATION		1
#define S_PTT_BEEP_FREQUENCY		2
#define S_PTT_BEEP_DURATION			3
#define S_PHY_TXDELAY				4
#define S_PHY_MATFST				5
#define S_PHY_LENGTHOFVW			6
#define S_PHY_RXDEVFACTOR			7


// CHAR values
#define C_STANDBY_BEEP_VOLUME		0
#define C_PTT_BEEP_VOLUME			1
#define C_PHY_TXGAIN				2
#define C_PHY_RXINV					3
#define C_PHY_TXDCSHIFT				4
#define C_DV_USE_RPTR_SETTING		5
#define C_DV_USE_URCALL_SETTING		6
#define C_DV_DIRECT					7
#define C_DPRS_ENABLED				8
#define C_DPRS_SYMBOL				9
#define C_DISP_CONTRAST				10
#define C_DISP_BACKLIGHT			11


#define SETTING_LONG(a) (settings.s.long_values[a])
#define SETTING_SHORT(a) (settings.s.short_values[a])
#define SETTING_CHAR(a) (settings.s.char_values[a])

void settings_init(void);


#endif /* SETTINGS_H_ */