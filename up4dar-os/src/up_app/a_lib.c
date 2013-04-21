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


char software_ptt = 0;

#define MAX_APP_NAME_LENGTH  9
#define MAX_BUTTON_TEXT_LENGTH 8

typedef struct a_app_context {
	
	struct a_app_context * next;
	
	char screen_num;
	
	char name[MAX_APP_NAME_LENGTH + 1]; // app name
	
	char button_text[3][MAX_BUTTON_TEXT_LENGTH + 1];
	
	int (*key_event_handler) (void * a, int key_num, int key_event);  // function to be called on key events
	
	void * private_data; // the app can use this to store internal private data
	
	
	
	} app_context_t;


static app_context_t * current_app = NULL;
static app_context_t * app_list_head = NULL;

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
    const char * button2, const char * button3)
{
	app_context_t * a = (app_context_t *) app_context;
	
	strncpy(a->button_text[0], button1, MAX_BUTTON_TEXT_LENGTH);
	strncpy(a->button_text[1], button2, MAX_BUTTON_TEXT_LENGTH);
	strncpy(a->button_text[2], button3, MAX_BUTTON_TEXT_LENGTH);
	
	a->button_text[0][MAX_BUTTON_TEXT_LENGTH] = 0;
	a->button_text[1][MAX_BUTTON_TEXT_LENGTH] = 0;
	a->button_text[2][MAX_BUTTON_TEXT_LENGTH] = 0;
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

static void set_help_text (void)
{
	const app_context_t * a = current_app;
	
	vd_clear_rect(help_layer, 0, 0, 40, 6); // name
	vd_prints_xy(help_layer, 0, 0, VDISP_FONT_4x6, 0, a->name);
	
	vd_clear_rect(help_layer, 0, 58, 32, 6); // button 1
	vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, a->button_text[0]);
	
	vd_clear_rect(help_layer, 39, 58, 32, 6); // button 2
	vd_prints_xy(help_layer, 39, 58, VDISP_FONT_4x6, 0, a->button_text[1]);
	
	vd_clear_rect(help_layer, 76, 58, 32, 6); // button 3
	vd_prints_xy(help_layer, 76, 58, VDISP_FONT_4x6, 0, a->button_text[2]);
	
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
		
		vdisp_i2s(buf + 1, 2, 10, 1, new_volume);
		vd_prints_xy(help_layer, 115, 58, VDISP_FONT_4x6, 0, buf);
		
		lcd_show_help_layer(help_layer);
		help_layer_timer = 3; // approx 2 seconds
	}
}

char dcs_mode = 0;
char hotspot_mode = 0;

static char snmp_reset_cmnty = 0;

void a_dispatch_key_event( int key_num, int key_event )
{
	if (key_num == A_KEY_BUTTON_APP_MANAGER)
	{
		if (key_event == A_KEY_PRESSED)
		{
			snmp_reset_cmnty = 0;
			
			software_ptt = 0; // prevent TXing forever...
			
			app_manager_select_next();
			lcd_show_help_layer(help_layer);
			
			if (current_app->screen_num == VDISP_REF_LAYER)
			{
				help_layer_timer = 0; // show help forever..
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
		
		if (current_app->key_event_handler != NULL)
		{
			res = current_app->key_event_handler(current_app, key_num, key_event);
		}
		
		if (res == 0) // handler didn't use this event
		{
			if ((key_event == A_KEY_PRESSED) || (key_event == A_KEY_REPEAT))
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

#define REF_NUM_ITEMS 6
#define REF_SELECTION_SPECIAL 6
static char ref_selected_item = 0;
static char ref_items[REF_NUM_ITEMS] = { 0, 0, 0, 0, 1, 2 };
static const char ref_item_max_val[REF_NUM_ITEMS] = { 2, 2, 9, 9, 9, 25 };
static const char * const ref_modes[3] = { "D-STAR Modem",
										   "IP Reflector",
										   "Hotspot     "};
static const char * const ref_types[3] = { "DCS", "TST", "XRF" };


static void ref_print_status (void)
{
	
	vd_prints_xy(VDISP_REF_LAYER, 36, 12, VDISP_FONT_6x8, (ref_selected_item == 0),
		ref_modes[(int) ref_items[0]]);
	
	#define XPOS 10
	vd_prints_xy(VDISP_REF_LAYER, XPOS, 24, VDISP_FONT_6x8, (ref_selected_item == 1),
		ref_types[(int) ref_items[1]]);
	
	
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 3*6, 24, VDISP_FONT_6x8, (ref_selected_item == 2),
		ref_items[2] + 0x30);
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 4*6, 24, VDISP_FONT_6x8, (ref_selected_item == 3),
		ref_items[3] + 0x30);
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 5*6, 24, VDISP_FONT_6x8, (ref_selected_item == 4),
		ref_items[4] + 0x30);
	
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 6*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		0x20);
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 7*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		ref_items[5] + 0x41);
	vd_printc_xy(VDISP_REF_LAYER, XPOS + 8*6, 24, VDISP_FONT_6x8, (ref_selected_item == 5),
		0x20);
	
	#undef XPOS
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
					SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) = 1;
				}
				break;
			
			case A_KEY_BUTTON_2:  // disconnect button
			
				ref_selected_item = 0;
			
				if (dcs_mode != 0)
				{
					dcs_off();
					SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) = 0;
				}
				break;
			
			case A_KEY_BUTTON_3:  // select button
				if (!dcs_is_connected())
				{
					ref_selected_item ++;
					if (ref_selected_item >= REF_NUM_ITEMS)
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
		
		dcs_mode = (ref_items[0] != 0); // "IP Reflector" "Hotspot"
		hotspot_mode = (ref_items[0] == 2); //  "Hotspot"
		
		int n = ref_items[2] * 100 +
				ref_items[3] * 10 +
				ref_items[4];
				
		dcs_select_reflector( n, ref_items[5] + 0x41, ref_items[1] );
		
		SETTING_CHAR(C_REF_TYPE) = ref_items[1];
		SETTING_SHORT(S_REF_SERVER_NUM) = n;
		SETTING_CHAR(C_REF_MODULE_CHAR) = ref_items[5] + 0x41;
		SETTING_CHAR(C_DCS_MODE) = ref_items[0];
		
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
	
	if ((key_num == A_KEY_BUTTON_1) && (event_type == A_KEY_RELEASED))
	{
		software_ptt = 0;
		return 1;
	}
	
	if ((key_num == A_KEY_BUTTON_2) && (event_type == A_KEY_PRESSED))
	{
		if (ambe_get_automute() != 0) // automute is currently on
		{
			ambe_set_automute(0);
		}
		else
		{
			ambe_set_automute(1);
		}
		
		return 1;
	}
	
	return 0;
}


