/*
    Project:    Canyon
    Purpose:    ISA Bus Interface
    Author:     Andrew Greenwood
    Date:       July 2018

    Use this with 2x shift registers (e.g. SN74HC595) and 1x universal shift
    register (e.g. SN74ALS299), which will be used for the 16-bit address/port
    and 8-bit data, respectively.

    The registers need to be wired so that the shifted bits flow like this:
        outputPin --> 595 A --> 595 B --> 299 --> inputPin

    Reads/writes are in LSB order. When writing, data is shifted out first,
    followed by the address. The data ends up in the 299. When reading, only
    the address is shifted out, and the data is loaded into the 299 from the
    ISA card, before being shifted in.

    Some shortcuts were taken to save on the Arduino pin count:
    1. Connect OE pins of all shift registers to GND so they are always LOW
    2. Connect SRCLR and CLR to +5V so they are always HIGH
    3. Connect S0 of the 299 shift register to +5V so it is always HIGH

    The clock pins are connected together.

    See the comments for the ISABus constructor arguments regarding the pins
    that the Arduino is required to connect to.
*/

#ifndef ISABUS_H
#define ISABUS_H

#include <stdint.h>

#ifndef ARDUINO
    // Not compiling for embedded use - use simulated ISA bus
    #include "prototype/ISABus.h"
#else
class ISABus {
    public:
        ISABus(
            uint8_t outputPin,  // To first shift register's serial input (SER)
            uint8_t inputPin,   // From final shift register's serial output (Qh')
            uint8_t clockPin,   // Shift clock pin (SRCLK,SRCLK,CLK)
            uint8_t latchPin,   // Shift register store pin (RCLK,RCLK)
            uint8_t loadPin,    // To S1 pin of final shift register
            uint8_t ioWritePin, // To IOW pin of ISA device (active low)
            uint8_t ioReadPin,  // To IOR pin of ISA device (active low)
            uint8_t resetPin);  // To RESET pin of ISA device (active low)

        void reset() const;

        void write(
            uint16_t address,
            uint8_t data) const;

        uint8_t read(
            uint16_t address) const;

    private:
        uint8_t m_outputPin;
        uint8_t m_inputPin;
        uint8_t m_clockPin;
        uint8_t m_latchPin;
        uint8_t m_loadPin;
        uint8_t m_ioWritePin;
        uint8_t m_ioReadPin;
        uint8_t m_resetPin;
};
#endif

#endif
