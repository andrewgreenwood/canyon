/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

// This is very broken at the moment
#define USE_MPU401_INTERRUPTS

#include "ISAPlugAndPlay.h"
#include "ISABus.h"
#include "OPL3SA.h"
#include "MPU401.h"
#include "OPL3.h"
#include "MIDIBuffer.h"
#include "MIDI.h"
#include "OPL3Synth.h"
#include "ISRState.h"

const uint16_t mpu401IoBaseAddress  = 0x330;
const uint8_t  mpu401IRQ            = 5;
const uint16_t opl3IoBaseAddress    = 0x388;

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
OPL3 opl3(isaBus, opl3IoBaseAddress);
OPL3Synth opl3Synth(opl3);

MIDIBuffer midiBuffer;

typedef struct __attribute__((packed)) NoteData {
    unsigned midiChannel    : 4;
    unsigned opl3Channel    : 5;
    unsigned midiNote       : 7;
} NoteData;

NoteData g_playingNotes[OPL3_NUMBER_OF_CHANNELS];

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

    isrBegin();

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
            Serial.println("Not supported - ignoring");
            // Not a supported message - ignore it and wait for the next
            // status byte
            message.status = 0;
        } else if (length == expectedLength) {
            midiBuffer.put(message);

            // Keep the status byte for the next message
            length = 1;
        }
    }

    isrEnd();
}

uint8_t ch;

void setup()
{
    pinMode(3, OUTPUT); // Debugging LED

    Serial.begin(9600);
    Serial.println("Canyon\n------");

    Serial.print("Initialising OPL3SA... ");
    if (!opl3sa.init(mpu401IoBaseAddress, mpu401IRQ, opl3IoBaseAddress)) {
        Serial.println("Failed");
        for (;;) {}
    }
    Serial.println("Done");

    Serial.print("Initialising OPL3... ");
    if (!opl3.detect()) {
        Serial.println("Failed to detect");
        for (;;) {}
    }
    opl3.reset();
    Serial.println("Done");

    Serial.print("Initialising MPU-401... ");
    mpu401.init(mpu401IoBaseAddress);
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
    // 17 32 35
    #define TEST_CHANNEL 17
    #define TEST_OP1     32
    #define TEST_OP2     35

//    ch = opl3Synth.allocateChannel(Melody2OpChannelType);


    /*
    opl3.writeOperator(TEST_OP1, OPL3_OPERATOR_REGISTER_A, 0x21);
    opl3.writeOperator(TEST_OP1, OPL3_OPERATOR_REGISTER_B, 0x18);
    opl3.writeOperator(TEST_OP1, OPL3_OPERATOR_REGISTER_C, 0xf0);
    opl3.writeOperator(TEST_OP1, OPL3_OPERATOR_REGISTER_D, 0x77);

    opl3.writeOperator(TEST_OP2, OPL3_OPERATOR_REGISTER_A, 0x21);
    opl3.writeOperator(TEST_OP2, OPL3_OPERATOR_REGISTER_B, 0x10);
    opl3.writeOperator(TEST_OP2, OPL3_OPERATOR_REGISTER_C, 0xf0);
    opl3.writeOperator(TEST_OP2, OPL3_OPERATOR_REGISTER_D, 0x7f);

    opl3.writeChannel(TEST_CHANNEL, OPL3_CHANNEL_REGISTER_C, 0x30);
    */

    // panning - TODO
    for (int i = 0; i < OPL3_NUMBER_OF_CHANNELS; ++ i) {
        opl3.writeChannel(i, OPL3_CHANNEL_REGISTER_C, 0x30);
    }

    // Note allocation
    for (int i = 0; i < OPL3_NUMBER_OF_CHANNELS; ++ i) {
        g_playingNotes[i].midiChannel = 0;
        g_playingNotes[i].opl3Channel = 31; // Not a valid channel
        g_playingNotes[i].midiNote = 0;
    }

    Serial.println("\nReady!\n");
}

uint16_t frequencyToFnum(
    uint32_t freqHundredths,
    uint8_t block)
{
    if (freqHundredths < 3) {
        return 0;
    } else if (freqHundredths > 620544) {
        return 1023;
    }

    return (freqHundredths * (524288L >> block)) / 2485800;
}

uint8_t frequencyToBlock(
    uint32_t freqHundredths)
{
    // The maximum frequency doubles each time the block number increases.
    // We start with 48.48Hz and try to find the lowest block that can
    // represent the desired frequency, for the greatest accuracy.
    uint8_t block;
    uint32_t maxFreq = 4848;

    for (block = 0; block < 7; ++ block) {
        if (freqHundredths < maxFreq) {
            return block;
        }

        maxFreq <<= 1;
    }

    return block;
}

