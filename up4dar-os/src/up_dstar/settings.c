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
 * settings.c
 *
 * Created: 04.06.2012 17:53:43
 *  Author: mdirska
 */ 



#include "FreeRTOS.h"

#include "gcc_builtin.h"

#include "settings.h"

#include "up_net/snmp_data.h"
#include "flashc.h"
#include "rx_dstar_crc_header.h"


settings_t settings;

static struct
{
	short server_num;
	char module_char;
	char type;
	int dcs_connect_after_boot;
} ref_home;

const limits_t long_values_limits[NUM_LONG_VALUES] = {
	{  0,  0,  0  },
	{  0,  0,  0  },
	{  0,  0,  0  },
	{  0,  0,  0  },
	{  0,  0,  0  },
	{  0,  0,  0  },
	{  0,  0,  0  }
};

const limits_t short_values_limits[NUM_SHORT_VALUES] = {
	// #define S_STANDBY_BEEP_FREQUENCY		0
	{  200,		3000,		1000  },
	// #define S_STANDBY_BEEP_DURATION		1
	{  20,		500,		100  },
	// #define S_PTT_BEEP_FREQUENCY			2
	{  200,		3000,		600  },
	// #define S_PTT_BEEP_DURATION			3
	{  20,		500,		100  },
	// #define S_PHY_TXDELAY				4
	{  0,		255,		50  },
	// #define S_PHY_MATFST					5
	{  0,		255,		0  },
	// #define S_PHY_LENGTHOFVW				6
	{  0,		255,		1  },
	// #define S_PHY_RXDEVFACTOR			7
	{  0,		2000,		45  },
	// #define S_RPTR_BEEP_FREQUENCY		8
	{  200,		3000,		400  },
	// #define S_RPTR_BEEP_DURATION			9
	{  20,		500,		100  },
	// #define S_REF_SERVER_NUM				10
	{  1,		999,		1  }
};

const limits_t char_values_limits[NUM_CHAR_VALUES] = {
	// #define C_STANDBY_BEEP_VOLUME		0
	{  0,		100,		10  },
	// #define C_PTT_BEEP_VOLUME			1
	{  0,		100,		50  },
	//	#define C_PHY_TXGAIN				2
	{  -128,		127,		16  },
	// #define C_PHY_RXINV					3
	{  0,		 1,			0  },
	// #define C_PHY_TXDCSHIFT				4
	{  -128,		127,		30  },
	// #define C_DV_USE_RPTR_SETTING		5
	{  1,		 5,			1  },
	// #define C_DV_USE_URCALL_SETTING		6
	{  1,		 10,		1  },
	// #define C_DV_DIRECT					7
	{  0,		 1,			0  },
	// #define C_DPRS_ENABLED				8
	{  0,		 1,			0  },
	// #define C_DPRS_SYMBOL				9
	{  0,		 5,			2  },
	// #define C_DISP_CONTRAST				10
	{  0,		 100,			50  },
	// #define C_DISP_BACKLIGHT				11
	{  0,		 100,			50  },
	// #define C_SPKR_VOLUME				12
	{  -57,		 6,			0  },
	// #define C_STANDBY_BEEP_VOLUME		13
	{  0,		100,		10  },
	// #define C_REF_MODULE_CHAR			14
	{  65,		90,		67  },
	// #define C_DISABLE_UDP_BEACON			15
	{  0,		1,		0  },
	// #define C_DCS_MODE					16
	{  0,		3,		0	  },
	// #define C_DCS_CONNECT_AFTER_BOOT		17
	{  0,		1,		0	  },
	// #define C_REF_TYPE					18
	{  0,		1,		0	  },
	// #define C_REF_SOURCE_MODULE_CHAR		19
	{  0,		1,		0	  },
	// #define C_RMU_ENABLED				20
	{  0,		1,		0	  },
	// #define C_REF_TIMER					21
	{  0,		1,		0	  },
	// #define C_RMU_QRG_STEP				22
	{  0,		1,		0	  }
};




