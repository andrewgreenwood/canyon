/*
    Project:    Canyon
    Purpose:    Front panel controller
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       September 2023

    TODO:
    - Switch/key debouncing
    - Smoothing of synth type control
    - MIDI input handling
    - Channels
    - EEPROM programming
*/

#include <MIDI.h>
#include "parallel_pins.h"
#include "control_mapper.h"

// Tags:
// 0x00nn   Key
// 0x01nn   MIDI CC (switch, controller = nn)
// 0x07nn   MIDI CC (7-bit, controller = nn)

#define KEY_TAG(key)        (key)
#define CC_TAG(cc)          (0x0700 | cc)
#define CC_SWITCH_TAG(cc)   (0x0100 | cc)

#define IS_KEY_TAG(tag)         ((tag & 0xff00) == 0x0000)
#define IS_CC_TAG(tag)          ((tag & 0xff00) == 0x0700)
#define IS_CC_SWITCH_TAG(tag)   ((tag & 0xff00) == 0x0100)

#define GET_TAG_CC(tag)         (tag & 0x00ff)

#define TAG_KEY_LOAD        KEY_TAG('L')
#define TAG_KEY_SAVE        KEY_TAG('S')
#define TAG_KEY_NUMBER_1    KEY_TAG(1)
#define TAG_KEY_NUMBER_2    KEY_TAG(2)
#define TAG_KEY_NUMBER_3    KEY_TAG(3)
#define TAG_KEY_NUMBER_4    KEY_TAG(4)
#define TAG_KEY_NUMBER_5    KEY_TAG(5)
#define TAG_KEY_NUMBER_6    KEY_TAG(6)
#define TAG_KEY_NUMBER_7    KEY_TAG(7)
#define TAG_KEY_NUMBER_8    KEY_TAG(8)
#define TAG_KEY_NUMBER_9    KEY_TAG(9)
#define TAG_KEY_NUMBER_10   KEY_TAG(10)
#define TAG_KEY_NUMBER_11   KEY_TAG(11)
#define TAG_KEY_NUMBER_12   KEY_TAG(12)
#define TAG_KEY_NUMBER_13   KEY_TAG(13)
#define TAG_KEY_NUMBER_14   KEY_TAG(14)
#define TAG_KEY_NUMBER_15   KEY_TAG(15)
#define TAG_KEY_NUMBER_16   KEY_TAG(16)

// Analog pins
const uint8_t mux1Pin = A0;
const uint8_t mux2Pin = A1;
const uint8_t algorithmKnobPin = A2;
const uint8_t lfoStartKnobPin = A3;
const uint8_t op1FeedbackKnobPin = A4;

// Digital pins
const uint8_t loadButtonPin = 2;
const uint8_t mux3Pin = 3;
const uint8_t mux4Pin = 4;
const uint8_t muxAddress3Pin = 5;
const uint8_t muxAddress2Pin = 6;
const uint8_t muxAddress1Pin = 7;
const uint8_t muxAddress0Pin = 8;
const uint8_t shiftStorePin = 9;
const uint8_t shiftUpdatePin = 10;
const uint8_t shiftSerialPin = 11;
const uint8_t saveButtonPin = 12;
const uint8_t readySignalPin = 13;

const uint8_t algorithmMidiCC = 9;

Knob algorithmKnob(algorithmKnobPin, CC_TAG(algorithmMidiCC));
Knob lfoStartKnob(lfoStartKnobPin, CC_TAG(12));
Knob op1FeedbackKnob(op1FeedbackKnobPin, CC_TAG(79));
Switch loadButton(loadButtonPin, TAG_KEY_LOAD);
Switch saveButton(saveButtonPin, TAG_KEY_SAVE);

ParallelPins commonMuxAddressPins(5, 6, 7, 8);  // TODO: replace with consts from above
AnalogMultiplexer mux1(mux1Pin, commonMuxAddressPins);
AnalogMultiplexer mux2(mux2Pin, commonMuxAddressPins);
DigitalMultiplexer mux3(mux3Pin, commonMuxAddressPins);
DigitalMultiplexer mux4(mux4Pin, commonMuxAddressPins);

