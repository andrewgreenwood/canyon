#include <arduino.h>
#include "ISAPlugAndPlay.h"
#include "OPL3SA.h"

// TODO: Move these into the main program
#define SOUNDBLASTER_IO_ADDRESS  0x220
#define MPU401_IO_ADDRESS        0x330
#define CONTROL_IO_ADDRESS       0x370
#define ADLIB_IO_ADDRESS         0x388
#define WSS_IO_ADDRESS           0x530

#define SOUNDBLASTER_IRQ         5
#define SOUNDBLASTER_DMA         1

#define WSS_IRQ                  5
#define WSS_DMA                  1

#define MPU401_IRQ               5


#define OPL3SA2_PM            0x01  // Power Management
#define OPL3SA2_SYS_CTRL      0x02  // System Control
#define OPL3SA2_IRQ_CONFIG    0x03  // IRQ Sharing
#define OPL3SA2_DMA_CONFIG    0x06  // DMA Sharing
#define OPL3SA2_MASTER_LEFT   0x07  // Left volume/enable
#define OPL3SA2_MASTER_RIGHT  0x08  // Right volume/enable
#define OPL3SA2_MIC           0x09  // Mic volume?
#define OPL3SA2_MISC          0x0a  // Misc (version etc.)
#define OPL3SA3_WIDE          0x14
#define OPL3SA3_BASS          0x15
#define OPL3SA3_TREBLE        0x16

#define VERSION_UNKNOWN       0
#define VERSION_YMF711        1
#define VERSION_YMF715        2
#define VERSION_YMF715B       3
#define VERSION_YMF715E       4

#define CHIPSET_UNKNOWN       0
#define CHIPSET_OPL3SA2       0
#define CHIPSET_OPL3SA3       1

// Power management modes
#define OPL3SA2_PM_MODE0      0x00
#define OPL3SA2_PM_MODE1      0x04
#define OPL3SA2_PM_MODE2      0x05
#define OPL3SA2_PM_MODE3      0x27

// IRQ configs
#define OPL3SA2_SHARED_IRQ    0x0d

// DMA configs
#define OPL3SA2_SHARED_DMA    0x03


OPL3SA::OPL3SA(
    const ISABus &isaBus)
: m_isaBus(isaBus)
{
}

bool OPL3SA::init(
    uint16_t mpu401Port,
    uint8_t mpu401IRQ,
    uint16_t adlibPort)
{
    ISAPlugAndPlay pnp(m_isaBus);

    const uint8_t key[32] = {
        0xb1, 0xd8, 0x6c, 0x36, 0x9b, 0x4d, 0xa6, 0xd3,
        0x69, 0xb4, 0x5a, 0xad, 0xd6, 0xeb, 0x75, 0xba,
        0xdd, 0xee, 0xf7, 0x7b, 0x3d, 0x9e, 0xcf, 0x67,
        0x33, 0x19, 0x8c, 0x46, 0xa3, 0x51, 0xa8, 0x54
    };

    pnp.reset();
    pnp.sendKey(key);
    pnp.setReadAddress(0x203);
    pnp.wake(0x81);

    pnp.selectLogicalDevice(0);
    pnp.deactivate();

    if (!pnp.assignAddress(0, SOUNDBLASTER_IO_ADDRESS)) {
        Serial.println("Failed to set SB IO address");
        return false;
    }

    if (!pnp.assignIRQ(2, SOUNDBLASTER_IRQ)) {
        Serial.println("Failed to set SB IRQ");
        return false;
    }

    if (!pnp.assignDMA(1, SOUNDBLASTER_DMA)) {
        Serial.println("Failed to set SB DMA");
        return false;
    }

    if (!pnp.assignAddress(0, WSS_IO_ADDRESS)) {
        Serial.println("Failed to set WSS IO address");
        return false;
    }

    if (!pnp.assignIRQ(0, WSS_IRQ)) {
        Serial.println("Failed to set WSS IRQ");
        return false;
    }

    if (!pnp.assignDMA(0, WSS_DMA)) {
        Serial.println("Failed to set WSS DMA");
        return false;
    }

    if (!pnp.assignAddress(2, adlibPort)) {
        Serial.println("Failed to set Adlib IO address");
        return false;
    }

    if (!pnp.assignAddress(3, mpu401Port)) {
        Serial.println("Failed to set MPU-401 IO address");
        return false;
    }

    if (!pnp.assignIRQ(4, mpu401IRQ)) {
        Serial.println("Failed to set MPU-401 IRQ");
        return false;
    }

    if (!pnp.assignAddress(4, CONTROL_IO_ADDRESS)) {
        Serial.println("Failed to set control IO address");
        return false;
    }

    pnp.activate();

    //Serial.print("OPL3SA2 version: ");
    //Serial.println(readControl(OPL3SA2_MISC), HEX);
    //Serial.println(readControl(OPL3SA2_MIC), HEX);

    // Full power
    writeControl(OPL3SA2_PM, OPL3SA2_PM_MODE0);
    writeControl(OPL3SA2_SYS_CTRL, 0x00);

    // Shared IRQ - FIXME: This assumes IRQ is shared
    writeControl(OPL3SA2_IRQ_CONFIG, OPL3SA2_SHARED_IRQ);

    // Shared DMA - FIXME: This assumes DMA is shared (we don't even use it)
    writeControl(OPL3SA2_DMA_CONFIG, OPL3SA2_SHARED_DMA);

    // Maximum volume
    writeControl(OPL3SA2_MASTER_LEFT, 0x00);
    writeControl(OPL3SA2_MASTER_RIGHT, 0x00);

    // Can we turn off the mic?
    writeControl(OPL3SA2_MIC, 0xff);

    // ???
    writeControl(OPL3SA2_MISC, 0x8f);

    //Serial.println(readControl(OPL3SA2_MISC), HEX);

    return true;
}

void OPL3SA::writeControl(
    uint8_t reg,
    uint8_t data)
{
    m_isaBus.write(CONTROL_IO_ADDRESS, reg);
    delay(100);
    m_isaBus.write(CONTROL_IO_ADDRESS + 1, data);
}

uint8_t OPL3SA::readControl(
    uint8_t reg)
{
    m_isaBus.write(CONTROL_IO_ADDRESS, reg);
    delay(100);
    return m_isaBus.read(CONTROL_IO_ADDRESS + 1);
}
