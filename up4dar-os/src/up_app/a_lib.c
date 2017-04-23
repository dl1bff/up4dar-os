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
 * a_lib.c
 *
 * Created: 08.07.2012 12:32:34
 *  Author: mdirska
 */ 

#include "a_lib.h"
#include "a_lib_internal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "gcc_builtin.h"
#include "up_dstar\dstar.h"
#include "up_net\snmp_data.h"
#include "up_dstar\settings.h"
#include "up_dstar\vdisp.h"
#include "up_io\lcd.h"
#include "up_dstar\dcs.h"
#include "up_io\wm8510.h"
#include "up_dstar\ambe.h"
#include "up_net\dhcp.h"
#include "up_dstar\dvset.h"
#include "up_dstar\r2cs.h"
#include "up_dstar\rmuset.h"
#include "up_dstar\urcall.h"


char software_ptt = 0;

#define MAX_APP_NAME_LENGTH  9
#define MAX_BUTTON_TEXT_LENGTH 8

typedef struct a_app_context {
	
	struct a_app_context * next;
	
	char screen_num;
	
	char name[MAX_APP_NAME_LENGTH + 1]; // app name
	
	char button_text[4][MAX_BUTTON_TEXT_LENGTH + 1];
	
	int (*key_event_handler) (void * a, int key_num, int key_event);  // function to be called on key events
	
	void * private_data; // the app can use this to store internal private data
	
	
	
	} app_context_t;


static app_context_t * current_app = NULL;
static app_context_t * app_list_head = NULL;
static app_context_t * main_screen = NULL;
static bool r2cs_flag = false;
static int r2cs_idx = 0;

void * a_new_app ( const char * app_name, char screen_num)
{
	app_context_t * a = (app_context_t *) pvPortMalloc(sizeof (app_context_t));
	
	if (a == NULL)
		return NULL;
		
	memset(a, 0, sizeof (app_context_t));
	
	a->screen_num = screen_num;
	
	strncpy(a->name, app_name, MAX_APP_NAME_LENGTH);
	
	a->name[MAX_APP_NAME_LENGTH] = 0;
	
	if (app_list_head == NULL)
	{
		app_list_head = a;
		current_app = a;
	}		
	else
	{
		app_context_t * tmp_a = app_list_head;
		
		while (tmp_a->next != NULL ) // go to end of the list
		{
			tmp_a = tmp_a->next;
		}
		
		tmp_a->next = a; // set new app at end of the list
	}		
	
	return a;
}

void a_set_button_text ( void * app_context, const char * button1,
    const char * button2, const char * button3, const char * button4)
{
	app_context_t * a = (app_context_t *) app_context;
	
	strncpy(a->button_text[0], button1, MAX_BUTTON_TEXT_LENGTH);
	strncpy(a->button_text[1], button2, MAX_BUTTON_TEXT_LENGTH);
	strncpy(a->button_text[2], button3, MAX_BUTTON_TEXT_LENGTH);
	strncpy(a->button_text[3], button4, MAX_BUTTON_TEXT_LENGTH);
	
	a->button_text[0][MAX_BUTTON_TEXT_LENGTH] = 0;
	a->button_text[1][MAX_BUTTON_TEXT_LENGTH] = 0;
	a->button_text[2][MAX_BUTTON_TEXT_LENGTH] = 0;
	a->button_text[3][MAX_BUTTON_TEXT_LENGTH] = 0;
}

void a_set_button_text_pos ( void * app_context, const char * button, int button_pos)
{
	app_context_t * a = (app_context_t *) app_context;
	
	strncpy(a->button_text[button_pos], button, MAX_BUTTON_TEXT_LENGTH);
	a->button_text[button_pos][MAX_BUTTON_TEXT_LENGTH] = 0;
}

void a_set_key_event_handler ( void * app_context, 
	int (*key_event_handler) (void * a, int key_num, int key_event))
{
	app_context_t * a = (app_context_t *) app_context;
	
	a->key_event_handler = key_event_handler;

}

void a_set_private_data ( void * app_context, void * priv )
{
	app_context_t * a = (app_context_t *) app_context;
	
	a->private_data = priv;
}

void * a_get_private_data ( void * app_context )
{
	app_context_t * a = (app_context_t *) app_context;
	
	return a->private_data;
}


void * a_malloc ( void * app_context, int num_bytes )
{
	return pvPortMalloc(num_bytes);
}



/*
static int active_app = 0;
static int num_apps = 4;
*/
static int help_layer_timer = 0;
static int help_layer;
bool key_douple_function = false;

