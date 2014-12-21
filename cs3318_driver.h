/*
 * cs3318_driver.h
 *
 * Created: 02.12.2014 14:05:56
 *  Author: ole
 */ 


#ifndef CS3318_DRIVER_H_
#define CS3318_DRIVER_H_

#include "preferences.h" //for q13_2
#include <inttypes.h>
#include <stdbool.h>

uint8_t cs3318_get_nslaves(void);
void cs3318_init(void);
q13_2 cs3318_stepMasterVol(int direction);
void cs3318_mute(uint8_t channel, bool mute);
void cs3318_setVolReg(uint8_t chip, uint8_t regaddr, q13_2 volume_in_db_x4);
q13_2 cs3318_getVolReg(uint8_t chip, uint8_t regaddr);

#endif /* CS3318_DRIVER_H_ */