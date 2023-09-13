/*
    Project:    Canyon
    Purpose:    Parallel I/O through Arduino digital pins
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       September 2023
*/

#ifndef PARALLEL_PINS_H
#define PARALLEL_PINS_H 1

#include <arduino.h>

// Convenience class to directly set digital pins from 2-bit to 8-bit values
// Pins do not need to be on the same port, or in any particular order
// This is faster than using digitalWrite but slower than direct access
class ParallelPins {
    public:
        ParallelPins(
            uint8_t pinA,       // High bit
            uint8_t pinB,
            uint8_t pinC = 255,
            uint8_t pinD = 255,
            uint8_t pinE = 255,
            uint8_t pinF = 255,
            uint8_t pinG = 255,
            uint8_t pinH = 255)
        : m_numberOfPins(0), m_pinMaskB(0), m_pinMaskD(0)
        {
            uint8_t pins[8] = {pinA, pinB, pinC, pinD, pinE, pinF, pinG, pinH};

            for (int i = 0; i < 8; ++ i) {
                if (pins[i] != 255) {
                  ++ m_numberOfPins;
                  m_pinBitmaps[i] = getPinBit(pins[i]);
                  m_pinMaskB |= m_pinBitmaps[i] >> 8;
                  m_pinMaskD |= m_pinBitmaps[i] & 0xff;
                }
            }
        }

        void operator = (uint8_t value)
        {
            uint16_t setBits = 0;
            uint8_t bit = 0x1 << (m_numberOfPins - 1);
            for (int i = 0; i < 8; ++ i) {
                //Serial.println(m_pinBitmaps[i], HEX);
                if (value & bit) {
                    setBits |= m_pinBitmaps[i];
                }
                bit >>= 1;
            }

            // Set pins as outputs
            DDRB |= m_pinMaskB;
            DDRD |= m_pinMaskD;

            // Update ports
            PORTB &= ~m_pinMaskB;
            PORTB |= setBits >> 8;
            PORTD &= ~m_pinMaskD;
            PORTD |= setBits & 0xff;
        }

        operator uint8_t ()
        {
            // TODO: Set pins as inputs
            // DDRB &= ~m_pinMaskB;
            // DDRD &= ~m_pinMaskD;

            // TODO: Interpret PORTB and PORTD bits
            //return m_value;
            return 0;
        }

    private:
        uint16_t getPinBit(uint8_t pin)
        {
            if (pin < 14) {
                return 0x1 << pin;
            } else {
                return 0;
            }
        }

        uint8_t m_numberOfPins;
        // High bits = PORTB, low bits = PORTD
        uint16_t m_pinBitmaps[8];
        uint8_t m_pinMaskB;
        uint8_t m_pinMaskD;
};

#endif
