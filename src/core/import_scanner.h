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

#include "nid_resolver.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace AstroPort {

struct ImportedSymbol {
    std::string encodedNid;  // 11 base64 characters, without the '#...' suffix
    std::string suffix;      // text after the first '#', kept for diagnostics
    uint64_t nid = 0;        // decoded hash (0 when the name is malformed)
    bool decoded = false;
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
};

struct ImportCoverage {
    uint64_t uniqueNids = 0;
    uint64_t known = 0;        // NID present in the resolver table
    uint64_t implemented = 0;  // NID mapped to a non-null PC stub
    uint64_t missing = 0;
    std::vector<ImportedSymbol> missingSymbols;  // one per unique missing NID
    std::map<std::string, uint64_t> missingBySuffix;

    // Correlation observed between a symbol's "#lib#module" token and the
    // library that the resolver reports for known symbols. It is a diagnostic
    // hint, not a decoded mapping of the token format.
    std::map<std::string, std::map<std::string, uint64_t>> knownBySuffix;
};

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
