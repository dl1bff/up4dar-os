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
 * r2cs.h
 *
 * Created: 04.03.2014 08:37:32
 *  Author: rballis
 */ 


#ifndef R2CS_H_
#define R2CS_H_

#define  R2CS_HISTORY_DIM 5

void r2cs(int layer, int position);
bool r2csURCALL(void);
int r2cs_count(void);
int r2cs_position(void);
char* r2cs_get(int position);
void r2cs_append(const char urcall[8]);
void r2cs_set_orig(void);
void r2cs_print(int layer, int position);


#endif /* R2CS_H_ */