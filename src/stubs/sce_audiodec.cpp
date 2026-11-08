#include "sce_audiodec.h"

#include "../audio/audio_backend.h"
#include <libatrac9.h>

#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace AstroPort::Stubs {
namespace {

constexpr int32_t kErrorApiFail = static_cast<int32_t>(0x807F0000);
constexpr int32_t kErrorInvalidType = static_cast<int32_t>(0x807F0001);
constexpr int32_t kErrorArgument = static_cast<int32_t>(0x807F0002);
constexpr int32_t kErrorParamSize = static_cast<int32_t>(0x807F0004);
constexpr int32_t kErrorBsiSize = static_cast<int32_t>(0x807F0005);
constexpr int32_t kErrorAuInfoSize = static_cast<int32_t>(0x807F0006);
constexpr int32_t kErrorPcmItemSize = static_cast<int32_t>(0x807F0007);
constexpr int32_t kErrorCtrlPointer = static_cast<int32_t>(0x807F0008);
constexpr int32_t kErrorParamPointer = static_cast<int32_t>(0x807F0009);
constexpr int32_t kErrorBsiPointer = static_cast<int32_t>(0x807F000A);
constexpr int32_t kErrorAuInfoPointer = static_cast<int32_t>(0x807F000B);
constexpr int32_t kErrorPcmItemPointer = static_cast<int32_t>(0x807F000C);
constexpr int32_t kErrorAuPointer = static_cast<int32_t>(0x807F000D);
constexpr int32_t kErrorPcmPointer = static_cast<int32_t>(0x807F000E);
constexpr int32_t kErrorHandle = static_cast<int32_t>(0x807F000F);
constexpr int32_t kErrorWordLength = static_cast<int32_t>(0x807F0010);
constexpr int32_t kErrorAuSize = static_cast<int32_t>(0x807F0011);
constexpr int32_t kErrorPcmSize = static_cast<int32_t>(0x807F0012);
constexpr int32_t kErrorAt9Config = static_cast<int32_t>(0x807F1000);

static_assert(sizeof(SceAudiodecAuInfo) == 24);
static_assert(sizeof(SceAudiodecPcmItem) == 24);
static_assert(sizeof(SceAudiodecCtrl) == 32);
static_assert(sizeof(SceAudiodecAt9Params) == 12);
static_assert(sizeof(SceAudiodecAt9Info) == 36);

struct DecoderEntry {
    AstroPort::At9DecodeContext context;

    ~DecoderEntry() {
        if (context.decoderState) {
            Atrac9ReleaseHandle(context.decoderState);
        }
    }
};

std::mutex g_mutex;
uint32_t g_libraryInitCount = 0;
std::unordered_map<int32_t, std::unique_ptr<DecoderEntry>> g_decoders;
int32_t g_nextHandle = 1;

int32_t ValidateCreateCtrl(const SceAudiodecCtrl* ctrl) {
    if (!ctrl) return kErrorCtrlPointer;
    if (!ctrl->params) return kErrorParamPointer;
    if (!ctrl->bitstreamInfo) return kErrorBsiPointer;

    const auto* params = static_cast<const SceAudiodecAt9Params*>(ctrl->params);
    const auto* info = static_cast<const SceAudiodecAt9Info*>(ctrl->bitstreamInfo);
    if (params->size != sizeof(SceAudiodecAt9Params)) return kErrorParamSize;
    if (info->size != sizeof(SceAudiodecAt9Info)) return kErrorBsiSize;
    if (params->wordLength != SCE_AUDIODEC_WORD_LENGTH_16BIT) return kErrorWordLength;
    return SCE_AUDIODEC_OK;
}

int32_t ValidateDecodeCtrl(const SceAudiodecCtrl* ctrl) {
    if (!ctrl) return kErrorCtrlPointer;
    if (!ctrl->params) return kErrorParamPointer;
    if (!ctrl->bitstreamInfo) return kErrorBsiPointer;
    if (!ctrl->auInfo) return kErrorAuInfoPointer;
    if (!ctrl->pcmItem) return kErrorPcmItemPointer;
    const auto* params =
        static_cast<const SceAudiodecAt9Params*>(ctrl->params);
    if (params->size != sizeof(SceAudiodecAt9Params)) {
        return kErrorParamSize;
    }
    if (params->wordLength != SCE_AUDIODEC_WORD_LENGTH_16BIT) {
        return kErrorWordLength;
    }
    if (static_cast<const SceAudiodecAt9Info*>(ctrl->bitstreamInfo)->size !=
        sizeof(SceAudiodecAt9Info)) {
        return kErrorBsiSize;
    }
    if (ctrl->auInfo->size != sizeof(SceAudiodecAuInfo)) return kErrorAuInfoSize;
    if (ctrl->pcmItem->size != sizeof(SceAudiodecPcmItem)) return kErrorPcmItemSize;
    if (!ctrl->auInfo->address) return kErrorAuPointer;
    if (!ctrl->pcmItem->address) return kErrorPcmPointer;
    if (ctrl->auInfo->dataSize == 0) return kErrorAuSize;
    if (ctrl->pcmItem->dataSize == 0) return kErrorPcmSize;
    return SCE_AUDIODEC_OK;
}

void FillAt9Info(SceAudiodecAt9Info& info,
                 const AstroPort::At9DecodeContext& context) {
    const uint64_t totalSamples =
        static_cast<uint64_t>(context.samplesPerFrame) *
        context.framesPerSuperframe;
    info.channels = context.channelCount;
    info.bitrate = totalSamples == 0 ? 0 : static_cast<uint32_t>(
        static_cast<uint64_t>(context.superframeSize) * 8 *
        context.sampleRate / totalSamples);
    info.sampleRate = context.sampleRate;
    info.superframeSize = context.superframeSize;
    info.framesPerSuperframe = context.framesPerSuperframe;
    info.nextFrameSize = context.superframeSize;
    info.samplesPerFrame = context.samplesPerFrame;
    info.result = SCE_AUDIODEC_OK;
}

} // namespace

