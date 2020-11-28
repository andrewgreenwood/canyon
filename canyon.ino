/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

// Undefine to use polling
#define USE_MPU401_INTERRUPTS

// Define this to have serial output
#define WITH_SERIAL

#include "ISAPlugAndPlay.h"
#include "ISABus.h"
#include "OPL3SA.h"
#include "MPU401.h"
#include "OPL3Hardware.h"
#include "MIDIBuffer.h"
#include "MIDI.h"
#include "ISRState.h"

const uint16_t mpu401IoBaseAddress  = 0x330;
const uint8_t  mpu401IRQ            = 5;
const uint16_t opl3IoBaseAddress    = 0x388;

const uint8_t mpu401IntPin = 2;
const uint8_t isaSlaveSelectPin = 3;
const uint8_t isaReadPin = 5;
const uint8_t isaLoadPin = 6;
const uint8_t isaLatchPin = 7;
const uint8_t isaResetPin = 8;
const uint8_t isaWritePin = 9;
const uint8_t diagnosticLedPin = 10;
const uint8_t isaOutputPin = 11;
const uint8_t isaInputPin = 12;
const uint8_t isaClockPin = 13;

int diagnosticLedBrightness = 0;
int diagnosticLedFrame = 0;

ISABus isaBus(isaOutputPin, isaInputPin, isaSlaveSelectPin, isaClockPin,
              isaLatchPin, isaLoadPin, isaWritePin, isaReadPin, isaResetPin);

OPL3SA opl3sa(isaBus);
MPU401 mpu401(isaBus);
OPL3::Hardware opl3(isaBus, opl3IoBaseAddress);

MIDIBuffer midiBuffer;

typedef struct __attribute__((packed)) NoteData {
    unsigned midiChannel    : 4;
    unsigned opl3Channel    : 5;
    unsigned midiNote       : 7;
} NoteData;

NoteData g_playingNotes[OPL3::NumberOfChannels];

/*
    IRQ 5 is raised whenever MIDI data is ready on the MPU-401 UART port. This
    routine just buffers the incoming data.
*/

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
            continue;
        } else {
            // Data byte
            message.data[length - 1] = data;

            ++ length;
        }

        expectedLength = getExpectedMidiMessageLength(message.status);

        if (expectedLength == 0) {
            #ifdef WITH_SERIAL
            Serial.println("Not supported - ignoring status:");
            Serial.println(message.status);
            #endif
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

/*
    If something goes wrong during startup, the diagnostic LED will flash
    a particular number of times to indicate which part of the program the
    failure occurred at.
*/

void fail(int count)
{
    digitalWrite(diagnosticLedPin, LOW);
    delay(1000);

    for (;;) {
        for (int i = 0; i < count; ++ i) {
            digitalWrite(diagnosticLedPin, HIGH);
            delay(250);
            digitalWrite(diagnosticLedPin, LOW);
            delay(250);
        }

        delay(1750);
    }
}

void setup()
{
    pinMode(diagnosticLedPin, OUTPUT);
    digitalWrite(diagnosticLedPin, HIGH);

#ifdef WITH_SERIAL
    Serial.begin(9600);
    Serial.println("Canyon\n------");
#endif

    isaBus.reset();

    Serial.print("Initialising OPL3SA... ");
    if (!opl3sa.init(mpu401IoBaseAddress, mpu401IRQ, opl3IoBaseAddress)) {
#ifdef WITH_SERIAL
        Serial.println("Failed");
#endif
        fail(1);
    }
#ifdef WITH_SERIAL
    Serial.println("Done");
    Serial.print("Initialising OPL3... ");
#endif
    if (!opl3.detect()) {
#ifdef WITH_SERIAL
        Serial.println("Failed to detect");
#endif
        fail(2);
    }
    opl3.init();
#ifdef WITH_SERIAL
    Serial.println("Done");
    Serial.print("Initialising MPU-401... ");
#endif
    mpu401.init(mpu401IoBaseAddress);
    if (!mpu401.reset()) {
#ifdef WITH_SERIAL
        Serial.println("Failed");
#endif
        fail(3);
    }
#ifdef WITH_SERIAL
    Serial.println("Done");
#endif

    // Set up interrupt handling for the MPU-401 now (after init)
#ifdef USE_MPU401_INTERRUPTS
#ifdef WITH_SERIAL
    Serial.println("Interrupt mode ENABLED");
#endif
    pinMode(mpu401IntPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(mpu401IntPin), receiveMpu401Data, RISING);
#else
#ifdef WITH_SERIAL
    Serial.println("Interrupt mode NOT ENABLED");
#endif
#endif

    // panning - TODO
    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        //opl3.setChannelRegister(i, OPL3::ChannelRegisterC, 0x30);
    }

    // Note allocation
    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        g_playingNotes[i].midiChannel = 0;
        g_playingNotes[i].opl3Channel = 31; // Not a valid channel
        g_playingNotes[i].midiNote = 0;
    }

#ifdef WITH_SERIAL
    Serial.println("\nReady!\n");
#endif

    digitalWrite(diagnosticLedPin, LOW);
}

void printMidiMessage(
    struct MIDIMessage &message)
{
#ifdef WITH_SERIAL
    Serial.print(message.status, HEX);
    Serial.print(" ");
    Serial.print(message.data[0], HEX);
    Serial.print(" ");
    Serial.print(message.data[1], HEX);
    Serial.print("\n");
#endif
}

