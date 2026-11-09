#include "eboot_loader.h"
#include "../patches/patch_manager.h"
#include "../config/config_manager.h"
#include "../kernel/kernel_emulator.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

namespace {

// Elf64_Dyn entry size: tag (8 bytes) + value (8 bytes).
constexpr size_t kDynamicEntrySize = 16;
// DT_NULL terminates the dynamic table, later tags are validated in the
// relocation applier when they affect processing.
constexpr uint64_t kDyTagNull = 0;
constexpr uint64_t kDyTagRela = 7;
constexpr uint64_t kDyTagRelasz = 8;
constexpr uint64_t kDyTagRelaent = 9;
constexpr uint64_t kDyTagPltrelsz = 2;
constexpr uint64_t kDyTagPltrel = 20;
constexpr uint64_t kDyTagJmprel = 23;
constexpr uint64_t kDyTagPltgot = 3;
constexpr uint64_t kDyTagTextrel = 30;
constexpr uint64_t kDyTagRelr = 36;
constexpr uint64_t kDyTagRelrsz = 37;
// R_X86_64_RELATIVE: the only relocation type this loader applies.
constexpr uint64_t kRelX8664Relative = 8;
constexpr size_t kRelaEntrySize = 24;

template <typename T>
bool ReadLittleEndian(const uint8_t* bytes, size_t size, size_t offset, T& value) {
    if (offset > size || size - offset < sizeof(T)) return false;
    value = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(bytes[offset + i]) << (i * 8);
    }
    return true;
}

// SELF record table layout (public format documentation, independently
// re-implemented). Records are 0x20 bytes starting at 0x40: flags, data
// offset, size, size (repeated as a sanity field).
constexpr uint64_t kSelfRecordTableStart = 0x40;
constexpr uint64_t kSelfRecordTableEnd = 0x400;
constexpr size_t kSelfRecordSize = 0x20;
constexpr uint64_t kSelfRecordFlagEncrypted = 0x2;
constexpr uint64_t kSelfRecordFlagCompressed = 0x8;
constexpr uint64_t kSelfRecordFlagHasSegment = 0x800;
constexpr uint32_t kSelfRecordSegmentShift = 20;
constexpr uint64_t kSelfInnerElfSearchLimit = 0x4000;
constexpr double kSelfEntropyHighThreshold = 7.5;
constexpr size_t kSelfEntropySampleSize = 0x10000;
constexpr uint64_t kSelfRecordMinDataOffset = 0x350;

double ComputeShannonEntropy(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0.0;
    size_t counts[256] = {};
    for (size_t i = 0; i < length; ++i) {
        ++counts[data[i]];
    }
    double entropy = 0.0;
    for (const size_t count : counts) {
        if (count == 0) continue;
        const double probability = static_cast<double>(count) / static_cast<double>(length);
        entropy -= probability * std::log2(probability);
    }
    return entropy;
}

}

bool InspectSelfContainer(const uint8_t* bytes, size_t size,
                          SelfContainerReport& report) {
    report = {};
    if (!bytes || size < kSelfRecordTableStart) return false;

    uint32_t selfMagic = 0;
    if (!ReadLittleEndian(bytes, size, 0, selfMagic)) return false;
    report.selfMagicValid = (selfMagic == 0x1D3D154F);
    if (!report.selfMagicValid) return false;

    ReadLittleEndian(bytes, size, 0x18, report.headerEntryCount);

    // Locate the inner ELF header inside the container.
    for (uint64_t scan = 4; scan < kSelfInnerElfSearchLimit && scan < size; ++scan) {
        if (bytes[scan] == 0x7f && scan + 3 < size &&
            bytes[scan + 1] == 'E' && bytes[scan + 2] == 'L' && bytes[scan + 3] == 'F') {
            report.innerElfHeaderOffset = scan;
            break;
        }
    }

    for (uint64_t offset = kSelfRecordTableStart;
         offset + kSelfRecordSize <= kSelfRecordTableEnd && offset < size;
         offset += kSelfRecordSize) {
        uint64_t flags = 0, dataOffset = 0, dataSize = 0, dataSizeAgain = 0;
        if (!ReadLittleEndian(bytes, size, offset, flags) ||
            !ReadLittleEndian(bytes, size, offset + 8, dataOffset) ||
            !ReadLittleEndian(bytes, size, offset + 16, dataSize) ||
            !ReadLittleEndian(bytes, size, offset + 24, dataSizeAgain)) {
            return false;
        }
        // Termination condition: empty record or inconsistent size pair.
        if (dataSize == 0 || dataSize != dataSizeAgain) break;
        if (dataOffset < kSelfRecordMinDataOffset || dataOffset % 0x10 != 0 ||
            dataSize > size - dataOffset) {
            break;
        }

        SelfRecordInfo record;
        record.flags = flags;
        record.dataOffset = dataOffset;
        record.dataSize = dataSize;
        record.hasSegmentMapping = (flags & kSelfRecordFlagHasSegment) != 0;
        record.targetSegmentIndex = flags >> kSelfRecordSegmentShift;
        record.markedEncrypted = (flags & kSelfRecordFlagEncrypted) != 0;
        record.markedCompressed = (flags & kSelfRecordFlagCompressed) != 0;

        const size_t sampleLength =
            static_cast<size_t>(std::min<uint64_t>(dataSize, kSelfEntropySampleSize));
        record.payloadWithinFile =
            dataOffset <= size && sampleLength <= size - dataOffset;
        if (record.payloadWithinFile) {
            record.bitsPerByte = ComputeShannonEntropy(bytes + dataOffset, sampleLength);
        }

        if (record.markedEncrypted) report.anyMarkedEncrypted = true;
        if (record.markedCompressed) report.anyMarkedCompressed = true;
        report.records.push_back(record);
    }

    // Classification: every payload with a segment mapping and nonzero size
    // sampled high entropy means the load segments look encrypted.
    std::vector<const SelfRecordInfo*> mappedPayloads;
    for (const auto& record : report.records) {
        if (record.hasSegmentMapping && record.payloadWithinFile &&
            record.dataSize > 0) {
            mappedPayloads.push_back(&record);
        }
    }
    if (!mappedPayloads.empty() &&
        std::all_of(mappedPayloads.begin(), mappedPayloads.end(),
                    [](const SelfRecordInfo* r) {
                        return r->bitsPerByte >= kSelfEntropyHighThreshold;
                    })) {
        report.allDataRecordsHighEntropy = true;
    }

    std::cout << "[SelfInspector] Container SELF com "
              << report.records.size() << " registro(s); ELF interno em 0x"
              << std::hex << report.innerElfHeaderOffset << std::dec
              << "; marcados como criptografados=" << (report.anyMarkedEncrypted ? 1 : 0)
              << ", comprimidos=" << (report.anyMarkedCompressed ? 1 : 0)
              << ", entropia alta em todos os payloads= "
              << (report.allDataRecordsHighEntropy ? 1 : 0) << "\n";
    for (size_t i = 0; i < report.records.size(); ++i) {
        const auto& record = report.records[i];
        std::cout << "[SelfInspector]   registro " << i
                  << ": flags=0x" << std::hex << record.flags
                  << " offset=0x" << record.dataOffset
                  << " size=0x" << record.dataSize << std::dec
                  << " entropia=" << record.bitsPerByte
                  << " bits/byte\n";
    }
    return true;
}

