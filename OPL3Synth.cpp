#ifndef ARDUINO
    #include <stdio.h>
#endif

#include "OPL3Synth.h"

OPL3Synth::OPL3Synth(
    OPL3 &opl3)
: m_channel0_4op(false), m_channel1_4op(false), m_channel2_4op(false),
  m_channel9_4op(false), m_channel10_4op(false), m_channel11_4op(false),
  m_percussionMode(false), m_kickKeyOn(false), m_snareKeyOn(false),
  m_tomTomKeyOn(false), m_cymbalKeyOn(false), m_hiHatKeyOn(false),
  m_opl3(opl3)
{
    unsigned int i;
    // TODO: Initialise operators

    for (i = 0; i <= OPL3_MAX_CHANNEL; ++ i) {
        m_channelBusy[i] = false;
    }
}

uint8_t OPL3Synth::allocateChannel(
    OPL3ChannelType type)
{
    uint8_t channel;

    channel = findAvailableChannel(type);

    if (channel != OPL3_INVALID_CHANNEL) {
        m_channelBusy[channel] = true;
    }

    return channel;
}

bool OPL3Synth::freeChannel(
    uint8_t channel)
{
    if (getChannelType(channel) == NullChannelType) {
        return false;
    }

    if (!m_channelBusy[channel]) {
        return false;
    }

    m_channelBusy[channel] = false;

    return true;
}

uint8_t OPL3Synth::findAvailableChannel(
    OPL3ChannelType type) const
{
    const uint8_t *channelPriorities;
    int8_t i;

    switch (type) {
        case NullChannelType:
            return OPL3_INVALID_CHANNEL;

        case Melody2OpChannelType:
            channelPriorities = melody2OpChannelPriority;
            break;

        case Melody4OpChannelType:
            channelPriorities = melody4OpChannelPriority;
            break;

        case KickChannelType:
            if ((m_percussionMode) && (!m_channelBusy[OPL3_KICK_CHANNEL])) {
                return OPL3_KICK_CHANNEL;
            }
            return OPL3_INVALID_CHANNEL;

        case SnareChannelType:
            if ((m_percussionMode) && (!m_channelBusy[OPL3_SNARE_CHANNEL])) {
                return OPL3_SNARE_CHANNEL;
            }
            return OPL3_INVALID_CHANNEL;

        case TomTomChannelType:
            if ((m_percussionMode) && (!m_channelBusy[OPL3_TOMTOM_CHANNEL])) {
                return OPL3_TOMTOM_CHANNEL;
            }
            return OPL3_INVALID_CHANNEL;

        case CymbalChannelType:
            if ((m_percussionMode) && (!m_channelBusy[OPL3_CYMBAL_CHANNEL])) {
                return OPL3_CYMBAL_CHANNEL;
            }
            return OPL3_INVALID_CHANNEL;

        case HiHatChannelType:
            if ((m_percussionMode) && (!m_channelBusy[OPL3_HIHAT_CHANNEL])) {
                return OPL3_HIHAT_CHANNEL;
            }
            return OPL3_INVALID_CHANNEL;

        default:
            channelPriorities = (uint8_t *)0;
            break;
    };

    if (!channelPriorities) {
        return OPL3_INVALID_CHANNEL;
    }

    for (i = 0; channelPriorities[i] != OPL3_INVALID_CHANNEL; ++ i) {
        // Skip percussion channels if they are being used
        if ((channelPriorities[i] >= 6) && (channelPriorities[i] <= 8)) {
            if (m_percussionMode) {
                continue;
            }
        }

        if ((!m_channelBusy[i]) && (getChannelType(i) == type)) {
            return i;
        }
    }

    return OPL3_INVALID_CHANNEL;
}


//void OPL3Synth::setFrequency

bool OPL3Synth::keyOn(
    uint8_t channel)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    return true;
}

bool OPL3Synth::keyOff(
    uint8_t channel)
{
    if (!isAllocatedChannel(channel)) {
        return false;
    }

    return true;
}

bool OPL3Synth::enableSustain(
    uint8_t channel,
    uint8_t operatorIndex)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    m_operators[channelOperator].enableSustain();

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_A);
}

bool OPL3Synth::disableSustain(
    uint8_t channel,
    uint8_t operatorIndex)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    m_operators[channelOperator].disableSustain();

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_A);
}

bool OPL3Synth::enableEnvelopeScaling(
    uint8_t channel,
    uint8_t operatorIndex)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    m_operators[channelOperator].enableEnvelopeScaling();

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_A);
}

bool OPL3Synth::disableEnvelopeScaling(
    uint8_t channel,
    uint8_t operatorIndex)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    m_operators[channelOperator].disableEnvelopeScaling();

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_A);
}

