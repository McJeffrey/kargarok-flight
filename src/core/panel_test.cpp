// Testbuild (KFF_DEV_AUTOTEST): zwei Szenarien fuer das Einstellungsfeld. Nur in der Testbuild-Quellliste (CMake).
//
//   panel-umschalt  mensch_ritt zur Laufzeit per svc_config->set_bool (derselbe Konfig-Weg wie das Panel): AN, dann Ruf
//                   als Mensch und Ritt. Waehrend des Rittes AUS; der Ritt muss weiterlaufen. Nach dem Absteigen Ruf als
//                   Mensch mit AUS: abgelehnt, autowolf verwandelt wie bisher. Stage F_SP103,0,0,-1 (Link als Mensch).
//   panel-fenster   Testfenster per svc_ui->window_push. Sein Tab ruft dieselbe Aufbaufunktion wie das Mods-Panel
//                   (kff::panel::buildContents). Keine Eingaben; der Screenshot zeigt das Fenster.
#include "core/kff/panel.hpp"

#include "core/kff/autotest.hpp"
#include "core/kff/config.hpp"
#include "core/kff/log.hpp"
#include "core/kff/services.hpp"
#include "core/kff/session.hpp"
#include "core/kff/steuerung.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdint>

namespace kff::panel {
namespace {

using autotest::Context;
using autotest::Scenario;

// ---------------------------------------------------------------------------------------------
// panel-umschalt
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kUmReadyAt = 240;        // frueheste Ruf-Tick (Ticks ab dem ersten Link-Frame)
constexpr uint32_t kUmReadyLimit = 3000;    // Ticks auf einen bereiten Link, sonst Fehler
constexpr uint32_t kUmHold = 30;            // R gehalten, Ticks
constexpr uint32_t kUmPressOffset = 15;     // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kUmOffRideFrame = 60;    // Ritt-Frame, ab dem mensch_ritt AUS gesetzt wird
constexpr uint32_t kUmDismountFrame = 150;  // Ritt-Frame, ab dem B zum Absteigen kommt
constexpr uint32_t kUmRideWait = 600;       // Ticks auf den Ritt nach dem Ruf, sonst Fehler
constexpr uint32_t kUmLandWait = 900;       // Ticks auf das Ende des Rittes nach B, sonst Fehler
constexpr uint32_t kUmPause = 60;           // Ticks zwischen Rittende und dem AUS-Ruf
constexpr uint32_t kUmTail = 300;           // Ticks nach dem AUS-Ruf, dann fertig

enum class UmStage : uint8_t {
    Ready,       // wartet auf Link als Mensch ohne Sitzung, setzt AN
    CallOn,      // R halten, B tippen (AN)
    WaitRideOn,  // wartet auf Phase Riding
    RideOn,      // Ritt: AUS waehrend des Rittes, B zum Absteigen
    Landing,     // wartet auf Phase Idle
    Pause,       // Pause nach dem Ritt, dann Ruf mit AUS
    CallOff,     // R halten, B tippen (AUS)
    Tail,        // Beobachtung, dann fertig
    Done,
};

struct UmState {
    UmStage stage = UmStage::Ready;
    uint32_t t0 = 0;         // Tick des Stufenbeginns
    uint32_t callStart = 0;  // erster R-Tick des Rufes
    bool offSet = false;     // AUS im Ritt schon gesetzt
};
UmState g_um;

// Konfig-Weg des Panels: set_bool auf die Variable mensch_ritt, danach zur Kontrolle gelesen.
bool setMenschRitt(bool value) {
    const uint64_t var = config::handleOf(config::Key::MenschRitt);
    if (svc_config == nullptr || var == 0) {
        kff::logError("panel-umschalt: Konfig-Variable mensch_ritt fehlt");
        return false;
    }
    if (svc_config->set_bool(mod_ctx, var, value) != MOD_OK) {
        kff::logError("panel-umschalt: set_bool fehlgeschlagen");
        return false;
    }
    bool read = !value;
    (void)svc_config->get_bool(mod_ctx, var, &read);
    kff::logInfo("panel-umschalt: mensch_ritt auf %d gesetzt (set_bool), gelesen %d", value ? 1 : 0, read ? 1 : 0);
    return read == value;
}

// Ruf als Mensch moeglich (wie me_can_call in mensch.cpp): Link ist Mensch, sitzt nicht, kein Ereignis, keine Sitzung.
bool humanReady(const Context& c) {
    daAlink_c* link = kff::liveLink();
    return link != nullptr && link->checkWolf() == 0 && !link->checkCargoCarry() && !link->checkEventRun() &&
           !dComIfGp_event_runCheck() && c.sess.phase == kff::Phase::Idle && c.sess.birdId == kff::kNoProc;
}

// R halten und in der Mitte B tippen. `start` ist der erste R-Tick. Gibt true, sobald der Ruf abgeschlossen ist.
bool pressCall(Context& c, uint32_t start) {
    const uint32_t t = c.frame;
    if (t >= start && t < start + kUmHold) {
        c.holdR();
    }
    if (t == start + kUmPressOffset) {
        c.tapB();
    }
    return t >= start + kUmHold;
}

bool umschaltFail(Context& c, const char* why) {
    c.note(why);
    g_um.stage = UmStage::Done;
    return true;
}

bool umschaltTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_um.stage) {
    case UmStage::Ready:
        if (t >= kUmReadyLimit) {
            return umschaltFail(c, "panel-umschalt: FEHLER kein bereiter Link als Mensch");
        }
        if (t >= kUmReadyAt && humanReady(c)) {
            if (!setMenschRitt(true)) {
                return umschaltFail(c, "panel-umschalt: Umschalten fehlgeschlagen, Abbruch");
            }
            g_um.callStart = t + 10;
            g_um.t0 = t;
            g_um.stage = UmStage::CallOn;
            c.note("panel-umschalt: AN gesetzt, Ruf als Mensch folgt");
        }
        break;
    case UmStage::CallOn:
        if (pressCall(c, g_um.callStart)) {
            g_um.t0 = t;
            g_um.stage = UmStage::WaitRideOn;
            c.note("panel-umschalt: Ruf als Mensch abgesetzt (AN)");
        }
        break;
    case UmStage::WaitRideOn:
        if (c.sess.phase == kff::Phase::Riding) {
            g_um.t0 = t;
            g_um.stage = UmStage::RideOn;
            c.note("panel-umschalt: Ritt laeuft (AN)");
        } else if (t - g_um.t0 > kUmRideWait) {
            return umschaltFail(c, "panel-umschalt: FEHLER kein Ritt nach dem Ruf (AN)");
        }
        break;
    case UmStage::RideOn:
        if (c.sess.phase != kff::Phase::Riding) {
            return umschaltFail(c, "panel-umschalt: FEHLER der Ritt endete vor dem Absteigen");
        }
        if (!g_um.offSet && c.sess.rideFrames >= kUmOffRideFrame) {
            g_um.offSet = true;
            (void)setMenschRitt(false);
            c.note("panel-umschalt: AUS waehrend des Rittes, der Ritt laeuft weiter");
        }
        if (c.sess.rideFrames >= kUmDismountFrame) {
            c.tapB();
            c.note("panel-umschalt: B zum Absteigen");
            g_um.t0 = t;
            g_um.stage = UmStage::Landing;
        }
        break;
    case UmStage::Landing:
        if (c.sess.phase == kff::Phase::Idle) {
            g_um.t0 = t;
            g_um.stage = UmStage::Pause;
            c.note("panel-umschalt: Ritt beendet");
        } else if (t - g_um.t0 > kUmLandWait) {
            return umschaltFail(c, "panel-umschalt: FEHLER Ritt nach B nicht beendet");
        }
        break;
    case UmStage::Pause:
        if (t - g_um.t0 >= kUmPause && humanReady(c)) {
            g_um.callStart = t + 10;
            g_um.t0 = t;
            g_um.stage = UmStage::CallOff;
            c.note("panel-umschalt: Ruf als Mensch mit AUS (erwartet: abgelehnt, autowolf)");
        } else if (t - g_um.t0 > kUmRideWait) {
            return umschaltFail(c, "panel-umschalt: FEHLER Link nicht bereit fuer den Ruf mit AUS");
        }
        break;
    case UmStage::CallOff:
        if (pressCall(c, g_um.callStart)) {
            g_um.t0 = t;
            g_um.stage = UmStage::Tail;
        }
        break;
    case UmStage::Tail:
        if (t - g_um.t0 >= kUmTail) {
            c.note("panel-umschalt: fertig");
            g_um.stage = UmStage::Done;
            return true;
        }
        break;
    case UmStage::Done:
        return true;
    }
    return false;
}

