#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace AstroPort {

// Modos de Resolução Nativos identificados na engine
enum class ResolutionMode {
    Res_1080p,  // 1920 x 1080 (2.073.600 pixels)
    Res_2k,     // 2432 x 1368 (3.326.976 pixels)
    Res_4k      // 3840 x 2160 (8.294.400 pixels)
};

struct ResolutionProfile {
    ResolutionMode mode;
    uint32_t width;
    uint32_t height;
    uint32_t pixelCount;
    const char* nativeTag;
    uint64_t dataVaddr;
    uint64_t fileOffset;
};

// Códigos de Retorno SCE
constexpr int32_t SCE_OK = 0;
constexpr int32_t SCE_KERNEL_ERROR_GENERIC = -1;
constexpr int32_t SCE_KERNEL_ERROR_INVAL = -22;
constexpr int32_t SCE_KERNEL_ERROR_NOMEM = -12;

// Botões do Controle DualSense / PlayStation
enum ScePadButton : uint32_t {
    SCE_PAD_L3              = 0x00000002,
    SCE_PAD_R3              = 0x00000004,
    SCE_PAD_OPTIONS         = 0x00000008,
    SCE_PAD_UP              = 0x00000010,
    SCE_PAD_RIGHT           = 0x00000020,
    SCE_PAD_DOWN            = 0x00000040,
    SCE_PAD_LEFT            = 0x00000080,
    SCE_PAD_L2              = 0x00000100,
    SCE_PAD_R2              = 0x00000200,
    SCE_PAD_L1              = 0x00000400,
    SCE_PAD_R1              = 0x00000800,
    SCE_PAD_TRIANGLE        = 0x00001000,
    SCE_PAD_CIRCLE          = 0x00002000,
    SCE_PAD_CROSS           = 0x00004000,
    SCE_PAD_SQUARE          = 0x00008000,
    SCE_PAD_TOUCH_PAD       = 0x00100000,
    SCE_PAD_INTERCEPTED     = 0x80000000
};

// Estrutura de Estado de Entrada do ScePad
struct ScePadData {
    uint32_t buttons;
    uint8_t  leftStickX;
    uint8_t  leftStickY;
    uint8_t  rightStickX;
    uint8_t  rightStickY;
    uint8_t  analogButtonsL2;
    uint8_t  analogButtonsR2;
    uint8_t  padding[2];
    uint32_t connected;
    uint64_t timestamp;
};

} // namespace AstroPort
