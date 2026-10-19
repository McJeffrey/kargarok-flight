// Kern: Ruf, Ritt, Absteigen und Aufraeumen (v1-Paritaet) und die Kern-Hook-Handler.
// Modul-Callbacks laufen hier, jeder einzeln abgesichert.
//
// Eingabe (Pad-Lesung, Ruf-Kombination, Konsum, UI-Block) steht in input.cpp (WP3), Schnittstelle core/input_core.hpp.
#include "d/dolzel_rel.h"

#include "core/actions.hpp"
#include "core/handlers.hpp"
#include "core/input_core.hpp"
#include "core/summon.hpp"
#include "core/timers.hpp"
#include "core/kff/autotest.hpp"
#include "core/kff/config.hpp"
#include "core/kff/fakepath.hpp"
#include "core/kff/hooks.hpp"
#include "core/kff/input.hpp"
#include "core/kff/log.hpp"
#include "core/kff/module.hpp"
#include "core/kff/ride.hpp"
#include "core/kff/session.hpp"

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_e_ym.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_midna.h"
#include "d/actor/d_a_player.h"
#include "d/d_camera.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_file_sel_warning.h"
#include "d/d_meter2_info.h"
#include "d/d_menu_save.h"
#include "d/d_menu_window.h"
#include "d/d_msg_object.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_controller_pad.h"
#if defined(KFF_DEV_AUTOTEST)
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#endif

#include <cmath>
#include <cstdio>

#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"

