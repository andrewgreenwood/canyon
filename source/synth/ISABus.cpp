/*
    Project:    Canyon
    Purpose:    ISA Bus Interface
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018

    TODO:
        Remove bit-banging support (enabled when USE_SPI is not defined)
        Clean up pins passed to constructor (SPI uses specific pins)

    Interrupts used to be disabled if an ISA bus read/write took place
    outside of an interrupt handler, but this effectively disables all
    of the delays. So this code is commented-out for now (but should
    work so long as interrupts are not enabled in canyon.ino)
*/

#include <arduino.h>
#include <SPI.h>
#include "ISABus.h"
#include "ISRState.h"

#define USE_SPI         1
#define ISA_IOR_DELAY   10
#define ISA_IOW_DELAY   10
#define ISA_PRE_DELAY   10
#define ISA_POST_DELAY  10
#define DO_LSB          1

ISABus::ISABus(
    uint8_t outputPin,
    uint8_t inputPin,
    uint8_t clockPin,
    uint8_t latchPin,
    uint8_t loadPin,
    uint8_t ioWritePin,
    uint8_t ioReadPin,
    uint8_t resetPin)
: m_outputPin(outputPin), m_inputPin(inputPin),
  m_clockPin(clockPin), m_latchPin(latchPin),
  m_loadPin(loadPin), m_ioWritePin(ioWritePin),
  m_ioReadPin(ioReadPin), m_resetPin(resetPin)
{
}

void ISABus::reset() const
{
    Serial.println("Resetting ISA bus");

    // This needs to be set as output or the microcontroller will be unable
    // to determine whether it is acting as an SPI slave or not
    pinMode(10, OUTPUT);

    // Raise IOW and IOR on the ISA bus
    pinMode(m_ioWritePin, OUTPUT);
    pinMode(m_ioReadPin, OUTPUT);
    digitalWrite(m_ioWritePin, HIGH);
    digitalWrite(m_ioReadPin, HIGH);

    // Lower RESET on the ISA bus
    pinMode(m_resetPin, OUTPUT);
    digitalWrite(m_resetPin, HIGH);

    // Shift register / SPI pins
    pinMode(m_latchPin, OUTPUT);
    pinMode(m_outputPin, OUTPUT);
    pinMode(m_inputPin, INPUT);
    pinMode(m_clockPin, OUTPUT);
    pinMode(m_loadPin, OUTPUT);

    // Ready to go
    digitalWrite(m_resetPin, LOW);
}

void ISABus::write(
    uint16_t address,
    uint8_t data) const
{
#ifdef PRINT_IO
    Serial.print("OUT 0x");
    Serial.print(address, HEX);
    Serial.print(", 0x");
    Serial.println(data, HEX);
#endif

    if (!inISR()) {
        //noInterrupts();
    }

    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

#ifdef USE_SPI
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));

    SPI.transfer(data);
    //SPI.transfer16(address);
    SPI.transfer(address & 0xff);
    SPI.transfer((address & 0xff00) >> 8);
#else
#ifdef DO_LSB
    // The data goes first so it ends up in the bi-directional shift register
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, data);

    // Next, we write the address
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, address & 0xff);
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, (address & 0xff00) >> 8);
#else
    // The data goes first so it ends up in the universal shift register
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, data);

    // Next, we write the address
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, (address & 0xff00) >> 8);
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, address & 0xff);
#endif
#endif

    // Store the data into the 595s we're using for the address
    digitalWrite(m_latchPin, LOW);
    digitalWrite(m_latchPin, HIGH);

    delayMicroseconds(ISA_PRE_DELAY);

    // Lower the IOW pin on the ISA bus to indicate we're writing data
    digitalWrite(m_ioWritePin, LOW);
    delayMicroseconds(ISA_IOR_DELAY);
    digitalWrite(m_ioWritePin, HIGH);

    delayMicroseconds(ISA_POST_DELAY);

#ifdef USE_SPI
    SPI.endTransaction();
#endif

    if (!inISR()) {
        //interrupts();
    }
}

uint8_t ISABus::read(
    uint16_t address) const
{
    uint8_t data = 0;

#ifdef PRINT_IO
    Serial.print("IN 0x");
    Serial.print(address, HEX);
    Serial.print(" --> ");
#endif

    if (!inISR()) {
        //noInterrupts();
    }

#ifdef USE_SPI
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));

    SPI.transfer(address & 0xff);
    SPI.transfer((address & 0xff00) >> 8);
#else
#ifdef DO_LSB
    // Send out the address
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, address & 0xff);
    shiftOut(m_outputPin, m_clockPin, LSBFIRST, (address & 0xff00) >> 8);
#else
    // Send out the address
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, (address & 0xff00) >> 8);
    shiftOut(m_outputPin, m_clockPin, MSBFIRST, address & 0xff);
#endif
#endif

    // Store the data into the 595s we're using for the address
    digitalWrite(m_latchPin, LOW);
    digitalWrite(m_latchPin, HIGH);

    // Put the data shift register into 'load' mode
    digitalWrite(m_loadPin, HIGH);

    delayMicroseconds(ISA_PRE_DELAY);

    // Lower the IOR pin on the ISA bus to indicate we want to read data
    digitalWrite(m_ioReadPin, LOW);

    // Give the device some time to respond
    delayMicroseconds(ISA_IOR_DELAY);

    // Load the data from the ISA data pins into the universal shift register
#ifdef USE_SPI
    SPI.transfer(0);
#else
    digitalWrite(m_clockPin, LOW);
    digitalWrite(m_clockPin, HIGH);
#endif

    // Stop writing
    digitalWrite(m_ioReadPin, HIGH);

    delayMicroseconds(ISA_POST_DELAY);

    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

#ifdef USE_SPI
    data = SPI.transfer(0);
    SPI.endTransaction();
#else
    // The first bit is immediately available, so get that first
    data = digitalRead(m_inputPin);

    // Shift the remaining 7 bits in
    for (int i = 1; i < 8; ++ i) {
        digitalWrite(m_clockPin, LOW);
        digitalWrite(m_clockPin, HIGH);
        data |= digitalRead(m_inputPin) << i;
    }

    // Leave the clock pin in a low state
    digitalWrite(m_clockPin, LOW);
#endif

    if (!inISR()) {
        //interrupts();
    }

#ifdef PRINT_IO
    Serial.println(data, HEX);
#endif

    return data;
}
