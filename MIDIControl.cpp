#ifdef ARDUINO
#include <arduino.h>
#endif

#include "MIDIControl.h"

MIDIControl::MIDIControl()
{
    for (int i = 0; i < MIDI_POLYPHONY; ++ i) {
        m_notes[i].sentinel = NULL_NOTE_DATA;
    }
}

bool MIDIControl::playNote(
    uint8_t channel,
    uint8_t note,
    uint8_t velocity)
{
    uint8_t noteIndex;

    if ((channel > 15) || (note > 127) || (velocity > 127)) {
        return false;
    }

    if (findPlayingNote(channel, note, noteIndex)) {
        stopNote(channel, note);
    }

    if (!findFreeNote(channel, noteIndex)) {
        // TODO: Steal a playing note
        return false;
    }

    Serial.print("Allocated note slot ");
    Serial.println(noteIndex, DEC);

    m_notes[noteIndex].opl3Channel = 0;     // TODO
    m_notes[noteIndex].midiChannel = channel;
    m_notes[noteIndex].midiNote = note;

    return true;
}

bool MIDIControl::stopNote(
    uint8_t channel,
    uint8_t note)
{
    uint8_t noteIndex;
    uint8_t i;

    if ((channel > 15) || (note > 127)) {
        return false;
    }

    if (!findPlayingNote(channel, note, noteIndex)) {
        return false;
    }

    for (i = noteIndex + 1; i < MIDI_POLYPHONY; ++ i) {
        m_notes[i - 1].sentinel = m_notes[i].sentinel;
    }

    m_notes[MIDI_POLYPHONY - 1].sentinel = NULL_NOTE_DATA;

    Serial.print("Freed note slot ");
    Serial.println(noteIndex, DEC);

    return true;
}

bool MIDIControl::setProgram(
    uint8_t channel,
    uint8_t program)
{
    if ((channel > 15) || (program > 127)) {
        return false;
    }

    // Not supported yet

    return false;
}

bool MIDIControl::setController(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if ((channel > 15) || (controller > 127) || (value > 127)) {
        return false;
    }

    // Not supported yet

    return false;
}

bool MIDIControl::bendPitch(
    uint8_t channel,
    int16_t amount)
{
//    if ((channel > 15) || (amount < -32768
    // Not supported yet

    return false;
}

bool MIDIControl::findPlayingNote(
    uint8_t channel,
    uint8_t note,
    uint8_t &noteIndex) const
{
    for (uint8_t i = 0; i < MIDI_POLYPHONY; ++ i) {
        if ((m_notes[i].midiChannel == channel)
            && (m_notes[i].midiNote == note)) {
            noteIndex = i;
            return true;
        }
    }

    return false;
}

bool MIDIControl::findFreeNote(
    uint8_t channel,
    uint8_t &noteIndex) const
{
    for (uint8_t i = 0; i < MIDI_POLYPHONY; ++ i) {
        if (m_notes[i].sentinel == NULL_NOTE_DATA) {
            noteIndex = i;
            return true;
        }
    }

    return false;
}
