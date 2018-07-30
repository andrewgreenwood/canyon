#include <stdio.h>
#include "../MIDIBuffer.h"

#ifndef ARDUINO
volatile MIDIBuffer buf;

void processMIDIByte(
    uint8_t data)
{
}

int main()
{
    MIDIMessage msg;
    MIDIMessage msg2;

    msg.status = 0x90;
    msg.noteData.note = 0x4c;
    msg.noteData.velocity = 0x7f;
    buf.put(msg);
    // 3

    msg.noteData.note = 0x40;
    msg.noteData.velocity = 0x70;
    buf.put(msg);
    // 2 --> 5

    msg.noteData.note = 0x3a;
    msg.noteData.velocity = 0x7a;
    buf.put(msg);
    // 2 --> 7

    msg.status = 0xc0;
    msg.programData.program = 0x69;
    buf.put(msg);
    // 2 --> 9

    msg.status = 0xc0;
    msg.programData.program = 0x41;
    buf.put(msg);
    // 1 --> 10

    // Should have 10 bytes in the buffer now

    printf("--- reading ---\n");
//    printf("Usage = %d\n", buf.usage());

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);

    msg2.status = 0; msg2.data[0] = 0; msg2.data[1] = 0;
    printf("%d\n", buf.get(msg2));
    printf("%02x %02x %02x\n\n", msg2.status, msg2.data[0], msg2.data[1]);
}
#endif
