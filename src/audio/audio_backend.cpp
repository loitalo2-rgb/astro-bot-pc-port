#include "audio_backend.h"
#include <libatrac9.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace AstroPort {

AudioBackend& AudioBackend::Get() {
    static AudioBackend instance;
    return instance;
}

bool AudioBackend::InitWASAPI(uint32_t sampleRate, uint32_t channels) {
#ifdef _WIN32
    HRESULT hr;

    // Inicializa COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "[AudioBackend] CoInitializeEx falhou: 0x" << std::hex << hr << "\n";
        return false;
    }
    m_comInitialized = SUCCEEDED(hr);

    // Enumerador de dispositivos
    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] Falha ao criar MMDeviceEnumerator: 0x" << std::hex << hr << "\n";
        return false;
    }

    // Dispositivo padrão de saída
    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] Nenhum dispositivo de audio encontrado!\n";
        return false;
    }
    m_wasapiDevice = device;

    // Ativa o IAudioClient
    IAudioClient* audioClient = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] Falha ao ativar IAudioClient!\n";
        device->Release();
        m_wasapiDevice = nullptr;
        return false;
    }
    m_wasapiClient = audioClient;

    // Configura o formato de áudio PCM
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = (WORD)channels;
    wfx.nSamplesPerSec  = sampleRate;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize          = 0;

    // Buffer de 50ms
    REFERENCE_TIME bufDuration = 500000;
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                  bufDuration, 0, &wfx, nullptr);
    if (FAILED(hr)) {
        // Fallback: tenta sem event callback
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufDuration, 0, &wfx, nullptr);
    }
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] IAudioClient::Initialize falhou: 0x" << std::hex << hr << "\n";
        audioClient->Release();
        device->Release();
        m_wasapiClient = nullptr;
        m_wasapiDevice = nullptr;
        return false;
    }

    // Obtém IAudioRenderClient
    IAudioRenderClient* renderClient = nullptr;
    hr = audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient);
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] Falha ao obter IAudioRenderClient!\n";
        audioClient->Release();
        device->Release();
        m_wasapiClient = nullptr;
        m_wasapiDevice = nullptr;
        return false;
    }
    m_wasapiRender = renderClient;

    // Inicia o stream
    hr = audioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] IAudioClient::Start falhou: 0x"
                  << std::hex << hr << "\n";
        renderClient->Release();
        audioClient->Release();
        device->Release();
        m_wasapiRender = nullptr;
        m_wasapiClient = nullptr;
        m_wasapiDevice = nullptr;
        return false;
    }

    std::cout << "[AudioBackend] WASAPI inicializado: " << sampleRate << "Hz, "
              << channels << "ch, 16-bit PCM\n";
    return true;
#else
    std::cout << "[AudioBackend] WASAPI nao disponivel nesta plataforma.\n";
    return true;
#endif
}

bool AudioBackend::Initialize(const AudioPortConfig& config, uint32_t masterVolume) {
    if (m_initialized) return true;

    m_sampleRate = config.sampleRate;
    m_channels   = config.channels;
    m_masterVol  = masterVolume / 100.0f;

    // Buffer de mixing: 1 segundo de audio
    m_mixBuffer.resize(m_sampleRate * m_channels, 0);

    if (!InitWASAPI(m_sampleRate, m_channels)) {
        std::cerr << "[AudioBackend] Aviso: WASAPI falhou — audio em modo silencioso.\n";
    }

    m_initialized = true;
    m_running.store(true);

    std::cout << "[AudioBackend] Sub-sistema de audio pronto.\n";
    std::cout << "  -> Taxa de Amostragem: " << m_sampleRate << " Hz\n";
    std::cout << "  -> Canais: " << m_channels << "\n";
    std::cout << "  -> Volume Mestre: " << masterVolume << "%\n";
    return true;
}

void AudioBackend::Shutdown() {
    if (!m_initialized) return;
    m_running.store(false);
    if (m_mixThread.joinable()) m_mixThread.join();
    m_portOpen = false;

#ifdef _WIN32
    if (m_wasapiRender)  { reinterpret_cast<IAudioRenderClient*>(m_wasapiRender)->Release(); m_wasapiRender = nullptr; }
    if (m_wasapiClient)  { reinterpret_cast<IAudioClient*>(m_wasapiClient)->Stop();
                           reinterpret_cast<IAudioClient*>(m_wasapiClient)->Release(); m_wasapiClient = nullptr; }
    if (m_wasapiDevice)  { reinterpret_cast<IMMDevice*>(m_wasapiDevice)->Release(); m_wasapiDevice = nullptr; }
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
#endif

    m_initialized = false;
    std::cout << "[AudioBackend] Sub-sistema de audio encerrado.\n";
}

