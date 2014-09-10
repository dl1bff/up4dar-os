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
#include "rmuset.h"
#include "urcall.h"
#include "settings.h"
#include "vdisp.h"

#include "gcc_builtin.h"

#include <pm.h>

int ref_selected_item = 0;
static const char ref_item_min_val[RMUSET_MAX_REF] = { 0, 0, 0, false };
static const char ref_item_max_val[RMUSET_MAX_REF] = { 92, 9, 15, true };

int feld_selected_item = 6;

static const char * const ref_qrg_MHz[93] = { "137", "138", "139",
	"140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
	"150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
	"410", "411", "412", "413", "414", "415", "416", "417", "418", "419",
	"420", "421", "422", "423", "424", "425", "426", "427", "428", "429",
	"430", "431", "432", "433", "434", "435", "436", "437", "438", "439",
	"440", "441", "442", "443", "444", "445", "446", "447", "448", "449",
	"450", "451", "452", "453", "454", "455", "456", "457", "458", "459",
	"460", "461", "462", "463", "464", "465", "466", "467", "468", "469",
	"470", "471", "472", "473", "474", "475", "476", "477", "478", "479" };
	
static const char * const ref_qrg_100kHz[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
	
//static const char * const ref_qrg_ kHz_step[4] = { "25", "12,5", "6,25", "5" }
//
//static const char * const ref_qrg_5kHz[20] = { "0000", "0500",
	//"1000", "1500",
	//"2000", "2500",
	//"3000", "3500",
	//"4000", "4500",
	//"5000", "5500",
	//"6000", "6500",
	//"7000", "7500",
	//"8000", "8500",
	//"9000", "9500" };
//
static const char * const ref_qrg_6_25kHz[16] = { "0000", "0625",
	"1250", "1875",
	"2500", "3125",
	"3750", "4375",
	"5000", "5625",
	"6250", "6875",
	"7500", "8125",
	"8750", "9375" };
	//
//static const char * const ref_qrg_12_5kHz[8] = { "0000", "1250",
	//"2500", "3750",
	//"5000", "6250",
	//"7500", "8750"};
//
//static const char * const ref_qrg_25kHz[4] = { "0000", "2500",
	//"5000", "7500" };
//
static const char * const ref_enabled[2] = { "disabled", "enabled" };
	
int tx_qrg_MHz_invers = 0;
int tx_qrg_100kHz_invers = 0;
int tx_qrg_6_25kHz_invers = 0;
int rx_qrg_MHz_invers = 0;
int rx_qrg_100kHz_invers = 0;
int rx_qrg_6_25kHz_invers = 0;
int enabled_invers = 1;

void rmuset_ref(int act)
{
	if (feld_selected_item == 0)
	{
		for (int i = 0; i < ref_item_max_val[0]; i++)
		{
			if (memcmp(settings.s.qrg_tx, ref_qrg_MHz[i], 3) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[0])
			ref_selected_item = ref_item_max_val[0];
		else if (ref_selected_item > ref_item_max_val[0])
			ref_selected_item = ref_item_min_val[0];
		
		memcpy(settings.s.qrg_tx, ref_qrg_MHz[ref_selected_item], 3);
		dstarRMUSetQRG();
	}
	else if (feld_selected_item == 1)
	{
		for (int i = 0; i < ref_item_max_val[1]; i++)
		{
			if (memcmp(settings.s.qrg_tx + 3, ref_qrg_100kHz[i], 1) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[1])
			ref_selected_item = ref_item_max_val[1];
		else if (ref_selected_item > ref_item_max_val[1])
			ref_selected_item = ref_item_min_val[1];
		
		memcpy(settings.s.qrg_tx + 3, ref_qrg_100kHz[ref_selected_item], 1);
		dstarRMUSetQRG();
	}
	else if (feld_selected_item == 2)
	{
		for (int i = 0; i < ref_item_max_val[2]; i++)
		{
			if (memcmp(settings.s.qrg_tx + 4, ref_qrg_6_25kHz[i], 2) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[2])
			ref_selected_item = ref_item_max_val[2];
		else if (ref_selected_item > ref_item_max_val[2])
			ref_selected_item = ref_item_min_val[2];
		
		memcpy(settings.s.qrg_tx + 4, ref_qrg_6_25kHz[ref_selected_item], 2);
		memcpy(settings.s.qrg_tx + 6, ref_qrg_6_25kHz[ref_selected_item] + 2, 2);
		memcpy(settings.s.qrg_tx + 8, "0", 1);
		dstarRMUSetQRG();
	}
	if (feld_selected_item == 3)
	{
		for (int i = 0; i < ref_item_max_val[0]; i++)
		{
			if (memcmp(settings.s.qrg_rx, ref_qrg_MHz[i], 3) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[0])
			ref_selected_item = ref_item_max_val[0];
		else if (ref_selected_item > ref_item_max_val[0])
			ref_selected_item = ref_item_min_val[0];
		
		memcpy(settings.s.qrg_rx, ref_qrg_MHz[ref_selected_item], 3);
		dstarRMUSetQRG();
	}
	else if (feld_selected_item == 4)
	{
		for (int i = 0; i < ref_item_max_val[1]; i++)
		{
			if (memcmp(settings.s.qrg_rx + 3, ref_qrg_100kHz[i], 1) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[1])
			ref_selected_item = ref_item_max_val[1];
		else if (ref_selected_item > ref_item_max_val[1])
			ref_selected_item = ref_item_min_val[1];
		
		memcpy(settings.s.qrg_rx + 3, ref_qrg_100kHz[ref_selected_item], 1);
		dstarRMUSetQRG();
	}
	else if (feld_selected_item == 5)
	{
		for (int i = 0; i < ref_item_max_val[2]; i++)
		{
			if (memcmp(settings.s.qrg_rx + 4, ref_qrg_6_25kHz[i], 2) == 0)
			{
				ref_selected_item = i;
				break;
			}
		}
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
		
		if (ref_selected_item < ref_item_min_val[2])
			ref_selected_item = ref_item_max_val[2];
		else if (ref_selected_item > ref_item_max_val[2])
			ref_selected_item = ref_item_min_val[2];
		
		memcpy(settings.s.qrg_rx + 4, ref_qrg_6_25kHz[ref_selected_item], 2);
		memcpy(settings.s.qrg_rx + 6, ref_qrg_6_25kHz[ref_selected_item] + 2, 2);
		memcpy(settings.s.qrg_rx + 8, "0", 1);
		dstarRMUSetQRG();
	}
	else if (feld_selected_item == 6)
	{
		ref_selected_item = SETTING_CHAR(C_RMU_ENABLED);
		
		if (act == 0)
			ref_selected_item--;
		else if (act == 1)
			ref_selected_item++;
			
		if (ref_selected_item < ref_item_min_val[3])
			ref_selected_item = ref_item_max_val[3];
		else if (ref_selected_item > ref_item_max_val[3])
			ref_selected_item = ref_item_min_val[3];
			
		SETTING_CHAR(C_RMU_ENABLED) = ref_selected_item;
		dstarRMUEnable();
	}
}

void rmuset_feld(void)
{
	if (!rmuset_enabled())
	{
		feld_selected_item = 6;
		return;
	}
	
	feld_selected_item++;
	
	if (feld_selected_item > RMUSET_MAX_FELD)
		feld_selected_item = 0;
		
	tx_qrg_MHz_invers = 0;
	tx_qrg_100kHz_invers = 0;
	tx_qrg_6_25kHz_invers = 0;
	rx_qrg_MHz_invers = 0;
	rx_qrg_100kHz_invers = 0;
	rx_qrg_6_25kHz_invers = 0;
	enabled_invers = 0;
	
	if (feld_selected_item == 0)
		tx_qrg_MHz_invers = 1;
	else if (feld_selected_item == 1)
		tx_qrg_100kHz_invers = 1;
	else if (feld_selected_item == 2)
		tx_qrg_6_25kHz_invers = 1;
	else if (feld_selected_item == 3)
		rx_qrg_MHz_invers = 1;
	else if (feld_selected_item == 4)
		rx_qrg_100kHz_invers = 1;
	else if (feld_selected_item == 5)
		rx_qrg_6_25kHz_invers = 1;
	else if (feld_selected_item == 6)
		enabled_invers = 1;
}

bool rmuset_enabled(void)
{
	return (SETTING_CHAR(C_RMU_ENABLED) == 1 && (rmu_enabled)) ? true : false;
}

void rmuset_print(void)
{
	char str[RMUSET_LINE_LENGTH];
	
	vd_clear_rect(VDISP_RMUSET_LAYER, 0, 12, 146, 43);
	
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	vd_prints_xy(VDISP_RMUSET_LAYER, 0, 12, VDISP_FONT_5x8, 0, "TX-QRG:");
	vd_prints_xy(VDISP_RMUSET_LAYER, 0, 23, VDISP_FONT_5x8, 0, "RX-QRG:");


	vd_prints_xy(VDISP_RMUSET_LAYER, 93, 12, VDISP_FONT_5x8, 0, "MHz");
	vd_prints_xy(VDISP_RMUSET_LAYER, 93, 23, VDISP_FONT_5x8, 0, "MHz");
	
	if (!rmuset_enabled())
	{
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 12, VDISP_FONT_5x8, 0, "---.---.--");
		vd_prints_xy(VDISP_RMUSET_LAYER, 40, 23, VDISP_FONT_5x8, 0, "---.---.--");
		
		feld_selected_item = 6;
		enabled_invers = 1;
	}
	else
	{
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx, 3);
		vd_prints_xy(VDISP_RMUSET_LAYER, 39, 12, VDISP_FONT_5x8, tx_qrg_MHz_invers, str);
		vd_prints_xy(VDISP_RMUSET_LAYER, 54, 12, VDISP_FONT_5x8, 0, ".");
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx + 3, 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 59, 12, VDISP_FONT_5x8, tx_qrg_100kHz_invers, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_tx + 4, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 65, 12, VDISP_FONT_5x8, tx_qrg_6_25kHz_invers, str);

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, ".", 1);
		memcpy(str + 1, settings.s.qrg_tx + 6, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 75, 12, VDISP_FONT_5x8, tx_qrg_6_25kHz_invers, str);

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx, 3);
		vd_prints_xy(VDISP_RMUSET_LAYER, 39, 23, VDISP_FONT_5x8, rx_qrg_MHz_invers, str);
		vd_prints_xy(VDISP_RMUSET_LAYER, 54, 23, VDISP_FONT_5x8, 0, ".");
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx + 3, 1);
		vd_prints_xy(VDISP_RMUSET_LAYER, 59, 23, VDISP_FONT_5x8, rx_qrg_100kHz_invers, str);
	
		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, settings.s.qrg_rx + 4, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 65, 23, VDISP_FONT_5x8, rx_qrg_6_25kHz_invers, str);

		memset(str, '\0', RMUSET_LINE_LENGTH);
		memcpy(str, ".", 1);
		memcpy(str + 1, settings.s.qrg_rx + 6, 2);
		vd_prints_xy(VDISP_RMUSET_LAYER, 75, 23, VDISP_FONT_5x8, rx_qrg_6_25kHz_invers, str);
	}
		
	vd_prints_xy(VDISP_RMUSET_LAYER, 40, 35, VDISP_FONT_5x8, enabled_invers, ref_enabled[rmuset_enabled()]);
}