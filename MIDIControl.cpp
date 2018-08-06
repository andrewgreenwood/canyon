#ifdef ARDUINO
    #include <arduino.h>
#else
    #include <cstdio>
#endif

#include "MIDIControl.h"

MIDIControl::MIDIControl(
    OPL3::Hardware &device)
: m_device(device)
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
    uint32_t frequency;
    uint8_t block;
    uint16_t fnum;
    uint8_t opl3Channel;

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

    opl3Channel = m_device.allocateChannel(OPL3::Melody2OpChannelType);
    if (opl3Channel == OPL3::InvalidChannel) {
        return false;
    }

    m_notes[noteIndex].opl3Channel = opl3Channel;
    m_notes[noteIndex].midiChannel = channel;
    m_notes[noteIndex].midiNote = note;

    // TODO: Send actual channel parameters
    m_device.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterA, 0x21);
    m_device.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterB, 0x10);
    m_device.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterC, 0xf0);
    m_device.setOperatorRegister(opl3Channel, 0, OPL3::OperatorRegisterD, 0x74);

    m_device.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterA, 0x21);
    m_device.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterB, 0x10);
    m_device.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterC, 0x70);
    m_device.setOperatorRegister(opl3Channel, 1, OPL3::OperatorRegisterD, 0x74);

    frequency = OPL3::getNoteFrequency(note);
    block = OPL3::getFrequencyBlock(frequency);
    fnum = OPL3::getFrequencyFnum(frequency, block);

    m_device.setChannelRegister(opl3Channel,
                                OPL3::ChannelRegisterA,
                                fnum & 0xff);

    m_device.setChannelRegister(opl3Channel,
                                OPL3::ChannelRegisterB,
                                (fnum >> 8) | (block << 2) | 0x20);

    return true;
}

bool MIDIControl::stopNote(
    uint8_t channel,
    uint8_t note)
{
    uint8_t noteIndex;
    uint32_t frequency;
    uint8_t block;
    uint16_t fnum;
    uint8_t i;

    if ((channel > 15) || (note > 127)) {
        return false;
    }

    if (!findPlayingNote(channel, note, noteIndex)) {
        return false;
    }

    frequency = OPL3::getNoteFrequency(note);
    block = OPL3::getFrequencyBlock(frequency);
    fnum = OPL3::getFrequencyFnum(frequency, block);

    m_device.setChannelRegister(m_notes[noteIndex].opl3Channel,
                                OPL3::ChannelRegisterA,
                                fnum & 0xff);

    m_device.setChannelRegister(m_notes[noteIndex].opl3Channel,
                                OPL3::ChannelRegisterB,
                                (fnum >> 8) | (block << 2));

    m_device.freeChannel(m_notes[noteIndex].opl3Channel);

    for (i = noteIndex + 1; i < MIDI_POLYPHONY; ++ i) {
        m_notes[i - 1].sentinel = m_notes[i].sentinel;
    }

    m_notes[MIDI_POLYPHONY - 1].sentinel = NULL_NOTE_DATA;

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
