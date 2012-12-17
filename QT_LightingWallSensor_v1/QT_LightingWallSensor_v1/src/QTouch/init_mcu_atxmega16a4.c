/*******************************************************************************
*   $FILE:  main.c
*   Atmel Corporation:  http://www.atmel.com \n
*   Support email:  touch@atmel.com
******************************************************************************/

/*  License
*   Copyright (c) 2010, Atmel Corporation All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without
*   modification, are permitted provided that the following conditions are met:
*
*   1. Redistributions of source code must retain the above copyright notice,
*   this list of conditions and the following disclaimer.
*
*   2. Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
*   3. The name of ATMEL may not be used to endorse or promote products derived
*   from this software without specific prior written permission.
*
*   THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
*   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
*   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
*   SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
*   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
*   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
*   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*----------------------------------------------------------------------------
                            compiler information
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                                include files
----------------------------------------------------------------------------*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include "touch.h"
#include "touch_api.h"

#include "xmega_usart.h"
#include "board_config_TouchPanel.h"
/*----------------------------------------------------------------------------
                            manifest constants
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
                            type definitions
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                                prototypes
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                            Structure Declarations
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                                    macros
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                                global variables
----------------------------------------------------------------------------*/
/* Timer period in msec. */
extern uint16_t qt_measurement_period_msec;

/*----------------------------------------------------------------------------
                                extern variables
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
                                static variables
----------------------------------------------------------------------------*/

/* flag set by timer ISR when it's time to measure touch */
extern volatile uint8_t time_to_measure_touch;

/* current time, set by timer ISR */
extern volatile uint16_t current_time_ms_touch;

#if defined(__ATxmega16A4__)
/*============================================================================
Name    :   init_timer_isr
------------------------------------------------------------------------------
Purpose :   configure timer ISR to fire regularly
============================================================================*/

void init_timer_isr( void )
{
    /*  Set timer period    */
   TCC0.PER = TICKS_PER_MS * qt_measurement_period_msec;
   /*  select clock source */
   TCC0.CTRLA = (TOUCH_DATA_T)4;
   /*  Set Compare A interrupt to low level   */
   TCC0.INTCTRLB = 1u;
   /*  enable low lever interrupts in power manager interrupt control  */
   PMIC.CTRL |= 1u;
}
/*============================================================================
Name    :   set_timer_period
------------------------------------------------------------------------------
Purpose :   changed the timer period runtime
Input   :   qt_measurement_period_msec
Output  :   n/a
Notes   :
============================================================================*/
void set_timer_period(uint16_t qt_measurement_period_msec)
{
	/*  set timer compare value (how often timer ISR will fire,set to 1 ms interrupt) */
  TCC0.PER = TICKS_PER_MS * qt_measurement_period_msec;
}

/*============================================================================
Name    :   CCP write helper function written in assembly.
------------------------------------------------------------------------------
Purpose :   This function is written in assembly because of the time critical
operation of writing to the registers for xmega.
Input   :   address - A pointer to the address to write to.
value   - The value to put in to the register.
============================================================================*/
void CCPWrite( volatile uint8_t * address, uint8_t value )
{
   volatile uint8_t * tmpAddr = address;
#ifdef RAMPZ
   RAMPZ = 0;
#endif
   asm volatile(
                "movw r30,  %0"	"\n\t"
                "ldi  r16,  %2"	"\n\t"
                "out   %3, r16"	"\n\t"
                "st     Z,  %1"
                :
                : "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "m" (CCP)
                : "r16", "r30", "r31"
               );
}

