// diag_eboot -- diagnostic CLI.
//
// Reads a SELF container or raw ELF64 file and prints, without executing
// anything:
//   - the SELF record table summary (flags, offsets, sizes, entropy per record)
//   - inner ELF64 metadata (entry point, PT_LOAD segments, PT_DYNAMIC)
//   - the SELF->ELF reconstruction result and its validation by our own parser
//   - the fail-closed loader verdict for the file
//
// Usage: diag_eboot <path-to-eboot.bin-or-elf> [output-rebuilt.elf]

#include "core/eboot_loader.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Uso: diag_eboot <arquivo> [saida-reconstruida.elf]\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Nao foi possivel abrir: " << argv[1] << "\n";
        return 1;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
    if (!file) {
        std::cerr << "Leitura incompleta.\n";
        return 1;
    }
    std::cout << "Arquivo: " << argv[1] << " (" << bytes.size() << " bytes)\n\n";

    AstroPort::SelfContainerReport selfReport;
    bool selfParsed =
        AstroPort::InspectSelfContainer(bytes.data(), bytes.size(), selfReport);
    if (selfParsed) {
        std::cout << "Informe da inspecao de SELF:\n";
        std::cout << "  magic valido            : sim\n";
        std::cout << "  ELF interno em          : 0x" << std::hex
                  << selfReport.innerElfHeaderOffset << std::dec << "\n";
        std::cout << "  registros               : "
                  << selfReport.records.size() << "\n";
        std::cout << "  algum marcado cript.    : "
                  << (selfReport.anyMarkedEncrypted ? 1 : 0) << "\n";
        std::cout << "  algum marcado comprimido: "
                  << (selfReport.anyMarkedCompressed ? 1 : 0) << "\n";
        std::cout << "  todos alta entropia     : "
                  << (selfReport.allDataRecordsHighEntropy ? 1 : 0) << "\n";
    } else {
        std::cout << "Informe da inspecao de SELF: nao e um SELF valido "
                     "(provavel ELF cru)\n";
    }

    AstroPort::Elf64RebuiltInfo rebuilt;
    bool rebuiltOk = false;
    if (selfParsed) {
        std::cout << "\nReconstrucao SELF -> ELF64 (analise; nada sera "
                     "executado):\n";
        rebuiltOk = AstroPort::ReconstructElf64FromSelf(bytes.data(),
                                                        bytes.size(), rebuilt);
        std::cout << "  resultado: "
                  << (rebuiltOk ? "reconstruido" : "recusado") << "\n";
        if (rebuiltOk) {
            // Validate the rebuilt image with our own parser: metadata,
            // then load mapping and relocation analysis on the artifact.
            AstroPort::Elf64Metadata metadata;
            if (AstroPort::ParseElf64Metadata(rebuilt.bytes.data(),
                                              rebuilt.bytes.size(),
                                              metadata, true)) {
                std::cout << "  auto-validacao (metadados): OK; "
                          << metadata.loadSegments.size() << " PT_LOAD"
                          << (metadata.hasDynamicSegment ? ", PT_DYNAMIC" : "")
                          << "\n";
                AstroPort::Elf64MappedImage image;
                if (AstroPort::MapElf64LoadSegments(rebuilt.bytes.data(),
                                                    rebuilt.bytes.size(),
                                                    metadata, image)) {
                    std::cout << "  auto-validacao (mapeamento): OK; base 0x"
                              << std::hex << image.virtualBase
                              << " imagem " << std::dec << image.bytes.size()
                              << " bytes\n";
                    AstroPort::Elf64RelocationResult relocations;
                    AstroPort::ApplyElf64RelativeRelocations(
                        rebuilt.bytes.data(), rebuilt.bytes.size(), metadata,
                        image, relocations);
                } else {
                    std::cout << "  auto-validacao (mapeamento): recusada\n";
                }
            } else {
                std::cout << "  auto-validacao (metadados): FALHOU\n";
            }
        }
    }

    const uint64_t elfOffset =
        selfParsed ? selfReport.innerElfHeaderOffset : 0;
    if (!selfParsed && bytes.size() >= 64) {
        AstroPort::Elf64Metadata metadata;
        if (AstroPort::ParseElf64Metadata(bytes.data(), bytes.size(),
                                          metadata, false)) {
            std::cout << "Metadados ELF64 diretos: e_type " << metadata.type
                      << " e_machine " << metadata.machine << " entry 0x"
                      << std::hex << metadata.entryVaddr << std::dec << "\n";
        }
    }
    (void)elfOffset;

    if (rebuiltOk && argc == 3) {
        // Optional analysis artifact, written only when explicitly requested.
        std::ofstream out(argv[2], std::ios::binary);
        out.write(reinterpret_cast<const char*>(rebuilt.bytes.data()),
                  static_cast<std::streamsize>(rebuilt.bytes.size()));
        std::cout << "\nArquivo de analise escrito: " << argv[2] << " ("
                  << rebuilt.bytes.size() << " bytes)\n";
    }

    std::cout << "\nVeredito do loader (caminho fail-closed completo):\n";
    AstroPort::EbootLoader& loader = AstroPort::EbootLoader::Get();
    const bool loadedOk = loader.Load(argv[1]);
    std::cout << "  Load(): " << (loadedOk ? "aceito" : "recusado") << "\n";
    return 0;
}