// static int app_manager_key_state = 0;

/*

static const app_context_t dstar_app_context = {
	"DSTAR",
	{
		 "PTT", "", ""
	},
	NULL,
	NULL
};

static const app_context_t gps_app_context = {
	"GPS",
	{
		"", "", ""
	},
	NULL,
	NULL
};
*/
/*
static const app_context_t ref_app_context = {
	"MODE",
	{
		"CONNECT", "DISC", "SELECT"
	},
	NULL,
	NULL
};

*/
/*
static app_context_t * ref_app_context;

static const app_context_t debug_app_context = {
	"DEBUG",
	{
		"", "REBOOT", ""
	},
	NULL,
	NULL
};


*/

bool tx_info = false;

void tx_info_on(void)
{
	if ((!tx_info)  && (!r2cs_flag) && (current_app->screen_num == VDISP_MAIN_LAYER))
	{
		char str[22];

		lcd_show_menu_layer(help_layer);
		help_layer_timer = 0; // display permanent
	
		vd_clear_rect(help_layer, 0, 12, 146, 43);	

		for (int i = 8; i < 66; i++)
		{
			vd_set_pixel(help_layer, i, 19, 0, 1, 1);
			vd_set_pixel(help_layer, i, 30, 0, 1, 1);
			vd_set_pixel(help_layer, i + 1, 31, 0, 1, 1);
		}
		for (int i = 19; i < 30; i++)
		{
			vd_set_pixel(help_layer, 8, i, 0, 1, 1);
			vd_set_pixel(help_layer, 65, i, 0, 1, 1);
			vd_set_pixel(help_layer, 66, i + 1, 0, 1, 1);
		}
	
		for (int i = 8; i < 112; i++)
		{
			vd_set_pixel(help_layer, i, 37, 0, 1, 1);
			vd_set_pixel(help_layer, i, 48, 0, 1, 1);
			vd_set_pixel(help_layer, i + 1, 49, 0, 1, 1);
		}
		for (int i = 37; i < 49; i++)
		{
			vd_set_pixel(help_layer, 8, i, 0, 1, 1);
			vd_set_pixel(help_layer, 112, i, 0, 1, 1);
			vd_set_pixel(help_layer, 113, i + 1, 0, 1, 1);
		}

		memset(str, '\0', 22);
		memcpy(str, getURCALL(), CALLSIGN_LENGTH);
	
		vd_prints_xy(help_layer, 10, 21, VDISP_FONT_5x8, 0, "ur");
		vd_prints_xy(help_layer, 20, 21, VDISP_FONT_5x8, 0, str);
	
		vd_prints_xy(help_layer, 70, 18, VDISP_FONT_4x6, 0, "R1");
		vd_prints_xy(help_layer, 70, 26, VDISP_FONT_4x6, 0, "R2");
		
		if (SETTING_CHAR(C_DV_DIRECT) != 1)
		{
			memset(str, '\0', 22);
		
			memcpy(str, settings.s.rpt1 + ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		
			vd_prints_xy(help_layer, 80, 18, VDISP_FONT_4x6, 0, str);
		
			memset(str, '\0', 22);
		
			memcpy(str, settings.s.rpt2 + ((SETTING_CHAR(C_DV_USE_RPTR_SETTING) - 1)*CALLSIGN_LENGTH), CALLSIGN_LENGTH);
		
			vd_prints_xy(help_layer, 80, 26, VDISP_FONT_4x6, 0, str);
		}
		else
		{
			vd_prints_xy(help_layer, 80, 18, VDISP_FONT_4x6, 0, "DIRECT  ");
			vd_prints_xy(help_layer, 80, 26, VDISP_FONT_4x6, 0, "DIRECT  ");
		}
	
		memset(str, '\0', 22);
		memcpy(str, settings.s.txmsg, TXMSG_LENGTH);
	
		vd_prints_xy(help_layer, 10, 40, VDISP_FONT_5x8, 0, str);

		tx_info = true;
	}
}

void tx_info_off(void)
{
	if (tx_info)
	{
		tx_info = false;
		lcd_show_menu_layer(help_layer);
		help_layer_timer = 3; // approx 2 seconds
	}
}

static void set_help_text (void)
{
	const app_context_t * a = current_app;
	
	vd_clear_rect(help_layer, 0, 0, 40, 6); // name
	vd_prints_xy(help_layer, 0, 0, VDISP_FONT_4x6, 0, a->name);
	
	vd_clear_rect(help_layer, 0, 58, 25, 6); // button 1
	vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, a->button_text[0]);
	
	vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
	vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, a->button_text[1]);
	
	vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
	vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, a->button_text[2]);
	
	vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
	vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, a->button_text[3]);
	
	/*
	vd_clear_rect(help_layer, 91, 14, 32, 6); // button UP
	vd_prints_xy(help_layer, 91, 14, VDISP_FONT_4x6, 0, a->button_text[3]);
	
	vd_clear_rect(help_layer, 91, 38, 32, 6); // button DOWN
	vd_prints_xy(help_layer, 91, 38, VDISP_FONT_4x6, 0, a->button_text[4]);
	*/
}


