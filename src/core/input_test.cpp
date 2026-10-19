// WP3-Testszenarien der Eingabe. Nur Testbuild (CMake KFF_DEV_AUTOTEST=ON), Auswahl mit KFF_AUTOTEST=<name>.
//
//   wp3-karte-ritt   F_SP121 (Wolf): Ritt, Karte wie die Aktionsbelegung OPEN_MAP_SCREEN, B allein schliesst die Karte,
//                    zweite Karte mit R+B (Ruf abgelehnt, B schliesst), der Ritt laeuft weiter (E5, WP3.2)
//   wp3-ui-halt      F_SP121 (Wolf): Ritt, Dusklight-Dialog (dialog_push) 150 Ticks sichtbar, Drift und Hoehe, nach dem
//                    Schliessen kein Sprung; zweiter Dialog im Landeanflug (ui_halt)
//   wp3-chord-b      F_SP103 (Mensch): R+B dreimal, B kommt bei Link an (KFF_TEST_OHNE_AUTOWOLF=1 schaltet autowolf ab,
//                    sonst verwandelt autowolf Link beim ersten Ruf) (WP3.5, Teil 1)
//   wp3-dialog-b     F_SP124 (Startdialog): R+B im Dialog, B blaettert den Text weiter (WP3.5, Teil 2)
//   wp3-schild       F_SP103 (Mensch, KFF_TEST_OHNE_AUTOWOLF=1): T1 digitales R ohne Lock + B, T2 Zielsperre + R-Flanke
//                    + B (WP3.6)
//   wp3-schild-wolf  F_SP121 (Wolf): T8 Zielsperre + R+B, T11 Pausen-Latch (R+B im Menue, Start+R+B im selben Tick),
//                    T12 R+B+Steuerkreuz oben im selben Tick (WP3.6, D8)
//   wp3-r-analog     F_SP121 (Wolf): Spiel-Lock gemessen (Sweep), R analog 0,44 und 0,61 mit B, Lock im Ritt und nach dem
//                    Ritt (RK (a), P2-04, WP3.7)
//
// Eingaben nur ueber den Kern-Kontext (Pad-Injektion). Ergebniszeilen "kff: wp3: ERGEBNIS <szenario> ok|fail ...".
// Bildmarken "kff: autotest: SHOT <name>" (der Runner run-autotest-wp3.sh macht ein Bild des eigenen Fensters).
#include "d/dolzel_rel.h"

#include "core/input_core.hpp"
#include "core/kff/autotest.hpp"
#include "core/kff/input.hpp"
#include "core/kff/log.hpp"
#include "core/kff/module.hpp"
#include "core/kff/services.hpp"
#include "core/kff/session.hpp"

#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace kff::autotest {
namespace {

constexpr uint32_t kStart = static_cast<uint32_t>(PAD_BUTTON_START);
constexpr uint32_t kUp = static_cast<uint32_t>(PAD_BUTTON_UP);
constexpr uint32_t kTrigL = static_cast<uint32_t>(PAD_TRIGGER_L);
constexpr uint32_t kTrigR = static_cast<uint32_t>(PAD_TRIGGER_R);

int win() {
    return static_cast<int>(dMeter2Info_getWindowStatus());
}

unsigned pauseFlag() {
    return static_cast<unsigned>(dComIfGp_isPauseFlag());
}

const char* reasonNow() {
    const char* r = core::rideCallBlockReason(liveLink());
    return r != nullptr ? r : "(kein Grund, Ruf ginge)";
}

uint32_t rejected() {
    return core::inputCounts().callRejected;
}

uint32_t chords() {
    return core::inputCounts().chordConsumed;
}

uint32_t restored() {
    return core::inputCounts().chordRestored;
}

bool envOn(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] == '1';
}

void shot(Context& c, const char* name) {
    char b[96];
    std::snprintf(b, sizeof b, "SHOT %s", name);
    c.note(b);
}

void result(const char* name, bool ok, const char* fmt, ...) KFF_PRINTF(3, 4);
void result(const char* name, bool ok, const char* fmt, ...) {
    char body[400];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof body, fmt, args);
    va_end(args);
    logInfo("wp3: ERGEBNIS %s %s %s fremdeingabe=%u", name, ok ? "ok" : "fail", body,
        static_cast<unsigned>(foreignInputCount()));
}

// Eigene Voegel in der Welt (fpcNm_KAGO_e mit mType 1).
int countBird(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e && static_cast<daKago_c*>(ac)->mType == kFriendlyKagoType) {
        ++*static_cast<int*>(data);
    }
    return 1;
}

int ownBirds() {
    int n = 0;
    fopAcIt_Executor(countBird, &n);
    return n;
}

bool birdPos(cXyz& out) {
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    out = k->current.pos;
    return true;
}

