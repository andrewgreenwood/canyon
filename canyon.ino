/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

#include "ISAPlugAndPlay.h"
#include "ISABus.h"

const uint8_t isaWritePin = 2;  // nc
const uint8_t isaInputPin = 3;
const uint8_t isaOutputPin = 4;
const uint8_t isaReadPin = 5;   // nc
const uint8_t isaLoadPin = 6;
const uint8_t isaClockPin = 7;
const uint8_t isaResetPin = 8;  // nc

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLoadPin,
              isaWritePin, isaReadPin, isaResetPin);

ISAPlugAndPlay pnp(isaBus);


void pnp_write(
  byte reg,
  byte data)
{
  delayMicroseconds(4);
  isaBus.write(0x279, reg);
  delayMicroseconds(4);
  isaBus.write(0xa79, data);
}

void setup()
{
    int i;

    const uint8_t key[32] = {
        0xb1, 0xd8, 0x6c, 0x36, 0x9b, 0x4d, 0xa6, 0xd3,
        0x69, 0xb4, 0x5a, 0xad, 0xd6, 0xeb, 0x75, 0xba,
        0xdd, 0xee, 0xf7, 0x7b, 0x3d, 0x9e, 0xcf, 0x67,
        0x33, 0x19, 0x8c, 0x46, 0xa3, 0x51, 0xa8, 0x54
    };

    Serial.begin(9600);

    delay(1000);
    Serial.println("Starting");

    pnp.reset();
    pnp.sendKey(key);

    Serial.println(pnp.setReadAddress(0x0203));

    pnp.wake(0x81);
    pnp.selectLogicalDevice(0);
    pnp.deactivate();

    // Sound Blaster
    if (!pnp.assignAddress(0, 0x0220)) {
        Serial.println("Failed to assign SB address");
        return;
    }

    if (!pnp.assignIRQ(2, 5)) {
        Serial.println("Failed to assign SB IRQ");
        return;
    }

    if (!pnp.assignDMA(1, 1)) {
        Serial.println("Failed to assign SB DMA");
        return;
    }

    // Windows Sound System
    if (!pnp.assignAddress(0, 0x530)) { // TODO: check me
        Serial.println("Failed to assign WSS address");
        return;
    }

    if (!pnp.assignIRQ(0, 5)) {
        Serial.println("Failed to assign WSS IRQ");
        return;
    }

    if (!pnp.assignDMA(0, 1)) {
        Serial.println("Failed to assign WSS DMA");
        return;
    }

    // Adlib
    if (!pnp.assignAddress(2, 0x388)) {
        Serial.println("Failed to assign Adlib address");
        return;
    }

    // MPU-401
    if (!pnp.assignAddress(3, 0x330)) {
        Serial.println("Failed to assign MPU-401 address");
        return;
    }

    if (!pnp.assignAddress(4, 5)) {
        Serial.println("Failed to assign MPU-401 IRQ");
        return;
    }

    // Control
    if (!pnp.assignAddress(4, 0x370)) {
        Serial.println("Failed to assign control address");
        return;
    }

    pnp.activate();

    Serial.println("PnP setup complete");
}

void loop()
{
}
