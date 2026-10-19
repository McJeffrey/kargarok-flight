// Fenster mit dem Tab "Flight buttons" (UiService 2.3): je ein SELECT pro Flugaktion (E-9). Der Tab-Name ist der sichtbare
// Titel des Fensters (K3). Die Hilfetexte stehen rechts (help_rml). UiWindowDesc hat in UiService 2.3 kein Titelfeld.
// Alle Rueckrufe geben nichts zurueck und lassen keine Ausnahme heraus (ui.h); Fehler gehen ins Log.
#include "core/kff/steuerung.hpp"

#include "core/actions.hpp"
#include "core/kff/actions.hpp"
#include "core/kff/log.hpp"

#include <cstdio>
#include <cstring>

namespace kff::steuerung {
namespace {

constexpr char kIntro[] = "Each flight action uses one GameCube button. You set the controller button or key for each "
                          "GameCube button in Dusklight under Settings, Input, Configure Inputs.";
constexpr char kFixed[] = "Fixed buttons: the Control Stick steers. Held fully to one side, it flies circles. Held fully up "
                          "or down, it flies loops. The C Stick moves the camera. Start pauses.";
constexpr char kStatusFree[] = "If the button you pick is used by another flight action, the two actions swap, "
                               "if that is possible.";

struct Row {
    actions::ActId id;
    const char* label;
    const char* help;
};

constexpr Row kFlying[] = {
    {actions::ActId::Faster, "Faster (hold)",
        "Faster (hold). Default: R. Hold to speed up, also from a standstill. Top speed takes about 3 seconds. An analog "
        "trigger changes faster the harder you press it. Release to keep your speed. No effect while landing."},
    {actions::ActId::Slower, "Slower (hold)",
        "Slower (hold). Default: L. Hold to slow down until the bird stands still in the air: that is the hover. Release "
        "to keep your speed. An analog trigger changes faster the harder you press it. No effect while landing."},
    {actions::ActId::GearUp, "Speed up (tap)",
        "Speed up (tap). Default: D-Pad Up. Each tap adds 20 percent of top speed. From hover, a tap ends the hover at "
        "20 percent. No effect while landing."},
    {actions::ActId::GearDown, "Speed down (tap)",
        "Speed down (tap). Default: D-Pad Down. Each tap takes away 20 percent of top speed. At zero, the bird hovers. "
        "No effect while hovering or landing."},
    {actions::ActId::Dash, "Dash",
        "Dash. Default: A. Tap to dash forward. Not while you hold Brake or while landing, and for about 3 seconds "
        "after a dash starts. From hover, it ends the hover and speeds up from a standstill."},
    {actions::ActId::BarrelRoll, "Barrel roll",
        "Barrel roll. Default: Y. Tap to roll once: a full turn in about 2 seconds. The direction follows the Control "
        "Stick. Not while landing or hovering."},
    {actions::ActId::Brake, "Brake (hold)",
        "Brake (hold). Default: Z. Hold to slow down to 30 percent of top speed at most. Holding it also ends a running "
        "dash. Release it to return to your speed. Brake does not change the hover, but while you hold it, Dash is blocked. "
        "No effect while landing."},
};

constexpr Row kCamera[] = {
    {actions::ActId::CameraBehind, "Camera behind bird",
        "Camera behind bird. Default: X. Tap to swing the camera behind the bird."},
    {actions::ActId::CameraCloser, "Camera closer (hold)",
        "Camera closer (hold). Default: D-Pad Right. Hold to move the camera closer."},
    {actions::ActId::CameraFarther, "Camera farther (hold)",
        "Camera farther (hold). Default: D-Pad Left. Hold to move the camera farther away."},
};

constexpr Row kCall[] = {
    {actions::ActId::CallHold, "Call: hold",
        "Call: hold. Default: R. On L or R, press it firmly and keep it held, then tap the Call: then tap button "
        "to call the bird."},
    {actions::ActId::CallTap, "Call: then tap",
        "Call: then tap. Default: B, also used by Dismount. While you hold Call: hold, tap this button to call the bird."},
    {actions::ActId::Dismount, "Dismount",
        "Dismount. Default: B. While riding, tap to land: the bird slows down and sinks. "
        "With no ground or water below, Link lets go at once. Tap again to let go at once."},
};

// Auswahllisten: alle Aktionen mit "Not set" an Position 0 (12 Eintraege). Pflichtaktionen ohne "Not set" (K1: 11).
const char* g_options[kff::kBindableCount] = {};
const char* g_optionsPflicht[kff::kBindableCount - 1] = {};
UiWindowHandle g_window = 0;   // offenes Fenster, sonst 0 (on_closed setzt zurueck)
UiElementHandle g_status = 0;  // Statuszeile des aktuellen Tabs (neu bei jedem Aufbau)
UiElementHandle g_rowElem[actions::kActCount] = {};  // Zeile je Aktion (K10, neu bei jedem Aufbau, 0 = keine)
UiTabDesc g_tab = UI_TAB_DESC_INIT;
UiDialogAction g_dialogActions[2] = {};

void fillOptions() {
    for (size_t i = 0; i < kff::kBindableCount; ++i) {
        g_options[i] = kff::kBindable[i].label;
    }
    for (size_t i = 0; i + 1 < kff::kBindableCount; ++i) {
        g_optionsPflicht[i] = kff::kBindable[i + 1].label;  // Index i der Liste = Belegungsindex i+1 (K1)
    }
}

void* userOf(actions::ActId id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint8_t>(id)));
}

