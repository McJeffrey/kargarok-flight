// Fenster "Kargarok Flight buttons" und die Texte des Mods-Panels fuer die Flugtasten (E-9). Die Auswahl laeuft ueber
// SELECT-Listen (UI_BINDING_CALLBACKS). Jede Auswahl geht durch actions::setFromUi (E-5), also dieselbe Stelle, die
// der Test benutzt.
#pragma once

#include "core/kff/services.hpp"

#include <cstddef>
#include <cstdint>

namespace kff::steuerung {

// Oeffnet das Fenster mit dem Tab "Flight buttons" (window_push). Ist es schon offen, geschieht nichts. Gibt das
// Ergebnis des Dienstes zurueck; der Knopf im Panel ignoriert es.
ModResult openWindow();

// Rueckruf des Knopfs "Change flight buttons ..." im Mods-Panel (UiPressedFn).
void onOpenPressed(ModContext*, void*);

// Beschriftung des Knopfs im Mods-Panel: "Change flight buttons (default)" oder "Change flight buttons (N changed)".
void formatButtonLabel(char* buf, size_t size);

#if defined(KFF_DEV_AUTOTEST)
// Testbuild: derselbe Weg wie das SELECT beim Setzen (Auswahl pruefen, speichern, Statuszeile setzen). action ist
// der Index von actions::ActId.
void selectForTest(uint8_t action, int64_t value);
// Testbuild: setzt den Fokus auf die Zeile der Aktion (elem_focus, UiService 2.3). MOD_UNAVAILABLE, solange das Fenster
// die Zeile noch nicht hat oder der Dienst sie nicht fokussieren kann. action ist der Index von actions::ActId.
ModResult focusForTest(uint8_t action);
// Testbuild: schliesst das Fenster "Kargarok Flight buttons", falls offen.
void closeForTest();
// Testbuild: reine Tests der Belegung (actions::runPureTests) und Szenarien steuerung-s2 bis steuerung-s9 und
// steuerung-demo (core/steuerung_test.cpp). Von panel::registerTestScenarios aufgerufen.
void registerTestScenarios();
#endif

}  // namespace kff::steuerung
