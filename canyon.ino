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
const uint8_t isaInputPin = 12;
const uint8_t isaOutputPin = 11;
const uint8_t isaReadPin = 5;   // nc
const uint8_t isaLoadPin = 6;
const uint8_t isaClockPin = 13;
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

    while (mpu401.canRead()) {
        data = mpu401.readData();
        buffer[bufferIndex ++] = data;

        if (bufferIndex == 512) {
            bufferIndex = 0;
        }
        ++ mpu401Counter;
    }
}

// Frequencies of notes within each block
const uint16_t freqBlockTable[12] = {
    0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287
};

const uint16_t lowestFrequency = 0x156;
const uint16_t highestFrequency = 0x2ae;

void calcFrequencyAndBlock(
    uint8_t note,
    uint16_t *frequency,
    uint8_t *block)
{
    // Maximum note
    if (note > 95) {
        return;
    }

    *block = note / 12;
    *frequency = freqBlockTable[note % 12];
}

void opl3Write(
    bool primary,
    uint8_t reg,
    uint8_t data)
{
    uint16_t basePort = primary ? 0x388 : 0x38a;

    isaBus.write(basePort, reg);
    isaBus.write(basePort + 1, data);
}

#include <SPI.h>

void setup()
{
    Serial.begin(9600);

    uint8_t data;

    pinMode(7, OUTPUT);
    #if 0
    pinMode(13, OUTPUT);
    for (;;) {
        SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));

        //SPI.transfer(0xab);
        SPI.transfer(0xfe);
        SPI.transfer(0xfe);
        //SPI.endTransaction();
        //SPI.endTransaction();

        // Store
        //pinMode(7, OUTPUT);
        digitalWrite(7, LOW);
        digitalWrite(7, HIGH);

        // Set bidirectional register to LOAD
        digitalWrite(isaLoadPin, HIGH);

        // Trigger the read
        digitalWrite(isaReadPin, LOW);

        delay(10);

        // Clock in the data
        //pinMode(isaClockPin, OUTPUT);
//        digitalWrite(isaClockPin, LOW);
//        digitalWrite(isaClockPin, HIGH);
        //SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
        SPI.transfer(0);
        //SPI.endTransaction();

        // End reading
        digitalWrite(isaReadPin, HIGH);

        // Set bidirectional register to SHIFT
        digitalWrite(isaLoadPin, LOW);

        /*
        data = digitalRead(isaInputPin);
        // Shift the remaining 7 bits in
        for (int i = 1; i < 8; ++ i) {
            digitalWrite(isaClockPin, LOW);
            digitalWrite(isaClockPin, HIGH);
            data |= digitalRead(isaInputPin) << i;
        }
        Serial.println(data, HEX);
        */

        // Leave the clock pin in a low state
        //digitalWrite(isaClockPin, LOW);

        //SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
        for (int j = 0; j < 4; ++ j) {
            Serial.print(SPI.transfer(0), HEX);
            Serial.print(' ');
        }
        SPI.endTransaction();

        /*
        for (int j = 0; j < 256; ++ j) {
            Serial.println(j);
            isaBus.write(j << 8, 0);
        }
        */
        Serial.print('\n');
        delay(1000);
    }

    return; // TODO: REMOVE ME
    #endif

    pinMode(13, OUTPUT);
    Serial.println(opl3sa.init(0x330, 5, 0x388));

    Serial.println("Initialising OPL3");
    for (int i = 0; i <= 0xff; ++ i) {
        isaBus.write(0x388, i);
        isaBus.write(0x389, 0);
        isaBus.write(0x38a, i);
        isaBus.write(0x38b, 0);
    }

    opl3Write(true, 0x20, 0x01);
    opl3Write(true, 0x40, 0x18);
    opl3Write(true, 0x60, 0xf0);   // Attack/Decay
    opl3Write(true, 0x80, 0x77);   // Sustain/Release
    opl3Write(true, 0xa0, 0x98);
    opl3Write(true, 0x23, 0x01);
    opl3Write(true, 0x43, 0x00);
    opl3Write(true, 0x63, 0xf0);   // Attack/Decay

    Serial.println("Done initialising OPL3");

    mpu401.init(0x330);
    Serial.println(mpu401.reset());

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

uint8_t sysExData[7];

void handleSysEx()
{
    uint16_t basePort;
    uint8_t reg;
    uint8_t data;

    basePort = (sysExData[2] & 0x80) ? 0x38a : 0x388;
    reg = (sysExData[2] << 4) | sysExData[3];
    data = (sysExData[4] << 4) | sysExData[5];
/*
    Serial.print("Writing: ");
    Serial.print(basePort, HEX);
    Serial.print(" ");
    Serial.print(reg, HEX);
    Serial.print(" ");
    Serial.println(data, HEX);
*/
    isaBus.write(basePort, reg);
    //delay(1);
    isaBus.write(basePort + 1, data);
    //delay(1);
}

int bufferInputIndex = 0;

bool inSysEx = false;
int midiIndex = 0;

uint8_t note = 0;
int counter = 0;
unsigned long startTime, endTime;

void old_loop()
{
    startTime = micros();
    isaBus.write(0x388, 01);
    endTime = micros();

    Serial.println(endTime - startTime);

    delay(100);
}

void loop()
{
    uint8_t data;
    
    uint16_t fnum;
    uint8_t block;
    uint8_t byte_b0;

    startTime = micros();

    for (note = 0; note < 96; ++ note) {
    calcFrequencyAndBlock(note, &fnum, &block);

    byte_b0 = 0x20 | (fnum >> 8) | (block << 2);

    opl3Write(true, 0xa0, fnum);
    opl3Write(true, 0xb0, byte_b0);

    //opl3Write(0, 0xa0, pitch);
    //opl3Write(true, 0x8f, 0x41);   // Sustain/Release (carrier)
    }
    
    endTime = micros();

    Serial.println((endTime - startTime));
    
    //byte_b0 &= 0x1f;
    //opl3Write(0, 0xb0, byte_b0);

    /*
    if (counter % 6) {
        delay(50);
    }

    if (++ counter == 100) {
        delay(100);
        counter = 0;
    }
    */

    return;

    while (bufferInputIndex != bufferIndex) {
        digitalWrite(13, HIGH);
        data = buffer[bufferInputIndex];
        if (data == 0xf0) {
            inSysEx = true;
            midiIndex = 0;
        } else if (data == 0xf7) {
            inSysEx = false;
            handleSysEx();
        }

        if (midiIndex < 8) {
            sysExData[midiIndex] = data;
        }

        if (inSysEx) {
            ++ midiIndex;
        }

        /*
        if (col == 0) {
            if (buffer[bufferInputIndex] < 0x80) {
                Serial.println("corrupt");
            }

            //Serial.print(bufferInputIndex);
            //Serial.print(" - ");
        }

        printHex(buffer[bufferInputIndex]);
        ++ col;
        if (col == 3) {
            Serial.print('\n');
            col = 0;
        }
        */

        if (++ bufferInputIndex > 511) {
            bufferInputIndex = 0;
        }

        if (mpu401Counter >= 512) {
            Serial.println("Buffer exceeded!");
        }

        -- mpu401Counter;
    }
    digitalWrite(13, LOW);

    return;
#if 0
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
#endif
}
