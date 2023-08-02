/*
    Project:    Canyon
    Purpose:    MIDI control of OPL3 channels/operators
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2023
*/

#ifndef CANYON_MIDICONTROL_H
#define CANYON_MIDICONTROL_H 1

#include "OPL3Hardware.h"

// TODO: Increase this to 16 eventually
#define NUMBER_OF_MIDI_CHANNELS 4

class MIDIControl {
    public:
        MIDIControl(OPL3::Hardware &opl3);
        
        void init();

        void playNote(
            uint8_t channel,
            uint8_t note,
            uint8_t velocity);

        void stopNote(
            uint8_t channel,
            uint8_t note);

        void setController(
            uint8_t channel,
            uint8_t controller,
            uint8_t value);
        
        void setPitchBend(
            uint8_t channel,
            uint16_t amount);

        void service();

    private:
        typedef struct __attribute__((packed)) NoteData {
            void clear()
            {
                midiChannel = 0;
                opl3Channel = UnusedOpl3Channel;
                midiNote = 0;
                velocity = 0;
                duration = 0;
                releaseDuration = 0;
                lfoTriggered = false;
                sustained = false;
                releasing = false;
            }

            unsigned midiChannel    : 4;
            unsigned opl3Channel    : 5;
            unsigned midiNote       : 7;
            unsigned velocity       : 7;
            unsigned duration       : 15;   // in milliseconds
            unsigned releaseDuration: 15;
            unsigned lfoTriggered   : 1;
            unsigned sustained      : 1;    // note off deferred from stopNote until sustain off
            unsigned releasing      : 1;
        } NoteData;

        void stopAllNotes(
            uint8_t channel,
            bool immediate = false);

        void silence(
            NoteData &note);

        void updateAttenuation(
            uint8_t channel);

        void setTremolo(
            uint8_t channel,
            uint8_t operatorIndex,
            bool enable);

        void setVibrato(
            uint8_t channel,
            uint8_t operatorIndex,
            bool enable);

        enum {
            NoNoteSlot = -1,
            UnusedOpl3Channel = 31
        };

        OPL3::Hardware &m_opl3;

        unsigned int m_numberOfPlayingNotes;
        NoteData m_playingNotes[OPL3::NumberOfChannels];

        typedef struct __attribute__((packed)) MidiChannelData {
            OPL3::ChannelType type;
            unsigned outputs        : 2;
            unsigned synthType      : 2;
            unsigned feedbackModulationFactor   : 3;

            struct {
                unsigned level          : 6;
                unsigned attackRate     : 4;
                unsigned decayRate      : 4;
                unsigned sustainLevel   : 4;
                unsigned releaseRate    : 4;
                unsigned keyScaleLevel  : 2;
                unsigned waveform       : 3;
                unsigned frequencyMultiplicationFactor  : 4;
                unsigned tremolo        : 1;
                unsigned vibrato        : 1;
                unsigned envelopeScaling : 1;

                unsigned velocityToLevel : 6;   // How much velocity influences level/attenuation
            } operatorData[4];

            unsigned volume             : 6;
            unsigned lfoStartDelay      : 4;    // Affects vibrato/tremolo
            signed pitchBend            : 9;    // -200 to +200 cents
            unsigned sustaining         : 1;    // Sustain pedal pressed
        } MidiChannelData;

        // Maybe one day we'll support multiple channels
        MidiChannelData m_channelData[NUMBER_OF_MIDI_CHANNELS];

        unsigned long m_previousMillis;
};

#endif