float horiz(const cXyz& a, const cXyz& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Ruf-Vorlauf: ab Frame 300 und canCall R 30 Ticks halten, B bei +15. Optional analog (f) statt Digital-R.
// 1 = Ritt steht, 0 = warten, -1 = kein Ritt (nach drei Versuchen).
struct Prep {
    int stage = 0;
    uint32_t t0 = 0;
    uint32_t tries = 0;
};

int prepRide(Context& c, Prep& p, float analog = -1.0f) {
    const uint32_t t = c.frame;
    if (c.sess.phase == Phase::Riding) {
        return 1;
    }
    if (p.stage == 0) {
        if (t >= 300 && c.canCall()) {
            p.stage = 1;
            p.t0 = t;
            ++p.tries;
            c.note(analog >= 0.0f ? "wp3: Ruf (R analog halten, B tippen)" : "wp3: Ruf (R halten, B tippen)");
        } else if (t > 5000) {
            return -1;
        }
        return 0;
    }
    const uint32_t m = t - p.t0;
    if (m < 30) {
        if (analog >= 0.0f) {
            c.holdRAnalog(analog);
        } else {
            c.holdR();
        }
    }
    if (m == 15) {
        c.tapB();
    }
    if (m > 700) {
        if (p.tries >= 3) {
            return -1;
        }
        p.stage = 0;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// wp3-karte-ritt
// ---------------------------------------------------------------------------------------------

uint32_t consumedEdges() {
    return core::inputCounts().consumedEdges;
}

struct KarteState {
    Prep prep;
    int stage = 0;
    uint32_t t0 = 0;
    uint32_t rejBefore = 0;
    uint32_t chordBefore = 0;
    uint32_t edgesBefore = 0;   // vom Kern im Ritt verbrauchte Flanken vor der ersten Karte
    int winOpen1 = -1;
    int winOpen2 = -1;
    int pressesB = 0;           // B-Tipps (ohne R) bei offener Karte 1
    int pressesToClose = 0;     // B-Tipps bis Karte 1 zu war
    bool closedB = false;
    uint32_t closeTicks1 = 0;
    int pressesB2 = 0;          // B-Tipps nach R+B bei offener Karte 2
    bool closed2 = false;
    bool closedByRB = false;
    bool landing = false;
    bool phaseLost = false;
    uint32_t rejDelta = 0;
    uint32_t chordDelta = 0;
    uint32_t edgesInMap = 0;
    uint32_t rideMark = 0;
    uint32_t rideMapMark = 0;
    uint32_t rideInMap = 0;
    int lastWin = -1;
};
KarteState g_karte;

// Wie die Aktionsbelegung OPEN_MAP_SCREEN (dMeterMap_c::meter_map_move, d_meter_map.cpp, TARGET_PC): Kartenstatus 2 und
// Tastenrichtung 0x400 unter denselben Bedingungen. Das Steuerkreuz verbraucht der Kern im Ritt, die Aktionsbelegung
// (Tastatur oder Controller) ist der Kartenweg im Ritt. Sie ist nicht per Pad-Injektion erreichbar.
void karteOpen(const char* tag) {
    const unsigned before = dMeter2Info_getMapStatus();
    const bool frei = !dComIfGp_event_runCheck() && !dMsgObject_isTalkNowCheck() && (before == 0 || before == 1) &&
                      !dMeter2Info_isSub2DStatus(1);
    if (frei) {
        dMeter2Info_setMapStatus(2);
        dMeter2Info_setMapKeyDirection(0x400);
    }
    logInfo("wp3: karte-ritt: Karte %s angefordert (wie Aktionsbelegung OPEN_MAP_SCREEN), frei=%d mapStatus %u -> %u "
            "Fenster=%d pause=%u",
        tag, frei ? 1 : 0, before, static_cast<unsigned>(dMeter2Info_getMapStatus()), win(), pauseFlag());
}

bool karteTick(Context& c) {
    KarteState& g = g_karte;
    const uint32_t t = c.frame;
    if (g.stage == 0) {
        const int r = prepRide(c, g.prep);
        if (r < 0) {
            result("karte-ritt", false, "kein Ritt");
            return true;
        }
        if (r == 0) {
            return false;
        }
        g.stage = 1;
        g.t0 = t;
        c.note("wp3: karte-ritt: Ritt steht");
        return false;
    }
    const uint32_t m = t - g.t0;
    if (c.sess.phase != Phase::Riding) {
        g.phaseLost = true;
    }
    if (c.sess.landFrames > 0) {
        g.landing = true;
    }
    if (win() != g.lastWin) {
        logInfo("wp3: karte-ritt: Fenster %d -> %d (mapStatus=%u pause=%u, Stufe %d, m=%u)", g.lastWin, win(),
            static_cast<unsigned>(dMeter2Info_getMapStatus()), pauseFlag(), g.stage, m);
        g.lastWin = win();
    }
    switch (g.stage) {
    case 1:
        if (m == 90) {
            g.edgesBefore = consumedEdges();
            karteOpen("1");
            g.rideMapMark = c.sess.rideFrames;
            g.stage = 2;
            g.t0 = t;
        }
        break;
    case 2:  // Karte 1: B allein, alle 60 Ticks, bis die Karte zu ist (B zoomt erst heraus, dann schliesst es)
        if (m == 60) {
            g.winOpen1 = win();
            g.rideInMap = c.sess.rideFrames - g.rideMapMark;
            logInfo("wp3: karte-ritt: Karte 1 nach 60 Ticks, Fenster=%d mapStatus=%u pause=%u phase=%s Ritt-Frames seit "
                    "dem Oeffnen=%u Voegel=%d",
                win(), static_cast<unsigned>(dMeter2Info_getMapStatus()), pauseFlag(), phaseName(c.sess.phase),
                g.rideInMap, ownBirds());
            shot(c, "karte-offen");
            g.rejBefore = rejected();
            g.chordBefore = chords();
        }
        if (m >= 90 && (m - 90) % 60 == 0 && !g.closedB && g.pressesB < 4) {
            ++g.pressesB;
            c.tapB();
            logInfo("wp3: karte-ritt: B allein bei offener Karte (%d), Fenster=%d", g.pressesB, win());
        }
        if (m > 90 && !g.closedB && win() == 0) {
            g.closedB = true;
            g.closeTicks1 = m - 90;
            g.pressesToClose = g.pressesB;
            logInfo("wp3: karte-ritt: Karte 1 durch B geschlossen nach %d B-Tipps (%u Ticks), phase=%s landFrames=%u",
                g.pressesB, g.closeTicks1, phaseName(c.sess.phase), c.sess.landFrames);
        }
        if (m == 400) {
            logInfo("wp3: karte-ritt: nach Karte 1 Fenster=%d phase=%s landFrames=%u chord+%u rejected+%u verbraucht+%u",
                win(), phaseName(c.sess.phase), c.sess.landFrames, chords() - g.chordBefore, rejected() - g.rejBefore,
                consumedEdges() - g.edgesBefore);
            shot(c, "nach-karte");  // Ritt nach Karte 1, vor Karte 2 (der Runner nimmt das Bild mit Verzoegerung auf)
            g.stage = 3;
            g.t0 = t;
        }
        break;
    case 3:
        if (m == 90) {
            karteOpen("2");
            g.stage = 4;
            g.t0 = t;
        }
        break;
    case 4:  // Karte 2: R+B (Ruf abgelehnt), danach B allein, bis die Karte zu ist
        if (m == 60) {
            g.winOpen2 = win();
            logInfo("wp3: karte-ritt: Karte 2 nach 60 Ticks, Fenster=%d pause=%u", win(), pauseFlag());
        }
        if (m >= 80 && m < 100) {
            c.holdR();
        }
        if (m == 90) {
            g.rejBefore = rejected();
            g.chordBefore = chords();
            logInfo("wp3: karte-ritt: R+B bei offener Karte, Grund jetzt: %s", reasonNow());
            c.tapB();
        }
        if (m > 90 && !g.closed2 && win() == 0) {
            g.closed2 = true;
            g.closedByRB = g.pressesB2 == 0;
            logInfo("wp3: karte-ritt: Karte 2 geschlossen nach %u Ticks (%s), phase=%s", m - 90,
                g.closedByRB ? "durch das B von R+B" : "durch B allein", phaseName(c.sess.phase));
        }
        if (m >= 150 && (m - 150) % 60 == 0 && !g.closed2 && g.pressesB2 < 4) {
            ++g.pressesB2;
            c.tapB();
            logInfo("wp3: karte-ritt: B allein bei offener Karte 2 (%d), Fenster=%d", g.pressesB2, win());
        }
        if (m == 450) {
            g.rejDelta = rejected() - g.rejBefore;
            g.chordDelta = chords() - g.chordBefore;
            g.edgesInMap = consumedEdges() - g.edgesBefore;
            g.rideMark = c.sess.rideFrames;
            g.stage = 5;
            g.t0 = t;
        }
        break;
    case 5:
        if (m == 150) {
            const uint32_t rideAfter = c.sess.rideFrames - g.rideMark;
            const int birds = ownBirds();
            const bool ok = g.winOpen1 == 4 && g.winOpen2 == 4 && g.closedB && g.closed2 && g.edgesInMap == 0 &&
                            g.rejDelta >= 1 && g.chordDelta == 0 && !g.landing && !g.phaseLost && rideAfter >= 100 &&
                            birds == 1;
            result("karte-ritt", ok,
                "fenster1=%d fenster2=%d bTippsBisZu=%d(%u Ticks) karte2Zu=%d(rb=%d, bTipps=%d) rbAbgelehnt=%u chord=%u "
                "verbrauchteFlanken=%u landeanflug=%d rittVerloren=%d rittFramesInKarte=%u rittFramesDanach=%u voegel=%d",
                g.winOpen1, g.winOpen2, g.closedB ? g.pressesToClose : -1, g.closeTicks1, g.closed2 ? 1 : 0,
                g.closedByRB ? 1 : 0, g.pressesB2, g.rejDelta, g.chordDelta, g.edgesInMap, g.landing ? 1 : 0,
                g.phaseLost ? 1 : 0, g.rideInMap, rideAfter, birds);
            return true;
        }
        break;
    }
    return false;
}

const Scenario kKarteRitt = {
    "wp3-karte-ritt",
    "WP3.2: Karte im Ritt (Aktionsbelegung), B bedient die Karte, R+B in der Karte abgelehnt, Ritt laeuft weiter",
    karteTick,
};

// ---------------------------------------------------------------------------------------------
// wp3-ui-halt
// ---------------------------------------------------------------------------------------------

void dlgPressed(ModContext*, UiDialogHandle, void*) {}

bool pushDialog(UiDialogHandle* out, const char* body) {
    if (svc_ui == nullptr) {
        return false;
    }
    static UiDialogAction action = UI_DIALOG_ACTION_INIT;
    action.label = "OK";
    action.on_pressed = dlgPressed;
    action.keep_open = false;
    UiDialogDesc d = UI_DIALOG_DESC_INIT;
    d.title = "Kargarok Flight Test (WP3)";
    d.body_rml = body;
    d.actions = &action;
    d.action_count = 1;
    return svc_ui->dialog_push(mod_ctx, &d, out) == MOD_OK;
}

void closeDialog(UiDialogHandle h, bool pushed) {
    if (pushed && svc_ui != nullptr) {
        svc_ui->dialog_close(mod_ctx, h);
    }
}

// Ablauf: Ritt 1 kurz geradeaus, B (Landeanflug), sofort ein Dialog 90 Ticks, schliessen, Landung. Danach Ritt 2: 150
// Ticks geradeaus, Dialog 150 Ticks (B allein, R+B), schliessen, 30 Ticks ohne Sprung, B.
struct HaltState {
    Prep prep;
    int stage = 0;
    uint32_t t0 = 0;
    UiDialogHandle dlg = 0;
    bool pushed = false;
    bool visibleSeen = false;
    cXyz p0;
    cXyz prev;
    cXyz pc;
    // Teil 1: Landeanflug
    bool landStarted = false;
    bool landPushed = false;
    float landDocDy = 0.0f;
    float landDocDrift = 0.0f;
    uint32_t landFramesAtDoc = 0;
    uint32_t landFramesAfterDoc = 0;
    bool rideEndInLandDoc = false;
    bool landEnded = false;
    uint32_t landEndAfter = 0;  // Ticks nach dem Schliessen bis zum Ritt-Ende
    // Teil 2: Reiseflug
    float cruiseStep = 0.0f;    // mittlerer Weg je Tick vor dem Dokument
    float docDrift = 0.0f;      // horizontaler Weg waehrend des Dokuments
    float docDyMax = 0.0f;      // groesste Hoehenaenderung waehrend des Dokuments
    float afterDyMax = 0.0f;    // groesste Hoehenaenderung in 30 Ticks nach dem Schliessen
    float afterStepMax = 0.0f;  // groesster Weg je Tick in 30 Ticks nach dem Schliessen
    float afterMoved = 0.0f;    // horizontaler Weg in 30 Ticks nach dem Schliessen
    bool dismountInDoc = false;
    uint32_t rejBefore = 0;
    uint32_t rejDelta = 0;
};
HaltState g_halt;

void haltResult(const HaltState& g, bool complete) {
    const bool landOk = g.landStarted && g.landPushed && !g.rideEndInLandDoc && g.landDocDy < 20.0f &&
                        g.landDocDrift < 20.0f && g.landFramesAfterDoc == g.landFramesAtDoc && g.landEnded;
    const bool cruiseOk = g.pushed && g.visibleSeen && g.docDrift < 50.0f && g.docDyMax < 50.0f && g.afterDyMax < 50.0f &&
                          g.afterStepMax <= g.cruiseStep * 1.2f + 5.0f && g.afterMoved > 0.0f && !g.dismountInDoc &&
                          g.rejDelta >= 1;
    result("ui-halt", complete && landOk && cruiseOk,
        "LAND(anflug=%d dialog=%d hoeheDok=%.1f driftDok=%.1f landFrames=%u->%u rittEndeImDok=%d rittEnde=%d "
        "endeNach=%u) REISE(schritt=%.1f driftDok=%.1f hoeheDok=%.1f hoeheNach30=%.1f schrittNachMax=%.1f wegNach30=%.1f "
        "absteigenImDok=%d rbAbgelehnt=%u) vollstaendig=%d",
        g.landStarted ? 1 : 0, g.landPushed ? 1 : 0, static_cast<double>(g.landDocDy),
        static_cast<double>(g.landDocDrift), g.landFramesAtDoc, g.landFramesAfterDoc, g.rideEndInLandDoc ? 1 : 0,
        g.landEnded ? 1 : 0, g.landEndAfter, static_cast<double>(g.cruiseStep), static_cast<double>(g.docDrift),
        static_cast<double>(g.docDyMax), static_cast<double>(g.afterDyMax), static_cast<double>(g.afterStepMax),
        static_cast<double>(g.afterMoved), g.dismountInDoc ? 1 : 0, g.rejDelta, complete ? 1 : 0);
}

bool haltTick(Context& c) {
    HaltState& g = g_halt;
    const uint32_t t = c.frame;
    const uint32_t m = t - g.t0;
    cXyz pos;
    const bool havePos = birdPos(pos);
    switch (g.stage) {
    case 0: {  // Ritt 1
        const int r = prepRide(c, g.prep);
        if (r < 0) {
            result("ui-halt", false, "kein Ritt 1");
            return true;
        }
        if (r == 1) {
            g.stage = 1;
            g.t0 = t;
            c.note("wp3: ui-halt: Ritt 1 steht");
        }
        break;
    }
    case 1:  // 60 Ticks geradeaus, dann B (Landeanflug), im naechsten Tick der Dialog
        if (m == 60) {
            c.tapB();
            c.note("wp3: ui-halt: B fuer den Landeanflug");
        }
        if (m == 61) {
            g.landStarted = c.sess.phase == Phase::Riding && c.sess.landFrames > 0;
            g.landFramesAtDoc = c.sess.landFrames;
            if (havePos) {
                g.p0 = pos;
                g.prev = pos;
            }
            if (g.landStarted) {
                g.landPushed = pushDialog(&g.dlg, "WP3 ui_halt: Dokument im Landeanflug, der Vogel muss stehen.");
            }
            logInfo("wp3: ui-halt: Landeanflug=%d landFrames=%u phase=%s, Dialog %s, Vogel y=%.1f", g.landStarted ? 1 : 0,
                c.sess.landFrames, phaseName(c.sess.phase), g.landPushed ? "geoeffnet" : "nicht geoeffnet",
                havePos ? static_cast<double>(pos.y) : 0.0);
            g.stage = g.landPushed ? 2 : 3;
            g.t0 = t;
        }
        break;
    case 2:  // Dialog im Landeanflug, 90 Ticks
        if (havePos) {
            g.landDocDrift += horiz(pos, g.prev);
            const float dy = std::fabs(pos.y - g.p0.y);
            if (dy > g.landDocDy) {
                g.landDocDy = dy;
            }
            g.prev = pos;
        }
        if (c.sess.phase != Phase::Riding) {
            g.rideEndInLandDoc = true;
        }
        if (m == 30) {
            shot(c, "dokument-landeanflug");
        }
        if (m == 90) {
            g.landFramesAfterDoc = c.sess.landFrames;
            closeDialog(g.dlg, g.landPushed);
            logInfo("wp3: ui-halt: Landeanflug-Dialog geschlossen, Hoehe max %.1f, Drift %.1f, landFrames %u -> %u, "
                    "phase=%s",
                static_cast<double>(g.landDocDy), static_cast<double>(g.landDocDrift), g.landFramesAtDoc,
                g.landFramesAfterDoc, phaseName(c.sess.phase));
            g.stage = 3;
            g.t0 = t;
        }
        break;
    case 3:  // Ende von Ritt 1 abwarten, dann Ritt 2
        if (!g.landEnded && c.sess.phase != Phase::Riding) {
            g.landEnded = true;
            g.landEndAfter = m;
            logInfo("wp3: ui-halt: Ritt 1 beendet %u Ticks nach dem Schliessen (phase=%s)", m, phaseName(c.sess.phase));
        }
        if (m >= 600) {
            haltResult(g, false);
            return true;
        }
        if (g.landEnded && c.canCall()) {
            g.prep = Prep{};
            g.stage = 4;
            g.t0 = t;
        }
        break;
    case 4: {  // Ritt 2
        const int r = prepRide(c, g.prep);
        if (r < 0) {
            haltResult(g, false);
            return true;
        }
        if (r == 1) {
            g.stage = 5;
            g.t0 = t;
            c.note("wp3: ui-halt: Ritt 2 steht");
        }
        break;
    }
    case 5:  // 150 Ticks geradeaus (Reisetempo), Weg je Tick in den letzten 30 Ticks, dann der Dialog
        if (m == 120 && havePos) {
            g.prev = pos;
            g.p0 = pos;
        }
        if (m == 150 && havePos) {
            g.cruiseStep = horiz(pos, g.p0) / 30.0f;
            g.p0 = pos;
            g.prev = pos;
            g.rejBefore = rejected();
            g.pushed = pushDialog(&g.dlg, "WP3 ui_halt: Dokument sichtbar, der Vogel muss stehen.");
            logInfo("wp3: ui-halt: Dialog %s, Reiseweg je Tick %.1f, Vogel (%.1f,%.1f,%.1f)",
                g.pushed ? "geoeffnet" : "NICHT geoeffnet", static_cast<double>(g.cruiseStep),
                static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z));
            g.stage = 6;
            g.t0 = t;
        }
        break;
    case 6:  // Dokument sichtbar, 150 Ticks
        if (core::uiDocumentVisible()) {
            g.visibleSeen = true;
        }
        if (havePos) {
            g.docDrift += horiz(pos, g.prev);
            const float dy = std::fabs(pos.y - g.p0.y);
            if (dy > g.docDyMax) {
                g.docDyMax = dy;
            }
            g.prev = pos;
        }
        if (c.sess.phase != Phase::Riding || c.sess.landFrames > 0) {
            g.dismountInDoc = true;
        }
        if (m == 30) {
            shot(c, "dokument");
        }
        if (m == 60) {
            c.tapB();
            c.note("wp3: ui-halt: B allein bei sichtbarem Dokument");
        }
        if (m >= 85 && m < 100) {
            c.holdR();
        }
        if (m == 90) {
            logInfo("wp3: ui-halt: R+B bei sichtbarem Dokument, Grund jetzt: %s", reasonNow());
            c.tapB();
        }
        if (m == 150) {
            g.rejDelta = rejected() - g.rejBefore;
            closeDialog(g.dlg, g.pushed);
            if (havePos) {
                g.pc = pos;
                g.prev = pos;
            }
            logInfo("wp3: ui-halt: Dialog geschlossen, Drift im Dokument %.1f, Hoehe max %.1f, sichtbar=%d, phase=%s",
                static_cast<double>(g.docDrift), static_cast<double>(g.docDyMax), g.visibleSeen ? 1 : 0,
                phaseName(c.sess.phase));
            g.stage = 7;
            g.t0 = t;
        }
        break;
    case 7:  // 30 Ticks nach dem Schliessen: kein Sprung, dann B (Ritt-Ende)
        if (havePos && m >= 1 && m <= 30) {
            const float step = horiz(pos, g.prev);
            if (step > g.afterStepMax) {
                g.afterStepMax = step;
            }
            const float dy = std::fabs(pos.y - g.pc.y);
            if (dy > g.afterDyMax) {
                g.afterDyMax = dy;
            }
            g.prev = pos;
        }
        if (m == 30) {
            if (havePos) {
                g.afterMoved = horiz(pos, g.pc);
            }
            logInfo("wp3: ui-halt: 30 Ticks nach dem Schliessen, Weg %.1f, groesster Schritt %.1f, Hoehe max %.1f",
                static_cast<double>(g.afterMoved), static_cast<double>(g.afterStepMax),
                static_cast<double>(g.afterDyMax));
            shot(c, "nach-dokument");
        }
        if (m == 40) {
            c.tapB();
        }
        if (m == 60) {
            haltResult(g, true);
            return true;
        }
        break;
    }
    return false;
}

const Scenario kUiHalt = {
    "wp3-ui-halt",
    "ui_halt: Dusklight-Dialog im Landeanflug und im Reiseflug, der Vogel steht und haelt die Hoehe, danach kein Sprung",
    haltTick,
};

// ---------------------------------------------------------------------------------------------
// Gemeinsam fuer die Mensch-Tests (F_SP103): autowolf per Testschalter aus, Link frei am Boden
// ---------------------------------------------------------------------------------------------

void autowolfSwitch(bool& done) {
    if (done) {
        return;
    }
    done = true;
    if (envOn("KFF_TEST_OHNE_AUTOWOLF") && moduleActive(&modules::autowolf())) {
        disableModule(&modules::autowolf());
        logInfo("wp3: Modul autowolf fuer diesen Test abgeschaltet (KFF_TEST_OHNE_AUTOWOLF=1)");
    }
}

bool humanFree(daAlink_c* link) {
    return link != nullptr && link->checkWolf() == 0 && !link->checkEventRun() && !dComIfGp_event_runCheck() &&
           link->mLinkAcch.ChkGroundHit() && !link->checkCargoCarry();
}

// ---------------------------------------------------------------------------------------------
// wp3-chord-b
// ---------------------------------------------------------------------------------------------

struct ChordBState {
    bool switched = false;
    int stage = 0;
    uint32_t t0 = 0;
    int attempt = 0;
    uint32_t chordBefore = 0;
    uint32_t restBefore = 0;
    uint32_t rejBefore = 0;
    int linkB = 0;
    int good = 0;
    int linkBCount = 0;
    uint32_t chordSum = 0;
    uint32_t restSum = 0;
    uint32_t rejSum = 0;
};
ChordBState g_chordB;

bool chordBTick(Context& c) {
    ChordBState& g = g_chordB;
    autowolfSwitch(g.switched);
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    if (g.stage == 0) {
        if (t >= 300 && humanFree(link)) {
            g.stage = 1;
            g.t0 = t;
            logInfo("wp3: chord-b: Link als Mensch frei, proc=%u", static_cast<unsigned>(link->mProcID));
        } else if (t > 4000) {
            result("chord-b", false, "Link nie frei als Mensch");
            return true;
        }
        return false;
    }
    const uint32_t m = t - g.t0;
    if (m < 20) {
        c.holdR();
    }
    if (m == 10) {
        g.chordBefore = chords();
        g.restBefore = restored();
        g.rejBefore = rejected();
        logInfo("wp3: chord-b: Versuch %d R+B, Grund jetzt: %s", g.attempt + 1, reasonNow());
        c.tapB();
    }
    if (m == 11) {
        // Link-Frame des Ticks m == 10 ist gelaufen: hat Link die B-Flanke gesehen (setStickData, mItemTrigger)?
        g.linkB = link != nullptr && (link->mItemTrigger & daAlink_c::BTN_B) != 0 ? 1 : 0;
    }
    if (m == 60) {
        const uint32_t dc = chords() - g.chordBefore;
        const uint32_t dr = restored() - g.restBefore;
        const uint32_t dj = rejected() - g.rejBefore;
        g.chordSum += dc;
        g.restSum += dr;
        g.rejSum += dj;
        g.linkBCount += g.linkB;
        const bool good = dc == 1 && dr == 1 && dj == 0 && g.linkB == 1 && link != nullptr && link->checkWolf() == 0;
        if (good) {
            ++g.good;
        }
        logInfo("wp3: chord-b: Versuch %d chord+%u restored+%u rejected+%u linkB=%d wolf=%d proc=%u -> %s", g.attempt + 1,
            dc, dr, dj, g.linkB, link != nullptr ? static_cast<int>(link->checkWolf()) : -1,
            link != nullptr ? static_cast<unsigned>(link->mProcID) : 0xFFFFu, good ? "ok" : "abweichend");
        if (g.attempt == 0) {
            shot(c, "chord-b-1");
        }
    }
    if (m == 150) {
        ++g.attempt;
        if (g.attempt >= 3) {
            result("chord-b", g.good == 3,
                "versuche=3 gut=%d chord=%u restored=%u rejected=%u linkB=%d autowolfAktiv=%d", g.good, g.chordSum,
                g.restSum, g.rejSum, g.linkBCount, moduleActive(&modules::autowolf()) ? 1 : 0);
            return true;
        }
        g.t0 = t;
    }
    return false;
}

const Scenario kChordB = {
    "wp3-chord-b",
    "WP3.5: R+B als Mensch ohne uebernehmendes Modul, B geht an Link zurueck (chordRestored, mItemTrigger B)",
    chordBTick,
};

// ---------------------------------------------------------------------------------------------
// wp3-dialog-b
// ---------------------------------------------------------------------------------------------

struct DialogState {
    int stage = 0;                  // 0 warten auf Dialog, 1 warten auf ruhigen Tasten-Wartezustand, 2 Versuch laeuft
    uint32_t t0 = 0;
    uint32_t calmSince = 0;         // Tick, seit dem Nachricht und Status unveraendert auf eine Taste warten
    uint32_t calmId = 0xFFFFFFFFu;
    unsigned calmSt = 0xFFFFu;
    uint32_t lastTalkAt = 0;
    int attempt = 0;
    uint32_t idBefore = 0;
    unsigned stBefore = 0;
    bool moved = false;             // nach B: Status oder Nachricht gewechselt (Seite weiter oder Dialog zu)
    uint32_t movedAt = 0;
    int advances = 0;
    int closes = 0;                 // davon: B schloss das Textfenster (talk 1 -> 0)
    uint32_t rejBefore = 0;
    uint32_t rejSum = 0;
    uint32_t chordBefore = 0;
    uint32_t chordSum = 0;
    uint32_t restBefore = 0;
    uint32_t restSum = 0;
    uint32_t lastLogId = 0xFFFFFFFFu;
    unsigned lastLogSt = 0xFFFFu;
    uint32_t lastLogT = 0xFFFFFFFFu;
};
DialogState g_dialog;

// Status des Nachrichtenobjekts (d_msg_object.cpp: 6 Text laeuft, 5/8/9/14/16/20/21 warten auf eine Taste, 1 kein Text).
unsigned msgStatus() {
    return dMsgObject_getMsgObjectClass() != nullptr ? static_cast<unsigned>(dMsgObject_c::getStatus()) : 0xFFFFu;
}

// Nachrichten-ID nur mit Nachrichtenobjekt: dMsgObject_c::getMessageID liest ohne Nullpruefung (vor dem ersten Raum
// gibt es kein Objekt, Lauf wp3f-fix2-dialog-b: SIGSEGV in getMessageID beim Start).
uint32_t msgId() {
    return dMsgObject_getMsgObjectClass() != nullptr ? dMsgObject_getMessageID() : 0xFFFFFFFFu;
}

bool msgKeyWait(unsigned st) {
    return st == 5 || st == 8 || st == 9 || st == 14 || st == 16 || st == 20 || st == 21;
}

bool dialogFinish(const DialogState& g, bool talk) {
    // Jeder Versuch ist ein R+B im offenen Textfenster: der Ruf wird abgelehnt (Grund, B bleibt beim Spiel), kein Chord.
    const bool ok = g.advances >= 2 && g.chordSum == 0 && g.restSum == 0 && g.rejSum == static_cast<uint32_t>(g.attempt);
    result("dialog-b", ok, "versuche=%d weiter=%d davonZu=%d rejected=%u chord=%u restored=%u talkEnde=%d", g.attempt,
        g.advances, g.closes, g.rejSum, g.chordSum, g.restSum, talk ? 0 : 1);
    return true;
}

// Ablauf: sobald das Textfenster ruhig auf eine Taste wartet (Status 5/8/9/14/16/20/21, 10 Ticks unveraendert), R halten
// und B bei +5 tippen. Gezaehlt wird, ob danach Seite, Status oder Fenster wechseln. Bis zu 8 Versuche; nach dem Schliessen
// eines Fensters wartet der Test bis zu 900 Ticks auf weiteren Text desselben Ereignisses.
bool dialogTick(Context& c) {
    DialogState& g = g_dialog;
    const uint32_t t = c.frame;
    const bool talk = dMsgObject_isTalkNowCheck();
    const uint32_t id = msgId();
    const unsigned st = msgStatus();
    if ((t % 60 == 0 && t != g.lastLogT) || id != g.lastLogId || st != g.lastLogSt) {
        g.lastLogT = t;
        g.lastLogId = id;
        g.lastLogSt = st;
        logInfo("wp3: dialog-b: t=%u talk=%d msgId=%u status=%u event=%d Fenster=%d", t, talk ? 1 : 0, id, st,
            dComIfGp_event_runCheck() ? 1 : 0, win());
    }
    if (talk) {
        g.lastTalkAt = t;
    }
    if (!(talk && msgKeyWait(st) && id == g.calmId && st == g.calmSt)) {
        g.calmId = id;
        g.calmSt = st;
        g.calmSince = t;
    }
    const bool calm = talk && msgKeyWait(st) && t - g.calmSince >= 10;

    switch (g.stage) {
    case 0:
        if (talk && t >= 120) {
            g.stage = 1;
            shot(c, "dialog-start");
            logInfo("wp3: dialog-b: Dialog offen, msgId=%u status=%u", id, st);
        } else if (t > 6000) {
            result("dialog-b", false, "kein Dialog bis t=6000");
            return true;
        }
        return false;
    case 1:
        if (calm) {
            g.stage = 2;
            g.t0 = t;
            g.idBefore = id;
            g.stBefore = st;
            g.moved = false;
            g.rejBefore = rejected();
            g.chordBefore = chords();
            g.restBefore = restored();
            c.holdR();
            logInfo("wp3: dialog-b: Versuch %d: R halten, B bei +5, msgId=%u status=%u (ruhig seit %u Ticks)",
                g.attempt + 1, id, st, t - g.calmSince);
        } else if (t - g.lastTalkAt > 900 || t > 12000) {
            logInfo("wp3: dialog-b: kein weiterer Text seit %u Ticks (event=%d)", t - g.lastTalkAt,
                dComIfGp_event_runCheck() ? 1 : 0);
            return dialogFinish(g, talk);
        }
        return false;
    default: {
        const uint32_t m = t - g.t0;
        if (m < 10) {
            c.holdR();
        }
        if (m == 5) {
            logInfo("wp3: dialog-b: Versuch %d R+B, Grund jetzt: %s", g.attempt + 1, reasonNow());
            c.tapB();
        }
        if (m > 5 && !g.moved && (id != g.idBefore || st != g.stBefore || !talk)) {
            g.moved = true;
            g.movedAt = m - 5;
        }
        if (m == 70) {
            const uint32_t dj = rejected() - g.rejBefore;
            const uint32_t dc = chords() - g.chordBefore;
            const uint32_t dr = restored() - g.restBefore;
            g.rejSum += dj;
            g.chordSum += dc;
            g.restSum += dr;
            if (g.moved) {
                ++g.advances;
                if (!talk) {
                    ++g.closes;
                }
            }
            logInfo("wp3: dialog-b: Versuch %d msgId %u -> %u status %u -> %u talk=%d Wechsel nach %u Ticks "
                    "rejected+%u chord+%u restored+%u -> %s",
                g.attempt + 1, g.idBefore, id, g.stBefore, st, talk ? 1 : 0, g.moved ? g.movedAt : 0u, dj, dc, dr,
                g.moved ? (talk ? "weiter" : "Fenster zu") : "stehen");
            if (g.attempt == 1) {
                shot(c, "dialog-weiter");
            }
            ++g.attempt;
            if (g.attempt >= 8) {
                return dialogFinish(g, talk);
            }
            g.stage = 1;
        }
        return false;
    }
    }
}

const Scenario kDialogB = {
    "wp3-dialog-b",
    "WP3.5: R+B im Startdialog (F_SP124), Ruf abgelehnt, B blaettert den Text weiter",
    dialogTick,
};

// ---------------------------------------------------------------------------------------------
// wp3-schild (Mensch)
// ---------------------------------------------------------------------------------------------

struct SchildState {
    bool switched = false;
    int stage = 0;
    uint32_t t0 = 0;
    uint32_t chordBefore = 0;
    uint32_t restBefore = 0;
    uint32_t rejBefore = 0;
    // T1
    uint32_t t1Chord = 0;
    uint32_t t1Rest = 0;
    uint32_t t1Rej = 0;
    uint32_t t1LockTicks = 0;
    bool t1Guard = false;
    // T2
    uint32_t t2Chord = 0;
    uint32_t t2Rej = 0;
    int t2Attn = -1;
    char t2Reason[64] = "";
    uint32_t t2LockEdges = 0;
    bool t2Guard = false;
};
SchildState g_schild;

bool schildTick(Context& c) {
    SchildState& g = g_schild;
    autowolfSwitch(g.switched);
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    const interface_of_controller_pad& last = lastPad();
    if (g.stage == 0) {
        if (t >= 300 && humanFree(link)) {
            g.stage = 1;
            g.t0 = t;
            logInfo("wp3: schild: Link als Mensch frei, Schild=%d", daPy_py_c::checkShieldGet() ? 1 : 0);
        } else if (t > 4000) {
            result("schild", false, "Link nie frei als Mensch");
            return true;
        }
        return false;
    }
    c.gameLocks();
    const uint32_t m = t - g.t0;
    switch (g.stage) {
    case 1:  // T1: R nur digital (Analogwert 0, kein Spiel-Lock) + B
        if (m < 20) {
            c.hold(kTrigR);
            c.holdRAnalog(0.0f);
        }
        if (m >= 1 && m <= 25 && last.mHoldLockR != 0) {
            ++g.t1LockTicks;
        }
        if (m == 10) {
            g.chordBefore = chords();
            g.restBefore = restored();
            g.rejBefore = rejected();
            logInfo("wp3: schild: T1 digitales R + B, Grund jetzt: %s", reasonNow());
            c.tapB();
        }
        if (link != nullptr && link->mProcID == daAlink_c::PROC_GUARD_ATTACK) {
            g.t1Guard = true;
        }
        if (m == 60) {
            g.t1Chord = chords() - g.chordBefore;
            g.t1Rest = restored() - g.restBefore;
            g.t1Rej = rejected() - g.rejBefore;
            logInfo("wp3: schild: T1 chord+%u restored+%u rejected+%u R-Lock-Ticks=%u Schildangriff=%d", g.t1Chord,
                g.t1Rest, g.t1Rej, g.t1LockTicks, g.t1Guard ? 1 : 0);
        }
        if (m == 120) {
            g.stage = 2;
            g.t0 = t;
        }
        break;
    case 2:  // T2: L gehalten (Zielsperre), R analog 1,0 ab +30 (R-Flanke), B bei +40
        if (m < 80) {
            c.hold(kTrigL);
        }
        if (m >= 30 && m < 50) {
            c.holdRAnalog(1.0f);
        }
        if (m >= 31 && m <= 50 && last.mTrigLockR != 0) {
            ++g.t2LockEdges;
        }
        if (m == 40) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            g.t2Attn = link != nullptr && link->mAttention != nullptr && link->checkAttentionLock() ? 1 : 0;
            std::snprintf(g.t2Reason, sizeof g.t2Reason, "%s", reasonNow());
            logInfo("wp3: schild: T2 Zielsperre=%d R-Lock=%u, R+B, Grund jetzt: %s", g.t2Attn,
                static_cast<unsigned>(last.mHoldLockR), g.t2Reason);
            c.tapB();
            shot(c, "schild-t2");
        }
        if (link != nullptr && link->mProcID == daAlink_c::PROC_GUARD_ATTACK) {
            g.t2Guard = true;
        }
        if (m == 100) {
            g.t2Chord = chords() - g.chordBefore;
            g.t2Rej = rejected() - g.rejBefore;
            logInfo("wp3: schild: T2 chord+%u rejected+%u R-Flanken=%u Schildangriff=%d", g.t2Chord, g.t2Rej,
                g.t2LockEdges, g.t2Guard ? 1 : 0);
            logInfo("wp3: schild: T3 (Schildangriff als Vanilla-Kontrolle) nicht ausfuehrbar: Schild=%d im frischen "
                    "Spielstand, ein Schildwechsel zur Laufzeit laedt kein Modell (d_a_alink.cpp setShieldModel nur beim "
                    "Erzeugen)",
                daPy_py_c::checkShieldGet() ? 1 : 0);
            const bool t1ok = g.t1Chord == 1 && g.t1Rest == 1 && g.t1Rej == 0 && g.t1LockTicks == 0 && !g.t1Guard;
            const bool t2ok = g.t2Chord == 0 && g.t2Rej == 1 && g.t2Attn == 1 &&
                              std::strncmp(g.t2Reason, "Zielsperre", 10) == 0;
            result("schild", t1ok && t2ok,
                "T1(chord=%u restored=%u rejected=%u rLockTicks=%u schildangriff=%d) T2(zielsperre=%d grund=\"%s\" "
                "chord=%u rejected=%u rFlanken=%u schildangriff=%d) T3=nicht_ausfuehrbar",
                g.t1Chord, g.t1Rest, g.t1Rej, g.t1LockTicks, g.t1Guard ? 1 : 0, g.t2Attn, g.t2Reason, g.t2Chord, g.t2Rej,
                g.t2LockEdges, g.t2Guard ? 1 : 0);
            return true;
        }
        break;
    }
    return false;
}

const Scenario kSchild = {
    "wp3-schild",
    "WP3.6 T1/T2: digitales R ohne Lock + B (Ruf, B zurueck), Zielsperre + R-Flanke + B (abgelehnt)",
    schildTick,
};

// ---------------------------------------------------------------------------------------------
// wp3-schild-wolf
// ---------------------------------------------------------------------------------------------

struct WolfState {
    int stage = 0;
    uint32_t t0 = 0;
    uint32_t chordBefore = 0;
    uint32_t rejBefore = 0;
    uint32_t restBefore = 0;
    // T8
    int t8Attn = -1;
    char t8Reason[64] = "";
    uint32_t t8Rej = 0;
    uint32_t t8Chord = 0;
    // T11a
    int t11aWin = -1;
    char t11aReason[64] = "";
    uint32_t t11aRej = 0;
    bool t11aClosed = false;
    uint32_t t11aCloseAt = 0;
    bool t11aSummon = false;
    uint32_t t11aChord = 0;
    // T11b
    char t11bReason[64] = "";
    uint32_t t11bRej = 0;
    int t11bWin = -1;
    bool t11bClosed = false;
    bool t11bSummon = false;
    uint32_t t11bChord = 0;
    // T12
    uint32_t t12Chord = 0;
    uint32_t t12Rej = 0;
    uint32_t t12Rest = 0;
    int t12WinMax = 0;
    bool t12Summon = false;
};
WolfState g_wolf;

void copyReason(char* out, size_t n) {
    std::snprintf(out, n, "%s", reasonNow());
}

bool wolfTick(Context& c) {
    WolfState& g = g_wolf;
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    if (g.stage == 0) {
        if (t >= 300 && c.canCall() && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            g.stage = 1;
            g.t0 = t;
            c.note("wp3: schild-wolf: Wolf frei");
        } else if (t > 5000) {
            result("schild-wolf", false, "Wolf nie frei");
            return true;
        }
        return false;
    }
    c.gameLocks();
    const uint32_t m = t - g.t0;
    switch (g.stage) {
    case 1:  // T8: L gehalten, R analog 1,0 ab +30, B bei +40
        if (m < 80) {
            c.hold(kTrigL);
        }
        if (m >= 30 && m < 50) {
            c.holdRAnalog(1.0f);
        }
        if (m == 40) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            g.t8Attn = link != nullptr && link->mAttention != nullptr && link->checkAttentionLock() ? 1 : 0;
            copyReason(g.t8Reason, sizeof g.t8Reason);
            logInfo("wp3: schild-wolf: T8 Zielsperre=%d R+B, Grund jetzt: %s", g.t8Attn, g.t8Reason);
            c.tapB();
        }
        if (m == 120) {
            g.t8Chord = chords() - g.chordBefore;
            g.t8Rej = rejected() - g.rejBefore;
            logInfo("wp3: schild-wolf: T8 chord+%u rejected+%u Vogel=%d", g.t8Chord, g.t8Rej, ownBirds());
            g.stage = 2;
            g.t0 = t;
        }
        break;
    case 2:  // T11a: START, R+B im Pausenmenue, R bleibt 300 Ticks nach dem Schliessen gehalten
        if (m == 30) {
            c.tap(kStart);
            c.note("wp3: schild-wolf: T11a START");
        }
        if (m >= 90) {
            c.holdR();
        }
        if (m == 90) {
            g.t11aWin = win();
        }
        if (m == 95) {
            shot(c, "t11a-menue");
        }
        if (m == 100) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            copyReason(g.t11aReason, sizeof g.t11aReason);
            logInfo("wp3: schild-wolf: T11a Fenster=%d pause=%u, R+B im Menue, Grund jetzt: %s", win(), pauseFlag(),
                g.t11aReason);
            c.tapB();
        }
        if (m > 100 && !g.t11aClosed && win() == 0 && pauseFlag() == 0) {
            g.t11aClosed = true;
            g.t11aCloseAt = m;
            logInfo("wp3: schild-wolf: T11a Menue zu nach %u Ticks (B kam beim Spiel an)", m - 100);
        }
        if (c.sess.birdId != kNoProc) {
            g.t11aSummon = true;
        }
        if (g.t11aClosed && m == g.t11aCloseAt + 300) {
            g.t11aRej = rejected() - g.rejBefore;
            g.t11aChord = chords() - g.chordBefore;
            logInfo("wp3: schild-wolf: T11a 300 Ticks nach dem Schliessen mit R gehalten: chord+%u rejected+%u Vogel=%d",
                g.t11aChord, g.t11aRej, ownBirds());
            g.stage = 3;
            g.t0 = t;
        }
        if (!g.t11aClosed && m > 600) {
            logInfo("wp3: schild-wolf: T11a Menue schliesst nicht, Fenster=%d pause=%u", win(), pauseFlag());
            g.stage = 3;
            g.t0 = t;
        }
        break;
    case 3:  // T11b: START, R und B im selben Tick
        if (m >= 60) {
            c.holdR();
        }
        if (m == 70) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            c.tap(kStart);
            c.tapB();
            c.note("wp3: schild-wolf: T11b START + R + B im selben Tick");
        }
        if (m == 71) {
            // Der Grund entsteht erst in padReadPost nach der Injektion (Startflanke im Pad), deshalb hier nachlesen.
            std::snprintf(g.t11bReason, sizeof g.t11bReason, "%s",
                rejected() != g.rejBefore ? core::lastRejectReason() : "(keine Ablehnung)");
            logInfo("wp3: schild-wolf: T11b Grund im Tick: %s (rejected+%u chord+%u)", g.t11bReason,
                rejected() - g.rejBefore, chords() - g.chordBefore);
        }
        if (m == 130) {
            g.t11bWin = win();
            // B nur, wenn ein Menue offen ist: ein B bei gehaltenem R ohne Menue waere ein neuer, gueltiger Ruf.
            const bool open = win() != 0 || pauseFlag() != 0;
            logInfo("wp3: schild-wolf: T11b Fenster=%d pause=%u, %s", win(), pauseFlag(),
                open ? "B schliesst" : "kein Menue offen, kein B");
            if (open) {
                c.tapB();
            }
        }
        if (m > 130 && !g.t11bClosed && win() == 0 && pauseFlag() == 0) {
            g.t11bClosed = true;
            g.t0 = t - 130;  // Bezug bleibt m == 130 + x
        }
        if (c.sess.birdId != kNoProc) {
            g.t11bSummon = true;
        }
        if (m == 500) {
            g.t11bRej = rejected() - g.rejBefore;
            g.t11bChord = chords() - g.chordBefore;
            logInfo("wp3: schild-wolf: T11b R gehalten bis +500: chord+%u rejected+%u geschlossen=%d Vogel=%d", g.t11bChord,
                g.t11bRej, g.t11bClosed ? 1 : 0, ownBirds());
            g.stage = 4;
            g.t0 = t;
        }
        break;
    case 4:  // T12: R+B und Steuerkreuz oben im selben Tick (Item-Rad oeffnet in diesem Frame)
        if (m >= 60 && m < 80) {
            c.holdR();
        }
        if (m == 70) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            g.restBefore = restored();
            c.tap(kUp);
            c.tapB();
            c.note("wp3: schild-wolf: T12 R+B+oben im selben Tick");
        }
        if (m > 70 && win() > g.t12WinMax) {
            g.t12WinMax = win();
        }
        if (c.sess.birdId != kNoProc) {
            g.t12Summon = true;
        }
        if (m == 72) {
            logInfo("wp3: schild-wolf: T12 nach 2 Ticks: Fenster=%d chord+%u rejected+%u restored+%u phase=%s", win(),
                chords() - g.chordBefore, rejected() - g.rejBefore, restored() - g.restBefore, phaseName(c.sess.phase));
        }
        if (m == 140 && win() != 0) {
            c.tapB();  // Item-Rad schliessen
        }
        if (m == 260) {
            g.t12Chord = chords() - g.chordBefore;
            g.t12Rej = rejected() - g.rejBefore;
            g.t12Rest = restored() - g.restBefore;
            const bool t8ok = g.t8Chord == 0 && g.t8Rej == 1 && g.t8Attn == 1 &&
                              std::strncmp(g.t8Reason, "Zielsperre", 10) == 0;
            const bool t11aok = g.t11aRej >= 1 && g.t11aChord == 0 && g.t11aClosed && !g.t11aSummon;
            const bool t11bok = g.t11bRej >= 1 && g.t11bChord == 0 && !g.t11bSummon &&
                                std::strncmp(g.t11bReason, "Startflanke", 11) == 0;
            result("schild-wolf", t8ok && t11aok && t11bok,
                "T8(zielsperre=%d grund=\"%s\" chord=%u rejected=%u) T11a(fenster=%d grund=\"%s\" rejected=%u "
                "geschlossen=%d chord=%u vogel=%d) T11b(grund=\"%s\" rejected=%u fenster=%d chord=%u vogel=%d) "
                "T12(chord=%u rejected=%u restored=%u fensterMax=%d vogel=%d)",
                g.t8Attn, g.t8Reason, g.t8Chord, g.t8Rej, g.t11aWin, g.t11aReason, g.t11aRej, g.t11aClosed ? 1 : 0,
                g.t11aChord, g.t11aSummon ? 1 : 0, g.t11bReason, g.t11bRej, g.t11bWin, g.t11bChord,
                g.t11bSummon ? 1 : 0, g.t12Chord, g.t12Rej, g.t12Rest, g.t12WinMax, g.t12Summon ? 1 : 0);
            return true;
        }
        break;
    }
    return false;
}

