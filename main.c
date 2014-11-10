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
#include "twi_master_driver.h"
#include "cli.h"

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

typedef struct {
	bool (*taskfunc)(void);
	const Task_flag_t bitmask;
} Tasklist_t;

void cmd_iicr(char * stropt);
void cmd_iicr_help();

TWI_Master_t twiMaster;    /*!< TWI master module. */

Task_flag_t taskflags = 0;
const char welcomeMsg[] = "\nWelcome to Octogain!\nType help for list of commands\n";

int main(void)
{
	int i;

	Tasklist_t tasklist[] =
	{
		{cli_task, Task_CLI_bm},
        {NULL, 0}
	};

	LED_PORT.DIRSET = LED_PIN_bm;
	
	CLKSYS_Enable( OSC_RC32MEN_bm );
	do {} while ( CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0 );
	CLKSYS_Main_ClockSource_Select( CLK_SCLKSEL_RC32M_gc );
	CLKSYS_Disable( OSC_RC2MEN_bm | OSC_RC32KEN_bm );

	SLEEP.CTRL = (SLEEP.CTRL & ~SLEEP_SMODE_gm) | SLEEP_SMODE_IDLE_gc;
	SLEEP.CTRL |= SLEEP_SEN_bm;

    register_cli_command("help", cmd_help, cmd_help);
    register_cli_command("iicr", cmd_iicr, cmd_iicr_help);

    /* Initialize debug USART */
	USART_init(&USARTD0);
    
	/* Initialize TWI master. */
	TWI_MasterInit(&twiMaster, &TWIC, TWI_MASTER_INTLVL_LO_gc, TWI_BAUD(F_CPU, 100000));

	// Enable global interrupts
	sei();

	printf("%s%s", welcomeMsg, CLI_PROMPT);

	while(1)
	{
        while (taskflags) { //run as long as any taskflag is set
            i=0;
            while (tasklist[i].taskfunc) { // go through all tasks
                if (taskflags & tasklist[i].bitmask) { //check taskflag
                    if (tasklist[i].taskfunc()) { //run taskfunction
                        taskflags &= ~tasklist[i].bitmask; //if it returns true (done), clear the taskflag
                    }
                }
                i++;
            }
        }
        LED_PORT.OUTTGL = LED_PIN_bm;
        __asm__ __volatile__ ("sleep");
    }
}


void cmd_iicr(char * stropt) {
    uint8_t cs3318_regaddr = 0x81;
	TWI_MasterWriteRead(&twiMaster, 0x40, &cs3318_regaddr, 1, 1);

	while (twiMaster.status != TWIM_STATUS_READY) {
    	/* Wait until transaction is complete. */
	}
    printf("0x%02X\n", twiMaster.readData[0]);
}

void cmd_iicr_help() {
    printf("iicr help text\n");
}    

ISR(TWIC_TWIM_vect)
{
    TWI_MasterInterruptHandler(&twiMaster);
}
