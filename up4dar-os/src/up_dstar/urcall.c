/*
 * urcall.c
 *
 * Created: 10.03.2014 19:39:58
 *  Author: rballis
 */ 

#include "FreeRTOS.h"
#include "vdisp.h"
#include "r2cs.h"
#include "settings.h"
#include "up_dstar/urcall.h"
#include "up_dstar/r2cs.h"

#include "gcc_builtin.h"

char* getURCALL()
{
	if (r2csURCALL())
		return r2cs_get(r2cs_position());
	else
		return settings.s.urcall + ((SETTING_CHAR(C_DV_USE_URCALL_SETTING) - 1)*CALLSIGN_LENGTH);
}
