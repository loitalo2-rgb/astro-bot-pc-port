#include "core/eboot_loader.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace {

struct TemporaryFixture {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("astro_elf_metadata_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".elf");

    ~TemporaryFixture() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

template <typename T>
void WriteLittleEndian(std::vector<uint8_t>& bytes, size_t offset, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        bytes[offset + i] = static_cast<uint8_t>(value & 0xff);
        value >>= 8;
    }
}

std::vector<uint8_t> MakeValidElf() {
    std::vector<uint8_t> bytes(128, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    WriteLittleEndian<uint16_t>(bytes, 16, 2);
    WriteLittleEndian<uint16_t>(bytes, 18, 62);
    WriteLittleEndian<uint32_t>(bytes, 20, 1);
    WriteLittleEndian<uint64_t>(bytes, 24, 0x401000);
    WriteLittleEndian<uint64_t>(bytes, 32, 64);
    WriteLittleEndian<uint16_t>(bytes, 52, 64);
    WriteLittleEndian<uint16_t>(bytes, 54, 56);
    WriteLittleEndian<uint16_t>(bytes, 56, 1);

    WriteLittleEndian<uint32_t>(bytes, 64, 1);
    WriteLittleEndian<uint32_t>(bytes, 68, 5);
    WriteLittleEndian<uint64_t>(bytes, 72, 120);
    WriteLittleEndian<uint64_t>(bytes, 80, 0x401000);
    WriteLittleEndian<uint64_t>(bytes, 96, 8);
    WriteLittleEndian<uint64_t>(bytes, 104, 16);
    for (size_t i = 0; i < 8; ++i) {
        bytes[120 + i] = static_cast<uint8_t>(0xa0 + i);
    }
    return bytes;
}

std::vector<uint8_t> MakeMultiSegmentElf() {
    auto bytes = MakeValidElf();
    bytes.resize(192, 0);
    WriteLittleEndian<uint16_t>(bytes, 56, 2);
    WriteLittleEndian<uint64_t>(bytes, 72, 176);
    for (size_t i = 0; i < 8; ++i) {
        bytes[176 + i] = static_cast<uint8_t>(0xb0 + i);
        bytes[184 + i] = static_cast<uint8_t>(0xc0 + i);
    }

    WriteLittleEndian<uint32_t>(bytes, 120, 1);
    WriteLittleEndian<uint32_t>(bytes, 124, 4);
    WriteLittleEndian<uint64_t>(bytes, 128, 184);
    WriteLittleEndian<uint64_t>(bytes, 136, 0x402000);
    WriteLittleEndian<uint64_t>(bytes, 152, 8);
    WriteLittleEndian<uint64_t>(bytes, 160, 8);
    return bytes;
}

// ---------------------------------------------------------------------------
// Synthetic relocation fixture (self-authored; not derived from any game file)
// ---------------------------------------------------------------------------
constexpr uint64_t kRelocImageBase = 0x401000;
constexpr size_t kRelocGotSlotOffset = 0x10;
constexpr size_t kRelocRelaOffset = 0x100;

struct RelocElfSpec {
    bool includeDynamic = true;   // PT_DYNAMIC program header present
    bool includeRela = true;      // DT_RELA
    bool includeRelasz = true;    // DT_RELASZ
    bool includeRelaent = true;   // DT_RELAENT
    uint64_t relaentValue = 24;
    uint64_t relaVaddr = kRelocImageBase + 0x100;  // inside the mapped image
    uint64_t relaSize = 24;
    uint64_t relocOffset = kRelocImageBase + kRelocGotSlotOffset;
    uint64_t relocInfo = 8;       // R_X86_64_RELATIVE, symbol index 0
    uint64_t addend = 0x401018;
    std::vector<std::pair<uint64_t, uint64_t>> extraDynamicEntries;
};

std::vector<uint8_t> MakeRelocationsElf(const RelocElfSpec& spec) {
    std::vector<std::pair<uint64_t, uint64_t>> dynEntries;
    const auto pushDyn = [&dynEntries](uint64_t tag, uint64_t value) {
        dynEntries.emplace_back(tag, value);
    };
    if (spec.includeRela) pushDyn(7, spec.relaVaddr);
    if (spec.includeRelasz) pushDyn(8, spec.relaSize);
    if (spec.includeRelaent) pushDyn(9, spec.relaentValue);
    for (const auto& entry : spec.extraDynamicEntries) {
        pushDyn(entry.first, entry.second);
    }
    pushDyn(0, 0);  // DT_NULL terminates the table

    const size_t dynamicEntryCount = dynEntries.size();
    const size_t dynamicBytes =
        spec.includeDynamic ? dynamicEntryCount * 16 : 0;
    const size_t imageDataSize = 0x120 + dynamicBytes;
    const size_t memSize = imageDataSize + 8;
    const size_t fileDataOffset = 176;

    std::vector<uint8_t> bytes(fileDataOffset + imageDataSize, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    WriteLittleEndian<uint16_t>(bytes, 16, 3);       // ET_DYN
    WriteLittleEndian<uint16_t>(bytes, 18, 62);      // EM_X86_64
    WriteLittleEndian<uint32_t>(bytes, 20, 1);
    WriteLittleEndian<uint64_t>(bytes, 24, kRelocImageBase + 8);
    WriteLittleEndian<uint64_t>(bytes, 32, 64);      // phoff
    WriteLittleEndian<uint16_t>(bytes, 52, 64);      // ehsize
    WriteLittleEndian<uint16_t>(bytes, 54, 56);      // phentsize
    WriteLittleEndian<uint16_t>(bytes, 56, spec.includeDynamic ? 2 : 1);

    size_t ph = 64;
    WriteLittleEndian<uint32_t>(bytes, ph, 1);       // PT_LOAD
    WriteLittleEndian<uint32_t>(bytes, ph + 4, 5);   // PF_R | PF_X
    WriteLittleEndian<uint64_t>(bytes, ph + 8, fileDataOffset);
    WriteLittleEndian<uint64_t>(bytes, ph + 16, kRelocImageBase);
    WriteLittleEndian<uint64_t>(bytes, ph + 32, imageDataSize);
    WriteLittleEndian<uint64_t>(bytes, ph + 40, memSize);
    WriteLittleEndian<uint64_t>(bytes, ph + 48, 16);

    // Image contents: filler pattern, one GOT slot marker, one relocation
    // entry and (optionally) an in-image copy of the dynamic table.
    for (size_t i = 0; i < imageDataSize; ++i) {
        bytes[fileDataOffset + i] = 0x5a;
    }
    for (size_t i = 0; i < 8; ++i) {
        bytes[fileDataOffset + kRelocGotSlotOffset + i] =
            static_cast<uint8_t>(0xa0 + i);
    }
    if (spec.includeDynamic) {
        for (size_t i = 0; i < dynamicEntryCount; ++i) {
            WriteLittleEndian<uint64_t>(bytes,
                                        fileDataOffset + 0x120 + i * 16,
                                        dynEntries[i].first);
            WriteLittleEndian<uint64_t>(bytes,
                                        fileDataOffset + 0x120 + i * 16 + 8,
                                        dynEntries[i].second);
        }
        ph = 64 + 56;
        WriteLittleEndian<uint32_t>(bytes, ph, 2);   // PT_DYNAMIC
        WriteLittleEndian<uint32_t>(bytes, ph + 4, 4);
        WriteLittleEndian<uint64_t>(bytes, ph + 8,
                                    fileDataOffset + 0x120);
        WriteLittleEndian<uint64_t>(bytes, ph + 16, kRelocImageBase + 0x120);
        WriteLittleEndian<uint64_t>(bytes, ph + 32, dynamicBytes);
        WriteLittleEndian<uint64_t>(bytes, ph + 40, dynamicBytes);
        WriteLittleEndian<uint64_t>(bytes, ph + 48, 16);
    }
    WriteLittleEndian<uint64_t>(bytes, fileDataOffset + kRelocRelaOffset,
                                spec.relocOffset);
    WriteLittleEndian<uint64_t>(bytes, fileDataOffset + kRelocRelaOffset + 8,
                                spec.relocInfo);
    WriteLittleEndian<uint64_t>(bytes, fileDataOffset + kRelocRelaOffset + 16,
                                spec.addend);
    return bytes;
}

uint64_t ReadLe64(const std::vector<uint8_t>& bytes, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    }
    return value;
}

}

int main() {
    auto valid = MakeValidElf();
    AstroPort::Elf64Metadata metadata;
    if (!AstroPort::ParseElf64Metadata(valid.data(), valid.size(), metadata) ||
        metadata.entryVaddr != 0x401000 ||
        !metadata.hasExecutableSegment ||
        metadata.executableSegmentOffset != 120 ||
        metadata.executableSegmentVaddr != 0x401000 ||
        metadata.executableSegmentFileSize != 8 ||
        metadata.executableSegmentMemorySize != 16 ||
        metadata.type != 2 || metadata.machine != 62 ||
        metadata.loadSegments.size() != 1) {
        std::cerr << "Valid synthetic ELF64 metadata was not parsed correctly.\n";
        return 1;
    }

    AstroPort::Elf64MappedImage mapped;
    if (!AstroPort::MapElf64LoadSegments(valid.data(), valid.size(), metadata,
                                         mapped) ||
        mapped.virtualBase != 0x401000 || mapped.bytes.size() != 16) {
        std::cerr << "Valid PT_LOAD segment was not mapped.\n";
        return 1;
    }
    for (size_t i = 0; i < 8; ++i) {
        if (mapped.bytes[i] != static_cast<uint8_t>(0xa0 + i)) {
            std::cerr << "PT_LOAD file bytes were not copied correctly.\n";
            return 1;
        }
    }
    for (size_t i = 8; i < mapped.bytes.size(); ++i) {
        if (mapped.bytes[i] != 0) {
            std::cerr << "PT_LOAD BSS region was not zero-initialized.\n";
            return 1;
        }
    }

    AstroPort::Elf64MappedImage rejectedImage;
    if (AstroPort::MapElf64LoadSegments(valid.data(), valid.size(), metadata,
                                        rejectedImage, 15) ||
        !rejectedImage.bytes.empty()) {
        std::cerr << "Image size limit was not enforced.\n";
        return 1;
    }
    auto unsupportedType = metadata;
    unsupportedType.type = 1;
    if (AstroPort::MapElf64LoadSegments(valid.data(), valid.size(),
                                        unsupportedType, rejectedImage)) {
        std::cerr << "Relocatable ELF was incorrectly accepted as an image.\n";
        return 1;
    }
    auto unsupportedMachine = metadata;
    unsupportedMachine.machine = 183;
    if (AstroPort::MapElf64LoadSegments(valid.data(), valid.size(),
                                        unsupportedMachine, rejectedImage)) {
        std::cerr << "Non-x86_64 ELF was incorrectly accepted as an image.\n";
        return 1;
    }

    auto multiSegment = MakeMultiSegmentElf();
    AstroPort::Elf64Metadata multiMetadata;
    AstroPort::Elf64MappedImage multiImage;
    if (!AstroPort::ParseElf64Metadata(multiSegment.data(), multiSegment.size(),
                                       multiMetadata) ||
        multiMetadata.loadSegments.size() != 2 ||
        !AstroPort::MapElf64LoadSegments(multiSegment.data(),
                                         multiSegment.size(),
                                         multiMetadata, multiImage) ||
        multiImage.virtualBase != 0x401000 ||
        multiImage.bytes.size() != 0x1008) {
        std::cerr << "Multiple PT_LOAD segments were not mapped correctly.\n";
        return 1;
    }
    if (multiImage.bytes[0] != 0xb0 ||
        multiImage.bytes[8] != 0 ||
        multiImage.bytes[16] != 0 ||
        multiImage.bytes[0x1000] != 0xc0) {
        std::cerr << "Multi-segment data, gap, or BSS contents are incorrect.\n";
        return 1;
    }

    auto overlappingMetadata = multiMetadata;
    overlappingMetadata.loadSegments[1].virtualAddress = 0x401008;
    if (AstroPort::MapElf64LoadSegments(multiSegment.data(),
                                        multiSegment.size(),
                                        overlappingMetadata, rejectedImage)) {
        std::cerr << "Overlapping PT_LOAD ranges were incorrectly accepted.\n";
        return 1;
    }

    // ---------------------------------------------------------------------------
    // Relocations: R_X86_64_RELATIVE support plus explicit rejections.
    // ---------------------------------------------------------------------------
    {
        RelocElfSpec spec;
        auto relocElf = MakeRelocationsElf(spec);
        AstroPort::Elf64Metadata relocMetadata;
        if (!AstroPort::ParseElf64Metadata(relocElf.data(), relocElf.size(),
                                           relocMetadata) ||
            !relocMetadata.hasDynamicSegment ||
            relocMetadata.loadSegments.size() != 1 ||
            relocMetadata.dynamicSegment.fileSize != 64) {
            std::cerr << "PT_DYNAMIC was not parsed from the relocation fixture.\n";
            return 1;
        }

        AstroPort::Elf64MappedImage relocImage;
        if (!AstroPort::MapElf64LoadSegments(relocElf.data(), relocElf.size(),
                                             relocMetadata, relocImage) ||
            relocImage.virtualBase != kRelocImageBase ||
            relocImage.bytes.empty()) {
            std::cerr << "Relocation fixture image was not mapped.\n";
            return 1;
        }
        const std::vector<uint8_t> beforeRelocations = relocImage.bytes;

        AstroPort::Elf64RelocationResult relocResult;
        if (!AstroPort::ApplyElf64RelativeRelocations(
                relocElf.data(), relocElf.size(), relocMetadata, relocImage,
                relocResult) ||
            relocResult.relativeApplied != 1) {
            std::cerr << "Valid R_X86_64_RELATIVE relocation was not applied.\n";
            return 1;
        }
        const uint64_t patched =
            ReadLe64(relocImage.bytes, kRelocGotSlotOffset);
        if (patched != kRelocImageBase + spec.addend) {
            std::cerr << "Relocated GOT slot did not receive base + addend.\n";
            return 1;
        }
        for (size_t i = 0; i < beforeRelocations.size(); ++i) {
            const bool insidePatchedSlot =
                i >= kRelocGotSlotOffset && i < kRelocGotSlotOffset + 8;
            if (!insidePatchedSlot &&
                relocImage.bytes[i] != beforeRelocations[i]) {
                std::cerr << "Relocation application touched bytes outside the target slot.\n";
                return 1;
            }
        }

        // Fixture without PT_DYNAMIC: success with zero relocations applied.
        RelocElfSpec withoutDynamic;
        withoutDynamic.includeDynamic = false;
        auto dynFreeElf = MakeRelocationsElf(withoutDynamic);
        AstroPort::Elf64Metadata dynFreeMetadata;
        AstroPort::Elf64MappedImage dynFreeImage;
        if (!AstroPort::ParseElf64Metadata(dynFreeElf.data(), dynFreeElf.size(),
                                           dynFreeMetadata) ||
            dynFreeMetadata.hasDynamicSegment ||
            !AstroPort::MapElf64LoadSegments(dynFreeElf.data(),
                                             dynFreeElf.size(),
                                             dynFreeMetadata, dynFreeImage)) {
            std::cerr << "Fixture without PT_DYNAMIC was not mapped.\n";
            return 1;
        }
        const std::vector<uint8_t> dynFreeBefore = dynFreeImage.bytes;
        AstroPort::Elf64RelocationResult dynFreeResult;
        if (!AstroPort::ApplyElf64RelativeRelocations(
                dynFreeElf.data(), dynFreeElf.size(), dynFreeMetadata,
                dynFreeImage, dynFreeResult) ||
            dynFreeResult.relativeApplied != 0 ||
            dynFreeImage.bytes != dynFreeBefore) {
            std::cerr << "Fixture without PT_DYNAMIC was not handled as a no-op.\n";
            return 1;
        }

        // Dynamic table with only DT_NULL: still a no-op.
        RelocElfSpec emptyDynamic;
        emptyDynamic.includeRela = false;
        emptyDynamic.includeRelasz = false;
        emptyDynamic.includeRelaent = false;
        auto emptyDynElf = MakeRelocationsElf(emptyDynamic);
        AstroPort::Elf64Metadata emptyDynMetadata;
        AstroPort::Elf64MappedImage emptyDynImage;
        if (!AstroPort::ParseElf64Metadata(emptyDynElf.data(),
                                           emptyDynElf.size(),
                                           emptyDynMetadata) ||
            !AstroPort::MapElf64LoadSegments(emptyDynElf.data(),
                                             emptyDynElf.size(),
                                             emptyDynMetadata, emptyDynImage)) {
            std::cerr << "Empty-dynamic fixture was not mapped.\n";
            return 1;
        }
        AstroPort::Elf64RelocationResult emptyDynResult;
        if (!AstroPort::ApplyElf64RelativeRelocations(
                emptyDynElf.data(), emptyDynElf.size(), emptyDynMetadata,
                emptyDynImage, emptyDynResult) ||
            emptyDynResult.relativeApplied != 0) {
            std::cerr << "Empty dynamic table was not treated as a no-op.\n";
            return 1;
        }

        const auto expectRejected = [](const RelocElfSpec& spec,
                                       const char* name) {
            auto fixture = MakeRelocationsElf(spec);
            AstroPort::Elf64Metadata metadata;
            if (!AstroPort::ParseElf64Metadata(fixture.data(), fixture.size(),
                                               metadata)) {
                std::cerr << "Rejected fixture failed too early in metadata parsing: "
                          << name << '\n';
                return false;
            }
            AstroPort::Elf64MappedImage image;
            if (!AstroPort::MapElf64LoadSegments(fixture.data(), fixture.size(),
                                                 metadata, image)) {
                std::cerr << "Rejected fixture image was not mapped first: "
                          << name << '\n';
                return false;
            }
            const std::vector<uint8_t> before = image.bytes;
            AstroPort::Elf64RelocationResult result;
            if (AstroPort::ApplyElf64RelativeRelocations(
                    fixture.data(), fixture.size(), metadata, image, result) ||
                result.relativeApplied != 0 || image.bytes != before) {
                std::cerr << "Rejected relocation did not fail closed: "
                          << name << '\n';
                return false;
            }
            return true;
        };

        RelocElfSpec unknownType;
        unknownType.relocInfo = 1;  // R_X86_64_64: symbol-based, not supported
        if (!expectRejected(unknownType, "unknown relocation type")) return 1;

        RelocElfSpec nonRelativeSymbol;
        nonRelativeSymbol.relocInfo = (1ULL << 32) | 8;
        if (!expectRejected(nonRelativeSymbol, "symbol-bearing relocation"))
            return 1;

        RelocElfSpec offsetBeforeBase;
        offsetBeforeBase.relocOffset = 0x400000;
        if (!expectRejected(offsetBeforeBase, "r_offset before image base"))
            return 1;

        RelocElfSpec offsetBeyondImage;
        offsetBeyondImage.relocOffset = 0x402000;
        if (!expectRejected(offsetBeyondImage, "r_offset beyond image")) return 1;

        RelocElfSpec unalignedTableSize;
        unalignedTableSize.relaSize = 25;
        if (!expectRejected(unalignedTableSize, "DT_RELASZ not multiple of 24"))
            return 1;

        RelocElfSpec unsupportedEntrySize;
        unsupportedEntrySize.relaentValue = 17;
        if (!expectRejected(unsupportedEntrySize, "unsupported DT_RELAENT"))
            return 1;

        RelocElfSpec pltRels;
        pltRels.extraDynamicEntries.emplace_back(23, 0x401110);  // DT_JMPREL
        auto pltFixture = MakeRelocationsElf(pltRels);
        AstroPort::Elf64Metadata pltMetadata;
        AstroPort::Elf64MappedImage pltImage;
        if (!AstroPort::ParseElf64Metadata(pltFixture.data(), pltFixture.size(), pltMetadata) ||
            !AstroPort::MapElf64LoadSegments(pltFixture.data(), pltFixture.size(), pltMetadata, pltImage)) {
            std::cerr << "DT_JMPREL fixture was not mapped.\n";
            return 1;
        }
        AstroPort::Elf64RelocationResult pltResult;
        if (!AstroPort::ApplyElf64RelativeRelocations(pltFixture.data(), pltFixture.size(), pltMetadata, pltImage, pltResult)) {
            std::cerr << "DT_JMPREL fixture relocation application failed.\n";
            return 1;
        }

        RelocElfSpec textRel;
        textRel.extraDynamicEntries.emplace_back(30, 1);  // DT_TEXTREL
        if (!expectRejected(textRel, "DT_TEXTREL present")) return 1;

        RelocElfSpec packedRels;
        packedRels.extraDynamicEntries.emplace_back(36, 0x401130);  // DT_RELR
        if (!expectRejected(packedRels, "DT_RELR present")) return 1;

        RelocElfSpec relaWithoutSize;
        relaWithoutSize.includeRelasz = false;
        if (!expectRejected(relaWithoutSize, "DT_RELA without DT_RELASZ"))
            return 1;

        RelocElfSpec sizeWithoutRela;
        sizeWithoutRela.includeRela = false;
        if (!expectRejected(sizeWithoutRela, "DT_RELASZ without DT_RELA"))
            return 1;

        RelocElfSpec tableBeforeBase;
        tableBeforeBase.relaVaddr = 0x400000;
        if (!expectRejected(tableBeforeBase, "DT_RELA before image base"))
            return 1;

        RelocElfSpec tableBeyondImage;
        tableBeyondImage.relaVaddr = 0x402000;
        if (!expectRejected(tableBeyondImage, "DT_RELA beyond image")) return 1;

        RelocElfSpec tableEndBeyondImage;
        tableEndBeyondImage.relaVaddr = 0x401150;
        if (!expectRejected(tableEndBeyondImage, "DT_RELA table end beyond image"))
            return 1;
    }

    auto checkRejected = [](std::vector<uint8_t> bytes, const char* name) {
        AstroPort::Elf64Metadata metadata;
        metadata.entryVaddr = 1;
        metadata.hasExecutableSegment = true;
        if (AstroPort::ParseElf64Metadata(bytes.data(), bytes.size(), metadata)) {
            std::cerr << "Malformed ELF accepted: " << name << '\n';
            return false;
        }
        if (metadata.entryVaddr != 0 || metadata.hasExecutableSegment) {
            std::cerr << "Failed parse retained partial metadata: " << name << '\n';
            return false;
        }
        return true;
    };

    if (!checkRejected(std::vector<uint8_t>(63), "truncated header")) return 1;

    auto malformed = valid;
    malformed[4] = 1;
    if (!checkRejected(malformed, "wrong ELF class")) return 1;

    malformed = valid;
    malformed[5] = 2;
    if (!checkRejected(malformed, "wrong byte order")) return 1;

    malformed = valid;
    WriteLittleEndian<uint16_t>(malformed, 52, 63);
    if (!checkRejected(malformed, "undersized ELF header")) return 1;

    malformed = valid;
    WriteLittleEndian<uint16_t>(malformed, 54, 55);
    if (!checkRejected(malformed, "undersized program header")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 112, 3);
    if (!checkRejected(malformed, "non-power-of-two segment alignment")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 112, 8);
    WriteLittleEndian<uint64_t>(malformed, 72, 121);
    if (!checkRejected(malformed, "incongruent segment alignment")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 32,
                                std::numeric_limits<uint64_t>::max());
    if (!checkRejected(malformed, "program header offset overflow")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 72, 124);
    WriteLittleEndian<uint64_t>(malformed, 96, 8);
    if (!checkRejected(malformed, "segment outside file")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 104, 7);
    if (!checkRejected(malformed, "memory size smaller than file size")) return 1;

    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 80,
                                std::numeric_limits<uint64_t>::max() - 3);
    WriteLittleEndian<uint64_t>(malformed, 104, 8);
    if (!checkRejected(malformed, "virtual address overflow")) return 1;

