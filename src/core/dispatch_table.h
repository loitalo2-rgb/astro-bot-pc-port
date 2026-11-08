#pragma once
#include <cstdint>

// Estrutura da Tabela de Despacho (Dispatch Table)
// Contém ponteiros para as implementações que o jogo deve chamar.
// No inicio, apontam para implementações nativas ou stubs seguros.

struct SyscallDispatchTable {
    // I/O Syscalls
    int32_t (*SceKernelOpen)(const char* path, int flags, int mode);
    int32_t (*SceKernelClose)(int fd);
    int64_t (*SceKernelRead)(int fd, void* buf, size_t nbytes);
    int64_t (*SceKernelLseek)(int fd, int64_t offset, int whence);

    // Graphics Syscalls (exemplo inicial)
    void* (*SceGpuCreateBuffer)(size_t size, uint32_t flags);
};

// Instância global da tabela, centralizando o controle
extern SyscallDispatchTable g_Dispatch;

// Inicializa a tabela com os Shims de desenvolvimento
void InitializeDispatchTable();
