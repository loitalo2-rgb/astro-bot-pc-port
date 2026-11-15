#include <iostream>
#include <windows.h>

// Função que será o nosso "alvo" (como se fosse o jogo)
void __stdcall OriginalFunction(int a, int b) {
    std::cout << "[Target] Original: " << a << " + " << b << " = " << (a + b) << std::endl;
}

int main() {
    std::cout << "[Target] PID: " << GetCurrentProcessId() << std::endl;
    std::cout << "[Target] Endereco da funcao: " << (void*)&OriginalFunction << std::endl;

    while (true) {
        OriginalFunction(10, 20);
        Sleep(2000); // Espera 2 segundos
    }
    return 0;
}
