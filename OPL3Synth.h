#ifndef CANYON_OPL3SYNTH_H
#define CANYON_OPL3SYNTH_H 1

#include <stdint.h>
#include "OPL3.h"
#include "OPL3Operator.h"

#define OPL3_INVALID_CHANNEL    ((uint8_t)-1)
#define OPL3_INVALID_OPERATOR   ((uint8_t)-1)

// Pseudo-channels
#define OPL3_KICK_CHANNEL       18
#define OPL3_SNARE_CHANNEL      19
#define OPL3_TOMTOM_CHANNEL     20
#define OPL3_CYMBAL_CHANNEL     21
#define OPL3_HIHAT_CHANNEL      22

#define isPercussionChannel(channel) \
    ((channel >= OPL3_KICK_CHANNEL) && (channel <= OPL3_HIHAT_CHANNEL))

#define OPL3_MAX_CHANNEL        OPL3_HIHAT_CHANNEL

enum OPL3ChannelType {
    NullChannelType = 0,
    Melody2OpChannelType,
    Melody4OpChannelType,
    KickChannelType,
    SnareChannelType,
    TomTomChannelType,
    CymbalChannelType,
    HiHatChannelType
};

class __attribute__((packed)) OPL3Synth {
    public:
        OPL3Synth(
            OPL3 &opl3);

        uint8_t allocateChannel(
            OPL3ChannelType type);

        bool freeChannel(
            uint8_t channel);

        bool setFrequency(
            uint8_t channel,
            uint16_t frequency);

        bool keyOn(
            uint8_t channel);

        bool keyOff(
            uint8_t channel);

        bool enableSustain(
            uint8_t channel,
            uint8_t operatorIndex);

        bool disableSustain(
            uint8_t channel,
            uint8_t operatorIndex);

        bool enableEnvelopeScaling(
            uint8_t channel,
            uint8_t operatorIndex);

        bool disableEnvelopeScaling(
            uint8_t channel,
            uint8_t operatorIndex);

        bool setFrequencyMultiplicationFactor(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t factor);

        bool setAttenuation(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t attenuation);

        bool setAttackRate(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t rate);

        bool setDecayRate(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t rate);

        bool setSustainLevel(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t rate);

        bool setReleaseRate(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t rate);

        bool setWaveform(
            uint8_t channel,
            uint8_t operatorIndex,
            uint8_t waveform);

        //    private:
        OPL3ChannelType getChannelType(
            uint8_t channel) const;

        bool isValidChannel(
            uint8_t channel) const;

        bool isAllocatedChannel(
            uint8_t channel) const;

        uint8_t findAvailableChannel(
            OPL3ChannelType type) const;

        unsigned int getOperatorCount(
            uint8_t channel) const;

        uint8_t getChannelOperator(
            uint8_t channel,
            uint8_t operatorIndex) const;

        bool updateOperator(
            uint8_t operatorIndex,
            uint8_t registerId);

        OPL3 &m_opl3;

        OPL3Operator m_operators[38];

        // Each bit represents whether a channel has been allocated or not
        unsigned m_channelBusyBitmap : 23;

        unsigned m_channel0_4op     : 1;
        unsigned m_channel1_4op     : 1;
        unsigned m_channel2_4op     : 1;
        unsigned m_channel9_4op     : 1;
        unsigned m_channel10_4op    : 1;
        unsigned m_channel11_4op    : 1;

        unsigned m_percussionMode   : 1;
        unsigned m_kickKeyOn        : 1;
        unsigned m_snareKeyOn       : 1;
        unsigned m_tomTomKeyOn      : 1;
        unsigned m_cymbalKeyOn      : 1;
        unsigned m_hiHatKeyOn       : 1;

        // Channels 6 thru 8 are considered last when allocating channels, as
        // any use of percussion will render them unavailable. Channels that
        // can function in 4-op mode are allocated in pairs when used as 2-op.
        const uint8_t melody2OpChannelPriority[19] = {
            15, 16, 17, 0, 3, 1, 4, 2, 5, 9, 12, 10, 13, 11, 14, 6, 7, 8,
            OPL3_INVALID_CHANNEL
        };

        // Search backwards to ensure minimal conflicts with 2-op channels.
        const uint8_t melody4OpChannelPriority[7] = {
            11, 10, 9, 2, 1, 0,
            OPL3_INVALID_CHANNEL
        };
};

#endif
