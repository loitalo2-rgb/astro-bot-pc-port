#include <chrono>
#include <iostream>
#include "config/config_manager.h"
#include "core/eboot_loader.h"

int main() {
    // Carrega configuração (settings.ini será copiado ao build e encontrado em ./config)
    AstroPort::ConfigManager::Get().Load("config/settings.ini");
    // Usa o singleton do loader
    AstroPort::EbootLoader& loader = AstroPort::EbootLoader::Get();
    const std::string ebootPath = "data/eboot.bin"; // ajuste se necessário
    auto start = std::chrono::high_resolution_clock::now();
    bool ok = loader.Load(ebootPath);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "EbootLoader::Load result=" << ok << ", elapsed=" << elapsed.count() << " s\n";
    return ok ? 0 : 1;
}
