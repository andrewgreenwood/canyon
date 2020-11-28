#define OPL3_NUMBER_OF_CHANNELS     18
#define OPL3_NUMBER_OF_OPERATORS    36

#include <stdio.h>
#include <stdint.h>

#include "../OPL3Operator.h"
#include "../OPL3Synth.h"
#include "../OPL3.h"
#include "../ISABus.h"

class OPL3Patch {
    public:

    private:
        OPL3Operator m_operators[4];
};

int main()
{
    ISABus fakeIsa;
    OPL3 opl3(fakeIsa, 0x388);
    OPL3Synth s(opl3);
    unsigned int i;

    /*
    for (i = 0; i <= OPL3_HIHAT_CHANNEL; ++ i) {
        printf("%d == %d\n", i, s.getChannelType(i));
    }
    */

    //for (i = 0; i < 40; ++ i) {
//        printf("%d -- %d\n", i, s.allocateChannel(Melody2OpChannelType));
    //}

    uint8_t ch1 = OPL3_INVALID_CHANNEL;
    uint8_t ch2 = OPL3_INVALID_CHANNEL;
    uint8_t ch3 = OPL3_INVALID_CHANNEL;

    ch1 = s.allocateChannel(Melody2OpChannelType);
    printf("Allocated channel %d\n", ch1);

    printf("%d\n", s.setAttackRate(ch1, 0, 15));
    printf("%d\n", s.setDecayRate(ch1, 0, 7));
    printf("%d\n", s.setSustainLevel(ch1, 0, 4));
    printf("%d\n", s.setReleaseRate(ch1, 0, 10));

    s.setAttenuation(ch1, 0, 1);
    s.setAttenuation(ch1, 1, 2);
}

