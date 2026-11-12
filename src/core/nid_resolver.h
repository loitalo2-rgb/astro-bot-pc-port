#pragma once

// ==============================================================================
// nid_resolver.h — Resolvedor de NIDs (Symbol Hash IDs) do PlayStation 5
//
// No PS5, cada função importada de um módulo SCE é identificada por um NID
// (Name Identifier): SHA-1 do nome concatenado ao salt SCE; os 8 primeiros
// bytes são tratados em little-endian e codificados em 11 caracteres base64.
// O formato no binário é: "hashbase64#libhash#modhash"
//
// Este módulo mantém a tabela de NIDs conhecidos e resolve cada símbolo
// para o endereço do stub correspondente no processo PC.
// ==============================================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>

namespace AstroPort {

// Tipo de função stub: ponteiro para função sem argumentos retornando int32
using StubFn = std::function<int32_t()>;

struct NidEntry {
    uint64_t    nid;           // Hash numérico do NID
    std::string libraryName;   // Ex: "libSceAudioOut"
    std::string symbolName;    // Nome legível (para debug)
    void*       stubAddress;   // Endereço do stub no processo PC
};

class NidResolver {
public:
    static NidResolver& Get();

    // Inicializa a tabela com todos os NIDs conhecidos do ASTRO BOT
    void BuildTable();

    // Resolve um NID string ("bzQExy189ZI#s#s") → endereço do stub
    void* Resolve(const std::string& nidString) const;
    void* ResolveByHash(uint64_t nidHash) const;

    // Consulta a entrada sem resolver para um stub (nullptr se o NID não é
    // conhecido). Usada pelo catálogo offline de imports.
    const NidEntry* FindByHash(uint64_t nidHash) const;

    // Decodifica os 11 caracteres base64 de um NID para o mesmo inteiro que
    // ComputeNid produz (primeiros 8 bytes do SHA-1 em little-endian).
    static uint64_t DecodeBase64Nid(const std::string& encoded);

    // Computa o NID hash de um nome de função
    static uint64_t ComputeNid(const std::string& symbolName);

    // Carrega mapeamento de NIDs diretamente do link.json ou analysis.json
    bool LoadFromJson(const std::string& jsonPath);

    // Registra NID decodificado a partir do hash string base64 ("Ou3iL1abvng#libkernel")
    void RegisterEncoded(const std::string& encodedNid, const std::string& library, const std::string& symbolName, void* stub);

    size_t GetTableSize() const { return m_table.size(); }

private:
    NidResolver() = default;
    void Register(const std::string& symbolName, const std::string& library, void* stub);

    std::unordered_map<uint64_t, NidEntry> m_table;
};

} // namespace AstroPort
