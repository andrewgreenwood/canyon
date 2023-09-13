/*
    Project:    Canyon
    Purpose:    Simulated ISA Bus Interface (for prototyping embedded code)
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifndef CANYON_SIMULATED_ISABUS_H
#define CANYON_SIMULATED_ISABUS_H 1

#include <stdint.h>

class ISABus {
    public:
        ISABus();

        void reset() const;

        void write(
            uint16_t address,
            uint8_t data) const;

        uint8_t read(
            uint16_t address) const;

    private:
};

#endif
