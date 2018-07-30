#ifndef MIDI_H
#define MIDI_H 1

#include <stdint.h>

uint8_t getExpectedMidiMessageLength(
    uint8_t status);

struct MIDIMessage {
    uint8_t status;

    union {
        uint8_t data[2];

        struct {
            uint8_t note;
            uint8_t velocity;
        } noteData;

        struct {
            uint8_t controller;
            uint8_t value;
        } controllerData;

        struct {
            uint8_t program;
        } programData;

        // ...
    };
};

#endif
