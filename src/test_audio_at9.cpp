#include "audio/audio_backend.h"
#include "core/nid_resolver.h"
#include "stubs/sce_audiodec.h"
#include "stubs/sce_stubs.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <vector>

int main() {
    using namespace AstroPort;
    using namespace AstroPort::Stubs;

    auto& nidResolver = NidResolver::Get();
    nidResolver.BuildTable();
    constexpr struct {
        const char* symbol;
        const char* nid;
        uint64_t value;
    } knownNids[] = {
        { "sceAudiodecInitLibrary", "VjhsmxpcezI", 0x56386c9b1a5c7b32ULL },
        { "sceAudiodecTermLibrary", "h5jSB2QIDV0", 0x8798d20764080d5dULL },
        { "sceAudiodecCreateDecoder", "O3f1sLMWRvs", 0x3b77f5b0b31646fbULL },
        { "sceAudiodecDeleteDecoder", "Tp+ZEy69mLk", 0x4e9f99132ebd98b9ULL },
        { "sceAudiodecClearContext", "6Vf9WTLDoss", 0xe957fd5932c3a2cbULL },
        { "sceAudiodecDecode", "KHXHMDLkILw", 0x2875c73032e420bcULL },
        { "sceAudioOutInit", "JfEPXVxhFqA", 0x25f10f5d5c6116a0ULL },
        { "sceAudioOutOutputs", "w3PdaSTSwGE", 0xc373dd6924d2c061ULL },
        { "sceAudioOutSetVolume", "b+uAV89IlxE", 0x6feb8057cf489711ULL }
    };
    for (const auto& entry : knownNids) {
        void* const resolved = nidResolver.Resolve(
            std::string(entry.nid) + "#libSceAudiodec");
        if (resolved == nullptr ||
            nidResolver.ComputeNid(entry.symbol) != entry.value ||
            nidResolver.ResolveByHash(entry.value) != resolved) {
            std::cerr << "NID lookup mismatch for " << entry.symbol << ".\n";
            return 1;
        }
    }
    nidResolver.RegisterEncoded(
        knownNids[0].nid, "libSceAudiodec",
        std::string(knownNids[0].nid) + "#libSceAudiodec", nullptr);
    if (nidResolver.Resolve(std::string(knownNids[0].nid) +
                            "#libSceAudiodec") == nullptr) {
        std::cerr << "Import metadata cleared a registered Audiodec stub.\n";
        return 1;
    }

    SceAudiodecAt9Params params = {};
    params.size = sizeof(params);
    params.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BIT;
    constexpr uint8_t config[] = { 0xFE, 0x74, 0x17, 0xF0 };
    std::copy(std::begin(config), std::end(config), params.configData);
    SceAudiodecAt9Info info = {};
    info.size = sizeof(info);
    SceAudiodecCtrl ctrl = {};
    ctrl.params = &params;
    ctrl.bitstreamInfo = &info;

    if (sceAudiodecCreateDecoder(&ctrl, SCE_AUDIODEC_CODEC_AT9) !=
        static_cast<int32_t>(0x807F0002)) {
        std::cerr << "AT9 decoder was created before library initialization.\n";
        return 1;
    }

    if (sceAudiodecInitLibrary(SCE_AUDIODEC_CODEC_AT9) != SCE_AUDIODEC_OK) {
        std::cerr << "AT9 library failed to initialize.\n";
        return 2;
    }

    const int32_t decoderHandle =
        sceAudiodecCreateDecoder(&ctrl, SCE_AUDIODEC_CODEC_AT9);
    if (decoderHandle <= 0 || info.channels != 2 ||
        info.superframeSize == 0 || info.samplesPerFrame == 0 ||
        info.framesPerSuperframe == 0) {
        std::cerr << "AT9 decoder failed to create or returned invalid info.\n";
        return 3;
    }

    const uint32_t frameBytes =
        info.superframeSize / info.framesPerSuperframe;
    std::vector<uint8_t> compressedFrame(frameBytes - 1, 0);
    std::vector<int16_t> pcm(
        static_cast<size_t>(info.channels) * info.samplesPerFrame);
    SceAudiodecAuInfo auInfo = {
        sizeof(SceAudiodecAuInfo), compressedFrame.data(),
        static_cast<uint32_t>(compressedFrame.size())
    };
    SceAudiodecPcmItem pcmItem = {
        sizeof(SceAudiodecPcmItem), pcm.data(),
        static_cast<uint32_t>(pcm.size() * sizeof(int16_t))
    };
    ctrl.auInfo = &auInfo;
    ctrl.pcmItem = &pcmItem;

    if (sceAudiodecDecode(decoderHandle, &ctrl) == SCE_AUDIODEC_OK ||
        auInfo.dataSize != compressedFrame.size() ||
        pcmItem.dataSize != pcm.size() * sizeof(int16_t)) {
        std::cerr << "AT9 decoder accepted a truncated compressed frame.\n";
        return 4;
    }

    pcmItem.dataSize = static_cast<uint32_t>(
        (pcm.size() - 1) * sizeof(int16_t));
    if (sceAudiodecDecode(decoderHandle, &ctrl) !=
        static_cast<int32_t>(0x807F0012)) {
        std::cerr << "AT9 decoder accepted an undersized PCM buffer.\n";
        return 5;
    }

    if (sceAudiodecClearContext(decoderHandle) != SCE_AUDIODEC_OK ||
        sceAudiodecDeleteDecoder(decoderHandle) != SCE_AUDIODEC_OK ||
        sceAudiodecDeleteDecoder(decoderHandle) !=
            static_cast<int32_t>(0x807F000F) ||
        sceAudiodecTermLibrary(SCE_AUDIODEC_CODEC_AT9) != SCE_AUDIODEC_OK) {
        std::cerr << "AT9 decoder lifecycle cleanup failed.\n";
        return 6;
    }

    if (Stubs::sceAudioOutInit() != Stubs::SCE_OK ||
        Stubs::sceAudioOutOpen(0, 0, 0, 256, 48000, 1) != 1) {
        std::cerr << "AudioOut stereo port failed to open.\n";
        return 7;
    }

    const int16_t pcmSample[] = { 0, 0 };
    const int16_t misalignedPcmSample = 0;
    auto& audioBackend = AudioBackend::Get();
    if (audioBackend.OutputData(1, pcmSample, sizeof(pcmSample)) >= 0 ||
        audioBackend.OutputData(1, &misalignedPcmSample,
                                sizeof(misalignedPcmSample)) >= 0 ||
        audioBackend.OutputData(2, pcmSample, sizeof(pcmSample)) >= 0 ||
        audioBackend.SetVolume(2, 0.5f) >= 0) {
        std::cerr << "AudioOut reported success for an unavailable stream or invalid handle.\n";
        return 8;
    }

    const SceAudioOutOutputParam output = { 1, 0, nullptr };
    if (Stubs::sceAudioOutOutputs(nullptr, 1) !=
            Stubs::SCE_AUDIO_OUT_ERROR_INVALID_PARAMETER ||
        Stubs::sceAudioOutOutputs(&output, 1) != Stubs::SCE_OK) {
        std::cerr << "AudioOut multi-output ABI validation failed.\n";
        return 9;
    }
    const int32_t volume[] = { 16384, 16384 };
    if (Stubs::sceAudioOutSetVolume(1, 3, volume) != Stubs::SCE_OK ||
        Stubs::sceAudioOutClose(1) != Stubs::SCE_OK) {
        std::cerr << "AudioOut volume or close call failed.\n";
        return 10;
    }
    if (Stubs::sceAudioOutSetVolume(1, 3, volume) >= 0 ||
        Stubs::sceAudioOutClose(1) >= 0) {
        std::cerr << "AudioOut accepted operations on a closed port.\n";
        return 11;
    }

    return 0;
}
