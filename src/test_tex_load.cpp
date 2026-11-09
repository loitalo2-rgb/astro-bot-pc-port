#include "gpu/vulkan_backend.h"
#include "assets/asset_loader.h"
#include <iostream>
#include <vector>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: test_tex_load <arquivo.gnfp>\n";
        return 1;
    }

    std::cout << "[TestTextureLoad] Carregando: " << argv[1] << "\n";

    std::ifstream file(argv[1], std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir arquivo.\n";
        return 1;
    }

    // Estrutura simplificada de carga para teste
    AstroPort::GnfTexture tex;
    tex.name = argv[1];

    // Leitura rudimentar do payload para teste
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    // Usa o membro 'data' em vez de 'pixelData'
    tex.data.resize(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(tex.data.data()), size);

    tex.width = 256;
    tex.height = 256;
    tex.vulkanFormat = 145; // BC7_UNORM
    tex.valid = true;

    auto& backend = AstroPort::VulkanBackend::Get();
    if (!backend.Initialize(nullptr, 800, 600, false)) {
        std::cerr << "Erro: Backend Vulkan nao inicializado.\n";
        return 1;
    }

    if (backend.UploadTexture(tex)) {
        std::cout << "[TestTextureLoad] Sucesso! Textura enviada para a GPU.\n";
    } else {
        std::cerr << "[TestTextureLoad] Falha no upload para GPU.\n";
    }

    backend.Shutdown();
    return 0;
}
