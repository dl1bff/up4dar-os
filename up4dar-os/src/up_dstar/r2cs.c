/*

Copyright (C) 2014   Ralf Ballis, DL2MRB (dl2mrb@mnet-mail.de)

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
 * r2cs.c
 *
 * Created: 04.03.2014 08:37:09
 *  Author: rballis
 */ 
#include "FreeRTOS.h"
#include "vdisp.h"
#include "r2cs.h"
#include "settings.h"
#include "up_dstar/r2cs.h"

#include "gcc_builtin.h"

char urcall_history[CALLSIGN_LENGTH * R2CS_HISTORY_DIM];
bool urcall_from_r2cs = false;
int history_max = -1;
int urcall_position = 0;

void r2cs(int layer, int position)
{
	char urcall[CALLSIGN_LENGTH + 1];

	memset(urcall, '\0', (CALLSIGN_LENGTH + 1));

	vd_clear_rect(layer, 8, 12, 146, 43);
	
	for (int i = 30; i < 88; i++)
	{
		vd_set_pixel(layer, i, 24, 0, 1, 1);
		vd_set_pixel(layer, i, 35, 0, 1, 1);
		vd_set_pixel(layer, i + 1, 36, 0, 1, 1);
	}
	for (int i = 24; i < 35; i++)
	{
		vd_set_pixel(layer, 30, i, 0, 1, 1);
		vd_set_pixel(layer, 87, i, 0, 1, 1);
		vd_set_pixel(layer, 88, i + 1, 0, 1, 1);
	}

	if (position <= history_max && position >= 0 && history_max >= 0)
	{
		urcall_from_r2cs = true;
		urcall_position = position;
		memcpy(urcall, urcall_history + (position * CALLSIGN_LENGTH), CALLSIGN_LENGTH);
	}
	else
	{
		urcall_from_r2cs = false;
		SETTING_CHAR(C_DV_USE_URCALL_SETTING  ) = 1;
		memcpy(urcall, settings.s.urcall + ((SETTING_CHAR(C_DV_USE_URCALL_SETTING  ) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
	}
	
	vd_prints_xy(layer, 32, 26, VDISP_FONT_5x8, 0, "ur");
	vd_prints_xy(layer, 42, 26, VDISP_FONT_5x8, 0, urcall);
}

bool r2csURCALL(void)
{
	return urcall_from_r2cs;
}

int r2cs_count(void)
{
	return history_max;
}

int r2cs_position(void)
{
	return urcall_position;
}

char* r2cs_get(int position)
{
	if (position <= history_max)
		return urcall_history + (position * CALLSIGN_LENGTH);
	
	return NULL;
}

void r2cs_append(const char urcall[8])
{
	for (int i = 0; i <= history_max; i++)
	{
		if (memcmp(urcall_history + (i * CALLSIGN_LENGTH), urcall, CALLSIGN_LENGTH) == 0)
			return;
	}

	if (history_max < (R2CS_HISTORY_DIM - 1))
		history_max++;
	
	if (history_max > 0)
	{
		for (int i = history_max; i > 0 ; i--)
		{
			memcpy(urcall_history + (i * CALLSIGN_LENGTH), urcall_history + ((i - 1) * CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		}
	}

	memcpy(urcall_history + (0 * CALLSIGN_LENGTH), urcall, CALLSIGN_LENGTH);
}

void r2cs_print(int layer, int position)
{
	char urcall[CALLSIGN_LENGTH + 1];
	char tmp_buf[7];
	int y_pos = 12;
	int disp_inverse = 0;

	vd_clear_rect(layer, 0, y_pos, 145, 43);
	
	vd_printc_xy(layer, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(layer, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
				
	for (int i = 30; i < 83; i++)
	{
		vd_set_pixel(layer, i, y_pos, 0, 1, 1);
		vd_set_pixel(layer, i, (y_pos + 10), 0, 1, 1);
		vd_set_pixel(layer, i, ((y_pos + 8) + (10 * 3)), 0, 1, 1);
		vd_set_pixel(layer, i + 1, (((y_pos + 8) + (10 * 3)) + 1), 0, 1, 1);
	}
	
	for (int i = y_pos; i < ((y_pos + 8) + (10 * 3)); i++)
	{
		vd_set_pixel(layer, 30, i, 0, 1, 1);
		vd_set_pixel(layer, 82, i, 0, 1, 1);
		vd_set_pixel(layer, 83, i + 1, 0, 1, 1);
	}
	
	y_pos += 4;
	vd_prints_xy(layer, 32, y_pos, VDISP_FONT_4x6, 0, "RX HISTORY");
	
	for (int i = position; (i <= r2cs_count() && i < (position + 3)); i++)
	{
		y_pos += 8;

		if (i == position)
			disp_inverse = 1;
		else
			disp_inverse = 0;
					
		vdisp_i2s(tmp_buf, 1, 10, 1, (i + 1) );

		memset(urcall, '\0', (CALLSIGN_LENGTH + 1));
		memcpy(urcall, urcall_history + (i * CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		
		vd_prints_xy(layer, 32, y_pos + 2, VDISP_FONT_4x6, 0, tmp_buf);
		vd_printc_xy(layer, 36, y_pos + 2, VDISP_FONT_4x6, 0, ':');
		vd_prints_xy(layer, 40, y_pos + 1, VDISP_FONT_5x8, disp_inverse, urcall);
	}				
}
