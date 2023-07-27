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
#include "freq.h"

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
            Serial.print("Not supported - ignoring status: ");
            Serial.print(message.status, HEX);
            Serial.println("");
            #endif
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

// TODO: Increase this to 16 eventually
#define NUMBER_OF_MIDI_CHANNELS 4

class MidiControl {
    public:
        MidiControl()
        : m_numberOfPlayingNotes(0),
          m_previousMillis(millis())
        {
            for (int i = 0; i < NUMBER_OF_MIDI_CHANNELS; ++ i) {
                m_channelData[i].type = OPL3::Melody2OpChannelType;
                m_channelData[i].outputs = 0x3;
                m_channelData[i].synthType = 0;

                for (int op = 0; op < 4; ++ op) {
                    m_channelData[i].operatorData[op].attenuation = 16;
                    m_channelData[i].operatorData[op].attackRate = 14;
                    m_channelData[i].operatorData[op].decayRate = 1;
                    m_channelData[i].operatorData[op].sustainLevel = 5;
                    m_channelData[i].operatorData[op].releaseRate = 7;
                    m_channelData[i].operatorData[op].waveform = 0;
                    m_channelData[i].operatorData[op].frequencyMultiplicationFactor = 0;
                }

                m_channelData[i].lfoStartDelay = 5;     // TESTING
            }

            for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                m_playingNotes[i].clear();
            }
        }

        ~MidiControl()
        {
        }
        
        void init()
        {
            //opl3.setChannelType(OPL3::Melody2OpChannelType);
            // TODO: Enable/disable this dynamically
            opl3.enablePercussion();

            opl3.setVibratoDepth(true);
            opl3.setTremoloDepth(false);
        }

        void playNote(
            uint8_t channel,
            uint8_t note,
            uint8_t velocity)
        {
            uint8_t opl3Channel = UnusedOpl3Channel;
            int noteSlot = NoNoteSlot;

            if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (note > 0x7f) || (velocity > 0x7f)) {
                return;
            }

            for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                if (m_playingNotes[i].opl3Channel == UnusedOpl3Channel) {
                    noteSlot = i;
                    break;
                }
            }

            if (noteSlot == NoNoteSlot) {
                return;
            }

            opl3Channel = opl3.allocateChannel(m_channelData[channel].type);
            if (opl3Channel == OPL3::InvalidChannel) {
#ifdef WITH_SERIAL
                Serial.println("Cannot allocate OPL3 channel");
#endif
                return;
            }

#ifdef WITH_SERIAL
            Serial.print("Channel ");
            Serial.print(opl3Channel);
            Serial.println("");
#endif

            opl3.setOutput(opl3Channel, m_channelData[channel].outputs);
            opl3.setSynthType(opl3Channel, m_channelData[channel].synthType);
            opl3.setFeedbackModulationFactor(opl3Channel, m_channelData[channel].feedbackModulationFactor);

            for (int op = 0; op < 4; ++ op) {
                #define SET_OPERATOR_DATA(method, member) \
                    opl3.method(opl3Channel, op, m_channelData[channel].operatorData[op].member)

                SET_OPERATOR_DATA(setWaveform, waveform);
                SET_OPERATOR_DATA(setTremolo, tremolo);
                SET_OPERATOR_DATA(setVibrato, vibrato);
                SET_OPERATOR_DATA(setAttenuation, attenuation);     // TODO: Consider velocity, velocity-to-level
                SET_OPERATOR_DATA(setAttackRate, attackRate);
                SET_OPERATOR_DATA(setDecayRate, decayRate);
                SET_OPERATOR_DATA(setSustainLevel, sustainLevel);
                SET_OPERATOR_DATA(setReleaseRate, releaseRate);
                SET_OPERATOR_DATA(setKeyScaleLevel, keyScaleLevel);
                SET_OPERATOR_DATA(setEnvelopeScaling, envelopeScaling);
                SET_OPERATOR_DATA(setFrequencyMultiplicationFactor, frequencyMultiplicationFactor);
                SET_OPERATOR_DATA(setSustain, sustain);

                #undef SET_OPERATOR_DATA
            }

            m_playingNotes[noteSlot].midiChannel = channel;
            m_playingNotes[noteSlot].opl3Channel = opl3Channel;
            m_playingNotes[noteSlot].midiNote = note;
            m_playingNotes[noteSlot].velocity = velocity;

            ++ m_numberOfPlayingNotes;

