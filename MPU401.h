/*
    Project:    Canyon
    Purpose:    MPU-401 UART I/O
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifndef MPU401_H
#define MPU401_H

#include <stdint.h>
#include "ISABus.h"

class MPU401 {
    public:
        MPU401(
            const ISABus &isaBus);

        void init(
            uint16_t ioAddress);

        bool reset() const;

        bool canWrite() const;

        bool canRead() const;

        void waitWriteReady() const;

        void waitReadReady() const;

        void writeCommand(
            uint8_t command) const;

        void writeData(
            uint8_t data) const;

        uint8_t readData() const;

    private:
        const ISABus &m_isaBus;
        uint16_t m_ioAddress;
};

#endif
