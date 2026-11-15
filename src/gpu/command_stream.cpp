#include "gpu/command_stream.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AstroPort {

namespace {

constexpr uint8_t kMagic[4] = {'A', 'G', 'C', 'R'};
constexpr size_t kHeaderSize = 16;

void WriteU16(uint8_t* out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFFu);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void WriteU32(uint8_t* out, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFFu);
    }
}

uint16_t ReadU16(const uint8_t* in) {
    return static_cast<uint16_t>(in[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(in[1]) << 8);
}

uint32_t ReadU32(const uint8_t* in) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(in[i]) << (8 * i);
    }
    return value;
}

void WriteF32(uint8_t* out, float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, bits);
}

float ReadF32(const uint8_t* in) {
    const uint32_t bits = ReadU32(in);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void EncodeRecord(const GpuCommand& command, uint8_t* out) {
    std::memset(out, 0, kGpuCommandRecordSize);
    WriteU16(out + 0, static_cast<uint16_t>(command.op));
    WriteU16(out + 2, 0);  // reserved
    WriteU32(out + 4, command.frameIndex);
    WriteU32(out + 8, command.width);
    WriteU32(out + 12, command.height);
    WriteU32(out + 16, command.formatId);
    WriteU32(out + 20, command.mipLevels);
    WriteU32(out + 24, command.indexOffset);
    WriteU32(out + 28, command.indexCount);
    WriteU32(out + 32, command.materialIndex);
    WriteF32(out + 36, command.rotY);
    WriteF32(out + 40, command.rotX);
    WriteF32(out + 44, command.zoom);
    WriteF32(out + 48, command.jump);
    WriteF32(out + 52, command.horizontalOffset);
    // out[56..63] stays zero: reserved for future fields, must remain zero.
}

bool DecodeRecord(const uint8_t* in, GpuCommand& command, std::string& error) {
    const uint16_t rawOp = ReadU16(in + 0);
    switch (rawOp) {
        case static_cast<uint16_t>(GpuCommandOp::BeginFrame):
        case static_cast<uint16_t>(GpuCommandOp::SetTransform):
        case static_cast<uint16_t>(GpuCommandOp::BindTexture):
        case static_cast<uint16_t>(GpuCommandOp::Draw):
        case static_cast<uint16_t>(GpuCommandOp::EndFrame):
            command.op = static_cast<GpuCommandOp>(rawOp);
            break;
        default:
            error = "opcode desconhecido: " + std::to_string(rawOp);
            return false;
    }

    if (ReadU16(in + 2) != 0) {
        error = "campo reservado do registro nao e zero";
        return false;
    }
    for (size_t i = 56; i < kGpuCommandRecordSize; ++i) {
        if (in[i] != 0) {
            error = "bytes reservados do registro nao sao zero";
            return false;
        }
    }

    command.frameIndex = ReadU32(in + 4);
    command.width = ReadU32(in + 8);
    command.height = ReadU32(in + 12);
    command.formatId = ReadU32(in + 16);
    command.mipLevels = ReadU32(in + 20);
    command.indexOffset = ReadU32(in + 24);
    command.indexCount = ReadU32(in + 28);
    command.materialIndex = ReadU32(in + 32);
    command.rotY = ReadF32(in + 36);
    command.rotX = ReadF32(in + 40);
    command.zoom = ReadF32(in + 44);
    command.jump = ReadF32(in + 48);
    command.horizontalOffset = ReadF32(in + 52);
    return true;
}

bool AllFinite(const GpuCommand& command) {
    return std::isfinite(command.rotY) && std::isfinite(command.rotX) &&
           std::isfinite(command.zoom) && std::isfinite(command.jump) &&
           std::isfinite(command.horizontalOffset);
}

} // namespace

const char* GpuCommandOpName(GpuCommandOp op) {
    switch (op) {
        case GpuCommandOp::BeginFrame: return "BeginFrame";
        case GpuCommandOp::SetTransform: return "SetTransform";
        case GpuCommandOp::BindTexture: return "BindTexture";
        case GpuCommandOp::Draw: return "Draw";
        case GpuCommandOp::EndFrame: return "EndFrame";
    }
    return "Desconhecido";
}

void GpuCommandRecorder::Reset() {
    m_commands.clear();
    m_nextFrameIndex = 0;
    m_frameCount = 0;
    m_frameOpen = false;
}

void GpuCommandRecorder::BeginFrame(uint32_t width, uint32_t height) {
    if (m_frameOpen) return;  // never nest frames
    GpuCommand command;
    command.op = GpuCommandOp::BeginFrame;
    command.frameIndex = m_nextFrameIndex++;
    command.width = width;
    command.height = height;
    m_commands.push_back(command);
    m_frameOpen = true;
}

void GpuCommandRecorder::SetModelTransform(float rotY, float rotX, float zoom,
                                           float jump, float horizontalOffset) {
    if (!m_frameOpen) return;  // transforms only take effect inside a frame
    GpuCommand command;
    command.op = GpuCommandOp::SetTransform;
    command.frameIndex = m_nextFrameIndex - 1;
    command.rotY = rotY;
    command.rotX = rotX;
    command.zoom = zoom;
    command.jump = jump;
    command.horizontalOffset = horizontalOffset;
    m_commands.push_back(command);
}