namespace kff {

// WP5: Ruf-Gate (Whitelist der Link-Zustaende) im Modul griff (src/modules/griff.cpp).
const char* griffCallGateReason(daAlink_c* link);

namespace {

// Werte aus d_a_kago.cpp und dem bewiesenen MVP.
constexpr int kSceneTypeDefault = 3;                  // SCENE_TYPE_DEFAULT (kein Fluss-/See-Zweig)
constexpr float kNoRoof = 1.0e9f;                     // Decke weg (Modus frei)
constexpr uint32_t kDepartDeleteFrames = 100;         // 90 Frames Steigflug, dann Entfernen (WP6.10: nach 100 Ticks weg).
                                                      // Befund 6 (F2): der Antrag geht schon bei departFrames+2 hinaus
                                                      // (ridePadTick), der Vogel ist dann 100 Ticks nach R+B weg (vorher 101)
constexpr uint32_t kRiderLostFrames = 90;             // Kern-Ticks ohne Cargo-Carry in Folge (WP9, R4; griff-Befund K4: groesser
                                                      // als das 60-Tick-Wiederanbindungsfenster von griff; Kern nur Sicherheitsnetz)
constexpr uint32_t kCreateTimeoutFrames = 900;        // Vogel nie gesehen -> Sitzung zuruecksetzen
constexpr uint32_t kDeleteRetryFrames = 60;           // Entfernen wiederholen, falls der Vogel lebt
constexpr uint32_t kPendingGiveUpTicks = 900;         // angefordertes Entfernen nach so vielen Pad-Ticks aufgeben
constexpr uint32_t kLogEvery = 600;                   // Ritt-Log-Takt in Kern-Ticks (R9: alle 600 Ticks, etwa 20 s)
constexpr uint32_t kLinkLogEvery = 600;               // Link-Log-Takt je Link-Aufruf, nur im Testbuild (R9)
constexpr uint32_t kFallWatchFrames = 600;            // Kern-Ticks, in denen mFallHeight nach dem Absteigen gehalten wird (WP9, R4)
constexpr uint32_t kLandMaxFrames = 240;               // Obergrenze des Landeanflugs in Ticks (8 s bei 30 Hz); T9: vorher 60
constexpr float kLandSink = 8.0f;                     // Mindest-Sinkrate im Landeanflug je Tick (Einheiten)
constexpr float kLandClearance = 150.0f;              // Bodenabstand des Vogels, bei dem der Landeanflug endet. Annahme, im Spiel zu pruefen
constexpr uint32_t kLandPlanFrames = 60;           // Ticks, in denen der Anflug auf kLandClearance zielt (T9, adaptive Sinkrate)
constexpr float kLandSinkMax = 60.0f;                  // hoechste Sinkrate je Tick im Landeanflug (T9)
constexpr float kNoGroundY = -1.0e5f;                 // checkGroundHeight liefert darunter "kein Boden" (freiheit: -1e6)
constexpr float kDefaultSpeedScale = 0.5f;            // Gang 3 von 5 (30/40/50/75/100 %)
constexpr int kDefaultGear = 3;

struct Counts {
    uint32_t humanRefused = 0;
    uint32_t busyRefused = 0;
    uint32_t eventRefused = 0;
    uint32_t dismount = 0;
    uint32_t dismountFromB = 0;      // Absteigen durch B-Tipp aus dem Schnappschuss
    uint32_t dismountIgnored = 0;    // B-Tipp im Ritt ohne Wirkung (Aktion des Vogels passt nicht)
    uint32_t deleteRequested = 0;
    uint32_t deleteRetry = 0;
    uint32_t deleted = 0;
    uint32_t endClean = 0;           // Ritt-Ende ohne Abflugflug: endRideClean (Tod, Menue, Ereignis) und finishDismount (Befund 4)
    uint32_t riderLost = 0;
    uint32_t forcedFly = 0;
    uint32_t linkChanged = 0;
    uint32_t orphanRemoved = 0;      // eigene Voegel ohne Sitzung beim Szenenwechsel entfernt
    uint32_t wallVeto = 0;
    uint32_t balloonBlocked = 0;
    uint32_t drawForced = 0;
    uint32_t exceptions = 0;
    uint32_t keepCleared = 0;
    uint32_t ymCleared = 0;
    uint32_t deadRider = 0;
    uint32_t transformAborted = 0;
    uint32_t saveLocked = 0;         // Speichern im Ritt am Schreibpunkt abgebrochen (D2)
    uint32_t landings = 0;           // Landeanflug nach B mit Boden darunter gestartet
    uint32_t landingDone = 0;        // Landeanflug beendet (Mindesthoehe, Zeitlimit oder Abgrund)
    uint32_t rideEndDuplicate = 0;   // zweites Ritt-Ende desselben Rittes unterdrueckt (griff K3)
    uint32_t pathGuard = 0;          // Ersatzpfad vor executeFly/executeStagger gesetzt, mpPath1 war null (griff K6)
    uint32_t warpEnded = 0;          // Ritt beim Warp-Start beendet (D1)
    uint32_t sceneKept = 0;          // positionsgetriebener Szenenwechsel im Ritt, fuer freiheit stehen gelassen (D1)
    uint32_t goneRelease = 0;        // Link und Midna ohne den Vogel geloest, weil der Vogel nicht mehr auffindbar war (D-C)
    uint32_t skipCleared = 0;        // WP4: Skip-Rueckruf zeigte auf einen Vogel, der entfernt wird; Skip-System geleert
    uint32_t eventReset = 0;         // WP4: Ereignis mit dem eigenen Vogel als Beteiligtem beim Entfernen beendet
    uint32_t summonEventEnd = 0;     // WP4 (T3, D6): Ereignis vor dem Ritt-Start, gerufener Vogel geordnet entfernt
    uint32_t rootCancel = 0;         // Befund 2 (F2): Wurzelversatz der Fallanimation nach dem Loslassen zurueckgenommen
};
Counts g_counts;
uint32_t g_logForcedFly = 0;
uint32_t g_logWaiting = 0;
uint32_t g_logDismountIgnored = 0;
uint32_t g_logSaveLock = 0;
uint32_t g_logDrop = 0;
uint32_t g_logHumanCall = 0;      // F02: "human form - ride needs wolf" ratenbegrenzt
uint32_t g_logTransformCall = 0;  // F02: "call ignored, transformation running" ratenbegrenzt
uint32_t g_logUiHalt = 0;         // ui_halt: Landeanflug und B-Wunsch pausiert, solange ein Dokument sichtbar ist
uint32_t g_logLanding = 0;
float g_landX = 0.0f;  // E9: Position des Vogels beim Start des Landeanflugs (Messung: Strecke bis zur Freigabe)
float g_landZ = 0.0f;
uint32_t g_dismountWish = 0;                 // P2-10: B-Wunsch in einer Vogelaktion, die weder Flug noch Angriff ist (Ticks)
constexpr uint32_t kDismountWishTicks = 60;  // P2-10: so lange gilt der B-Wunsch
bool g_linkExecuteReady = true;              // HK-02: daAlink_c::execute-Pre installiert (Kern-Installation)
uint32_t g_logPathGuard = 0;
uint32_t g_logPathNull = 0;  // D-D: Pfad des Vogels war beim Execute null und ist ersetzt worden (Log-Takt)
bool g_fallHold = false;   // nach dem Absteigen: mFallHeight auf die aktuelle Hoehe halten (kein Sturzschaden)
uint32_t g_fallWatch = 0;  // Kern-Ticks seit dem Absteigen (WP9, R4), 0 = keine Beobachtung
// Befund 2 (F2): Fenster nach dem Loslassen am Landeanflug (Boden darunter, Link in der Luft). Solange es laeuft, nimmt
// linkPosMovePost den Wurzelversatz der Fallanimation in x/z zurueck. Obergrenze kReleaseFallTicks (Kern-Ticks).
constexpr uint32_t kReleaseFallTicks = 90;
// A3 B1: Mindestdauer des Fensters (Kern-Ticks). Beruehrt Link bei der Freigabe Boden oder Wasser, geht er oft erst nach
// 2 Ticks in den Fall (E9 LINK 2, proc 253). In dieser Zeit geht die Waagerechte auf den Stand vor dem Tick zurueck
// (linkPosMovePost), Boden- und Wasserkontakt schliessen das Fenster erst danach.
constexpr uint32_t kReleaseFallMinTicks = 10;
bool g_releaseFall = false;
uint32_t g_releaseFallTicks = 0;
float g_releaseFallX = 0.0f;       // A3 B1: Waagerechte von Link vor Proc und posMove des Ticks (linkExecutePre)
float g_releaseFallZ = 0.0f;
bool g_releaseFallSnapOk = false;  // A3 B1: der Schnappschuss gehoert zum laufenden Tick (linkPosMovePost verbraucht ihn)
uint32_t g_rideLogTick = 0;  // WP9 (R9): Kern-Tick der letzten Ritt-Zeile
bool g_rideLogged = false;   // WP9: im laufenden Ritt ist schon eine Ritt-Zeile geschrieben (Takt ab dem Ritt-Start)
bool g_rideEndSent = false;  // das Ende des laufenden Rittes ist an die Module gemeldet (griff K3)
// Angefordertes Entfernen des Vogels. Bleibt auch dann bestehen, wenn die Sitzung schon zurueckgesetzt ist (endRideClean):
// die Wiederholung in retryPendingDelete und der _delete-Hook loesen es erst, wenn der Vogel verschwunden ist.
fpc_ProcID g_pendingBirdId = kNoProc;
uint32_t g_pendingWait = 0;  // Pad-Ticks seit dem letzten Entfernen-Wiederholen
ride::SceneChange g_sceneChange = ride::SceneChange::None;  // Einordnung des laufenden Szenenwechsels (D1)

// Werte des zuletzt angeforderten Entfernens fuer die Vertragszeile "bird deleted" (F). Beim Entfernen nach
// endRideClean ist die Sitzung schon leer, die Werte kommen dann aus diesem Snapshot.
struct LastEnd {
    fpc_ProcID id = kNoProc;
    uint32_t rideFrames = 0;
    uint32_t departFrames = 0;
    bool riding = false;
    bool departing = false;
};
LastEnd g_lastEnd;

// Ruft jedes aktive Modul in Listenreihenfolge auf. Eine Exception stoppt nur diesen Aufruf.
template <class Fn>
void forEachModule(const char* event, Fn&& fn) {
    size_t count = 0;
    Module* const* list = moduleList(count);
    for (size_t i = 0; i < count; ++i) {
        if (!moduleActive(list[i])) {
            continue;
        }
        try {
            fn(*list[i]);
        } catch (...) {
            logEvery(g_counts.exceptions, 3, 300, "module %s threw in %s (call skipped)", list[i]->name(),
                event);
        }
    }
}

void notifyRideStart(daKago_c* bird) {
    Session& s = session();
    forEachModule("onRideStart", [&](Module& m) { m.onRideStart(s, bird); });
}

// griff K3: das Ende eines Rittes meldet der Kern genau einmal. Ohne diesen Schutz kaeme ein zweites onRideEnd,
// wenn fopAcM_delete den Vogel synchron ueber kagoDeletePre entfernt, bevor die Phase auf Idle steht (shutdownBird).
void notifyRideEnd(daKago_c* bird, EndReason why) {
    if (g_rideEndSent) {
        ++g_counts.rideEndDuplicate;
        return;
    }
    g_rideEndSent = true;
    Session& s = session();
    forEachModule("onRideEnd", [&](Module& m) { m.onRideEnd(s, bird, why); });
}

void notifyFrameBird(daKago_c* bird) {
    Session& s = session();
    forEachModule("onFrameBird", [&](Module& m) { m.onFrameBird(s, bird); });
}

void notifyFrameLink(daAlink_c* link) {
    Session& s = session();
    forEachModule("onFrameLink", [&](Module& m) { m.onFrameLink(s, link); });
}

// Hook-Huelle fuer Pre-Hooks: eine Exception kostet nur diesen Frame, nicht das Paket.
template <class Fn>
HookAction guardedPre(const char* where, Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in %s (frame skipped)", where);
        return HOOK_CONTINUE;
    }
}

// Ritt-Frame: Yaw-Summe und Hoehenspanne fuer die Logs.
void trackYaw(daKago_c* k) {
    Session& s = session();
    const int16_t yaw = k->current.angle.y;
    if (!s.yawInit) {
        s.yawInit = true;
        s.yawPrev = yaw;
        s.yawUnwrapped = 0;
    } else {
        s.yawUnwrapped += static_cast<int16_t>(yaw - s.yawPrev);
        s.yawPrev = yaw;
    }
    const float yawDeg = static_cast<float>(s.yawUnwrapped) * 360.0f / 65536.0f;
    if (yawDeg < s.yawMinDeg) {
        s.yawMinDeg = yawDeg;
    }
    if (yawDeg > s.yawMaxDeg) {
        s.yawMaxDeg = yawDeg;
    }
    if (k->current.pos.y < s.birdMinY) {
        s.birdMinY = k->current.pos.y;
    }
    if (k->current.pos.y > s.birdMaxY) {
        s.birdMaxY = k->current.pos.y;
    }
}

void logRide(daKago_c* k) {
    const Session& s = session();
    daAlink_c* link = liveLink();
    float lx = 0.0f;
    float ly = 0.0f;
    float lz = 0.0f;
    float dist = -1.0f;
    unsigned riding = 0;
    unsigned linkProc = 0xFFFFu;
    if (link != nullptr) {
        lx = link->current.pos.x;
        ly = link->current.pos.y;
        lz = link->current.pos.z;
        const float dx = k->current.pos.x - lx;
        const float dy = k->current.pos.y - ly;
        const float dz = k->current.pos.z - lz;
        dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        riding = link->checkCargoCarry() ? 1u : 0u;
        linkProc = static_cast<unsigned>(link->mProcID);
    }
    const float yawDeg = static_cast<float>(s.yawUnwrapped) * 360.0f / 65536.0f;
    logInfo("ride f=%u pos=(%.1f,%.1f,%.1f) yaw=%d yawDeg=%.1f pitch=%d action=%d mode=%d riding=%u "
            "link=(%.1f,%.1f,%.1f) dist=%.1f linkProc=%u stick=(%.2f,%.2f) mGround=%.1f mRoof=%.1f",
        s.rideFrames, k->current.pos.x, k->current.pos.y, k->current.pos.z, static_cast<int>(k->current.angle.y),
        yawDeg, static_cast<int>(k->current.angle.x), k->mAction, k->mMode, riding, lx, ly, lz, dist, linkProc,
        k->mStickX, k->mStickY, k->mGroundHeight, k->mRoofHeight);
}

void logDepart(daKago_c* k) {
    const Session& s = session();
    logInfo("depart f=%u pos=(%.1f,%.1f,%.1f) yaw=%d action=%d mode=%d field728=%d field6de=%u",
        s.departFrames, k->current.pos.x, k->current.pos.y, k->current.pos.z, static_cast<int>(k->current.angle.y),
        k->mAction, k->mMode, k->field_0x728, static_cast<unsigned>(k->field_0x6de));
}

// P2-03: Kartenzugriff laeuft (Speichern oder Autosave). mDoMemCd_Ctrl_c::save setzt field_0x1fc8 auf 0, der Kartenthread
// danach wieder auf 1 (m_Do_MemCard.cpp). Die Sperre gilt hoechstens kCardBusyTicks Pad-Ticks, damit ein haengender
// Thread den Ruf nicht dauerhaft sperrt. Der Wert 0 gilt auch vor dem ersten Kartenzugriff, darum zaehlt nur ein
// gesehenes Schreiben (g_cardWriteSeen).
constexpr uint32_t kCardBusyTicks = 600;
bool g_cardWriteSeen = false;
uint32_t g_cardWriteTick = 0;
bool cardWriteBusy() {
    return g_cardWriteSeen && g_mDoMemCd_control.field_0x1fc8 == 0 && core::padTick() - g_cardWriteTick < kCardBusyTicks;
}

// Ein Grund, warum der Ruf jetzt NICHT geht, oder nullptr, wenn er geht (Politik: Sperren Ereignis, Dialog,
// Pause/Menue, Wasser, Klettern, Fall, Verwandlung, eigener Vogel, Link tot; D16: Zielsperre, R-Status, Schildangriff,
// Luft, UI). Der Text geht ins Log, ratenbegrenzt.
const char* callBlockReason(daAlink_c* link) {
    const Session& s = session();
    if (s.phase == Phase::Transforming) {
        return "transforming";  // WP4: fester Grund-Text (Log "call rejected (transforming)")
    }
    if (link != nullptr && link->mProcID == daAlink_c::PROC_METAMORPHOSE) {
        return "transforming";  // WP4: Verwandlung ueber den Spielweg (Schnellverwandlung, Midna), ohne Phase Transforming
    }
    if (!g_linkExecuteReady) {
        return "Kern-Hook daAlink_c::execute fehlt";  // HK-02: ohne Hook kein Ruf, B bleibt beim Spiel
    }
    if (!moduleActive(&modules::flug()) || !moduleActive(&modules::freiheit())) {
        return "Modul flug oder freiheit abgeschaltet";  // HK-01: Ruf nur mit vollstaendigem Ritt
    }
    if (mDoCPd_c::getTrigStart(PAD_1) != 0) {
        return "Startflanke (Menue oeffnet in diesem Tick)";  // chord-ui.md, Gate "Startflanke"
    }
    if (dComIfGp_isPauseFlag() != 0) {
        return "Pause oder Menue offen";
    }
    if (dMeter2Info_getWindowStatus() != 0) {
        return "Menue, Karte oder Item-Rad offen";  // E5: Fenster 2, 4, 10, 11 (ohne Pause-Flag)
    }
    if (dMsgObject_isTalkNowCheck()) {
        return "Gespraech offen";  // E5: NPC-Dialog
    }
    if (dComIfGp_event_runCheck()) {
        return "event";  // WP4 (T3): fester Grund-Text (Log "call rejected (event)")
    }
    if (core::uiDocumentVisible()) {
        return "Dusklight-Dokument sichtbar";
    }
    if (cardWriteBusy()) {
        return "Kartenzugriff laeuft (Speichern oder Autosave)";  // P2-03
    }
    if (link == nullptr) {
        return "kein Link";
    }
    // N1 (12.10.2026, lockert WP6.10/D7): der abfliegende Vogel sperrt den Ruf nicht mehr. Bei angenommenem Ruf entfernt
    // summonBird ihn sofort (requestDelete ueber die Prozess-ID, derselbe Pfad wie am Abflug-Ende). Gesperrt bleibt nur
    // der Landeanflug nach B (Boden darunter, Link noch im Ritt).
    if (s.birdId != kNoProc && s.landFrames > 0) {
        return "departing";
    }
    if (s.birdId != kNoProc && !birdDeparting(s)) {
        return "eigener Vogel aktiv";  // N1: ein abfliegender Vogel (birdDeparting) sperrt den Ruf nicht
    }
    if (link->checkEventRun()) {
        return "event";  // WP4 (T3): Link im Ereignis oder in einer Demo
    }
    if (link->checkCargoCarry()) {
        return "Link reitet bereits";
    }
    if (link->mProcID == daAlink_c::PROC_DEAD) {
        return "Link tot";
    }
    // WP5 (griff): Reiten, Verwandlung, Wasser, Seil, Kette, Haengen, Klettern, Enterhaken, Schaden, Luft.
    if (const char* wp5 = griffCallGateReason(link)) {
        return wp5;
    }
    // Fix A (lebensverlust-zyklus3.md): Fall- und Schadens-Procs sperren den Ruf. Sonst startet der Ritt im Schaden
    // (Log: RIDE start linkProc=263 = PROC_WOLF_DAMAGE).
    if (link->mProcID == daAlink_c::PROC_FALL || link->mProcID == daAlink_c::PROC_WOLF_FALL) {
        return "Fall";
    }
    if (link->mProcID == daAlink_c::PROC_WOLF_DAMAGE || link->mProcID == daAlink_c::PROC_POLY_DAMAGE) {
        return "Link im Schaden";
    }
    if (link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0) {
        return "Wasser";
    }
    if (link->checkClimbMove()) {
        return "Klettern";
    }
    // D16 (chord-ui.md, Abschnitt 0): Zielsperre (analoges L, d_attention Lockon), R-Status ungleich NONE (nur wenn das
    // Spiel selbst einen setzt: Schildangriff, Greifen, Pferd, Bumerang; Halten von R allein setzt keinen, Setter-Liste
    // d_a_alink.cpp:9691-9696, 11905, d_a_alink_grab.inc:1523), laufender Schildangriff, Luft (kein Bodenkontakt).
    if (link->mAttention != nullptr && link->checkAttentionLock()) {
        return "Zielsperre (analoges L)";
    }
    if (dComIfGp_getRStatus() != BUTTON_STATUS_NONE) {
        return "R-Status belegt";
    }
    if (link->mProcID == daAlink_c::PROC_GUARD_ATTACK) {
        return "Schildangriff laeuft";
    }
    if (!link->mLinkAcch.ChkGroundHit()) {
        return "Luft (Sprung oder Fall)";
    }
    // WP6 (ORCH-6): eigener Vogel ohne Sitzung in der Welt, Spawnpunkt ueber Link nicht frei ("room").
    if (const char* why = summon::callBlockReason(link)) {
        return why;
    }
    return nullptr;
}

// Erster Ritt-Frame: Ersatzpfad, Boden, Decke, Ritt-Zustand (v1 start_ride, Modus frei).
// WP9 (R4): alle Zeitgrenzen des Rittes zaehlen Kern-Ticks (Pad-Lesungen, je einmal), auch in Pause und Ereignis.
// Die Execute-Hooks pruefen nur noch die Grenzen. Das Landefenster ruht bei einem sichtbaren Dusklight-Dokument (WP3).
// Frist-Konstanten bleiben oben in dieser Datei. Aufruf nur aus core::ridePadTick.
void kernTimersTick(Session& s) {
    if (s.birdId != kNoProc) {
        ++s.birdAge;  // Ruf: Ticks, bis der Vogel erscheint (kCreateTimeoutFrames, trackBird)
    }
    // Befund 3 (F2): derselbe Tick wie uiHalt in kagoExecutePre. input().uiDocument ist hier noch der Schnappschuss des
    // Vortick (padReadPost setzt ihn erst danach), darum zaehlte der Tick, in dem das Dokument erscheint, einmal zu viel.
    if (s.landFrames > 0 && !core::uiDocumentVisible()) {
        ++s.landFrames;  // Landefenster nach B (kLandMaxFrames, kLandPlanFrames)
    }
    if (g_releaseFall && ++g_releaseFallTicks >= kReleaseFallTicks) {
        g_releaseFall = false;  // Befund 2: Fenster nach dem Loslassen abgelaufen
    }
    if (g_fallHold && g_fallWatch > 0) {
        ++g_fallWatch;  // Fallreferenz nach dem Absteigen (kFallWatchFrames)
    }
    if (s.phase == Phase::Riding && !s.rideGrip) {
        daAlink_c* link = liveLink();
        if (link != nullptr && link->checkCargoCarry()) {
            s.riderLost = 0;
        } else if (link != nullptr) {
            ++s.riderLost;  // Wiederanbindung: Kern-Ticks in Folge ohne Cargo-Carry (kRiderLostFrames, kernTimersTick)
        }
    }
}

void startRide(daKago_c* k) {
    Session& s = session();
    fakepath::refresh(k);
    k->mSceneType = kSceneTypeDefault;
    k->mGroundHeight = k->checkGroundHeight(k->current.pos, nullptr);
    // Decke: freiheit setzt sie jeden Frame (executeFly-Pre und checkRoofHeight-Pre mit SKIP). Hier nur der Startwert.
    k->mRoofHeight = kNoRoof;  // D9: keine Decke, kein Schluessel
    g_fallHold = false;
    g_fallWatch = 0;
    g_releaseFall = false;  // Befund 2: ein neuer Ritt hat kein Fenster aus dem vorigen Abflug
    g_rideLogged = false;   // WP9: die erste Ritt-Zeile dieses Rittes kommt sofort
    g_rideEndSent = false;  // griff K3: dieser Ritt meldet sein Ende selbst (einmal)
    if (!s.rideGrip) {
        // Wolf-Link: Cargo (Midna und Link). Mensch im Griff (Modul mensch): kein Cargo, Link haelt den Vogel selbst.
        k->setMidnaRideOn();
        k->setPlayerRideOn();
    }
    k->field_0x6dc = 0;
    k->field_0x6de = 0xff;  // Sichtbarkeit: draw() kehrt bei 0 sofort zurueck
    k->field_0x6df = 1;     // executeWait() startet keinen eigenen Ritt mehr
    k->setActionMode(daKago_c::ACTION_FLY_e, 0);

    s.phase = Phase::Riding;
    s.rideFrames = 0;
    s.yawInit = false;
    s.riderLost = 0;
    s.lastAction = -1;
    s.birdMinY = k->current.pos.y;
    s.birdMaxY = k->current.pos.y;
    // Lagezustand und Gang fuer diesen Ritt neu (Schnittstellenvertrag Teil 2).
    s.att = AttitudeState{};
    s.speedScale = kDefaultSpeedScale;
    s.gear = kDefaultGear;
    s.fovExtraDeg = 0.0f;
    logInfo("RIDE start bird=0x%x pos=(%.1f,%.1f,%.1f) roof=%.0f ground=%.1f linkCall=%u linkProc=%u",
        static_cast<unsigned>(fopAcM_GetID(k)), k->current.pos.x, k->current.pos.y, k->current.pos.z,
        static_cast<double>(k->mRoofHeight), k->mGroundHeight, s.linkCalls,
        static_cast<unsigned>(daAlink_getAlinkActorClass() != nullptr ? daAlink_getAlinkActorClass()->mProcID : 0xFFFFu));
    notifyRideStart(k);
}

// Kurzer Hinweis im Spiel. UiService ist optional: ohne Dienst bleibt es beim Log.
void toastNotice(ModContext* ctx, const char* text) {
    if (svc_ui == nullptr) {
        logInfo("hint (no UI service): %s", text);
        return;
    }
    UiToastDesc d = UI_TOAST_DESC_INIT;
    d.body_rml = text;
    d.duration_ms = 3000;
    if (svc_ui->push_toast(ctx, &d) != MOD_OK) {
        logWarn("toast refused: %s", text);
    }
}

// Hinweise mit der Taste Absteigen (Pflichtaktion, nie leer). Jeder Hinweis hat einen eigenen festen Puffer.
void notifyDismountOnlyInFlight(ModContext* ctx) {
    static char text[160];
    std::snprintf(text, sizeof text, "Dismount only works while the bird is flying. Press %s again.",
        actions::keyLabel(actions::indexOf(actions::ActId::Dismount)));
    toastNotice(ctx, text);
}

void notifySaveRefused(ModContext* ctx) {
    static char text[160];
    std::snprintf(text, sizeof text, "Dismount first (%s), then save.",
        actions::keyLabel(actions::indexOf(actions::ActId::Dismount)));
    toastNotice(ctx, text);
}

bool hasOwnBird() {
    return session().birdId != kNoProc;
}

// Ruf-Kombination verarbeiten. Die Annahme (callBlockReason) hat der Pad-Hook schon geprueft und B verschluckt;
// hier nur noch der Start (Wolf: Vogel, Mensch: Modul autowolf). Jeder Abbruch hier gibt B zurueck.
void tryCall(daAlink_c* link) {
    Session& s = session();
    if (s.phase == Phase::Transforming) {
        logEvery(g_logTransformCall, 3, 60, "call ignored, transformation running");
        core::restoreChordB("transformation running");
        return;
    }
    if (link->checkWolf() == 0) {
        bool handled = false;
        forEachModule("onHumanCall", [&](Module& m) {
            if (!handled) {
                handled = m.onHumanCall(s, link);
            }
        });
        if (!handled) {
            ++g_counts.humanRefused;
            logEvery(g_logHumanCall, 3, 60, "human form - ride needs wolf (no module took the call)");
            core::restoreChordB("human form, no module took the call");
        }
        return;
    }
    if (const char* why = callBlockReason(link)) {
        // Ereignis-Sperre getrennt zaehlen (eventRefused war nie gesetzt); sonst Busy.
        if (dComIfGp_event_runCheck() != 0) {
            ++g_counts.eventRefused;
        } else {
            ++g_counts.busyRefused;
        }
        logEvery(g_logDrop, 3, 60, "call dropped at link frame: %s", why);
        core::restoreChordB(why);
        return;
    }
    if (summonBird(link) != MOD_OK) {
        core::restoreChordB("spawn failed");
    }
}

// Link haelt den Vogel in mCargoCarryAcKeep (setPlayerRideOn -> setCargoCarry). offCargoCarry() loescht nur den
// Modus, der Zeiger bleibt. Vor dem Freigeben des Vogels wird er geloest, denn Hakenschuss-Pfade lesen ihn.
// Der Vergleich liest den Vogel nicht.
void clearCargoKeep(daKago_c* bird) {
    daAlink_c* link = liveLink();
    if (link != nullptr && link->mCargoCarryAcKeep.getActor() == static_cast<fopAc_ac_c*>(bird)) {
        link->mCargoCarryAcKeep.clearData();
        ++g_counts.keepCleared;
        logInfo("Link cargo keep cleared (bird is being freed)");
    }
}

// E_YM-Akteure, die den Vogel als mpKago halten, verlieren den Zeiger. E_YM liest ihn in ACT_RIVER jeden Tick.
int clearYmRef(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac == nullptr || fopAcM_GetName(ac) != fpcNm_E_YM_e) {
        return 1;
    }
    daE_YM_c* ym = static_cast<daE_YM_c*>(ac);
    if (ym->mpKago != static_cast<daKago_c*>(data)) {
        return 1;
    }
    ym->mpKago = nullptr;
    if (ym->mAction == daE_YM_c::ACT_RIVER) {
        ym->mMode = 0;
    }
    ++g_counts.ymCleared;
    return 1;
}

