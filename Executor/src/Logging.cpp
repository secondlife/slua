#include "Luau/Executor.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace Luau
{
namespace Executor
{

static std::string vformat(const char* fmt, va_list args)
{
    va_list measure;
    va_copy(measure, args);
    int needed = vsnprintf(nullptr, 0, fmt, measure);
    va_end(measure);

    if (needed <= 0)
        return std::string();

    std::string out;
    out.resize((size_t)needed + 1);
    vsnprintf(&out[0], out.size(), fmt, args);
    out.pop_back();
    return out;
}

static void vlog(LogLevel level, const char* source, const char* fmt, va_list args)
{
    LogCallback callback = logCallback();
    if (callback == nullptr)
        return;

    std::string message = vformat(fmt, args);
    callback(level, source, message.c_str());
}

void logDebug(const char* source, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Debug, source, fmt, args);
    va_end(args);
}

void logInfo(const char* source, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Info, source, fmt, args);
    va_end(args);
}

void logWarn(const char* source, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LogLevel::Warn, source, fmt, args);
    va_end(args);
}

} // namespace Executor
} // namespace Luau
