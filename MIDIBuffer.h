#ifndef CANYON_MIDIBUFFER_H
#define CANYON_MIDIBUFFER_H 1

#include <stdint.h>
#include "MIDI.h"

#define MIDI_BUFFER_SIZE    16

class MIDIBuffer {
    public:
        MIDIBuffer();

        void put(
            const MIDIMessage &message) volatile;

        bool get(
            MIDIMessage &message) volatile;

        bool peek(
            MIDIMessage &message) const volatile;

        uint16_t capacity() const volatile;

        uint16_t usage() const volatile;

        bool hasContent() const volatile;

    private:
        void putByte(
            uint8_t data) volatile;

        uint8_t getByte() volatile;

        uint8_t peekByte() volatile;

        volatile uint8_t m_buffer[MIDI_BUFFER_SIZE];
        volatile uint16_t m_writeIndex;
        volatile uint16_t m_readIndex;
        volatile uint16_t m_usage;
        uint8_t m_lastPutStatus;
        uint8_t m_lastGetStatus;
};

#endif
