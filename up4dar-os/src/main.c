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



#include <asf.h>

#include "board.h"
#include "gpio.h"
#include "power_clocks_lib.h"


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "up_io/serial.h"

#include "up_dstar/phycomm.h"

#include "up_dstar/dstar.h"

#include "up_dstar/txtest.h"

#include "up_io/eth.h"
#include "up_net/ipneigh.h"

#include "up_dstar/vdisp.h"
#include "up_dstar/rtclock.h"
#include "up_dstar/ambe.h"

#define mainLED_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )
#define ledSTACK_SIZE		configMINIMAL_STACK_SIZE



U32 counter = 0;
U32 errorCounter = 0;


/* Structure used to pass parameters to the LED tasks. */
typedef struct LED_PARAMETERS
{
	unsigned portBASE_TYPE uxLED;		/*< The output the task should use. */
	portTickType xFlashRate;	/*< The rate at which the LED should flash. */
} xLEDParameters;

/* The task that is created eight times - each time with a different xLEDParaemtes 
structure passed in as the parameter. */
static void vLEDFlashTask( void *pvParameters );



static unsigned char x_counter = 0;




/*
static U32 counter_FRO = 0;
static U32 counter_RRE = 0;
static U32 counter_ROVR = 0;
*/





uint32_t  pwm_value = 520;

static void set_pwm(void)
{
	char buf[10];
	
	vdisp_i2s(buf, 8, 10, 0, pwm_value);
	vdisp_prints_xy( 0, 56, VDISP_FONT_6x8, 0, buf );
	
	AVR32_PWM.channel[0].cdty = pwm_value;
}



static int touchKeyCounter[6] = { 0,0,0,0,0,0 };

static void vParTestToggleLED( portBASE_TYPE uxLED ) 
{
	
	
	switch(uxLED)
	{
	case 0:
			
			// gpio_toggle_pin(AVR32_PIN_PB27);
			
			// gpio_toggle_pin(AVR32_PIN_PB19);
			
			eth_send_vdisp_frame();
	
			const int pins[6] = {
				AVR32_PIN_PA18,
				AVR32_PIN_PA19,
				AVR32_PIN_PA20,
				AVR32_PIN_PA21,
				AVR32_PIN_PA22,
				AVR32_PIN_PA23
			};
			int i;
				
			for (i=0; i < ((sizeof pins) / (sizeof pins[0])); i++)
			{
				if (gpio_get_pin_value(pins[i]) == 0)
				{
					touchKeyCounter[i] ++;
				}
				else
				{
					touchKeyCounter[i] = 0;
				}					
					
				if ((touchKeyCounter[i] == 2) && (tx_active == 0))
				{
					switch(i)
					{
					case 0:
						vdisp_clear_rect (0, 0, 128, 64);
						vdisp_prints_xy( 30, 48, VDISP_FONT_6x8, 0, "Service Mode" );
						dstarChangeMode(1);
						break;

					case 1:
						vdisp_clear_rect (0, 0, 128, 64);
						vdisp_prints_xy( 30, 48, VDISP_FONT_6x8, 0, "EMR" );
						dstarResetCounters();
						tx_active = 2;
							
							
						break;
										
					case 2:
						vdisp_clear_rect (0, 0, 128, 64);
						vdisp_prints_xy( 30, 48, VDISP_FONT_6x8, 0, "Mode 4 (DVR)" );
						dstarChangeMode(4);
						break;
										
					case 3:
						vdisp_clear_rect (0, 0, 128, 64);
						vdisp_prints_xy( 30, 48, VDISP_FONT_6x8, 0, "Mode 2 (SUM)" );
						dstarChangeMode(2);
						break;
							
					case 4:
						/* vdisp_clear_rect (0, 0, 128, 64);
							dstarResetCounters();
						tx_active = 1;
						*/
							
						pwm_value --;
						set_pwm();
						touchKeyCounter[i] = 0;
						break;
							
					case 5:
							
						pwm_value ++;
						set_pwm();
						touchKeyCounter[i] = 0;
						break;
										
					}
				}

			}
		
			
		break;
		
	case 1:
			// gpio_toggle_pin(AVR32_PIN_PB28);
			//gpio_toggle_pin(AVR32_PIN_PB18);
			
			x_counter ++;
			
		
			rtclock_disp_xy(84, 0, x_counter & 0x02, 1);
			
			
		break;
	}

}

