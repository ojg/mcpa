/* This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file *********************************************************************
 *
 * \brief
 *      XMEGA USART driver source file.
 *
 *      This file contains the function implementations the XMEGA interrupt
 *      and polled USART driver.
 *
 *      The driver is not intended for size and/or speed critical code, since
 *      most functions are just a few lines of code, and the function call
 *      overhead would decrease code performance. The driver is intended for
 *      rapid prototyping and documentation purposes for getting started with
 *      the XMEGA ADC module.
 *
 *      For size and/or speed critical code, it is recommended to copy the
 *      function contents directly into your application instead of making
 *      a function call.
 *
 *      Some functions use the following construct:
 *          "some_register = ... | (some_parameter ? SOME_BIT_bm : 0) | ..."
 *      Although the use of the ternary operator ( if ? then : else ) is discouraged,
 *      in some occasions the operator makes it possible to write pretty clean and
 *      neat code. In this driver, the construct is used to set or not set a
 *      configuration bit based on a boolean input parameter, such as
 *      the "some_parameter" in the example above.
 *
 * \par Application note:
 *      AVR1307: Using the XMEGA USART
 *
 * \par Documentation
 *      For comprehensive code documentation, supported compilers, compiler
 *      settings and supported devices see readme.html
 *
 * \author
 *      Atmel Corporation: http://www.atmel.com \n
 *      Support email: avr@atmel.com
 *
 * $Revision: 481 $
 * $Date: 2007-03-06 10:12:53 +0100 (ty, 06 mar 2007) $  \n
 *
 * Copyright (c) 2008, Atmel Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of ATMEL may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
 * SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include "usart_driver.h"
#include "board.h"
#include <stdio.h>

static int uart_getchar(FILE *stream);
static int uart_putchar(char c, FILE *stream);

USART_data_t USART_data;
FILE usart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);

extern Task_flag_t taskflags;

/*! \brief Initializes buffer and selects what USART module to use.
 *
 *  Initializes receive and transmit buffer and selects what USART module to use,
 *  and stores the data register empty interrupt level.
 *
 *  \param usart_data           The USART_data_t struct instance.
 *  \param usart                The USART module.
 *  \param dreIntLevel          Data register empty interrupt level.
 */
static void USART_InterruptDriver_Initialize(USART_data_t * usart_data,
                                      USART_t * usart,
                                      USART_DREINTLVL_t dreIntLevel)
{
	usart_data->usart = usart;
	usart_data->dreIntLevel = dreIntLevel;

	usart_data->buffer.RX_Tail = 0;
	usart_data->buffer.RX_Head = 0;
	usart_data->buffer.TX_Tail = 0;
	usart_data->buffer.TX_Head = 0;
}


/*! \brief Set USART DRE interrupt level.
 *
 *  Set the interrupt level on Data Register interrupt.
 *
 *  \note Changing the DRE interrupt level in the interrupt driver while it is
 *        running will not change the DRE interrupt level in the USART before the
 *        DRE interrupt have been disabled and enabled again.
 *
 *  \param usart_data         The USART_data_t struct instance
 *  \param dreIntLevel        Interrupt level of the DRE interrupt.
 */
/*
static void USART_InterruptDriver_DreInterruptLevel_Set(USART_data_t * usart_data,
                                                 USART_DREINTLVL_t dreIntLevel)
{
	usart_data->dreIntLevel = dreIntLevel;
}
*/

/*! \brief Test if there is data in the transmitter software buffer.
 *
 *  This function can be used to test if there is free space in the transmitter
 *  software buffer.
 *
 *  \param usart_data The USART_data_t struct instance.
 *
 *  \retval true      There is data in the receive buffer.
 *  \retval false     The receive buffer is empty.
 */
static bool USART_TXBuffer_FreeSpace(USART_data_t * usart_data)
{
	/* Make copies to make sure that volatile access is specified. */
	uint8_t tempHead = (usart_data->buffer.TX_Head + 1) & USART_TX_BUFFER_MASK;
	uint8_t tempTail = usart_data->buffer.TX_Tail;

	/* There are data left in the buffer unless Head and Tail are equal. */
	return (tempHead != tempTail);
}



/*! \brief Put data (5-8 bit character).
 *
 *  Stores data byte in TX software buffer and enables DRE interrupt if there
 *  is free space in the TX software buffer.
 *
 *  \param usart_data The USART_data_t struct instance.
 *  \param data       The data to send.
 */
