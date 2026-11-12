// Import catalog tests: NID byte order, resolver lookup and ELF64 dynamic-symbol
// scanning on a synthetic image. Nothing is executed or resolved into memory.

#include "core/eboot_loader.h"
#include "core/import_scanner.h"
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

struct SyntheticElf {
    std::vector<uint8_t> bytes;
    uint64_t strtabOffset = 0x200;
    uint64_t strtabSize = 64;
    uint64_t symtabOffset = 0x300;
    uint64_t symtabSize = 4 * 24;
};

SyntheticElf MakeElf() {
    SyntheticElf elf;
    elf.bytes.assign(0x500, 0);
    auto& bytes = elf.bytes;

    // ELF64 header.
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;  // ELFCLASS64
    bytes[5] = 1;  // little-endian
    bytes[6] = 1;  // EV_CURRENT
    WriteLe(bytes, 0x10, 3, 2);     // e_type = ET_DYN
    WriteLe(bytes, 0x12, 0x3E, 2);  // e_machine = x86-64
    WriteLe(bytes, 0x14, 1, 4);     // e_version
    WriteLe(bytes, 0x20, 0x40, 8);  // e_phoff
    WriteLe(bytes, 0x34, 64, 2);    // e_ehsize
    WriteLe(bytes, 0x36, 56, 2);    // e_phentsize
    WriteLe(bytes, 0x38, 2, 2);     // e_phnum

    // PT_LOAD: file 0x100, vaddr 0x1000, 0x400 bytes, read-only.
    WriteLe(bytes, 0x40 + 0, 1, 4);
    WriteLe(bytes, 0x40 + 4, 4, 4);
    WriteLe(bytes, 0x40 + 8, 0x100, 8);
    WriteLe(bytes, 0x40 + 16, 0x1000, 8);
    WriteLe(bytes, 0x40 + 32, 0x400, 8);
    WriteLe(bytes, 0x40 + 40, 0x400, 8);
    WriteLe(bytes, 0x40 + 48, 0x100, 8);

    // PT_DYNAMIC: file 0x100, vaddr 0x1000, 0x100 bytes (16 entries).
    WriteLe(bytes, 0x78 + 0, 2, 4);
    WriteLe(bytes, 0x78 + 8, 0x100, 8);
    WriteLe(bytes, 0x78 + 16, 0x1000, 8);
    WriteLe(bytes, 0x78 + 32, 0x100, 8);
    WriteLe(bytes, 0x78 + 40, 0x100, 8);
    WriteLe(bytes, 0x78 + 48, 0x100, 8);

    // String table at file 0x200, vaddr 0x1100.
    const size_t strtab = static_cast<size_t>(elf.strtabOffset);
    WriteString(bytes, strtab, "");
    const uint64_t nidOffset = 1;
    WriteString(bytes, strtab + nidOffset, "gQX+4GDQjpM#s#s");  // malloc
    const uint64_t exportOffset = nidOffset + 16;
    WriteString(bytes, strtab + exportOffset, "exportName");
    const uint64_t badOffset = exportOffset + 11;
    WriteString(bytes, strtab + badOffset, "badname");
    const uint64_t libOffset = badOffset + 8;
    WriteString(bytes, strtab + libOffset, "libTest");
    const uint64_t neededOffset = libOffset + 8;
    WriteString(bytes, strtab + neededOffset, "needed.prx");

    // Symbol table at file 0x300, vaddr 0x1200.
    const size_t symtab = static_cast<size_t>(elf.symtabOffset);
    // Entry 0: null.
    // Entry 1: malloc import (undefined).
    WriteLe(bytes, symtab + 24, nidOffset, 4);
    WriteLe(bytes, symtab + 24 + 4, 0x12, 1);
    WriteLe(bytes, symtab + 24 + 6, 0, 2);
    // Entry 2: export (defined, section 3).
    WriteLe(bytes, symtab + 48, exportOffset, 4);
    WriteLe(bytes, symtab + 48 + 4, 0x12, 1);
    WriteLe(bytes, symtab + 48 + 6, 3, 2);
    WriteLe(bytes, symtab + 48 + 8, 0x1010, 8);
    // Entry 3: malformed import name.
    WriteLe(bytes, symtab + 72, badOffset, 4);
    WriteLe(bytes, symtab + 72 + 4, 0x12, 1);

    // Dynamic tags at file 0x100.
    size_t tag = 0x100;
    const auto addTag = [&](uint64_t type, uint64_t value) {
        WriteLe(bytes, tag, type, 8);
        WriteLe(bytes, tag + 8, value, 8);
        tag += 16;
    };
    addTag(5, 0x1100);   // DT_STRTAB
    addTag(10, elf.strtabSize);
    addTag(6, 0x1200);   // DT_SYMTAB
    addTag(11, 24);      // DT_SYMENT
    addTag(0x6100003f, elf.symtabSize);
    addTag(1, neededOffset);  // DT_NEEDED
    addTag(0x61000049, (1ull << 32) | libOffset);
    addTag(0, 0);        // DT_NULL
    return elf;
}

} // namespace