const Scenario kUmschalt = {
    "panel-umschalt",
    "Einstellungsfeld: mensch_ritt per set_bool AN (Ruf als Mensch, Ritt), AUS waehrend des Rittes, dann Ruf als Mensch mit AUS",
    umschaltTick,
};

// ---------------------------------------------------------------------------------------------
// panel-fenster
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kFeOpenAt = 240;  // Tick, ab dem das Testfenster offen ist (bleibt bis zum Ende des Laufs)

UiWindowHandle g_feWindow = 0;
bool g_feOpened = false;
UiTabDesc g_feTab = UI_TAB_DESC_INIT;

// Tab-Aufbau: dieselbe Funktion wie das Mods-Panel.
ModResult feTabBuild(ModContext*, UiWindowHandle, UiElementHandle leftPane, UiElementHandle, void*, ModError*) {
    return buildContents(leftPane);
}

bool fensterTick(Context& c) {
    if (g_feOpened || c.frame < kFeOpenAt) {
        return false;
    }
    g_feOpened = true;
    if (svc_ui == nullptr) {
        c.note("panel-fenster: kein UiService, kein Testfenster");
        return false;
    }
    g_feTab.title = "Kargarok Flight";  // wie der Name im Mods-Fenster (mod.json), nur Testbuild
    g_feTab.build = feTabBuild;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &g_feTab;
    desc.tab_count = 1;
    const ModResult r = svc_ui->window_push(mod_ctx, &desc, &g_feWindow);
    kff::logInfo("panel-fenster: window_push -> %d, Fenster %llu", static_cast<int>(r),
        static_cast<unsigned long long>(g_feWindow));
    c.note(r == MOD_OK ? "panel-fenster: Testfenster offen" : "panel-fenster: window_push fehlgeschlagen");
    return false;
}

