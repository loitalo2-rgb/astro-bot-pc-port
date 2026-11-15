// AGCR command stream tests: recording, validation, serialization round-trip
// and fail-closed rejection of malformed files. No GPU and no game code.

#include "gpu/command_stream.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

std::vector<AstroPort::GpuCommand> MakeTwoFrames() {
    AstroPort::GpuCommandRecorder recorder;
    recorder.BeginFrame(1920, 1080);
    recorder.SetModelTransform(0.25f, 0.15f, 1.5f, 0.75f, -0.5f);
    recorder.BindTexture(0xCB60, 1024, 1024, 11);
    recorder.Draw(0, 36, 0);
    recorder.Draw(36, 12, 2);
    recorder.EndFrame();
    recorder.BeginFrame(1280, 720);
    recorder.SetModelTransform(0.5f, 0.15f, 1.0f, 0.0f, 0.0f);
    recorder.Draw(0, 24, 1);
    recorder.EndFrame();
    return recorder.Commands();
}

std::vector<uint8_t> ReadAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

bool Rejects(const std::vector<uint8_t>& bytes, std::string& error) {
    std::vector<AstroPort::GpuCommand> decoded;
    return !AstroPort::DecodeGpuCommandStream(bytes, decoded, error);
}

class CountingSink : public AstroPort::GpuCommandSink {
public:
    void OnBeginFrame(const AstroPort::GpuCommand& command) override {
        ++beginFrames;
        lastFrame = command.frameIndex;
    }
    void OnSetTransform(const AstroPort::GpuCommand&) override { ++transforms; }
    void OnBindTexture(const AstroPort::GpuCommand&) override { ++textures; }
    void OnDraw(const AstroPort::GpuCommand& command) override {
        ++draws;
        indices += command.indexCount;
    }
    void OnEndFrame(const AstroPort::GpuCommand&) override { ++endFrames; }

    uint32_t beginFrames = 0;
    uint32_t endFrames = 0;
    uint32_t transforms = 0;
    uint32_t textures = 0;
    uint32_t draws = 0;
    uint64_t indices = 0;
    uint32_t lastFrame = 0;
};

} // namespace

