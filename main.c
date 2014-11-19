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
#include <string.h>

typedef struct {
	bool (*taskfunc)(void);
	const Task_flag_t bitmask;
} Tasklist_t;

void cmd_MasterVol(char *);
void cmd_MasterVol_help(void);
void cmd_Debug(char *);
void cmd_Debug_help(void);
bool rotary_task(void);
void CS3318_init(void);

uint8_t debuglevel = 0;
#define DEBUG_PRINT(level, format, ...) if (debuglevel >= level) printf(format, ##__VA_ARGS__)

Task_flag_t taskflags = 0;
const char welcomeMsg[] = "\nWelcome to Octogain!\nType help for list of commands\n";

int main(void)
{
	int i;
    
    /* Define a list of tasks */
	Tasklist_t tasklist[] =
	{
		{cli_task, Task_CLI_bm},
        {rotary_task, Task_Rotary_bm},
        {NULL, 0}
	};

    /* Set clock to 32MHz */
	CLKSYS_Enable( OSC_RC32MEN_bm );
	do {} while ( CLKSYS_IsReady( OSC_RC32MRDY_bm ) == 0 );
	CLKSYS_Main_ClockSource_Select( CLK_SCLKSEL_RC32M_gc );
	CLKSYS_Disable( OSC_RC2MEN_bm | OSC_RC32KEN_bm );

    /* Initialize sleep mode to idle */
	SLEEP.CTRL = (SLEEP.CTRL & ~SLEEP_SMODE_gm) | SLEEP_SMODE_IDLE_gc;

    /* Register cli command functions */
    register_cli_command("help", cmd_help, cmd_help);
    register_cli_command("iicr", cmd_iicr, cmd_iicr_help);
    register_cli_command("iicw", cmd_iicw, cmd_iicw_help);
    register_cli_command("vol", cmd_MasterVol, cmd_MasterVol_help);
    register_cli_command("debug", cmd_Debug, cmd_Debug_help);

    /* Set debug LED pin to output */
    LED_PORT.DIRSET = LED_PIN_bm;

    /* Take CS3318 out of reset */
    CS3318_RESET_PORT.DIRSET = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTSET = CS3318_RESET_PIN_bm;

    /* Initialize debug USART */
	USART_init(&USARTD0);
    
	/* Initialize TWI master. */
	TWI_MasterInit(&TWIC, TWI_MASTER_INTLVL_LO_gc, TWI_BAUD(F_CPU, 100000));

    /* Set up rotary encoder input and irq */
    ROT_PORT.DIRCLR = ROT_A_PIN_bm | ROT_B_PIN_bm;
    ROT_PORT.PIN0CTRL = PORT_OPC_PULLUP_gc;
    ROT_PORT.PIN1CTRL = PORT_OPC_PULLUP_gc;
    ROT_PORT.INTMASK = ROT_A_PIN_bm | ROT_B_PIN_bm;
    ROT_PORT.INTCTRL = PORT_INTLVL_MED_gc;
    PMIC.CTRL |= PMIC_MEDLVLEN_bm;

	/* Enable global interrupts */
	sei();

    /* Init CS3318 */
    CS3318_init();
    
    /* Ready to run */
	printf("%s%s", welcomeMsg, CLI_PROMPT);

	while(1)
	{
        LED_PORT.OUTCLR = LED_PIN_bm;
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
        LED_PORT.OUTSET = LED_PIN_bm;
        SLEEP.CTRL |= SLEEP_SEN_bm;
        __asm__ __volatile__ ("sleep");
        SLEEP.CTRL &= ~SLEEP_SEN_bm;
    }
}

typedef int16_t q13_2;
#define CS3318_ADDR 0x40
extern TWI_Master_t twiMaster;

void cs3318_write(uint8_t addr, uint8_t value)
{
    uint8_t data[2] = {addr, value};
    TWI_MasterWrite(&twiMaster, CS3318_ADDR, data, 2);

    while (twiMaster.status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }

}

uint8_t cs3318_read(uint8_t addr)
{
    TWI_MasterWriteRead(&twiMaster, CS3318_ADDR, &addr, 1, 1);

    while (twiMaster.status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }

    //printf("0x%02X\n", twiMaster.readData[0]);
    return twiMaster.readData[0];
}


void cs3318_setVolReg(uint8_t regaddr, q13_2 volume_in_db_x4) 
{
    uint8_t regval = (volume_in_db_x4 >> 1) + 210;
    uint8_t quarterdb_val = volume_in_db_x4 & 1;
    cs3318_write(regaddr, regval);
    if (regaddr == 0x11 || regaddr == 0x14 || regaddr == 0x17) {
        cs3318_write(regaddr+1, (cs3318_read(regaddr+1) & 0xFE) | quarterdb_val);
    }        
    else {
        regaddr--;
        cs3318_write(0x09, (cs3318_read(0x09) & ~(1 << regaddr)) | (quarterdb_val << regaddr));
    }
}

