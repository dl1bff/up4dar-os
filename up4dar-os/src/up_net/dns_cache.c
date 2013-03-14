/*

Copyright (C) 2013   Artem Prilutskiy, R3ABM (r3abm@dstar.su)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "dns_cache.h"

#include "semphr.h"

#include "dhcp.h"
#include "dns.h"

#include "up_sys/timer.h"

#define FALSE          0
#define TRUE           1

#define POLL_INTERVAL  200
#define CACHE_EXPIRE   (300 * 1000 / POLL_INTERVAL)

struct dns_cache_slot
{
  const char* host;
  uint8_t address[4];
  int expire;
  dns_cache_handler callback;
};

static xSemaphoreHandle lock;
static struct dns_cache_slot slots[DNS_CACHE_SLOT_COUNT];
static int current = -1;

void dns_cache_set_slot(int slot, const char* host, dns_cache_handler callback)
{
  if (xSemaphoreTakeRecursive(lock, portMAX_DELAY) == pdTRUE)
  {
    slots[slot].host = host;
    slots[slot].expire = 0;
    slots[slot].callback = callback;
    xSemaphoreGiveRecursive(lock);
  }
}

int dns_cache_get_address(int slot, uint8_t* address)
{
  if (xSemaphoreTakeRecursive(lock, portMAX_DELAY) == pdTRUE)
  {
    memcpy(address, slots[slot].address, 4);
    int result = (slots[slot].expire > 0);
    xSemaphoreGiveRecursive(lock);
    return result;
  }
  return FALSE;
}

void dns_cache_event()
{
  int candidate = -1;

  if (xSemaphoreTakeRecursive(lock, portMAX_DELAY) == pdTRUE)
  {

    for (size_t index = 0; index < DNS_CACHE_SLOT_COUNT; index ++)
    {
      if (slots[index].expire > 0)
        slots[index].expire --;
      if ((slots[index].expire == 0) &&
          (slots[index].host != NULL))
        candidate = index;
    }

    if ((current == -1) &&
        (candidate >= 0) &&
        (dhcp_is_ready() != 0) &&
        (dns_get_lock() != 0) &&
        (dns_req_A(slots[candidate].host) == 0))
      current = candidate;

    if ((current >= 0) &&
        (dns_result_available() != 0))
    {
      int result = dns_get_A_addr(slots[current].address);
      dns_release_lock();
      if ( result == 0)
      {
        slots[current].expire = CACHE_EXPIRE;
        if (slots[current].callback != NULL)
          slots[current].callback(current);
      }
      current = -1;
    }

    xSemaphoreGiveRecursive(lock);
  }
}

void dns_cache_init()
{
  lock = xSemaphoreCreateRecursiveMutex();
  memset(slots, 0, sizeof(slots));
  timer_set_slot(TIMER_SLOT_DNS_CACHE, POLL_INTERVAL, dns_cache_event);
}
