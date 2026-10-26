// Recovered user utilities: wildcard pattern scanner and console output.
#include "interfaces.h"

#include <psapi.h>

#include <cstdarg>
#include <cstdio>

namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

} // namespace

// 0x100376C0. Scans the named module's mapped image for a wildcard byte
// pattern. Tokens are fixed-width ("XX " / "? "); the target advances by 2 for
// a lone '?' and by 3 otherwise, and treats "??" as a 3-byte token.
uintptr_t FindPattern(std::string moduleName, std::string pattern) {
    HMODULE module = GetModuleHandleA(moduleName.c_str());
    MODULEINFO info = {};
    K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info));

    const unsigned char *cursor =
        reinterpret_cast<const unsigned char *>(module);
    const unsigned char *end = cursor + info.SizeOfImage;
    const char *patternStart = pattern.c_str();
    const char *token = patternStart;
    uintptr_t matchStart = 0;

    while (cursor < end) {
        char current = *token;
        if (!current)
            return matchStart;

        if (current != '?') {
            unsigned char value = static_cast<unsigned char>(
                (HexNibble(token[0]) << 4) | HexNibble(token[1]));
            if (*cursor != value) {
                token = patternStart;
                matchStart = 0;
                ++cursor;
                continue;
            }
        }

        if (!matchStart)
            matchStart = reinterpret_cast<uintptr_t>(cursor);

        if (!token[2])
            return matchStart;

        token += (token[0] == '?' && token[1] != '?') ? 2 : 3;
        ++cursor;
    }
    return 0;
}

// 0x100042D0. UCRT printf wrapper writing to stdout; the target calls
// _stdio_common_vfprintf with the iob(1) stream, which vprintf expands to.
int ConsolePrint(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}
