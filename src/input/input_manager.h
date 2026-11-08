#pragma once

#include "../common/types.h"
#include <cstdint>
#include <array>
#include <string>

namespace AstroPort {

// ============================================================
// Mapeamento Xbox <-> PlayStation (todos os botões)
// ============================================================

// Botões do controle Xbox (XInput WORD bitmask)
enum class XboxButton : uint16_t {
    DPad_Up        = 0x0001,
    DPad_Down      = 0x0002,
    DPad_Left      = 0x0004,
    DPad_Right     = 0x0008,
    Start          = 0x0010,  // Menu / Options
    Back           = 0x0020,  // View / Share / Create
    LS             = 0x0040,  // Left Stick Click = L3
    RS             = 0x0080,  // Right Stick Click = R3
    LB             = 0x0100,  // Left Bumper = L1
    RB             = 0x0200,  // Right Bumper = R1
    A              = 0x1000,  // A = Cruz
    B              = 0x2000,  // B = Círculo
    X              = 0x4000,  // X = Quadrado
    Y              = 0x8000   // Y = Triângulo
    // LT e RT são analógicos (bLeftTrigger / bRightTrigger)
};

// Mapeamento canônico Xbox → PlayStation
struct ButtonMapping {
    XboxButton xboxBtn;
    ScePadButton sceBtn;
    const char* xboxName;
    const char* sceName;
};

static constexpr ButtonMapping kButtonMap[] = {
    { XboxButton::A,         SCE_PAD_CROSS,     "A",      "Cruz (X)"     },
    { XboxButton::B,         SCE_PAD_CIRCLE,    "B",      "Círculo (O)"  },
    { XboxButton::X,         SCE_PAD_SQUARE,    "X",      "Quadrado ([])" },
    { XboxButton::Y,         SCE_PAD_TRIANGLE,  "Y",      "Triângulo (△)" },
    { XboxButton::LB,        SCE_PAD_L1,        "LB",     "L1"           },
    { XboxButton::RB,        SCE_PAD_R1,        "RB",     "R1"           },
    { XboxButton::LS,        SCE_PAD_L3,        "LS",     "L3"           },
    { XboxButton::RS,        SCE_PAD_R3,        "RS",     "R3"           },
    { XboxButton::Start,     SCE_PAD_OPTIONS,   "Menu",   "Options"      },
    { XboxButton::Back,      SCE_PAD_TOUCH_PAD, "View",   "Create/Share" },
    { XboxButton::DPad_Up,   SCE_PAD_UP,        "D↑",     "D-Pad Cima"   },
    { XboxButton::DPad_Down, SCE_PAD_DOWN,      "D↓",     "D-Pad Baixo"  },
    { XboxButton::DPad_Left, SCE_PAD_LEFT,      "D←",     "D-Pad Esq"    },
    { XboxButton::DPad_Right,SCE_PAD_RIGHT,     "D→",     "D-Pad Dir"    },
};

static constexpr size_t kButtonMapCount = sizeof(kButtonMap) / sizeof(kButtonMap[0]);

// Estado completo de um controle (suporta até 4 controles Xbox)
struct ControllerState {
    bool     connected      = false;
    uint32_t playerIndex    = 0;
    std::string deviceName  = "Desconectado";

    // Botões digitais
    uint32_t buttons        = 0;
    uint32_t prevButtons    = 0;   // Frame anterior (detecção de pressionamento)

    // Gatilhos analógicos (0.0 = solto, 1.0 = totalmente pressionado)
    float    lt             = 0.0f;  // LT = L2
    float    rt             = 0.0f;  // RT = R2

    // Sticks analógicos (−1.0 a +1.0, com dead zone já aplicada)
    float    lx             = 0.0f;  // Stick Esquerdo X
    float    ly             = 0.0f;  // Stick Esquerdo Y (positivo = cima)
    float    rx             = 0.0f;  // Stick Direito X
    float    ry             = 0.0f;  // Stick Direito Y (positivo = cima)

    // Rumble atual
    float    rumbleLarge    = 0.0f;  // Motor de baixa frequência (L)
    float    rumbleSmall    = 0.0f;  // Motor de alta frequência (R)

    // Helpers de estado por frame
    bool IsPressed(ScePadButton btn)    const { return (buttons & btn) != 0; }
    bool JustPressed(ScePadButton btn)  const { return (buttons & btn) && !(prevButtons & btn); }
    bool JustReleased(ScePadButton btn) const { return !(buttons & btn) && (prevButtons & btn); }
};

// Configurações de sensibilidade e dead zones
struct InputConfig {
    float stickDeadZoneInner  = 0.12f;  // Dead zone interna (anel morto ao centro)
    float stickDeadZoneOuter  = 0.95f;  // Dead zone externa (satura a 1.0)
    float triggerThreshold    = 0.15f;  // Threshold digital para L2/R2
    bool  invertLeftY         = false;  // Inverte stick esquerdo vertical
    bool  invertRightY        = false;  // Inverte câmera vertical
    bool  swapSticks          = false;  // Troca stick esquerdo/direito (config alternativa)
    uint32_t rumbleIntensity  = 100;    // Intensidade do vibração (0-100%)
};

class InputManager {
public:
    static InputManager& Get();

    bool Initialize(bool enableDualSense, bool enableXInput);
    void Shutdown();
    void Update();

    // Lê estado do SCE ScePad (chamado pela emulação de libScePad.prx)
    bool ReadPadData(uint32_t handle, ScePadData* outData);

    // Controle de vibração
    bool SetVibration(uint32_t handle, uint8_t largeMotor, uint8_t smallMotor);
    bool SetVibrationF(uint32_t controllerIndex, float large, float small);
    void StopAllVibration();

    // Acesso ao estado completo (para debug / UI)
    const ControllerState& GetController(uint32_t index = 0) const;
    uint32_t GetConnectedCount() const;

    // Configuração
    void SetInputConfig(const InputConfig& cfg) { m_config = cfg; }
    const InputConfig& GetInputConfig() const   { return m_config; }

    // Debug: imprime o estado atual no console
    void PrintState(uint32_t index = 0) const;

private:
    InputManager() = default;

    static constexpr uint32_t kMaxControllers = 4;

    bool m_initialized     = false;
    bool m_dualSenseActive = false;
    bool m_xinputActive    = false;

    InputConfig m_config;
    std::array<ControllerState, kMaxControllers> m_controllers;

    // ScePadData derivada do ControllerState primário (controller 0)
    ScePadData m_currentPadState{};

    // Processamento de dead zone circular (mais preciso que dead zone quadrada)
    static float ApplyDeadZone(float value, float inner, float outer);
    static void  ApplyCircularDeadZone(float& x, float& y, float inner, float outer);

    // Conversão float stick → byte PS5
    static uint8_t FloatToStickByte(float v);

    // Poll XInput
    void PollXInput();

    // Mapeamento de nomes para debug
    static const char* GetButtonName(ScePadButton btn);
};

} // namespace AstroPort