int main() {
    using namespace AstroPort;

    // --------------------------------------------------------------
    // Recording.
    // --------------------------------------------------------------
    const std::vector<GpuCommand> stream = MakeTwoFrames();
    expect(stream.size() == 10, "two-frame stream has 10 commands");
    expect(stream.front().op == GpuCommandOp::BeginFrame,
           "stream starts with BeginFrame");
    expect(stream.back().op == GpuCommandOp::EndFrame,
           "stream ends with EndFrame");
    expect(stream[0].frameIndex == 0 && stream[8].frameIndex == 1,
           "frame indices are sequential");

    GpuCommandRecorder misuse;
    misuse.Draw(0, 36, 0);          // ignored: no frame open
    misuse.SetModelTransform(0.f, 0.f, 1.f, 0.f, 0.f);  // ignored
    misuse.EndFrame();              // ignored
    expect(misuse.Empty(), "commands outside a frame are ignored");
    misuse.BeginFrame(640, 480);
    misuse.BeginFrame(640, 480);    // ignored: never nest
    misuse.EndFrame();
    expect(misuse.Size() == 2, "nested BeginFrame is ignored");
    expect(!misuse.IsFrameOpen() && misuse.FrameCount() == 1,
           "frame closes exactly once");

    // --------------------------------------------------------------
    // Validation.
    // --------------------------------------------------------------
    {
        std::string error;
        expect(ValidateGpuCommandStream(stream, error),
               "well-formed stream validates");

        std::vector<GpuCommand> empty;
        expect(!ValidateGpuCommandStream(empty, error), "empty stream is refused");

        std::vector<GpuCommand> drawOutside;
        GpuCommand draw;
        draw.op = GpuCommandOp::Draw;
        draw.indexCount = 3;
        drawOutside.push_back(draw);
        expect(!ValidateGpuCommandStream(drawOutside, error),
               "draw outside a frame is refused");

        std::vector<GpuCommand> unterminated(stream.begin(), stream.end() - 1);
        expect(!ValidateGpuCommandStream(unterminated, error),
               "open frame at the end is refused");

        std::vector<GpuCommand> zeroFrame = stream;
        zeroFrame[0].width = 0;
        expect(!ValidateGpuCommandStream(zeroFrame, error),
               "zero-sized frame is refused");

        std::vector<GpuCommand> badTransform = stream;
        badTransform[1].zoom = std::numeric_limits<float>::quiet_NaN();
        expect(!ValidateGpuCommandStream(badTransform, error),
               "non-finite transform is refused");

        std::vector<GpuCommand> badMips = stream;
        badMips[2].mipLevels = 0;
        expect(!ValidateGpuCommandStream(badMips, error),
               "texture without mips is refused");

        std::vector<GpuCommand> zeroDraw = stream;
        zeroDraw[3].indexCount = 0;
        expect(!ValidateGpuCommandStream(zeroDraw, error),
               "draw without indices is refused");

        std::vector<GpuCommand> skippedFrame = stream;
        skippedFrame[6].frameIndex = 5;
        expect(!ValidateGpuCommandStream(skippedFrame, error),
               "non-sequential frame index is refused");

        // Load-time texture binds may precede the first frame.
        std::vector<GpuCommand> leadingTexture;
        GpuCommand texture;
        texture.op = GpuCommandOp::BindTexture;
        texture.width = 64;
        texture.height = 64;
        texture.mipLevels = 1;
        leadingTexture.push_back(texture);
        leadingTexture.insert(leadingTexture.end(), stream.begin(),
                              stream.end());
        expect(ValidateGpuCommandStream(leadingTexture, error),
               "texture bind before the first frame is accepted");

        // ...but textures alone are not a frame stream.
        expect(!ValidateGpuCommandStream({texture}, error),
               "stream with textures only is refused");
    }

    // --------------------------------------------------------------
    // Serialization round-trip.
    // --------------------------------------------------------------
    const std::string path =
        (std::filesystem::temp_directory_path() / "astroport_cmd_test.agcr")
            .string();
    {
        std::string error;
        expect(SerializeGpuCommandStream(stream, path, error),
               "valid stream serializes");

        std::vector<GpuCommand> loaded;
        expect(LoadGpuCommandStream(path, loaded, error),
               "serialized stream loads back");
        expect(loaded.size() == stream.size(), "round-trip keeps command count");
        for (size_t i = 0; i < loaded.size(); ++i) {
            expect(loaded[i].op == stream[i].op, "round-trip keeps opcode");
            expect(loaded[i].frameIndex == stream[i].frameIndex,
                   "round-trip keeps frame index");
            expect(loaded[i].indexOffset == stream[i].indexOffset &&
                       loaded[i].indexCount == stream[i].indexCount,
                   "round-trip keeps draw range");
        }
        expect(loaded[1].zoom == 1.5f && loaded[1].horizontalOffset == -0.5f,
               "round-trip keeps transform values");
        expect(loaded[2].formatId == 0xCB60 && loaded[2].mipLevels == 11,
               "round-trip keeps texture metadata");

        expect(std::filesystem::file_size(path) ==
                   16u + stream.size() * 64u,
               "file is header plus fixed-size records");

        const std::string description = DescribeGpuCommandStream(loaded);
        expect(description.find("frame 0 1920x1080") != std::string::npos,
               "description names the first frame");
        expect(description.find("texture format=0xcb60") != std::string::npos,
               "description includes the texture format");
        expect(description.find("draw offset=36 count=12 material=2") !=
                   std::string::npos,
               "description includes draw ranges");

        CountingSink sink;
        expect(ReplayGpuCommandStream(loaded, sink, error),
               "validated stream replays");
        expect(sink.beginFrames == 2 && sink.endFrames == 2,
               "replay sees both frames");
        expect(sink.draws == 3 && sink.indices == 72,
               "replay sees every draw and index");
        expect(sink.textures == 1 && sink.transforms == 2,
               "replay sees texture binds and transforms");
        expect(sink.lastFrame == 1, "replay preserves frame order");
    }

    // --------------------------------------------------------------
    // Fail-closed decoding of malformed files.
    // --------------------------------------------------------------
    {
        const std::vector<uint8_t> good = ReadAll(path);
        expect(good.size() == 16u + stream.size() * 64u,
               "test fixture read back");

        std::string error;
        std::vector<uint8_t> bad = good;
        bad[0] = 'X';
        expect(Rejects(bad, error), "wrong magic is refused");

        bad = good;
        bad[4] = 2;  // version
        expect(Rejects(bad, error), "unknown version is refused");

        bad = good;
        bad[6] = 32;  // record size
        expect(Rejects(bad, error), "unexpected record size is refused");

        bad = good;
        bad[12] = 1;  // header reserved
        expect(Rejects(bad, error), "non-zero header reserved byte is refused");

        bad = good;
        bad[8] = static_cast<uint8_t>((stream.size() + 1) & 0xFF);
        expect(Rejects(bad, error), "declared count mismatch is refused");

        bad = good;
        bad.push_back(0);
        expect(Rejects(bad, error), "trailing byte is refused");

        bad = good;
        bad.resize(bad.size() - 1);
        expect(Rejects(bad, error), "truncated file is refused");

        bad = good;
        bad[16] = 99;  // opcode of the first record
        expect(Rejects(bad, error), "unknown opcode is refused");

        bad = good;
        bad[16 + 2] = 1;  // record reserved
        expect(Rejects(bad, error), "non-zero record reserved byte is refused");

        bad = good;
        bad[16 + 56] = 1;  // trailing reserved bytes of a record
        expect(Rejects(bad, error), "non-zero record padding is refused");

        // A structurally decodable but invalid stream must also be refused:
        // turn the second record (SetTransform) into an EndFrame, closing the
        // frame twice.
        bad = good;
        std::memset(bad.data() + 16 + 64, 0, 2);
        bad[16 + 64 + 0] = 5;
        expect(Rejects(bad, error),
               "decodable but invalid stream is refused");

        std::vector<GpuCommand> missing;
        expect(!LoadGpuCommandStream(path + ".missing", missing, error),
               "missing file is refused");
    }

    std::remove(path.c_str());

    if (failures != 0) {
        std::cerr << failures << " command stream test(s) failed.\n";
        return 1;
    }
    std::cout << "Command stream validation passed.\n";
    return 0;
}
