/*
 * octogain.c
 *
 * Created: 06.11.2014 20:32:10
 *  Author: ole
 */ 

#define F_CPU 2000000

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	PORTD.DIRSET = PIN5_bm;
	
	while(1)
	{
		_delay_ms(500);  // Wait for 500ms
		PORTD.OUTTGL = PIN5_bm;
	}
}
