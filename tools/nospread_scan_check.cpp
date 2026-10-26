// Offline validation of src/nospread_scan.cpp against the installed
// Left 4 Dead 2 client.dll. The file image is mapped at its preferred base
// (or relocated if that address is unavailable) and the real scan code is
// executed. Expected values are the current-install fingerprint:
//   function RVA 0x1ACDB0, seed RVA 0x6BF868 (client.dll 7.9 MB).
#include "nospread_scan.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

uint32_t ReadU32(const uint8_t *p) {
    uint32_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

void WriteU32(uint8_t *p, uint32_t value) {
    std::memcpy(p, &value, sizeof(value));
}

bool ReadFileBytes(const char *path, std::vector<uint8_t> *out) {
    FILE *file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || !file)
        return false;
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (length <= 0) {
        fclose(file);
        return false;
    }
    out->resize(static_cast<size_t>(length));
    const size_t read = fread(out->data(), 1, out->size(), file);
    fclose(file);
    return read == out->size();
}

void ApplyRelocations(uint8_t *image, size_t imageSize,
                      const IMAGE_SECTION_HEADER &section,
                      const uint8_t *fileData, uintptr_t delta) {
    const uint8_t *reloc = fileData + section.PointerToRawData;
    size_t remaining = section.SizeOfRawData;
    while (remaining >= sizeof(IMAGE_BASE_RELOCATION)) {
        const IMAGE_BASE_RELOCATION *block =
            reinterpret_cast<const IMAGE_BASE_RELOCATION *>(reloc);
        if (block->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) ||
            block->SizeOfBlock > remaining)
            break;
        const size_t count =
            (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        const WORD *entries = reinterpret_cast<const WORD *>(reloc +
                                                             sizeof(*block));
        for (size_t i = 0; i < count; ++i) {
            const WORD type = entries[i] >> 12;
            const uint16_t offset = entries[i] & 0x0FFF;
            const size_t target = block->VirtualAddress + offset;
            if (type != IMAGE_REL_BASED_HIGHLOW || target + 4 > imageSize)
                continue;
            WriteU32(image + target, ReadU32(image + target) +
                                        static_cast<uint32_t>(delta));
        }
        reloc += block->SizeOfBlock;
        remaining -= block->SizeOfBlock;
    }
}

} // namespace

int main(int argc, char **argv) {
    const char *path =
        argc > 1 ? argv[1]
                 : "C:\\Program Files (x86)\\Steam\\steamapps\\common\\"
                   "Left 4 Dead 2\\left4dead2\\bin\\client.dll";

    std::vector<uint8_t> file;
    if (!ReadFileBytes(path, &file)) {
        std::printf("FAIL: cannot read %s\n", path);
        return 1;
    }

    const IMAGE_DOS_HEADER *dos =
        reinterpret_cast<const IMAGE_DOS_HEADER *>(file.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        std::printf("FAIL: bad DOS header\n");
        return 1;
    }
    const IMAGE_NT_HEADERS32 *nt =
        reinterpret_cast<const IMAGE_NT_HEADERS32 *>(file.data() +
                                                     dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        std::printf("FAIL: bad NT header\n");
        return 1;
    }

    const uint32_t preferredBase = nt->OptionalHeader.ImageBase;
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    const IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION(nt);

    uint8_t *image = reinterpret_cast<uint8_t *>(VirtualAlloc(
        reinterpret_cast<LPVOID>(static_cast<uintptr_t>(preferredBase)),
        imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    bool atPreferred = image != nullptr;
    if (!image)
        image = reinterpret_cast<uint8_t *>(
            VirtualAlloc(nullptr, imageSize, MEM_RESERVE | MEM_COMMIT,
                         PAGE_READWRITE));
    if (!image) {
        std::printf("FAIL: VirtualAlloc\n");
        return 1;
    }
    std::memset(image, 0, imageSize);

    const size_t headers =
        nt->OptionalHeader.SizeOfHeaders < imageSize
            ? nt->OptionalHeader.SizeOfHeaders
            : imageSize;
    std::memcpy(image, file.data(), headers);

    const IMAGE_SECTION_HEADER *relocSection = nullptr;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const IMAGE_SECTION_HEADER &section = sections[i];
        const size_t copy =
            section.SizeOfRawData < imageSize - section.VirtualAddress
                ? section.SizeOfRawData
                : imageSize - section.VirtualAddress;
        if (copy)
            std::memcpy(image + section.VirtualAddress,
                        file.data() + section.PointerToRawData, copy);
        if (std::memcmp(section.Name, ".reloc", 6) == 0)
            relocSection = &section;
    }

    if (!atPreferred && relocSection) {
        ApplyRelocations(image, imageSize, *relocSection, file.data(),
                         reinterpret_cast<uintptr_t>(image) - preferredBase);
    }

    NoSpreadScanResult result = {};
    const bool ok = NoSpread_ScanModule(image, imageSize, &result);
    if (!ok) {
        std::printf("FAIL: scan did not resolve\n");
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }

    const uintptr_t functionRva =
        reinterpret_cast<uintptr_t>(result.function) -
        reinterpret_cast<uintptr_t>(image);
    const uintptr_t seedRva = reinterpret_cast<uintptr_t>(result.seed) -
                              reinterpret_cast<uintptr_t>(image);
    const uint8_t *function = static_cast<const uint8_t *>(result.function);

    std::printf("mapped at preferred base: %s\n", atPreferred ? "yes" : "no");
    std::printf("function RVA: 0x%X (bytes %02X %02X %02X)\n",
                static_cast<unsigned>(functionRva), function[0], function[1],
                function[2]);
    std::printf("seed RVA    : 0x%X\n", static_cast<unsigned>(seedRva));

    const bool expected =
        functionRva == 0x1ACDB0 && seedRva == 0x6BF868 &&
        seedRva + sizeof(uint32_t) <= imageSize;
    std::printf("expected current-install fingerprint (0x1ACDB0 / 0x6BF868): %s\n",
                expected ? "PASS" : "DIFFERS");
    VirtualFree(image, 0, MEM_RELEASE);
    return expected ? 0 : 1;
}