void a_app_manager_select_first(void)
{
	current_app = app_list_head;
	
	if (current_app != NULL)
	{
		set_help_text();
		
		lcd_show_layer(current_app->screen_num);
	}
}

static void app_manager_select_next(void)
{
	app_context_t * a = current_app;
	
	if (!a)
		return;
	
	if (a->next != NULL)
	{
		current_app = a->next;
	}
	else
	{
		current_app = app_list_head;
	}
	
	set_help_text();
	
	lcd_show_layer(current_app->screen_num);
}




void a_app_manager_service(void)
{
	if (help_layer_timer > 0)
	{
		help_layer_timer --;
		
		if (help_layer_timer == 0)
		{
			lcd_show_help_layer(0); // turn off help
		}
	}
	
	
}


static void set_speaker_volume (int up)
{
	int new_volume = SETTING_CHAR(C_SPKR_VOLUME) + ( up ? 1 : -1 );
	
	if ((new_volume <= 6) && (new_volume >= -57))
	{
		SETTING_CHAR(C_SPKR_VOLUME) = new_volume;
		
		char buf[4];
		
		if (new_volume < 0)
		{
			new_volume = -new_volume;
			buf[0] = '-';
		}
		else
		{
			buf[0] = '+';
		}
		
		lcd_show_menu_layer(help_layer);
		help_layer_timer = 0; // display permanent
		
		vd_clear_rect(help_layer, 0, 12, 146, 43);

		vdisp_i2s(buf + 1, 2, 10, 1, new_volume);
		vd_prints_xy(help_layer, 30, 30, VDISP_FONT_6x8, 0, "Volume");
		vd_prints_xy(help_layer, 70, 30, VDISP_FONT_6x8, 0, buf);
		
		lcd_show_help_layer(help_layer);
		help_layer_timer = 3; // approx 2 seconds
	}
}

char dcs_mode = 0;
char hotspot_mode = 0;
char repeater_mode = 0;
char parrot_mode = 0;
int key_lock = 0;

static char snmp_reset_cmnty = 0;

bool refresh_main_menu = false;