actions::ActId idOf(void* user) {
    return static_cast<actions::ActId>(static_cast<uint8_t>(reinterpret_cast<uintptr_t>(user)));
}

void setStatus(const char* text) {
    if (svc_ui != nullptr && g_status != 0) {
        (void)svc_ui->elem_set_text(mod_ctx, g_status, text);
    }
}

// Gemeinsamer Weg fuer das Setzen einer Auswahl (SELECT und Test): applyChange, speichern, Statuszeile.
void applySelect(actions::ActId id, int64_t value) {
    if (value < 0 || value >= static_cast<int64_t>(kff::kBindableCount)) {
        setStatus("Not possible: unknown button.");
        return;
    }
    const actions::Aenderung r = actions::setFromUi(id, static_cast<uint8_t>(value));
    setStatus(r.text.c_str());
}

// Rueckrufe des SELECT (UiControlGetFn, UiControlSetFn). Getter billig: nur der Index.
void getSelect(ModContext*, void* user, UiControlValue* out) {
    try {
        if (out != nullptr) {
            const actions::ActId id = idOf(user);
            out->int_value = actions::uiIndexOf(id, actions::indexOf(id));
        }
    } catch (...) {
        logError("steuerung: Ausnahme beim Lesen der Auswahl");
    }
}

void setSelect(ModContext*, void* user, const UiControlValue* value) {
    try {
        if (value != nullptr) {
            const actions::ActId id = idOf(user);
            const int64_t ui = value->int_value;
            // Ausserhalb der Liste: -1, applySelect weist das als unbekannte Taste ab (K1).
            const int64_t bind = (ui < 0 || ui >= actions::optionCount(id))
                ? -1 : actions::bindIndexOf(id, static_cast<int>(ui));
            applySelect(id, bind);
        }
    } catch (...) {
        logError("steuerung: Ausnahme beim Setzen der Auswahl");
    }
}

void addText(UiElementHandle pane, const char* text, UiElementHandle* out) {
    if (svc_ui->pane_add_text(mod_ctx, pane, text, out) != MOD_OK) {
        logWarn("steuerung: Textzeile nicht angelegt");
    }
}

void addSection(UiElementHandle pane, const char* title) {
    if (svc_ui->pane_add_section(mod_ctx, pane, title) != MOD_OK) {
        logWarn("steuerung: Abschnitt '%s' nicht angelegt", title);
    }
}

// Zeilen-Handles leeren: vor jedem Aufbau des Tabs und beim Schliessen des Fensters (K10).
void clearRows() {
    for (size_t i = 0; i < actions::kActCount; ++i) {
        g_rowElem[i] = 0;
    }
}

void addRow(UiElementHandle pane, const Row& r) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_SELECT;
    desc.label = r.label;
    desc.help_rml = r.help;
    desc.binding = UI_BINDING_CALLBACKS;
    desc.get = getSelect;
    desc.set = setSelect;
    desc.user_data = userOf(r.id);
    const bool pflicht = actions::mandatory(r.id);  // K1: Pflichtaktionen ohne "Not set"
    desc.options = pflicht ? g_optionsPflicht : g_options;
    desc.option_count = static_cast<size_t>(actions::optionCount(r.id));
    UiElementHandle elem = 0;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, &elem) != MOD_OK) {
        logWarn("steuerung: Auswahl '%s' nicht angelegt", r.label);
    } else {
        g_rowElem[static_cast<size_t>(r.id)] = elem;  // K10: Handle fuer den Fokus im Test
        logInfo("steuerung: Auswahl %s: %d Optionen%s", r.label, actions::optionCount(r.id),
            pflicht ? ", ohne Not set" : "");
    }
}

void addRows(UiElementHandle pane, const Row* rows, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        addRow(pane, rows[i]);
    }
}

void onDialogReset(ModContext*, UiDialogHandle, void*) {
    try {
        actions::resetAll();
        setStatus("All flight buttons are back on the default layout.");
    } catch (...) {
        logError("steuerung: Ausnahme beim Zuruecksetzen");
    }
}

void onDialogCancel(ModContext*, UiDialogHandle, void*) {}

