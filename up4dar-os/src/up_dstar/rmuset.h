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
 * rmuset.h
 *
 * Created: 01.06.2014 13:11:26
 *  Author: rballis
 */ 


#ifndef RMUSET_H_
#define RMUSET_H_

#define RMUSET_LINE_LENGTH 11
#define RMUSET_MAX_FELD 5

void rmuset_field(int act);
void rmuset_cursor(int act);
void rmuset_print(void);


#endif /* RMUSET_H_ */