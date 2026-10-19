// Autotest: Pad-Injektion nach Zeitplan. Nur im Testbuild (CMake-Option KFF_DEV_AUTOTEST, Compile-Definition
// KFF_DEV_AUTOTEST=1) und nur mit KFF_AUTOTEST=<szenario> aktiv.
//
// Release (KFF_DEV_AUTOTEST nicht definiert): dieser Header bleibt als Schnittstelle erhalten, damit die Module
// unveraendert kompilieren. Die Funktionen sind dann leere Stubs; src/core/autotest.cpp wird nicht gebaut (Entscheidung D11).
//
// Ein aktives Szenario ERSETZT in jedem Tick den gesamten Zustand von Port 1 (Tasten, Stick, C-Stick, Trigger,
// Analogwerte). Echte Eingaben wirken dann nicht; ein Roh-Pad ungleich 0 wird als Fremdeingabe gezaehlt und
// mit Frame ins Log geschrieben. Ein Lauf mit Fremdeingabe ist verfaelscht.
//
// Szenario registrieren:
//   - im Modul: autotestScenario() liefert &ein_statisches_Scenario (Kern registriert es), oder
//   - im Modul install(): kff::autotest::registerScenario(&ein_statisches_Scenario).
// Der Wert von KFF_AUTOTEST ist der Name des Szenarios, z. B. KFF_AUTOTEST=zyklus.
#pragma once

#include "core/kff/services.hpp"

#include "m_Do/m_Do_controller_pad.h"

#include <cstdint>

namespace kff {

struct Session;

namespace autotest {

// Kontext eines Ticks. Eingaben gelten nur fuer diesen Tick, der Kern setzt sie danach zurueck.
struct Context {
    uint32_t frame;          // Ticks seit dem ersten Link-Frame (ein Tick je mDoCPd_c::read)
    Session& sess;           // Sitzungszustand, nur lesen (Phase, rideFrames, birdId, ...)

    void holdR();                    // R gehalten (Trigger 1.0). Die Flanke erkennt der Kern selbst.
    void tapB();                     // B gedrueckt (Flanke und gehalten)
    void tapA();                     // A gedrueckt (Flanke und gehalten)
    void tap(uint32_t buttons);      // Flanke und gehalten fuer diesen Tick, z. B. tap(PAD_BUTTON_Y)
    void hold(uint32_t buttons);     // gehalten ohne Flanke, z. B. hold(PAD_TRIGGER_L)
    void holdRAnalog(float v);       // R analog (Trigger 0..1, ohne Digitalbit); ab 0,61 zaehlt R als gehalten (Ruf)
    void stick(float x, float y);    // linker Stick, je -1..1
    void cStick(float x, float y);   // C-Stick, je -1..1
    void note(const char* text);     // Log-Zeile "kff: autotest: <text>"
    bool canCall() const;            // Link ist Wolf, sitzt nicht, kein Ereignis, kein eigener Vogel
    // WP3: Lock-Flaggen von L und R wie im Spiel (mDoCPd_c::LRlockCheck auf den Analogwerten dieses Ticks, Vorzustand =
    // Endzustand des Pads im letzten Tick). Ohne Aufruf folgen die Locks den Digitalbits (bisheriges Verhalten).
    void gameLocks();
};

// Liefert true, wenn das Szenario fertig ist. Danach gibt der Kern keine Eingaben mehr aus.
using TickFn = bool (*)(Context& ctx);

struct Scenario {
    const char* name;         // Wert von KFF_AUTOTEST (eindeutig)
    const char* description;  // Kurztext fuer das Log
    TickFn tick;
};

#if defined(KFF_DEV_AUTOTEST)

// Registriert die eingebauten Szenarien zyklus, konsum, ruf-spam, leben3, steuerung. Vor den Modul-Szenarien aufrufen.
void init();
// Registriert ein Szenario. Das Objekt muss statisch leben. MOD_CONFLICT bei doppeltem Namen.
ModResult registerScenario(const Scenario* scenario);
// Liest KFF_AUTOTEST und aktiviert das Szenario. Unbekannter Name: Fehler im Log, keine Injektion.
void selectFromEnvironment();
// Aktives Szenario oder nullptr.
const Scenario* active();
// Vom Kern aufgerufen, im Post-Hook von mDoCPd_c::read, vor dem Schnappschuss und vor dem Konsum.
void inject(interface_of_controller_pad& pad);
// Fremdeingaben seit dem Start des Szenarios (Roh-Pad ungleich 0 vor der Injektion).
uint32_t foreignInputCount();
// WP3: vom Kern am Ende von padReadPost aufgerufen (Endzustand des Pads nach Ruf und Konsum). lastPad() liefert ihn.
void observe(const interface_of_controller_pad& pad);
const interface_of_controller_pad& lastPad();
// WP3: Szenarien der Eingabe (src/core/input_test.cpp), von init() registriert.
void registerInputScenarios();

#else

// Release: leere Stubs, kein Szenario-Code. registerScenario meldet MOD_OK, damit Module wie flug und autowolf bei
// ihrer Installation nicht deaktiviert werden. Es wird nichts gespeichert und nichts eingespielt.
inline void init() {}
inline ModResult registerScenario(const Scenario*) {
    return MOD_OK;
}
inline void selectFromEnvironment() {}
inline const Scenario* active() {
    return nullptr;
}
inline void inject(interface_of_controller_pad&) {}
inline uint32_t foreignInputCount() {
    return 0;
}
inline void observe(const interface_of_controller_pad&) {}

inline void Context::holdR() {}
inline void Context::tapB() {}
inline void Context::tapA() {}
inline void Context::tap(uint32_t) {}
inline void Context::hold(uint32_t) {}
inline void Context::holdRAnalog(float) {}
inline void Context::stick(float, float) {}
inline void Context::cStick(float, float) {}
inline void Context::note(const char*) {}
inline bool Context::canCall() const {
    return false;
}
inline void Context::gameLocks() {}

#endif

}  // namespace autotest
}  // namespace kff
