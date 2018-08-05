#ifndef CANYON_OPL3PATCH_H
#define CANYON_OPL3PATCH_H 1

#include "OPL3Operator.h"

namespace OPL3 {

const uint8_t OperatorsPerChannel = 4;

typedef enum {
    NoOutput     = 0x0,
    LeftOutput   = 0x1,
    RightOutput  = 0x2,
    StereoOutput = 0x3
} OutputType;

class Patch {
    public:
        Patch()
        : m_output(StereoOutput),
          m_feedbackModulationFactor(0),
          m_synthType(0)
        {
        }

        Operator *getOperator(
            uint8_t operatorIndex)
        {
            if (operatorIndex >= OperatorsPerChannel) {
                return NULL;
            }

            return &m_operators[operatorIndex];
        }

        bool setOutput(
            uint8_t output)
        {
            if (output > 3) {
                return false;
            }

            m_output = output;
            return true;
        }

        uint8_t getOutput()
        { return m_output; }

        bool setFeedbackModulationFactor(
            uint8_t factor)
        {
            if (factor > 7) {
                return false;
            }

            m_feedbackModulationFactor = factor;
            return true;
        }

        uint8_t getFeedbackModulationFactor()
        { return m_feedbackModulationFactor; }

        bool setSynthType(
            uint8_t type)
        {
            if (type > 7) {
                return false;
            }

            m_synthType = type;
            return true;
        }

        uint8_t getSynthType()
        { return m_synthType; }

    private:
        Operator m_operators[OperatorsPerChannel];

        unsigned m_output : 2;
        unsigned m_feedbackModulationFactor : 3;

        // 2 modes for 2-op channels, 4 more for 4-op channels
        unsigned m_synthType : 3;
};

}

#endif