    AstroPort::Elf64Metadata headerOnly;
    malformed = valid;
    WriteLittleEndian<uint64_t>(malformed, 32,
                                std::numeric_limits<uint64_t>::max());
    if (!AstroPort::ParseElf64Metadata(malformed.data(), malformed.size(),
                                       headerOnly, false) ||
        headerOnly.entryVaddr != 0x401000) {
        std::cerr << "Header-only metadata inspection should not read program headers.\n";
        return 1;
    }

    TemporaryFixture tempFixture;
    {
        std::ofstream fixtureFile(tempFixture.path, std::ios::binary);
        if (!fixtureFile ||
            !fixtureFile.write(reinterpret_cast<const char*>(valid.data()),
                               static_cast<std::streamsize>(valid.size()))) {
            std::cerr << "Could not write synthetic ELF lifecycle fixture.\n";
            return 1;
        }
    }

    AstroPort::EbootLoader& loader = AstroPort::EbootLoader::Get();
    loader.Load(tempFixture.path.string());
    const AstroPort::LoadedEboot& loaded = loader.GetLoadedInfo();
    const bool failClosed = loaded.valid && loaded.base &&
                            loaded.isRawElf &&
                            loaded.entryVaddr == 0x401000 &&
                            loaded.codeOffset == 120 &&
                            !loader.Launch();
    if (!failClosed) {
        std::cerr << "ELF load did not behave as expected with valid ELF.\n";
        return 1;
    }