int32_t AudioBackend::OpenPort(const AudioPortConfig& config) {
    if (m_portOpen || config.channels != m_channels ||
        config.sampleRate != m_sampleRate) {
        std::cerr << "[AudioBackend] Formato da porta incompatível com WASAPI.\n";
        return -1;
    }
    m_portBufferFrames = config.bufferFrames ? config.bufferFrames : 256;
    m_portOpen = true;
    std::cout << "[AudioBackend] sceAudioOutOpen: porta criada (rate=" 
              << config.sampleRate << ", ch=" << config.channels << ")\n";
    return 1; // handle fixo
}

int32_t AudioBackend::ClosePort(uint32_t portHandle) {
    if (portHandle != 1 || !m_portOpen) {
        std::cerr << "[AudioBackend] Tentativa de fechar uma porta de áudio inválida.\n";
        return -1;
    }
    m_portOpen = false;
    return 0;
}

int32_t AudioBackend::OutputData(uint32_t portHandle, const void* pcmData, uint32_t sizeBytes) {
    if (portHandle != 1 || !m_portOpen) {
        std::cerr << "[AudioBackend] Tentativa de saída em uma porta de áudio inválida.\n";
        return -1;
    }
    if (!pcmData) return 0;
    if (m_channels == 0 ||
        sizeBytes % (m_channels * sizeof(int16_t)) != 0) {
        std::cerr << "[AudioBackend] Buffer PCM não alinhado ao formato da porta.\n";
        return -1;
    }
    if (!m_wasapiRender || !m_wasapiClient) {
        std::cerr << "[AudioBackend] Saída PCM indisponível: WASAPI não está inicializado.\n";
        return -1;
    }

#ifdef _WIN32
    auto* renderClient = reinterpret_cast<IAudioRenderClient*>(m_wasapiRender);
    auto* audioClient  = reinterpret_cast<IAudioClient*>(m_wasapiClient);

    UINT32 bufferSize = 0, padding = 0;
    HRESULT hr = audioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] IAudioClient::GetBufferSize falhou: 0x"
                  << std::hex << hr << "\n";
        return -1;
    }
    hr = audioClient->GetCurrentPadding(&padding);
    if (FAILED(hr) || padding > bufferSize) {
        std::cerr << "[AudioBackend] IAudioClient::GetCurrentPadding falhou: 0x"
                  << std::hex << hr << "\n";
        return -1;
    }
    UINT32 available = bufferSize - padding;

    UINT32 framesNeeded = sizeBytes / (m_channels * sizeof(int16_t));
    if (framesNeeded == 0) return 0;
    if (available < framesNeeded) {
        std::cerr << "[AudioBackend] Buffer WASAPI sem espaço para todo o bloco PCM.\n";
        return -1;
    }
    UINT32 framesToWrite = std::min(available, framesNeeded);
    if (framesToWrite == 0) return 0;

    BYTE* outputBuf = nullptr;
    hr = renderClient->GetBuffer(framesToWrite, &outputBuf);
    if (FAILED(hr) || !outputBuf) {
        std::cerr << "[AudioBackend] IAudioRenderClient::GetBuffer falhou: 0x"
                  << std::hex << hr << "\n";
        return -1;
    }
    size_t bytesToCopy = framesToWrite * m_channels * sizeof(int16_t);

    if (m_masterVol < 1.0f) {
        const int16_t* src = reinterpret_cast<const int16_t*>(pcmData);
        int16_t* dst = reinterpret_cast<int16_t*>(outputBuf);
        for (size_t i = 0; i < bytesToCopy / sizeof(int16_t); i++) {
            dst[i] = static_cast<int16_t>(src[i] * m_masterVol);
        }
    } else {
        std::memcpy(outputBuf, pcmData, bytesToCopy);
    }
    hr = renderClient->ReleaseBuffer(framesToWrite, 0);
    if (FAILED(hr)) {
        std::cerr << "[AudioBackend] IAudioRenderClient::ReleaseBuffer falhou: 0x"
                  << std::hex << hr << "\n";
        return -1;
    }
    return 0;
#else
    return -1;
#endif
}

int32_t AudioBackend::OutputPortData(uint32_t portHandle, const void* pcmData) {
    if (portHandle != 1 || !m_portOpen) {
        std::cerr << "[AudioBackend] Tentativa de saída em uma porta de áudio inválida.\n";
        return -1;
    }
    const uint64_t sizeBytes = static_cast<uint64_t>(m_portBufferFrames) *
                               m_channels * sizeof(int16_t);
    if (sizeBytes > std::numeric_limits<uint32_t>::max()) return -1;
    return OutputData(portHandle, pcmData, static_cast<uint32_t>(sizeBytes));
}

int32_t AudioBackend::SetVolume(uint32_t portHandle, float volume) {
    if (portHandle != 1 || !m_portOpen) {
        std::cerr << "[AudioBackend] Tentativa de ajustar o volume de uma porta inválida.\n";
        return -1;
    }
    m_masterVol = std::clamp(volume, 0.0f, 1.0f);
    return 0;
}

