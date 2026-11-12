#include "core/import_binding.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <unordered_map>

namespace AstroPort {

namespace {

constexpr uint64_t kDtNull = 0;
constexpr uint64_t kDtStrtab = 5;
constexpr uint64_t kDtSymtab = 6;
constexpr uint64_t kDtRela = 7;
constexpr uint64_t kDtRelasz = 8;
constexpr uint64_t kDtStrsz = 10;
constexpr uint64_t kDtSyment = 11;
constexpr uint64_t kDtPltrel = 20;
constexpr uint64_t kDtJmprel = 23;
constexpr uint64_t kDtPltrelsz = 2;
constexpr uint64_t kDtSceSymtabSize = 0x6100003f;

constexpr uint64_t kRelEntrySize = 24;
constexpr uint64_t kElf64SymSize = 24;
constexpr uint64_t kDtRelaFormat = 7;

constexpr uint32_t kRelNone = 0;
constexpr uint32_t kRel64 = 1;
constexpr uint32_t kRelGlobDat = 6;
constexpr uint32_t kRelJumpSlot = 7;
constexpr uint32_t kRelRelative = 8;

constexpr size_t kMaxNameLength = 4096;

struct Tables {
    uint64_t relaOffset = 0;
    uint64_t relaSize = 0;
    uint64_t jmprelOffset = 0;
    uint64_t jmprelSize = 0;
    uint64_t symtabOffset = 0;
    uint64_t strtabOffset = 0;
    uint64_t strtabSize = 0;
    uint64_t symbolCount = 0;
    bool hasRela = false;
    bool hasJmprel = false;
};

uint64_t ReadLe64(const uint8_t* bytes, size_t size, size_t offset, bool& ok) {
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

uint32_t ReadLe32(const uint8_t* bytes, size_t size, size_t offset, bool& ok) {
    if (offset > size || size - offset < 4) {
        ok = false;
        return 0;
    }
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool MapVaddr(uint64_t virtualBase, size_t imageSize, uint64_t vaddr,
              uint64_t bytes, uint64_t& offset) {
    if (vaddr < virtualBase) return false;
    const uint64_t delta = vaddr - virtualBase;
    if (delta > imageSize || bytes > imageSize - delta) return false;
    offset = delta;
    return true;
}

bool ReadCString(const uint8_t* bytes, size_t size, size_t tableOffset,
                 uint64_t tableSize, uint64_t relativeOffset,
                 std::string& out) {
    if (relativeOffset >= tableSize) return false;
    const size_t begin = tableOffset + static_cast<size_t>(relativeOffset);
    const size_t limit =
        std::min(tableOffset + static_cast<size_t>(tableSize), size);
    if (begin >= limit) return false;
    const size_t maxLength = std::min<size_t>(limit - begin, kMaxNameLength);
    const void* terminator = std::memchr(bytes + begin, 0, maxLength);
    if (!terminator) return false;
    out.assign(reinterpret_cast<const char*>(bytes + begin),
               static_cast<const uint8_t*>(terminator) - (bytes + begin));
    return true;
}

bool ParseTables(const Elf64MappedImage& image, const Elf64Metadata& metadata,
                 Tables& tables, std::string& error) {
    tables = Tables{};
    if (!metadata.hasDynamicSegment) {
        error = "imagem sem PT_DYNAMIC";
        return false;
    }
    const auto& dynamic = metadata.dynamicSegment;
    uint64_t dynamicOffset = 0;
    if (!MapVaddr(image.virtualBase, image.bytes.size(), dynamic.virtualAddress,
                  dynamic.fileSize, dynamicOffset)) {
        error = "PT_DYNAMIC fora da imagem mapeada";
        return false;
    }
    if (dynamic.fileSize % 16 != 0) {
        error = "PT_DYNAMIC com tamanho nao multiplo de 16";
        return false;
    }

    const uint8_t* bytes = image.bytes.data();
    const size_t size = image.bytes.size();

    std::vector<std::pair<uint64_t, uint64_t>> tags;
    bool terminated = false;
    for (uint64_t offset = 0; offset < dynamic.fileSize; offset += 16) {
        bool ok = true;
        const uint64_t tag =
            ReadLe64(bytes, size, static_cast<size_t>(dynamicOffset + offset),
                     ok);
        const uint64_t value = ReadLe64(
            bytes, size, static_cast<size_t>(dynamicOffset + offset + 8), ok);
        if (!ok) {
            error = "entrada da tabela dinamica fora da imagem";
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
    uint64_t syment = 0;
    uint64_t symbolBytes = 0;
    if (!findTag(kDtSymtab, symtabVaddr) || !findTag(kDtStrtab, strtabVaddr) ||
        !findTag(kDtStrsz, tables.strtabSize) ||
        !findTag(kDtSyment, syment) ||
        !findTag(kDtSceSymtabSize, symbolBytes)) {
        error = "tabela dinamica sem SYMTAB/STRTAB/STRSZ/SYMENT/SYMTABSZ";
        return false;
    }
    if (syment != kElf64SymSize) {
        error = "DT_SYMENT inesperado";
        return false;
    }
    if (tables.strtabSize == 0) {
        error = "DT_STRSZ zero";
        return false;
    }
    if (symbolBytes % kElf64SymSize != 0) {
        error = "DT_SCE_SYMTABSZ nao e multiplo de 24";
        return false;
    }
    tables.symbolCount = symbolBytes / kElf64SymSize;
    if (!MapVaddr(image.virtualBase, size, symtabVaddr, symbolBytes,
                  tables.symtabOffset)) {
        error = "tabela de simbolos fora da imagem mapeada";
        return false;
    }
    if (!MapVaddr(image.virtualBase, size, strtabVaddr, tables.strtabSize,
                  tables.strtabOffset)) {
        error = "tabela de strings fora da imagem mapeada";
        return false;
    }

    uint64_t relaSize = 0;
    uint64_t relaEnt = kRelEntrySize;
    if (findTag(kDtRela, tables.relaOffset) ||
        findTag(kDtRelasz, relaSize)) {
        uint64_t relaVaddr = 0;
        if (!findTag(kDtRela, relaVaddr) || !findTag(kDtRelasz, relaSize)) {
            error = "DT_RELA/DT_RELASZ inconsistentes";
            return false;
        }
        if (findTag(9 /* DT_RELAENT */, relaEnt) &&
            relaEnt != kRelEntrySize) {
            error = "DT_RELAENT inesperado";
            return false;
        }
        if (relaSize % kRelEntrySize != 0) {
            error = "DT_RELASZ nao e multiplo de 24";
            return false;
        }
        if (!MapVaddr(image.virtualBase, size, relaVaddr, relaSize,
                      tables.relaOffset)) {
            error = "DT_RELA fora da imagem mapeada";
            return false;
        }
        tables.relaSize = relaSize;
        tables.hasRela = relaSize != 0;
    }

    uint64_t jmprelSize = 0;
    uint64_t pltRel = 0;
    if (findTag(kDtJmprel, tables.jmprelOffset) ||
        findTag(kDtPltrelsz, jmprelSize)) {
        uint64_t jmprelVaddr = 0;
        if (!findTag(kDtJmprel, jmprelVaddr) ||
            !findTag(kDtPltrelsz, jmprelSize)) {
            error = "DT_JMPREL/DT_PLTRELSZ inconsistentes";
            return false;
        }
        if (!findTag(kDtPltrel, pltRel) || pltRel != kDtRelaFormat) {
            error = "DT_PLTREL nao e RELA (7); PLT em formato REL nao suportado";
            return false;
        }
        if (jmprelSize % kRelEntrySize != 0) {
            error = "DT_PLTRELSZ nao e multiplo de 24";
            return false;
        }
        if (!MapVaddr(image.virtualBase, size, jmprelVaddr, jmprelSize,
                      tables.jmprelOffset)) {
            error = "DT_JMPREL fora da imagem mapeada";
            return false;
        }
        tables.jmprelSize = jmprelSize;
        tables.hasJmprel = jmprelSize != 0;
    }

    return true;
}

// Reads one RELA entry from the mapped image.
bool ReadRelocation(const Elf64MappedImage& image, uint64_t tableOffset,
                    uint64_t index, uint64_t& rOffset, uint32_t& type,
                    uint32_t& symbolIndex, int64_t& addend) {
    bool ok = true;
    const size_t offset = static_cast<size_t>(tableOffset + index * kRelEntrySize);
    rOffset = ReadLe64(image.bytes.data(), image.bytes.size(), offset, ok);
    const uint64_t info =
        ReadLe64(image.bytes.data(), image.bytes.size(), offset + 8, ok);
    addend = static_cast<int64_t>(ReadLe64(
        image.bytes.data(), image.bytes.size(), offset + 16, ok));
    if (!ok) return false;
    type = static_cast<uint32_t>(info & 0xffffffffu);
    symbolIndex = static_cast<uint32_t>(info >> 32);
    return true;
}

bool TargetInBounds(const Elf64MappedImage& image, uint64_t rOffset) {
    if (rOffset < image.virtualBase) return false;
    const uint64_t delta = rOffset - image.virtualBase;
    return delta <= image.bytes.size() && 8 <= image.bytes.size() - delta;
}

bool ReadSymbolName(const Elf64MappedImage& image, const Tables& tables,
                    uint32_t symbolIndex, std::string& name) {
    if (symbolIndex >= tables.symbolCount) return false;
    const size_t entry =
        static_cast<size_t>(tables.symtabOffset + symbolIndex * kElf64SymSize);
    bool ok = true;
    const uint32_t nameOffset =
        ReadLe32(image.bytes.data(), image.bytes.size(), entry, ok);
    if (!ok || nameOffset == 0) return false;
    return ReadCString(image.bytes.data(), image.bytes.size(),
                       static_cast<size_t>(tables.strtabOffset),
                       tables.strtabSize, nameOffset, name);
}

bool IterateTables(const Elf64MappedImage& image, const Tables& tables,
                   const std::function<bool(uint64_t, uint32_t, uint32_t,
                                            int64_t)>& visit,
                   std::string& error) {
    if (tables.hasRela) {
        const uint64_t count = tables.relaSize / kRelEntrySize;
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t rOffset = 0;
            uint32_t type = 0;
            uint32_t symbolIndex = 0;
            int64_t addend = 0;
            if (!ReadRelocation(image, tables.relaOffset, i, rOffset, type,
                                symbolIndex, addend)) {
                error = "entrada DT_RELA fora da imagem";
                return false;
            }
            if (!visit(rOffset, type, symbolIndex, addend)) return false;
        }
    }
    if (tables.hasJmprel) {
        const uint64_t count = tables.jmprelSize / kRelEntrySize;
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t rOffset = 0;
            uint32_t type = 0;
            uint32_t symbolIndex = 0;
            int64_t addend = 0;
            if (!ReadRelocation(image, tables.jmprelOffset, i, rOffset, type,
                                symbolIndex, addend)) {
                error = "entrada DT_JMPREL fora da imagem";
                return false;
            }
            if (!visit(rOffset, type, symbolIndex, addend)) return false;
        }
    }
    return true;
}

} // namespace

bool PlanImportBinding(const uint8_t* bytes, size_t size,
                       const Elf64Metadata& metadata,
                       const NidResolver& resolver, ImportBindingPlan& plan,
                       std::string& error) {
    plan = ImportBindingPlan{};
    error.clear();
    if (!bytes || size < 64) {
        error = "imagem menor que o cabecalho ELF64";
        return false;
    }

    Elf64MappedImage image;
    if (!MapElf64LoadSegments(bytes, size, metadata, image)) {
        error = "nao foi possivel mapear os PT_LOAD";
        return false;
    }

    Tables tables;
    if (!ParseTables(image, metadata, tables, error)) return false;

    plan.relaEntries = tables.relaSize / kRelEntrySize;
    plan.jmprelEntries = tables.jmprelSize / kRelEntrySize;

    std::unordered_map<uint32_t, uint64_t> slots;
    std::set<uint32_t> invalidSymbolIndices;

    std::string iterateError;
    const bool iterated = IterateTables(
        image, tables,
        [&](uint64_t rOffset, uint32_t type, uint32_t symbolIndex,
            int64_t /*addend*/) {
            if (!TargetInBounds(image, rOffset)) {
                ++plan.invalidSlots;
                return true;
            }
            switch (type) {
                case kRelRelative:
                    ++plan.relativeEntries;
                    return true;
                case kRel64:
                case kRelGlobDat:
                case kRelJumpSlot:
                    ++plan.symbolicEntries;
                    ++plan.symbolicByType[type];
                    ++slots[symbolIndex];
                    return true;
                case kRelNone:
                    error = "relocation R_X86_64_NONE inesperada";
                    return false;
                default:
                    error = "tipo de relocation 0x" +
                            std::to_string(type) +
                            " nao suportado para binding de imports";
                    return false;
            }
        },
        iterateError);
    if (!iterated) {
        if (error.empty()) error = iterateError;
        return false;
    }

    // Correlate each referenced symbol index with the resolver table.
    for (const auto& [symbolIndex, count] : slots) {
        ImportBindingSymbol symbol;
        symbol.slots = count;
        std::string name;
        if (!ReadSymbolName(image, tables, symbolIndex, name)) {
            invalidSymbolIndices.insert(symbolIndex);
            plan.invalidSlots += count;
            plan.unresolvedSymbols.push_back(std::move(symbol));
            continue;
        }
        const size_t hashPos = name.find('#');
        symbol.encodedNid =
            hashPos == std::string::npos ? name : name.substr(0, hashPos);
        if (hashPos != std::string::npos) symbol.suffix = name.substr(hashPos + 1);
        if (symbol.encodedNid.size() == 11) {
            symbol.nid = NidResolver::DecodeBase64Nid(symbol.encodedNid);
        }
        if (symbol.nid == 0) {
            invalidSymbolIndices.insert(symbolIndex);
            plan.invalidSlots += count;
            plan.unresolvedSymbols.push_back(std::move(symbol));
            continue;
        }
        const NidEntry* entry = resolver.FindByHash(symbol.nid);
        if (!entry) {
            ++plan.unknownSymbols;
            plan.unresolvableSlots += count;
            plan.unresolvedSymbols.push_back(std::move(symbol));
            continue;
        }
        symbol.known = true;
        symbol.libraryName = entry->libraryName;
        symbol.symbolName = entry->symbolName;
        symbol.hasStub = entry->stubAddress != nullptr;
        if (!symbol.hasStub) {
            ++plan.knownWithoutStub;
            plan.unresolvableSlots += count;
            plan.unresolvedSymbols.push_back(std::move(symbol));
            continue;
        }
        ++plan.resolvableSymbols;
        plan.bindableSlots += count;
    }

    plan.distinctSymbols = slots.size();
    std::sort(plan.unresolvedSymbols.begin(), plan.unresolvedSymbols.end(),
              [](const ImportBindingSymbol& a, const ImportBindingSymbol& b) {
                  if (a.slots != b.slots) return a.slots > b.slots;
                  return a.encodedNid < b.encodedNid;
              });
    return true;
}

bool ApplyElf64RelocationsWithImports(const uint8_t* bytes, size_t size,
                                      const Elf64Metadata& metadata,
                                      Elf64MappedImage& image,
                                      const NidResolver& resolver,
                                      ImportBindingPlan& plan,
                                      Elf64RelocationResult& result,
                                      std::string& error) {
    result = {};
    if (!PlanImportBinding(bytes, size, metadata, resolver, plan, error)) {
        return false;
    }
    if (!plan.complete()) {
        error = "vinculacao recusada: " + std::to_string(plan.unresolvableSlots) +
                " slot(s) sem stub e " + std::to_string(plan.invalidSlots) +
                " slot(s) invalido(s); nenhum byte alterado";
        return false;
    }

    Tables tables;
    if (!ParseTables(image, metadata, tables, error)) return false;

    std::unordered_map<uint32_t, uint64_t> resolved;
    // Re-resolve every symbol index exactly as the plan did.
    std::string name;
    for (uint32_t symbolIndex = 0; symbolIndex < tables.symbolCount;
         ++symbolIndex) {
        if (!ReadSymbolName(image, tables, symbolIndex, name)) continue;
        const size_t hashPos = name.find('#');
        const std::string encoded =
            hashPos == std::string::npos ? name : name.substr(0, hashPos);
        if (encoded.size() != 11) continue;
        const uint64_t nid = NidResolver::DecodeBase64Nid(encoded);
        if (nid == 0) continue;
        const NidEntry* entry = resolver.FindByHash(nid);
        if (entry && entry->stubAddress) {
            resolved[symbolIndex] =
                reinterpret_cast<uint64_t>(entry->stubAddress);
        }
    }

    std::string iterateError;
    const bool iterated = IterateTables(
        image, tables,
        [&](uint64_t rOffset, uint32_t type, uint32_t symbolIndex,
            int64_t addend) {
            uint64_t value = 0;
            if (type == kRelRelative) {
                value = image.virtualBase + static_cast<uint64_t>(addend);
            } else {
                const auto it = resolved.find(symbolIndex);
                if (it == resolved.end()) {
                    error = "simbolo sem stub durante a aplicacao";
                    return false;
                }
                value = it->second;
                if (type == kRel64) value += static_cast<uint64_t>(addend);
            }
            const size_t target =
                static_cast<size_t>(rOffset - image.virtualBase);
            for (size_t b = 0; b < 8; ++b) {
                image.bytes[target + b] =
                    static_cast<uint8_t>((value >> (b * 8)) & 0xff);
            }
            if (type == kRelRelative) ++result.relativeApplied;
            else ++result.symbolicApplied;
            return true;
        },
        iterateError);
    if (!iterated) {
        if (error.empty()) error = iterateError;
        return false;
    }
    return true;
}

} // namespace AstroPort
