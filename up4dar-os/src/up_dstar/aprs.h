#ifndef APRS_H
#define APRS_H

#include "FreeRTOS.h"
#include "gcc_builtin.h"

// !! IMPORTANT !!!!
// Please add next settings:


void aprs_process_gps_data(const char** parameters);
size_t aprs_get_slow_data(uint8_t* data);

void aprs_init();

#endif