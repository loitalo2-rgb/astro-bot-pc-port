#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

struct GnfHeader {
    char magic[4];
    uint32_t dataSize;
    uint32_t versionOrFlags;
    uint32_t textureCount;
};

struct GnfTextureEntry {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t dataOffset;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: diag_gnf <arquivo.gnfp>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir: " << argv[1] << "\n";
        return 1;
    }

    GnfHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (std::memcmp(header.magic, "GNF ", 4) != 0) {
        std::cerr << "Erro: Magic GNF invalido!\n";
        return 1;
    }

    std::cout << "Arquivo GNF: " << argv[1] << "\n";
    std::cout << "  Tamanho dos dados: " << header.dataSize << " bytes\n";
    std::cout << "  Quantidade de texturas: " << header.textureCount << "\n";

    // Pula para a tabela de descritores (pos 0x10)
    for(uint32_t i = 0; i < std::min(header.textureCount, 10u); ++i) {
        GnfTextureEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
        std::cout << "  Textura [" << i << "]: "
                  << entry.width << "x" << entry.height
                  << " | Format: 0x" << std::hex << entry.format << std::dec
                  << " | Offset: 0x" << std::hex << entry.dataOffset << std::dec << "\n";
    }

    return 0;
}
