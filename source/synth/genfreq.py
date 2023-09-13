# Generate array of 1200 16-bit integer values that represent all frequencies for
# notes in octave 0 along with cent values
#
# Frequencies are in 100ths of hz

import math
from decimal import Decimal

note_names = ['C-', 'C#', 'D-', 'D#', 'E-', 'F-', 'F#', 'G-', 'G#', 'A-', 'A#', 'B-']

# Working with cent values causes some drift away from semitone frequencies
# eventually, so the cent values are generated per-semitone
semitone_scaling = Decimal("1.0594630943592952646")
cent_scaling = Decimal("1.00057779")

semitone = 0
octave = 0
freq = Decimal("16.35160")      # C-0

print("""#include "freq.h"

#ifdef ARDUINO
    #include <avr/pgmspace.h>
#else
    #define PROGMEM
#endif

uint32_t getNoteFrequency(
    uint8_t note,
    int16_t cents)
{
    const PROGMEM static uint16_t frequencyTable[1200] = {""")

while semitone < 12:
    cent = 0
    octave = math.floor(semitone / 12)
    value = int(round(freq, 2) * 100)
    print('        {}, // {}{}'.format(value, note_names[semitone % 12], octave))
    cent_freq = freq

    # Skip the first value as we already output that
    suffix = ','
    while cent < 99:
        cent_freq *= cent_scaling
        value = int(round(cent_freq, 2) * 100)
        if semitone == 11 and cent == 98:
            suffix = ''
        print('        {}{}'.format(value, suffix))
        cent += 1

    freq *= semitone_scaling

    semitone += 1

print("""    };

    int8_t octave;
    uint32_t baseFrequency;
    uint32_t multiplier;
    int index;

    if ((note == 0) && (cents < 0))
        return 0;

    if (note > 127)
        return 0;

    index = (note * 100) + cents;
    octave = (index / 1200) - 1;
    index %= 1200;

//    printf("Index %d octave %d\\n", index, octave);

#ifdef ARDUINO
    baseFrequency = pgm_read_word_near(frequencyTable + index);
#else
    baseFrequency = frequencyTable[index];
#endif

    if (octave >= 0) {
        multiplier = 1 << octave;
        return baseFrequency * multiplier;
    } else {
        return baseFrequency / 2;
    }
}

#ifndef ARDUINO
#include <stdio.h>
int main()
{
    printf("%d\\n", getNoteFrequency(0, -1));
    printf("%d\\n", getNoteFrequency(0, 0));
    printf("%d\\n", getNoteFrequency(0, 1));

    printf("%d\\n", getNoteFrequency(24, -1));
    printf("%d\\n", getNoteFrequency(24, 0));
    printf("%d\\n", getNoteFrequency(24, 1));

    printf("%d\\n", getNoteFrequency(127, -100));
    printf("%d\\n", getNoteFrequency(127, 0));
    printf("%d\\n", getNoteFrequency(127, 100));
}
#endif
""")
