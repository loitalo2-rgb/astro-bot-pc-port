// SELF -> ELF64 reconstruction validation (analysis only; nothing executes).
//
// Builds synthetic SELF containers whose structure mirrors real PS5 layouts
// (record table at 0x40, inner ELF header with stale phdr offsets, trailing
// vaddr-less data after the last covered record) and verifies both the
// successful reconstruction and the fail-closed refusals.

#include "core/eboot_loader.h"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>

namespace {

constexpr uint64_t kSelfMagic = 0x1D3D154F;
constexpr size_t kInnerElfOffset = 0x100;
constexpr size_t kPhdrTableOffset = 0x140;   // file offset of the phdr table
constexpr size_t kPhdrCount = 5;
constexpr size_t kLoad0SelfOffset = 0x400;   // record 0 payload
constexpr size_t kLoad0Size = 0x300;
constexpr size_t kLoad1SelfOffset = 0x800;   // record 1 payload
constexpr size_t kLoad1Size = 0x200;
constexpr size_t kTrailingSelfOffset = 0xA00;  // right after record 1's data
constexpr size_t kTrailingSize = 0x60;
constexpr size_t kFileSize = 0xC00;

struct RebuildSpec {
    bool markEncryptedRec0 = false;
    bool corruptRecordSize = false;   // record size != p_filesz
    bool outOfRangeIndex = false;     // record covers phdr index 7
    bool ehFrameOutsideLoads = false; // uncovered segment vaddr not in a load
    bool ehFrameVaddrZero = false;    // uncovered vaddr-0 before the last record
    bool randomizeLoad0 = false;      // high entropy payload
};

void WriteLe(std::vector<uint8_t>& bytes, size_t offset, uint64_t value,
             size_t width) {
    for (size_t i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

void WritePhdr(std::vector<uint8_t>& bytes, size_t offset, uint32_t type,
               uint32_t flags, uint64_t fileOffset, uint64_t vaddr,
               uint64_t filesz, uint64_t memsz, uint64_t align) {
    WriteLe(bytes, offset, type, 4);
    WriteLe(bytes, offset + 4, flags, 4);
    WriteLe(bytes, offset + 8, fileOffset, 8);
    WriteLe(bytes, offset + 16, vaddr, 8);
    WriteLe(bytes, offset + 24, vaddr, 8);      // p_paddr == p_vaddr
    WriteLe(bytes, offset + 32, filesz, 8);
    WriteLe(bytes, offset + 40, memsz, 8);
    WriteLe(bytes, offset + 48, align, 8);
}

std::vector<uint8_t> MakeSelfFixture(const RebuildSpec& spec) {
    std::vector<uint8_t> bytes(kFileSize, 0);
    WriteLe(bytes, 0, kSelfMagic, 4);
    WriteLe(bytes, 0x18, 2, 2);

    // Record table: two covered records, then the terminator.
    uint64_t flags0 = 0x800 | (0ULL << 20);
    if (spec.markEncryptedRec0) flags0 |= 0x2;
    WriteLe(bytes, 0x40, flags0, 8);
    WriteLe(bytes, 0x48, kLoad0SelfOffset, 8);
    WriteLe(bytes, 0x50, kLoad0Size, 8);
    WriteLe(bytes, 0x58, kLoad0Size, 8);
    if (spec.outOfRangeIndex) {
        WriteLe(bytes, 0x60, 0x800 | (7ULL << 20), 8);
    } else {
        WriteLe(bytes, 0x60, 0x800 | (2ULL << 20), 8);
    }
    WriteLe(bytes, 0x68, kLoad1SelfOffset, 8);
    WriteLe(bytes, 0x70, spec.corruptRecordSize ? kLoad1Size + 1 : kLoad1Size, 8);
    WriteLe(bytes, 0x78, kLoad1Size, 8);
    // 0x80: empty record, terminates the table.

    // Inner ELF64 header at 0x100 (stale e_phoff; rebuilt image fixes it).
    bytes[kInnerElfOffset + 0] = 0x7f;
    bytes[kInnerElfOffset + 1] = 'E';
    bytes[kInnerElfOffset + 2] = 'L';
    bytes[kInnerElfOffset + 3] = 'F';
    bytes[kInnerElfOffset + 4] = 2;
    bytes[kInnerElfOffset + 5] = 1;
    bytes[kInnerElfOffset + 6] = 1;
    WriteLe(bytes, kInnerElfOffset + 16, 0xFE10, 2);  // PS5 OS-specific e_type
    WriteLe(bytes, kInnerElfOffset + 18, 62, 2);      // EM_X86_64
    WriteLe(bytes, kInnerElfOffset + 20, 1, 4);       // e_version
    WriteLe(bytes, kInnerElfOffset + 24, 0x600000, 8);  // e_entry (stale too)
    WriteLe(bytes, kInnerElfOffset + 32, kPhdrTableOffset - kInnerElfOffset, 8);
    WriteLe(bytes, kInnerElfOffset + 52, 64, 2);      // e_ehsize
    WriteLe(bytes, kInnerElfOffset + 54, 56, 2);      // e_phentsize
    WriteLe(bytes, kInnerElfOffset + 56, kPhdrCount, 2);

    // Phdr table (stale file offsets; vaddrs are authoritative).
    constexpr uint32_t kTypeLoad = 1;
    constexpr uint32_t kTypeEhFrame = 0x6474E550;
    constexpr uint32_t kTypeOsSpecific = 0x6FFFFF01;
    constexpr uint32_t kTypeNote = 4;
    size_t ph = kPhdrTableOffset;
    WritePhdr(bytes, ph, kTypeLoad, 5, 0x1000, 0x400000, kLoad0Size,
              kLoad0Size, 0x1000);                    // [0] load (covered)
    WritePhdr(bytes, ph + 56, kTypeEhFrame, 4, 0x1100,
              spec.ehFrameOutsideLoads ? 0x600000u
                   : (spec.ehFrameVaddrZero ? 0 : 0x400100u),
              0x100, 0x100, 1);                       // [1] inside load 0
    WritePhdr(bytes, ph + 112, kTypeLoad, 5, 0x2000, 0x401000, kLoad1Size,
              kLoad1Size, 0x1000);                    // [2] load (covered)
    WritePhdr(bytes, ph + 168, kTypeOsSpecific, 0, 0x3000, 0, kTrailingSize,
              kTrailingSize, 0x10);                   // [3] trailing, vaddr 0
    WritePhdr(bytes, ph + 224, kTypeNote, 0, 0, 0, 0, 0, 0);  // [4] dataless

    // Payloads: low entropy by default; high entropy variant for load 0.
    if (spec.randomizeLoad0) {
        uint64_t state = 0x9E3779B97F4A7C15ULL;
        for (size_t i = 0; i < kLoad0Size; ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            bytes[kLoad0SelfOffset + i] = static_cast<uint8_t>(state & 0xff);
        }
        for (size_t i = 0; i < kLoad1Size; ++i) {
            bytes[kLoad1SelfOffset + i] = static_cast<uint8_t>(i % 7);
        }
    } else {
        for (size_t i = 0; i < kLoad0Size; ++i) {
            bytes[kLoad0SelfOffset + i] = static_cast<uint8_t>(i % 3);
        }
        for (size_t i = 0; i < kLoad1Size; ++i) {
            bytes[kLoad1SelfOffset + i] = static_cast<uint8_t>(i % 7);
        }
    }
    for (size_t i = 0; i < kTrailingSize; ++i) {
        bytes[kTrailingSelfOffset + i] = static_cast<uint8_t>(0xF0 - (i % 5));
    }
    return bytes;
}

uint64_t ReadLe(const std::vector<uint8_t>& bytes, size_t offset,
                size_t width) {
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    }
    return value;
}

} // namespace

int main() {
    size_t failures = 0;
    const auto expect = [&](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "SELF reconstruction test failed: " << name << "\n";
            ++failures;
        }
    };

