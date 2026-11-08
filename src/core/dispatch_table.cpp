#include "dispatch_table.h"
#include <iostream>
#include <windows.h>

// --- Implementações (Shims) ---

int32_t Shim_Open(const char* path, int flags, int mode) {
    std::cout << "[Shim] Abrindo: " << path << std::endl;
    // Aqui entra sua logica de CreateFileA
    return 1;
}

int32_t Shim_Close(int fd) {
    std::cout << "[Shim] Fechando arquivo." << std::endl;
    return 0;
}

int64_t Shim_Read(int fd, void* buf, size_t nbytes) {
    std::cout << "[Shim] Lendo arquivo..." << std::endl;
    return 0;
}

int64_t Shim_Lseek(int fd, int64_t offset, int whence) {
    return 0;
}

void* Shim_GpuCreateBuffer(size_t size, uint32_t flags) {
    std::cout << "[Shim] Alocando buffer GPU de " << size << " bytes." << std::endl;
    return nullptr;
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
