/*
 * led7seg_driver.h
 *
 * Created: 03.08.2015 21:24:50
 *  Author: ole
 */ 


#ifndef LED7SEG_DRIVER_H_
#define LED7SEG_DRIVER_H_

#include "preferences.h" //q13_2

void display_init(q13_2 volume_startup);
void display_volume(q13_2 volume_x4);
void display_mute(void);


#endif /* LED7SEG_DRIVER_H_ */