#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

bool IsIdentifierCharacter(unsigned char value) {
    return std::isalnum(value) != 0 || value == '_';
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: scan_scekernel_strings <caminho-para-eboot.bin>\n"
                     "Lista strings ASCII sceKernel e seus offsets no arquivo; "
                     "nao calcula enderecos de funcoes.\n";
        return 1;
    }

    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "Erro: nao foi possivel abrir o arquivo: " << argv[1]
                  << '\n';
        return 1;
    }

    const std::streamoff fileSize = input.tellg();
    if (fileSize < 0 ||
        static_cast<std::uintmax_t>(fileSize) >
            std::numeric_limits<std::size_t>::max() ||
        static_cast<std::uintmax_t>(fileSize) >
            static_cast<std::uintmax_t>(
                std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "Erro: tamanho do arquivo invalido ou grande demais.\n";
        return 1;
    }

    std::vector<unsigned char> bytes(static_cast<std::size_t>(fileSize));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input || input.gcount() !=
                          static_cast<std::streamsize>(bytes.size())) {
            std::cerr << "Erro: leitura incompleta do arquivo.\n";
            return 1;
        }
    }

    constexpr char prefix[] = "sceKernel";
    constexpr std::size_t prefixLength = sizeof(prefix) - 1;
    constexpr std::size_t maxStringLength = 256;
    std::size_t found = 0;

    std::cout << "Arquivo: " << argv[1] << " (" << bytes.size() << " bytes)\n"
              << "Offsets abaixo localizam strings no arquivo, nao funcoes "
                 "executaveis.\n";

    for (std::size_t offset = 0;
         bytes.size() >= prefixLength &&
         offset <= bytes.size() - prefixLength;
         ++offset) {
        if (offset > 0 && IsIdentifierCharacter(bytes[offset - 1])) {
            continue;
        }
        if (!std::equal(prefix, prefix + prefixLength, bytes.begin() + offset)) {
            continue;
        }

        const std::size_t endLimit =
            offset + (bytes.size() - offset < maxStringLength
                          ? bytes.size() - offset
                          : maxStringLength);
        std::size_t end = offset;
        while (end < endLimit && bytes[end] >= 0x20 && bytes[end] <= 0x7e) {
            ++end;
        }

        if (end - offset < prefixLength) {
            continue;
        }

        std::cout << "0x" << std::hex << std::uppercase << std::setw(8)
                  << std::setfill('0') << offset << std::dec << "  "
                  << std::string(bytes.begin() + offset, bytes.begin() + end);
        if (end == endLimit && end < bytes.size() && bytes[end] >= 0x20 &&
            bytes[end] <= 0x7e) {
            std::cout << " [truncada]";
        }
        std::cout << '\n';
        ++found;
    }

    std::cout << "Total: " << found << " string(s) encontrada(s).\n";
    return 0;
}
