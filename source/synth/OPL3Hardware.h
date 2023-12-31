/*
    Project:    Canyon
    Purpose:    OPL3 control
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifndef CANYON_OPL3HARDWARE_H
#define CANYON_OPL3HARDWARE_H 1

#include <stdint.h>
#include "ISABus.h"

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
    NullChannelType      = 0,
    Melody2OpChannelType = 1,
    Melody4OpChannelType = 2,
    KickChannelType      = 3,
    SnareChannelType     = 4,
    TomTomChannelType    = 5,
    CymbalChannelType    = 6,
    HiHatChannelType     = 7
} ChannelType;

typedef enum {
    NullOperatorType      = 0,
    CarrierOperatorType   = 1,
    ModulatorOperatorType = 2
} OperatorType;

uint16_t getFrequencyFnum(
    uint32_t frequencyHundredths,
    uint8_t block);

uint8_t getFrequencyBlock(
    uint32_t frequencyHundredths);

typedef struct ChannelParameters {
    unsigned type            : 3;   // See 'ChannelType' enum

    unsigned frequencyNumber : 10;

    unsigned block           : 3;
    unsigned keyOn           : 1;

    unsigned output          : 2;
    unsigned feedbackModulationFactor   : 3;
    unsigned synthType       : 2;
} ChannelParameters;

typedef struct OperatorParameters {
    unsigned tremolo         : 1;
    unsigned vibrato         : 1;
    unsigned sustain         : 1;
    unsigned ksr             : 1;
    unsigned frequencyMultiplicationFactor  : 4;

    unsigned keyScaleLevel   : 2;
    unsigned attenuation     : 6;

    unsigned attackRate      : 4;
    unsigned decayRate       : 4;

    unsigned sustainLevel    : 4;
    unsigned releaseRate     : 4;

    unsigned waveform        : 3;
} OperatorParameters;

class Hardware {
    public:
        Hardware(
            ISABus &isaBus,
            uint16_t ioBaseAddress);

        bool detect() const;

        void init();

        bool enablePercussion();

        bool disablePercussion();

        uint8_t allocateChannel(
            ChannelType type);

        bool freeChannel(
            uint8_t channel);

        // Global
        
        bool setTremoloDepth(
            bool depth);

        bool setVibratoDepth(
            bool depth);

        // Channel

        ChannelType getChannelType(
            uint8_t channel) const;

        bool setFrequency(
            uint8_t channel,
            uint32_t frequency);

        bool keyOn(
            uint8_t channel);

        bool keyOff(
            uint8_t channel);

        bool setOutput(
            uint8_t channel,
            uint8_t output);

        bool setFeedbackModulationFactor(
            uint8_t channel,
            uint8_t factor);

        bool setSynthType(
            uint8_t channel,
            uint8_t type);

        // Operator

        unsigned int getOperatorCount(
            uint8_t channel) const;

        OperatorType getOperatorType(
            uint8_t channel,
            uint8_t channelOperator) const;

        bool setTremolo(
            uint8_t channel,
            uint8_t channelOperator,
            bool tremolo);

        bool setVibrato(
            uint8_t channel,
            uint8_t channelOperator,
            bool vibrato);

        bool setSustain(
            uint8_t channel,
            uint8_t channelOperator,
            bool sustain);

        bool setEnvelopeScaling(
            uint8_t channel,
            uint8_t channelOperator,
            bool scaling);

        bool setFrequencyMultiplicationFactor(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t factor);

        bool setAttackRate(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t rate);

        bool setDecayRate(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t rate);

        bool setSustainLevel(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t level);

        bool setReleaseRate(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t rate);

        bool setKeyScaleLevel(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t level);

        bool setAttenuation(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t attenuation);

        bool setWaveform(
            uint8_t channel,
            uint8_t channelOperator,
            uint8_t waveform);

    private:
        bool isValidChannel(
            uint8_t channel) const;

        bool isPhysicalChannel(
            uint8_t channel) const;

        bool isAllocatedChannel(
            uint8_t channel) const;

        uint8_t shiftFreeChannel(
            uint8_t *list,
            uint8_t &freeCount);

        void addFreeChannel(
            uint8_t *list,
            uint8_t &freeCount,
            uint8_t channel);

        void removeFreeChannel(
            uint8_t *list,
            uint8_t &freeCount,
            uint8_t channel);

        uint8_t getChannelOperator(
            uint8_t channel,
            uint8_t operatorIndex) const;

        bool writeGlobalRegister(
            GlobalRegister reg,
            uint8_t data) const;

        bool commitGlobalData(
            GlobalRegister reg) const;

        bool commitChannelData(
            uint8_t channel,
            ChannelRegister reg) const;

        bool commitOperatorData(
            uint8_t op,
            OperatorRegister reg) const;

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
        //unsigned m_allocatedChannelBitmap : 23;
        uint32_t m_allocatedChannelBitmap;

        // These are the "physical" channels and operators that exist in the
        // OPL3 device
        ChannelParameters m_channelParameters[18];
        OperatorParameters m_operatorParameters[36];

        unsigned m_tremoloDepth     : 1;
        unsigned m_vibratoDepth     : 1;

        unsigned m_percussionMode   : 1;
        unsigned m_kickKeyOn        : 1;
        unsigned m_snareKeyOn       : 1;
        unsigned m_tomTomKeyOn      : 1;
        unsigned m_cymbalKeyOn      : 1;
        unsigned m_hiHatKeyOn       : 1;

        uint8_t m_free2OpChannels[18];
        uint8_t m_numberOfFree2OpChannels;

        uint8_t m_free4OpChannels[6];
        uint8_t m_numberOfFree4OpChannels;
};

}

#endif