void a_dispatch_key_event( int layer_num, int key_num, int key_event )
{
	if ((key_lock) && (key_num != A_KEY_BUTTON_2)) return;
	
	if (key_num == A_KEY_BUTTON_APP_MANAGER)
	{
		if (key_event == A_KEY_PRESSED)
		{
			if (dvset_isedit())
			{
				dvset_cursor(1);
				dvset_print();
				
				return;
			}
			else if (dvset_isselected())
			{
				a_set_button_text_pos(main_screen, "SELECT", 0);
				vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
				vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);

				a_set_button_text_pos(main_screen, "", 1);
				vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
				vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
				
				a_set_button_text_pos(main_screen, "", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
				
				a_set_button_text_pos(main_screen, "MENU", 3);
				vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
				vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);

				settings_init();
				dvset_cancel();
				dvset_print();
				
				refresh_main_menu = true;
			
				return;
			}
			else if (r2cs_flag)
			{
				r2cs_flag = false;
				lcd_show_menu_layer(help_layer);
				help_layer_timer = 3; // approx 2 seconds
			
				a_set_button_text_pos(main_screen, "R>CS", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
			
				return;
			}
			
			if (refresh_main_menu)
			{
				a_set_button_text_pos(main_screen, "PTT", 0);
				vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
				vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);

				a_set_button_text_pos(main_screen, "MUTE", 1);
				vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
				vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
				
				a_set_button_text_pos(main_screen, "R>CS", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
				
				a_set_button_text_pos(main_screen, "MENU", 3);
				vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
				vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);

				refresh_main_menu = false;
			}
		
			snmp_reset_cmnty = 0;
			
			software_ptt = 0; // prevent TXing forever...
			
			app_manager_select_next();
			lcd_show_help_layer(help_layer);
			
			if ((current_app->screen_num == VDISP_REF_LAYER) ||
				(current_app->screen_num == VDISP_DVSET_LAYER) ||
				(current_app->screen_num == VDISP_MAIN_LAYER && r2csURCALL()))
			{
				help_layer_timer = 0; // show help forever..
			}
			else if (current_app->screen_num == VDISP_RMUSET_LAYER)
			{
				help_layer_timer = 0; // show help forever..

				rmuset_print();				
			}
			else
			{
				help_layer_timer = 5; // approx 2 seconds
			}			
		}
		
		if (key_event == A_KEY_HOLD_5S)
		{
			if (SETTING_CHAR(C_DISABLE_UDP_BEACON) != 0)  // toggle UDP beacon
			{
				SETTING_CHAR(C_DISABLE_UDP_BEACON) = 0; // beacon is now on
				dhcp_init(0); // switch on DHCP
				wm8510_beep(50, 1200, 100);
				
				snmp_reset_cmnty = 1;
			}
			else
			{
				SETTING_CHAR(C_DISABLE_UDP_BEACON) = 1; // // beacon is now off
				wm8510_beep(800, 300, 100);
			}
		}
		
		if (key_event == A_KEY_HOLD_10S)
		{
			if ((snmp_reset_cmnty == 2) && (SETTING_CHAR(C_DISABLE_UDP_BEACON) == 0))
			{
				settings.s.snmp_cmnty[0] = 0; // erase first byte, new string will then
				   // be generated automatically
				wm8510_beep(500, 1200, 100);
				snmp_reset_cmnty = 0;
			}
		}			
		
	}
	else
	{
		if (snmp_reset_cmnty == 1)
		{
			if ((key_num == A_KEY_BUTTON_UP) && (key_event == A_KEY_HOLD_2S))
			{
				snmp_reset_cmnty = 2;
			}
		}
		
		int res = 0;
		
		if (layer_num == VDISP_CURRENT_LAYER)
		{
			if (current_app->key_event_handler != NULL)
			{
				res = current_app->key_event_handler(current_app, key_num, key_event);
			}
		}
		else
		{
			app_context_t * tmp_app = app_list_head;
			
			while (tmp_app != NULL)
			{
				if ((tmp_app->screen_num == layer_num) && 
					(tmp_app->key_event_handler != NULL))
				{
					res = tmp_app->key_event_handler(tmp_app, key_num, key_event);
					break;
				}
				
				tmp_app = tmp_app->next;
			}
		}
		
		if (res == 0) // handler didn't use this event
		{
			if (((key_event == A_KEY_PRESSED) || (key_event == A_KEY_REPEAT)) && (!r2cs_flag))
			{
				switch (key_num)
				{
					case A_KEY_BUTTON_UP:
						set_speaker_volume(1);
						break;
						
					case A_KEY_BUTTON_DOWN:
						set_speaker_volume(0);
						break;
				}
			}				
		}
	}			
}

#define REF_NUM_ITEMS 7
#define REF_SELECTION_SPECIAL 7
static char ref_selected_item = 0;
static char ref_items[REF_NUM_ITEMS] = { 0, 0, 0, 0, 1, 2, 0 };
static const char ref_item_max_val[REF_NUM_ITEMS] = { 4, 2, 9, 9, 9, 25, 6 };
static const char * const ref_modes[5] = { "D-STAR Modem",
										   "IP Reflector",
										   "Hotspot     ",
										   "Repeater    ",
										   "Parrot (DVR)" };
static const char * const ref_types[3] = { "DCS", "XLX", "XRF" };
static const char * const ref_timer_desc[7] = { "off", "5 min.", "10 min.", "15 min.", "20 min.", "30 min.", "40 min." };
	

static void set_mode_vars(void)
{
	dcs_mode = ((ref_items[0] != 0) && (ref_items[0] != 4)); // "IP Reflector" "Hotspot" "Repeater"
	hotspot_mode = (ref_items[0] == 2); //  "Hotspot"
	repeater_mode = (ref_items[0] == 3); //  "Repeater"
	parrot_mode = (ref_items[0] == 4); // "Parrot"
}


