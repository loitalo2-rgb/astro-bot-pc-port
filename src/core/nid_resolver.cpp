#include "nid_resolver.h"
#include "abi_bridge.h"
#include "dispatch_table.h"
#include "../stubs/sce_stubs.h"
#include "../audio/audio_backend.h"
#include "../input/input_manager.h"
#include "../kernel/kernel_emulator.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <array>
#include <iterator>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

NidResolver& NidResolver::Get() {
    static NidResolver instance;
    return instance;
}

uint64_t NidResolver::DecodeBase64Nid(const std::string& encoded) {
    if (encoded.size() != 11) return 0;

    std::array<uint8_t, 8> bytes{};
    size_t outputBit = 0;
    for (size_t i = 0; i < encoded.size(); ++i) {
        const char c = encoded[i];
        uint8_t value;
        if (c >= 'A' && c <= 'Z') value = static_cast<uint8_t>(c - 'A');
        else if (c >= 'a' && c <= 'z') value = static_cast<uint8_t>(c - 'a' + 26);
        else if (c >= '0' && c <= '9') value = static_cast<uint8_t>(c - '0' + 52);
        else if (c == '+') value = 62;
        else if (c == '-') value = 63;
        else return 0;

        for (int bit = 5; bit >= 0; --bit) {
            const bool set = ((value >> bit) & 1) != 0;
            if (outputBit < 64) {
                if (set) {
                    bytes[outputBit / 8] |=
                        static_cast<uint8_t>(1u << (7 - outputBit % 8));
                }
                ++outputBit;
            } else if (set) {
                return 0;
            }
        }
    }

    // The encoded bytes are the first 8 digest bytes in reverse order, so
    // reading them big-endian yields the same integer as ComputeNid (which
    // treats the digest as little-endian).
    uint64_t result = 0;
    for (uint8_t byte : bytes) result = (result << 8) | byte;
    return result;
}

