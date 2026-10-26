// MD5_PseudoRandom port of sub_10037080 / sub_10036A00.
//
// The target hashes the 4-byte command number (one 64-byte block: the
// number, 0x80, zero padding, 32-bit length 32) and returns the dword at
// digest offset 6. Standard MD5; verified against hashlib output for a
// range of seeds (tools/md5_check.cpp).
#include "md5.h"

#include <cstring>

namespace {

inline uint32_t RotateLeft(uint32_t value, uint32_t count) {
    return (value << count) | (value >> (32 - count));
}

uint32_t LoadLE32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

const uint32_t kConstants[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

const uint8_t kShifts[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

} // namespace

uint32_t MD5_PseudoRandom(uint32_t seed) {
    uint8_t block[64];
    std::memset(block, 0, sizeof(block));
    block[0] = static_cast<uint8_t>(seed);
    block[1] = static_cast<uint8_t>(seed >> 8);
    block[2] = static_cast<uint8_t>(seed >> 16);
    block[3] = static_cast<uint8_t>(seed >> 24);
    block[4] = 0x80;
    block[56] = 32; // bit length of the 4-byte message

    uint32_t message[16];
    for (int i = 0; i < 16; ++i)
        message[i] = LoadLE32(block + i * 4);

    uint32_t a = 0x67452301;
    uint32_t b = 0xefcdab89;
    uint32_t c = 0x98badcfe;
    uint32_t d = 0x10325476;

    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t f;
        uint32_t g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }

        f = f + a + kConstants[i] + message[g];
        a = d;
        d = c;
        c = b;
        b = b + RotateLeft(f, kShifts[i]);
    }

    a += 0x67452301;
    b += 0xefcdab89;
    c += 0x98badcfe;
    d += 0x10325476;

    uint8_t digest[16];
    digest[0] = static_cast<uint8_t>(a);
    digest[1] = static_cast<uint8_t>(a >> 8);
    digest[2] = static_cast<uint8_t>(a >> 16);
    digest[3] = static_cast<uint8_t>(a >> 24);
    digest[4] = static_cast<uint8_t>(b);
    digest[5] = static_cast<uint8_t>(b >> 8);
    digest[6] = static_cast<uint8_t>(b >> 16);
    digest[7] = static_cast<uint8_t>(b >> 24);
    digest[8] = static_cast<uint8_t>(c);
    digest[9] = static_cast<uint8_t>(c >> 8);
    digest[10] = static_cast<uint8_t>(c >> 16);
    digest[11] = static_cast<uint8_t>(c >> 24);
    digest[12] = static_cast<uint8_t>(d);
    digest[13] = static_cast<uint8_t>(d >> 8);
    digest[14] = static_cast<uint8_t>(d >> 16);
    digest[15] = static_cast<uint8_t>(d >> 24);

    return LoadLE32(digest + 6);
}