void clearYmRefs(daKago_c* bird) {
    fopAcIt_Executor(clearYmRef, bird);
}

#if defined(KFF_DEV_AUTOTEST)
// WP4 (nur Testbuild): KFF_WP4_AUS=1 ist der Vergleichslauf ohne die WP4-Absicherungen (Skip-Hygiene, Ereignis-Sperre beim
// Ruf). Einmal gelesen, danach gecacht (linkExecutePre laeuft jeden Frame).
bool wp4Off() {
    static const bool off = testFlag("KFF_WP4_AUS");
    return off;
}
#endif

// WP4 (T3): Ereignis-Sperre fuer den gerufenen Vogel (kein Ritt-Start im Ereignis, Sitzung endet). Im Release immer an.
bool summonEventGate() {
#if defined(KFF_DEV_AUTOTEST)
    return !wp4Off();
#else
    return true;
#endif
}

// WP4 (Skip-Hygiene, Befund P0): Vor dem Entfernen eines Vogels darf das Ereignis-System keinen Skip-Rueckruf auf ihn
// behalten. Der Skipper faellt bei totem Skip-Akteur auf Link zurueck (d_event.cpp:924-928) und ruft dann
// daKago_c::DemoSkipCallBack mit Link als Vogel auf. Zeigt mSkipActorId auf diesen Vogel, wird das Skip-System geleert.
// Ist der eigene Vogel Beteiligter eines laufenden Ereignisses (mPt1/mPt2, z. B. sein Perch-Auftrag aus executeEvent),
// endet das Ereignis mit ihm, damit es nicht mit einem toten Akteur weiterlaeuft. executeEvent hat die Kamera gestoppt,
// deshalb startet sie wieder (wie demo_skip, d_a_kago.cpp:563-566). Nur Prozess-IDs, der Vogel wird nicht gelesen.
void birdDeleteHygiene(fpc_ProcID id, bool own, const char* where) {
#if defined(KFF_DEV_AUTOTEST)
    if (wp4Off()) {
        return;
    }
#endif
    if (id == kNoProc) {
        return;
    }
    dEvt_control_c* evt = dComIfGp_getEvent();
    if (evt == nullptr) {
        return;
    }
    if (evt->mSkipActorId == id) {
        evt->clearSkipSystem();
        ++g_counts.skipCleared;
        logInfo("skip cleared on bird delete (%s, id=0x%x)", where, static_cast<unsigned>(id));
    }
    if (own && evt->runCheck() && (evt->mPt1 == id || evt->mPt2 == id)) {
        // Ein schon angefordertes Ende (Flag 8, ausgewertet im naechsten Step, d_event.cpp:975) wird nicht wiederholt
        // (requestDelete und danach _delete im selben Frame); ein zweites reset() bestellte bei einem Karten-Ereignis
        // dessen Folge noch einmal (d_event.cpp:697-702).
        const bool pending = evt->chkEventFlag(8) != 0;
        if (!pending) {
            dComIfGp_event_reset();
            ++g_counts.eventReset;
        }
        if (dCamera_c* cam = dCam_getBody()) {
            cam->Start();
            cam->SetTrimSize(0);
        }
        logInfo("event reset on bird delete (%s, id=0x%x)%s, camera started", where, static_cast<unsigned>(id),
            pending ? ": already requested" : "");
    }
}

// Eigene Voegel ohne Sitzung entfernen (nach einem Szenenwechsel, wenn der Vogel nicht mehr verfolgt wird).
int removeOrphanBird(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac == nullptr || fopAcM_GetName(ac) != fpcNm_KAGO_e) {
        return 1;
    }
    daKago_c* k = static_cast<daKago_c*>(ac);
    const fpc_ProcID keep = *static_cast<fpc_ProcID*>(data);
    if (k->mType != kFriendlyKagoType || fopAcM_GetID(k) == keep) {
        return 1;
    }
    birdDeleteHygiene(fopAcM_GetID(k), false, "orphan");  // WP4: Skip-Rueckruf loesen (kein Ereignis-Ende: evtl. Vanilla-Vogel)
    fopAcM_delete(fopAcM_GetID(k));
    ++g_counts.orphanRemoved;
    logInfo("orphan bird 0x%x removed (no session)", static_cast<unsigned>(fopAcM_GetID(k)));
    return 1;
}

// Nach dem Absteigen faellt Link aus dem Vogel. mFallHeight und field_0x33c8 (Sturzreferenz, die das Spiel beim
// Bodenkontakt auswertet: var_f31 = field_0x33c8 - pos.y, d_a_alink.cpp) sind dann noch der Gipfel des Rittes und
// wuerden beim Aufkommen als Sturz gewertet. Bis zur ersten Bodenberuehrung werden beide auf die aktuelle Hoehe gesetzt.
void holdFallReference(daAlink_c* link) {
    link->mFallHeight = link->current.pos.y;
    link->field_0x33c8 = link->current.pos.y;
}

// F5 R2-A: Sturzreferenz auf eine feste Hoehe setzen (hier: Bodenhoehe unter Link). Link selbst wird nicht bewegt.
void setFallReference(daAlink_c* link, float y) {
    link->mFallHeight = y;
    link->field_0x33c8 = y;
}

// Vogel loesen: Link und Midna freigeben, Zeiger auf den Vogel loeschen. Vor dem Entfernen aufrufen.
void detachBird(daKago_c* bird) {
    releaseRider(bird);
    clearCargoKeep(bird);
    clearYmRefs(bird);
}

// D-C: Der Vogel ist nicht mehr auffindbar, ohne ueber den Entfernen-Pfad gegangen zu sein. Link und Midna werden trotzdem
// geloest, damit kein Cargo-Zustand und kein Midna-Zeiger bleibt. Der Vogel selbst wird nicht angefasst, sein Zeiger ist
// dann ungueltig. Verwendet werden nur Modus-Flag, Zeiger-Feld der Halterung (clearData setzt nur Felder) und die Zeiger
// von Midna (offCargoActor setzt mpKago auf null). Midna wird nur geloest, wenn sie noch einen Zeiger auf einen Vogel haelt.
void releaseRiderOfGoneBird() {
    daAlink_c* link = liveLink();
    bool released = false;
    if (link != nullptr && link->checkCargoCarry()) {
        link->offCargoCarry();  // setzt nur den Modus zurueck
        released = true;
    }
    if (link != nullptr && link->mCargoCarryAcKeep.getActor() != nullptr) {
        link->mCargoCarryAcKeep.clearData();  // setzt nur mID und mActor auf null, der Vogel wird nicht gelesen
        released = true;
    }
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna != nullptr && midna->mpKago != nullptr) {
        midna->offTagWaitPos();
        midna->offCargoActor();
        released = true;
    }
    if (released) {
        ++g_counts.goneRelease;
        logInfo("bird gone: Link and Midna released without the bird");
    }
}

void trackBird() {
    Session& s = session();
    fopAc_ac_c* actor = nullptr;
    const s32 found = fopAcM_SearchByID(s.birdId, &actor);  // s.birdAge zaehlt kernTimersTick (Kern-Ticks, WP9)
    if (found == 0) {
        if (s.birdSeen) {
            logInfo("bird gone (not deleted through the hook), state reset");
            if (s.phase == Phase::Riding) {
                notifyRideEnd(nullptr, EndReason::BirdGone);
            }
            releaseRiderOfGoneBird();  // D-C: Link und Midna loesen, bevor die Sitzung leer ist
            resetSession();
        } else if (s.birdAge > kCreateTimeoutFrames) {
            logInfo("bird never appeared, state reset");
            resetSession();
        }
        return;
    }
    if (actor != nullptr) {
        s.birdSeen = true;
    }
    // Summoning ohne Ritt-Start: Zeitgrenze in Kern-Ticks in summon::tick (WP6, R4).
    // Befund 5 (F2): Das Wiederholen eines angeforderten Entfernens macht allein retryPendingDelete (begrenzt auf
    // kPendingGiveUpTicks, logWarn bei Aufgabe). Hier stand ein zweiter Pfad ohne Grenze, der alle 60 Ticks schrieb.
}

// Angefordertes Entfernen (auch nach endRideClean): prueft je Pad-Tick, ob der Vogel noch lebt, wiederholt fopAcM_delete
// nach kDeleteRetryFrames Ticks und gibt nach kPendingGiveUpTicks auf (dann bleibt nur der Sitzungs-Zaehler im Log).
void retryPendingDelete() {
    if (g_pendingBirdId == kNoProc) {
        return;
    }
    fopAc_ac_c* actor = nullptr;
    if (fopAcM_SearchByID(g_pendingBirdId, &actor) == 0) {
        logInfo("pending bird 0x%x removed", static_cast<unsigned>(g_pendingBirdId));
        g_pendingBirdId = kNoProc;
        return;
    }
    ++g_pendingWait;
    if (g_pendingWait >= kPendingGiveUpTicks) {
        logWarn("pending bird 0x%x still alive after %u ticks, pending state dropped",
            static_cast<unsigned>(g_pendingBirdId), g_pendingWait);
        g_pendingBirdId = kNoProc;
        return;
    }
    if (g_pendingWait % kDeleteRetryFrames == 0) {
        fopAcM_delete(g_pendingBirdId);
        ++g_counts.deleteRetry;
        logInfo("pending bird delete retried (still alive)");
    }
}

// Skriptzustaende des Vogels (z. B. Ereignis durch Wolf-Heulen) fuehren im Ritt zu Haengern.
// Ausserhalb von Ereignissen zurueck in FLY Modus 1.
void keepRideState(daKago_c* k) {
    if (k->mAction == daKago_c::ACTION_FLY_e || k->mAction == daKago_c::ACTION_ATTACK_e) {
        return;
    }
#if defined(KFF_DEV_AUTOTEST)
    if (testFlag("KFF_TEST_NOGUARD")) {
        return;  // WP5.16 (Testbuild): Griff-Schutz aus, Vanilla-Stagger laeuft bis zum Abwurf
    }
#endif
    if (dComIfGp_event_runCheck()) {
        return;
    }
    ++g_counts.forcedFly;
    logEvery(g_logForcedFly, 1, kLogEvery, "action %d forced to FLY during ride (count %u)", k->mAction,
        g_counts.forcedFly);
    k->setActionMode(daKago_c::ACTION_FLY_e, 1);
}

}  // namespace

// N1: der Vogel ist zum Entfernen vorgemerkt (g_pendingBirdId, bis zum _delete-Hook oder dem Verwaisungs-Ende).
// Liegt im Namensraum kff (ausserhalb des anonymen Namensraums, damit summon.cpp ihn sieht).
bool isPendingBird(fpc_ProcID id) {
    return id != kNoProc && id == g_pendingBirdId;
}

namespace ride {

SceneChange sceneChange() {
    return g_sceneChange;
}

bool isPositionDriven() {
    return g_sceneChange == SceneChange::Position || g_sceneChange == SceneChange::None;
}

}  // namespace ride

// ---------------------------------------------------------------------------------------------
// Aktionen des Kerns (siehe core/kff/session.hpp). summonBird steht in src/core/summon.cpp (WP6).
// ---------------------------------------------------------------------------------------------

// Boden unter dem Vogel: groundY ist die Bodenhoehe (checkGroundHeight), false ohne Boden (Abgrund).
bool groundBelow(daKago_c* bird, float* groundY) {
    // T9: echtes Gelaende unter dem Vogel, ohne den Flugversatz mFlightGroundAltitude aus checkGroundHeight.
    // Eigene Bodensonde nach unten wie d_a_kago.cpp (Start 500 ueber der Position); fehlt der Boden, Abgrund.
    dBgS_GndChk gndChk;
    cXyz gndChkPos(bird->current.pos.x, bird->current.pos.y + 500.0f, bird->current.pos.z);
    gndChk.SetPos(&gndChkPos);
    *groundY = dComIfG_Bgsp().GroundCross(&gndChk);
    return *groundY > kNoGroundY;
}

// E9: Freigabe nach einem Landeanflug ohne Horizontalgeschwindigkeit: Links Geschwindigkeit wird auf 0 gesetzt, damit er
// nicht ueber Kanten geschleudert wird. Gemessen fuer den Log: Strecke vom B-Punkt bis zur Freigabe, Gang, Geschwindig-
// keiten von Vogel und Link.
// Z1 (d): gear wird VOR notifyRideEnd erfasst (tempo setzt den Gang in onRideEnd zurueck, daher stand im Log immer
// "gang 3"). "link horizontal letzter Frame" ist die Bewegung des letzten Ritt-Frames (current - old) VOR dem Nullsetzen;
// die Zeile entsteht nach dem Nullsetzen und nennt die Restwerte (Soll 0.000).
void releaseLandedHorizontal(daKago_c* bird, const char* why, int gear) {
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return;
    }
    const float dxB = bird->current.pos.x - g_landX;
    const float dzB = bird->current.pos.z - g_landZ;
    const float dxL = link->current.pos.x - link->old.pos.x;
    const float dzL = link->current.pos.z - link->old.pos.z;
    const float before = std::sqrt(dxL * dxL + dzL * dzL);
    link->speedF = 0.0f;
    link->speed.x = 0.0f;
    link->speed.z = 0.0f;
    link->old.pos.x = link->current.pos.x;
    link->old.pos.z = link->current.pos.z;
    logInfo("landing release (%s): strecke %.1f gang %d bird speedF %.3f link horizontal letzter Frame %.3f, nach Freigabe "
            "speedF %.3f speed (%.3f, %.3f)",
        why, static_cast<double>(std::sqrt(dxB * dxB + dzB * dzB)), gear, static_cast<double>(bird->speedF),
        static_cast<double>(before), static_cast<double>(link->speedF), static_cast<double>(link->speed.x),
        static_cast<double>(link->speed.z));
}

