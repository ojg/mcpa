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
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>

typedef int16_t q13_2;

typedef struct {
	bool (*taskfunc)(void);
	const Task_flag_t bitmask;
} Tasklist_t;

struct Preferences_t {
    q13_2 vol_stepsize;
    int8_t vol_mutedB;
    int8_t vol_startup;
    int8_t vol_min;
    int8_t vol_max;
};

struct Preferences_t preferences;
struct Preferences_t EEMEM eeprom_preferences = {
    .vol_stepsize = 2 << 2,     // in quarter-dB
    .vol_startup = -20,         // in dB
    .vol_min = -96,             // in dB
    .vol_max = 22,              // in dB
};

void cmd_MasterVol(char *);
void cmd_MasterVol_help(void);
void cmd_Debug(char *);
void cmd_Debug_help(void);
void cmd_Prefs(char *);
void cmd_Prefs_help(void);
bool rotary_task(void);
void cs3318_init(void);

uint8_t debuglevel = 0;
#define DEBUG_PRINT(level, format, ...) if (debuglevel >= level) printf(format, ##__VA_ARGS__)

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

extern TWI_Master_t twiMaster;

#define MAX_SLAVES 4
uint8_t cs3318_addr[MAX_SLAVES] = {0x40, 0, 0, 0};

static void cs3318_write(uint8_t chip, uint8_t addr, uint8_t value)
{
    uint8_t data[2] = {addr, value};
    TWI_MasterWrite(&twiMaster, cs3318_addr[chip], data, 2);

    while (twiMaster.status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }
    if (twiMaster.result == TWIM_RESULT_NACK_RECEIVED) {
        printf("Error: I2C NAK received\n");
    }    
}

static uint8_t cs3318_read(uint8_t chip, uint8_t addr)
{
    TWI_MasterWriteRead(&twiMaster, cs3318_addr[chip], &addr, 1, 1);

    while (twiMaster.status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }
    if (twiMaster.result == TWIM_RESULT_NACK_RECEIVED) {
        printf("Error: I2C NAK received\n");
        return 0;
    }

    DEBUG_PRINT(2, "0x%02X\n", twiMaster.readData[0]);
    return twiMaster.readData[0];
}

static q13_2 cs3318_getVolReg(uint8_t chip, uint8_t regaddr) 
{
    q13_2 volume_in_db_x4;
    uint8_t regval;
    uint8_t quarterdb_val;
    regval = cs3318_read(chip, regaddr);
    if (regaddr == 0x11 || regaddr == 0x14 || regaddr == 0x17) { //if master volume registers, get bit in ctrl register
        quarterdb_val = cs3318_read(chip, regaddr+1) & 0x01;
    }
    else { //if channel offset registers, get corresponding bit in quaarter db register
        regaddr--;
        quarterdb_val = cs3318_read(chip, 0x09) & (1 << regaddr);
    }
    volume_in_db_x4 = (((q13_2)regval - 210) << 1) | quarterdb_val;
    return volume_in_db_x4;
}

static void cs3318_setVolReg(uint8_t chip, uint8_t regaddr, q13_2 volume_in_db_x4) 
{
    uint8_t regval = (volume_in_db_x4 >> 1) + 210;
    uint8_t quarterdb_val = volume_in_db_x4 & 1;
    cs3318_write(chip, regaddr, regval);
    if (regaddr == 0x11 || regaddr == 0x14 || regaddr == 0x17) { //if master volume registers, set bit in ctrl register
        cs3318_write(chip, regaddr+1, (cs3318_read(chip, regaddr+1) & 0xFE) | quarterdb_val);
    }        
    else { //if channel offset registers, set corresponding bit in quaarter db register
        regaddr--;
        cs3318_write(chip, 0x09, (cs3318_read(chip, 0x09) & ~(1 << regaddr)) | (quarterdb_val << regaddr));
    }
}