// Knopf "Reset flight buttons to default": Bestaetigung mit den Aktionen Reset und Cancel.
void onResetPressed(ModContext*, void*) {
    try {
        if (svc_ui == nullptr) {
            return;
        }
        g_dialogActions[0] = UI_DIALOG_ACTION_INIT;
        g_dialogActions[0].label = "Reset";
        g_dialogActions[0].on_pressed = onDialogReset;
        g_dialogActions[1] = UI_DIALOG_ACTION_INIT;
        g_dialogActions[1].label = "Cancel";
        g_dialogActions[1].on_pressed = onDialogCancel;
        UiDialogDesc d = UI_DIALOG_DESC_INIT;
        d.title = "Reset flight buttons";
        d.body_rml = "Set all flight buttons back to the default layout?";
        d.actions = g_dialogActions;
        d.action_count = 2;
        UiDialogHandle dialog = 0;
        const ModResult r = svc_ui->dialog_push(mod_ctx, &d, &dialog);
        if (r != MOD_OK) {
            logWarn("steuerung: Bestaetigung nicht geoeffnet (%d)", static_cast<int>(r));
        }
    } catch (...) {
        logError("steuerung: Ausnahme beim Oeffnen der Bestaetigung");
    }
}

void addResetButton(UiElementHandle pane) {
    UiControlDesc desc = UI_CONTROL_DESC_INIT;
    desc.kind = UI_CONTROL_BUTTON;
    desc.label = "Reset flight buttons to default";
    desc.tooltip = "Sets every flight action back to its default button.";
    desc.on_pressed = onResetPressed;
    if (svc_ui->pane_add_control(mod_ctx, pane, &desc, nullptr) != MOD_OK) {
        logWarn("steuerung: Knopf Reset nicht angelegt");
    }
}

// Tab-Aufbau (UiTabBuildFn): linke Spalte in der Reihenfolge aus E-9. Die Hilfe rechts kommt von den SELECT-Zeilen.
ModResult buildTab(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle, void*, ModError*) {
    g_status = 0;
    clearRows();
    try {
        if (svc_ui == nullptr || left == 0) {
            return MOD_OK;
        }
        fillOptions();
        addText(left, kIntro, nullptr);
        addText(left, kStatusFree, &g_status);  // K9: direkt unter dem Einleitungstext, ohne Scrollen sichtbar
        addSection(left, "Flying");
        addRows(left, kFlying, sizeof kFlying / sizeof kFlying[0]);
        addSection(left, "Camera");
        addRows(left, kCamera, sizeof kCamera / sizeof kCamera[0]);
        addSection(left, "Call and dismount");
        addRows(left, kCall, sizeof kCall / sizeof kCall[0]);
        addSection(left, "Fixed");
        addText(left, kFixed, nullptr);
        addResetButton(left);
    } catch (...) {
        logError("steuerung: Ausnahme beim Aufbau des Fensters");
    }
    return MOD_OK;
}

void onClosed(ModContext*, UiWindowHandle, void*) {
    g_window = 0;
    g_status = 0;
    clearRows();
}

}  // namespace

ModResult openWindow() {
    if (svc_ui == nullptr) {
        logInfo("steuerung: kein UiService, kein Fenster");
        return MOD_OK;
    }
    if (g_window != 0) {
        return MOD_OK;  // schon offen
    }
    g_tab = UI_TAB_DESC_INIT;
    g_tab.title = "Flight buttons";
    g_tab.build = buildTab;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &g_tab;
    desc.tab_count = 1;
    desc.on_closed = onClosed;
    const ModResult r = svc_ui->window_push(mod_ctx, &desc, &g_window);
    if (r != MOD_OK) {
        g_window = 0;
        logWarn("steuerung: window_push fehlgeschlagen (%d)", static_cast<int>(r));
    }
    return r;
}

void onOpenPressed(ModContext*, void*) {
    try {
        (void)openWindow();
    } catch (...) {
        logError("steuerung: Ausnahme beim Oeffnen des Fensters");
    }
}

void formatButtonLabel(char* buf, size_t size) {
    const int n = actions::changedCount();
    if (n == 0) {
        std::snprintf(buf, size, "Change flight buttons (default)");
    } else {
        std::snprintf(buf, size, "Change flight buttons (%d changed)", n);
    }
}

#if defined(KFF_DEV_AUTOTEST)
void selectForTest(uint8_t action, int64_t value) {
    if (action >= static_cast<uint8_t>(actions::ActId::Count)) {
        return;
    }
    applySelect(static_cast<actions::ActId>(action), value);
}

// Testbuild: Fokus auf die Zeile der Aktion (elem_focus, K10). MOD_UNAVAILABLE, solange die Zeile noch nicht gebaut ist.
ModResult focusForTest(uint8_t action) {
    if (svc_ui == nullptr || action >= static_cast<uint8_t>(actions::ActId::Count)) {
        return MOD_UNAVAILABLE;
    }
    const UiElementHandle elem = g_rowElem[action];
    if (elem == 0) {
        return MOD_UNAVAILABLE;
    }
    return svc_ui->elem_focus(mod_ctx, elem);
}

void closeForTest() {
    if (svc_ui != nullptr && g_window != 0) {
        (void)svc_ui->window_close(mod_ctx, g_window);
    }
}
#endif

}  // namespace kff::steuerung
