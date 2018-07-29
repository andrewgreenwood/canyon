/*
    Project:    Canyon
    Purpose:    ISA Bus Interface
    Author:     Andrew Greenwood
    Date:       July 2018
*/

#include <arduino.h>
#include <SPI.h>
#include "ISABus.h"

#define USE_SPI         1
#define ISA_IO_WAIT     10000
#define DO_LSB          1

ISABus::ISABus(
    uint8_t outputPin,
    uint8_t inputPin,
    uint8_t clockPin,
    uint8_t loadPin,
    uint8_t ioWritePin,
    uint8_t ioReadPin,
    uint8_t resetPin)
: m_outputPin(outputPin), m_inputPin(inputPin), m_clockPin(clockPin),
  m_loadPin(loadPin), m_ioWritePin(ioWritePin), m_ioReadPin(ioReadPin),
  m_resetPin(resetPin)
{
    pinMode(10, OUTPUT);    // slave select pin
    reset();
}

void ISABus::reset() const
{
    // Raise IOW and IOR on the ISA bus
    pinMode(m_ioWritePin, OUTPUT);
    pinMode(m_ioReadPin, OUTPUT);
    digitalWrite(m_ioWritePin, HIGH);
    digitalWrite(m_ioReadPin, HIGH);

    // Lower RESET on the ISA bus
    pinMode(m_resetPin, OUTPUT);
    digitalWrite(m_resetPin, HIGH);

    pinMode(m_outputPin, OUTPUT);
    pinMode(m_inputPin, INPUT);
    pinMode(m_clockPin, OUTPUT);
    pinMode(m_loadPin, OUTPUT);

    // Ready to go
    digitalWrite(m_resetPin, LOW);

    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    SPI.transfer(0);
    SPI.transfer(0);
    SPI.transfer(0);
    digitalWrite(7, LOW);
    digitalWrite(7, HIGH);
    SPI.endTransaction();
}

void ISABus::write(
    uint16_t address,
    uint8_t data) const
{
    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

    Serial.print("OUT 0x");
    Serial.print(address, HEX);
    Serial.print(", 0x");
    Serial.println(data, HEX);

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

    //delay(500);

    // The output pin is wired up to the SRCLK on the 595s as well as the SER
    // input, so we flip it here to perform a store
    delay(100);
    digitalWrite(7, LOW);
    digitalWrite(7, HIGH);

    delay(100);

    // Lower the IOW pin on the ISA bus to indicate we're writing data
    digitalWrite(m_ioWritePin, LOW);
    //for (int wait = 0; wait < ISA_IO_WAIT; ++ wait) {}
    delayMicroseconds(ISA_IO_WAIT);
    digitalWrite(m_ioWritePin, HIGH);

#ifdef USE_SPI
    SPI.endTransaction();
#endif
}

uint8_t ISABus::read(
    uint16_t address) const
{
    uint8_t data = 0;

    Serial.print("IN 0x");
    Serial.print(address, HEX);
    Serial.print(" --> ");

#ifdef USE_SPI
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));

    //SPI.transfer16(address);
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

    // The output pin is wired up to the SRCLK on the 595s as well as the SER
    // input, so we flip it here to perform a store
    digitalWrite(7, LOW);
    digitalWrite(7, HIGH);

    // Put the data shift register into 'load' mode
    digitalWrite(m_loadPin, HIGH);

    // Lower the IOR pin on the ISA bus to indicate we want to read data
    digitalWrite(m_ioReadPin, LOW);

    // Give the device some time to respond
    delayMicroseconds(ISA_IO_WAIT);
    //for (int wait = 0; wait < ISA_IO_WAIT; ++ wait) {}

    // Load the data from the ISA data pins into the universal shift register
#ifdef USE_SPI
    SPI.transfer(0);
#else
    digitalWrite(m_clockPin, LOW);
    digitalWrite(m_clockPin, HIGH);
#endif

    // Stop writing
    digitalWrite(m_ioReadPin, HIGH);

    // Put the data shift register into 'shift' mode
    digitalWrite(m_loadPin, LOW);

    SPI.endTransaction();
#ifdef USE_SPI
    //SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    data = SPI.transfer(0);
//    SPI.endTransaction();
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

    Serial.println(data, HEX);
    return data;
}