// Z1 (b): Boden unter Link (gleiche Sonde wie groundBelow: Start 500 ueber der Position, nach unten).
bool groundBelowLink(daAlink_c* link, float* groundY = nullptr) {
    dBgS_GndChk gndChk;
    cXyz gndChkPos(link->current.pos.x, link->current.pos.y + 500.0f, link->current.pos.z);
    gndChk.SetPos(&gndChkPos);
    const float ground = dComIfG_Bgsp().GroundCross(&gndChk);
    if (groundY != nullptr) {
        *groundY = ground;  // F5 R2-A: Bodenhoehe unter Link (Sturzreferenz beim Entladen nach dem Absteigen)
    }
    return ground > kNoGroundY;
}

// F6: Wasserflaeche unter einem Punkt (Spl-Gruppe, wie d_a_kago.cpp setSibukiEffect). false ohne Wasser.
bool waterBelowPoint(const cXyz& pos, float* waterY) {
    dBgS_ObjGndChk_Spl gndChkSpl;
    cXyz gndChkPos(pos.x, pos.y + 500.0f, pos.z);
    gndChkSpl.SetPos(&gndChkPos);
    *waterY = dComIfG_Bgsp().GroundCross(&gndChkSpl);
    return *waterY > kNoGroundY;
}

// F6: Oberflaeche unter einem Punkt: die hoehere von Boden und Wasserflaeche. false, wenn beides fehlt (Abgrund).
bool surfaceBelowPoint(const cXyz& pos, float* surfaceY) {
    dBgS_GndChk gndChk;
    cXyz gndChkPos(pos.x, pos.y + 500.0f, pos.z);
    gndChk.SetPos(&gndChkPos);
    const float ground = dComIfG_Bgsp().GroundCross(&gndChk);
    float water = kNoGroundY;
    const bool hasWater = waterBelowPoint(pos, &water);
    const bool hasGround = ground > kNoGroundY;
    if (hasWater && (!hasGround || water > ground)) {
        *surfaceY = water;
        return true;
    }
    *surfaceY = ground;
    return hasGround;
}

// F6: Oberflaeche unter dem Vogel (Landeanflug, Entladen).
bool surfaceBelow(daKago_c* bird, float* surfaceY) {
    return surfaceBelowPoint(bird->current.pos, surfaceY);
}

// F6: Oberflaeche unter Link (Sturzreferenz nach dem Absteigen, Entladen).
bool surfaceBelowLink(daAlink_c* link, float* surfaceY = nullptr) {
    float surface = kNoGroundY;
    const bool found = surfaceBelowPoint(link->current.pos, &surface);
    if (surfaceY != nullptr) {
        *surfaceY = surface;
    }
    return found;
}

#if defined(KFF_DEV_AUTOTEST)
// Z1 (e), nur Testbuild: Lage und Wurzelbewegung von Link bei der Freigabe und in den ersten Link-Frames danach
// (Herkunft des Versatzes im ersten Fall-Frame ohne Geschwindigkeit). rootMode ist daAlink_c::field_0x2f99
// (Bits 4/1/2: Wurzel-Z/X/Y wird in posMove als Bewegung uebernommen), root ist field_0x34d4 (Wurzel der Animation).
constexpr uint32_t kReleaseDiagFrames = 3;
uint32_t g_releaseDiag = 0;
void logReleaseDiag(daAlink_c* link, const char* when) {
    logInfo("release diag %s: pos=(%.1f,%.1f,%.1f) old=(%.1f,%.1f,%.1f) proc=%u rootMode=0x%x root=(%.2f,%.2f,%.2f) "
            "shapeY=%d angleY=%d speedF=%.3f cargo=%d",
        when, static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
        static_cast<double>(link->current.pos.z), static_cast<double>(link->old.pos.x),
        static_cast<double>(link->old.pos.y), static_cast<double>(link->old.pos.z),
        static_cast<unsigned>(link->mProcID), static_cast<unsigned>(link->field_0x2f99),
        static_cast<double>(link->field_0x34d4.x), static_cast<double>(link->field_0x34d4.y),
        static_cast<double>(link->field_0x34d4.z), static_cast<int>(link->shape_angle.y),
        static_cast<int>(link->current.angle.y), static_cast<double>(link->speedF), link->checkCargoCarry() ? 1 : 0);
}
#endif

// Endgueltiges Absteigen: Ritt-Ende an die Module, Link und Midna loesen, Vogel fliegt weg und wird spaeter entfernt.
void finishDismount(daKago_c* bird, EndReason reason, const char* why) {
    Session& s = session();
    const int gear = s.gear;  // Z1 (d): vor notifyRideEnd (tempo setzt den Gang dort zurueck)
    notifyRideEnd(bird, reason);
    releaseRider(bird);
    if (s.landFrames > 0) {
        releaseLandedHorizontal(bird, why, gear);  // E9: Landeanflug (Freigabe, zweites B, Abgrund, Zeitlimit)
    }
    bird->shape_angle.y = bird->current.angle.y;  // setFlyAway() nimmt shape_angle.y als Kurs
    bird->setFlyAway();
    const uint32_t rideFrame = s.rideFrames;
    const uint32_t landed = s.landFrames;
    // Z1 (b), OE1 (Void-Out ist Vanilla): Die Sturzreferenz wird einmal auf die Loslasshoehe gesetzt und nur dann weiter
    // gehalten, wenn unter Link Boden liegt (kein Sturzschaden aus der Ritthoehe). Ohne Boden faellt Link wie nach einem
    // Sprung von der Loslasshoehe: Vanilla checkRestartRoom (d_a_alink.cpp, field_0x33c8 - y > 2000 ohne Boden) loest den
    // Void-Out aus. Befund WP1 f-void1: mit dauerndem Halten kam der Void-Out erst 619 Ticks nach dem Ende.
    bool hold = false;
    if (daAlink_c* link = liveLink()) {
        holdFallReference(link);
        hold = surfaceBelowLink(link);  // F6: Boden oder Wasserflaeche unter Link halten (Void-Out nur ohne beides)
#if defined(KFF_DEV_AUTOTEST)
        logReleaseDiag(link, "frame +0 (Freigabe)");
        g_releaseDiag = kReleaseDiagFrames;
#endif
    }
    g_fallHold = hold;
    g_fallWatch = hold ? 1 : 0;
    g_releaseFall = hold;  // Befund 2: Fenster fuer linkPosMovePost; ohne Boden darunter gilt der Void-Out der Vanilla
    g_releaseFallTicks = 0;
    s.phase = Phase::Dismounting;
    s.departFrames = 0;
    s.riderLost = 0;
    s.landFrames = 0;
    s.landing = false;
    ++g_counts.dismount;
    // Z1 (c): Grundname in der Zeile, dazu eine Ende-Zeile im Format von endRideClean (jedes Ritt-Ende hat eine
    // "ride ended clean (<Grund>"-Zeile).
    logInfo("dismount (%s, %s) at ride frame %u after a landing approach of %u frames, the bird flies away and is removed "
            "after %u frames",
        endReasonName(reason), why, rideFrame, landed, kDepartDeleteFrames);
    ++g_counts.endClean;  // Befund 4 (F2): ein Abflug ist ein Ritt-Ende ohne Abflugflug und zaehlt wie endRideClean
    logInfo("ride ended clean (%s, %s), bird departing, fallHold=%d", endReasonName(reason), why, hold ? 1 : 0);
}

// Absteigen. B mit Boden darunter startet zuerst den Landeanflug (Steuerungstabelle Zeile 24): der Vogel sinkt, der Ritt
// laeuft weiter, bis kLandClearance erreicht ist oder kLandMaxFrames vergangen sind. Ohne Boden (Abgrund) oder bei
// anderen Gruenden sofort loslassen.
void dismount(daKago_c* bird, EndReason reason, const char* why) {
    Session& s = session();
    if (bird == nullptr || s.phase != Phase::Riding) {
        return;
    }
    if (reason == EndReason::DismountButton) {
        if (s.landFrames > 0) {
            finishDismount(bird, reason, why);  // T9: sofort los. Grund des Aufrufers (griff: Landeanflug ueber Wasser, H5 F7)
            return;
        }
        float ground = 0.0f;
        if (surfaceBelow(bird, &ground)) {  // F6: Landeanflug auf Wasser oder Boden
            s.landFrames = 1;
            s.landing = true;  // freiheit setzt die Untergrenze aus
            g_landX = bird->current.pos.x;
            g_landZ = bird->current.pos.z;
            ++g_counts.landings;
            logInfo("landing approach (%s): ground %.1f, bird %.1f, gang %d, speedF %.3f, the bird brakes and sinks for up to "
                    "%u frames",
                why, static_cast<double>(ground), static_cast<double>(bird->current.pos.y), s.gear,
                static_cast<double>(bird->speedF), kLandMaxFrames);
            return;
        }
    }
    finishDismount(bird, reason, why);
}

void requestDelete(EndReason reason, const char* why) {
    Session& s = session();
    if (s.birdId == kNoProc || s.deleteRequested) {
        return;
    }
    // F: Werte fuer die Vertragszeile "bird deleted" sichern, bevor die Sitzung leer ist (endRideClean).
    g_lastEnd.id = s.birdId;
    g_lastEnd.rideFrames = s.rideFrames;
    g_lastEnd.departFrames = s.departFrames;
    g_lastEnd.riding = s.phase == Phase::Riding;
    g_lastEnd.departing = s.phase == Phase::Dismounting;
    fopAc_ac_c* actor = nullptr;
    const bool alive = fopAcM_SearchByID(s.birdId, &actor) != 0;
    daKago_c* bird = alive ? liveBird() : nullptr;
    if (s.phase == Phase::Riding) {
        notifyRideEnd(bird, reason);  // einmal: danach ist der Ritt beendet, kein zweites onRideEnd im _delete-Hook
        s.phase = Phase::Idle;
    }
    if (bird != nullptr) {
        detachBird(bird);                                   // Link und Midna loesen, Zeiger freigeben
        bird->setActionMode(daKago_c::ACTION_WAIT_e, 0);    // erst WAIT ...
        bird->field_0x6df = 1;                              // ... dann Entfernen (kein Ritt-Neustart)
        fakepath::refresh(bird);  // mpPath1 gueltig halten, falls der Vogel noch einen Frame laeuft (absturz-mppath1)
    } else if (!alive && s.birdSeen) {
        releaseRiderOfGoneBird();  // D-C: der Vogel war da und ist schon weg, Link und Midna trotzdem loesen
    }
    if (alive) {
        birdDeleteHygiene(s.birdId, true, endReasonName(reason));  // WP4: Skip-Rueckruf und eigenes Ereignis vor dem Entfernen loesen
        fopAcM_delete(s.birdId);
        g_pendingBirdId = s.birdId;  // bleibt angefordert, auch wenn endRideClean die Sitzung gleich leert
        g_pendingWait = 0;
    }
    s.deleteRequested = true;
    s.deleteWait = 0;
    ++g_counts.deleteRequested;
    logInfo("bird delete requested (%s, %s), id=0x%x alive=%d", endReasonName(reason), why,
        static_cast<unsigned>(s.birdId), alive ? 1 : 0);
}

void endRideClean(EndReason reason, const char* why) {
    daAlink_c* link = liveLink();
    // F5 (Befund B3, Korrektur 1): Entladen im Ritt auf Reiseflughoehe. Der regulaere Abstieg setzt Link vor der Freigabe
    // auf den Boden (Landeanflug). Der Entladepfad gibt ihn auf der Ritthoehe frei; danach ist die Mod weg und kann die
    // Sturzreferenz nicht mehr halten, Vanilla rechnet den ganzen Fall (Lauf H2: rund 8300 Einheiten, Leben 12 -> 0).
    // Deshalb wird Link hier auf den Boden unter dem Vogel gesetzt, wie am Ende des Landeanflugs. Nur beim Entladen
    // (Shutdown) im Ritt, nur mit Boden darunter und nur oberhalb des Landeanflug-Abstands.
    float snapGround = 0.0f;
    bool snap = false;
    if (reason == EndReason::Shutdown && link != nullptr && session().phase == Phase::Riding) {
        if (daKago_c* bird = liveBird()) {
            snap = surfaceBelow(bird, &snapGround) && link->current.pos.y - snapGround > kLandClearance;  // F6: Wasser oder Boden
        }
    }
    requestDelete(reason, why);
    if (snap) {
        const float fromY = link->current.pos.y;
        link->current.pos.y = snapGround;
        link->old.pos.y = snapGround;
        logInfo("unload landing: Link von %.1f auf den Boden %.1f gesetzt (kein Sturz aus der Ritthoehe)",
            static_cast<double>(fromY), static_cast<double>(snapGround));
    }
    // F5 R2-A (Befund B3, Korrektur 2): Entladen in Phase Dismounting. Link ist dann frei (finishDismount hat ihn
    // losgelassen) und faellt mit Vanilla-Physik. Die Haltung der Sturzreferenz (linkExecutePre, jeden Frame) endet mit dem
    // Entladen. Die Referenz bliebe auf der Freigabehoehe, und der Fall aus der Hoehe waere toedlich (Lauf H2). Deshalb wird
    // sie auf den Boden unter Link gesetzt, nicht auf die Freigabehoehe. Link wird nicht bewegt (kein Teleport). Ohne Boden
    // (Abgrund) bleibt es bei Vanilla (Void-Out).
    bool groundRef = false;
    float groundNow = 0.0f;
    const bool freeLink = reason == EndReason::Shutdown && link != nullptr && session().phase == Phase::Dismounting &&
                          !link->checkCargoCarry();
#if defined(KFF_DEV_AUTOTEST)
    const bool r2On = std::getenv("KFF_TEST_R2OFF") == nullptr;  // Kontrolllauf (nur Testbuild): Korrektur aus
#else
    const bool r2On = true;
#endif
    if (freeLink && r2On && !link->mLinkAcch.ChkGroundHit()) {
        groundRef = surfaceBelowLink(link, &groundNow);  // F6: Wasserflaeche oder Boden
    }
    if (groundRef) {
        const float fromY = link->current.pos.y;
        // Vanilla (linkPosMove): ist Modusflagge 2 gesetzt, hebt das Spiel mFallHeight auf jede hoehere Position im Flug.
        // Ohne diese Flagge bleibt die Referenz auf dem Boden (Befund R2-A, Lauf keep-1: 12 -> 4 Leben beim Aufkommen).
        const bool flag2 = link->checkModeFlg(2) != 0;
        link->offModeFlg(2);
        setFallReference(link, groundNow);
        logInfo("unload free fall (Dismounting): Link frei bei y %.1f, Fallreferenz auf den Boden %.1f gesetzt (Abstand %.1f, "
                "Modusflagge 2 vorher %d, kein Teleport)",
            static_cast<double>(fromY), static_cast<double>(groundNow), static_cast<double>(fromY - groundNow), flag2 ? 1 : 0);
    }
    // Ist Link in der Luft (Menue, Ereignis, Titel), bleibt die Fallhoehe des Rittes gehalten, bis er den Boden
    // beruehrt; sonst wird die Ritthoehe beim Aufkommen als Sturz gewertet. Beim Tod entscheidet das Spiel.
    // Z1 (b), OE1: wie in finishDismount nur mit Boden unter Link; ohne Boden gilt der Vanilla-Void-Out.
    const bool hold = !groundRef && link != nullptr && reason != EndReason::Death && !link->mLinkAcch.ChkGroundHit() &&
                      surfaceBelowLink(link);  // F6: Wasser oder Boden
    if (groundRef) {
        g_fallHold = false;  // F5 R2-A: die Referenz ist gesetzt, es gibt nach dem Entladen keine Haltung mehr
        g_fallWatch = 0;
    } else if (hold) {
        holdFallReference(link);
        g_fallHold = true;
        g_fallWatch = 1;
    } else {
        g_fallHold = false;
        g_fallWatch = 0;
    }
    ++g_counts.endClean;
    logInfo("ride ended clean (%s, %s), session freed, fallHold=%d", endReasonName(reason), why, hold ? 1 : 0);
    resetSession();
}

