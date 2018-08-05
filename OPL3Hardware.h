#ifndef CANYON_OPL3HARDWARE_H
#define CANYON_OPL3HARDWARE_H 1

#include <stdint.h>
#include "ISABus.h"

/*
#define OPL3_KICK_CHANNEL           18
#define OPL3_SNARE_CHANNEL          19
#define OPL3_TOMTOM_CHANNEL         20
#define OPL3_CYMBAL_CHANNEL         21
#define OPL3_HIHAT_CHANNEL          22

#define OPL3_MAX_CHANNEL            OPL3_HIHAT_CHANNEL
#define OPL3_NUMBER_OF_CHANNELS     (OPL3_MAX_CHANNEL + 1)
*/

namespace OPL3 {

enum {
    InvalidChannel  = 0xff,
    InvalidOperator = 0xff,

    KickChannel     = 18,
    SnareChannel    = 19,
    TomTomChannel   = 20,
    CymbalChannel   = 21,
    HiHatChannel    = 22,

    MaxChannelNumber    = HiHatChannel,
    NumberOfChannels    = MaxChannelNumber + 1
};
    
typedef enum {
    GlobalRegisterA = 0x0001,
    GlobalRegisterB = 0x0002,
    GlobalRegisterC = 0x0003,
    GlobalRegisterD = 0x0004,
    GlobalRegisterE = 0x0008,
    GlobalRegisterF = 0x00bd,
    GlobalRegisterG = 0x0104,
    GlobalRegisterH = 0x0105
} GlobalRegister;

typedef enum {
    ChannelRegisterA = 0xa0,
    ChannelRegisterB = 0xb0,
    ChannelRegisterC = 0xc0
} ChannelRegister;

typedef enum {
    OperatorRegisterA = 0x20,
    OperatorRegisterB = 0x40,
    OperatorRegisterC = 0x60,
    OperatorRegisterD = 0x80,
    OperatorRegisterE = 0xe0
} OperatorRegister;

typedef enum {
    NullChannelType = 0,
    Melody2OpChannelType,
    Melody4OpChannelType,
    KickChannelType,
    SnareChannelType,
    TomTomChannelType,
    CymbalChannelType,
    HiHatChannelType
} ChannelType;


uint16_t getFrequencyFnum(
    uint32_t frequencyHundredths,
    uint8_t block);

uint8_t getFrequencyBlock(
    uint32_t frequencyHundredths);

uint32_t getNoteFrequency(
    uint8_t note);

class Hardware {
    public:
        Hardware(
            ISABus &isaBus,
            uint16_t ioBaseAddress);

        bool detect() const;

        void reset() const;

        uint8_t allocateChannel(
            ChannelType type);

        bool freeChannel(
            uint8_t channel);

        bool setChannelRegister(
            uint8_t channel,
            ChannelRegister reg,
            uint8_t data) const;

        bool setOperatorRegister(
            uint8_t channel,
            uint8_t operatorIndex,
            OperatorRegister reg,
            uint8_t data) const;

    private:
        ChannelType getChannelType(
            uint8_t channel) const;

        bool isValidChannel(
            uint8_t channel) const;

        bool isAllocatedChannel(
            uint8_t channel) const;

        uint8_t findAvailableChannel(
            ChannelType type) const;

        unsigned int getOperatorCount(
            uint8_t channel) const;

        uint8_t getChannelOperator(
            uint8_t channel,
            uint8_t operatorIndex) const;

        bool writeGlobalRegister(
            GlobalRegister reg,
            uint8_t data) const;

        bool writeChannelRegister(
            uint8_t channel,
            ChannelRegister reg,
            uint8_t data) const;

        bool writeOperatorRegister(
            uint8_t op,
            OperatorRegister reg,
            uint8_t data) const;

        uint8_t readStatus() const;

        void writeData(
            bool primaryRegisterSet,
            uint8_t opl3Register,
            uint8_t data) const;

        ISABus &m_isaBus;
        uint16_t m_ioBaseAddress;

        // Each bit represents whether a channel has been allocated or not
        // OPL3 provides 18 channels, but some additional ones are allocated
        // here to cater for percussion
        unsigned m_allocatedChannelBitmap : 23;

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
            InvalidChannel
        };

        // Search backwards to ensure minimal conflicts with 2-op channels.
        const uint8_t melody4OpChannelPriority[7] = {
            11, 10, 9, 2, 1, 0,
            InvalidChannel
        };
};

}

#endif
