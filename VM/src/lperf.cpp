// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#include "lua.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <time.h>

static double clock_period()
{
#if defined(_WIN32)
    LARGE_INTEGER result = {};
    QueryPerformanceFrequency(&result);
    return 1.0 / double(result.QuadPart);
#elif defined(__APPLE__)
    mach_timebase_info_data_t result = {};
    mach_timebase_info(&result);
    return double(result.numer) / double(result.denom) * 1e-9;
#elif defined(__linux__) || defined(__FreeBSD__)
    return 1e-9;
#elif defined(__EMSCRIPTEN__)
    return 1e-3;
#else
    return 1.0 / double(CLOCKS_PER_SEC);
#endif
}

static double clock_timestamp()
{
#if defined(_WIN32)
    LARGE_INTEGER result = {};
    QueryPerformanceCounter(&result);
    return double(result.QuadPart);
#elif defined(__APPLE__)
    return double(mach_absolute_time());
#elif defined(__linux__) || defined(__FreeBSD__)
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1e9 + now.tv_nsec;
#elif defined(__EMSCRIPTEN__)
    return emscripten_get_now();
#else
    return double(clock());
#endif
}

double lua_clock()
{
    static double period = clock_period();

    return clock_timestamp() * period;
}

// ServerLua: per-thread CPU time for deterministic yield gap measurement.
//  I don't entirely know how useful this is in practice, but it's clear to me that
//  we at least need to get rid of some of the scheduling overhead for the interrupt
//  timing delta checks to be moderately useful.
double lua_cputime()
{
#if defined(__APPLE__)
    mach_port_t thread = mach_thread_self();
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    kern_return_t kr = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count);
    mach_port_deallocate(mach_task_self(), thread);
    if (kr == KERN_SUCCESS)
    {
        return info.user_time.seconds + info.user_time.microseconds * 1e-6
             + info.system_time.seconds + info.system_time.microseconds * 1e-6;
    }
#elif defined(__linux__) || defined(__FreeBSD__)
    timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
    {
        return ts.tv_sec + ts.tv_nsec * 1e-9;
    }
#endif

    // If all else fails, fall back to wall clock.
    return lua_clock();
}
