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
 * nodeinfo.c
 *
 * Created: 11.07.2015 13:42:54
 *  Author: rballis
 */ 

#include "FreeRTOS.h"
#include "nodeinfo.h"
#include "urcall.h"
#include "settings.h"
#include "vdisp.h"

#include "gcc_builtin.h"

#define XPOS_NODEINFO_LAYER 30

static const char ref_item_min_val[NODEINFO_MAX_REF] = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 2, 2 };
static const char ref_item_max_val[NODEINFO_MAX_REF] = { 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 1, 11, 11, 11, 11 };
	
static const char * const ref_val[12] = { " ", "-", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

int nodeinfo_pos = -1;

void nodeinfo_ref(int act)
{
}

void nodeinfo_feld(int act)
{
	if ((act == 0) && (nodeinfo_pos > -1))
	{
		--nodeinfo_pos;
	}
	else if ((act == 1) && (nodeinfo_pos < (NODEINFO_MAX_REF - 1)))
	{
		nodeinfo_pos++;
	}
}

void nodeinfo_print(void)
{
	char str[NODEINFO_LINE_LENGTH];
	
	vd_clear_rect(VDISP_NODEINFO_LAYER, 0, 12, 146, 43);
	
	vd_printc_xy(VDISP_NODEINFO_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_NODEINFO_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 12, VDISP_FONT_5x8, 0, "LAT:");
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 22, VDISP_FONT_5x8, 0, "LON:");
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 32, VDISP_FONT_5x8, 0, "QRG:");
	vd_prints_xy(VDISP_NODEINFO_LAYER, 0, 42, VDISP_FONT_5x8, 0, "DUP:");
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER, 12, VDISP_FONT_5x8, (nodeinfo_pos == 0), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat + 1, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 1 * 5, 12, VDISP_FONT_5x8, (nodeinfo_pos == 1), str);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 2 * 5, 12, VDISP_FONT_5x8, 0, ".");
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat + 2, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 3 * 5, 12, VDISP_FONT_5x8, (nodeinfo_pos == 2), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat + 3, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 4 * 5, 12, VDISP_FONT_5x8, (nodeinfo_pos == 3), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat + 4, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 5 * 5, 12, VDISP_FONT_5x8, (nodeinfo_pos == 4), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lat + 5, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 6 * 5, 12, VDISP_FONT_5x8, (nodeinfo_pos == 5), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER, 22, VDISP_FONT_5x8, (nodeinfo_pos == 6), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon + 1, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 1 * 5, 22, VDISP_FONT_5x8, (nodeinfo_pos == 7), str);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 2 * 5, 22, VDISP_FONT_5x8, 0, ".");
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon + 2, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 3 * 5, 22, VDISP_FONT_5x8, (nodeinfo_pos == 8), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon + 3, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 4 * 5, 22, VDISP_FONT_5x8, (nodeinfo_pos == 9), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon + 4, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 5 * 5, 22, VDISP_FONT_5x8, (nodeinfo_pos == 10), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_pos_lon + 5, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 6 * 5, 22, VDISP_FONT_5x8, (nodeinfo_pos == 11), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER, 32, VDISP_FONT_5x8, (nodeinfo_pos == 12), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg + 1, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 1 * 5, 32, VDISP_FONT_5x8, (nodeinfo_pos == 13), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg + 2, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 2 * 5, 32, VDISP_FONT_5x8, (nodeinfo_pos == 14), str);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 3 * 5, 32, VDISP_FONT_5x8, 0, ".");

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg + 3, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 4 * 5, 32, VDISP_FONT_5x8, (nodeinfo_pos == 15), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg + 4, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 5 * 5, 32, VDISP_FONT_5x8, (nodeinfo_pos == 16), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_qrg + 5, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 6 * 5, 32, VDISP_FONT_5x8, (nodeinfo_pos == 17), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER, 42, VDISP_FONT_5x8, (nodeinfo_pos == 18), str);
	
	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup + 1, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 1 * 5, 42, VDISP_FONT_5x8, (nodeinfo_pos == 19), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup + 2, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 2 * 5, 42, VDISP_FONT_5x8, (nodeinfo_pos == 20), str);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 3 * 5, 42, VDISP_FONT_5x8, 0, ".");

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup + 3, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 4 * 5, 42, VDISP_FONT_5x8, (nodeinfo_pos == 21), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup + 4, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 5 * 5, 42, VDISP_FONT_5x8, (nodeinfo_pos == 22), str);

	memset(str, '\0', NODEINFO_LINE_LENGTH);
	memcpy(str, settings.s.node_dup + 5, 1);
	vd_prints_xy(VDISP_NODEINFO_LAYER, XPOS_NODEINFO_LAYER + 6 * 5, 42, VDISP_FONT_5x8, (nodeinfo_pos == 23), str);
}