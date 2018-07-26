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

//ISAPlugAndPlay pnp(isaBus);

const uint8_t key[32] PROGMEM = {
    0xb1, 0xd8, 0x6c, 0x36, 0x9b, 0x4d, 0xa6, 0xd3,
    0x69, 0xb4, 0x5a, 0xad, 0xd6, 0xeb, 0x75, 0xba,
    0xdd, 0xee, 0xf7, 0x7b, 0x3d, 0x9e, 0xcf, 0x67,
    0x33, 0x19, 0x8c, 0x46, 0xa3, 0x51, 0xa8, 0x54
};

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
    Serial.begin(9600);

    delay(1000);
    Serial.println("Starting");
/*
    pnp.reset();
    pnp.sendKey(key);

    Serial.println(pnp.setReadAddress(0x0203));

    pnp.wake(0x81);
    pnp.selectLogicalDevice(0);
    pnp.deactivate();
    Serial.println(pnp.assignAddress(0, 0x0220));

    delay(1000);
*/
    
    pnp_write(0x02, 0x01);

    Serial.println("Sending key");
    isaBus.write(0x279, 0x00);
    isaBus.write(0x279, 0x00);
    for (i = 0; i < 32; ++ i) {
        isaBus.write(0x0279, key[i]);
        delay(1);
    }

    // Set read address 0x0203
    pnp_write(0x00, 0x203 >> 2);

    // Wake
    Serial.println("Wake");
    pnp_write(0x03, 0x81);

    // Select logical device 0
    pnp_write(0x07, 0x00);

    // Deactivate
    pnp_write(0x30, 0x00);

    // Assign address 0
    pnp_write(0x60, 0x02);
    pnp_write(0x61, 0x20);

    Serial.println("Reading return value");

    isaBus.write(0x0279, 0x60);
    delay(1);
    Serial.print(isaBus.read(0x0203), HEX);
    Serial.println("");

    isaBus.write(0x0279, 0x61);
    delay(1);
    Serial.print(isaBus.read(0x0203), HEX);
    Serial.println("");
    //Serial.println("--------------------------------------");
}

void loop()
{
    int i;
  
    for (i = 0; i < 256; ++ i) {
        //isaBus.write(0x279);
        Serial.print(isaBus.read(0x220), HEX);
        Serial.println("");
        delay(2000);
    }
}