Knob op1WaveformKnob(mux1, 0, CC_TAG(18));
Knob op1FrequencyMultiplierKnob(mux1, 1, CC_TAG(75));
Knob op2WaveformKnob(mux1, 2, CC_TAG(19));
Knob op2FrequencyMultiplierKnob(mux1, 3, CC_TAG(76));
Knob op3WaveformKnob(mux1, 4, CC_TAG(20));
Knob op3FrequencyMultiplierKnob(mux1, 5, CC_TAG(77));
Knob op4WaveformKnob(mux1, 6, CC_TAG(21));
Knob op4FrequencyMultiplierKnob(mux1, 7, CC_TAG(78));
Knob op1AttackKnob(mux1, 8, CC_TAG(86));
Knob op1DecayKnob(mux1, 9, CC_TAG(87));
Knob op2AttackKnob(mux1, 10, CC_TAG(103));
Knob op2DecayKnob(mux1, 11, CC_TAG(104));
Knob op3AttackKnob(mux1, 12, CC_TAG(109));
Knob op3DecayKnob(mux1, 13, CC_TAG(110));
Knob op4AttackKnob(mux1, 14, CC_TAG(115));
Knob op4DecayKnob(mux1, 15, CC_TAG(116));

Knob op1SustainKnob(mux2, 0, CC_TAG(88));
Knob op1ReleaseKnob(mux2, 1, CC_TAG(89));
Knob op2SustainKnob(mux2, 2, CC_TAG(105));
Knob op2ReleaseKnob(mux2, 3, CC_TAG(106));
Knob op3SustainKnob(mux2, 4, CC_TAG(111));
Knob op3ReleaseKnob(mux2, 5, CC_TAG(112));
Knob op4SustainKnob(mux2, 6, CC_TAG(117));
Knob op4ReleaseKnob(mux2, 7, CC_TAG(118));
Knob op1VelocityToLevelKnob(mux2, 8, CC_TAG(14));
Knob op1LevelKnob(mux2, 9, CC_TAG(85));
Knob op2VelocityToLevelKnob(mux2, 10, CC_TAG(15));
Knob op2LevelKnob(mux2, 11, CC_TAG(102));
Knob op3VelocityToLevelKnob(mux2, 12, CC_TAG(16));
Knob op3LevelKnob(mux2, 13, CC_TAG(108));
Knob op4VelocityToLevelKnob(mux2, 14, CC_TAG(17));
Knob op4LevelKnob(mux2, 15, CC_TAG(114));

Switch op1EnvelopeScalingSwitch(mux3, 0, CC_SWITCH_TAG(80));
Switch op2EnvelopeScalingSwitch(mux3, 1, CC_SWITCH_TAG(81));
Switch op3EnvelopeScalingSwitch(mux3, 2, CC_SWITCH_TAG(82));
Switch op4EnvelopeScalingSwitch(mux3, 3, CC_SWITCH_TAG(83));
Switch op1LevelScalingSwitch(mux3, 4, CC_SWITCH_TAG(90));
Switch op2LevelScalingSwitch(mux3, 5, CC_SWITCH_TAG(107));
Switch op3LevelScalingSwitch(mux3, 6, CC_SWITCH_TAG(113));
Switch op4LevelScalingSwitch(mux3, 7, CC_SWITCH_TAG(119));
Switch op1TremoloSwitch(mux3, 8, CC_SWITCH_TAG(23));
Switch op1VibratoSwitch(mux3, 9, CC_SWITCH_TAG(28));
Switch op2TremoloSwitch(mux3, 10, CC_SWITCH_TAG(24));
Switch op2VibratoSwitch(mux3, 11, CC_SWITCH_TAG(29));
Switch op3TremoloSwitch(mux3, 12, CC_SWITCH_TAG(25));
Switch op3VibratoSwitch(mux3, 13, CC_SWITCH_TAG(30));
Switch op4TremoloSwitch(mux3, 14, CC_SWITCH_TAG(26));
Switch op4VibratoSwitch(mux3, 15, CC_SWITCH_TAG(31));

Switch key1Button(mux4, 0, TAG_KEY_NUMBER_1);
Switch key2Button(mux4, 1, TAG_KEY_NUMBER_2);
Switch key3Button(mux4, 2, TAG_KEY_NUMBER_3);
Switch key4Button(mux4, 3, TAG_KEY_NUMBER_4);
Switch key5Button(mux4, 4, TAG_KEY_NUMBER_5);
Switch key6Button(mux4, 5, TAG_KEY_NUMBER_6);
Switch key7Button(mux4, 6, TAG_KEY_NUMBER_7);
Switch key8Button(mux4, 7, TAG_KEY_NUMBER_8);
Switch key9Button(mux4, 8, TAG_KEY_NUMBER_9);
Switch key10Button(mux4, 9, TAG_KEY_NUMBER_10);
Switch key11Button(mux4, 10, TAG_KEY_NUMBER_11);
Switch key12Button(mux4, 11, TAG_KEY_NUMBER_12);
Switch key13Button(mux4, 12, TAG_KEY_NUMBER_13);
Switch key14Button(mux4, 13, TAG_KEY_NUMBER_14);
Switch key15Button(mux4, 14, TAG_KEY_NUMBER_15);
Switch key16Button(mux4, 15, TAG_KEY_NUMBER_16);

