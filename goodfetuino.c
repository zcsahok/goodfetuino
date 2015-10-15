/* GoodFETuino - MSP430 GoodFET client for Arduino
 * Copyright (C) 2015 Zoltán Csahók
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#include <util/delay.h>

#include <string.h>

#define BAUD 115200

typedef unsigned char byte;

#include "uart.h"

#include "jtag430x2.h"


//=== GoodFET interface functions
#include "command.h"            // for CMDDATALEN
unsigned char cmddata[CMDDATALEN];
static byte led_state;
volatile static byte led_timer;
void led_off()
{
    PORTB &= ~(1 << PB5);       // LED off
    led_state = 0;
    led_timer = 0;
}

void led_toggle()
{
    //PORTB ^= (1 << PB5);
    if (led_state == 0) {
        PORTB |= (1 << PB5);    // LED on
        led_state = 1;
        led_timer = 5;
    }
}

void serial_tx(unsigned char c)
{
    uart_putc(c);
}

void delay(unsigned int count)
{
    while (count--) {
        _delay_us(1);
        //_delay_loop_1(1);
    }
}

void msdelay(unsigned int ms)
{
    while (ms--) {
        _delay_ms(1);
    }
}

void txword(unsigned int l)
{
    serial_tx(l & 0xFF);        // lo byte
    l >>= 8;
    serial_tx(l & 0xFF);        // hi byte
}

void txhead(unsigned char app, unsigned char verb, unsigned long len)
{
    uart_putc(app);
    uart_putc(verb);
    txword(len);
}

void txdata(unsigned char app, unsigned char verb, unsigned long len)
{
    unsigned int i = 0;
//  if(silent) return;
    txhead(app, verb, len);
    for (i = 0; i < len; i++) {
        serial_tx(cmddata[i]);
    }
}

void txstring(unsigned char app, unsigned char verb, const char *str)
{
    unsigned long len = strlen(str);
    txhead(app, verb, len);
    while (len--)
        uart_putc(*(str++));
}

void debugstr(const char *str)
{
    txstring(0xFF, 0xFF, str);
}

char nibbletohex(byte x)
{
    x &= 0x0f;
    return x < 10 ? '0' + x : 'A' - 10 + x;
}

void debughex(u16 v)
{
    char buffer[5];
    buffer[0] = nibbletohex(v >> 12);
    buffer[1] = nibbletohex(v >> 8);
    buffer[2] = nibbletohex(v >> 4);
    buffer[3] = nibbletohex(v);
    buffer[4] = 0;
    debugstr(buffer);
}

#include "jtag.h"               // for SETTCLK/CLRTCLK
void jtag430_tclk_flashpulses(int count)
{
    // send pulses at ~370 kHz @ 16 MHz
    // should be 350 +/- 100 kHz
    cli();
    while (count--) {
        SETTCLK;
        _delay_us(1);
        CLRTCLK;
        _delay_us(1);
    }
    sei();
}

uint16_t ntohs(uint16_t netshort)
{
    return netshort;
}

uint16_t htons(uint16_t hostshort)
{
    return hostshort;
}

uint32_t htonl(uint32_t hostlong)
{
    return hostlong;
}

//===

volatile byte uart_timer;

unsigned int uart_rx()
{
    uart_timer = 25;
    while (uart_timer) {
        unsigned int result = uart_getc();
        if ((result & 0xff00) == 0) {
            return result;
        }
    }
    return 0xffff;
}

// =============================================================

// slow timer
ISR(TIMER2_OVF_vect)
{
    if (uart_timer) {
        --uart_timer;
    }
    if (led_timer) {
        if (led_timer == 1) {
            led_off();
        } else {
            --led_timer;
        }
    }
}

// =============================================================

int main(void)
{
    // Set up Timer2 (normal mode), freq ~55 Hz
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);   // Fcpu/1024
    TIMSK2 = (1 << TOIE2);      // Enable timer overflow interrupt

    DDRB |= (1 << PB5);         // LED
    led_off();

    uart_init(UART_BAUD_SELECT_DOUBLE_SPEED(BAUD, F_CPU));

    /*
     * now enable interrupts...
     */
    sei();

    msdelay(10);


    byte min = 0;
    byte packet[4];
    byte i = 0;

    while (1) {

        unsigned int data = uart_rx();

        // flush packet if data is invalid
        if (data & 0xff00) {
            if (i >= min) {
                txhead(JTAG430X2, NOK, 0);
                min = 1;
            }
            i = 0;
            continue;
        }

        packet[i++] = data;

        if (i < 4) {
            continue;           // keep looking for further bytes
        }

        byte app = packet[0];
        byte verb = packet[1];
        int len = packet[2] + 256 * packet[3];

        byte ok = 1;

        // read the data
        for (i = 0; i < len; i++) {
            data = uart_rx();

            if (data & 0xff00) {
                ok = 0;
                break;
            }
            if (i < CMDDATALEN) {
                cmddata[i] = data & 0xff;
            } else {
                ok = 0;         // just ignore the extra bytes
            }
        }

        if (ok && app == JTAG430X2) {
            // start processing
            jtag430x2_app.handle(app, verb, len);
        } else {
            txhead(JTAG430X2, NOK, 0);
        }

        i = 0;
    }

}