#ifdef WITH_SERIAL
//            Serial.print("Note "); Serial.print(note);
//            Serial.print(" freq "); Serial.print(getNoteFrequency(note, 0));
//            Serial.println("");
#endif

            m_channelData[channel].pitchBend = 0;

            opl3.setFrequency(opl3Channel, getNoteFrequency(note, m_channelData[channel].pitchBend));

            opl3.keyOn(opl3Channel);
        }

        void stopNote(
            uint8_t channel,
            uint8_t note)
        {
            uint8_t opl3Channel = UnusedOpl3Channel;
            int noteSlot = NoNoteSlot;

            if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (note > 0x7f)) {
                return;
            }

            for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
                if ((m_playingNotes[i].midiNote == note)
                    && (m_playingNotes[i].midiChannel == channel)
                    && (m_playingNotes[i].opl3Channel != UnusedOpl3Channel)) {
                    noteSlot = i;
                    break;
                }
            }

            if (noteSlot == NoNoteSlot) {
                return;
            }

            opl3Channel = m_playingNotes[noteSlot].opl3Channel;
            if (opl3Channel == UnusedOpl3Channel) {
#ifdef WITH_SERIAL
                Serial.println("Invalid OPL3 channel");
#endif
                return;
            }

            opl3.keyOff(opl3Channel);
            opl3.freeChannel(opl3Channel);

            m_playingNotes[noteSlot].clear();

            -- m_numberOfPlayingNotes;
        }

        void setController(
            uint8_t channel,
            uint8_t controller,
            uint8_t value)
        {
            if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (controller > 0x7f) || (value > 0x7f)) {
                return;
            }

#ifdef WITH_SERIAL
            Serial.print("MIDI channel ");
            Serial.print(channel);
            Serial.print(" controller ");
            Serial.print(controller);
            Serial.print(" value ");
            Serial.print(value);
            Serial.println("");
