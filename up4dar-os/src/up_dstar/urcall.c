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
