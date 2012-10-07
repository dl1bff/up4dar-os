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

#define MAX_APP_NAME_LENGTH  8
#define MAX_BUTTON_TEXT_LENGTH 8

typedef struct a_app_context {
	char name[MAX_APP_NAME_LENGTH + 1]; // app name
	
	char button_text[5][MAX_BUTTON_TEXT_LENGTH + 1];
	
	void (*key_event_handler) (void * a, int key_num, int key_event);  // function to be called on key events
	
	void * private_data; // the app can use this to store internal private data
	
	
	
	} app_context_t;


void a_set_app_name ( void * app_context, const char * app_name)
{
	app_context_t * a = (app_context_t *) app_context;
	
	strncpy(a->name, app_name, MAX_APP_NAME_LENGTH);
	
	a->name[MAX_APP_NAME_LENGTH] = 0;
}


void a_set_key_event_handler ( void * app_context, 
	void (*key_event_handler) (void * a, int key_num, int key_event))
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



static int active_app = 0;
static int num_apps = 2;
static int help_layer_timer = 0;
static int help_layer = 0;

// static int app_manager_key_state = 0;


static const app_context_t dstar_app_context = {
	"DSTAR",
	{
		 "", "CONNECT", "MODE", "DCS +", "DCS -"
	},
	NULL,
	NULL
};

static const app_context_t gps_app_context = {
	"GPS",
	{
		"", "REBOOT", "", "", ""
	},
	NULL,
	NULL
};


static void set_help_text (void)
{
	const app_context_t * a = NULL;
	
	if (active_app == 0)
	{
		a = &dstar_app_context;
	}
	else if (active_app == 1)
	{
		a = &gps_app_context;
	}
	
	if (!a)
		return;
	
	vd_clear_rect(help_layer, 0, 0, 32, 6); // name
	vd_prints_xy(help_layer, 0, 0, VDISP_FONT_4x6, 0, a->name);
	
	vd_clear_rect(help_layer, 0, 58, 32, 6); // button 1
	vd_prints_xy(help_layer, 0, 58, VDISP_FONT_4x6, 0, a->button_text[0]);
	
	vd_clear_rect(help_layer, 39, 58, 32, 6); // button 2
	vd_prints_xy(help_layer, 39, 58, VDISP_FONT_4x6, 0, a->button_text[1]);
	
	vd_clear_rect(help_layer, 76, 58, 32, 6); // button 3
	vd_prints_xy(help_layer, 76, 58, VDISP_FONT_4x6, 0, a->button_text[2]);
	
	vd_clear_rect(help_layer, 91, 14, 32, 6); // button UP
	vd_prints_xy(help_layer, 91, 14, VDISP_FONT_4x6, 0, a->button_text[3]);
	
	vd_clear_rect(help_layer, 91, 38, 32, 6); // button UP
	vd_prints_xy(help_layer, 91, 38, VDISP_FONT_4x6, 0, a->button_text[4]);
}


static void app_manager_select_next(void)
{
	active_app ++;
	
	if (active_app >= num_apps)
	{
		active_app = 0;
	}
	
	set_help_text();
	
	switch (active_app)
	{
		case 0:  // DSTAR
			
			lcd_show_layer(0);
			break;
			
		case 1: // GPS
		
			lcd_show_layer(1);
			break;
			
	}
	
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

int dcs_mode = 0;

void a_dispatch_key_event( int key_num, int key_event )
{
	if (key_num != A_KEY_BUTTON_APP_MANAGER)
	{
		// dispatch to current app
		switch (active_app)
		{
			case 0: // DSTAR App
				if (key_event == A_KEY_PRESSED)
				{
					switch(key_num)
					{
						case A_KEY_BUTTON_3: // Mode
							if (dcs_mode != 0)
							{
								if (!dcs_is_connected())
								{
									dcs_mode = 0;
								}
							}
							else
							{
								dcs_mode = 1;
							}
							break;
						
						case A_KEY_BUTTON_2: // CONNECT
							if (dcs_mode != 0)
							{
								dcs_on_off();
							}				
							break;
							
						case A_KEY_BUTTON_UP: // DCS +
							if (dcs_mode != 0)
							{
								dcs_select_reflector(1);
							}								
							break;
							
						case A_KEY_BUTTON_DOWN: // DCS -
							if (dcs_mode != 0)
							{
								dcs_select_reflector(0);
							}								
							break;	
							
					}
				}
				else if (key_event == A_KEY_REPEAT)
				{
					switch(key_num)
					{
						case A_KEY_BUTTON_UP: // DCS +
						if (dcs_mode != 0)
						{
							dcs_select_reflector(1);
						}							
						break;
						
						case A_KEY_BUTTON_DOWN: // DCS -
						if (dcs_mode != 0)
						{
							dcs_select_reflector(0);
						}							
						break;
						
					}
				}
				break;
				
			default:
				switch(key_num)
				{
					case A_KEY_BUTTON_2:
						AVR32_WDT.ctrl = 0x55001001;
						AVR32_WDT.ctrl = 0xAA001001;
						
						break;
						
				}				
			
				break;
			
		}
		return;
	}
	
	switch (key_event)
	{
		case A_KEY_PRESSED:
			
			if (help_layer_timer > 0)
			{
				lcd_show_help_layer(0); // switch off help
				help_layer_timer = 0;
			}
			else
			{
				set_help_text();
				lcd_show_help_layer(help_layer);
				help_layer_timer = 7; // approx 3 seconds
			}			
			break;
		
		
			
		case A_KEY_HOLD_500MS:
			
			app_manager_select_next();
			set_help_text();
			lcd_show_help_layer(help_layer);
			help_layer_timer = 7; // approx 3 seconds
			
			
			break;
			
		
	}
}


void a_app_manager_init(void)
{
	help_layer = vd_new_screen();
	
	// TODO error handling
	
	vd_clear_rect(help_layer, 0, 0, 128, 64);
	
	int i;
	
	for (i=0; i < 112; i++)
	{
		vd_set_pixel(help_layer, i, 56, 0, 1, 1);
	}
	
	
#define SIDEBOX_WIDTH 38
#define SIDEBOX_HEIGHT 12
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
		vd_set_pixel(help_layer, 127-i, BOX1_YPOS, 0, 1, 1);
		vd_set_pixel(help_layer, 127-i, BOX1_YPOS+SIDEBOX_HEIGHT, 0, 1, 1);
		
		vd_set_pixel(help_layer, 127-i, BOX2_YPOS, 0, 1, 1);
		vd_set_pixel(help_layer, 127-i, BOX2_YPOS+SIDEBOX_HEIGHT, 0, 1, 1);
		
		vd_set_pixel(help_layer, i, 7, 0, 1, 1);
	}
	
	for (i=0; i <= SIDEBOX_HEIGHT; i++)
	{
		vd_set_pixel(help_layer, 127-SIDEBOX_WIDTH, BOX1_YPOS+i, 0, 1, 1);
		vd_set_pixel(help_layer, 127-SIDEBOX_WIDTH, BOX2_YPOS+i, 0, 1, 1);
	}

	// vd_prints_xy(help_layer, 4, 58, VDISP_FONT_4x6, 0,"TEST");
}

