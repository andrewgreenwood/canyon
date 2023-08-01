/*
    Project:    Canyon
    Purpose:    OPL3 control
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifdef ARDUINO
#include <arduino.h>
#else
#include <cstdio>
#endif

#include "OPL3Hardware.h"

namespace OPL3 {

// F-Number = Music Frequency * 2^(20-Block) / 49716 Hz 
uint16_t getFrequencyFnum(
    uint32_t frequencyHundredths,
    uint8_t block)
{
    uint16_t fnum = 0;

    // This is an integer-only way of doing the above calculation
    // We bit-shift to take care of the 2^(20-Block) part
    // THe 2485800 is derived from 49716Hz
    fnum = (frequencyHundredths * (1048576L >> block)) / 2485800;
    return fnum < 1024 ? fnum : 1024;
}

uint8_t getFrequencyBlock(
    uint32_t frequencyHundredths)
{
    // The maximum frequency doubles each time the block number increases.
    // We start with 24.24Hz and try to find the lowest block that can
    // represent the desired frequency, for the greatest accuracy.
    uint8_t block;
    uint32_t maxFrequency = 2424;

    for (block = 0; block < 7; ++ block) {
        if (frequencyHundredths < maxFrequency) {
            return block;
        }

        maxFrequency <<= 1;
    }

    return block;
}

Hardware::Hardware(
    ISABus &isaBus,
    uint16_t ioBaseAddress)
: m_isaBus(isaBus),
  m_ioBaseAddress(ioBaseAddress),
  m_allocatedChannelBitmap(0),
  m_tremoloDepth(false),
  m_vibratoDepth(false),
  m_percussionMode(false),
  m_kickKeyOn(false),
  m_snareKeyOn(false),
  m_tomTomKeyOn(false),
  m_cymbalKeyOn(false),
  m_hiHatKeyOn(false),
  m_numberOfFree2OpChannels(0),
  m_numberOfFree4OpChannels(0)
{
    // Initialisation of channel/operator parameters is deferred
    // until init() is called
}

bool Hardware::detect() const
{
    uint8_t status;

    // Reset timers
    writeGlobalRegister(GlobalRegisterD, 0x60);

    // Reset IRQ
    writeGlobalRegister(GlobalRegisterD, 0x80);

    // Check status
    status = readStatus();
    if (status != 0) {
        return false;
    }

    // Set the timers
    writeGlobalRegister(GlobalRegisterB, 0xff);

    // Unmask timer 1
    writeGlobalRegister(GlobalRegisterD, 0x21);

    // Allow the timer to expire
#ifdef ARDUINO
    delayMicroseconds(80);
#endif

    // Re-check status
    status = readStatus();

    // Reset timers
    writeGlobalRegister(GlobalRegisterD, 0x60);

    // Reset IRQ
    writeGlobalRegister(GlobalRegisterD, 0x80);

    // Fail if OPL2 not detected
    if (status != 0xc0) {
        return false;
    }

    // Is this OPL3?
    return ((readStatus() & 0x06) == 0);
}

void Hardware::init()
{
    unsigned int i;

    m_allocatedChannelBitmap = 0;

    m_percussionMode = false;
    m_kickKeyOn = false;
    m_snareKeyOn = false;
    m_tomTomKeyOn = false;
    m_cymbalKeyOn = false;
    m_hiHatKeyOn = false;

    // Set up channel lists (2-op by default)
    m_numberOfFree2OpChannels = 18;
    for (i = 0; i < 18; ++ i) {
        m_free2OpChannels[i] = i;
    }

    // Disable waveform selection, reset test register
    writeGlobalRegister(GlobalRegisterA, 0x00);

    // Enable waveform selection
    writeGlobalRegister(GlobalRegisterA, 0x20);

    // Disable keyboard split
    writeGlobalRegister(GlobalRegisterE, 0x40);

    // Enable OPL3 mode
    writeGlobalRegister(GlobalRegisterH, 0x01);

    // Turn off 4-op channels
    writeGlobalRegister(GlobalRegisterG, 0x00);

    // Turn off percussion mode
    writeGlobalRegister(GlobalRegisterF, 0x00);

    // Init channels (as 2-op melody channels)
    for (i = 0; i < 18; ++ i) {
        m_channelParameters[i].type = Melody2OpChannelType;
        m_channelParameters[i].frequencyNumber = 0;
        m_channelParameters[i].block = 0;
        m_channelParameters[i].keyOn = false;
        m_channelParameters[i].output = 0;
        m_channelParameters[i].feedbackModulationFactor = 0;
        m_channelParameters[i].synthType = 0;

        commitChannelData(i, ChannelRegisterB); // Key-off first
        commitChannelData(i, ChannelRegisterA);
        commitChannelData(i, ChannelRegisterC);
    }

    // Init operators
    for (i = 0; i < 36; ++ i) {
        m_operatorParameters[i].sustain = false;
        m_operatorParameters[i].ksr = 0;
        m_operatorParameters[i].frequencyMultiplicationFactor = 0;
        m_operatorParameters[i].keyScaleLevel = 0;
        m_operatorParameters[i].attenuation = 0;
        m_operatorParameters[i].attackRate = 0;
        m_operatorParameters[i].decayRate = 0;
        m_operatorParameters[i].sustainLevel = 0;
        m_operatorParameters[i].releaseRate = 0;
        m_operatorParameters[i].waveform = 0;

        commitOperatorData(i, OperatorRegisterA);
        commitOperatorData(i, OperatorRegisterB);
        commitOperatorData(i, OperatorRegisterC);
        commitOperatorData(i, OperatorRegisterD);
        commitOperatorData(i, OperatorRegisterE);
    }
}

bool Hardware::enablePercussion()
{
    int i, j;

    if (m_percussionMode) {
        return true;
    }

    // Cannot enable percussion mode while channels 6, 7 or 8 are in use.
    for (i = 6; i <= 8; ++ i) {
        if (isAllocatedChannel(i)) {
            return false;
        }
    }

    m_percussionMode = true;

    // TODO: Use new channel add/remove
    // Make channels 6, 7 and 8 unavailable as 2-op melody channels
    j = 0;
    for (i = 0; i < m_numberOfFree2OpChannels; ++ i) {
        if ((m_free2OpChannels[i] < 6) || (m_free2OpChannels[i] > 8)) {
            m_free2OpChannels[j ++] = m_free2OpChannels[i];
        }
    }
    m_numberOfFree2OpChannels = j;

    return m_percussionMode;
}

bool Hardware::disablePercussion()
{
    if (!m_percussionMode) {
        return true;
    }

    // Cannot disable percussion mode while any percussion is playing
    if ((m_kickKeyOn) || (m_snareKeyOn) || (m_tomTomKeyOn) || (m_cymbalKeyOn)
        || (m_hiHatKeyOn)) {
        return false;
    }

    m_percussionMode = false;

    // TODO: Use new channel add/remove
    // Allow allocation of channels 6, 7 and 8 as 2-op melody channels
    for (int i = 6; i < 9; ++ i) {
        m_free2OpChannels[m_numberOfFree2OpChannels ++] = i;
    }

    return !m_percussionMode;
}

uint8_t Hardware::allocateChannel(
    ChannelType type)
{
    uint8_t channel = InvalidChannel;
    uint8_t fourOpCapableChannels[6] = {0, 1, 2, 9, 10, 11};

    switch (type) {
        case NullChannelType:
            return InvalidChannel;

        case Melody2OpChannelType:
            if (m_numberOfFree2OpChannels == 0) {
                // Convert a 4-op channel pair if we can
                uint8_t *candidates = fourOpCapableChannels;
                uint8_t bits = 0;

                for (int i = 0; i < 6; ++ i) {
                    if ((getChannelType(candidates[i]) == Melody4OpChannelType) &&
                        (getChannelType(candidates[i] + 3) == NullChannelType) &&
                        (!isAllocatedChannel(candidates[i])) &&
                        (!isAllocatedChannel(candidates[i] + 3))) {
                        removeFreeChannel(m_free4OpChannels, m_numberOfFree4OpChannels, candidates[i]);
                        // We'll be using the first channel of the pair - leave 2nd one free
                        addFreeChannel(m_free2OpChannels, m_numberOfFree2OpChannels, candidates[i] + 3);
                        m_channelParameters[candidates[i]].type = Melody2OpChannelType;
                        m_channelParameters[candidates[i] + 3].type = Melody2OpChannelType;
                        channel = candidates[i];
                        break;
                    }
                }

                if (channel != InvalidChannel) {
                    for (int i = 0; i < 6; ++ i) {
                        if (getChannelType(candidates[i]) == Melody4OpChannelType) {
                            bits |= 1 << i;
                        }
                    }

                    writeGlobalRegister(GlobalRegisterG, bits);
                }

                break;
            } else {
                // Take the first available channel
                channel = shiftFreeChannel(m_free2OpChannels, m_numberOfFree2OpChannels);
            }

            break;

        case Melody4OpChannelType:
            if (m_numberOfFree4OpChannels == 0) {
                // Maybe we can turn a 2-op channel pair into a 4-op channel?
                uint8_t *candidates = fourOpCapableChannels;
                uint8_t bits = 0;

                for (int i = 0; i < 6; ++ i) {
                    if ((getChannelType(candidates[i]) == Melody2OpChannelType) &&
                        (getChannelType(candidates[i] + 3) == Melody2OpChannelType) &&
                        (!isAllocatedChannel(candidates[i])) &&
                        (!isAllocatedChannel(candidates[i] + 3))) {
                        removeFreeChannel(m_free2OpChannels, m_numberOfFree2OpChannels, candidates[i]);
                        removeFreeChannel(m_free2OpChannels, m_numberOfFree2OpChannels, candidates[i] + 3);
                        m_channelParameters[candidates[i]].type = Melody4OpChannelType;
                        m_channelParameters[candidates[i] + 3].type = NullChannelType;
                        channel = candidates[i];
                        break;
                    }
                }

                if (channel != InvalidChannel) {
                    for (int i = 0; i < 6; ++ i) {
                        if (getChannelType(candidates[i]) == Melody4OpChannelType) {
                            bits |= 1 << i;
                        }
                    }

                    writeGlobalRegister(GlobalRegisterG, bits);
                }
            } else {
                // Take the first available channel
                channel = shiftFreeChannel(m_free4OpChannels, m_numberOfFree4OpChannels);
            }

            break;
            
        case KickChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(KickChannel))) {
                channel = KickChannel;
            }
            break;

        case SnareChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(SnareChannel))) {
                channel = SnareChannel;
            }
            break;

        case TomTomChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(TomTomChannel))) {
                channel = TomTomChannel;
            }
            break;

        case CymbalChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(CymbalChannel))) {
                channel = CymbalChannel;
            }
            break;

        case HiHatChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(HiHatChannel))) {
                channel = HiHatChannel;
            }
            break;

        default:
            return InvalidChannel;
    };

    if (channel != InvalidChannel) {
        m_allocatedChannelBitmap |= 1L << channel;
    }

    return channel;
}

bool Hardware::freeChannel(
    uint8_t channel)
{
    ChannelType channel_type;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    if ((channel_type = getChannelType(channel)) == NullChannelType) {
        return false;
    }

    if ((1L << channel & m_allocatedChannelBitmap) == 0) {
        return false;
    }

    m_allocatedChannelBitmap &= ~(1L << channel);

    // Put the channel in the next slot after the last one
    switch (channel_type) {
        case Melody2OpChannelType:
            addFreeChannel(m_free2OpChannels, m_numberOfFree2OpChannels, channel);
            break;

        case Melody4OpChannelType:
            addFreeChannel(m_free4OpChannels, m_numberOfFree4OpChannels, channel);
            break;
    };

    return true;
}

bool Hardware::setTremoloDepth(
    bool depth)
{
    m_tremoloDepth = depth;
    return commitGlobalData(GlobalRegisterF);
}

bool Hardware::setVibratoDepth(
    bool depth)
{
    m_vibratoDepth = depth;
    return commitGlobalData(GlobalRegisterF);
}

ChannelType Hardware::getChannelType(
    uint8_t channel) const
{
    if (!isValidChannel(channel)) {
        return NullChannelType;
    }

    if (isPhysicalChannel(channel)) {
        return (ChannelType)m_channelParameters[channel].type;
    } else {
        switch (channel) {
            case KickChannel:
                return m_percussionMode ? KickChannelType
                                        : NullChannelType;

            case SnareChannel:
                return m_percussionMode ? SnareChannelType
                                        : NullChannelType;

            case TomTomChannel:
                return m_percussionMode ? TomTomChannelType
                                        : NullChannelType;

            case CymbalChannel:
                return m_percussionMode ? CymbalChannelType
                                        : NullChannelType;

            case HiHatChannel:
                return m_percussionMode ? HiHatChannelType
                                        : NullChannelType;

            default:
                return NullChannelType;
        }
    };
}

bool Hardware::setFrequency(
    uint8_t channel,
    uint32_t frequency)
{
    uint8_t block;
    uint16_t fnum;
    uint8_t realChannel;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    block = getFrequencyBlock(frequency);
    fnum = getFrequencyFnum(frequency, block);

    if (isPhysicalChannel(channel)) {
        realChannel = channel;
    } else {
        switch (channel) {
            case KickChannel:
                realChannel = 6;
                break;

            case SnareChannel:
                realChannel = 7;
                break;

            case TomTomChannel:
                realChannel = 8;
                break;

            default:
                // Can't set frequency for HiHat or Cymbal channels
                return false;
        }
    }

    m_channelParameters[realChannel].frequencyNumber = fnum;
    m_channelParameters[realChannel].block = block;

    return ((commitChannelData(realChannel, ChannelRegisterA)) &&
            (commitChannelData(realChannel, ChannelRegisterB)));
}

bool Hardware::keyOn(
    uint8_t channel)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    if (isPhysicalChannel(channel)) {
        m_channelParameters[channel].keyOn = true;
        return commitChannelData(channel, ChannelRegisterB);
    } else {
        switch (channel) {
            case KickChannel:
                m_kickKeyOn = true;
                break;

            case SnareChannel:
                m_snareKeyOn = true;
                break;

            case TomTomChannel:
                m_tomTomKeyOn = true;
                break;
            
            case HiHatChannel:
                m_hiHatKeyOn = true;
                break;

            case CymbalChannel:
                m_cymbalKeyOn = true;
                break;

            default:
                return false;
        }

        return commitGlobalData(GlobalRegisterF);
    }
}

bool Hardware::keyOff(
    uint8_t channel)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    if (isPhysicalChannel(channel)) {
        m_channelParameters[channel].keyOn = false;
        return commitChannelData(channel, ChannelRegisterB);
    } else {
        switch (channel) {
            case KickChannel:
                m_kickKeyOn = false;
                break;

            case SnareChannel:
                m_snareKeyOn = false;
                break;

            case TomTomChannel:
                m_tomTomKeyOn = false;
                break;
            
            case HiHatChannel:
                m_hiHatKeyOn = false;
                break;

            case CymbalChannel:
                m_cymbalKeyOn = false;
                break;

            default:
                return false;
        }

        return commitGlobalData(GlobalRegisterF);
    }
}

#define SET_CHANNEL_VALUE(channel, realChannel, member, value, maxValue, channelRegister) \
    if ((!isAllocatedChannel(channel)) || (value > maxValue)) \
        return false; \
    m_channelParameters[realChannel].member = value; \
    return commitChannelData(realChannel, channelRegister);

bool Hardware::setOutput(
    uint8_t channel,
    uint8_t output)
{
    uint8_t realChannel;

    if (isPhysicalChannel(channel)) {
        realChannel = channel;
    } else {
        // TODO: Check these channels
        switch (channel) {
            case KickChannel:
                realChannel = 6;
                break;

            case SnareChannel:
                realChannel = 7;
                break;

            case TomTomChannel:
                realChannel = 8;
                break;

            case CymbalChannel:
                realChannel = 8;
                break;

            case HiHatChannel:
                realChannel = 7;
                break;

            default:
                return false;
        }
    }

    SET_CHANNEL_VALUE(channel, realChannel, output, output, 3, ChannelRegisterC);
}

bool Hardware::setFeedbackModulationFactor(
    uint8_t channel,
    uint8_t factor)
{
    // TODO: Determine if percussion channels support this
    if (!isPhysicalChannel(channel)) {
        return false;
    }

    SET_CHANNEL_VALUE(channel, channel, feedbackModulationFactor, factor, 7, ChannelRegisterC);
}

bool Hardware::setSynthType(
    uint8_t channel,
    uint8_t type)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    // TODO: Percussion?

    switch (getChannelType(channel)) {
        case Melody2OpChannelType:
            if (type > 1) {
                return false;
            }

            m_channelParameters[channel].synthType = type;
            return commitChannelData(channel, ChannelRegisterC);

            break;

        case Melody4OpChannelType:
            if (type > 3) {
                return false;
            }

            m_channelParameters[channel].synthType = type;
            return commitChannelData(channel, ChannelRegisterC);

            return false;

        default:
            return false;
    }
}

#define SET_OPERATOR_VALUE(channel, channelOperator, member, value, maxValue, operatorRegister) \
    uint8_t op; \
    if ((!isAllocatedChannel(channel)) || (value > maxValue)) \
        return false; \
    if ((op = getChannelOperator(channel, channelOperator)) == InvalidOperator) \
        return false; \
    m_operatorParameters[op].member = value; \
    return commitOperatorData(op, operatorRegister);

unsigned int Hardware::getOperatorCount(
    uint8_t channel) const
{
    if (!isValidChannel(channel)) {
        return 0;
    }

    switch (getChannelType(channel)) {
        case SnareChannelType:
        case TomTomChannelType:
        case CymbalChannelType:
        case HiHatChannelType:
            return 1;

        case Melody2OpChannelType:
        case KickChannelType:
            return 2;

        case Melody4OpChannelType:
            return 4;

        default:
            return 0;
    };
}

// TODO: Check if this is correct for percussion
OperatorType Hardware::getOperatorType(
    uint8_t channel,
    uint8_t channelOperator) const
{
    OperatorType type = NullOperatorType;

    if ((!isValidChannel(channel)) || (channelOperator >= getOperatorCount(channel))) {
        return NullOperatorType;
    }

    switch (getChannelType(channel)) {
        case SnareChannelType:
        case TomTomChannelType:
        case CymbalChannelType:
        case HiHatChannelType:
            if (channelOperator == 0) {
                type = CarrierOperatorType;
            }
            break;

        case Melody2OpChannelType:
        case KickChannelType:
            switch (m_channelParameters[channel].synthType) {
                case 0:
                    switch (channelOperator) {
                        case 0:
                            type = ModulatorOperatorType;
                            break;
                        case 1:
                            type = CarrierOperatorType;
                            break;
                    }
                    break;

                case 1:
                    type = CarrierOperatorType;
                    break;
            }
            break;

        case Melody4OpChannelType:
            switch (m_channelParameters[channel].synthType) {
                case 0:
                    switch (channelOperator) {
                        case 0:
                        case 1:
                        case 2:
                            type = ModulatorOperatorType;
                            break;

                        case 3:
                            type = CarrierOperatorType;
                            break;
                    }
                    break;

                case 1:
                    switch (channelOperator) {
                        case 0:
                        case 3:
                            type = CarrierOperatorType;
                            break;
                        case 1:
                        case 2:
                            type = ModulatorOperatorType;
                            break;
                    }
                    break;

                case 2:
                    switch (channelOperator) {
                        case 0:
                        case 2:
                            type = ModulatorOperatorType;
                            break;
                        case 1:
                        case 3:
                            type = CarrierOperatorType;
                            break;
                    }
                    break;

                case 3:
                    switch (channelOperator) {
                        case 0:
                        case 2:
                        case 3:
                            type = CarrierOperatorType;
                            break;
                        case 1:
                            type = ModulatorOperatorType;
                            break;
                    }
                    break;
            }
    };

    return type;
}

bool Hardware::setTremolo(
    uint8_t channel,
    uint8_t channelOperator,
    bool tremolo)
{
    SET_OPERATOR_VALUE(channel, channelOperator, tremolo, tremolo, 1, OperatorRegisterA);
}

bool Hardware::setVibrato(
    uint8_t channel,
    uint8_t channelOperator,
    bool vibrato)
{
    SET_OPERATOR_VALUE(channel, channelOperator, vibrato, vibrato, 1, OperatorRegisterA);
}

bool Hardware::setSustain(
    uint8_t channel,
    uint8_t channelOperator,
    bool sustain)
{
    SET_OPERATOR_VALUE(channel, channelOperator, sustain, sustain, 1, OperatorRegisterA);
}

bool Hardware::setEnvelopeScaling(
    uint8_t channel,
    uint8_t channelOperator,
    bool scaling)
{
    SET_OPERATOR_VALUE(channel, channelOperator, ksr, scaling, 1, OperatorRegisterA);
}

bool Hardware::setFrequencyMultiplicationFactor(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t factor)
{
    SET_OPERATOR_VALUE(channel, channelOperator, frequencyMultiplicationFactor, factor, 15, OperatorRegisterA);
}

bool Hardware::setAttackRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    SET_OPERATOR_VALUE(channel, channelOperator, attackRate, rate, 15, OperatorRegisterC);
}

bool Hardware::setDecayRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    SET_OPERATOR_VALUE(channel, channelOperator, decayRate, rate, 15, OperatorRegisterC);
}

bool Hardware::setSustainLevel(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t level)
{
    SET_OPERATOR_VALUE(channel, channelOperator, sustainLevel, level, 15, OperatorRegisterD);
}

bool Hardware::setReleaseRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    SET_OPERATOR_VALUE(channel, channelOperator, releaseRate, rate, 15, OperatorRegisterD);
}

bool Hardware::setKeyScaleLevel(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t level)
{
    SET_OPERATOR_VALUE(channel, channelOperator, keyScaleLevel, level, 3, OperatorRegisterB);
}

bool Hardware::setAttenuation(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t attenuation)
{
    SET_OPERATOR_VALUE(channel, channelOperator, attenuation, attenuation, 63, OperatorRegisterB);
}

bool Hardware::setWaveform(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t waveform)
{
    SET_OPERATOR_VALUE(channel, channelOperator, waveform, waveform, 7, OperatorRegisterE);
}

bool Hardware::isValidChannel(
    uint8_t channel) const
{
    return (channel <= MaxChannelNumber);
}

bool Hardware::isPhysicalChannel(
    uint8_t channel) const
{
    return (channel < 18);
}

bool Hardware::isAllocatedChannel(
    uint8_t channel) const
{
    return ((isValidChannel(channel)) &&
            (1L << channel & m_allocatedChannelBitmap));
}

uint8_t Hardware::shiftFreeChannel(
    uint8_t *list,
    uint8_t &freeCount)
{
    uint8_t channel = list[0];
    -- freeCount;

    for (int i = 0; i < freeCount; ++ i) {
        list[i] = list[i + 1];
    }

    return channel;
}

void Hardware::addFreeChannel(
    uint8_t *list,
    uint8_t &freeCount,
    uint8_t channel)
{
    list[freeCount ++] = channel;
}

void Hardware::removeFreeChannel(
    uint8_t *list,
    uint8_t &freeCount,
    uint8_t channel)
{
    int i = 0, j = 0;

    // Skip everything before the channel in the list
    for (i = 0; list[i] != channel; ++ i);

    // Shift the rest along by 1 element
    for (j = i + 1; j < freeCount; ++ j) {
        list[i ++] = list[j];
    }

    -- freeCount;
}

uint8_t Hardware::getChannelOperator(
    uint8_t channel,
    uint8_t operatorIndex) const
{
    ChannelType type;

    if (!isValidChannel(channel)) {
        return InvalidOperator;
    }

    if ((operatorIndex >= getOperatorCount(channel))) {
        return InvalidOperator;
    }

    type = getChannelType(channel);

    switch (channel) {
        case InvalidChannel:
            return InvalidOperator;

        case KickChannel:
            return (operatorIndex == 0) ? 12 : 15;

        case SnareChannel:
            return 16;

        case TomTomChannel:
            return 14;

        case CymbalChannel:
            return 17;

        case HiHatChannel:
            return 13;

        default:
            return ((channel / 3) * 6) + (channel % 3) + (operatorIndex * 3);
    };
}

bool Hardware::writeGlobalRegister(
    GlobalRegister reg,
    uint8_t data) const
{
    bool primary;

    switch (reg) {
        case GlobalRegisterA:
        case GlobalRegisterB:
        case GlobalRegisterC:
        case GlobalRegisterD:
        case GlobalRegisterE:
        case GlobalRegisterF:
        case GlobalRegisterG:
        case GlobalRegisterH:
            break;

        default:
            return false;
    };

    primary = !(reg & 0x0100);
    writeData(primary, reg & 0x00ff, data);

    return true;
}

bool Hardware::commitGlobalData(
    GlobalRegister reg) const
{
    if (reg == GlobalRegisterF) {
        return writeGlobalRegister(GlobalRegisterF,
                               0x20 | (m_kickKeyOn << 4)
                                    | (m_snareKeyOn << 3)
                                    | (m_tomTomKeyOn << 2)
                                    | (m_cymbalKeyOn << 1)
                                    | (m_hiHatKeyOn)
                                    | (m_tremoloDepth << 7)
                                    | (m_vibratoDepth << 6));
    } else {
        return false;
    }
}

bool Hardware::commitChannelData(
    uint8_t channel,
    ChannelRegister reg) const
{
    uint8_t data;

    if (!isPhysicalChannel(channel)) {
        return false;
    }

    switch (reg) {
        case ChannelRegisterA:
            data = m_channelParameters[channel].frequencyNumber & 0xff;
            break;

        case ChannelRegisterB:
            data = (m_channelParameters[channel].keyOn << 5)
                 | (m_channelParameters[channel].block << 2)
                 | (m_channelParameters[channel].frequencyNumber >> 8);
            break;

        case ChannelRegisterC:
            data = (m_channelParameters[channel].output << 4)
                 | (m_channelParameters[channel].feedbackModulationFactor << 1)
                 | (m_channelParameters[channel].synthType & 0x1);

            // In 4-op mode we need to write the high bit of synth type to channel+3
            // and also the output bits (which it seems are allowed to be different
            // but we'll keep them the same)
            if (getChannelType(channel) == Melody4OpChannelType) {
                uint8_t secondChannel = channel + 3;
                writeData(secondChannel < 9, reg | (secondChannel % 9),
                    (m_channelParameters[channel].output << 4) |
                    ((m_channelParameters[channel].synthType & 0x2) >> 1));
            }

            break;

        default:
            return false;
    }

    // Each OPL3 I/O address pair caters for half of the 18 channels.
    writeData(channel < 9, reg | (channel % 9), data);

    return true;
}

bool Hardware::commitOperatorData(
    uint8_t op,
    OperatorRegister reg) const
{
    uint8_t regOffset;
    uint8_t data;

    if (op > 36) {
        return false;
    }

    switch (reg) {
        case OperatorRegisterA:
            data = (m_operatorParameters[op].tremolo << 7)
                 | (m_operatorParameters[op].vibrato << 6)
                 | (m_operatorParameters[op].sustain << 5)
                 | (m_operatorParameters[op].ksr << 4)
                 | (m_operatorParameters[op].frequencyMultiplicationFactor);
            break;

        case OperatorRegisterB:
            data = (m_operatorParameters[op].keyScaleLevel << 6)
                 | (m_operatorParameters[op].attenuation);
            break;

        case OperatorRegisterC:
            data = (m_operatorParameters[op].attackRate << 4)
                 | (m_operatorParameters[op].decayRate & 0x0f);
            break;

        case OperatorRegisterD:
            data = (m_operatorParameters[op].sustainLevel << 4)
                 | (m_operatorParameters[op].releaseRate & 0x0f);
            break;

        case OperatorRegisterE:
            data = (m_operatorParameters[op].waveform);
            break;

        default:
            return false;
    }

    // Operators don't map to a contiguous set of registers, so here we work
    // out the offset to use. Similar to the channels, each OPL3 I/O address
    // pair covers half of the 36 operators.
    regOffset = op % 18;

    if (regOffset >= 12) {
        regOffset += 4;
    } else if (regOffset >= 6) {
        regOffset += 2;
    }

    writeData(op < 18, reg | regOffset, data);

    return true;
}

uint8_t Hardware::readStatus() const
{
    return m_isaBus.read(m_ioBaseAddress);
}

void Hardware::writeData(
    bool primaryRegisterSet,
    uint8_t reg,
    uint8_t data) const
{
    uint16_t address;

    if (primaryRegisterSet) {
        address = m_ioBaseAddress;
    } else {
        address = m_ioBaseAddress + 2;
    }

    m_isaBus.write(address, reg);
    m_isaBus.write(address + 1, data);
}

}