void GpuCommandRecorder::BindTexture(uint32_t formatId, uint32_t width,
                                     uint32_t height, uint32_t mipLevels) {
    GpuCommand command;
    command.op = GpuCommandOp::BindTexture;
    command.frameIndex =
        m_frameOpen ? m_nextFrameIndex - 1 : m_nextFrameIndex;
    command.formatId = formatId;
    command.width = width;
    command.height = height;
    command.mipLevels = mipLevels;
    m_commands.push_back(command);
}

void GpuCommandRecorder::Draw(uint32_t indexOffset, uint32_t indexCount,
                              uint32_t materialIndex) {
    if (!m_frameOpen) return;
    GpuCommand command;
    command.op = GpuCommandOp::Draw;
    command.frameIndex = m_nextFrameIndex - 1;
    command.indexOffset = indexOffset;
    command.indexCount = indexCount;
    command.materialIndex = materialIndex;
    m_commands.push_back(command);
}

void GpuCommandRecorder::EndFrame() {
    if (!m_frameOpen) return;
    GpuCommand command;
    command.op = GpuCommandOp::EndFrame;
    command.frameIndex = m_nextFrameIndex - 1;
    m_commands.push_back(command);
    m_frameOpen = false;
    ++m_frameCount;
}

bool ValidateGpuCommandStream(const std::vector<GpuCommand>& commands,
                              std::string& error) {
    error.clear();
    if (commands.empty()) {
        error = "stream vazio";
        return false;
    }
    if (commands.front().op != GpuCommandOp::BeginFrame &&
        commands.front().op != GpuCommandOp::BindTexture) {
        error = "stream nao comeca com BeginFrame ou BindTexture";
        return false;
    }
    if (commands.back().op != GpuCommandOp::EndFrame) {
        error = "stream nao termina com EndFrame";
        return false;
    }

    bool frameOpen = false;
    bool haveFrame = false;
    uint32_t frames = 0;
    uint32_t expectedFrame = 0;
    for (size_t i = 0; i < commands.size(); ++i) {
        const GpuCommand& command = commands[i];
        const std::string where = "comando " + std::to_string(i) + " (" +
                                  GpuCommandOpName(command.op) + ")";

        switch (command.op) {
            case GpuCommandOp::BeginFrame:
                if (frameOpen) {
                    error = where + ": frame ja aberto";
                    return false;
                }
                if (command.width == 0 || command.height == 0) {
                    error = where + ": dimensoes de frame invalidas";
                    return false;
                }
                if (!haveFrame) {
                    expectedFrame = command.frameIndex;
                    haveFrame = true;
                }
                if (command.frameIndex != expectedFrame) {
                    error = where + ": indice de frame fora de sequencia";
                    return false;
                }
                expectedFrame = command.frameIndex + 1;
                frameOpen = true;
                break;

            case GpuCommandOp::SetTransform:
                if (!frameOpen) {
                    error = where + ": transform fora de frame";
                    return false;
                }
                if (!AllFinite(command)) {
                    error = where + ": transform nao finito";
                    return false;
                }
                break;

            case GpuCommandOp::BindTexture:
                // Texture binds may happen between frames (load time).
                if (command.width == 0 || command.height == 0) {
                    error = where + ": dimensoes de textura invalidas";
                    return false;
                }
                if (command.mipLevels == 0 || command.mipLevels > 32) {
                    error = where + ": numero de mips invalido";
                    return false;
                }
                break;

            case GpuCommandOp::Draw:
                if (!frameOpen) {
                    error = where + ": draw fora de frame";
                    return false;
                }
                if (command.indexCount == 0) {
                    error = where + ": draw sem indices";
                    return false;
                }
                if (static_cast<uint64_t>(command.indexOffset) +
                        command.indexCount >
                    std::numeric_limits<uint32_t>::max()) {
                    error = where + ": intervalo de indices passa de 32 bits";
                    return false;
                }
                break;

            case GpuCommandOp::EndFrame:
                if (!frameOpen) {
                    error = where + ": EndFrame sem frame aberto";
                    return false;
                }
                frameOpen = false;
                ++frames;
                break;
        }
    }

    if (frameOpen) {
        error = "frame nao fechado no fim do stream";
        return false;
    }
    if (frames == 0) {
        error = "stream sem frames completos";
        return false;
    }
    return true;
}