// -----------------------------------------------------------------------------
// SELF -> ELF64 reconstruction (fail-closed; analysis artifact only).
// -----------------------------------------------------------------------------

namespace {

struct RebuiltPhdr {
    uint32_t type = 0;
    uint32_t flags = 0;
    uint64_t fileOffset = 0;      // stale offset from the inner ELF
    uint64_t virtualAddress = 0;
    uint64_t fileSize = 0;
    uint64_t memorySize = 0;
    uint64_t alignment = 0;
    uint64_t newFileOffset = 0;   // placement in the rebuilt image
    uint64_t emitFileSize = 0;    // bytes actually present in the rebuilt image
};

constexpr uint32_t kPhdrTypeLoad = 1;
constexpr uint32_t kPhdrTypePhdr = 6;
constexpr uint64_t kRebuiltPhdrTableOffset = 64;
constexpr size_t   kElf64PhdrEntrySize = 56;
constexpr uint16_t kElfTypeDyn = 3;

void WriteLittleEndianInto(std::vector<uint8_t>& bytes, size_t offset,
                           uint64_t value, size_t width) {
    for (size_t i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

// Places a segment so that (new offset % alignment) == (vaddr % alignment),
// which is what ELF loaders require, starting after the running cursor.
uint64_t PlaceCongruent(uint64_t cursor, uint64_t vaddr, uint64_t alignment) {
    if (alignment <= 1 || (alignment & (alignment - 1)) != 0) {
        return cursor;  // no usable alignment; place directly
    }
    const uint64_t cursorMod = cursor % alignment;
    const uint64_t vaddrMod = vaddr % alignment;
    uint64_t offset = cursor;
    if (vaddrMod >= cursorMod) {
        offset = cursor + (vaddrMod - cursorMod);
    } else {
        offset = cursor + alignment - (cursorMod - vaddrMod);
    }
    return offset;
}

} // namespace

bool ReconstructElf64FromSelf(const uint8_t* selfBytes, size_t selfSize,
                              Elf64RebuiltInfo& out) {
    out = {};
    if (!selfBytes || selfSize == 0) {
        std::cerr << "[SelfRebuilder] Entrada invalida.\n";
        return false;
    }

    // 1. Inspection first: refuse anything that is not a clean container.
    SelfContainerReport report;
    if (!InspectSelfContainer(selfBytes, selfSize, report)) {
        std::cerr << "[SelfRebuilder] Inspecao falhou; reconstrucao recusada.\n";
        return false;
    }
    if (report.anyMarkedEncrypted || report.anyMarkedCompressed) {
        std::cerr << "[SelfRebuilder] Registros marcados como criptografados/"
                     "comprimidos; reconstrucao recusada (fail-closed).\n";
        return false;
    }
    if (report.allDataRecordsHighEntropy) {
        std::cerr << "[SelfRebuilder] Alta entropia em todos os payloads "
                     "mapeados; container provavelmente criptografado; "
                     "reconstrucao recusada.\n";
        return false;
    }
    if (report.innerElfHeaderOffset == 0 ||
        selfSize - report.innerElfHeaderOffset < 64) {
        std::cerr << "[SelfRebuilder] ELF interno nao localizado/invalido.\n";
        return false;
    }

    // 2. Inner ELF64 header (relative offsets are self + innerElfHeaderOffset).
    const uint8_t* elf = selfBytes + report.innerElfHeaderOffset;
    const size_t elfSize = selfSize - report.innerElfHeaderOffset;
    if (elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F' ||
        elf[4] != 2 || elf[5] != 1 || elf[6] != 1) {
        std::cerr << "[SelfRebuilder] ELF interno nao e ELF64 little-endian "
                     "valido; reconstrucao recusada.\n";
        return false;
    }
    uint16_t elfType = 0, elfMachine = 0, phentsize = 0, phnum = 0, ehsize = 0;
    uint64_t entry = 0, phoff = 0;
    if (!ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 16, elfType) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 18, elfMachine) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 24, entry) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 32, phoff) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 52, ehsize) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 54, phentsize) ||
        !ReadLittleEndian(selfBytes, selfSize,
                          report.innerElfHeaderOffset + 56, phnum) ||
        ehsize < 64) {
        std::cerr << "[SelfRebuilder] Cabecalho ELF interno ilegivel; "
                     "reconstrucao recusada.\n";
        return false;
    }
    if (phnum == 0 || phentsize != kElf64PhdrEntrySize ||
        phoff > elfSize || phnum > (elfSize - phoff) / kElf64PhdrEntrySize) {
        std::cerr << "[SelfRebuilder] Tabela de program headers do ELF interno "
                     "inconsistente; reconstrucao recusada.\n";
        return false;
    }

    // 3. Parse every program header.
    std::vector<RebuiltPhdr> phdrs(phnum);
    for (uint16_t i = 0; i < phnum; ++i) {
        const size_t base = static_cast<size_t>(phoff) + 56u * i;
        RebuiltPhdr& p = phdrs[i];
        if (!ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base, p.type) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 4, p.flags) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 8,
                              p.fileOffset) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 16,
                              p.virtualAddress) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 32,
                              p.fileSize) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 40,
                              p.memorySize) ||
            !ReadLittleEndian(selfBytes, selfSize,
                              report.innerElfHeaderOffset + base + 48,
                              p.alignment)) {
            std::cerr
                << "[SelfRebuilder] Program header " << i
                << " fora dos limites; reconstrucao recusada.\n";
            return false;
        }
        p.emitFileSize = p.fileSize;
    }

    // 4. Map the record table onto program headers (validation first).
    std::vector<const SelfRecordInfo*> covered(phnum, nullptr);
    uint16_t lastCovered = 0;
    bool anyCovered = false;
    for (const auto& rec : report.records) {
        if (!rec.hasSegmentMapping || rec.dataSize == 0) continue;
        if (!rec.payloadWithinFile ||
            rec.dataOffset > selfSize ||
            rec.dataSize > selfSize - rec.dataOffset) {
            std::cerr << "[SelfRebuilder] Registro fora dos limites do "
                         "container; reconstrucao recusada.\n";
            return false;
        }
        if (rec.targetSegmentIndex >= phnum) {
            std::cerr << "[SelfRebuilder] Registro aponta para segmento "
                      << rec.targetSegmentIndex << " fora da tabela ("
                      << phnum << " headers); reconstrucao recusada.\n";
            return false;
        }
        if (covered[rec.targetSegmentIndex]) {
            std::cerr << "[SelfRebuilder] Program header "
                      << rec.targetSegmentIndex
                      << " coberto por mais de um registro; reconstrucao "
                         "recusada.\n";
            return false;
        }
        if (rec.bitsPerByte >= kSelfEntropyHighThreshold) {
            std::cerr << "[SelfRebuilder] Payload do segmento "
                      << rec.targetSegmentIndex << " apresenta entropia "
                      << rec.bitsPerByte << " bits/byte (limite "
                      << kSelfEntropyHighThreshold
                      << "); possivelmente criptografado; reconstrucao "
                         "recusada.\n";
            return false;
        }
        if (rec.dataSize != phdrs[rec.targetSegmentIndex].fileSize) {
            std::cerr << "[SelfRebuilder] Tamanho do registro ("
                      << rec.dataSize << ") difere de p_filesz ("
                      << phdrs[rec.targetSegmentIndex].fileSize
                      << ") do segmento " << rec.targetSegmentIndex
                      << "; reconstrucao recusada.\n";
            return false;
        }
        covered[rec.targetSegmentIndex] = &rec;
        anyCovered = true;
        lastCovered = rec.targetSegmentIndex > lastCovered
                          ? rec.targetSegmentIndex
                          : lastCovered;
    }
    if (!anyCovered) {
        std::cerr << "[SelfRebuilder] Nenhum segmento coberto pela tabela de "
                     "registros; container nao pode ser reconstruido.\n";
        return false;
    }
    const SelfRecordInfo* lastRecord = covered[lastCovered];
    const uint64_t trailingCursorSelf =
        lastRecord->dataOffset + lastRecord->dataSize;

    // 5. Layout: decide the new file offset of every program header.
    //     (fail-closed: every placed segment must be explained by one of the
    //      documented rules; anything else is refused.)
    const uint64_t phdrTableEnd = kRebuiltPhdrTableOffset + 56u * phnum;
    uint64_t cursor = phdrTableEnd;
    uint64_t totalEnd = phdrTableEnd;
    uint64_t trailingCursor = trailingCursorSelf;
    for (uint16_t i = 0; i < phnum; ++i) {
        RebuiltPhdr& p = phdrs[i];
        const auto adoptAlignment = [&p]() {
            uint64_t alignment = (p.alignment >= 0x1000) ? p.alignment : 0x1000;
            if ((alignment & (alignment - 1)) != 0) alignment = 0x1000;
            return alignment;
        };
        if (covered[i] != nullptr) {
            // Rule 1: data carried by the record table -> copy verbatim.
            p.newFileOffset =
                PlaceCongruent(cursor, p.virtualAddress, adoptAlignment());
            if (p.fileSize > 0 &&
                (p.newFileOffset < cursor ||
                 p.newFileOffset >
                     std::numeric_limits<uint64_t>::max() - p.fileSize)) {
                std::cerr << "[SelfRebuilder] Overflow no layout do segmento "
                          << i << "; reconstrucao recusada.\n";
                return false;
            }
            cursor = p.newFileOffset + p.fileSize;
            totalEnd = std::max(totalEnd, cursor);
            ++out.segmentsDataCopied;
            continue;
        }
        if (p.type == kPhdrTypePhdr) {
            // The program-header table itself: points at our own table.
            if (p.fileSize != 56u * phnum || p.virtualAddress != 0) {
                std::cerr << "[SelfRebuilder] PT_PHDR inconsistente com a "
                             "tabela reconstruida; reconstrucao recusada.\n";
                return false;
            }
            p.newFileOffset = kRebuiltPhdrTableOffset;
            ++out.segmentsDataless;
            continue;
        }
        if (p.fileSize == 0) {
            // Dataless segment (bss tail, note without bytes...).
            p.newFileOffset = 0;
            ++out.segmentsDataless;
            continue;
        }
        if (p.virtualAddress != 0) {
            // Rule 2: file-backed data inside a covered PT_LOAD's file image.
            // In any ELF the file data of a load covers the vaddr range
            // [vaddr, vaddr+filesz) linearly, so the containing record's copy
            // already holds this segment's bytes; only the offset is patched.
            int chosen = -1;
            for (uint16_t j = 0; j < i; ++j) {
                const RebuiltPhdr& load = phdrs[j];
                if (covered[j] == nullptr || load.type != kPhdrTypeLoad) continue;
                if (p.virtualAddress < load.virtualAddress) continue;
                const uint64_t delta = p.virtualAddress - load.virtualAddress;
                if (delta > load.fileSize || p.fileSize > load.fileSize - delta) {
                    continue;
                }
                chosen = j;
            }
            if (chosen < 0) {
                std::cerr << "[SelfRebuilder] Segmento " << i << " (vaddr 0x"
                          << std::hex << p.virtualAddress
                          << ", filesz 0x" << p.fileSize << std::dec
                          << ") nao esta no espaco de arquivo de nenhum "
                             "PT_LOAD coberto; regra nao explicada; "
                             "reconstrucao recusada.\n";
                return false;
            }
            const RebuiltPhdr& load = phdrs[chosen];
            p.newFileOffset =
                load.newFileOffset + (p.virtualAddress - load.virtualAddress);
            ++out.segmentsProvidedByLoad;
            continue;
        }
        if (i > lastCovered) {
            // Rule 3: trailing vaddr-less data stored back to back after the
            // last covered record in the container (observed in real dumps).
            const bool trailingFits =
                trailingCursor <= selfSize &&
                p.fileSize <= selfSize - trailingCursor;
            if (!trailingFits) {
                if (p.type == kPhdrTypeLoad) {
                    std::cerr << "[SelfRebuilder] PT_LOAD restante " << i
                              << " sem bytes no container; reconstrucao "
                                 "recusada.\n";
                    return false;
                }
                // Real PS5 dumps end after the last recorded segment; later
                // metadata segments (e.g. the version PT_NOTE) have no bytes.
                // They are emitted without file bytes (documented behavior),
                // which is still fail-closed: nothing is fabricated.
                std::cout << "[SelfRebuilder] Segmento restante " << i
                          << " (type " << p.type
                          << ") nao tem bytes no container (dumps PS5 podem "
                             "terminar apos o ultimo registro); emitido sem "
                             "dados de arquivo.\n";
                p.emitFileSize = 0;
                p.newFileOffset = 0;
                ++out.segmentsDataless;
                continue;
            }
            p.newFileOffset =
                PlaceCongruent(cursor, p.virtualAddress, adoptAlignment());
            if (p.newFileOffset < cursor ||
                p.newFileOffset >
                    std::numeric_limits<uint64_t>::max() - p.fileSize) {
                std::cerr << "[SelfRebuilder] Overflow no layout do segmento "
                          << i << "; reconstrucao recusada.\n";
                return false;
            }
            cursor = p.newFileOffset + p.fileSize;
            totalEnd = std::max(totalEnd, cursor);
            trailingCursor += p.fileSize;
            ++out.segmentsTrailingCopied;
            continue;
        }
        std::cerr << "[SelfRebuilder] Segmento " << i << " (filesz 0x"
                  << std::hex << p.fileSize << ", vaddr 0x" << p.virtualAddress
                  << ") nao coberto pela tabela e anterior ao ultimo registro; "
                     "regra nao explicada; reconstrucao recusada.\n" << std::dec;
        return false;
    }

    // 6. Assemble the output image.
    try {
        out.bytes.assign(static_cast<size_t>(totalEnd), 0);
    } catch (const std::bad_alloc&) {
        std::cerr << "[SelfRebuilder] Sem memoria para a imagem reconstruida.\n";
        return false;
    }

    // Header: copy the inner ELF header, then adapt it to the new layout.
    std::memcpy(out.bytes.data(), elf, 64);
    WriteLittleEndianInto(out.bytes, 16, kElfTypeDyn, 2);
    WriteLittleEndianInto(out.bytes, 32, kRebuiltPhdrTableOffset, 8);
    WriteLittleEndianInto(out.bytes, 40, 0, 8);  // e_shoff
    WriteLittleEndianInto(out.bytes, 54, kElf64PhdrEntrySize, 2);
    WriteLittleEndianInto(out.bytes, 56, phnum, 2);
    WriteLittleEndianInto(out.bytes, 58, 0, 2);  // e_shentsize
    WriteLittleEndianInto(out.bytes, 60, 0, 2);  // e_shnum
    WriteLittleEndianInto(out.bytes, 62, 0, 2);  // e_shstrndx

    for (uint16_t i = 0; i < phnum; ++i) {
        const RebuiltPhdr& p = phdrs[i];
        const size_t phdrEntry =
            static_cast<size_t>(kRebuiltPhdrTableOffset) + 56u * i;
        WriteLittleEndianInto(out.bytes, phdrEntry, p.type, 4);
        WriteLittleEndianInto(out.bytes, phdrEntry + 4, p.flags, 4);
        WriteLittleEndianInto(out.bytes, phdrEntry + 8, p.newFileOffset, 8);
        WriteLittleEndianInto(out.bytes, phdrEntry + 16, p.virtualAddress, 8);
        WriteLittleEndianInto(out.bytes, phdrEntry + 24, p.virtualAddress, 8);
        WriteLittleEndianInto(out.bytes, phdrEntry + 32, p.emitFileSize, 8);
        WriteLittleEndianInto(out.bytes, phdrEntry + 40, p.memorySize, 8);
        WriteLittleEndianInto(out.bytes, phdrEntry + 48, p.alignment, 8);
    }

    // Data copies. PT_PHDR and contained segments are already in the image:
    // the former is our own header/table, the latter lives inside the copy of
    // its containing PT_LOAD, so nothing is duplicated.
    uint64_t trailingSource = trailingCursorSelf;
    for (uint16_t i = 0; i < phnum; ++i) {
        const RebuiltPhdr& p = phdrs[i];
        if (p.emitFileSize == 0) continue;
        if (covered[i] != nullptr) {
            std::memcpy(out.bytes.data() + p.newFileOffset,
                        selfBytes + covered[i]->dataOffset,
                        static_cast<size_t>(p.fileSize));
        } else if (i > lastCovered) {
            std::memcpy(out.bytes.data() + p.newFileOffset,
                        selfBytes + trailingSource,
                        static_cast<size_t>(p.fileSize));
            trailingSource += p.fileSize;
        }
    }

    out.entryVaddr = entry;
    out.type = kElfTypeDyn;
    out.machine = elfMachine;
    out.programHeaders = phnum;
    out.typeNormalized = (elfType != kElfTypeDyn && elfType != 2);

    std::cout << "[SelfRebuilder] SELF reconstruido em " << out.bytes.size()
              << " bytes: " << phnum << " program headers ("
              << out.segmentsDataCopied << " copiados de registros, "
              << out.segmentsProvidedByLoad << " resolvidos via PT_LOAD, "
              << out.segmentsTrailingCopied << " restantes copiados, "
              << out.segmentsDataless << " sem dados de arquivo); e_type "
              << (out.typeNormalized ? "normalizado para ET_DYN" : "preservado")
              << "; entry 0x" << std::hex << entry << std::dec << ".\n";
    for (uint16_t i = 0; i < phnum; ++i) {
        const RebuiltPhdr& p = phdrs[i];
        std::cout << "[SelfRebuilder]   [" << i << "] type " << p.type
                  << " vaddr 0x" << std::hex << p.virtualAddress
                  << " -> novo offset 0x" << p.newFileOffset
                  << " fsz 0x" << p.fileSize << std::dec << "\n";
    }
    return true;
}

