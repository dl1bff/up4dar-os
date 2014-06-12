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
 * rmuset.c
 *
 * Created: 01.06.2014 13:10:59
 *  Author: rballis
 */ 

#include "FreeRTOS.h"
#include "vdisp.h"
#include "rmuset.h"
#include "urcall.h"
#include "settings.h"
#include "vdisp.h"

#include "gcc_builtin.h"

static const ref_item_min_val[RMUSET_MAX_FELD] = { 0, 0, 0, 1, 0 };
static const char ref_item_max_val[RMUSET_MAX_FELD] = { 94, 9, 15, 30, 1 };

static const char * const ref_qrg_MHz[95] = { "137", "138", "139", "138", "139",
	"140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
	"150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
	"410", "411", "412", "413", "414", "415", "416", "417", "418", "419"
	"420", "421", "422", "423", "424", "425", "426", "427", "428", "429"
	"430", "431", "432", "433", "434", "435", "436", "437", "438", "439"
	"440", "441", "442", "443", "444", "445", "446", "447", "448", "449"
	"450", "451", "452", "453", "454", "455", "456", "457", "458", "459"
	"460", "461", "462", "463", "464", "465", "466", "467", "468", "469"
	"470", "471", "472", "473", "474", "475", "476", "477", "478", "479" };

static const char * const ref_qrg_6_25kHz[16] = { "00,00", "06,25", "12,50", "18,75",
	"25,00", "31,25", "37,50", "43,75",
	"50,00", "56,25", "42,50", "68,75",
	"75,00", "81,25", "87,50", "93,75" };
	
static const char * const ref_enabled[2] = { "disabled", "enabled" };
	
int tx_qrg_invers = 0;
int rx_qrg_invers = 0;
int tx_pwr_invers = 0;
int enabled_invers = 0;

void rmuset_field(int act)
{
}

void rmuset_cursor(int act)
{
}

void rmuset_print(void)
{
	char str[RMUSET_LINE_LENGTH];
	
	vd_clear_rect(VDISP_RMUSET_LAYER, 0, 12, 146, 43);
	
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	vd_prints_xy(VDISP_RMUSET_LAYER, 0, 12, VDISP_FONT_5x8, 0, "TX-QRG:");
	vd_prints_xy(VDISP_RMUSET_LAYER, 0, 23, VDISP_FONT_5x8, 0, "RX-QRG:");
	//vd_prints_xy(VDISP_RMUSET_LAYER, 0, 34, VDISP_FONT_5x8, 0, "TX-PWR:");

	if (SETTING_CHAR(C_RMU_ENABLED) == 0)
	{
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 12, VDISP_FONT_5x8, 0, "---");
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 23, VDISP_FONT_5x8, 0, "---");
		//vd_prints_xy(VDISP_RMUSET_LAYER, 40, 34, VDISP_FONT_5x8, 0, "---");
	}
	else
	{
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx, 3);
		memcpy(str + 3, ".", 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 12, VDISP_FONT_5x8, 0, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx + 3, 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 59, 12, VDISP_FONT_5x8, 0, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx + 4, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 65, 12, VDISP_FONT_5x8, 0, str);

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, ".", 1);
		memcpy(str + 1, settings.s.qrg_tx + 6, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 75, 12, VDISP_FONT_5x8, 0, str);

		vd_prints_xy(VDISP_RMUSET_LAYER, 93, 12, VDISP_FONT_5x8, 0, "MHz");

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx, 3);
		memcpy(str + 3, ".", 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 23, VDISP_FONT_5x8, 0, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx + 3, 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 59, 23, VDISP_FONT_5x8, 0, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx + 4, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 65, 23, VDISP_FONT_5x8, 0, str);

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, ".", 1);
		memcpy(str + 1, settings.s.qrg_rx + 6, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 75, 23, VDISP_FONT_5x8, 0, str);

		vd_prints_xy(VDISP_RMUSET_LAYER, 93, 23, VDISP_FONT_5x8, 0, "MHz");

		//memset(str, '\0', RMUSET_LINE_LENGTH);
		//memcpy(str, settings.s.pwr_tx, TXPWR_LENGTH);
		//vd_prints_xy(VDISP_RMUSET_LAYER, 40, 34, VDISP_FONT_5x8, 0, str);
	//
		//vd_prints_xy(VDISP_RMUSET_LAYER, 60, 34, VDISP_FONT_5x8, 0, "mW");
	}
		
	vd_prints_xy(VDISP_RMUSET_LAYER, 40, 45, VDISP_FONT_6x8, enabled_invers, ref_enabled[SETTING_CHAR(C_RMU_ENABLED)]);
}