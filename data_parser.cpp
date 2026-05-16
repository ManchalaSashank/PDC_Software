#include "data_parser.h"

#include "crc.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static uint16_t read_uint16_be(unsigned char* b, int i)
{
    return (b[i] << 8) | b[i + 1];
}

static uint32_t read_uint32_be(unsigned char* b, int i)
{
    return (static_cast<uint32_t>(b[i]) << 24) |
           (b[i + 1] << 16) |
           (b[i + 2] << 8) |
            b[i + 3];
}

static float read_float_be(unsigned char* b, int i)
{
    uint32_t temp =
        (static_cast<uint32_t>(b[i]) << 24) |
        (b[i + 1] << 16) |
        (b[i + 2] << 8) |
         b[i + 3];

    float value;
    std::memcpy(&value, &temp, sizeof(float));
    return value;
}

static bool hasBytes(int length, int index, int bytes)
{
    return index >= 0 && bytes >= 0 && index + bytes <= length;
}

double pmuTimestampSeconds(uint32_t soc, uint32_t fracsec, uint32_t timeBase)
{
    uint32_t fraction = fracsec & 0x00FFFFFF;
    uint32_t base = (timeBase == 0) ? 1000000 : timeBase;
    return static_cast<double>(soc) + (static_cast<double>(fraction) / static_cast<double>(base));
}

bool parseConfigFrame(unsigned char* buffer, int length, PMUConfig& config)
{
    if (length < 30 || buffer[0] != 0xAA)
        return false;

    uint16_t frameSize = read_uint16_be(buffer, 2);
    if (frameSize > length || frameSize < 30)
        return false;

    uint16_t receivedCrc = read_uint16_be(buffer, frameSize - 2);
    uint16_t computedCrc = calculate_crc(buffer, frameSize - 2);
    if (receivedCrc != computedCrc)
        return false;

    PMUConfig parsed;
    parsed.pmuID = read_uint16_be(buffer, 4);
    parsed.timeBase = read_uint32_be(buffer, 14) & 0x00FFFFFF;
    if (parsed.timeBase == 0)
        parsed.timeBase = 1000000;

    uint16_t numPmu = read_uint16_be(buffer, 18);
    if (numPmu == 0)
        return false;

    int index = 20;
    if (!hasBytes(frameSize, index, 24))
        return false;

    parsed.stationName = std::string(reinterpret_cast<char*>(buffer + index), 16);
    parsed.stationName.erase(std::find(parsed.stationName.begin(), parsed.stationName.end(), '\0'), parsed.stationName.end());
    while (!parsed.stationName.empty() && parsed.stationName.back() == ' ')
        parsed.stationName.pop_back();

    index += 16;
    parsed.pmuID = read_uint16_be(buffer, index);
    index += 2;

    index += 2; // FORMAT. The current data parser expects floating-point payloads.

    if (!hasBytes(frameSize, index, 6))
        return false;

    parsed.phasorCount = read_uint16_be(buffer, index);
    index += 2;
    parsed.analogCount = read_uint16_be(buffer, index);
    index += 2;
    parsed.digitalCount = read_uint16_be(buffer, index);
    index += 2;

    int channelNameBytes = 16 * (parsed.phasorCount + parsed.analogCount + (16 * parsed.digitalCount));
    if (!hasBytes(frameSize, index, channelNameBytes))
        return false;

    for (int i = 0; i < parsed.phasorCount; ++i)
    {
        std::string label(reinterpret_cast<char*>(buffer + index + (i * 16)), 16);
        label.erase(std::find(label.begin(), label.end(), '\0'), label.end());
        while (!label.empty() && label.back() == ' ')
            label.pop_back();
        parsed.phasorLabels.push_back(label.empty() ? ("PH" + std::to_string(i + 1)) : label);
    }

    int analogLabelOffset = index + (16 * parsed.phasorCount);
    for (int i = 0; i < parsed.analogCount; ++i)
    {
        std::string label(reinterpret_cast<char*>(buffer + analogLabelOffset + (i * 16)), 16);
        label.erase(std::find(label.begin(), label.end(), '\0'), label.end());
        while (!label.empty() && label.back() == ' ')
            label.pop_back();
        parsed.analogLabels.push_back(label.empty() ? ("Analog " + std::to_string(i + 1)) : label);
    }

    index += channelNameBytes;

    int unitsBytes = (4 * parsed.phasorCount) + (4 * parsed.analogCount) + (4 * parsed.digitalCount);
    if (!hasBytes(frameSize, index, unitsBytes))
        return false;

    for (int i = 0; i < parsed.phasorCount; ++i)
    {
        uint32_t unit = read_uint32_be(buffer, index + (i * 4));
        parsed.phasorIsVoltage.push_back((unit & 0xFF000000) == 0x00000000);
    }

    index += unitsBytes;

    if (!hasBytes(frameSize, index, 6))
        return false;

    index += 2; // FNOM
    index += 2; // CFGCNT

    int16_t dataRate = static_cast<int16_t>(read_uint16_be(buffer, index));
    parsed.dataRate = dataRate == 0 ? 50 : std::abs(dataRate);
    parsed.valid = true;

    config = parsed;
    return true;
}

bool parseDataFrame(unsigned char* buffer, int length,
                    const PMUConfig& config,
                    PMUFrame& frame)
{
    if (length < 16)
        return false;

    if (buffer[0] != 0xAA || buffer[1] != 0x01)
        return false;

    uint16_t receivedCrc = read_uint16_be(buffer, length - 2);
    uint16_t computedCrc = calculate_crc(buffer, length - 2);
    if (receivedCrc != computedCrc)
        return false;

    frame.pmuID = read_uint16_be(buffer, 4);
    frame.soc = read_uint32_be(buffer, 6);
    frame.fracsec = read_uint32_be(buffer, 10);
    frame.pmuTimestampSeconds = pmuTimestampSeconds(frame.soc, frame.fracsec, config.timeBase);

    int index = 16;

    frame.phasors.clear();
    frame.analogs.clear();

    for (int i = 0; i < config.phasorCount; i++)
    {
        if (!hasBytes(length, index, 8))
            return false;

        float magnitude = read_float_be(buffer, index);
        index += 4;

        float angleRad = read_float_be(buffer, index);
        index += 4;

        PhasorData p;
        p.label = config.phasorLabels.size() > i ? config.phasorLabels[i] : ("PH" + std::to_string(i + 1));
        p.isVoltage = config.phasorIsVoltage.size() > i ? config.phasorIsVoltage[i] : true;
        p.magnitude = magnitude;
        p.angleDeg = angleRad * 180.0f / 3.14159265f;

        frame.phasors.push_back(p);
    }

    if (!hasBytes(length, index, 8 + (4 * config.analogCount)))
        return false;

    frame.frequency = read_float_be(buffer, index);
    frame.hasFrequency = std::fabs(frame.frequency) > 0.001f;
    index += 4;

    frame.rocof = read_float_be(buffer, index);
    frame.hasRocof = std::fabs(frame.rocof) > 0.000001f;
    index += 4;

    for (int i = 0; i < config.analogCount; i++)
    {
        float analogVal = read_float_be(buffer, index);
        index += 4;

        frame.analogs.push_back(analogVal);
    }

    return true;
}