#endif

            #define CHANNEL_CONTROLLER_CASE(controller, method, member, value) \
                case controller: \
                    m_channelData[channel].member = value; \
                    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) { \
                        if (m_playingNotes[i].midiChannel == channel) \
                            opl3.method(m_playingNotes[i].opl3Channel, value); \
                    } \
                    break;

            #define OPERATOR_CONTROLLER_CASE(controller, operatorIndex, method, member, value) \
                case controller: \
                    m_channelData[channel].operatorData[operatorIndex].member = value; \
                    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) { \
                        if (m_playingNotes[i].midiChannel == channel) \
                            opl3.method(m_playingNotes[i].opl3Channel, operatorIndex, value); \
                    } \
                    break;

            // Todo: ops 3/4, synth type and percussion mode, sustain
            switch (controller) {
                // General

                // This is a combination of channel type and synth type
                // 0 - 11       2-op melody, synth type 0
                // 12 - 23      2-op melody, synth type 1
                // 24 - 35      4-op melody, synth type 0
                // 36 - 47      4-op melody, synth type 1
                // 48 - 59      4-op melody, synth type 2
                // 60 - 71      4-op melody, synth type 3
                // 72 - 82      percussion, kick
                // 83 - 93      percussion, snare
                // 94 - 104     percussion, tom-tom
                // 105 - 115    percussion, cymbal
                // 116 - 127    percussion, hi-hat
                case 9:
                {
                    OPL3::ChannelType newChannelType = m_channelData[channel].type;
                    
                    if (value < 24) {
                        newChannelType = OPL3::Melody2OpChannelType;
                        m_channelData[channel].synthType = value < 12 ? 0 : 1;
                    } else if (value < 72) {
                        newChannelType = OPL3::Melody4OpChannelType;
                        if (value < 36) {
                            m_channelData[channel].synthType = 0;
                        } else if (value < 48) {
                            m_channelData[channel].synthType = 1;
                        } else if (value < 60) {
                            m_channelData[channel].synthType = 2;
                        } else {
                            m_channelData[channel].synthType = 3;
                        }
                    } else if (value < 83) {
                        newChannelType = OPL3::KickChannelType;
                    } else if (value < 94) {
                        newChannelType = OPL3::SnareChannelType;
                    } else if (value < 105) {
                        newChannelType = OPL3::TomTomChannelType;
                    } else if (value < 116) {
                        newChannelType = OPL3::CymbalChannelType;
                    } else {
                        newChannelType = OPL3::HiHatChannelType;
                    }

                    if (m_channelData[channel].type != newChannelType) {
                        stopAllNotes(channel, true);
                        m_channelData[channel].type = newChannelType;
                    }

                    break;
                }

                case 120:   // All sound off
                    stopAllNotes(channel, true);
                    break;

                case 121:   // Reset controllers to default values
                    // TODO
                    break;

                case 123:   // All notes off
                    stopAllNotes(channel, false);
                    break;

                CHANNEL_CONTROLLER_CASE(10, setOutput, outputs, (value < 43 ? 0x2 : (value > 85 ? 0x1 : 0x3)));

                // TODO: Don't actually set vibrato/tremolo here, just flag whether
                // enabled or not and set them after LFO delay

                // Operator 1
                OPERATOR_CONTROLLER_CASE(18, 0, setWaveform, waveform, value >> 4);
                OPERATOR_CONTROLLER_CASE(23, 0, setTremolo, tremolo, value >> 6);
                OPERATOR_CONTROLLER_CASE(28, 0, setVibrato, vibrato, value >> 6);
                OPERATOR_CONTROLLER_CASE(85, 0, setAttenuation, attenuation, 63 - (value >> 1));
                OPERATOR_CONTROLLER_CASE(86, 0, setAttackRate, attackRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(87, 0, setDecayRate, decayRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(88, 0, setSustainLevel, sustainLevel, value >> 3);
                OPERATOR_CONTROLLER_CASE(89, 0, setReleaseRate, releaseRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(90, 0, setKeyScaleLevel, keyScaleLevel, value >> 5);
                OPERATOR_CONTROLLER_CASE(80, 0, setEnvelopeScaling, envelopeScaling, value >> 6);
                OPERATOR_CONTROLLER_CASE(75, 0, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
                OPERATOR_CONTROLLER_CASE(66, 0, setSustain, sustain, value >> 6);
                CHANNEL_CONTROLLER_CASE(79, setFeedbackModulationFactor, feedbackModulationFactor, value >> 4);

                // Operator 2
                OPERATOR_CONTROLLER_CASE(19, 1, setWaveform, waveform, value >> 4);
                OPERATOR_CONTROLLER_CASE(24, 1, setTremolo, tremolo, value >> 6);
                OPERATOR_CONTROLLER_CASE(29, 1, setVibrato, vibrato, value >> 6);
                OPERATOR_CONTROLLER_CASE(102, 1, setAttenuation, attenuation, 63 - (value >> 1));
                OPERATOR_CONTROLLER_CASE(103, 1, setAttackRate, attackRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(104, 1, setDecayRate, decayRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(105, 1, setSustainLevel, sustainLevel, value >> 3);
                OPERATOR_CONTROLLER_CASE(106, 1, setReleaseRate, releaseRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(107, 1, setKeyScaleLevel, keyScaleLevel, value >> 5);
                OPERATOR_CONTROLLER_CASE(81, 1, setEnvelopeScaling, envelopeScaling, value >> 6);
                OPERATOR_CONTROLLER_CASE(76, 1, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
                OPERATOR_CONTROLLER_CASE(67, 1, setSustain, sustain, value >> 6);
                
                // Operator 3
                OPERATOR_CONTROLLER_CASE(20, 2, setWaveform, waveform, value >> 4);
                OPERATOR_CONTROLLER_CASE(25, 2, setTremolo, tremolo, value >> 6);
                OPERATOR_CONTROLLER_CASE(30, 2, setVibrato, vibrato, value >> 6);
                OPERATOR_CONTROLLER_CASE(108, 2, setAttenuation, attenuation, 63 - (value >> 1));
                OPERATOR_CONTROLLER_CASE(109, 2, setAttackRate, attackRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(110, 2, setDecayRate, decayRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(111, 2, setSustainLevel, sustainLevel, value >> 3);
                OPERATOR_CONTROLLER_CASE(112, 2, setReleaseRate, releaseRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(113, 2, setKeyScaleLevel, keyScaleLevel, value >> 5);
                OPERATOR_CONTROLLER_CASE(82, 2, setEnvelopeScaling, envelopeScaling, value >> 6);
                OPERATOR_CONTROLLER_CASE(77, 2, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
                OPERATOR_CONTROLLER_CASE(68, 2, setSustain, sustain, value >> 6);
                
                // Operator 4
                OPERATOR_CONTROLLER_CASE(21, 3, setWaveform, waveform, value >> 4);
                OPERATOR_CONTROLLER_CASE(26, 3, setTremolo, tremolo, value >> 6);
                OPERATOR_CONTROLLER_CASE(31, 3, setVibrato, vibrato, value >> 6);
                OPERATOR_CONTROLLER_CASE(114, 3, setAttenuation, attenuation, 63 - (value >> 1));
                OPERATOR_CONTROLLER_CASE(115, 3, setAttackRate, attackRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(116, 3, setDecayRate, decayRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(117, 3, setSustainLevel, sustainLevel, value >> 3);
                OPERATOR_CONTROLLER_CASE(118, 3, setReleaseRate, releaseRate, 15 - (value >> 3));
                OPERATOR_CONTROLLER_CASE(119, 3, setKeyScaleLevel, keyScaleLevel, value >> 5);
                OPERATOR_CONTROLLER_CASE(83, 3, setEnvelopeScaling, envelopeScaling, value >> 6);
                OPERATOR_CONTROLLER_CASE(78, 3, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
                OPERATOR_CONTROLLER_CASE(69, 3, setSustain, sustain, value >> 6);
                                
                default:
#ifdef WITH_SERIAL
                    Serial.print("Unknown controller ");
                    Serial.print(controller);
                    Serial.println("");
#endif
                    break;
            };

            #undef CHANNEL_CONTROLLER_CASE
            #undef OPERATOR_CONTROLLER_CASE
        }

        void service()
        {
            unsigned long elapsedMillis = millis() - m_previousMillis;

            if (elapsedMillis == 0)
                return;

            m_previousMillis = millis();

            for (int noteSlot = 0; noteSlot < OPL3::NumberOfChannels; ++ noteSlot) {
                NoteData &note = m_playingNotes[noteSlot];
                if (note.opl3Channel != UnusedOpl3Channel) {
                    // We stop caring about note duration after 32.767 seconds (TODO: check this!)
                    uint16_t newDuration = note.duration + elapsedMillis;
                    if (newDuration > 32767) {
                        newDuration = 32767;
                    }

                    note.duration = newDuration;

                    // LFO start delay handling (controls OPL3 vibrato/tremolo)
                    int lfoStartDelay = m_channelData[note.midiChannel].lfoStartDelay;
                    lfoStartDelay *= 150;
                    if ((newDuration > lfoStartDelay) && (!note.lfoTriggered)) {
                        note.lfoTriggered = true;
                        Serial.print(note.opl3Channel);
                        Serial.println(" - enabling vibrato");
                        for (int op = 0; op < 4; ++ op) {
                            // TODO: Set vibrato or tremolo based on whether those are enabled
                            // for this operator
                            //if (m_channelData[midiChannel].tremolo) {
                                opl3.setTremolo(note.opl3Channel, op, true);
                            //}
                        }
                    }

                    if ((newDuration == 32767) || (newDuration % 500 == 0)) {
                        Serial.print(noteSlot);
                        Serial.print(" --> ");
                        Serial.print(newDuration);
                        Serial.println("");
                    }

                    // Hack to test pitch bend
                    //m_channelData[note.midiChannel].pitchBend -= 1;
                    //Serial.print(m_channelData[note.midiChannel].pitchBend);
                    //Serial.println("");
                    //opl3.setFrequency(note.opl3Channel, getNoteFrequency(note.midiNote, m_channelData[note.midiChannel].pitchBend) / 10);
                }
            }
        }

    private:
        void stopAllNotes(
            uint8_t channel,
            bool immediate = false)
        {
            uint8_t opl3Channel;

            for (int noteSlot = 0; noteSlot < OPL3::NumberOfChannels; ++ noteSlot) {
                opl3Channel = m_playingNotes[noteSlot].opl3Channel;
                if ((channel == m_playingNotes[noteSlot].midiChannel) && (opl3Channel != UnusedOpl3Channel)) {
                    if (immediate) {
                        // Max attenuation and release rate for all operators
                        for (int op = 0; op < 4; ++ op) {
                            opl3.setReleaseRate(opl3Channel, op, 15);
                            opl3.setAttenuation(opl3Channel, op, 63);
                        }
                    }

                    opl3.keyOff(opl3Channel);
                    opl3.freeChannel(opl3Channel);

                    m_playingNotes[noteSlot].clear();
                }
            }

            m_numberOfPlayingNotes = 0;
        }

        enum {
            NoNoteSlot = -1,
            UnusedOpl3Channel = 31
        };

        typedef struct __attribute__((packed)) NoteData {
            void clear()
            {
                midiChannel = 0;
                opl3Channel = UnusedOpl3Channel;
                midiNote = 0;
                velocity = 0;
                duration = 0;
                lfoTriggered = false;
            }

            unsigned midiChannel    : 4;
            unsigned opl3Channel    : 5;
            unsigned midiNote       : 7;
            unsigned velocity       : 7;
            unsigned duration       : 15;    // in milliseconds
            unsigned lfoTriggered   : 1;
            unsigned padding        : 1;
        } NoteData;

        unsigned int m_numberOfPlayingNotes;
        NoteData m_playingNotes[OPL3::NumberOfChannels];

        typedef struct __attribute__((packed)) MidiChannelData {
            OPL3::ChannelType type;
            unsigned outputs        : 2;
            unsigned synthType      : 2;
            unsigned feedbackModulationFactor   : 3;

            struct {
                unsigned attenuation    : 6;
                unsigned attackRate     : 4;
                unsigned decayRate      : 4;
                unsigned sustainLevel   : 4;
                unsigned releaseRate    : 4;
                unsigned keyScaleLevel  : 2;
                unsigned waveform       : 3;
                unsigned frequencyMultiplicationFactor  : 4;
                unsigned sustain        : 1;
                unsigned tremolo        : 1;
                unsigned vibrato        : 1;
                unsigned envelopeScaling : 1;
            } operatorData[4];

            unsigned lfoStartDelay      : 4;    // Affects vibrato/tremolo
            signed pitchBend            : 9;    // -200 to +200 cents
        } MidiChannelData;

        // Maybe one day we'll support multiple channels
        MidiChannelData m_channelData[NUMBER_OF_MIDI_CHANNELS];

        unsigned long m_previousMillis;
};

MidiControl midiControl;

void setup()
{
    pinMode(diagnosticLedPin, OUTPUT);
    digitalWrite(diagnosticLedPin, HIGH);

#ifdef WITH_SERIAL
    Serial.begin(9600);
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

    // panning - TODO
    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        //opl3.setChannelRegister(i, OPL3::ChannelRegisterC, 0x30);
    }

    // Note allocation
    /*
    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        g_playingNotes[i].midiChannel = 0;
        g_playingNotes[i].opl3Channel = 31; // Not a valid channel
        g_playingNotes[i].midiNote = 0;
    }
    */

    //opl3.enablePercussion();
    midiControl.init();

#ifdef WITH_SERIAL
    Serial.println("\nReady!\n");
#endif

    digitalWrite(diagnosticLedPin, LOW);

//    previousMillis = millis();
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

//#define TEST_MODE 0

#ifdef TEST_MODE
    // Enable 4-op channels
    //isaBus.write(0x38a, 0x04);
    //isaBus.write(0x38b, 0x3f);

    // Percussion mode
    //isaBus.write(0x388, 0xbd);  isaBus.write(0x389, 0x20);

    //isaBus.write(0x388, 0xa6);  isaBus.write(0x389, 0xff);      // f-num
    //isaBus.write(0x388, 0xb6);  isaBus.write(0x389, 0x07);      // key off
    //isaBus.write(0x388, 0xc6);  isaBus.write(0x389, 0x30);      // output

    //isaBus.write(0x388, 0xa7);  isaBus.write(0x389, 0xff);      // f-num
    //isaBus.write(0x388, 0xb7);  isaBus.write(0x389, 0x07);      // key off
    //isaBus.write(0x388, 0xc7);  isaBus.write(0x389, 0x30);      // output

    //isaBus.write(0x388, 0xa8);  isaBus.write(0x389, 0xff);      // f-num
    //isaBus.write(0x388, 0xb8);  isaBus.write(0x389, 0x07);      // key off
    //isaBus.write(0x388, 0xc8);  isaBus.write(0x389, 0x30);      // output

    // Set kick operator 1 values
    isaBus.write(0x388, 0x50);  isaBus.write(0x389, 0x02);      // Attenuation
    isaBus.write(0x388, 0x70);  isaBus.write(0x389, 0x74);      // Attack/decay
    isaBus.write(0x388, 0x90);  isaBus.write(0x389, 0x44);      // Sustain/release
    isaBus.write(0x388, 0xf0);  isaBus.write(0x389, 0x00);      // Waveform

    // Set kick operator 2 values
    isaBus.write(0x388, 0x53);  isaBus.write(0x389, 0x02);      // Attenuation
    isaBus.write(0x388, 0x73);  isaBus.write(0x389, 0x74);      // Attack/decay
    isaBus.write(0x388, 0x93);  isaBus.write(0x389, 0x44);      // Sustain/release
    isaBus.write(0x388, 0xf3);  isaBus.write(0x389, 0x00);      // Waveform

    // Set snare operator 1 value
    isaBus.write(0x388, 0x54);  isaBus.write(0x389, 0x02);      // Attenuation
    isaBus.write(0x388, 0x74);  isaBus.write(0x389, 0x74);      // Attack/decay
    isaBus.write(0x388, 0x94);  isaBus.write(0x389, 0x44);      // Sustain/release
    isaBus.write(0x388, 0xf4);  isaBus.write(0x389, 0x01);      // Waveform

    // Set operator 2 values
    //isaBus.write(0x388, 0x53);  isaBus.write(0x389, 0x00);      // Attenuation
    //isaBus.write(0x388, 0x73);  isaBus.write(0x389, 0x73);      // Attack/decay
    //isaBus.write(0x388, 0x93);  isaBus.write(0x389, 0x77);      // Sustain/release
    //isaBus.write(0x388, 0xf3);  isaBus.write(0x389, 0x00);      // Waveform

    // Set operator 3 values
    //isaBus.write(0x388, 0x48);  isaBus.write(0x389, 0x00);      // Attenuation
    //isaBus.write(0x388, 0x68);  isaBus.write(0x389, 0x73);      // Attack/decay
    //isaBus.write(0x388, 0x88);  isaBus.write(0x389, 0x77);      // Sustain/release
    //isaBus.write(0x388, 0xe8);  isaBus.write(0x389, 0x05);      // Waveform

    // Set operator 4 values
    //isaBus.write(0x388, 0x4b);  isaBus.write(0x389, 0x00);      // Attenuation
    //isaBus.write(0x388, 0x6b);  isaBus.write(0x389, 0x73);      // Attack/decay
    //isaBus.write(0x388, 0x8b);  isaBus.write(0x389, 0x77);      // Sustain/release
    //isaBus.write(0x388, 0xeb);  isaBus.write(0x389, 0x06);      // Waveform
#endif

    while (midiBuffer.hasContent()) {
        diagnosticLedBrightness = 0xff;

        if (midiBuffer.get(message)) {
            //printMidiMessage(message);
#ifdef TEST_MODE
            switch (message.status) {
                case 0x90:
                    Serial.println("On");
                    // Set channel values
                    isaBus.write(0x388, 0xa7);  isaBus.write(0x389, 0xff);      // f-num
                    isaBus.write(0x388, 0xb7);  isaBus.write(0x389, 0x07);      // block and high f-num, KEY ON
                    isaBus.write(0x388, 0xc7);  isaBus.write(0x389, 0x30);      // output

                    isaBus.write(0x388, 0xbd);  isaBus.write(0x389, 0x28);      // Percussion, on

                    break;

                case 0x80:
                    Serial.println("Off");
                    // Set channel values
                    //isaBus.write(0x388, 0xb6);  isaBus.write(0x389, 0x07);      // block and high f-num, KEY OFF
                    isaBus.write(0x388, 0xbd);  isaBus.write(0x389, 0x20);      // Percussion, off

                    break;
            };
#else
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
            };
#endif
        }
    }
}

void loop()
{
    #if 0
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
