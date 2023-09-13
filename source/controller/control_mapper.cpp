/*
    Project:    Canyon
    Purpose:    Control mapper
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       September 2023
*/

#include "control_mapper.h"

ArduinoDigitalPinSet arduinoDigitalPins;
ArduinoAnalogPinSet arduinoAnalogPins;

/*
    DigitalControl
*/

DigitalControl::DigitalControl(
    uint8_t pin,
    ControlTagType tagData = 0)
: Control(tagData),
  m_pinSet(arduinoDigitalPins),
  m_pin(pin),
  m_value(255)  // Out of range to force an initial update
{
    pinMode(pin, INPUT_PULLUP);
}

DigitalControl::DigitalControl(
    DigitalMultiplexer &multiplexer,
    uint8_t channel,
    ControlTagType tagData = 0)
: Control(tagData),
  m_pinSet(multiplexer),
  m_pin(channel)
{
}

bool DigitalControl::update()
{
    // TODO: debounce
    uint16_t newValue = m_pinSet.read(m_pin);
    bool hasChanged = newValue != m_value;
    if (hasChanged) {
        m_value = newValue;
    }

    return hasChanged;
}


/*
    AnalogControl
*/

AnalogControl::AnalogControl(
    uint8_t pin,
    ControlTagType tagData = 0)
: Control(tagData),
  m_pinSet(arduinoAnalogPins),
  m_pin(pin),
  m_value(2048)     // Out of range to force an initial update
{
}

AnalogControl::AnalogControl(
    AnalogMultiplexer &multiplexer,
    uint8_t channel,
    ControlTagType tagData = 0)
: Control(tagData),
  m_pinSet(multiplexer),
  m_pin(channel)
{
}

bool AnalogControl::update()
{
    uint16_t newValue = m_pinSet.read(m_pin);

    bool hasChanged = false;
    int16_t diff = (int16_t)newValue - (int16_t)m_value;
    if ((diff >= 4) || (diff <= -4)) {
        hasChanged = true;
        m_value = newValue;
    }

    return hasChanged;
}


