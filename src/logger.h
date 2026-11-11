#include <fstream>
#include <iostream>
#include <windows.h>

std::ofstream g_log;

void LogStep(const char* step) {
    if (g_log.is_open()) {
        g_log << step << std::endl;
        g_log.flush();
    }
}
