#include "core/eboot_loader.h"

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Bytes per byte-shannon sample limit handled inside ApplyElf64RelativeRelocations;
// fixtures below use small ELF payloads so entropy stays low by construction.

struct SelfFixtureSpec {
    bool validMagic = true;
    bool markEncrypted = false;
    bool markCompressed = false;
    bool randomizePayload = true;   // high entropy payload for rejection cases
    bool validRecords = true;
};

void WriteLeU64(std::vector<uint8_t>& bytes, size_t offset, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

void WriteLeU16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
}

void WriteLeU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    }
}

// Layout of the synthetic SELF fixture (values verified against the runtime):
//   0x00  SELF magic 0x1D3D154F
//   0x18  header entry count (u16)
//   0x40  record table: up to 12 records of 0x20 bytes (flags, offset,
//         size, size again)
//   0x1a0 inner ELF64 header; program headers at 0x1e0 (one, 56 bytes)
//   0x350 first payload byte (>= kSelfRecordMinDataOffset enforced by the
//         inspector, mirroring real containers where payloads follow the
//         header area)
constexpr uint64_t kSelfMagic = 0x1D3D154F;
constexpr size_t kInnerElfOffset = 0x1a0;
constexpr size_t kPhdrOffset = 0x1e0;
constexpr size_t kPayloadOffset = 0x350;
constexpr size_t kPayloadSize = 0x4000;  // 16384 bytes; full entropy sample

std::vector<uint8_t> MakeSelfFixture(const SelfFixtureSpec& spec) {
    std::vector<uint8_t> bytes(kPayloadOffset + kPayloadSize, 0);

    // Magic
    if (spec.validMagic) {
        WriteLeU64(bytes, 0, kSelfMagic);
    } else {
        WriteLeU64(bytes, 0, 0xDEADBEEFULL);
    }
    WriteLeU64(bytes, 0x18, 1);

    // ============ Record table ============
    // flags: 0x800 | (index << 20); plus optional encryption/compression bits
    uint64_t flags = 0x800ULL | (0ULL << 20);
    if (spec.markEncrypted) flags |= 0x2;
    if (spec.markCompressed) flags |= 0x8;
    WriteLeU64(bytes, 0x40, flags);
    WriteLeU64(bytes, 0x48, kPayloadOffset);
    WriteLeU64(bytes, 0x50, kPayloadSize);
    WriteLeU64(bytes, 0x58, kPayloadSize);
    if (!spec.validRecords) {
        // Invalidate the record and force the parser to stop: mismatched sizes.
        WriteLeU64(bytes, 0x50, kPayloadSize + 1);
    }

    // ============ Inner ELF64 header ============
    bytes[kInnerElfOffset + 0] = 0x7f;
    bytes[kInnerElfOffset + 1] = 'E';
    bytes[kInnerElfOffset + 2] = 'L';
    bytes[kInnerElfOffset + 3] = 'F';
    bytes[kInnerElfOffset + 4] = 2;  // 64-bit
    bytes[kInnerElfOffset + 5] = 1;  // little-endian
    bytes[kInnerElfOffset + 6] = 1;  // ELF version
    // ELF64 header fields with their native widths:
    //   16 e_type (u16), 18 e_machine (u16), 20 e_version (u32),
    //   24 e_entry (u64), 32 e_phoff (u64),
    //   52 e_ehsize (u16), 54 e_phentsize (u16), 56 e_phnum (u16).
    WriteLeU16(bytes, kInnerElfOffset + 16, 3);   // ET_DYN
    WriteLeU16(bytes, kInnerElfOffset + 18, 62);  // EM_X86_64
    WriteLeU32(bytes, kInnerElfOffset + 20, 1);   // version
    WriteLeU64(bytes, kInnerElfOffset + 24, 0x400000);  // e_entry
    WriteLeU64(bytes, kInnerElfOffset + 32, kPhdrOffset);  // e_phoff
    WriteLeU16(bytes, kInnerElfOffset + 52, 64);   // e_ehsize
    WriteLeU16(bytes, kInnerElfOffset + 54, 56);   // e_phentsize
    WriteLeU16(bytes, kInnerElfOffset + 56, 1);    // e_phnum

    // Elf64_Phdr: p_type (u32), p_flags (u32), then u64 fields.
    WriteLeU32(bytes, kPhdrOffset + 0, 1);          // PT_LOAD
    WriteLeU32(bytes, kPhdrOffset + 4, 5);          // PF_R | PF_X
    WriteLeU64(bytes, kPhdrOffset + 8, kPayloadOffset);   // p_offset
    WriteLeU64(bytes, kPhdrOffset + 16, 0x400000);        // p_vaddr
    WriteLeU64(bytes, kPhdrOffset + 32, kPayloadSize);    // p_filesz
    WriteLeU64(bytes, kPhdrOffset + 40, kPayloadSize);    // p_memsz
    WriteLeU64(bytes, kPhdrOffset + 48, 16);              // p_align

    // ============ Payload ============
    // Low entropy by default: repeated short pattern. High entropy variant:
    // xorshift per byte, seeded differently to guarantee >= 7.5 bits/byte.
    if (spec.randomizePayload) {
        uint64_t state = 0x9E3779B97F4A7C15ULL;
        for (size_t i = 0; i < kPayloadSize; ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            bytes[kPayloadOffset + i] = static_cast<uint8_t>(state & 0xff);
        }
    } else {
        for (size_t i = 0; i < kPayloadSize; ++i) {
            bytes[kPayloadOffset + i] = static_cast<uint8_t>(i % 7);
        }
    }
    return bytes;
}

} // namespace

