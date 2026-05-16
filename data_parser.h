#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <QMetaType>

struct PMUConfig
{
    uint16_t pmuID = 0;
    uint16_t phasorCount = 0;
    uint16_t analogCount = 0;
    uint16_t digitalCount = 0;
    uint32_t timeBase = 1000000;
    int dataRate = 50;
    bool valid = false;
    std::string stationName;
    std::vector<std::string> phasorLabels;
    std::vector<bool> phasorIsVoltage;
    std::vector<std::string> analogLabels;
};

struct PhasorData
{
    std::string label;
    bool isVoltage = true;
    float magnitude;
    float angleDeg;
};

struct PMUFrame
{
    uint16_t pmuID;
    uint32_t soc;
    uint32_t fracsec;
    uint64_t systemUnixMs = 0;
    double pmuTimestampSeconds = 0.0;
    double latencyMs = 0.0;

    float frequency;
    float rocof;
    bool hasFrequency = false;
    bool hasRocof = false;

    std::vector<PhasorData> phasors;
    std::vector<float> analogs;
};

bool parseConfigFrame(unsigned char* buffer,
                      int length,
                      PMUConfig& config);

double pmuTimestampSeconds(uint32_t soc,
                           uint32_t fracsec,
                           uint32_t timeBase);

bool parseDataFrame(unsigned char* buffer,
                    int length,
                    const PMUConfig& config,
                    PMUFrame& frame);

Q_DECLARE_METATYPE(PMUConfig)
Q_DECLARE_METATYPE(PMUFrame)
