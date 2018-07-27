/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

#include "ISAPlugAndPlay.h"
#include "ISABus.h"
#include "OPL3SA.h"
#include "MPU401.h"

const uint8_t isaWritePin = 2;  // nc
const uint8_t isaInputPin = 3;
const uint8_t isaOutputPin = 4;
const uint8_t isaReadPin = 5;   // nc
const uint8_t isaLoadPin = 6;
const uint8_t isaClockPin = 7;
const uint8_t isaResetPin = 8;  // nc

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLoadPin,
              isaWritePin, isaReadPin, isaResetPin);

OPL3SA opl3sa(isaBus);
MPU401 mpu401(isaBus);

void setup()
{
    Serial.begin(9600);
    delay(1000);
    Serial.println(opl3sa.init(0x330, 5, 0x388));
    mpu401.init(0x330);
    Serial.println(mpu401.reset());
}

void loop()
{
}
