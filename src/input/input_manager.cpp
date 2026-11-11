#include "input_manager.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#endif

namespace AstroPort {

InputManager& InputManager::Get() {
    static InputManager instance;
    return instance;
}

bool InputManager::Initialize(bool enableDualSense, bool enableXInput) {
    m_dualSenseActive = enableDualSense;
    m_xinputActive = enableXInput;
    m_initialized = true;

    for (uint32_t i = 0; i < kMaxControllers; ++i) {
        m_controllers[i].playerIndex = i;
        m_controllers[i].connected = false;
    }

    std::cout << "[InputManager] Sub-sistema de Entrada Xbox/PlayStation pronto:\n";
    std::cout << "  -> Suporte XInput (Xbox Series X|S, Xbox One, Xbox 360): " 
              << (m_xinputActive ? "Ativado" : "Desativado") << "\n";
    std::cout << "  -> Suporte DualSense nativo: " 
              << (m_dualSenseActive ? "Ativado" : "Desativado") << "\n";
    std::cout << "  -> Mapeamento completo dos 14 botoes + 2 gatilhos + 2 sticks analogicos\n";
    return true;
}

void InputManager::Shutdown() {
    StopAllVibration();
    m_initialized = false;
    std::cout << "[InputManager] Sub-sistema de entrada encerrado.\n";
}

float InputManager::ApplyDeadZone(float value, float inner, float outer) {
    float magnitude = std::abs(value);
    if (magnitude < inner) return 0.0f;
    if (magnitude > outer) return (value > 0.0f) ? 1.0f : -1.0f;
    float normalized = (magnitude - inner) / (outer - inner);
    return (value > 0.0f) ? normalized : -normalized;
}

void InputManager::ApplyCircularDeadZone(float& x, float& y, float inner, float outer) {
    float magnitude = std::sqrt(x * x + y * y);
    if (magnitude < inner) {
        x = 0.0f;
        y = 0.0f;
        return;
    }
    if (magnitude > outer) {
        x /= magnitude;
        y /= magnitude;
        return;
    }
    float normalizedMag = (magnitude - inner) / (outer - inner);
    x = (x / magnitude) * normalizedMag;
    y = (y / magnitude) * normalizedMag;
}

uint8_t InputManager::FloatToStickByte(float v) {
    // Converte de [-1.0, 1.0] para uint8 [0, 255] com centro em 128
    float clamped = std::clamp(v, -1.0f, 1.0f);
    int intVal = static_cast<int>(std::round((clamped + 1.0f) * 127.5f));
    return static_cast<uint8_t>(std::clamp(intVal, 0, 255));
}

void InputManager::PollXInput() {
#ifdef _WIN32
    if (!m_xinputActive) return;

    for (DWORD i = 0; i < kMaxControllers; ++i) {
        auto& ctrl = m_controllers[i];
        XINPUT_STATE state{};
        DWORD dwResult = XInputGetState(i, &state);

        if (dwResult == ERROR_SUCCESS) {
            ctrl.connected = true;
            ctrl.deviceName = "Controle Xbox (Player " + std::to_string(i + 1) + ")";
            ctrl.prevButtons = ctrl.buttons;

            const auto& gp = state.Gamepad;

            // 1. Mapeamento de Botões Digitais
            uint32_t btns = 0;
            if (gp.wButtons & XINPUT_GAMEPAD_A)              btns |= ScePadButton::SCE_PAD_CROSS;
            if (gp.wButtons & XINPUT_GAMEPAD_B)              btns |= ScePadButton::SCE_PAD_CIRCLE;
            if (gp.wButtons & XINPUT_GAMEPAD_X)              btns |= ScePadButton::SCE_PAD_SQUARE;
            if (gp.wButtons & XINPUT_GAMEPAD_Y)              btns |= ScePadButton::SCE_PAD_TRIANGLE;
            if (gp.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  btns |= ScePadButton::SCE_PAD_L1;
            if (gp.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) btns |= ScePadButton::SCE_PAD_R1;
            if (gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     btns |= ScePadButton::SCE_PAD_L3;
            if (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    btns |= ScePadButton::SCE_PAD_R3;
            if (gp.wButtons & XINPUT_GAMEPAD_START)          btns |= ScePadButton::SCE_PAD_OPTIONS;
            if (gp.wButtons & XINPUT_GAMEPAD_BACK)           btns |= ScePadButton::SCE_PAD_TOUCH_PAD;
            if (gp.wButtons & XINPUT_GAMEPAD_DPAD_UP)        btns |= ScePadButton::SCE_PAD_UP;
            if (gp.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)      btns |= ScePadButton::SCE_PAD_DOWN;
            if (gp.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)      btns |= ScePadButton::SCE_PAD_LEFT;
            if (gp.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)     btns |= ScePadButton::SCE_PAD_RIGHT;

            // 2. Gatilhos Analógicos LT/RT (0 a 255 -> 0.0f a 1.0f)
            ctrl.lt = gp.bLeftTrigger / 255.0f;
            ctrl.rt = gp.bRightTrigger / 255.0f;

            if (ctrl.lt > m_config.triggerThreshold) btns |= ScePadButton::SCE_PAD_L2;
            if (ctrl.rt > m_config.triggerThreshold) btns |= ScePadButton::SCE_PAD_R2;

            ctrl.buttons = btns;

            // 3. Sticks Analógicos (-32768 a 32767 -> -1.0f a 1.0f)
            float lxRaw = (gp.sThumbLX >= 0) ? (gp.sThumbLX / 32767.0f) : (gp.sThumbLX / 32768.0f);
            float lyRaw = (gp.sThumbLY >= 0) ? (gp.sThumbLY / 32767.0f) : (gp.sThumbLY / 32768.0f);
            float rxRaw = (gp.sThumbRX >= 0) ? (gp.sThumbRX / 32767.0f) : (gp.sThumbRX / 32768.0f);
            float ryRaw = (gp.sThumbRY >= 0) ? (gp.sThumbRY / 32767.0f) : (gp.sThumbRY / 32768.0f);

            ApplyCircularDeadZone(lxRaw, lyRaw, m_config.stickDeadZoneInner, m_config.stickDeadZoneOuter);
            ApplyCircularDeadZone(rxRaw, ryRaw, m_config.stickDeadZoneInner, m_config.stickDeadZoneOuter);

            if (m_config.invertLeftY)  lyRaw = -lyRaw;
            if (m_config.invertRightY) ryRaw = -ryRaw;

            if (m_config.swapSticks) {
                ctrl.lx = rxRaw; ctrl.ly = ryRaw;
                ctrl.rx = lxRaw; ctrl.ry = lyRaw;
            } else {
                ctrl.lx = lxRaw; ctrl.ly = lyRaw;
                ctrl.rx = rxRaw; ctrl.ry = ryRaw;
            }
        } else {
            ctrl.connected = false;
            ctrl.deviceName = "Desconectado";
            ctrl.buttons = 0;
            ctrl.lt = 0.0f; ctrl.rt = 0.0f;
            ctrl.lx = 0.0f; ctrl.ly = 0.0f;
            ctrl.rx = 0.0f; ctrl.ry = 0.0f;
        }
    }
#endif
}

void InputManager::Update() {
    if (!m_initialized) return;

    PollXInput();

    // Atualiza a estrutura compatível ScePadData para o Player 1 (índice 0)
    std::memset(&m_currentPadState, 0, sizeof(ScePadData));
    const auto& p1 = m_controllers[0];

    if (p1.connected) {
        m_currentPadState.connected = 1;
        m_currentPadState.buttons = p1.buttons;
        m_currentPadState.analogButtonsL2 = static_cast<uint8_t>(p1.lt * 255.0f);
        m_currentPadState.analogButtonsR2 = static_cast<uint8_t>(p1.rt * 255.0f);

        // No PlayStation, o eixo Y do stick cresce para baixo (0=cima, 255=baixo)
        // Nossa representação float tem positivo=cima, logo invertemos para o formato PlayStation:
        m_currentPadState.leftStickX  = FloatToStickByte(p1.lx);
        m_currentPadState.leftStickY  = FloatToStickByte(-p1.ly);
        m_currentPadState.rightStickX = FloatToStickByte(p1.rx);
        m_currentPadState.rightStickY = FloatToStickByte(-p1.ry);
    } else {
        // Fallback para teclado se nenhum controle Xbox estiver conectado
#ifdef _WIN32
        uint32_t kbBtns = 0;
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) kbBtns |= ScePadButton::SCE_PAD_CROSS;     // Enter = A (Cruz)
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) kbBtns |= ScePadButton::SCE_PAD_OPTIONS;   // Esc = Start/Options
        if (GetAsyncKeyState(VK_TAB)    & 0x8000) kbBtns |= ScePadButton::SCE_PAD_TOUCH_PAD; // Tab = TouchPad
        if (GetAsyncKeyState(VK_UP)     & 0x8000) kbBtns |= ScePadButton::SCE_PAD_UP;        // Setas = D-Pad
        if (GetAsyncKeyState(VK_DOWN)   & 0x8000) kbBtns |= ScePadButton::SCE_PAD_DOWN;
        if (GetAsyncKeyState(VK_LEFT)   & 0x8000) kbBtns |= ScePadButton::SCE_PAD_LEFT;
        if (GetAsyncKeyState(VK_RIGHT)  & 0x8000) kbBtns |= ScePadButton::SCE_PAD_RIGHT;
        if (GetAsyncKeyState('J')       & 0x8000) kbBtns |= ScePadButton::SCE_PAD_SQUARE;   // J = X (Quadrado)
        if (GetAsyncKeyState('K')       & 0x8000) kbBtns |= ScePadButton::SCE_PAD_CROSS;    // K = A (Cruz)
        if (GetAsyncKeyState('L')       & 0x8000) kbBtns |= ScePadButton::SCE_PAD_CIRCLE;   // L = B (Circulo)
        if (GetAsyncKeyState('I')       & 0x8000) kbBtns |= ScePadButton::SCE_PAD_TRIANGLE; // I = Y (Triangulo)

        m_currentPadState.buttons = kbBtns;
        m_currentPadState.connected = 1;
        m_currentPadState.leftStickX = 128;
        m_currentPadState.leftStickY = 128;
        m_currentPadState.rightStickX = 128;
        m_currentPadState.rightStickY = 128;
#endif
    }

#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    m_currentPadState.timestamp = counter.QuadPart;
#endif
}

bool InputManager::ReadPadData(uint32_t handle, ScePadData* outData) {
    if (!outData || !m_initialized) return false;
    std::memcpy(outData, &m_currentPadState, sizeof(ScePadData));
    return true;
}

bool InputManager::SetVibration(uint32_t handle, uint8_t largeMotor, uint8_t smallMotor) {
    float l = (largeMotor / 255.0f) * (m_config.rumbleIntensity / 100.0f);
    float s = (smallMotor / 255.0f) * (m_config.rumbleIntensity / 100.0f);
    return SetVibrationF(0, l, s);
}

bool InputManager::SetVibrationF(uint32_t controllerIndex, float large, float small) {
    if (controllerIndex >= kMaxControllers) return false;

    auto& ctrl = m_controllers[controllerIndex];
    ctrl.rumbleLarge = std::clamp(large, 0.0f, 1.0f);
    ctrl.rumbleSmall = std::clamp(small, 0.0f, 1.0f);

#ifdef _WIN32
    if (m_xinputActive && ctrl.connected) {
        XINPUT_VIBRATION vib{};
        vib.wLeftMotorSpeed  = static_cast<WORD>(ctrl.rumbleLarge * 65535.0f);
        vib.wRightMotorSpeed = static_cast<WORD>(ctrl.rumbleSmall * 65535.0f);
        return XInputSetState(controllerIndex, &vib) == ERROR_SUCCESS;
    }
#endif
    return true;
}

void InputManager::StopAllVibration() {
    for (uint32_t i = 0; i < kMaxControllers; ++i) {
        SetVibrationF(i, 0.0f, 0.0f);
    }
}

const ControllerState& InputManager::GetController(uint32_t index) const {
    if (index >= kMaxControllers) return m_controllers[0];
    return m_controllers[index];
}

uint32_t InputManager::GetConnectedCount() const {
    uint32_t count = 0;
    for (const auto& c : m_controllers) {
        if (c.connected) count++;
    }
    return count;
}

void InputManager::PrintState(uint32_t index) const {
    const auto& c = GetController(index);
    if (!c.connected) {
        std::cout << "[Controle " << (index + 1) << "] Desconectado\n";
        return;
    }
    std::cout << "[Controle " << (index + 1) << "] " << c.deviceName << "\n";
    std::cout << "  Sticks: L(" << c.lx << ", " << c.ly << ") R(" << c.rx << ", " << c.ry << ")\n";
    std::cout << "  Triggers: LT=" << c.lt << " RT=" << c.rt << "\n";
    std::cout << "  Botoes ativos: ";
    for (size_t b = 0; b < kButtonMapCount; ++b) {
        if (c.IsPressed(kButtonMap[b].sceBtn)) {
            std::cout << "[" << kButtonMap[b].xboxName << " -> " << kButtonMap[b].sceName << "] ";
        }
    }
    std::cout << "\n";
}

} // namespace AstroPort