const Scenario kSchildWolf = {
    "wp3-schild-wolf",
    "WP3.6 T8/T11/T12: Zielsperre + R+B (Wolf), Pausen-Latch, Start+R+B im selben Tick, R+B+oben im selben Tick",
    wolfTick,
};

// ---------------------------------------------------------------------------------------------
// wp3-r-analog
// ---------------------------------------------------------------------------------------------

// Achsweg in Prozent fuer einen Triggerwert f (mDoCPd_TRIGGER_CONV: f = Rohwert / 140, Rohwert = Achse * 255 - 30).
double axisPct(float f) {
    return (30.0 + 140.0 * static_cast<double>(f)) / 255.0 * 100.0;
}

struct RAnalogState {
    Prep prep;
    int stage = 0;
    uint32_t t0 = 0;
    float f = 0.0f;
    float prevF = 0.0f;
    float onAt = -1.0f;
    float offAt = -1.0f;
    uint32_t chordBefore = 0;
    uint32_t rejBefore = 0;
    uint32_t call044 = 0;
    uint32_t rej044 = 0;
    uint32_t call061 = 0;
    int ride = 0;                // 0 = Ritt A (Ende mit f 1,0), 1 = Ritt B (Ende mit f 0,75)
    uint32_t rideTicks[2] = {0, 0};
    uint32_t rideHold[2] = {0, 0};   // Ticks im Ritt mit mHoldLockR im Endzustand des Pads (Spiel sieht R)
    uint32_t rideTrig[2] = {0, 0};
    uint32_t afterTrig[2] = {0, 0};  // R-Flanken in 30 Ticks nach dem Ritt (falsche Flanke P2-04)
    int afterHold[2] = {-1, -1};     // mHoldLockR 5 Ticks nach dem Ritt (Vanilla: 1)
    int releaseTicks[2] = {-1, -1};  // Ticks bis mHoldLockR 0 nach dem Loslassen
    uint32_t endT = 0;
};
RAnalogState g_ra;