Control *controls[] = {
    &algorithmKnob,
    &lfoStartKnob,

    &op1WaveformKnob,
    &op1FrequencyMultiplierKnob,
    &op1FeedbackKnob,
    &op1EnvelopeScalingSwitch,
    &op1AttackKnob,
    &op1DecayKnob,
    &op1SustainKnob,
    &op1ReleaseKnob,
    &op1TremoloSwitch,
    &op1VibratoSwitch,
    &op1LevelScalingSwitch,
    &op1VelocityToLevelKnob,
    &op1LevelKnob,

    &op2WaveformKnob,
    &op2FrequencyMultiplierKnob,
    &op2EnvelopeScalingSwitch,
    &op2AttackKnob,
    &op2DecayKnob,
    &op2SustainKnob,
    &op2ReleaseKnob,
    &op2TremoloSwitch,
    &op2VibratoSwitch,
    &op2LevelScalingSwitch,
    &op2VelocityToLevelKnob,
    &op2LevelKnob,

    &op3WaveformKnob,
    &op3FrequencyMultiplierKnob,
    &op3EnvelopeScalingSwitch,
    &op3AttackKnob,
    &op3DecayKnob,
    &op3SustainKnob,
    &op3ReleaseKnob,
    &op3TremoloSwitch,
    &op3VibratoSwitch,
    &op3LevelScalingSwitch,
    &op3VelocityToLevelKnob,
    &op3LevelKnob,

    &op4WaveformKnob,
    &op4FrequencyMultiplierKnob,
    &op4EnvelopeScalingSwitch,
    &op4AttackKnob,
    &op4DecayKnob,
    &op4SustainKnob,
    &op4ReleaseKnob,
    &op4TremoloSwitch,
    &op4VibratoSwitch,
    &op4LevelScalingSwitch,
    &op4VelocityToLevelKnob,
    &op4LevelKnob,

    &loadButton,
    &saveButton,

    &key1Button,
    &key2Button,
    &key3Button,
    &key4Button,
    &key5Button,
    &key6Button,
    &key7Button,
    &key8Button,
    &key9Button,
    &key10Button,
    &key11Button,
    &key12Button,
    &key13Button,
    &key14Button,
    &key15Button,
    &key16Button,

    NULL
};

MIDI_CREATE_DEFAULT_INSTANCE();

#define LED_OP1_TO_OP2  0x000001
#define LED_OP2_TO_OP3  0x000002
#define LED_OP3_TO_OP4  0x000004
#define LED_OP1_TO_OUT  0x000008
#define LED_OP2_TO_OUT  0x000010
#define LED_OP3_TO_OUT  0x000020
#define LED_OP4_TO_OUT  0x000040
#define LED_KEY_16      0x000100
#define LED_KEY_15      0x000200
#define LED_KEY_14      0x000400
#define LED_KEY_13      0x000800
#define LED_KEY_12      0x001000
#define LED_KEY_11      0x002000
#define LED_KEY_10      0x004000
#define LED_KEY_9       0x008000
#define LED_KEY_8       0x010000
#define LED_KEY_7       0x020000
#define LED_KEY_6       0x040000
#define LED_KEY_5       0x080000
#define LED_KEY_4       0x100000
#define LED_KEY_3       0x200000
#define LED_KEY_2       0x400000
#define LED_KEY_1       0x800000

#define LED_ALL_OPS     0x0000ff
#define LED_ALL_KEYS    0xffff00


class LEDs {
    public:
        LEDs()
        : m_led_bits(0)
        {
        }

        void set(uint32_t leds)
        {
            m_led_bits |= leds;
            update();
        }

        void clear(uint32_t leds)
        {
            m_led_bits &= ~leds;
            update();
        }

    private:
        void update() const
        {
            uint32_t pattern = m_led_bits;
            for (int i = 0; i < 24; ++ i) {
                digitalWrite(shiftSerialPin, pattern & 0x1 ? HIGH: LOW);

                // Toggle SRCLK to load the bit
                digitalWrite(shiftStorePin, HIGH);
                digitalWrite(shiftStorePin, LOW);

                pattern >>= 1;
            }

            // Toggle RCLK to update output
            digitalWrite(shiftUpdatePin, HIGH);
            digitalWrite(shiftUpdatePin, LOW);
        }

