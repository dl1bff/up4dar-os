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
 * lcd.h
 *
 * Created: 26.05.2012 16:39:17
 *  Author: mdirska
 */ 


#ifndef LCD_H_
#define LCD_H_

extern char lcd_current_layer;
extern char lcd_update_screen;

void lcd_init(void);
void lcd_show_layer (int layer);
void lcd_set_backlight (int v);
void lcd_set_contrast (int v);
void lcd_show_help_layer(int help_layer);

#endif /* LCD_H_ */
