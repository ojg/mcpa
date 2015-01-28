/*
 * rc5_driver.c
 *
 * Created: 10.01.2015 18:19:19
 *  Author: ole
 */ 

#include "board.h"
#include <stdio.h>
#include <avr/io.h>
#include <stdbool.h>

void rc5_init(void)
{
    /* Init pin change irq */
    IR_PORT.DIRCLR = IR_PIN_bm;
    IR_PORT.IR_PINCTRL = PORT_OPC_PULLUP_gc;
    IR_PORT.INTMASK |= IR_PIN_bm;
    IR_PORT.INTCTRL = PORT_INTLVL_MED_gc;
    PMIC.CTRL |= PMIC_MEDLVLEN_bm;
}

uint8_t rc5_state = 0;
uint32_t rc5_code;

void rc5_irqhandler(void)
{
    if (IR_PORT.INTFLAGS & IR_PIN_bm) {
        IR_PORT.INTFLAGS |= IR_PIN_bm; //reset irq flag
        bool pinval = IR_PORT.IN & IR_PIN_bm;

        if (rc5_state == 0) { //was idle
            if (!pinval) {
                // Start timer as freerunning
                TCC4.CNT = 0;
                TCC4.CTRLA |= TC45_CLKSEL_DIV64_gc; //500kHz
                rc5_state = 1;
                rc5_code = 0xFFFFFFFF;
                DEBUG_PRINT(2, "start\n");
            }
        }
        else {
            uint16_t count = TCC4.CNT;
            TCC4.CNT = 0;
            if (count > 400 && count < 490) { //valid short period
                rc5_state++;
                if (pinval)
                    rc5_code = (rc5_code << 1) | 1;
                else
                    rc5_code <<= 1;
                //DEBUG_PRINT(2, "s %u %lu\n", rc5_state, rc5_code);
            }
            else if (count > 840 && count < 940) { // valid long period
                rc5_state += 2;
                if (pinval)
                    rc5_code = (rc5_code << 2) | 3;
                else
                    rc5_code <<= 2;
                //DEBUG_PRINT(2, "l %u %lu\n", rc5_state, rc5_code);
            }
            else { //invalid period value
                //DEBUG_PRINT(2, "restart\n");
                rc5_state = 1;
                rc5_code = 0xFFFFFFFF;
                TCC4.CNT = 0;
                TCC4.CTRLA |= TC45_CLKSEL_DIV64_gc; //500kHz
            }
        }

        if (rc5_state >= 27) {
            //DEBUG_PRINT(2, "%lu\n", rc5_code);
            rc5_state = 0;
            TCC4.CTRLA = 0;
            set_taskflags(Task_IR_RX_bm);
        }
    }
}

uint16_t get_rc5_code(void)
{
    uint16_t temp=0;
    //printf("MC: 0x%lX\n", rc5_code);
    for (int i=0; i<14; i++) {
        if (rc5_code & 1)
            temp |= 0x4000;
        temp >>= 1;
        rc5_code >>= 2;
    }
    return ~temp;
}