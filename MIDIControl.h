#ifndef CANYON_MIDICONTROL_H
#define CANYON_MIDICONTROL_H 1

#include <stdint.h>

#define MIDI_CHANNEL_COUNT  16
#define MIDI_POLYPHONY      18

#define NULL_NOTE_DATA      0xffff
typedef union __attribute__((packed)) NoteData {
    // Populated with 0xffff to indicate "no note"
    uint16_t sentinel;

    struct {
        unsigned opl3Channel    : 5;    // 0 ~ 23 (including pseudo-channels)
        unsigned midiChannel    : 4;    // 0 ~ 15
        unsigned midiNote       : 7;    // 0 ~ 127
    };
} NoteData;

class MIDIControl {
    public:
        MIDIControl();

        bool playNote(
            uint8_t channel,
            uint8_t note,
            uint8_t velocity);

        bool stopNote(
            uint8_t channel,
            uint8_t note);

        bool setProgram(
            uint8_t channel,
            uint8_t program);

        bool setController(
            uint8_t channel,
            uint8_t controller,
            uint8_t value);

        bool bendPitch(
            uint8_t channel,
            int16_t amount);

    private:
        bool findPlayingNote(
            uint8_t channel,
            uint8_t note,
            uint8_t &noteIndex) const;

        bool findFreeNote(
            uint8_t channel,
            uint8_t &noteIndex) const;

        NoteData m_notes[MIDI_POLYPHONY];
};

#endif
