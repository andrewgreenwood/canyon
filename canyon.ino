/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

// This is very broken at the moment
//#define USE_MPU401_INTERRUPTS

#include "ISAPlugAndPlay.h"
#include "ISABus.h"
#include "OPL3SA.h"
#include "MPU401.h"
#include "MIDIBuffer.h"
#include "MIDI.h"

const uint8_t mpu401IntPin = 2;
const uint8_t isaReadPin = 5;
const uint8_t isaLoadPin = 6;
const uint8_t isaLatchPin = 7;
const uint8_t isaResetPin = 8;
const uint8_t isaWritePin = 9;
const uint8_t isaOutputPin = 11;
const uint8_t isaInputPin = 12;
const uint8_t isaClockPin = 13;

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLatchPin,
              isaLoadPin, isaWritePin, isaReadPin, isaResetPin);

OPL3SA opl3sa(isaBus);
MPU401 mpu401(isaBus);

MIDIBuffer midiBuffer;

/*
    IRQ 5 is raised whenever MIDI data is ready on the MPU-401 UART port. This
    routine just buffers the incoming data.
*/

volatile bool handlingMpu401Input = false;

void receiveMpu401Data()
{
    uint8_t data;
    uint16_t expectedLength;
    static uint8_t length = 0;

    static struct MIDIMessage message = {
        0, {0, 0}
    };

    /*
    noInterrupts();
    if (handlingMpu401Input) {
        // Don't enter this routine asynchronously if it's already running!
        interrupts();
        return;
    }
    handlingMpu401Input = true;
    interrupts();
    */

    while (mpu401.canRead()) {

        data = mpu401.readData();

        if (data & 0x80) {
            // Status byte
            message.status = data;
            length = 1;
        } else if (message.status == 0) {
            // Unknown status - skip
            digitalWrite(3, HIGH);
            continue;
        } else {
            // Data byte
            message.data[length - 1] = data;

            ++ length;
        }

        expectedLength = getExpectedMidiMessageLength(message.status);

        if (expectedLength == 0) {
            // Not a supported message - ignore it and wait for the next
            // status byte
            message.status = 0;
        } else if (length == expectedLength) {
            midiBuffer.put(message);

            // Keep the status byte for the next message
            length = 1;
        }
    }
/*
    noInterrupts();
    handlingMpu401Input = false;
    interrupts();
    */
}

void setup()
{
    pinMode(3, OUTPUT); // Debugging LED

    Serial.begin(9600);
    Serial.println("Canyon\n------");

    Serial.print("Initialising OPL3SA... ");
    if (!opl3sa.init(0x330, 5, 0x388)) {
        Serial.println("Failed");
        for (;;) {}
    }
    Serial.println("Done");

    Serial.print("Initialising OPL3... ");
    for (int i = 0; i <= 0xff; ++ i) {
        // Primary register set
        isaBus.write(0x388, i);
        isaBus.write(0x389, 0);

        // Secondary register set
        isaBus.write(0x38a, i);
        isaBus.write(0x38b, 0);
    }
    Serial.println("Done");

    Serial.print("Initialising MPU-401... ");
    mpu401.init(0x330);
    if (!mpu401.reset()) {
        Serial.println("Failed");
        for (;;) {}
    }
    Serial.println("Done");

    // Set up interrupt handling for the MPU-401 now (after init)
    #ifdef USE_MPU401_INTERRUPTS
    Serial.println("Interrupt mode ENABLED");
    pinMode(mpu401IntPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(mpu401IntPin), receiveMpu401Data, RISING);
    #else
    Serial.println("Interrupt mode NOT ENABLED");
    #endif

    // Set up some initial sound to play with
    opl3Write(true, 0x20, 0x21);
    opl3Write(true, 0x40, 0x18);
    opl3Write(true, 0x60, 0xf0);   // Attack/Decay
    opl3Write(true, 0x80, 0x77);   // Sustain/Release
    opl3Write(true, 0xa0, 0x98);
    opl3Write(true, 0x23, 0x21);
    opl3Write(true, 0x43, 0x00);
    opl3Write(true, 0x63, 0xf4);   // Attack/Decay
    opl3Write(true, 0x83, 0x7f);   // Sustain/Release

    Serial.println("\nReady!\n");
}


// Frequencies of notes within each block
const uint16_t freqBlockTable[12] = {
    0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287
};

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


uint16_t noData = 0;

void serviceMidiInput()
{
    bool hasData = false;
    struct MIDIMessage message;
    uint16_t fnum;
    uint8_t block;
    uint8_t byte_b0;

    #ifndef USE_MPU401_INTERRUPTS
    digitalWrite(3, HIGH);
    if (mpu401.canRead()) {
        digitalWrite(3, LOW);
        noData = 0;
        receiveMpu401Data();
    }
    #endif

    /*
    if (noData > 50000) {
        Serial.println("HANG?");
        Serial.print("Status = ");
        Serial.println(isaBus.read(0x331), HEX);
        Serial.print("Data = ");
        Serial.println(isaBus.read(0x330), HEX);
        Serial.print("Last message = ");
        Serial.print(message.status, HEX);
        Serial.print(",");
        Serial.print(message.data[0], HEX);
        Serial.print(",");
        Serial.println(message.data[1], HEX);
        noData = 0;
    }

    if (!midiBuffer.hasContent()) {
        ++ noData;
    }
    */

    while (midiBuffer.hasContent()) {
        noData = 0;
        hasData = true;
        /*
        Serial.print(midiBuffer.usage());
        Serial.print(" -- ");
        */
        if (midiBuffer.get(message)) {
            /*
            Serial.print(message.status, HEX);
            Serial.print(" ");
            Serial.print(message.data[0], HEX);
            Serial.print(" ");
            Serial.print(message.data[1], HEX);
            Serial.print("\n");
            */

            if (message.status == 0x90) {
                calcFrequencyAndBlock(message.data[0] - 12, &fnum, &block);
                byte_b0 = 0x20 | (fnum >> 8) | (block << 2);

                opl3Write(0, 0xa0, fnum);
                opl3Write(0, 0xb0, byte_b0);
            } else if (message.status == 0x80) {
                calcFrequencyAndBlock(message.data[0] - 12, &fnum, &block);
                byte_b0 = (fnum >> 8) | (block << 2);
                opl3Write(0, 0xb0, byte_b0);
            }
        }
        //Serial.print(getMidiMessage(), HEX);
    }

    if (hasData) {
//        Serial.println("");
    }

}

void loop()
{
    serviceMidiInput();
}
