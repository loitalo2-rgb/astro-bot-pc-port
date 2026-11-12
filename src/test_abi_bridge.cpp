// ABI bridge tests: a thunk must translate SysV AMD64 arguments to the host
// Microsoft x64 convention. The test functions are compiled with the default
// host convention; the call sites use the sysv_abi attribute.

#include "core/abi_bridge.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

// --- targets, host (Microsoft x64) convention ---
int Target4(int a, int b, int c, int d) {
    return a * 1000 + b * 100 + c * 10 + d;
}

int Target6(int a, int b, int c, int d, int e, int f) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}

int TargetPointer(int* out, int value) {
    if (!out) return -1;
    *out = value * 2;
    return 0;
}

int64_t TargetWide(int64_t a, int64_t b) {
    return a * 1000000 + b;
}

int g_slots[6] = {0, 0, 0, 0, 0, 0};

int TargetRecorder(int a, int b, int c, int d, int e, int f) {
    g_slots[0] = a;
    g_slots[1] = b;
    g_slots[2] = c;
    g_slots[3] = d;
    g_slots[4] = e;
    g_slots[5] = f;
    return a + b + c + d + e + f;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-calling-convention"
#endif

using SysVFn4 = int(__attribute__((sysv_abi)) *)(int, int, int, int);
using SysVFn6 = int(__attribute__((sysv_abi)) *)(int, int, int, int, int, int);
using SysVFnPtr = int(__attribute__((sysv_abi)) *)(int*, int);
using SysVFnWide = int64_t(__attribute__((sysv_abi)) *)(int64_t, int64_t);
using SysVFnRecorder =
    int(__attribute__((sysv_abi)) *)(int, int, int, int, int, int);

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename Fn>
Fn ThunkFor(void* target) {
    return reinterpret_cast<Fn>(AstroPort::AbiBridge::Get().CreateIntegerThunk(target));
}

} // namespace

int main() {
#if !defined(_WIN32)
    std::cout << "ABI bridge test skipped: host already uses SysV.\n";
    return 0;
#endif
    using namespace AstroPort;

    // Baseline: calling a host-convention target through a SysV pointer is the
    // bug the bridge fixes. It is exercised by the probe program during
    // development and omitted here so the suite does not depend on the values
    // left in registers by an incompatible call.

    // Four integer registers, order and value.
    {
        SysVFn4 fn = ThunkFor<SysVFn4>(reinterpret_cast<void*>(&Target4));
        expect(fn != nullptr, "thunk created for 4-argument target");
        expect(fn(1, 2, 3, 4) == 1234, "4 integer args arrive in order");
        expect(fn(9, 8, 7, 6) == 9876, "4 integer args vary");
    }

    // Six arguments: 5th and 6th travel on the stack for the host ABI.
    {
        SysVFn6 fn = ThunkFor<SysVFn6>(reinterpret_cast<void*>(&Target6));
        expect(fn(1, 2, 3, 4, 5, 6) == 1 + 4 + 9 + 16 + 25 + 36,
               "6 integer args arrive in order");
        expect(fn(0, 0, 0, 0, 7, 8) == 7 * 5 + 8 * 6,
               "arguments 5 and 6 are copied to the stack");
    }

    // Pointer argument.
    {
        SysVFnPtr fn = ThunkFor<SysVFnPtr>(reinterpret_cast<void*>(&TargetPointer));
        int value = 0;
        expect(fn(&value, 21) == 0 && value == 42,
               "pointer argument reaches the target");
        expect(fn(nullptr, 1) == -1, "null pointer is preserved");
    }

    // 64-bit arguments.
    {
        SysVFnWide fn = ThunkFor<SysVFnWide>(reinterpret_cast<void*>(&TargetWide));
        expect(fn(1234, 5678) == 1234LL * 1000000 + 5678,
               "64-bit args arrive intact");
    }

    // Full recorder: all six slots and argument order.
    {
        SysVFnRecorder fn =
            ThunkFor<SysVFnRecorder>(reinterpret_cast<void*>(&TargetRecorder));
        std::memset(g_slots, 0, sizeof(g_slots));
        const int sum = fn(10, 20, 30, 40, 50, 60);
        expect(sum == 210, "recorder sum");
        expect(g_slots[0] == 10 && g_slots[1] == 20 && g_slots[2] == 30 &&
                   g_slots[3] == 40 && g_slots[4] == 50 && g_slots[5] == 60,
               "all six arguments recorded in order");
    }

    // Cache: same target yields the same thunk.
    {
        void* first =
            AbiBridge::Get().CreateIntegerThunk(reinterpret_cast<void*>(&Target4));
        void* second =
            AbiBridge::Get().CreateIntegerThunk(reinterpret_cast<void*>(&Target4));
        expect(first == second, "thunks are cached per target");
        expect(first != reinterpret_cast<void*>(&Target4),
               "thunk address differs from the target");
        expect(AbiBridge::Get().GetThunkCount() >= 4,
               "thunk count tracks created thunks");
    }

    // Repeated calls keep the stack balanced.
    {
        SysVFn6 fn = ThunkFor<SysVFn6>(reinterpret_cast<void*>(&Target6));
        int total = 0;
        for (int i = 0; i < 1000; ++i) {
            total += fn(1, 1, 1, 1, 1, 1);
        }
        expect(total == 1000 * 21, "1000 calls keep the stack balanced");
    }

    if (failures != 0) {
        std::cerr << failures << " ABI bridge test(s) failed.\n";
        return 1;
    }
    std::cout << "ABI bridge validation passed.\n";
    return 0;
}
