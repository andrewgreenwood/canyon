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

const uint8_t mpu401IntPin = 2;
const uint8_t isaInputPin = 3;
const uint8_t isaOutputPin = 4;
const uint8_t isaReadPin = 5;   // nc
const uint8_t isaLoadPin = 6;
const uint8_t isaClockPin = 7;
const uint8_t isaResetPin = 8;  // nc
const uint8_t isaWritePin = 9;  // nc

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLoadPin,
              isaWritePin, isaReadPin, isaResetPin);

OPL3SA opl3sa(isaBus);
MPU401 mpu401(isaBus);

volatile uint16_t mpu401Counter = 0;
volatile uint8_t buffer[512];
volatile int bufferIndex = 0;

void onMPU401Input()
{
    uint8_t data;

    digitalWrite(13, HIGH);
    while (mpu401.canRead()) {
        data = mpu401.readData();
        buffer[bufferIndex ++] = data;

        if (bufferIndex == 512) {
            bufferIndex = 0;
        }
        ++ mpu401Counter;
    }
    digitalWrite(13, LOW);

    /*
    Serial.println("MPU-401 input available");
    while (mpu401.canRead()) {
        digitalWrite(13, HIGH);
        Serial.println(mpu401.readData(), HEX);
        digitalWrite(13, LOW);
    }
    Serial.println("end of MPU-401 data");
    */
}

void setup()
{
    Serial.begin(9600);
    delay(1000);
    Serial.println(opl3sa.init(0x330, 5, 0x388));
    mpu401.init(0x330);
    Serial.println(mpu401.reset());

    pinMode(13, OUTPUT);

    pinMode(mpu401IntPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(mpu401IntPin), onMPU401Input, RISING);
}

/*
    This loop is quite primitive at the moment and just accepts incoming
    data from the MPU-401 UART port, displaying what it received.

    To aid with developing the rest of the software, initially it is just
    going to accept SysEx messages of the following format:

    F0 7D xa 0b 0d 0e F7

    Where 'x' is 0 for the primary register set (ports 0x338/0x339) and 1
    for the secondary register set (ports 0x33a/0x33b), 'a' and 'b' are each
    half of the register and 'c' and 'e' are each half of the data.
*/

void printHex(
    uint8_t data)
{
    if (data < 0x10) {
        Serial.print("0");
    }

    Serial.print(data, HEX);
}

uint8_t currentStatus = 0;
int length = 0;
int expectedLength = 0;
bool inSysEx = false;
uint8_t sysExData[7];

void handleSysEx()
{
    uint16_t basePort;
    uint8_t reg;
    uint8_t data;

    basePort = (sysExData[2] & 0x80) ? 0x38a : 0x388;
    reg = (sysExData[2] << 4) | sysExData[3];
    data = (sysExData[4] << 4) | sysExData[5];

    Serial.print("Writing: ");
    Serial.print(basePort, HEX);
    Serial.print(" ");
    Serial.print(reg, HEX);
    Serial.print(" ");
    Serial.println(data, HEX);

    isaBus.write(basePort, reg);
    delay(1);
    isaBus.write(basePort + 1, data);
    delay(1);
}

int bufferInputIndex = 0;

int col = 2;

void loop()
{
    //Serial.println(mpu401Counter);

    if (bufferInputIndex != bufferIndex) {
        Serial.println("--- mark ---");
    }

    while (bufferInputIndex != bufferIndex) {
        if (col == 0) {
            if (buffer[bufferInputIndex] < 0x80) {
                Serial.println("corrupt");
            }

            if (mpu401Counter > 512) {
                Serial.println("Buffer exceeded!");
            }

            //Serial.print(bufferInputIndex);
            //Serial.print(" - ");
        }

        //printHex(buffer[bufferInputIndex]);
        ++ col;
        if (col == 3) {
            //Serial.print('\n');
            col = 0;
        }

        if (++ bufferInputIndex > 511) {
            bufferInputIndex = 0;
        }

        -- mpu401Counter;
    }

    return;
    uint8_t data;
    uint8_t buffer[256];
    int bufferIndex = 0;

    if (!mpu401.canRead()) {
        return;
    }

    while (mpu401.canRead()) {
        digitalWrite(13, HIGH);
        buffer[bufferIndex ++] = mpu401.readData();
        digitalWrite(13, LOW);

        if (bufferIndex > 255) {
            Serial.println("Buffer overflow");
            return;
        }
    }

/*
    for (int i = 0; i < bufferIndex; ++ i) {
        printHex(buffer[i]);
    }

    Serial.println("");
*/

    for (int i = 0; i < bufferIndex; ++ i) {
        data = buffer[i];
    if (inSysEx) {
        if (data == 0xf7) {
            Serial.println("End of SysEx");
            inSysEx = false;
            handleSysEx();
            memset(sysExData, 0, 7);
        } else if (data >= 0x80) {
            Serial.println("SysEx interrupted!");
            inSysEx = false;
        }
    }

    if ((data & 0x80) && (!inSysEx)) {
        if (length < expectedLength) {
            Serial.println("Message too short!");
        }

        length = 0;
        currentStatus = data;

        switch (currentStatus & 0xf0) {
            case 0x80:
            case 0x90:
            case 0xa0:
            case 0xb0:
            case 0xe0:
                expectedLength = 3;
                break;

            case 0xc0:
            case 0xd0:
                expectedLength = 2;
                break;

            case 0xf0:
                switch (currentStatus) {
                    case 0xf0:
                        Serial.println("Start of SysEx");
                        inSysEx = true;
                        expectedLength = 0;
                        break;

                    case 0xf7:
                    case 0xfe:
                        expectedLength = 1;
                        break;
                    // ...
                }
                break;
        }

        printHex(data);
        Serial.print(' ');
    } else {
        printHex(data);
        Serial.print(' ');
    }

    if ((inSysEx) && (length < 8)) {
        sysExData[length] = data;
    }

    ++ length;

    if ((!inSysEx) && (length >= expectedLength)) {
        length = 0;
        expectedLength = 0;
        Serial.print('\n');
    }
    }
}
