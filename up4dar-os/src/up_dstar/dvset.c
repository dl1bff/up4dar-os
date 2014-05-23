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
 * call.c
 *
 * Created: 14.02.2014 23:19:25
 *  Author: rballis
 */ 
#include "FreeRTOS.h"
#include "vdisp.h"
#include "dvset.h"
#include "urcall.h"
#include "settings.h"
#include "vdisp.h"

#include "gcc_builtin.h"

char edit_str[DVSET_LINE_LENGTH];
		
bool dvset_selected = false;
bool dvset_edit = false;
int dvset_idx = 0;
int feld_idx = 0;
int sub_feld_idx = 0;

int cursor_idx = 0;

int ur_invers = 1;
int rpt_invers = 0;
int my_invers = 0;
int my_ext_invers = 0;
int txmsg_invers = 0;

void dvset_field(int act)
{
	if (dvset_isedit())
	{
		if (act == 1)
		{
			if ((dvset_idx != 4) && (edit_str[cursor_idx] == ' '))
			{
				edit_str[cursor_idx] = 'Z';
				return;
			}
			else if (edit_str[cursor_idx] == ' ')
			{
				edit_str[cursor_idx] = '}';
				return;
			}
			else if ((dvset_idx != 4) && (edit_str[cursor_idx] == 'A'))
			{
				edit_str[cursor_idx] = '9';
				return;
			}
			else if ((dvset_idx != 4) && (edit_str[cursor_idx] == '/'))
			{
				edit_str[cursor_idx] = ' ';
				return;
			}
			
			edit_str[cursor_idx]--;
		}
		else if (act == 0)
		{
			if ((dvset_idx != 4) && (edit_str[cursor_idx] == 'Z'))
			{
				edit_str[cursor_idx] = ' ';
				return;
			}
			else if (edit_str[cursor_idx] == '}')
			{
				edit_str[cursor_idx] = ' ';
				return;
			}
			else if ((dvset_idx != 4) && (edit_str[cursor_idx] == '9'))
			{
				edit_str[cursor_idx] = 'A';
				return;
			}
			else if ((dvset_idx != 4) && (edit_str[cursor_idx] == ' '))
			{
				edit_str[cursor_idx] = '/';
				return;
			}
			
			edit_str[cursor_idx]++;
		}
	}
	else
	{
		if (!dvset_isselected())
		{
			if (act == 0)
			{
				if (dvset_idx > 0)
					dvset_idx--;
				else
					dvset_idx = 4;
			}
			else if (act == 1)
			{
				if (dvset_idx < 4)
					dvset_idx++;
				else
					dvset_idx = 0;
			}
		
			feld_idx = 0;
			sub_feld_idx = 0;
		
			ur_invers = 0;
			rpt_invers = 0;
			my_invers = 0;
			my_ext_invers = 0;
			txmsg_invers = 0;
		
			if (dvset_idx == 0)
				ur_invers = 1;
			else if (dvset_idx == 1)
				rpt_invers = 1;
			else if (dvset_idx == 2)
				my_invers = 1;
			else if (dvset_idx == 3)
				my_ext_invers = 1;
			else if (dvset_idx == 4)
				txmsg_invers = 1;
		}
		else
		{
			if (dvset_idx == 0)
			{
				if ((act == 0) && (feld_idx > 0))
					feld_idx--;
				else if ((act == 1) && (feld_idx < (NUM_URCALL_SETTINGS -1)))
					feld_idx++;
			}
			else if (dvset_idx == 1)
			{
				if (act == 0)
				{
					if ((sub_feld_idx == 0) && (feld_idx >= 0))
					{
						sub_feld_idx = 1;
						feld_idx--;
					}
					else
					{
						sub_feld_idx = 0;
					}
				}
				else if (act == 1)
				{
					if (sub_feld_idx == 0)
					{
						sub_feld_idx = 1;
					}
					else if (feld_idx < (NUM_RPT_SETTINGS - 1))
					{
						sub_feld_idx = 0;
						feld_idx++;
					}
				}
			}
		}
	}
}