int32_t sceAudiodecInitLibrary(uint32_t codecType) {
    if (codecType != SCE_AUDIODEC_CODEC_AT9) return kErrorInvalidType;
    std::lock_guard lock(g_mutex);
    ++g_libraryInitCount;
    return SCE_AUDIODEC_OK;
}

int32_t sceAudiodecTermLibrary(uint32_t codecType) {
    if (codecType != SCE_AUDIODEC_CODEC_AT9) return kErrorInvalidType;
    std::lock_guard lock(g_mutex);
    if (g_libraryInitCount > 0) --g_libraryInitCount;
    return SCE_AUDIODEC_OK;
}

int32_t sceAudiodecCreateDecoder(SceAudiodecCtrl* ctrl,
                                 uint32_t codecType) {
    if (codecType != SCE_AUDIODEC_CODEC_AT9) return kErrorInvalidType;
    if (const int32_t validation = ValidateCreateCtrl(ctrl);
        validation != SCE_AUDIODEC_OK) {
        return validation;
    }

    std::lock_guard lock(g_mutex);
    if (g_libraryInitCount == 0) return kErrorArgument;

    const auto* params =
        static_cast<const SceAudiodecAt9Params*>(ctrl->params);
    auto entry = std::make_unique<DecoderEntry>();
    auto& backend = AstroPort::AudioBackend::Get();
    if (!backend.InitAt9Decoder(entry->context, params->configData,
                                sizeof(params->configData))) {
        return kErrorAt9Config;
    }

    int32_t handle = g_nextHandle;
    while (handle <= 0 || g_decoders.contains(handle)) {
        handle = handle == std::numeric_limits<int32_t>::max() ? 1 : handle + 1;
    }
    g_nextHandle = handle == std::numeric_limits<int32_t>::max() ? 1 : handle + 1;

    auto* info = static_cast<SceAudiodecAt9Info*>(ctrl->bitstreamInfo);
    FillAt9Info(*info, entry->context);
    g_decoders.emplace(handle, std::move(entry));
    return handle;
}

int32_t sceAudiodecDeleteDecoder(int32_t handle) {
    std::lock_guard lock(g_mutex);
    if (g_decoders.erase(handle) == 0) return kErrorHandle;
    return SCE_AUDIODEC_OK;
}

int32_t sceAudiodecClearContext(int32_t handle) {
    std::lock_guard lock(g_mutex);
    const auto it = g_decoders.find(handle);
    if (it == g_decoders.end()) return kErrorHandle;

    uint8_t config[4];
    std::memcpy(config, it->second->context.configData, sizeof(config));
    auto& backend = AstroPort::AudioBackend::Get();
    backend.TermAt9Decoder(it->second->context);
    if (!backend.InitAt9Decoder(it->second->context, config, sizeof(config))) {
        return kErrorAt9Config;
    }
    return SCE_AUDIODEC_OK;
}

int32_t sceAudiodecDecode(int32_t handle, SceAudiodecCtrl* ctrl) {
    std::lock_guard lock(g_mutex);
    const auto it = g_decoders.find(handle);
    if (it == g_decoders.end()) return kErrorHandle;
    if (const int32_t validation = ValidateDecodeCtrl(ctrl);
        validation != SCE_AUDIODEC_OK) {
        return validation;
    }

    auto& context = it->second->context;
    const size_t pcmCapacitySamples =
        ctrl->pcmItem->dataSize / sizeof(int16_t);
    if (pcmCapacitySamples <
        static_cast<size_t>(context.samplesPerFrame) * context.channelCount) {
        return kErrorPcmSize;
    }

    uint32_t samplesOut = 0;
    uint32_t bytesConsumed = 0;
    if (!AstroPort::AudioBackend::Get().DecodeAt9Frame(
            context,
            static_cast<const uint8_t*>(ctrl->auInfo->address),
            ctrl->auInfo->dataSize,
            static_cast<int16_t*>(ctrl->pcmItem->address),
            pcmCapacitySamples,
            &samplesOut,
            &bytesConsumed)) {
        return kErrorApiFail;
    }

    ctrl->auInfo->dataSize = bytesConsumed;
    ctrl->pcmItem->dataSize =
        samplesOut * context.channelCount *
        static_cast<uint32_t>(sizeof(int16_t));
    auto* info = static_cast<SceAudiodecAt9Info*>(ctrl->bitstreamInfo);
    if (info && info->size == sizeof(SceAudiodecAt9Info)) {
        FillAt9Info(*info, context);
    }
    return SCE_AUDIODEC_OK;
}

} // namespace AstroPort::Stubs
