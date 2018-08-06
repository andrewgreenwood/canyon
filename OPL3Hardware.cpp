#ifdef ARDUINO
#include <arduino.h>
#else
#include <cstdio>
#endif

#include "OPL3Hardware.h"

namespace OPL3 {

uint16_t getFrequencyFnum(
    uint32_t frequencyHundredths,
    uint8_t block)
{
    if (frequencyHundredths < 3) {
        return 0;
    } else if (frequencyHundredths > 620544) {
        return 1023;
    }

    return (frequencyHundredths * (524288L >> block)) / 2485800;
}

uint8_t getFrequencyBlock(
    uint32_t frequencyHundredths)
{
    // The maximum frequency doubles each time the block number increases.
    // We start with 48.48Hz and try to find the lowest block that can
    // represent the desired frequency, for the greatest accuracy.
    uint8_t block;
    uint32_t maxFrequency = 4848;

    for (block = 0; block < 7; ++ block) {
        if (frequencyHundredths < maxFrequency) {
            return block;
        }

        maxFrequency <<= 1;
    }

    return block;
}

uint32_t getNoteFrequency(
    uint8_t note)
{
    int8_t octave;
    uint32_t multiplier;

    // Frequencies in octave 0
    const uint16_t noteFrequencies[12] = {
        1635,
        1732,
        1835,
        1945,
        2060,
        2183,
        2312,
        2450,
        2596,
        2750,
        2914,
        3087
    };

    if (note > 115) {
        note = 115;
    }

    octave = (note / 12) - 1;
    if (octave >= 0) {
        multiplier = 1 << octave;
        return noteFrequencies[note % 12] * multiplier;
    } else {
        return noteFrequencies[note % 12] / 2;
    }
}

Hardware::Hardware(
    ISABus &isaBus,
    uint16_t ioBaseAddress)
: m_isaBus(isaBus),
  m_ioBaseAddress(ioBaseAddress)
{
    uint8_t i;

    // TODO - init parameters
    //memset(m_operatorParameters, 0, sizeof(m_operatorParameters));

    //for (i = 0; i < 36; ++ i) {
        //m_operatorParameters
    //}
}

bool Hardware::detect() const
{
    uint8_t status;

    // Reset timers
    //writeData(true, 0x04, 0x60);
    writeGlobalRegister(GlobalRegisterD, 0x60);

    // Reset IRQ
    //writeData(true, 0x04, 0x80);
    writeGlobalRegister(GlobalRegisterD, 0x80);

    // Check status
    status = readStatus();
    if (status != 0) {
        return false;
    }

    // Set the timers
    //writeData(true, 0x02, 0xff);
    writeGlobalRegister(GlobalRegisterB, 0xff);

    // Unmask timer 1
    //writeData(true, 0x04, 0x21);
    writeGlobalRegister(GlobalRegisterD, 0x21);

    // Allow the timer to expire
#ifdef ARDUINO
    delayMicroseconds(80);
#endif

    // Re-check status
    status = readStatus();

    // Reset timers
    //writeData(true, 0x04, 0x60);
    writeGlobalRegister(GlobalRegisterD, 0x60);

    // Reset IRQ
    //writeData(true, 0x04, 0x80);
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

    // Disable waveform selection, reset test register
    //writeData(true, 0x01, 0x00);
    writeGlobalRegister(GlobalRegisterA, 0x00);

    // Stop all sounds
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

    for (i = 0; i < 36; ++ i) {
        m_operatorParameters[i].sustain = false;
        m_operatorParameters[i].ksr = 0;
        m_operatorParameters[i].frequencyMultiplicationFactor = 0;
        m_operatorParameters[i].keyScalingLevel = 0;
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

    // TODO: Maybe initialise the other channel and operator registers?

    // Enable waveform selection
    //writeData(true, 0x01, 0x20);
    writeGlobalRegister(GlobalRegisterA, 0x20);

    // Disable keyboard split
    //writeData(true, 0x08, 0x40);
    writeGlobalRegister(GlobalRegisterE, 0x40);

    // Enable OPL3 mode
    //writeData(false, 0x05, 0x01);
    writeGlobalRegister(GlobalRegisterH, 0x01);
}

bool Hardware::enablePercussion()
{
    uint8_t i;

    if (m_percussionMode) {
        return true;
    }

    // Cannot enable percussion mode while channels 6, 7 or 8 are in use.
    for (i = 6; i <= 8; ++ i) {
        if (isAllocatedChannel(i)) {
            return false;
        }
    }

    // Enable percussion mode
    if (writeGlobalRegister(GlobalRegisterF, 0x20)) {
        m_percussionMode = true;
    }

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

    if (writeGlobalRegister(GlobalRegisterF, 0x00)) {
        m_percussionMode = false;
    }

    return !m_percussionMode;
}

uint8_t Hardware::allocateChannel(
    ChannelType type)
{
    uint8_t channel;

    channel = findAvailableChannel(type);

    if (channel != InvalidChannel) {
       m_allocatedChannelBitmap |= 1L << channel;
    }

    return channel;
}

bool Hardware::freeChannel(
    uint8_t channel)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    if (getChannelType(channel) == NullChannelType) {
        return false;
    }

    if ((1L << channel & m_allocatedChannelBitmap) == 0) {
        return false;
    }

    m_allocatedChannelBitmap &= ~(1L << channel);

    return true;
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

        return writeGlobalRegister(GlobalRegisterF,
                                   0x20 | (m_kickKeyOn << 4)
                                        | (m_snareKeyOn << 3)
                                        | (m_tomTomKeyOn << 2)
                                        | (m_cymbalKeyOn << 1)
                                        | (m_hiHatKeyOn));
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

        return writeGlobalRegister(GlobalRegisterF,
                                   0x20 | (m_kickKeyOn << 4)
                                        | (m_snareKeyOn << 3)
                                        | (m_tomTomKeyOn << 2)
                                        | (m_cymbalKeyOn << 1)
                                        | (m_hiHatKeyOn));
    }
}

