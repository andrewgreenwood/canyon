/*
    Project:    Canyon
    Purpose:    OPL3 I/O
    Author:     Andrew Greenwood
    Date:       July 2018
*/

#ifdef ARDUINO
#include <arduino.h>
#endif

#include "OPL3.h"

OPL3::OPL3(
    ISABus &isaBus,
    uint16_t ioBaseAddress)
: m_isaBus(isaBus), m_ioBaseAddress(ioBaseAddress)
{
}

bool OPL3::detect() const
{
    uint8_t status;

    // Reset timers
    writeData(true, 0x04, 0x60);

    // Reset IRQ
    writeData(true, 0x04, 0x80);

    // Check status
    status = readStatus();
    if (status != 0) {
        return false;
    }

    // Set the timers
    writeData(true, 0x02, 0xff);

    // Unmask timer 1
    writeData(true, 0x04, 0x21);

    // Allow the timer to expire
#ifdef ARDUINO
    delayMicroseconds(80);
#endif

    // Re-check status
    status = readStatus();

    // Reset timers
    writeData(true, 0x04, 0x60);

    // Reset IRQ
    writeData(true, 0x04, 0x80);

    // Fail if OPL2 not detected
    if (status != 0xc0) {
        return false;
    }

    // Is this OPL3?
    return ((readStatus() & 0x06) == 0);
}

void OPL3::reset() const
{
    unsigned int i;

    // Disable waveform selection, reset test register
    writeData(true, 0x01, 0x00);

    // Stop all sounds
    for (i = 0; i < OPL3_NUMBER_OF_CHANNELS; ++ i) {
        writeChannel(i, OPL3_CHANNEL_REGISTER_B, 0x00);
    }

    for (i = 0; i < OPL3_NUMBER_OF_OPERATORS; ++ i) {
        writeOperator(i, OPL3_OPERATOR_REGISTER_B, 0x00);
        writeOperator(i, OPL3_OPERATOR_REGISTER_C, 0x00);
    }

    // TODO: Maybe initialise the other channel and operator registers?

    // Enable waveform selection
    writeData(true, 0x01, 0x20);

    // Disable keyboard split
    writeData(true, 0x08, 0x40);

    // Enable OPL3 mode
    writeData(false, 0x05, 0x01);
}

bool OPL3::writeGlobal(
    uint8_t registerId,
    uint8_t data) const
{
    switch (registerId) {
        case OPL3_GLOBAL_REGISTER_A:
        case OPL3_GLOBAL_REGISTER_B:
            break;

        default:
            return false;
    };

    // Global register A is 0x04. On the primary register set, this controls
    // timers (not exposed via this method). On the secondary register set,
    // they control 4-op channel mode which is what we want this to do.
    writeData(registerId != OPL3_GLOBAL_REGISTER_A,
              registerId, data);

    return true;
}

bool OPL3::writeChannel(
    uint8_t channelNumber,
    uint8_t registerId,
    uint8_t data) const
{
    if (channelNumber >= OPL3_NUMBER_OF_CHANNELS) {
        return false;
    }

    switch (registerId) {
        case OPL3_CHANNEL_REGISTER_A:
        case OPL3_CHANNEL_REGISTER_B:
        case OPL3_CHANNEL_REGISTER_C:
            break;

        default:
            return false;
    }

    writeData(channelNumber < OPL3_NUMBER_OF_CHANNELS / 2,
              registerId | (channelNumber % (OPL3_NUMBER_OF_CHANNELS / 2)),
              data);

    return true;
}

bool OPL3::writeOperator(
    uint8_t operatorNumber,
    uint8_t registerId,
    uint8_t data) const
{
    uint8_t registerOffset;

    if (operatorNumber >= OPL3_NUMBER_OF_OPERATORS) {
        return false;
    }

    switch (registerId) {
        case OPL3_OPERATOR_REGISTER_A:
        case OPL3_OPERATOR_REGISTER_B:
        case OPL3_OPERATOR_REGISTER_C:
        case OPL3_OPERATOR_REGISTER_D:
        case OPL3_OPERATOR_REGISTER_E:
            break;

        default:
            return false;
    }

    // Operators don't map to a contiguous set of registers, so here we work
    // out the offset to use
    registerOffset = operatorNumber % (OPL3_NUMBER_OF_OPERATORS / 2);

    if (registerOffset >= 12) {
        registerOffset += 4;
    } else if (registerOffset >= 6) {
        registerOffset += 2;
    }

    if (registerId == OPL3_OPERATOR_REGISTER_E) {
        Serial.println(registerId | registerOffset, HEX);
        Serial.println(data, HEX);
    }

    writeData(operatorNumber < OPL3_NUMBER_OF_OPERATORS / 2,
              registerId | registerOffset,
              data);

    return true;
}

uint8_t OPL3::readStatus() const
{
    return m_isaBus.read(m_ioBaseAddress);
}

void OPL3::writeData(
    bool primaryRegisterSet,
    uint8_t opl3Register,
    uint8_t data) const
{
    uint16_t address;

    if (primaryRegisterSet) {
        address = m_ioBaseAddress;
    } else {
        address = m_ioBaseAddress + 2;
    }

    m_isaBus.write(address, opl3Register);
    m_isaBus.write(address + 1, data);
}
