// low-overhead timestamp-counter clock for quanta preemption checks.
//  This is largely a formalization of the `LLRDTSCTimer` pattern in
//  viewer and server.
#include "Luau/Executor.h"

#include "Luau/Common.h"

#include "lua.h"

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#define SLUA_QUANTA_TSC_X64 1
#elif defined(__aarch64__) && !defined(_MSC_VER)
#define SLUA_QUANTA_TSC_ARM64 1
#endif

#if defined(SLUA_QUANTA_TSC_X64) || defined(SLUA_QUANTA_TSC_ARM64)
#define SLUA_QUANTA_TSC 1
#endif

namespace Luau
{
namespace Executor
{

#if defined(SLUA_QUANTA_TSC_X64)
static inline uint64_t read_tsc()
{
    // Shouldn't need a fence.
    return __rdtsc();
}

static double measure_seconds_per_tick()
{
    // One-time calibration against lua_clock(), assuming an invariant TSC.
    // Similar to the `ms_sleep(1)` strategy in indra.
    double wall_begin = lua_clock();
    uint64_t tsc_begin = read_tsc();

    // Long enough for a stable ratio, short enough not to matter at setup
    constexpr double kCalibrationWindow = 0.004;
    double wall_end;
    do
    {
        wall_end = lua_clock();
    } while (wall_end - wall_begin < kCalibrationWindow);
    uint64_t tsc_end = read_tsc();

    LUAU_ASSERT(tsc_end > tsc_begin);
    if (tsc_end <= tsc_begin)
        return 0.0;

    return (wall_end - wall_begin) / (double)(tsc_end - tsc_begin);
}
#elif defined(SLUA_QUANTA_TSC_ARM64)
static inline uint64_t read_tsc()
{
    // Generic timer virtual count, readable from EL0 on Linux and macOS.
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static double measure_seconds_per_tick()
{
    // The architecture publishes the exact frequency, no calibration needed.
    // Apple Silicon reports 24MHz -> ~41.7ns per tick, ~3600 ticks per 150us
    // quanta.
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    LUAU_ASSERT(freq != 0);
    return freq != 0 ? 1.0 / (double)freq : 0.0;
}
#endif

#if defined(SLUA_QUANTA_TSC)
static double tsc_quanta_clock([[maybe_unused]] lua_State* L)
{
    static const double seconds_per_tick = measure_seconds_per_tick();
    return (double)read_tsc() * seconds_per_tick;
}
#endif

#if !defined(SLUA_QUANTA_TSC)
static double fallback_quanta_clock([[maybe_unused]] lua_State* L)
{
    return lua_clock();
}
#endif

lua_clockProvider resolveDefaultQuantaClock()
{
#if defined(SLUA_QUANTA_TSC)
    // Warm up now so any calibration cost lands at
    // provisioner setup rather than inside the first script's run window
    tsc_quanta_clock(nullptr);
    return tsc_quanta_clock;
#else
    return fallback_quanta_clock;
#endif
}

} // namespace Executor
} // namespace Luau
