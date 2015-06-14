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

#ifndef APRS_H
#define APRS_H

#include "FreeRTOS.h"
#include "gcc_builtin.h"

void aprs_process_gps_data(const char** parameters, size_t count);

size_t build_altitude_extension(char* buffer);
void copy_extension(char* buffer, const char* parameter);
size_t build_position_report(char* buffer, const char** parameters);
size_t build_aprs_call(char* buffer);
void build_packet(const char** parameters);
int has_packet_data(void);
int parse_digits(const char* data, size_t length);
long parse_time(const char* time);
void process_position_fix_data(const char** parameters);

uint8_t aprs_get_slow_data(uint8_t* data);
void aprs_reset(void);

void aprs_handle_cache_event(void);

void aprs_send_beacon(void);
void aprs_init(void);

void calculate_aprs_password(char* password);
void send_aprs_udp_report(void);

void aprs_send_user_report(uint8_t * gps_a_data, uint16_t gps_a_len);
#endif