static bool USART_TXBuffer_PutByte(USART_data_t * usart_data, uint8_t data)
{
	uint8_t tempCTRLA;
	uint8_t tempTX_Head;
	bool TXBuffer_FreeSpace;
	USART_Buffer_t * TXbufPtr = &usart_data->buffer;

	while (!USART_TXBuffer_FreeSpace(usart_data)) {} //block on wait for free space

	tempTX_Head = TXbufPtr->TX_Head;
	TXbufPtr->TX[tempTX_Head]= data;
	/* Advance buffer head. */
	TXbufPtr->TX_Head = (tempTX_Head + 1) & USART_TX_BUFFER_MASK;

	/* Enable DRE interrupt. */
	tempCTRLA = usart_data->usart->CTRLA;
	tempCTRLA = (tempCTRLA & ~USART_DREINTLVL_gm) | usart_data->dreIntLevel;
	usart_data->usart->CTRLA = tempCTRLA;

	return TXBuffer_FreeSpace;
}



/*! \brief Test if there is data in the receive software buffer.
 *
 *  This function can be used to test if there is data in the receive software
 *  buffer.
 *
 *  \param usart_data         The USART_data_t struct instance
 *
 *  \retval true      There is data in the receive buffer.
 *  \retval false     The receive buffer is empty.
 */
static inline bool USART_RXBufferData_Available(USART_data_t * usart_data)
{
	/* Make copies to make sure that volatile access is specified. */
	uint8_t tempHead = usart_data->buffer.RX_Head;
	uint8_t tempTail = usart_data->buffer.RX_Tail;

	/* There are data left in the buffer unless Head and Tail are equal. */
	return (tempHead != tempTail);
}



/*! \brief Get received data (5-8 bit character).
 *
 *  The function USART_RXBufferData_Available should be used before this
 *  function is used to ensure that data is available.
 *
 *  Returns data from RX software buffer.
 *
 *  \param usart_data       The USART_data_t struct instance.
 *
 *  \return         Received data.
 */
static inline uint8_t USART_RXBuffer_GetByte(USART_data_t * usart_data)
{
	USART_Buffer_t * bufPtr;
	uint8_t ans;

	bufPtr = &usart_data->buffer;
	ans = (bufPtr->RX[bufPtr->RX_Tail]);

	/* Advance buffer tail. */
	bufPtr->RX_Tail = (bufPtr->RX_Tail + 1) & USART_RX_BUFFER_MASK;

	return ans;
}



/*! \brief RX Complete Interrupt Service Routine.
 *
 *  RX Complete Interrupt Service Routine.
 *  Stores received data in RX software buffer.
 *
 *  \param usart_data      The USART_data_t struct instance.
 */
static inline bool USART_RXComplete(USART_data_t * usart_data)
{
	uint8_t c = usart_data->usart->DATA;
	uint8_t tempRX_Head = (usart_data->buffer.RX_Head + 1) & USART_RX_BUFFER_MASK;

	if (c == '\r') { //replace cr with newline
		c = '\n';
	}

	if (c == '\b' || c == '\x7f') { //backspace or delete
		if (usart_data->buffer.RX_Head == usart_data->buffer.RX_Tail) { // empty buffer
			USART_TXBuffer_PutByte(usart_data, '\a');
		}
		else {
			USART_TXBuffer_PutByte(usart_data, c); // remove char from buffer
			usart_data->buffer.RX[usart_data->buffer.RX_Head] = 0;
			usart_data->buffer.RX_Head = (usart_data->buffer.RX_Head - 1) & USART_RX_BUFFER_MASK;
		}
	}
	else if (tempRX_Head == usart_data->buffer.RX_Tail) { // full buffer
		USART_TXBuffer_PutByte(usart_data, '\a');
	}
	else {
		USART_TXBuffer_PutByte(usart_data, c); // normal case, put char in buffer
		usart_data->buffer.RX[usart_data->buffer.RX_Head] = c;
		usart_data->buffer.RX_Head = tempRX_Head;
	}

	if (c == '\n' && tempRX_Head != usart_data->buffer.RX_Tail) { // newline: flag cli task
		USART_TXBuffer_PutByte(usart_data, '\r');
		taskflags |= Task_CLI_bm;
	}
	return true;
}



