#ifndef CRC_H
#define CRC_H

#include <cstdint>
#include <cstddef>

uint16_t calculate_crc(const unsigned char* data, size_t len);

#endif