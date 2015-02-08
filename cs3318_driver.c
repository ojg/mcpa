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

static uint8_t cs3318_addr[MAX_SLAVE_BOARDS] = {0, 0, 0, 0};
static uint8_t cs3318_nslaves = 1;

uint8_t cs3318_get_nslaves() {
    return cs3318_nslaves;
}

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
        return 0; //TODO: Handle return better, 0 may be a correct register value
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
    if (channel == 0) { //master
        DEBUG_PRINT(1, "%s master\n", mute ? "mute" : "unmute");
        if (mute) {
            for (uint8_t i = 0; i < cs3318_nslaves; i++) {
                cs3318_write(i, 0x12, cs3318_read(i, 0x12) | 0x2);
            }
        } else {
            for (uint8_t i = 0; i < cs3318_nslaves; i++) {
                cs3318_write(i, 0x12, cs3318_read(i, 0x12) & ~0x2);
            }
        }
    }
    else {
        uint8_t chip = (channel - 1) >> 3;
        if (chip >= cs3318_nslaves) {
            printf("Invalid channel %d\n", channel);
            return;
        }
        DEBUG_PRINT(1, "%s channel %d\n", mute ? "mute" : "unmute", channel);
        channel = (channel - 1) & 0x7;
        if (mute) {
            cs3318_write(chip, 0x0a, cs3318_read(chip, 0x0a) | 1 << channel);
        } else {
            cs3318_write(chip, 0x0a, cs3318_read(chip, 0x0a) & ~(1 << channel));
        }
    }
}


q13_2 cs3318_stepMasterVol(int direction)
{
    struct Preferences_t * prefs = get_preferences();
    q13_2 volume_in_db_x4 = cs3318_getVolReg(0, 0x11);
    q13_2 new_volume = volume_in_db_x4 + direction * prefs->vol_stepsize;
    if (new_volume <= prefs->vol_max << 2 && new_volume >= prefs->vol_min << 2) {
        DEBUG_PRINT(1, "Set mastervolume to %d: %.2f dB\n", new_volume, Q13_2_TO_FLOAT(new_volume));
        for (uint8_t i = 0; i < cs3318_nslaves; i++) {
            cs3318_setVolReg(i, 0x11, new_volume);
        }
        return new_volume;
    }
    return volume_in_db_x4;
}

void cs3318_init(void)
{
    uint8_t i;
    uint8_t data[2];
    struct Preferences_t * prefs = get_preferences();
    TWI_Master_t * twiMaster = get_TWI_master();

    /* Take CS3318 out of reset */
    CS3318_RESET_PORT.DIRSET = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTCLR = CS3318_RESET_PIN_bm;
    CS3318_RESET_PORT.OUTSET = CS3318_RESET_PIN_bm;
    
    // Find all slave boards
    for (cs3318_nslaves = 0; cs3318_nslaves < MAX_SLAVE_BOARDS; cs3318_nslaves++) {
        // Read 0x1c of chip at default addr
        data[0] = 0x1c;
        TWI_MasterWriteRead(twiMaster, 0x40, data, 1, 1);
        while (twiMaster->status != TWIM_STATUS_READY) {}
        if (twiMaster->result == TWIM_RESULT_NACK_RECEIVED)
            break;
        DEBUG_PRINT(1, "Found CS3318 number %d\n", cs3318_nslaves);

        // If not NAK then write new addr to 0x1b + enout bit
        cs3318_addr[cs3318_nslaves] = 0x40 + cs3318_nslaves + 1;
        data[0] = 0x1b;
        data[1] = (cs3318_addr[cs3318_nslaves] << 1) | 1;
        TWI_MasterWrite(twiMaster, 0x40, data, 2);
        while (twiMaster->status != TWIM_STATUS_READY) {}
        DEBUG_PRINT(1, "Write address %d\n", cs3318_addr[cs3318_nslaves]);
    }

    // Set master volume
    for (i = 0; i < cs3318_nslaves; i++) {
        cs3318_setVolReg(i, 0x11, prefs->vol_startup << 2);
    }

    // Set channel offsets
    for (i = 0; i < cs3318_nslaves; i++) {
        for (uint8_t ch = 0; ch < 8; ch++) {
            cs3318_setVolReg(i, ch + 1, Q5_2_TO_Q13_2(prefs->vol_ch_offset[i * 8 + ch]));
        }
    }

    // Power down channels
    for (i = 0; i < cs3318_nslaves; i++) {
        cs3318_write(i, 0xd, prefs->ch_powerdown[i]);
    }

    // Power up cs3318
    for (i = 0; i < cs3318_nslaves; i++) {
        cs3318_write(i, 0xe, 0);
    }
}
