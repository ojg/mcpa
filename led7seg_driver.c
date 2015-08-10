/*
 * led7seg_driver.c
 *
 * Created: 03.08.2015 21:24:32
 *  Author: ole
 */ 

#include "led7seg_driver.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

static const uint8_t number_to_digit[10] = {
    1+2+4+8+16+32,  //0
    2+4,            //1
    1+2+8+16+64,    //2
    1+2+4+8+64,     //3
    2+4+32+64,      //4
    1+4+8+32+64,    //5
    1+4+8+16+32+64, //6
    1+2+4,          //7
    1+2+4+8+16+32+64,//8
    1+2+4+8+32+64,  //9
};
static uint8_t led_digits[4] = {0, 0, 0, 0};
static uint8_t led_count = 0;
static uint32_t screensave_count = 0;
static const uint32_t screensave_timeout = 800000;

void display_init(q13_2 volume_startup)
{
    PORTA.DIRSET = 0xFF;
    PORTC.DIRSET = 0xF0;
    PORTA.OUT = 0xFF;
    PORTC.OUTSET = 0x00;
    TCC5.CNT = 0;
    TCC5.CCA = 50; //~20kHz refresh rate
    TCC5.CTRLA |= TC45_CLKSEL_DIV8_gc; //4MHz
    PMIC.CTRL |= PMIC_HILVLEN_bm;
    display_volume(volume_startup);
}

void display_volume(q13_2 volume_x4)
{
    char str[6];

    sprintf(str, "%05.1f", Q13_2_TO_FLOAT(volume_x4));
    DEBUG_PRINT(2, "led-display string: %s\n", str);

    led_digits[0] = (volume_x4 < 0) ? 64 : 0;

    led_digits[1] = (str[1] == '0') ? 0 : number_to_digit[str[1] - 48];

    led_digits[2] = number_to_digit[str[2] - 48] + 128;

    led_digits[3] = number_to_digit[str[4] - 48];

    TCC5.INTCTRLB |= TC45_CCAINTLVL_HI_gc; //turn on irq
    screensave_count = 0;
}

void display_mute(void)
{
    TCC5.INTCTRLB &= ~TC45_CCAINTLVL_HI_gc; //turn off irq
    PORTC.OUTCLR = 0xF0;
    PORTA.OUT = 64;
    PORTC.OUTSET = 0x80;
}

ISR(TCC5_CCA_vect)
{
    if (++led_count > 3) {
        led_count = 0;
    }
    PORTC.OUTCLR = 0xF0;

    if (++screensave_count == screensave_timeout) {
        TCC5.INTCTRLB &= ~TC45_CCAINTLVL_HI_gc; //turn off irq
        PORTA.OUT = 128;
        PORTC.OUT = 0x40;
    }
    else {
        PORTA.OUT = led_digits[led_count];
        PORTC.OUTSET = 0x10 << led_count;
        TCC5.CNT = 0;
    }
}