/*============================================================================
Name    :   init_system
------------------------------------------------------------------------------
Purpose :   initialize host app, pins, watchdog, etc
============================================================================*/
void init_system( void )
{
    uint8_t PSconfig;
    uint8_t clkCtrl;

   /*  Configure Oscillator and Clock source   */

   /*  Select Prescaler A divider as 4 and Prescaler B & C divider as (1,1) respectively.  */
   /*  Overall divide by 4 i.e. A*B*C  */
   PSconfig = (uint8_t) CLK_PSADIV_4_gc | CLK_PSBCDIV_1_1_gc;
   /*  Enable internal 32 MHz ring oscillator. */
   OSC.CTRL |= OSC_RC32MEN_bm;
   CCPWrite( &CLK.PSCTRL, PSconfig );
   /*  Wait until oscillator is ready. */
   while ( ( OSC.STATUS & OSC_RC32MRDY_bm ) == 0 );
   /*  Set the 32 MHz ring oscillator as the main clock source */
   clkCtrl = ( CLK.CTRL & ~CLK_SCLKSEL_gm ) | CLK_SCLKSEL_RC32M_gc;
   CCPWrite( &CLK.CTRL, clkCtrl );

   /*  Route clk signal to port pin    */
   /*  PORTCFG_CLKEVOUT = 0x03;    */
   /*  PORTE_DIRSET = 0x80;    */

	/************************************************************************/
	/* Self address configuration with dip switch on PORTB[0..3]            */
	/************************************************************************/

	/* Configure PortB as inputs with internal pull-ups.
	   The internal pull-up will drive the line high	*/
	PORTB.DIRCLR = (PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm);	// all 4 pins of port B	as inputs
	
	/* Writing any of the PINnCTRL registers will update only the PINnCTRL
	registers matching the mask in the MPCMASK register	for that port. */
	PORTCFG.MPCMASK = (PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm);	// all 4 pins of port B
	PORTB.PIN0CTRL = (PORTB.PIN0CTRL & ~PORT_OPC_gm) | PORT_OPC_PULLUP_gc;	// pull-ups on
	
	/************************************************************************/
	/* RS-485 Transceiver configuration                                     */
	/************************************************************************/
	
	/* +----------------------------------------------+
	   | RS-485 control pin |        Function		  |
	   |--------------------|-------------------------|
	   | LOW				| Receiver output enabled |
	   |--------------------|-------------------------|
	   | HIGH				| Driver output enabled   |
	   +----------------------------------------------+ */
	
	/* Initially go to listening mode: enable receiver */
	RS485_DRIVER_PORT.DIRSET = RS584_DRIVER_CTRL_bm;	// pin as output
	RS485_DRIVER_PORT.OUTCLR = RS584_DRIVER_CTRL_bm;	// drive pin low
	
	/* USARTD0 used for RS-485 transmission */
	/* Set USART transmission 19200 baud rate @32MHz CPU */
	/* BSCALE = -7		*/
	/* CLK2X = 0		*/
	/* BSEL = 13205		*/
	/* Error = 0,00%	*/
		
	/* USART initialization should use the following sequence:
		1. Set the TxD pin value high, and optionally set the XCK pin low.
		2. Set the TxD and optionally the XCK pin as output.
		3. Set the baud rate and frame format.
		4. Set the mode of operation (enables XCK pin output in synchronous mode).
		5. Enable the transmitter or the receiver, depending on the usage.
	For interrupt-driven USART operation, global interrupts should be disabled during the
	initialization. */	
		
	/* PD3 (TXD) as output - high */
	PORTD.DIRSET = PIN3_bm;
	PORTD.OUTSET = PIN3_bm;
	
	/* PD2 (RXD) as input */
	PORTD.DIRCLR = PIN2_bm;
	
	/* Enable system clock to peripheral */
	// Should be enabled after restart - USARTD0
	PR.PRPD &= ~PR_USART0_bm;
	
	/* Set the baud rate: use BSCALE and BSEL */
	xmega_usart_baudrate(USART_RS485, USART_RS485_BSEL_19200, USART_RS485_BSCALE_19200);	// 19200bps
	
	/* Set frame format */
	xmega_usart_format_set(USART_RS485, USART_RS485_CHAR_SIZE, USART_RS485_PARITY, USART_RS485_STOP_BIT);

	/* Set communication mode */
	xmega_usart_set_mode(USART_RS485, USART_RS485_CMODE);
	
	/* Set interrupts level */
	xmega_usart_set_rx_interrupt_level(USART_RS485, USART_RXCINTLVL_LO_gc);
	xmega_usart_set_tx_interrupt_level(USART_RS485, USART_TXCINTLVL_LO_gc);
	
	/* Enable transmitter and receiver */
	xmega_usart_tx_enable(USART_RS485);
	xmega_usart_rx_enable(USART_RS485);

	/* USARTE0 used for terminal */
		
	/* PE3 (TXD) as output - high */
	PORTE.DIRSET = PIN3_bm;
	PORTE.OUTSET = PIN3_bm;
	
	/* PE2 (RXD) as input */
	PORTE.DIRCLR = PIN2_bm;
	
	/* Enable system clock to peripheral */
	// Should be enabled after restart - USARTD0
	PR.PRPE &= ~PR_USART0_bm;
	
	/* Set the baud rate: use BSCALE and BSEL */
	xmega_usart_baudrate(USART_TERMINAL, USART_TERMINAL_BSEL_19200, USART_TERMINAL_BSCALE_19200);	// 19200bps
	
	/* Set frame format */
	xmega_usart_format_set(USART_TERMINAL, USART_TERMINAL_CHAR_SIZE, USART_TERMINAL_PARITY, USART_TERMINAL_STOP_BIT);

	/* Set communication mode */
	xmega_usart_set_mode(USART_TERMINAL, USART_TERMINAL_CMODE);
	
	/* Set interrupts level */
	xmega_usart_set_rx_interrupt_level(USART_TERMINAL, USART_RXCINTLVL_LO_gc);
	xmega_usart_set_tx_interrupt_level(USART_TERMINAL, USART_TXCINTLVL_LO_gc);
	
	/* Enable transmitter and receiver */
	xmega_usart_tx_enable(USART_TERMINAL);
	xmega_usart_rx_enable(USART_TERMINAL);
}

ISR(TCC0_CCA_vect)
{
    /*  set flag: it's time to measure touch    */
    time_to_measure_touch = 1u;

    /*  update the current time  */
    current_time_ms_touch += qt_measurement_period_msec;
}
#endif