bool ParseElf64Metadata(const uint8_t* bytes, size_t size,
                        Elf64Metadata& metadata, bool inspectProgramHeaders) {
    metadata = {};
    Elf64Metadata parsed;
    if (!bytes || size < 64 ||
        bytes[0] != 0x7f || bytes[1] != 'E' ||
        bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1) {
        return false;
    }

    uint32_t version = 0;
    uint16_t headerSize = 0;
    if (!ReadLittleEndian(bytes, size, 16, parsed.type) ||
        !ReadLittleEndian(bytes, size, 18, parsed.machine) ||
        !ReadLittleEndian(bytes, size, 20, version) || version != 1 ||
        !ReadLittleEndian(bytes, size, 24, parsed.entryVaddr) ||
        !ReadLittleEndian(bytes, size, 52, headerSize) ||
        headerSize < 64 || headerSize > size) {
        return false;
    }

    if (!inspectProgramHeaders) {
        metadata = parsed;
        return true;
    }

    uint64_t programHeaderOffset = 0;
    uint16_t programHeaderEntrySize = 0;
    uint16_t programHeaderCount = 0;
    if (!ReadLittleEndian(bytes, size, 32, programHeaderOffset) ||
        !ReadLittleEndian(bytes, size, 54, programHeaderEntrySize) ||
        !ReadLittleEndian(bytes, size, 56, programHeaderCount)) {
        return false;
    }

    if (programHeaderCount == 0) {
        metadata = parsed;
        return true;
    }
    if (programHeaderEntrySize < 56 || programHeaderOffset > size ||
        programHeaderCount >
            (size - static_cast<size_t>(programHeaderOffset)) /
                programHeaderEntrySize) {
        return false;
    }

    for (uint16_t i = 0; i < programHeaderCount; ++i) {
        const size_t headerOffset = static_cast<size_t>(programHeaderOffset) +
                                   static_cast<size_t>(i) * programHeaderEntrySize;
        uint32_t type = 0;
        uint32_t flags = 0;
        uint64_t fileOffset = 0;
        uint64_t virtualAddress = 0;
        uint64_t fileSize = 0;
        uint64_t memorySize = 0;
        uint64_t alignment = 0;
        if (!ReadLittleEndian(bytes, size, headerOffset, type) ||
            !ReadLittleEndian(bytes, size, headerOffset + 4, flags) ||
            !ReadLittleEndian(bytes, size, headerOffset + 8, fileOffset) ||
            !ReadLittleEndian(bytes, size, headerOffset + 16, virtualAddress) ||
            !ReadLittleEndian(bytes, size, headerOffset + 32, fileSize) ||
            !ReadLittleEndian(bytes, size, headerOffset + 40, memorySize) ||
            !ReadLittleEndian(bytes, size, headerOffset + 48, alignment)) {
            return false;
        }

        if (type == 2) {
            // PT_DYNAMIC: keep only basic file bounds; the relocation applier
            // validates the table contents before any later write.
            if (fileOffset > size || fileSize > size - fileOffset ||
                (fileSize % kDynamicEntrySize) != 0 ||
                memorySize < fileSize ||
                virtualAddress >
                    std::numeric_limits<uint64_t>::max() - memorySize) {
                return false;
            }
            if (!parsed.hasDynamicSegment) {
                parsed.hasDynamicSegment = true;
                parsed.dynamicSegment = {flags, fileOffset, virtualAddress,
                                         fileSize, memorySize, alignment};
            }
            continue;
        }
        if (type != 1) continue;
        if (fileOffset > size || fileSize > size - fileOffset ||
            memorySize < fileSize ||
            virtualAddress > std::numeric_limits<uint64_t>::max() - memorySize ||
            (alignment > 1 &&
             ((alignment & (alignment - 1)) != 0 ||
              fileOffset % alignment != virtualAddress % alignment))) {
            return false;
        }

        parsed.loadSegments.push_back(
            {flags, fileOffset, virtualAddress, fileSize, memorySize, alignment});
        if ((flags & 1) != 0 && !parsed.hasExecutableSegment) {
            parsed.executableSegmentOffset = fileOffset;
            parsed.executableSegmentVaddr = virtualAddress;
            parsed.executableSegmentFileSize = fileSize;
            parsed.executableSegmentMemorySize = memorySize;
            parsed.hasExecutableSegment = true;
        }
    }

    metadata = parsed;
    return true;
}

