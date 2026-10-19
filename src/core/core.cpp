// Lebenszyklus der Mod: mod_initialize, mod_update, mod_shutdown.
//
// Reihenfolge beim Laden: Konfiguration, Autotest-Register, Kern-Hooks (Besitzer "core"), Startpruefung der Voegel,
// danach jedes Modul in Listenreihenfolge (Besitzer = Modulname), danach die Registry-Pruefung und die Autotest-Auswahl.
// Ein Modul, dessen install() fehlschlaegt oder das einen Registrierungsfehler meldet, wird abgeschaltet. Der Kern laeuft
// trotzdem weiter.
//
// Entladen: erst der Ritt (shutdownBird: Ende mit Grund Shutdown, eigener Vogel entfernt), dann die Module (onShutdown),
// dann die Zusammenfassung. Die Hooks melden sich erst danach ab, weil der Loader mod_shutdown vor hook_remove_mod aufruft.
#include "core/handlers.hpp"
#include "core/kff/autotest.hpp"
#include "core/kff/config.hpp"
#include "core/kff/hooks.hpp"
#include "core/kff/log.hpp"
#include "core/kff/module.hpp"
#include "core/kff/panel.hpp"
#include "core/kff/session.hpp"
#include "core/kff/version.hpp"

#include <cstdlib>

namespace {

void shutdownModules() {
    size_t count = 0;
    kff::Module* const* list = kff::moduleList(count);
    for (size_t i = 0; i < count; ++i) {
        if (!kff::moduleActive(list[i])) {
            continue;
        }
        try {
            list[i]->onShutdown(kff::session());
        } catch (...) {
            kff::logWarn("module %s threw in onShutdown (ignored)", list[i]->name());
        }
    }
}

}  // namespace

#if defined(KFF_DEV_AUTOTEST)
namespace kff::zustand {
void install();  // src/core/zustand_test.cpp (nur Testbuild)
}
#endif

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError*) {
#if defined(KFF_DEV_AUTOTEST)
    const char* env = std::getenv("KFF_AUTOTEST");
    kff::logInfo("initialize v" KFF_MOD_VERSION " (Kargarok Flight, Testbuild), KFF_AUTOTEST=%s",
        env != nullptr ? env : "(aus)");
#else
    kff::logInfo("initialize v" KFF_MOD_VERSION " (Kargarok Flight, Release, ohne Autotest)");
#endif
    kff::core::logRegion();

    kff::config::registerVars();
    kff::panel::install();  // Einstellungsfeld im Mods-Fenster; ohne UiService ohne Wirkung
#if defined(KFF_DEV_AUTOTEST)
    kff::autotest::init();
    kff::panel::registerTestScenarios();
#endif
    kff::installCoreHooks();
#if defined(KFF_DEV_AUTOTEST)
    kff::zustand::install();  // Testbuild: Spielstand wie 100 Prozent und feste Tageszeit (KFF_TEST_UNLOCK, KFF_TEST_TIME)
#endif
    kff::core::startupBirdCheck();

    size_t count = 0;
    kff::Module* const* list = kff::moduleList(count);
    for (size_t i = 0; i < count; ++i) {
        kff::Module* m = list[i];
        kff::registry().setOwner(m->name());
        kff::registryResetOwnerFailures();
        ModResult r = MOD_ERROR;
        try {
            r = m->install(kff::registry());
        } catch (...) {
            kff::logError("module %s threw in install", m->name());
            r = MOD_ERROR;
        }
        const uint32_t failures = kff::registryOwnerFailures();
        if (r != MOD_OK || failures != 0) {
            // Fehlgeschlagene Installation oder Registrierungskonflikt (MOD_CONFLICT, Ziel nicht aufloesbar): das Modul
            // bekommt keine Callbacks mehr, seine bereits eingetragenen Callbacks bleiben stehen und sind wirkungslos.
            kff::disableModule(m);
            kff::logError("module %s install -> %s, %u registration failure(s), module disabled, %u callback(s) inert",
                m->name(), kff::modResultName(r), static_cast<unsigned>(failures),
                static_cast<unsigned>(kff::hookCallbackCount(m)));
            continue;
        }
        kff::logInfo("module %s installed, %u callback(s)", m->name(),
            static_cast<unsigned>(kff::hookCallbackCount(m)));
#if defined(KFF_DEV_AUTOTEST)
        if (const kff::autotest::Scenario* scenario = m->autotestScenario()) {
            const ModResult sr = kff::autotest::registerScenario(scenario);
            if (sr != MOD_OK) {
                kff::logWarn("module %s autotest scenario %s not registered (%s)", m->name(), scenario->name,
                    kff::modResultName(sr));
            }
        }
#endif
    }
    kff::registry().setOwner("core");
    kff::registryCheck();
#if defined(KFF_DEV_AUTOTEST)
    kff::autotest::selectFromEnvironment();
#endif
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    kff::core::shutdownBird();
    shutdownModules();
    kff::core::logSummary();
    kff::logInfo("unloaded");
    return MOD_OK;
}

}  // extern "C"
