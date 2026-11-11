#pragma once

#include <cstdint>

namespace AstroPort::Stubs {

constexpr uint32_t SCE_AUDIODEC_CODEC_AT9 = 1;
constexpr int32_t SCE_AUDIODEC_WORD_LENGTH_16BIT = 16;
constexpr int32_t SCE_AUDIODEC_OK = 0;

struct SceAudiodecAuInfo {
    uint32_t size;
    void* address;
    uint32_t dataSize;
};

struct SceAudiodecPcmItem {
    uint32_t size;
    void* address;
    uint32_t dataSize;
};

struct SceAudiodecCtrl {
    void* params;
    void* bitstreamInfo;
    SceAudiodecAuInfo* auInfo;
    SceAudiodecPcmItem* pcmItem;
};

struct SceAudiodecAt9Params {
    uint32_t size;
    int32_t wordLength;
    uint8_t configData[4];
};

struct SceAudiodecAt9Info {
    uint32_t size;
    uint32_t channels;
    uint32_t bitrate;
    uint32_t sampleRate;
    uint32_t superframeSize;
    uint32_t framesPerSuperframe;
    uint32_t nextFrameSize;
    uint32_t samplesPerFrame;
    int32_t result;
};

int32_t sceAudiodecInitLibrary(uint32_t codecType);
int32_t sceAudiodecTermLibrary(uint32_t codecType);
int32_t sceAudiodecCreateDecoder(SceAudiodecCtrl* ctrl, uint32_t codecType);
int32_t sceAudiodecDeleteDecoder(int32_t handle);
int32_t sceAudiodecClearContext(int32_t handle);
int32_t sceAudiodecDecode(int32_t handle, SceAudiodecCtrl* ctrl);

} // namespace AstroPort::Stubs