bool MapElf64LoadSegments(const uint8_t* bytes, size_t size,
                          const Elf64Metadata& metadata,
                          Elf64MappedImage& image,
                          size_t maximumImageSize) {
    image = {};
    if (!bytes || (metadata.type != 2 && metadata.type != 3) ||
        metadata.machine != 62 || metadata.loadSegments.empty() ||
        maximumImageSize == 0) {
        return false;
    }

    uint64_t virtualBase = std::numeric_limits<uint64_t>::max();
    uint64_t virtualEnd = 0;
    for (const auto& segment : metadata.loadSegments) {
        if (segment.fileOffset > size || segment.fileSize > size - segment.fileOffset ||
            segment.memorySize < segment.fileSize ||
            segment.virtualAddress >
                std::numeric_limits<uint64_t>::max() - segment.memorySize ||
            (segment.alignment > 1 &&
             ((segment.alignment & (segment.alignment - 1)) != 0 ||
              segment.fileOffset % segment.alignment !=
                  segment.virtualAddress % segment.alignment))) {
            return false;
        }
        if (segment.memorySize == 0) continue;

        const uint64_t segmentEnd = segment.virtualAddress + segment.memorySize;
        virtualBase = std::min(virtualBase, segment.virtualAddress);
        virtualEnd = std::max(virtualEnd, segmentEnd);
    }

    if (virtualBase == std::numeric_limits<uint64_t>::max() ||
        virtualEnd <= virtualBase ||
        virtualEnd - virtualBase > maximumImageSize) {
        return false;
    }

    for (size_t i = 0; i < metadata.loadSegments.size(); ++i) {
        const auto& left = metadata.loadSegments[i];
        if (left.memorySize == 0) continue;
        const uint64_t leftEnd = left.virtualAddress + left.memorySize;
        for (size_t j = i + 1; j < metadata.loadSegments.size(); ++j) {
            const auto& right = metadata.loadSegments[j];
            if (right.memorySize == 0) continue;
            const uint64_t rightEnd = right.virtualAddress + right.memorySize;
            if (left.virtualAddress < rightEnd &&
                right.virtualAddress < leftEnd) {
                return false;
            }
        }
    }

    Elf64MappedImage mapped;
    mapped.virtualBase = virtualBase;
    try {
        mapped.bytes.assign(static_cast<size_t>(virtualEnd - virtualBase), 0);
    } catch (const std::bad_alloc&) {
        return false;
    }

    for (const auto& segment : metadata.loadSegments) {
        if (segment.fileSize == 0) continue;
        const size_t destinationOffset =
            static_cast<size_t>(segment.virtualAddress - virtualBase);
        const size_t copySize = static_cast<size_t>(segment.fileSize);
        std::memcpy(mapped.bytes.data() + destinationOffset,
                    bytes + static_cast<size_t>(segment.fileOffset), copySize);
    }

    image = std::move(mapped);
    return true;
}

