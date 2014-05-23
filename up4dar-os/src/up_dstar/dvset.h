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
 * call.h
 *
 * Created: 14.02.2014 23:19:50
 *  Author: rballis
 */ 


#ifndef DVSET_H_
#define DVSET_H_

#define DVSET_LINE_LENGTH 22

void dvset_field(int act);
void dvset_cursor(int act);
void dvset_select(bool select);
void dvset_cancel(void);
void dvset_clear(void);
void dvset_store(void);
void dvset_goedit(void);
void dvset_backspace(void);
bool dvset_isselected(void);
bool dvset_isedit(void);
void dvset(void);
void dvset_print(void);


#endif /* DVSET_H_ */