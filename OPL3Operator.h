/*
    Project:    Canyon
    Purpose:    OPL3 operator class
    Author:     Andrew Greenwood
    License:    See license.txt
    Date:       July 2018
*/

#ifndef CANYON_OPL3OPERATOR_H
#define CANYON_OPL3OPERATOR_H 1

#include <stdint.h>

namespace OPL3 {

class __attribute__((packed)) Operator {
    public:
        Operator()
        : m_attackRate(0), m_decayRate(0), m_sustainLevel(0), m_releaseRate(0)
        // TODO
        {}

        void enableSustain()
        { m_sustain = true; }

        void disableSustain()
        { m_sustain = false; }

        bool isSustainEnabled() const
        { return m_sustain; }

        void enableEnvelopeScaling()
        { m_envelopeScaling = true; }

        void disableEnvelopeScaling()
        { m_envelopeScaling = false; }

        bool isEnvelopeScalingEnabled() const
        { return m_envelopeScaling; }

        bool setFrequencyMultiplicationFactor(
            uint8_t factor)
        {
            if (factor > 15) {
                return false;
            }

            m_freqMultiplier = factor;
            return true;
        }

        uint8_t getFrequencyMultiplicationFactor() const
        { return m_freqMultiplier; }

        bool setKeyScalingLevel()
        {
            // TODO
            return true;
        }

        uint8_t getKeyScalingLevel()
        {
            // TODO
            return 0;
        }

        bool setAttenuation(
            uint8_t attenuation)
        {
            if (attenuation > 63) {
                return false;
            }

            m_attenuation = attenuation;
            return true;
        }

        uint8_t getAttenuation() const
        { return m_attenuation; }

        bool setAttackRate(
            uint8_t rate)
        {
            if (rate > 15) {
                return false;
            }

            m_attackRate = rate;
            return true;
        }

        uint8_t getAttackRate() const
        { return m_attackRate; }

        bool setDecayRate(
            uint8_t rate)
        {
            if (rate > 15) {
                return false;
            }

            m_decayRate = rate;
            return true;
        }

        uint8_t getDecayRate() const
        { return m_decayRate; }

        bool setSustainLevel(
            uint8_t level)
        {
            if (level > 15) {
                return false;
            }

            m_sustainLevel = level;
            return true;
        }

        uint8_t getSustainLevel() const
        { return m_sustainLevel; }

        bool setReleaseRate(
            uint8_t rate)
        {
            if (rate > 15) {
                return false;
            }

            m_releaseRate = rate;
            return true;
        }

        uint8_t getReleaseRate()
        { return m_releaseRate; }

        bool setWaveform(
            uint8_t waveform)
        {
            if (waveform > 7) {
                return false;
            }

            m_waveform = waveform;
            return true;
        }

        uint8_t getWaveform()
        { return m_waveform; }

    private:
        unsigned m_sustain          : 1;
        unsigned m_envelopeScaling  : 1;
        unsigned m_freqMultiplier   : 4;

        unsigned m_keyScaleLevel    : 2;
        unsigned m_attenuation      : 6;

        unsigned m_attackRate       : 4;
        unsigned m_decayRate        : 4;
        unsigned m_sustainLevel     : 4;
        unsigned m_releaseRate      : 4;

        unsigned m_waveform         : 3;
};

}

#endif
