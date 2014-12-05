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
#include "cs3318_driver.h"
#include "preferences.h"

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>


typedef struct {
	bool (*taskfunc)(void);
	const Task_flag_t bitmask;
} Tasklist_t;

struct Preferences_t preferences;

struct Preferences_t EEMEM eeprom_preferences = {
    .vol_stepsize = 2 << 2,     // in quarter-dB
    .vol_startup = -20,         // in dB
    .vol_min = -96,             // in dB
    .vol_max = 22,              // in dB
};

struct Preferences_t * get_preferences(void) {
    return &preferences;
}

void cmd_MasterVol(char *);
void cmd_MasterVol_help(void);
void cmd_Debug(char *);
void cmd_Debug_help(void);
void cmd_Prefs(char *);
void cmd_Prefs_help(void);
bool rotary_task(void);

uint8_t debuglevel = 0;
inline uint8_t get_debuglevel() {
    return debuglevel;
}

Task_flag_t taskflags = 0;

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
    set_sleep_mode(SLEEP_SMODE_IDLE_gc);

    /* Register cli command functions */
    register_cli_command("help", cmd_help, cmd_help_help);
    register_cli_command("iicr", cmd_iicr, cmd_iicr_help);
    register_cli_command("iicw", cmd_iicw, cmd_iicw_help);
    register_cli_command("vol", cmd_MasterVol, cmd_MasterVol_help);
    register_cli_command("prefs", cmd_Prefs, cmd_Prefs_help);
    register_cli_command("debug", cmd_Debug, cmd_Debug_help);

    /* Set debug LED pin to output */
    LED_PORT.DIRSET = LED_PIN_bm;

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
    
    /* Read preferences from EEPROM to SRAM */
    eeprom_read_block(&preferences, &eeprom_preferences, sizeof(preferences));

    /* Init CS3318 */
    cs3318_init();
    
    /* Ready to run */
	printf_P(PSTR("\nWelcome to Octogain!\nReset status %d\nType help for list of commands\n%s"), RST.STATUS, CLI_PROMPT);
    RST.STATUS |= 0x3F;

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
        LED_PORT.OUTSET = LED_PIN_bm;
        sleep_enable();
        sleep_cpu();
        sleep_disable();
    }
}

void cmd_MasterVol(char * stropt)
{
    int numparams, direction=0;
    char subcmd[10];
    float vol_db = 0.0f;

    numparams = sscanf(stropt, "%9s %f\n", subcmd, &vol_db);
    if (numparams > 2) {
        printf("Unknown options\n");
        cmd_MasterVol_help();
        return;
    }
    
    if (numparams < 1) {
        uint8_t chip = 0; //TODO: loop through chips
        q13_2 volume_in_db_x4 = cs3318_getVolReg(chip, 0x11);
        printf("Master volume: %.2f dB\n", Q13_2_TO_FLOAT(volume_in_db_x4));
    }
    else if (!strncmp(subcmd, "up", 2)) {
        direction = 1;
    }
    else if (!strncmp(subcmd, "down", 4)) {
        direction = -1;
    }
    else if (!strncmp(subcmd, "mute", 4)) {
        cs3318_mute(numparams == 1 ? 0 : (uint8_t)vol_db, true);
    }
    else if (!strncmp(subcmd, "unmute", 6)) {
        cs3318_mute(numparams == 1 ? 0 : (uint8_t)vol_db, false);
    }
    else if (!strncmp(subcmd, "set", 3)) {
        uint8_t chip = 0; //TODO: loop through chips
        DEBUG_PRINT(1, "Set mastervolume to %.2fdB\n", vol_db);
        cs3318_setVolReg(chip, 0x11, FLOAT_TO_Q13_2(vol_db));
    }
    else if (!strncmp(subcmd, "ch", 2)) {
        uint8_t chip = 0; //TODO: find chip from channel
        int channel;
        numparams = sscanf(subcmd, "ch%d", &channel);
        if (numparams != 1 || channel < 1 || channel > 8) {
            printf("Invalid channel\n");
            return;
        }
        cs3318_setVolReg(chip, channel, FLOAT_TO_Q13_2(vol_db));
    }
    else {
        printf("Unknown sub-command\n");
        cmd_MasterVol_help();
    }

    cs3318_stepMasterVol(direction);
}

