/*
 * midi_driver.h
 *
 * Created: 14.12.2014 21:46:28
 *  Author: ole
 */ 


#ifndef MIDI_DRIVER_H_
#define MIDI_DRIVER_H_

#include "preferences.h"

void MIDI_init(USART_t * usart);
void MIDI_send_mastervol(q13_2 volume_in_db_x4);

#endif /* MIDI_DRIVER_H_ */