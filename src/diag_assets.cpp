#include "assets/jxmd_parser.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: diag_assets <arquivo.jxm>\n";
        return 1;
    }

    AstroPort::JxmdMesh mesh;
    if (!AstroPort::ParseJxmdFile(argv[1], mesh)) {
        std::cerr << "Falha ao analisar arquivo: " << argv[1] << "\n";
        return 1;
    }

    std::cout << "Diagnóstico de Assets: " << mesh.name << "\n";
    std::cout << "  Texturas (" << mesh.textures.size() << "):\n";
    for (const auto& tex : mesh.textures) {
        std::cout << "    ID: " << tex.id << " | Res: " << tex.width << "x" << tex.height
                  << " | Formato: 0x" << std::hex << tex.format << std::dec << "\n";
    }

    std::cout << "  Materiais (" << mesh.materials.size() << "):\n";
    for (const auto& mat : mesh.materials) {
        std::cout << "    ID: " << mat.id << " | Texturas: ";
        for (uint32_t texId : mat.textureIds) std::cout << texId << " ";
        std::cout << "\n";
    }

    return 0;
}
