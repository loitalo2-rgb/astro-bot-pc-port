#include "core/abi_bridge.h"

#include <cstring>
#include <mutex>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

namespace {

constexpr size_t kThunkAlignment = 16;
constexpr size_t kPageSize = 64 * 1024;

// Register numbers used by the encodings below.
constexpr int kRcx = 1;
constexpr int kRdx = 2;
constexpr int kRsi = 6;
constexpr int kRdi = 7;
constexpr int kR8 = 8;
constexpr int kR9 = 9;
constexpr int kR10 = 10;

void Emit32(std::vector<uint8_t>& code, size_t value) {
    for (int i = 0; i < 4; ++i) {
        code.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

void EmitRex(std::vector<uint8_t>& code, int reg, int rm, bool wide) {
    uint8_t rex = wide ? 0x48 : 0x40;
    if (reg & 8) rex |= 0x04;  // REX.R
    if (rm & 8) rex |= 0x01;   // REX.B
    code.push_back(rex);
}

// mov dst, [rsp + disp32]
void EmitMovRegMem(std::vector<uint8_t>& code, int dst, size_t disp) {
    EmitRex(code, dst, 4, true);
    code.push_back(0x8B);
    code.push_back(static_cast<uint8_t>(0x80 | ((dst & 7) << 3) | 0x04));
    code.push_back(0x24);  // SIB: base = rsp, no index
    Emit32(code, disp);
}

// mov [rsp + disp32], src
void EmitMovMemReg(std::vector<uint8_t>& code, int src, size_t disp) {
    EmitRex(code, src, 4, true);
    code.push_back(0x89);
    code.push_back(static_cast<uint8_t>(0x80 | ((src & 7) << 3) | 0x04));
    code.push_back(0x24);
    Emit32(code, disp);
}

// movaps [rsp + disp32], xmm
void EmitMovapsStore(std::vector<uint8_t>& code, int xmm, size_t disp) {
    if (xmm & 8) code.push_back(0x44);  // REX.R
    code.push_back(0x0F);
    code.push_back(0x29);
    code.push_back(static_cast<uint8_t>(0x80 | ((xmm & 7) << 3) | 0x04));
    code.push_back(0x24);
    Emit32(code, disp);
}

// movaps xmm, [rsp + disp32]
void EmitMovapsLoad(std::vector<uint8_t>& code, int xmm, size_t disp) {
    if (xmm & 8) code.push_back(0x44);
    code.push_back(0x0F);
    code.push_back(0x28);
    code.push_back(static_cast<uint8_t>(0x80 | ((xmm & 7) << 3) | 0x04));
    code.push_back(0x24);
    Emit32(code, disp);
}

// movss/movsd [rsp + disp32], xmm
void EmitSseStoreMem(std::vector<uint8_t>& code, int xmm, size_t disp,
                     bool isDouble) {
    if (xmm & 8) code.push_back(0x44);
    code.push_back(isDouble ? 0xF2 : 0xF3);
    code.push_back(0x0F);
    code.push_back(0x11);
    code.push_back(static_cast<uint8_t>(0x80 | ((xmm & 7) << 3) | 0x04));
    code.push_back(0x24);
    Emit32(code, disp);
}

// Bytes de um thunk SysV -> MS x64 para a assinatura informada.
//
// Entrada (chamador SysV): rsp = 8 mod 16, [rsp] = retorno.
// O quadro e escolhido com tamanho 8 mod 16, de modo que, depois de
// "sub rsp, frame", rsp fique 0 mod 16: e o alinhamento exigido pela Microsoft
// imediatamente antes de "call".
//
// Layout do quadro (rsp depois do sub):
//   0x00..0x1F        espaco de sombra da MS
//   0x20..            5o argumento em diante (pilha da MS)
//   gprOff..+0x30     copia de RDI/RSI/RDX/RCX/R8/R9
//   xmmOff..+0x80     copia de XMM0..XMM7 (alinhado em 16)
std::vector<uint8_t> BuildThunk(void* target, const AbiSignature& signature) {
    std::vector<uint8_t> code;
    const size_t intCount = signature.Count(AbiValueKind::Integer);
    const size_t floatCount = signature.argCount - intCount;
    const size_t stackArgs = signature.argCount > 4 ? signature.argCount - 4 : 0;

    const size_t base = 0x20 + 8 * stackArgs;
    const size_t gprOff = base;
    size_t cursor = base + (intCount != 0 ? 6 * 8 : 0);
    const size_t xmmOff = (cursor + 15) & ~static_cast<size_t>(15);
    cursor = xmmOff + (floatCount != 0 ? 8 * 16 : 0);
    size_t frame = cursor;
    if (frame % 16 == 0) frame += 8;  // entry 8 mod 16 -> frame 8 mod 16

    // sub rsp, frame
    code.push_back(0x48);
    code.push_back(0x81);
    code.push_back(0xEC);
    Emit32(code, frame);

    constexpr int kSysVInt[6] = {kRdi, kRsi, kRdx, kRcx, kR8, kR9};
    constexpr int kMsInt[4] = {kRcx, kRdx, kR8, kR9};

    if (intCount != 0) {
        for (int i = 0; i < 6; ++i) {
            EmitMovMemReg(code, kSysVInt[i], gprOff + 8 * i);
        }
    }
    if (floatCount != 0) {
        // SysV atribui XMM0, XMM1, ... na ordem dos argumentos float/double.
        for (size_t i = 0; i < floatCount; ++i) {
            EmitMovapsStore(code, static_cast<int>(i), xmmOff + 16 * i);
        }
    }

    // Argumentos posicionais 5+ vao para a pilha da MS. A origem pode ser um
    // registrador (mistos/4 primeiros) ou a pilha do SysV (assinatura inteira
    // com 7+ argumentos).
    int intSrc = 0;
    int sseSrc = 0;
    int stackSource = 0;
    for (size_t i = 0; i < signature.argCount; ++i) {
        const AbiValueKind kind = signature.kinds[i];
        if (kind == AbiValueKind::Integer) {
            if (i >= 4) {
                if (intSrc < 6) {
                    EmitMovRegMem(code, kR10, gprOff + 8 * intSrc);
                } else {
                    EmitMovRegMem(code, kR10, frame + 8 + 8 * stackSource);
                    ++stackSource;
                }
                EmitMovMemReg(code, kR10, 0x20 + 8 * (i - 4));
            }
            ++intSrc;
        } else {
            if (i >= 4) {
                EmitSseStoreMem(code, sseSrc, 0x20 + 8 * (i - 4),
                                kind == AbiValueKind::Double);
            }
            ++sseSrc;
        }
    }

    // Os 4 primeiros slots posicionais usam registradores na MS.
    intSrc = 0;
    sseSrc = 0;
    for (size_t i = 0; i < signature.argCount && i < 4; ++i) {
        if (signature.kinds[i] == AbiValueKind::Integer) {
            EmitMovRegMem(code, kMsInt[i], gprOff + 8 * intSrc);
            ++intSrc;
        } else {
            EmitMovapsLoad(code, static_cast<int>(i), xmmOff + 16 * sseSrc);
            ++sseSrc;
        }
    }

    code.push_back(0x48);
    code.push_back(0xB8);  // mov rax, imm64
    const uint64_t address = reinterpret_cast<uint64_t>(target);
    for (int i = 0; i < 8; ++i) {
        code.push_back(static_cast<uint8_t>((address >> (8 * i)) & 0xFF));
    }
    code.push_back(0xFF);
    code.push_back(0xD0);  // call rax

    code.push_back(0x48);
    code.push_back(0x81);
    code.push_back(0xC4);  // add rsp, frame
    Emit32(code, frame);
    code.push_back(0xC3);  // ret
    return code;
}

std::string MakeCacheKey(void* target, const AbiSignature& signature) {
    std::string key;
    key.resize(sizeof(target));
    std::memcpy(&key[0], &target, sizeof(target));
    key.push_back(static_cast<char>(signature.argCount));
    for (size_t i = 0; i < signature.argCount; ++i) {
        key.push_back(static_cast<char>(signature.kinds[i]));
    }
    return key;
}

std::mutex g_mutex;

} // namespace

bool AbiSignature::Add(AbiValueKind kind) {
    if (argCount >= kMaxArgs) return false;
    kinds[argCount++] = kind;
    return true;
}

size_t AbiSignature::Count(AbiValueKind kind) const {
    size_t total = 0;
    for (size_t i = 0; i < argCount; ++i) {
        if (kinds[i] == kind) ++total;
    }
    return total;
}

bool AbiSignature::IsSupported() const {
    if (argCount > kMaxArgs) return false;
    size_t ints = 0;
    size_t floats = 0;
    for (size_t i = 0; i < argCount; ++i) {
        switch (kinds[i]) {
            case AbiValueKind::Integer:
                ++ints;
                break;
            case AbiValueKind::Float:
            case AbiValueKind::Double:
                ++floats;
                break;
            default:
                return false;
        }
    }
    if (floats == 0) return true;  // puramente inteira: qualquer quantidade
    // Mistos/float: todas as origens precisam caber nos registradores do SysV.
    // Estouro de pilha misto exigiria o classificador completo da ABI.
    return ints <= 6 && floats <= 8;
}

AbiBridge& AbiBridge::Get() {
    static AbiBridge instance;
    return instance;
}

AbiBridge::~AbiBridge() {
    Shutdown();
}

size_t AbiBridge::GetThunkSize() {
    AbiSignature signature;
    signature.argCount = static_cast<uint8_t>(AbiSignature::kMaxArgs);
    for (size_t i = 0; i < signature.argCount; ++i) {
        signature.kinds[i] = AbiValueKind::Integer;
    }
    return BuildThunk(nullptr, signature).size();
}

void* AbiBridge::CreateIntegerThunk(void* target) {
    AbiSignature signature;
    signature.argCount = 6;
    for (size_t i = 0; i < 6; ++i) {
        signature.kinds[i] = AbiValueKind::Integer;
    }
    return CreateThunk(target, signature);
}

void* AbiBridge::CreateThunk(void* target, const AbiSignature& signature) {
    if (!target || !signature.IsSupported()) return nullptr;
#if !defined(_WIN32)
    // Em plataformas SysV a ABI do convidado e a nativa: nada a traduzir.
    (void)g_mutex;
    return target;
#else
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string key = MakeCacheKey(target, signature);
    const auto existing = m_thunks.find(key);
    if (existing != m_thunks.end()) return existing->second;

    const std::vector<uint8_t> code = BuildThunk(target, signature);
    if (code.empty()) return nullptr;
    void* thunk = AllocateCode(code.size());
    if (!thunk) return nullptr;
    std::memcpy(thunk, code.data(), code.size());
    FlushInstructionCache(GetCurrentProcess(), thunk, code.size());
    m_thunks.emplace(key, thunk);
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
