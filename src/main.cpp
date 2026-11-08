// ==============================================================================
// main.cpp — Ponto de Entrada do ASTRO BOT PC Port
// Fase 6: Tela de Inicialização Interativa, Configurações de Controle e Gráficos,
//         Medidor de FPS em Tempo Real e Visualizador 3D do Astro Bot
// ==============================================================================

#include "core/dispatch_table.h"
#include "core/eboot_loader.h"
#include "config/config_manager.h"
#include "kernel/kernel_emulator.h"
#include "gpu/vulkan_backend.h"
#include "gpu/shader_recompiler.h"
#include "input/input_manager.h"
#include "gameplay/player_movement.h"
#include "gameplay/level_collision.h"
#include "common/software_rasterizer.h"
#include "patches/patch_manager.h"
#include "assets/asset_loader.h"
#include "audio/audio_backend.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace AstroPort;

static constexpr const char* kConfigPath = "./config/settings.ini";

static fs::path ExecutableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length > 0) {
        return fs::path(std::string(buffer, length)).parent_path();
    }
#endif
    return fs::current_path();
}

static std::string ResolveGameDataPath(const AppConfig& cfg) {
    const fs::path exeDir = ExecutableDirectory();
    std::vector<fs::path> candidates;

    if (!cfg.gameDataPath.empty()) {
        fs::path configured(cfg.gameDataPath);
        if (configured.is_relative()) configured = exeDir / configured;
        candidates.push_back(configured);
    }

    candidates.push_back(exeDir / "data");
    candidates.push_back(exeDir / "game" / "data");
    candidates.push_back(exeDir.parent_path() / "data");

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate / "prein", ec)) {
            const fs::path canonical = fs::weakly_canonical(candidate, ec);
            return ec ? candidate.string() : canonical.string();
        }
    }

    return cfg.gameDataPath;
}

// Estado da Interface
enum class ActiveTab {
    Viewport3D = 0,
    GraphicsSettings = 1,
    ControllerSettings = 2,
    LevelsAndVideos = 3
};

static ActiveTab g_currentTab = ActiveTab::Viewport3D;
static HWND g_hwnd = nullptr;
static HWND g_vkChild = nullptr;
static bool g_running = true;
static bool g_fullscreen = false;
static RECT g_prevWindowRect = {};
static DWORD g_prevWindowStyle = 0;

// Métricas de FPS e Performance
static float g_fps = 60.0f;
static float g_frametimeMs = 16.6f;
static float g_paintFps = 0.0f;
static float g_softwareSceneMs = 0.0f;
static uint64_t g_frameCount = 0;
static uint64_t g_perfPaintCount = 0;
static uint64_t g_perfSceneCount = 0;
static double g_perfPaintTotalMs = 0.0;
static double g_perfSceneTotalMs = 0.0;
static float g_fpsHistory[60] = {};
static int g_fpsHistIndex = 0;
#ifdef _WIN32
static HDC g_backbufferDC = nullptr;
static HBITMAP g_backbufferBitmap = nullptr;
static HBITMAP g_backbufferOriginalBitmap = nullptr;
static int g_backbufferWidth = 0;
static int g_backbufferHeight = 0;
static HFONT g_uiFontTitle = nullptr;
static HFONT g_uiFontSub = nullptr;
static HFONT g_uiFontFps = nullptr;
static HFONT g_uiFontMs = nullptr;
static HFONT g_uiFontHead = nullptr;
static HFONT g_uiFontBody = nullptr;
#endif

// Ângulos e Parâmetros 3D do Astro Bot
static float g_modelRotY = 0.0f;
static float g_modelRotX = 0.15f;
static float g_modelZoom = 1.0f;
static float g_playerX = 0.0f;
static float g_playerZ = 0.0f;
static float g_playerSpawnX = 0.0f;
static float g_playerSpawnZ = 0.0f;
static float g_playerGroundY = 0.0f;
static float g_playerMoveRangeX = 0.0f;
static float g_playerMoveRangeZ = 0.0f;
static float g_playerSupportRadius = 0.08f;
static bool g_playerSpawnValid = false;
static bool g_playerOnGround = false;
static float g_jumpHeight = 0.0f;
static float g_jumpVelocity = 0.0f;
static float g_attackTimer = 0.0f;
static std::ofstream g_logFile;
static JxmdMesh g_levelMesh;
static LevelCollisionMesh g_levelCollision;
static bool g_levelLoaded = false;
static int g_levelTriCount = 0;

static float GetPlayerCollisionHeight(const JxmdMesh* playerMesh) {
    if (!playerMesh || !playerMesh->valid) return 0.75f;

    const float playerExtent = std::max(
        playerMesh->bboxMax[0] - playerMesh->bboxMin[0],
        std::max(playerMesh->bboxMax[1] - playerMesh->bboxMin[1],
                 playerMesh->bboxMax[2] - playerMesh->bboxMin[2]));
    const float levelExtent = std::max(
        g_levelMesh.bboxMax[0] - g_levelMesh.bboxMin[0],
        std::max(g_levelMesh.bboxMax[1] - g_levelMesh.bboxMin[1],
                 g_levelMesh.bboxMax[2] - g_levelMesh.bboxMin[2]));
    if (!std::isfinite(playerExtent) || !std::isfinite(levelExtent) ||
        playerExtent <= 1e-6f || levelExtent <= 0.0f) {
        return 0.75f;
    }

    const float playerScale = (levelExtent * 0.06f) / playerExtent;
    const float bodyHeight =
        (playerMesh->bboxMax[1] - playerMesh->bboxMin[1]) * playerScale;
    return std::isfinite(bodyHeight) && bodyHeight > 0.0f
        ? bodyHeight
        : 0.75f;
}

// Logger duplo (Console + Arquivo)
static void LogMessage(const std::string& msg) {
    std::cout << msg << std::endl;
    if (g_logFile.is_open()) {
        g_logFile << msg << std::endl;
        g_logFile.flush();
    }
}

// Calcula o retangulo interno do viewport 3D dentro da janela principal.
static void ComputeVulkanViewportRect(int width, int height, RECT& out) {
    int vpW = (width - 70) * 3 / 5;
    int left = 31;
    int top = 172;                 // abaixo dos rotulos GDI do viewport
    int right = 30 + vpW - 1;
    int bottom = (height - 70) - 1;
    if (right <= left) right = left + 1;
    if (bottom <= top) bottom = top + 1;
    out = { left, top, right, bottom };
}

// Reposiciona o HWND filho do Vulkan e redimensiona o swapchain.
static void UpdateVulkanViewport() {
    if (!g_hwnd || !g_vkChild) return;

    RECT client;
    GetClientRect(g_hwnd, &client);
    RECT vr;
    ComputeVulkanViewportRect(client.right, client.bottom, vr);

    const bool visible = (g_currentTab == ActiveTab::Viewport3D &&
                          !g_levelLoaded && client.right > 0 && client.bottom > 0);
    const int cw = vr.right - vr.left;
    const int ch = vr.bottom - vr.top;

    ShowWindow(g_vkChild, visible ? SW_SHOW : SW_HIDE);
    SetWindowPos(g_vkChild, HWND_TOP, vr.left, vr.top, cw, ch, SWP_NOACTIVATE);

    if (visible) {
        VulkanBackend::Get().Resize((uint32_t)cw, (uint32_t)ch);
    }
}