/*-----------------------------------------------------------*/

static void vStartLEDFlashTasks( unsigned portBASE_TYPE uxPriority )
{
unsigned portBASE_TYPE uxLEDTask;
xLEDParameters *pxLEDParameters;
const unsigned portBASE_TYPE uxNumOfLEDs = 2;
// const portTickType xFlashRate = 900;

	/* Create the eight tasks. */
	for( uxLEDTask = 0; uxLEDTask < uxNumOfLEDs; ++uxLEDTask )
	{
		/* Create and complete the structure used to pass parameters to the next 
		created task. */
		pxLEDParameters = ( xLEDParameters * ) pvPortMalloc( sizeof( xLEDParameters ) );
		pxLEDParameters->uxLED = uxLEDTask;
		
		if (uxLEDTask == 0)
		{
			pxLEDParameters->xFlashRate =  40;  // configTICK_RATE_HZ / 5;
		}
		else
		{
			pxLEDParameters->xFlashRate = configTICK_RATE_HZ;
			// pxLEDParameters->xFlashRate /= portTICK_RATE_MS;
		}

		
		

		/* Spawn the task. */
		xTaskCreate( vLEDFlashTask, (signed char *) "LEDx", ledSTACK_SIZE, ( void * ) pxLEDParameters, uxPriority, ( xTaskHandle * ) NULL );
	}
}
/*-----------------------------------------------------------*/


static void vLEDFlashTask( void *pvParameters )
{
xLEDParameters *pxParameters;

	/* Queue a message for printing to say the task has started. */
	//vPrintDisplayMessage( &pcTaskStartMsg );

	pxParameters = ( xLEDParameters * ) pvParameters;

	for(;;)
	{
		/* Delay for half the flash period then turn the LED on. */
		vTaskDelay( pxParameters->xFlashRate / ( portTickType ) 2 );
		vParTestToggleLED( pxParameters->uxLED );

		/* Delay for half the flash period then turn the LED off. */
		vTaskDelay( pxParameters->xFlashRate / ( portTickType ) 2 );
		vParTestToggleLED( pxParameters->uxLED );
	}
}

// ---------------

/*-----------------------------------------------------------*/


/*

static volatile avr32_usart_t  *usart0 = (&AVR32_USART0);

static void vUSART0Task( void *pvParameters )
{
	
	unsigned char blob[8];
	
	for(;;)
	{
		int x,y,i;
		
		for (x=0; x < 16; x++)
		{
			for (y=0; y < 8; y++)
			{
				while ((usart0->csr & AVR32_USART_CSR_TXEMPTY_MASK) == 0)
				{
					// vTaskDelay( 1 );
					taskYIELD();
				}
				usart0->thr = 0x80 | (x << 3) | y;

				vdisp_get_pixel( x << 3, y << 3, blob );
				
				int mask = 0x80;
				
				for (i=0; i < 8; i++)
				{
					int m = 1;
					int d = 0;
					int j;
					
					for (j=0; j < 8; j++)
					{
						if ((blob[j] & mask) != 0)
						{
							d |= m;
						}
						m = m << 1;
					}
							
					while ((usart0->csr & AVR32_USART_CSR_TXEMPTY_MASK) == 0)
					{
						//vTaskDelay( 1 );
						taskYIELD();
					}
								
					usart0->thr = d;

					mask = mask >> 1;					
				}
			}
		}
		
		vTaskDelay( 1 );
		for (i=0; i < 9; i++)
		{
			while ((usart0->csr & AVR32_USART_CSR_TXEMPTY_MASK) == 0)
			{
				//vTaskDelay( 1 );
				taskYIELD();
			}
			usart0->thr = 0x0d;
		}					
	}
	
}

*/

static void vRXEthTask( void *pvParameters )
{
	while (1)
	{
		eth_recv_frame();
		// taskYIELD();
		vTaskDelay(3);
	}		
}



#define LCD_PIN_RES		AVR32_PIN_PA02
#define LCD_PIN_E		AVR32_PIN_PB12
#define LCD_PIN_CS1		AVR32_PIN_PB13
#define LCD_PIN_CS2		AVR32_PIN_PB14
#define LCD_PIN_DI		AVR32_PIN_PB21
#define LCD_PIN_RW		AVR32_PIN_PB22



