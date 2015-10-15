#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <stdint.h>

extern void led_off();
extern void led_toggle();

extern void serial_tx(unsigned char);

#endif