bool rAnalogTick(Context& c) {
    RAnalogState& g = g_ra;
    const uint32_t t = c.frame;
    const interface_of_controller_pad& last = lastPad();
    if (g.stage == 0) {
        if (t >= 300 && c.canCall()) {
            g.stage = 1;
            g.t0 = t;
            g.f = 0.0f;
            c.note("wp3: r-analog: Sweep 0 -> 1 -> 0 (Spiel-Lock)");
        } else if (t > 5000) {
            result("r-analog", false, "Wolf nie frei");
            return true;
        }
        return false;
    }
    c.gameLocks();
    const uint32_t m = t - g.t0;
    switch (g.stage) {
    case 1: {  // Sweep hoch: f += 0,01 je 2 Ticks
        if (last.mHoldLockR != 0 && g.onAt < 0.0f) {
            g.onAt = g.prevF;
        }
        g.prevF = g.f;
        c.holdRAnalog(g.f);
        if (m % 2 == 1) {
            g.f += 0.01f;
        }
        if (g.f > 1.0f) {
            g.f = 1.0f;
            g.stage = 2;
            g.t0 = t;
        }
        break;
    }
    case 2: {  // Sweep runter
        if (last.mHoldLockR == 0 && g.offAt < 0.0f && m > 2) {
            g.offAt = g.prevF;
        }
        g.prevF = g.f;
        c.holdRAnalog(g.f);
        if (m % 2 == 1) {
            g.f -= 0.01f;
        }
        if (g.f < 0.0f) {
            g.f = 0.0f;
            logInfo("wp3: r-analog: Spiel-Lock an ab f=%.2f (Achse %.1f %%), aus ab f=%.2f (Achse %.1f %%), Ruf des Kerns ab "
                    "f=0.61 (Achse %.1f %%)",
                static_cast<double>(g.onAt), axisPct(g.onAt), static_cast<double>(g.offAt), axisPct(g.offAt),
                axisPct(0.61f));
            g.stage = 3;
            g.t0 = t;
        }
        break;
    }
    case 3:  // f 0,44 + B: kein Ruf
        if (m >= 30 && m < 60) {
            c.holdRAnalog(0.44f);
        }
        if (m == 45) {
            g.chordBefore = chords();
            g.rejBefore = rejected();
            c.tapB();
            c.note("wp3: r-analog: R analog 0,44 + B");
        }
        if (m == 150) {
            g.call044 = chords() - g.chordBefore;
            g.rej044 = rejected() - g.rejBefore;
            logInfo("wp3: r-analog: 0,44 + B: chord+%u rejected+%u Vogel=%d", g.call044, g.rej044, ownBirds());
            g.stage = 4;
            g.t0 = t;
            g.prep = Prep{};
            g.chordBefore = chords();
        }
        break;
    case 4: {  // f 0,61 + B: Ruf und Ritt
        const int r = prepRide(c, g.prep, 0.61f);
        if (r < 0) {
            result("r-analog", false, "kein Ritt mit R analog 0,61 (Ritt %d)", g.ride + 1);
            return true;
        }
        if (r == 1) {
            if (g.ride == 0) {
                g.call061 = chords() - g.chordBefore;
            }
            logInfo("wp3: r-analog: Ritt %d steht (R analog 0,61), chord+%u", g.ride + 1, chords() - g.chordBefore);
            g.stage = 5;
            g.t0 = t;
        }
        break;
    }
    case 5: {  // im Ritt: R analog 1,0 (Ritt B danach 0,75), der Endzustand des Pads ist, was das Spiel sieht
        const float want = (g.ride == 1 && m >= 90) ? 0.75f : 1.0f;
        c.holdRAnalog(want);
        if (m >= 2) {
            ++g.rideTicks[g.ride];
            if (last.mHoldLockR != 0) {
                ++g.rideHold[g.ride];
            }
            if (last.mTrigLockR != 0) {
                ++g.rideTrig[g.ride];
            }
        }
        if (m == 60) {
            shot(c, g.ride == 0 ? "r-analog-ritt-a" : "r-analog-ritt-b");
        }
        if (m == 150) {
            c.tapB();
            logInfo("wp3: r-analog: Ritt %d B mit R analog %.2f, Ticks im Ritt %u, Spiel sah R-Lock %u Ticks, Flanken %u",
                g.ride + 1, static_cast<double>(want), g.rideTicks[g.ride], g.rideHold[g.ride], g.rideTrig[g.ride]);
        }
        if (m > 150 && c.sess.phase != Phase::Riding) {
            g.stage = 6;
            g.t0 = t;
            g.endT = t;
        }
        if (m > 150) {
            if (last.mHoldLockR != 0) {
                ++g.rideHold[g.ride];
            }
        }
        if (m > 900) {
            result("r-analog", false, "Ritt %d endet nicht", g.ride + 1);
            return true;
        }
        break;
    }
    case 6: {  // nach dem Ritt: R weiter gehalten (Ritt A 1,0, Ritt B 0,75), 30 Ticks Flanken zaehlen
        const float want = g.ride == 1 ? 0.75f : 1.0f;
        if (m < 30) {
            c.holdRAnalog(want);
            if (m >= 1 && last.mTrigLockR != 0) {
                ++g.afterTrig[g.ride];
            }
            if (m == 5) {
                g.afterHold[g.ride] = last.mHoldLockR != 0 ? 1 : 0;
            }
        } else {
            c.holdRAnalog(0.0f);  // loslassen
            if (g.releaseTicks[g.ride] < 0 && m >= 31 && last.mHoldLockR == 0) {
                g.releaseTicks[g.ride] = static_cast<int>(m - 30);
            }
        }
        if (m == 100) {
            logInfo("wp3: r-analog: Ritt %d Ende: Flanken nach dem Ritt %u, Lock 5 Ticks danach %d, Lock aus nach %d Ticks",
                g.ride + 1, g.afterTrig[g.ride], g.afterHold[g.ride], g.releaseTicks[g.ride]);
            if (g.ride == 0) {
                g.ride = 1;
                g.stage = 4;
                g.t0 = t;
                g.prep = Prep{};
                g.chordBefore = chords();
            } else {
                const bool ok = g.onAt > 0.89f && g.onAt < 0.92f && g.offAt > 0.58f && g.offAt < 0.61f &&
                                g.call044 == 0 && g.rej044 == 0 && g.call061 == 1 && g.rideHold[0] == 0 &&
                                g.rideHold[1] == 0 && g.rideTrig[0] == 0 && g.rideTrig[1] == 0 && g.afterTrig[0] == 0 &&
                                g.afterTrig[1] == 0 && g.afterHold[0] == 1 && g.afterHold[1] == 1 &&
                                g.releaseTicks[0] >= 0 && g.releaseTicks[0] <= 60 && g.releaseTicks[1] >= 0 &&
                                g.releaseTicks[1] <= 60;
                result("r-analog", ok,
                    "lockAn=%.2f(%.1f%%) lockAus=%.2f(%.1f%%) ruf044=%u abgelehnt044=%u ruf061=%u "
                    "A(ritt=%u spielLock=%u flanken=%u nachFlanken=%u nachLock=%d losNach=%d) "
                    "B(ritt=%u spielLock=%u flanken=%u nachFlanken=%u nachLock=%d losNach=%d) lockReparaturen=%u",
                    static_cast<double>(g.onAt), axisPct(g.onAt), static_cast<double>(g.offAt), axisPct(g.offAt),
                    g.call044, g.rej044, g.call061, g.rideTicks[0], g.rideHold[0], g.rideTrig[0], g.afterTrig[0],
                    g.afterHold[0], g.releaseTicks[0], g.rideTicks[1], g.rideHold[1], g.rideTrig[1], g.afterTrig[1],
                    g.afterHold[1], g.releaseTicks[1], core::inputCounts().lockRepairs);
                return true;
            }
        }
        break;
    }
    }
    return false;
}

const Scenario kRAnalog = {
    "wp3-r-analog",
    "WP3.7: Spiel-Lock gemessen, R analog 0,44/0,61 + B, R-Lock im Ritt neutral, nach dem Ritt keine falsche Flanke",
    rAnalogTick,
};

}  // namespace

void registerInputScenarios() {
    (void)registerScenario(&kKarteRitt);
    (void)registerScenario(&kUiHalt);
    (void)registerScenario(&kChordB);
    (void)registerScenario(&kDialogB);
    (void)registerScenario(&kSchild);
    (void)registerScenario(&kSchildWolf);
    (void)registerScenario(&kRAnalog);
}

}  // namespace kff::autotest