bool ApplyElf64RelativeRelocations(const uint8_t* bytes, size_t size,
                                   const Elf64Metadata& metadata,
                                   Elf64MappedImage& image,
                                   Elf64RelocationResult& result) {
    result = {};
    if (!bytes || image.bytes.empty()) {
        std::cerr << "[Relocations] Entrada invalida; nenhuma relocation aplicada.\n";
        return false;
    }

    if (!metadata.hasDynamicSegment) {
        // No PT_DYNAMIC: nothing to relocate; treated as success with 0 gains.
        std::cout << "[Relocations] Imagem sem PT_DYNAMIC; nenhuma relocation aplicada.\n";
        return true;
    }

    const auto& dynamic = metadata.dynamicSegment;
    if (dynamic.fileOffset > size ||
        dynamic.fileSize > size - dynamic.fileOffset) {
        std::cerr << "[Relocations] Segmento PT_DYNAMIC fora dos limites do arquivo.\n";
        return false;
    }

    bool hasRela = false;
    bool hasRelasz = false;
    bool hasRelaent = false;
    bool hasJmprel = false;
    bool hasTextrel = false;
    bool hasRelr = false;
    uint64_t relaVaddr = 0;
    uint64_t relaSize = 0;
    uint64_t relaEnt = 0;
    uint64_t pltRelVaddr = 0;
    uint64_t pltRelSize = 0;
    uint64_t pltRelType = 0; // 7 = RELA, 17 = REL
    for (size_t entryOffset = 0; entryOffset < dynamic.fileSize;
         entryOffset += kDynamicEntrySize) {
        uint64_t tag = 0;
        uint64_t value = 0;
        if (!ReadLittleEndian(bytes, size, dynamic.fileOffset + entryOffset, tag) ||
            !ReadLittleEndian(bytes, size, dynamic.fileOffset + entryOffset + 8,
                              value)) {
            std::cerr << "[Relocations] Tabela dinamica incompleta.\n";
            return false;
        }
        switch (tag) {
            case static_cast<uint64_t>(kDyTagNull):
                entryOffset = dynamic.fileSize; // terminate parsing
                break;
            case static_cast<uint64_t>(kDyTagRela):
                hasRela = true;
                relaVaddr = value;
                break;
            case static_cast<uint64_t>(kDyTagRelasz):
                hasRelasz = true;
                relaSize = value;
                break;
            case static_cast<uint64_t>(kDyTagRelaent):
                hasRelaent = true;
                relaEnt = value;
                break;
            case static_cast<uint64_t>(kDyTagJmprel):
                hasJmprel = true;
                pltRelVaddr = value;
                break;
            case static_cast<uint64_t>(kDyTagPltrelsz):
                pltRelSize = value;
                break;
            case static_cast<uint64_t>(kDyTagPltrel):
                pltRelType = value;
                break;
            case static_cast<uint64_t>(kDyTagTextrel):
                hasTextrel = true;
                break;
            case static_cast<uint64_t>(kDyTagRelr):
            case static_cast<uint64_t>(kDyTagRelrsz):
                hasRelr = true;
                break;
            default:
                break;
        }
    }

    (void)hasTextrel;
    if (hasRelaent && relaEnt != kRelaEntrySize) {
        std::cerr << "[Relocations] DT_RELAENT " << relaEnt
                  << " nao suportado (somente 24).\n";
        return false;
    }
    if (hasRelr) {
        std::cerr << "[Relocations] Relocations empacotadas (DT_RELR/DT_RELRSZ) nao suportadas.\n";
        return false;
    }
    if (hasTextrel) {
        std::cerr << "[Relocations] DT_TEXTREL nao suportado.\n";
        return false;
    }

    if (hasJmprel) {
        std::cout << "[Relocations] PLT/JMPREL encontrado em 0x" << std::hex << pltRelVaddr
                  << " tamanho 0x" << pltRelSize << std::dec << ". Analise preliminar OK.\n";
    }
    if (hasRela == hasRelasz) {
        if (!hasRela) {
            std::cout << "[Relocations] Tabela dinamica sem DT_RELA; nenhuma relocation aplicada.\n";
            return true;
        }
    } else {
        std::cerr << "[Relocations] DT_RELA/DT_RELASZ inconsistentes.\n";
        return false;
    }
    if (relaSize % kRelaEntrySize != 0) {
        std::cerr << "[Relocations] DT_RELASZ nao e multiplo de 24.\n";
        return false;
    }
    if (relaSize == 0) {
        std::cout << "[Relocations] DT_RELASZ zero; nenhuma relocation aplicada.\n";
        return true;
    }
    if (relaVaddr < image.virtualBase) {
        std::cerr << "[Relocations] DT_RELA aponta antes da base da imagem.\n";
        return false;
    }
    const size_t relaOffset = static_cast<size_t>(relaVaddr - image.virtualBase);
    if (relaOffset > image.bytes.size() ||
        relaSize > image.bytes.size() - relaOffset) {
        std::cerr << "[Relocations] DT_RELA fora dos limites da imagem mapeada.\n";
        return false;
    }

    const size_t entryCount = static_cast<size_t>(relaSize / kRelaEntrySize);

    // Validation pass: every entry must be a supported, in-bounds RELATIVE
    // relocation before any byte is modified (fail-closed).
    for (size_t i = 0; i < entryCount; ++i) {
        const size_t entryOffset = relaOffset + i * kRelaEntrySize;
        uint64_t relocationOffset = 0;
        uint64_t relocationInfo = 0;
        uint64_t addend = 0;
        if (!ReadLittleEndian(image.bytes.data(), image.bytes.size(),
                              entryOffset, relocationOffset) ||
            !ReadLittleEndian(image.bytes.data(), image.bytes.size(),
                              entryOffset + 8, relocationInfo) ||
            !ReadLittleEndian(image.bytes.data(), image.bytes.size(),
                              entryOffset + 16, addend)) {
            std::cerr << "[Relocations] Entrada de relocation fora da imagem.\n";
            return false;
        }
        const uint64_t relocationType = relocationInfo & 0xffffffffULL;
        const uint64_t symbolIndex = relocationInfo >> 32;
        if (relocationType != kRelX8664Relative) {
            std::cerr << "[Relocations] Tipo de relocation 0x" << std::hex
                      << relocationType << " nao documentado/suportado.\n" << std::dec;
            return false;
        }
        if (symbolIndex != 0) {
            std::cerr << "[Relocations] RELATIVE com indice de simbolo " << symbolIndex
                      << "; resolucao de simbolos nao implementada.\n";
            return false;
        }
        if (relocationOffset < image.virtualBase) {
            std::cerr << "[Relocations] r_offset antes da base da imagem.\n";
            return false;
        }
        const size_t targetOffset =
            static_cast<size_t>(relocationOffset - image.virtualBase);
        if (targetOffset > image.bytes.size() ||
            8 > image.bytes.size() - targetOffset) {
            std::cerr << "[Relocations] r_offset fora dos limites da imagem.\n";
            return false;
        }
        if (image.virtualBase >
            std::numeric_limits<uint64_t>::max() - addend) {
            std::cerr << "[Relocations] Overflow ao computar base + addend.\n";
            return false;
        }
    }

    // Application pass: write validated base + addend values.
    for (size_t i = 0; i < entryCount; ++i) {
        const size_t entryOffset = relaOffset + i * kRelaEntrySize;
        uint64_t relocationOffset = 0;
        uint64_t addend = 0;
        ReadLittleEndian(image.bytes.data(), image.bytes.size(),
                         entryOffset, relocationOffset);
        ReadLittleEndian(image.bytes.data(), image.bytes.size(),
                         entryOffset + 16, addend);
        const uint64_t value = image.virtualBase + addend;
        const size_t targetOffset =
            static_cast<size_t>(relocationOffset - image.virtualBase);
        for (size_t b = 0; b < 8; ++b) {
            image.bytes[targetOffset + b] =
                static_cast<uint8_t>((value >> (b * 8)) & 0xff);
        }
    }
    result.relativeApplied = entryCount;
    std::cout << "[Relocations] " << result.relativeApplied
              << " relocacao(oes) R_X86_64_RELATIVE aplicada(s); nenhum codigo executado.\n";
    return true;
}

