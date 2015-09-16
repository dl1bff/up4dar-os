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
 * nodeinfo.h
 *
 * Created: 11.07.2015 13:43:30
 *  Author: rballis
 */ 


#ifndef NODEINFO_H_
#define NODEINFO_H_

#define NODEINFO_LINE_LENGTH 8
#define NODEINFO_MAX_REF 24

void nodeinfo_ref(int act);
void nodeinfo_feld(int act);
void nodeinfo_print(void);


#endif /* NODEINFO_H_ */