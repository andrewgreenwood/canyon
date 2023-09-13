/*
    Project:    Canyon
    Purpose:    ISA Plug and Play
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018

    The "Plug and Play ISA Specification" describes a method of allowing ISA
    expansion cards to have their hardware resources (I/O addresses, IRQs and
    DMA channels) configured via software.

    The implementation here is based upon the specification documented at the
    following web address:

    www.osdever.net/documents/PNP-ISA-v1.0a.pdf

    Ports 0x279 and 0xa79 are used by all ISA PnP devices. A key corresponding
    to the device to be configured is sent (via sendKey), which isolates that
    card so that we can talk only to it. A separate address is configured to
    receive data from the device (via setReadAddress). After this, devices can
    be enabled/disabled and resources can be assigned.
*/

#include <stdint.h>
#include "ISABus.h"

class ISAPlugAndPlay {
    public:
        ISAPlugAndPlay(
            const ISABus &isaBus);

        void reset() const;

        void sendKey(
            const uint8_t *key) const;

        bool setReadAddress(
            uint16_t address);

        void wake(
            uint8_t csn) const;

        void selectLogicalDevice(
            uint8_t deviceId) const;

        void activate() const;

        void deactivate() const;

        bool assignAddress(
            uint8_t index,
            uint16_t address) const;

        bool assignIRQ(
            uint8_t index,
            uint8_t irq) const;

        bool assignDMA(
            uint8_t index,
            uint8_t dma) const;

    private:
        void write(
            uint8_t pnpRegister,
            uint8_t data) const;

        uint8_t read(
            uint8_t pnpRegister) const;

        uint16_t getReadAddress() const
        {
            // The lowest 2 bits are fixed at 11
            return (m_packedReadAddress << 2) | 0x03;
        }

        const ISABus &m_isaBus;
        uint8_t m_packedReadAddress;

        static const uint16_t registerAddress;
        static const uint16_t writeAddress;
        static const uint16_t ioDelay;
};