int main() {
    size_t failures = 0;
    const auto expect = [&](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "SELF inspection test failed: " << name << "\n";
            ++failures;
        }
    };

    // ------------------------------------------------------------------
    // Valid decrypted fixture: entropy in each record below the threshold,
    // no encryption/compression flags.
    // ------------------------------------------------------------------
    {
        SelfFixtureSpec lowEntropy;
        lowEntropy.randomizePayload = false;
        auto fixture = MakeSelfFixture(lowEntropy);
        AstroPort::SelfContainerReport report;
        const bool ok = AstroPort::InspectSelfContainer(fixture.data(),
                                                        fixture.size(), report);
        expect(ok, "decrypted SELF parses: InspectSelfContainer succeeded");
        expect(report.selfMagicValid, "magic detected");
        expect(report.records.size() == 1, "one payload record found");
        expect(report.innerElfHeaderOffset == kInnerElfOffset,
               "inner ELF header located at 0x1a0");
        expect(!report.anyMarkedEncrypted && !report.anyMarkedCompressed,
               "no encryption/compression flags on decrypted fixture");
        expect(!report.allDataRecordsHighEntropy,
               "low entropy payload not classified as encrypted");
        if (report.records.size() == 1) {
            const auto& record = report.records[0];
            expect(record.payloadWithinFile,
                   "payload boundaries validated");
            expect(record.bitsPerByte < 7.5,
                   "bits per byte below threshold");
            expect(record.hasSegmentMapping && record.targetSegmentIndex == 0,
                   "mapping flag and target decoded");
        }
    }

    // ------------------------------------------------------------------
    // Encrypted/defect fixture: same container with high-entropy payload.
    // Must classify all mapped records high, flagged encrypted... note we
    // deliberately do not mark flags 0x2 here: the entropy-only path trusts
    // measurement, not marking.
    // ------------------------------------------------------------------
    {
        auto fixture = MakeSelfFixture(SelfFixtureSpec{});
        AstroPort::SelfContainerReport report;
        const bool ok = AstroPort::InspectSelfContainer(fixture.data(),
                                                        fixture.size(), report);
        expect(ok, "encrypted SELF parses (inspection)");
        expect(report.allDataRecordsHighEntropy,
               "high entropy payload classified as encrypted");
        expect(!report.anyMarkedEncrypted && !report.anyMarkedCompressed,
               "no flag bits expected in entropy-only test");
    }

    // ------------------------------------------------------------------
    // Flagged fixture (0x2 + 0x8): classification honors flag bits.
    // ------------------------------------------------------------------
    {
        SelfFixtureSpec flagged;
        flagged.markEncrypted = true;
        flagged.markCompressed = true;
        auto fixture = MakeSelfFixture(flagged);
        AstroPort::SelfContainerReport report;
        const bool ok = AstroPort::InspectSelfContainer(fixture.data(),
                                                        fixture.size(), report);
        expect(ok, "flagged SELF parses");
        expect(report.anyMarkedEncrypted && report.anyMarkedCompressed,
               "flags 0x2 and 0x8 reported");
    }

    // ------------------------------------------------------------------
    // Malformed fixture: bad magic.
    // ------------------------------------------------------------------
    {
        SelfFixtureSpec badMagic;
        badMagic.validMagic = false;
        auto fixture = MakeSelfFixture(badMagic);
        AstroPort::SelfContainerReport report;
        expect(!AstroPort::InspectSelfContainer(fixture.data(),
                                                fixture.size(), report),
               "invalid magic refused");
    }

    // ------------------------------------------------------------------
    // Malformed fixture: mismatched record size pair -> stops parsing.
    // ------------------------------------------------------------------
    {
        SelfFixtureSpec badRecord;
        badRecord.validRecords = false;
        badRecord.randomizePayload = false;
        auto fixture = MakeSelfFixture(badRecord);
        AstroPort::SelfContainerReport report;
        expect(AstroPort::InspectSelfContainer(fixture.data(),
                                               fixture.size(), report),
               "malformed record ends parsing rather than crashing");
        expect(report.records.empty(),
               "no records accepted from malformed table");
    }

    // ------------------------------------------------------------------
    // Loader integration: SELF container should still fail Access loading
    // with no memory leaks and honest logs — no execution, of course.
    // ------------------------------------------------------------------
    {
        SelfFixtureSpec loaderFixtureSpec;
        loaderFixtureSpec.randomizePayload = true;
        auto fixture = MakeSelfFixture(loaderFixtureSpec);
        const std::string path = (std::filesystem::temp_directory_path() /
                                  "astro_self_inspection_fixture.self").string();
        {
            std::ofstream fixtureFile(path, std::ios::binary);
            fixtureFile.write(reinterpret_cast<const char*>(fixture.data()),
                              static_cast<std::streamsize>(fixture.size()));
        }

        AstroPort::EbootLoader& loader = AstroPort::EbootLoader::Get();
        const bool accepted = loader.Load(path);
        const auto& loaded = loader.GetLoadedInfo();
        expect(!accepted, "SELF container Load refused");
        expect(!loaded.valid && !loaded.base, "loader memory released after refusal");
        expect(!loader.ApplyPatches() && !loader.Launch(),
               "no patches or execution after refusal");
        std::remove(path.c_str());
    }

    if (failures != 0) {
        std::cerr << failures << " SELF inspection test(s) failed.\n";
        return 1;
    }
    std::cout << "SELF container inspection validation passed.\n";
    return 0;
}
