/*
 * cs3318_driver.c
 *
 * Created: 02.12.2014 14:05:33
 *  Author: ole
 */ 

#include "cs3318_driver.h"
#include "twi_master_driver.h"
#include "preferences.h"
#include "board.h"

#include <stdio.h>

extern uint8_t debuglevel;

#define MAX_SLAVES 4
uint8_t cs3318_addr[MAX_SLAVES] = {0x40, 0, 0, 0};

static void cs3318_write(uint8_t chip, uint8_t addr, uint8_t value)
{
    uint8_t data[2] = {addr, value};
    TWI_Master_t * twiMaster = get_TWI_master();
    TWI_MasterWrite(twiMaster, cs3318_addr[chip], data, 2);

    while (twiMaster->status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }
    if (twiMaster->result == TWIM_RESULT_NACK_RECEIVED) {
        printf("Error: I2C NAK received\n");
    }
}

static uint8_t cs3318_read(uint8_t chip, uint8_t addr)
{
    TWI_Master_t * twiMaster = get_TWI_master();
    TWI_MasterWriteRead(twiMaster, cs3318_addr[chip], &addr, 1, 1);

    while (twiMaster->status != TWIM_STATUS_READY) {
        /* Wait until transaction is complete. */
    }
    if (twiMaster->result == TWIM_RESULT_NACK_RECEIVED) {
        printf("Error: I2C NAK received\n");
        return 0;
    }

    DEBUG_PRINT(2, "0x%02X\n", twiMaster->readData[0]);
    return twiMaster->readData[0];
}

q13_2 cs3318_getVolReg(uint8_t chip, uint8_t regaddr)
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

void cs3318_setVolReg(uint8_t chip, uint8_t regaddr, q13_2 volume_in_db_x4)
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

void cs3318_mute(uint8_t channel, bool mute)
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


void cs3318_stepMasterVol(int direction)
{
    if (direction != 0) {
        struct Preferences_t * prefs = get_preferences();
        uint8_t chip = 0; //TODO: loop through chips
        q13_2 volume_in_db_x4 = cs3318_getVolReg(chip, 0x11);
        volume_in_db_x4 += direction * prefs->vol_stepsize;
        if (volume_in_db_x4 <= prefs->vol_max << 2 && volume_in_db_x4 >= prefs->vol_min << 2) {
            DEBUG_PRINT(1, "Set mastervolume to %d: %.2f\n", volume_in_db_x4, Q13_2_TO_FLOAT(volume_in_db_x4));
            cs3318_setVolReg(chip, 0x11, volume_in_db_x4);
        }
    }
}

void cs3318_init(void)
{
    struct Preferences_t * prefs = get_preferences();

    /* Take CS3318 out of reset */
    CS3318_RESET_PORT.DIRSET = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTCLR = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTSET = CS3318_RESET_PIN_bm;
    
    // TODO: Find and init all chips
    // Read ID of first chip at default addr

    /* Set master volume */
    cs3318_setVolReg(0, 0x11, prefs->vol_startup << 2);

    /* Power up cs3318 */
    cs3318_write(0, 0xe, 0);
}
