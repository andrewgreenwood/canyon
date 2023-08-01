#ifdef ARDUINO
    #include <arduino.h>
#else
    #include <cstddef>
#endif
#include "MIDIControl.h"
#include "freq.h"

// TODO: Increase this to 16 eventually
#define NUMBER_OF_MIDI_CHANNELS 4

#define FOR_EACH_PLAYING_NOTE(channel, slot, code) \
    for (int slot##index = 0; slot##index < OPL3::NumberOfChannels; ++ slot##index) { \
        NoteData &slot = m_playingNotes[slot##index]; \
        if ((channel == slot.midiChannel) && (slot.opl3Channel != UnusedOpl3Channel)) { \
            code; \
        } \
    }

MIDIControl::MIDIControl(OPL3::Hardware &opl3)
: m_opl3(opl3),
  m_numberOfPlayingNotes(0),
  m_previousMillis(millis())
{
    // Default patch
    for (int i = 0; i < NUMBER_OF_MIDI_CHANNELS; ++ i) {
        m_channelData[i].type = OPL3::Melody2OpChannelType;
        m_channelData[i].outputs = 0x3;
        m_channelData[i].synthType = 0;

        for (int op = 0; op < 4; ++ op) {
            m_channelData[i].operatorData[op].level = 48;
            m_channelData[i].operatorData[op].attackRate = 11;
            m_channelData[i].operatorData[op].decayRate = 1;
            m_channelData[i].operatorData[op].sustainLevel = 5;
            m_channelData[i].operatorData[op].releaseRate = 7;
            m_channelData[i].operatorData[op].keyScaleLevel = 0;
            m_channelData[i].operatorData[op].waveform = 0;
            m_channelData[i].operatorData[op].frequencyMultiplicationFactor = 0;
            m_channelData[i].operatorData[op].tremolo = false;
            m_channelData[i].operatorData[op].vibrato = false;
            m_channelData[i].operatorData[op].envelopeScaling = false;
            m_channelData[i].operatorData[op].velocityToLevel = 32;
        }

        m_channelData[i].volume = 63;
        m_channelData[i].lfoStartDelay = 5;     // TODO: Make adjustable
        m_channelData[i].pitchBend = 0;
        m_channelData[i].sustaining = false;
    }

    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        m_playingNotes[i].clear();
    }
}

void MIDIControl::init()
{
    //opl3.setChannelType(OPL3::Melody2OpChannelType);
    // TODO: Enable/disable this dynamically
    //opl3.enablePercussion();

    m_opl3.setVibratoDepth(true);
    m_opl3.setTremoloDepth(false);
}

void MIDIControl::playNote(
    uint8_t channel,
    uint8_t note,
    uint8_t velocity)
{
    uint8_t opl3Channel = UnusedOpl3Channel;
    NoteData *noteData = NULL;
    //int noteSlot = NoNoteSlot;

    if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (note > 0x7f) || (velocity > 0x7f)) {
        return;
    }

    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        if (m_playingNotes[i].opl3Channel == UnusedOpl3Channel) {
            noteData = &m_playingNotes[i];
            break;
        }
    }

    //if (noteSlot == NoNoteSlot) {
    if (!noteData)
        return;

    opl3Channel = m_opl3.allocateChannel(m_channelData[channel].type);

    if (opl3Channel == OPL3::InvalidChannel) {
        // Try to steal a channel for a note that is in the release phase
        for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
            if (m_playingNotes[i].releasing) {
                noteData = &m_playingNotes[i];
                opl3Channel = noteData->opl3Channel;
                noteData->clear();
                -- m_numberOfPlayingNotes;
                break;
            }
        }
    }

    if (opl3Channel == OPL3::InvalidChannel) {
//        Serial.println("Cannot allocate OPL3 channel");
        return;
    }

#if 0
    Serial.print("Channel ");
    Serial.print(opl3Channel);
    Serial.println("");