bool SerializeGpuCommandStream(const std::vector<GpuCommand>& commands,
                               const std::string& path, std::string& error) {
    error.clear();
    if (!ValidateGpuCommandStream(commands, error)) return false;
    if (commands.size() >
        (std::numeric_limits<uint32_t>::max() - kHeaderSize) /
            kGpuCommandRecordSize) {
        error = "stream grande demais para o formato";
        return false;
    }

    std::vector<uint8_t> bytes(kHeaderSize +
                               commands.size() * kGpuCommandRecordSize, 0);
    std::memcpy(bytes.data(), kMagic, sizeof(kMagic));
    WriteU16(bytes.data() + 4, kGpuCommandStreamVersion);
    WriteU16(bytes.data() + 6, static_cast<uint16_t>(kGpuCommandRecordSize));
    WriteU32(bytes.data() + 8, static_cast<uint32_t>(commands.size()));
    WriteU32(bytes.data() + 12, 0);  // reserved

    size_t offset = kHeaderSize;
    for (const GpuCommand& command : commands) {
        EncodeRecord(command, bytes.data() + offset);
        offset += kGpuCommandRecordSize;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "nao foi possivel criar " + path;
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) {
        std::remove(path.c_str());
        error = "falha ao gravar " + path;
        return false;
    }
    return true;
}

bool DecodeGpuCommandStream(const std::vector<uint8_t>& bytes,
                            std::vector<GpuCommand>& commands,
                            std::string& error) {
    error.clear();
    commands.clear();
    if (bytes.size() < kHeaderSize) {
        error = "arquivo menor que o cabecalho";
        return false;
    }
    if (std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0) {
        error = "magic inesperado (esperado AGCR)";
        return false;
    }
    const uint16_t version = ReadU16(bytes.data() + 4);
    if (version != kGpuCommandStreamVersion) {
        error = "versao nao suportada: " + std::to_string(version);
        return false;
    }
    const uint16_t recordSize = ReadU16(bytes.data() + 6);
    if (recordSize != kGpuCommandRecordSize) {
        error = "tamanho de registro nao suportado: " +
                std::to_string(recordSize);
        return false;
    }
    if (ReadU32(bytes.data() + 12) != 0) {
        error = "campo reservado do cabecalho nao e zero";
        return false;
    }
    const uint32_t count = ReadU32(bytes.data() + 8);
    const uint64_t expected =
        kHeaderSize + static_cast<uint64_t>(count) * kGpuCommandRecordSize;
    if (expected != bytes.size()) {
        error = "tamanho do arquivo nao confere com a contagem declarada";
        return false;
    }

    commands.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        GpuCommand command;
        if (!DecodeRecord(bytes.data() + kHeaderSize +
                              static_cast<size_t>(i) * kGpuCommandRecordSize,
                          command, error)) {
            error = "registro " + std::to_string(i) + ": " + error;
            commands.clear();
            return false;
        }
        commands.push_back(command);
    }

    if (!ValidateGpuCommandStream(commands, error)) {
        error = "stream invalido: " + error;
        commands.clear();
        return false;
    }
    return true;
}

bool LoadGpuCommandStream(const std::string& path,
                          std::vector<GpuCommand>& commands,
                          std::string& error) {
    error.clear();
    commands.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "nao foi possivel abrir " + path;
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size < 0) {
        error = "nao foi possivel medir " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (static_cast<size_t>(file.gcount()) != bytes.size()) {
            error = "leitura incompleta de " + path;
            return false;
        }
    }
    return DecodeGpuCommandStream(bytes, commands, error);
}

std::string DescribeGpuCommandStream(const std::vector<GpuCommand>& commands) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4);
    for (const GpuCommand& command : commands) {
        switch (command.op) {
            case GpuCommandOp::BeginFrame:
                out << "frame " << command.frameIndex << " "
                    << command.width << "x" << command.height << '\n';
                break;
            case GpuCommandOp::SetTransform:
                out << "  transform rotY=" << command.rotY
                    << " rotX=" << command.rotX << " zoom=" << command.zoom
                    << " jump=" << command.jump
                    << " offset=" << command.horizontalOffset << '\n';
                break;
            case GpuCommandOp::BindTexture:
                out << "  texture format=0x" << std::hex << command.formatId
                    << std::dec << " " << command.width << "x"
                    << command.height << " mips=" << command.mipLevels << '\n';
                break;
            case GpuCommandOp::Draw:
                out << "  draw offset=" << command.indexOffset
                    << " count=" << command.indexCount
                    << " material=" << command.materialIndex << '\n';
                break;
            case GpuCommandOp::EndFrame:
                out << "endframe " << command.frameIndex << '\n';
                break;
        }
    }
    return out.str();
}

bool ReplayGpuCommandStream(const std::vector<GpuCommand>& commands,
                            GpuCommandSink& sink, std::string& error) {
    if (!ValidateGpuCommandStream(commands, error)) return false;
    for (const GpuCommand& command : commands) {
        switch (command.op) {
            case GpuCommandOp::BeginFrame: sink.OnBeginFrame(command); break;
            case GpuCommandOp::SetTransform: sink.OnSetTransform(command); break;
            case GpuCommandOp::BindTexture: sink.OnBindTexture(command); break;
            case GpuCommandOp::Draw: sink.OnDraw(command); break;
            case GpuCommandOp::EndFrame: sink.OnEndFrame(command); break;
            default:
                error = "opcode desconhecido durante replay";
                return false;
        }
    }
    return true;
}

} // namespace AstroPort
