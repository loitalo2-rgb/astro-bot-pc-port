#include "core/eboot_loader.h"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2 || !argv[1] || argv[1][0] == '\0') {
        std::cerr << "Usage: test_eboot_load <path-to-eboot.bin-or-elf>\n";
        return 2;
    }

    AstroPort::EbootLoader& loader = AstroPort::EbootLoader::Get();
    const bool loadAccepted = loader.Load(argv[1]);
    const AstroPort::LoadedEboot& loaded = loader.GetLoadedInfo();
    std::cout << "Structural load accepted: "
              << (loadAccepted ? "yes" : "no") << '\n'
              << "Bytes: " << loaded.size << '\n'
              << "Raw ELF: " << (loaded.isRawElf ? "yes" : "no") << '\n'
              << "G3PROBE: " << (loaded.isG3Probe ? "yes" : "no") << '\n'
              << "Entry point: 0x" << std::hex << loaded.entryVaddr << '\n'
              << "Code offset: 0x" << loaded.codeOffset << std::dec << '\n';
    std::cout << "The game was not executed.\n";
    return loadAccepted ? 0 : 1;
}