    // ------------------------------------------------------------------
    // Successful reconstruction of a representative container.
    // ------------------------------------------------------------------
    {
        auto fixture = MakeSelfFixture(RebuildSpec{});
        AstroPort::Elf64RebuiltInfo rebuilt;
        const bool ok = AstroPort::ReconstructElf64FromSelf(
            fixture.data(), fixture.size(), rebuilt);
        expect(ok, "representative SELF reconstructed");
        if (ok) {
            expect(rebuilt.programHeaders == kPhdrCount,
                   "all five program headers rebuilt");
            expect(rebuilt.type == 3 && rebuilt.typeNormalized,
                   "PS5 e_type 0xFE10 normalized to ET_DYN");
            expect(rebuilt.machine == 62, "machine preserved");
            expect(rebuilt.entryVaddr == 0x600000, "entry preserved");
            expect(rebuilt.segmentsDataCopied == 2 &&
                   rebuilt.segmentsProvidedByLoad == 1 &&
                   rebuilt.segmentsTrailingCopied == 1 &&
                   rebuilt.segmentsDataless == 1,
                   "segment placement counters correct");
            expect(rebuilt.bytes.size() == 0x3060,
                   "rebuilt image size matches the layout");

            const auto& b = rebuilt.bytes;
            expect(b[4] == 2 && b[5] == 1 && b[6] == 1, "ELF64 class preserved");
            expect(ReadLe(b, 16, 2) == 3, "e_type normalized in bytes");
            expect(ReadLe(b, 32, 8) == 64, "e_phoff moved to 64");
            expect(ReadLe(b, 40, 8) == 0 && ReadLe(b, 60, 2) == 0 &&
                   ReadLe(b, 62, 2) == 0,
                   "section headers stripped");

            // New phdr placements.
            expect(ReadLe(b, 64 + 8, 8) == 0x1000,
                   "load 0 placed congruent with vaddr 0x400000");
            expect(ReadLe(b, 64 + 56 + 8, 8) == 0x1100,
                   "eh-frame points into load 0's image");
            expect(ReadLe(b, 64 + 112 + 8, 8) == 0x2000,
                   "load 1 placed after load 0's data");
            expect(ReadLe(b, 64 + 168 + 8, 8) == 0x3000,
                   "trailing segment placed (page-aligned) after the loads");
            expect(ReadLe(b, 64 + 224 + 8, 8) == 0,
                   "dataless note has offset 0");

            // Content checks: every rebuilt byte matches its SELF source.
            bool load0Equal = true;
            for (size_t i = 0; i < kLoad0Size; ++i) {
                if (b[0x1000 + i] != fixture[kLoad0SelfOffset + i]) {
                    load0Equal = false;
                    break;
                }
            }
            expect(load0Equal, "load 0 bytes copied from the record payload");
            bool ehEqual = true;
            for (size_t i = 0; i < 0x100; ++i) {
                if (b[0x1100 + i] != fixture[kLoad0SelfOffset + 0x100 + i]) {
                    ehEqual = false;
                    break;
                }
            }
            expect(ehEqual, "eh-frame bytes live inside load 0's copy");
            bool load1Equal = true;
            for (size_t i = 0; i < kLoad1Size; ++i) {
                if (b[0x2000 + i] != fixture[kLoad1SelfOffset + i]) {
                    load1Equal = false;
                    break;
                }
            }
            expect(load1Equal, "load 1 bytes copied from the record payload");
            bool trailingEqual = true;
            for (size_t i = 0; i < kTrailingSize; ++i) {
                if (b[0x3000 + i] != fixture[kTrailingSelfOffset + i]) {
                    trailingEqual = false;
                    break;
                }
            }
            expect(trailingEqual, "trailing bytes copied after the last record");

            // Our own parser and mapper accept the rebuilt artifact.
            AstroPort::Elf64Metadata metadata;
            expect(AstroPort::ParseElf64Metadata(b.data(), b.size(), metadata,
                                                 true),
                   "rebuilt image parses as ELF64 metadata");
            AstroPort::Elf64MappedImage image;
            expect(AstroPort::MapElf64LoadSegments(b.data(), b.size(),
                                                   metadata, image),
                   "rebuilt PT_LOADs satisfy the mapping checks");
            expect(image.bytes.size() == 0x1200,
                   "mapped virtual image covers both loads");
            expect(image.virtualBase == 0x400000, "virtual base is load 0");
        }
    }

