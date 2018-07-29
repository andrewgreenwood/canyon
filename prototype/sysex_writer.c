#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>
#include <malloc.h>

#define DEVICE_ID   2

HMIDIOUT handle;
uint8_t buffer[7];

HANDLE event;

void opl3Write(
    uint8_t registerSet,
    uint8_t opl3Register,
    uint8_t data)
{
    int i;
    MIDIHDR buffer_header;
    MMRESULT result;

    buffer_header.lpData = buffer;
    buffer_header.dwBufferLength = 7;
    buffer_header.dwBytesRecorded = 0;
    buffer_header.dwUser = 0;
    buffer_header.dwFlags = 0;
    buffer_header.lpNext = 0;
    buffer_header.reserved = 0;
    buffer_header.dwOffset = 0;

    result = midiOutPrepareHeader(handle, &buffer_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        printf("midiOutPrepareHeader - error %u\n", result);
    }

    buffer[0] = 0xf0;
    buffer[1] = 0x7d;

    buffer[2]  = registerSet == 1 ? 0x10 : 0x00;
    buffer[2] |= opl3Register >> 4;

    buffer[3] = opl3Register & 0x0f;

    buffer[4] = data >> 4;
    buffer[5] = data & 0x0f;

    buffer[6] = 0xf7;

    /*
    for (i = 0; i < 7; ++ i) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
    */

    do {
        ResetEvent(event);
        result = midiOutLongMsg(handle, &buffer_header, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR) {
            printf("midiOutLongMsg - error %u\n", result);
        }
        WaitForSingleObject(event, INFINITE);
    } while (!(buffer_header.dwFlags & MHDR_DONE));


    result = midiOutUnprepareHeader(handle, &buffer_header, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR) {
        printf("midiOutUnprepareHeader - error %u\n", result);
    }

    Sleep(4);
}


// Frequencies of notes within each block
const uint16_t freqBlockTable[12] = {
    0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287
};

const uint16_t lowestFrequency = 0x156;
const uint16_t highestFrequency = 0x2ae;


void calcFrequencyAndBlock(
    uint8_t note,
    uint16_t *frequency,
    uint8_t *block)
{
    // Maximum note
    if (note > 95) {
        return;
    }

    *block = note / 12;
    *frequency = freqBlockTable[note % 12];
    //*frequency = 0x157;

    printf("%d --> %x %x\n", note, *block, *frequency);
}


int main()
{
    MMRESULT result;
    uint8_t block;
    uint16_t fnum;

    event = CreateEvent(NULL, TRUE, FALSE, NULL);
    printf("Event is %d\n", event);

    result = midiOutOpen(&handle, DEVICE_ID, (DWORD_PTR)event, 0, CALLBACK_EVENT);
    if (result != MMSYSERR_NOERROR) {
        printf("midiOutError - error %u\n", result);
    }

    printf("Wait --> %d\n", WaitForSingleObject(event, INFINITE));

    // Reset everything!
    int i;
    for (i = 0x20; i <= 0xff; ++ i) {
        //opl3Write(0, i, 0x00);
    }

    opl3Write(0, 0x20, 0x01);
    opl3Write(0, 0x40, 0x18);
    opl3Write(0, 0x60, 0xf0);   // Attack/Decay
    opl3Write(0, 0x80, 0x77);   // Sustain/Release
    opl3Write(0, 0xa0, 0x98);
    opl3Write(0, 0x23, 0x01);
    opl3Write(0, 0x43, 0x00);
    opl3Write(0, 0x63, 0x70);   // Attack/Decay

    //uint8_t pitch = 0x98;
    uint8_t note = 0;
    uint8_t byte_b0 = 0;

    for (;;) {
        printf("%d\n", note);
        calcFrequencyAndBlock(note, &fnum, &block);
        byte_b0 = 0x20 | (fnum >> 8) | (block << 2);

        opl3Write(0, 0xa0, fnum);
        opl3Write(0, 0xb0, byte_b0);

        //opl3Write(0, 0xa0, pitch);
        opl3Write(0, 0x83, 0x41);   // Sustain/Release (carrier)
        //opl3Write(0, 0xb0, 0x34);

        Sleep(2);

//        byte_b0 &= 0x1f;
//        opl3Write(0, 0xb0, byte_b0);

        note ++;
        if (note > 95) {
            note = 0;
        }

        //pitch += 10;

/*        switch (pitch) {
            case 0x98:
                pitch = 0x48;
                break;

            case 0x48:
                pitch = 0x28;
                break;

            case 0x28:
                pitch = 0xb8;
                break;

            default:
                pitch = 0x98;
        }*/

        Sleep(10);
        //opl3Write(0, 0x83, 0x7f);
        //opl3Write(0, 0xa0, 0xff);
        //Sleep(200);
    }

    /*
    int j;
    for (;;) {
        printf("Sending\n");
        for (j = 0; j < 16; ++ j) {
            midiOutShortMsg(handle, 0x007f4e90);
        }
        Sleep(100);
    }
    */


    result = midiOutClose(handle);
    if (result != MMSYSERR_NOERROR) {
        printf("midiOutClose - error %u\n", result);
    }

    printf("Wait --> %d\n", WaitForSingleObject(event, INFINITE));

    return 0;
}
