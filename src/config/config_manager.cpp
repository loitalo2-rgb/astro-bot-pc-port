#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

namespace AstroPort {

ConfigManager& ConfigManager::Get() {
    static ConfigManager instance;
    return instance;
}

std::string ConfigManager::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ConfigManager::Load(const std::string& iniFilePath) {
    std::ifstream file(iniFilePath);
    if (!file.is_open()) {
        // Tenta caminhos alternativos
        std::vector<std::string> fallbacks = {
            "config/settings.ini",
            "./config/settings.ini",
            "../config/settings.ini",
            "../../config/settings.ini"
        };
        for (const auto& alt : fallbacks) {
            file.open(alt);
            if (file.is_open()) {
                std::cout << "[ConfigManager] Configuracao carregada de: " << alt << "\n";
                break;
            }
        }
    }

    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Aviso: Nao foi possivel abrir settings.ini, usando valores padrao.\n";
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, eqPos));
        std::string val = Trim(line.substr(eqPos + 1));

        // Display
        if (key == "Resolution") {
            std::string lowerVal = val;
            std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), ::tolower);
            if (lowerVal == "1080p") {
                m_config.resolution = ResolutionMode::Res_1080p;
                m_config.targetWidth = 1920;
                m_config.targetHeight = 1080;
            } else if (lowerVal == "2k" || lowerVal == "1440p") {
                m_config.resolution = ResolutionMode::Res_2k;
                m_config.targetWidth = 2432;
                m_config.targetHeight = 1368;
            } else if (lowerVal == "4k" || lowerVal == "2160p") {
                m_config.resolution = ResolutionMode::Res_4k;
                m_config.targetWidth = 3840;
                m_config.targetHeight = 2160;
            }
        } else if (key == "WindowMode") {
            m_config.windowMode = val;
        } else if (key == "VSync") {
            m_config.vsync = (val == "true" || val == "1");
        } else if (key == "FramerateLimit") {
            m_config.framerateLimit = std::stoul(val);
        } else if (key == "InternalRenderScale") {
            m_config.internalRenderScale = std::stof(val);
        }
        else if (key == "FastLoad") {
            m_config.fastLoad = (val == "true" || val == "1");
        }

        else if (key == "ForceNonTiledDeferred") {
            m_config.forceNonTiledDeferred = (val == "true" || val == "1");
        } else if (key == "DisableGiProbes") {
            m_config.disableGiProbes = (val == "true" || val == "1");
        } else if (key == "DisableLightingShaders") {
            m_config.disableLightingShaders = (val == "true" || val == "1");
        } else if (key == "ApplyResolutionPatch") {
            m_config.applyResolutionPatch = (val == "true" || val == "1");
        }
        // FSR 4 & Upscaling
        else if (key == "EnableFSR4" || key == "FSR4") {
            m_config.enableFsr4 = (val == "true" || val == "1");
        } else if (key == "FSR4Quality") {
            m_config.fsr4Quality = val;
        } else if (key == "RCASSharpening") {
            m_config.rcasSharpening = std::stof(val);
        }
        // Graphics
        else if (key == "Renderer") {
            m_config.rendererBackend = val;
        } else if (key == "GPUIndex") {
            m_config.gpuIndex = std::stoul(val);
        } else if (key == "EnableValidationLayers") {
            m_config.enableValidationLayers = (val == "true" || val == "1");
        }
        // Audio
        else if (key == "Backend") {
            m_config.audioBackend = val;
        } else if (key == "Channels") {
            m_config.audioChannels = std::stoul(val);
        } else if (key == "MasterVolume") {
            m_config.masterVolume = std::stoul(val);
        }
        // Input
        else if (key == "EnableDualSense") {
            m_config.enableDualSense = (val == "true" || val == "1");
        } else if (key == "EnableXInput") {
            m_config.enableXInput = (val == "true" || val == "1");
        } else if (key == "XboxDeadZoneInner") {
            m_config.xboxDeadZoneInner = std::stof(val);
        } else if (key == "XboxDeadZoneOuter") {
            m_config.xboxDeadZoneOuter = std::stof(val);
        } else if (key == "XboxTriggerThreshold") {
            m_config.xboxTriggerThreshold = std::stof(val);
        } else if (key == "XboxRumbleIntensity") {
            m_config.xboxRumbleIntensity = std::stoul(val);
        } else if (key == "XboxInvertRightY") {
            m_config.xboxInvertRightY = (val == "true" || val == "1");
        }
        // System
        else if (key == "GameDataPath") {
            m_config.gameDataPath = val;
        } else if (key == "SaveDataPath") {
            m_config.saveDataPath = val;
        }
    }

    std::cout << "[ConfigManager] Configuracao carregada com sucesso!\n";
    std::cout << "  -> FastLoad: " << (m_config.fastLoad ? "Ativado" : "Desativado") << "\n";
    std::cout << "  -> Resolucao: " << m_config.targetWidth << "x" << m_config.targetHeight << "\n";
    std::cout << "  -> Renderizador Nao-Tiled: " << (m_config.forceNonTiledDeferred ? "Ativado" : "Desativado") << "\n";
    std::cout << "  -> Desativacao de GI Probes: " << (m_config.disableGiProbes ? "Ativado" : "Desativado") << "\n";
    return true;
}

ResolutionProfile ConfigManager::GetActiveResolutionProfile() const {
    switch (m_config.resolution) {
        case ResolutionMode::Res_1080p:
            return { ResolutionMode::Res_1080p, 1920, 1080, 2073600, "Tv_1920x1080", 0x8111005, 0xf6d71b8 };
        case ResolutionMode::Res_2k:
            return { ResolutionMode::Res_2k, 2432, 1368, 3326976, "Tv_2432x1368", 0x81a3ff2, 0xf6d71d0 };
        case ResolutionMode::Res_4k:
        default:
            return { ResolutionMode::Res_4k, 3840, 2160, 8294400, "Tv_3840x2160", 0x8066928, 0xf6d7218 };
    }
}

} // namespace AstroPort
