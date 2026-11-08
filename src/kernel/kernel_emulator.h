#pragma once

#include "../common/types.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace AstroPort {

struct DirectMemoryBlock {
    void* hostAddress = nullptr;
    size_t size = 0;
    uint32_t memoryType = 0; // 0 = Generic, 1 = GARLIC (VRAM), 2 = ONION (Coherent)
};

class KernelEmulator {
public:
    static KernelEmulator& Get();

    bool Initialize();
    void Shutdown();

    // Gerenciamento de Memoria Virtual (sceKernelMmap / sceKernelAllocateDirectMemory)
    void* AllocateVirtualMemory(size_t size, uint32_t protectionFlags);
    bool FreeVirtualMemory(void* ptr, size_t size);

    DirectMemoryBlock AllocateDirectMemory(size_t size, size_t alignment, uint32_t memoryType);
    bool ReleaseDirectMemory(void* hostAddress);

    // Threads e Sincronizacao (scePthread)
    uint64_t CreateThread(void* (*entryPoint)(void*), void* arg, const char* name, size_t stackSize = 0);
    bool JoinThread(uint64_t threadId, void** valuePtr);

    // Timers e Ciclos (sceKernelGetProcessTime)
    uint64_t GetProcessTimeMicroseconds() const;
    uint64_t GetPerformanceCounter() const;

    // Sistema de Arquivos e Redirecionamento de Caminhos (/app0/ -> pasta local)
    static std::string ResolveGamePath(const std::string& originalPath);
    static int32_t SceKernelOpen(const char* path, int flags, int mode);
    static int32_t SceKernelClose(int fd);
    static int64_t SceKernelRead(int fd, void* buf, size_t nbytes);
    static int64_t SceKernelLseek(int fd, int64_t offset, int whence);

private:
    KernelEmulator() = default;
    bool m_initialized = false;
    uint64_t m_startTime = 0;
    uint64_t m_perfFrequency = 0;

    std::unordered_map<void*, DirectMemoryBlock> m_directAllocations;
};

} // namespace AstroPort