int main() {
    using namespace AstroPort;

    // --------------------------------------------------------------
    // NID byte order: encoded NIDs observed in the real eboot.
    // --------------------------------------------------------------
    {
        expect(NidResolver::DecodeBase64Nid("gQX+4GDQjpM") ==
                   NidResolver::ComputeNid("malloc"),
               "malloc NID decodes to ComputeNid(malloc)");
        expect(NidResolver::DecodeBase64Nid("rTXw65xmLIA") ==
                   NidResolver::ComputeNid("sceKernelAllocateDirectMemory"),
               "sceKernelAllocateDirectMemory NID matches");
        expect(NidResolver::DecodeBase64Nid("tIhsqj0qsFE") == NidResolver::ComputeNid("free"),
               "free NID matches");
        expect(NidResolver::DecodeBase64Nid("gQX+4GDQjp") == 0,
               "short NID is refused");
        expect(NidResolver::DecodeBase64Nid("!!!!!!!!!!!") == 0,
               "invalid base64 is refused");

        NidResolver::Get().BuildTable();
        expect(NidResolver::Get().Resolve("gQX+4GDQjpM#s#s") ==
                   reinterpret_cast<void*>(::malloc),
               "resolver maps malloc to the native function");
        const NidEntry* entry =
            NidResolver::Get().FindByHash(NidResolver::ComputeNid("malloc"));
        expect(entry != nullptr && entry->stubAddress != nullptr,
               "FindByHash returns the malloc entry");
        expect(NidResolver::Get().ResolveByHash(0x1234567890ABCDEFULL) ==
                   nullptr,
               "unknown hash resolves to null");
    }

    // --------------------------------------------------------------
    // Names recovered from the real eboot by applying the NID algorithm to
    // standard C/C++ symbol names (no external database).
    // --------------------------------------------------------------
    {
        expect(NidResolver::DecodeBase64Nid("zr094EQ39Ww") ==
                   NidResolver::ComputeNid("__cxa_pure_virtual"),
               "__cxa_pure_virtual NID matches the real eboot");
        const NidEntry* pureVirtual =
            NidResolver::Get().FindByHash(NidResolver::ComputeNid(
                "__cxa_pure_virtual"));
        expect(pureVirtual != nullptr && pureVirtual->stubAddress != nullptr,
               "__cxa_pure_virtual has an aborting native stub");
        expect(NidResolver::DecodeBase64Nid("XwLA5cTHjt4") ==
                   NidResolver::ComputeNid("__gxx_personality_v0"),
               "__gxx_personality_v0 NID matches the real eboot");
        const NidEntry* personality = NidResolver::Get().FindByHash(
            NidResolver::ComputeNid("__gxx_personality_v0"));
        expect(personality != nullptr && personality->stubAddress == nullptr,
               "exception personality is known but has no stub");
        expect(NidResolver::DecodeBase64Nid("f7uOxY9mM1U") ==
                   NidResolver::ComputeNid("__stack_chk_guard"),
               "__stack_chk_guard NID matches the real eboot");
    }

    // --------------------------------------------------------------
    // ELF64 import scan.
    // --------------------------------------------------------------
    SyntheticElf elf = MakeElf();
    ImportScanResult scan;
    std::string error;
    expect(ScanElf64Imports(elf.bytes.data(), elf.bytes.size(), scan, error),
           "synthetic ELF scans");
    expect(scan.symbolCount == 4, "four symbols found");
    expect(scan.importCount == 2, "two imports found");
    expect(scan.exportCount == 1, "one export found");
    expect(scan.malformedNames == 1, "one malformed import name");
    expect(scan.uniqueNids == 1, "one unique NID");
    expect(scan.neededModules.size() == 1 &&
               scan.neededModules[0] == "needed.prx",
           "DT_NEEDED is read");
    expect(scan.importLibraries.size() == 1 &&
               scan.importLibraries[0] == "libTest",
           "imported SCE library is read");
    expect(scan.imports.size() == 2 && scan.imports[0].decoded &&
               scan.imports[0].nid == NidResolver::ComputeNid("malloc"),
           "import NID decoded");
    expect(scan.imports[0].suffix == "s#s", "suffix preserved");

    ImportCoverage coverage;
    EvaluateImportCoverage(scan, NidResolver::Get(), coverage);
    expect(coverage.uniqueNids == 1, "coverage counts unique NIDs");
    expect(coverage.known == 1 && coverage.implemented == 1,
           "malloc is known and implemented");
    expect(coverage.missing == 0, "no missing NIDs in the fixture");

    // --------------------------------------------------------------
    // Fail-closed structural rejections.
    // --------------------------------------------------------------
    {
        SyntheticElf bad = MakeElf();
        // DT_SCE_SYMTABSZ not a multiple of 24 (tag index 4 at 0x100+64).
        WriteLe(bad.bytes, 0x140 + 8, 70, 8);
        ImportScanResult result;
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "unaligned symbol table size is refused");

        bad = MakeElf();
        WriteLe(bad.bytes, 0x140, 0x61000099, 8);  // unknown tag replaces size
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "missing DT_SCE_SYMTABSZ is refused");

        bad = MakeElf();
        WriteLe(bad.bytes, 0x110 + 8, 0, 8);  // DT_STRSZ = 0
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "zero string table is refused");

        bad = MakeElf();
        WriteLe(bad.bytes, 0x120 + 8, 0x9000, 8);  // DT_SYMTAB outside PT_LOAD
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "symbol table outside PT_LOAD is refused");

        bad = MakeElf();
        WriteLe(bad.bytes, 0x120 + 8, 0x1500, 8);  // DT_SYMTAB past loaded data
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "symbol table past loaded file data is refused");

        bad = MakeElf();
        for (size_t i = 0; i < 16; ++i) {
            WriteLe(bad.bytes, 0x100 + i * 16, 5, 8);  // never DT_NULL
        }
        expect(!ScanElf64Imports(bad.bytes.data(), bad.bytes.size(), result,
                                 error),
               "dynamic table without DT_NULL is refused");
    }

    if (failures != 0) {
        std::cerr << failures << " import test(s) failed.\n";
        return 1;
    }
    std::cout << "Import catalog validation passed.\n";
    return 0;
}
