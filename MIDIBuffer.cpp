/*
    Project:    Canyon
    Purpose:    MIDI message buffering
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#include "MIDIBuffer.h"

MIDIBuffer::MIDIBuffer()
: m_writeIndex(0), m_readIndex(0), m_usage(0), m_lastPutStatus(0),
  m_lastGetStatus(0)
{
}

void MIDIBuffer::put(
    const MIDIMessage &message) volatile
{
    uint8_t fullLength = getExpectedMidiMessageLength(message.status);
    uint8_t length = fullLength;

    //printf("Usage before: %d\n", m_usage);

    if (length == 0) {
        //printf("Not a supported message\n");
        return;
    }

    if (length > 3) {
        //printf("Message too long\n");
        return;
    }

    if (!(message.status & 0x80)) {
        //printf("Bad status\n");
        return;
    }

    if ((message.data[0] & 0x80) || (message.data[1] & 0x80)) {
        //printf("Bad data\n");
        return;
    }

    if (message.status == m_lastPutStatus) {
        -- length;
    }

    if (m_usage + length > MIDI_BUFFER_SIZE) {
        //printf("Full\n");
        return;
    }

    if (message.status != m_lastPutStatus) {
        putByte(message.status);
        m_lastPutStatus = message.status;
    }

    for (unsigned int i = 0; i < fullLength - 1; ++ i) {
        putByte(message.data[i]);
    }

    if (m_writeIndex == MIDI_BUFFER_SIZE) {
        m_writeIndex = 0;
    }

    m_usage += length;

    //printf("Usage after: %d\n", m_usage);
}

bool MIDIBuffer::get(
    MIDIMessage &message) volatile
{
    uint8_t length;
    uint8_t fullLength;
    uint8_t data;

    //printf("Usage before: %d\n", m_usage);

    // Make sure there is something waiting in the queue
    if (m_usage == 0) {
        return false;
    }

    data = peekByte();

    if (data & 0x80) {
        m_lastGetStatus = data;
    }

    fullLength = getExpectedMidiMessageLength(m_lastGetStatus);
    length = fullLength;

    // Using last status byte so the length will be shorter
    if (!(data & 0x80)) {
        -- length;
    }

    // Make sure the rest of the message is present
    if (m_usage < length) {
        //printf("Not enough data\n");
        return false;
    }

    message.status = m_lastGetStatus;

    if (data & 0x80) {
        getByte();
    }

    for (int i = 0; i < fullLength - 1; ++ i) {
        message.data[i] = getByte();
    }

    m_usage -= length;
    //printf("Usage after: %d\n", m_usage);

    return true;
}

bool MIDIBuffer::peek(
    MIDIMessage &message) const volatile
{
    if (m_usage == 0) {
        return false;
    }

    //message.status = m_buffer[m_readIndex].status;
    //message.data[0] = m_buffer[m_readIndex].data[0];
    //message.data[1] = m_buffer[m_readIndex].data[1];

    return true;
}

uint16_t MIDIBuffer::capacity() const volatile
{
    return MIDI_BUFFER_SIZE;
}

uint16_t MIDIBuffer::usage() const volatile
{
    return m_usage;
}

bool MIDIBuffer::hasContent() const volatile
{
    return m_usage > 0;
}

void MIDIBuffer::putByte(
    uint8_t data) volatile
{
    if (m_usage == MIDI_BUFFER_SIZE) {
        //printf("FULL\n");
        return;
    }

    m_buffer[m_writeIndex] = data;

    if (++ m_writeIndex == MIDI_BUFFER_SIZE) {
        m_writeIndex = 0;
    }
}

uint8_t MIDIBuffer::getByte() volatile
{
    uint8_t data;

    if (m_usage == 0) {
        //printf("EMPTY\n");
        return 0;
    }

    data = m_buffer[m_readIndex];

    if (++ m_readIndex == MIDI_BUFFER_SIZE) {
        m_readIndex = 0;
    }

    return data;
}

uint8_t MIDIBuffer::peekByte() volatile
{
    uint8_t data;

    if (m_usage == 0) {
        //printf("EMPTY\n");
        return 0;
    }

    data = m_buffer[m_readIndex];

    return data;
}
