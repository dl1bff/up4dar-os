/*

Copyright (C) 2012   Michael Dirska, DL1BFF (dl1bff@mdx.de)

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
 * snmp_data.h
 *
 * Created: 06.05.2012 13:46:44
 *  Author: mdirska
 */ 


#ifndef SNMP_DATA_H_
#define SNMP_DATA_H_


int snmp_encode_int ( int32_t value, uint8_t * res, int * res_len, int maxlen );

#define SNMP_GET_FUNC(func)   int (func) (int32_t arg, uint8_t * res, int * res_len, int maxlen);

#define SNMP_SET_FUNC(func)   int (func) (int32_t arg, const uint8_t * req, int req_len);


SNMP_GET_FUNC( snmp_get_voltage )

SNMP_GET_FUNC ( snmp_get_phy_cpuid )

SNMP_GET_FUNC ( snmp_get_phy_sysinfo )

SNMP_GET_FUNC ( snmp_get_phy_sysparam )
SNMP_SET_FUNC ( snmp_set_phy_sysparam )

SNMP_GET_FUNC ( snmp_get_flashstatus )
SNMP_SET_FUNC ( snmp_set_flashstatus )

SNMP_GET_FUNC ( snmp_get_setting_long )
SNMP_SET_FUNC ( snmp_set_setting_long )

SNMP_GET_FUNC ( snmp_get_setting_short )
SNMP_SET_FUNC ( snmp_set_setting_short )

SNMP_GET_FUNC ( snmp_get_setting_char )
SNMP_SET_FUNC ( snmp_set_setting_char )

#endif /* SNMP_DATA_H_ */