bool Hardware::setOutput(
    uint8_t channel,
    uint8_t output)
{
    uint8_t physicalChannel;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    if (output > 3) {
        return false;
    }

    // I don't think any percussion channels support this...
    if (!isPhysicalChannel(channel)) {
        return false;
    }

    m_channelParameters[channel].output = output;
    return commitChannelData(channel, ChannelRegisterC);
}

uint8_t Hardware::getOutput(
    uint8_t channel)
{
    if ((!isAllocatedChannel(channel)) || (!isPhysicalChannel(channel))) {
        return false;
    }

    return m_channelParameters[channel].output;
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
            if (type > 5) {
                return false;
            }

            m_channelParameters[channel].synthType = type;

            // TODO
            return false;

        default:
            return false;
    }
}

uint8_t Hardware::getSynthType(
    uint8_t channel) const
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    // TODO: Percussion

    return m_channelParameters[channel].synthType;
}

bool Hardware::setAttackRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    uint8_t op;
    uint8_t data;

    if ((!isAllocatedChannel(channel)) || (rate > 15)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    m_operatorParameters[op].attackRate = rate;

    return commitOperatorData(op, OperatorRegisterC);
}

uint8_t Hardware::getAttackRate(
    uint8_t channel,
    uint8_t channelOperator)
{
    uint8_t op;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    return m_operatorParameters[op].attackRate;
}

bool Hardware::setDecayRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    uint8_t op;
    uint8_t data;

    if ((!isAllocatedChannel(channel)) || (rate > 15)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    m_operatorParameters[op].decayRate = rate;

    return commitOperatorData(op, OperatorRegisterC);
}

uint8_t Hardware::getDecayRate(
    uint8_t channel,
    uint8_t channelOperator)
{
    uint8_t op;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    return m_operatorParameters[op].decayRate;
}