static q13_2 dB_to_q13_2(int msd, int lsd)
{
    int sign = 1;
    q13_2 volume_in_db_x4 = ((q13_2)msd) << 2;

    if (msd < 0)
        sign = -1;

    if (lsd >= 75) {
        volume_in_db_x4 += sign * 3;
    }
    else if (lsd >= 50 || lsd == 5) {
        volume_in_db_x4 += sign * 2;
    }
    else if (lsd >= 25) {
        volume_in_db_x4 += sign * 1;
    }

    if (volume_in_db_x4 > (22 * 4) || volume_in_db_x4 < (-96 * 4)) {
        printf("Value outside range 22 to -96dB\n");
        volume_in_db_x4 = -96 * 4;
    }
    return volume_in_db_x4;
}

static int vol_stepsize = 2;
static int vol_mutedB = -60;
static int vol_startup = -20;

static void cs3318_stepMasterVol(int direction)
{
    if (direction != 0) {
        uint16_t volreg; //this does not step the quarter-db register
        volreg = cs3318_read(0x11);
        volreg += direction * vol_stepsize;
        if (volreg < 0xfe && volreg >= 0x12)
            DEBUG_PRINT(1, "Set mastervolume to %d\n", ((int16_t)volreg - 210)>>1);
            cs3318_write(0x11, volreg);
    }
}

void CS3318_init(void)
{
    /* Set master volume */
    cs3318_setVolReg(0x11, dB_to_q13_2(vol_startup, 0));

    /* Power up cs3318 */
    cs3318_write(0xe, 0);
}


void cmd_MasterVol(char * stropt)
{
    int numparams, msd, lsd=0, direction=0;
    char subcmd[5];

    numparams = sscanf(stropt, "%4s %d.%d\n", subcmd, &msd, &lsd);
    if (numparams < 1 || numparams > 3) {
        printf("Unknown options\n");
        cmd_MasterVol_help();
        return;
    }
    
    if (!strncmp(subcmd, "up", 2)) {
        direction = 1;
    }
    else if (!strncmp(subcmd, "down", 4)) {
        direction = -1;
    }
    else if (!strncmp(subcmd, "mute", 4)) {
        DEBUG_PRINT(1, "Set mastervolume to %d dB\n", vol_mutedB);
        cs3318_setVolReg(0x11, dB_to_q13_2(vol_mutedB, 0));        
    }
    else if (!strncmp(subcmd, "set", 3)) {
        DEBUG_PRINT(1, "Set mastervolume to %d.%d\n", msd, lsd);
        cs3318_setVolReg(0x11, dB_to_q13_2(msd, lsd));
    }
    else if (!strncmp(subcmd, "ch", 2)) {
        int channel;
        numparams = sscanf(subcmd, "ch%d", &channel);
        if (numparams != 1 || channel < 1 || channel > 8) {
            printf("Invalid channel\n");
            return;
        }
        cs3318_setVolReg(channel, dB_to_q13_2(msd, lsd));
    }
    else {
        printf("Unknown sub-command\n");
        cmd_MasterVol_help();
    }

    cs3318_stepMasterVol(direction);
}

void cmd_MasterVol_help()
{
    printf("vol up\n");
    printf("vol down\n");
    printf("vol mute\n");
    printf("vol set [master value in dB]\n");
    printf("vol ch[channel number] [offset value in dB]\n");
    printf("Example: vol set -23.75\n");
    printf("Example: vol ch3 -2.5\n");
}


bool rotary_task(void)
{
    //direction: old,new:idx
    //CW:  0,1:1 1,3:7 3,2:e 2,0:8
    //CCW: 0,2:2 2,3:b 3,1:d 1,0:4
    static int8_t direction_table[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    static uint8_t current_rotaryval = 0;
    uint8_t new_rotaryval = ROT_PORT.IN & 3;
    int8_t direction = direction_table[current_rotaryval<<2 | new_rotaryval];
    cs3318_stepMasterVol(direction);
    current_rotaryval = new_rotaryval;
    return true;
}

ISR(PORTD_INT_vect)
{
    if (ROT_PORT.INTFLAGS & ROT_A_PIN_bm) {
        ROT_PORT.INTFLAGS |=  ROT_A_PIN_bm;
        taskflags |= Task_Rotary_bm;
    }
    else if (ROT_PORT.INTFLAGS & ROT_B_PIN_bm) {
        ROT_PORT.INTFLAGS |=  ROT_B_PIN_bm;
        taskflags |= Task_Rotary_bm;
    }
}


#define MAX_DEBUGLEVEL 1
void cmd_Debug(char * stropt)
{
    int numparams, dbglvl;

    numparams = sscanf(stropt, "%d\n", &dbglvl);
    if (numparams < 1) {
        printf("Debuglevel = %d\n", debuglevel);
        return;
    }

    if (dbglvl > 0 && dbglvl <= MAX_DEBUGLEVEL) {
        debuglevel = dbglvl;
    }
}

void cmd_Debug_help()
{
    printf("debug [number]\n");
    printf("number is >=0 and less than %d\n", MAX_DEBUGLEVEL);
}
