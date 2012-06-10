/*

Copyright (C) 2011   Denis Bederov, DL3OCK (denis.bederov@gmx.de)

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
    Example:
	
	  unsigned char orig_header[41];
	  
	  // put 39 header bytes in orig_header[0..38]
	
      unsigned short crc_orig_header =
             rx_dstar_crc_header(orig_header);
      
      orig_header[39] =  crc_orig_header       & 0xff;
      orig_header[40] = (crc_orig_header >> 8) & 0xff;


*/


#include "rx_dstar_crc_header.h"


unsigned short rx_dstar_crc_header(const unsigned char* header){
  // Generatorpolynom G(x) = x^16 + x^12 + x^5 + 1
  // ohne die fuehrende 1 UND in umgekehrter Reihenfolge
  register const unsigned short genpoly = 0x8408;
  
  register unsigned short crc = 0xffff;
  
  for (char i=0; i<39; ++i){
    crc ^= *header++;
    for (char j=0; j<8; ++j){
      if ( crc & 0x1 ) {
        crc >>= 1;
        crc ^= genpoly;
      } else {
        crc >>= 1;
      }
    }
  }
  
  // Beachte die Reihenfolge der CRC-Bytes!!!
  // Zunaechst kommt Low- und dann High-Byte
  // in "LSB first" Reihenfolge.
  return (crc ^ 0xffff);        // invertiere das Ergebnis
}


unsigned short rx_dstar_crc_data(const unsigned char* data, int num){
  // Generatorpolynom G(x) = x^16 + x^12 + x^5 + 1
  // ohne die fuehrende 1 UND in umgekehrter Reihenfolge
  register const unsigned short genpoly = 0x8408;
  
  register unsigned short crc = 0xffff;
  
  for (int i=0; i<num; ++i){
    crc ^= *data++;
    for (char j=0; j<8; ++j){
      if ( crc & 0x1 ) {
        crc >>= 1;
        crc ^= genpoly;
      } else {
        crc >>= 1;
      }
    }
  }
  
  // Beachte die Reihenfolge der CRC-Bytes!!!
  // Zunaechst kommt Low- und dann High-Byte
  // in "LSB first" Reihenfolge.
  return (crc ^ 0xffff);        // invertiere das Ergebnis
}
