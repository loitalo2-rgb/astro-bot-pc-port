#include <iostream>
#include <windows.h>

// Tipo para o ponteiro de função
typedef void (*SyscallOpen_t)(const char* path);

// Função original (simula o comportamento nativo do console)
void Original_SceKernelOpen(const char* path) {
    std::cout << "[Console Original] Solicitando abertura de arquivo: " << path << std::endl;
}

// Shim Nativo (sua implementação para PC / Windows)
void Shim_SceKernelOpen(const char* path) {
    std::cout << "[Shim PC Nativo] Interceptado! Redirecionando " << path << " -> Win32 CreateFileA" << std::endl;
}

// Tabela de Despacho (Dispatch Table)
SyscallOpen_t g_SceKernelOpen = Original_SceKernelOpen;

int main() {
    std::cout << "=== Demonstracao Segura: Despacho Local (Function Pointer Swapping) ===" << std::endl << std::endl;

    // Fase 1: Chamada padrão (Original)
    std::cout << "--- Fase 1: Execucao Sem Shim ---" << std::endl;
    g_SceKernelOpen("app0:data/prein/characters/gfx/astro_lowpoly_costume.jxm");

    Sleep(1000);

    // Fase 2: Ativação do Shim (Re-roteamento seguro)
    std::cout << std::endl << "--- Fase 2: Aplicando Shim Nativo na Tabela de Despacho ---" << std::endl;
    g_SceKernelOpen = Shim_SceKernelOpen;

    // Fase 3: Execução Interceptada
    std::cout << std::endl << "--- Fase 3: Execucao Com Shim Nativo ---" << std::endl;
    g_SceKernelOpen("app0:data/prein/characters/gfx/astro_lowpoly_costume.jxm");
    g_SceKernelOpen("app0:data/levels/hub_crashsite.jxm");

    std::cout << std::endl << "=== Teste Concluido com Sucesso (Seguro, Sem Injecao de Memoria) ===" << std::endl;
    return 0;
}
