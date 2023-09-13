/*
    Project:    Canyon
    Purpose:    MPU-401 UART I/O
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#include <arduino.h>
#include "MPU401.h"

MPU401::MPU401(
    const ISABus &isaBus)
: m_isaBus(isaBus), m_ioAddress(0)
{
}

void MPU401::init(
    uint16_t ioAddress)
{
    m_ioAddress = ioAddress;
}

bool MPU401::reset() const
{
    unsigned long startTime;
    delay(10);
    writeCommand(0xff);
    delay(10);

    startTime = millis();
    while (readData() != 0xfe) {
        if (millis() - startTime > 5000) {
            return false;
        }
    };

    writeCommand(0x3f);

    return true;
}

bool MPU401::canWrite() const
{
    return !(m_isaBus.read(m_ioAddress + 1) & 0x40);
}

bool MPU401::canRead() const
{
    return !(m_isaBus.read(m_ioAddress + 1) & 0x80);
}

void MPU401::waitWriteReady() const
{
    while (!canWrite()) {}
}

void MPU401::waitReadReady() const
{
    while (!canRead()) {}
}

void MPU401::writeCommand(
    uint8_t command) const
{
    waitWriteReady();
    m_isaBus.write(m_ioAddress + 1, command);
}

void MPU401::writeData(
    uint8_t data) const
{
    waitWriteReady();
    m_isaBus.write(m_ioAddress, data);
}

uint8_t MPU401::readData() const
{
    //waitReadReady();
    return m_isaBus.read(m_ioAddress);
}
