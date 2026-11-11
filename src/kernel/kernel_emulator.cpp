#include "kernel_emulator.h"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace AstroPort {

KernelEmulator& KernelEmulator::Get() {
    static KernelEmulator instance;
    return instance;
}

bool KernelEmulator::Initialize() {
    if (m_initialized) return true;

#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    m_perfFrequency = freq.QuadPart;
    m_startTime = counter.QuadPart;
#endif

    m_initialized = true;
    std::cout << "[KernelEmulator] Sub-sistema libkernel inicializado (Timer freq: "
              << m_perfFrequency << " Hz)\n";
    return true;
}

void KernelEmulator::Shutdown() {
    for (auto& [ptr, block] : m_directAllocations) {
        if (block.hostAddress) {
#ifdef _WIN32
            VirtualFree(block.hostAddress, 0, MEM_RELEASE);
#else
            free(block.hostAddress);
#endif
        }
    }
    m_directAllocations.clear();
    m_initialized = false;
    std::cout << "[KernelEmulator] Sub-sistema libkernel encerrado.\n";
}

void* KernelEmulator::AllocateVirtualMemory(size_t size, uint32_t protectionFlags) {
#ifdef _WIN32
    DWORD flProtect = PAGE_READWRITE;
    if (protectionFlags & 0x01) flProtect = PAGE_EXECUTE_READWRITE;
    return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, flProtect);
#else
    return malloc(size);
#endif
}

bool KernelEmulator::FreeVirtualMemory(void* ptr, size_t size) {
    if (!ptr) return false;
#ifdef _WIN32
    return VirtualFree(ptr, 0, MEM_RELEASE) != 0;
#else
    free(ptr);
    return true;
#endif
}

DirectMemoryBlock KernelEmulator::AllocateDirectMemory(size_t size, size_t alignment, uint32_t memoryType) {
    DirectMemoryBlock block;
    block.size = size;
    block.memoryType = memoryType;

    // Alinha o tamanho em multiplos de 64KB (pagina grande do console)
    size_t alignedSize = (size + 0xFFFF) & ~0xFFFF;

#ifdef _WIN32
    block.hostAddress = VirtualAlloc(nullptr, alignedSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    block.hostAddress = malloc(alignedSize);
#endif

    if (block.hostAddress) {
        m_directAllocations[block.hostAddress] = block;
        const char* typeStr = (memoryType == 1) ? "GARLIC (VRAM)" : (memoryType == 2 ? "ONION (Coerente)" : "Generico");
        std::cout << "[KernelEmulator] Memoria Direta alocada: " << (alignedSize / (1024 * 1024))
                  << " MB [" << typeStr << "] no endereco " << block.hostAddress << "\n";
    }

    return block;
}

bool KernelEmulator::ReleaseDirectMemory(void* hostAddress) {
    auto it = m_directAllocations.find(hostAddress);
    if (it == m_directAllocations.end()) return false;

#ifdef _WIN32
    VirtualFree(hostAddress, 0, MEM_RELEASE);
#else
    free(hostAddress);
#endif
    m_directAllocations.erase(it);
    return true;
}

uint64_t KernelEmulator::CreateThread(void* (*entryPoint)(void*), void* arg, const char* name, size_t stackSize) {
#ifdef _WIN32
    struct ThreadData {
        void* (*entry)(void*);
        void* arg;
        std::string name;
    };

    auto data = new ThreadData{ entryPoint, arg, name ? name : "AstroWorker" };

    auto wrapper = [](void* p) -> DWORD {
        auto d = static_cast<ThreadData*>(p);

        // Simulação de TLS (Thread-Local Storage) do Guest
        // No PS5, FS:[0] aponta para o TCB (Thread Control Block)
        void* fakeTcb = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (fakeTcb) {
            // Em um sistema real, usaríamos SetThreadContext ou uma syscall customizada
            // para colocar fakeTcb no registrador FS. Aqui guardamos para referência.
            // (AstroBot detectará o ambiente HLE através desse mapeamento)
            std::cout << "[KernelEmulator] Thread \"" << d->name << "\" iniciada com Guest TCB em " << fakeTcb << "\n";
        }

        d->entry(d->arg);

        if (fakeTcb) VirtualFree(fakeTcb, 0, MEM_RELEASE);
        delete d;
        return 0;
    };

    DWORD threadId = 0;
    HANDLE hThread = ::CreateThread(nullptr, stackSize, (LPTHREAD_START_ROUTINE)wrapper, data, 0, &threadId);
    if (hThread) {
        CloseHandle(hThread);
        std::cout << "[KernelEmulator] Thread criada e despachada: \"" << data->name
                  << "\" (ID: " << threadId << ")\n";
        return threadId;
    }
#endif
    return 0;
}

bool KernelEmulator::JoinThread(uint64_t threadId, void** valuePtr) {
    // Implementacao basica de sincronizacao de thread
    return true;
}

uint64_t KernelEmulator::GetProcessTimeMicroseconds() const {
#ifdef _WIN32
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    if (m_perfFrequency == 0) return 0;
    return ((current.QuadPart - m_startTime) * 1000000ULL) / m_perfFrequency;
#else
    return 0;
#endif
}

uint64_t KernelEmulator::GetPerformanceCounter() const {
#ifdef _WIN32
    LARGE_INTEGER current;
    QueryPerformanceCounter(&current);
    return current.QuadPart;
#else
    return 0;
#endif
}

std::string KernelEmulator::ResolveGamePath(const std::string& originalPath) {
    const std::string baseGameDir = "D:/Games/abaixar/jogo/PPSA21567/";

    std::string path = originalPath;
    for (char& c : path) {
        if (c == '\\') c = '/';
    }

    // Redireciona /app0/ ou app0:
    if (path.rfind("/app0/", 0) == 0) {
        return baseGameDir + path.substr(6);
    }
    if (path.rfind("app0:", 0) == 0) {
        return baseGameDir + path.substr(5);
    }
    // Redireciona caminhos relativos de dados "data/"
    if (path.rfind("data/", 0) == 0) {
        return baseGameDir + path;
    }
    // Redireciona /savedata/
    if (path.rfind("/savedata/", 0) == 0 || path.rfind("savedata/", 0) == 0) {
        return "./savedata/" + path.substr(path.find_last_of('/') + 1);
    }

    return path;
}

int32_t KernelEmulator::SceKernelOpen(const char* path, int flags, int mode) {
    if (!path) return -1;
    std::string resolved = ResolveGamePath(path);
    std::cout << "[KernelEmulator] sceKernelOpen: \"" << path << "\" -> \"" << resolved << "\"\n";

    FILE* f = fopen(resolved.c_str(), "rb");
    if (!f) {
        std::cerr << "  [!] Arquivo nao encontrado: " << resolved << "\n";
        return -1;
    }
    return (int32_t)(uintptr_t)f;
}

int32_t KernelEmulator::SceKernelClose(int fd) {
    if (fd <= 0) return -1;
    FILE* f = (FILE*)(uintptr_t)fd;
    return fclose(f);
}

int64_t KernelEmulator::SceKernelRead(int fd, void* buf, size_t nbytes) {
    if (fd <= 0 || !buf) return -1;
    FILE* f = (FILE*)(uintptr_t)fd;
    return (int64_t)fread(buf, 1, nbytes, f);
}

int64_t KernelEmulator::SceKernelLseek(int fd, int64_t offset, int whence) {
    if (fd <= 0) return -1;
    FILE* f = (FILE*)(uintptr_t)fd;
    fseek(f, (long)offset, whence);
    return ftell(f);
}

} // namespace AstroPort
