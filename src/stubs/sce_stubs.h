#pragma once

// ==============================================================================
// Stubs dos módulos SCE (libSceNp*, libSceSaveData, libSceNet, etc.)
// Cada função stub mantém assinatura idêntica à ABI do PS5, retornando SCE_OK
// ou um valor neutro que não trave a engine durante a inicialização.
// ==============================================================================

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "../audio/audio_backend.h"
#include "sce_audiodec.h"

namespace AstroPort::Stubs {

constexpr int32_t SCE_OK = 0;
constexpr int32_t SCE_NP_ERROR_NOT_INITIALIZED = 0x80550001;
constexpr int32_t SCE_NET_ERROR_NO_ENTRY = 0x80410004;
constexpr int32_t SCE_AUDIO_OUT_ERROR_INVALID_PARAMETER = -1;

struct SceAudioOutOutputParam {
    int32_t handle;
    int32_t padding;
    const void* pcmData;
};

static_assert(sizeof(SceAudioOutOutputParam) == 16);

// ------------------------------------------------------------------------------
// libSceNpManager.prx — Sistema de conta e autenticação PSN
// ------------------------------------------------------------------------------
inline int32_t sceNpManagerInit() {
    std::cout << "[Stub:NpManager] sceNpManagerInit -> Offline (bypass PSN)\n";
    return SCE_OK;
}
inline int32_t sceNpManagerGetNpId(void* npId) {
    // Preenche um NpId dummy para evitar crash no lobby
    if (npId) std::memset(npId, 0, 64);
    return SCE_OK;
}
inline int32_t sceNpManagerTerm() { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceNpTrophy2.prx — Sistema de Troféus
// ------------------------------------------------------------------------------
inline int32_t sceNpTrophy2Init()              { return SCE_OK; }
inline int32_t sceNpTrophy2CreateContext(void*, void*, uint32_t) { return SCE_OK; }
inline int32_t sceNpTrophy2UnlockTrophy(void*, int32_t, int32_t*) {
    std::cout << "[Stub:Trophy2] Troféu desbloqueado! (modo offline, sem sincronizar com PSN)\n";
    return SCE_OK;
}
inline int32_t sceNpTrophy2Term() { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceSaveData.native.prx — Dados salvos (redireciona para disco local)
// ------------------------------------------------------------------------------
inline int32_t sceSaveDataInitialize3() {
    std::filesystem::create_directories("./savedata");
    std::cout << "[Stub:SaveData] sceSaveDataInitialize3 -> Pasta './savedata' criada.\n";
    return SCE_OK;
}
inline int32_t sceSaveDataMount2(void* params, uint32_t* mountHandle) {
    if (mountHandle) *mountHandle = 1;
    return SCE_OK;
}
inline int32_t sceSaveDataUmount(uint32_t mountHandle) { return SCE_OK; }
inline int32_t sceSaveDataTerm() { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceNet.prx / libSceNetCtl.prx — Rede e conectividade
// Retorna OFFLINE para todos as consultas de estado da rede
// ------------------------------------------------------------------------------
inline int32_t sceNetInit()           { return SCE_OK; }
inline int32_t sceNetCtlInit()        { return SCE_OK; }
inline int32_t sceNetCtlGetState(int32_t* state) {
    if (state) *state = 3; // SCE_NET_CTL_STATE_DISCONNECTED
    return SCE_OK;
}
inline int32_t sceNetCtlTerm()        { return SCE_OK; }
inline int32_t sceNetTerm()           { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceHttp.prx / libSceHttp2.prx / libSceSsl.prx — HTTP e SSL
// ------------------------------------------------------------------------------
inline int32_t sceHttpInit(int32_t, int32_t, size_t) { return SCE_OK; }
inline int32_t sceHttp2Init(int32_t, int32_t, int32_t, size_t) { return SCE_OK; }
inline int32_t sceSslInit(size_t) { return SCE_OK; }
inline int32_t sceSslTerm() { return SCE_OK; }
inline int32_t sceHttpTerm() { return SCE_OK; }
inline int32_t sceHttp2Term() { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceVoiceChat.prx — Chat de voz
// ------------------------------------------------------------------------------
inline int32_t sceVoiceChatInit()     { return SCE_OK; }
inline int32_t sceVoiceChatTerm()     { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceGameLiveStreaming.prx — Transmissão ao vivo do jogo
// ------------------------------------------------------------------------------
inline int32_t sceGameLiveStreamingInitialize() { return SCE_OK; }
inline int32_t sceGameLiveStreamingTerminate()  { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceShare.prx / libSceSharePlay.prx — Share
// ------------------------------------------------------------------------------
inline int32_t sceShareInit()         { return SCE_OK; }
inline int32_t sceShareTerm()         { return SCE_OK; }
inline int32_t sceSharePlayInit()     { return SCE_OK; }
inline int32_t sceSharePlayTerm()     { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceRemoteplay.prx — Remote Play
// ------------------------------------------------------------------------------
inline int32_t sceRemoteplayInitialize() { return SCE_OK; }
inline int32_t sceRemoteplayTerminate()  { return SCE_OK; }

// ------------------------------------------------------------------------------
// libScePlayGo.prx — Streaming de instalação progressiva
// Stub retorna "instalação completa" (100%)
// ------------------------------------------------------------------------------
inline int32_t scePlayGoInitialize()   { return SCE_OK; }
inline int32_t scePlayGoGetLocus(void*, const uint32_t*, uint8_t*, uint32_t) {
    // Retorna LOCUS_LOCAL_FAST: dados disponíveis localmente
    return SCE_OK;
}
inline int32_t scePlayGoTerminate()    { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceAppContent.prx — DLC e conteúdo adicional
// Retorna "todos os DLCs disponíveis"
// ------------------------------------------------------------------------------
inline int32_t sceAppContentInitialize()  { return SCE_OK; }
inline int32_t sceAppContentAddcontMount(void*, void*) { return SCE_OK; }
inline int32_t sceAppContentTerminate()   { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceNpGameIntent.prx / libSceNpSessionSignaling.prx — Intents e Sessões
// ------------------------------------------------------------------------------
inline int32_t sceNpGameIntentInitialize()  { return SCE_OK; }
inline int32_t sceNpSessionSignalingInit()  { return SCE_OK; }
inline int32_t sceNpSessionSignalingTerm()  { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceCommonDialog.prx / libSceMsgDialog.native.prx — Diálogos do Sistema
// Dialogo de mensagem do sistema: aceita e prossegue automaticamente
// ------------------------------------------------------------------------------
inline int32_t sceCommonDialogInitialize()  { return SCE_OK; }
inline int32_t sceMsgDialogInitialize()     { return SCE_OK; }
inline int32_t sceMsgDialogOpen(void*)      {
    std::cout << "[Stub:MsgDialog] Dialogo de sistema ignorado (modo PC offline).\n";
    return SCE_OK;
}
inline int32_t sceMsgDialogClose()          { return SCE_OK; }
inline int32_t sceMsgDialogTerm()           { return SCE_OK; }
inline int32_t sceCommonDialogTerm()        { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceSigninDialog.prx / libScePlayerInvitationDialog.prx — Entrar na conta
// ------------------------------------------------------------------------------
inline int32_t sceSigninDialogInitialize()          { return SCE_OK; }
inline int32_t scePlayerInvitationDialogInitialize(){ return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceIme.prx / libSceImeDialog.prx — Teclado virtual
// ------------------------------------------------------------------------------
inline int32_t sceImeDialogInit(void*)   {
    std::cout << "[Stub:ImeDialog] Teclado virtual nao suportado (modo PC: use teclado fisico).\n";
    return SCE_OK;
}

// ------------------------------------------------------------------------------
// libSceNpUniversalDataSystem.prx — Dados de usuário PSN
// ------------------------------------------------------------------------------
inline int32_t sceNpUniversalDataSystemInitialize()  { return SCE_OK; }
inline int32_t sceNpUniversalDataSystemTerm()        { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceAmpr.prx — Audio Memory Processing
// ------------------------------------------------------------------------------
inline int32_t sceAmprInitialize() { return SCE_OK; }
inline int32_t sceAmprTerminate()  { return SCE_OK; }

// ------------------------------------------------------------------------------
// libScePsml_debug.prx — PlayStation Machine Learning (debug)
// ------------------------------------------------------------------------------
inline int32_t scePsmlInitialize() { return SCE_OK; }
inline int32_t scePsmlTerminate()  { return SCE_OK; }

// ------------------------------------------------------------------------------
// libSceAudioOut.prx — Saída de Áudio WASAPI
// ------------------------------------------------------------------------------
inline int32_t sceAudioOutInit() { return SCE_OK; }
inline int32_t sceAudioOutOpen(int32_t userId, int32_t type, int32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    if (param != 1) return SCE_AUDIO_OUT_ERROR_INVALID_PARAMETER;
    AudioPortConfig cfg;
    cfg.sampleRate = freq;
    cfg.channels = 2;
    cfg.bufferFrames = len;
    return AudioBackend::Get().OpenPort(cfg);
}
inline int32_t sceAudioOutOutput(int32_t handle, const void* ptr) {
    const int32_t result =
        AudioBackend::Get().OutputPortData(static_cast<uint32_t>(handle), ptr);
    return result < 0 ? result : SCE_OK;
}
inline int32_t sceAudioOutClose(int32_t handle) {
    return AudioBackend::Get().ClosePort((uint32_t)handle);
}
inline int32_t sceAudioOutOutputs(const SceAudioOutOutputParam* params, uint32_t count) {
    if (count > 8 || (count != 0 && !params)) {
        return SCE_AUDIO_OUT_ERROR_INVALID_PARAMETER;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const int32_t result =
            AudioBackend::Get().OutputPortData(
                static_cast<uint32_t>(params[i].handle), params[i].pcmData);
        if (result < 0) return result;
    }
    return SCE_OK;
}
inline int32_t sceAudioOutSetVolume(int32_t handle, int32_t flags, const int32_t* volumes) {
    if (!volumes) return SCE_AUDIO_OUT_ERROR_INVALID_PARAMETER;
    (void)flags;
    return AudioBackend::Get().SetVolume(
        static_cast<uint32_t>(handle), static_cast<float>(volumes[0]) / 32768.0f);
}

} // namespace AstroPort::Stubs
