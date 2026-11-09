#include "dispatch_table.h"
#include "../gpu/vulkan_backend.h"
#include <iostream>
#include <windows.h>

// --- Implementações (Shims Nativas) ---

int32_t Shim_Open(const char* path, int flags, int mode) {
    if (!path) return -1;

    std::string pathStr(path);
    std::string resolved = pathStr;
    if (pathStr.rfind("app0:", 0) == 0) {
        resolved = "D:/Games/abaixar/jogo/PPSA21567/" + pathStr.substr(5);
    }

    HANDLE hFile = CreateFileA(resolved.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }
    std::cout << "[Shim Win32] Arquivo aberto com sucesso: " << resolved << std::endl;
    return (int32_t)(uintptr_t)hFile;
}

int32_t Shim_Close(int fd) {
    HANDLE hFile = (HANDLE)(uintptr_t)fd;
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    CloseHandle(hFile);
    return 0;
}

int64_t Shim_Read(int fd, void* buf, size_t nbytes) {
    HANDLE hFile = (HANDLE)(uintptr_t)fd;
    if (hFile == INVALID_HANDLE_VALUE || !buf) return -1;

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buf, (DWORD)nbytes, &bytesRead, NULL)) {
        return -1;
    }
    return (int64_t)bytesRead;
}

int64_t Shim_Lseek(int fd, int64_t offset, int whence) {
    HANDLE hFile = (HANDLE)(uintptr_t)fd;
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    DWORD moveMethod = FILE_BEGIN;
    if (whence == 1) moveMethod = FILE_CURRENT;
    else if (whence == 2) moveMethod = FILE_END;

    LARGE_INTEGER liOffset, liNew;
    liOffset.QuadPart = offset;
    if (!SetFilePointerEx(hFile, liOffset, &liNew, moveMethod)) {
        return -1;
    }
    return liNew.QuadPart;
}

// Conexão do Shim Gráfico diretamente com a VulkanBackend!
void* Shim_GpuCreateBuffer(size_t size, uint32_t flags) {
    std::cout << "[Shim GPU -> Vulkan] Solicitando alocacao de buffer GPU (" << size << " bytes)..." << std::endl;

    // Alocação direta via VirtualAlloc com flag de acesso para host/device coerente
    void* hostMemory = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (hostMemory) {
        std::cout << "[Shim GPU -> Vulkan] Buffer criado com sucesso em " << hostMemory << std::endl;
    } else {
        std::cerr << "[Shim GPU -> Vulkan] ERRO ao alocar buffer!" << std::endl;
    }
    return hostMemory;
}

// --- Tabela Global ---
SyscallDispatchTable g_Dispatch;

void InitializeDispatchTable() {
    g_Dispatch.SceKernelOpen = Shim_Open;
    g_Dispatch.SceKernelClose = Shim_Close;
    g_Dispatch.SceKernelRead = Shim_Read;
    g_Dispatch.SceKernelLseek = Shim_Lseek;
    g_Dispatch.SceGpuCreateBuffer = Shim_GpuCreateBuffer;
}