static void ref_print_status (void)
{
	
	vd_prints_xy(VDISP_REF_LAYER, 36, 12, VDISP_FONT_6x8, (ref_selected_item == 0),
		ref_modes[(int) ref_items[0]]);
	
	#define XPOS_NODEINFO_LAYER 10
	vd_prints_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER, 24, VDISP_FONT_6x8, (ref_selected_item == 1),
		ref_types[(int) ref_items[1]]);
	
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 3*6, 24, VDISP_FONT_6x8, (ref_selected_item == 2),
		ref_items[2] + 0x30);
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 4*6, 24, VDISP_FONT_6x8, (ref_selected_item == 3),
		ref_items[3] + 0x30);
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 5*6, 24, VDISP_FONT_6x8, (ref_selected_item == 4),
		ref_items[4] + 0x30);
	
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 6*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		0x20);
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 7*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		ref_items[5] + 0x41);
	vd_printc_xy(VDISP_REF_LAYER, XPOS_NODEINFO_LAYER + 8*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		0x20);
		
	vd_clear_rect(VDISP_REF_LAYER, 0, 36, 120, 12);

	if (repeater_mode || hotspot_mode)
	{
		vd_prints_xy(VDISP_REF_LAYER, 0, 36, VDISP_FONT_6x8, 0, "Hometimer");
	
		vd_prints_xy(VDISP_REF_LAYER, 60, 36, VDISP_FONT_6x8, (ref_selected_item == 6),
			ref_timer_desc[(int) ref_items[6]]);
	}
	
	#undef XPOS
}

void set_ref_params (int ref_num, int ref_letter, int ref_type)
{
	int n = ref_num;
	
	ref_items[4] = n % 10;
	n /= 10;
	ref_items[3] = n % 10;
	n /= 10;
	ref_items[2] = n % 10;
	
	ref_items[5] = ref_letter - 0x41;
	
	ref_items[1] = ref_type;
	
	ref_print_status();
}

static int dvset_app_key_event_handler (void * app_context, int key_num, int event_type)
{
	// app_context_t * a = (app_context_t *) app_context;
	
	if ((key_num == A_KEY_BUTTON_1) && (event_type == A_KEY_PRESSED))
	{
		refresh_main_menu = true;

		if (dvset_isedit())
		{
			a_set_button_text_pos(main_screen, "WRITE", 0);
			vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
			vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);
			
			a_set_button_text_pos(main_screen, "CLEAR", 1);
			vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
			vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
			
			a_set_button_text_pos(main_screen, "EDIT", 2);
			vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
			vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
			
			a_set_button_text_pos(main_screen, "CANCEL", 3);
			vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
			vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);
			
			dvset_store();
		}
		else
		{
			if (!dvset_isselected())
			{
				a_set_button_text_pos(main_screen, "WRITE", 0);
				vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
				vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);
			
				a_set_button_text_pos(main_screen, "CLEAR", 1);
				vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
				vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
			
				a_set_button_text_pos(main_screen, "EDIT", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
			
				a_set_button_text_pos(main_screen, "CANCEL", 3);
				vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
				vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);
			
				dvset_select(true);
			}
			else
			{
				a_set_button_text_pos(main_screen, "SELECT", 0);
				vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
				vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);
			
				a_set_button_text_pos(main_screen, "", 1);
				vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
				vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
			
				a_set_button_text_pos(main_screen, "", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
			
				a_set_button_text_pos(main_screen, "MENU", 3);
				vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
				vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);
			
				dvset_select(false);
			}
		}
			
		dvset_print();
	}
	else if ((key_num == A_KEY_BUTTON_2) && (event_type == A_KEY_PRESSED))
	{
		if (dvset_isedit())
		{
			dvset_backspace();
		}
		else
		{
			dvset_clear();
		}

		dvset_print();
	}
	else if ((key_num == A_KEY_BUTTON_3) && (event_type == A_KEY_PRESSED))
	{
		if (dvset_isedit())
		{
			dvset_cursor(0);
		}
		else
		{
			refresh_main_menu = true;

			a_set_button_text_pos(main_screen, "STORE", 0);
			vd_clear_rect(help_layer, 0, 58, 24, 6); // button 1
			vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, main_screen->button_text[0]);
			
			a_set_button_text_pos(main_screen, "BS", 1);
			vd_clear_rect(help_layer, 34, 58, 24, 6); // button 2
			vd_prints_xy(help_layer, 34, 58, VDISP_FONT_4x6, 0, main_screen->button_text[1]);
			
			a_set_button_text_pos(main_screen, "   <", 2);
			vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
			vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
			
			a_set_button_text_pos(main_screen, "   >", 3);
			vd_clear_rect(help_layer, 98, 58, 24, 6); // button 4
			vd_prints_xy(help_layer, 98, 58, VDISP_FONT_4x6, 0, main_screen->button_text[3]);
			
			dvset_goedit();
		}
		
		dvset_print();
	}
	else if ((key_num == A_KEY_BUTTON_DOWN) && (event_type == A_KEY_PRESSED || event_type == A_KEY_REPEAT))
	{
		dvset_field(1);
			
		dvset_print();
		
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_UP) && (event_type == A_KEY_PRESSED || event_type == A_KEY_REPEAT))
	{
		dvset_field(0);
			
		dvset_print();
		
		return 1;
	}
	
	return 1;
}

