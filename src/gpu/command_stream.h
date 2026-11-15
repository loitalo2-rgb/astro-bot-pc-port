#pragma once

// AstroPort GPU command stream (AGCR).
//
// This is the port's own, neutral recording format. It is NOT the guest GNM/AGC
// packet format: no guest command stream is available offline, so nothing here
// claims binary compatibility with it. The point is to make every operation the
// renderer emits recordable, serializable and replayable without a GPU, so that
// frames can be inspected, diffed and validated outside the engine.
//
// Records are fixed size (64 bytes), little-endian and versioned. Unknown
// opcodes, unknown versions, trailing bytes or reserved fields that are not zero
// are refused instead of guessed.

#include <cstdint>
#include <string>
#include <vector>

namespace AstroPort {

enum class GpuCommandOp : uint16_t {
    BeginFrame = 1,
    SetTransform = 2,
    BindTexture = 3,
    Draw = 4,
    EndFrame = 5,
};

constexpr uint32_t kGpuCommandRecordSize = 64;
constexpr uint16_t kGpuCommandStreamVersion = 1;

struct GpuCommand {
    GpuCommandOp op = GpuCommandOp::BeginFrame;
    uint32_t frameIndex = 0;

    // BeginFrame
    uint32_t width = 0;
    uint32_t height = 0;

    // BindTexture
    uint32_t formatId = 0;
    uint32_t mipLevels = 0;

    // Draw
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;

    // SetTransform
    float rotY = 0.0f;
    float rotX = 0.0f;
    float zoom = 1.0f;
    float jump = 0.0f;
    float horizontalOffset = 0.0f;
};

const char* GpuCommandOpName(GpuCommandOp op);

// Collects commands in emission order. Frame balance is tracked internally, so
// misuse cannot create an unbalanced stream: EndFrame without an open frame is
// ignored, and BeginFrame inside an open frame closes nothing.
class GpuCommandRecorder {
public:
    void Reset();

    void BeginFrame(uint32_t width, uint32_t height);
    void SetModelTransform(float rotY, float rotX, float zoom, float jump,
                           float horizontalOffset);
    void BindTexture(uint32_t formatId, uint32_t width, uint32_t height,
                     uint32_t mipLevels);
    void Draw(uint32_t indexOffset, uint32_t indexCount, uint32_t materialIndex);
    void EndFrame();

    bool IsFrameOpen() const { return m_frameOpen; }
    uint32_t FrameCount() const { return m_frameCount; }
    const std::vector<GpuCommand>& Commands() const { return m_commands; }
    size_t Size() const { return m_commands.size(); }
    bool Empty() const { return m_commands.empty(); }

private:
    std::vector<GpuCommand> m_commands;
    uint32_t m_nextFrameIndex = 0;
    uint32_t m_frameCount = 0;
    bool m_frameOpen = false;
};

// Structural validation of an in-memory stream. Returns false and sets `error`
// on the first violation. Never repairs or reorders the stream.
bool ValidateGpuCommandStream(const std::vector<GpuCommand>& commands,
                              std::string& error);

// Binary (de)serialization. Serialize validates first and refuses to write an
// invalid stream. Load validates magic, version, record size, declared count,
// exact file length and every record, then validates the stream structure.
bool SerializeGpuCommandStream(const std::vector<GpuCommand>& commands,
                               const std::string& path, std::string& error);
bool LoadGpuCommandStream(const std::string& path,
                          std::vector<GpuCommand>& commands,
                          std::string& error);
bool DecodeGpuCommandStream(const std::vector<uint8_t>& bytes,
                            std::vector<GpuCommand>& commands,
                            std::string& error);

// Human-readable dump, one line per command. Safe to print for any stream.
std::string DescribeGpuCommandStream(const std::vector<GpuCommand>& commands);

// Replay sink. Only validated streams reach the sink; the default methods make
// a sink implement just the operations it cares about.
class GpuCommandSink {
public:
    virtual ~GpuCommandSink() = default;
    virtual void OnBeginFrame(const GpuCommand&) {}
    virtual void OnSetTransform(const GpuCommand&) {}
    virtual void OnBindTexture(const GpuCommand&) {}
    virtual void OnDraw(const GpuCommand&) {}
    virtual void OnEndFrame(const GpuCommand&) {}
};

bool ReplayGpuCommandStream(const std::vector<GpuCommand>& commands,
                            GpuCommandSink& sink, std::string& error);

} // namespace AstroPort