void a_app_manager_init(void)
{
	
	app_context_t * a;
	
	a = a_new_app( "DSTAR", VDISP_MAIN_LAYER);
	a_set_button_text(a, "PTT", "MUTE", "");
	a_set_key_event_handler(a, main_app_key_event_handler);
	
	a = a_new_app( "GPS", VDISP_GPS_LAYER);
	
	a = a_new_app( "REFLECTOR", VDISP_REF_LAYER);
	a_set_button_text(a, "CONNECT", "DISC", "SELECT");
	a_set_key_event_handler(a, ref_app_key_event_handler);
	
	a = a_new_app( "AUDIO", VDISP_AUDIO_LAYER);
	// a_set_button_text(a, "", "", "");
	// a_set_key_event_handler(a, debug_app_key_event_handler);
	
	a = a_new_app( "DEBUG", VDISP_DEBUG_LAYER);
	a_set_button_text(a, "", "REBOOT", "");
	a_set_key_event_handler(a, debug_app_key_event_handler);
	
	
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
	
	if (SETTING_CHAR(C_DCS_MODE) == 1)
	{
		ref_items[0] = 1;
	}		
	
	if ((SETTING_CHAR(C_REF_TYPE) >= 0) &&
		(SETTING_CHAR(C_REF_TYPE) <= 2))
	{
		ref_items[1] = SETTING_CHAR(C_REF_TYPE);
	}
	
	
	dcs_mode = (ref_items[0] != 0); // "IP Reflector" "Hotspot"
	hotspot_mode = (ref_items[0] == 2); //  "Hotspot"
	
	int n = ref_items[2] * 100 +
	ref_items[3] * 10 +
	ref_items[4];
	
	dcs_select_reflector( n, ref_items[5] + 0x41,  ref_items[1] );
	
	if ((dcs_mode != 0)  && 
		(SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) == 1)) 
	{
		ref_selected_item = REF_SELECTION_SPECIAL;
		dcs_on();
	}
	
	ref_print_status();
	
	// TODO error handling
	
	// vd_clear_rect(help_layer, 0, 0, 128, 64);
	
	int i;
	
	for (i=0; i < 112; i++)
	{
		vd_set_pixel(help_layer, i, 56, 0, 1, 1);
	}
	
	
	
#define SIDEBOX_WIDTH 41
// #define SIDEBOX_HEIGHT 12
#define BOX1_YPOS 10
#define BOX2_YPOS 34
	
	for (i=0; i < 7; i++)
	{
		vd_set_pixel(help_layer, 37, 57+i, 0, 1, 1);
		vd_set_pixel(help_layer, 74, 57+i, 0, 1, 1);
		vd_set_pixel(help_layer, 111, 57+i, 0, 1, 1);
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
	
	set_help_text();
}

