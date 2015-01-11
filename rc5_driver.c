/*
 * rc5_driver.c
 *
 * Created: 10.01.2015 18:19:19
 *  Author: ole
 */ 

#include "board.h"
#include <stdio.h>
#include <avr/io.h>

void rc5_init(void)
{
    /* Init pin change irq */
    IR_PORT.DIRCLR = IR_PIN_bm | IR_PIN_bm;
    IR_PORT.PIN4CTRL = PORT_OPC_PULLUP_gc;
    IR_PORT.INTMASK = IR_PIN_bm;
    IR_PORT.INTCTRL = PORT_INTLVL_MED_gc;
    PMIC.CTRL |= PMIC_MEDLVLEN_bm;
}

void rc5_irqhandler(void)
{
    if (IR_PORT.INTFLAGS & IR_PIN_bm) {
        IR_PORT.INTFLAGS |= IR_PIN_bm; //reset irq flag
        DEBUG_PRINT(2, IR_PORT.IN & IR_PIN_bm ? "1 " : "0 ");
        //set_taskflags(Task_RC5_bm);
    }                   
}