static void cs3318_mute(uint8_t channel, bool mute)
{
    uint8_t chip = 0; //TODO: loop through chips
    if (channel == 0) { //master
        DEBUG_PRINT(1, "%s master\n", mute ? "mute" : "unmute");
        if (mute) {
            cs3318_write(chip, 0x12, cs3318_read(chip, 0x12) | 0x2);
        } else {
            cs3318_write(chip, 0x12, cs3318_read(chip, 0x12) & ~0x2);
        }
    }
    else if (channel < 8) {
        DEBUG_PRINT(1, "%s channel %d\n", mute ? "mute" : "unmute", channel);
        if (mute) {
            cs3318_write(chip, 0x0a, cs3318_read(chip, 0x0a) | 1 << (channel-1));
        } else {
            cs3318_write(chip, 0x0a, cs3318_read(chip, 0x0a) & ~(1 << (channel-1)));
        }
    }
    else {
        printf_P(PSTR("Wrong channel number\n"));
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

    if (volume_in_db_x4 > (preferences.vol_max * 4) || volume_in_db_x4 < (preferences.vol_min * 4)) {
        printf("Value outside range %d to %ddB\n", preferences.vol_max, preferences.vol_min);
        volume_in_db_x4 = preferences.vol_min * 4;
    }
    return volume_in_db_x4;
}

static void cs3318_stepMasterVol(int direction)
{
    if (direction != 0) {
        uint8_t chip = 0; //TODO: loop through chips
        q13_2 volume_in_db_x4 = cs3318_getVolReg(chip, 0x11);
        volume_in_db_x4 += direction * preferences.vol_stepsize;
        if (volume_in_db_x4 <= preferences.vol_max << 2 && volume_in_db_x4 >= preferences.vol_min << 2) {
            DEBUG_PRINT(1, "Set mastervolume to %d: %.2f\n", volume_in_db_x4, (float)volume_in_db_x4 / 4.0f);
            cs3318_setVolReg(chip, 0x11, volume_in_db_x4);
        }
    }
}

void cs3318_init(void)
{
    /* Take CS3318 out of reset */
    CS3318_RESET_PORT.DIRSET = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTCLR = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTSET = CS3318_RESET_PIN_bm;
    
    // TODO: Find and init all chips
    // Read ID of first chip at default addr

    /* Set master volume */
    cs3318_setVolReg(0, 0x11, dB_to_q13_2(preferences.vol_startup, 0));

    /* Power up cs3318 */
    cs3318_write(0, 0xe, 0);
}


void cmd_MasterVol(char * stropt)
{
    int numparams, msd, lsd=0, direction=0;
    char subcmd[10];

    numparams = sscanf(stropt, "%9s %d.%d\n", subcmd, &msd, &lsd);
    if (numparams > 3) {
        printf("Unknown options\n");
        cmd_MasterVol_help();
        return;
    }
    
    if (numparams < 1) {
        uint8_t chip = 0; //TODO: loop through chips
        q13_2 volume_in_db_x4 = cs3318_getVolReg(chip, 0x11);
        printf("Master volume: %.2f dB\n", (float)volume_in_db_x4 / 4.0f);
    }
    else if (!strncmp(subcmd, "up", 2)) {
        direction = 1;
    }
    else if (!strncmp(subcmd, "down", 4)) {
        direction = -1;
    }
    else if (!strncmp(subcmd, "mute", 4)) {
        if (numparams == 1) msd = 0;
        cs3318_mute(msd, true);
    }
    else if (!strncmp(subcmd, "unmute", 6)) {
        if (numparams == 1) msd = 0;
        cs3318_mute(msd, false);
    }
    else if (!strncmp(subcmd, "set", 3)) {
        uint8_t chip = 0; //TODO: loop through chips
        DEBUG_PRINT(1, "Set mastervolume to %d.%02d\n", msd, lsd);
        cs3318_setVolReg(chip, 0x11, dB_to_q13_2(msd, lsd));
    }
    else if (!strncmp(subcmd, "ch", 2)) {
        uint8_t chip = 0; //TODO: find chip from channel
        int channel;
        numparams = sscanf(subcmd, "ch%d", &channel);
        if (numparams != 1 || channel < 1 || channel > 8) {
            printf("Invalid channel\n");
            return;
        }
        cs3318_setVolReg(chip, channel, dB_to_q13_2(msd, lsd));
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
    int numparams, msd, lsd;
    char subcmd[16];

    numparams = sscanf(stropt, "%15s %d.%d\n", subcmd, &msd, &lsd);
    if (numparams > 3) {
        printf("Unknown options\n");
        cmd_Prefs_help();
        return;
    }
    else if (numparams < 1) {
        printf("Preferences:\n");
        printf(" stepsize: %d.%02d dB\n", preferences.vol_stepsize>>2, (preferences.vol_stepsize & 3) * 25);
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
        if (msd <= 22 && msd >= preferences.vol_min) {
            preferences.vol_max = msd;
        }
    }
    else if (!strncmp(subcmd, "min", 3)) {
        if (msd <= preferences.vol_max && msd >= -96) {
            preferences.vol_min = msd;
        }
    }
    else if (!strncmp(subcmd, "startup", 7)) {
        if (msd <= preferences.vol_max && msd >= preferences.vol_min) {
            preferences.vol_startup = msd;
        }
    }
    else if (!strncmp(subcmd, "stepsize", 8)) {
        if (msd <= 20 && (msd > 0 || lsd > 0)) {
            preferences.vol_stepsize = dB_to_q13_2(msd, lsd);
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
