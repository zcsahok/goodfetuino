GoodFETuino
===========

This is an port of the [GoodFET](https://github.com/travisgoodspeed/goodfet) JTAG430 firmware app to Arduino platform.
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


Using
-----

[MSPDebug](https://github.com/dlbeer/mspdebug) is a nice and handy backend.
Replace `/dev/ttyUSB0` with the port used in Makefile.

```
mspdebug -j -d /dev/ttyUSB0 goodfet
```


Enjoy.

