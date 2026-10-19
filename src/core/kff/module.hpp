// Modul-Schnittstelle. Jedes Modul ist genau eine Datei (src/modules/<name>.cpp) und liefert genau
// eine Instanz ueber kff::modules::<name>(). Der Kern ruft die Callbacks in der Listenreihenfolge
// (src/core/modules.cpp) auf. Jeder Aufruf ist einzeln abgesichert: eine Exception beendet nur den
// Aufruf dieses Moduls in diesem Frame, nicht das Paket.
//
// Minimales Modul:
//
//   #include "core/kff/module.hpp"
//   namespace { class FlugModule final : public kff::Module {
//       public:
//           const char* name() const override { return "flug"; }
//           kff::ModResult install(kff::Registry& reg) override {
//               return reg.addPre(kff::Hook::KagoExecuteFly, &on_fly_pre, kff::prio::kFlug);
//           }
//   }; FlugModule g; }
//   namespace kff::modules { Module& flug() { return g; } }
#pragma once

#include "core/kff/autotest.hpp"
#include "core/kff/hooks.hpp"
#include "core/kff/session.hpp"

#include <cstddef>

class daAlink_c;
class daKago_c;

namespace kff {

class Module {
public:
    virtual ~Module() = default;

    // Kurzname, z. B. "flug". Besitzer fuer Logs und Replace-Konflikte.
    virtual const char* name() const = 0;

    // Einmal aus mod_initialize, nach den Kern-Hooks. Hier Hooks (reg.addPre/addPost/replace),
    // Kamera-Operatoren und Szenarien. Ein Fehler wird geloggt, das Paket laeuft weiter.
    virtual ModResult install(Registry& reg) {
        (void)reg;
        return MOD_OK;
    }

    // Ruf-Kombination bei menschlichem Link (Wolf-Form fehlt). true = Modul uebernimmt
    // (z. B. Verwandlung zum Wolf, danach summonBird). Sonst meldet der Kern "human form".
    virtual bool onHumanCall(Session& s, daAlink_c* link) {
        (void)s;
        (void)link;
        return false;
    }

    // Ritt beginnt (Phase Riding). Der Kern hat den Ritt-Zustand schon gesetzt (setMidnaRideOn, setPlayerRideOn).
    virtual void onRideStart(Session& s, daKago_c* bird) {
        (void)s;
        (void)bird;
    }

    // Pre-Hook von daKago_c::execute des eigenen Vogels, jeden Frame in Riding (nicht in Dismounting),
    // nach dem Ersatzpfad und vor action()/executeFly().
    virtual void onFrameBird(Session& s, daKago_c* bird) {
        (void)s;
        (void)bird;
    }

    // Pre-Hook von daAlink_c::execute, jeden Link-Frame, unabhaengig von Sitzung und Phase.
    virtual void onFrameLink(Session& s, daAlink_c* link) {
        (void)s;
        (void)link;
    }

    // Ritt endet (Riding -> Dismounting oder Entfernen). bird kann nullptr sein (Vogel schon weg).
    // Aufruf VOR dem Loesen von Link und Midna.
    virtual void onRideEnd(Session& s, daKago_c* bird, EndReason why) {
        (void)s;
        (void)bird;
        (void)why;
    }

#if defined(KFF_DEV_AUTOTEST)
    // Szenario fuer KFF_AUTOTEST oder nullptr. Das Objekt muss statisch leben. Nur im Testbuild (D11): im Release
    // gibt es weder diese Schnittstelle noch Szenario-Code (WP7 Autotest-Gate).
    virtual const autotest::Scenario* autotestScenario() const { return nullptr; }
#endif

    // Entladen, nach dem Loesen des Vogels durch den Kern.
    virtual void onShutdown(Session& s) { (void)s; }
};

// Alle Module in Aufrufreihenfolge (src/core/modules.cpp).
Module* const* moduleList(size_t& count);
// Ein Modul, dessen install() fehlschlaegt, wird abgeschaltet: keine Callbacks mehr (src/core/modules.cpp).
void disableModule(const Module* m);
// false, wenn das Modul abgeschaltet wurde.
bool moduleActive(const Module* m);

// Eine Factory pro Modul (Definition in src/modules/<name>.cpp).
namespace modules {
Module& flug();
Module& freiheit();
Module& griff();
Module& tempo();
Module& kamera();
Module& autowolf();
Module& mensch();
}  // namespace modules

}  // namespace kff
