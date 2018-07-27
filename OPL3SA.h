#ifndef OPL3SA_H
#define OPL3SA_H 1

#include <stdlib.h>
#include "ISABus.h"

class OPL3SA {
    public:
        OPL3SA(
            const ISABus &isaBus);

        bool init(
            uint16_t mpu401Port,
            uint8_t mpu401IRQ,
            uint16_t adlibPort);

    private:
        void writeControl(
            uint8_t reg,
            uint8_t data);

        uint8_t readControl(
            uint8_t reg);

        const ISABus &m_isaBus;
        uint16_t m_address;
};

#endif
