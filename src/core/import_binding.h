#pragma once

// Import-binding planner for a rebuilt/raw PS5 ELF64 image.
//
// The loader already handles R_X86_64_RELATIVE. This module answers the next
// question: of the symbolic relocations (R_X86_64_64, GLOB_DAT, JUMP_SLOT) the
// image requires, how many can actually be bound to a native stub today?
//
// Nothing is executed. Planning only reads PT_DYNAMIC/DT_RELA/DT_JMPREL, the
// dynamic symbol table and the resolver table. Applying is atomic: every slot
// is validated first, and if a single symbolic slot cannot be resolved to a
// non-null stub, no byte of the image is modified (fail-closed).

#include "eboot_loader.h"
#include "nid_resolver.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace AstroPort {

struct ImportBindingSymbol {
    std::string encodedNid;
    std::string suffix;
    std::string libraryName;   // from the resolver, when known
    std::string symbolName;    // from the resolver, when known
    uint64_t nid = 0;
    uint64_t slots = 0;
    bool known = false;
    bool hasStub = false;
};

struct ImportBindingPlan {
    uint64_t relaEntries = 0;
    uint64_t jmprelEntries = 0;
    uint64_t relativeEntries = 0;
    uint64_t symbolicEntries = 0;
    std::map<uint32_t, uint64_t> symbolicByType;

    uint64_t distinctSymbols = 0;
    uint64_t resolvableSymbols = 0;  // known and backed by a native stub
    uint64_t knownWithoutStub = 0;
    uint64_t unknownSymbols = 0;

    uint64_t bindableSlots = 0;    // symbolic slots whose symbol has a stub
    uint64_t unresolvableSlots = 0;
    uint64_t invalidSlots = 0;     // r_offset/symbol index outside the image

    std::vector<ImportBindingSymbol> unresolvedSymbols;

    // True only when every relocation is valid and every symbolic slot has a
    // native stub. Only then may the image be considered fully bindable.
    bool complete() const {
        return invalidSlots == 0 && unresolvableSlots == 0;
    }
};

// Reads the relocation tables and correlates each symbolic relocation with the
// resolver table. Fails closed on structural inconsistencies (missing tables,
// JMPREL not in RELA format, symbols outside the loaded image, symbol index out
// of range at table level).
bool PlanImportBinding(const uint8_t* bytes, size_t size,
                       const Elf64Metadata& metadata,
                       const NidResolver& resolver, ImportBindingPlan& plan,
                       std::string& error);

// Plans, then applies R_X86_64_RELATIVE and the symbolic relocations in one
// pass. Returns false without modifying the image when the plan is incomplete.
bool ApplyElf64RelocationsWithImports(const uint8_t* bytes, size_t size,
                                      const Elf64Metadata& metadata,
                                      Elf64MappedImage& image,
                                      const NidResolver& resolver,
                                      ImportBindingPlan& plan,
                                      Elf64RelocationResult& result,
                                      std::string& error);

} // namespace AstroPort
