#ifndef COMMAND_BUILDER_H
#define COMMAND_BUILDER_H

#include <vector>
#include <cstdint>

std::vector<unsigned char> build_command_frame(
    uint16_t pmuId,
    uint16_t commandCode
);

#endif