/*
 * main.c
 *
 * Created: 06.11.2014 20:32:10
 *  Author: ole
 */ 

#define F_CPU 32000000

#include "board.h"
#include "clksys_driver.h"
#include "usart_driver.h"

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>


static const char welcomeMsg[] = "Welcome to Octogain!\n>";

Task_flag_t taskflags = 0;

#define sleep() __asm__ __volatile__ ("sleep")

int main(void)
{
	char cmdbuf[10] = {0,0,0,0,0,0,0,0,0,0};
	
	LED_PORT.DIRSET = LED_PIN_bm;
	
	CLKSYS_Enable( OSC_RC32MEN_bm );
	do {} while ( CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0 );
	CLKSYS_Main_ClockSource_Select( CLK_SCLKSEL_RC32M_gc );
	CLKSYS_Disable( OSC_RC2MEN_bm | OSC_RC32KEN_bm );

	SLEEP.CTRL = (SLEEP.CTRL & ~SLEEP_SMODE_gm) | SLEEP_SMODE_IDLE_gc;
	SLEEP.CTRL |= SLEEP_SEN_bm;

	USART_init(&USARTD0);

	// Enable global interrupts
	sei();

	printf("%s", welcomeMsg);

	while(1)
	{
		//_delay_ms(100);  // Wait for 500ms
		sleep();
		if (taskflags) {
			fgets(cmdbuf, 10, stdin);
			printf("%s", cmdbuf);
			taskflags = 0;
		}
		LED_PORT.OUTTGL = LED_PIN_bm;
	}
}