const Scenario kFenster = {
    "panel-fenster",
    "Einstellungsfeld: Testfenster per window_push, Tab ruft denselben Aufbau wie das Mods-Panel, keine Eingaben",
    fensterTick,
};

// ---------------------------------------------------------------------------------------------
// panel-live
// ---------------------------------------------------------------------------------------------
// Live-Wirkung der Schalter im laufenden Ritt, nur Mod-interne Pad-Eingabe (Context). Die Markierung
// "kff-shot: <name>" im Log loest in tools/run-panel.sh je einen Screenshot des Spielfensters aus (shot-<name>.png).
//   hud_anzeigen        Gang-Zeile aus, an, aus per set_bool; dazwischen ein Testfenster mit demselben Panel
//   sichtfeld_effekt    Sichtfeld-Zusatz im Log: sofort 0 nach dem Ausschalten, wieder > 0 bei hohem Gang
//   lenkung_invertieren Wert, den die Steuerung je Frame liest (Log)

constexpr uint32_t kLvReadyAt = 240;      // frueheste Ruf-Tick
constexpr uint32_t kLvReadyLimit = 3000;  // Ticks auf einen bereiten Link, sonst Fehler
constexpr uint32_t kLvRideWait = 600;     // Ticks auf den Ritt nach dem Ruf, sonst Fehler
constexpr uint32_t kLvSettle = 90;        // Ticks Ritt vor der Bildfolge

enum class LvStage : uint8_t { Ready, CallOn, WaitRideOn, Settle, Script, Done };

enum class LvAct : uint8_t { Log, Shot, SetHud, OpenWin, CloseWin, SetInvert, Gear, SetFov, FovLog, Stop };

struct LvEvt {
    uint32_t at;       // Ticks ab Beginn der Bildfolge
    LvAct act;
    bool on;           // Wert fuer SetHud, SetInvert, SetFov
    const char* text;  // Log-Text, oder Markierung "kff-shot: <name>"
};

