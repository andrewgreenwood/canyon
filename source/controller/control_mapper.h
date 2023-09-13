/*
    Project:    Canyon
    Purpose:    Control mapper
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       September 2023
*/

#ifndef CONTROL_MAPPER_H
#define CONTROL_MAPPER_H 1

#include <arduino.h>
#include "parallel_pins.h"

typedef uint32_t ControlTagType;

class Control;

/*
    This is something that has pins, such as a multiplexer (which may
    need to select a different channel before data can be read)
    All pins must be analog or digital (no mixing)
*/
class PinSet {
    public:
        virtual ~PinSet() {}
        virtual int read(
            uint8_t pin) = 0;
};

/*
    Arduino pins are wrapped in a couple of tiny classes which are
    used if only a pin number is specified
*/
class ArduinoDigitalPinSet: public PinSet {
    public:
        virtual ~ArduinoDigitalPinSet() {}

        virtual int read(
            uint8_t pin)
        {
            return digitalRead(pin);
        }
};

class ArduinoAnalogPinSet: public PinSet {
    public:
        virtual ~ArduinoAnalogPinSet() {}

        virtual int read(
            uint8_t pin)
        {
            // TODO: Translate 0 to A0 etc.
            return analogRead(pin);
        }
};

// Multiplexers

class DigitalMultiplexer: public PinSet {
    public:
        DigitalMultiplexer(
            uint8_t signalPin,
            const ParallelPins &addressPins,
            uint8_t enablePin = 255)
        : m_signalPin(signalPin),
          m_addressPins(addressPins),
          m_enablePin(enablePin)
        {
            pinMode(signalPin, INPUT_PULLUP);
        }

        virtual ~DigitalMultiplexer() {}

        virtual int read(
            uint8_t pin)
        {
            // Select channel
            m_addressPins = pin;
            // Do the read
            return digitalRead(m_signalPin);
        }

    private:
        uint8_t m_signalPin;
        ParallelPins m_addressPins;
        uint8_t m_enablePin;
};

class AnalogMultiplexer: public PinSet {
    public:
        AnalogMultiplexer(
            uint8_t signalPin,
            const ParallelPins &addressPins,
            uint8_t enablePin = 255)
        : m_signalPin(signalPin),
          m_addressPins(addressPins),
          m_enablePin(enablePin)
        {
        }

        virtual ~AnalogMultiplexer() {}

        virtual int read(
            uint8_t pin)
        {
            // Select channel
            m_addressPins = pin;
            // Do the read
            return analogRead(m_signalPin);
        }

    private:
        uint8_t m_signalPin;
        ParallelPins m_addressPins;
        uint8_t m_enablePin;
};

/*
    Base control type

    All controls can be updated and have their value retrieved
    They can have a tag associated with them, to store whatever data is
    required by the program when working with this control (for example,
    a MIDI CC number)
*/
class Control {
    public:
        Control(ControlTagType tagData = 0)
        : m_tagData(tagData)
        { }

        ControlTagType getTagData()
        { return m_tagData; }

        virtual ~Control() {}
        virtual bool update() = 0;
        virtual uint16_t value() = 0;

    private:
        // Any data the program wants to store against the control
        ControlTagType m_tagData;
};

/*
    Digital controls may only be associated with digital pins or
    digital multiplexers (TODO: allow attachment to analog? since
    specifying pin A0, A1 etc. does work)
*/
class DigitalControl: public Control {
    public:
        DigitalControl(
            uint8_t pin,
            ControlTagType tagData = 0);

        DigitalControl(
            DigitalMultiplexer &multiplexer,
            uint8_t channel,
            ControlTagType tagData = 0);

        virtual ~DigitalControl() {}

        virtual bool update();

        virtual uint16_t value()
        {
            // Inverted because input is pull-up
            return m_value == HIGH ? 0 : 1;
        }

    private:
        PinSet &m_pinSet;
        uint8_t m_pin;
        uint8_t m_value;
};

/*
    Analog controls may only be associated with analog pins or analog
    multiplexers
    Values are 8-bit and in the range 0-255
    Some effort is taken to avoid spurious value changes, which requires the
    resolution of the data to be lowered
*/
class AnalogControl: public Control {
    public:
        AnalogControl(
            uint8_t pin,
            ControlTagType tagData = 0);

        AnalogControl(
            AnalogMultiplexer &multiplexer,
            uint8_t channel,
            ControlTagType tagData = 0);

        virtual ~AnalogControl() {}

        virtual bool update();

        virtual uint16_t value()
        {
            return m_value >> 2;
        }

    private:
        PinSet &m_pinSet;
        uint8_t m_pin;
        uint16_t m_value;
};

/*
    Aliases for control types that don't add any extra functionality
*/
typedef AnalogControl Knob;
typedef DigitalControl Switch;

#endif
