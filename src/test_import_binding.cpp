// Import-binding tests: planner and atomic application of RELATIVE + symbolic
// relocations on a synthetic ELF64 image. Nothing is executed.

#include "core/eboot_loader.h"
#include "core/import_binding.h"
#include "core/nid_resolver.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

void WriteLe(std::vector<uint8_t>& bytes, size_t offset, uint64_t value,
             size_t width) {
    for (size_t i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

void WriteString(std::vector<uint8_t>& bytes, size_t offset,
                 const std::string& text) {
    std::memcpy(bytes.data() + offset, text.data(), text.size());
}

struct Fixture {
    std::vector<uint8_t> bytes;
    uint64_t addend = 0x40;
    uint64_t targetRelative = 0x1700;
    uint64_t target64 = 0x1708;
    uint64_t targetGlobDat = 0x1710;
    uint64_t targetJumpSlot = 0x1718;
};

constexpr size_t kRelaOffset = 0x600;
constexpr size_t kJmprelOffset = 0x700;
constexpr size_t kDynOffset = 0x100;
constexpr size_t kSymbolCount = 3;
constexpr uint64_t kMallocEncodedOffset = 1;
constexpr uint64_t kUnknownEncodedOffset = 17;

Fixture MakeFixture() {
    Fixture fixture;
    fixture.bytes.assign(0x900, 0);
    auto& bytes = fixture.bytes;

    // ELF64 header.
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    WriteLe(bytes, 0x10, 3, 2);     // ET_DYN
    WriteLe(bytes, 0x12, 0x3E, 2);  // x86-64
    WriteLe(bytes, 0x14, 1, 4);
    WriteLe(bytes, 0x20, 0x40, 8);  // e_phoff
    WriteLe(bytes, 0x34, 64, 2);
    WriteLe(bytes, 0x36, 56, 2);
    WriteLe(bytes, 0x38, 2, 2);

    // PT_LOAD: file 0x100, vaddr 0x1000, 0x1000 bytes.
    WriteLe(bytes, 0x40 + 0, 1, 4);
    WriteLe(bytes, 0x40 + 4, 5, 4);
    WriteLe(bytes, 0x40 + 8, 0x100, 8);
    WriteLe(bytes, 0x40 + 16, 0x1000, 8);
    WriteLe(bytes, 0x40 + 32, 0x800, 8);
    WriteLe(bytes, 0x40 + 40, 0x800, 8);
    WriteLe(bytes, 0x40 + 48, 0x100, 8);

    // PT_DYNAMIC: file 0x100, vaddr 0x1000, 0x200 bytes.
    WriteLe(bytes, 0x78 + 0, 2, 4);
    WriteLe(bytes, 0x78 + 4, 6, 4);
    WriteLe(bytes, 0x78 + 8, 0x100, 8);
    WriteLe(bytes, 0x78 + 16, 0x1000, 8);
    WriteLe(bytes, 0x78 + 32, 0x200, 8);
    WriteLe(bytes, 0x78 + 40, 0x200, 8);
    WriteLe(bytes, 0x78 + 48, 0x100, 8);

    // String table at file 0x400 (vaddr 0x1300).
    WriteString(bytes, 0x400 + kMallocEncodedOffset, "gQX+4GDQjpM#s#s");
    WriteString(bytes, 0x400 + kUnknownEncodedOffset, "zzzzzzzzzzA#s#s");

    // Symbol table at file 0x500 (vaddr 0x1400).
    WriteLe(bytes, 0x500 + 24, kMallocEncodedOffset, 4);  // symbol 1: malloc
    WriteLe(bytes, 0x500 + 24 + 4, 0x12, 1);
    WriteLe(bytes, 0x500 + 48, kUnknownEncodedOffset, 4);  // symbol 2: unknown
    WriteLe(bytes, 0x500 + 48 + 4, 0x12, 1);

    // DT_RELA at file 0x600 (vaddr 0x1500): relative, 64, GLOB_DAT.
    WriteLe(bytes, kRelaOffset + 0, fixture.targetRelative, 8);
    WriteLe(bytes, kRelaOffset + 8, 8, 8);  // RELATIVE, sym 0
    WriteLe(bytes, kRelaOffset + 16, fixture.addend, 8);

    WriteLe(bytes, kRelaOffset + 24, fixture.target64, 8);
    WriteLe(bytes, kRelaOffset + 32, (1ull << 32) | 1, 8);  // R_X86_64_64, sym 1
    WriteLe(bytes, kRelaOffset + 40, 0, 8);

    WriteLe(bytes, kRelaOffset + 48, fixture.targetGlobDat, 8);
    WriteLe(bytes, kRelaOffset + 56, (1ull << 32) | 6, 8);  // GLOB_DAT, sym 1
    WriteLe(bytes, kRelaOffset + 64, 0, 8);

    // DT_JMPREL at file 0x700 (vaddr 0x1600): JUMP_SLOT malloc.
    WriteLe(bytes, kJmprelOffset + 0, fixture.targetJumpSlot, 8);
    WriteLe(bytes, kJmprelOffset + 8, (1ull << 32) | 7, 8);  // JUMP_SLOT, sym 1
    WriteLe(bytes, kJmprelOffset + 16, 0, 8);

    // Dynamic tags.
    size_t tag = kDynOffset;
    const auto addTag = [&](uint64_t type, uint64_t value) {
        WriteLe(bytes, tag, type, 8);
        WriteLe(bytes, tag + 8, value, 8);
        tag += 16;
    };
    addTag(5, 0x1300);   // DT_STRTAB
    addTag(10, 0x80);    // DT_STRSZ
    addTag(6, 0x1400);   // DT_SYMTAB
    addTag(11, 24);      // DT_SYMENT
    addTag(0x6100003f, kSymbolCount * 24);
    addTag(7, 0x1500);   // DT_RELA
    addTag(8, 3 * 24);   // DT_RELASZ
    addTag(9, 24);       // DT_RELAENT
    addTag(23, 0x1600);  // DT_JMPREL
    addTag(2, 24);       // DT_PLTRELSZ
    addTag(20, 7);       // DT_PLTREL = RELA
    addTag(0, 0);

    return fixture;
}

uint64_t ReadValue(const std::vector<uint8_t>& bytes, uint64_t vaddr,
                   uint64_t virtualBase) {
    const size_t offset = static_cast<size_t>(vaddr - virtualBase);
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

bool PlanFixture(const Fixture& fixture, AstroPort::ImportBindingPlan& plan,
                 std::string& error) {
    using namespace AstroPort;
    Elf64Metadata metadata;
    if (!ParseElf64Metadata(fixture.bytes.data(), fixture.bytes.size(),
                            metadata)) {
        return false;
    }
    return PlanImportBinding(fixture.bytes.data(), fixture.bytes.size(),
                             metadata, NidResolver::Get(), plan, error);
}

} // namespace

int main() {
    using namespace AstroPort;
    NidResolver::Get().BuildTable();

    // --------------------------------------------------------------
    // Planning and application on a fully resolvable fixture.
    // --------------------------------------------------------------
    {
        Fixture fixture = MakeFixture();
        ImportBindingPlan plan;
        std::string error;
        expect(PlanFixture(fixture, plan, error), "fully resolvable plan builds");
        expect(plan.relaEntries == 3 && plan.jmprelEntries == 1,
               "relocation table sizes are read");
        expect(plan.relativeEntries == 1, "one RELATIVE entry");
        expect(plan.symbolicEntries == 3, "three symbolic entries");
        expect(plan.symbolicByType.size() == 3 &&
                   plan.symbolicByType.at(1) == 1 &&
                   plan.symbolicByType.at(6) == 1 &&
                   plan.symbolicByType.at(7) == 1,
               "symbolic types 1/6/7 counted");
        expect(plan.distinctSymbols == 1 && plan.resolvableSymbols == 1,
               "one distinct resolvable symbol");
        expect(plan.bindableSlots == 3 && plan.unresolvableSlots == 0 &&
                   plan.invalidSlots == 0,
               "three bindable slots");
        expect(plan.complete(), "plan is complete");

        Elf64Metadata metadata;
        ParseElf64Metadata(fixture.bytes.data(), fixture.bytes.size(), metadata);
        Elf64MappedImage image;
        expect(MapElf64LoadSegments(fixture.bytes.data(), fixture.bytes.size(),
                                    metadata, image),
               "fixture image maps");
        Elf64RelocationResult result;
        ImportBindingPlan applyPlan;
        expect(ApplyElf64RelocationsWithImports(
                   fixture.bytes.data(), fixture.bytes.size(), metadata, image,
                   NidResolver::Get(), applyPlan, result, error),
               "relocations apply when the plan is complete");
        expect(result.relativeApplied == 1 && result.symbolicApplied == 3,
               "applied one RELATIVE and three symbolic slots");
        expect(ReadValue(image.bytes, fixture.targetRelative, image.virtualBase) ==
                   image.virtualBase + fixture.addend,
               "RELATIVE slot = base + addend");
        const uint64_t mallocAddress = reinterpret_cast<uint64_t>(
            NidResolver::Get().GetCallableStub(
                NidResolver::ComputeNid("malloc")));
        expect(mallocAddress != 0, "malloc exposes a callable stub");
        expect(ReadValue(image.bytes, fixture.target64, image.virtualBase) ==
                   mallocAddress,
               "R_X86_64_64 slot = callable malloc");
        expect(ReadValue(image.bytes, fixture.targetGlobDat,
                         image.virtualBase) == mallocAddress,
               "GLOB_DAT slot = callable malloc");
        expect(ReadValue(image.bytes, fixture.targetJumpSlot,
                         image.virtualBase) == mallocAddress,
               "JUMP_SLOT slot = callable malloc");
    }

    // --------------------------------------------------------------
    // Unknown symbol: planning reports it, application is atomic.
    // --------------------------------------------------------------
    {
        Fixture fixture = MakeFixture();
        // Extend JMPREL with a slot against symbol 2 (unknown NID).
        WriteLe(fixture.bytes, kJmprelOffset + 24, 0x1720, 8);
        WriteLe(fixture.bytes, kJmprelOffset + 32, (2ull << 32) | 7, 8);
        WriteLe(fixture.bytes, kJmprelOffset + 40, 0, 8);
        // DT_PLTRELSZ now holds two entries (tag 9 at 0x100 + 9*16).
        WriteLe(fixture.bytes, kDynOffset + 9 * 16 + 8, 48, 8);

        ImportBindingPlan plan;
        std::string error;
        expect(PlanFixture(fixture, plan, error), "unknown-symbol plan builds");
        expect(plan.unknownSymbols == 1 && plan.unresolvableSlots == 1,
               "unknown symbol counted");
        expect(!plan.complete(), "plan with unknown symbol is incomplete");

        Elf64Metadata metadata;
        ParseElf64Metadata(fixture.bytes.data(), fixture.bytes.size(), metadata);
        Elf64MappedImage image;
        MapElf64LoadSegments(fixture.bytes.data(), fixture.bytes.size(), metadata,
                             image);
        const std::vector<uint8_t> before = image.bytes;
        Elf64RelocationResult result;
        ImportBindingPlan applyPlan;
        expect(!ApplyElf64RelocationsWithImports(
                   fixture.bytes.data(), fixture.bytes.size(), metadata, image,
                   NidResolver::Get(), applyPlan, result, error),
               "application is refused when a symbol is unknown");
        expect(image.bytes == before, "refused application changed no byte");
        expect(!error.empty(), "refusal carries an error message");
    }

    // --------------------------------------------------------------
    // Structural rejections and invalid slots.
    // --------------------------------------------------------------
    {
        Fixture fixture = MakeFixture();
        // r_offset outside the mapped image.
        WriteLe(fixture.bytes, kRelaOffset + 24, 0x9000, 8);
        ImportBindingPlan plan;
        std::string error;
        expect(PlanFixture(fixture, plan, error), "out-of-bounds plan builds");
        expect(plan.invalidSlots == 1 && !plan.complete(),
               "out-of-bounds target invalidates the plan");
    }
    {
        Fixture fixture = MakeFixture();
        // Unsupported relocation type 0x10 (DTPMOD64) in the first RELA entry.
        WriteLe(fixture.bytes, kRelaOffset + 8, 0x10, 8);
        ImportBindingPlan plan;
        std::string error;
        expect(!PlanFixture(fixture, plan, error),
               "unsupported relocation type is refused");
    }
    {
        Fixture fixture = MakeFixture();
        // DT_PLTREL = REL (17) instead of RELA (7).
        WriteLe(fixture.bytes, kDynOffset + 10 * 16 + 8, 17, 8);
        ImportBindingPlan plan;
        std::string error;
        expect(!PlanFixture(fixture, plan, error),
               "PLT in REL format is refused");
    }
    {
        Fixture fixture = MakeFixture();
        // Remove DT_PLTRELSZ: JMPREL address without size must be refused.
        WriteLe(fixture.bytes, kDynOffset + 9 * 16, 0x61000099, 8);
        ImportBindingPlan plan;
        std::string error;
        expect(!PlanFixture(fixture, plan, error),
               "JMPREL size without JMPREL address is refused");
    }
    {
        Fixture fixture = MakeFixture();
        // Replace the malloc NID with a malformed name.
        WriteString(fixture.bytes, 0x400 + kMallocEncodedOffset, "badname#s#s");
        ImportBindingPlan plan;
        std::string error;
        expect(PlanFixture(fixture, plan, error), "malformed-name plan builds");
        expect(plan.invalidSlots == 3 && !plan.complete(),
               "malformed NID invalidates its slots");
    }

    if (failures != 0) {
        std::cerr << failures << " import binding test(s) failed.\n";
        return 1;
    }
    std::cout << "Import binding validation passed.\n";
    return 0;
}
