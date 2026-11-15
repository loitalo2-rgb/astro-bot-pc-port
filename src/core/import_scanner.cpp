#include "core/import_scanner.h"

#include "core/eboot_loader.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <unordered_set>

namespace AstroPort {

namespace {

constexpr uint64_t kDtNull = 0;
constexpr uint64_t kDtNeeded = 1;
constexpr uint64_t kDtStrtab = 5;
constexpr uint64_t kDtSymtab = 6;
constexpr uint64_t kDtStrsz = 10;
constexpr uint64_t kDtSyment = 11;
// PS5-specific dynamic tag observed in real eboot.bin/libc.prx: symbol table
// size in bytes (symtab .. jmprel range).
constexpr uint64_t kDtSceSymtabSize = 0x6100003f;
// PS5-specific tag: imported library name (low 32 bits = strtab offset).
constexpr uint64_t kDtSceImportLib = 0x61000049;
constexpr uint64_t kElf64SymSize = 24;
constexpr size_t kMaxNameLength = 4096;

uint64_t ReadLe64(const uint8_t* bytes, size_t size, size_t offset,
                  bool& ok) {
    if (offset > size || size - offset < 8) {
        ok = false;
        return 0;
    }
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

uint32_t ReadLe32(const uint8_t* bytes, size_t size, size_t offset,
                  bool& ok) {
    if (offset > size || size - offset < 4) {
        ok = false;
        return 0;
    }
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint16_t ReadLe16(const uint8_t* bytes, size_t size, size_t offset,
                  bool& ok) {
    if (offset > size || size - offset < 2) {
        ok = false;
        return 0;
    }
    return static_cast<uint16_t>(bytes[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

bool MapVaddr(const std::vector<Elf64Metadata::LoadSegment>& segments,
              uint64_t vaddr, uint64_t bytes, uint64_t& fileOffset) {
    for (const auto& segment : segments) {
        if (vaddr < segment.virtualAddress) continue;
        const uint64_t delta = vaddr - segment.virtualAddress;
        if (delta > segment.fileSize) continue;
        if (bytes > segment.fileSize - delta) continue;
        fileOffset = segment.fileOffset + delta;
        return true;
    }
    return false;
}

bool ReadCString(const uint8_t* bytes, size_t size, size_t tableOffset,
                 size_t tableSize, uint64_t relativeOffset, std::string& out) {
    if (relativeOffset >= tableSize) return false;
    const size_t begin = tableOffset + static_cast<size_t>(relativeOffset);
    const size_t limit =
        std::min(tableOffset + tableSize, static_cast<size_t>(size));
    if (begin >= limit) return false;
    const size_t maxLength = std::min<size_t>(limit - begin, kMaxNameLength);
    const void* terminator = std::memchr(bytes + begin, 0, maxLength);
    if (!terminator) return false;
    const size_t length = static_cast<const uint8_t*>(terminator) -
                          (bytes + begin);
    out.assign(reinterpret_cast<const char*>(bytes + begin), length);
    return true;
}

// Standard base64 alphabet (same one used by the NID encoding).
int Base64Digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace

bool DecodeImportSuffix(const std::string& suffix,
                        const std::vector<std::string>& importLibraries,
                        const std::vector<std::string>& neededModules,
                        ImportSuffixInfo& out) {
    out = ImportSuffixInfo{};
    if (suffix.size() != 3 || suffix[1] != '#') return false;
    const int libraryDigit = Base64Digit(suffix[0]);
    const int moduleDigit = Base64Digit(suffix[2]);
    if (libraryDigit < 0 || moduleDigit < 0) return false;
    if (static_cast<size_t>(libraryDigit) >= importLibraries.size()) {
        return false;
    }
    // Slot 0 is the importing module; DT_NEEDED entries start at 1.
    if (moduleDigit < 1 ||
        static_cast<size_t>(moduleDigit - 1) >= neededModules.size()) {
        return false;
    }
    out.decoded = true;
    out.libraryIndex = static_cast<uint32_t>(libraryDigit);
    out.moduleIndex = static_cast<uint32_t>(moduleDigit);
    out.libraryName = importLibraries[libraryDigit];
    out.moduleName = neededModules[moduleDigit - 1];
    return true;
}

bool ScanElf64Imports(const uint8_t* bytes, size_t size,
                      ImportScanResult& result, std::string& error) {
    result = ImportScanResult{};
    error.clear();
    if (!bytes || size < 64) {
        error = "imagem menor que o cabecalho ELF64";
        return false;
    }

    Elf64Metadata metadata;
    if (!ParseElf64Metadata(bytes, size, metadata) ||
        !metadata.hasDynamicSegment) {
        error = "ELF64 sem PT_DYNAMIC valido";
        return false;
    }
    result.isElf64 = true;

    const Elf64Metadata::LoadSegment& dynamic = metadata.dynamicSegment;
    if (dynamic.fileOffset > size || dynamic.fileSize > size - dynamic.fileOffset) {
        error = "PT_DYNAMIC fora dos limites do arquivo";
        return false;
    }
    if (dynamic.fileSize % 16 != 0) {
        error = "PT_DYNAMIC com tamanho nao multiplo de 16";
        return false;
    }

    const auto readTag = [&](size_t offset, uint64_t& tag, uint64_t& value) {
        bool ok = true;
        tag = ReadLe64(bytes, size, offset, ok);
        value = ReadLe64(bytes, size, offset + 8, ok);
        return ok;
    };

    std::vector<std::pair<uint64_t, uint64_t>> tags;
    bool terminated = false;
    for (size_t offset = 0; offset < dynamic.fileSize; offset += 16) {
        uint64_t tag = 0;
        uint64_t value = 0;
        if (!readTag(static_cast<size_t>(dynamic.fileOffset) + offset, tag,
                     value)) {
            error = "entrada da tabela dinamica fora do arquivo";
            return false;
        }
        if (tag == kDtNull) {
            terminated = true;
            break;
        }
        tags.emplace_back(tag, value);
    }
    if (!terminated) {
        error = "tabela dinamica sem DT_NULL";
        return false;
    }

    const auto findTag = [&](uint64_t wanted, uint64_t& value) {
        for (const auto& [tag, val] : tags) {
            if (tag == wanted) {
                value = val;
                return true;
            }
        }
        return false;
    };

    uint64_t symtabVaddr = 0;
    uint64_t strtabVaddr = 0;
    uint64_t strtabBytes = 0;
    uint64_t syment = 0;
    uint64_t symtabBytes = 0;
    if (!findTag(kDtSymtab, symtabVaddr) ||
        !findTag(kDtStrtab, strtabVaddr) ||
        !findTag(kDtStrsz, strtabBytes) ||
        !findTag(kDtSyment, syment)) {
        error = "tabela dinamica sem SYMTAB/STRTAB/STRSZ/SYMENT";
        return false;
    }
    if (!findTag(kDtSceSymtabSize, symtabBytes)) {
        error = "tabela dinamica sem DT_SCE_SYMTABSZ (0x6100003f)";
        return false;
    }
    if (syment != kElf64SymSize) {
        error = "DT_SYMENT inesperado: " + std::to_string(syment);
        return false;
    }
    if (strtabBytes == 0) {
        error = "DT_STRSZ zero";
        return false;
    }
    if (symtabBytes % kElf64SymSize != 0) {
        error = "DT_SCE_SYMTABSZ nao e multiplo de 24";
        return false;
    }

    uint64_t symtabOffset = 0;
    uint64_t strtabOffset = 0;
    if (!MapVaddr(metadata.loadSegments, symtabVaddr, symtabBytes,
                  symtabOffset)) {
        error = "tabela de simbolos fora dos PT_LOAD";
        return false;
    }
    if (!MapVaddr(metadata.loadSegments, strtabVaddr, strtabBytes,
                  strtabOffset)) {
        error = "tabela de strings fora dos PT_LOAD";
        return false;
    }

    result.symtabVaddr = symtabVaddr;
    result.strtabVaddr = strtabVaddr;
    result.symtabBytes = symtabBytes;
    result.strtabBytes = strtabBytes;
    result.symbolCount = symtabBytes / kElf64SymSize;

    std::unordered_set<uint64_t> uniqueNids;
    for (uint64_t i = 0; i < result.symbolCount; ++i) {
        const size_t entryOffset =
            static_cast<size_t>(symtabOffset + i * kElf64SymSize);
        bool ok = true;
        const uint32_t nameOffset = ReadLe32(bytes, size, entryOffset, ok);
        const uint16_t sectionIndex =
            ReadLe16(bytes, size, entryOffset + 6, ok);
        if (!ok) {
            error = "entrada de simbolo fora do arquivo";
            return false;
        }
        if (nameOffset == 0) continue;  // null symbol / unnamed entry

        std::string name;
        if (!ReadCString(bytes, size, static_cast<size_t>(strtabOffset),
                         static_cast<size_t>(strtabBytes), nameOffset, name)) {
            error = "nome de simbolo fora da string table";
            return false;
        }

        if (sectionIndex != 0) {
            ++result.exportCount;
            continue;
        }

        ++result.importCount;
        ImportedSymbol symbol;
        const size_t hashPos = name.find('#');
        if (hashPos == std::string::npos) {
            symbol.encodedNid = name;
        } else {
            symbol.encodedNid = name.substr(0, hashPos);
            symbol.suffix = name.substr(hashPos + 1);
        }
        if (symbol.encodedNid.size() == 11) {
            symbol.nid = NidResolver::DecodeBase64Nid(symbol.encodedNid);
            symbol.decoded = symbol.nid != 0;
        }
        if (!symbol.decoded) ++result.malformedNames;
        else uniqueNids.insert(symbol.nid);
        result.imports.push_back(std::move(symbol));
    }

    result.uniqueNids = uniqueNids.size();

    for (const auto& [tag, value] : tags) {
        if (tag == kDtNeeded) {
            std::string name;
            if (!ReadCString(bytes, size, static_cast<size_t>(strtabOffset),
                             static_cast<size_t>(strtabBytes), value, name)) {
                error = "DT_NEEDED fora da string table";
                return false;
            }
            result.neededModules.push_back(std::move(name));
        } else if (tag == kDtSceImportLib) {
            const uint32_t nameOffset = static_cast<uint32_t>(value & 0xffffffff);
            std::string name;
            if (!ReadCString(bytes, size, static_cast<size_t>(strtabOffset),
                             static_cast<size_t>(strtabBytes), nameOffset, name)) {
                error = "DT_SCE_IMPORT_LIB fora da string table";
                return false;
            }
            // Keep tag order: the suffix's first digit is an index into this
            // vector, so de-duplicating would shift later indices.
            result.importLibraries.push_back(std::move(name));
        }
    }

    // Decode the "#lib#module" token now that both tables are known.
    for (ImportedSymbol& symbol : result.imports) {
        ImportSuffixInfo info;
        if (DecodeImportSuffix(symbol.suffix, result.importLibraries,
                               result.neededModules, info)) {
            symbol.suffixDecoded = true;
            symbol.libraryIndex = info.libraryIndex;
            symbol.moduleIndex = info.moduleIndex;
            symbol.libraryName = std::move(info.libraryName);
            symbol.moduleName = std::move(info.moduleName);
            ++result.suffixDecodedCount;
        } else {
            ++result.suffixUndecodedCount;
        }
    }

    return true;
}

void EvaluateImportCoverage(const ImportScanResult& scan,
                            const NidResolver& resolver,
                            ImportCoverage& coverage) {
    coverage = ImportCoverage{};
    std::unordered_set<uint64_t> seen;
    for (const ImportedSymbol& symbol : scan.imports) {
        if (!symbol.decoded) continue;
        if (!seen.insert(symbol.nid).second) continue;
        ++coverage.uniqueNids;
        const std::string library =
            symbol.suffixDecoded ? symbol.libraryName : std::string("<unknown>");
        const std::string module =
            symbol.suffixDecoded ? symbol.moduleName : std::string("<unknown>");
        const NidEntry* entry = resolver.FindByHash(symbol.nid);
        if (!entry) {
            ++coverage.missing;
            ++coverage.missingBySuffix[symbol.suffix];
            ++coverage.missingByLibrary[library];
            ++coverage.missingByModule[module];
            coverage.missingSymbols.push_back(symbol);
            continue;
        }
        ++coverage.known;
        ++coverage.knownBySuffix[symbol.suffix][entry->libraryName];
        ++coverage.knownByLibrary[entry->libraryName];
        if (entry->stubAddress != nullptr) ++coverage.implemented;
    }
}

} // namespace AstroPort