void releaseRider(daKago_c* bird) {
    if (bird == nullptr) {
        return;
    }
    daAlink_c* link = liveLink();
    if (link != nullptr && link->checkCargoCarry()) {
        bird->setRideOff();  // Midna und Link
        return;
    }
    // Absturz in der Luft: offCargoCarry() setzt nur den Modus zurueck, Midna haelt den Vogel noch.
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna != nullptr && midna->mpKago == bird) {
        midna->offTagWaitPos();
        midna->offCargoActor();
    }
}

void resetSession() {
    Session& s = session();
    s.birdId = kNoProc;
    s.birdSeen = false;
    s.birdAge = 0;
    s.birdCalls = 0;
    s.phase = Phase::Idle;
    s.rideFrames = 0;
    s.departFrames = 0;
    s.riderLost = 0;
    s.deleteWait = 0;
    s.deleteRequested = false;
    s.lastAction = -1;
    s.yawInit = false;
    s.callRequested = false;
    s.dismountRequested = false;
    s.landFrames = 0;
    s.landing = false;
    s.rideGrip = false;
    g_dismountWish = 0;
}

// ---------------------------------------------------------------------------------------------
// Kern-Handler
// ---------------------------------------------------------------------------------------------

namespace core {

// B2 (F4): Speichern im Ritt wird verweigert (cardSavePre, Pre mit SKIP). Das Menue wartet dann auf SaveSync. Ohne
// Schreibauftrag liefert SaveSync bei Kartenstatus READY immer 0, das Menue bliebe stehen. Die Verweigerung schliesst den
// Auftrag deshalb mit einem Fehler ab: Kartenstatus FATAL_ERROR, SaveSync liefert 2 (Fehlertext, das Menue schliesst sich).
// Der vorherige Status kommt zurueck, sobald das Menue das Ergebnis gelesen hat, spaetestens nach kCardInjectMaxTicks
// Pad-Ticks oder beim Entladen.
struct CardInject {
    bool active = false;
    s32 prevState = 0;   // Kartenstatus vor der Verweigerung
    s32 prevField = 0;   // field_0x1fc8 vor der Verweigerung
    uint32_t ticks = 0;  // Pad-Ticks seit der Verweigerung
};
CardInject g_cardInject;
constexpr uint32_t kCardInjectMaxTicks = 120;

dMenu_save_c* currentSaveMenu() {
    dMw_c* window = dMeter2Info_getMenuWindowClass();
    return window != nullptr ? window->mpMenuSave : nullptr;
}

// Kartenschicht ruht: kein Befehl und kein Schreiben, Lesen oder Formatieren. Nur dann greift der Abschluss ein.
bool cardIdle() {
    const s32 state = g_mDoMemCd_control.mCardState;
    return g_mDoMemCd_control.mCardCommand == mDoMemCd_Ctrl_c::COMM_NONE_e &&
           state != mDoMemCd_Ctrl_c::CARD_STATE_WRITE_e && state != mDoMemCd_Ctrl_c::CARD_STATE_READ_e &&
           state != mDoMemCd_Ctrl_c::CARD_STATE_FORMAT_e;
}

// Kartenstatus zurueckschreiben (auch beim Entladen, damit kein FATAL_ERROR-Status zurueckbleibt).
void cardInjectRestore(const char* why) {
    if (!g_cardInject.active) {
        return;
    }
    g_mDoMemCd_control.mCardState = g_cardInject.prevState;
    g_mDoMemCd_control.field_0x1fc8 = g_cardInject.prevField;
    logInfo("card layer restored after the refused save (%s, %u pad ticks)", why, g_cardInject.ticks);
    g_cardInject = CardInject{};
}

// Abschluss einer Verweigerung. Laeuft die Kartenschicht gerade (Schreiben, Lesen, Formatieren), bleibt es beim Verweigern
// (Log): dann kann das Menue wie bisher warten, der Fall ist im Test nicht erzeugt worden.
void cardRefusalComplete() {
    if (g_cardInject.active) {
        return;  // der Abschluss der vorigen Verweigerung laeuft noch
    }
    if (!cardIdle()) {
        logWarn("save refused during ride, card layer busy (command %d, state %d): no completion, the menu may wait",
            g_mDoMemCd_control.mCardCommand, g_mDoMemCd_control.mCardState);
        return;
    }
    g_cardInject.active = true;
    g_cardInject.prevState = g_mDoMemCd_control.mCardState;
    g_cardInject.prevField = g_mDoMemCd_control.field_0x1fc8;
    g_cardInject.ticks = 0;
    g_mDoMemCd_control.mCardState = mDoMemCd_Ctrl_c::CARD_STATE_FATAL_ERROR_e;
    g_mDoMemCd_control.field_0x1fc8 = 1;
    if (dMenu_save_c* menu = currentSaveMenu()) {
        menu->mCmdState = 0;  // ein Wert ungleich 0 erscheint erst mit dem SaveSync des Menues (2)
    }
    logInfo("save refused during ride: card layer completes with error (state %d restored after the menu has read it)",
        g_cardInject.prevState);
}

// Je Pad-Tick: Kartenstatus zurueckgeben, sobald das Menue das Ergebnis gelesen hat oder die Frist abgelaufen ist.
void cardInjectTick() {
    if (!g_cardInject.active) {
        return;
    }
    ++g_cardInject.ticks;
    dMenu_save_c* menu = currentSaveMenu();
    const bool consumed = menu != nullptr && menu->mCmdState != 0;
    if (consumed || g_cardInject.ticks >= kCardInjectMaxTicks) {
        cardInjectRestore(consumed ? "menu read the result" : "time limit");
    }
}

// Je Pad-Tick, aufgerufen aus padReadPost (input.cpp) vor dem Schnappschuss.
void ridePadTick() {
    Session& s = session();
    cardInjectTick();  // B2: Kartenstatus nach einer verweigerten Speicherung zurueckgeben
    retryPendingDelete();  // angefordertes Entfernen des Vogels: auch ohne Link und ohne Sitzung weiterverfolgen
    summon::deferredTick();  // Befund 1: der wartende Ruf erzeugt den neuen Vogel erst nach dem Entfernen des alten
    // D-B: der Abflug zaehlt Pad-Ticks statt Vogel-Execute-Frames. Der Vogel-Execute steht im Game-Over still (Befund D-A);
    // das Entfernen laeuft hier deshalb unabhaengig vom Vogel.
    if (s.phase == Phase::Dismounting) {
        ++s.departFrames;
        // Befund 6 (F2, WP6.10): Antrag zwei Ticks vor der Frist. fopAcM_delete wirkt im Folgetick. Gemessen auf der Basis
        // (wp6-timeout): der Vogel war 101 Ticks nach R+B weg, Antrag bei departFrames 99, _delete bei 100. Jetzt bei 98.
        if (s.departFrames + 2 >= kDepartDeleteFrames && !s.deleteRequested) {
            requestDelete(EndReason::DeleteRequested, "departed");
        }
    }
    kernTimersTick(s);  // WP9 (R4): Landefenster, Fallreferenz, Wiederanbindung und Rufzeit je Kern-Tick
    g_sceneChange = ride::SceneChange::None;  // D1: die Einordnung eines Szenenwechsels gilt nur im selben Tick
    // WP6: Zeitgrenzen des Rufs in Kern-Ticks (R4). Liest keinen Eingabe-Schnappschuss (nur Sitzung, Link, Pause).
    summon::tick();
}

const char* rideCallBlockReason(daAlink_c* link) {
    return callBlockReason(link);
}

uint32_t& coreExceptionCount() {
    return g_counts.exceptions;
}

HookAction linkExecutePre(ModContext*, void* args, void*, void*) {
    return guardedPre("daAlink_c::execute", [&]() -> HookAction {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        if (link == nullptr) {
            if (session().callRequested) {
                session().callRequested = false;
                core::restoreChordB("no link at link frame");
            }
            return HOOK_CONTINUE;
        }
        // A3 B1: Waagerechte vor Proc und posMove dieses Ticks; linkPosMovePost setzt sie in der Mindestdauer darauf zurueck.
        if (g_releaseFall && g_releaseFallTicks < kReleaseFallMinTicks) {
            g_releaseFallX = link->current.pos.x;
            g_releaseFallZ = link->current.pos.z;
            g_releaseFallSnapOk = true;
        }
        Session& s = session();
        ++s.linkCalls;

        const fpc_ProcID linkId = fopAcM_GetID(link);
        if (linkId != s.linkId) {
            summon::abortDeferred("Link gewechselt");  // Befund 1: ein wartender Ruf gehoert zum alten Link
            if (s.linkId != kNoProc) {
                ++g_counts.linkChanged;
                logInfo("Link changed (id 0x%x -> 0x%x) at link call %u", static_cast<unsigned>(s.linkId),
                    static_cast<unsigned>(linkId), s.linkCalls);
            } else {
                logInfo("Link found id=0x%x wolf=%u at link call %u", static_cast<unsigned>(linkId),
                    static_cast<unsigned>(link->checkWolf()), s.linkCalls);
            }
            s.linkId = linkId;
            if (s.birdId != kNoProc) {
                // Der Vogel gehoert zum alten Link (Szenenwechsel): entfernen, nicht mehr verfolgen.
                requestDelete(EndReason::LinkChanged, "new Link actor");
            }
            if (s.phase == Phase::Transforming) {
                // Verwandlung des alten Links ist mit ihm verloren: Sitzung freigeben, sonst blockiert sie alle Rufe.
                ++g_counts.transformAborted;
                logInfo("transformation aborted, Link changed");
                resetSession();
            }
            // Eigene Voegel ohne Sitzung (z. B. aus dem Speicher des alten Raumes) entfernen.
            fpc_ProcID keep = s.birdId;
            fopAcIt_Executor(removeOrphanBird, &keep);
        }

        if (s.birdId != kNoProc) {
            trackBird();
        }

        if (s.callRequested) {
            s.callRequested = false;
            // D8: nur ein Ruf aus dem laufenden Pad-Tick (kein Latch ueber Ticks ohne Pad-Hook).
            if (core::callRequestFresh()) {
                tryCall(link);
            } else {
                core::dropStaleCall();
            }
        }

        if (link->mProcID == daAlink_c::PROC_DEAD && s.birdId != kNoProc) {
            // Link ist tot: der Vogel verschwindet sofort, auch im Abflug (D-A). Sonst friert er im Game-Over-Ereignis ein,
            // weil sein Execute dort nicht laeuft.
            ++g_counts.deadRider;
            if (g_counts.deadRider == 1) {
                logInfo("Link dead while riding or summoning, bird removed");
            }
            endRideClean(EndReason::Death, "Link tot");
        } else if (s.phase == Phase::Summoning && summonEventGate() &&
                   (dComIfGp_event_runCheck() != 0 || link->checkEventRun())) {
            // WP4 (T3, D6): Ein Ereignis startet, bevor der Ritt beginnt (Ruf im Startframe eines Ereignisses). Der gerufene
            // Vogel wird geordnet entfernt, statt mitten im Ereignis aufzusitzen oder zu warten.
            ++g_counts.summonEventEnd;
            endRideClean(EndReason::Event, "Ereignis vor dem Ritt-Start");
        } else if (s.phase == Phase::Riding) {
            if (dComIfGp_event_runCheck() != 0 || link->checkEventRun()) {
                // Ereignis im Ritt (Zwischensequenz, Gespraech, Szenenwechsel durch Ereignis): der Ritt endet vorher sauber
                // (D1). Das Spiel laeuft danach unveraendert. Das Modul freiheit beendet den Ritt nicht selbst.
                endRideClean(EndReason::Event, "Ereignis im Ritt");
            } else if (s.rideGrip) {
                // Griff (Mensch): der Halt wird im Modul mensch geprueft (Griff-Waechter, Ritt-Ende bei Verlust).
                s.riderLost = 0;
            } else if (link->checkCargoCarry()) {
                s.riderLost = 0;
                // Wolf-Link tritt nur am Boden in PROC_WOLF_CARGO_CARRY ein. In der Luft wird er hier gestartet.
                if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY && link->checkWolf() != 0) {
                    link->procWolfCargoCarryInit();
                }
            } else if (s.riderLost >= kRiderLostFrames) {  // zaehlt kernTimersTick je Kern-Tick (WP9, R4)
                ++g_counts.riderLost;
                toastNotice(mod_ctx, "The bird lost its grip on you. You dismounted.");  // P2-08: kein stilles Loslassen
                logInfo("rider lost: no cargo carry for %u Kern-Ticks, bird leaves (no call of ours)", s.riderLost);
                if (daKago_c* bird = liveBird()) {
                    dismount(bird, EndReason::RiderLost, "rider lost");
                } else {
                    logInfo("rider lost and bird gone, state reset");
                    notifyRideEnd(nullptr, EndReason::BirdGone);
                    releaseRiderOfGoneBird();  // D-C
                    resetSession();
                }
            }
        }

        if (g_fallHold) {
            const bool touched = link->mLinkAcch.ChkGroundHit();
            const bool wet = link->mLinkAcch.ChkWaterHit();  // F6: Wasserberuehrung beendet die Haltung wie Bodenkontakt
            if (touched || wet || g_fallWatch >= kFallWatchFrames) {
                g_fallHold = false;
                logInfo("fall reference released (%s): mFallHeight %.1f, field_0x33c8 %.1f, y %.1f",
                    touched ? "ground touched" : (wet ? "water reached" : "watch timeout"), static_cast<double>(link->mFallHeight),
                    static_cast<double>(link->field_0x33c8), static_cast<double>(link->current.pos.y));
            } else {
                holdFallReference(link);  // g_fallWatch zaehlt kernTimersTick je Kern-Tick (WP9, R4)
            }
        }
#if defined(KFF_DEV_AUTOTEST)
        if (g_releaseDiag > 0) {
            // Z1 (e): Stand VOR diesem Link-Execute; "+2 (pre)" zeigt die Wirkung des ersten Fall-Frames.
            char when[32];
            std::snprintf(when, sizeof when, "frame +%u (pre)", kReleaseDiagFrames + 1 - g_releaseDiag);
            logReleaseDiag(link, when);
            --g_releaseDiag;
        }
#endif

        notifyFrameLink(link);

#if defined(KFF_DEV_AUTOTEST)
        const bool linkLogDue = s.linkCalls % kLinkLogEvery == 0;  // Diagnose je Link-Aufruf nur im Testbuild (R9)
#else
        const bool linkLogDue = false;
#endif
        if (s.linkCalls <= 3 || linkLogDue) {
            logInfo("link calls=%u id=0x%x pos=(%.1f,%.1f,%.1f) wolf=%u cargo=%d proc=%u eventRun=%d phase=%s",
                s.linkCalls, static_cast<unsigned>(fopAcM_GetID(link)), link->current.pos.x, link->current.pos.y,
                link->current.pos.z, static_cast<unsigned>(link->checkWolf()), link->checkCargoCarry() ? 1 : 0,
                static_cast<unsigned>(link->mProcID), link->checkEventRun() ? 1 : 0, phaseName(s.phase));
        }
        return HOOK_CONTINUE;
    });
}

