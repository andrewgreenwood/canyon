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
  m_ioBaseAddress(ioBaseAddress),
  m_allocatedChannelBitmap(0),
  m_channel0_4op(false), m_channel1_4op(false), m_channel2_4op(false),
  m_channel9_4op(false), m_channel10_4op(false), m_channel11_4op(false),
  m_percussionMode(false), m_kickKeyOn(false), m_snareKeyOn(false),
  m_tomTomKeyOn(false), m_cymbalKeyOn(false), m_hiHatKeyOn(false)
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

void Hardware::reset() const
{    unsigned int i;

    // Disable waveform selection, reset test register
    //writeData(true, 0x01, 0x00);
    writeGlobalRegister(GlobalRegisterA, 0x00);

    // Stop all sounds
    for (i = 0; i < 18; ++ i) {
        writeChannelRegister(i, ChannelRegisterB, 0x00);
    }

    for (i = 0; i < 36; ++ i) {
        writeOperatorRegister(i, OperatorRegisterB, 0x00);
        writeOperatorRegister(i, OperatorRegisterC, 0x00);
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

    data = (rate << 4) | (m_operatorParameters[op].decayRate & 0x0f);

    return writeOperatorRegister(op, OperatorRegisterC, data);
}

uint8_t Hardware::getAttackRate(
    uint8_t channel,
    uint8_t channelOperator)
{
}

bool Hardware::setChannelRegister(
    uint8_t channel,
    ChannelRegister reg,
    uint8_t data) const
{
    ChannelType type;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    type = getChannelType(channel);

    switch (type) {
        case NullChannelType:
            return false;

        case Melody2OpChannelType:
        case Melody4OpChannelType:
            return writeChannelRegister(channel, reg, data);

        case KickChannelType:
        case SnareChannelType:
        case TomTomChannelType:
        case CymbalChannelType:
        case HiHatChannelType:
            // TODO - mask the key-on bit, set f-num where relevant (need
            // to look into this more).
            return false;

        default:
            return false;
    }
}

bool Hardware::setOperatorRegister(
    uint8_t channel,
    uint8_t operatorIndex,
    OperatorRegister reg,
    uint8_t data) const
{
    uint8_t actualOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    actualOperator = getChannelOperator(channel, operatorIndex);
    if (actualOperator == InvalidOperator) {
        return false;
    }

    return writeOperatorRegister(actualOperator, reg, data);
}

ChannelType Hardware::getChannelType(
    uint8_t channel) const
{
    if (!isValidChannel(channel)) {
        return NullChannelType;
    }

    switch (channel) {
        case 0:
            return m_channel0_4op ? Melody4OpChannelType
                                  : Melody2OpChannelType;

        case 1:
            return m_channel1_4op ? Melody4OpChannelType
                                  : Melody2OpChannelType;

        case 2:
            return m_channel2_4op ? Melody4OpChannelType
                                  : Melody2OpChannelType;

        case 3:
            return m_channel0_4op ? NullChannelType
                                  : Melody2OpChannelType;

        case 4:
            return m_channel1_4op ? NullChannelType
                                  : Melody2OpChannelType;

        case 5:
            return m_channel1_4op ? NullChannelType
                                  : Melody2OpChannelType;

        case 6:
        case 7:
        case 8:
            return m_percussionMode ? NullChannelType
                                    : Melody2OpChannelType;

        case 9:
            return m_channel9_4op ? Melody4OpChannelType
                                  : Melody2OpChannelType;

        case 10:
            return m_channel10_4op ? Melody4OpChannelType
                                   : Melody2OpChannelType;

        case 11:
            return m_channel11_4op ? Melody4OpChannelType
                                   : Melody2OpChannelType;

        case 12:
            return m_channel9_4op ? NullChannelType
                                  : Melody2OpChannelType;

        case 13:
            return m_channel10_4op ? NullChannelType
                                   : Melody2OpChannelType;

        case 14:
            return m_channel11_4op ? NullChannelType
                                   : Melody2OpChannelType;

        case 15:
        case 16:
        case 17:
            return Melody2OpChannelType;

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
    };
}

bool Hardware::isValidChannel(
    uint8_t channel) const
{
    return (channel <= MaxChannelNumber);
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
        // Skip percussion channels if they are being used
        if ((channelPriorities[i] >= 6) && (channelPriorities[i] <= 8)) {
            if (m_percussionMode) {
                continue;
            }
        }

        if ((!isAllocatedChannel(i)) && (getChannelType(i) == type)) {
            return i;
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

bool Hardware::writeChannelRegister(
    uint8_t channel,
    ChannelRegister reg,
    uint8_t data) const
{
    if (channel >= 18) {
        return false;
    }

    switch (reg) {
        case ChannelRegisterA:
        case ChannelRegisterB:
        case ChannelRegisterC:
            break;

        default:
            return false;
    }

    // Each OPL3 I/O address pair caters for half of the 18 channels.
    writeData(channel < 9, reg | (channel % 9), data);

    return true;
}

bool Hardware::writeOperatorRegister(
    uint8_t op,
    OperatorRegister reg,
    uint8_t data) const
{
    uint8_t regOffset;

    if (op > 36) {
        return false;
    }

    switch (reg) {
        case OperatorRegisterA:
        case OperatorRegisterB:
        case OperatorRegisterC:
        case OperatorRegisterD:
        case OperatorRegisterE:
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


#include "ISABus.h"
int main()
{
    ISABus bus;
    OPL3::Hardware device(bus, 0x388);

    uint8_t ch;
    ch = device.allocateChannel(OPL3::Melody2OpChannelType);

    printf("Channel %d\n", ch);
    device.setAttackRate(ch, 0, 7);
}

