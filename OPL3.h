/*
    Project:    Canyon
    Purpose:    OPL3 I/O
    Author:     Andrew Greenwood
    Date:       July 2018

    OPL3 provides a number of data registers which can be used to control
    global, channel-specific and operator-specific parameters. These are
    spread across two pairs of I/O addresses. Operator numbers need to be
    mapped to the appropriate register, which further complicates matters.

    The purpose of the OPL3 class is to make it more convenient to access
    the registers. Because several parameters are packed into each register,
    it is difficult to accurately describe them. So, here they are simply
    presented with names such as OPL3_OPERATOR_REGISTER_A, and these are
    intended to be used with the appropriate method (e.g. writeOperator),
    which will write the data to the corresponding register at the appropriate
    I/O address.

    It does not attempt to perform any further abstraction than this - it is
    to be used as a foundation for other components to build upon.

    Register data is not cached, and there is no way to read it back from the
    OPL3 device.

    The documentation on this web page helped a lot:
    http://www.fit.vutbr.cz/~arnost/opl/opl3.html
*/

#ifndef CANYON_OPL3_H
#define CANYON_OPL3_H 1

#include <stdint.h>
#include "ISABus.h"

// These are maximum values - percussion and 4-operator mode will reduce the
// available channels and operators
#define OPL3_NUMBER_OF_CHANNELS     18
#define OPL3_NUMBER_OF_OPERATORS    36

// 4-operator channel enable flags
#define OPL3_GLOBAL_REGISTER_A      0x04
// Tremolo depth, vibrato depth, percussion mode, percussion key-on flags
#define OPL3_GLOBAL_REGISTER_B      0xbd

// Frequency number (low 8 bits)
#define OPL3_CHANNEL_REGISTER_A     0xa0
// Key-on, block number, frequency number (high 2 bits)
#define OPL3_CHANNEL_REGISTER_B     0xb0
// Right, left, feedback modulation factor, synth type
#define OPL3_CHANNEL_REGISTER_C     0xc0

// Tremolo, vibrato, sustain, KSR, frequency multiplication factor
#define OPL3_OPERATOR_REGISTER_A    0x20
// Key scaling level, output level attenuation
#define OPL3_OPERATOR_REGISTER_B    0x40
// Attack rate, decay rate
#define OPL3_OPERATOR_REGISTER_C    0x60
// Sustain level, release rate
#define OPL3_OPERATOR_REGISTER_D    0x80
// Waveform select
#define OPL3_OPERATOR_REGISTER_E    0xe0

class OPL3 {
    public:
        OPL3(
            ISABus &isaBus,
            uint16_t ioBaseAddress);

        // Determine whether an OPL3 device is present at the I/O address.
        bool OPL3::detect() const;

        // Stop all sounds and reset the device. Must be called at startup to
        // put the device into OPL3 mode.
        void reset() const;

        // Write data to a global register
        bool writeGlobal(
            uint8_t registerId,     // OPL3_GLOBAL_REGISTER_x
            uint8_t data) const;

        // Write data to a register for a particular channel
        bool writeChannel(
            uint8_t channelNumber,  // 0 ~ OPL3_NUMBER_OF_CHANNELS
            uint8_t registerId,     // OPL_CHANNEL_REGISTER_x
            uint8_t data) const;

        // Write data to a register for a particular operator
        bool writeOperator(
            uint8_t operatorNumber, // 0 ~ OPL3_NUMBER_OF_OPERATORS
            uint8_t registerId,     // OPL_OPERATOR_REGISTER_x
            uint8_t data) const;

    private:
        uint8_t readStatus() const;

        void writeData(
            bool primaryRegisterSet,
            uint8_t opl3Register,
            uint8_t data) const;

        ISABus &m_isaBus;
        uint16_t m_ioBaseAddress;
};

#endif
