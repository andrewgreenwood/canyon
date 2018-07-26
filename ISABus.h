#ifndef ISABUS_H
#define ISABUS_H

#include <stdint.h>

class ISABus {
    public:
        ISABus(
            uint8_t outputPin,  // To first shift register's serial input (SER)
            uint8_t inputPin,   // From final shift register's serial output (Qh')
            uint8_t clockPin,   // Shift clock pin (RCLK,RCLK,CLK)
            uint8_t loadPin,    // To S1 pin of final shift register
            uint8_t ioWritePin, // To IOW pin of device (active low)
            uint8_t ioReadPin,  // To IOR pin of device (active low)
            uint8_t resetPin);  // To RESET pin of device (active low)

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
        uint8_t m_loadPin;
        uint8_t m_ioWritePin;
        uint8_t m_ioReadPin;
        uint8_t m_resetPin;
};

#endif