bool OPL3Synth::setFrequencyMultiplicationFactor(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t factor)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setFrequencyMultiplicationFactor(factor)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_B);
}

bool OPL3Synth::setAttenuation(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t attenuation)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setAttenuation(attenuation)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_B);
}

bool OPL3Synth::setAttackRate(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t rate)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setAttackRate(rate)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_C);
}

bool OPL3Synth::setDecayRate(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t rate)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setDecayRate(rate)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_C);
}

bool OPL3Synth::setSustainLevel(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t level)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setSustainLevel(level)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_D);
}

bool OPL3Synth::setReleaseRate(
    uint8_t channel,
    uint8_t operatorIndex,
    uint8_t rate)
{
    uint8_t channelOperator;

    if (!isAllocatedChannel(channel)) {
        return false;
    }

    channelOperator = getChannelOperator(channel, operatorIndex);
    if (channelOperator == OPL3_INVALID_OPERATOR) {
        return false;
    }

    if (!m_operators[channelOperator].setReleaseRate(rate)) {
        return false;
    }

    return updateOperator(channelOperator, OPL3_OPERATOR_REGISTER_D);
}

OPL3ChannelType OPL3Synth::getChannelType(
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

        case OPL3_KICK_CHANNEL:
            return m_percussionMode ? KickChannelType
                                    : NullChannelType;

        case OPL3_SNARE_CHANNEL:
            return m_percussionMode ? SnareChannelType
                                    : NullChannelType;

        case OPL3_TOMTOM_CHANNEL:
            return m_percussionMode ? TomTomChannelType
                                    : NullChannelType;

        case OPL3_CYMBAL_CHANNEL:
            return m_percussionMode ? CymbalChannelType
                                    : NullChannelType;

        case OPL3_HIHAT_CHANNEL:
            return m_percussionMode ? HiHatChannelType
                                    : NullChannelType;

        default:
            return NullChannelType;
    };
}

bool OPL3Synth::isValidChannel(
    uint8_t channel) const
{
    /* Out of range? */
    if (channel > OPL3_MAX_CHANNEL) {
        return false;
    }

    return true;
}

bool OPL3Synth::isAllocatedChannel(
    uint8_t channel) const
{
    return ((isValidChannel(channel)) && (m_channelBusy[channel]));
}

unsigned int OPL3Synth::getOperatorCount(
    uint8_t channel) const
{
    if (!isValidChannel(channel)) {
        return OPL3_INVALID_OPERATOR;
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

uint8_t OPL3Synth::getChannelOperator(
    uint8_t channel,
    uint8_t operatorIndex) const
{
    OPL3ChannelType type;

    if (!isValidChannel(channel)) {
        return OPL3_INVALID_OPERATOR;
    }

    if ((operatorIndex >= getOperatorCount(channel))) {
        return OPL3_INVALID_OPERATOR;
    }

    type = getChannelType(channel);

    switch (channel) {
        case OPL3_INVALID_CHANNEL:
            return OPL3_INVALID_OPERATOR;

        case OPL3_KICK_CHANNEL:
            return (operatorIndex == 0) ? 12 : 15;

        case OPL3_SNARE_CHANNEL:
            return 16;

        case OPL3_TOMTOM_CHANNEL:
            return 14;

        case OPL3_CYMBAL_CHANNEL:
            return 17;

        case OPL3_HIHAT_CHANNEL:
            return 13;

        default:
            return ((channel / 3) * 6) + (channel % 3) + (operatorIndex * 3);
    };
}

bool OPL3Synth::updateOperator(
    uint8_t operatorIndex,
    uint8_t registerId)
{
    uint8_t value = 0;
    OPL3Operator &op = m_operators[operatorIndex];

    switch (registerId) {
        case OPL3_OPERATOR_REGISTER_A:
            value = (op.isSustainEnabled() << 5)
                  | (op.isEnvelopeScalingEnabled() << 4)
                  | op.getFrequencyMultiplicationFactor();
            break;

        case OPL3_OPERATOR_REGISTER_B:
            value = (op.getKeyScalingLevel() << 7)
                  | op.getAttenuation();
            break;

        case OPL3_OPERATOR_REGISTER_C:
            value = (op.getAttackRate() << 4)
                  | op.getDecayRate();
            break;

        case OPL3_OPERATOR_REGISTER_D:
            value = (op.getSustainLevel() << 4)
                  | op.getReleaseRate();
            break;

        case OPL3_OPERATOR_REGISTER_E:
            //value = op.getWaveform();
            break;

        default:
            return false;
    }

    return m_opl3.writeOperator(operatorIndex, registerId, value);
}