static bool EnsureUiFonts() {
    if (g_uiFontTitle && g_uiFontSub && g_uiFontFps && g_uiFontMs &&
        g_uiFontHead && g_uiFontBody) {
        return true;
    }

    g_uiFontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_uiFontSub = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_uiFontFps = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
    g_uiFontMs = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
    g_uiFontHead = CreateFontA(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_uiFontBody = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");

    if (g_uiFontTitle && g_uiFontSub && g_uiFontFps && g_uiFontMs &&
        g_uiFontHead && g_uiFontBody) {
        return true;
    }

    LogMessage("[UI] Falha ao criar as fontes da interface.");
    if (g_uiFontTitle) DeleteObject(g_uiFontTitle);
    if (g_uiFontSub) DeleteObject(g_uiFontSub);
    if (g_uiFontFps) DeleteObject(g_uiFontFps);
    if (g_uiFontMs) DeleteObject(g_uiFontMs);
    if (g_uiFontHead) DeleteObject(g_uiFontHead);
    if (g_uiFontBody) DeleteObject(g_uiFontBody);
    g_uiFontTitle = nullptr;
    g_uiFontSub = nullptr;
    g_uiFontFps = nullptr;
    g_uiFontMs = nullptr;
    g_uiFontHead = nullptr;
    g_uiFontBody = nullptr;
    return false;
}

static void DestroyUiFonts() {
    if (g_uiFontTitle) DeleteObject(g_uiFontTitle);
    if (g_uiFontSub) DeleteObject(g_uiFontSub);
    if (g_uiFontFps) DeleteObject(g_uiFontFps);
    if (g_uiFontMs) DeleteObject(g_uiFontMs);
    if (g_uiFontHead) DeleteObject(g_uiFontHead);
    if (g_uiFontBody) DeleteObject(g_uiFontBody);
    g_uiFontTitle = nullptr;
    g_uiFontSub = nullptr;
    g_uiFontFps = nullptr;
    g_uiFontMs = nullptr;
    g_uiFontHead = nullptr;
    g_uiFontBody = nullptr;
}

static int ParsePerfBenchmarkSeconds(const char* commandLine) {
    if (!commandLine) return 0;
    constexpr char option[] = "--perf-seconds=";
    const std::string args(commandLine);
    const size_t optionPos = args.find(option);
    if (optionPos == std::string::npos) return 0;

    const char* valueStart = args.c_str() + optionPos + sizeof(option) - 1;
    char* valueEnd = nullptr;
    const long seconds = std::strtol(valueStart, &valueEnd, 10);
    if (valueEnd == valueStart ||
        (*valueEnd != '\0' && *valueEnd != ' ' && *valueEnd != '\t') ||
        seconds < 5 || seconds > 300) {
        throw std::invalid_argument(
            "--perf-seconds must be an integer from 5 through 300.");
    }
    return static_cast<int>(seconds);
}

// Procedimento de Janela com Double-Buffered Painting
static LRESULT CALLBACK AstroWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                g_running = false;
                PostQuitMessage(0);
                return 0;
            }
            if (wParam == VK_F11) {
                g_fullscreen = !g_fullscreen;
                if (g_fullscreen) {
                    GetWindowRect(hwnd, &g_prevWindowRect);
                    g_prevWindowStyle = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
                    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                    int scrW = GetSystemMetrics(SM_CXSCREEN);
                    int scrH = GetSystemMetrics(SM_CYSCREEN);
                    SetWindowPos(hwnd, HWND_TOP, 0, 0, scrW, scrH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                } else {
                    SetWindowLongPtr(hwnd, GWL_STYLE, g_prevWindowStyle);
                    SetWindowPos(hwnd, HWND_NOTOPMOST,
                        g_prevWindowRect.left, g_prevWindowRect.top,
                        g_prevWindowRect.right - g_prevWindowRect.left,
                        g_prevWindowRect.bottom - g_prevWindowRect.top,
                        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }
                return 0;
            }
            bool tabChanged = false;
            if (wParam == '1') { g_currentTab = ActiveTab::Viewport3D; tabChanged = true; }
            if (wParam == '2') { g_currentTab = ActiveTab::GraphicsSettings; tabChanged = true; }
            if (wParam == '3') { g_currentTab = ActiveTab::ControllerSettings; tabChanged = true; }
            if (wParam == '4') { g_currentTab = ActiveTab::LevelsAndVideos; tabChanged = true; }
            if (wParam == VK_TAB) {
                int next = ((int)g_currentTab + 1) % 4;
                g_currentTab = (ActiveTab)next;
                tabChanged = true;
            }
            if (tabChanged) UpdateVulkanViewport();
            if (wParam == VK_SPACE) {
                // Pulo do Astro
                if (g_jumpHeight <= 0.01f &&
                    (!g_levelLoaded || g_playerOnGround)) {
                    g_jumpVelocity = 4.0f;
                    g_playerOnGround = false;
                }
            }
            break;
        }

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                UpdateVulkanViewport();
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            const auto paintStart = std::chrono::steady_clock::now();
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!EnsureUiFonts()) {
                EndPaint(hwnd, &ps);
                return 0;
            }

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int width  = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            if (!g_backbufferDC) {
                g_backbufferDC = CreateCompatibleDC(hdc);
                if (!g_backbufferDC) {
                    LogMessage("[UI] Falha ao criar DC do backbuffer.");
                    EndPaint(hwnd, &ps);
                    return 0;
                }
            }
            if (!g_backbufferBitmap || g_backbufferWidth != width ||
                g_backbufferHeight != height) {
                HBITMAP newBitmap = CreateCompatibleBitmap(hdc, width, height);
                if (!newBitmap) {
                    LogMessage("[UI] Falha ao criar bitmap do backbuffer.");
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                if (g_backbufferBitmap) {
                    SelectObject(g_backbufferDC, g_backbufferOriginalBitmap);
                    DeleteObject(g_backbufferBitmap);
                    g_backbufferBitmap = nullptr;
                    g_backbufferWidth = 0;
                    g_backbufferHeight = 0;
                }
                HGDIOBJ originalBitmap =
                    SelectObject(g_backbufferDC, newBitmap);
                if (!originalBitmap || originalBitmap == HGDI_ERROR) {
                    DeleteObject(newBitmap);
                    LogMessage("[UI] Falha ao selecionar bitmap do backbuffer.");
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                g_backbufferBitmap = newBitmap;
                g_backbufferOriginalBitmap =
                    static_cast<HBITMAP>(originalBitmap);
                g_backbufferWidth = width;
                g_backbufferHeight = height;
            }
            HDC memDC = g_backbufferDC;

            // Fundo espacial temático do ASTRO BOT
            HBRUSH bgBrush = CreateSolidBrush(RGB(10, 16, 32));
            FillRect(memDC, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            SetBkMode(memDC, TRANSPARENT);

            // =================================================================
            // CABEÇALHO & MEDIDOR DE FPS EM TEMPO REAL
            // =================================================================
            HFONT oldFont = (HFONT)SelectObject(memDC, g_uiFontTitle);

            SetTextColor(memDC, RGB(0, 210, 255)); // Azul ciano brilhante Astro
            TextOutA(memDC, 30, 18, "ASTRO BOT", 9);

            SelectObject(memDC, g_uiFontSub);
            SetTextColor(memDC, RGB(160, 185, 215));
            TextOutA(memDC, 165, 25, "PC Recompilation | PlayStation 5 (PPSA21567) | Team ASOBI", 58);

            // Painel do Medidor de FPS (Canto Superior Direito)
            RECT fpsBox = { width - 290, 14, width - 30, 60 };
            HBRUSH fpsBoxBrush = CreateSolidBrush(RGB(18, 28, 54));
            FillRect(memDC, &fpsBox, fpsBoxBrush);
            DeleteObject(fpsBoxBrush);

            HPEN fpsBorder = CreatePen(PS_SOLID, 1, RGB(40, 75, 130));
            HPEN oldPen = (HPEN)SelectObject(memDC, fpsBorder);
            Rectangle(memDC, fpsBox.left, fpsBox.top, fpsBox.right, fpsBox.bottom);

            // FPS em destaque verde neon
            SelectObject(memDC, g_uiFontFps);
            SetTextColor(memDC, (g_fps >= 55.0f) ? RGB(40, 255, 120) : RGB(255, 180, 40));
            
            char bufFps[32];
            snprintf(bufFps, sizeof(bufFps), "Loop %4.1f", g_fps);
            TextOutA(memDC, width - 280, 22, bufFps, (int)strlen(bufFps));

            // Frametime em ms
            SelectObject(memDC, g_uiFontMs);
            SetTextColor(memDC, RGB(160, 200, 240));
            char bufMs[32];
            snprintf(bufMs, sizeof(bufMs), "%4.1f ms", g_frametimeMs);
            TextOutA(memDC, width - 125, 20, bufMs, (int)strlen(bufMs));

            SetTextColor(memDC, RGB(100, 230, 255));
            char paintFpsText[32];
            snprintf(paintFpsText, sizeof(paintFpsText), "Paint: %4.1f FPS",
                     g_paintFps);
            TextOutA(memDC, width - 155, 38, paintFpsText,
                     static_cast<int>(strlen(paintFpsText)));

            // Gráfico histórico de FPS (Sparkline no painel)
            SetDCBrushColor(memDC, RGB(0, 210, 255));
            for (int i = 0; i < 50; i++) {
                int idx = (g_fpsHistIndex - 50 + i + 60) % 60;
                float val = g_fpsHistory[idx];
                int barHeight = (int)((val / 65.0f) * 18.0f);
                if (barHeight > 20) barHeight = 20;
                if (barHeight < 2) barHeight = 2;

                RECT barRect = {
                    (width - 80) + (i),
                    54 - barHeight,
                    (width - 80) + (i + 1),
                    54
                };
                FillRect(memDC, &barRect,
                         reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            }

            // =================================================================
            // BARRA DE NAVEGAÇÃO DE ABAS
            // =================================================================
            HPEN divPen = CreatePen(PS_SOLID, 1, RGB(35, 55, 90));
            SelectObject(memDC, divPen);
            MoveToEx(memDC, 30, 72, nullptr);
            LineTo(memDC, width - 30, 72);

            const char* tabNames[4] = {
                "[1] 3D ASTRO BOT (AO VIVO)",
                "[2] CONFIGURAÇÕES GRÁFICAS",
                "[3] CONTROLE DE XBOX",
                "[4] NÍVEIS & ASSETS PS5"
            };

            int tabStartX = 30;
            for (int t = 0; t < 4; t++) {
                bool isSel = ((int)g_currentTab == t);
                int tabWidth = (width - 70) / 4;
                RECT tabRect = { tabStartX + t * tabWidth, 78, tabStartX + (t + 1) * tabWidth - 8, 108 };

                HBRUSH tBrush = CreateSolidBrush(isSel ? RGB(0, 110, 210) : RGB(18, 30, 52));
                FillRect(memDC, &tabRect, tBrush);
                DeleteObject(tBrush);

                SelectObject(memDC, g_uiFontSub);
                SetTextColor(memDC, isSel ? RGB(255, 255, 255) : RGB(150, 175, 205));
                DrawTextA(memDC, tabNames[t], -1, &tabRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // =================================================================
            // RENDERIZAÇÃO DO CONTEÚDO DAS ABAS
            // =================================================================
            // --- ABA 1: VISUALIZADOR 3D DO ASTRO BOT (CENÁRIO REAL) ---
            if (g_currentTab == ActiveTab::Viewport3D) {
                // Painel de Viewport 3D
                int vpW = (width - 70) * 3 / 5;
                RECT vpRect = { 30, 120, 30 + vpW, height - 70 };
                HBRUSH vpBg = CreateSolidBrush(RGB(6, 10, 22));
                FillRect(memDC, &vpRect, vpBg);
                DeleteObject(vpBg);

                HBRUSH vpBorder = CreateSolidBrush(RGB(0, 140, 230));
                FrameRect(memDC, &vpRect, vpBorder);
                DeleteObject(vpBorder);
                const int viewportClip = SaveDC(memDC);
                if (viewportClip != 0) {
                    IntersectClipRect(memDC, vpRect.left + 1, vpRect.top + 1,
                                      vpRect.right, vpRect.bottom);
                }

                const auto* pMesh = g_levelLoaded
                    ? &g_levelMesh
                    : AssetLoader::Get().GetCachedAstroMesh();

                // Renderiza o cenário ou o Astro Bot 3D
                int centerX = vpRect.left + vpW / 2;
                int centerY = vpRect.top + (vpRect.bottom - vpRect.top) / 2 +
                    (g_levelLoaded ? 0 : (int)(g_jumpHeight * 120.0f));
                const int renderWidth = vpW;
                const int renderHeight = vpRect.bottom - vpRect.top;
                const auto softwareSceneStart = std::chrono::steady_clock::now();
                const COLORREF viewportBackground = RGB(6, 10, 22);
                static std::vector<uint32_t> scenePixels;
                static std::vector<float> sceneDepth;
                const size_t renderPixelCount =
                    static_cast<size_t>(renderWidth) *
                    static_cast<size_t>(renderHeight);
                scenePixels.resize(renderPixelCount);
                sceneDepth.resize(renderPixelCount);
                const uint32_t dibBackground =
                    ((viewportBackground & 0x000000FFu) << 16) |
                    (viewportBackground & 0x0000FF00u) |
                    ((viewportBackground & 0x00FF0000u) >> 16);
                std::fill(scenePixels.begin(), scenePixels.end(), dibBackground);
                std::fill(sceneDepth.begin(), sceneDepth.end(),
                          -std::numeric_limits<float>::infinity());
                float scale = 140.0f * g_modelZoom;
                float bboxCenterX = 0.0f;
                float bboxCenterY = 0.0f;
                float bboxCenterZ = 0.0f;
                float projectionCameraDistance = 4.0f;

                if (pMesh && pMesh->valid) {
                    bboxCenterX = (pMesh->bboxMin[0] + pMesh->bboxMax[0]) * 0.5f;
                    bboxCenterY = (pMesh->bboxMin[1] + pMesh->bboxMax[1]) * 0.5f;
                    bboxCenterZ = (pMesh->bboxMin[2] + pMesh->bboxMax[2]) * 0.5f;

                    const float extentX = pMesh->bboxMax[0] - pMesh->bboxMin[0];
                    const float extentY = pMesh->bboxMax[1] - pMesh->bboxMin[1];
                    const float extentZ = pMesh->bboxMax[2] - pMesh->bboxMin[2];
                    const float maxExtent = std::max(1.0f, std::max(extentX, std::max(extentY, extentZ)));
                    const float viewportSize = (float)std::min<int>(vpW, (int)(vpRect.bottom - vpRect.top));
                    scale = (viewportSize * 0.48f / maxExtent) * g_modelZoom;
                    projectionCameraDistance = maxExtent * 2.5f + 1.0f;

                    // Projeção em perspectiva com distância positiva para evitar inversões.
                    static std::vector<POINT> projPts;
                    static std::vector<float> projectedDepth;
                    projPts.resize(pMesh->vertices.size());
                    projectedDepth.resize(pMesh->vertices.size());
                    for (size_t i = 0; i < pMesh->vertices.size(); i++) {
                        const auto& v = pMesh->vertices[i];
                        float px = v.x - bboxCenterX;
                        float py = v.y - bboxCenterY;
                        float pz = v.z - bboxCenterZ;

                        // Rotação Y
                        float x1 = px * cosf(g_modelRotY) - pz * sinf(g_modelRotY);
                        float z1 = px * sinf(g_modelRotY) + pz * cosf(g_modelRotY);
                        // Rotação X
                        float y2 = py * cosf(g_modelRotX) - z1 * sinf(g_modelRotX);
                        float z2 = py * sinf(g_modelRotX) + z1 * cosf(g_modelRotX);

                        const float denominator = projectionCameraDistance + z2;
                        const float dist = projectionCameraDistance /
                            std::max(projectionCameraDistance * 0.1f, denominator);

                        projPts[i].x = centerX + (int)(x1 * scale * dist);
                        projPts[i].y = centerY - (int)(y2 * scale * dist);
                        projectedDepth[i] = z2;
                    }

                    if (g_levelLoaded) {
                        struct ShadedTriangle {
                            uint32_t i0;
                            uint32_t i1;
                            uint32_t i2;
                            float depth;
                            COLORREF color;
                        };

                        static std::vector<ShadedTriangle> triangles;
                        triangles.clear();
                        triangles.reserve(pMesh->indices.size() / 3);
                        constexpr float lightX = -0.35f;
                        constexpr float lightY = 0.78f;
                        constexpr float lightZ = -0.51f;
                        for (size_t i = 0; i + 2 < pMesh->indices.size(); i += 3) {
                            const uint32_t i0 = pMesh->indices[i];
                            const uint32_t i1 = pMesh->indices[i + 1];
                            const uint32_t i2 = pMesh->indices[i + 2];
                            if (i0 >= projPts.size() || i1 >= projPts.size() ||
                                i2 >= projPts.size()) {
                                continue;
                            }

                            const JxmdVertex& a = pMesh->vertices[i0];
                            const JxmdVertex& b = pMesh->vertices[i1];
                            const JxmdVertex& c = pMesh->vertices[i2];
                            const float abX = b.x - a.x;
                            const float abY = b.y - a.y;
                            const float abZ = b.z - a.z;
                            const float acX = c.x - a.x;
                            const float acY = c.y - a.y;
                            const float acZ = c.z - a.z;
                            const float normalX = abY * acZ - abZ * acY;
                            const float normalY = abZ * acX - abX * acZ;
                            const float normalZ = abX * acY - abY * acX;
                            const float normalLength = std::sqrt(
                                normalX * normalX + normalY * normalY +
                                normalZ * normalZ);
                            if (!std::isfinite(normalLength) ||
                                normalLength <= 1e-8f) {
                                continue;
                            }

                            const float lighting = std::clamp(
                                0.30f + 0.70f *
                                    std::abs((normalX * lightX +
                                              normalY * lightY +
                                              normalZ * lightZ) / normalLength),
                                0.0f, 1.0f);
                            const BYTE red = static_cast<BYTE>(12.0f + 25.0f * lighting);
                            const BYTE green = static_cast<BYTE>(26.0f + 72.0f * lighting);
                            const BYTE blue = static_cast<BYTE>(46.0f + 112.0f * lighting);
                            triangles.push_back({
                                i0, i1, i2,
                                (projectedDepth[i0] + projectedDepth[i1] +
                                 projectedDepth[i2]) / 3.0f,
                                RGB(red, green, blue)
                            });
                        }

                        for (const ShadedTriangle& triangle : triangles) {
                            const SoftwareRasterVertex face[3] = {
                                { static_cast<float>(projPts[triangle.i0].x - vpRect.left),
                                  static_cast<float>(projPts[triangle.i0].y - vpRect.top),
                                  1.0f / std::max(
                                    projectionCameraDistance * 0.1f,
                                    projectionCameraDistance +
                                        projectedDepth[triangle.i0]) },
                                { static_cast<float>(projPts[triangle.i1].x - vpRect.left),
                                  static_cast<float>(projPts[triangle.i1].y - vpRect.top),
                                  1.0f / std::max(
                                    projectionCameraDistance * 0.1f,
                                    projectionCameraDistance +
                                        projectedDepth[triangle.i1]) },
                                { static_cast<float>(projPts[triangle.i2].x - vpRect.left),
                                  static_cast<float>(projPts[triangle.i2].y - vpRect.top),
                                  1.0f / std::max(
                                    projectionCameraDistance * 0.1f,
                                    projectionCameraDistance +
                                        projectedDepth[triangle.i2]) }
                            };
                            const uint32_t color =
                                static_cast<uint32_t>(triangle.color);
                            const uint32_t dibColor =
                                ((color & 0x000000FFu) << 16) |
                                (color & 0x0000FF00u) |
                                ((color & 0x00FF0000u) >> 16);
                            RasterizeDepthTriangle(
                                scenePixels, sceneDepth, renderWidth,
                                renderHeight, face, dibColor);
                        }
                    } else {
                        HPEN headPen = CreatePen(PS_SOLID, 1, RGB(0, 220, 255));
                        HPEN bodyPen = CreatePen(PS_SOLID, 1, RGB(220, 240, 255));
                        HPEN capePen = CreatePen(PS_SOLID, 1, RGB(0, 110, 220));

                        for (size_t i = 0; i + 2 < pMesh->indices.size(); i += 3) {
                            const uint32_t i0 = pMesh->indices[i];
                            const uint32_t i1 = pMesh->indices[i + 1];
                            const uint32_t i2 = pMesh->indices[i + 2];
                            if (i0 >= projPts.size() || i1 >= projPts.size() ||
                                i2 >= projPts.size()) {
                                continue;
                            }

                            if (pMesh->vertices[i0].y > 0.35f) {
                                SelectObject(memDC, headPen);
                            } else if (pMesh->vertices[i0].color == 0xFF0070D0) {
                                SelectObject(memDC, capePen);
                            } else {
                                SelectObject(memDC, bodyPen);
                            }
                            MoveToEx(memDC, projPts[i0].x, projPts[i0].y, nullptr);
                            LineTo(memDC, projPts[i1].x, projPts[i1].y);
                            LineTo(memDC, projPts[i2].x, projPts[i2].y);
                            LineTo(memDC, projPts[i0].x, projPts[i0].y);
                        }
                        DeleteObject(headPen);
                        DeleteObject(bodyPen);
                        DeleteObject(capePen);
                    }
                }

                if (g_levelLoaded && g_playerSpawnValid && pMesh && pMesh->valid) {
                    const JxmdMesh* playerMesh = AssetLoader::Get().GetCachedAstroMesh();
                    float groundY = g_playerGroundY;
                    const float playerWorldX =
                        g_playerSpawnX + g_playerX * g_playerMoveRangeX;
                    const float playerWorldZ =
                        g_playerSpawnZ - g_playerZ * g_playerMoveRangeZ;
                    if (playerMesh && playerMesh->valid &&
                        g_levelCollision.FindGroundHeightNear(
                            playerWorldX, playerWorldZ, g_playerGroundY,
                            0.35f, 0.5f, groundY)) {
                        const float playerCenterX =
                            (playerMesh->bboxMin[0] + playerMesh->bboxMax[0]) * 0.5f;
                        const float playerCenterZ =
                            (playerMesh->bboxMin[2] + playerMesh->bboxMax[2]) * 0.5f;
                        const float playerExtent = std::max(
                            playerMesh->bboxMax[0] - playerMesh->bboxMin[0],
                            std::max(playerMesh->bboxMax[1] - playerMesh->bboxMin[1],
                                     playerMesh->bboxMax[2] - playerMesh->bboxMin[2]));
                        const float levelExtent = std::max(
                            pMesh->bboxMax[0] - pMesh->bboxMin[0],
                            std::max(pMesh->bboxMax[1] - pMesh->bboxMin[1],
                                     pMesh->bboxMax[2] - pMesh->bboxMin[2]));
                        const float playerScale = playerExtent > 1e-6f
                            ? (levelExtent * 0.06f) / playerExtent
                            : 0.0f;
                        const float playerOffsetY =
                            groundY + g_jumpHeight - bboxCenterY;
                        static std::vector<POINT> playerPoints;
                        static std::vector<float> playerDepth;
                        playerPoints.resize(playerMesh->vertices.size());
                        playerDepth.resize(playerMesh->vertices.size());
                        for (size_t i = 0; i < playerMesh->vertices.size(); ++i) {
                            const JxmdVertex& vertex = playerMesh->vertices[i];
                            const float worldX = playerWorldX +
                                (vertex.x - playerCenterX) * playerScale;
                            const float worldY = playerOffsetY +
                                (vertex.y - playerMesh->bboxMin[1]) * playerScale;
                            const float worldZ = g_playerSpawnZ +
                                -g_playerZ * g_playerMoveRangeZ +
                                (vertex.z - playerCenterZ) * playerScale;

                            const float px = worldX - bboxCenterX;
                            const float py = worldY;
                            const float pz = worldZ - bboxCenterZ;
                            const float x1 = px * cosf(g_modelRotY) -
                                             pz * sinf(g_modelRotY);
                            const float z1 = px * sinf(g_modelRotY) +
                                             pz * cosf(g_modelRotY);
                            const float y2 = py * cosf(g_modelRotX) -
                                             z1 * sinf(g_modelRotX);
                            const float z2 = py * sinf(g_modelRotX) +
                                             z1 * cosf(g_modelRotX);
                            const float denominator = projectionCameraDistance + z2;
                            const float perspective = projectionCameraDistance /
                                std::max(projectionCameraDistance * 0.1f, denominator);
                            playerPoints[i].x =
                                centerX + static_cast<int>(x1 * scale * perspective);
                            playerPoints[i].y =
                                centerY - static_cast<int>(y2 * scale * perspective);
                            playerDepth[i] = 1.0f / std::max(
                                projectionCameraDistance * 0.1f, denominator);
                        }

                        for (size_t i = 0; i + 2 < playerMesh->indices.size(); i += 3) {
                            const uint32_t i0 = playerMesh->indices[i];
                            const uint32_t i1 = playerMesh->indices[i + 1];
                            const uint32_t i2 = playerMesh->indices[i + 2];
                            if (i0 >= playerPoints.size() || i1 >= playerPoints.size() ||
                                i2 >= playerPoints.size()) {
                                continue;
                            }
                            const SoftwareRasterVertex face[3] = {
                                { static_cast<float>(playerPoints[i0].x - vpRect.left),
                                  static_cast<float>(playerPoints[i0].y - vpRect.top),
                                  playerDepth[i0] },
                                { static_cast<float>(playerPoints[i1].x - vpRect.left),
                                  static_cast<float>(playerPoints[i1].y - vpRect.top),
                                  playerDepth[i1] },
                                { static_cast<float>(playerPoints[i2].x - vpRect.left),
                                  static_cast<float>(playerPoints[i2].y - vpRect.top),
                                  playerDepth[i2] }
                            };
                            const uint32_t color =
                                playerMesh->vertices[i0].y > 0.35f
                                    ? RGB(0, 220, 255)
                                    : RGB(220, 240, 255);
                            const uint32_t dibColor =
                                ((color & 0x000000FFu) << 16) |
                                (color & 0x0000FF00u) |
                                ((color & 0x00FF0000u) >> 16);
                            RasterizeDepthTriangle(
                                scenePixels, sceneDepth, renderWidth,
                                renderHeight, face, dibColor);
                        }
                    }
                }
                g_softwareSceneMs = static_cast<float>(
                    std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - softwareSceneStart)
                        .count());
                g_perfSceneTotalMs += g_softwareSceneMs;
                ++g_perfSceneCount;
                if (viewportClip != 0) RestoreDC(memDC, viewportClip);
                if (g_levelLoaded) {
                    BITMAPINFO viewportBitmapInfo{};
                    viewportBitmapInfo.bmiHeader.biSize =
                        sizeof(viewportBitmapInfo.bmiHeader);
                    viewportBitmapInfo.bmiHeader.biWidth = renderWidth;
                    viewportBitmapInfo.bmiHeader.biHeight = -renderHeight;
                    viewportBitmapInfo.bmiHeader.biPlanes = 1;
                    viewportBitmapInfo.bmiHeader.biBitCount = 32;
                    viewportBitmapInfo.bmiHeader.biCompression = BI_RGB;
                    SetDIBitsToDevice(
                        memDC, vpRect.left, vpRect.top,
                        static_cast<DWORD>(renderWidth),
                        static_cast<DWORD>(renderHeight),
                        0, 0, 0, static_cast<UINT>(renderHeight),
                        scenePixels.data(), &viewportBitmapInfo, DIB_RGB_COLORS);
                }
                HBRUSH viewportFrame = CreateSolidBrush(RGB(0, 140, 230));
                FrameRect(memDC, &vpRect, viewportFrame);
                DeleteObject(viewportFrame);

                // Overlay de status no Viewport
                SelectObject(memDC, g_uiFontSub);
                SetTextColor(memDC, RGB(0, 220, 255));
                char viewportInfo[192];
                if (g_levelLoaded) {
                    snprintf(viewportInfo, sizeof(viewportInfo), "● ASTRO BOT PC — HUB: %s (%d tris; %u originais/%u fallback)",
                             pMesh ? pMesh->name.c_str() : "Gorilla Nebula",
                             g_levelTriCount, g_levelMesh.sourceChunkCount,
                             g_levelMesh.proceduralChunkCount);
                } else {
                    snprintf(viewportInfo, sizeof(viewportInfo), "● RENDERIZADOR 3D: ASTRO BOT (%s)",
                             pMesh && pMesh->valid ? pMesh->name.c_str() : "SEM MALHA");
                }
                TextOutA(memDC, 45, 132, viewportInfo, (int)strlen(viewportInfo));
                SetTextColor(memDC, RGB(180, 205, 235));
                char rendererInfo[128];
                snprintf(rendererInfo, sizeof(rendererInfo),
                         "Software scene: %.1f ms | GPU: AMD Radeon RX 9060 XT",
                         g_softwareSceneMs);
                TextOutA(memDC, 45, 154, rendererInfo,
                         static_cast<int>(strlen(rendererInfo)));

                // Painel Lateral de Interação do Astro
                int sideX = 30 + vpW + 20;
                int sideW = width - sideX - 30;

                SelectObject(memDC, g_uiFontHead);
                SetTextColor(memDC, RGB(255, 200, 80));
                TextOutA(memDC, sideX, 120, "COMANDOS DE INTERAÇÃO 3D", 24);

                SelectObject(memDC, g_uiFontBody);
                SetTextColor(memDC, RGB(220, 235, 250));
                std::string cmds[] = {
                    "R-Stick / Mouse:  Gira Astro 360°",
                    "L-Stick:          Move Astro pelo hub",
                    "Gatilho LT / RT:  Zoom In / Out",
                    "Botao A / Space:  Pulo do Astro no solo",
                    "Botao X (Xbox):   Golpe / Soco",
                    "Botao Y (Xbox):   Vibracao Haptica",
                    "LB / RB:          Mudar de Aba",
                    "",
                    "Status da Animacao:",
                    (g_jumpHeight > 0.05f) ? "  [●] ACAO: PULO ATIVO!" :
                    (g_attackTimer > 0.05f) ? "  [●] ACAO: SOCO / ATAQUE!" :
                                              "  [ ] Astro em Espera (Idle 60 FPS)"
                };

                int commandY = 150;
                for (const std::string& command : cmds) {
                    TextOutA(memDC, sideX, commandY,
                             command.c_str(), static_cast<int>(command.size()));
                    commandY += 22;
                }

                // Caixa de Estatísticas do Modelo
                RECT statsBox = { sideX, height - 232, width - 30, height - 70 };
                HBRUSH statsBrush = CreateSolidBrush(RGB(14, 22, 42));
                FillRect(memDC, &statsBox, statsBrush);
                DeleteObject(statsBrush);

                SelectObject(memDC, g_uiFontHead);
                SetTextColor(memDC, RGB(100, 230, 255));
                TextOutA(memDC, sideX + 12, height - 222, "GEOMETRIA REAL DO JXMD", 23);

                SelectObject(memDC, g_uiFontBody);
                SetTextColor(memDC, RGB(200, 220, 245));
                char statLines[6][192] = {};
                if (pMesh && pMesh->valid) {
                    const double sizeMb = (double)pMesh->fileSizeBytes / (1024.0 * 1024.0);
                    const unsigned long long tris = (unsigned long long)(pMesh->indexCount / 3);
                    snprintf(statLines[0], 192, "Arquivo:   %s", pMesh->name.c_str());
                    snprintf(statLines[1], 192, "Tamanho:   %.2f MB | Chunks: %u", sizeMb, pMesh->chunkCount);
                    snprintf(statLines[2], 192, "Vertices:  %u | Indices: %u", pMesh->vertexCount, pMesh->indexCount);
                    snprintf(statLines[3], 192, "Poligonos: %llu | Submalhas: %u", tris, pMesh->submeshCount);
                    snprintf(statLines[4], 192, "BBox Y:    %.2f .. %.2f", pMesh->bboxMin[1], pMesh->bboxMax[1]);
                    snprintf(statLines[5], 192, "Textura:   arbot_eyes_single_red_d.gnfp (BC7)");
                } else {
                    snprintf(statLines[0], 192, "Arquivo:   (nenhum JXMD carregado)");
                    snprintf(statLines[1], 192, "Tamanho:   --");
                    snprintf(statLines[2], 192, "Vertices:  0 | Indices: 0");
                    snprintf(statLines[3], 192, "Poligonos: 0 | Submalhas: 0");
                    snprintf(statLines[4], 192, "BBox Y:    --");
                    snprintf(statLines[5], 192, "Textura:   --");
                }
                for (int i = 0; i < 6; i++) {
                    TextOutA(memDC, sideX + 12, height - 194 + i * 21, statLines[i], (int)strlen(statLines[i]));
                }
            }

            // --- ABA 2: CONFIGURAÇÕES GRÁFICAS ---
            else if (g_currentTab == ActiveTab::GraphicsSettings) {
                SelectObject(memDC, g_uiFontHead);
                SetTextColor(memDC, RGB(255, 200, 80));
                TextOutA(memDC, 35, 120, "PAINEL DE CONFIGURAÇÕES GRÁFICAS & DISPLAY", 41);

                SelectObject(memDC, g_uiFontBody);
                SetTextColor(memDC, RGB(220, 235, 250));

                const auto& cfg = ConfigManager::Get().GetConfig();
                const auto profile = ConfigManager::Get().GetActiveResolutionProfile();

                std::string gLines[] = {
                    "Resolução Ativa:          " + std::string(profile.nativeTag) + " (" + std::to_string(profile.width) + "x" + std::to_string(profile.height) + ")",
                    "Modo de Janela:           " + cfg.windowMode + " [Alternar com F11]",
                    "Sincronização Vertical:   " + std::string(cfg.vsync ? "Ativado (Travado a 60 FPS)" : "Desativado"),
                    "AMD FSR 4 (Super Resol):  Ativado [142 passes | Perfil: " + cfg.fsr4Quality + " | RCAS: " + std::to_string(cfg.rcasSharpening).substr(0, 4) + "]",
                    "Renderizador:             Non-Tiled Deferred Renderer [Patched @ 0x80d914d]",
                    "Iluminação Global:        GI Probes Desativadas [Patched @ 0x80723fb]",
                    "Patches XML & ISA:        inFAMOUS XML + ISA Rewrite (1619 instrucoes mapeadas)",
                    "Tabela de NIDs (SCE):     " + std::to_string(NidResolver::Get().GetTableSize()) + " NIDs Registrados (libkernel, libc, libSceFios2)",
                    "Memória GARLIC (VRAM):    16 MB Alocado em 0x264e92b0000",
                    "Memória ONION (Coerente): 8 MB Alocado em 0x264ea2b0000",
                    "Placa de Vídeo Física:    AMD Radeon RX 9060 XT (Vendor 0x1002)",
                    "VRAM Dedicada GDDR6:      16.304 MB",
                    "API Gráfica do Driver:    Vulkan 1.3 (C:\\Windows\\System32\\vulkan-1.dll)",
                    "Shader Cache SPIR-V:      " + std::to_string(ShaderRecompiler::Get().GetCacheSize()) + " Shaders Prontos em Memoria"
                };

                for (size_t i = 0; i < sizeof(gLines)/sizeof(gLines[0]); i++) {
                    TextOutA(memDC, 45, 145 + (int)i * 24, gLines[i].c_str(), (int)gLines[i].size());
                }

                // Dica inferior
                SetTextColor(memDC, RGB(120, 230, 160));
                TextOutA(memDC, 45, height - 90, "Dica: Você pode alterar a resolução no settings.ini (1080p, 2k, 4k).", 68);
            }

            // --- ABA 3: CONFIGURAÇÕES DO CONTROLE DE XBOX ---
            else if (g_currentTab == ActiveTab::ControllerSettings) {
                SelectObject(memDC, g_uiFontHead);
                SetTextColor(memDC, RGB(100, 240, 140));
                TextOutA(memDC, 35, 120, "CALIBRAÇÃO & RESPOSTA DO CONTROLE XBOX (XINPUT)", 47);

                SelectObject(memDC, g_uiFontBody);
                SetTextColor(memDC, RGB(220, 235, 250));

                const auto& pad = InputManager::Get().GetController(0);
                std::string cLines[] = {
                    "Status do Periférico:     " + std::string(pad.connected ? "CONECTADO (Player 1 - XInput Ativo)" : "Nenhum controle detectado"),
                    "Dead-Zone Radial:         0.12 (Elimina drift dos analógicos)",
                    "Intensidade de Vibração:  100% (Motores Hápticos Duplos)",
                    "Stick Esquerdo (LS):      X: " + std::to_string(pad.lx).substr(0, 5) + "  Y: " + std::to_string(pad.ly).substr(0, 5),
                    "Stick Direito (RS):       X: " + std::to_string(pad.rx).substr(0, 5) + "  Y: " + std::to_string(pad.ry).substr(0, 5),
                    "Gatilho LT (Propulsão):   " + std::to_string((int)(pad.lt * 100)) + "%",
                    "Gatilho RT (Gancho/Tiro): " + std::to_string((int)(pad.rt * 100)) + "%",
                    "Feedback Háptico:         Pressione Y no controle para testar vibração máxima",
                    "Botões em Tempo Real:     "
                };

                for (int i = 0; i < 8; i++) {
                    TextOutA(memDC, 45, 155 + i * 28, cLines[i].c_str(), (int)cLines[i].size());
                }

                // Luzes indicadoras dos botões pressionados
                int btnX = 45;
                int btnY = 155 + 8 * 28;
                TextOutA(memDC, btnX, btnY, "Botões Ativos: ", 15);

                const char* bNames[] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "D-Up", "D-Down", "D-Left", "D-Right", "Menu", "View" };
                bool bActive[] = {
                    pad.IsPressed(SCE_PAD_CROSS), pad.IsPressed(SCE_PAD_CIRCLE),
                    pad.IsPressed(SCE_PAD_SQUARE), pad.IsPressed(SCE_PAD_TRIANGLE),
                    pad.IsPressed(SCE_PAD_L1), pad.IsPressed(SCE_PAD_R1),
                    pad.lt > 0.15f, pad.rt > 0.15f,
                    pad.IsPressed(SCE_PAD_UP), pad.IsPressed(SCE_PAD_DOWN),
                    pad.IsPressed(SCE_PAD_LEFT), pad.IsPressed(SCE_PAD_RIGHT),
                    pad.IsPressed(SCE_PAD_OPTIONS), pad.IsPressed(SCE_PAD_TOUCH_PAD)
                };

                for (int b = 0; b < 14; b++) {
                    RECT bRect = { btnX + 140 + b * 54, btnY - 2, btnX + 140 + (b + 1) * 54 - 6, btnY + 22 };
                    HBRUSH bBrush = CreateSolidBrush(bActive[b] ? RGB(40, 255, 120) : RGB(25, 40, 70));
                    FillRect(memDC, &bRect, bBrush);
                    DeleteObject(bBrush);

                    SetTextColor(memDC, bActive[b] ? RGB(0, 0, 0) : RGB(160, 180, 210));
                    DrawTextA(memDC, bNames[b], -1, &bRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }

            // --- ABA 4: NÍVEIS & VÍDEOS CATALOGADOS ---
            else if (g_currentTab == ActiveTab::LevelsAndVideos) {
                SelectObject(memDC, g_uiFontHead);
                SetTextColor(memDC, RGB(255, 200, 80));
                TextOutA(memDC, 35, 120, "MUNDOS & VÍDEOS DO ASTRO BOT (DADOS EXTRAÍDOS)", 46);

                SelectObject(memDC, g_uiFontBody);
                SetTextColor(memDC, RGB(220, 235, 250));

                auto levels = AssetLoader::Get().GetAvailableLevels();
                auto videos = AssetLoader::Get().GetAvailableVideos();

                TextOutA(memDC, 45, 155, "Mundos e Galáxias Disponíveis:", 30);
                for (size_t i = 0; i < levels.size() && i < 6; i++) {
                    std::string lStr = "  [★] " + levels[i];
                    TextOutA(memDC, 55, 180 + (int)i * 24, lStr.c_str(), (int)lStr.size());
                }

                int vidY = 180 + (int)levels.size() * 24 + 15;
                TextOutA(memDC, 45, vidY, "Cinemáticas e Vídeos de Animação:", 33);
                for (size_t i = 0; i < videos.size() && i < 6; i++) {
                    std::string vStr = "  [▶] " + videos[i];
                    TextOutA(memDC, 55, vidY + 25 + (int)i * 24, vStr.c_str(), (int)vStr.size());
                }
            }

            // =================================================================
            // BARRA INFERIOR DE AJUDA & COMANDOS
            // =================================================================
            SelectObject(memDC, divPen);
            MoveToEx(memDC, 30, height - 55, nullptr);
            LineTo(memDC, width - 30, height - 55);

            SelectObject(memDC, g_uiFontSub);
            SetTextColor(memDC, RGB(140, 165, 195));
            TextOutA(memDC, 35, height - 42,
                "Navegação:  [LB/RB] Alternar Abas   |   [R-Stick] Girar Astro 360°   |   [A] Pulo   |   [X] Golpe   |   [ESC] Sair", 112);

            // Copia o buffer de memória diretamente para a janela (Zero Flicker)
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            // Limpeza de recursos GDI
            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldPen);
            DeleteObject(divPen);

            static auto paintSampleStart = paintStart;
            static uint32_t paintSampleCount = 0;
            ++paintSampleCount;
            const auto paintSampleEnd = std::chrono::steady_clock::now();
            const float paintSampleSeconds =
                std::chrono::duration<float>(paintSampleEnd - paintSampleStart)
                    .count();
            if (paintSampleSeconds >= 1.0f) {
                g_paintFps = static_cast<float>(paintSampleCount) /
                             paintSampleSeconds;
                paintSampleCount = 0;
                paintSampleStart = paintSampleEnd;
            }
            EndPaint(hwnd, &ps);
            ++g_perfPaintCount;
            g_perfPaintTotalMs += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - paintStart).count();
            return 0;
        }

        case WM_CLOSE:
            g_running = false;
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            if (g_backbufferDC) {
                if (g_backbufferOriginalBitmap) {
                    SelectObject(g_backbufferDC, g_backbufferOriginalBitmap);
                }
                if (g_backbufferBitmap) DeleteObject(g_backbufferBitmap);
                DeleteDC(g_backbufferDC);
                g_backbufferDC = nullptr;
                g_backbufferBitmap = nullptr;
                g_backbufferOriginalBitmap = nullptr;
                g_backbufferWidth = 0;
                g_backbufferHeight = 0;
            }
            DestroyUiFonts();
            g_running = false;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
#else
int main(int argc, char* argv[]) {
    void* hInstance = nullptr;
    int nCmdShow = 1;
    const char* lpCmdLine = (argc > 1) ? argv[1] : "";
#endif

    int perfBenchmarkSeconds = 0;
    g_logFile.open("astro_launch.log", std::ios::out | std::ios::trunc);

#ifdef _WIN32
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
#endif

    // Modo de diagnostico: AstroBot_PC.exe --dump-jxmd [caminho]
    {
        std::string cmdLine = lpCmdLine ? lpCmdLine : "";
        const size_t dumpPos = cmdLine.find("--dump-jxmd");
        if (dumpPos != std::string::npos) {
            std::string path = cmdLine.substr(dumpPos + 11);
            const size_t first = path.find_first_not_of(" \t\"");
            const size_t last = path.find_last_not_of(" \t\"");
            if (first != std::string::npos && last != std::string::npos) {
                path = path.substr(first, last - first + 1);
            } else {
                path.clear();
            }

            if (path.empty()) {
                path = "D:/Games/abaixar/jogo/PPSA21567/data/prein/characters/gfx/astro_lowpoly_costume.jxm";
            }

            const std::string dump = DumpJxmdFile(path);
            std::cout << dump;
            std::ofstream dumpFile("jxmd_dump.txt", std::ios::out | std::ios::trunc);
            if (dumpFile.is_open()) {
                dumpFile << dump;
            }
            if (g_logFile.is_open()) g_logFile.close();
            return 0;
        }
    }

    LogMessage("===========================================================");
    LogMessage("   ASTRO BOT PC — Inicializacao da Engine & Visualizador 3D");
    LogMessage("   Build: Fase 6 | SDK 09.00 | PPSA21567");
    LogMessage("   Target GPU: AMD Radeon RX 9060 XT (16GB VRAM)");
    LogMessage("   Target Input: Xbox Series X|S / Xbox One (XInput)");
    LogMessage("===========================================================\n");

    try {
        perfBenchmarkSeconds = ParsePerfBenchmarkSeconds(lpCmdLine);
        if (perfBenchmarkSeconds > 0) {
            LogMessage("[Main] Benchmark de runtime habilitado por " +
                       std::to_string(perfBenchmarkSeconds) +
                       " s; um CSV exclusivo sera gravado no diretorio atual.");
        }

        // 1. Configurações
        ConfigManager::Get().Load(kConfigPath);
        const auto& cfg = ConfigManager::Get().GetConfig();
        const auto profile = ConfigManager::Get().GetActiveResolutionProfile();

        LogMessage(std::string("[Main] Modo de Resolucao Ativo: ") + profile.nativeTag);

        const std::string gameDataPath = ResolveGameDataPath(cfg);
        const std::string ebootPath = gameDataPath.empty()
            ? std::string()
            : (fs::path(gameDataPath).parent_path() / "eboot.bin").string();

        LogMessage("[Main] GameDataPath: " + (gameDataPath.empty() ? std::string("(nao configurado)") : gameDataPath));

        // 1b. Tabela de Despacho Nativa (Shims)
        InitializeDispatchTable();

        // 2. Kernel e Memória Direta (GARLIC e ONION)
        KernelEmulator::Get().Initialize();

        // 3. Janela Win32
#ifdef _WIN32
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = AstroWindowProc;
        wc.hInstance     = (HINSTANCE)hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"AstroPortWindow";
        RegisterClassEx(&wc);

        DWORD style = (cfg.windowMode == "Fullscreen")
            ? (WS_POPUP | WS_VISIBLE)
            : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

        int winWidth  = (profile.width > 1280) ? 1280 : (int)profile.width;
        int winHeight = (profile.height > 720) ? 720 : (int)profile.height;

        g_hwnd = CreateWindowExW(
            0, L"AstroPortWindow",
            L"ASTRO BOT PC (PPSA21567) — AMD Radeon RX 9060 XT",
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            winWidth, winHeight,
            nullptr, nullptr,
            (HINSTANCE)hInstance, nullptr
        );

        if (!g_hwnd) {
            throw std::runtime_error("Falha ao criar janela Win32!");
        }

        ShowWindow(g_hwnd, nCmdShow);
        UpdateWindow(g_hwnd);

        // 3b. HWND filho dedicado ao viewport Vulkan (mantem a UI GDI intacta)
        WNDCLASSEX vkChildClass = {};
        vkChildClass.cbSize        = sizeof(WNDCLASSEX);
        vkChildClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        vkChildClass.lpfnWndProc   = DefWindowProcW;
        vkChildClass.hInstance     = (HINSTANCE)hInstance;
        vkChildClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        vkChildClass.lpszClassName = L"AstroVulkanViewport";
        RegisterClassEx(&vkChildClass);

        RECT clientRect = {};
        GetClientRect(g_hwnd, &clientRect);
        RECT vkRect = {};
        ComputeVulkanViewportRect(clientRect.right, clientRect.bottom, vkRect);

        g_vkChild = CreateWindowExW(
            0, L"AstroVulkanViewport", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            vkRect.left, vkRect.top,
            vkRect.right - vkRect.left, vkRect.bottom - vkRect.top,
            g_hwnd, nullptr, (HINSTANCE)hInstance, nullptr
        );

        if (!g_vkChild) {
            throw std::runtime_error("Falha ao criar HWND filho do viewport Vulkan!");
        }

        // 4. Vulkan 1.3 Context (renderiza no HWND filho)
        VulkanBackend::Get().Initialize(
            g_vkChild,
            (uint32_t)(vkRect.right - vkRect.left),
            (uint32_t)(vkRect.bottom - vkRect.top),
            cfg.vsync);
#else
        void* g_hwnd = nullptr;
        VulkanBackend::Get().Initialize(g_hwnd, profile.width, profile.height, cfg.vsync);
#endif

        // 5. InputManager (Xbox Controller)
        InputConfig inputCfg;
        inputCfg.stickDeadZoneInner = cfg.xboxDeadZoneInner;
        inputCfg.stickDeadZoneOuter = cfg.xboxDeadZoneOuter;
        inputCfg.triggerThreshold   = cfg.xboxTriggerThreshold;
        inputCfg.rumbleIntensity    = cfg.xboxRumbleIntensity;
        inputCfg.invertRightY       = cfg.xboxInvertRightY;
        InputManager::Get().SetInputConfig(inputCfg);
        InputManager::Get().Initialize(cfg.enableDualSense, cfg.enableXInput);

        // 5b. AudioBackend (WASAPI)
        AudioPortConfig audioCfg;
        audioCfg.sampleRate = 48000;
        audioCfg.channels = 2;
        AudioBackend::Get().Initialize(audioCfg, cfg.masterVolume);

        // 6. AssetLoader (Fase 7 — JXMD real & GNF Textures)
        if (!gameDataPath.empty()) {
            AssetLoader::Get().Initialize(gameDataPath);
            if (AssetLoader::Get().LoadLevelMesh("hub_crashsite", g_levelMesh)) {
                g_levelLoaded = true;
                g_levelTriCount = static_cast<int>(g_levelMesh.indices.size() / 3);
                LogMessage("[Main] Mundo 1 (Hub do Crash Site) carregado com sucesso!");
                if (g_levelCollision.Build(g_levelMesh)) {
                    float minCollisionX = 0.0f;
                    float maxCollisionX = 0.0f;
                    float minCollisionZ = 0.0f;
                    float maxCollisionZ = 0.0f;
                    const float centerX =
                        (g_levelMesh.bboxMin[0] + g_levelMesh.bboxMax[0]) * 0.5f;
                    const float centerZ =
                        (g_levelMesh.bboxMin[2] + g_levelMesh.bboxMax[2]) * 0.5f;
                    if (g_levelCollision.FindNearestGroundPoint(
                            centerX, centerZ, g_playerSpawnX, g_playerSpawnZ,
                        g_playerGroundY, g_playerSupportRadius,
                        GetPlayerCollisionHeight(
                            AssetLoader::Get().GetCachedAstroMesh())) &&
                        g_levelCollision.GetHorizontalBounds(
                            minCollisionX, maxCollisionX,
                            minCollisionZ, maxCollisionZ)) {
                        g_playerMoveRangeX =
                            std::max(0.0f, (maxCollisionX - minCollisionX) * 0.18f) /
                            kPlayerHorizontalLimit;
                        g_playerMoveRangeZ =
                            std::max(0.0f, (maxCollisionZ - minCollisionZ) * 0.18f) /
                            kPlayerHorizontalLimit;
                        g_playerSpawnValid =
                            g_playerMoveRangeX > 0.0f && g_playerMoveRangeZ > 0.0f;
                        g_playerOnGround = g_playerSpawnValid;
                    }
                    LogMessage("[Main] Colisao de solo preparada com " +
                        std::to_string(g_levelCollision.GetWalkableTriangleCount()) +
                        " triangulos caminhaveis de " +
                        std::to_string(g_levelMesh.sourceChunkCount) +
                        " chunks JXMD originais; " +
                        std::to_string(g_levelMesh.proceduralChunkCount) +
                        " chunks visuais procedurais foram excluidos da colisao.");
                    if (!g_playerSpawnValid) {
                        LogMessage("[Main] Nao foi possivel escolher um ponto de spawn caminhavel.");
                    }
                } else {
                    LogMessage("[Main] Mesh do hub nao contem triangulos de solo utilizaveis.");
                }
            }
        } else {
            LogMessage("[Main] Aviso: GameDataPath vazio; assets reais nao serao carregados.");
        }
        UpdateVulkanViewport();

        // 7. Resolucao de NIDs, eboot/ELF e Patches
        NidResolver::Get().BuildTable();
        const std::string linkJsonCandidates[] = {
            "out/link.json",
            "D:/Jogos ps4/issn-windows/issn-windows/out/link.json"
        };
        for (const auto& jsonPath : linkJsonCandidates) {
            if (fs::exists(jsonPath)) {
                NidResolver::Get().LoadFromJson(jsonPath);
                break;
            }
        }

        std::string execToLoad;
        const std::string execCandidates[] = {
            ebootPath,
            "D:/Jogos ps4/issn-windows/issn-windows/out/eboot.elf",
            "D:/Jogos ps4/issn-windows/issn-windows/out/boot-linked.bin",
            "out/eboot.elf",
            "eboot.bin"
        };
        for (const auto& cand : execCandidates) {
            if (!cand.empty() && fs::exists(cand)) {
                execToLoad = cand;
                break;
            }
        }

        if (!execToLoad.empty()) {
            LogMessage("[Main] Carregando binario do jogo: " + execToLoad);
            EbootLoader& loader = EbootLoader::Get();
            if (loader.Load(execToLoad)) {
                LogMessage("[Main] A imagem passou pelas etapas de carregamento "
                           "implementadas; isto nao confirma a execucao do jogo.");
                if (!loader.Launch()) {
                    LogMessage("[Main] Jogo nao iniciado: runtime PS5 ainda nao "
                               "esta implementado.");
                }
            } else {
                LogMessage("[Main] Jogo nao iniciado: o carregador parou antes do "
                           "mapeamento de segmentos, relocacoes e imports.");
            }
        } else {
            LogMessage("[Main] Nenhum binario eboot.elf/eboot.bin encontrado nas pastas padrao.");
        }

        LogMessage("\n[Main] Interface e subsistemas auxiliares inicializados; "
                   "o binario do jogo nao foi executado.");
        LogMessage("[Main] Visualizador, medidor de FPS e entrada Xbox ativos.\n");

        // 8. Loop Principal Interativo (60 FPS)
        MSG msg = {};
        auto lastTime = std::chrono::steady_clock::now();
        auto lastFpsTimer = std::chrono::steady_clock::now();
        const auto perfBenchmarkStart = std::chrono::steady_clock::now();
        uint32_t frameCounter = 0;
        uint64_t perfLoopCount = 0;
        g_perfPaintCount = 0;
        g_perfSceneCount = 0;
        g_perfPaintTotalMs = 0.0;
        g_perfSceneTotalMs = 0.0;

        while (g_running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    g_running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (!g_running) break;
            ++perfLoopCount;

            // Mede Frametime e FPS exatos
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;
            if (deltaTime <= 0.0001f) deltaTime = 0.016f;
            g_frametimeMs = deltaTime * 1000.0f;

            frameCounter++;
            g_frameCount++;
            auto elapsedFps = std::chrono::duration<float>(currentTime - lastFpsTimer).count();
            if (elapsedFps >= 0.5f) {
                g_fps = (float)frameCounter / elapsedFps;
                frameCounter = 0;
                lastFpsTimer = currentTime;

                // Registra no histórico
                g_fpsHistory[g_fpsHistIndex] = g_fps;
                g_fpsHistIndex = (g_fpsHistIndex + 1) % 60;
            }

            // Atualiza Input do Controle de Xbox
            InputManager::Get().Update();
            const auto& pad = InputManager::Get().GetController(0);

            if (pad.connected) {
                if (g_levelLoaded && g_playerSpawnValid) {
                    const PlayerPlanarPosition candidate = AdvancePlayerPlanarPosition(
                        { g_playerX, g_playerZ }, pad.lx, -pad.ly, deltaTime);
                    const float candidateWorldX =
                        g_playerSpawnX + candidate.x * g_playerMoveRangeX;
                    const float candidateWorldZ =
                        g_playerSpawnZ - candidate.z * g_playerMoveRangeZ;
                    float candidateGroundY = g_playerGroundY;
                    const float playerCollisionHeight = GetPlayerCollisionHeight(
                        AssetLoader::Get().GetCachedAstroMesh());
                    float movedWorldX = 0.0f;
                    float movedWorldZ = 0.0f;
                    if (g_levelCollision.MoveOnGroundWithSlide(
                            g_playerSpawnX + g_playerX * g_playerMoveRangeX,
                            g_playerSpawnZ - g_playerZ * g_playerMoveRangeZ,
                            candidateWorldX, candidateWorldZ, g_playerGroundY,
                            0.35f, 0.5f, movedWorldX, movedWorldZ,
                            candidateGroundY, g_playerSupportRadius,
                            playerCollisionHeight)) {
                        if (g_playerMoveRangeX > 0.0f) {
                            g_playerX =
                                (movedWorldX - g_playerSpawnX) /
                                g_playerMoveRangeX;
                        }
                        if (g_playerMoveRangeZ > 0.0f) {
                            g_playerZ =
                                (g_playerSpawnZ - movedWorldZ) /
                                g_playerMoveRangeZ;
                        }
                        g_playerGroundY = candidateGroundY;
                        g_playerOnGround = g_jumpHeight <= 0.0f;
                    }
                } else if (!g_levelLoaded) {
                    g_playerX = AdvancePlayerHorizontalPosition(
                        g_playerX, pad.lx, deltaTime);
                }

                // Rotação 3D com o Stick Direito (RS) ou rotação contínua idle
                if (fabsf(pad.rx) > 0.1f) {
                    g_modelRotY += pad.rx * 0.06f;
                } else {
                    g_modelRotY += 0.008f; // Rotação suave contínua em repouso
                }

                if (fabsf(pad.ry) > 0.1f) {
                    g_modelRotX += pad.ry * 0.04f;
                    if (g_modelRotX > 0.6f) g_modelRotX = 0.6f;
                    if (g_modelRotX < -0.4f) g_modelRotX = -0.4f;
                }

                // Zoom com os Gatilhos LT / RT
                if (pad.rt > 0.1f) g_modelZoom += pad.rt * 0.03f;
                if (pad.lt > 0.1f) g_modelZoom -= pad.lt * 0.03f;
                if (g_modelZoom < 0.5f) g_modelZoom = 0.5f;
                if (g_modelZoom > 2.5f) g_modelZoom = 2.5f;

                // Alternar abas com LB e RB
                if (pad.JustPressed(SCE_PAD_L1)) {
                    int prev = ((int)g_currentTab + 3) % 4;
                    g_currentTab = (ActiveTab)prev;
                    UpdateVulkanViewport();
                }
                if (pad.JustPressed(SCE_PAD_R1)) {
                    int next = ((int)g_currentTab + 1) % 4;
                    g_currentTab = (ActiveTab)next;
                    UpdateVulkanViewport();
                }

                // Ação de Pulo com Botão A
                if (pad.JustPressed(SCE_PAD_CROSS) && g_jumpHeight <= 0.01f &&
                    (!g_levelLoaded || g_playerOnGround)) {
                    g_jumpVelocity = 4.0f;
                    g_playerOnGround = false;
                }

                // Ação de Ataque com Botão X
                if (pad.JustPressed(SCE_PAD_SQUARE)) {
                    g_attackTimer = 0.35f;
                }

                // Vibração Háptica de Teste com Botão Y
                float rumbleL = 0.0f;
                float rumbleR = 0.0f;
                if (pad.IsPressed(SCE_PAD_TRIANGLE)) {
                    rumbleL = 0.9f;
                    rumbleR = 0.9f;
                } else {
                    if (pad.lt > 0.1f) rumbleL += pad.lt * 0.5f;
                    if (pad.rt > 0.1f) rumbleR += pad.rt * 0.5f;
                    if (g_jumpHeight > 0.01f) rumbleL += 0.3f;
                    if (g_attackTimer > 0.01f) rumbleR += 0.6f;
                }
                InputManager::Get().SetVibrationF(0, rumbleL, rumbleR);
            } else {
                g_modelRotY += 0.01f;
            }

            // Física de pulo: altura em unidades JXMD quando o hub está carregado.
            if (g_jumpVelocity != 0.0f || g_jumpHeight > 0.0f) {
                const float physicsDelta =
                    std::min(deltaTime, kMaxMovementDeltaTime);
                g_jumpHeight += g_jumpVelocity * physicsDelta;
                g_jumpVelocity -= 10.0f * physicsDelta;
                if (g_jumpHeight <= 0.0f) {
                    g_jumpHeight = 0.0f;
                    g_jumpVelocity = 0.0f;
                    g_playerOnGround = g_levelLoaded && g_playerSpawnValid;
                }
            } else if (g_levelLoaded && g_playerSpawnValid) {
                g_playerOnGround = true;
            }

            if (g_attackTimer > 0.0f) {
                g_attackTimer -= deltaTime;
                if (g_attackTimer < 0.0f) g_attackTimer = 0.0f;
            }

            // O viewport Vulkan fica oculto no hub GDI e nas demais abas.
            VulkanBackend::Get().SetModelTransform(
                g_modelRotY, g_modelRotX, g_modelZoom, g_jumpHeight, g_playerX);
            if (g_currentTab == ActiveTab::Viewport3D && !g_levelLoaded &&
                g_hwnd && !IsIconic(g_hwnd)) {
                VulkanBackend::Get().BeginFrame();
                VulkanBackend::Get().EndFrame();
            }

            if (g_hwnd) {
                InvalidateRect(g_hwnd, nullptr, FALSE);
            }

            // Trava taxa de quadros a ~60 FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            if (perfBenchmarkSeconds > 0 &&
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - perfBenchmarkStart)
                        .count() >= perfBenchmarkSeconds) {
                g_running = false;
            }
        }

        if (perfBenchmarkSeconds > 0) {
            const double elapsedSeconds =
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - perfBenchmarkStart)
                    .count();
            if (elapsedSeconds <= 0.0) {
                throw std::runtime_error(
                    "Benchmark de runtime terminou com duracao invalida.");
            }
            const auto outputTimestamp = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            const std::string perfOutputPath =
                "astro_perf_" + std::to_string(outputTimestamp) + ".csv";
            std::ofstream perfFile(perfOutputPath,
                                   std::ios::out | std::ios::trunc);
            if (!perfFile.is_open()) {
                throw std::runtime_error(
                    "Nao foi possivel criar " + perfOutputPath + ".");
            }
            perfFile << "elapsed_seconds,loop_frames,loop_fps,paint_count,"
                        "paint_fps,paint_mean_ms,scene_count,scene_mean_ms\n";
            perfFile << std::fixed << std::setprecision(3)
                     << elapsedSeconds << ',' << perfLoopCount << ','
                     << perfLoopCount / elapsedSeconds << ','
                     << g_perfPaintCount << ','
                     << g_perfPaintCount / elapsedSeconds << ','
                     << (g_perfPaintCount > 0
                             ? g_perfPaintTotalMs / g_perfPaintCount
                             : 0.0)
                     << ',' << g_perfSceneCount << ','
                     << (g_perfSceneCount > 0
                             ? g_perfSceneTotalMs / g_perfSceneCount
                             : 0.0)
                     << '\n';
            perfFile.close();
            if (!perfFile) {
                throw std::runtime_error(
                    "Falha ao gravar os resultados em " + perfOutputPath + ".");
            }
            LogMessage("[Main] Benchmark concluido: " +
                       std::to_string(perfLoopCount / elapsedSeconds) +
                       " loop FPS, " +
                       std::to_string(g_perfPaintCount / elapsedSeconds) +
                       " paint FPS, " +
                       std::to_string(
                           g_perfPaintCount > 0
                               ? g_perfPaintTotalMs / g_perfPaintCount
                               : 0.0) +
                       " ms/pintura; resultados em " + perfOutputPath + ".");
        }

        // Encerramento
        LogMessage("[Main] Encerrando subsistemas...");
        InputManager::Get().StopAllVibration();
        InputManager::Get().Shutdown();
        AudioBackend::Get().Shutdown();
        VulkanBackend::Get().Shutdown();
        KernelEmulator::Get().Shutdown();

        LogMessage("[Main] ASTRO BOT PC encerrado com sucesso.");
        if (g_logFile.is_open()) g_logFile.close();
        return 0;

    } catch (const std::exception& e) {
        std::string err = std::string("[Main] ERRO FATAL: ") + e.what();
        LogMessage(err);
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "ASTRO BOT PC — Erro Fatal", MB_ICONERROR | MB_OK);
#endif
        if (g_logFile.is_open()) g_logFile.close();
        return 1;
    }
}
