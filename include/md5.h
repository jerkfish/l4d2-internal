// Valve's MD5_PseudoRandom, recovered from sub_10037080 (0x10037080):
// MD5 over the 4-byte little-endian seed, then the dword at digest offset 6.
#pragma once

#include <cstdint>

uint32_t MD5_PseudoRandom(uint32_t seed);