    // ------------------------------------------------------------------
    // Fail-closed refusals.
    // ------------------------------------------------------------------
    {
        RebuildSpec spec;
        spec.markEncryptedRec0 = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "encrypted marker refuses reconstruction");
    }
    {
        RebuildSpec spec;
        spec.corruptRecordSize = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "record/p_filesz mismatch refuses reconstruction");
    }
    {
        RebuildSpec spec;
        spec.outOfRangeIndex = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "record covering a nonexistent segment refuses reconstruction");
    }
    {
        RebuildSpec spec;
        spec.ehFrameOutsideLoads = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "unexplained uncovered segment refuses reconstruction");
    }
    {
        RebuildSpec spec;
        spec.ehFrameVaddrZero = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "uncovered vaddr-0 data before the last record refuses");
    }
    {
        RebuildSpec spec;
        spec.randomizeLoad0 = true;
        auto fixture = MakeSelfFixture(spec);
        AstroPort::Elf64RebuiltInfo rebuilt;
        expect(!AstroPort::ReconstructElf64FromSelf(fixture.data(),
                                                    fixture.size(), rebuilt),
               "high-entropy payload refuses reconstruction");
    }

    if (failures != 0) {
        std::cerr << failures << " SELF reconstruction test(s) failed.\n";
        return 1;
    }
    std::cout << "SELF -> ELF64 reconstruction validation passed.\n";
    return 0;
}
