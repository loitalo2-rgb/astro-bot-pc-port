#pragma once

// ==============================================================================
// abi_bridge.h — Ponte de ABI SysV AMD64 (PS5) -> Microsoft x64 (Windows)
//
// O codigo do jogo foi compilado para System V AMD64: argumentos inteiros em
// RDI, RSI, RDX, RCX, R8, R9. Os stubs desta porta sao compilados para
// Microsoft x64: RCX, RDX, R8, R9. Chamar um stub diretamente a partir do
// codigo do convidado entregaria os argumentos nos registradores errados.
//
// Esta ponte gera, em memoria executavel, um thunk que:
//   1. realoca o retorno e o espaco de sombra exigido pela ABI da Microsoft;
//   2. move os 4 primeiros argumentos inteiros de SysV para MS x64;
//   3. copia os argumentos 5 e 6 (R8/R9 no SysV) para a pilha;
//   4. chama o alvo e devolve o controle ao chamador original.
//
// Limites explicitos (recusa por desenho, nao "melhor esforco"):
//   - somente argumentos inteiros/ponteiros (sem float/double);
//   - no maximo 6 argumentos (7+ exigem copia de pilha adicional);
//   - sem retorno de struct por ponteiro oculto e sem funcoes variadicas.
// Esses casos precisam de thunks especificos e continuam bloqueados.
// ==============================================================================

#include <cstddef>
#include <unordered_map>

namespace AstroPort {

class AbiBridge {
public:
    static AbiBridge& Get();

    // Retorna um thunk SysV->Win64 para o alvo. Em plataformas que ja usam
    // SysV, retorna o proprio alvo. O thunk e cacheado por alvo.
    void* CreateIntegerThunk(void* target);

    size_t GetThunkCount() const;
    void Shutdown();

    // Tamanho do codigo gerado, util para testes e diagnostico.
    static size_t GetThunkSize();

private:
    AbiBridge() = default;
    ~AbiBridge();
    void* AllocateCode(size_t bytes);
    void* m_page = nullptr;
    size_t m_pageUsed = 0;
    size_t m_pageSize = 0;
    std::unordered_map<void*, void*> m_thunks;
};

} // namespace AstroPort
