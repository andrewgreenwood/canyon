#ifndef CANYON_OPL3_H
#define CANYON_OPL3_H 1

#include <stdint.h>
#include "ISABus.h"

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

        bool OPL3::detect() const;

        void reset() const;

        bool writeGlobal(
            uint8_t registerId,
            uint8_t data) const;

        bool writeChannel(
            uint8_t channelNumber,
            uint8_t registerId,
            uint8_t data) const;

        bool writeOperator(
            uint8_t operatorNumber,
            uint8_t registerId,
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