#if defined(KFF_DEV_AUTOTEST)
// Testbuild WP7.1/WP7.6/WP7.11: wirft genau einmal im Pre-Callback des Kerns, vor guardedPre, also ungefangen an das SDK:
// fail_mod, der Loader entlaedt die Mod, mod_shutdown beendet den Vogel (endRideClean, Grund Shutdown).
//   KFF_TEST_UNLOAD=1      im Ritt, nach 180 Ritt-Frames (WP7.1, WP7.11)
//   KFF_TEST_UNLOAD=summon Vogel gerufen (Phase Summoning), noch nicht im Ritt (WP7.6, verwaister Vogel)
// F5 (nur Testcode): KFF_TEST_UNLOAD_FRAME=<n> setzt den Ritt-Frame fuer KFF_TEST_UNLOAD=1 (Standard 180), gleicher Ort, anderer Zeitpunkt.
uint32_t testUnloadFrame() {
    const char* v = std::getenv("KFF_TEST_UNLOAD_FRAME");
    const unsigned long n = v != nullptr ? std::strtoul(v, nullptr, 10) : 0UL;
    return (n >= 1UL && n < 100000UL) ? static_cast<uint32_t>(n) : 180u;
}
void testUnloadPoint() {
    static bool fired = false;
    if (fired) {
        return;
    }
    const char* mode = std::getenv("KFF_TEST_UNLOAD");
    if (mode == nullptr) {
        return;
    }
    const bool inRide = mode[0] == '1' && isRiding() && session().rideFrames >= testUnloadFrame();
    const bool summoning = std::strcmp(mode, "summon") == 0 && session().phase == Phase::Summoning;
    // F5 R2-A (nur Testcode): KFF_TEST_UNLOAD=dismount entlaedt in Phase Dismounting, KFF_TEST_UNLOAD_FRAME Ticks nach dem
    // Absteigen (20..99 setzen; der Vogel ist nach kDepartDeleteFrames = 100 Ticks weg).
    const bool dismounting = std::strcmp(mode, "dismount") == 0 && session().phase == Phase::Dismounting &&
                             session().departFrames >= testUnloadFrame();
    if (!inRide && !summoning && !dismounting) {
        return;
    }
    fired = true;
    if (daAlink_c* link = liveLink()) {
        float ground = 0.0f;
        const bool hasGround = groundBelowLink(link, &ground);
        logInfo("KFF_TEST_UNLOAD vor dem Entladen: phase=%s, Link y %.1f, Boden %.1f (%s), Abstand %.1f, cargo %d, Leben "
                "%u/%u, Fallreferenz %.1f",
            phaseName(session().phase), static_cast<double>(link->current.pos.y), static_cast<double>(ground),
            hasGround ? "ja" : "kein", static_cast<double>(link->current.pos.y - ground), link->checkCargoCarry() ? 1 : 0,
            static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()),
            static_cast<double>(link->field_0x33c8));
    }
    if (std::getenv("KFF_TEST_KEEP") != nullptr) {
        // F5 R2-B (nur Diagnose): Ritt-Ende wie beim Entladen (endRideClean, Grund Shutdown), aber OHNE SDK-Entladen. Die
        // Hooks bleiben aktiv, damit das Lebenslog nach dem Entladen weiterlaeuft. Die Mod ist dann NICHT entladen.
        logWarn("KFF_TEST_KEEP (Diagnose): Ritt-Ende wie beim Entladen, Mod bleibt geladen");
        endRideClean(EndReason::Shutdown, "Entladen (Diagnose KEEP)");
        g_fallHold = false;
        g_fallWatch = 0;
        return;
    }
    logWarn("KFF_TEST_UNLOAD (%s): Ausnahme im Pre-Callback des Kerns (Testbuild), das SDK entlaedt die Mod",
        inRide ? "im Ritt" : (dismounting ? "Abstieg, Phase Dismounting" : "Vogel gerufen, nicht im Ritt"));
    throw std::runtime_error("KFF_TEST_UNLOAD");
}
#endif

HookAction kagoExecutePre(ModContext*, void* args, void*, void*) {
#if defined(KFF_DEV_AUTOTEST)
    testUnloadPoint();
#endif
    return guardedPre("daKago_c::execute", [&]() -> HookAction {
        daKago_c* k = mods::arg<daKago_c*>(args, 0);
        if (!isOwnBird(k)) {
            return HOOK_CONTINUE;
        }
        Session& s = session();
        ++s.birdCalls;
        k->mSceneType = kSceneTypeDefault;
        // D-D: ohne Pfad stuerzen Flug, Stagger und Abflug ab. Jeder Execute des eigenen Vogels prueft den Pfad, nicht nur
        // Ritt und Abflug. Die Ersatzpfad-Behandlung ist dieselbe wie in griff K6 (fakepath::refresh).
        if (k->mpPath1 == nullptr) {
            fakepath::refresh(k);
            ++g_counts.pathGuard;
            logEvery(g_logPathNull, 3, 300, "mpPath1 was null at bird execute, fake path set (count %u)",
                g_counts.pathGuard);
        }

        if (s.phase == Phase::Dismounting) {
            fakepath::refresh(k);  // FLY Modus 3 liest mpPath1
            // Die Entfernungsfrist zaehlt in padReadPost (D-B). Hier nur noch der Log.
            if (s.departFrames == 1 || s.departFrames % kLogEvery == 0) {
                logDepart(k);
            }
            return HOOK_CONTINUE;
        }

        if (s.phase != Phase::Riding) {
            daAlink_c* startLink = liveLink();
            // WP4 (T3): kein Ritt-Start, solange ein Ereignis laeuft; linkExecutePre beendet die Sitzung im selben Frame.
            const bool eventNow = summonEventGate() && startLink != nullptr &&
                                  (dComIfGp_event_runCheck() != 0 || startLink->checkEventRun());
            // WP6: summon::rideStartAllowed (Link lebt, kein Ereignis; Testbuild KFF_TEST_NORIDE).
            if (k->mAction == daKago_c::ACTION_WAIT_e && k->field_0x6df == 0 && startLink != nullptr && !eventNow &&
                summon::rideStartAllowed(startLink)) {
                startRide(k);
            } else {
                logEvery(g_logWaiting, 3, 120, "waiting for ride action=%d flag6df=%u mode=%d", k->mAction,
                    static_cast<unsigned>(k->field_0x6df), k->mMode);
            }
            return HOOK_CONTINUE;
        }

        fakepath::refresh(k);  // der Pfad muss zu Kurs und Position passen, bevor action() ihn liest

        // ui_halt (WP3): ein sichtbares Dusklight-Dokument blockiert die Pad-Eingabe, das Spiel laeuft weiter. Der Vogel
        // steht dann (Modul flug), Landeanflug und B-Wunsch ruhen bis zum Schliessen (kein Sinken, keine Frist).
        const bool uiHalt = input().uiDocument;
        if (uiHalt && (s.landFrames > 0 || g_dismountWish > 0)) {
            logEvery(g_logUiHalt, 1, 300, "landing approach and dismount wish paused, Dusklight document visible "
                                          "(landFrames %u, wish %u)",
                s.landFrames, g_dismountWish);
        }

        // Landeanflug nach B (Steuerungstabelle Zeile 24): der Vogel sinkt, bis Mindesthoehe erreicht ist oder die Zeit
        // um ist. Ohne Boden darunter sofort loslassen. Der Ritt laeuft dabei weiter (Module sehen Phase Riding).
        if (s.landFrames > 0 && !uiHalt) {
            // T9: Landeanflug. Zweites B = sofort los (bewusste Entscheidung). Freigabe bei Bodenabstand <= kLandClearance,
            // sobald Links Fuesse den Boden beruehren oder nach kLandMaxFrames. Ohne Boden sofort (Abgrund).
            if (s.dismountRequested) {
                s.dismountRequested = false;
                ++g_counts.landingDone;
                finishDismount(k, EndReason::DismountButton, "zweites B im Landeanflug");
                return HOOK_CONTINUE;
            }
            float ground = 0.0f;
            const bool hasGround = surfaceBelow(k, &ground);  // F6: Wasser oder Boden (hoehere Flaeche)
            const float clearance = k->current.pos.y - ground;
            daAlink_c* landLink = liveLink();
            const bool feetOnGround = landLink != nullptr && landLink->mLinkAcch.ChkGroundHit();
            if (!hasGround || clearance <= kLandClearance || feetOnGround || s.landFrames >= kLandMaxFrames) {
                ++g_counts.landingDone;
                logInfo("landing done: clearance %.1f, feet %d, frames %u, ground %s", static_cast<double>(clearance),
                    feetOnGround ? 1 : 0, s.landFrames, hasGround ? "yes" : "no");
                finishDismount(k, EndReason::DismountButton, hasGround ? "Landeanflug fertig" : "Abgrund im Landeanflug");
                return HOOK_CONTINUE;
            }
            // Adaptive Sinkrate: in kLandPlanFrames Ticks auf kLandClearance, danach mit kLandSinkMax weiter.
            const float left = static_cast<float>(kLandPlanFrames) - static_cast<float>(s.landFrames);
            float sink = (clearance - kLandClearance) / (left > 1.0f ? left : 1.0f);
            sink = sink < kLandSink ? kLandSink : (sink > kLandSinkMax ? kLandSinkMax : sink);
            k->current.pos.y -= sink;  // s.landFrames zaehlt kernTimersTick je Kern-Tick (WP9, R4)
            logTelemetry(g_logLanding, 3, kLogEvery, "landing approach frame %u, clearance %.1f, sink %.1f", s.landFrames,
                static_cast<double>(clearance), static_cast<double>(sink));
        }

        // B-Tipp aus dem Schnappschuss: Absteigen (Aktion muss Fliegen oder Angriff sein, kein Ereignis).
        if (s.dismountRequested) {
            s.dismountRequested = false;
            if (k->mAction == daKago_c::ACTION_FLY_e || k->mAction == daKago_c::ACTION_ATTACK_e) {
                ++g_counts.dismountFromB;
                dismount(k, EndReason::DismountButton, "B-Tipp");
                return HOOK_CONTINUE;
            }
            ++g_counts.dismountIgnored;
            logEvery(g_logDismountIgnored, 3, 60, "B-Tipp ignored, bird action %d", k->mAction);
            g_dismountWish = kDismountWishTicks;  // P2-10: B gilt bis zur naechsten Flug- oder Angriffsaktion
        } else if (g_dismountWish > 0 && !uiHalt) {
            if (k->mAction == daKago_c::ACTION_FLY_e || k->mAction == daKago_c::ACTION_ATTACK_e) {
                g_dismountWish = 0;
                ++g_counts.dismountFromB;
                dismount(k, EndReason::DismountButton, "B-Wunsch aus Zwischenaktion");
                return HOOK_CONTINUE;
            }
            if (--g_dismountWish == 0) {
                notifyDismountOnlyInFlight(mod_ctx);
            }
        }

        if (k->mAction != s.lastAction) {
            s.lastAction = k->mAction;
            if (k->mAction == daKago_c::ACTION_ATTACK_e) {
                logInfo("dash (A) at ride frame %u", s.rideFrames);
            }
        }
        keepRideState(k);
        trackYaw(k);
        // WP9 (R9, R4): Ritt-Zeile alle kLogEvery Kern-Ticks (Pad-Lesungen), die erste sofort nach dem Ritt-Start.
        if (!g_rideLogged || timers::elapsed(g_rideLogTick) >= kLogEvery) {
            g_rideLogged = true;
            g_rideLogTick = timers::now();
            logRide(k);
        }
        ++s.rideFrames;
        notifyFrameBird(k);
        return HOOK_CONTINUE;
    });
}

// daE_YM_c::_delete, Pre: haelt der eigene Vogel einen Lock auf diesen E_YM, wird er vorher geloest
// (flySpeedCalcLockOn liest den Zeiger ungeprueft).
HookAction ymDeletePre(ModContext*, void* args, void*, void*) {
    return guardedPre("daE_YM_c::_delete", [&]() -> HookAction {
        daE_YM_c* ym = mods::arg<daE_YM_c*>(args, 0);
        daKago_c* bird = liveBird();
        if (ym != nullptr && bird != nullptr && bird->getLockActor() == static_cast<fopAc_ac_c*>(ym)) {
            bird->setLockActor(nullptr);
            logInfo("E_YM deleted while the bird was locked on it, lock cleared");
        }
        return HOOK_CONTINUE;
    });
}

