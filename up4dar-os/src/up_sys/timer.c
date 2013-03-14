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

#include "timer.h"

#include "semphr.h"
#include "task.h"

#define TIMER_INTERVAL  50

struct timer_slot
{
  int timeout;
  int interval;
  timer_handler callback;
};

xSemaphoreHandle lock;
struct timer_slot slots[TIMER_SLOT_COUNT];

void timer_set_slot(int slot, int interval, timer_handler callback)
{
  if (xSemaphoreTakeRecursive(lock, portMAX_DELAY) == pdTRUE)
  {
    slots[slot].timeout = 0;
    slots[slot].interval = interval;
    slots[slot].callback = callback;
    xSemaphoreGiveRecursive(lock);
  }
}

void timer_task()
{
  for ( ; ; )
  {
    if (xSemaphoreTakeRecursive(lock, portMAX_DELAY) == pdTRUE)
    {
      for (size_t index = 0; index < TIMER_SLOT_COUNT; index ++)
      {
        struct timer_slot* slot = &slots[index];
        if (slot->callback != NULL)
        {
          slot->timeout -= TIMER_INTERVAL;
          if (slot->timeout <= 0)
          {
            slot->callback(index);
            slot->timeout = slot->interval;
          }
        }
      }
      xSemaphoreGiveRecursive(lock);
    }
    vTaskDelay(TIMER_INTERVAL);
  }
}

void timer_init()
{
  lock = xSemaphoreCreateRecursiveMutex();
  memset(slots, 0, sizeof(slots));
  xTaskCreate(timer_task, "Timer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
}
