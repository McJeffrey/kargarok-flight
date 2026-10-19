// Kern-Tick-Zeitbasis (R4, WP9). Ein Kern-Tick ist eine Pad-Lesung (mDoCPd_c::read, Post, input.cpp), grob 30 je
// Sekunde (kSimPeriod). Der Zaehler laeuft auch in Pause und Ereignis weiter; Execute-Aufrufe von Link und Vogel
// tun das nicht. Darum zaehlen alle Zeitgrenzen des Pakets in Kern-Ticks: Abflug, Ruf (Vogel erscheint nicht),
// Wiederanbindung (Griff fehlt, Link ohne Cargo-Carry), Landefenster und Fallreferenz nach dem Absteigen.
//
// Regeln:
//  - Zeitgrenzen werden nicht in Execute-Hooks gezaehlt, nur dort geprueft. Gezaehlt wird je Pad-Lesung in
//    core::ridePadTick (ride.cpp, kernTimersTick).
//  - Ein Zeitstempel ist eine Zahl (Kern-Tick), nie ein Zeiger (R5).
//  - Nur uint32-Differenzen: der Zaehler laeuft um, die Differenz bleibt richtig.
#pragma once

#include "core/input_core.hpp"

#include <cstdint>

namespace kff {
namespace timers {

// Kern-Ticks je Sekunde im Normalbetrieb (Logiktakt 1/30 s). Nur fuer Texte und Umrechnung in Sekunden.
constexpr uint32_t kTicksPerSecond = 30;

// Aktueller Kern-Tick: Anzahl der Pad-Lesungen seit dem Laden der Mod.
inline uint32_t now() {
    return core::padTick();
}

// Kern-Ticks seit dem Zeitpunkt start (umlaufsicher, uint32).
inline uint32_t elapsed(uint32_t start) {
    return core::padTick() - start;
}

}  // namespace timers
}  // namespace kff