#define USER_PAGE_CHECKSUM  126
#define BOOT_LOADER_CONFIGURATION 127


static uint32_t get_crc(void)
{
	return rx_dstar_crc_data( (uint8_t *) &settings, 504) | (SETTINGS_VERSION << 16);
}


void settings_init(void)
{
	memcpy(& settings, (const void *) AVR32_FLASHC_USER_PAGE, 512);
	
	uint32_t chk = get_crc();
	
	if (chk != settings.settings_words[USER_PAGE_CHECKSUM])  // checksum wrong, set default values
	{
		int i;
		
		memset (&settings, 0, 504); // fill user page with 0
		
		for (i=0; i < NUM_LONG_VALUES; i++)
		{
			settings.s.long_values[i] = long_values_limits[i].init_value;
		}
		
		for (i=0; i < NUM_SHORT_VALUES; i++)
		{
			settings.s.short_values[i] = short_values_limits[i].init_value;
		}
		
		for (i=0; i < NUM_CHAR_VALUES; i++)
		{
			settings.s.char_values[i] = char_values_limits[i].init_value;
		}
				
		memset(settings.s.rpt1, ' ', CALLSIGN_LENGTH * NUM_RPT_SETTINGS);
		memset(settings.s.rpt2, ' ', CALLSIGN_LENGTH * NUM_RPT_SETTINGS);
		memset(settings.s.urcall, ' ', CALLSIGN_LENGTH * NUM_URCALL_SETTINGS);
		
		memset(settings.s.dprs_msg, ' ', DPRS_MSG_LENGTH);
		memset(settings.s.txmsg, ' ', TXMSG_LENGTH);
		
		memcpy(settings.s.my_callsign, "NOCALL  ", CALLSIGN_LENGTH);
		memcpy(settings.s.my_ext, "    ", CALLSIGN_EXT_LENGTH);
		memcpy(settings.s.rpt1, "DB0DF  B", CALLSIGN_LENGTH);
		memcpy(settings.s.rpt2, "DB0DF  G", CALLSIGN_LENGTH);
		memcpy(settings.s.urcall + (0*CALLSIGN_LENGTH), "CQCQCQ  ", CALLSIGN_LENGTH);
		memcpy(settings.s.urcall + (1*CALLSIGN_LENGTH), "       U", CALLSIGN_LENGTH);
		memcpy(settings.s.urcall + (2*CALLSIGN_LENGTH), "CQCQ DVR", CALLSIGN_LENGTH);
		memcpy(settings.s.qrg_tx, "430375000", QRG_LENGTH);
		memcpy(settings.s.qrg_rx, "430375000", QRG_LENGTH);
	}
}

void settings_get_home_ref(void)
{
	SETTING_SHORT(S_REF_SERVER_NUM) = ref_home.server_num;
	SETTING_CHAR(C_REF_MODULE_CHAR) = ref_home.module_char;
	SETTING_CHAR(C_REF_TYPE) = ref_home.type;
	SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT) = ref_home.dcs_connect_after_boot;
}

void settings_set_home_ref(void)
{
	ref_home.server_num = SETTING_SHORT(S_REF_SERVER_NUM);
	ref_home.module_char = SETTING_CHAR(C_REF_MODULE_CHAR);
	ref_home.type = SETTING_CHAR(C_REF_TYPE);
	ref_home.dcs_connect_after_boot = SETTING_CHAR(C_DCS_CONNECT_AFTER_BOOT);
}

void settings_write(void)
{
	uint32_t chk = get_crc();
		
	if (chk != settings.settings_words[USER_PAGE_CHECKSUM])
	{
		settings.settings_words[USER_PAGE_CHECKSUM] = chk;
		settings.settings_words[BOOT_LOADER_CONFIGURATION] = 0x929E1424; // boot loader config word
			
		flashc_memcpy(AVR32_FLASHC_USER_PAGE, & settings, 512, TRUE);
	}
}

