
/*

Copyright (C) 2011,2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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

#ifndef PHYCOMM_H_
#define PHYCOMM_H_

#define SET_QRG	0x44
#define IND_QRG 0x45
#define SET_RMU 0x46
#define IND_RMU 0x47


// void phyCommInit( xQueueHandle dq );
void phyCommInit( xQueueHandle dq, int comPortHandle );
void phyCommSend (const char * buf, int len);
void phyCommSendCmd (const char * cmd, int len);

#endif /* PHYCOMM_H_ */