static int rmuset_app_key_event_handler (void * app_context, int key_num, int event_type)
{
	// app_context_t * a = (app_context_t *) app_context;

	if ((event_type == A_KEY_PRESSED) || (event_type == A_KEY_REPEAT))
	{
		if ((key_num == A_KEY_BUTTON_3) && (event_type == A_KEY_PRESSED))
		{
			rmuset_feld();
		}
		else if ((key_num == A_KEY_BUTTON_DOWN) && (event_type == A_KEY_PRESSED || event_type == A_KEY_REPEAT))
		{
			rmuset_ref(0);
		}
		else if ((key_num == A_KEY_BUTTON_UP) && (event_type == A_KEY_PRESSED || event_type == A_KEY_REPEAT))
		{
			rmuset_ref(1);
		}
		
		rmuset_print();
	}
	
	return 1;
}

static int ref_app_key_event_handler (void * app_context, int key_num, int key_event)
{
	// app_context_t * a = (app_context_t *) app_context;
	char v;
	
	if ((key_event == A_KEY_PRESSED) || (key_event == A_KEY_REPEAT))
	{
		switch (key_num)
		{
			case A_KEY_BUTTON_1:  // connect button
				
				if (dcs_mode != 0)
				{
					ref_selected_item = REF_SELECTION_SPECIAL;
					dcs_on();
					if (repeater_mode || hotspot_mode)
						ambe_set_ref_timer(1);
					SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) = 1;
				}
				break;
			
			case A_KEY_BUTTON_2:  // disconnect button
			
				ref_selected_item = 0;
			
				if (dcs_mode != 0)
				{
					dcs_off();
					if (repeater_mode || hotspot_mode)
						ambe_set_ref_timer(0);
					SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) = 0;
				}
				break;
			
			case A_KEY_BUTTON_3:  // select button
				if (!dcs_is_connected())
				{
					int num_items = REF_NUM_ITEMS;
					
					num_items -= (repeater_mode || hotspot_mode) ? 0 : 1;
					
					ref_selected_item ++;
					if (ref_selected_item >= num_items)
					{
						ref_selected_item = 0;
					}
				}
				break;
				
			case A_KEY_BUTTON_UP:
				if (ref_selected_item != REF_SELECTION_SPECIAL)
				{
					v = ref_items[(int) ref_selected_item];
					v++;
					if (v > ref_item_max_val[(int) ref_selected_item])
					{
						v = 0;
					}
					ref_items[(int) ref_selected_item] = v;
				}				
				break;
				
			case A_KEY_BUTTON_DOWN:
				if (ref_selected_item != REF_SELECTION_SPECIAL)
				{
					v = ref_items[(int) ref_selected_item];
				
					if (v == 0)
					{
						v = ref_item_max_val[(int) ref_selected_item];
					}
					else
					{
						v--;
					}
					ref_items[(int) ref_selected_item] = v;
				}					
				break;
				
		}
		
		
		set_mode_vars();
	
		
		int n = ref_items[2] * 100 +
				ref_items[3] * 10 +
				ref_items[4];
				
		dcs_select_reflector( n, ref_items[5] + 0x41, ref_items[1]);
		
		SETTING_CHAR(C_REF_TYPE) = ref_items[1];
		SETTING_SHORT(S_REF_SERVER_NUM) = n;
		SETTING_CHAR(C_REF_MODULE_CHAR) = ref_items[5] + 0x41;
		SETTING_CHAR(C_DCS_MODE) = ref_items[0];
		SETTING_CHAR(C_REF_TIMER) = ref_items[6];
		
		//settings_set_home_ref();
		
		ref_print_status();
		
		

	}
	
	
	return 1;	
}

