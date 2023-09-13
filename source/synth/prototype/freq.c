#include <stdio.h>
#include <stdint.h>

const uint16_t freqBlockTable[12] = {
    0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x15e, 0x202, 0x241, 0x263, 0x287
};

const uint16_t lowestFrequency = 0x156;
const uint16_t highestFrequency = 0x2ae;


void calcFrequencyAndBlock(
    uint8_t note,
    uint16_t *frequency,
    uint8_t *block)
{
    *block = note / 12;
    *frequency = freqBlockTable[note % 12];
}

int main() {
    uint16_t frequency
}
