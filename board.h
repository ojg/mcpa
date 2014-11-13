/*
 * board.h
 *
 * Created: 06.11.2014 22:40:38
 *  Author: ole
 */ 


#ifndef INCFILE1_H_
#define INCFILE1_H_

#include <inttypes.h>

#define ROT_A_PIN_bm PIN0_bm
#define ROT_B_PIN_bm PIN1_bm
#define ROT_PORT PORTD

#define LED_PIN_bm PIN5_bm
#define LED_PORT PORTD

#define CS3318_RESET_PIN_bm PIN7_bm
#define CS3318_RESET_PORT PORTD

#define USART_TX_PIN PIN3_bm
#define USART_RX_PIN PIN2_bm
#define USART_PORT PORTD
#define USART_RXC_vect USARTD0_RXC_vect
#define USART_DRE_vect USARTD0_DRE_vect

#define TWI_SDA_PIN_bm PIN0_bm
#define TWI_SCL_PIN_bm PIN1_bm
#define TWI_PORT PORTC

typedef volatile uint8_t Task_flag_t;
#define Task_CLI_bm 1
#define Task_Rotary_bm 2


#endif /* INCFILE1_H_ */