void serviceMidiInput()
{
    struct MIDIMessage message;
    uint32_t freq;
    uint16_t fnum;
    uint8_t block;
    uint8_t byte_b0;
    uint8_t channel;

    int noteSlot = -1;
    uint8_t opl3Channel = OPL3::InvalidChannel;

    #ifndef USE_MPU401_INTERRUPTS
    if (mpu401.canRead()) {
        receiveMpu401Data();
    }
    #endif

    while (midiBuffer.hasContent()) {
        diagnosticLedBrightness = 0xff;

        if (midiBuffer.get(message)) {
            channel = message.status & 0x0f;

            if ((message.status & 0xf0) == 0x90) {
                noteSlot = -1;
                for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                    if (g_playingNotes[i].opl3Channel == 31) {
                        noteSlot = i;
                    }
                }

                if (noteSlot == -1) {
                    //Serial.println("No free note slots!");
                    continue;
                }

                opl3.enablePercussion();
                opl3Channel = opl3.allocateChannel(OPL3::Melody2OpChannelType);
                //Serial.println(opl3Channel);
                //opl3Channel = opl3.allocateChannel(OPL3::Melody2OpChannelType);
                if (opl3Channel == OPL3::InvalidChannel) {
                    //Serial.println("Cannot allocate OPL3 channel");
                    continue;
                }

                opl3.setOutput(opl3Channel, 3);

                opl3.setAttenuation(opl3Channel, 0, 16);

                opl3.setAttackRate(opl3Channel, 0, 15);
                opl3.setDecayRate(opl3Channel, 0, 0);
                opl3.setSustainLevel(opl3Channel, 0, 7);
                opl3.setReleaseRate(opl3Channel, 0, 7);

                opl3.setAttenuation(opl3Channel, 1, 0);

                opl3.setAttackRate(opl3Channel, 1, 15);
                opl3.setDecayRate(opl3Channel, 1, 0);
                opl3.setSustainLevel(opl3Channel, 1, 7);
                opl3.setReleaseRate(opl3Channel, 1, 7);

                opl3.setSynthType(opl3Channel, 0);

                /*
                opl3.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterA, 0x21);
                opl3.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterB, 0x10);
                opl3.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterC, 0xf0);
                opl3.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterD, 0x74);

                opl3.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterA, 0x21);
                opl3.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterB, 0x10);
                opl3.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterC, 0xf0);
                opl3.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterD, 0x74);
                */

                g_playingNotes[noteSlot].midiChannel = channel;
                g_playingNotes[noteSlot].opl3Channel = opl3Channel;
                g_playingNotes[noteSlot].midiNote = message.data[0];

                freq = OPL3::getNoteFrequency(message.data[0]);
                block = OPL3::getFrequencyBlock(freq);
                fnum = OPL3::getFrequencyFnum(freq, block);

                byte_b0 = 0x20 | (fnum >> 8) | (block << 2);

                opl3.setFrequency(opl3Channel, freq);
                opl3.keyOn(opl3Channel);
                //opl3.setChannelRegister(opl3Channel, OPL3::ChannelRegisterA, fnum);
                //opl3.setChannelRegister(opl3Channel, OPL3::ChannelRegisterB, byte_b0);
            } else if ((message.status & 0xf0) == 0x80) {
                noteSlot = -1;

                for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                    if ((g_playingNotes[i].midiNote == message.data[0])
                        && (g_playingNotes[i].midiChannel == channel)
                        && (g_playingNotes[i].opl3Channel != 31)) {
                        noteSlot = i;
                        break;
                    }
                }

                if (noteSlot == -1) {
                    //Serial.println("Note slot not allocated");
                    continue;
                }

                opl3Channel = g_playingNotes[noteSlot].opl3Channel;
                //Serial.println(opl3Channel);
                if (opl3Channel == OPL3::InvalidChannel) {
#ifdef WITH_SERIAL
                    Serial.println("Invalid OPL3 channel");
                    continue;
#endif
                }

                freq = OPL3::getNoteFrequency(message.data[0]);
                block = OPL3::getFrequencyBlock(freq);
                fnum = OPL3::getFrequencyFnum(freq, block);

                byte_b0 = (fnum >> 8) | (block << 2);
                //opl3.setChannelRegister(opl3Channel, OPL3::ChannelRegisterB, byte_b0);
                opl3.keyOff(opl3Channel);

                opl3.freeChannel(opl3Channel);

                // TODO: Shuffle along
                g_playingNotes[noteSlot].midiChannel = 0;
                g_playingNotes[noteSlot].opl3Channel = 31;
                g_playingNotes[noteSlot].midiNote = 0;
            } else if ((message.status & 0xf0) == 0xb0) {
                for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                    if ((g_playingNotes[i].midiChannel == channel)
                        && (g_playingNotes[i].opl3Channel != 31)) {
                        if (message.data[0] == 37) {
                            //opl3.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterE,
                            //                         message.data[1] / 16);
                        }
                    }
                }
            }
        }
    }
}

void loop()
{
    if (diagnosticLedBrightness > 0) {
        if (++ diagnosticLedFrame > 500) {
            analogWrite(diagnosticLedPin, -- diagnosticLedBrightness);
            diagnosticLedFrame = 0;
        }
    }

    serviceMidiInput();
}
