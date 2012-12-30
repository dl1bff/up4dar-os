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
 * up_crypto.c
 *
 * Created: 19.08.2012 10:29:32
 *  Author: mdirska
 */ 


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"


#include "curve25519_donna.h"

#include "up_dstar/rtclock.h"
#include "up_dstar/vdisp.h"
#include "up_dstar/ambe_q.h"

#include "gcc_builtin.h"

#include "up_crypto.h"
#include "up_crypto_init.h"
#include "up_dstar/ambe.h"

#include "sha1.h"

struct up_random 
{
	int counter;
	unsigned short adc_value;
	unsigned char ambe_buf[AMBE_Q_DATASIZE];
	unsigned char cpuID[15];
};

static struct up_random randmem;


static unsigned char const server_pubkey1[32] = {
	0x1e, 0x55, 0x08, 0xad, 0x9d, 0xc7, 0xf3, 0xe2,
	0x71, 0xf1, 0x49, 0x03, 0xf1, 0xb2, 0x81, 0xd0,
	0xdd, 0xfd, 0x8a, 0x80, 0x39, 0x82, 0xa7, 0xdf,
	0xe4, 0x1e, 0x1d, 0xe3, 0x9b, 0x27, 0x42, 0x1b
};

static unsigned char const server_pubkey2[32] = {
	0x11, 0x16, 0xd4, 0x4a, 0xe9, 0x80, 0x7f, 0xb5,
	0x41, 0x8d, 0x75, 0x16, 0x2d, 0x57, 0xc9, 0x28,
	0xee, 0x92, 0x1b, 0xe4, 0x08, 0x40, 0x9a, 0x4f,
	0x8c, 0xc4, 0xc1, 0x53, 0x57, 0xd9, 0x9c, 0x64
};

static unsigned char const server_pubkey3[32] = {
	0x15, 0x57, 0xad, 0xb4, 0x64, 0xde, 0x3e, 0xe4,
	0x5d, 0x9a, 0x28, 0x9d, 0xc2, 0x86, 0x57, 0x03,
	0x0d, 0xe2, 0x1b, 0x82, 0xc5, 0xe5, 0x8d, 0xf5,
	0x09, 0xf3, 0xed, 0xcb, 0x01, 0x39, 0x75, 0x43
};

static unsigned char ecc_secret_key[32];
static unsigned char ecc_public_key[32];

// static unsigned char r1[32];

static const unsigned char basepoint[32] = { 9 };
	
static ambe_q_t * mic_ambe_q;

static SHA1Context random_ctx;

static int random_seed = 0;
static unsigned char crypto_init_ready = 0;

int crypto_get_random_bytes (unsigned char * dest, int num_bytes)
{
	if ((num_bytes <= 0) || (num_bytes > 20))
		return -1;
	
	randmem.counter ++;
	
	SHA1Reset(&random_ctx);	
	SHA1Input(&random_ctx, (unsigned char * ) &randmem, sizeof randmem);
	SHA1Result(&random_ctx);
	
	memcpy (dest, & random_ctx.Message_Digest, num_bytes);
	
	return 0;
}


int crypto_get_random_15bit(void)
{
	return crypto_get_random_16bit() & 0x7FFF;
}	

int crypto_get_random_16bit(void)
{
	if (random_seed == 0) // at start of program seed is zero
	{
		memcpy(&random_seed, (unsigned char *) 0x80800204, sizeof random_seed);
		/*
		while (crypto_init_ready == 0) // wait for task to get first real random number
		{
			vTaskDelay(200);
		}
		
		crypto_get_random_bytes ((unsigned char *) &random_seed, sizeof random_seed);
		*/
	}
	
	random_seed = random_seed * 0x343fd + 0x269EC3; // fast PRNG
	return (random_seed >> 0x10) & 0xFFFF;
}

int crypto_is_ready (void)
{
	return crypto_init_ready;
}

static portTASK_FUNCTION( cryptoTask, pvParameters )
{
	// char buf[10];
	// unsigned long i;
	
	memcpy(randmem.cpuID, (unsigned char *) 0x80800204, 15);
	
	vTaskDelay(3000);
	
	vdisp_prints_xy( 0, 0, VDISP_FONT_6x8, 1, "  " );
	
	randmem.adc_value = AVR32_ADC.cdr0; // voltage adc input
	
	ambe_start_encode();
	vTaskDelay(100);
	ambe_q_flush(mic_ambe_q, 1);
	vTaskDelay(200);
	ambe_q_get(mic_ambe_q, randmem.ambe_buf);
	ambe_stop_encode();
	
	vdisp_prints_xy( 0, 0, VDISP_FONT_6x8, 0, "  " );
	
	randmem.counter = rtclock_get_ticks();
	
	crypto_get_random_bytes(ecc_secret_key, 20);
	crypto_get_random_bytes(ecc_secret_key+20, 12);
	
	curve25519_donna(ecc_public_key, ecc_secret_key, basepoint);
	
	crypto_get_random_bytes ((unsigned char *) &random_seed, sizeof random_seed);
	
	crypto_init_ready = 1;
	
	for( ;; )
	{
				
		vTaskDelay(1000);
		
		/*
		i = rtclock_get_ticks();
		
		vdisp_i2s(buf, 9, 10, 0, i);
		vdisp_prints_xy(0, 8, VDISP_FONT_6x8, 0, buf);
		
		curve25519_donna(r1, ecc_secret_key, server_pubkey1);
		
		i = rtclock_get_ticks();
		
		vdisp_i2s(buf, 9, 10, 0, i);
		vdisp_prints_xy(0, 16, VDISP_FONT_6x8, 0, buf);
		
		for (i=0; i < 7; i++)
		{
			vdisp_i2s(buf, 2, 16, 1, ((unsigned char *) &ecc_secret_key) [i]);
			vdisp_prints_xy(i * 18, 24, VDISP_FONT_6x8, 0, buf);
		}
		*/
		
	}
}




void crypto_init(ambe_q_t * microphone_ambe_q)
{
	mic_ambe_q = microphone_ambe_q;
	
	xTaskCreate( cryptoTask, ( signed char * ) "crypto", 1400, NULL,
	tskIDLE_PRIORITY + 1 , ( xTaskHandle * ) NULL );
}