        uint32_t m_led_bits;
};

LEDs leds;

midi::Channel midiChannel = 1;

// Synth type control values:
// 0 50 102 153 204 255

void updateSynthType(uint8_t value)
{
    leds.clear(LED_ALL_OPS);

    if (value < 25) {
        leds.set(LED_OP1_TO_OP2 | LED_OP2_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 0, midiChannel);
    } else if ((value >= 25) && (value < 75)) {
        leds.set(LED_OP1_TO_OUT | LED_OP2_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 12, midiChannel);
    } else if ((value >= 75) && (value < 125)) {
        leds.set(LED_OP1_TO_OP2 | LED_OP2_TO_OP3 | LED_OP3_TO_OP4 | LED_OP4_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 24, midiChannel);
    } else if ((value >= 125) && (value < 175)) {
        leds.set(LED_OP1_TO_OUT | LED_OP2_TO_OP3 | LED_OP3_TO_OP4 | LED_OP4_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 36, midiChannel);
    } else if ((value >= 175) && (value < 225)) {
        leds.set(LED_OP1_TO_OP2 | LED_OP2_TO_OUT | LED_OP3_TO_OP4 | LED_OP4_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 48, midiChannel);
    } else {
        leds.set(LED_OP1_TO_OUT | LED_OP2_TO_OP3 | LED_OP3_TO_OUT | LED_OP4_TO_OUT);
        MIDI.sendControlChange(algorithmMidiCC, 60, midiChannel);
    }
}

long ticker = 0;
bool heartbeat = false;
bool testMode = false;

void setup() {
    pinMode(algorithmKnobPin, INPUT);
    pinMode(lfoStartKnobPin, INPUT);
    pinMode(op1FeedbackKnobPin, INPUT);
    pinMode(loadButtonPin, INPUT_PULLUP);
    pinMode(saveButtonPin, INPUT_PULLUP);

    pinMode(mux1Pin, INPUT);
    pinMode(mux2Pin, INPUT);
    pinMode(mux3Pin, INPUT_PULLUP);
    pinMode(mux4Pin, INPUT_PULLUP);

    pinMode(muxAddress0Pin, OUTPUT);
    pinMode(muxAddress1Pin, OUTPUT);
    pinMode(muxAddress2Pin, OUTPUT);
    pinMode(muxAddress3Pin, OUTPUT);

    pinMode(shiftStorePin, OUTPUT);
    pinMode(shiftUpdatePin, OUTPUT);
    pinMode(shiftSerialPin, OUTPUT);

    pinMode(readySignalPin, INPUT);

    // Go into test mode if both "Load" and "Save" are held during startup
    // FIXME: This doesn't work
    if ((digitalRead(loadButtonPin) == LOW) && (digitalRead(saveButtonPin) == LOW)) {
        testMode = true;
        Serial.begin(31250);
    } else {
        MIDI.begin(MIDI_CHANNEL_OMNI);
        MIDI.turnThruOn();
    }

    leds.clear(LED_ALL_KEYS | LED_ALL_OPS);
    //leds.set(LED_ALL_OPS);

    // Wait for "ready" signal from synth board so we don't send MIDI CC too
    // early. this will be floating when disconnected which is also fine.
    while (digitalRead(readySignalPin) == LOW) {}

    ticker = millis();
}

uint8_t testNote = 1;

/*
    The test loop displays incoming MIDI data bits using the top LEDs
    It also sends MIDI notes
    The op1 carrier output LED blinks to indicate that the program is running
*/
void TestLoop()
{
    int incomingByte = 0;

    if (Serial.available() > 0) {
        incomingByte = Serial.read();

        uint32_t x = 0;

        if (incomingByte & 0x80) x |= LED_KEY_1;
        if (incomingByte & 0x40) x |= LED_KEY_2;
        if (incomingByte & 0x20) x |= LED_KEY_3;
        if (incomingByte & 0x10) x |= LED_KEY_4;
        if (incomingByte & 0x08) x |= LED_KEY_5;
        if (incomingByte & 0x04) x |= LED_KEY_6;
        if (incomingByte & 0x02) x |= LED_KEY_7;
        if (incomingByte & 0x01) x |= LED_KEY_8;

        leds.clear(LED_ALL_KEYS);
        leds.set(x);
        delay(500);
    }

    if (millis() - ticker > 500) {
        leds.clear(LED_ALL_OPS);
        if (heartbeat) {
            leds.set(LED_OP1_TO_OUT);
            Serial.write(0x90);
            Serial.write(testNote);
            Serial.write(0x7f);
        } else {
            leds.clear(LED_OP1_TO_OUT);
            Serial.write(0x80);
            Serial.write(testNote);
            Serial.write(0x7f);
            if (++ testNote > 0x7f) {
                testNote = 1;
            }
        }
        heartbeat = !heartbeat;
        ticker = millis();
    }
}