// Einordnung eines Szenenwechsels (D1). Menue und Ereignis beenden den Ritt vor dem Wechsel (endRideClean); alles andere
// ist positionsgetrieben und bleibt im Ritt. Dort weist es das Modul freiheit ab (SKIP_ORIGINAL). Ein Ritt ohne Vogel
// oder ohne Ritt-Phase wird nur eingeordnet.
void classifySceneChange(const char* where) {
    Session& s = session();
    const bool menu = dComIfGp_isPauseFlag() != 0;
    // Ereignis wie in linkExecutePre: Spielereignis oder Link im Ereignis.
    daAlink_c* link = liveLink();
    const bool event = dComIfGp_event_runCheck() != 0 || (link != nullptr && link->checkEventRun());
    if (menu || event) {
        g_sceneChange = menu ? ride::SceneChange::Menu : ride::SceneChange::Event;
    } else {
        g_sceneChange = ride::SceneChange::Position;
    }
    if (s.birdId == kNoProc || (s.phase != Phase::Riding && s.phase != Phase::Summoning)) {
        return;
    }
    if (menu || event) {
        endRideClean(menu ? EndReason::Menu : EndReason::Event, where);
    } else if (s.phase == Phase::Riding) {
        ++g_counts.sceneKept;
    }
}

// dComIfGp_setNextStage, Pre: Szenenwechsel durch Menue (Titel, Laden, Beenden) oder Ereignis beenden den Ritt sauber.
HookAction stageNextStagePre(ModContext*, void*, void*, void*) {
    return guardedPre("dComIfGp_setNextStage", [&]() -> HookAction {
        classifySceneChange("Szenenwechsel (setNextStage) durch Menue oder Ereignis");
        return HOOK_CONTINUE;
    });
}

// dStage_changeScene, Pre: derselbe Schnitt fuer den Szenenwechsel selbst (Ereignis-Gate, Zwischensequenzen).
HookAction stageChangeScenePre(ModContext*, void*, void*, void*) {
    return guardedPre("dStage_changeScene", [&]() -> HookAction {
        classifySceneChange("Szenenwechsel (dStage_changeScene) durch Menue oder Ereignis");
        return HOOK_CONTINUE;
    });
}

// Warp (daAlink_c::procCoWarpInit, Pre, kein SKIP): Midna-, Ooccoo- und Portal-Warps beenden den Ritt beim Start,
// nicht erst beim Szenenwechsel. Der Warp selbst laeuft danach unveraendert weiter (D1).
HookAction linkProcCoWarpInitPre(ModContext*, void*, void*, void*) {
    return guardedPre("daAlink_c::procCoWarpInit", [&]() -> HookAction {
        if (hasOwnBird() && session().phase != Phase::Dismounting) {
            g_sceneChange = ride::SceneChange::Warp;
            ++g_counts.warpEnded;
            endRideClean(EndReason::Warp, "Warp (procCoWarpInit)");
        }
        return HOOK_CONTINUE;
    });
}

// griff K6: vor dem ersten fakepath::refresh kann mpPath1 des eigenen Vogels null sein (Spawn-Fenster). Flug und
// Stagger lesen den Pfad ungeprueft. Nur der eigene Vogel bekommt hier den Ersatzpfad.
void guardPathBeforeCall(daKago_c* k) {
    if (k != nullptr && isOwnBird(k) && k->mpPath1 == nullptr) {
        fakepath::refresh(k);
        ++g_counts.pathGuard;
        logEvery(g_logPathGuard, 3, 300, "mpPath1 was null before flight or stagger, fake path set (count %u)",
            g_counts.pathGuard);
    }
}

HookAction kagoExecuteFlyPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daKago_c::executeFly", [&]() -> HookAction {
        guardPathBeforeCall(mods::arg<daKago_c*>(args, 0));
        return HOOK_CONTINUE;
    });
}

HookAction kagoExecuteStaggerPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daKago_c::executeStagger", [&]() -> HookAction {
        guardPathBeforeCall(mods::arg<daKago_c*>(args, 0));
        return HOOK_CONTINUE;
    });
}

#if defined(KFF_DEV_AUTOTEST)
// Schadensdiagnose (lebensverlust-zyklus3.md, Fix C): jede Schadensquelle des eigenen Links mit Proc und Betrag ins Log.
// Nur im Testbuild; greift nicht ins Spiel ein.
uint32_t g_logDamage = 0;

HookAction linkSetDamagePointPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daAlink_c::setDamagePoint", [&]() -> HookAction {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        if (link != nullptr && link == liveLink()) {
            logEvery(g_logDamage, 400, 10, "DMG setDamagePoint amount=%d zora=%d timer=%d proc=%u",
                mods::arg<int>(args, 1), mods::arg<int>(args, 2), mods::arg<int>(args, 3),
                static_cast<unsigned>(link->mProcID));
        }
        return HOOK_CONTINUE;
    });
}

#endif

// Befund 2 (F2): posMove addiert den Wurzelversatz der Animation in x/z (sp78 = field_0x34d4 - sp108, d_a_alink.cpp
// posMove). Beim Wechsel WAIT -> FALL nach dem Loslassen sind das 53 Einheiten in einem Tick (Abnahme abn68r1-w6-ruf-1,
// E9 LINK 2 dh=53.1123). Im Fenster g_releaseFall, solange Link in der Luft ist, wird genau dieser Versatz nach posMove
// zurueckgenommen. Sturzhoehe und Geschwindigkeit bleiben unberuehrt (releaseLandedHorizontal, finishDismount).
// A3 B1: In der Mindestdauer kReleaseFallMinTicks geht die Waagerechte auf den Stand vor Proc und posMove zurueck, auch bei
// Boden- oder Wasserkontakt. Der reine Wurzelversatz reichte nicht: im Lauf f7-t1-abflugwolf-1 (Ritt 2) blieben nach der
// Ruecknahme 49.6 Einheiten aus posMove stehen, E9 LINK 2 ueber der Grenze.
void linkPosMovePost(ModContext*, void* args, void*, void*) {
    if (!g_releaseFall) {
        return;
    }
    try {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        if (link == nullptr) {
            return;
        }
        if (g_releaseFallTicks < kReleaseFallMinTicks && g_releaseFallSnapOk) {
            g_releaseFallSnapOk = false;
            const float dx = link->current.pos.x - g_releaseFallX;
            const float dz = link->current.pos.z - g_releaseFallZ;
            link->current.pos.x = g_releaseFallX;
            link->current.pos.z = g_releaseFallZ;
            if (dx != 0.0f || dz != 0.0f) {
                ++g_counts.rootCancel;
            }
            if (std::fabs(dx) + std::fabs(dz) > 1.0f) {
                logInfo("fall root cancel (release window): dx=%.2f dz=%.2f removed after posMove, proc=%u", static_cast<double>(dx),
                    static_cast<double>(dz), static_cast<unsigned>(link->mProcID));
            }
            return;
        }
        // Nach der Mindestdauer schliessen Boden oder Wasser das Fenster wie bisher (F6).
        if (g_releaseFallTicks >= kReleaseFallMinTicks &&
            (link->mLinkAcch.ChkGroundHit() || link->mLinkAcch.ChkWaterHit())) {
            g_releaseFall = false;  // Boden oder Wasser erreicht (F6): das Fenster ist zu
            return;
        }
        const float dx = link->transAnimeMoveX();
        const float dz = link->transAnimeMoveZ();
        if (dx == 0.0f && dz == 0.0f) {
            return;
        }
        link->current.pos.x -= dx;
        link->current.pos.z -= dz;
        ++g_counts.rootCancel;
        if (std::fabs(dx) + std::fabs(dz) > 1.0f) {
            logInfo("fall root cancel (release window): dx=%.2f dz=%.2f removed after posMove, proc=%u", static_cast<double>(dx),
                static_cast<double>(dz), static_cast<unsigned>(link->mProcID));
        }
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in posMove post (ignored)");
    }
}

#if defined(KFF_DEV_AUTOTEST)

void linkCheckPolyDamagePost(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        const int result = retval != nullptr ? *static_cast<int*>(retval) : 0;
        if (result != 0 && link != nullptr && link == liveLink()) {
            logEvery(g_logDamage, 400, 10, "DMG checkPolyDamage result=%d proc=%u", result,
                static_cast<unsigned>(link->mProcID));
        }
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in checkPolyDamage diagnosis (ignored)");
    }
}

HookAction linkProcWolfDamageInitPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daAlink_c::procWolfDamageInit", [&]() -> HookAction {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        if (link != nullptr && link == liveLink()) {
            // Ohne Trefferobjekt (NULL) friert der Wolf am Boden oder Eis ein, sonst kommt der Schaden von einem Gegner.
            // Test-Diagnose: Besitzer des Trefferobjekts (Profilname je AT/CO/TG) fuer die Trennung Gegner/Umgebung.
            dCcD_GObjInf* obj = static_cast<dCcD_GObjInf*>(mods::arg<void*>(args, 1));
            const fopAc_ac_c* at = obj != nullptr ? obj->GetAtHitAc() : nullptr;
            const fopAc_ac_c* co = obj != nullptr ? obj->GetCoHitAc() : nullptr;
            const fopAc_ac_c* tg = obj != nullptr ? obj->GetTgHitAc() : nullptr;
            // Abstand Link zum Trefferobjekt (Gegner oder Umgebung), damit Gegnertreffer am Landeort belegt sind.
            auto dist3 = [link](const fopAc_ac_c* a) -> double {
                const double dx = static_cast<double>(a->current.pos.x) - static_cast<double>(link->current.pos.x);
                const double dy = static_cast<double>(a->current.pos.y) - static_cast<double>(link->current.pos.y);
                const double dz = static_cast<double>(a->current.pos.z) - static_cast<double>(link->current.pos.z);
                return std::sqrt(dx * dx + dy * dy + dz * dz);
            };
            logEvery(g_logDamage, 400, 10,
                "DMG procWolfDamageInit source=%s proc=%u hitObj=%p prof(at)=%d prof(co)=%d prof(tg)=%d dist(co)=%.0f dist(tg)=%.0f",
                obj == nullptr ? "Boden/Eis (kein Trefferobjekt)" : "Trefferobjekt", static_cast<unsigned>(link->mProcID),
                static_cast<const void*>(obj), at != nullptr ? static_cast<int>(fopAcM_GetProfName(at)) : -1,
                co != nullptr ? static_cast<int>(fopAcM_GetProfName(co)) : -1,
                tg != nullptr ? static_cast<int>(fopAcM_GetProfName(tg)) : -1,
                co != nullptr ? dist3(co) : -1.0, tg != nullptr ? dist3(tg) : -1.0);
        }
        return HOOK_CONTINUE;
    });
}

void linkCheckWolfLandActionPost(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        const int result = retval != nullptr ? *static_cast<int*>(retval) : 0;
        if (link != nullptr && link == liveLink()) {
            logEvery(g_logDamage, 400, 10, "DMG checkWolfLandAction arg=%d result=%d proc=%u", mods::arg<int>(args, 1),
                result, static_cast<unsigned>(link->mProcID));
        }
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in checkWolfLandAction diagnosis (ignored)");
    }
}
#endif

// Tod (checkDeadAction, Pre ohne SKIP): der Ritt endet vor dem Tod sauber, das Spiel verarbeitet den Tod danach
// normal. Politik: Tod zulassen, Ritt vorher beenden, nie ein Veto (tod-hp.md, Hook-Ziele).
HookAction linkCheckDeadActionPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daAlink_c::checkDeadAction", [&]() -> HookAction {
        daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
        if (link == nullptr || !hasOwnBird() || !link->checkDeadHP()) {  // D-A: auch im Abflug
            return HOOK_CONTINUE;
        }
        endRideClean(EndReason::Death, "Tod (checkDeadAction)");
        return HOOK_CONTINUE;
    });
}

// Zweites Netz fuer den Tod (procCoDeadInit, Pre ohne SKIP): Zwangs-Game-Over ohne HP-Verlust (checkRestartDead)
// und direkte Aufrufer. Ohne Sitzung kein Eingriff.
HookAction linkProcCoDeadInitPre(ModContext*, void*, void*, void*) {
    return guardedPre("daAlink_c::procCoDeadInit", [&]() -> HookAction {
        if (hasOwnBird()) {  // D-A: auch im Abflug
            endRideClean(EndReason::Death, "Game-Over (procCoDeadInit)");
        }
        return HOOK_CONTINUE;
    });
}

// Titel (dComIfG_changeOpeningScene, Pre ohne SKIP): der Reset-Pfad setzt den Titel unbedingt an. D5: Titel erlaubt,
// der Ritt endet vorher sauber.
HookAction openingScenePre(ModContext*, void*, void*, void*) {
    return guardedPre("dComIfG_changeOpeningScene", [&]() -> HookAction {
        if (hasOwnBird()) {
            endRideClean(EndReason::Menu, "Titel");
        }
        return HOOK_CONTINUE;
    });
}

// Laden (dComIfGs_gameStart, Pre ohne SKIP): Sicherheitsnetz, der Ritt ist dann meist schon beendet.
HookAction gameStartPre(ModContext*, void*, void*, void*) {
    return guardedPre("dComIfGs_gameStart", [&]() -> HookAction {
        if (hasOwnBird()) {
            endRideClean(EndReason::Menu, "Laden");
        }
        return HOOK_CONTINUE;
    });
}

// Speichern im Ritt (saveMoveDisp / saveMoveDisp2, Pre mit SKIP). E1 (verbindlich): Speichern im Ritt wird verweigert,
// der Ritt laeuft weiter. Ein Speicherversuch beendet den Ritt nie, sonst fiele Link nach dem Menue. Die Sperre gilt an
// jedem Speicher-Eintrag, auch ohne vorhandene Karte (Anlegepfad, Warnfenster Status 0). Ein offenes Warnfenster
// (Status 1) wird vorher geschlossen und das Menue in den Abbruchzweig (wie "Nein") gesetzt.
static HookAction saveLockPre(ModContext* ctx, void* args, const char* path) {
    if (session().phase != Phase::Riding) {
        return HOOK_CONTINUE;
    }
    dMenu_save_c* menu = mods::arg<dMenu_save_c*>(args, 0);
    if (menu != nullptr && menu->mWarning != nullptr && menu->mWarning->getStatus() != 0) {
        menu->mWarning->closeInit();
        menu->yesnoCancelAnmSet();
    }
    // Abbruchzweig wie closeSelect (TYPE_DEFAULT): Status 3, dann PROC_SAVE_WAIT. Das Fenster kehrt zum Sammelbildschirm
    // zurueck (d_menu_window.cpp collect_save_move_proc), ohne Schreiben. Ohne diese Zustandswahl bliebe das Menue stehen:
    // SKIP laesst saveMoveDisp nie laufen, der Zustand bleibt auf PROC_SAVE_MOVE_DISP.
    if (menu != nullptr) {
        menu->mSaveStatus = 3;
        menu->mMenuProc = static_cast<u8>(dMenu_save_c::PROC_SAVE_WAIT);
    }
    ++g_counts.saveLocked;
    logEvery(g_logSaveLock, 1, 60, "save locked during ride (path %s), write refused, ride continues", path);
    notifySaveRefused(ctx);
    return HOOK_SKIP_ORIGINAL;
}

