// Einstellungsfeld "Kargarok Flight" im Mods-Fenster (UiService 2.3). Der Host zeigt davor die Beschreibung aus mod.json.
// Aufbau: Abschnitte Flight, Display, Controls, Experimental und eine Fussezeile. Das Panel bleibt kurz. Die
// Flugtasten stehen im Fenster "Flight buttons" (core/steuerung.cpp), das der Knopf im Abschnitt Controls oeffnet.
//
// Schalter und Zahl sind UI_BINDING_CONFIG_VAR an die Konfig-Variablen gebunden (ohne Zwischenspeicher). Der Schalter
// der Tipps bindet ueber Rueckrufe, weil das Einschalten den Zaehler tipps_rest auf kTipRides setzt.
// Mensch-Ritt wird beim Ruf gelesen (mensch.cpp human_call), der Startgang beim Ritt-Start (tempo.cpp onRideStart).
// HUD und Sichtfeld liest tempo.cpp je Tick (tick, aufgerufen aus onFrameBird), die Lenkung flug.cpp je Frame.
// Die Flugtasten wirken ab dem naechsten Pad-Tick (E-7). Die Beschriftung des Knopfes folgt der wirksamen Belegung
// (steuerung::formatButtonLabel) und wird bei jeder Aenderung neu gesetzt.
//
// Regel aus dem UiService-Header: Ein Nicht-MOD_OK oder eine Ausnahme aus build/update deaktiviert die ganze Mod.
// Darum geben alle Funktionen hier MOD_OK zurueck, Fehler gehen nur ins Log.
// Entladen und Neuladen: der Host entfernt das Panel beim Abmelden der Mod (ui_remove_mod). Hier ist nichts zu tun.
#include "core/kff/panel.hpp"

#include "core/kff/config.hpp"
#include "core/kff/log.hpp"
#include "core/kff/steuerung.hpp"
#include "core/kff/version.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace kff::panel {
namespace {

UiElementHandle g_button = 0;
char g_lastLabel[64] = "";

void addSection(UiElementHandle pane, const char* title) {
    if (svc_ui->pane_add_section(mod_ctx, pane, title) != MOD_OK) {
        logWarn("panel: Abschnitt '%s' nicht angelegt", title);
    }
}

void addText(UiElementHandle pane, const char* text, UiElementHandle* outElem) {
    if (svc_ui->pane_add_text(mod_ctx, pane, text, outElem) != MOD_OK) {
        logWarn("panel: Textzeile nicht angelegt: %s", text);
    }
}

// Schalter fuer eine bool-Konfig-Variable. Standardwert und Speicherung kommen von der Variable.
void addToggle(UiElementHandle pane, const char* label, const char* tooltip, config::Key key) {
    const uint64_t var = config::handleOf(key);
    if (var == 0) {
        logWarn("panel: '%s' ohne Konfig-Variable, Zeile fehlt", label);
        return;
    }
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = label;
    desc.tooltip = tooltip;
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = var;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr) != MOD_OK) {
        logWarn("panel: Schalter '%s' nicht angelegt", label);
    }
}

// Schalter "Show tips on your first rides" (tipps_anzeigen, Standard AN). Rueckrufe statt Konfig-Bindung: Beim Einschalten
// wird der Zaehler tipps_rest auf kTipRides gesetzt, damit der Tooltip ("Switch it on again ...") stimmt.
bool tipsModified(ModContext*, void*) {
    return !config::getBool(config::Key::TipsShow);
}

void getTipsShow(ModContext*, void*, UiControlValue* out) {
    if (out != nullptr) {
        out->bool_value = config::getBool(config::Key::TipsShow);
    }
}

void setTipsShow(ModContext*, void*, const UiControlValue* value) {
    try {
        if (value == nullptr) {
            return;
        }
        const bool on = value->bool_value;
        if (!config::setBool(config::Key::TipsShow, on)) {
            logWarn("panel: tipps_anzeigen nicht gespeichert");
        }
        if (on && !config::setInt(config::Key::TipsRest, config::kTipRides)) {
            logWarn("panel: tipps_rest nicht gesetzt");
        }
        logInfo("panel: Tipps %s, Zaehler %lld", on ? "an" : "aus",
            static_cast<long long>(config::getInt(config::Key::TipsRest)));
    } catch (...) {
        logError("panel: Ausnahme beim Setzen der Tipps");
    }
}

void addTipsToggle(UiElementHandle pane) {
    if (config::handleOf(config::Key::TipsShow) == 0) {
        logWarn("panel: 'Show tips' ohne Konfig-Variable, Zeile fehlt");
        return;  // ohne Speicherung hielte der Schalter nicht
    }
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_TOGGLE;
    desc.label = "Show tips on your first rides";
    desc.tooltip = "Shows short button tips on your first 3 rides. Switch it on again to see them on the next 3 rides. "
                   "Applies from your next ride.";
    desc.binding = UI_BINDING_CALLBACKS;
    desc.get = getTipsShow;
    desc.set = setTipsShow;
    desc.is_modified = tipsModified;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr) != MOD_OK) {
        logWarn("panel: Schalter Tipps nicht angelegt");
    }
}

