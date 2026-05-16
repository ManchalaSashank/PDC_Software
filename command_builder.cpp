#include "command_builder.h"
#include "crc.h"
#include <vector>
#include <ctime>

#define SYNC_CMD 0xAA
#define TYPE_CMD 0x41

void append_uint16_be(std::vector<unsigned char>& buffer, uint16_t value)
{
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void append_uint32_be(std::vector<unsigned char>& buffer, uint32_t value)
{
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

std::vector<unsigned char> build_command_frame(
    uint16_t pmuId,
    uint16_t commandCode)
{
    std::vector<unsigned char> frame;

    frame.push_back(SYNC_CMD);
    frame.push_back(TYPE_CMD);

    append_uint16_be(frame, 0); // placeholder frame size
    append_uint16_be(frame, pmuId);

    // SOC + FRACSEC
    time_t now = time(nullptr);
    append_uint32_be(frame, static_cast<uint32_t>(now));
    append_uint32_be(frame, 0);

    append_uint16_be(frame, commandCode);

    // Now update frame size
    uint16_t frameSize = frame.size() + 2; // + CRC
    frame[2] = (frameSize >> 8) & 0xFF;
    frame[3] = frameSize & 0xFF;

    uint16_t crc = calculate_crc(frame.data(), frame.size());
    append_uint16_be(frame, crc);

    return frame;
}