HookAction saveMoveDispPre(ModContext* ctx, void* args, void*, void*) {
    return guardedPre("dMenu_save_c::saveMoveDisp", [&]() -> HookAction { return saveLockPre(ctx, args, "a"); });
}

HookAction saveMoveDisp2Pre(ModContext* ctx, void* args, void*, void*) {
    return guardedPre("dMenu_save_c::saveMoveDisp2", [&]() -> HookAction { return saveLockPre(ctx, args, "b"); });
}

// E11: updateAutoSave (Autosave-Automat, dusk/autosave.cpp) setzt dieses Flag, solange er laeuft. Nur dort schreibt der
// Autosave die Karte (writeAutoSave). Das Flag gilt nur zwischen Pre und Post dieses Aufrufs. Das manuelle Speichern im
// Menue laeuft ausserhalb und bleibt im Ritt gesperrt (E1). Autosave-Schreibvorgaenge werden nie per SKIP verweigert.
// Hinweis: Ein Autosave im Ritt ist im Spiel praktisch nicht ausloesbar (Ladezonen sind im Flug gesperrt). Belegt sind der
// Code und das manuelle Speichern (e1-speichern); ein Autosave-Lauf im Ritt ist nicht getestet.
bool g_inAutoSaveUpdate = false;

HookAction autoSavePre(ModContext*, void*, void*, void*) {
    return guardedPre("updateAutoSave", [&]() -> HookAction {
        g_inAutoSaveUpdate = true;
        return HOOK_CONTINUE;
    });
}

void autoSavePost(ModContext*, void*, void*, void*) {
    g_inAutoSaveUpdate = false;
}

// Kartenschreiben (mDoMemCd_Ctrl_c::save, Pre mit SKIP): Sicherheitsnetz fuer jeden manuellen Schreibpfad. E1: Im Ritt wird
// das Schreiben verweigert, der Ritt laeuft weiter (kein Ritt-Ende). Ausgenommen ist nur der Autosave (E11, siehe oben).
HookAction cardSavePre(ModContext* ctx, void*, void*, void*) {
    return guardedPre("mDoMemCd_Ctrl_c::save", [&]() -> HookAction {
        if (session().phase == Phase::Riding && !g_inAutoSaveUpdate) {
            ++g_counts.saveLocked;
            logEvery(g_logSaveLock, 1, 60, "card write refused during ride (mDoMemCd_Ctrl_c::save), ride continues");
            notifySaveRefused(ctx);
            cardRefusalComplete();  // B2: das Menue bekommt ein Fehlerergebnis statt endlos zu warten
            return HOOK_SKIP_ORIGINAL;
        }
        g_cardWriteSeen = true;  // P2-03: ein Kartenschreiben laeuft (Speichern oder Autosave), cardWriteBusy() prueft es
        g_cardWriteTick = core::padTick();
        return HOOK_CONTINUE;
    });
}

// K4: Soft-Reset (Reset-Callback des Spiels, Pre ohne SKIP): im Ritt zuerst sauber beenden, dann der Titel.
HookAction mainResetPre(ModContext*, void*, void*, void*) {
    return guardedPre("mDoRst_resetCallBack", [&]() -> HookAction {
        if (hasOwnBird()) {
            endRideClean(EndReason::Reset, "Soft-Reset");
        }
        return HOOK_CONTINUE;
    });
}

// HK-02: Ergebnis der Installation des Pre-Hooks auf daAlink_c::execute (hooks.cpp, installCoreHooks).
void setLinkExecuteReady(bool ready) {
    g_linkExecuteReady = ready;
}

void kagoCheckWallHitFallReplace(ModContext*, void* args, void* retval, void*) {
    bool result = false;
    try {
        daKago_c* k = mods::arg<daKago_c*>(args, 0);
        if (isOwnBird(k)) {
            ++g_counts.wallVeto;  // der eigene Vogel stoesst nie ab (fliegt durch Wand-Kontakt)
            result = false;
        } else {
            result = origCheckWallHitFall(k, mods::arg<int>(args, 1));
        }
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in checkWallHitFall (no stagger)");
        result = false;
    }
    if (retval != nullptr) {
        *static_cast<bool*>(retval) = result;
    }
}

void kagoExecuteBalloonMenuReplace(ModContext*, void* args, void* retval, void*) {
    int result = 0;
    try {
        daKago_c* k = mods::arg<daKago_c*>(args, 0);
        if (isOwnBird(k)) {
            // Absteigen laeuft ueber den Schnappschuss (kagoExecutePre). Hier nur: kein Ballonmenue.
            ++g_counts.balloonBlocked;
            result = 0;
        } else {
            result = origExecuteBalloonMenu(k);
        }
    } catch (...) {
        logEvery(g_counts.exceptions, 3, 300, "exception in executeBalloonMenu (no menu)");
        result = 0;
    }
    if (retval != nullptr) {
        *static_cast<int*>(retval) = result;
    }
}

HookAction kagoDrawPre(ModContext*, void* args, void*, void*) {
    return guardedPre("daKago_c::draw", [&]() -> HookAction {
        daKago_c* k = mods::arg<daKago_c*>(args, 0);
        if (isOwnBird(k) && k->field_0x6de == 0) {
            k->field_0x6de = 0xff;
            ++g_counts.drawForced;
        }
        return HOOK_CONTINUE;
    });
}

HookAction kagoDeletePre(ModContext*, void* args, void*, void*) {
    return guardedPre("daKago_c::_delete", [&]() -> HookAction {
        daKago_c* k = mods::arg<daKago_c*>(args, 0);
        if (k != nullptr) {
            // WP4: letztes Netz fuer jeden Kargarok, der freigegeben wird (auch ohne requestDelete, z. B. Szenenwechsel oder
            // direktes fopAcM_delete): kein Skip-Rueckruf auf ihn; beim eigenen Vogel endet sein Ereignis mit ihm.
            const fpc_ProcID id = fopAcM_GetID(k);
            birdDeleteHygiene(id, isOwnBird(k) || id == g_pendingBirdId, "_delete");
        }
        if (k != nullptr && fopAcM_GetID(k) == g_pendingBirdId) {
            g_pendingBirdId = kNoProc;  // das angeforderte Entfernen ist vollzogen (auch nach endRideClean)
            logInfo("pending bird removed (_delete hook)");
            if (!isOwnBird(k) && g_lastEnd.id == fopAcM_GetID(k)) {
                // F: die Sitzung ist schon leer (endRideClean). Die Vertragszeile kommt aus dem Snapshot von requestDelete.
                ++g_counts.deleted;
                logInfo("bird deleted id=0x%x riding=%d departing=%d rideFrames=%u departFrames=%u",
                    static_cast<unsigned>(g_lastEnd.id), g_lastEnd.riding ? 1 : 0, g_lastEnd.departing ? 1 : 0,
                    g_lastEnd.rideFrames, g_lastEnd.departFrames);
                g_lastEnd.id = kNoProc;
            }
        }
        if (!isOwnBird(k)) {
            return HOOK_CONTINUE;
        }
        Session& s = session();
        const bool wasRiding = s.phase == Phase::Riding;
        const bool wasDeparting = s.phase == Phase::Dismounting;
        if (wasRiding) {
            notifyRideEnd(k, EndReason::BirdGone);  // unerwartetes Entfernen im Ritt
        }
        detachBird(k);  // Link und Midna loesen, bevor der Vogel freigegeben wird
        ++g_counts.deleted;
        logInfo("bird deleted id=0x%x riding=%d departing=%d rideFrames=%u departFrames=%u",
            static_cast<unsigned>(s.birdId), wasRiding ? 1 : 0, wasDeparting ? 1 : 0, s.rideFrames, s.departFrames);
        g_lastEnd.id = kNoProc;  // der Snapshot ist verbraucht (eigener Vogel, Sitzung wird jetzt geleert)
        resetSession();
        return HOOK_CONTINUE;
    });
}

// Startpruefung beim Laden des Mods (WP7): Beim Laden gibt es keine Sitzung, also gehoert jeder eigene Vogel in der Welt
// einer frueheren Ladung dieses Mods und ist verwaist. Ein einziger Durchlauf ueber die Akteure (kein Lauf je Frame)
// entfernt ihn ueber removeOrphanBird (Log "orphan bird 0x... removed (no session)"). Mehr als ein eigener Vogel ist ein
// Befund und wird als Warnung gemeldet.
void startupBirdCheck() {
    const uint32_t before = g_counts.orphanRemoved;
    fpc_ProcID none = kNoProc;
    fopAcIt_Executor(removeOrphanBird, &none);
    const uint32_t removed = g_counts.orphanRemoved - before;
    if (removed > 1) {
        logWarn("startup check: %u own birds (fpcNm_KAGO_e) found, only one is allowed, all removed",
            static_cast<unsigned>(removed));
    } else {
        logInfo("startup check: %u own bird(s) removed", static_cast<unsigned>(removed));
    }
}

// Entladen (mod_shutdown, vor dem Abmelden der Hooks): Ritt-Ende wie jedes andere (endRideClean, Grund Shutdown), danach
// der eigene Vogel per fopAcM_delete, falls er noch lebt. Die Hooks laufen dabei noch, denn der Loader ruft mod_shutdown
// vor hook_remove_mod auf. Lebt der Vogel nach dem Entfernen noch, wird das gemeldet: das Spiel entfernt ihn dann spaeter.
// mpPath1 bleibt gesetzt (Heap, nie freigegeben), denn ein Frame mit mpPath1 == nullptr stuerzt Flug und Stagger ab
// (Spaetfund absturz-mppath1).
void shutdownBird() {
    cardInjectRestore("Entladen");  // B2: kein FATAL_ERROR-Kartenstatus nach dem Entladen zuruecklassen
    const fpc_ProcID birdId = session().birdId;
    if (birdId == kNoProc) {
        resetSession();
        return;
    }
    endRideClean(EndReason::Shutdown, "Entladen");
    fopAc_ac_c* actor = nullptr;
    if (fopAcM_SearchByID(birdId, &actor) != 0) {
        fopAcM_delete(birdId);  // requestDelete hat das meist schon getan; ein zweiter Aufruf wird vom Spiel toleriert
    }
    actor = nullptr;
    // fopAcM_delete wirkt im Frame-Ablauf. Wird der Ritt aus dem Vogel-Execute heraus beendet (Testbuild KFF_TEST_UNLOAD),
    // lebt der Vogel hier noch, bis dieser Lauf endet. Danach laeuft kein Callback mehr: das Entfernen ist ab hier nicht
    // mehr messbar (Grenze, siehe known_gaps von WP7).
    if (fopAcM_SearchByID(birdId, &actor) != 0) {
        logInfo("shutdown, bird set to WAIT and removed (fopAcM_delete angefordert 0x%x, Entfernen nach dem laufenden Frame)",
            static_cast<unsigned>(birdId));
    } else {
        logInfo("shutdown, bird set to WAIT and removed (sofort entfernt, 0x%x)", static_cast<unsigned>(birdId));
    }
    // Keine Ticks mehr: Fallhoehe und Entfernen-Wiederholen werden nicht weiter beobachtet.
    g_fallHold = false;
    g_fallWatch = 0;
    g_pendingBirdId = kNoProc;
    g_pendingWait = 0;
    resetSession();
}

void logSummary() {
    const Session& s = session();
    const summon::Stats& sm = summon::stats();
    const InputCounts& in = inputCounts();
    logInfo("SUMMARY linkCalls=%u birdCalls=%u summon=%u dismount=%u dismountFromB=%u dismountIgnored=%u "
            "deleted=%u deleteRequested=%u deleteRetry=%u endClean=%u riderLost=%u forcedFly=%u linkChanged=%u "
            "orphanRemoved=%u",
        s.linkCalls, s.birdCalls, sm.summon, g_counts.dismount, g_counts.dismountFromB,
        g_counts.dismountIgnored, g_counts.deleted, g_counts.deleteRequested, g_counts.deleteRetry,
        g_counts.endClean, g_counts.riderLost, g_counts.forcedFly, g_counts.linkChanged, g_counts.orphanRemoved);
    logInfo("COUNTS humanRefused=%u busyRefused=%u eventRefused=%u createFailed=%u chord=%u callRejected=%u "
            "wallVeto=%u balloonBlocked=%u drawForced=%u exceptions=%u yawMinDeg=%.1f yawMaxDeg=%.1f birdMinY=%.1f "
            "birdMaxY=%.1f",
        g_counts.humanRefused, g_counts.busyRefused, g_counts.eventRefused, sm.createFailed,
        in.chordConsumed, in.callRejected, g_counts.wallVeto, g_counts.balloonBlocked,
        g_counts.drawForced, g_counts.exceptions, static_cast<double>(s.yawMinDeg),
        static_cast<double>(s.yawMaxDeg), static_cast<double>(s.birdMinY), static_cast<double>(s.birdMaxY));
    logInfo("CORE2 keepCleared=%u ymCleared=%u summonTimeout=%u deadRider=%u transformAborted=%u "
            "consumedHeldTicks=%u consumedEdges=%u",
        g_counts.keepCleared, g_counts.ymCleared, sm.summonTimeout, g_counts.deadRider,
        g_counts.transformAborted, in.consumedHeld, in.consumedEdges);
    logInfo("CORE3 saveLocked=%u chordRestored=%u", g_counts.saveLocked, in.chordRestored);
    logInfo("CORE5 goneRelease=%u skipCleared=%u eventReset=%u summonEventEnd=%u", g_counts.goneRelease,
        g_counts.skipCleared, g_counts.eventReset, g_counts.summonEventEnd);
    logInfo("CORE6 spawnRejected=%u roomRejected=%u strayFound=%u rideStartWait=%u summonAborted=%u transformTimeout=%u",
        sm.spawnRejected, sm.roomRejected, sm.strayFound, sm.rideStartWait, sm.summonAborted, sm.transformTimeout);
    logInfo("CORE4 landings=%u landingDone=%u rideEndDuplicate=%u pathGuard=%u warpEnded=%u sceneKept=%u",
        g_counts.landings, g_counts.landingDone, g_counts.rideEndDuplicate, g_counts.pathGuard, g_counts.warpEnded,
        g_counts.sceneKept);
    logInputSummary();  // WP3 (input.cpp): restliche Ablehnungszeilen, Gruende und Abgleich (Zeile INPUT)
}

}  // namespace core
}  // namespace kff