bool calcFrequencyAndBlock(
    uint8_t note,
    uint16_t *frequency,
    uint8_t *block)
{
    int8_t octave;
    uint32_t freqHundredth;
    uint32_t multiplier;

    // Frequencies in octave 0
    const uint16_t noteFrequencies[12] = {
        1635,
        1732,
        1835,
        1945,
        2060,
        2183,
        2312,
        2450,
        2596,
        2750,
        2914,
        3087
    };

    if (note > 115) {
        *frequency = 1023;
        *block = 7;
        return false;
    }

    octave = (note / 12) - 1;
    if (octave >= 0) {
        multiplier = 1 << octave;
        freqHundredth = noteFrequencies[note % 12] * multiplier;
    } else {
        freqHundredth = noteFrequencies[note % 12] / 2;
    }

    *block = frequencyToBlock(freqHundredth);
    *frequency = frequencyToFnum(freqHundredth, *block);

    return true;
}

uint16_t noData = 0;

void serviceMidiInput()
{
    bool hasData = false;
    struct MIDIMessage message;
    uint16_t fnum;
    uint8_t block;
    uint8_t byte_b0;

    int noteSlot = -1;
    uint8_t opl3Channel = OPL3_INVALID_CHANNEL;

    #ifndef USE_MPU401_INTERRUPTS
    digitalWrite(3, HIGH);
    if (mpu401.canRead()) {
        digitalWrite(3, LOW);
        noData = 0;
        receiveMpu401Data();
    }
    #endif

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
                noteSlot = -1;
                for (int i = 0; i < OPL3_NUMBER_OF_CHANNELS; ++ i) {
                    if (g_playingNotes[i].opl3Channel == 31) {
                        noteSlot = i;
                    }
                }

                if (noteSlot == -1) {
                    Serial.println("No free note slots!");
                    continue;
                }

                opl3Channel = opl3Synth.allocateChannel(Melody2OpChannelType);
                if (opl3Channel == OPL3_INVALID_CHANNEL) {
                    Serial.println("Cannot allocate OPL3 channel");
                    continue;
                }

                opl3Synth.enableSustain(opl3Channel, 0);
                opl3Synth.setFrequencyMultiplicationFactor(opl3Channel, 0, 1);
                opl3Synth.setAttenuation(opl3Channel, 0, 16);
                opl3Synth.setAttackRate(opl3Channel, 0, 15);
                opl3Synth.setDecayRate(opl3Channel, 0, 0);
                opl3Synth.setSustainLevel(opl3Channel, 0, 7);
                opl3Synth.setReleaseRate(opl3Channel, 0, 7);

                opl3Synth.enableSustain(opl3Channel, 1);
                opl3Synth.setFrequencyMultiplicationFactor(opl3Channel, 1, 1);
                opl3Synth.setAttenuation(opl3Channel, 1, 16);
                opl3Synth.setAttackRate(opl3Channel, 1, 15);
                opl3Synth.setDecayRate(opl3Channel, 1, 0);
                opl3Synth.setSustainLevel(opl3Channel, 1, 7);
                opl3Synth.setReleaseRate(opl3Channel, 1, 6);

                g_playingNotes[noteSlot].midiChannel = 0;
                g_playingNotes[noteSlot].opl3Channel = opl3Channel;
                g_playingNotes[noteSlot].midiNote = message.data[0];

                if (calcFrequencyAndBlock(message.data[0], &fnum, &block)) {
                    /*
                    Serial.print("Block ");
                    Serial.print(block, DEC);
                    Serial.print(" - FNum ");
                    Serial.println(fnum, DEC);
                    */
                    byte_b0 = 0x20 | (fnum >> 8) | (block << 2);
                }

                /*
                Serial.print("ON ");
                Serial.print(opl3Channel, DEC);
                Serial.print(" ");
                Serial.println(message.data[0], DEC);
                */
                opl3.writeChannel(opl3Channel, OPL3_CHANNEL_REGISTER_A, fnum);
                opl3.writeChannel(opl3Channel, OPL3_CHANNEL_REGISTER_B, byte_b0);
            } else if (message.status == 0x80) {
                noteSlot = -1;

                for (int i = 0; i < OPL3_NUMBER_OF_CHANNELS; ++ i) {
                    if ((g_playingNotes[i].midiNote == message.data[0])
                        && (g_playingNotes[i].opl3Channel != 31)) {
                        noteSlot = i;
                        break;
                    }
                }

                if (noteSlot == -1) {
                    Serial.println("Note slot not allocated");
                    continue;
                }

                opl3Channel = g_playingNotes[noteSlot].opl3Channel;
                if (opl3Channel == OPL3_INVALID_CHANNEL) {
                    Serial.println("Invalid OPL3 channel");
                    continue;
                }

                if (calcFrequencyAndBlock(message.data[0], &fnum, &block)) {
                    /*
                    Serial.print("OFF ");
                    Serial.print(opl3Channel, DEC);
                    Serial.print(" ");
                    Serial.println(message.data[0], DEC);
                    */
                    byte_b0 = (fnum >> 8) | (block << 2);
                    opl3.writeChannel(opl3Channel, OPL3_CHANNEL_REGISTER_B, byte_b0);
                } else {
                    Serial.println("Error in calcFrequencyAndBlock!");
                }

                opl3Synth.freeChannel(opl3Channel);

                // TODO: Shuffle along
                g_playingNotes[noteSlot].midiChannel = 0;
                g_playingNotes[noteSlot].opl3Channel = 31;
                g_playingNotes[noteSlot].midiNote = 0;
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
