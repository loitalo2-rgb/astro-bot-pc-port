#pragma once

// ==============================================================================
// audio_backend.h — Subsistema de Áudio
// Traduz chamadas da libSceAudioOut.prx / libSceAudiodec.native.prx para
// Windows Audio Session API (WASAPI) via COM.
//
// Suporta:
//  - Decodificação de streams AT9 (ATRAC9) nativa do PS5
//  - Saída PCM estéreo (2ch), 5.1 (6ch) e 7.1 (8ch) via WASAPI
//  - Propagação espacial 3D (stub libSceAudioPropagation)
// ==============================================================================

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace AstroPort {

// Formatos de áudio suportados pelo PS5
enum class AudioFormat : uint8_t {
    PCM_S16_Mono     = 0,
    PCM_S16_Stereo   = 1,
    PCM_S16_71       = 2,
};

struct AudioPortConfig {
    AudioFormat format = AudioFormat::PCM_S16_Stereo;
    uint32_t    sampleRate = 48000;
    uint32_t    channels = 2;
    uint32_t    bufferFrames = 256;
};

struct At9DecodeContext {
    uint8_t  configData[4] = {};    // 4 bytes de configuração ATRAC9
    uint32_t channelCount = 0;
    uint32_t sampleRate = 0;
    uint32_t samplesPerFrame = 0;
    uint32_t frameBytes = 0;
    uint32_t superframeSize = 0;
    uint32_t framesPerSuperframe = 0;
    void*    decoderState = nullptr;
};

class AudioBackend {
public:
    static AudioBackend& Get();

    bool Initialize(const AudioPortConfig& config, uint32_t masterVolume);
    void Shutdown();

    // libSceAudioOut.prx stubs com implementação WASAPI real
    int32_t OpenPort(const AudioPortConfig& config);
    int32_t ClosePort(uint32_t portHandle);
    int32_t OutputData(uint32_t portHandle, const void* pcmData, uint32_t sizeBytes);
    int32_t OutputPortData(uint32_t portHandle, const void* pcmData);
    int32_t SetVolume(uint32_t portHandle, float volume);

    // libSceAudiodec.native.prx — Decodificação AT9
    bool InitAt9Decoder(At9DecodeContext& ctx, const uint8_t* configBytes, size_t configSize);
    void TermAt9Decoder(At9DecodeContext& ctx);
    bool DecodeAt9Frame(At9DecodeContext& ctx,
                        const uint8_t* compressedData, size_t compressedSize,
                        int16_t* pcmOut, size_t pcmCapacitySamples,
                        uint32_t* samplesOut, uint32_t* bytesConsumed);

    // libSceAudioPropagation.prx — Propagação 3D (simplificada)
    void SetListenerPosition(float x, float y, float z);
    void SetSourcePosition(uint32_t sourceId, float x, float y, float z);

    bool IsRunning() const { return m_running.load(); }

private:
    AudioBackend() = default;

    bool InitWASAPI(uint32_t sampleRate, uint32_t channels);
    void MixThread();

    bool              m_initialized = false;
    bool              m_comInitialized = false;
    std::atomic<bool> m_running{ false };
    std::thread       m_mixThread;
    std::mutex        m_bufferMutex;

    uint32_t m_sampleRate = 48000;
    uint32_t m_channels   = 2;
    uint32_t m_portBufferFrames = 256;
    bool m_portOpen = false;
    float    m_masterVol  = 1.0f;

    // Buffer de mixing interno (circular)
    std::vector<int16_t> m_mixBuffer;
    uint32_t m_writePos = 0;
    uint32_t m_readPos  = 0;

    // Posição 3D do listener (para propagação espacial)
    float m_listenerX = 0.0f, m_listenerY = 0.0f, m_listenerZ = 0.0f;

    // Handle WASAPI (opaque pointer para evitar incluir windows.h no header)
    void* m_wasapiDevice  = nullptr;
    void* m_wasapiClient  = nullptr;
    void* m_wasapiRender  = nullptr;
};

} // namespace AstroPort
