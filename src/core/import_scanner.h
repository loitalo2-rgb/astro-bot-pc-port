#pragma once

// Offline import catalog for a rebuilt PS5 ELF64 image.
//
// Reads only metadata: PT_DYNAMIC, the dynamic symbol table, the string table
// and the SCE tags that describe the symbol-table size and imported libraries.
// Nothing is relocated, resolved into memory or executed. The goal is to answer
// "which NIDs does this module need, and how many of them does the port already
// know?" before any runtime work is attempted.
//
// PS5 symbol names have the form "NID#lib#module" (11 base64 characters before
// the first '#'). Undefined symbols (st_shndx == 0) are imports; defined ones
// are exports.
//
// The two characters after the NID are base64 digits, not free text:
//   * the first digit indexes DT_SCE_IMPORT_LIB (the import library);
//   * the second digit is the DT_NEEDED index plus one (slot 0 is the
//     importing module itself), i.e. the module that provides the library.
// This was validated against the real eboot.bin: 1.747 of 1.747 imports carry
// valid in-range indices, and 1.709 resolve to a module with the expected name
// (the remaining 38 are libraries that share a module, e.g. libScePosix →
// libkernel). Indices outside the tables are rejected (fail-closed).

#include "nid_resolver.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace AstroPort {

struct ImportSuffixInfo {
    bool decoded = false;
    uint32_t libraryIndex = 0;  // index into importLibraries
    uint32_t moduleIndex = 0;   // 0 = importing module, otherwise NEEDED index+1
    std::string libraryName;    // importLibraries[libraryIndex]
    std::string moduleName;     // neededModules[moduleIndex-1], or "<self>"
};

struct ImportedSymbol {
    std::string encodedNid;  // 11 base64 characters, without the '#...' suffix
    std::string suffix;      // text after the first '#', kept for diagnostics
    uint64_t nid = 0;        // decoded hash (0 when the name is malformed)
    bool decoded = false;
    bool suffixDecoded = false;
    uint32_t libraryIndex = 0;
    uint32_t moduleIndex = 0;
    std::string libraryName;  // from the suffix, empty when not decodable
    std::string moduleName;   // from the suffix, empty when not decodable
};

struct ImportScanResult {
    bool isElf64 = false;
    uint64_t symtabVaddr = 0;
    uint64_t strtabVaddr = 0;
    uint64_t symtabBytes = 0;
    uint64_t strtabBytes = 0;
    uint64_t symbolCount = 0;
    uint64_t importCount = 0;
    uint64_t exportCount = 0;
    uint64_t malformedNames = 0;
    uint64_t uniqueNids = 0;

    std::vector<std::string> neededModules;    // DT_NEEDED strings
    std::vector<std::string> importLibraries;  // DT_SCE_IMPORT_LIB names
    std::vector<ImportedSymbol> imports;       // one entry per imported symbol
    uint64_t suffixDecodedCount = 0;
    uint64_t suffixUndecodedCount = 0;
};

struct ImportCoverage {
    uint64_t uniqueNids = 0;
    uint64_t known = 0;        // NID present in the resolver table
    uint64_t implemented = 0;  // NID mapped to a non-null PC stub
    uint64_t missing = 0;
    std::vector<ImportedSymbol> missingSymbols;  // one per unique missing NID
    std::map<std::string, uint64_t> missingBySuffix;  // raw "#lib#module" token
    std::map<std::string, uint64_t> missingByLibrary;  // decoded library name
    std::map<std::string, uint64_t> missingByModule;   // decoded module name

    // Correlation observed between a symbol's "#lib#module" token and the
    // library that the resolver reports for known symbols. With the suffix
    // decoded, missingByLibrary is the authoritative grouping; this stays as a
    // cross-check for known symbols.
    std::map<std::string, std::map<std::string, uint64_t>> knownBySuffix;
    std::map<std::string, uint64_t> knownByLibrary;
};

// Decodes the two base64 digits after the NID ("#lib#module"). The first digit
// indexes importLibraries; the second is the DT_NEEDED index plus one (slot 0
// is the importing module). Returns false, leaving out untouched, when the
// token shape is not exact or either index is out of range (fail-closed).
bool DecodeImportSuffix(const std::string& suffix,
                        const std::vector<std::string>& importLibraries,
                        const std::vector<std::string>& neededModules,
                        ImportSuffixInfo& out);

// Scans a rebuilt/raw ELF64 image. Fails closed on structural inconsistencies
// (missing dynamic tags, tables outside a PT_LOAD, symbol entries out of range,
// symbol table size not a multiple of 24). Individual malformed symbol names are
// counted, not fatal: they cannot be resolved and are reported as such.
bool ScanElf64Imports(const uint8_t* bytes, size_t size,
                      ImportScanResult& result, std::string& error);

// Compares the scanned NIDs against the resolver table. Does not mutate either.
void EvaluateImportCoverage(const ImportScanResult& scan,
                            const NidResolver& resolver,
                            ImportCoverage& coverage);

} // namespace AstroPort
