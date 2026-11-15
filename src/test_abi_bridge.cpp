// ABI bridge tests: a thunk must translate SysV AMD64 arguments to the host
// Microsoft x64 convention. The test functions are compiled with the default
// host convention; the call sites use the sysv_abi attribute.

#include "core/abi_bridge.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>
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

// --- targets for typed signatures (host/MS x64 convention) ---

int TargetMixedIntDouble(int a, double b, double c, int d) {
    return a * 1000 + static_cast<int>(b * 10.0) +
           static_cast<int>(c * 100.0) + d;
}

int TargetMixedFloat(int a, float b, int c, float d) {
    return a * 1000 + static_cast<int>(b * 10.0f) + c * 100 +
           static_cast<int>(d * 1000.0f);
}

double Target8Double(double a, double b, double c, double d, double e,
                     double f, double g, double h) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

int Target8Int(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

int Target4Double1Int(double a, double b, double c, double d, int e) {
    return static_cast<int>(a + b + c + d) * 100 + e;
}

int Target4Int1Double(int a, int b, int c, int d, double e) {
    return a * 1000 + b * 100 + c * 10 + d + static_cast<int>(e * 10.0);
}

int Target9Double(double, double, double, double, double, double, double,
                  double, double) {
    return 0;
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
using SysVFnMixedIntDouble =
    int(__attribute__((sysv_abi)) *)(int, double, double, int);
using SysVFnMixedFloat =
    int(__attribute__((sysv_abi)) *)(int, float, int, float);
using SysVFn8Double = double(__attribute__((sysv_abi)) *)(
    double, double, double, double, double, double, double, double);
using SysVFn8Int =
    int(__attribute__((sysv_abi)) *)(int, int, int, int, int, int, int, int);
using SysVFn4Double1Int =
    int(__attribute__((sysv_abi)) *)(double, double, double, double, int);
using SysVFn4Int1Double =
    int(__attribute__((sysv_abi)) *)(int, int, int, int, double);
using SysVFn9Double = int(__attribute__((sysv_abi)) *)(
    double, double, double, double, double, double, double, double, double);

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename Fn>
Fn ThunkFor(void* target) {
    return reinterpret_cast<Fn>(AstroPort::AbiBridge::Get().CreateIntegerThunk(target));
}

AstroPort::AbiSignature Sig(std::initializer_list<AstroPort::AbiValueKind> kinds) {
    AstroPort::AbiSignature signature;
    for (AstroPort::AbiValueKind kind : kinds) signature.Add(kind);
    return signature;
}

template <typename Fn>
Fn TypedThunkFor(void* target,
                 std::initializer_list<AstroPort::AbiValueKind> kinds) {
    return reinterpret_cast<Fn>(
        AstroPort::AbiBridge::Get().CreateThunk(target, Sig(kinds)));
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

    // --- typed signatures: float/double and mixed arguments ---

    // Integer + double + double + integer: SysV uses RDI/RSI for ints and
    // XMM0/XMM1 for doubles; MS uses RCX, XMM1, XMM2, R9.
    {
        SysVFnMixedIntDouble fn = TypedThunkFor<SysVFnMixedIntDouble>(
            reinterpret_cast<void*>(&TargetMixedIntDouble),
            {AbiValueKind::Integer, AbiValueKind::Double,
             AbiValueKind::Double, AbiValueKind::Integer});
        expect(fn != nullptr, "mixed int/double thunk created");
        expect(fn(1, 2.5, 3.25, 4) == 1354,
               "mixed int/double arguments arrive in positional order");
    }

    // Integer + float + integer + float: independent SysV register counters.
    {
        SysVFnMixedFloat fn = TypedThunkFor<SysVFnMixedFloat>(
            reinterpret_cast<void*>(&TargetMixedFloat),
            {AbiValueKind::Integer, AbiValueKind::Float,
             AbiValueKind::Integer, AbiValueKind::Float});
        expect(fn != nullptr, "mixed int/float thunk created");
        expect(fn(1, 2.5f, 3, 4.25f) == 1 * 1000 + 25 + 300 + 4250,
               "mixed int/float arguments arrive in positional order");
    }

    // Eight doubles: XMM0..XMM7 on SysV; XMM0..XMM3 plus four stack slots on MS.
    {
        SysVFn8Double fn = TypedThunkFor<SysVFn8Double>(
            reinterpret_cast<void*>(&Target8Double),
            {AbiValueKind::Double, AbiValueKind::Double,
             AbiValueKind::Double, AbiValueKind::Double,
             AbiValueKind::Double, AbiValueKind::Double,
             AbiValueKind::Double, AbiValueKind::Double});
        expect(fn != nullptr, "eight-double thunk created");
        expect(fn(1, 2, 3, 4, 5, 6, 7, 8) == 204.0,
               "eight doubles arrive in order (4 registers + 4 stack slots)");
    }

    // Eight integers: args 7 and 8 move from the SysV stack to the MS stack.
    {
        SysVFn8Int fn = TypedThunkFor<SysVFn8Int>(
            reinterpret_cast<void*>(&Target8Int),
            {AbiValueKind::Integer, AbiValueKind::Integer,
             AbiValueKind::Integer, AbiValueKind::Integer,
             AbiValueKind::Integer, AbiValueKind::Integer,
             AbiValueKind::Integer, AbiValueKind::Integer});
        expect(fn != nullptr, "eight-integer thunk created");
        expect(fn(1, 2, 3, 4, 5, 6, 7, 8) == 204,
               "eight integers arrive in order (SysV stack -> MS stack)");
    }

    // Integer at position 5 sourced from a register (mixed, MS stack slot).
    {
        SysVFn4Double1Int fn = TypedThunkFor<SysVFn4Double1Int>(
            reinterpret_cast<void*>(&Target4Double1Int),
            {AbiValueKind::Double, AbiValueKind::Double,
             AbiValueKind::Double, AbiValueKind::Double,
             AbiValueKind::Integer});
        expect(fn != nullptr, "four doubles + int thunk created");
        expect(fn(1, 2, 3, 4, 7) == 1007,
               "integer in the 5th position reaches the MS stack slot");
    }

    // Double at position 5 sourced from XMM0 (mixed, MS stack slot).
    {
        SysVFn4Int1Double fn = TypedThunkFor<SysVFn4Int1Double>(
            reinterpret_cast<void*>(&Target4Int1Double),
            {AbiValueKind::Integer, AbiValueKind::Integer,
             AbiValueKind::Integer, AbiValueKind::Integer,
             AbiValueKind::Double});
        expect(fn != nullptr, "four ints + double thunk created");
        expect(fn(1, 2, 3, 4, 5.5) == 1289,
               "double in the 5th position reaches the MS stack slot");
    }

    // Fail-closed limits.
    {
        AbiSignature nineDoubles;
        for (int i = 0; i < 9; ++i) nineDoubles.Add(AbiValueKind::Double);
        expect(!nineDoubles.IsSupported(),
               "nine doubles are refused (SysV only has XMM0..XMM7)");
        expect(AbiBridge::Get().CreateThunk(
                   reinterpret_cast<void*>(&Target9Double),
                   nineDoubles) == nullptr,
               "unsupported float signature yields no thunk");

        AbiSignature sevenIntsOneDouble;
        for (int i = 0; i < 7; ++i) sevenIntsOneDouble.Add(AbiValueKind::Integer);
        sevenIntsOneDouble.Add(AbiValueKind::Double);
        expect(!sevenIntsOneDouble.IsSupported(),
               "mixed signature with 7 integer args is refused");

        AbiSignature tooMany;
        for (size_t i = 0; i < AbiSignature::kMaxArgs; ++i) {
            expect(tooMany.Add(AbiValueKind::Integer),
                   "16 integer args are accepted by the descriptor");
        }
        expect(!tooMany.Add(AbiValueKind::Integer),
               "the 17th argument is refused by the descriptor");
        expect(tooMany.IsSupported(),
               "16 integer args are within the supported limits");

        expect(AbiBridge::Get().CreateThunk(nullptr,
                                            Sig({AbiValueKind::Integer})) ==
                   nullptr,
               "null target yields no thunk");
    }

    // Cache key includes the signature, not just the target.
    {
        void* target = reinterpret_cast<void*>(&TargetMixedIntDouble);
        void* first = AbiBridge::Get().CreateThunk(target,
                                                   Sig({AbiValueKind::Integer}));
        void* again = AbiBridge::Get().CreateThunk(target,
                                                   Sig({AbiValueKind::Integer}));
        void* different = AbiBridge::Get().CreateThunk(
            target, Sig({AbiValueKind::Double}));
        expect(first == again, "same target and signature share the thunk");
        expect(first != different,
               "different signatures yield different thunks");
        expect(AbiBridge::Get().GetThunkSize() > 0,
               "reported thunk size upper bound is non-zero");
    }

    if (failures != 0) {
        std::cerr << failures << " ABI bridge test(s) failed.\n";
        return 1;
    }
    std::cout << "ABI bridge validation passed.\n";
    return 0;
}