const LvEvt kLvScript[] = {
    {0, LvAct::Log, false, "panel-live: Ritt laeuft, hud_anzeigen im Ausgangsstand an"},
    {60, LvAct::Shot, false, "kff-shot: 01-spiel-hud-an"},
    {90, LvAct::SetHud, false, nullptr},
    {150, LvAct::Shot, false, "kff-shot: 02-spiel-hud-aus"},
    {180, LvAct::SetHud, true, nullptr},
    {240, LvAct::Shot, false, "kff-shot: 03-spiel-hud-an"},
    {270, LvAct::SetHud, false, nullptr},
    {330, LvAct::Shot, false, "kff-shot: 04-spiel-hud-aus"},
    {360, LvAct::SetHud, true, nullptr},
    {390, LvAct::OpenWin, false, nullptr},
    {450, LvAct::Shot, false, "kff-shot: 05-fenster-hud-an"},
    {480, LvAct::SetHud, false, nullptr},
    {540, LvAct::Shot, false, "kff-shot: 06-fenster-hud-aus"},
    {570, LvAct::CloseWin, false, nullptr},
    {600, LvAct::Shot, false, "kff-shot: 07-spiel-hud-aus"},
    {630, LvAct::SetHud, true, nullptr},
    {660, LvAct::Shot, false, "kff-shot: 08-spiel-hud-an"},
    {690, LvAct::SetInvert, true, nullptr},
    {720, LvAct::SetInvert, false, nullptr},
    {750, LvAct::Gear, false, nullptr},
    {780, LvAct::Gear, false, nullptr},
    {810, LvAct::Gear, false, nullptr},
    {840, LvAct::FovLog, false, nullptr},
    {870, LvAct::SetFov, false, nullptr},
    {871, LvAct::FovLog, false, nullptr},
    {900, LvAct::SetFov, true, nullptr},
    {901, LvAct::FovLog, false, nullptr},
    {930, LvAct::Stop, false, nullptr},
};
constexpr size_t kLvScriptCount = sizeof(kLvScript) / sizeof(kLvScript[0]);

UiWindowHandle g_lvWindow = 0;
bool g_lvWindowOpen = false;

// Konfig-Weg des Panels fuer einen beliebigen bool-Schluessel: set_bool, danach zur Kontrolle gelesen.
bool setBoolKey(config::Key key, const char* name, bool value) {
    const uint64_t var = config::handleOf(key);
    if (svc_config == nullptr || var == 0) {
        kff::logError("panel-live: Konfig-Variable %s fehlt", name);
        return false;
    }
    if (svc_config->set_bool(mod_ctx, var, value) != MOD_OK) {
        kff::logError("panel-live: set_bool %s fehlgeschlagen", name);
        return false;
    }
    bool read = !value;
    (void)svc_config->get_bool(mod_ctx, var, &read);
    kff::logInfo("panel-live: %s auf %d gesetzt (set_bool), gelesen %d", name, value ? 1 : 0, read ? 1 : 0);
    return read == value;
}

// Testfenster mit demselben Tab-Aufbau wie das Mods-Panel (feTabBuild -> buildContents).
void lvOpenWindow(Context& c) {
    if (svc_ui == nullptr) {
        c.note("panel-live: kein UiService, kein Testfenster");
        return;
    }
    g_feTab.title = "Kargarok Flight";  // wie der Name im Mods-Fenster (mod.json), nur Testbuild
    g_feTab.build = feTabBuild;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &g_feTab;
    desc.tab_count = 1;
    const ModResult r = svc_ui->window_push(mod_ctx, &desc, &g_lvWindow);
    g_lvWindowOpen = r == MOD_OK;
    kff::logInfo("panel-live: window_push -> %d, Fenster %llu", static_cast<int>(r),
        static_cast<unsigned long long>(g_lvWindow));
    c.note(g_lvWindowOpen ? "panel-live: Testfenster offen" : "panel-live: window_push fehlgeschlagen");
}

void lvCloseWindow(Context& c) {
    if (svc_ui == nullptr || !g_lvWindowOpen) {
        return;
    }
    const ModResult r = svc_ui->window_close(mod_ctx, g_lvWindow);
    g_lvWindowOpen = false;
    kff::logInfo("panel-live: window_close -> %d", static_cast<int>(r));
    c.note("panel-live: Testfenster geschlossen");
}

