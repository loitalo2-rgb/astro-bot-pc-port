#pragma once

// ==============================================================================
// abi_bridge.h — Ponte de ABI SysV AMD64 (PS5) -> Microsoft x64 (Windows)
//
// O codigo do jogo foi compilado para System V AMD64: argumentos inteiros em
// RDI, RSI, RDX, RCX, R8, R9 e argumentos de ponto flutuante em XMM0..XMM7, com
// contadores independentes. Os stubs desta porta sao compilados para Microsoft
// x64: os 4 primeiros argumentos ocupam RCX/RDX/R8/R9 (inteiros) ou
// XMM0..XMM3 (float/double) por posicao, e do 5o em diante vao para a pilha.
//
// Esta ponte gera, em memoria executavel, um thunk especifico para uma
// assinatura. Ele:
//   1. abre o quadro e o espaco de sombra exigido pela ABI da Microsoft;
//   2. salva os registradores de origem do SysV em um scratch no quadro;
//   3. copia argumentos de pilha do SysV para o slot correto da pilha MS;
//   4. carrega os registradores de destino na ordem posicional da MS;
//   5. chama o alvo e devolve o controle ao chamador original.
//
// Retorno inteiro/ponteiro (RAX) e float/double (XMM0) coincidem nas duas ABIs
// e nao precisam de traducao.
//
// Limites explicitos (recusa por desenho, nao "melhor esforco"):
//   - sem funcoes variadicas (a ABI da Microsoft exige duplicar argumentos de
//     ponto flutuante em registradores inteiros e uma contagem diferente);
//   - sem retorno de struct por ponteiro oculto;
//   - assinaturas mistas (com float/double) so sao aceitas quando todos os
//     argumentos cabem em registradores no SysV (<= 6 inteiros e <= 8 SSE);
//     estouro de pilha misto exigiria o classificador completo da ABI;
//   - somente assinaturas puramente inteiras podem ter 7+ argumentos; nesse
//     caso os argumentos de pilha do SysV sao copiados um a um.
// Qualquer assinatura fora dessas regras retorna nullptr (fail-closed).
// ==============================================================================

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace AstroPort {

enum class AbiValueKind : uint8_t {
    Integer = 0,  // inteiro ou ponteiro (8 bytes)
    Float = 1,    // float de 32 bits
    Double = 2,   // double de 64 bits
};

// Descreve a assinatura na ordem dos argumentos. Nao descreve o retorno: o
// retorno inteiro/ponteiro usa RAX e o retorno float/double usa XMM0 nas duas
// ABIs. Structs de retorno nao sao representaveis aqui (recusa por desenho).
struct AbiSignature {
    static constexpr size_t kMaxArgs = 16;

    uint8_t argCount = 0;
    AbiValueKind kinds[kMaxArgs] = {};

    // Acrescenta um argumento. Retorna false quando kMaxArgs ja foi atingido.
    bool Add(AbiValueKind kind);

    size_t Count(AbiValueKind kind) const;

    // Verifica as regras de suporte descritas no topo do arquivo.
    bool IsSupported() const;
};

class AbiBridge {
public:
    static AbiBridge& Get();

    // Thunk universal para alvos de assinatura desconhecida com ate 6
    // argumentos inteiros/ponteiros. E o caminho usado pelo resolvedor de NIDs,
    // que nao possui informacao de assinatura.
    void* CreateIntegerThunk(void* target);

    // Thunk especifico para uma assinatura. Em plataformas que ja usam SysV,
    // retorna o proprio alvo. O thunk e cacheado por (alvo, assinatura).
    void* CreateThunk(void* target, const AbiSignature& signature);

    size_t GetThunkCount() const;
    void Shutdown();

    // Maior codigo gerado por uma assinatura suportada (limite superior), util
    // para diagnostico e testes.
    static size_t GetThunkSize();

private:
    AbiBridge() = default;
    ~AbiBridge();
    void* AllocateCode(size_t bytes);
    void* m_page = nullptr;
    size_t m_pageUsed = 0;
    size_t m_pageSize = 0;
    std::unordered_map<std::string, void*> m_thunks;
};

} // namespace AstroPort