static int debug_app_key_event_handler (void * app_context, int key_num, int event_type)
{
	// app_context_t * a = (app_context_t *) app_context;
	
	if ((key_num == A_KEY_BUTTON_2) && (event_type == A_KEY_PRESSED))
	{
		AVR32_WDT.ctrl = 0x55001001; // enable watchdog -> reboot
		AVR32_WDT.ctrl = 0xAA001001;
	}
	
	return 0;
}

static int main_app_key_event_handler (void * app_context, int key_num, int event_type)
{
	// app_context_t * a = (app_context_t *) app_context;
	
	if ((key_num == A_KEY_BUTTON_1) && (event_type == A_KEY_PRESSED))
	{
		software_ptt = 1;
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_1) && (event_type == A_KEY_RELEASED))
	{
		software_ptt = 0;
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_2) && (event_type == A_KEY_RELEASED))
	{
		if ((!key_douple_function) && (key_lock == 0))
		{
			if (ambe_get_automute() != 0) // automute is currently on
			{
				ambe_set_automute(0);
			}
			else
			{
				ambe_set_automute(1);
			}
		}
		else
		{
			key_douple_function = false;
		}
		
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_2) && (event_type == A_KEY_HOLD_2S))
	{
		key_douple_function = true;
		if (key_lock != 0)
		{
			key_lock = 0;
		}
		else
		{
			key_lock = 1;
		}
	}
	else if ((key_num == A_KEY_BUTTON_3) && (event_type == A_KEY_PRESSED))
	{
		if (r2cs_count() >= 0)
		{
			if (!r2cs_flag)
			{
				r2cs_flag = true;
				r2cs_idx = 0;

				lcd_show_menu_layer(help_layer);
				help_layer_timer = 0; // display permanent
			
				a_set_button_text_pos(main_screen, "SET", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
				
				r2cs_print(help_layer, r2cs_idx);
			}
			else
			{
				lcd_show_menu_layer(help_layer);
				help_layer_timer = 3; // approx 2 seconds

				a_set_button_text_pos(main_screen, "R>CS", 2);
				vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
				vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);

				r2cs_flag = false;
				r2cs(help_layer, r2cs_idx);
			}
		}
		
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_3) && (event_type == A_KEY_HOLD_2S))
	{
		r2cs_flag = false;
		lcd_show_menu_layer(help_layer);
		help_layer_timer = 3; // approx 2 seconds
		r2cs(help_layer, -1);
		
		a_set_button_text_pos(main_screen, "R>CS", 2);
		vd_clear_rect(help_layer, 66, 58, 24, 6); // button 3
		vd_prints_xy(help_layer, 66, 58, VDISP_FONT_4x6, 0, main_screen->button_text[2]);
		
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_DOWN) && (event_type == A_KEY_PRESSED) && (r2cs_flag) && (r2cs_idx < r2cs_count()))
	{
		r2cs_idx++;
		r2cs_print(help_layer, r2cs_idx);
		
		return 1;
	}
	else if ((key_num == A_KEY_BUTTON_UP) && (event_type == A_KEY_PRESSED) && (r2cs_flag) && (r2cs_idx > 0))
	{
		r2cs_idx--;
		r2cs_print(help_layer, r2cs_idx);
		
		return 1;
	}
	
	return 0;
}


