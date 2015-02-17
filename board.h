/*
 * board.h
 *
 * Created: 06.11.2014 22:40:38
 *  Author: ole
 */ 


#ifndef BOARD_H_
#define BOARD_H_

#include <inttypes.h>

#define LED_CHAR_PORT PORTA
#define LED_SEL_PORT PORTC

#define TWI_SDA_PIN_bm PIN0_bm
#define TWI_SCL_PIN_bm PIN1_bm
#define TWI_PORT PORTC

#define MIDI_RX_PIN PIN2_bm
#define MIDI_TX_PIN PIN3_bm
#define MIDI_PORT PORTC
#define MIDI_RXC_vect USARTC0_RXC_vect
#define MIDI_DRE_vect USARTC0_DRE_vect

#define ROT_A_PIN_bm PIN0_bm
#define ROT_B_PIN_bm PIN1_bm
#define ROT_PORT PORTD

#define USART_RX_PIN PIN2_bm
#define USART_TX_PIN PIN3_bm
#define USART_PORT PORTD
#define USART_RXC_vect USARTD0_RXC_vect
#define USART_DRE_vect USARTD0_DRE_vect

#define IR_PIN_bm PIN4_bm
#define IR_PORT PORTD
#define IR_PINCTRL PIN4CTRL

#define DEBUGLED_PIN_bm PIN5_bm
#define DEBUGLED_PORT PORTD

#define MUTE_PIN_bm PIN6_bm
#define MUTE_PORT PORTD

#define CS3318_RESET_PIN_bm PIN7_bm
#define CS3318_RESET_PORT PORTD

typedef volatile uint8_t Task_flag_t;
#define Task_CLI_bm 1
#define Task_Rotary_bm 2
#define Task_MIDI_RX_bm 4
#define Task_IR_RX_bm 8
void set_taskflags(Task_flag_t bitmask);

uint8_t get_debuglevel();
#define DEBUG_PRINT(level, format, ...) if (get_debuglevel() >= level) printf(format, ##__VA_ARGS__)

#define MAX_SLAVE_BOARDS 4

#endif /* INCFILE1_H_ */