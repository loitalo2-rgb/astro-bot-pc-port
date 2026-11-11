#pragma once

#include "../common/types.h"
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace AstroPort {

// ==============================================================================
// EbootLoader inspects SELF/ELF metadata. It does not decrypt SELF payloads,
// resolve game imports, or execute an entry point.
// ==============================================================================

struct LoadedEboot {
    uint8_t* base = nullptr;  // Base de memória mapeada
    size_t   size = 0;        // Tamanho total
    uint64_t entryVaddr = 0;  // Endereço virtual do entry point (ELF e_entry)
    uint64_t codeOffset = 0;  // Known file offset of executable bytes, if resolved
    bool     valid = false;
    bool     isRawElf = false;
    bool     isG3Probe = false;
};

struct Elf64Metadata {
    uint16_t type = 0;
    uint16_t machine = 0;
    uint64_t entryVaddr = 0;
    uint64_t executableSegmentOffset = 0;
    uint64_t executableSegmentVaddr = 0;
    uint64_t executableSegmentFileSize = 0;
    uint64_t executableSegmentMemorySize = 0;
    bool hasExecutableSegment = false;
    struct LoadSegment {
        uint32_t flags = 0;
        uint64_t fileOffset = 0;
        uint64_t virtualAddress = 0;
        uint64_t fileSize = 0;
        uint64_t memorySize = 0;
        uint64_t alignment = 0;
    };
    std::vector<LoadSegment> loadSegments;

    // PT_DYNAMIC: needed to locate the relocation table. Metadata only; the
    // dynamic table is never executed and its entries are validated later.
    bool hasDynamicSegment = false;
    LoadSegment dynamicSegment;
};

struct Elf64MappedImage {
    uint64_t virtualBase = 0;
    std::vector<uint8_t> bytes;
};

struct Elf64RelocationResult {
    size_t relativeApplied = 0;  // R_X86_64_RELATIVE entries applied
};

// -----------------------------------------------------------------------------
// SELF container inspection (metadata only; never decrypts or maps payloads).
// Record-table layout derived from publicly documented SELF/fSELF format
// references and independently re-implemented; no third-party code is copied.
// -----------------------------------------------------------------------------

struct SelfRecordInfo {
    uint64_t flags = 0;          // Record flags; bit 0x2 / 0x8 classification
    uint64_t dataOffset = 0;     // File offset of the record payload
    uint64_t dataSize = 0;       // Payload size
    bool hasSegmentMapping = false;   // flag 0x800: maps to an ELF program header
    uint64_t targetSegmentIndex = 0;  // ELF program header index (flags >> 20)
    double bitsPerByte = 0.0;    // Shannon entropy sample of the payload
    bool markedEncrypted = false;     // flag 0x2
    bool markedCompressed = false;    // flag 0x8
    bool payloadWithinFile = false;   // sample could be taken with bounds checks
};

struct SelfContainerReport {
    bool selfMagicValid = false;      // SELF magic present at offset 0
    bool allDataRecordsHighEntropy = false;  // Every mapped payload looks encrypted
    bool anyMarkedEncrypted = false;
    bool anyMarkedCompressed = false;
    uint64_t headerEntryCount = 0;    // Value read at SELF header offset 0x18
    uint64_t innerElfHeaderOffset = 0;  // Offset of the first inner ELF magic
    std::vector<SelfRecordInfo> records;
};

// Reads the SELF record table (0x40..0x400), classifies payload entropy with a
// 64 KiB sample per record, and flags encryption/compression markers. Pure
// inspection: it does not decrypt, decompress, map or execute anything.
bool InspectSelfContainer(const uint8_t* bytes, size_t size,
                          SelfContainerReport& report);

// -----------------------------------------------------------------------------
// SELF -> ELF64 reconstruction (analysis artifact only).
//
// Rebuilds a clean ELF64 image from an already-decrypted SELF container:
//   - covered program headers (record flag 0x800, index in flags >> 20) have
//     their file data copied from the record's payload location;
//   - file-backed program headers not covered by the table are resolved by
//     the vaddr->file mapping of the containing covered PT_LOAD;
//   - trailing segments without a vaddr following the last covered record are
//     copied back to back after it (as observed in real PS5 containers);
//   - segments with fileSize 0 are emitted without file bytes.
//
// Fail-closed: refuses containers with any encryption/compression marker or a
// mapped payload at/above the entropy threshold, inconsistent record/header
// sizes, uncovered file-backed segments it cannot explain, or any structural
// inconsistency. The output bytes are for analysis only: nothing is executed,
// no entry point is called, and no import is resolved.
// -----------------------------------------------------------------------------

struct Elf64RebuiltInfo {
    std::vector<uint8_t> bytes;
    uint64_t entryVaddr = 0;
    uint16_t type = 0;             // e_type of the rebuilt image
    uint16_t machine = 0;
    uint16_t programHeaders = 0;
    bool typeNormalized = false;   // e_type rewritten to ET_DYN for analysis
    size_t segmentsDataCopied = 0;      // program headers sourced from records
    size_t segmentsProvidedByLoad = 0;  // file-backed headers inside a PT_LOAD
    size_t segmentsTrailingCopied = 0;  // vaddr-less data behind the last record
    size_t segmentsDataless = 0;        // fileSize == 0 (incl. PT_PHDR)
};

bool ReconstructElf64FromSelf(const uint8_t* selfBytes, size_t selfSize,
                              Elf64RebuiltInfo& out);

bool ParseElf64Metadata(const uint8_t* bytes, size_t size,
                        Elf64Metadata& metadata,
                        bool inspectProgramHeaders = true);
bool MapElf64LoadSegments(const uint8_t* bytes, size_t size,
                          const Elf64Metadata& metadata,
                          Elf64MappedImage& image,
                          size_t maximumImageSize = 512u * 1024u * 1024u);

// Applies only the documented R_X86_64_RELATIVE relocation (type 8,
// symbol index 0) from the PT_DYNAMIC/DT_RELA table. Every other relocation
// type or unsupported dynamic entry (DT_JMPREL, DT_TEXTREL, DT_RELR) is an
// explicit error. The image is validated completely before the first write
// (fail-closed), no code is executed and no entry point is called.
bool ApplyElf64RelativeRelocations(const uint8_t* bytes, size_t size,
                                   const Elf64Metadata& metadata,
                                   Elf64MappedImage& image,
                                   Elf64RelocationResult& result);

class EbootLoader {
public:
    static EbootLoader& Get();
    ~EbootLoader();

    bool Load(const std::string& ebootPath);
    bool ApplyPatches();
    bool Launch();           // Refuses execution until the PS5 runtime is implemented

    uint8_t* GetBase() const { return m_loaded.base; }
    size_t   GetSize() const { return m_loaded.size; }
    const LoadedEboot& GetLoadedInfo() const { return m_loaded; }

private:
    EbootLoader() = default;

    LoadedEboot m_loaded;

    bool MapFileIntoMemory(const std::string& path);
    void ReleaseMappedMemory();
    bool ParseSelfHeader();
    bool ParseElfHeaders();
    bool ProcessRelocations();
    bool ResolveImports();

    // Location of the ELF header in the SELF container; not a payload offset.
    static constexpr uint64_t kElfMagicOffset     = 0x1a0;
};

} // namespace AstroPort