void lvRunEvent(Context& c, const LvEvt& ev) {
    switch (ev.act) {
    case LvAct::Log:
    case LvAct::Shot:
        c.note(ev.text);
        break;
    case LvAct::SetHud:
        (void)setBoolKey(config::Key::HudShow, "hud_anzeigen", ev.on);
        break;
    case LvAct::OpenWin:
        lvOpenWindow(c);
        break;
    case LvAct::CloseWin:
        lvCloseWindow(c);
        break;
    case LvAct::SetInvert:
        (void)setBoolKey(config::Key::InvertSteer, "lenkung_invertieren", ev.on);
        kff::logInfo("panel-live: Steuerung liest lenkung_invertieren=%d je Frame",
            config::getBool(config::Key::InvertSteer) ? 1 : 0);
        break;
    case LvAct::Gear:
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
        c.note("panel-live: Gang hoch (D-Pad oben)");
        break;
    case LvAct::SetFov:
        (void)setBoolKey(config::Key::FovEffect, "sichtfeld_effekt", ev.on);
        break;
    case LvAct::FovLog:
        kff::logInfo("panel-live: Tick %u Gang %d sichtfeld_effekt=%d fovExtraDeg=%.2f", c.frame, c.sess.gear,
            config::getBool(config::Key::FovEffect) ? 1 : 0, static_cast<double>(c.sess.fovExtraDeg));
        break;
    case LvAct::Stop:
        c.note("panel-live: fertig");
        break;
    }
}

struct LvState {
    LvStage stage = LvStage::Ready;
    uint32_t t0 = 0;
    uint32_t callStart = 0;
    uint32_t scriptStart = 0;
    size_t next = 0;
};
LvState g_lv;

bool lvFail(Context& c, const char* why) {
    c.note(why);
    g_lv.stage = LvStage::Done;
    return true;
}

bool liveTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_lv.stage) {
    case LvStage::Ready:
        if (t >= kLvReadyLimit) {
            return lvFail(c, "panel-live: FEHLER kein bereiter Link als Mensch");
        }
        if (t >= kLvReadyAt && humanReady(c)) {
            if (!setMenschRitt(true)) {
                return lvFail(c, "panel-live: mensch_ritt setzen fehlgeschlagen, Abbruch");
            }
            g_lv.callStart = t + 10;
            g_lv.stage = LvStage::CallOn;
            c.note("panel-live: mensch_ritt AN, Ruf als Mensch folgt");
        }
        break;
    case LvStage::CallOn:
        if (pressCall(c, g_lv.callStart)) {
            g_lv.t0 = t;
            g_lv.stage = LvStage::WaitRideOn;
            c.note("panel-live: Ruf abgesetzt");
        }
        break;
    case LvStage::WaitRideOn:
        if (c.sess.phase == kff::Phase::Riding) {
            g_lv.t0 = t;
            g_lv.stage = LvStage::Settle;
            c.note("panel-live: Ritt laeuft");
        } else if (t - g_lv.t0 > kLvRideWait) {
            return lvFail(c, "panel-live: FEHLER kein Ritt nach dem Ruf");
        }
        break;
    case LvStage::Settle:
        if (c.sess.phase != kff::Phase::Riding) {
            return lvFail(c, "panel-live: FEHLER Ritt vorzeitig beendet");
        }
        if (t - g_lv.t0 >= kLvSettle) {
            g_lv.scriptStart = t;
            g_lv.stage = LvStage::Script;
        }
        break;
    case LvStage::Script:
        if (c.sess.phase != kff::Phase::Riding) {
            return lvFail(c, "panel-live: FEHLER Ritt waehrend der Bildfolge beendet");
        }
        // Sanfter Kurs: der Watchdog (Stillstand ohne Stick) beendet sonst den Ritt nach 300 Ticks.
        c.stick(0.25f, 0.0f);
        while (g_lv.next < kLvScriptCount && t - g_lv.scriptStart >= kLvScript[g_lv.next].at) {
            const LvEvt& ev = kLvScript[g_lv.next];
            ++g_lv.next;
            lvRunEvent(c, ev);
            if (ev.act == LvAct::Stop) {
                g_lv.stage = LvStage::Done;
                return true;
            }
        }
        break;
    case LvStage::Done:
        return true;
    }
    return false;
}

const Scenario kLive = {
    "panel-live",
    "Einstellungsfeld live im Ritt: hud_anzeigen aus/an/aus mit Bildfolge, Testfenster mit demselben Panel, Gang hoch, sichtfeld_effekt und lenkung_invertieren per Log",
    liveTick,
};

}  // namespace

void registerTestScenarios() {
    steuerung::registerTestScenarios();  // S3: reine Tests und Szenarien steuerung-s2 bis -s9, -demo (steuerung_test.cpp)
    (void)autotest::registerScenario(&kUmschalt);
    (void)autotest::registerScenario(&kFenster);
    (void)autotest::registerScenario(&kLive);
}

}  // namespace kff::panel
