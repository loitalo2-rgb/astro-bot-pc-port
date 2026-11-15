#include <windows.h>
#include <iostream>

// Função substituta (nosso "Shim")
void __stdcall HookedFunction(int a, int b) {
    std::cout << "[Shim] Hooked! " << a << " + " << b << " interceptado!" << std::endl;
}

int main() {
    DWORD pid;
    std::cout << "Digite o PID do target.exe: ";
    std::cin >> pid;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "Falha ao abrir processo!" << std::endl;
        return 1;
    }

    uintptr_t targetAddr;
    std::cout << "Digite o endereco da OriginalFunction: ";
    std::cin >> std::hex >> targetAddr;

    // Criar o JMP para o nosso Hook
    unsigned char jmp[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // JMP [RIP+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // 64-bit addr
    };
    uintptr_t addr = (uintptr_t)&HookedFunction;
    memcpy(&jmp[6], &addr, 8);

    SIZE_T written;
    DWORD oldProtect;
    VirtualProtectEx(hProcess, (void*)targetAddr, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
    WriteProcessMemory(hProcess, (void*)targetAddr, jmp, 14, &written);
    VirtualProtectEx(hProcess, (void*)targetAddr, 14, oldProtect, &oldProtect);

    std::cout << "Hook aplicado com sucesso!" << std::endl;
    CloseHandle(hProcess);
    return 0;
}