#endif

    m_opl3.setOutput(opl3Channel, m_channelData[channel].outputs);
    m_opl3.setSynthType(opl3Channel, m_channelData[channel].synthType);
    m_opl3.setFeedbackModulationFactor(opl3Channel, m_channelData[channel].feedbackModulationFactor);

    for (int op = 0; op < 4; ++ op) {
        #define SET_OPERATOR_DATA(method, member) \
            m_opl3.method(opl3Channel, op, m_channelData[channel].operatorData[op].member)

        SET_OPERATOR_DATA(setWaveform, waveform);

        // These get switched on when the LFO start delay elapses (if
        // set for the operator)
        m_opl3.setTremolo(opl3Channel, op, false);
        m_opl3.setVibrato(opl3Channel, op, false);

        //SET_OPERATOR_DATA(setTremolo, tremolo);
        //SET_OPERATOR_DATA(setVibrato, vibrato);

        //SET_OPERATOR_DATA(setAttenuation, attenuation);     // TODO: Consider velocity, velocity-to-level
        SET_OPERATOR_DATA(setAttackRate, attackRate);
        SET_OPERATOR_DATA(setDecayRate, decayRate);
        SET_OPERATOR_DATA(setSustainLevel, sustainLevel);
        SET_OPERATOR_DATA(setReleaseRate, releaseRate);
        SET_OPERATOR_DATA(setKeyScaleLevel, keyScaleLevel);
        SET_OPERATOR_DATA(setEnvelopeScaling, envelopeScaling);
        SET_OPERATOR_DATA(setFrequencyMultiplicationFactor, frequencyMultiplicationFactor);
        //SET_OPERATOR_DATA(setSustain, sustain);
        m_opl3.setSustain(opl3Channel, op, true);

        #undef SET_OPERATOR_DATA
    }

    noteData->midiChannel = channel;
    noteData->opl3Channel = opl3Channel;
    noteData->midiNote = note;
    noteData->velocity = velocity;

    ++ m_numberOfPlayingNotes;

    m_opl3.setFrequency(opl3Channel, getNoteFrequency(note, m_channelData[channel].pitchBend));
    updateAttenuation(channel);
    m_opl3.keyOn(opl3Channel);
}

void MIDIControl::stopNote(
    uint8_t channel,
    uint8_t note)
{
    uint8_t opl3Channel = UnusedOpl3Channel;
    //int noteSlot = NoNoteSlot;
    NoteData *noteData = NULL;

    //Serial.print("stopNote ");
    //Serial.print(note);
    //Serial.println("");

    if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (note > 0x7f)) {
        return;
    }

    // There could be several of the same note playing (e.g. due to
    // sustain pedal) - stop all of them
    for (int i = 0; i < OPL3::NumberOfChannels; ++ i) {
        if ((m_playingNotes[i].midiNote == note)
            && (m_playingNotes[i].midiChannel == channel)
            && (m_playingNotes[i].opl3Channel != UnusedOpl3Channel)) {
            //noteSlot = i;
            noteData = &m_playingNotes[i];

            //if (noteSlot == NoNoteSlot) {
                //return;
            //}

            opl3Channel = noteData->opl3Channel;
            if (opl3Channel == UnusedOpl3Channel) {
//                Serial.println("Invalid OPL3 channel");
                return;
            }

            // We'll be called again when the sustain pedal is released
            if (!m_channelData[channel].sustaining) {
                m_opl3.keyOff(opl3Channel);
                //opl3.freeChannel(opl3Channel);

                noteData->releasing = true;
                //noteData->clear();

                //-- m_numberOfPlayingNotes;
            } else {
                noteData->sustained = true;
            }
        }
    }
}

void MIDIControl::setController(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if ((channel >= NUMBER_OF_MIDI_CHANNELS) || (controller > 0x7f) || (value > 0x7f)) {
        return;
    }

#if 0
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
                    m_opl3.method(m_playingNotes[i].opl3Channel, value); \
            } \
            break;

#if 0
    #define FOR_EACH_PLAYING_NOTE(index, code) \
        for (int index = 0; index < OPL3::NumberOfChannels; ++ index) { \
            if (m_playingNotes[index].midiChannel == channel) { \
                code; \
            } \
        }