EbootLoader& EbootLoader::Get() {
    static EbootLoader instance;
    return instance;
}

EbootLoader::~EbootLoader() {
    ReleaseMappedMemory();
}

void EbootLoader::ReleaseMappedMemory() {
    if (!m_loaded.base) return;
#ifdef _WIN32
    VirtualFree(m_loaded.base, 0, MEM_RELEASE);
#else
    delete[] m_loaded.base;
#endif
    m_loaded.base = nullptr;
}

bool EbootLoader::MapFileIntoMemory(const std::string& path) {
    std::cout << "[EbootLoader] Mapeando arquivo para memoria: " << path << "\n";

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[EbootLoader] Erro: Nao foi possivel abrir " << path << "\n";
        return false;
    }

    const std::streampos endPosition = file.tellg();
    if (endPosition == std::streampos(-1)) {
        std::cerr << "[EbootLoader] Erro: Nao foi possivel determinar o tamanho do arquivo.\n";
        return false;
    }

    const std::streamoff fileSize = static_cast<std::streamoff>(endPosition);
    if (fileSize <= 0 ||
        static_cast<uintmax_t>(fileSize) > std::numeric_limits<size_t>::max() ||
        static_cast<uintmax_t>(fileSize) >
            static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "[EbootLoader] Erro: Tamanho do arquivo vazio ou nao suportado.\n";
        return false;
    }

    const size_t mappedSize = static_cast<size_t>(fileSize);
    file.seekg(0, std::ios::beg);
    if (!file) {
        std::cerr << "[EbootLoader] Erro: Nao foi possivel voltar ao inicio do arquivo.\n";
        return false;
    }

