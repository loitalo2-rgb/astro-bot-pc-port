#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

struct GnfTextureEntry {
    uint32_t unknown1;
    uint32_t unknown2;
    uint32_t format;
    uint32_t dataOffset;
};

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    std::ifstream file(argv[1], std::ios::binary);
    if (!file.is_open()) return 1;

    file.seekg(0x20, std::ios::beg);

    GnfTextureEntry entry;
    file.read(reinterpret_cast<char*>(&entry), sizeof(entry));

    std::cout << "Descritor de textura em 0x20:\n";
    std::cout << "  Format: 0x" << std::hex << entry.format << "\n";
    std::cout << "  Data Offset Relativo: 0x" << entry.dataOffset << "\n";

    return 0;
}