/*! \brief Data Register Empty Interrupt Service Routine.
 *
 *  Data Register Empty Interrupt Service Routine.
 *  Transmits one byte from TX software buffer. Disables DRE interrupt if buffer
 *  is empty. Argument is pointer to USART (USART_data_t).
 *
 *  \param usart_data      The USART_data_t struct instance.
 */
static inline void USART_DataRegEmpty(USART_data_t * usart_data)
{
	USART_Buffer_t * bufPtr;
	bufPtr = &usart_data->buffer;

	/* Check if all data is transmitted. */
	uint8_t tempTX_Tail = usart_data->buffer.TX_Tail;
	if (bufPtr->TX_Head == tempTX_Tail){
	    /* Disable DRE interrupts. */
		uint8_t tempCTRLA = usart_data->usart->CTRLA;
		tempCTRLA = (tempCTRLA & ~USART_DREINTLVL_gm) | USART_DREINTLVL_OFF_gc;
		usart_data->usart->CTRLA = tempCTRLA;

	}else{
		/* Start transmitting. */
		uint8_t data = bufPtr->TX[usart_data->buffer.TX_Tail];
		usart_data->usart->DATA = data;

		/* Advance buffer tail. */
		bufPtr->TX_Tail = (bufPtr->TX_Tail + 1) & USART_TX_BUFFER_MASK;
	}
}


/*! \brief Put data (9 bit character).
 *
 *  Use the function USART_IsTXDataRegisterEmpty before using this function to
 *  put 9 bit character to the TX register.
 *
 *  \param usart      The USART module.
 *  \param data       The data to send.
 */
/*
static void USART_NineBits_PutChar(USART_t * usart, uint16_t data)
{
	if(data & 0x0100) {
		usart->CTRLB |= USART_TXB8_bm;
	}else {
		usart->CTRLB &= ~USART_TXB8_bm;
	}

	usart->DATA = (data & 0x00FF);
}
*/

/*! \brief Get received data (9 bit character).
 *
 *  This function reads out the received 9 bit character (uint16_t).
 *  Use the function USART_IsRXComplete to check if anything is received.
 *
 *  \param usart      The USART module.
 *
 *  \retval           Received data.
 */
/*
static uint16_t USART_NineBits_GetChar(USART_t * usart)
{
	if(usart->CTRLB & USART_RXB8_bm) {
		return(0x0100 | usart->DATA);
	}else {
		return(usart->DATA);
	}
}
*/

static int uart_putchar (char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	
	USART_TXBuffer_PutByte(&USART_data, c);
	
	return 0;
}


static int uart_getchar(FILE *stream)
{
	uint8_t data;
	(void)stream;

	while (!USART_RXBufferData_Available(&USART_data)) {};

	data = USART_RXBuffer_GetByte(&USART_data);

	return data;
}


void USART_init(USART_t * usart)
{
	USART_PORT.DIRSET   = USART_TX_PIN;
	USART_PORT.DIRCLR   = USART_RX_PIN;
	
	// Use USARTD0 and initialize buffers
	USART_InterruptDriver_Initialize(&USART_data, usart, USART_DREINTLVL_LO_gc);
	
	// USARTx0, 8 Data bits, No Parity, 1 Stop bit
	USART_Format_Set(USART_data.usart, USART_CHSIZE_8BIT_gc,
	USART_PMODE_DISABLED_gc, false);
	
	// Enable RXC interrupt
	USART_RxdInterruptLevel_Set(USART_data.usart, USART_RXCINTLVL_LO_gc);
	
	// Set Baudrate to 115200 bps: (((F_CPU/baudrate) - 1) / 16)
	USART_Baudrate_Set(usart, 17 , 0);

	/* Enable both RX and TX. */
	USART_Rx_Enable(USART_data.usart);
	USART_Tx_Enable(USART_data.usart);

	// Enable PMIC interrupt level low
	PMIC.CTRL |= PMIC_LOLVLEX_bm;
	
	stdout = stdin = &usart_stream;
}


//  Receive complete interrupt service routine.
//  Calls the common receive complete handler with pointer to the correct USART
//  as argument.
ISR( USART_RXC_vect ) // Note that this vector name is a define mapped to the correct interrupt vector
{                     // See "board.h"
	USART_RXComplete( &USART_data );
}


//  Data register empty  interrupt service routine.
//  Calls the common data register empty complete handler with pointer to the
//  correct USART as argument.
ISR( USART_DRE_vect ) // Note that this vector name is a define mapped to the correct interrupt vector
{                     // See "board.h"
	USART_DataRegEmpty( &USART_data );
}
