/*
 * midi_driver.c
 *
 * Created: 14.12.2014 21:44:12
 *  Author: ole
 */ 

#include "usart_driver.h"
#include "board.h"
#include "preferences.h" //q13_2
#include "cs3318_driver.h"
#include "led7seg_driver.h"
#include <stdio.h>

USART_data_t MIDI_data;
enum MIDI_RX_state_t { IDLE, STATUS_RX, DATA1_RX, DATA2_RX};
enum MIDI_RX_state_t MIDI_RX_state;

void MIDI_init(USART_t * usart)
{
    MIDI_PORT.DIRSET   = MIDI_TX_PIN;
    MIDI_PORT.DIRCLR   = MIDI_RX_PIN;
    
    // Use USARTD0 and initialize buffers
    USART_InterruptDriver_Initialize(&MIDI_data, usart, USART_DREINTLVL_MED_gc);
    
    // USARTx0, 8 Data bits, No Parity, 1 Stop bit
    USART_Format_Set(MIDI_data.usart, USART_CHSIZE_8BIT_gc,
    USART_PMODE_DISABLED_gc, false);
    
    // Enable RXC interrupt
    USART_RxdInterruptLevel_Set(MIDI_data.usart, USART_RXCINTLVL_MED_gc);
    
    // Set Baudrate to 31250 bps: (((F_CPU/baudrate) - 1) / 16)
    USART_Baudrate_Set(usart, 126 , -1); // 31250

    /* Enable both RX and TX. */
    USART_Rx_Enable(MIDI_data.usart);
    USART_Tx_Enable(MIDI_data.usart);

    // Enable PMIC interrupt level medium
    PMIC.CTRL |= PMIC_MEDLVLEN_bm;
}

void MIDI_send_mastervol(q13_2 volume_in_db_x4)
{
    USART_TXBuffer_PutByte(&MIDI_data, 0xB0);
    USART_TXBuffer_PutByte(&MIDI_data, 7);
    USART_TXBuffer_PutByte(&MIDI_data, ((volume_in_db_x4 >> 2) + 96) & 0x7f);
    USART_TXBuffer_PutByte(&MIDI_data, 0xB0);
    USART_TXBuffer_PutByte(&MIDI_data, 27);
    USART_TXBuffer_PutByte(&MIDI_data, volume_in_db_x4 & 0x3);
}


static inline bool MIDI_RXComplete(USART_data_t * usart_data)
{
	uint8_t c = usart_data->usart->DATA;
	uint8_t tempRX_Head = (usart_data->buffer.RX_Head + 1) & USART_RX_BUFFER_MASK;

    usart_data->buffer.RX[usart_data->buffer.RX_Head] = c;
    usart_data->buffer.RX_Head = tempRX_Head;

    set_taskflags(Task_MIDI_RX_bm);
    return true;
}


bool MIDI_rx_task(void)
{
    USART_Buffer_t * bufPtr = &MIDI_data.buffer;
    uint8_t c = bufPtr->RX[bufPtr->RX_Tail];
    static uint8_t data[3];

    DEBUG_PRINT(2, "0x%02X\n", c);

    /* Advance buffer tail. */
    bufPtr->RX_Tail = (bufPtr->RX_Tail + 1) & USART_RX_BUFFER_MASK;

    if (c & 0x80) {
        MIDI_RX_state = IDLE;
    }        

    switch (MIDI_RX_state) {
        case STATUS_RX:
            if (!(c & 0x80)) {
                data[1] = c;
                MIDI_RX_state = DATA1_RX;
            }
            break;
        case DATA1_RX:
            if (!(c & 0x80)) {
                data[2] = c;
                DEBUG_PRINT(1, "0x%02X, 0x%02X, 0x%02X\n", data[0], data[1], data[2]);
                if (data[0] == 0xB0 && data[1] == 7) {
                    struct Preferences_t * prefs = get_preferences();
                    q13_2 vol_int = (data[2] - 99) << 2;
                    if (vol_int <= prefs->vol_max << 2 && vol_int >= prefs->vol_min << 2) {
                        for (uint8_t i = 0; i < cs3318_get_nslaves(); i++) {
                            cs3318_setVolReg(i, 0x11, vol_int);
                        }
                        display_volume(vol_int);
                    }                        
                }
                MIDI_RX_state = IDLE;
            }
            break;
        case IDLE:
        default:  
            if (c & 0x80) {
                data[0] = c;
                MIDI_RX_state = STATUS_RX;
            }
            break;
    }
    
    if (bufPtr->RX_Tail == bufPtr->RX_Head)
        return true;
    else
        return false;    
}

//  Receive complete interrupt service routine.
//  Calls the common receive complete handler with pointer to the correct USART
//  as argument.
ISR( MIDI_RXC_vect ) // Note that this vector name is a define mapped to the correct interrupt vector
{                     // See "board.h"
    DEBUGLED_PORT.OUTCLR = DEBUGLED_PIN_bm;
    MIDI_RXComplete(&MIDI_data);
}


//  Data register empty  interrupt service routine.
//  Calls the common data register empty complete handler with pointer to the
//  correct USART as argument.
ISR( MIDI_DRE_vect ) // Note that this vector name is a define mapped to the correct interrupt vector
{                     // See "board.h"
    DEBUGLED_PORT.OUTCLR = DEBUGLED_PIN_bm;
    USART_DataRegEmpty( &MIDI_data );
}
