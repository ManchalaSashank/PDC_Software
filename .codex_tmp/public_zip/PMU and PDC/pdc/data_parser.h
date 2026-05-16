#pragma once
#include <vector>
#include <cstdint>

struct PhasorData
{
    float magnitude;
    float angleDeg;
};

struct PMUFrame
{
    uint16_t pmuID;
    uint32_t soc;
    uint32_t fracsec;

    float frequency;
    float rocof;

    std::vector<PhasorData> phasors;
    std::vector<float> analogs;
};

bool parseDataFrame(unsigned char* buffer,
                    int length,
                    uint16_t phasorCount,
                    uint16_t analogCount,
                    PMUFrame& frame);