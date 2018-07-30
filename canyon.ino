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

#define MIDI_BUFFER_SIZE    128

volatile uint8_t midiBuffer[MIDI_BUFFER_SIZE];
volatile unsigned int midiBufferWriteIndex = 0;
volatile unsigned int midiBufferReadIndex = 0;
volatile unsigned int midiBufferUsage = 0;

unsigned int getAvailableMidiBufferSpace()
{
    return MIDI_BUFFER_SIZE - midiBufferUsage;
}

uint8_t getExpectedMidiMessageLength(
    uint8_t status)
{
    switch (status & 0xf0) {
        case 0xc0:
        case 0xd0:
            return 2;

        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
        case 0xe0:
            return 3;

        case 0xf6:
        case 0xf7:
        case 0xf8:
        case 0xfa:
        case 0xfb:
        case 0xfc:
        case 0xfe:
        case 0xff:
            return 1;

        case 0xf1:
        case 0xf3:
            return 2;

        case 0xf2:
            return 3;

        default:
            // Unsupported (ignore until next status)
            return 0;
    }
}

/*
    IRQ 5 is raised whenever MIDI data is ready on the MPU-401 UART port. This
    routine just buffers the incoming data.

    A small amount of buffer space is saved by maintaining a "running status"
    byte, where multiple data relating to the same status byte arrives (e.g.
    multiple control changes on the same channel).

    Long (i.e. System Exclusive) messages are ignored.

    Note that data is not validated here.
*/
void mpu401InterruptHandler()
{
    static uint8_t currentStatus = 0;
    uint8_t data;
    bool skip;

    while (mpu401.canRead()) {
        data = mpu401.readData();
        skip = false;

        digitalWrite(3, LOW);
        if (getAvailableMidiBufferSpace() < 1) {
            // Not enough room - ignore the message

            digitalWrite(3, HIGH);
            currentStatus = 0;
            skip = true;
        } else if (data & 0x80) {
            // Status byte
            if (data == 0xf0) {
                // Skip System Exclusive messagers
                currentStatus = 0;
                skip = true;
            } else if ((data == currentStatus) && (currentStatus < 0xf0)) {
                // The status byte can be inferred from the previous one
                // ("running status")
                skip = true;
            } else {
                currentStatus = data;
            }
        } else if (currentStatus == 0) {
            // Unknown status byte, so can't do anything meaningful with
            // the data
            skip = true;
        }

        if (!skip) {
            midiBuffer[midiBufferWriteIndex] = data;
            ++ midiBufferUsage;

            // Wrap around
            if (++ midiBufferWriteIndex == MIDI_BUFFER_SIZE) {
                midiBufferWriteIndex = 0;
            }
        }
    }

    digitalWrite(3, LOW);
}

bool midiDataAvailable()
{
    if (mpu401.canRead()) {
        noInterrupts();
        mpu401InterruptHandler();
        interrupts();
    }

    return (midiBufferUsage > 0);
}

uint8_t peekMidiByte()
{
    uint8_t data;

    if (!midiDataAvailable()) {
        return 0;
    }

    data = midiBuffer[midiBufferReadIndex];

    return data;
}

uint8_t readMidiByte()
{
    uint8_t data;

    data = peekMidiByte();

    // Wrap around
    if (++ midiBufferReadIndex == MIDI_BUFFER_SIZE) {
        midiBufferReadIndex = 0;
    }

    -- midiBufferUsage;

    return data;
}

typedef struct MidiMessage {
    uint8_t status;
    uint8_t data[2];
} MidiMessage;

struct MidiMessage getMidiMessage()
{
    static uint8_t currentStatus = 0;
    unsigned int expectedLength = 0;
    unsigned int length = 0;
    bool failed = false;

    MidiMessage message = {};

    while ((midiDataAvailable()) && (!failed)) {
        // Add missing status byte where multiple messages for the same
        // status byte were received (either the sender omitted them, or we
        // stripped them out ourselves).
        if (peekMidiByte() & 0x80) {
            currentStatus = readMidiByte();
        } else if (currentStatus == 0) {
            // Data byte for an unknown status - consume it and move on
            readMidiByte();
            continue;
        }

        // Is this message complete?
        expectedLength = getExpectedMidiMessageLength(currentStatus);
        if (midiBufferUsage + 1 < expectedLength) {
            //Serial.println("Incomplete message (1)");
            break;
        }

        // We know there is enough data in the buffer now, so we can proceed
        message.status = currentStatus;

        for (unsigned int i = 0; i < expectedLength - 1; ++ i) {
            if (!midiDataAvailable()) {
                Serial.println("Incomplete message (3)");
                failed = true;
                break;
            }

            // If there's a status byte now, drop this message (as it's clearly
            // incomplete/corrupt) and move on to the next
            if (peekMidiByte() & 0x80) {
                Serial.println("Incomplete message (2)");
                break;
            }

            message.data[i] = readMidiByte();
        }
    }

    if (failed) {
        message.status = 0;
        message.data[0] = 0;
        message.data[1] = 0;
    }

    return message;
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
    pinMode(mpu401IntPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(mpu401IntPin), mpu401InterruptHandler, RISING);

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


void serviceMidiInput()
{
    bool hasData = false;
    MidiMessage message;
    uint16_t fnum;
    uint8_t block;
    uint8_t byte_b0;

    while (midiDataAvailable()) {
        hasData = true;
        message = getMidiMessage();
        if (message.status > 0) {
            Serial.print(message.status, HEX);
            Serial.print(" ");
            Serial.print(message.data[0], HEX);
            Serial.print(" ");
            Serial.print(message.data[1], HEX);
            Serial.print("\n");

            if (message.status == 0x90) {
                calcFrequencyAndBlock(message.data[0], &fnum, &block);
                byte_b0 = 0x20 | (fnum >> 8) | (block << 2);

                opl3Write(0, 0xa0, fnum);
                opl3Write(0, 0xb0, byte_b0);
            } else if (message.status == 0x80) {
                calcFrequencyAndBlock(message.data[0], &fnum, &block);
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