static void lcd_send(int linksrechts, int rs, int data)
{
	// uint32_t d = data << 24; 
	
	uint32_t d = 0;
	int i;
	
	if (rs != 0)
	{
		gpio_set_pin_high(LCD_PIN_DI);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_DI);
	}
	
	if (linksrechts == 1)
	{
		gpio_set_pin_high(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_high(LCD_PIN_CS2);
	}
	
	for (i=0; i < 8; i++)
	{
		if ((data & 1) != 0)
		{
			d |= (1 << 23);
		}
		
		d = d << 1;
		data = data >> 1;
	}		
	 
	
	gpio_set_group_high(1 /* PORT B */, d);
	gpio_set_group_low(1 /* PORT B */, d ^ 0xFF000000);
	
	
	gpio_set_pin_high(LCD_PIN_E);
	
	taskYIELD();
	
	gpio_set_pin_low(LCD_PIN_E);
	
	
	if (linksrechts == 1)
	{
		gpio_set_pin_low(LCD_PIN_CS1);
	}
	else
	{
		gpio_set_pin_low(LCD_PIN_CS2);
	}
	
	taskYIELD();
}

static void vLCDTask( void *pvParameters )
{
	gpio_set_pin_low(LCD_PIN_RES);
	gpio_set_pin_low(LCD_PIN_E);
	gpio_set_pin_low(LCD_PIN_CS1);
	gpio_set_pin_low(LCD_PIN_CS2);
	gpio_set_pin_low(LCD_PIN_RW);
	
	gpio_set_pin_high(AVR32_PIN_PB19); // kontrast
	gpio_set_pin_high(AVR32_PIN_PB18); // backlight
	
	vTaskDelay( 30 );
	
	gpio_set_pin_high(LCD_PIN_RES);
	
	vTaskDelay( 10 );
	
	lcd_send(1, 0, 0x3f);
	lcd_send(2, 0, 0x3f);
	
	
	unsigned char blob[8];
	
	for(;;)
	{
		int x,y,i;
		
		for (x=0; x < 16; x++)
		{
			for (y=0; y < 8; y++)
			{
				
				int r = ((x >= 8) ? 2 : 1);
				lcd_send(r, 0, 0x40 | ((x & 0x07) << 3));
				lcd_send(r, 0, 0xB8 | (y & 0x07));

				vdisp_get_pixel( x << 3, y << 3, blob );
				
				int mask = 0x80;
				
				for (i=0; i < 8; i++)
				{
					int m = 1;
					int d = 0;
					int j;
					
					for (j=0; j < 8; j++)
					{
						if ((blob[j] & mask) != 0)
						{
							d |= m;
						}
						m = m << 1;
					}
					
					lcd_send(r, 1, d);

					mask = mask >> 1;					
				}
			}
		}
		
		vTaskDelay( 5 );
	}
	
}




int main (void)
{
	board_init();

	unsigned char * pixelBuf;
	
	eth_init(& pixelBuf);
	
	vdisp_init(pixelBuf);
	vdisp_clear_rect(0,0, 128, 64);
	
	ipneigh_init();
		
	vStartLEDFlashTasks( mainLED_TASK_PRIORITY );
	
//	xTaskCreate( vUSART0Task, (signed char *) "USART0", ledSTACK_SIZE, ( void * ) 0, mainLED_TASK_PRIORITY, ( xTaskHandle * ) NULL );
	
	xTaskCreate( vRXEthTask, (signed char *) "rxeth", 300, ( void * ) 0, mainLED_TASK_PRIORITY, ( xTaskHandle * ) NULL );
	
	xTaskCreate( vLCDTask, (signed char *) "LCD", 300, ( void * ) 0, mainLED_TASK_PRIORITY, ( xTaskHandle * ) NULL );
	
	vdisp_prints_xy(0, 0, VDISP_FONT_8x12, 0,  "Universal");
	vdisp_prints_xy(0, 12, VDISP_FONT_8x12, 0, " Platform");
	vdisp_prints_xy(0, 24, VDISP_FONT_8x12, 0, "  for Digital");
	vdisp_prints_xy(0, 36, VDISP_FONT_8x12, 0, "   Amateur Radio");
	
	xQueueHandle dstarQueue;
	
	dstarQueue = xQueueCreate( 10, sizeof (struct dstarPacket) );
	
	dstarInit( dstarQueue );
	
	phyCommInit( dstarQueue );
	
	txtest_init();
	
	ambeInit();
	
	
	vTaskStartScheduler();
  
	return 0;
}