bool AudioBackend::InitAt9Decoder(At9DecodeContext& ctx, const uint8_t* configBytes, size_t configSize) {
    if (!configBytes || configSize != ATRAC9_CONFIG_DATA_SIZE) {
        std::cerr << "[AudioBackend] Configuração ATRAC9 inválida: esperados 4 bytes.\n";
        return false;
    }

    TermAt9Decoder(ctx);
    auto* decoder = Atrac9GetHandle();
    if (!decoder) {
        std::cerr << "[AudioBackend] Não foi possível alocar o decoder ATRAC9.\n";
        return false;
    }

    uint8_t config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, configBytes, sizeof(config));
    if (Atrac9InitDecoder(decoder, config) != 0) {
        Atrac9ReleaseHandle(decoder);
        std::cerr << "[AudioBackend] LibAtrac9 rejeitou a configuração do stream.\n";
        return false;
    }

    Atrac9CodecInfo info = {};
    if (Atrac9GetCodecInfo(decoder, &info) != 0 ||
        info.channels <= 0 || info.channels > 8 ||
        info.samplingRate <= 0 || info.frameSamples <= 0 ||
        info.superframeSize <= 0 ||
        info.framesInSuperframe <= 0 ||
        info.superframeSize % info.framesInSuperframe != 0) {
        Atrac9ReleaseHandle(decoder);
        std::cerr << "[AudioBackend] LibAtrac9 retornou metadados inválidos.\n";
        return false;
    }

    ctx.decoderState = decoder;
    ctx.channelCount = static_cast<uint32_t>(info.channels);
    ctx.sampleRate = static_cast<uint32_t>(info.samplingRate);
    ctx.samplesPerFrame = static_cast<uint32_t>(info.frameSamples);
    ctx.superframeSize = static_cast<uint32_t>(info.superframeSize);
    ctx.framesPerSuperframe = static_cast<uint32_t>(info.framesInSuperframe);
    ctx.frameBytes = ctx.superframeSize / ctx.framesPerSuperframe;
    std::memcpy(ctx.configData, config, sizeof(ctx.configData));

    std::cout << "[AudioBackend] AT9 decoder init (config: "
              << std::hex << (int)ctx.configData[0] << " "
              << (int)ctx.configData[1] << " "
              << (int)ctx.configData[2] << " "
              << (int)ctx.configData[3] << std::dec << ", "
              << ctx.channelCount << "ch, " << ctx.samplesPerFrame
              << " samples/frame)\n";
    return true;
}

void AudioBackend::TermAt9Decoder(At9DecodeContext& ctx) {
    if (ctx.decoderState) {
        Atrac9ReleaseHandle(ctx.decoderState);
    }
    ctx = {};
}

bool AudioBackend::DecodeAt9Frame(At9DecodeContext& ctx,
                                  const uint8_t* compressedData, size_t compressedSize,
                                  int16_t* pcmOut, size_t pcmCapacitySamples,
                                  uint32_t* samplesOut, uint32_t* bytesConsumed) {
    if (!ctx.decoderState || !compressedData || !pcmOut || !samplesOut ||
        !bytesConsumed ||
        ctx.frameBytes == 0 || compressedSize < ctx.frameBytes) {
        return false;
    }

    const size_t requiredSamples =
        static_cast<size_t>(ctx.samplesPerFrame) * ctx.channelCount;
    if (requiredSamples == 0 || pcmCapacitySamples < requiredSamples ||
        requiredSamples > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    int bytesUsed = 0;
    const int result = Atrac9Decode(
        ctx.decoderState, compressedData, pcmOut, &bytesUsed);
    if (result != 0 || bytesUsed <= 0 ||
        static_cast<uint32_t>(bytesUsed) > ctx.frameBytes) {
        std::cerr << "[AudioBackend] Falha ao decodificar frame ATRAC9 (status="
                  << result << ", bytes=" << bytesUsed << ").\n";
        return false;
    }

    *samplesOut = ctx.samplesPerFrame;
    *bytesConsumed = static_cast<uint32_t>(bytesUsed);
    return true;
}

void AudioBackend::SetListenerPosition(float x, float y, float z) {
    m_listenerX = x; m_listenerY = y; m_listenerZ = z;
}

void AudioBackend::SetSourcePosition(uint32_t sourceId, float x, float y, float z) {
    // Calcula atenuação por distância para propagação espacial simplificada
    float dx = x - m_listenerX, dy = y - m_listenerY, dz = z - m_listenerZ;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    float attenuation = 1.0f / (1.0f + dist * 0.1f);
    (void)attenuation; // será usado no mixing futuro
}

} // namespace AstroPort