void a_app_manager_init(void)
{
	
	app_context_t * a;
	
	a = a_new_app( "DSTAR", VDISP_MAIN_LAYER);
	a_set_button_text(a, "PTT", "MUTE", "R>CS", "MENU");
	a_set_key_event_handler(a, main_app_key_event_handler);
	main_screen = a;
	
	a = a_new_app( "RMU SET", VDISP_RMUSET_LAYER);
	a_set_button_text(a, "", "", "SELECT", "MENU");
	a_set_key_event_handler(a, rmuset_app_key_event_handler);
	
	a = a_new_app( "DV SET", VDISP_DVSET_LAYER);
	a_set_button_text(a, "SELECT", "", "", "MENU");
	a_set_key_event_handler(a, dvset_app_key_event_handler);
	
	a = a_new_app( "GPS", VDISP_GPS_LAYER);
	a_set_button_text(a, "", "", "", "MENU");
	
	a = a_new_app( "MODE SET", VDISP_REF_LAYER);
	a_set_button_text(a, "CONNECT", "DISC", "SELECT", "MENU");
	a_set_key_event_handler(a, ref_app_key_event_handler);
	
	a = a_new_app( "AUDIO", VDISP_AUDIO_LAYER);
	a_set_button_text(a, "", "", "", "MENU");
	// a_set_key_event_handler(a, debug_app_key_event_handler);
	
	a = a_new_app( "DEBUG", VDISP_DEBUG_LAYER);
	a_set_button_text(a, "", "REBOOT", "", "MENU");
	a_set_key_event_handler(a, debug_app_key_event_handler);
	
	a = a_new_app( "NODE INFO", VDISP_NODEINFO_LAYER);
	a_set_button_text(a, "GPS", "", "", "MENU");
	// a_set_key_event_handler(a, debug_app_key_event_handler);
	
	help_layer = vd_new_screen();
	
	if ((SETTING_SHORT(S_REF_SERVER_NUM) > 0) &&
	     (SETTING_SHORT(S_REF_SERVER_NUM) < 1000))
	{
		int n = SETTING_SHORT(S_REF_SERVER_NUM);
		
		ref_items[4] = n % 10;
		n /= 10;
		ref_items[3] = n % 10;
		n /= 10;
		ref_items[2] = n % 10;
	}
	
	if ((SETTING_CHAR(C_REF_MODULE_CHAR) >= 'A') &&
		(SETTING_CHAR(C_REF_MODULE_CHAR) <= 'Z'))
	{
		ref_items[5] = SETTING_CHAR(C_REF_MODULE_CHAR) - 0x41;
	}
	
	if ((SETTING_CHAR(C_REF_TIMER) >= 0) &&
		(SETTING_CHAR(C_REF_TIMER) <= 7))
	{
		ref_items[6] = SETTING_CHAR(C_REF_TIMER);
	}
	
	if ((SETTING_CHAR(C_DCS_MODE) >= 0) &&
		(SETTING_CHAR(C_DCS_MODE) <= 5))
	{
		ref_items[0] = SETTING_CHAR(C_DCS_MODE);
	}		
	
	if ((SETTING_CHAR(C_REF_TYPE) >= 0) &&
		(SETTING_CHAR(C_REF_TYPE) <= 2))
	{
		ref_items[1] = SETTING_CHAR(C_REF_TYPE);
	}
	
	set_mode_vars();
	
	int n = ref_items[2] * 100 +
	ref_items[3] * 10 +
	ref_items[4];
	
	dcs_select_reflector( n, ref_items[5] + 0x41,  ref_items[1]);
	
	if ((dcs_mode != 0)  && 
		(SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) == 1)) 
	{
		ref_selected_item = REF_SELECTION_SPECIAL;
		dcs_on();
	}
	
	settings_set_home_ref();
	
	// TODO error handling
	
	// vd_clear_rect(help_layer, 0, 0, 128, 64);
	
	int i;
	
	for (i=0; i < 128; i++)
	{
		vd_set_pixel(help_layer, i, 56, 0, 1, 1);
	}
	
	
	
#define SIDEBOX_WIDTH 41
// #define SIDEBOX_HEIGHT 12
#define BOX1_YPOS 10
#define BOX2_YPOS 34
	
	for (i=0; i < 7; i++)
	{
		vd_set_pixel(help_layer, 32, 57+i, 0, 1, 1);
		vd_set_pixel(help_layer, 64, 57+i, 0, 1, 1);
		vd_set_pixel(help_layer, 96, 57+i, 0, 1, 1);
		vd_set_pixel(help_layer, SIDEBOX_WIDTH - 1, i, 0, 1, 1);
	}


	for (i=0; i < SIDEBOX_WIDTH; i++)
	{
		vd_set_pixel(help_layer, i, 7, 0, 1, 1);
	}
	
	/*
	for (i=0; i <= SIDEBOX_HEIGHT; i++)
	{
		vd_set_pixel(help_layer, 127-SIDEBOX_WIDTH, BOX1_YPOS+i, 0, 1, 1);
		vd_set_pixel(help_layer, 127-SIDEBOX_WIDTH, BOX2_YPOS+i, 0, 1, 1);
	}
	*/

	// vd_prints_xy(help_layer, 4, 58, VDISP_FONT_4x6, 0,"TEST");
	
	vd_prints_xy(VDISP_REF_LAYER, 0, 12, VDISP_FONT_6x8, 0, "Mode:");
	
	vd_printc_xy(VDISP_REF_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_REF_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	vd_printc_xy(VDISP_DVSET_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_DVSET_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 13, VDISP_FONT_8x12, 0, 0x1e); // arrow up
	vd_printc_xy(VDISP_RMUSET_LAYER, 120, 39, VDISP_FONT_8x12, 0, 0x1f); // arrow up
	
	set_help_text();
}

