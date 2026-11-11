#include "assets/gnf_format.h"

namespace AstroPort {
namespace {

constexpr uint32_t vkBC1RgbaSrgb = 134;
constexpr uint32_t vkBC4Unorm = 139;
constexpr uint32_t vkBC7Unorm = 145;
constexpr uint32_t vkBC7Srgb = 146;

} // namespace

GnfFormatInfo LookupGnfFormat(uint32_t formatId) {
    switch (formatId) {
        case 0xCAF0u:
        case 0x8AF0u:
            return {true, "BC4_UNORM (GNF PS5)", vkBC4Unorm, 8u, true};
        case 0xCB10u:
        case 0xCB50u:
            return {true, "BC7_UNORM (GNF PS5)", vkBC7Unorm, 16u, true};
        case 0xCB30u:
        case 0xCB60u:
        case 0x8B60u:
            return {true, "BC7_SRGB (GNF PS5)", vkBC7Srgb, 16u, true};
        case 0xCAA0u:
            return {true, "BC1_SRGB (GNF PS5)", vkBC1RgbaSrgb, 8u, true};
        default:
            return {};
    }
}

} // namespace AstroPort
