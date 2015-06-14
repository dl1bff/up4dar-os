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
 * txtask.h
 *
 * Created: 30.03.2013 06:52:56
 *  Author: mdirska
 */ 


#ifndef TXTASK_H_
#define TXTASK_H_



void txtask_init( ambe_q_t * mic );
void set_phy_parameters(void);
void send_dcs_state(void);
void send_feedback(void);

#endif /* TXTASK_H_ */