static std::array<uint8_t, 20> Sha1(const uint8_t* data, size_t size) {
    std::vector<uint8_t> message(data, data + size);
    const uint64_t bitLength = static_cast<uint64_t>(size) * 8;
    message.push_back(0x80);
    while ((message.size() % 64) != 56) message.push_back(0);
    for (int i = 7; i >= 0; --i) {
        message.push_back(static_cast<uint8_t>(bitLength >> (i * 8)));
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (size_t offset = 0; offset < message.size(); offset += 64) {
        uint32_t words[80] = {};
        for (size_t i = 0; i < 16; ++i) {
            const size_t j = offset + i * 4;
            words[i] = (static_cast<uint32_t>(message[j]) << 24) |
                       (static_cast<uint32_t>(message[j + 1]) << 16) |
                       (static_cast<uint32_t>(message[j + 2]) << 8) |
                       static_cast<uint32_t>(message[j + 3]);
        }
        for (size_t i = 16; i < 80; ++i) {
            const uint32_t value =
                words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16];
            words[i] = (value << 1) | (value >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (uint32_t i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            const uint32_t rotatedA = (a << 5) | (a >> 27);
            const uint32_t next = rotatedA + f + e + k + words[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = next;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<uint8_t, 20> digest{};
    const uint32_t state[] = { h0, h1, h2, h3, h4 };
    for (size_t i = 0; i < 5; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            digest[i * 4 + j] =
                static_cast<uint8_t>(state[i] >> ((3 - j) * 8));
        }
    }
    return digest;
}

uint64_t NidResolver::ComputeNid(const std::string& symbolName) {
    // PS4/PS5 NIDs use SHA-1(name + SCE salt); the first 8 digest bytes are
    // serialized little-endian before base64 encoding.
    static constexpr uint8_t kNidSalt[] = {
        0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
        0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30
    };

    std::vector<uint8_t> input(symbolName.begin(), symbolName.end());
    input.insert(input.end(), std::begin(kNidSalt), std::end(kNidSalt));
    const auto digest = Sha1(input.data(), input.size());

    uint64_t nid = 0;
    for (int i = 7; i >= 0; --i) {
        nid = (nid << 8) | digest[static_cast<size_t>(i)];
    }
    return nid;
}

void NidResolver::Register(const std::string& symbolName, const std::string& library, void* stub) {
    uint64_t nid = ComputeNid(symbolName);
    m_table[nid] = { nid, library, symbolName, stub };
}

void NidResolver::RegisterEncoded(const std::string& encodedNid, const std::string& library, const std::string& symbolName, void* stub) {
    uint64_t nid = DecodeBase64Nid(encodedNid);
    if (!stub) {
        const auto existing = m_table.find(nid);
        if (existing != m_table.end()) {
            stub = existing->second.stubAddress;
        }
    }
    m_table[nid] = { nid, library, symbolName.empty() ? encodedNid : symbolName, stub };
}

bool NidResolver::LoadFromJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[NidResolver] Nao foi possivel abrir JSON de NIDs: " << jsonPath << "\n";
        return false;
    }

    std::string line;
    size_t imported = 0;
    while (std::getline(file, line)) {
        // Procura por "encodedNid#library"
        size_t quoteStart = line.find('\"');
        if (quoteStart == std::string::npos) continue;
        size_t quoteEnd = line.find('\"', quoteStart + 1);
        if (quoteEnd == std::string::npos) continue;

        std::string token = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        size_t hashPos = token.find('#');
        if (hashPos != std::string::npos && hashPos > 0) {
            std::string encodedNid = token.substr(0, hashPos);
            std::string lib = token.substr(hashPos + 1);
            RegisterEncoded(encodedNid, lib, token, nullptr);
            imported++;
        }
    }

    std::cout << "[NidResolver] " << imported << " NIDs importados de " << jsonPath << " (Total na tabela: " << m_table.size() << ")\n";
    return true;
}

namespace {
    static uint64_t Stub_GetProcessTime() {
        return AstroPort::KernelEmulator::Get().GetProcessTimeMicroseconds();
    }
    static uint64_t Stub_GetTscFrequency() {
        return AstroPort::KernelEmulator::Get().GetPerformanceCounter();
    }
    static void Stub_Usleep(uint32_t usec) {
#ifdef _WIN32
        Sleep(usec / 1000);
#endif
    }
    static void Stub_Sleep(uint32_t sec) {
#ifdef _WIN32
        Sleep(sec * 1000);
#endif
    }
    static int32_t Stub_SetThreadName(const char* /*name*/) {
        return 0;
    }
    static int32_t Stub_AllocateDirectMemory(size_t size, size_t alignment, uint32_t type, void** outAddr) {
        auto block = AstroPort::KernelEmulator::Get().AllocateDirectMemory(size, alignment, type);
        if (outAddr) *outAddr = block.hostAddress;
        return block.hostAddress ? 0 : -1;
    }
    static int32_t Stub_ReleaseDirectMemory(void* addr) {
        return AstroPort::KernelEmulator::Get().ReleaseDirectMemory(addr) ? 0 : -1;
    }
}

// ==============================================================================
// Tabela completa de NIDs identificados no eboot.bin do ASTRO BOT (PPSA21567)
// Referência cruzada com shadPS4 NID database e PS4/PS5 reverse engineering wiki
// ==============================================================================
void NidResolver::BuildTable() {
    std::cout << "[NidResolver] Construindo tabela de NIDs...\n";

    // --- libkernel.prx ---
    Register("sceKernelMmap",                    "libkernel", (void*)0);
    Register("sceKernelMunmap",                  "libkernel", (void*)0);
    Register("sceKernelAllocateDirectMemory",     "libkernel", (void*)Stub_AllocateDirectMemory);
    Register("sceKernelReleaseDirectMemory",      "libkernel", (void*)Stub_ReleaseDirectMemory);
    Register("sceKernelMapDirectMemory",          "libkernel", (void*)0);
    Register("sceKernelGetProcessTime",           "libkernel", (void*)Stub_GetProcessTime);
    Register("sceKernelGetTscFrequency",          "libkernel", (void*)Stub_GetTscFrequency);
    Register("sceKernelReadTsc",                  "libkernel", (void*)Stub_GetTscFrequency);
    Register("sceKernelUsleep",                   "libkernel", (void*)Stub_Usleep);
    Register("sceKernelSleep",                    "libkernel", (void*)Stub_Sleep);
    Register("sceKernelSetThreadName",            "libkernel", (void*)Stub_SetThreadName);
    Register("scePthreadCreate",                  "libkernel", (void*)0);
    Register("scePthreadJoin",                    "libkernel", (void*)0);
    Register("scePthreadMutexInit",               "libkernel", (void*)0);
    Register("scePthreadMutexLock",               "libkernel", (void*)0);
    Register("scePthreadMutexUnlock",             "libkernel", (void*)0);
    Register("scePthreadMutexDestroy",            "libkernel", (void*)0);
    Register("scePthreadCondInit",                "libkernel", (void*)0);
    Register("scePthreadCondSignal",              "libkernel", (void*)0);
    Register("scePthreadCondWait",                "libkernel", (void*)0);
    Register("scePthreadSemInit",                 "libkernel", (void*)0);
    Register("scePthreadSemPost",                 "libkernel", (void*)0);
    Register("scePthreadSemWait",                 "libkernel", (void*)0);
    Register("sceKernelOpen",                     "libkernel", (void*)+[](const char* path, int flags, int mode) -> int32_t { return g_Dispatch.SceKernelOpen(path, flags, mode); });
    Register("sceKernelClose",                    "libkernel", (void*)+[](int fd) -> int32_t { return g_Dispatch.SceKernelClose(fd); });
    Register("sceKernelRead",                     "libkernel", (void*)+[](int fd, void* buf, size_t nbytes) -> int64_t { return g_Dispatch.SceKernelRead(fd, buf, nbytes); });
    Register("sceKernelWrite",                    "libkernel", (void*)0);
    Register("sceKernelLseek",                    "libkernel", (void*)+[](int fd, int64_t offset, int whence) -> int64_t { return g_Dispatch.SceKernelLseek(fd, offset, whence); });
    Register("sceKernelFstat",                    "libkernel", (void*)0);
    Register("sceKernelGetdents",                 "libkernel", (void*)0);

    // --- libc.prx ---
    Register("malloc",   "libc", (void*)::malloc);
    Register("free",     "libc", (void*)::free);
    Register("realloc",  "libc", (void*)::realloc);
    Register("memcpy",   "libc", (void*)::memcpy);
    Register("memset",   "libc", (void*)::memset);
    Register("memcmp",   "libc", (void*)::memcmp);
    Register("strlen",   "libc", (void*)::strlen);
    Register("strncpy",  "libc", (void*)::strncpy);
    Register("strcmp",   "libc", (void*)::strcmp);
    Register("strncmp",  "libc", (void*)::strncmp);
    Register("sprintf",  "libc", (void*)::sprintf);
    Register("snprintf", "libc", (void*)::snprintf);
    Register("printf",   "libc", (void*)::printf);
    Register("fprintf",  "libc", (void*)::fprintf);
    Register("fopen",    "libc", (void*)::fopen);
    Register("fclose",   "libc", (void*)::fclose);
    Register("fread",    "libc", (void*)::fread);
    Register("fwrite",   "libc", (void*)::fwrite);
    Register("fseek",    "libc", (void*)::fseek);
    Register("ftell",    "libc", (void*)::ftell);
    Register("abort",    "libc", (void*)::abort);
    Register("exit",     "libc", (void*)::exit);
    Register("qsort",    "libc", (void*)::qsort);
    Register("bsearch",  "libc", (void*)::bsearch);
    Register("atoi",     "libc", (void*)::atoi);
    Register("atof",     "libc", (void*)::atof);
    Register("strtol",   "libc", (void*)::strtol);
    Register("strtod",   "libc", (void*)::strtod);

    // --- libc: simbolos identificados por forca bruta sobre a tabela real ---
    // Os nomes abaixo foram recuperados aplicando o proprio algoritmo de NID
    // (SHA-1 + salt SCE) a nomes padrao de C/C++ e conferindo a presenca no
    // eboot. Nenhuma base externa de NIDs foi copiada. Sem stub nativo, o
    // resolvedor apenas reconhece o nome e reporta a ausencia de vinculo; a
    // chamada real continua impossivel ate existir a ponte de ABI SysV->Win64.
    Register("memmove",             "libc", nullptr);
    Register("memchr",              "libc", nullptr);
    Register("bcmp",                "libc", nullptr);
    Register("strcpy",              "libc", nullptr);
    Register("strncat",             "libc", nullptr);
    Register("strcasecmp",          "libc", nullptr);
    Register("strncasecmp",         "libc", nullptr);
    Register("calloc",              "libc", nullptr);
    Register("aligned_alloc",       "libc", nullptr);
    Register("posix_memalign",      "libc", nullptr);
    Register("memalign",            "libc", nullptr);
    Register("strtof",              "libc", nullptr);
    Register("strtoull",            "libc", nullptr);
    Register("strtoumax",           "libc", nullptr);
    Register("wcslen",              "libc", nullptr);
    Register("strerror",            "libc", nullptr);
    Register("perror",              "libc", nullptr);
    Register("asctime",             "libc", nullptr);
    Register("puts",                "libc", nullptr);
    Register("fputs",               "libc", nullptr);
    Register("putchar",             "libc", nullptr);
    Register("fgetc",               "libc", nullptr);
    Register("fflush",              "libc", nullptr);
    Register("feof",                "libc", nullptr);
    Register("setvbuf",             "libc", nullptr);
    Register("vfprintf",            "libc", nullptr);
    Register("vsprintf",            "libc", nullptr);
    Register("vsnprintf",           "libc", nullptr);
    Register("sscanf",              "libc", nullptr);

    // --- libc: matematica (argumentos em XMM, sem varargs) ---
    // Ainda sem stub para nao presumir compatibilidade de ABI sem teste.
    Register("powf",                "libc", nullptr);
    Register("pow",                 "libc", nullptr);
    Register("sinf",                "libc", nullptr);
    Register("sin",                 "libc", nullptr);
    Register("cos",                 "libc", nullptr);
    Register("tanf",                "libc", nullptr);
    Register("tan",                 "libc", nullptr);
    Register("expf",                "libc", nullptr);
    Register("exp2",                "libc", nullptr);
    Register("logf",                "libc", nullptr);
    Register("log2f",               "libc", nullptr);
    Register("log10f",              "libc", nullptr);
    Register("log10",               "libc", nullptr);
    Register("fmod",                "libc", nullptr);
    Register("ldexpf",              "libc", nullptr);

    // --- libc: threads, semaforos e tempo (exigem emulacao, nao repasse) ---
    Register("pthread_mutex_lock",      "libc", nullptr);
    Register("pthread_mutex_unlock",    "libc", nullptr);
    Register("pthread_mutex_init",      "libc", nullptr);
    Register("pthread_mutex_destroy",   "libc", nullptr);
    Register("pthread_self",            "libc", nullptr);
    Register("pthread_key_create",      "libc", nullptr);
    Register("pthread_setspecific",     "libc", nullptr);
    Register("sem_init",                "libc", nullptr);
    Register("sem_destroy",             "libc", nullptr);
    Register("sem_wait",                "libc", nullptr);
    Register("sem_post",                "libc", nullptr);
    Register("time",                    "libc", nullptr);
    Register("clock",                   "libc", nullptr);
    Register("usleep",                  "libc", nullptr);
    Register("atexit",                  "libc", nullptr);

    // --- libc/C++ runtime: nomes conhecidos, sem semantica implementada ---
    Register("__gxx_personality_v0",    "libc", nullptr);
    Register("__stack_chk_guard",       "libc", nullptr);
    Register("__cxa_atexit",            "libc", nullptr);
    Register("__cxa_guard_acquire",     "libc", nullptr);
    Register("__cxa_guard_release",     "libc", nullptr);
    Register("__cxa_throw",             "libc", nullptr);
    Register("__cxa_begin_catch",       "libc", nullptr);
    Register("__cxa_end_catch",         "libc", nullptr);
    Register("__cxa_rethrow",           "libc", nullptr);
    Register("_Unwind_Resume",          "libc", nullptr);
    Register("_ZSt9terminatev",         "libc", nullptr);
    Register("_Znwm",                   "libc", nullptr);
    Register("_Znam",                   "libc", nullptr);
    Register("_ZdlPv",                  "libc", nullptr);
    Register("_ZdaPv",                  "libc", nullptr);

    // --- libc: falhas fatais, stubs sem argumentos (seguros na ABI atual) ---
    Register("__cxa_pure_virtual", "libc", (void*)+[]() -> int32_t {
        std::cerr << "[NidResolver] __cxa_pure_virtual chamado; abortando.\n";
        std::abort();
    });
    Register("__stack_chk_fail", "libc", (void*)+[]() -> int32_t {
        std::cerr << "[NidResolver] __stack_chk_fail: corrupcao de pilha; abortando.\n";
        std::abort();
    });

    // --- libSceSysmodule.prx ---
    Register("sceSysmoduleLoadModule",   "libSceSysmodule", (void*)Stubs::scePsmlInitialize);
    Register("sceSysmoduleUnloadModule", "libSceSysmodule", (void*)Stubs::scePsmlTerminate);
    Register("sceSysmoduleIsLoaded",     "libSceSysmodule", (void*)0);

    // --- libSceFiber.prx ---
    Register("sceFiberInitialize",  "libSceFiber", (void*)0);
    Register("sceFiberRun",         "libSceFiber", (void*)0);
    Register("sceFiberSwitch",      "libSceFiber", (void*)0);
    Register("sceFiberFinalize",    "libSceFiber", (void*)0);
    Register("sceFiberGetSelf",     "libSceFiber", (void*)0);

    // --- libSceAudioOut.prx ---
    Register("sceAudioOutInit",      "libSceAudioOut", (void*)Stubs::sceAudioOutInit);
    Register("sceAudioOutOpen",     "libSceAudioOut", (void*)Stubs::sceAudioOutOpen);
    Register("sceAudioOutClose",    "libSceAudioOut", (void*)Stubs::sceAudioOutClose);
    Register("sceAudioOutOutput",   "libSceAudioOut", (void*)Stubs::sceAudioOutOutput);
    Register("sceAudioOutOutputs",  "libSceAudioOut", (void*)Stubs::sceAudioOutOutputs);
    Register("sceAudioOutSetVolume","libSceAudioOut", (void*)Stubs::sceAudioOutSetVolume);

    // --- libSceAudiodec.native.prx ---
    Register("sceAudiodecInitLibrary",     "libSceAudiodec", (void*)Stubs::sceAudiodecInitLibrary);
    Register("sceAudiodecTermLibrary",     "libSceAudiodec", (void*)Stubs::sceAudiodecTermLibrary);
    Register("sceAudiodecCreateDecoder",   "libSceAudiodec", (void*)Stubs::sceAudiodecCreateDecoder);
    Register("sceAudiodecDeleteDecoder",   "libSceAudiodec", (void*)Stubs::sceAudiodecDeleteDecoder);
    Register("sceAudiodecClearContext",    "libSceAudiodec", (void*)Stubs::sceAudiodecClearContext);
    Register("sceAudiodecDecode",          "libSceAudiodec", (void*)Stubs::sceAudiodecDecode);

    // --- libScePad.prx ---
    Register("scePadInit",              "libScePad", (void*)0);
    Register("scePadOpen",              "libScePad", (void*)0);
    Register("scePadClose",             "libScePad", (void*)0);
    Register("scePadReadState",         "libScePad", (void*)0);
    Register("scePadSetVibration",      "libScePad", (void*)0);
    Register("scePadSetLightBar",       "libScePad", (void*)0);
    Register("scePadSetMotionSensorState","libScePad", (void*)0);
    Register("scePadGetDeviceInfo",     "libScePad", (void*)0);

    // --- libSceVideoOut.prx ---
    Register("sceVideoOutOpen",             "libSceVideoOut", (void*)0);
    Register("sceVideoOutClose",            "libSceVideoOut", (void*)0);
    Register("sceVideoOutSetFlipRate",      "libSceVideoOut", (void*)0);
    Register("sceVideoOutRegisterBuffers",  "libSceVideoOut", (void*)0);
    Register("sceVideoOutFlip",             "libSceVideoOut", (void*)0);
    Register("sceVideoOutGetResolutionStatus","libSceVideoOut",(void*)0);

    // --- libSceAgc.prx ---
    Register("sceAgcInit",                   "libSceAgc", (void*)0);
    Register("sceAgcTerm",                   "libSceAgc", (void*)0);
    Register("sceAgcCreateContext",          "libSceAgc", (void*)0);
    Register("sceAgcDestroyContext",         "libSceAgc", (void*)0);
    Register("sceAgcSubmit",                 "libSceAgc", (void*)0);
    Register("sceAgcWaitUntilSafeForRendering","libSceAgc", (void*)0);
    Register("sceAgcAllocateVideoMemory",    "libSceAgc", (void*)0);
    Register("sceAgcFreeVideoMemory",        "libSceAgc", (void*)0);
    Register("sceAgcCreateBuffer",           "libSceAgc", (void*)+[](size_t size, uint32_t flags) -> void* { return g_Dispatch.SceGpuCreateBuffer(size, flags); });
    Register("sceAgcCreateTexture",          "libSceAgc", (void*)0);
    Register("sceAgcCreateRenderTarget",     "libSceAgc", (void*)0);
    Register("sceAgcCreateDepthRenderTarget","libSceAgc", (void*)0);
    Register("sceAgcSetRenderTarget",        "libSceAgc", (void*)0);
    Register("sceAgcSetDepthRenderTarget",   "libSceAgc", (void*)0);
    Register("sceAgcSetViewport",            "libSceAgc", (void*)0);
    Register("sceAgcSetScissor",             "libSceAgc", (void*)0);
    Register("sceAgcDraw",                   "libSceAgc", (void*)0);
    Register("sceAgcDrawIndexed",            "libSceAgc", (void*)0);
    Register("sceAgcDispatch",               "libSceAgc", (void*)0);

    // --- libSceAgcDriver.prx ---
    Register("sceAgcDriverInit",             "libSceAgcDriver", (void*)0);
    Register("sceAgcDriverTerm",             "libSceAgcDriver", (void*)0);
    Register("sceAgcDriverGetFrame",         "libSceAgcDriver", (void*)0);

    // --- libSceNp* (todos stub) ---
    Register("sceNpManagerInit",             "libSceNpManager",  (void*)Stubs::sceNpManagerInit);
    Register("sceNpManagerTerm",             "libSceNpManager",  (void*)Stubs::sceNpManagerTerm);
    Register("sceNpManagerGetNpId",          "libSceNpManager",  (void*)Stubs::sceNpManagerGetNpId);
    Register("sceNpTrophy2Init",             "libSceNpTrophy2",  (void*)Stubs::sceNpTrophy2Init);
    Register("sceNpTrophy2Term",             "libSceNpTrophy2",  (void*)Stubs::sceNpTrophy2Term);
    Register("sceNpTrophy2UnlockTrophy",     "libSceNpTrophy2",  (void*)Stubs::sceNpTrophy2UnlockTrophy);
    Register("sceSaveDataInitialize3",       "libSceSaveData",   (void*)Stubs::sceSaveDataInitialize3);
    Register("sceSaveDataMount2",            "libSceSaveData",   (void*)Stubs::sceSaveDataMount2);
    Register("sceSaveDataUmount",            "libSceSaveData",   (void*)Stubs::sceSaveDataUmount);
    Register("sceSaveDataTerm",              "libSceSaveData",   (void*)Stubs::sceSaveDataTerm);
    Register("sceNetInit",                   "libSceNet",        (void*)Stubs::sceNetInit);
    Register("sceNetTerm",                   "libSceNet",        (void*)Stubs::sceNetTerm);
    Register("sceNetCtlInit",                "libSceNetCtl",     (void*)Stubs::sceNetCtlInit);
    Register("sceNetCtlGetState",            "libSceNetCtl",     (void*)Stubs::sceNetCtlGetState);
    Register("sceNetCtlTerm",                "libSceNetCtl",     (void*)Stubs::sceNetCtlTerm);
    Register("scePlayGoInitialize",          "libScePlayGo",     (void*)Stubs::scePlayGoInitialize);
    Register("scePlayGoTerminate",           "libScePlayGo",     (void*)Stubs::scePlayGoTerminate);
    Register("sceAppContentInitialize",      "libSceAppContent", (void*)Stubs::sceAppContentInitialize);
    Register("sceAppContentTerminate",       "libSceAppContent", (void*)Stubs::sceAppContentTerminate);
    Register("sceMsgDialogInitialize",       "libSceMsgDialog",  (void*)Stubs::sceMsgDialogInitialize);
    Register("sceMsgDialogOpen",             "libSceMsgDialog",  (void*)Stubs::sceMsgDialogOpen);
    Register("sceMsgDialogClose",            "libSceMsgDialog",  (void*)Stubs::sceMsgDialogClose);
    Register("sceMsgDialogTerm",             "libSceMsgDialog",  (void*)Stubs::sceMsgDialogTerm);

    std::cout << "[NidResolver] Tabela construida: " << m_table.size() << " NIDs registrados.\n";
}

void* NidResolver::Resolve(const std::string& nidString) const {
    // Extrai apenas a parte hash antes do primeiro '#'
    std::string hashPart = nidString.substr(0, nidString.find('#'));
    uint64_t nid = DecodeBase64Nid(hashPart);
    return ResolveByHash(nid);
}

void* NidResolver::ResolveByHash(uint64_t nidHash) const {
    auto it = m_table.find(nidHash);
    if (it != m_table.end()) {
        return it->second.stubAddress;
    }
    return nullptr;
}

const NidEntry* NidResolver::FindByHash(uint64_t nidHash) const {
    const auto it = m_table.find(nidHash);
    return it == m_table.end() ? nullptr : &it->second;
}

void* NidResolver::GetCallableStub(uint64_t nidHash) const {
    const NidEntry* entry = FindByHash(nidHash);
    if (!entry || !entry->stubAddress) return nullptr;
    return AbiBridge::Get().CreateIntegerThunk(entry->stubAddress);
}

} // namespace AstroPort
