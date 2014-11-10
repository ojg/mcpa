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
    int slaveaddr, numbytes, numparams;
    uint8_t regaddr;

    numparams = sscanf(stropt, "%i %i %i\n", &slaveaddr, (int*)&regaddr, &numbytes);
    if (numparams == 2) numbytes = 1;
    if (numparams < 2 || numparams > 3) {
        printf("Unknown options\n");
        cmd_iicr_help();
        return;
    }
    if (numbytes > TWIM_READ_BUFFER_SIZE) {
        printf("Too many bytes\n");
        return;
    }

	TWI_MasterWriteRead(&twiMaster, slaveaddr, &regaddr, 1, numbytes);

	while (twiMaster.status != TWIM_STATUS_READY) {
    	/* Wait until transaction is complete. */
	}

    for (int i=0; i<numbytes; i++) {
        printf("0x%02X ", twiMaster.readData[i]);
    }
    printf("\n");
}

void cmd_iicr_help() {
    printf("iicr [chip address] [register address] [number of bytes]\n");
}    

ISR(TWIC_TWIM_vect)
{
    TWI_MasterInterruptHandler(&twiMaster);
}
