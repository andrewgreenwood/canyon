/*
    Project:    Canyon
    Purpose:    MIDI message handling
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#include "MIDI.h"

uint8_t getExpectedMidiMessageLength(
    uint8_t status)
{
    switch (status & 0xf0) {
        case 0xc0:
        case 0xd0:
            return 2;

        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
        case 0xe0:
            return 3;

        case 0xf6:
        case 0xf7:
        case 0xf8:
        case 0xfa:
        case 0xfb:
        case 0xfc:
        case 0xfe:
        case 0xff:
            return 1;

        case 0xf1:
        case 0xf3:
            return 2;

        case 0xf2:
            return 3;

        default:
            // Unsupported (ignore until next status)
            return 0;
    }
}
