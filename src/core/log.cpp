#include "core/kff/log.hpp"

#include "core/kff/services.hpp"

#include <cstdarg>
#include <cstdio>

namespace kff {
namespace {

void emit(LogLevel level, const char* fmt, va_list args) {
    char body[480];
    std::vsnprintf(body, sizeof(body), fmt, args);
    char line[512];
    std::snprintf(line, sizeof(line), "kff: %s", body);
    if (svc_log == nullptr) {
        return;
    }
    svc_log->write(mod_ctx, level, line);
}

}  // namespace

void logInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    emit(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void logWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    emit(LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

void logError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    emit(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

void logEvery(uint32_t& counter, uint32_t first, uint32_t every, const char* fmt, ...) {
    ++counter;
    if (counter > first && (every == 0 || (counter - first) % every != 0)) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    emit(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void logTelemetry(uint32_t& counter, uint32_t first, uint32_t every, const char* fmt, ...) {
#if defined(KFF_DEV_AUTOTEST)
    ++counter;
    if (counter > first && (every == 0 || (counter - first) % every != 0)) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    emit(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
#else
    (void)counter;
    (void)first;
    (void)every;
    (void)fmt;
#endif
}

}  // namespace kff