    malformed = valid;
    malformed[4] = 1;
    {
        std::ofstream fixtureFile(tempFixture.path, std::ios::binary);
        if (!fixtureFile ||
            !fixtureFile.write(reinterpret_cast<const char*>(malformed.data()),
                               static_cast<std::streamsize>(malformed.size()))) {
            std::cerr << "Could not write malformed ELF lifecycle fixture.\n";
            return 1;
        }
    }
    const bool malformedAccepted = loader.Load(tempFixture.path.string());
    const AstroPort::LoadedEboot& rejected = loader.GetLoadedInfo();
    const bool staleStateCleared = !malformedAccepted && !rejected.valid &&
                                   !rejected.base && rejected.isRawElf &&
                                   rejected.entryVaddr == 0 &&
                                   rejected.codeOffset == 0;
    if (!staleStateCleared) {
        std::cerr << "Rejected ELF retained stale loader state or resources.\n";
        return 1;
    }

    {
        std::ofstream emptyFile(tempFixture.path, std::ios::binary);
        if (!emptyFile) {
            std::cerr << "Could not write empty-file lifecycle fixture.\n";
            return 1;
        }
    }
    const bool emptyAccepted = loader.Load(tempFixture.path.string());
    const AstroPort::LoadedEboot& empty = loader.GetLoadedInfo();
    if (emptyAccepted || empty.valid || empty.base || empty.size != 0) {
        std::cerr << "Empty input was not rejected cleanly.\n";
        return 1;
    }

    const bool missingAccepted = loader.Load(tempFixture.path.string());
    const AstroPort::LoadedEboot& missing = loader.GetLoadedInfo();
    if (missingAccepted || missing.valid || missing.base || missing.size != 0) {
        std::cerr << "Missing input was not rejected cleanly.\n";
        return 1;
    }

    std::cout << "ELF metadata and fail-closed loader validation passed.\n";
    return 0;
}
