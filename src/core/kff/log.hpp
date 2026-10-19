// Log des Pakets. Jede Zeile beginnt mit "kff: ". Dusklight schreibt sie als
// [INFO | com.kaizuki.kargarok_flight] kff: ...
#pragma once

#include <cstdint>

// printf-Formatpruefung nur fuer GCC und Clang. MSVC kennt __attribute__ nicht und baut sonst nicht.
#if defined(__GNUC__) || defined(__clang__)
#define KFF_PRINTF(fmt_index, arg_index) __attribute__((format(printf, fmt_index, arg_index)))
#else
#define KFF_PRINTF(fmt_index, arg_index)
#endif

namespace kff {

void logInfo(const char* fmt, ...) KFF_PRINTF(1, 2);
void logWarn(const char* fmt, ...) KFF_PRINTF(1, 2);
void logError(const char* fmt, ...) KFF_PRINTF(1, 2);

// Schreibt die ersten `first` Aufrufe und danach nur jeden `every`-ten (every 0: nur die ersten).
// `counter` gehoert dem Aufrufer und wird hier hochgezaehlt. Gegen Log-Spam in Hooks.
void logEvery(uint32_t& counter, uint32_t first, uint32_t every, const char* fmt, ...)
    KFF_PRINTF(4, 5);

// Telemetrie: wie logEvery, aber nur im Testbuild (KFF_DEV_AUTOTEST). Im Release wird nichts geschrieben.
// Fuer periodische Zustandszeilen der Entwicklung. Ereignis-, Ablehnungs- und Fehlerzeilen bleiben bei
// logInfo und logEvery.
void logTelemetry(uint32_t& counter, uint32_t first, uint32_t every, const char* fmt, ...)
    KFF_PRINTF(4, 5);

}  // namespace kff
