// diag_imports -- offline import/NID catalog for the real eboot.
//
// Rebuilds the SELF to an analysis-only ELF64 image in memory (when needed),
// scans PT_DYNAMIC/dynsym/strtab and reports which NIDs the module imports and
// how many the port already knows or implements. Nothing is executed, no
// import is patched, no game bytes are written.
//
// Usage: diag_imports <eboot.bin|image.elf> [--list-missing N]

#include "core/eboot_loader.h"
#include "core/import_scanner.h"
#include "core/nid_resolver.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kSelfMagic = 0x1D3D154F;
constexpr uint32_t kElfMagic = 0x464C457F;

bool ReadFile(const std::string& path, std::vector<uint8_t>& bytes) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamoff size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<size_t>(size));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (static_cast<size_t>(file.gcount()) != bytes.size()) return false;
    }
    return true;
}

uint32_t ReadLe32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: diag_imports <eboot.bin|image.elf> "
                     "[--list-missing N]\n";
        return 1;
    }
    size_t listMissing = 40;
    if (argc == 4 && std::string(argv[2]) == "--list-missing") {
        listMissing = static_cast<size_t>(std::stoul(argv[3]));
    } else if (argc != 2) {
        std::cerr << "Argumentos invalidos.\n";
        return 1;
    }

    std::vector<uint8_t> fileBytes;
    if (!ReadFile(argv[1], fileBytes) || fileBytes.size() < 4) {
        std::cerr << "Nao foi possivel ler: " << argv[1] << "\n";
        return 1;
    }

    const uint32_t magic = ReadLe32(fileBytes.data());
    std::vector<uint8_t> image;
    const uint8_t* elfBytes = fileBytes.data();
    size_t elfSize = fileBytes.size();
    bool rebuilt = false;

    if (magic == kSelfMagic) {
        AstroPort::Elf64RebuiltInfo info;
        if (!AstroPort::ReconstructElf64FromSelf(fileBytes.data(),
                                                 fileBytes.size(), info)) {
            std::cerr << "SELF recusado pela reconstrucao (fail-closed).\n";
            return 1;
        }
        image = std::move(info.bytes);
        elfBytes = image.data();
        elfSize = image.size();
        rebuilt = true;
    } else if (magic != kElfMagic) {
        std::cerr << "Formato nao reconhecido (esperado SELF PS5 ou ELF64).\n";
        return 1;
    }

    AstroPort::ImportScanResult scan;
    std::string error;
    if (!AstroPort::ScanElf64Imports(elfBytes, elfSize, scan, error)) {
        std::cerr << "Catalogo recusado: " << error << "\n";
        return 1;
    }

    AstroPort::NidResolver::Get().BuildTable();
    AstroPort::ImportCoverage coverage;
    AstroPort::EvaluateImportCoverage(scan, AstroPort::NidResolver::Get(),
                                      coverage);

    std::cout << "\n=== Catalogo de imports (somente leitura) ===\n";
    std::cout << "Arquivo            : " << argv[1] << "\n";
    if (rebuilt) {
        std::cout << "SELF reconstruido  : " << elfSize
                  << " bytes (apenas em memoria)\n";
    }
    std::cout << "Simbolos           : " << scan.symbolCount << "\n";
    std::cout << "Imports            : " << scan.importCount << "\n";
    std::cout << "Exports            : " << scan.exportCount << "\n";
    std::cout << "Nomes malformados  : " << scan.malformedNames << "\n";
    std::cout << "NIDs unicos        : " << scan.uniqueNids << "\n";
    std::cout << "Modulos (NEEDED)   : " << scan.neededModules.size() << "\n";
    std::cout << "Bibliotecas (SCE)  : " << scan.importLibraries.size() << "\n";
    std::cout << "NIDs conhecidos    : " << coverage.known << "\n";
    std::cout << "  com stub nativo  : " << coverage.implemented << "\n";
    std::cout << "NIDs sem mapeamento: " << coverage.missing << "\n";
    if (coverage.uniqueNids != 0) {
        const double percent = 100.0 * static_cast<double>(coverage.known) /
                               static_cast<double>(coverage.uniqueNids);
        std::cout << "Cobertura          : " << std::fixed
                  << std::setprecision(1) << percent << "%\n";
    }

    std::cout << "\nModulos importados:\n";
    for (const std::string& module : scan.neededModules) {
        std::cout << "  " << module << "\n";
    }
    std::cout << "\nBibliotecas importadas:\n";
    for (const std::string& library : scan.importLibraries) {
        std::cout << "  " << library << "\n";
    }

    if (!coverage.missingBySuffix.empty()) {
        std::vector<std::pair<uint64_t, std::string>> groups;
        for (const auto& [suffix, count] : coverage.missingBySuffix) {
            groups.emplace_back(count, suffix);
        }
        std::sort(groups.begin(), groups.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        std::cout << "\nNIDs sem mapeamento por grupo de simbolo:\n";
        for (const auto& [count, suffix] : groups) {
            std::cout << "  #" << suffix << "  " << count;
            const auto hint = coverage.knownBySuffix.find(suffix);
            if (hint != coverage.knownBySuffix.end()) {
                std::cout << "  (conhecidos neste grupo:";
                for (const auto& [library, known] : hint->second) {
                    std::cout << " " << library << "=" << known;
                }
                std::cout << ")";
            }
            std::cout << "\n";
        }
    }

    if (!coverage.missingSymbols.empty()) {
        std::cout << "\nPrimeiros NIDs sem mapeamento (de " << coverage.missing
                  << "):\n";
        const size_t count =
            std::min(listMissing, coverage.missingSymbols.size());
        for (size_t i = 0; i < count; ++i) {
            const AstroPort::ImportedSymbol& symbol =
                coverage.missingSymbols[i];
            std::cout << "  " << symbol.encodedNid << "  (0x" << std::hex
                      << std::setw(16) << std::setfill('0') << symbol.nid
                      << std::dec << std::setfill(' ') << ")";
            if (!symbol.suffix.empty()) std::cout << "  #" << symbol.suffix;
            std::cout << "\n";
        }
        if (coverage.missingSymbols.size() > count) {
            std::cout << "  ... e mais "
                      << coverage.missingSymbols.size() - count << "\n";
        }
    }

    return 0;
}
