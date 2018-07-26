#include <arduino.h>
#include "ISAPlugAndPlay.h"

const uint16_t ISAPlugAndPlay::registerAddress = 0x279;
const uint16_t ISAPlugAndPlay::writeAddress    = 0xa79;
const uint16_t ISAPlugAndPlay::ioDelay         = 4;

ISAPlugAndPlay::ISAPlugAndPlay(
    const ISABus &isaBus)
: m_isaBus(isaBus), m_packedReadAddress(0)
{
    // FIXME: We hang if we do this, unsure why. Need to reset manually for now.
    //reset();
}

void ISAPlugAndPlay::reset() const
{
    write(0x02, 0x01);
}

void ISAPlugAndPlay::sendKey(
    const uint8_t *key) const
{
    m_isaBus.write(registerAddress, 0x00);
    m_isaBus.write(registerAddress, 0x00);

    for (int i = 0; i < 32; ++ i) {
        m_isaBus.write(registerAddress, key[i]);
    }
}

bool ISAPlugAndPlay::setReadAddress(
    uint16_t address)
{
    uint8_t encoded_address;

    if ((address < 0x0203) || (address >= 0x3ff) || ((address & 0x03) != 0x03)) {
        return false;
    }

    encoded_address = (address >> 2);

    write(0x00, encoded_address);

    m_packedReadAddress = encoded_address;

    return true;
}

void ISAPlugAndPlay::wake(
    uint8_t csn) const
{
    write(0x03, csn);
}

void ISAPlugAndPlay::selectLogicalDevice(
    uint8_t deviceId) const
{
    write(0x07, deviceId);
}

void ISAPlugAndPlay::activate() const
{
    write(0x30, 0x01);
}

void ISAPlugAndPlay::deactivate() const
{
    write(0x30, 0x00);
}

bool ISAPlugAndPlay::assignAddress(
    uint8_t index,
    uint16_t address) const
{
    uint8_t pnpRegister = 0x60 + (index << 1);
    uint8_t addressHigh = address >> 8;
    uint8_t addressLow = address;

    // TODO: Check args

    write(pnpRegister, addressHigh);
    write(pnpRegister + 1, addressLow);

    return ((read(pnpRegister) == addressHigh)
         && (read(pnpRegister + 1) == addressLow));
}

bool ISAPlugAndPlay::assignIRQ(
    uint8_t index,
    uint8_t irq) const
{
    uint8_t pnpRegister = 0x70 + index;

    // TODO: Check args

    write(pnpRegister, irq);

    return (read(pnpRegister) == irq);
}

bool ISAPlugAndPlay::assignDMA(
    uint8_t index,
    uint8_t dma) const
{
    uint8_t pnpRegister = 0x74 + index;

    // TODO: Check args

    write(pnpRegister, dma);

    return (read(pnpRegister) == dma);
}

void ISAPlugAndPlay::write(
    uint8_t pnpRegister,
    uint8_t data) const
{
    delayMicroseconds(ioDelay);
    m_isaBus.write(registerAddress, pnpRegister);
    delayMicroseconds(ioDelay);
    m_isaBus.write(writeAddress, data);
}

uint8_t ISAPlugAndPlay::read(
    uint8_t pnpRegister) const
{
    if (m_packedReadAddress == 0) {
        return 0;
    }

    delayMicroseconds(ioDelay);
    m_isaBus.write(registerAddress, pnpRegister);
    delayMicroseconds(ioDelay);
    return m_isaBus.read(getReadAddress());
}
