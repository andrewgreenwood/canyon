/*
    Project:    Canyon
    Purpose:    Simulated ISA Bus Interface (for prototyping embedded code)
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018

    At the moment, this just displays whatever I/O addresses are being accessed.
    All input is returned as 0xFF.

    It could be extended to simulate devices being available at specific
    addresses (e.g. attach a device object), which could provide some more
    useful testing and debugging capabilities.
*/

#include <stdio.h>
#include "ISABus.h"

ISABus::ISABus()
{
}

void ISABus::reset() const
{
}

void ISABus::write(
    uint16_t address,
    uint8_t data) const
{
    printf("OUT %04x, %02x\n", address, data);
}

uint8_t ISABus::read(
    uint16_t address) const
{
    printf("IN %04x", address);
    return 0xff;
}
