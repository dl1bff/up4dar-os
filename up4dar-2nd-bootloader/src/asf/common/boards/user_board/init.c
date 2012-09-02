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

#include <asf.h>
#include <board.h>
#include <conf_board.h>

#include <gpio.h>




static const gpio_map_t lcd_gpio_map =
{
{ AVR32_PIN_PA02, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// RES
{ AVR32_PIN_PB12, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // E
{ AVR32_PIN_PB13, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// CS1
{ AVR32_PIN_PB14, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // CS2
{ AVR32_PIN_PB21, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D/I
{ AVR32_PIN_PB22, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // R/W
{ AVR32_PIN_PB24, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D0
{ AVR32_PIN_PB25, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D1
{ AVR32_PIN_PB26, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D2
{ AVR32_PIN_PB27, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D3
{ AVR32_PIN_PB28, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },	// D4
{ AVR32_PIN_PB29, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D5
{ AVR32_PIN_PB30, GPIO_DIR_OUTPUT | GPIO_INIT_LOW },  // D6
{ AVR32_PIN_PB31, GPIO_DIR_OUTPUT | GPIO_INIT_LOW }   // D7
};

static const gpio_map_t lcd_pwm_gpio_map =
{
{ AVR32_PWM_6_PIN, AVR32_PWM_6_FUNCTION },  // LCD_Backlight
{ AVR32_PWM_0_PIN, AVR32_PWM_0_FUNCTION }    // LCD_PWM_Kontrast
};


static const gpio_map_t switch_gpio_map =
{
{ AVR32_PIN_PA28, GPIO_DIR_INPUT | GPIO_PULL_UP },	// PTT
{ AVR32_PIN_PA18, GPIO_DIR_INPUT | GPIO_PULL_UP },	// SW1
{ AVR32_PIN_PA19, GPIO_DIR_INPUT | GPIO_PULL_UP },  // SW2
{ AVR32_PIN_PA20, GPIO_DIR_INPUT                },	// SW3 has external pull up
{ AVR32_PIN_PA21, GPIO_DIR_INPUT                },  // SW4 special analog input
{ AVR32_PIN_PA22, GPIO_DIR_INPUT | GPIO_PULL_UP },	// SW5
{ AVR32_PIN_PA23, GPIO_DIR_INPUT | GPIO_PULL_UP }   // SW6
	
};


static const gpio_map_t usart_gpio_map = {
{ AVR32_USART0_RXD_0_0_PIN, AVR32_USART0_RXD_0_0_FUNCTION },
{ AVR32_USART0_TXD_0_0_PIN, AVR32_USART0_TXD_0_0_FUNCTION },
{ AVR32_USART1_RXD_0_0_PIN, AVR32_USART1_RXD_0_0_FUNCTION },
{ AVR32_USART1_TXD_0_0_PIN, AVR32_USART1_TXD_0_0_FUNCTION }
};




void board_init(void)
{
	
		// first change to OSC0 (12MHz)
		pm_enable_osc0_crystal(& AVR32_PM, FOSC0);            // Enable the Osc0 in crystal mode
		pm_enable_clk0(& AVR32_PM, OSC0_STARTUP);                  // Crystal startup time
		pm_switch_to_clock(& AVR32_PM, AVR32_PM_MCSEL_OSC0);  // Then switch main clock to Osc0
		
		
		
		pm_enable_osc1_ext_clock(& AVR32_PM);  // ocs1 is external clock
		pm_enable_clk1(& AVR32_PM, OSC1_STARTUP);
		
		pm_pll_setup(&AVR32_PM
		, 0   // pll
		, 3 // mul
		, 0 // div  ->  f_vfo = 16.384 MHz * 8 = 131.072 MHz
		, 1   // osc
		, 16  // lockcount
		);
		
		pm_pll_set_option(&AVR32_PM
		, 0 // pll
		, 1 // pll_freq  (f_vfo range 80MHz - 180 MHz)
		, 1 // pll_div2  (f_pll1 = f_vfo / 2)
		, 0 // pll_wbwdisable
		);
		
		pm_pll_enable(&AVR32_PM, 0);
		
		pm_wait_for_pll0_locked(&AVR32_PM);
		
		pm_cksel(&AVR32_PM
		, 1, 1 // PBA  (CPU / 4) = 16.384 MHz
		, 0, 0 // PBB  65.536 MHz
		, 0, 0 // HSB	 = CPU 65.536 MHz
		);
		
		flashc_set_wait_state(1);  // one wait state if CPU clock > 33 MHz
		
		pm_switch_to_clock(&AVR32_PM, AVR32_PM_MCCTRL_MCSEL_PLL0); // switch to PLL0
		
		
		// --------------------------------------
		
		// USB clock
		
		// Use 12MHz from OSC0 and generate 96 MHz
		pm_pll_setup(&AVR32_PM, 1,  // pll.
		7,   // mul.
		1,   // div.
		0,   // osc.
		16); // lockcount.

		pm_pll_set_option(&AVR32_PM, 1, // pll.
		1,  // pll_freq: choose the range 80-180MHz.
		1,  // pll_div2.
		0); // pll_wbwdisable.

		// start PLL1 and wait forl lock
		pm_pll_enable(&AVR32_PM, 1);

		// Wait for PLL1 locked.
		pm_wait_for_pll1_locked(&AVR32_PM);

		pm_gc_setup(&AVR32_PM, AVR32_PM_GCLK_USBB,  // gc.
		1,  // osc_or_pll: use Osc (if 0) or PLL (if 1).
		1,  // pll_osc: select Osc0/PLL0 or Osc1/PLL1.
		0,  // diven.
		0); // div.
		pm_gc_enable(&AVR32_PM, AVR32_PM_GCLK_USBB);
		
		// --------------------------------------
	
	// LCD display
	
	gpio_enable_gpio( lcd_gpio_map, sizeof( lcd_gpio_map ) / sizeof( lcd_gpio_map[0] ) );
	
	int i;
	
	for (i=0; i < (sizeof( lcd_gpio_map ) / sizeof( lcd_gpio_map[0] )); i++)
	{
		gpio_configure_pin( lcd_gpio_map[i].pin, lcd_gpio_map[i].function);
	}
	
	
	gpio_enable_module( lcd_pwm_gpio_map, sizeof( lcd_pwm_gpio_map ) / sizeof( lcd_pwm_gpio_map[0] ) );
	
	
	// Backlight
	AVR32_PWM.channel[6].CMR.cpre = 3;
	AVR32_PWM.channel[6].cprd = 1000;
	AVR32_PWM.channel[6].cdty = 500;
	
	AVR32_PWM.ENA.chid6 = 1;
	
	// contrast
	AVR32_PWM.channel[0].CMR.cpre = 3;
	AVR32_PWM.channel[0].cprd = 1000;
	AVR32_PWM.channel[0].cdty = 520;
	
	AVR32_PWM.ENA.chid0 = 1;
	
	
	

	// switches

	gpio_enable_gpio( switch_gpio_map, sizeof( switch_gpio_map ) / sizeof( switch_gpio_map[0] ) );
	
	for (i=0; i < (sizeof( switch_gpio_map ) / sizeof( switch_gpio_map[0] )); i++)
	{
		gpio_configure_pin( switch_gpio_map[i].pin, switch_gpio_map[i].function);
	}
	
	
	// USART
	
	gpio_enable_module( usart_gpio_map, sizeof( usart_gpio_map ) / sizeof( usart_gpio_map[0] ) );
	
	
	
}
