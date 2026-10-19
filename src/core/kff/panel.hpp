// Einstellungsfeld "Kargarok Flight" im Mods-Fenster (UiService). Nur Anzeige und Bindung an die Konfig-Variablen.
#pragma once

#include "core/kff/services.hpp"

namespace kff::panel {

// Registriert das Mods-Panel des Pakets. Ohne UiService (svc_ui == nullptr) geschieht nichts.
// Gibt immer MOD_OK zurueck: ein Fehler im Panel darf die Mod nicht deaktivieren.
void install();

// Baut den Inhalt (Abschnitte Flight, Display, Controls, Experimental, Fussezeile) in `pane`. Dieselbe
// Funktion fuellt das Mods-Panel und das Testfenster (Testbuild). Gibt immer MOD_OK zurueck.
ModResult buildContents(UiElementHandle pane);

// Aktualisiert die Knopf-Beschriftung des zuletzt gebauten Inhalts, nur bei einer Aenderung.
// Der Panel-Update ruft das selbst; das Testfenster (Testbuild) ruft es aus seinem eigenen Update.
void updateContents();

#if defined(KFF_DEV_AUTOTEST)
// Testbuild: Szenarien panel-umschalt und panel-fenster registrieren (src/core/panel_test.cpp).
void registerTestScenarios();
#endif

}  // namespace kff::panel