int snmp_get_flashstatus (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	uint32_t chk = get_crc();
	
	if (chk != settings.settings_words[USER_PAGE_CHECKSUM])
	{
		res[0] = 1;  // settings have changed
	}
	else
	{
		res[0] = 0; // settings are identical to flash
	}		
		
	*res_len = 1;
	
	return 0;
}

int snmp_set_flashstatus (int32_t arg, const uint8_t * req, int req_len)
{
	if (req[ req_len - 1] == 2) // least significant byte == 2
	{
		uint32_t chk = get_crc();
	
		if (chk != settings.settings_words[USER_PAGE_CHECKSUM])
		{
			settings.settings_words[USER_PAGE_CHECKSUM] = chk;
			settings.settings_words[BOOT_LOADER_CONFIGURATION] = 0x929E1424; // boot loader config word
			
			flashc_memcpy(AVR32_FLASHC_USER_PAGE, & settings, 512, TRUE);
		
			settings_set_home_ref();
		}
	}
	
	return 0;
}

int snmp_get_setting_long (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	memcpy(res, settings.s.long_values + arg, 4); // assuming big-endian		
	*res_len = 4;
	return 0;
}

int snmp_get_setting_short (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	memcpy(res, settings.s.short_values + arg, 2); // assuming big-endian		
	*res_len = 2;
	return 0;
}

int snmp_get_setting_char (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	*res = settings.s.char_values[arg]; 	
	*res_len = 1;
	return 0;
}

int snmp_get_setting_bool (int32_t arg, uint8_t * res, int * res_len, int maxlen)
{
	*res = SETTING_BOOL(arg);
	*res_len = 1;
	return 0;
}

int snmp_set_setting_long (int32_t arg, const uint8_t * req, int req_len)
{
	limits_t limit = long_values_limits[arg];
	int value = 0;
	if ((req[0] & 0x80) != 0)
	{
		value = -1;
	}
	int i;
	for (i=0; i < req_len; i++)
	{
		value = (value << 8) | req[i];
	}
	if (value > limit.max_value)
	{
		return 1;
	}
	if (value < limit.min_value)
	{
		return 1;
	}
	settings.s.long_values[arg] = value;
	return 0;
}	

int snmp_set_ipv4_addr (int32_t arg, const uint8_t * req, int req_len)
{
	if (req_len != 4)
	{
		return 1;
	}
	
	memcpy(settings.s.long_values + arg, req, 4);
	return 0;
}

int snmp_set_setting_short (int32_t arg, const uint8_t * req, int req_len)
{
	limits_t limit = short_values_limits[arg];
	int value = 0;
	if ((req[0] & 0x80) != 0)
	{
		value = -1;
	}
	int i;
	for (i=0; i < req_len; i++)
	{
		value = (value << 8) | req[i];
	}
	if (value > limit.max_value)
	{
		return 1;
	}
	if (value < limit.min_value)
	{
		return 1;
	}
	settings.s.short_values[arg] = value;
	return 0;
}	

int snmp_set_setting_char (int32_t arg, const uint8_t * req, int req_len)
{
	limits_t limit = char_values_limits[arg];
	int value = 0;
	if ((req[0] & 0x80) != 0)
	{
		value = -1;
	}
	int i;
	for (i=0; i < req_len; i++)
	{
		value = (value << 8) | req[i];
	}
	if (value > limit.max_value)
	{
		return 1;
	}
	if (value < limit.min_value)
	{
		return 1;
	}
	settings.s.char_values[arg] = value;
	return 0;
}	

int snmp_set_setting_bool (int32_t arg, const uint8_t * req, int req_len)
{
	int value = 0;
	if ((req[0] & 0x80) != 0)
	{
		value = -1;
	}
	int i;
	for (i=0; i < req_len; i++)
	{
		value = (value << 8) | req[i];
	}
	
	SETTING_SET_BOOL(arg, value);
	return 0;
}