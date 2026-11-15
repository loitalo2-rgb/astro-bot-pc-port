#pragma once

#include <cstdint>

namespace AstroPort {

// GNM surface formats seen in the PPSA21567 dump, mapped to Vulkan. The table
// is centralized so the loader and the offline census cannot disagree.
//
// Evidence for the mapping (offline census, 2026-11-11):
//   0xCAF0 / 0x8AF0 -> BC4  (files named *_ao, *_r, *_i; single channel)
//   0xCB10 / 0xCB50 -> BC7 UNORM (files named *_n and *_mask; linear data)
//   0xCB30 / 0xCB60 / 0x8B60 -> BC7 sRGB (files named *_d, diffuse/color)
//   0xCAA0 -> BC1 sRGB (file name explicitly says *_bc1s)
//
// Anything not in this table is refused by the loader instead of being guessed:
// wrong block sizes decode as garbage, and a silent default hides real format
// gaps.
struct GnfFormatInfo {
    bool known = false;
    const char* name = "desconhecido";
    uint32_t vulkanFormat = 0;
    uint32_t blockBytes = 0;
    bool blockCompressed = true;
};

GnfFormatInfo LookupGnfFormat(uint32_t formatId);

} // namespace AstroPort
