#include "core/abi_bridge.h"

#include <cstring>
#include <initializer_list>
#include <mutex>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

namespace {

constexpr size_t kThunkSize = 69;
constexpr size_t kThunkAlignment = 16;
constexpr size_t kPageSize = 64 * 1024;

// Bytes de um thunk SysV -> MS x64, com o alvo em um imediato de 64 bits.
//
// Entrada (chamador SysV): [rsp] = retorno, RDI/RSI/RDX/RCX/R8/R9 = args 1..6.
// 1. sub rsp, 0x48        -> abre quadro; rsp passa a 0 mod 16 (pronto para call)
// 2. guarda o retorno original em [rsp+0x38]
// 3. move R8/R9 (args 5/6 no SysV) para [rsp+0x20]/[rsp+0x28]
//    (o alvo os vera em [entry+0x28]/[entry+0x30], com entry = rsp-8)
// 4. R10 = RDX (arg3), R11 = RCX (arg4); RCX = RDI (arg1), RDX = RSI (arg2);
//    R8 = R10 (arg3), R9 = R11 (arg4)
// 5. call alvo
// 6. restaura o retorno original, fecha o quadro e ret
std::vector<uint8_t> BuildThunk(void* target) {
    std::vector<uint8_t> code;
    const auto emit = [&](std::initializer_list<uint8_t> bytes) {
        code.insert(code.end(), bytes.begin(), bytes.end());
    };

    emit({0x48, 0x83, 0xEC, 0x48});              // sub rsp, 0x48
    emit({0x4C, 0x8B, 0x54, 0x24, 0x48});        // mov r10, [rsp+0x48]
    emit({0x4C, 0x89, 0x54, 0x24, 0x38});        // mov [rsp+0x38], r10

    emit({0x4C, 0x89, 0x44, 0x24, 0x20});        // mov [rsp+0x20], r8
    emit({0x4C, 0x89, 0x4C, 0x24, 0x28});        // mov [rsp+0x28], r9

    emit({0x49, 0x89, 0xD2});                    // mov r10, rdx
    emit({0x49, 0x89, 0xCB});                    // mov r11, rcx
    emit({0x48, 0x89, 0xF9});                    // mov rcx, rdi
    emit({0x48, 0x89, 0xF2});                    // mov rdx, rsi
    emit({0x4D, 0x89, 0xD0});                    // mov r8, r10
    emit({0x4D, 0x89, 0xD9});                    // mov r9, r11

    emit({0x48, 0xB8});                          // mov rax, imm64
    const uint64_t address = reinterpret_cast<uint64_t>(target);
    for (int i = 0; i < 8; ++i) {
        code.push_back(static_cast<uint8_t>((address >> (8 * i)) & 0xFF));
    }
    emit({0xFF, 0xD0});                          // call rax

    emit({0x4C, 0x8B, 0x54, 0x24, 0x38});        // mov r10, [rsp+0x38]
    emit({0x4C, 0x89, 0x54, 0x24, 0x48});        // mov [rsp+0x48], r10
    emit({0x48, 0x83, 0xC4, 0x48});              // add rsp, 0x48
    emit({0xC3});                                // ret
    return code;
}

std::mutex g_mutex;

} // namespace

AbiBridge& AbiBridge::Get() {
    static AbiBridge instance;
    return instance;
}

AbiBridge::~AbiBridge() {
    Shutdown();
}

size_t AbiBridge::GetThunkSize() {
    return kThunkSize;
}

void* AbiBridge::CreateIntegerThunk(void* target) {
    if (!target) return nullptr;
#if !defined(_WIN32)
    // Em plataformas SysV a ABI do convidado e a nativa: nada a traduzir.
    (void)g_mutex;
    return target;
#else
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto existing = m_thunks.find(target);
    if (existing != m_thunks.end()) return existing->second;

    const std::vector<uint8_t> code = BuildThunk(target);
    if (code.size() > kThunkSize) return nullptr;  // nunca publicar truncado
    void* thunk = AllocateCode(code.size());
    if (!thunk) return nullptr;
    std::memcpy(thunk, code.data(), code.size());
    FlushInstructionCache(GetCurrentProcess(), thunk, code.size());
    m_thunks.emplace(target, thunk);
    return thunk;
#endif
}

void* AbiBridge::AllocateCode(size_t bytes) {
#ifdef _WIN32
    const size_t aligned = (bytes + kThunkAlignment - 1) / kThunkAlignment *
                           kThunkAlignment;
    if (!m_page || m_pageUsed + aligned > m_pageSize) {
        void* page = VirtualAlloc(nullptr, kPageSize, MEM_COMMIT | MEM_RESERVE,
                                  PAGE_EXECUTE_READWRITE);
        if (!page) return nullptr;
        m_page = page;
        m_pageUsed = 0;
        m_pageSize = kPageSize;
    }
    auto* slot = static_cast<uint8_t*>(m_page) + m_pageUsed;
    m_pageUsed += aligned;
    return slot;
#else
    (void)bytes;
    return nullptr;
#endif
}

size_t AbiBridge::GetThunkCount() const {
    return m_thunks.size();
}

void AbiBridge::Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    m_thunks.clear();
#ifdef _WIN32
    if (m_page) {
        VirtualFree(m_page, 0, MEM_RELEASE);
        m_page = nullptr;
        m_pageUsed = 0;
        m_pageSize = 0;
    }
#endif
}

} // namespace AstroPort