int testOctave = 3;

void MainLoop()
{
    Control *control;

    for (int i = 0; controls[i]; ++ i) {
        // Do this here so that MIDI pass-thru isn't lagged by all the
        // analog reads
        MIDI.read();

        control = controls[i];
        if (control->update()) {
            if (IS_CC_TAG(control->getTagData())) {
                if (GET_TAG_CC(control->getTagData()) == 9) {
                    updateSynthType(control->value());
                } else {
                    MIDI.sendControlChange(GET_TAG_CC(control->getTagData()), control->value() >> 1, midiChannel);
                }
            } else if (IS_CC_SWITCH_TAG(control->getTagData())) {
                MIDI.sendControlChange(GET_TAG_CC(control->getTagData()), control->value() ? 0x7f: 0x00, midiChannel);
            } else if (IS_KEY_TAG(control->getTagData())) {
                // Test stuff (for now)
                // Number keys play notes, load/save change octave

                uint8_t note = 0;
                bool sendMidi = true;
                leds.clear(LED_ALL_KEYS);
                switch(control->getTagData()) {
                    case TAG_KEY_NUMBER_1:
                        leds.set(LED_KEY_1);
                        note = 0x0;
                        break;
                    case TAG_KEY_NUMBER_2:
                        leds.set(LED_KEY_2);
                        note = 0x1;
                        break;
                    case TAG_KEY_NUMBER_3:
                        leds.set(LED_KEY_3);
                        note = 0x2;
                        break;
                    case TAG_KEY_NUMBER_4:
                        leds.set(LED_KEY_4);
                        note = 0x3;
                        break;
                    case TAG_KEY_NUMBER_5:
                        leds.set(LED_KEY_5);
                        note = 0x4;
                        break;
                    case TAG_KEY_NUMBER_6:
                        leds.set(LED_KEY_6);
                        note = 0x5;
                        break;
                    case TAG_KEY_NUMBER_7:
                        leds.set(LED_KEY_7);
                        note = 0x6;
                        break;
                    case TAG_KEY_NUMBER_8:
                        leds.set(LED_KEY_8);
                        note = 0x7;
                        break;
                    case TAG_KEY_NUMBER_9:
                        leds.set(LED_KEY_9);
                        note = 0x8;
                        break;
                    case TAG_KEY_NUMBER_10:
                        leds.set(LED_KEY_10);
                        note = 0x9;
                        break;
                    case TAG_KEY_NUMBER_11:
                        leds.set(LED_KEY_11);
                        note = 0xa;
                        break;
                    case TAG_KEY_NUMBER_12:
                        leds.set(LED_KEY_12);
                        note = 0xb;
                        break;
                    case TAG_KEY_NUMBER_13:
                        leds.set(LED_KEY_13);
                        note = 0xc;
                        break;
                    case TAG_KEY_NUMBER_14:
                        leds.set(LED_KEY_14);
                        note = 0xd;
                        break;
                    case TAG_KEY_NUMBER_15:
                        leds.set(LED_KEY_15);
                        note = 0xe;
                        break;
                    case TAG_KEY_NUMBER_16:
                        leds.set(LED_KEY_16);
                        note = 0xf;
                        break;
                    case TAG_KEY_LOAD:
                        sendMidi = false;
                        if (control->value()) {
                            ++ testOctave;
                        }
                        break;
                    case TAG_KEY_SAVE:
                        sendMidi = false;
                        if (control->value()) {
                            -- testOctave;
                        }
                        break;
                }
                
                note += testOctave * 12;
                if (sendMidi) {
                    if (control->value()) {
                        MIDI.sendNoteOn(note, 0x7f, 1);
                    } else {
                        MIDI.sendNoteOff(note, 0x7f, 1);
                    }
                }
            }
        }
    }
}

void loop()
{
    if (testMode) {
        TestLoop();
    } else {
        MainLoop();
    }
}
