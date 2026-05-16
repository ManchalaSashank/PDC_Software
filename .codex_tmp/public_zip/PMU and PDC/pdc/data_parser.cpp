#include "data_parser.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include "crc.h"


using namespace std;

static uint16_t read_uint16_be(unsigned char* b, int i)
{
    return (b[i] << 8) | b[i + 1];
}

static uint32_t read_uint32_be(unsigned char* b, int i)
{
    return (b[i] << 24) |
           (b[i + 1] << 16) |
           (b[i + 2] << 8) |
            b[i + 3];
}

static float read_float_be(unsigned char* b, int i)
{
    uint32_t temp =
        (b[i] << 24) |
        (b[i + 1] << 16) |
        (b[i + 2] << 8) |
         b[i + 3];

    float value;
    memcpy(&value, &temp, sizeof(float));
    return value;
}

bool parseDataFrame(unsigned char* buffer, int length,
                    uint16_t phasorCount,
                    uint16_t analogCount,
                    PMUFrame& frame)
{
    if (length < 16)
        return false;

    if (buffer[0] != 0xAA || buffer[1] != 0x01)
        return false;

    frame.pmuID   = read_uint16_be(buffer, 4);
    frame.soc     = read_uint32_be(buffer, 6);
    frame.fracsec = read_uint32_be(buffer, 10);

    int index = 16;

    frame.phasors.clear();
    frame.analogs.clear();

    // Decode phasors
    for (int i = 0; i < phasorCount; i++)
    {
        float magnitude = read_float_be(buffer, index);
        index += 4;

        float angleRad = read_float_be(buffer, index);
        index += 4;

        float angleDeg = angleRad * 180.0f / 3.14159265f;

        PhasorData p;
        p.magnitude = magnitude;
        p.angleDeg  = angleDeg;

        frame.phasors.push_back(p);
    }

    // Frequency
    frame.frequency = read_float_be(buffer, index);
    index += 4;

    // ROCOF
    frame.rocof = read_float_be(buffer, index);
    index += 4;

    // Analogs
    for (int i = 0; i < analogCount; i++)
    {
        float analogVal = read_float_be(buffer, index);
        index += 4;

        frame.analogs.push_back(analogVal);
    }

    return true;
}