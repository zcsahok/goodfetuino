GoodFETuino
===========

This is an port of the [GoodFET](https://github.com/travisgoodspeed/goodfet) JTAG430 firmware app to Arduino platform.
It turns your Arduino into a MSP430 device programmer.

Currently only 4-wire JTAG is supported.
Code size is around 7k so it can run on smaller chips too.



Installing
------

Before building configure in Makefile the port Arduino is connected to.
Also make sure that gcc-avr and avr-libc are installed.
Normally they are if one was programming Arduino before.
Then build and upload using

```
make clean all upload
```

Pinout and connections
------

| Arduino | ATmega | JTAG  |
|:-------:|:------:|:-----:|
| A0      | PC0    |  TST  |
| A1      | PC1    |  RST  |
| A2      | PC2    |  TCK  |
| A3      | PC3    |  TMS  |
| A4      | PC4    |  TDI  |
| A5      | PC5    |  TDO  |
| GND     | GND    |  GND  |

Using the analog pins has the advantage that the spacing between the headers
is 0.1" allowing the use of a single male plug for connections.
All lines have nominally 220 ohm resistors in series.

See also [schematic_bb.png](https://github.com/zcsahok/goodfetuino/blob/master/schematic_bb.png)

**NOTE:** MSP430 devices run from 3.3 V. Do not use a 5 V Arduino as it
stresses the chip beyond the max ratings. Even though it may not
brick your chip but use at your own risk.

Using
-----

[MSPDebug](https://github.com/dlbeer/mspdebug) is a nice and handy backend.
Replace `/dev/ttyUSB0` with the port used in Makefile.

```
mspdebug -j -d /dev/ttyUSB0 goodfet
```


Enjoy.

