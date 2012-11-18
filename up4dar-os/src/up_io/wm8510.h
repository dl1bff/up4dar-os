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
 * wm8510.h
 *
 * Created: 22.04.2012 14:49:58
 *  Author: mdirska
 */ 


#ifndef WM8510_H_
#define WM8510_H_

#include "up_dstar/audio_q.h"

void wm8510Init( audio_q_t * tx, audio_q_t * rx );
void wm8510_beep(int duration_ms, int frequency_hz, int volume_percent);
int wm8510_get_spkr_volume (void);
void wm8510_set_spkr_volume (int vol);

#endif /* WM8510_H_ */
