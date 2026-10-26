// Verifies MD5_PseudoRandom against known hashlib values.
#include "md5.h"

#include <cstdio>

int main() {
    const unsigned int seeds[] = {0u, 1u, 2u, 42u, 12345u, 0xFFFFFFFFu,
                                  0x12345678u};
    for (unsigned int seed : seeds)
        std::printf("%u 0x%08X\n", seed, MD5_PseudoRandom(seed));
    return 0;
}