void cmd_MasterVol_help()
{
    printf_P(PSTR( \
    "vol up\n" \
    "vol down\n" \
    "vol mute [channel]\n" \
    "vol unmute [channel]\n" \
    "vol set [master value in dB]\n" \
    "vol ch[channel number] [offset value in dB]\n" \
    "Example: vol set -23.75\n" \
    "Example: vol ch3 -2.5\n"));
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
    LED_PORT.OUTCLR = LED_PIN_bm;

    if (ROT_PORT.INTFLAGS & ROT_A_PIN_bm) {
        ROT_PORT.INTFLAGS |=  ROT_A_PIN_bm;
        taskflags |= Task_Rotary_bm;
    }
    else if (ROT_PORT.INTFLAGS & ROT_B_PIN_bm) {
        ROT_PORT.INTFLAGS |=  ROT_B_PIN_bm;
        taskflags |= Task_Rotary_bm;
    }
}


#define MAX_DEBUGLEVEL 3
void cmd_Debug(char * stropt)
{
    int numparams, dbglvl;

    numparams = sscanf(stropt, "%d\n", &dbglvl);
    if (numparams < 1) {
        printf("Debuglevel = %d\n", debuglevel);
        return;
    }

    if (dbglvl >= 0 && dbglvl <= MAX_DEBUGLEVEL) {
        debuglevel = dbglvl;
    }
}

void cmd_Debug_help()
{
    printf_P(PSTR( \
    "debug [number]\n" \
    "number is >=0 and less than %d\n"), MAX_DEBUGLEVEL);
}


void cmd_Prefs(char * stropt)
{
    int numparams;
    char subcmd[16];
    float vol_db;

    numparams = sscanf(stropt, "%15s %f\n", subcmd, &vol_db);
    if (numparams > 3) {
        printf("Unknown options\n");
        cmd_Prefs_help();
        return;
    }
    else if (numparams < 1) {
        printf("Preferences:\n");
        printf(" stepsize: %.2f dB\n", Q13_2_TO_FLOAT(preferences.vol_stepsize));
        printf(" mute: %d dB\n", preferences.vol_mutedB);
        printf(" startup master: %d dB\n", preferences.vol_startup);
        printf(" max limit: %d dB\n", preferences.vol_max);
        printf(" min limit: %d dB\n", preferences.vol_min);
        return;
    }
    else if (!strncmp(subcmd, "save", 4)) {
        eeprom_update_block(&preferences, &eeprom_preferences, sizeof(preferences));
    }
    else if (!strncmp(subcmd, "max", 3)) {
        if (vol_db <= 22.0f && vol_db >= preferences.vol_min) {
            preferences.vol_max = vol_db;
        }
    }
    else if (!strncmp(subcmd, "min", 3)) {
        if (vol_db <= preferences.vol_max && vol_db >= -96.0f) {
            preferences.vol_min = vol_db;
        }
    }
    else if (!strncmp(subcmd, "startup", 7)) {
        if (vol_db <= preferences.vol_max && vol_db >= preferences.vol_min) {
            preferences.vol_startup = vol_db;
        }
    }
    else if (!strncmp(subcmd, "stepsize", 8)) {
        if (vol_db <= 20.0f && vol_db > 0.0f) {
            preferences.vol_stepsize = FLOAT_TO_Q13_2(vol_db);
        }
    }
    else {
        printf("Unknown sub-command\n");
        cmd_Prefs_help();
    }
}

void cmd_Prefs_help(void)
{
    printf_P(PSTR( \
    "prefs save <- write values to eeprom"
    "prefs max [value in dB]\n" \
    "prefs min [value in dB]\n" \
    "prefs startup [value in dB]\n" \
    "prefs stepsize [value in 1/4 dB resolution]\n" \
    "example: prefs stepsize 2.5\n"));
}
