
/*

Copyright (C) 2013   Michael Dirska, DL1BFF (dl1bff@mdx.de)

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

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

#include "slow_data.h"
#include "settings.h"
#include "gps.h"
#include "aprs.h"

uint8_t slow_data[5];
int slow_data_count = 0;


size_t get_slow_data_chunk(uint8_t* data)
{
  if (settings.s.dprs_source == 'A')
    return aprs_get_slow_data(data);
  else
    return gps_get_slow_data(data);
}

void build_slow_data(uint8_t* buffer, char last, char frame, int duration)
{
  if (last != 0)
  {
    buffer[0] = 0x55;
    buffer[1] = 0x55;
    buffer[2] = 0x55;
    return;
  }

  if (frame == 0)
  {
    buffer[0] = 0x55;
    buffer[1] = 0x2d;
    buffer[2] = 0x16;
    return;
  }

  if ((frame <= 8) && (duration < 20))  // send tx_msg only in first frame
  {
    int index = (frame - 1) >> 1;
    if (frame & 1)
    {
      buffer[0] = (0x40 | index) ^ 0x70;
      buffer[1] = settings.s.txmsg[index * 5 + 0] ^ 0x4F;
      buffer[2] = settings.s.txmsg[index * 5 + 1] ^ 0x93;
    }
    else
    {
      buffer[0] = settings.s.txmsg[index * 5 + 2] ^ 0x70;
      buffer[1] = settings.s.txmsg[index * 5 + 3] ^ 0x4F;
      buffer[2] = settings.s.txmsg[index * 5 + 4] ^ 0x93;
    }
    return;
  }

  if (frame & 1)
  {
    slow_data_count = get_slow_data_chunk(slow_data);
    if (slow_data_count > 0)
    {
      buffer[0] = (0x30 | slow_data_count) ^ 0x70;
      buffer[1] = slow_data[0] ^ 0x4F;
      buffer[2] = slow_data[1] ^ 0x93;
      return;
    }
  }
  else
  {
    if (slow_data_count > 2)
    {
      buffer[0] = slow_data[2] ^ 0x70;
      buffer[1] = slow_data[3] ^ 0x4F;
      buffer[2] = slow_data[4] ^ 0x93;
      return;
    }
  }

  buffer[0] = 0x16;
  buffer[1] = 0x29;
  buffer[2] = 0xf5;
}