// Zahl fuer eine int-Konfig-Variable mit Grenzen. Dieselben Grenzen klemmt config.cpp (kDefs) beim Lesen.
void addNumber(UiElementHandle pane, const char* label, const char* tooltip, config::Key key, int64_t lo, int64_t hi) {
    const uint64_t var = config::handleOf(key);
    if (var == 0) {
        logWarn("panel: '%s' ohne Konfig-Variable, Zeile fehlt", label);
        return;
    }
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_NUMBER;
    desc.label = label;
    desc.tooltip = tooltip;
    desc.binding = UI_BINDING_CONFIG_VAR;
    desc.config_var = var;
    desc.min = lo;
    desc.max = hi;
    desc.step = 1;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr) != MOD_OK) {
        logWarn("panel: Zahl '%s' nicht angelegt", label);
    }
}

// Knopf "Change flight buttons (...)": oeffnet das Fenster mit den Flugtasten. Beschriftung je nach Anzahl Aenderungen.
void addButton(UiElementHandle pane) {
    steuerung::formatButtonLabel(g_lastLabel, sizeof g_lastLabel);
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_BUTTON;
    desc.label = g_lastLabel;
    desc.tooltip = "Opens the Flight buttons window. Pick one GameCube button for each action.";
    desc.on_pressed = steuerung::onOpenPressed;
    UiElementHandle elem = 0;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, &elem) != MOD_OK) {
        logWarn("panel: Knopf Change buttons nicht angelegt");
        return;
    }
    g_button = elem;
}

// Panel-Update (jeder Frame, solange das Panel sichtbar ist): Beschriftung des Knopfes nur bei Aenderung setzen.
void updateLines() {
    char label[sizeof g_lastLabel];
    steuerung::formatButtonLabel(label, sizeof label);
    if (g_button != 0 && std::strcmp(label, g_lastLabel) != 0) {
        if (svc_ui->control_set_label(mod_ctx, g_button, label) == MOD_OK) {
            std::snprintf(g_lastLabel, sizeof g_lastLabel, "%s", label);
        }
    }
}

ModResult buildForMods(ModContext*, UiElementHandle pane, void*, ModError*) {
    g_button = 0;
    return buildContents(pane);
}

ModResult updateForMods(ModContext*, void*, ModError*) {
    try {
        updateLines();
    } catch (...) {
        logError("panel: Ausnahme beim Aktualisieren");
    }
    return MOD_OK;
}

}  // namespace

ModResult buildContents(UiElementHandle pane) {
    if (svc_ui == nullptr || pane == 0) {
        g_button = 0;  // kein Aufbau: alten Handle verwerfen (updateLines ueberspringt 0)
        return MOD_OK;
    }
    try {
        addSection(pane, "Flight");
        addNumber(pane, "Start speed (1 to 5)",
            "The speed each ride starts at: 1 to 5 mean 30, 40, 50, 75 or 100 percent of top speed. Step 3 (50 percent) "
            "is the default. Applies on your next call.",
            config::Key::StartGear, 1, 5);
        addToggle(pane, "Invert pitch (stick up lifts the nose)",
            "Off (default): stick up tips the nose down, like a plane. On: stick up lifts the nose. "
            "While hovering, On makes stick up climb instead of sink. Applies at once.",
            config::Key::InvertSteer);

        addSection(pane, "Display");
        addToggle(pane, "Show speed and hover",
            "Shows the speed in percent, or Hover when the bird stands still, the approximate km/h and the Dash status "
            "at the top of the screen while you ride. Applies at once.",
            config::Key::HudShow);
        addToggle(pane, "Wider view at high speed",
            "Widens the view by up to 10 degrees above 50 percent of top speed, the full 10 at top speed. Applies at once.",
            config::Key::FovEffect);
        addTipsToggle(pane);

        addSection(pane, "Controls");
        addButton(pane);

        addSection(pane, "Experimental");
        addToggle(pane, "Ride as Human Link",
            "Experimental. Link rides as a human, with the same camera and controls as the wolf ride. Applies on your next call.", config::Key::MenschRitt);

        addText(pane, "Version " KFF_MOD_VERSION ". Start speed and Ride as Human Link apply on your next call. "
                      "Show tips applies from your next ride. Everything else applies at once.", nullptr);
    } catch (...) {
        logError("panel: Ausnahme beim Aufbau, Panel unvollstaendig");
    }
    return MOD_OK;
}

void updateContents() {
    try {
        updateLines();
    } catch (...) {
        logError("panel: Ausnahme beim Aktualisieren");
    }
}

void install() {
    if (svc_ui == nullptr) {
        logInfo("panel: kein UiService, kein Einstellungsfeld");
        return;
    }
    UiModsPanelDesc desc = UI_MODS_PANEL_DESC_INIT;
    desc.build = buildForMods;
    desc.update = updateForMods;
    const ModResult r = svc_ui->register_mods_panel(mod_ctx, &desc);
    if (r != MOD_OK) {
        logWarn("panel: register_mods_panel fehlgeschlagen (%d), kein Einstellungsfeld", static_cast<int>(r));
        return;
    }
    logInfo("panel: Mods-Panel registriert");
}

}  // namespace kff::panel