void dvset_cursor(int act)
{
	if ((dvset_idx == 0) || (dvset_idx == 1) || (dvset_idx == 2))
	{
		if ((act == 0) && (cursor_idx > 0))
			cursor_idx--;
		else if ((act == 1) && (cursor_idx < (CALLSIGN_LENGTH -1)))
			cursor_idx++;
	}
	else if (dvset_idx == 3)
	{
		if ((act == 0) && (cursor_idx > 0))
			cursor_idx--;
		else if ((act == 1) && (cursor_idx < (CALLSIGN_EXT_LENGTH -1)))
			cursor_idx++;
	}
	else if (dvset_idx == 4)
	{
		if ((act == 0) && (cursor_idx > 0))
			cursor_idx--;
		else if ((act == 1) && (cursor_idx < (TXMSG_LENGTH -1)))
			cursor_idx++;
	}
}

void dvset_select(bool select)
{
	dvset_selected = select;
	
	if (select)
	{
		if (dvset_idx == 0)
			feld_idx = (SETTING_CHAR(C_DV_USE_URCALL_SETTING) - 1);
		else if (dvset_idx == 1)
			feld_idx = (SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1);
	}
	else
	{
		if (dvset_idx == 0)
		{
			SETTING_CHAR(C_DV_USE_URCALL_SETTING) = (feld_idx + 1);
		}
		else if (dvset_idx == 1)
		{
			if (feld_idx < 0)
			{
				SETTING_CHAR(C_DV_DIRECT) = 1;
			}
			else
			{
				SETTING_CHAR(C_DV_USE_RPTR_SETTING) = (feld_idx + 1);
				SETTING_CHAR(C_DV_DIRECT) = 0;				
			}
		}
		
		feld_idx = 0;
		sub_feld_idx = 0;
		
		//Schreiben in den Flash- Speicher
		settings_write();
	}
}

void dvset_cancel(void)
{
	dvset_selected = false;
	feld_idx = 0;
	sub_feld_idx = 0;
}

void dvset_clear(void)
{
	if (dvset_idx == 0)
	{
		memset(settings.s.urcall + (feld_idx * CALLSIGN_LENGTH), '\x20', CALLSIGN_LENGTH);
	}
	else if (dvset_idx == 1)
	{
		if (sub_feld_idx == 0)
			memset(settings.s.rpt1 + (feld_idx * CALLSIGN_LENGTH), '\x20', CALLSIGN_LENGTH);
		else if (sub_feld_idx == 1)
			memset(settings.s.rpt2 + (feld_idx * CALLSIGN_LENGTH), '\x20', CALLSIGN_LENGTH);
	}
	else if (dvset_idx == 2)
	{
		memcpy(settings.s.my_callsign, "NOCALL  ", CALLSIGN_LENGTH);
	}
	else if (dvset_idx == 3)
	{
		memset(settings.s.my_ext, '\x20', CALLSIGN_EXT_LENGTH);
	}
	else if (dvset_idx == 4)
	{
		memset(settings.s.txmsg, '\x20', TXMSG_LENGTH);
	}
}

void dvset_store(void)
{
	if (dvset_isedit())
	{
		dvset_edit = false;
		cursor_idx = 0;

		if (dvset_idx == 0)
		{
			memcpy(settings.s.urcall + (feld_idx * CALLSIGN_LENGTH), edit_str, CALLSIGN_LENGTH);
		}
		else if (dvset_idx == 1)
		{
			if (sub_feld_idx == 0)
				memcpy(settings.s.rpt1 + (feld_idx * CALLSIGN_LENGTH), edit_str, CALLSIGN_LENGTH);
			else if (sub_feld_idx == 1)
				memcpy(settings.s.rpt2 + (feld_idx * CALLSIGN_LENGTH), edit_str, CALLSIGN_LENGTH);
		}
		else if (dvset_idx == 2)
		{
			memcpy(settings.s.my_callsign, edit_str, CALLSIGN_LENGTH);
		}
		else if (dvset_idx == 3)
		{
			memcpy(settings.s.my_ext, edit_str, CALLSIGN_EXT_LENGTH);
		}
		else if (dvset_idx == 4)
		{
			memcpy(settings.s.txmsg, edit_str, TXMSG_LENGTH);
		}

		memset(edit_str, '\x20', DVSET_LINE_LENGTH);
	}
}

