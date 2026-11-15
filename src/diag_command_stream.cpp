// diag_command_stream -- offline analysis of an AGCR command stream.
//
// Reads a recording produced by the port's renderer (VulkanBackend recorder),
// validates it and prints a summary. --demo writes a small synthetic stream so
// the format and validator can be exercised without a GPU or the game.
//
// This never executes game code and makes no claim about guest GNM/AGC packets.

#include "gpu/command_stream.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

void PrintUsage() {
    std::cerr
        << "Uso:\n"
        << "  diag_command_stream <arquivo.agcr> [--dump <saida.txt>]\n"
        << "  diag_command_stream --demo <saida.agcr>\n";
}

int WriteDemo(const std::string& path) {
    AstroPort::GpuCommandRecorder recorder;
    recorder.BeginFrame(1920, 1080);
    recorder.SetModelTransform(0.0f, 0.15f, 1.0f, 0.0f, 0.0f);
    recorder.BindTexture(0xCB60, 1024, 1024, 11);
    recorder.Draw(0, 36, 0);
    recorder.Draw(36, 36, 1);
    recorder.EndFrame();

    recorder.BeginFrame(1920, 1080);
    recorder.SetModelTransform(0.1f, 0.15f, 1.0f, 0.0f, 0.0f);
    recorder.Draw(0, 36, 0);
    recorder.EndFrame();

    std::string error;
    if (!AstroPort::SerializeGpuCommandStream(recorder.Commands(), path,
                                              error)) {
        std::cerr << "Falha ao gravar demo: " << error << "\n";
        return 1;
    }
    std::cout << "Stream demo gravado em: " << path << " ("
              << recorder.Commands().size() << " comandos, "
              << recorder.FrameCount() << " frames)\n";
    std::cout << "Aviso: stream sintetico, gerado para exercitar o formato; "
                 "nao veio de um frame real.\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string first = argv[1];
    if (first == "--demo") {
        if (argc != 3) {
            PrintUsage();
            return 1;
        }
        return WriteDemo(argv[2]);
    }

    std::string dumpPath;
    if (argc == 4 && std::string(argv[2]) == "--dump") {
        dumpPath = argv[3];
    } else if (argc != 2) {
        PrintUsage();
        return 1;
    }

    std::vector<AstroPort::GpuCommand> commands;
    std::string error;
    if (!AstroPort::LoadGpuCommandStream(first, commands, error)) {
        std::cerr << "Stream recusado: " << error << "\n";
        return 1;
    }

    uint64_t draws = 0;
    uint64_t indices = 0;
    uint64_t textures = 0;
    uint64_t transforms = 0;
    uint32_t frames = 0;
    std::map<AstroPort::GpuCommandOp, uint64_t> perOp;
    std::map<uint32_t, uint64_t> perFormat;
    for (const auto& command : commands) {
        ++perOp[command.op];
        switch (command.op) {
            case AstroPort::GpuCommandOp::BeginFrame:
                ++frames;
                break;
            case AstroPort::GpuCommandOp::Draw:
                ++draws;
                indices += command.indexCount;
                break;
            case AstroPort::GpuCommandOp::BindTexture:
                ++textures;
                ++perFormat[command.formatId];
                break;
            case AstroPort::GpuCommandOp::SetTransform:
                ++transforms;
                break;
            case AstroPort::GpuCommandOp::EndFrame:
                break;
        }
    }

    const auto fileSize =
        std::filesystem::file_size(std::filesystem::path(first));

    std::cout << "=== Stream de comandos AGCR ===\n";
    std::cout << "Arquivo        : " << first << "\n";
    std::cout << "Tamanho        : " << fileSize << " bytes\n";
    std::cout << "Comandos       : " << commands.size() << "\n";
    std::cout << "Frames         : " << frames << "\n";
    std::cout << "Draws          : " << draws << " (" << indices
              << " indices)\n";
    std::cout << "Texturas       : " << textures << "\n";
    std::cout << "Transforms     : " << transforms << "\n";
    std::cout << "Por comando    : BeginFrame="
              << perOp[AstroPort::GpuCommandOp::BeginFrame]
              << " SetTransform="
              << perOp[AstroPort::GpuCommandOp::SetTransform]
              << " BindTexture="
              << perOp[AstroPort::GpuCommandOp::BindTexture] << " Draw="
              << perOp[AstroPort::GpuCommandOp::Draw] << " EndFrame="
              << perOp[AstroPort::GpuCommandOp::EndFrame] << "\n";
    if (!perFormat.empty()) {
        std::cout << "Formatos GNF   :";
        for (const auto& [formatId, count] : perFormat) {
            std::cout << " 0x" << std::hex << formatId << std::dec << "("
                      << count << ")";
        }
        std::cout << "\n";
    }
    std::cout << "Validacao      : OK\n";

    if (!dumpPath.empty()) {
        std::ofstream dump(dumpPath, std::ios::binary | std::ios::trunc);
        if (!dump) {
            std::cerr << "Nao foi possivel criar " << dumpPath << "\n";
            return 1;
        }
        dump << AstroPort::DescribeGpuCommandStream(commands);
        dump.close();
        if (!dump) {
            std::cerr << "Falha ao gravar " << dumpPath << "\n";
            return 1;
        }
        std::cout << "Dump textual   : " << dumpPath << "\n";
    }
    return 0;
}