#endif

    #define OPERATOR_CONTROLLER_CASE(controller, operatorIndex, method, member, value) \
        case controller: \
            m_channelData[channel].operatorData[operatorIndex].member = value; \
            for (int i = 0; i < OPL3::NumberOfChannels; ++ i) { \
                if (m_playingNotes[i].midiChannel == channel) \
                    m_opl3.method(m_playingNotes[i].opl3Channel, operatorIndex, value); \
            } \
            break;

    switch (controller) {
        // Global

        case 22:
        {
            m_opl3.setTremoloDepth(value > 63);
            break;
        }

        case 27:
        {
            m_opl3.setVibratoDepth(value > 63);
            break;
        }

        // General

        case 7:
        {
            m_channelData[channel].volume = value >> 1;
            updateAttenuation(channel);
            break;
        }

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
        
        case 64:    // Sustain
        {
            bool doSustain = value > 63;
            m_channelData[channel].sustaining = doSustain;
            if (!doSustain) {
                FOR_EACH_PLAYING_NOTE(channel, note,
                    if (note.sustained) {
                        note.sustained = false;
                        
                        m_opl3.keyOff(note.opl3Channel);
                        //opl3.freeChannel(note.opl3Channel);

                        note.releasing = true;
                        //note.clear();

                        //-- m_numberOfPlayingNotes;
                    }
                );
            }
            break;
        }

        // Operator 1
        OPERATOR_CONTROLLER_CASE(18, 0, setWaveform, waveform, value >> 4);
        //OPERATOR_CONTROLLER_CASE(23, 0, setTremolo, tremolo, value >> 6);
        case 23:
            // TODO: When tremolo/vibrato being turned on for any operator it should
            // clear lfoTriggered for the note, and when turning either of these
            // off it should immediately stop them
            //m_channelData[channel].operatorData[0].tremolo = value >> 6;
            setTremolo(channel, 0, value >> 6);
            break;
        //OPERATOR_CONTROLLER_CASE(28, 0, setVibrato, vibrato, value >> 6);
        case 28:
            //m_channelData[channel].operatorData[0].vibrato = value >> 6;
            setVibrato(channel, 0, value >> 6);
            break;
        //OPERATOR_CONTROLLER_CASE(85, 0, setAttenuation, attenuation, 63 - (value >> 1));
        case 85:
        {
            m_channelData[channel].operatorData[0].level = value >> 1;
            updateAttenuation(channel);
            break;
        }
        OPERATOR_CONTROLLER_CASE(86, 0, setAttackRate, attackRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(87, 0, setDecayRate, decayRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(88, 0, setSustainLevel, sustainLevel, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(89, 0, setReleaseRate, releaseRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(90, 0, setKeyScaleLevel, keyScaleLevel, value >> 5);   // FIXME: need to swap 1 and 2 around
        OPERATOR_CONTROLLER_CASE(80, 0, setEnvelopeScaling, envelopeScaling, value >> 6);
        OPERATOR_CONTROLLER_CASE(75, 0, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
        //OPERATOR_CONTROLLER_CASE(66, 0, setSustain, sustain, value >> 6);
        CHANNEL_CONTROLLER_CASE(79, setFeedbackModulationFactor, feedbackModulationFactor, value >> 4);
        case 14:
        {
            m_channelData[channel].operatorData[0].velocityToLevel = value >> 1;
            updateAttenuation(channel);
            break;
        }

        // Operator 2
        OPERATOR_CONTROLLER_CASE(19, 1, setWaveform, waveform, value >> 4);
        //OPERATOR_CONTROLLER_CASE(24, 1, setTremolo, tremolo, value >> 6);
        case 24:
            m_channelData[channel].operatorData[1].tremolo = value >> 6;
            break;
        //OPERATOR_CONTROLLER_CASE(29, 1, setVibrato, vibrato, value >> 6);
        case 29:
            m_channelData[channel].operatorData[1].vibrato = value >> 6;
            break;
//                OPERATOR_CONTROLLER_CASE(102, 1, setAttenuation, attenuation, 63 - (value >> 1));
        case 102:
        {
            m_channelData[channel].operatorData[1].level = value >> 1;
            updateAttenuation(channel);
            break;
        }
        OPERATOR_CONTROLLER_CASE(103, 1, setAttackRate, attackRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(104, 1, setDecayRate, decayRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(105, 1, setSustainLevel, sustainLevel, 15 -(value >> 3));
        OPERATOR_CONTROLLER_CASE(106, 1, setReleaseRate, releaseRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(107, 1, setKeyScaleLevel, keyScaleLevel, value >> 5);
        OPERATOR_CONTROLLER_CASE(81, 1, setEnvelopeScaling, envelopeScaling, value >> 6);
        OPERATOR_CONTROLLER_CASE(76, 1, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
        //OPERATOR_CONTROLLER_CASE(67, 1, setSustain, sustain, value >> 6);
        case 15:
        {
            m_channelData[channel].operatorData[1].velocityToLevel = value >> 1;
            updateAttenuation(channel);
            break;
        }
        // Operator 3
        OPERATOR_CONTROLLER_CASE(20, 2, setWaveform, waveform, value >> 4);
        //OPERATOR_CONTROLLER_CASE(25, 2, setTremolo, tremolo, value >> 6);
        case 25:
            m_channelData[channel].operatorData[2].tremolo = value >> 6;
            break;
        //OPERATOR_CONTROLLER_CASE(30, 2, setVibrato, vibrato, value >> 6);
        case 30:
            m_channelData[channel].operatorData[2].vibrato = value >> 6;
            break;
        //OPERATOR_CONTROLLER_CASE(108, 2, setAttenuation, attenuation, 63 - (value >> 1));
        case 108:
        {
            m_channelData[channel].operatorData[2].level = value >> 1;
            updateAttenuation(channel);
            break;
        }
        OPERATOR_CONTROLLER_CASE(109, 2, setAttackRate, attackRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(110, 2, setDecayRate, decayRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(111, 2, setSustainLevel, sustainLevel, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(112, 2, setReleaseRate, releaseRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(113, 2, setKeyScaleLevel, keyScaleLevel, value >> 5);
        OPERATOR_CONTROLLER_CASE(82, 2, setEnvelopeScaling, envelopeScaling, value >> 6);
        OPERATOR_CONTROLLER_CASE(77, 2, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
        //OPERATOR_CONTROLLER_CASE(68, 2, setSustain, sustain, value >> 6);
        case 16:
        {
            m_channelData[channel].operatorData[2].velocityToLevel = value >> 1;
            updateAttenuation(channel);
            break;
        }

        // Operator 4
        OPERATOR_CONTROLLER_CASE(21, 3, setWaveform, waveform, value >> 4);
        //OPERATOR_CONTROLLER_CASE(26, 3, setTremolo, tremolo, value >> 6);
        case 26:
            m_channelData[channel].operatorData[3].tremolo = value >> 6;
            break;
        //OPERATOR_CONTROLLER_CASE(31, 3, setVibrato, vibrato, value >> 6);
        case 31:
            m_channelData[channel].operatorData[3].vibrato = value >> 6;
            break;
        //OPERATOR_CONTROLLER_CASE(114, 3, setAttenuation, attenuation, 63 - (value >> 1));
        case 114:
        {
            m_channelData[channel].operatorData[3].level = value >> 1;
            updateAttenuation(channel);
            break;
        }
        OPERATOR_CONTROLLER_CASE(115, 3, setAttackRate, attackRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(116, 3, setDecayRate, decayRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(117, 3, setSustainLevel, sustainLevel, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(118, 3, setReleaseRate, releaseRate, 15 - (value >> 3));
        OPERATOR_CONTROLLER_CASE(119, 3, setKeyScaleLevel, keyScaleLevel, value >> 5);
        OPERATOR_CONTROLLER_CASE(83, 3, setEnvelopeScaling, envelopeScaling, value >> 6);
        OPERATOR_CONTROLLER_CASE(78, 3, setFrequencyMultiplicationFactor, frequencyMultiplicationFactor, value >> 3);
        //OPERATOR_CONTROLLER_CASE(69, 3, setSustain, sustain, value >> 6);
        case 17:
        {
            m_channelData[channel].operatorData[3].velocityToLevel = value >> 1;
            updateAttenuation(channel);
            break;
        }

        default:
#if 0
            Serial.print("Unknown controller ");
            Serial.print(controller);
            Serial.println("");
#endif
            break;
    };

    #undef CHANNEL_CONTROLLER_CASE
    #undef OPERATOR_CONTROLLER_CASE
}

void MIDIControl::setPitchBend(
    uint8_t channel,
    uint16_t amount)
{
    // This gives a range from -200 to +200
    int16_t cents = (((int32_t)amount - 8192) * 100) / (amount < 8192 ? 4096 : 4095);
    m_channelData[channel].pitchBend = cents;

    FOR_EACH_PLAYING_NOTE(channel, note,
        m_opl3.setFrequency(note.opl3Channel, getNoteFrequency(note.midiNote, m_channelData[note.midiChannel].pitchBend));
    );
}

void MIDIControl::service()
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
                //Serial.print(note.opl3Channel);
                //Serial.println(" - enabling vibrato");
                for (int op = 0; op < 4; ++ op) {
                    if (m_channelData[note.midiChannel].operatorData[op].vibrato) {
                        m_opl3.setVibrato(note.opl3Channel, op, true);
                    }
                    if (m_channelData[note.midiChannel].operatorData[op].tremolo) {
                        m_opl3.setTremolo(note.opl3Channel, op, true);
                    }
                }
            }

            if (note.releasing) {
                uint16_t newReleaseDuration = note.releaseDuration + elapsedMillis;
                if (newReleaseDuration > 32767) {
                    newReleaseDuration = 32767;
                }

                note.releaseDuration = newReleaseDuration;

                uint16_t expectedReleaseDuration = 0;
                for (int op = 0; op < m_opl3.getOperatorCount(note.opl3Channel); ++ op) {
                    uint16_t expectedOperatorReleaseDuration = 0;

                    switch (m_channelData[note.midiChannel].operatorData[op].releaseRate) {
                        case 0:
                            // Indefinite - keep resetting the duration
                            // The note will only silence when the note slot is re-used due
                            // to no others being available
                            // This doesn't seem useful - TODO: Don't support this value?
                            note.releaseDuration = 0;
                            break;
                        case 1:
                            expectedOperatorReleaseDuration = 18000;
                            break;
                        case 2:
                            expectedOperatorReleaseDuration = 10000;
                            break;
                        case 3:
                            expectedOperatorReleaseDuration = 4500;
                            break;
                        case 4:
                            expectedOperatorReleaseDuration = 2500;
                            break;
                        case 5:
                            expectedOperatorReleaseDuration = 1100;
                            break;
                        case 6:
                            expectedOperatorReleaseDuration = 500;
                            break;
                        case 7:
                            expectedOperatorReleaseDuration = 300;
                            break;
                        case 8:
                            expectedOperatorReleaseDuration = 150;
                            break;
                        case 9:
                            expectedOperatorReleaseDuration = 75;
                            break;
                        default:
                            expectedOperatorReleaseDuration = 50;
                            break;
                    }

                    if (expectedOperatorReleaseDuration > expectedReleaseDuration) {
                        expectedReleaseDuration = expectedOperatorReleaseDuration;
                    }
                }

                if (note.releaseDuration > expectedReleaseDuration) {
                    //Serial.println("Release end");
                    for (int op = 0; op < m_opl3.getOperatorCount(note.opl3Channel); ++ op) {
                        m_opl3.setAttenuation(note.opl3Channel, op, 63);
                    }
                    m_opl3.setFrequency(note.opl3Channel, 0);
                    m_opl3.freeChannel(note.opl3Channel);
                    note.clear();
                    -- m_numberOfPlayingNotes;
                }
            }

#if 0
            if ((newDuration == 32767) || (newDuration % 500 == 0)) {
                Serial.print(noteSlot);
                Serial.print(" --> ");
                Serial.print(newDuration);
                Serial.println("");
            }
#endif
        }
    }
}

void MIDIControl::stopAllNotes(
    uint8_t channel,
    bool immediate)
{
    uint8_t opl3Channel;

    for (int noteSlot = 0; noteSlot < OPL3::NumberOfChannels; ++ noteSlot) {
        opl3Channel = m_playingNotes[noteSlot].opl3Channel;
        if ((channel == m_playingNotes[noteSlot].midiChannel) && (opl3Channel != UnusedOpl3Channel)) {
            if (immediate) {
                // Max attenuation and release rate for all operators
                for (int op = 0; op < 4; ++ op) {
                    m_opl3.setReleaseRate(opl3Channel, op, 15);
                    m_opl3.setAttenuation(opl3Channel, op, 63);
                }
                m_playingNotes[noteSlot].clear();
            } else {
                m_playingNotes[noteSlot].releasing = true;
            }

            m_opl3.keyOff(opl3Channel);
            //opl3.freeChannel(opl3Channel);
        }
    }

    m_numberOfPlayingNotes = 0;
}

void MIDIControl::updateAttenuation(
    uint8_t channel)
{
    FOR_EACH_PLAYING_NOTE(channel, note,
        for (int op = 0; op < m_opl3.getOperatorCount(note.opl3Channel); ++ op) {
            uint16_t operatorLevel = m_channelData[channel].operatorData[op].level;
            uint16_t velocityToLevel = m_channelData[channel].operatorData[op].velocityToLevel;
            uint16_t baseLevel;
            uint16_t velocityLevelRange;
            uint16_t level;

            velocityLevelRange = (velocityToLevel * operatorLevel) / 63;
            baseLevel = operatorLevel - velocityLevelRange;
            level = ((note.velocity >> 1) * velocityLevelRange) / 63;

            //Serial.print("op "); Serial.print(op);
            //Serial.print(" velocityLevelRange:"); Serial.print(velocityLevelRange);
            //Serial.print(" baseLevel:"); Serial.print(baseLevel);
            level += baseLevel;
            //Serial.print(" level:"); Serial.print(level);
            
            // Scale level based on channel volume for carriers
            if (m_opl3.getOperatorType(note.opl3Channel, op) == OPL3::CarrierOperatorType) {
                level = (level * m_channelData[channel].volume) / 63;
            }

            //Serial.print(" -- final:");
            //Serial.println(level);

            m_opl3.setAttenuation(note.opl3Channel, op, 63 - level);
        }
    );
}

void MIDIControl::setTremolo(
    uint8_t channel,
    uint8_t operatorIndex,
    bool enable)
{
    bool wasEnabled = m_channelData[channel].operatorData[operatorIndex].tremolo;
    m_channelData[channel].operatorData[operatorIndex].tremolo = enable;

    if ((!wasEnabled) && (enable)) {
        int lfoStartDelay = m_channelData[channel].lfoStartDelay;
        lfoStartDelay *= 150;

        FOR_EACH_PLAYING_NOTE(channel, note,
            if ((note.duration > lfoStartDelay) && (!note.lfoTriggered)) {
                note.lfoTriggered = true;
                if (operatorIndex < m_opl3.getOperatorCount(note.opl3Channel)) {
                    m_opl3.setTremolo(note.opl3Channel, operatorIndex, true);
                }
            }
        );
    } else if ((wasEnabled) && (!enable)) {
        FOR_EACH_PLAYING_NOTE(channel, note,
            note.lfoTriggered = false;
            if (operatorIndex < m_opl3.getOperatorCount(note.opl3Channel)) {
                m_opl3.setTremolo(note.opl3Channel, operatorIndex, false);
            }
        );
    }
}

void MIDIControl::setVibrato(
    uint8_t channel,
    uint8_t operatorIndex,
    bool enable)
{
    bool wasEnabled = m_channelData[channel].operatorData[operatorIndex].vibrato;
    m_channelData[channel].operatorData[operatorIndex].vibrato = enable;

    if ((!wasEnabled) && (enable)) {
        int lfoStartDelay = m_channelData[channel].lfoStartDelay;
        lfoStartDelay *= 150;

        FOR_EACH_PLAYING_NOTE(channel, note,
            if ((note.duration > lfoStartDelay) && (!note.lfoTriggered)) {
                note.lfoTriggered = true;
                if (operatorIndex < m_opl3.getOperatorCount(note.opl3Channel)) {
                    m_opl3.setVibrato(note.opl3Channel, operatorIndex, true);
                }
            }
        );
    } else if ((wasEnabled) && (!enable)) {
        FOR_EACH_PLAYING_NOTE(channel, note,
            note.lfoTriggered = false;
            if (operatorIndex < m_opl3.getOperatorCount(note.opl3Channel)) {
                m_opl3.setVibrato(note.opl3Channel, operatorIndex, false);
            }
        );
    }
}

#undef FOR_EACH_PLAYING_NOTE