bool Hardware::setSustainLevel(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t level)
{
    uint8_t op;
    uint8_t data;

    if ((!isAllocatedChannel(channel)) || (level > 15)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    m_operatorParameters[op].sustainLevel = level;

    return commitOperatorData(op, OperatorRegisterD);
}

uint8_t Hardware::getSustainLevel(
    uint8_t channel,
    uint8_t channelOperator)
{
    uint8_t op;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    return m_operatorParameters[op].sustainLevel;
}

bool Hardware::setReleaseRate(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t rate)
{
    uint8_t op;
    uint8_t data;

    if ((!isAllocatedChannel(channel)) || (rate > 15)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    m_operatorParameters[op].releaseRate = rate;

    return commitOperatorData(op, OperatorRegisterD);
}

uint8_t Hardware::getReleaseRate(
    uint8_t channel,
    uint8_t channelOperator)
{
    uint8_t op;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    return m_operatorParameters[op].releaseRate;
}

bool Hardware::setAttenuation(
    uint8_t channel,
    uint8_t channelOperator,
    uint8_t attenuation)
{
    uint8_t op;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    op = getChannelOperator(channel, channelOperator);
    if (op == InvalidOperator) {
        return false;
    }

    m_operatorParameters[op].attenuation = attenuation;

    return commitOperatorData(op, OperatorRegisterB);
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

uint8_t Hardware::findAvailableChannel(
    ChannelType type) const
{
    const uint8_t *channelPriorities;
    int8_t i;
    uint8_t channel;

    switch (type) {
        case NullChannelType:
            return InvalidChannel;

        case Melody2OpChannelType:
            channelPriorities = melody2OpChannelPriority;
            break;

        case Melody4OpChannelType:
            channelPriorities = melody4OpChannelPriority;
            break;

        case KickChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(KickChannel))) {
                return KickChannel;
            }
            return InvalidChannel;

        case SnareChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(SnareChannel))) {
                return SnareChannel;
            }
            return InvalidChannel;

        case TomTomChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(TomTomChannel))) {
                return TomTomChannel;
            }
            return InvalidChannel;

        case CymbalChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(CymbalChannel))) {
                return CymbalChannel;
            }
            return InvalidChannel;

        case HiHatChannelType:
            if ((m_percussionMode) && (!isAllocatedChannel(HiHatChannel))) {
                return HiHatChannel;
            }
            return InvalidChannel;

        default:
            channelPriorities = (uint8_t *)0;
            break;
    };

    if (!channelPriorities) {
        return InvalidChannel;
    }

    for (i = 0; channelPriorities[i] != InvalidChannel; ++ i) {
        channel = channelPriorities[i];

        // Skip percussion channels if they are being used
        if ((channel >= 6) && (channel <= 8)) {
            if (m_percussionMode) {
                continue;
            }
        }

        if ((!isAllocatedChannel(channel)) && (getChannelType(channel) == type)) {
            return channel;
        }
    }

    return InvalidChannel;
}

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
                 | (m_channelParameters[channel].synthType & 0x1); // TODO: Check this is correct
            break;

        default:
            return false;
    }

    // Each OPL3 I/O address pair caters for half of the 18 channels.
    writeData(channel < 9, reg | (channel % 9), data);

    // TODO: If we're writing the synth type for a 4-operator melody channel,
    // one bit of it needs to be written to the second channel of the pair.

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
            data = (m_operatorParameters[op].sustain << 5)
                 | (m_operatorParameters[op].ksr << 4)
                 | (m_operatorParameters[op].frequencyMultiplicationFactor);
            break;

        case OperatorRegisterB:
            data = (m_operatorParameters[op].keyScalingLevel << 6)
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


#ifndef ARDUINO
#include "ISABus.h"
int main()
{
    ISABus bus;
    OPL3::Hardware device(bus, 0x388);

    uint8_t ch;

    device.init();

    device.enablePercussion();
    ch = device.allocateChannel(OPL3::CymbalChannelType);

    printf("Channel %d\n", ch);
    device.setAttackRate(ch, 0, 7);
    device.setDecayRate(ch, 0, 7);
    device.setAttackRate(ch, 1, 4);
    device.setSynthType(ch, 1);

    device.keyOn(ch);

    printf("%d\n", device.getAttackRate(ch, 0));

    printf("Device size == %d\n", sizeof(device));
}
#endif