void dvset_goedit(void)
{
	dvset_edit = true;

	memset(edit_str, '\x20', DVSET_LINE_LENGTH);
	
	if (dvset_idx == 0)
	{
		memcpy(edit_str, settings.s.urcall + (feld_idx * CALLSIGN_LENGTH), CALLSIGN_LENGTH);		
	}
	else if (dvset_idx == 1)
	{
		if (sub_feld_idx == 0)
			memcpy(edit_str, settings.s.rpt1 + (feld_idx * CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		else if (sub_feld_idx == 1)
			memcpy(edit_str, settings.s.rpt2 + (feld_idx * CALLSIGN_LENGTH), CALLSIGN_LENGTH);
	}
	else if (dvset_idx == 2)
	{
		memcpy(edit_str, settings.s.my_callsign, CALLSIGN_LENGTH);
	}
	else if (dvset_idx == 3)
	{
		memcpy(edit_str, settings.s.my_ext, CALLSIGN_EXT_LENGTH);
	}
	else if (dvset_idx == 4)
	{
		memcpy(edit_str, settings.s.txmsg, TXMSG_LENGTH);
	}
}

void dvset_backspace(void)
{
	if (cursor_idx == 0) return;
	
	for (int i = cursor_idx; i < DVSET_LINE_LENGTH; i++)
	{
		edit_str[i - 1] = edit_str[i];
		edit_str[i] = ' ';
	}
	
	cursor_idx--;
}

bool dvset_isselected(void)
{
	return dvset_selected;
}

bool dvset_isedit(void)
{
	return dvset_edit;
}

void dvset(void)
{
	if (!dvset_isselected())
	{
		dvset_print();
	}
}
	
void dvset_print(void)
{
	char str[DVSET_LINE_LENGTH];
		
	vd_clear_rect(VDISP_DVSET_LAYER, 0, 12, 146, 43);
		
	vd_printc_xy(VDISP_DVSET_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_DVSET_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	memset(str, '\0', DVSET_LINE_LENGTH);
		
	memcpy(str, settings.s.urcall + ((SETTING_CHAR(C_DV_USE_URCALL_SETTING) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		
	vd_prints_xy(VDISP_DVSET_LAYER, 0, 12, VDISP_FONT_5x8, ur_invers, "ur");
	vd_prints_xy(VDISP_DVSET_LAYER, 10, 12, VDISP_FONT_6x8, ur_invers, str);
		
	vd_prints_xy(VDISP_DVSET_LAYER, 0, 23, VDISP_FONT_5x8, rpt_invers, "R1");
	vd_prints_xy(VDISP_DVSET_LAYER, 63, 23, VDISP_FONT_5x8, rpt_invers, "R2");
		
	if (SETTING_CHAR(C_DV_DIRECT) != 1)
	{
		memset(str, '\0', DVSET_LINE_LENGTH);
			
		memcpy(str, settings.s.rpt1 + ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
			
		vd_prints_xy(VDISP_DVSET_LAYER, 10, 23, VDISP_FONT_6x8, rpt_invers, str);
			
		memset(str, '\0', DVSET_LINE_LENGTH);
			
		memcpy(str, settings.s.rpt2 + ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
			
		vd_prints_xy(VDISP_DVSET_LAYER, 73, 23, VDISP_FONT_6x8, rpt_invers, str);
	}
	else
	{
		vd_prints_xy(VDISP_DVSET_LAYER, 10, 23, VDISP_FONT_6x8, rpt_invers, "DIRECT  ");
		vd_prints_xy(VDISP_DVSET_LAYER, 73, 23, VDISP_FONT_6x8, rpt_invers, "DIRECT  ");
	}
		
	memset(str, '\0', DVSET_LINE_LENGTH);
	vd_prints_xy(VDISP_DVSET_LAYER, 0, 34, VDISP_FONT_5x8, 0, "my");

	if (dvset_isedit() && (dvset_idx == 2))
	{
		memcpy(str, edit_str, CALLSIGN_LENGTH);
		vd_prints_xy_inverse(VDISP_DVSET_LAYER, 10, 34, VDISP_FONT_6x8, cursor_idx, str);
	}
	else
	{
		memcpy(str, settings.s.my_callsign, CALLSIGN_LENGTH);
		vd_prints_xy(VDISP_DVSET_LAYER, 10, 34, VDISP_FONT_6x8, my_invers, str);
	}
		
	memset(str, '\0', DVSET_LINE_LENGTH);
	vd_prints_xy(VDISP_DVSET_LAYER, 63, 34, VDISP_FONT_6x8, 0, "/");
		
	if (dvset_isedit() && (dvset_idx == 3))
	{
		memcpy(str, edit_str, CALLSIGN_EXT_LENGTH);
		vd_prints_xy_inverse(VDISP_DVSET_LAYER, 68, 34, VDISP_FONT_6x8, cursor_idx, str);
	}
	else
	{
		memcpy(str, settings.s.my_ext, CALLSIGN_EXT_LENGTH);	
		vd_prints_xy(VDISP_DVSET_LAYER, 68, 34, VDISP_FONT_6x8, my_ext_invers, str);
	}
		
	memset(str, '\0', DVSET_LINE_LENGTH);
	
	if (dvset_isedit() && (dvset_idx == 4))
	{
		memcpy(str, edit_str, TXMSG_LENGTH);
		vd_prints_xy_inverse(VDISP_DVSET_LAYER, 0, 45, VDISP_FONT_6x8, cursor_idx, str);
	}
	else
	{
		memcpy(str, settings.s.txmsg, TXMSG_LENGTH);
		vd_prints_xy(VDISP_DVSET_LAYER, 0, 45, VDISP_FONT_6x8, txmsg_invers, str);
	}
	
	if (dvset_isselected() && (dvset_idx == 0 || (dvset_idx == 1)))
	{
		int count = 0;
		int y_pos = 12;
		int x_offset = 5;
		char tmp_buf[7];
		
		if (dvset_idx == 1)
			x_offset += 30;

		vd_clear_rect(VDISP_DVSET_LAYER, 20, 11, 52 + x_offset, 50);
			
		for (int i = 20; i < (73 + x_offset); i++)
		{
			vd_set_pixel(VDISP_DVSET_LAYER, i, y_pos, 0, 1, 1);
			vd_set_pixel(VDISP_DVSET_LAYER, i, (y_pos + 10), 0, 1, 1);
			vd_set_pixel(VDISP_DVSET_LAYER, i, ((y_pos + 8) + (10 * 3)), 0, 1, 1);
			vd_set_pixel(VDISP_DVSET_LAYER, i + 1, (((y_pos + 8) + (10 * 3)) + 1), 0, 1, 1);
		}
	
		for (int i = y_pos; i < ((y_pos + 8) + (10 * 3)); i++)
		{
			vd_set_pixel(VDISP_DVSET_LAYER, 20, i, 0, 1, 1);
			vd_set_pixel(VDISP_DVSET_LAYER, 72 + x_offset, i, 0, 1, 1);
			vd_set_pixel(VDISP_DVSET_LAYER, 73 + x_offset, i + 1, 0, 1, 1);
		}
	
		y_pos += 4;
		
		if (dvset_idx == 0)
		{
			vd_prints_xy(VDISP_DVSET_LAYER, 22, y_pos, VDISP_FONT_4x6, 0, "UR table");
			count = NUM_URCALL_SETTINGS;
		}
		else if (dvset_idx == 1)
		{
			vd_prints_xy(VDISP_DVSET_LAYER, 22, y_pos, VDISP_FONT_4x6, 0, "RPT table");
			count = NUM_RPT_SETTINGS;
		}
	
		for (int i = feld_idx; (i < count && i < (feld_idx + 3)); i++)
		{
			y_pos += 8;

			vdisp_i2s(tmp_buf, 2, 10, 1, (i + 1) );

			vd_prints_xy(VDISP_DVSET_LAYER, 22, y_pos + 2, VDISP_FONT_4x6, 0, tmp_buf);
			vd_printc_xy(VDISP_DVSET_LAYER, 30, y_pos + 2, VDISP_FONT_4x6, 0, ':');
			
			if (dvset_idx == 0)
			{
				int disp_inverse = 0;
				char urcall[CALLSIGN_LENGTH + 1];
				
				if ((i == feld_idx) && (!dvset_isedit()))
					disp_inverse = -2;
				else if ((i == feld_idx) && (dvset_isedit()))
					disp_inverse = cursor_idx;
				else
					disp_inverse = -1;					
			
				memset(urcall, '\0', (CALLSIGN_LENGTH + 1));
				
				if ((i == feld_idx) && (dvset_isedit()))
				{
					vd_clear_rect(VDISP_DVSET_LAYER, 18, 19, 56, 11);
						
					for (int i = 18; i < 76; i++)
					{
						vd_set_pixel(VDISP_DVSET_LAYER, i, 19, 0, 1, 1);
						vd_set_pixel(VDISP_DVSET_LAYER, i, 30, 0, 1, 1);
						vd_set_pixel(VDISP_DVSET_LAYER, i + 1, 31, 0, 1, 1);
					}
					for (int i = 19; i < 30; i++)
					{
						vd_set_pixel(VDISP_DVSET_LAYER, 18, i, 0, 1, 1);
						vd_set_pixel(VDISP_DVSET_LAYER, 76, i, 0, 1, 1);
						vd_set_pixel(VDISP_DVSET_LAYER, 76, i + 1, 0, 1, 1);
					}
					
					memcpy(urcall, edit_str, CALLSIGN_LENGTH);
					vd_prints_xy_inverse(VDISP_DVSET_LAYER, 21, 22, VDISP_FONT_6x8, disp_inverse, urcall);
				}
				else
				{
					memcpy(urcall, settings.s.urcall + (i*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
					vd_prints_xy_inverse(VDISP_DVSET_LAYER, 34, y_pos + 1, VDISP_FONT_5x8, disp_inverse, urcall);
				}
			}
			else if (dvset_idx == 1)
			{
				int disp1_inverse = 0;
				int disp2_inverse = 0;
				char r1[CALLSIGN_LENGTH + 1];
				char r2[CALLSIGN_LENGTH + 1];

				if ((i == feld_idx) && (!dvset_isedit()))
				{
					if ((sub_feld_idx == 0) || (feld_idx < 0))
					{
						disp1_inverse = -2;
						disp2_inverse = -1;
					}
					else
					{
						disp1_inverse = -1;
						disp2_inverse = -2;
					}
				}
				else if ((i == feld_idx) && (dvset_isedit()))
				{
					if (sub_feld_idx == 0)
					{
						disp1_inverse = cursor_idx;
						disp2_inverse = -1;
					}
					else
					{
						disp1_inverse = -1;
						disp2_inverse = cursor_idx;
					}
				}
				else
				{
					disp1_inverse = -1;
					disp2_inverse = -1;
				}
				
				if (i >= 0)
				{
					memset(r1, '\0', (CALLSIGN_LENGTH + 1));
					
					if ((i == feld_idx) && (sub_feld_idx == 0) && (dvset_isedit()))
					{
						vd_clear_rect(VDISP_DVSET_LAYER, 8, 19, 56, 11);
						
						for (int i = 8; i < 66; i++)
						{
							vd_set_pixel(VDISP_DVSET_LAYER, i, 19, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, i, 30, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, i + 1, 31, 0, 1, 1);
						}
						for (int i = 19; i < 30; i++)
						{
							vd_set_pixel(VDISP_DVSET_LAYER, 8, i, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, 65, i, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, 66, i + 1, 0, 1, 1);
						}
		
						memcpy(r1, edit_str, CALLSIGN_LENGTH);
						vd_prints_xy_inverse(VDISP_DVSET_LAYER, 11, 22, VDISP_FONT_6x8, disp1_inverse, r1);
					}
					else
					{
						memcpy(r1, settings.s.rpt1 + (i*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
						vd_prints_xy_inverse(VDISP_DVSET_LAYER, 34, y_pos + 1, VDISP_FONT_4x6, disp1_inverse, r1);
					}
											
					memset(r2, '\0', (CALLSIGN_LENGTH + 1));
					
					if ((i == feld_idx) && (sub_feld_idx == 1) && (dvset_isedit()))
					{
						vd_clear_rect(VDISP_DVSET_LAYER, 45, 19, 56, 11);
						
						for (int i = 45; i < 103; i++)
						{
							vd_set_pixel(VDISP_DVSET_LAYER, i, 19, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, i, 30, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, i + 1, 31, 0, 1, 1);
						}
						for (int i = 19; i < 30; i++)
						{
							vd_set_pixel(VDISP_DVSET_LAYER, 45, i, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, 102, i, 0, 1, 1);
							vd_set_pixel(VDISP_DVSET_LAYER, 103, i + 1, 0, 1, 1);
						}
						
						memcpy(r2, edit_str, CALLSIGN_LENGTH);
						vd_prints_xy_inverse(VDISP_DVSET_LAYER, 48, 22, VDISP_FONT_6x8, disp2_inverse, r2);
					}
					else
					{
						memcpy(r2, settings.s.rpt2 + (i*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
						vd_prints_xy_inverse(VDISP_DVSET_LAYER, 70, y_pos + 1, VDISP_FONT_4x6, disp2_inverse, r2);
					}
				}
				else
				{
					vd_prints_xy_inverse(VDISP_DVSET_LAYER, 34, y_pos + 1, VDISP_FONT_4x6, disp1_inverse, "DIRECT  ");
				}
			}
		}
	}
}
