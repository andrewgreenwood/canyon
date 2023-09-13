/*
    Project:    Canyon
    Purpose:    OPL3 ISA Sound Card controller
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018

    Designed for Yamaha Audician sound card
*/

// Undefine to use polling
// The interrupt code needs to disable interrupts during ISA bus transfers
// which might cause delays to be ignored. Lack of delays doesn't seem to
// cause a problem but this all seems to work fine without interrupts anyway.
//#define USE_MPU401_INTERRUPTS

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
#include "MIDIControl.h"

const uint16_t mpu401IoBaseAddress  = 0x330;
const uint8_t  mpu401IRQ            = 5;
const uint16_t opl3IoBaseAddress    = 0x388;

const uint8_t mpu401IntPin = 2;
const uint8_t readyPin = 4;
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

ISABus isaBus(isaOutputPin, isaInputPin, isaClockPin, isaLatchPin,
              isaLoadPin, isaWritePin, isaReadPin, isaResetPin);

OPL3SA opl3sa(isaBus);
MPU401 mpu401(isaBus);
OPL3::Hardware opl3(isaBus, opl3IoBaseAddress);

MIDIBuffer midiBuffer;

/*
    Handle MIDI input through the serial pin
*/

void processSerialInput()
{
    uint8_t data;
    uint16_t expectedLength;
    static uint8_t length = 0;

    static struct MIDIMessage message = {
        0, {0, 0}
    };

    while (Serial.available()) {
        data = Serial.read();

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
            // Not a supported message - ignore it and wait for the next
            // status byte
            message.status = 0;
            length = 0;
        } else if (length == expectedLength) {
            midiBuffer.put(message);

            // Keep the status byte for the next message
            length = 1;
        }
    }
}

/*
    Joystick/MIDI port handling
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
            // Not a supported message - ignore it and wait for the next
            // status byte
            message.status = 0;
            length = 0;
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

MIDIControl midiControl(opl3);

void setup()
{
    long startTime = millis();

    // This LED will stay on during initialisation
    pinMode(diagnosticLedPin, OUTPUT);
    digitalWrite(diagnosticLedPin, HIGH);

    // The controller board uses this to determine when startup has completed
    pinMode(readyPin, OUTPUT);
    digitalWrite(readyPin, LOW);

#ifdef WITH_SERIAL
    Serial.begin(31250);
    Serial.println("Canyon\n------");
#endif

    isaBus.reset();

#ifdef WITH_SERIAL
    Serial.print("Initialising OPL3SA... ");
#endif
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

    midiControl.init();

#ifdef WITH_SERIAL
    Serial.println(millis() - startTime);
    Serial.println("\nReady!\n");
#endif

    // Visual indication that startup has completed
    digitalWrite(diagnosticLedPin, LOW);

    // Signal to the controller board that we're ready for input
    digitalWrite(readyPin, HIGH);
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

    // Handle MIDI input through serial RX pin
    processSerialInput();

    // Handle MIDI input through joystick/MIDI port
    #ifndef USE_MPU401_INTERRUPTS
    if (mpu401.canRead()) {
        receiveMpu401Data();
    }
    #endif

    while (midiBuffer.hasContent()) {
        diagnosticLedBrightness = 0xff;

        if (midiBuffer.get(message)) {
            //printMidiMessage(message);

            channel = message.status & 0x0f;
            
            switch (message.status & 0xf0) {
                case 0x80:
                    midiControl.stopNote(channel, message.data[0]);
                    break;

                case 0x90:
                    midiControl.playNote(channel, message.data[0], message.data[1]);
                    break;

                case 0xb0:
                    midiControl.setController(channel, message.data[0], message.data[1]);
                    break;

                case 0xe0:
                    midiControl.setPitchBend(channel, (uint16_t)(message.data[1] << 7) | message.data[0]);
                    break;
            };
        }
    }
}

void loop()
{
    // This causes some output interference
    #if 1
    if (diagnosticLedBrightness > 0) {
        if (++ diagnosticLedFrame > 100) {
            analogWrite(diagnosticLedPin, -- diagnosticLedBrightness);
            diagnosticLedFrame = 0;
        }
    }
    #endif

    midiControl.service();
    serviceMidiInput();
}