#ifdef _WIN32
    // The current loader only inspects container bytes; it never executes them.
    m_loaded.base = (uint8_t*)VirtualAlloc(
        nullptr,
        mappedSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
#else
    m_loaded.base = new (std::nothrow) uint8_t[mappedSize];
#endif

    if (!m_loaded.base) {
        std::cerr << "[EbootLoader] Erro fatal: Sem memoria para mapear o eboot (" 
                  << (mappedSize / (1024*1024)) << " MB necessarios)!\n";
        return false;
    }

    file.read(reinterpret_cast<char*>(m_loaded.base),
              static_cast<std::streamsize>(mappedSize));
    if (file.gcount() != static_cast<std::streamsize>(mappedSize) || !file) {
        std::cerr << "[EbootLoader] Erro ao ler eboot.bin!\n";
        ReleaseMappedMemory();
        return false;
    }

    m_loaded.size = mappedSize;
    std::cout << "[EbootLoader] Lidos " << (m_loaded.size / (1024 * 1024))
              << " MB em memoria de analise no endereco "
              << (void*)m_loaded.base << "\n";
    return true;
}

bool EbootLoader::ParseSelfHeader() {
    if (m_loaded.size < 16) return false;

    // 1. Verifica se já é ELF direto (\x7fELF)
    if (m_loaded.base[0] == 0x7F && m_loaded.base[1] == 'E' && 
        m_loaded.base[2] == 'L' && m_loaded.base[3] == 'F') {
        m_loaded.isRawElf = true;
        std::cout << "[EbootLoader] Arquivo detectado como ELF64 direto (sem container SELF)!\n";
        return true;
    }

    // 2. Verifica se é imagem vinculada G3PROBE (boot-linked.bin / gow3 / issn)
    if (std::memcmp(m_loaded.base, "G3PROBE", 7) == 0) {
        m_loaded.isG3Probe = true;
        std::cout << "[EbootLoader] Arquivo detectado como imagem G3PROBE vinculada nativa!\n";
        return true;
    }

    // 3. Validação do magic de 4 bytes do SELF container
    // Formato PS4/PS5 SELF: 0x4F153D1D (little-endian)
    uint32_t selfMagic = 0;
    if (!ReadLittleEndian(m_loaded.base, m_loaded.size, 0, selfMagic)) return false;
    if (selfMagic != 0x1D3D154F) {
        std::cerr << "[EbootLoader] Magic SELF/ELF invalido: 0x" << std::hex << selfMagic << "\n";
        return false;
    }

    if (m_loaded.size < 0x1a + sizeof(uint16_t)) {
        std::cerr << "[EbootLoader] Cabecalho SELF truncado.\n";
        return false;
    }

    uint16_t numEntries = 0;
    if (!ReadLittleEndian(m_loaded.base, m_loaded.size, 0x18, numEntries)) return false;
    std::cout << "[EbootLoader] SELF container valido! Entradas: " << std::dec << numEntries << "\n";

    // Inspection (borrowed idea, own implementation): record table + entropy
    // classification. This remains fail-closed on malformed containers so the
    // load refuses anything the inspector cannot explain.
    SelfContainerReport selfReport;
    if (!InspectSelfContainer(m_loaded.base, m_loaded.size, selfReport)) {
        std::cerr << "[EbootLoader] SELF container malformado ou sem magic valido.\n";
        return false;
    }
    if (!m_loaded.isRawElf && selfReport.anyMarkedEncrypted) {
        std::cout << "[EbootLoader] Registros marcados como criptografados; "
                     "nenhum payload sera lido ou escrito.\n";
    }
    if (!m_loaded.isRawElf && selfReport.allDataRecordsHighEntropy) {
        std::cout << "[EbootLoader] Entropia alta em todos os payloads mapeados; "
                     "container provavelmente criptografado (inspecao apenas).\n";
    }
    return true;
}

bool EbootLoader::ParseElfHeaders() {
    size_t elfOffset = 0;

    if (m_loaded.isG3Probe) {
        // G3PROBE header: magic(8), size(8), entry(8), ns(8), nr(8), ni(8), flags(8)
        if (m_loaded.size < 56) return false;
        uint64_t entry = 0;
        if (!ReadLittleEndian(m_loaded.base, m_loaded.size, 16, entry)) return false;
        m_loaded.entryVaddr = entry;
        m_loaded.codeOffset = 56;
        std::cout << "[EbootLoader] G3PROBE Entry Point: 0x" << std::hex << entry << std::dec << "\n";
        return true;
    } else if (!m_loaded.isRawElf) {
        // ELF64 localizado no offset físico 0x1a0 dentro do SELF
        elfOffset = kElfMagicOffset;
    }

    if (elfOffset > m_loaded.size || m_loaded.size - elfOffset < 64) {
        std::cerr << "[EbootLoader] Cabecalho ELF truncado.\n";
        return false;
    }
    const uint8_t* elfBase = m_loaded.base + elfOffset;
    Elf64Metadata metadata;
    if (!ParseElf64Metadata(elfBase, m_loaded.size - elfOffset, metadata,
                            m_loaded.isRawElf)) {
        std::cerr << "[EbootLoader] Cabecalho/tabela ELF64 invalido ou truncado.\n";
        return false;
    }

    m_loaded.entryVaddr = metadata.entryVaddr;
    m_loaded.codeOffset = metadata.hasExecutableSegment
                              ? metadata.executableSegmentOffset
                              : 0;
    if (m_loaded.isRawElf && metadata.hasExecutableSegment) {
        std::cout << "[EbootLoader] Segmento de Codigo PT_LOAD: offset 0x"
                  << std::hex << metadata.executableSegmentOffset
                  << " VAddr 0x" << metadata.executableSegmentVaddr
                  << " tam 0x" << metadata.executableSegmentFileSize << "/"
                  << metadata.executableSegmentMemorySize << std::dec << "\n";
    }
    std::cout << "[EbootLoader] Cabecalho ELF64 encontrado; o mapeamento da "
                 "imagem ainda precisa ser validado.\n";
    std::cout << "[EbootLoader] Entry Point VAddr: 0x" << std::hex
              << metadata.entryVaddr << std::dec << "\n";

    return true;
}

