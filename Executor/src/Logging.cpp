#include "Luau/Executor.h"

#include "Luau/StringUtils.h"

#include <cstdarg>
#include <string>

namespace Luau
{
namespace Executor
{

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
