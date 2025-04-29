#nuvoprog-c Nuvoton microcontroller programmer (in C)

`nuvoprog` is an open source tool for programming Nuvoton microcontollers;
previously, they could only be programmed under Windows using Nuvoton's
proprietary tools. This tool assumes a Nuvoton NuLink family programmer
or compatible; no support is provided (yet) for other programmers.

This tool is VERY CRUDE a port of the excellent nuvoprog tool by Erin
Shepherd you can fin here: https://github.com/erincandescent/nuvoprog.git

Example usage:
```
$ nuvoprog-c --erase
$ nuvoprog-c --program file.ihx

```

You may also be interested in [libn76](https://github.com/erincandescent/libn76),
a SDCC-supporting BSP for the Nuvoton N76 family.

# Installing
This is a C project; just type:
```
$ make
$ sudo make install
```

libusb-dev package needs to be installed on your system, ex on Ubuntu:
```
$ sudo apt-get install libusb-1.0-0-dev
```

# Supported Devices
## Programmers

 * Nu-Link-Me (as found on Nu-Tiny devboards)
 * Nu-Link

## Target and tested devices

 * N76E003
 * N76E616
 * N76E885
 * MS51FB Series

# Missing functionality

* Firmware upgrades
* Debugging?

# Adding support for new devices

To add support for new devices, you will need:

 * Windows
 * The Nuvoton ICP tool, and
 * Wireshark

A Wireshark dissector for the protocol can be found in the misc directory.

Nuvoton have [an OpenOCD patch](http://openocd.zylin.com/#/c/4739/1) which you may find useful as reference material

## Other NuLink Programmers
If this is a protocol v2 programmer, you'll need to add support for that (The leading length field
changes from 8 to 16 bits, but othewise things are unchanged).

Add the VID and PID to the table in `protocol/device.go` and see if `nuvoprog` connects successfully.
If it doesn't, compare protocol exchanges in Wireshark

## Other Microcontrollers
First step is to see if the microcontroller belongs to the same family and if the connection and
programming flow is the same (The flow should be the same for the 8051T1 family, may differ for
others).

If they are, you probably just need to define target devide details:

 * Configuration bit codec
 * Target definition (see `target/n76/n76e003.go`)

You may need to get details like LDROM offsets from Wireshark dumps