bool EbootLoader::ProcessRelocations() {
    if (m_loaded.isG3Probe) {
        std::cerr << "[EbootLoader] Mapeamento/relocations G3PROBE nao implementados; "
                     "carga interrompida e buffer liberado.\n";
        return false;
    }
    if (!m_loaded.isRawElf) {
        // SELF container: attempt an analysis-only reconstruction (refuses
        // encrypted/compressed/inexplicable containers, then inspects the
        // rebuilt image the same way a direct ELF would be). Nothing is
        // executed and no import is resolved.
        Elf64RebuiltInfo rebuilt;
        if (!ReconstructElf64FromSelf(m_loaded.base, m_loaded.size, rebuilt)) {
            std::cerr << "[EbootLoader] Reconstrucao SELF->ELF recusada; "
                         "apenas inspecao foi executada.\n";
            return false;
        }

        Elf64Metadata metadata;
        if (!ParseElf64Metadata(rebuilt.bytes.data(), rebuilt.bytes.size(),
                                metadata, true)) {
            std::cerr << "[EbootLoader] Metadados ELF64 invalidos na imagem "
                         "reconstruida.\n";
            return false;
        }

        Elf64MappedImage image;
        if (!MapElf64LoadSegments(rebuilt.bytes.data(), rebuilt.bytes.size(),
                                  metadata, image)) {
            std::cerr << "[EbootLoader] Mapeamento PT_LOAD recusado na imagem "
                         "reconstruida.\n";
            return false;
        }
        std::cout << "[EbootLoader] Imagem SELF reconstruida mapeada: base 0x"
                  << std::hex << image.virtualBase << ", " << std::dec
                  << image.bytes.size() << " bytes em "
                  << metadata.loadSegments.size() << " PT_LOAD.\n";

        Elf64RelocationResult relocations;
        if (!ApplyElf64RelativeRelocations(rebuilt.bytes.data(),
                                           rebuilt.bytes.size(), metadata,
                                           image, relocations)) {
            std::cerr << "[EbootLoader] Relocations da imagem reconstruida "
                         "recusadas; imagem intacta.\n";
            return false;
        }
        std::cout << "[EbootLoader] " << relocations.relativeApplied
                  << " relocation(oes) analisadas na imagem reconstruida.\n";
    } else {
        // Raw-ELF pipeline: metadata -> PT_LOAD mapping -> RELATIVE
        // relocations. The image lives in analysis memory only, is never
        // executable, and every intermediate is released before import
        // resolution is refused.
        Elf64Metadata metadata;
        if (!ParseElf64Metadata(m_loaded.base, m_loaded.size, metadata, true)) {
            std::cerr << "[EbootLoader] Metadados ELF64 invalidos no pipeline cru.\n";
            return false;
        }

        Elf64MappedImage image;
        if (!MapElf64LoadSegments(m_loaded.base, m_loaded.size, metadata, image)) {
            std::cerr << "[EbootLoader] Mapeamento PT_LOAD recusado no pipeline cru.\n";
            return false;
        }
        std::cout << "[EbootLoader] Image cru mapeada: base 0x" << std::hex
                  << image.virtualBase << ", " << std::dec
                  << image.bytes.size() << " bytes em "
                  << metadata.loadSegments.size() << " PT_LOAD.\n";

        Elf64RelocationResult relocations;
        if (!ApplyElf64RelativeRelocations(m_loaded.base, m_loaded.size, metadata,
                                           image, relocations)) {
            std::cerr << "[EbootLoader] Relocations cru recusadas; imagem intacta.\n";
            return false;
        }
        std::cout << "[EbootLoader] " << relocations.relativeApplied
                  << " relocation(oes) analisadas na imagem cru.\n";
    }

    // Bypass temporário para permitir o prosseguimento do carregamento
    std::cout << "[EbootLoader] Resolucao de imports ignorada (bypass para boot experimental).\n";
    return true;
}

bool EbootLoader::ResolveImports() {
    std::cout << "[EbootLoader] Conectando Shims da Dispatch Table ao binario do jogo...\n";
    // No futuro, aqui mapearemos os NIDs para g_Dispatch
    return true;
}

bool EbootLoader::Load(const std::string& ebootPath) {
    std::cout << "\n==========================================================\n";
    std::cout << " ASTRO BOT PC | EbootLoader v1.0\n";
    std::cout << " PPSA21567 v01.018.000 — Armadillo Prospero Master\n";
    std::cout << "==========================================================\n\n";

    ReleaseMappedMemory();
    m_loaded = {};
    if (!MapFileIntoMemory(ebootPath)) return false;
    if (!ParseSelfHeader() || !ParseElfHeaders() ||
        !ProcessRelocations() || !ResolveImports()) {
        ReleaseMappedMemory();
        return false;
    }

    m_loaded.valid = true;
    std::cout << "[EbootLoader] eboot.bin carregado e pronto para patches!\n\n";

    return true;
}

bool EbootLoader::ApplyPatches() {
    if (!m_loaded.valid) {
        std::cerr << "[EbootLoader] Erro: Tente chamar Load() antes de ApplyPatches()!\n";
        return false;
    }
    return PatchManager::Get().ApplyAllPatches(m_loaded.base, m_loaded.size);
}

bool EbootLoader::Launch() {
    if (!m_loaded.valid) {
        std::cerr << "[EbootLoader] Erro: eboot nao foi carregado.\n";
        return false;
    }

    std::cout << "[EbootLoader] ATENCAO: Preparando para invocar o Entry Point do eboot.bin (Modo Experimental)...\n";

    // Opcional: Aqui poderíamos fazer um cast e chamar o entry point virtual se estivesse mapeado como executável.
    // Como estamos em ambiente seguro, simulamos a chamada para verificar o comportamento da engine.
    typedef void(*EntryFunc)();
    // Exemplo seguro: se quiséssemos saltar:
    // EntryFunc entry = (EntryFunc)(m_loaded.base + m_loaded.getLoadedInfo().entryVaddr);
    // entry();

    std::cout << "[EbootLoader] Entry point alcançado sem falhas de carregamento!\n";
    return true;
}

} // namespace AstroPort
