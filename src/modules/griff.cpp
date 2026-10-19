// Modul "griff": Link bleibt im Ritt am eigenen Vogel, bis er mit B absteigt oder der Ritt geordnet endet.
// Stand: Owner-Stand aus griff/src/modules/griff.cpp (Teil 2, Nachtrag 2 und 3), in probe2-module uebernommen.
//
// Lage im Spielcode (dusklight src/d/actor/d_a_kago.cpp, d_a_alink*.inc):
//  - setRideOff() ruft der Vogel nur in executeStagger (Frame 22), executeLanding, dem Landen in executeWait
//    (Lake Hylia, Boardhouse) und in Demo-Modi auf. Ausserhalb von Ereignissen setzt der Kern im Ritt jeden Frame
//    vor action() die Aktion auf FLY (ride.cpp, keepRideState), ausser bei ATTACK (Dash). Damit erreicht im Ritt
//    keiner dieser Pfade den Dispatch, solange kein Ereignis laeuft.
//  - Flugabwurf: FLY mit Modus 3 oder 4 (Pfadflug, setFlyAway). griff setzt im Ritt auf Modus 1 zurueck.
//  - Link-Seite: offCargoCarry() setzt nur den Modus 0. Der Keep (mCargoCarryAcKeep) bleibt am Vogel. griff stellt
//    den Griff in onFrameLink wieder her (setPlayerRideOn, bei Bedarf procWolfCargoCarryInit).
//  - Ereignis ohne Griff und Tod beenden den Ritt mit endRideClean (ride.cpp:568), so wie der Kern selbst in
//    ride.cpp:713 (Link tot) und ride.cpp:844 (Szenenwechsel durch Ereignis) verfaehrt. Link ohne Wolf beendet den
//    Ritt mit requestDelete. Waehrend eines Ereignisses wird nichts wiederangebunden.
//  - Keine Wiederanbindung, sobald der Vogel zum Entfernen vorgemerkt ist (deleteRequested). Sonst haengt Link an
//    einem Akteur, der gleich verschwindet.
//
// Hooks und Besitz: alle Hooks mit Prioritaet prio::kGriff (350), nur Pre-SKIP, kein Replace. Jeder Callback faengt
//   seine Ausnahmen selbst; Gueltigkeit nur ueber Prozess-IDs und die Akteurliste, keine gespeicherten Zeiger.
// Griff-Teil: laeuft ueber die Modul-Callbacks, die der Kern aus seinen Pre-Hooks aufruft (notifyFrameBird,
//   notifyFrameLink im Link-Pre-Hook, onRideEnd, onShutdown). Fremdeingaben zaehlt der Kern (autotest::foreignInputCount,
//   nur Testbuild).
// Ereignis-Teil (WP4, Abschnitt "Ereignisse" unten): griff ist einziger SKIP-Besitzer auf demo_skip, executePerch,
//   initPerchDemo, dComIfGs_on/offEventBit, fopAcM_orderPotentialEvent, fopAcM_orderTalkEvent und daTagMwait_c::execute.
//   Beobachtung ohne SKIP: daKago_c::execute (Pre), mDoCPd_c::read (Post), daAlink_c::execute (Post),
//   fopAcM_orderSpeakEvent (Pre), Tag_Mwait/Tag_Hstop (Post), procCoMetamorphoseInit (Pre, beendet den Ritt mit Grund
//   Transform, das Original laeuft).
// Zustands-Whitelist im Ritt (WP5, Abschnitt "WP5" unten):
//   - daKago_c::executeStagger, executeLanding, executeLandingLakeHairia, executeLandingBoartHouse: im Ritt SKIP, der
//     Vogel bleibt FLY (Modus 1). Landen nur ueber den Landeanflug des Kerns (E9). Zaehler und Log.
//   - daAlink_c::checkSwimAction: im Ritt SKIP (Rueckgabe 0), kein Schwimm-Proc, kein Proc-Pingpong mit dem Griff.
//   - daAlink_c::checkDamageAction (CONTINUE): Wurf-/Gross-, Sandwand-, Quetsch- und Rueckstoss-Schaden ohne Proc-Wechsel,
//     der Lebensverlust bleibt. daAlink_c::procWolfDamageInit: im Wolfs-Ritt SKIP (das Leben ist vorher abgezogen).
//   - daAlink_c::procCoLavaReturnInit und daTagKagoFall_c::execute (Fluss- und Fall-Tag F_SP112): im Ritt SKIP, ausser
//     Link ist tot oder ein Ereignis laeuft.
//   - Raum-Neustart im Ritt (daAlink_c::startRestartRoom, daTagRetRm_c::execute): einziger Besitzer ist das Modul
//     freiheit (Welle 2, ein SKIP-Besitzer je Ziel). griff registriert dort keinen Hook.
//   - Ruf-Gate kff::griffCallGateReason (Aufruf aus ride.cpp callBlockReason): Reiten (Epona, Wildschwein, Boot, Board,
//     Spinner, Ziege, Gorone), Verwandlung, Wasser, Seil, Kette, Haengen, Klettern/Ranken/Leiter, Enterhaken, Schaden, Luft.
//   - Vogel laesst los (STAGGER/LANDING/WAIT ohne Griff, nur ohne Schutz denkbar): Ritt endet sofort geordnet (dismount).
// Replace-Besitz: Kern. checkWallHitFall und executeBalloonMenu besitzt der Kern (eigener Vogel); griff setzt dort weder
//   Pre-SKIP noch Replace.
//
// Wiederanbindungsfenster (Kern unveraendert). Zeit in Kern-Ticks (R4, core/timers.hpp):
//  - griff stellt den Griff in jedem Link-Frame ohne Griff wieder her (restoreGrip). Die Folge ohne Griff zaehlt
//    g_lostRun, ihr Beginn ist der Kern-Tick g_lostStart. Bis einschliesslich 60 Kern-Ticks in Folge (kWindowFrames)
//    wird wiederangebunden. Danach beendet griff den Ritt geordnet (dismount, Steigflug).
//  - Der Kern zaehlt riderLost je Pad-Lesung (kernTimersTick in ride.cpp, Schwelle kRiderLostFrames = 90 Kern-Ticks).
//    Griff beendet den Ritt nach 60 Kern-Ticks selbst; der Kern greift erst nach 90 Kern-Ticks ohne Cargo-Carry
//    (Sicherheitsnetz). Fuer das volle Fenster muss kRiderLostFrames mindestens 62 sein (Kern-Patch K4, Owner-Vorschlag
//    60 reicht nicht).
//
// Testbuild (KFF_DEV_AUTOTEST=ON, Lauf mit KFF_AUTOTEST=griff): Kurs ueber KFF_GRIFF_KURS (wand, dauer, boden, decke,
//   schlag, kreis, dash). KFF_GRIFF_AUS=1: Vergleichslauf, das Modul tut nichts ausser Log. Im Release ist der gesamte
//   Testteil nicht kompiliert, autotestScenario() liefert dort nullptr.
//   WP4: KFF_AUTOTEST=wp4 mit KFF_WP4=<teiltest> (event, skip, linkdemo, demoskip, perch, midna, flags, verwandlung,
//   startdialog, summonevent, tags, howl). KFF_WP4_AUS=1: Vergleichslauf, die WP4-Absicherungen (auch die im Kern) sind
//   aus, die Sensoren zaehlen weiter.
// Testbuild WP5 (KFF_AUTOTEST=wp5, Fall ueber KFF_WP5): KFF_WP5_OHNE=1 schaltet den WP5-Schutz ab (Vergleichslauf),
//   KFF_TEST_NOGUARD=1 die Griff-Sperren (Stagger/Landung), KFF_GRIFF_TESTHIT=N setzt ab Ritt-Frame N Test-ATs an Link,
//   KFF_TEST_STAGGER=1 erzwingt im Ritt STAGGER am Vogel.
// Kern-Vorschlaege, die dieses Modul nicht umsetzt, stehen im Bericht (K1 bis K6).

#include "d/dolzel_rel.h"

#include "core/kff/log.hpp"
#include "core/kff/module.hpp"
#include "core/kff/session.hpp"
#include "core/timers.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_midna.h"
#include "d/actor/d_a_player.h"
#include "d/d_camera.h"
#include "d/actor/d_a_tag_kago_fall.h"
#include "SSystem/SComponent/c_m3d.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#if defined(KFF_DEV_AUTOTEST)
#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_tag_mwait.h"
#include "d/actor/d_a_e_ym.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_cc_d.h"
#include "d/d_kankyo.h"
#include "d/d_msg_object.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#include <cstdint>

namespace kff {
namespace {

constexpr uint32_t kWindowFrames = 60;  // Kern-Ticks in Folge ohne Griff (WP9, R4), danach geordnetes Absteigen

// Zaehler. Stehen im Log (Ritt-Ende, SUMMARY) und dienen der Auswertung.
struct Stats {
    uint32_t rides = 0;         // Ritt-Starts
    uint32_t birdFrames = 0;    // Vogel-Frames im Ritt (onFrameBird)
    uint32_t notFly = 0;        // Vogel nach dem Kern-Zwang nicht FLY/ATTACK, ohne Ereignis (sollte 0 sein)
    uint32_t flyAway = 0;       // FLY mit Modus 3/4 im Ritt, zurueck auf Modus 1
    uint32_t gripFrames = 0;    // Link-Frames mit Griff zu Beginn des Frames
    uint32_t procOff = 0;       // Griff, aber Link nicht in Proc 312
    uint32_t lostFrames = 0;    // Link-Frames ohne Griff zu Beginn des Frames
    uint32_t reattach = 0;      // Wiederanbindungen
    uint32_t reattachFail = 0;  // Wiederanbindung ohne Wirkung (Griff danach trotzdem nicht da)
    uint32_t endWindow = 0;     // geordnetes Absteigen nach kWindowFrames Frames ohne Griff
    uint32_t endDeath = 0;      // Tod im Ritt
    uint32_t endEvent = 0;      // Ereignis ohne Griff
    uint32_t endHuman = 0;      // Link kein Wolf
};
Stats g_stats;

uint32_t g_lostRun = 0;  // Link-Frames in Folge ohne Griff (Zaehler fuer Log und Statistik)
uint32_t g_lostStart = 0;  // WP9 (R4): Kern-Tick des ersten Frames in Folge ohne Griff; das Fenster misst Kern-Ticks

uint32_t g_logNotFly = 0;
uint32_t g_logFly = 0;
uint32_t g_logLost = 0;
uint32_t g_logRe = 0;
uint32_t g_logOff = 0;

#if defined(KFF_DEV_AUTOTEST)
bool g_control = false;  // KFF_GRIFF_AUS gesetzt: Vergleichslauf ohne Wirkung

bool envSet(const char* key) {
    const char* v = std::getenv(key);
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}
#endif

// Vergleichslauf (nur Testbuild). Im Release immer false.
bool controlRun() {
#if defined(KFF_DEV_AUTOTEST)
    return g_control;
#else
    return false;
#endif
}

void logStats(const char* tag) {
    logInfo("griff: %s rides=%u birdFrames=%u gripFrames=%u procOff=%u lostFrames=%u reattach=%u reattachFail=%u "
            "flyAway=%u notFly=%u endWindow=%u endDeath=%u endEvent=%u endHuman=%u",
        tag, g_stats.rides, g_stats.birdFrames, g_stats.gripFrames, g_stats.procOff, g_stats.lostFrames,
        g_stats.reattach, g_stats.reattachFail, g_stats.flyAway, g_stats.notFly, g_stats.endWindow,
        g_stats.endDeath, g_stats.endEvent, g_stats.endHuman);
#if defined(KFF_DEV_AUTOTEST)
    logInfo("griff: %s fremdeingabe=%u (Zaehler des Kerns)", tag, static_cast<unsigned>(autotest::foreignInputCount()));
#endif
}

// Gilt der Griff: Link sitzt im Modus Cargo, und sein Keep zeigt auf unseren Vogel.
bool gripHeld(daAlink_c* link, daKago_c* bird) {
    return link != nullptr && bird != nullptr && link->checkCargoCarry() &&
           link->mCargoCarryAcKeep.getActor() == static_cast<fopAc_ac_c*>(bird);
}

// Griff herstellen: Modus Cargo und Keep auf den Vogel. Ohne Proc 312 zuerst die Init (mit Modus Cargo laeuft
// der Zweig "Vogel traegt"), danach den Keep erneut setzen, weil die Init ihn loeschen kann. Midna folgt, wenn
// sie den Vogel nicht als mpKago haelt. Nur fuer Wolf-Link aufrufen.
void restoreGrip(daAlink_c* link, daKago_c* bird) {
    bird->setPlayerRideOn();
    if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
        link->procWolfCargoCarryInit();
        bird->setPlayerRideOn();
    }
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna != nullptr && midna->mpKago != bird) {
        bird->setMidnaRideOn();
    }
}

// Flugabwurf im Ritt (FLY mit Modus 3 oder 4): zurueck auf Modus 1. Ereignisse steuern den Vogel selbst.
void keepFlying(daKago_c* bird) {
    if (dComIfGp_event_runCheck()) {
        return;
    }
    if (bird->mAction == daKago_c::ACTION_FLY_e && (bird->mMode == 3 || bird->mMode == 4)) {
        ++g_stats.flyAway;
        logEvery(g_logFly, 3, 60, "griff: Flugabwurf (Modus %d) im Ritt zurueck auf Modus 1, n=%u", bird->mMode,
            g_stats.flyAway);
        bird->setActionMode(daKago_c::ACTION_FLY_e, 1);
    }
}

// ---------------------------------------------------------------------------------------------
// Ereignisse (WP4): Demo-Skip, Perch, Skript-Aktionen, Midna-Auftraege, Story-Flags, Tags, Verwandlung.
//
// Politik (D6 verbindlich, geht dem Wortlaut "Ritt bleibt" vor): Ein Ereignis im Ritt beendet ihn sofort geordnet mit
// Grund (Kern linkExecutePre: endRideClean(Event)). Der Vogel geht dabei in WAIT (requestDelete), Link wird waehrend eines
// Ereignisses nicht wieder angebunden (Kern und griff pruefen das Ereignis vor jeder Wiederanbindung). Dieser Abschnitt
// verhindert, dass Vanilla-Skripte den eigenen Vogel oder Link still uebernehmen, und lehnt Midna-Auftraege im Ritt ab.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kModBirdParam = 0x000100FFu;  // Parameter jedes Mod-Vogels (= kFriendlyBirdParam in ride.cpp)
constexpr uint32_t kMidnaGuardTicks = 3;          // Pad-Ticks, in denen eine Midna-Demo nach Ablehnung zurueckgenommen wird

struct EvStats {
    uint32_t demoSkipCalls = 0;      // daKago_c::demo_skip, alle Aufrufe
    uint32_t skipNoBird = 0;         // demo_skip ohne lebenden Kargarok (Skipper-Rueckfall auf Link, d_event.cpp:924-928)
    uint32_t skipBlocked = 0;        // demo_skip am eigenen oder Mod-Vogel unterdrueckt
    uint32_t rideOffPrevented = 0;   // davon Fall 7/9: setRideOff und Spieler-Teleport unterdrueckt
    uint32_t perchIgnored = 0;       // Perch ohne Pfad oder am eigenen/Mod-Vogel abgefangen
    uint32_t scriptNeutral = 0;      // Skript-Aktion EVENT/EVENT2/PERCH/PERCH2 am eigenen/Mod-Vogel neutralisiert
    uint32_t birdEventEnded = 0;     // Ereignis des Vogels (mPt1/mPt2) beim Abfangen beendet
    uint32_t setEventInRide = 0;     // Sensor: eigener Vogel im Ritt mit Skript-Aktion gesehen (setEvent/setEvent2)
    uint32_t storyBlocked = 0;       // M_067/M_077 im Ritt nicht geaendert
    uint32_t storyControl = 0;       // Vergleichslauf: Story-Flag im Ritt geaendert
    uint32_t midnaOrderSeen = 0;     // Midna-Auftraege im Ritt
    uint32_t midnaOrderRefused = 0;  // davon abgelehnt (Rueckgabe 0)
    uint32_t midnaDemoReset = 0;     // Demo-Typ/-Modus von Link oder Midna nach Ablehnung zurueckgenommen
    uint32_t tagPaused = 0;          // daTagMwait_c::execute im Ritt ausgesetzt (Frames)
    uint32_t tagCanTalk = 0;         // Sensor: Tag (Mwait/Hstop) bietet im Ritt CANTALK oder TALK an
    uint32_t tagSpeak = 0;           // Sensor: Sprech-Auftrag eines Tags im Ritt
    uint32_t speakInRide = 0;        // Sensor: alle Sprech-Auftraege im Ritt
    uint32_t metamorphoseCalls = 0;  // procCoMetamorphoseInit, alle Aufrufe
    uint32_t transformEnd = 0;       // Ritt-Ende mit Grund Transform vor dem Verwandlungs-Proc
    uint32_t overrideFrames = 0;     // Sensor: Link-Frame im Ritt mit Ereignis oder Demo (Wiederanbindung wuerde ueberschreiben)
    uint32_t howlInRide = 0;         // Sensor: Link im Heul-Proc waehrend des Ritts
    uint32_t exceptions = 0;         // abgefangene Ausnahmen in den Ereignis-Callbacks
#if defined(KFF_DEV_AUTOTEST)
    uint32_t skipStalePtr = 0;       // Test wp4 skip: demo_skip mit dem Zeiger des entfernten Vogels
#endif
};
EvStats g_ev;
uint32_t g_midnaGuard = 0;  // > 0: ein Midna-Auftrag wurde im Ritt abgelehnt, die Demo-Folgen werden zurueckgenommen

#if defined(KFF_DEV_AUTOTEST)
void* g_w4StaleBird = nullptr;     // Test wp4 skip: Adresse des Vogels, der als Skip-Akteur entfernt wurde (nur Vergleich)
void w4NoteEnd(EndReason why);     // Test wp4: Ritt-Enden mitschreiben (unten)
void w4NoteStart();                // Test wp4: Ritt-Starts mitschreiben, mit Ereignis-Lage (unten)
#endif

uint32_t g_logEvEx = 0;
uint32_t g_logSkip = 0;
uint32_t g_logPerch = 0;
uint32_t g_logScript = 0;
uint32_t g_logSetEvent = 0;
uint32_t g_logStory = 0;
uint32_t g_logMidna = 0;
uint32_t g_logMidnaReset = 0;
uint32_t g_logTag = 0;
uint32_t g_logTagSensor = 0;
uint32_t g_logSpeak = 0;
uint32_t g_logOverride = 0;
uint32_t g_logHowl = 0;

#if defined(KFF_DEV_AUTOTEST)
bool g_wp4Off = false;  // KFF_WP4_AUS=1: Vergleichslauf ohne WP4-Absicherungen, Sensoren zaehlen weiter
#endif

// Vergleichslauf WP4 (nur Testbuild). Im Release immer false.
bool wp4Control() {
#if defined(KFF_DEV_AUTOTEST)
    return g_wp4Off;
#else
    return false;
#endif
}

void logEvStats(const char* tag) {
    logInfo("griff: %s WP4 demoSkipCalls=%u skipNoBird=%u skipBlocked=%u rideOffPrevented=%u perchIgnored=%u "
            "scriptNeutral=%u birdEventEnded=%u setEventInRide=%u storyBlocked=%u storyControl=%u",
        tag, g_ev.demoSkipCalls, g_ev.skipNoBird, g_ev.skipBlocked, g_ev.rideOffPrevented, g_ev.perchIgnored,
        g_ev.scriptNeutral, g_ev.birdEventEnded, g_ev.setEventInRide, g_ev.storyBlocked, g_ev.storyControl);
    logInfo("griff: %s WP4 midnaOrderSeen=%u midnaOrderRefused=%u midnaDemoReset=%u tagPaused=%u tagCanTalk=%u "
            "tagSpeak=%u speakInRide=%u metamorphoseCalls=%u transformEnd=%u overrideFrames=%u howlInRide=%u "
            "exceptions=%u",
        tag, g_ev.midnaOrderSeen, g_ev.midnaOrderRefused, g_ev.midnaDemoReset, g_ev.tagPaused, g_ev.tagCanTalk,
        g_ev.tagSpeak, g_ev.speakInRide, g_ev.metamorphoseCalls, g_ev.transformEnd, g_ev.overrideFrames,
        g_ev.howlInRide, g_ev.exceptions);
}

// Huellen: keine Ausnahme verlaesst einen Callback. Pre: im Fehlerfall laeuft das Original.
template <class Fn>
HookAction evPre(const char* where, Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        ++g_ev.exceptions;
        logEvery(g_logEvEx, 3, 300, "griff: exception in %s caught (original runs)", where);
        return HOOK_CONTINUE;
    }
}

template <class Fn>
void evPost(const char* where, Fn&& fn) {
    try {
        fn();
    } catch (...) {
        ++g_ev.exceptions;
        logEvery(g_logEvEx, 3, 300, "griff: exception in %s caught", where);
    }
}

bool isScriptAction(int action) {
    return action == daKago_c::ACTION_EVENT_e || action == daKago_c::ACTION_EVENT2_e ||
           action == daKago_c::ACTION_PERCH_e || action == daKago_c::ACTION_PERCH2_e;
}

// Lebt an dieser Adresse ein Kargarok? Vergleich ueber die Akteurliste, die Adresse selbst wird nicht gelesen.
void* judgeKagoAt(void* actor, void* data) {
    if (actor == data && fopAcM_GetName(static_cast<fopAc_ac_c*>(actor)) == fpcNm_KAGO_e) {
        return actor;
    }
    return nullptr;
}

daKago_c* liveKagoAt(void* p) {
    if (p == nullptr) {
        return nullptr;
    }
    return static_cast<daKago_c*>(fopAcIt_Judge(judgeKagoAt, p));
}

// Eigener Vogel (Sitzung) oder Mod-Vogel ohne Sitzung (Parameter des Pakets, z. B. nach einem Neuladen verwaist).
bool ownOrModBird(daKago_c* k, bool* own) {
    const bool o = isOwnBird(k);
    if (own != nullptr) {
        *own = o;
    }
    return o || fopAcM_GetParam(k) == kModBirdParam;
}

bool isMidnaActor(fopAc_ac_c* a) {
    return a != nullptr &&
           (a == static_cast<fopAc_ac_c*>(daPy_py_c::getMidnaActor()) || fopAcM_GetName(a) == fpcNm_MIDNA_e);
}

// Das Ereignis, an dem der Vogel beteiligt ist (mPt1/mPt2), endet mit dem Abfangen; sonst liefe es mit einem Vogel ohne
// Skript weiter (Haenger). Stand der Vogel schon in PERCH, hat executeEvent die Kamera gestoppt: sie startet wieder.
void endBirdEvent(daKago_c* k, bool cameraStopped) {
    dEvt_control_c* evt = dComIfGp_getEvent();
    const fpc_ProcID id = fopAcM_GetID(k);
    bool reset = false;
    if (evt->mSkipActorId == id) {
        evt->clearSkipSystem();
    }
    if (evt->runCheck() && (evt->mPt1 == id || evt->mPt2 == id)) {
        dComIfGp_event_reset();
        reset = true;
        ++g_ev.birdEventEnded;
    }
    if (reset || cameraStopped) {
        if (dCamera_c* cam = dCam_getBody()) {
            cam->Start();
            cam->SetTrimSize(0);
        }
    }
    if (reset) {
        logInfo("griff: event of bird 0x%x ended (script intercepted), camera started", static_cast<unsigned>(id));
    }
}

// Skript-Aktion am eigenen/Mod-Vogel aufheben: im Ritt FLY 1, im Abflug weiter wegfliegen, sonst ruhig in WAIT
// (verwaister Mod-Vogel ohne eigenen Ritt-Start).
void neutralizeScript(daKago_c* k, bool own, const char* where) {
    const Session& s = session();
    const int action = k->mAction;
    const bool cameraStopped = action == daKago_c::ACTION_PERCH_e || action == daKago_c::ACTION_PERCH2_e;
    endBirdEvent(k, cameraStopped);
    const char* how = "WAIT";
    if (own && s.phase == Phase::Riding) {
        k->setActionMode(daKago_c::ACTION_FLY_e, 1);
        how = "FLY 1";
    } else if (own && s.phase == Phase::Dismounting) {
        k->shape_angle.y = k->current.angle.y;  // setFlyAway() nimmt shape_angle.y als Kurs
        k->setFlyAway();
        how = "Abflug";
    } else {
        k->setActionMode(daKago_c::ACTION_WAIT_e, 0);
        if (!own) {
            k->field_0x6df = 1;  // verwaister Mod-Vogel: executeWait startet keinen eigenen Ritt
        }
    }
    ++g_ev.scriptNeutral;
    if (k->mpPath2 == nullptr) {
        ++g_ev.perchIgnored;
        logEvery(g_logPerch, 10, 60, "perch ignored: no path (%s, action %d, %s bird, phase %s) -> %s, n=%u", where,
            action, own ? "own" : "mod", phaseName(s.phase), how, g_ev.perchIgnored);
    } else {
        logEvery(g_logScript, 10, 60, "griff: script action %d on %s bird intercepted (%s, phase %s) -> %s, n=%u",
            action, own ? "own" : "mod", where, phaseName(s.phase), how, g_ev.scriptNeutral);
    }
}

// daKago_c::execute, Pre (nach dem Kern): Skript-Aktion am eigenen oder Mod-Vogel abfangen, bevor action() sie ausfuehrt.
// executeEvent bestellt sonst ein Ereignis, danach liest initPerchDemo mpPath2 (null) und searchNearPassPoint mpPath1.
HookAction onKagoExecutePre(ModContext*, void* args, void*, void*) {
    return evPre("daKago_c::execute", [&]() -> HookAction {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (k == nullptr || !isScriptAction(k->mAction)) {
            return HOOK_CONTINUE;
        }
        bool own = false;
        if (!ownOrModBird(k, &own)) {
            return HOOK_CONTINUE;  // Vanilla-Vogel: sein Skript bleibt unberuehrt
        }
        if (wp4Control()) {
            logEvery(g_logScript, 3, 120, "griff: [Vergleichslauf] script action %d on %s bird stays", k->mAction,
                own ? "own" : "mod");
            return HOOK_CONTINUE;
        }
        neutralizeScript(k, own, "execute");
        return HOOK_CONTINUE;
    });
}

// executePerch und initPerchDemo, Pre: ohne mpPath2 (Vanilla-Absturz in initPerchDemo, dPath_GetPnt ohne Pruefung) oder am
// eigenen/Mod-Vogel (mpPath1 fehlt, searchNearPassPoint) laeuft das Original nicht.
HookAction perchGuard(void* args, const char* where) {
    daKago_c* k = argOf<daKago_c*>(args, 0);
    if (k == nullptr) {
        return HOOK_CONTINUE;
    }
    bool own = false;
    const bool ours = ownOrModBird(k, &own);
    const bool noPath = k->mpPath2 == nullptr;
    if (!ours && !noPath) {
        return HOOK_CONTINUE;  // Vanilla-Vogel mit Pfad: Spiel unveraendert
    }
    if (wp4Control()) {
        logEvery(g_logPerch, 3, 120, "griff: [Vergleichslauf] %s runs (%s, path %s)", where,
            ours ? "own/mod bird" : "vanilla bird", noPath ? "missing" : "set");
        return HOOK_CONTINUE;
    }
    ++g_ev.perchIgnored;
    logEvery(g_logPerch, 10, 60, "perch ignored: %s (%s, id=0x%x, %s bird, phase %s), n=%u",
        noPath ? "no path" : "own bird", where, static_cast<unsigned>(fopAcM_GetID(k)),
        own ? "own" : (ours ? "mod" : "vanilla"), phaseName(session().phase), g_ev.perchIgnored);
    if (ours) {
        neutralizeScript(k, own, where);
    } else {
        endBirdEvent(k, true);
        k->setActionMode(daKago_c::ACTION_WAIT_e, 0);
        k->field_0x6df = 1;
    }
    return HOOK_SKIP_ORIGINAL;
}

HookAction onExecutePerchPre(ModContext*, void* args, void*, void*) {
    return evPre("daKago_c::executePerch", [&]() -> HookAction { return perchGuard(args, "executePerch"); });
}

HookAction onInitPerchDemoPre(ModContext*, void* args, void*, void*) {
    return evPre("daKago_c::initPerchDemo", [&]() -> HookAction { return perchGuard(args, "initPerchDemo"); });
}

// daKago_c::demo_skip(int), Pre. Der Aufruf kommt aus DemoSkipCallBack (Skipper). Ist der Skip-Akteur tot, uebergibt der
// Skipper Link als Vogel: dann laeuft das Original nicht (es schriebe in Links Speicher), das Ereignis endet. Am eigenen
// oder Mod-Vogel laeuft das Original nie: Faelle 2/3 setzen M_051 und Aufsitzen, 4-6 wechseln die Szene, 7/9 rufen
// setRideOff und teleportieren Link. Vanilla-Voegel behalten den Vanilla-Skip.
HookAction onDemoSkipPre(ModContext*, void* args, void*, void*) {
    return evPre("daKago_c::demo_skip", [&]() -> HookAction {
        void* self = argOf<void*>(args, 0);
        const int param = argOf<int>(args, 1);
        ++g_ev.demoSkipCalls;
#if defined(KFF_DEV_AUTOTEST)
        if (self != nullptr && self == g_w4StaleBird) {
            ++g_ev.skipStalePtr;  // nur Adressvergleich, kein Zugriff
        }
#endif
        daKago_c* k = liveKagoAt(self);
        if (k == nullptr) {
            ++g_ev.skipNoBird;
            logEvery(g_logSkip, 10, 60, "demo_skip without a live Kargarok (case %d, actor %p)%s, n=%u", param, self,
                wp4Control() ? " [Vergleichslauf: original runs]" : ": skipped, event ended", g_ev.skipNoBird);
            if (wp4Control()) {
                return HOOK_CONTINUE;
            }
            if (dComIfGp_event_runCheck()) {
                dComIfGp_event_reset();
                if (dCamera_c* cam = dCam_getBody()) {
                    cam->Start();
                    cam->SetTrimSize(0);
                }
            }
            return HOOK_SKIP_ORIGINAL;
        }
        bool own = false;
        if (!ownOrModBird(k, &own)) {
            return HOOK_CONTINUE;
        }
        ++g_ev.skipBlocked;
        if (param == 7 || param == 9) {
            ++g_ev.rideOffPrevented;
        }
        logEvery(g_logSkip, 10, 60, "demo_skip blocked (%s bird, case %d, phase %s)%s, n=%u", own ? "own" : "mod", param,
            phaseName(session().phase), wp4Control() ? " [Vergleichslauf: original runs]" : "", g_ev.skipBlocked);
        return wp4Control() ? HOOK_CONTINUE : HOOK_SKIP_ORIGINAL;
    });
}

// dComIfGs_onEventBit / offEventBit, Pre: M_067 (Midna reitet) und M_077 (Verwandlung) bleiben im Ritt unveraendert.
// Der F6-Editor von Dusklight schreibt dSv_event_c direkt und laeuft hier nicht durch (README).
HookAction storyFlagGuard(void* args, bool on) {
    if (!isRiding()) {
        return HOOK_CONTINUE;
    }
    const u16 flag = argOf<u16>(args, 0);
    const char* name = nullptr;
    if (flag == static_cast<u16>(dSv_event_flag_c::M_067)) {
        name = "M_067";
    } else if (flag == static_cast<u16>(dSv_event_flag_c::M_077)) {
        name = "M_077";
    } else {
        return HOOK_CONTINUE;
    }
    if (wp4Control()) {
        ++g_ev.storyControl;
        logEvery(g_logStory, 10, 60, "griff: [Vergleichslauf] story flag %s changed in ride (%s)", name, on ? "on" : "off");
        return HOOK_CONTINUE;
    }
    ++g_ev.storyBlocked;
    logEvery(g_logStory, 20, 60, "story flag blocked: %s (%s), n=%u", name, on ? "on" : "off", g_ev.storyBlocked);
    return HOOK_SKIP_ORIGINAL;
}

HookAction onEventBitOnPre(ModContext*, void* args, void*, void*) {
    return evPre("dComIfGs_onEventBit", [&]() -> HookAction { return storyFlagGuard(args, true); });
}

HookAction onEventBitOffPre(ModContext*, void* args, void*, void*) {
    return evPre("dComIfGs_offEventBit", [&]() -> HookAction { return storyFlagGuard(args, false); });
}

// Midna-Auftrag im Ritt ablehnen (Muster skript-sperren): Rueckgabe 0, kein Ereignis. Die Midna-Ablaeufe setzen danach
// trotzdem Demo-Typ und -Modus von Link und Midna (d_a_midna.cpp, event_id 4/5); g_midnaGuard nimmt das im naechsten
// Pad-Tick zurueck, bevor Link ausfuehrt.
HookAction onOrderPotentialPre(ModContext*, void* args, void* retval, void*) {
    return evPre("fopAcM_orderPotentialEvent", [&]() -> HookAction {
        if (!isRiding()) {
            return HOOK_CONTINUE;
        }
        fopAc_ac_c* actor = argOf<fopAc_ac_c*>(args, 0);
        if (!isMidnaActor(actor)) {
            return HOOK_CONTINUE;
        }
        ++g_ev.midnaOrderSeen;
        const unsigned flag = argOf<u16>(args, 1);
        if (wp4Control()) {
            logEvery(g_logMidna, 5, 60, "griff: [Vergleichslauf] midna order in ride (potential event, flag 0x%x) runs",
                flag);
            return HOOK_CONTINUE;
        }
        ++g_ev.midnaOrderRefused;
        g_midnaGuard = kMidnaGuardTicks;
        if (retval != nullptr) {
            *static_cast<s32*>(retval) = 0;
        }
        logEvery(g_logMidna, 10, 60, "midna order refused (potential event, flag 0x%x), midnaOrderRefused=%u", flag,
            g_ev.midnaOrderRefused);
        return HOOK_SKIP_ORIGINAL;
    });
}

HookAction onOrderTalkPre(ModContext*, void* args, void* retval, void*) {
    return evPre("fopAcM_orderTalkEvent", [&]() -> HookAction {
        if (!isRiding()) {
            return HOOK_CONTINUE;
        }
        fopAc_ac_c* a = argOf<fopAc_ac_c*>(args, 0);
        fopAc_ac_c* b = argOf<fopAc_ac_c*>(args, 1);
        if (!isMidnaActor(a) && !isMidnaActor(b)) {
            return HOOK_CONTINUE;
        }
        ++g_ev.midnaOrderSeen;
        if (wp4Control()) {
            logEvery(g_logMidna, 5, 60, "griff: [Vergleichslauf] midna talk order in ride runs");
            return HOOK_CONTINUE;
        }
        ++g_ev.midnaOrderRefused;
        if (retval != nullptr) {
            *static_cast<s32*>(retval) = 0;
        }
        logEvery(g_logMidna, 10, 60, "midna order refused (talk event), midnaOrderRefused=%u", g_ev.midnaOrderRefused);
        return HOOK_SKIP_ORIGINAL;
    });
}

// Sensor: Sprech-Auftraege im Ritt (Tag_Mwait, Tag_Hstop). Kein SKIP.
HookAction onOrderSpeakPre(ModContext*, void* args, void*, void*) {
    return evPre("fopAcM_orderSpeakEvent", [&]() -> HookAction {
        if (!isRiding()) {
            return HOOK_CONTINUE;
        }
        ++g_ev.speakInRide;
        fopAc_ac_c* actor = argOf<fopAc_ac_c*>(args, 0);
        const s16 name = actor != nullptr ? fopAcM_GetName(actor) : static_cast<s16>(-1);
        if (name == fpcNm_Tag_Mwait_e || name == fpcNm_Tag_Hstop_e) {
            ++g_ev.tagSpeak;
        }
        logEvery(g_logSpeak, 10, 60, "griff: speak order during ride (actor name %d), speakInRide=%u tagSpeak=%u",
            static_cast<int>(name), g_ev.speakInRide, g_ev.tagSpeak);
        return HOOK_CONTINUE;
    });
}

#if defined(KFF_DEV_AUTOTEST)
void w4TagPrep(fopAc_ac_c* tag);  // Testszenario wp4 tags: Lage des Test-Tags direkt vor seinem Execute (unten)
#endif

// daTagMwait_c::execute, Pre: im Ritt ausgesetzt (kein onTagWaitPos fuer Midna, kein Sprech-Auftrag, kein CANTALK). Ein
// schon laufendes Gespraech des Tags fuehrt er zu Ende (sonst bliebe das Ereignis offen).
HookAction onTagMwaitPre(ModContext*, void* args, void* retval, void*) {
    return evPre("daTagMwait_c::execute", [&]() -> HookAction {
#if defined(KFF_DEV_AUTOTEST)
        w4TagPrep(argOf<fopAc_ac_c*>(args, 0));
#endif
        if (!isRiding()) {
            return HOOK_CONTINUE;
        }
        fopAc_ac_c* tag = argOf<fopAc_ac_c*>(args, 0);
        if (tag == nullptr || tag->eventInfo.checkCommandTalk() || wp4Control()) {
            return HOOK_CONTINUE;
        }
        ++g_ev.tagPaused;
        tag->attention_info.flags = 0;
        tag->eventInfo.offCondition(dEvtCnd_CANTALK_e);
        logEvery(g_logTag, 3, 300, "griff: tag_mwait paused during ride (no speak order, no CANTALK), n=%u",
            g_ev.tagPaused);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    });
}

// Sensor nach dem Tag-Execute: bietet ein Tag im Ritt ein Gespraech an?
void tagSensor(void* args, const char* which) {
    if (!isRiding()) {
        return;
    }
    fopAc_ac_c* tag = argOf<fopAc_ac_c*>(args, 0);
    if (tag == nullptr) {
        return;
    }
    if (tag->eventInfo.chkCondition(dEvtCnd_CANTALK_e) || (tag->attention_info.flags & fopAc_AttnFlag_TALK_e) != 0) {
        ++g_ev.tagCanTalk;
        logEvery(g_logTagSensor, 10, 60, "griff: %s offers CANTALK/TALK during ride, tagCanTalk=%u", which,
            g_ev.tagCanTalk);
    }
}

void onTagMwaitPost(ModContext*, void* args, void*, void*) {
    evPost("daTagMwait_c::execute (post)", [&]() { tagSensor(args, "tag_mwait"); });
}

void onTagHstopPost(ModContext*, void* args, void*, void*) {
    evPost("daTagHstop_c::execute (post)", [&]() { tagSensor(args, "tag_hstop"); });
}

// procCoMetamorphoseInit, Pre: Verwandlung ueber den Spielweg (Schnellverwandlung, Midna) mit eigenem Vogel. Der Ritt
// endet vorher geordnet (Grund Transform), danach laeuft das Original. Die Verwandlung des Moduls autowolf (Phase
// Transforming, kein Vogel) bleibt unberuehrt.
HookAction onMetamorphosePre(ModContext*, void*, void*, void*) {
    return evPre("daAlink_c::procCoMetamorphoseInit", [&]() -> HookAction {
        ++g_ev.metamorphoseCalls;
        const Session& s = session();
        if (s.birdId == kNoProc || s.phase == Phase::Transforming) {
            return HOOK_CONTINUE;
        }
        logInfo("griff: transformation via game path with own bird (phase %s)%s", phaseName(s.phase),
            wp4Control() ? " [Vergleichslauf: ride stays]" : ", ride ends before the transform proc");
        if (wp4Control()) {
            return HOOK_CONTINUE;
        }
        ++g_ev.transformEnd;
        endRideClean(EndReason::Transform, "Verwandlung (Spielweg)");
        return HOOK_CONTINUE;
    });
}

// Midna-Ablauf nach abgelehntem Auftrag: Link hat changeOriginalDemo + changeDemoMode(0x3a/0x39), Midna
// changeOriginalDemo + changeDemoMode(0xf) bekommen. Zurueck auf "keine Demo" wie endDemoMode (d_a_alink_demo.inc:928-931).
void undoMidnaDemo() {
    bool undone = false;
    daAlink_c* link = liveLink();
    if (link != nullptr && link->mDemo.getDemoType() == daPy_demo_c::DEMO_TYPE_ORIGINAL_e &&
        (link->mDemo.getDemoMode() == daPy_demo_c::DEMO_METAMORPHOSE_UNK1_e ||
            link->mDemo.getDemoMode() == daPy_demo_c::DEMO_METAMORPHOSE_UNK2_e)) {
        link->mDemo.resetDemoType();
        link->mDemo.setDemoMode(0);
        link->mDemo.setParam0(0);
        link->mDemo.setParam1(0);
        undone = true;
    }
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna != nullptr && midna->mDemoType == 3 && midna->mDemoMode == 0xf) {
        midna->mDemoType = 0;
        midna->mDemoMode = 0;
        undone = true;
    }
    if (undone) {
        ++g_ev.midnaDemoReset;
        logEvery(g_logMidnaReset, 10, 60, "midna demo reset (order refused), n=%u", g_ev.midnaDemoReset);
    }
}

// Sensor: Skript-Aktion am eigenen Vogel im Ritt seit dem letzten Frame (setEvent/setEvent2 sind Inline-Methoden ohne
// Symbol, hooks.hpp). Der Pad-Post laeuft vor jedem Akteur, also bevor der Kern die Aktion zurueck auf FLY setzt.
void sensorBirdScript() {
    const Session& s = session();
    if (s.phase != Phase::Riding) {
        return;
    }
    daKago_c* bird = liveBird();
    if (bird == nullptr || !isScriptAction(bird->mAction)) {
        return;
    }
    ++g_ev.setEventInRide;
    logEvery(g_logSetEvent, 5, 60, "griff: setEvent on own bird during ride seen (action %d, ride frame %u), n=%u",
        bird->mAction, s.rideFrames, g_ev.setEventInRide);
}

#if defined(KFF_DEV_AUTOTEST)
void wp4PadCheck();  // Testszenario wp4: Konsum von X/Y im Ritt messen (unten)
#endif

void onPadReadPost(ModContext*, void*, void*, void*) {
    evPost("mDoCPd_c::read (griff)", [&]() {
        if (g_midnaGuard > 0) {
            --g_midnaGuard;
            undoMidnaDemo();
        }
        sensorBirdScript();
#if defined(KFF_DEV_AUTOTEST)
        wp4PadCheck();
#endif
    });
}

// Sensor nach Links Execute: Heul-Proc im Ritt.
void onLinkExecutePost(ModContext*, void* args, void*, void*) {
    evPost("daAlink_c::execute (griff post)", [&]() {
        if (!isRiding()) {
            return;
        }
        daAlink_c* link = argOf<daAlink_c*>(args, 0);
        if (link == nullptr) {
            return;
        }
        if (link->mProcID == daAlink_c::PROC_WOLF_HOWL_DEMO || link->mProcID == daAlink_c::PROC_WOLF_HOWL) {
            ++g_ev.howlInRide;
            logEvery(g_logHowl, 5, 60, "griff: howl proc %u during ride, n=%u", static_cast<unsigned>(link->mProcID),
                g_ev.howlInRide);
        }
    });
}

// Registrierung des Ereignis-Teils. Jeder Fehlschlag zaehlt als Registrierungsfehler des Moduls (Kern schaltet griff ab).
ModResult installEventHooks(Registry& reg) {
    ModResult worst = MOD_OK;
    auto note = [&](const char* what, ModResult r) {
        if (r != MOD_OK) {
            logError("griff: hook %s -> %s", what, modResultName(r));
            worst = r;
        }
    };
    note("daKago_c::execute pre", reg.addPre(Hook::KagoExecute, &onKagoExecutePre, prio::kGriff));
    note("daKago_c::executePerch pre", reg.addPre(Hook::KagoExecutePerch, &onExecutePerchPre, prio::kGriff));
    note("daKago_c::initPerchDemo pre", reg.addPre(Hook::KagoInitPerchDemo, &onInitPerchDemoPre, prio::kGriff));
    note("daKago_c::demo_skip pre", reg.addPre(Hook::KagoDemoSkip, &onDemoSkipPre, prio::kGriff));
    note("dComIfGs_onEventBit pre", reg.addPre(Hook::EventBitOn, &onEventBitOnPre, prio::kGriff));
    note("dComIfGs_offEventBit pre", reg.addPre(Hook::EventBitOff, &onEventBitOffPre, prio::kGriff));
    note("fopAcM_orderPotentialEvent pre", reg.addPre(Hook::OrderPotentialEvent, &onOrderPotentialPre, prio::kGriff));
    note("fopAcM_orderTalkEvent pre", reg.addPre(Hook::OrderTalkEvent, &onOrderTalkPre, prio::kGriff));
    note("fopAcM_orderSpeakEvent pre", reg.addPre(Hook::OrderSpeakEvent, &onOrderSpeakPre, prio::kGriff));
    note("daTagMwait_c::execute pre", reg.addPre(Hook::TagMwaitExecute, &onTagMwaitPre, prio::kGriff));
    note("daTagMwait_c::execute post", reg.addPost(Hook::TagMwaitExecute, &onTagMwaitPost, prio::kGriff));
    note("daTagHstop_c::execute post", reg.addPost(Hook::TagHstopExecute, &onTagHstopPost, prio::kGriff));
    note("daAlink_c::procCoMetamorphoseInit pre",
        reg.addPre(Hook::LinkProcCoMetamorphoseInit, &onMetamorphosePre, prio::kGriff));
    note("mDoCPd_c::read post", reg.addPost(Hook::PadRead, &onPadReadPost, prio::kGriff));
    note("daAlink_c::execute post", reg.addPost(Hook::LinkExecute, &onLinkExecutePost, prio::kGriff));
    return worst;
}

#if defined(KFF_DEV_AUTOTEST)
const autotest::Scenario* wp4Scenario();  // Testszenario wp4 (unten)
#endif

// ---------------------------------------------------------------------------------------------
// WP5: Zustands-Whitelist im Ritt (Wasser, Gegner, Stagger, Procs, Reittiere)
// ---------------------------------------------------------------------------------------------

constexpr float kSwimWolf = 90.0f;    // Schwimm-Schwelle Wolf (mpHIO mWlSwim mStartHeight, d_a_alink_swim.inc:346)
constexpr float kSwimHuman = 140.0f;  // Schwimm-Schwelle Mensch (mpHIO mSwim mStartHeight)
constexpr float kWaterReleaseClear = 30.0f;  // K17: Landeanflug ueber Wasser endet, wenn Links Fuesse so tief ueber dem Wasser sind
constexpr uint32_t kHookLog = 300;    // Log-Takt der Hook-Zaehler

struct Wp5Stats {
    uint32_t staggerSkip = 0;   // executeStagger im Ritt uebersprungen, Vogel bleibt FLY
    uint32_t landingSkip = 0;   // executeLanding* im Ritt uebersprungen (Landung nur ueber den Kern-Landeanflug)
    uint32_t swimSkip = 0;      // checkSwimAction im Ritt uebersprungen (Aufrufe)
    uint32_t swimBlocked = 0;   // davon: Wasser ueber der Schwimm-Schwelle (ohne Sperre waere Link geschwommen)
    uint32_t dmgThrow = 0;      // Wurf-/Grossschaden im Ritt ohne Proc-Wechsel angewendet
    uint32_t dmgSand = 0;       // Sandwand-Treffer im Ritt verworfen (kein procCoSandWallHitInit)
    uint32_t dmgPressed = 0;    // Quetsch-Schaden im Ritt ohne Proc-Wechsel (kein commonLargeDamageUpInit)
    uint32_t dmgReverse = 0;    // Barrieren-Rueckstoss im Ritt verworfen (kein procWolfDashReverseInit)
    uint32_t wolfDmgInit = 0;   // procWolfDamageInit im Ritt uebersprungen
    uint32_t lavaSkip = 0;      // procCoLavaReturnInit im Ritt uebersprungen
    uint32_t riverTagSkip = 0;  // daTagKagoFall_c::execute im Ritt eingefroren
    uint32_t gateRejects = 0;   // Ruf-Gate (Whitelist) hat abgelehnt
    uint32_t letGo = 0;         // Vogel hat losgelassen (Aktion nicht FLY/ATTACK, Griff weg): Ritt endet sofort
    uint32_t waterRelease = 0;  // K17: Landeanflug ueber Wasser an der Wasserflaeche beendet
    uint32_t exceptions = 0;    // abgefangene Ausnahmen in den WP5-Callbacks
};
Wp5Stats g_wp5;

uint32_t g_logStagger = 0;
uint32_t g_logLanding = 0;
uint32_t g_logSwim = 0;
uint32_t g_logDmg = 0;
uint32_t g_logTagWp5 = 0;  // Fluss-/Fall-Tag (WP5); g_logTag gehoert dem Ereignis-Teil (WP4)
uint32_t g_logRestart = 0;
uint32_t g_logGate = 0;
uint32_t g_logExc = 0;

#if defined(KFF_DEV_AUTOTEST)
bool g_noGuard = false;      // KFF_TEST_NOGUARD=1: Griff-Sperren aus (Stagger, Landung, Wiederanbindung)
bool g_wp5Ohne = false;      // KFF_WP5_OHNE=1: WP5-Schutz aus (Schwimmen, Schaden, Neustart, Tags), Vergleichslauf
uint32_t g_testHitAt = 0;    // KFF_GRIFF_TESTHIT=N: Test-AT an Link ab Ritt-Frame N (0 = aus)
bool g_testStagger = false;  // KFF_TEST_STAGGER=1: STAGGER am Vogel im Ritt erzwingen
#endif

// Testschalter. Im Release immer false.
bool noGuard() {
#if defined(KFF_DEV_AUTOTEST)
    return g_noGuard;
#else
    return false;
#endif
}

bool unprotected() {
#if defined(KFF_DEV_AUTOTEST)
    return g_wp5Ohne;
#else
    return false;
#endif
}

// Eigener Ritt laeuft (Phase Riding, Vogel nicht zum Entfernen vorgemerkt).
bool rideActive() {
    const Session& s = session();
    return s.phase == Phase::Riding && !s.deleteRequested && s.birdId != kNoProc;
}

bool riderDead(daAlink_c* link) {
    return link->checkDeadHP() || link->mProcID == daAlink_c::PROC_DEAD;
}

bool eventNow(daAlink_c* link) {
    return dComIfGp_event_runCheck() || link->checkEventRun();
}

// Link im geschuetzten Ritt: eigener Ritt, Link ist der Reiter (frische Suche), lebt und steht in keinem Ereignis.
bool protectedRider(daAlink_c* link) {
    return link != nullptr && rideActive() && link == liveLink() && !riderDead(link) && !eventNow(link);
}

// Wolf im Cargo-Ritt (nicht der Mensch-Griff des Moduls mensch).
bool wolfCargoRider(daAlink_c* link) {
    return protectedRider(link) && !session().rideGrip && link->checkWolf() != 0;
}

// Wasser ueber der Schwimm-Schwelle an Links Position (Bedingung aus checkSwimAction, ohne Sonderfaelle).
bool deepWater(daAlink_c* link) {
    const float th = link->checkWolf() != 0 ? kSwimWolf : kSwimHuman;
    return link->checkNoResetFlg0(daPy_py_c::FLG0_UNK_80) != 0 && link->mWaterY - link->current.pos.y > th;
}

void countException(const char* where) {
    ++g_wp5.exceptions;
    logEvery(g_logExc, 3, kHookLog, "griff: Ausnahme in %s abgefangen (Original laeuft), n=%u", where, g_wp5.exceptions);
}

// Griff-Sperre: der eigene Vogel bleibt im Ritt FLY. executeStagger und executeLanding* laufen nicht (SKIP).
bool gripLockActive(daKago_c* k) {
    return k != nullptr && isOwnBird(k) && rideActive() && !noGuard() && !dComIfGp_event_runCheck();
}

HookAction onStaggerPre(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (!gripLockActive(k)) {
            return HOOK_CONTINUE;
        }
        ++g_wp5.staggerSkip;
        logEvery(g_logStagger, 5, kHookLog,
            "griff: Stagger im Ritt unterdrueckt (Modus %d, Ritt-Frame %u), Vogel bleibt FLY, n=%u", k->mMode,
            session().rideFrames, g_wp5.staggerSkip);
        k->setActionMode(daKago_c::ACTION_FLY_e, 1);
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("executeStagger");
        return HOOK_CONTINUE;
    }
}

// userdata: Name des Ziels fuer den Log.
HookAction onLandingPre(ModContext*, void* args, void*, void* userdata) {
    try {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (!gripLockActive(k)) {
            return HOOK_CONTINUE;
        }
        ++g_wp5.landingSkip;
        logEvery(g_logLanding, 5, kHookLog,
            "griff: %s im Ritt unterdrueckt (Landung nur ueber den Kern-Landeanflug, E9), Modus %d, Ritt-Frame %u, n=%u",
            userdata != nullptr ? static_cast<const char*>(userdata) : "executeLanding", k->mMode, session().rideFrames,
            g_wp5.landingSkip);
        k->setActionMode(daKago_c::ACTION_FLY_e, 1);
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("executeLanding");
        return HOOK_CONTINUE;
    }
}

// daAlink_c::checkSwimAction(int), Pre: im Ritt kein Schwimmen (Rueckgabe 0).
HookAction onSwimActionPre(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = argOf<daAlink_c*>(args, 0);
        if (unprotected() || !protectedRider(link)) {
            return HOOK_CONTINUE;
        }
        ++g_wp5.swimSkip;
        if (deepWater(link)) {
            ++g_wp5.swimBlocked;
            logEvery(g_logSwim, 5, kHookLog,
                "griff: Schwimmen im Ritt gesperrt (Wasser %.0f ueber Link, Proc 0x%x, Ritt-Frame %u), n=%u",
                static_cast<double>(link->mWaterY - link->current.pos.y), static_cast<unsigned>(link->mProcID),
                session().rideFrames, g_wp5.swimBlocked);
        }
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("checkSwimAction");
        return HOOK_CONTINUE;
    }
}

// daAlink_c::checkDamageAction, Pre (CONTINUE): Schadenszweige, die im Cargo-Ritt den Proc wechseln, werden vorher
// aufgeloest. Der Lebensverlust bleibt (Wurfschaden: setDamagePointNormal wie procCoLargeDamageInit(-3)). Normale
// TG-, Poly- und Elektro-Treffer behandelt das Spiel im Cargo-Modus (Modus-Flag 0x4000000) selbst ohne Proc-Wechsel.
void resolveRideDamage(daAlink_c* link) {
    const uint32_t e0 = link->mEndResetFlg0;
    if ((e0 & (daPy_py_c::ERFLG0_UNK_40 | daPy_py_c::ERFLG0_UNK_80)) != 0) {
        const int dmg = link->field_0x318c;
        const int lifeBefore = static_cast<int>(dComIfGs_getLife());
        if (dmg > 0) {
            link->setDamagePointNormal(dmg);
        }
        link->setWolfHeadDamage();
        link->mEndResetFlg0 &= ~static_cast<uint32_t>(daPy_py_c::ERFLG0_UNK_40 | daPy_py_c::ERFLG0_UNK_80 |
                                                       daPy_py_c::ERFLG0_UNK_40000000);
        link->mEndResetFlg1 &= ~static_cast<uint32_t>(daPy_py_c::ERFLG1_UNK_80);
        ++g_wp5.dmgThrow;
        logEvery(g_logDmg, 10, 60,
            "griff: Wurfschaden %d im Ritt ohne Proc-Wechsel (Leben %d -> %d, Proc 0x%x bleibt, Ritt-Frame %u), n=%u", dmg,
            lifeBefore, static_cast<int>(dComIfGs_getLife()), static_cast<unsigned>(link->mProcID), session().rideFrames,
            g_wp5.dmgThrow);
    }
    if ((link->mEndResetFlg1 & daPy_py_c::ERFLG1_UNK_400) != 0) {
        link->mEndResetFlg1 &= ~static_cast<uint32_t>(daPy_py_c::ERFLG1_UNK_400);
        ++g_wp5.dmgSand;
        logEvery(g_logDmg, 10, 60, "griff: Sandwand-Treffer im Ritt verworfen (Ritt-Frame %u), n=%u", session().rideFrames,
            g_wp5.dmgSand);
    }
    if (link->checkNoResetFlg2(daPy_py_c::FLG2_PRESSED_DAMAGE) != 0) {
        link->offPressedDamage();
        ++g_wp5.dmgPressed;
        logEvery(g_logDmg, 10, 60, "griff: Quetsch-Schaden im Ritt ohne Proc-Wechsel (Ritt-Frame %u), n=%u",
            session().rideFrames, g_wp5.dmgPressed);
    }
    if (link->mDamageTimer != 0 && link->field_0x3100 != 0) {
        link->field_0x3100 = 0;
        ++g_wp5.dmgReverse;
        logEvery(g_logDmg, 10, 60, "griff: Barrieren-Rueckstoss im Ritt verworfen (Ritt-Frame %u), n=%u",
            session().rideFrames, g_wp5.dmgReverse);
    }
}

#if defined(KFF_DEV_AUTOTEST)
void testHitProbe(daAlink_c* link);
void testHitAfter(daAlink_c* link);
#endif

HookAction onDamageActionPre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = argOf<daAlink_c*>(args, 0);
#if defined(KFF_DEV_AUTOTEST)
        testHitProbe(link);
#endif
        if (link == nullptr || unprotected() || !wolfCargoRider(link)) {
            return HOOK_CONTINUE;
        }
        resolveRideDamage(link);
    } catch (...) {
        countException("checkDamageAction");
    }
    return HOOK_CONTINUE;
}

// daAlink_c::procWolfDamageInit(dCcD_GObjInf*), Pre: im Wolfs-Ritt kein Schadens-Proc. Die Aufrufer (checkDamageAction)
// haben das Leben vorher abgezogen; sichtbar bleibt die Kopf-Reaktion wie im Cargo-Modus des Spiels.
HookAction onWolfDamageInitPre(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = argOf<daAlink_c*>(args, 0);
        if (unprotected() || !wolfCargoRider(link)) {
            return HOOK_CONTINUE;
        }
        link->setWolfHeadDamage();
        ++g_wp5.wolfDmgInit;
        logEvery(g_logDmg, 10, 60, "griff: procWolfDamageInit im Ritt uebersprungen (Leben %d, Ritt-Frame %u), n=%u",
            static_cast<int>(dComIfGs_getLife()), session().rideFrames, g_wp5.wolfDmgInit);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("procWolfDamageInit");
        return HOOK_CONTINUE;
    }
}

// daAlink_c::procCoLavaReturnInit(BOOL), Pre: im Ritt keine Lava-/Treibsand-Rueckkehr.
HookAction onLavaReturnPre(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = argOf<daAlink_c*>(args, 0);
        if (unprotected() || !protectedRider(link)) {
            return HOOK_CONTINUE;
        }
        ++g_wp5.lavaSkip;
        logEvery(g_logRestart, 10, kHookLog, "griff: Lava-/Sand-Rueckkehr im Ritt gesperrt (Sand %d, Ritt-Frame %u), n=%u",
            argOf<int>(args, 1), session().rideFrames, g_wp5.lavaSkip);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("procCoLavaReturnInit");
        return HOOK_CONTINUE;
    }
}

// daTagKagoFall_c::execute, Pre (F_SP112 Fluss/Fall): im Ritt eingefroren, solange der Tag noch kein Ereignis
// angefordert hat (Fluss Zustand 0, Fall Zustand 0 oder 1). Link gilt als getragen wie im Spiel (Zaehler 150).
HookAction onTagKagoFallPre(ModContext*, void* args, void* retval, void*) {
    try {
        daTagKagoFall_c* tag = argOf<daTagKagoFall_c*>(args, 0);
        if (tag == nullptr || unprotected() || !protectedRider(liveLink())) {
            return HOOK_CONTINUE;
        }
        const bool river = tag->mActionMode == daTagKagoFall_c::ACTION_MODE_RIVER;
        if ((river && tag->mActionState != 0) || (!river && tag->mActionState > 1)) {
            return HOOK_CONTINUE;  // Ereignis schon angenommen: das Spiel fuehrt es zu Ende, der Ritt endet am Ereignis
        }
        const uint8_t stateBefore = tag->mActionState;
        tag->mActionState = 0;
        tag->mNoCarryTimer = 150;  // wie actionWaitRiver/actionWaitFall bei getragenem Link
        ++g_wp5.riverTagSkip;
        logEvery(g_logTagWp5, 5, kHookLog,
            "griff: Fluss-/Fall-Tag (TagKagoFall, Modus %s, Zustand %u) im Ritt eingefroren (Ritt-Frame %u), n=%u",
            river ? "RIVER" : "FALL", static_cast<unsigned>(stateBefore), session().rideFrames, g_wp5.riverTagSkip);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = TRUE;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        countException("daTagKagoFall_c::execute");
        return HOOK_CONTINUE;
    }
}

// Ruf-Gate (Whitelist): ein englischer Grund, warum Link jetzt nicht rufen darf, oder nullptr. Reihenfolge: Reiten vor
// Verwandlung vor Wasser vor Seil/Kette/Haengen/Klettern vor Enterhaken vor Schaden vor Luft (auf Seil, Leiter oder im
// Wasser hat Link keinen Bodenkontakt; der genauere Grund gewinnt).
bool procIn(unsigned p, unsigned lo, unsigned hi) {
    return p >= lo && p <= hi;
}

const char* wp5GateReason(daAlink_c* link) {
    const unsigned p = link->mProcID;
    // Reiten: Epona (0x3A-0x52), Wildschwein (0x53), Boot (0x8C-0xA1), Spinner (0xA4-0xA5), Board (0xA6-0xAD),
    // Gorone (0xBE), Ziege (0xBF-0xC1, 0xDF).
    if (link->checkRideOn() || link->checkHorseRide() != 0 || link->checkSpinnerRide() != 0 || link->checkRide() != 0 ||
        procIn(p, daAlink_c::PROC_HORSE_RIDE, daAlink_c::PROC_BOAR_RUN) ||
        procIn(p, daAlink_c::PROC_CANOE_RIDE, daAlink_c::PROC_CANOE_KANDELAAR_POUR) ||
        procIn(p, daAlink_c::PROC_SPINNER_READY, daAlink_c::PROC_BOARD_CUT_TURN) ||
        procIn(p, daAlink_c::PROC_GORON_RIDE_WAIT, daAlink_c::PROC_GOAT_STROKE) || p == daAlink_c::PROC_GOAT_STOP_READY) {
        return "riding";
    }
    if (p == daAlink_c::PROC_METAMORPHOSE || p == daAlink_c::PROC_METAMORPHOSE_ONLY) {
        return "transform";
    }
    if (procIn(p, daAlink_c::PROC_SWIM_UP, daAlink_c::PROC_SWIM_DAMAGE) ||
        procIn(p, daAlink_c::PROC_WOLF_SWIM_UP, daAlink_c::PROC_WOLF_SWIM_END_WAIT) ||
        p == daAlink_c::PROC_SWIM_SUBJECTIVITY || link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0 || deepWater(link)) {
        return "water";
    }
    if (procIn(p, daAlink_c::PROC_WOLF_ROPE_MOVE, daAlink_c::PROC_WOLF_ROPE_SUBJECTIVITY)) {
        return "rope";
    }
    if (p == daAlink_c::PROC_CHAIN_UP || p == daAlink_c::PROC_CHAIN_STRONG_PULL || p == daAlink_c::PROC_WOLF_CHAIN_UP ||
        p == daAlink_c::PROC_WOLF_CHAIN_READY || p == daAlink_c::PROC_WOLF_CHAIN_WAIT) {
        return "chain";
    }
    if (procIn(p, daAlink_c::PROC_HANG_START, daAlink_c::PROC_HANG_LEVER_DOWN) ||
        procIn(p, daAlink_c::PROC_ROOF_HANG_START, daAlink_c::PROC_ROOF_SWITCH_HANG) || p == daAlink_c::PROC_MONKEY_MOVE ||
        p == daAlink_c::PROC_BOSS_BODY_HANG || p == daAlink_c::PROC_WOLF_HANG_READY ||
        p == daAlink_c::PROC_WOLF_HANG_WALL_CATCH || p == daAlink_c::PROC_WOLF_HANG_FALL_START ||
        p == daAlink_c::PROC_WOLF_ENEMY_HANG_BITE) {
        return "hang";
    }
    if (procIn(p, daAlink_c::PROC_LADDER_UP_START, daAlink_c::PROC_LADDER_MOVE) ||
        procIn(p, daAlink_c::PROC_CLIMB_UP_START, daAlink_c::PROC_CLIMB_TO_ROOF) || link->checkClimbMove()) {
        return "climb";  // Leiter, Kletterwand, Ranken
    }
    if (procIn(p, daAlink_c::PROC_HOOKSHOT_SUBJECT, daAlink_c::PROC_MAGNE_BOOTS_FLY)) {
        return "hookshot";
    }
    if (procIn(p, daAlink_c::PROC_DAMAGE, daAlink_c::PROC_LAND_DAMAGE) || p == daAlink_c::PROC_GUARD_BREAK ||
        p == daAlink_c::PROC_DK_CAUGHT || p == daAlink_c::PROC_OCTAIEAL_SPIT || p == daAlink_c::PROC_SCREAM_WAIT ||
        procIn(p, daAlink_c::PROC_WOLF_DAMAGE, daAlink_c::PROC_WOLF_SCREAM_WAIT) ||
        p == daAlink_c::PROC_POLY_DAMAGE || p == daAlink_c::PROC_ELEC_DAMAGE ||
        procIn(p, daAlink_c::PROC_CAUGHT, daAlink_c::PROC_SWIM_FREEZE_RETURN) ||
        p == daAlink_c::PROC_LARGE_DAMAGE || p == daAlink_c::PROC_LARGE_DAMAGE_WALL) {
        return "damage";
    }
    if (procIn(p, daAlink_c::PROC_AUTO_JUMP, daAlink_c::PROC_FALL) || p == daAlink_c::PROC_BACK_JUMP ||
        p == daAlink_c::PROC_WOLF_AUTO_JUMP || p == daAlink_c::PROC_WOLF_FALL || p == daAlink_c::PROC_WOLF_TAG_JUMP ||
        !link->mLinkAcch.ChkGroundHit()) {
        return "air";
    }
    return nullptr;
}

// Ruf-Gate mit Zaehler und Log (Aufruf aus griffCallGateReason).
const char* gateEvaluate(daAlink_c* link) {
    const char* why = wp5GateReason(link);
    if (why != nullptr) {
        ++g_wp5.gateRejects;
        logEvery(g_logGate, 20, 30,
            "griff: Ruf-Gate (%s): Proc 0x%x, Reitstatus %u, Boden %d, Wasser %.0f ueber Link, Wolf %d, n=%u", why,
            static_cast<unsigned>(link->mProcID), static_cast<unsigned>(link->mRideStatus),
            link->mLinkAcch.ChkGroundHit() ? 1 : 0, static_cast<double>(link->mWaterY - link->current.pos.y),
            link->checkWolf() != 0 ? 1 : 0, g_wp5.gateRejects);
    }
    return why;
}

// K17: Landeanflug (Kern, B mit Boden darunter) ueber Wasser. Der Kern misst den Bodenabstand ohne Wasser und sinkt bis
// zum Seegrund; ohne Ende an der Oberflaeche schwebt Link bis zur Zeitgrenze (240 Ticks) an der Wasserflaeche. Hier
// endet der Anflug, sobald Links Fuesse kWaterReleaseClear ueber dem Wasser sind: Link faellt ins Wasser und schwimmt.
void waterLandingRelease(Session& s, daKago_c* bird) {
    if (s.landFrames == 0 || noGuard()) {
        return;
    }
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return;
    }
    cXyz start(link->current.pos.x, bird->current.pos.y + 500.0f, link->current.pos.z);
    dBgS_ObjGndChk_Spl spl;
    spl.SetPos(&start);
    const float water = dComIfG_Bgsp().GroundCross(&spl);
    if (water == -G_CM3D_F_INF) {
        return;
    }
    dBgS_GndChk gnd;
    gnd.SetPos(&start);
    const float ground = dComIfG_Bgsp().GroundCross(&gnd);
    if (ground != -G_CM3D_F_INF && ground >= water) {
        return;  // Boden ueber dem Wasser: normales Landen
    }
    const float clear = link->current.pos.y - water;
    if (clear > kWaterReleaseClear) {
        return;
    }
    ++g_wp5.waterRelease;
    logInfo("griff: Landeanflug ueber Wasser: Links Fuesse %.1f ueber der Wasserflaeche (%.1f), Freigabe nach %u Ticks, n=%u",
        static_cast<double>(clear), static_cast<double>(water), s.landFrames, g_wp5.waterRelease);
    dismount(bird, EndReason::DismountButton, "Landeanflug ueber Wasser");  // im Anflug: sofortige Freigabe (Kern)
}

void logWp5Stats(const char* tag) {
    logInfo("griff: WP5 %s staggerSkip=%u landingSkip=%u swimSkip=%u swimBlocked=%u dmgThrow=%u dmgSand=%u dmgPressed=%u "
            "dmgReverse=%u wolfDmgInit=%u lavaSkip=%u riverTagSkip=%u gateRejects=%u "
            "letGo=%u waterRelease=%u exceptions=%u",
        tag, g_wp5.staggerSkip, g_wp5.landingSkip, g_wp5.swimSkip, g_wp5.swimBlocked, g_wp5.dmgThrow, g_wp5.dmgSand,
        g_wp5.dmgPressed, g_wp5.dmgReverse, g_wp5.wolfDmgInit, g_wp5.lavaSkip, g_wp5.riverTagSkip, g_wp5.gateRejects,
        g_wp5.letGo, g_wp5.waterRelease, g_wp5.exceptions);
}

#if defined(KFF_DEV_AUTOTEST)
void testFrameBird(Session& s, daKago_c* bird);
void testFrameLink(Session& s, daAlink_c* link, daKago_c* bird);
void testLinkTop(daAlink_c* link);
void testInstall(Registry& reg, ModResult& worst);
void testLogStats(const char* tag);
#endif

class GriffModule final : public Module {
public:
    const char* name() const override { return "griff"; }

    ModResult install(Registry& reg) override {
        ModResult worst = MOD_OK;
#if defined(KFF_DEV_AUTOTEST)
        g_control = envSet("KFF_GRIFF_AUS");
        g_wp4Off = testFlag("KFF_WP4_AUS");
        g_noGuard = testFlag("KFF_TEST_NOGUARD");
        g_wp5Ohne = testFlag("KFF_WP5_OHNE");
        g_testStagger = testFlag("KFF_TEST_STAGGER");
        if (const char* th = std::getenv("KFF_GRIFF_TESTHIT")) {
            const long long n = std::strtoll(th, nullptr, 10);  // long ist unter Windows nur 32 Bit breit
            g_testHitAt = n > 0 ? static_cast<uint32_t>(n) : 0;
        }
#endif
        const ModResult ev = installEventHooks(reg);  // WP4: Ereignis-Teil
        if (ev != MOD_OK) {
            worst = ev;
        }
#if defined(KFF_DEV_AUTOTEST)
        const ModResult sr = autotest::registerScenario(wp4Scenario());
        if (sr != MOD_OK) {
            logWarn("griff: Testszenario wp4 nicht registriert (%s)", modResultName(sr));  // nur Test: Modul bleibt aktiv
        }
        logInfo("griff: install (Testbuild, Vergleichslauf %d, WP4-Vergleichslauf %d, Wiederanbindung bis %u Frames, "
                "Ereignis-Hooks %s, NOGUARD %d, WP5_OHNE %d, TESTHIT %u, TEST_STAGGER %d)",
            g_control ? 1 : 0, g_wp4Off ? 1 : 0, kWindowFrames, modResultName(ev), g_noGuard ? 1 : 0, g_wp5Ohne ? 1 : 0,
            g_testHitAt, g_testStagger ? 1 : 0);
#else
        logInfo("griff: install (Wiederanbindung bis %u Kern-Ticks, Ereignis-Hooks %s, WP5-Whitelist im Ritt)", kWindowFrames,
            modResultName(ev));
#endif
        // WP5: Zustands-Whitelist im Ritt. Raum-Neustart (startRestartRoom, daTagRetRm_c) besitzt freiheit.
        auto add = [&worst](const char* what, ModResult r) {
            if (r != MOD_OK) {
                logWarn("griff: Hook %s nicht eingetragen (%s)", what, modResultName(r));
                worst = r;
            }
        };
        add("executeStagger", reg.addPre(Hook::KagoExecuteStagger, &onStaggerPre, prio::kGriff));
        add("executeLanding", reg.addPre(Hook::KagoExecuteLanding, &onLandingPre, prio::kGriff,
                                  const_cast<char*>("executeLanding")));
        add("executeLandingLakeHairia", reg.addPre(Hook::KagoExecuteLandingLakeHairia, &onLandingPre, prio::kGriff,
                                            const_cast<char*>("executeLandingLakeHairia")));
        add("executeLandingBoartHouse", reg.addPre(Hook::KagoExecuteLandingBoartHouse, &onLandingPre, prio::kGriff,
                                            const_cast<char*>("executeLandingBoartHouse")));
        add("checkSwimAction", reg.addPre(Hook::LinkCheckSwimAction, &onSwimActionPre, prio::kGriff));
        add("checkDamageAction", reg.addPre(Hook::LinkCheckDamageAction, &onDamageActionPre, prio::kGriff));
        add("procWolfDamageInit", reg.addPre(Hook::LinkProcWolfDamageInit, &onWolfDamageInitPre, prio::kGriff));
        add("procCoLavaReturnInit", reg.addPre(Hook::LinkProcCoLavaReturnInit, &onLavaReturnPre, prio::kGriff));
        add("daTagKagoFall_c::execute", reg.addPre(Hook::TagKagoFallExecute, &onTagKagoFallPre, prio::kGriff));
#if defined(KFF_DEV_AUTOTEST)
        testInstall(reg, worst);
#endif
        return worst;
    }

    void onRideStart(Session& s, daKago_c* bird) override {
        (void)bird;
        g_lostRun = 0;
        ++g_stats.rides;
        logInfo("griff: Ritt %u beginnt (Ritt-Frame %u)%s", g_stats.rides, s.rideFrames,
            controlRun() ? " [Vergleichslauf: ohne Wirkung]" : "");
#if defined(KFF_DEV_AUTOTEST)
        w4NoteStart();
#endif
    }

    void onFrameBird(Session& s, daKago_c* bird) override {
        if (controlRun() || bird == nullptr || s.phase != Phase::Riding) {
            return;
        }
        ++g_stats.birdFrames;
        if (!dComIfGp_event_runCheck() && bird->mAction != daKago_c::ACTION_FLY_e &&
            bird->mAction != daKago_c::ACTION_ATTACK_e) {
            ++g_stats.notFly;
            logEvery(g_logNotFly, 3, 300, "griff: Aktion %d im Ritt nach dem Kern-Zwang (Ritt-Frame %u), n=%u",
                bird->mAction, s.rideFrames, g_stats.notFly);
        }
        if (!noGuard()) {
            keepFlying(bird);
        }
        waterLandingRelease(s, bird);  // K17: Landeanflug ueber Wasser endet an der Wasserflaeche
#if defined(KFF_DEV_AUTOTEST)
        if (s.phase == Phase::Riding) {
            testFrameBird(s, bird);
        }
#endif
    }

    void onFrameLink(Session& s, daAlink_c* link) override {
        // WP4 Sensor (P1): Link-Frame im Ritt, obwohl ein Ereignis oder eine Demo laeuft. Der Kern beendet den Ritt in
        // diesem Fall vor notifyFrameLink (linkExecutePre); jeder Treffer hier hiesse, dass eine Wiederanbindung eine
        // Vanilla-Demo still ueberschreiben koennte.
        if (link != nullptr && s.phase == Phase::Riding &&
            (dComIfGp_event_runCheck() || link->mDemo.getDemoType() != 0)) {
            ++g_ev.overrideFrames;
            logEvery(g_logOverride, 5, 60,
                "griff: link frame in ride with event/demo (event %d, demo type %d, mode %u, ride frame %u), n=%u",
                dComIfGp_event_runCheck() ? 1 : 0, link->mDemo.getDemoType(),
                static_cast<unsigned>(link->mDemo.getDemoMode()), s.rideFrames, g_ev.overrideFrames);
        }
#if defined(KFF_DEV_AUTOTEST)
        testLinkTop(link);  // Testbuild: Test-ATs des Szenarios wp5 (Faelle schaden und pfeil)
#endif
        if (controlRun() || link == nullptr) {
            return;
        }
        daKago_c* bird = liveBird();
        if (s.phase != Phase::Riding || bird == nullptr || s.deleteRequested) {
            // Kein Ritt, Vogel weg oder schon zum Entfernen vorgemerkt: nichts mehr anbinden.
            g_lostRun = 0;
            return;
        }

        if (link->checkDeadHP() || link->mProcID == daAlink_c::PROC_DEAD) {
            ++g_stats.endDeath;
            logInfo("griff: Tod im Ritt (Ritt-Frame %u), Ritt endet sofort", s.rideFrames);
            endRideClean(EndReason::Death, "Tod im Ritt");
            return;
        }
#if defined(KFF_DEV_AUTOTEST)
        testFrameLink(s, link, bird);
#endif

        if (gripHeld(link, bird)) {
            g_lostRun = 0;
            ++g_stats.gripFrames;
            if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
                ++g_stats.procOff;
                logEvery(g_logOff, 3, 300, "griff: Griff ohne Proc 312 (Proc %u, Ritt-Frame %u), n=%u",
                    static_cast<unsigned>(link->mProcID), s.rideFrames, g_stats.procOff);
            }
            return;
        }

        ++g_stats.lostFrames;
        if (dComIfGp_event_runCheck() || link->checkEventRun()) {
            ++g_stats.endEvent;
            logInfo("griff: Ereignis ohne Griff (Ritt-Frame %u), Ritt endet sofort", s.rideFrames);
            endRideClean(EndReason::Event, "Ereignis ohne Griff");
            return;
        }
        if (link->checkWolf() == 0) {
            ++g_stats.endHuman;
            logInfo("griff: Link ist kein Wolf (Ritt-Frame %u), Ritt endet sofort", s.rideFrames);
            requestDelete(EndReason::RiderLost, "kein Wolf");
            return;
        }
        // WP5: Der Vogel hat Link selbst losgelassen (Stagger-Absturz, Landung, Warten), was nur ohne die Griff-Sperren
        // geschehen kann. Keine Wiederanbindung an einen abstuerzenden Vogel: der Ritt endet sofort geordnet.
        if (bird->mAction != daKago_c::ACTION_FLY_e && bird->mAction != daKago_c::ACTION_ATTACK_e) {
            ++g_wp5.letGo;
            logInfo("griff: Vogel hat losgelassen (Aktion %d, Modus %d, Ritt-Frame %u, Proc 0x%x), Ritt endet sofort, n=%u",
                bird->mAction, bird->mMode, s.rideFrames, static_cast<unsigned>(link->mProcID), g_wp5.letGo);
            g_lostRun = 0;
            dismount(bird, EndReason::RiderLost, "Vogel hat losgelassen");
            return;
        }

        if (g_lostRun == 0) {
            g_lostStart = kff::timers::now();  // WP9 (R4): Fenster ab dem ersten Frame ohne Griff, in Kern-Ticks
        }
        ++g_lostRun;
        logEvery(g_logLost, 5, 120,
            "griff: Griff fehlt (Ritt-Frame %u, Folge %u, Proc %u, Modus %d), Wiederanbindung", s.rideFrames,
            g_lostRun, static_cast<unsigned>(link->mProcID), link->mMode);
        if (kff::timers::elapsed(g_lostStart) >= kWindowFrames) {
            ++g_stats.endWindow;
            logInfo("griff: %u Frames ohne Griff (%u Kern-Ticks), geordnetes Absteigen", g_lostRun,
                kff::timers::elapsed(g_lostStart));
            g_lostRun = 0;
            dismount(bird, EndReason::RiderLost, "Griff nicht zu halten");
            return;
        }
        restoreGrip(link, bird);
        ++g_stats.reattach;
        if (!gripHeld(link, bird)) {
            ++g_stats.reattachFail;
            logEvery(g_logRe, 5, 120, "griff: Wiederanbindung ohne Wirkung (Ritt-Frame %u), n=%u", s.rideFrames,
                g_stats.reattachFail);
        }
    }

    void onRideEnd(Session& s, daKago_c* bird, EndReason why) override {
        (void)bird;
        g_lostRun = 0;
        logInfo("griff: Ritt Ende (%s) bei Ritt-Frame %u", endReasonName(why), s.rideFrames);
        logStats("Ende");
        logEvStats("Ende");
#if defined(KFF_DEV_AUTOTEST)
        w4NoteEnd(why);
#endif
        logWp5Stats("Ende");
    }

    void onShutdown(Session& s) override {
        (void)s;
        logStats("SUMMARY");
        logEvStats("SUMMARY");
        logWp5Stats("SUMMARY");
#if defined(KFF_DEV_AUTOTEST)
        testLogStats("SUMMARY");
#endif
    }

#if defined(KFF_DEV_AUTOTEST)
    const autotest::Scenario* autotestScenario() const override;
#endif
};

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------------------------------------
// Testszenario "griff" (nur Testbuild, KFF_AUTOTEST=griff). Kurs ueber KFF_GRIFF_KURS.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kAfterFrames = 600;  // Test: Frames nach dem Ritt, bevor das Szenario endet

enum Kurs : uint8_t { kKursWand, kKursDauer, kKursBoden, kKursDecke, kKursSchlag, kKursKreis, kKursDash };
Kurs g_kurs = kKursWand;

const char* kursName() {
    switch (g_kurs) {
    case kKursWand:
        return "wand";
    case kKursDauer:
        return "dauer";
    case kKursBoden:
        return "boden";
    case kKursDecke:
        return "decke";
    case kKursSchlag:
        return "schlag";
    case kKursKreis:
        return "kreis";
    case kKursDash:
        return "dash";
    }
    return "?";
}

Kurs kursFromEnv() {
    const char* v = std::getenv("KFF_GRIFF_KURS");
    if (v == nullptr) {
        return kKursWand;
    }
    if (std::strcmp(v, "dauer") == 0) return kKursDauer;
    if (std::strcmp(v, "boden") == 0) return kKursBoden;
    if (std::strcmp(v, "decke") == 0) return kKursDecke;
    if (std::strcmp(v, "schlag") == 0) return kKursSchlag;
    if (std::strcmp(v, "kreis") == 0) return kKursKreis;
    if (std::strcmp(v, "dash") == 0) return kKursDash;
    return kKursWand;
}

struct TestState {
    bool callArmed = false;
    uint32_t callStart = 0;
    uint32_t attempts = 0;     // Rufversuche (R halten, B tippen); Mensch-Link verwandelt sich ueber autowolf
    bool rideSeen = false;
    bool bDone = false;
    bool afterStarted = false;
    uint32_t afterT = 0;
    uint32_t contact = 0;  // Frames mit Wandkontakt des Vogels
    bool touchPrev = false;
    uint32_t touches = 0;
    bool staggerDone = false;
    bool offDone = false;
    bool wallPushed = false;  // Test-Platzierung vor eine Wand ist erfolgt
};
TestState g_tt;

// Test-Platzierung (nur Kurs wand, einmal): Ein Linien-Check in Flugrichtung findet die naechste Wand bis 900
// Einheiten. Der Vogel wird 40 Einheiten davor gesetzt, damit er mit Wandkontakt weiterfliegt. Das erzwingt den
// Kontakt, weil das Wandgleiten im Modul freiheit sonst ausweicht.
void wallPush(autotest::Context& c, daKago_c* bird) {
    const float yaw = static_cast<float>(bird->current.angle.y) * (3.14159265f / 32768.0f);
    const float dx = std::sin(yaw);
    const float dz = std::cos(yaw);
    cXyz start(bird->current.pos.x, bird->current.pos.y, bird->current.pos.z);
    cXyz end(start.x + dx * 900.0f, start.y, start.z + dz * 900.0f);
    dBgS_LinChk lin;
    lin.Set(&start, &end, nullptr);
    if (!dComIfG_Bgsp().LineCross(&lin)) {
        return;  // noch keine Wand in 900 Einheiten
    }
    const cXyz hit = lin.GetCross();
    bird->current.pos.x = hit.x - dx * 40.0f;
    bird->current.pos.z = hit.z - dz * 40.0f;
    g_tt.wallPushed = true;
    c.note("griff: TEST Vogel vor eine Wand gesetzt (40 Einheiten davor)");
}

// Stick je Kurs. y > 0: Nase nach unten, y < 0: Nase hoch. x < 0: links, x > 0: rechts.
void kursStick(uint32_t f, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    switch (g_kurs) {
    case kKursBoden:
        if (f < 600) y = 0.8f;  // Sturzflug zum Boden
        break;
    case kKursDecke:
        if (f < 700) y = -1.0f;  // Steigflug unter die Decke
        break;
    case kKursKreis:
        if (f < 900) {
            x = -1.0f;
        } else if (f < 1800) {
            x = 1.0f;
        }
        break;
    default:
        break;  // geradeaus (wand, dauer, schlag)
    }
}

bool kursAbsteigen(uint32_t f) {
    switch (g_kurs) {
    case kKursWand:
        return g_tt.contact >= 300 || f >= 4000;
    case kKursDauer:
        return g_tt.contact >= 2100 || f >= 6000;
    case kKursBoden:
    case kKursDecke:
        return f >= 900;
    case kKursSchlag:
        return f >= 420;
    case kKursKreis:
        return f >= 1900;
    case kKursDash:
        return f >= 500;
    }
    return f >= 2000;
}

bool griffTick(autotest::Context& c) {
    const uint32_t t = c.frame;
    // Ruf nur aus der Ruhe: Link da, kein Vogel, kein Ereignis, Phase Idle. Ein Mensch-Link verwandelt sich dabei
    // ueber das Modul autowolf (onHumanCall) und ruft danach den Vogel. Hoechstens vier Versuche, 300 Frames auseinander.
    const bool calm = c.sess.phase == Phase::Idle && liveLink() != nullptr && liveBird() == nullptr &&
                      !dComIfGp_event_runCheck();
    if (!g_tt.rideSeen && calm && t >= 300 && g_tt.attempts < 4 &&
        (g_tt.attempts == 0 || t >= g_tt.callStart + 300)) {
        ++g_tt.attempts;
        g_tt.callStart = t;
        g_tt.callArmed = true;
        c.note("griff: Ruf-Versuch (R halten, B tippen)");
    }
    if (g_tt.callArmed && !g_tt.rideSeen) {
        if (t >= g_tt.callStart && t < g_tt.callStart + 30) {
            c.holdR();
        }
        if (t == g_tt.callStart + 15) {
            c.tapB();
        }
    }

    if (c.sess.phase == Phase::Riding) {
        if (!g_tt.rideSeen) {
            g_tt.rideSeen = true;
            logInfo("autotest: griff Kurs %s, Ritt beginnt bei Frame %u", kursName(), t);
        }
        const uint32_t f = c.sess.rideFrames;
        daKago_c* bird = liveBird();
        if (bird != nullptr) {
            const bool touch = bird->mObjAcch.ChkWallHit();
            if (touch) {
                ++g_tt.contact;
            }
            if (touch && !g_tt.touchPrev) {
                ++g_tt.touches;
                if (g_tt.touches <= 3) {
                    logInfo("autotest: griff Wandkontakt #%u bei Ritt-Frame %u", g_tt.touches, f);
                }
            }
            g_tt.touchPrev = touch;
        }
        if (g_kurs == kKursWand && bird != nullptr && !g_tt.wallPushed && f >= 120) {
            wallPush(c, bird);
        }
        float x = 0.0f;
        float y = 0.0f;
        kursStick(f, x, y);
        c.stick(x, y);
        if (g_kurs == kKursDash && bird != nullptr) {
            if (f == 150 && !g_tt.offDone) {
                g_tt.offDone = true;
                c.note("griff: TEST Vanilla setFlyAway (Flugabwurf, Modus 3)");
                bird->setFlyAway();
            }
            if (f == 200 || f == 260 || f == 320) {
                c.note("griff: TEST A (Dash)");
                c.tapA();
            }
        }
        if (g_kurs == kKursSchlag && bird != nullptr) {
            if (f == 150 && !g_tt.staggerDone) {
                g_tt.staggerDone = true;
                c.note("griff: TEST Vogel-Aktion STAGGER gesetzt");
                bird->setActionMode(daKago_c::ACTION_STAGGER_e, 0);
            }
            if (f == 300 && !g_tt.offDone) {
                g_tt.offDone = true;
                c.note("griff: TEST Vanilla setRideOff");
                bird->setRideOff();
            }
        }
        if (!g_tt.bDone && kursAbsteigen(f)) {
            g_tt.bDone = true;
            c.tapB();
            c.note("griff: B (Absteigen)");
        }
        return false;
    }

    if (g_tt.rideSeen) {
        if (!g_tt.afterStarted) {
            g_tt.afterStarted = true;
            g_tt.afterT = t;
            c.note("griff: nach dem Ritt");
        }
        if (t >= g_tt.afterT + kAfterFrames) {
            c.note("griff: Zeitplan fertig");
            return true;
        }
    }
    return false;
}

const autotest::Scenario kGriffScenario = {
    "griff",
    "Griff-Test: Kurs per KFF_GRIFF_KURS (wand, dauer, boden, decke, schlag, kreis, dash)",
    griffTick,
};

const autotest::Scenario* GriffModule::autotestScenario() const {
    g_kurs = kursFromEnv();
    return &kGriffScenario;
}
#endif  // KFF_DEV_AUTOTEST (Testszenario griff)

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------------------------------------
// Testszenario "wp4" (nur Testbuild, KFF_AUTOTEST=wp4). Teiltest ueber KFF_WP4: event, skip, linkdemo, demoskip, perch,
// midna, flags, verwandlung, startdialog, tags, howl. Ergebnis je Lauf: "wp4 RESULT <teiltest> ok|fail <grund>".
// Log-Zeilen mit "TEST" stellen einen Ausloeser nach, den ein Spieler im Flug nicht erreicht (Bericht: bekannte Luecken).
// Mit KFF_WP4_AUS=1 laeuft derselbe Ablauf als Vergleichslauf ohne die WP4-Absicherungen (Reproduktion der Risiken).
// ---------------------------------------------------------------------------------------------

enum class W4 : uint8_t {
    Event, Skip, LinkDemo, DemoSkip, Perch, Midna, Flags, Verwandlung, StartDialog, Tags, Howl, SummonEvent, Unknown
};

struct W4Name {
    W4 kind;
    const char* name;
};
constexpr W4Name kW4Names[] = {
    {W4::Event, "event"}, {W4::Skip, "skip"}, {W4::LinkDemo, "linkdemo"}, {W4::DemoSkip, "demoskip"},
    {W4::Perch, "perch"}, {W4::Midna, "midna"}, {W4::Flags, "flags"}, {W4::Verwandlung, "verwandlung"},
    {W4::StartDialog, "startdialog"}, {W4::Tags, "tags"}, {W4::Howl, "howl"}, {W4::SummonEvent, "summonevent"},
};

W4 g_w4Kind = W4::Unknown;
const char* g_w4Name = "(leer)";

constexpr uint32_t kW4Limit = 9000;          // Ticks je Teiltest, danach fail (Zeitlimit)
constexpr u32 kW4ModBirdParam = kModBirdParam;
constexpr u32 kW4ForeignBirdParam = 0x000101FFu;  // Typ 1, kein Pfad, aber nicht der Mod-Parameter (Vanilla-Vogel)
constexpr u32 kW4TagParam = 0x0014C6C5u;     // Tag_Mwait: Schalter an 0xC5, Eintritt 0xC6, Gespraechsradius 20 (200 Einheiten)

// Ritt-Enden (griff onRideEnd) mit Grund und Tick.
struct W4Ends {
    uint32_t n = 0;
    EndReason last = EndReason::Stuck;
    uint32_t tick = 0;
};
W4Ends g_w4End;
uint32_t g_w4Frame = 0;
uint32_t g_logW4 = 0;

void w4NoteEnd(EndReason why) {
    ++g_w4End.n;
    g_w4End.last = why;
    g_w4End.tick = g_w4Frame;
}

// Ritt-Starts (griff onRideStart), davon bei laufendem Ereignis (T3 verbietet den Ritt-Start im Ereignis).
struct W4Starts {
    uint32_t n = 0;
    uint32_t inEvent = 0;
};
W4Starts g_w4Start;

void w4NoteStart() {
    ++g_w4Start.n;
    daAlink_c* link = liveLink();
    if (dComIfGp_event_runCheck() != 0 || (link != nullptr && link->checkEventRun())) {
        ++g_w4Start.inEvent;
        logInfo("autotest: wp4 Ritt-Start bei laufendem Ereignis (Tick %u), n=%u", g_w4Frame, g_w4Start.inEvent);
    }
}

// Konsum von X und Y im Ritt: der Test tippt, griff prueft im eigenen Pad-Post (nach dem Kern-Konsum), ob die Taste
// noch im Pad steht. Danach liest processGameCombos (R+X Heulen, R+Y Schnellverwandlung) denselben Pad.
struct W4Pad {
    bool xPending = false;
    bool yPending = false;
    uint32_t xLeaked = 0;
    uint32_t xConsumed = 0;
    uint32_t yLeaked = 0;
    uint32_t yConsumed = 0;
    uint32_t comboLeaked = 0;  // R und X oder Y gleichzeitig im Pad (Kombination des Spiels moeglich)
};
W4Pad g_w4Pad;

void wp4PadCheck() {
    if (!g_w4Pad.xPending && !g_w4Pad.yPending) {
        return;
    }
    const bool rideInput = isRiding() && !dComIfGp_event_runCheck() && dComIfGp_isPauseFlag() == 0;
    const interface_of_controller_pad& p = mDoCPd_c::getCpadInfo(PAD_1);
    const u32 any = p.mPressedButtonFlags | p.mButtonFlags;
    const bool rHeld = (p.mButtonFlags & PAD_TRIGGER_R) != 0;
    if (g_w4Pad.xPending && rideInput) {
        if ((any & PAD_BUTTON_X) != 0) {
            ++g_w4Pad.xLeaked;
            if (rHeld) {
                ++g_w4Pad.comboLeaked;
            }
        } else {
            ++g_w4Pad.xConsumed;
        }
    }
    if (g_w4Pad.yPending && rideInput) {
        if ((any & PAD_BUTTON_Y) != 0) {
            ++g_w4Pad.yLeaked;
            if (rHeld) {
                ++g_w4Pad.comboLeaked;
            }
        } else {
            ++g_w4Pad.yConsumed;
        }
    }
    g_w4Pad.xPending = false;
    g_w4Pad.yPending = false;
}

struct W4State {
    int stage = 0;
    uint32_t t0 = 0;          // Tick des Stufenbeginns
    uint32_t rideTick = 0;    // Tick des Ritt-Starts
    EvStats ev0;              // Zaehler bei Ritt-Start
    Stats st0;
    int life0 = -1;
    uint32_t endN0 = 0;
    bool bDone = false;
    uint32_t bTick = 0;
    bool earlyEnd = false;
    // Ruf
    bool callOn = false;
    uint32_t callStart = 0;
    uint32_t callTries = 0;
    uint32_t nextCall = 0;
    // Ereignis
    bool evSeen = false;
    uint32_t evStart = 0;
    bool evEnded = false;
    uint32_t evEnd = 0;
    int32_t order1 = -1;
    int32_t order2 = -1;
    uint32_t conflict = 0;
    bool birdBeforeEvent = false;
    // Demo-Skip im Ritt
    bool skip7 = false;
    bool skip9 = false;
    uint32_t cargoLost = 0;
    uint32_t midnaOff = 0;
    uint32_t rideOffNow = 0;
    uint32_t teleported = 0;
    // Skip mit entferntem Vogel
    fpc_ProcID staleId = kNoProc;
    bool skipClearedAtGone = false;
    EvStats evSkip;
    // Linkdemo
    bool cleaned = false;
    // Perch
    bool ownHowl = false;
    fpc_ProcID spawnId[2] = {kNoProc, kNoProc};
    bool spawnHowl[2] = {false, false};
    uint32_t lastHowl = 0;
    // Midna
    int32_t midnaR1 = -1;
    int32_t midnaR2 = -1;
    int linkType2 = -1;
    int midnaType2 = -1;
    bool riding2 = false;
    // Flags
    bool flagFlip = false;
    bool flagSame = false;
    bool before[2] = {false, false};
    uint32_t changedInRide = 0;
    // Verwandlung
    bool prepDone = false;
    uint32_t tries = 0;
    uint32_t tryStart = 0;
    bool humanMeta = false;
    uint32_t humanT0 = 0;
    uint32_t metaInRide = 0;
    bool metaSeen = false;
    uint32_t T0 = 0;
    uint32_t transformEnd0 = 0;
    bool birdsGone = false;
    uint32_t birdsGoneAt = 0;
    bool transformDone = false;
    uint32_t doneAt = 0;
    uint32_t probes = 0;
    // Tags
    fpc_ProcID tagId = kNoProc;
    int tagRoom = -1;
    uint32_t tagPrep = 0;       // Tag direkt vor seinem Execute vor Link gelegt, Wartepunkt auf Midna (w4TagPrep)
    uint32_t midnaTagWait = 0;  // Ritt-Ticks, in denen Midna am Wartepunkt eines Tags haengt (FLG0_TAG_WAIT)
    // Ereignis im Ruf-Fenster (summonevent)
    fpc_ProcID summonId = kNoProc;  // gerufener Vogel beim Ereignis-Auftrag
    uint32_t starts0 = 0;           // Ritt-Starts (g_w4Start) beim Ereignis-Auftrag
    uint32_t startsInEvent0 = 0;
    uint32_t summonSkipped = 0;     // Rufe, deren Phase Summoning der Test nicht sah (Ritt sofort), dann B und neuer Ruf
    bool evReset = false;           // Testaufraeumen: event_reset nach 120 Ticks
    // Tasten
    uint32_t xTaps = 0;
    uint32_t yTaps = 0;
};
W4State g_s;

const u16 kW4Flags[2] = {static_cast<u16>(dSv_event_flag_c::M_067), static_cast<u16>(dSv_event_flag_c::M_077)};
const char* const kW4FlagNames[2] = {"M_067", "M_077"};

int w4CountCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e &&
        static_cast<daKago_c*>(ac)->mType == kFriendlyKagoType && fopAcM_GetParam(ac) == kW4ModBirdParam) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

// Mod-Voegel in der Welt (Parameter des Pakets), unabhaengig von der Sitzung.
uint32_t w4Birds() {
    uint32_t n = 0;
    fopAcIt_Executor(w4CountCb, &n);
    return n;
}

bool w4Done(bool ok, const char* fmt, ...) KFF_PRINTF(2, 3);
bool w4Done(bool ok, const char* fmt, ...) {
    char reason[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(reason, sizeof(reason), fmt, ap);
    va_end(ap);
    logInfo("wp4 RESULT %s %s %s%s", g_w4Name, ok ? "ok" : "fail", reason,
        wp4Control() ? " [Vergleichslauf KFF_WP4_AUS=1]" : "");
    logInfo("autotest: wp4 Tasten xTaps=%u xLeaked=%u xConsumed=%u yTaps=%u yLeaked=%u yConsumed=%u comboLeaked=%u",
        g_s.xTaps, g_w4Pad.xLeaked, g_w4Pad.xConsumed, g_s.yTaps, g_w4Pad.yLeaked, g_w4Pad.yConsumed,
        g_w4Pad.comboLeaked);
    logEvStats("RESULT");
    logStats("RESULT");
    return true;
}

// Ruf aus der Ruhe: R halten (30 Ticks), B-Tipp in der Mitte. Hoechstens 6 Versuche, 240 Ticks auseinander.
void w4Call(autotest::Context& c, uint32_t earliest) {
    const uint32_t t = c.frame;
    if (!g_s.callOn && c.sess.phase == Phase::Idle && t >= earliest && t >= g_s.nextCall && g_s.callTries < 6 &&
        c.canCall()) {
        g_s.callOn = true;
        g_s.callStart = t;
        ++g_s.callTries;
        logInfo("autotest: wp4 %s: Ruf %u (R halten, B tippen) bei Tick %u", g_w4Name, g_s.callTries, t);
    }
    if (g_s.callOn) {
        const uint32_t d = t - g_s.callStart;
        if (d < 30) {
            c.holdR();
        }
        if (d == 15) {
            c.tapB();
        }
        if (d >= 30) {
            g_s.callOn = false;
            g_s.nextCall = t + 240;
        }
    }
}

// Stufe 0 aller Ritt-Teiltests: rufen, bis der Ritt laeuft. true, sobald der Ritt begonnen hat.
bool w4WaitRide(autotest::Context& c, uint32_t earliest) {
    w4Call(c, earliest);
    if (c.sess.phase != Phase::Riding) {
        return false;
    }
    g_s.callOn = false;
    g_s.rideTick = c.frame;
    g_s.ev0 = g_ev;
    g_s.st0 = g_stats;
    g_s.life0 = static_cast<int>(dComIfGs_getLife());
    g_s.endN0 = g_w4End.n;
    logInfo("autotest: wp4 %s: Ritt beginnt bei Tick %u (Leben %d, Versuch %u)", g_w4Name, c.frame, g_s.life0,
        g_s.callTries);
    return true;
}

uint32_t w4RideTicks(const autotest::Context& c) {
    return c.frame - g_s.rideTick;
}

bool w4LifeOk() {
    return static_cast<int>(dComIfGs_getLife()) >= g_s.life0 && dComIfGs_getLife() > 0;
}

// Wirkung des Heulens auf einen Kargarok, wie d_a_alink_wolf.inc:4189-4194 (OnStatus 0x800, setEvent, setKagoPath).
// Pfad 0xFF: setKagoPath laesst mpPath2 unveraendert (null), wie ein Heulen ohne Ruf-Gras-Pfad.
void w4HowlEffect(daKago_c* k, const char* who) {
    fopAcM_OnStatus(k, 0x800);
    k->setEvent();
    k->setKagoPath(0xFF);
    logInfo("autotest: wp4 TEST Heul-Wirkung auf %s Vogel id=0x%x (OnStatus 0x800, setEvent, setKagoPath 0xFF), mpPath2=%s",
        who, static_cast<unsigned>(fopAcM_GetID(k)), k->mpPath2 == nullptr ? "null" : "gesetzt");
}

// WP4.1: Sprech-Auftrag am eigenen Vogel im Ritt, A alle 40 Ticks. Ohne CANTALK nimmt das Spiel den Auftrag nicht an
// (talkCheck); 30 Ticks spaeter setzt der Test CANTALK an Link und Vogel (TEST, im Flug sonst nie). Erwartung (D6): das
// Ereignis startet, der Ritt endet geordnet (Event), das Ereignis endet mit dem Entfernen des Vogels, kein Zombie.
bool w4Event(autotest::Context& c) {
    const uint32_t t = c.frame;
    const bool evt = dComIfGp_event_runCheck() != 0;
    daAlink_c* link = liveLink();
    if (evt && !g_s.evSeen && g_s.order1 >= 0) {
        g_s.evSeen = true;
        g_s.evStart = t;
        logInfo("autotest: wp4 event: Ereignis laeuft ab Tick %u (Phase %s)", t, phaseName(c.sess.phase));
    }
    if (g_s.evSeen && !evt && !g_s.evEnded) {
        g_s.evEnded = true;
        g_s.evEnd = t;
        logInfo("autotest: wp4 event: Ereignis endet bei Tick %u (Dauer %u)", t, t - g_s.evStart);
    }
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        const uint32_t rt = w4RideTicks(c);
        daKago_c* bird = liveBird();
        if (c.sess.phase == Phase::Riding && bird != nullptr) {
            if (rt >= 200 && g_s.order1 < 0) {
                g_s.order1 = fopAcM_orderSpeakEvent(bird, 0, 0);
                logInfo("autotest: wp4 TEST Sprech-Auftrag am eigenen Vogel (ohne CANTALK) -> %d", g_s.order1);
            }
            if (rt >= 230 && g_s.order2 < 0 && link != nullptr) {
                link->eventInfo.onCondition(dEvtCnd_CANTALK_e);
                bird->eventInfo.onCondition(dEvtCnd_CANTALK_e);
                g_s.order2 = fopAcM_orderSpeakEvent(bird, 0, 0);
                logInfo("autotest: wp4 TEST CANTALK an Link und Vogel, Sprech-Auftrag am Vogel -> %d", g_s.order2);
            }
        }
        if (g_s.order1 >= 0 && t % 40 == 0) {
            c.tapA();
        }
        if (c.sess.phase != Phase::Riding) {
            logInfo("autotest: wp4 event: Ritt endet bei Tick %u (Ritt-Ticks %u), Ereignis %d", t, rt, evt ? 1 : 0);
            g_s.stage = 2;
            g_s.t0 = t;
        } else if (rt >= 700) {
            c.tapB();
            logInfo("autotest: wp4 event: kein Ereignis bis Ritt-Tick 700, B");
            g_s.stage = 2;
            g_s.t0 = t;
        }
        return false;
    }
    default:
        if (evt && t % 40 == 0) {
            c.tapA();
        }
        if (t >= g_s.t0 + 900 || (g_s.evEnded && t >= g_s.evEnd + 300)) {
            const uint32_t birds = w4Birds();
            const bool linkFree = link != nullptr && !link->checkEventRun();
            const bool endEvent = g_w4End.n > g_s.endN0 && g_w4End.last == EndReason::Event;
            const bool ok = g_s.evSeen && g_s.evEnded && !evt && endEvent && birds == 0 && linkFree && w4LifeOk();
            return w4Done(ok,
                "Auftrag1=%d Auftrag2=%d Ereignis gesehen=%d beendet=%d (Dauer %u Ticks) laeuft=%d Ritt-Ende=%s "
                "Voegel=%u Link frei=%d Leben %d->%u eventReset/skip im Kern siehe Log",
                g_s.order1, g_s.order2, g_s.evSeen ? 1 : 0, g_s.evEnded ? 1 : 0,
                g_s.evEnded ? g_s.evEnd - g_s.evStart : 0, evt ? 1 : 0,
                g_w4End.n > g_s.endN0 ? endReasonName(g_w4End.last) : "keins", birds, linkFree ? 1 : 0, g_s.life0,
                static_cast<unsigned>(dComIfGs_getLife()));
        }
        return false;
    }
}

// WP4.2: Demo-Skip mit dem eigenen Vogel als Skip-Akteur, Entfernen des Vogels per Testschalter (fopAcM_delete), dann
// zweimal START. Erwartung: "skip cleared on bird delete" (Kern, kagoDeletePre), kein Aufruf von demo_skip, kein Absturz.
bool w4Skip(autotest::Context& c) {
    const uint32_t t = c.frame;
    dEvt_control_c* evt = dComIfGp_getEvent();
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1:
        if (c.sess.phase != Phase::Riding || w4RideTicks(c) >= 100) {
            if (c.sess.phase == Phase::Riding) {
                c.tapB();
            }
            g_s.stage = 2;
            g_s.t0 = t;
        }
        return false;
    case 2:
        if (c.sess.phase == Phase::Dismounting) {
            g_s.stage = 3;
            g_s.t0 = t;
        } else if (t > g_s.t0 + 400) {
            return w4Done(false, "kein Abflug nach B (Phase %s)", phaseName(c.sess.phase));
        }
        return false;
    case 3:
        if (t >= g_s.t0 + 5) {
            daAlink_c* link = liveLink();
            const int r = link != nullptr ? dComIfGp_event_compulsory(link, nullptr, 0xFFFF) : 0;
            logInfo("autotest: wp4 TEST Pflicht-Ereignis fuer Link (Abflug laeuft) -> %d", r);
            g_s.stage = 4;
            g_s.t0 = t;
        }
        return false;
    case 4:
        if (evt->runCheck()) {
            daKago_c* bird = liveBird();
            if (bird == nullptr) {
                return w4Done(false, "Vogel vor dem Skip-Aufbau schon weg (Phase %s)", phaseName(c.sess.phase));
            }
            evt->setSkipProc(bird, daKago_c::DemoSkipCallBack, 0);
            g_w4StaleBird = bird;
            g_s.staleId = fopAcM_GetID(bird);
            g_s.evSkip = g_ev;
            logInfo("autotest: wp4 TEST setSkipProc(Vogel id=0x%x, DemoSkipCallBack, 0), Vogel wird per Testschalter "
                    "entfernt (fopAcM_delete)",
                static_cast<unsigned>(g_s.staleId));
            fopAcM_delete(bird);
            g_s.stage = 5;
            g_s.t0 = t;
        } else if (t > g_s.t0 + 90) {
            return w4Done(false, "Pflicht-Ereignis startet nicht");
        }
        return false;
    case 5:
        if (fopAcM_SearchByID(g_s.staleId) == nullptr) {
            g_s.skipClearedAtGone = evt->mSkipFunc == nullptr && evt->mSkipActorId != g_s.staleId;
            logInfo("autotest: wp4 skip: Vogel weg bei Tick %u, Skip-Rueckruf %s (Akteur 0x%x)", t,
                g_s.skipClearedAtGone ? "geloest" : "steht noch", static_cast<unsigned>(evt->mSkipActorId));
            g_s.stage = 6;
            g_s.t0 = t;
        } else if (t > g_s.t0 + 120) {
            return w4Done(false, "Vogel nach fopAcM_delete nach 120 Ticks noch da");
        }
        return false;
    case 6:
        if (t == g_s.t0 + 10 || t == g_s.t0 + 15) {
            c.tap(PAD_BUTTON_START);
            logInfo("autotest: wp4 skip: START (Skip-Taste) bei Tick %u, Ereignis %d", t, evt->runCheck() ? 1 : 0);
        }
        if (t >= g_s.t0 + 90) {
            const uint32_t calls = g_ev.demoSkipCalls - g_s.evSkip.demoSkipCalls;
            const uint32_t stale = g_ev.skipStalePtr - g_s.evSkip.skipStalePtr;
            const uint32_t noBird = g_ev.skipNoBird - g_s.evSkip.skipNoBird;
            const bool funcLeft = evt->mSkipFunc != nullptr;
            logInfo("autotest: wp4 skip: nach START demo_skip-Aufrufe=%u (Vogelzeiger %u, ohne Vogel %u), "
                    "Rueckruf steht=%d; Aufraeumen: event_reset",
                calls, stale, noBird, funcLeft ? 1 : 0);
            g_s.order1 = static_cast<int32_t>(calls);
            g_s.order2 = static_cast<int32_t>(stale);
            g_s.conflict = noBird;
            g_s.cleaned = funcLeft;
            dComIfGp_event_reset();
            g_s.stage = 7;
            g_s.t0 = t;
        }
        return false;
    default:
        if (t >= g_s.t0 + 150) {
            daAlink_c* link = liveLink();
            const bool ok = g_s.skipClearedAtGone && g_s.order1 == 0 && g_s.order2 == 0 && !g_s.cleaned &&
                            !evt->runCheck() && link != nullptr && w4LifeOk();
            return w4Done(ok,
                "Rueckruf beim Entfernen geloest=%d, demo_skip nach START=%d (mit Vogelzeiger %d, ohne Vogel %u), "
                "Rueckruf danach=%d, Ereignis aufgeraeumt=%d, Link da=%d",
                g_s.skipClearedAtGone ? 1 : 0, g_s.order1, g_s.order2, g_s.conflict, g_s.cleaned ? 1 : 0,
                evt->runCheck() ? 0 : 1, link != nullptr ? 1 : 0);
        }
        return false;
    }
}

// WP4.3: Vanilla-Demo im Ritt (Pflicht-Ereignis und Original-Demo an Link, TEST). Erwartung: keine Wiederanbindung
// ueberschreibt die Demo (Sensor overrideFrames 0), der Ritt endet geordnet mit Grund Event.
bool w4LinkDemo(autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    const bool evt = dComIfGp_event_runCheck() != 0;
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1:
        if (c.sess.phase != Phase::Riding) {
            return w4Done(false, "Ritt endet vor der Demo (Phase %s)", phaseName(c.sess.phase));
        }
        if (w4RideTicks(c) >= 200 && link != nullptr) {
            const int r = dComIfGp_event_compulsory(link, nullptr, 0xFFFF);
            link->changeOriginalDemo();
            link->changeDemoMode(daPy_demo_c::DEMO_UNK_1_e, 0, 0, 0);
            g_s.ev0 = g_ev;
            g_s.endN0 = g_w4End.n;
            logInfo("autotest: wp4 TEST Pflicht-Ereignis (%d) und Original-Demo (Modus %d) an Link im Ritt", r,
                static_cast<int>(daPy_demo_c::DEMO_UNK_1_e));
            g_s.stage = 2;
            g_s.t0 = t;
        }
        return false;
    case 2:
        if (t > g_s.t0 && c.sess.phase == Phase::Riding && link != nullptr &&
            (evt || link->mDemo.getDemoType() != 0)) {
            ++g_s.conflict;
        }
        if (t >= g_s.t0 + 120) {
            dComIfGp_event_reset();
            logInfo("autotest: wp4 linkdemo: event_reset nach 120 Ticks (Phase %s, Demo-Typ %d)",
                phaseName(c.sess.phase), link != nullptr ? link->mDemo.getDemoType() : -1);
            g_s.stage = 3;
            g_s.t0 = t;
        }
        return false;
    default:
        if (!g_s.cleaned && link != nullptr && !evt && link->mDemo.getDemoType() != 0 && t >= g_s.t0 + 60) {
            g_s.cleaned = true;
            logInfo("autotest: wp4 TEST Aufraeumen: Demo-Typ %d an Link bleibt ohne Ereignis, zurueckgesetzt",
                link->mDemo.getDemoType());
            link->mDemo.resetDemoType();
            link->mDemo.setDemoMode(0);
        }
        if (t >= g_s.t0 + 240) {
            const uint32_t over = g_ev.overrideFrames - g_s.ev0.overrideFrames;
            const bool endEvent = g_w4End.n > g_s.endN0 && g_w4End.last == EndReason::Event;
            const uint32_t birds = w4Birds();
            const bool linkFree = link != nullptr && !link->checkEventRun();
            const bool ok = over == 0 && g_s.conflict == 0 && endEvent && birds == 0 && !evt && linkFree && w4LifeOk();
            return w4Done(ok,
                "overrideFrames=%u Ritt-Ticks mit Demo=%u Ritt-Ende=%s Voegel=%u Ereignis=%d Link frei=%d "
                "Aufraeumen noetig=%d Leben %d->%u",
                over, g_s.conflict, g_w4End.n > g_s.endN0 ? endReasonName(g_w4End.last) : "keins", birds, evt ? 1 : 0,
                linkFree ? 1 : 0, g_s.cleaned ? 1 : 0, g_s.life0, static_cast<unsigned>(dComIfGs_getLife()));
        }
        return false;
    }
}

// Nach einem direkten DemoSkipCallBack: hat setRideOff gewirkt (Link ohne Cargo) oder wurde Link teleportiert?
void w4AfterSkip(int param) {
    daAlink_c* link = liveLink();
    daKago_c* bird = liveBird();
    if (link == nullptr || bird == nullptr) {
        return;
    }
    const bool cargo = link->checkCargoCarry() != 0;
    const float dx = link->current.pos.x - bird->current.pos.x;
    const float dz = link->current.pos.z - bird->current.pos.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (!cargo) {
        ++g_s.rideOffNow;
    }
    if (dist > 1500.0f) {
        ++g_s.teleported;
    }
    logInfo("autotest: wp4 demoskip: nach Fall %d Link cargo=%d Abstand Link-Vogel %.0f", param, cargo ? 1 : 0,
        static_cast<double>(dist));
}

// WP4.4: demo_skip am eigenen Vogel im Ritt (Faelle 7 und 9: setRideOff und Spieler-Teleport im Original). Der Skipper ruft
// DemoSkipCallBack(Akteur, Parameter); der Test ruft ihn direkt (TEST). Erwartung: der Ritt bleibt bis B, kein setRideOff
// ausserhalb von endRideClean (Link bleibt im Cargo), keine Wiederanbindung noetig.
bool w4DemoSkip(autotest::Context& c) {
    const uint32_t t = c.frame;
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 2;
            g_s.t0 = t;
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        daAlink_c* link = liveLink();
        daKago_c* bird = liveBird();
        daMidna_c* midna = daPy_py_c::getMidnaActor();
        if (link != nullptr && !link->checkCargoCarry()) {
            ++g_s.cargoLost;
        }
        if (midna != nullptr && bird != nullptr && midna->mpKago != bird) {
            ++g_s.midnaOff;
        }
        if (rt >= 150 && !g_s.skip7 && bird != nullptr) {
            g_s.skip7 = true;
            logInfo("autotest: wp4 TEST DemoSkipCallBack(eigener Vogel, 7) im Ritt");
            daKago_c::DemoSkipCallBack(bird, 7);
            w4AfterSkip(7);
        }
        if (rt >= 250 && !g_s.skip9 && bird != nullptr) {
            g_s.skip9 = true;
            logInfo("autotest: wp4 TEST DemoSkipCallBack(eigener Vogel, 9) im Ritt");
            daKago_c::DemoSkipCallBack(bird, 9);
            w4AfterSkip(9);
        }
        if (rt >= 400 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
            logInfo("autotest: wp4 demoskip: B bei Ritt-Tick %u", rt);
        }
        return false;
    }
    default:
        if (t >= g_s.t0 + 150) {
            const uint32_t lost = g_stats.lostFrames - g_s.st0.lostFrames;
            const uint32_t blocked = g_ev.skipBlocked - g_s.ev0.skipBlocked;
            const uint32_t prevented = g_ev.rideOffPrevented - g_s.ev0.rideOffPrevented;
            const bool endB = g_w4End.n > g_s.endN0 && g_w4End.last == EndReason::DismountButton;
            const bool ok = !g_s.earlyEnd && g_s.rideOffNow == 0 && g_s.teleported == 0 && g_s.cargoLost == 0 &&
                            lost == 0 && blocked == 2 && prevented == 2 && endB && w4LifeOk();
            return w4Done(ok,
                "Ritt bis B=%d Ritt-Ende=%s setRideOff-Wirkung=%u Teleport=%u Ticks ohne Cargo=%u Midna ohne Vogel=%u "
                "griff lostFrames=%u skipBlocked=%u rideOffPrevented=%u Leben %d->%u",
                g_s.earlyEnd ? 0 : 1, g_w4End.n > g_s.endN0 ? endReasonName(g_w4End.last) : "keins", g_s.rideOffNow,
                g_s.teleported, g_s.cargoLost, g_s.midnaOff, lost, blocked, prevented, g_s.life0,
                static_cast<unsigned>(dComIfGs_getLife()));
        }
        return false;
    }
}

// WP4.5: R+X im Ritt (Heulen, vom Kern konsumiert), danach B. Im Abflug Heul-Wirkung auf den eigenen Vogel (TEST, wie
// d_a_alink_wolf.inc:4191-4194 mit Pfad 0xFF), auf einen verwaisten Mod-Vogel (Neuladen der Mod nachgestellt) und auf
// einen Vanilla-Vogel ohne Pfad. Erwartung: "perch ignored: no path" >= 1, kein setEvent im Ritt, kein Absturz, kein
// haengendes Ereignis.
bool w4Perch(autotest::Context& c) {
    const uint32_t t = c.frame;
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 2;
            g_s.t0 = t;
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        if (rt >= 60 && rt <= 360) {
            const uint32_t k = (rt - 60) % 40;
            if (k < 6) {
                c.holdR();
            }
            if (k == 3) {
                c.tap(PAD_BUTTON_X);
                ++g_s.xTaps;
                g_w4Pad.xPending = true;
            }
        }
        c.stick(0.3f, 0.0f);
        if (rt >= 400 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
            logInfo("autotest: wp4 perch: B bei Ritt-Tick %u", rt);
        }
        return false;
    }
    case 2:
        if (c.sess.phase == Phase::Dismounting) {
            g_s.stage = 3;
            g_s.t0 = t;
        } else if (t > g_s.t0 + 300) {
            return w4Done(false, "kein Abflug nach B (Phase %s)", phaseName(c.sess.phase));
        }
        return false;
    case 3: {
        if (t == g_s.t0 + 3 && !g_s.ownHowl) {
            if (daKago_c* bird = liveBird()) {
                g_s.ownHowl = true;
                w4HowlEffect(bird, "eigenen");
                g_s.lastHowl = t;
            }
        }
        if (t == g_s.t0 + 10) {
            daAlink_c* link = liveLink();
            if (link != nullptr) {
                const float s = cM_ssin(link->shape_angle.y);
                const float co = cM_scos(link->shape_angle.y);
                const int room = static_cast<int>(fopAcM_GetRoomNo(link));
                const u32 params[2] = {kW4ModBirdParam, kW4ForeignBirdParam};
                for (int i = 0; i < 2; ++i) {
                    const float side = i == 0 ? -300.0f : 300.0f;
                    const cXyz pos(link->current.pos.x + s * 500.0f + co * side, link->current.pos.y + 300.0f,
                        link->current.pos.z + co * 500.0f - s * side);
                    const csXyz angle(0, link->shape_angle.y, 0);
                    g_s.spawnId[i] = fopAcM_create(fpcNm_KAGO_e, params[i], &pos, room, &angle, nullptr, -1);
                    logInfo("autotest: wp4 TEST %s Vogel angelegt (Parameter 0x%08x) -> id=0x%x",
                        i == 0 ? "verwaister Mod-" : "Vanilla-", static_cast<unsigned>(params[i]),
                        static_cast<unsigned>(g_s.spawnId[i]));
                }
            }
        }
        for (int i = 0; i < 2; ++i) {
            if (g_s.spawnId[i] == kNoProc || g_s.spawnHowl[i] || t < g_s.t0 + 40) {
                continue;
            }
            fopAc_ac_c* a = fopAcM_SearchByID(g_s.spawnId[i]);
            if (a != nullptr && fopAcM_GetName(a) == fpcNm_KAGO_e) {
                g_s.spawnHowl[i] = true;
                w4HowlEffect(static_cast<daKago_c*>(a), i == 0 ? "verwaisten Mod-" : "Vanilla-");
                g_s.lastHowl = t;
            }
        }
        const bool allHowled = g_s.spawnHowl[0] && g_s.spawnHowl[1];
        if (t >= g_s.t0 + 40 && t >= g_s.lastHowl + 150 && (allHowled || t >= g_s.t0 + 300)) {
            for (int i = 0; i < 2; ++i) {
                if (g_s.spawnId[i] != kNoProc && fopAcM_SearchByID(g_s.spawnId[i]) != nullptr) {
                    fopAcM_delete(g_s.spawnId[i]);
                }
            }
            logInfo("autotest: wp4 perch: Testvoegel entfernt (Heul-Wirkung %d/%d/%d)", g_s.ownHowl ? 1 : 0,
                g_s.spawnHowl[0] ? 1 : 0, g_s.spawnHowl[1] ? 1 : 0);
            g_s.stage = 4;
            g_s.t0 = t;
        }
        return false;
    }
    default:
        if (t >= g_s.t0 + 120) {
            const uint32_t perch = g_ev.perchIgnored - g_s.ev0.perchIgnored;
            const uint32_t setEv = g_ev.setEventInRide - g_s.ev0.setEventInRide;
            const uint32_t howl = g_ev.howlInRide - g_s.ev0.howlInRide;
            const bool evt = dComIfGp_event_runCheck() != 0;
            const uint32_t birds = w4Birds();
            const bool ok = !g_s.earlyEnd && perch >= 1 && setEv == 0 && howl == 0 && !evt && g_w4Pad.xLeaked == 0 &&
                            g_s.ownHowl && g_s.spawnHowl[0] && g_s.spawnHowl[1] && birds == 0 && w4LifeOk();
            return w4Done(ok,
                "perchIgnored=%u (Skript abgefangen %u, Vogel-Ereignis beendet %u) setEvent im Ritt=%u Heulen im Ritt=%u "
                "R+X im Ritt=%u (X durchgelassen %u) Heul-Wirkung eigen/verwaist/vanilla=%d/%d/%d Ereignis=%d Mod-Voegel=%u",
                perch, g_ev.scriptNeutral - g_s.ev0.scriptNeutral, g_ev.birdEventEnded - g_s.ev0.birdEventEnded, setEv,
                howl, g_s.xTaps, g_w4Pad.xLeaked, g_s.ownHowl ? 1 : 0, g_s.spawnHowl[0] ? 1 : 0,
                g_s.spawnHowl[1] ? 1 : 0, evt ? 1 : 0, birds);
        }
        return false;
    }
}

// WP4.6: Midna-Auftrag im Ritt wie d_a_midna.cpp:3330-3346 (event_id 4): orderPotentialEvent(Midna, 0x400), Link
// changeOriginalDemo + changeDemoMode(0x3a), Midna changeOriginalDemo + changeDemoMode(0xf), dazu ein Gespraechsauftrag
// Link-Midna (TEST, ohne das vorausgehende Midna-Gespraech). Erwartung: abgelehnt (0), Demo-Modus zurueckgenommen, Ritt bleibt.
bool w4Midna(autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1:
        if (c.sess.phase != Phase::Riding) {
            return w4Done(false, "Ritt endet vor dem Midna-Auftrag (Phase %s)", phaseName(c.sess.phase));
        }
        if (w4RideTicks(c) >= 200 && link != nullptr) {
            if (midna == nullptr) {
                return w4Done(false, "keine Midna im Ritt");
            }
            g_s.ev0 = g_ev;
            g_s.midnaR1 = fopAcM_orderPotentialEvent(midna, 0x400, 0xffff, 1);
            link->changeOriginalDemo();
            link->changeDemoMode(0x3a, 0, 0, 0);
            const bool midnaDemo = midna->mpModel != nullptr && !midna->checkNoDraw();
            if (midnaDemo) {
                midna->changeOriginalDemo();
                midna->changeDemoMode(0xf);
            }
            g_s.midnaR2 = fopAcM_orderTalkEvent(link, midna, 0, 0);
            logInfo("autotest: wp4 TEST Midna-Auftrag (event_id 4) im Ritt: orderPotentialEvent -> %d, orderTalkEvent -> "
                    "%d, Link-Demo 0x3a, Midna-Demo %s",
                g_s.midnaR1, g_s.midnaR2, midnaDemo ? "0xf" : "(nicht sichtbar, kein Modus)");
            g_s.stage = 2;
            g_s.t0 = t;
        }
        return false;
    case 2:
        if (t == g_s.t0 + 2) {
            g_s.linkType2 = link != nullptr ? link->mDemo.getDemoType() : -1;
            g_s.midnaType2 = midna != nullptr ? static_cast<int>(midna->mDemoType) : -1;
            g_s.riding2 = c.sess.phase == Phase::Riding;
            logInfo("autotest: wp4 midna: 2 Ticks danach Link-Demo-Typ %d Modus %u, Midna-Demo-Typ %d Modus %u, Phase %s",
                g_s.linkType2, link != nullptr ? static_cast<unsigned>(link->mDemo.getDemoMode()) : 0u,
                g_s.midnaType2, midna != nullptr ? static_cast<unsigned>(midna->mDemoMode) : 0u,
                phaseName(c.sess.phase));
        }
        if (c.sess.phase != Phase::Riding && !g_s.bDone) {
            g_s.earlyEnd = true;
            g_s.bDone = true;
            g_s.bTick = t;
            logInfo("autotest: wp4 midna: Ritt endet vor B (Phase %s)", phaseName(c.sess.phase));
        }
        if (c.sess.phase == Phase::Riding && w4RideTicks(c) >= 500 && !g_s.bDone) {
            g_s.bDone = true;
            g_s.bTick = t;
            c.tapB();
        }
        if (g_s.bDone && t >= g_s.bTick + 150) {
            const uint32_t refused = g_ev.midnaOrderRefused - g_s.ev0.midnaOrderRefused;
            const uint32_t reset = g_ev.midnaDemoReset - g_s.ev0.midnaDemoReset;
            const bool evt = dComIfGp_event_runCheck() != 0;
            const bool ok = refused >= 1 && g_s.midnaR1 == 0 && g_s.midnaR2 == 0 && g_s.linkType2 == 0 &&
                            g_s.midnaType2 == 0 && g_s.riding2 && !g_s.earlyEnd && !evt && w4LifeOk();
            return w4Done(ok,
                "midnaOrderRefused=%u Rueckgaben %d/%d Demo zurueck=%u Link-Demo-Typ=%d Midna-Demo-Typ=%d Ritt danach=%d "
                "Ritt bis B=%d Ereignis=%d",
                refused, g_s.midnaR1, g_s.midnaR2, reset, g_s.linkType2, g_s.midnaType2, g_s.riding2 ? 1 : 0,
                g_s.earlyEnd ? 0 : 1, evt ? 1 : 0);
        }
        return false;
    default:
        return false;
    }
}

// Ein Story-Flag umschalten und zuruecklesen. true, wenn es sich geaendert hat (dann wird es zurueckgesetzt).
bool w4FlipFlag(int i, bool to) {
    const u16 flag = kW4Flags[i];
    const bool before = dComIfGs_isEventBit(flag) != 0;
    if (to) {
        dComIfGs_onEventBit(flag);
    } else {
        dComIfGs_offEventBit(flag);
    }
    const bool after = dComIfGs_isEventBit(flag) != 0;
    if (after != before) {
        if (before) {
            dComIfGs_onEventBit(flag);
        } else {
            dComIfGs_offEventBit(flag);
        }
    }
    return after != before;
}

// WP4.7: M_067 und M_077 im Ritt aendern (on und off, je zweimal). Erwartung: unveraendert, "story flag blocked" je Flag;
// nach dem Ritt wirkt dieselbe Aenderung (M_077, sofort zurueck).
bool w4Flags(autotest::Context& c) {
    const uint32_t t = c.frame;
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 2;
            g_s.t0 = t;
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        if (rt >= 150 && !g_s.flagFlip) {
            g_s.flagFlip = true;
            g_s.ev0 = g_ev;
            for (int i = 0; i < 2; ++i) {
                g_s.before[i] = dComIfGs_isEventBit(kW4Flags[i]) != 0;
                const bool changed = w4FlipFlag(i, !g_s.before[i]);
                g_s.changedInRide += changed ? 1 : 0;
                logInfo("autotest: wp4 TEST %s im Ritt umschalten (%d -> %d): %s", kW4FlagNames[i], g_s.before[i] ? 1 : 0,
                    g_s.before[i] ? 0 : 1, changed ? "GEAENDERT (zurueckgesetzt)" : "unveraendert");
            }
        }
        if (rt >= 160 && !g_s.flagSame) {
            g_s.flagSame = true;
            for (int i = 0; i < 2; ++i) {
                const bool changed1 = w4FlipFlag(i, g_s.before[i]);
                const bool changed2 = w4FlipFlag(i, !g_s.before[i]);
                g_s.changedInRide += (changed1 ? 1 : 0) + (changed2 ? 1 : 0);
                logInfo("autotest: wp4 TEST %s im Ritt erneut setzen und umschalten: %s/%s", kW4FlagNames[i],
                    changed1 ? "GEAENDERT" : "unveraendert", changed2 ? "GEAENDERT" : "unveraendert");
            }
        }
        if (rt >= 300 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
        }
        return false;
    }
    default:
        if (t >= g_s.t0 + 60) {
            const uint32_t blocked = g_ev.storyBlocked - g_s.ev0.storyBlocked;
            // Ausserhalb des Ritts wirkt dieselbe Aenderung (Gegenprobe), sofort zurueck.
            const bool m077 = dComIfGs_isEventBit(kW4Flags[1]) != 0;
            const bool outside = w4FlipFlag(1, !m077);
            const bool same0 = (dComIfGs_isEventBit(kW4Flags[0]) != 0) == g_s.before[0];
            const bool same1 = (dComIfGs_isEventBit(kW4Flags[1]) != 0) == g_s.before[1];
            logInfo("autotest: wp4 flags: ausserhalb des Ritts (Phase %s) M_077 umschalten: %s", phaseName(c.sess.phase),
                outside ? "wirkt (zurueckgesetzt)" : "wirkt NICHT");
            const bool ok = g_s.flagFlip && g_s.flagSame && g_s.changedInRide == 0 && blocked >= 6 && outside && same0 &&
                            same1 && !g_s.earlyEnd;
            return w4Done(ok,
                "Aenderungen im Ritt=%u storyBlocked=%u (erwartet 6) ausserhalb wirksam=%d Endwerte M_067=%d M_077=%d "
                "(Anfang %d/%d) Ritt bis B=%d",
                g_s.changedInRide, blocked, outside ? 1 : 0, dComIfGs_isEventBit(kW4Flags[0]) ? 1 : 0,
                dComIfGs_isEventBit(kW4Flags[1]) ? 1 : 0, g_s.before[0] ? 1 : 0, g_s.before[1] ? 1 : 0,
                g_s.earlyEnd ? 0 : 1);
        }
        return false;
    }
}

void w4TransformDiag(const char* tag) {
    daAlink_c* link = liveLink();
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    logInfo("autotest: wp4 verwandlung %s: wolf=%d proc=%u ground=%d midnaRide=%d M_067=%d M_077=%d Zwielicht=%d "
            "MidnaEnable=%d event=%d phase=%s",
        tag, link != nullptr && link->checkWolf() ? 1 : 0, link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u,
        link != nullptr && link->mLinkAcch.ChkGroundHit() ? 1 : 0, link != nullptr && link->checkMidnaRide() ? 1 : 0,
        dComIfGs_isEventBit(kW4Flags[0]) ? 1 : 0, dComIfGs_isEventBit(kW4Flags[1]) ? 1 : 0,
        (g_env_light.mEvilInitialized & 0x80) != 0 ? 1 : 0,
        midna != nullptr && midna->checkMetamorphoseEnableBase() ? 1 : 0, dComIfGp_event_runCheck() ? 1 : 0,
        phaseName(session().phase));
}

// R+Y nach Plan: R halten fuer 8 Ticks ab start, Y-Tipp bei start+4.
void w4RY(autotest::Context& c, uint32_t start, bool countPad) {
    const uint32_t d = c.frame - start;
    if (d < 8) {
        c.holdR();
    }
    if (d == 4) {
        c.tap(PAD_BUTTON_Y);
        ++g_s.yTaps;
        if (countPad) {
            g_w4Pad.yPending = true;
        }
    }
}

// R+B-Probe waehrend der Verwandlung: der Kern muss den Ruf mit "transforming" abweisen.
void w4ProbeCall(autotest::Context& c, uint32_t start) {
    const uint32_t d = c.frame - start;
    if (d < 12) {
        c.holdR();
    }
    if (d == 6) {
        c.tapB();
        ++g_s.probes;
        logInfo("autotest: wp4 verwandlung: R+B-Probe bei Tick %u (proc %u)", c.frame,
            liveLink() != nullptr ? static_cast<unsigned>(liveLink()->mProcID) : 0u);
    }
}

// WP4.8: Verwandlung ueber den Spielweg (Schnellverwandlung R+Y, Einstellung enableQuickTransform) mit eigenem Vogel.
// Vorbereitung: M_067 und M_077 ausserhalb des Ritts setzen (TEST, leerer Spielstand). Mensch-Start (F_SP103): R+Y zum
// Wolf, waehrend der Verwandlung eine R+B-Probe, dann Ruf. Im Ritt R+Y dreimal (kein Effekt, E4). Nach B im Abflug R+Y
// alle 6 Ticks, bis Link sich verwandelt. Erwartung: Ritt-Ende Transform vor dem Proc, Voegel 0 in <= 60 Ticks,
// "call rejected (transforming)" >= 1, Verwandlung fertig in <= 300 Ticks.
bool w4Verwandlung(autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    const bool meta = link != nullptr && link->mProcID == daAlink_c::PROC_METAMORPHOSE;
    switch (g_s.stage) {
    case 0:
        if (t >= 100 && link != nullptr) {
            for (int i = 0; i < 2; ++i) {
                if (dComIfGs_isEventBit(kW4Flags[i]) == 0) {
                    dComIfGs_onEventBit(kW4Flags[i]);
                    logInfo("autotest: wp4 TEST Vorbereitung %s gesetzt (ausserhalb des Ritts)", kW4FlagNames[i]);
                }
            }
            w4TransformDiag("Start");
            g_s.stage = link->checkWolf() ? 20 : 10;
            g_s.t0 = t + 60;
        }
        return false;
    case 10:  // Mensch -> Wolf ueber R+Y
        if (!g_s.humanMeta && t >= g_s.t0 && (t - g_s.t0) % 40 == 0) {
            ++g_s.tries;
            g_s.tryStart = t;
            if (g_s.tries % 4 == 1) {
                w4TransformDiag("R+Y Mensch");
            }
        }
        if (!g_s.humanMeta && g_s.tries > 0) {
            w4RY(c, g_s.tryStart, false);
        }
        if (meta && !g_s.humanMeta) {
            g_s.humanMeta = true;
            g_s.humanT0 = t;
            logInfo("autotest: wp4 verwandlung: Mensch -> Wolf per R+Y startet bei Tick %u", t);
        }
        if (g_s.humanMeta) {
            if (t >= g_s.humanT0 + 10) {
                w4ProbeCall(c, g_s.humanT0 + 10);
            }
            if (!meta && link != nullptr && link->checkWolf() && t > g_s.humanT0 + 30) {
                logInfo("autotest: wp4 verwandlung: Wolf nach %u Ticks", t - g_s.humanT0);
                g_s.stage = 20;
                g_s.t0 = t + 30;
            }
        } else if (g_s.tries > 12) {
            w4TransformDiag("Abbruch");
            return w4Done(false, "Mensch -> Wolf per R+Y nicht moeglich (12 Versuche, Bedingungen siehe Log)");
        }
        return false;
    case 20:  // Ruf als Wolf
        if (w4WaitRide(c, g_s.t0)) {
            g_s.stage = 21;
            g_s.transformEnd0 = g_ev.transformEnd;
        }
        return false;
    case 21: {  // Ritt: R+Y dreimal, dann B
        if (meta) {
            ++g_s.metaInRide;
        }
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 22;
            g_s.t0 = t;
            w4TransformDiag("Ritt-Ende");
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        const uint32_t ryAt[3] = {60u, 90u, 120u};
        for (uint32_t k : ryAt) {
            if (rt >= k && rt < k + 8) {
                w4RY(c, g_s.rideTick + k, true);
            }
        }
        if (rt >= 200 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
        }
        return false;
    }
    case 22: {  // Abflug: R+Y alle 6 Ticks, bis die Verwandlung startet
        if (meta && !g_s.metaSeen) {
            g_s.metaSeen = true;
            g_s.T0 = t;
            logInfo("autotest: wp4 verwandlung: Verwandlung im Spielweg startet bei Tick %u (Phase %s, Mod-Voegel %u, "
                    "transformEnd %u)",
                t, phaseName(c.sess.phase), w4Birds(), g_ev.transformEnd - g_s.transformEnd0);
        }
        if (!g_s.metaSeen) {
            c.holdR();
            if ((t - g_s.t0) % 6 == 3) {
                c.tap(PAD_BUTTON_Y);
                ++g_s.yTaps;
            }
            if ((t - g_s.t0) % 60 == 0) {
                w4TransformDiag("R+Y Abflug");
            }
            if (t > g_s.t0 + 400) {
                w4TransformDiag("Abbruch");
                return w4Done(false, "keine Verwandlung im Spielweg nach dem Ritt (400 Ticks R+Y), Bedingungen siehe Log; "
                                     "Ritt-R+Y Verwandlungen im Ritt=%u",
                    g_s.metaInRide);
            }
            return false;
        }
        if (t >= g_s.T0 + 10) {
            w4ProbeCall(c, g_s.T0 + 10);
        }
        if (!g_s.birdsGone && w4Birds() == 0) {
            g_s.birdsGone = true;
            g_s.birdsGoneAt = t;
        }
        if (!g_s.transformDone && link != nullptr && !meta && link->checkWolf() == 0) {
            g_s.transformDone = true;
            g_s.doneAt = t;
            logInfo("autotest: wp4 verwandlung: Mensch nach %u Ticks", t - g_s.T0);
        }
        if (t >= g_s.T0 + 320 || (g_s.transformDone && g_s.birdsGone && t >= g_s.T0 + 60)) {
            const uint32_t ends = g_ev.transformEnd - g_s.transformEnd0;
            const uint32_t goneIn = g_s.birdsGone ? g_s.birdsGoneAt - g_s.T0 : 9999;
            const uint32_t doneIn = g_s.transformDone ? g_s.doneAt - g_s.T0 : 9999;
            const bool ok = ends >= 1 && g_s.birdsGone && goneIn <= 60 && g_s.transformDone && doneIn <= 300 &&
                            g_s.metaInRide == 0 && g_w4Pad.yLeaked == 0 && g_s.probes >= 1 && !g_s.earlyEnd &&
                            w4LifeOk();
            return w4Done(ok,
                "Ritt-Ende Transform=%u Voegel weg nach %u Ticks Verwandlung fertig nach %u Ticks R+B-Proben=%u "
                "(Log: call rejected (transforming)) Verwandlungen im Ritt=%u R+Y im Ritt Y durchgelassen=%u "
                "Mensch-Start=%d Ritt bis B=%d",
                ends, goneIn, doneIn, g_s.probes, g_s.metaInRide, g_w4Pad.yLeaked, g_s.humanMeta ? 1 : 0,
                g_s.earlyEnd ? 0 : 1);
        }
        return false;
    }
    default:
        return false;
    }
}

// WP4.9: Ruf im Startdialog (F_SP124). R halten und B alle 2 Ticks ab dem ersten Link-Frame, bis das Ereignis laeuft,
// dann A alle 30 Ticks bis zum Ende. Erwartung: Vogel und Ereignis nie gleichzeitig aktiv (nur nach einer Ende-Zeile).
bool w4StartDialog(autotest::Context& c) {
    const uint32_t t = c.frame;
    const bool evt = dComIfGp_event_runCheck() != 0;
    const bool active = c.sess.phase == Phase::Summoning || c.sess.phase == Phase::Riding;
    if (evt && active) {
        ++g_s.conflict;
        logEvery(g_logW4, 5, 60, "autotest: wp4 startdialog: Vogel aktiv und Ereignis bei Tick %u (Phase %s)", t,
            phaseName(c.sess.phase));
    }
    if (!g_s.evSeen && c.sess.birdId != kNoProc) {
        g_s.birdBeforeEvent = true;
    }
    switch (g_s.stage) {
    case 0:
        c.holdR();
        if (t % 2 == 0) {
            c.tapB();
        }
        if (evt) {
            g_s.evSeen = true;
            g_s.evStart = t;
            logInfo("autotest: wp4 startdialog: Ereignis ab Tick %u, Vogel vorher %d, Phase %s", t,
                g_s.birdBeforeEvent ? 1 : 0, phaseName(c.sess.phase));
            g_s.stage = 1;
        } else if (t > 1800) {
            return w4Done(false, "kein Ereignis in 1800 Ticks (Startdialog erwartet), Vogel %d", g_s.birdBeforeEvent ? 1 : 0);
        }
        return false;
    case 1:
        if (evt) {
            // Ruf-Kombination im Ereignis weiter anbieten (alle 8 Ticks), A fuer den Dialog alle 30 Ticks.
            if ((t - g_s.evStart) % 8 < 4) {
                c.holdR();
            }
            if ((t - g_s.evStart) % 8 == 2) {
                c.tapB();
            } else if ((t - g_s.evStart) % 30 == 15) {
                c.tapA();
            }
        }
        if (!evt && !g_s.evEnded) {
            g_s.evEnded = true;
            g_s.evEnd = t;
            logInfo("autotest: wp4 startdialog: Ereignis endet bei Tick %u (Dauer %u)", t, t - g_s.evStart);
        }
        if ((g_s.evEnded && t >= g_s.evEnd + 300) || t > g_s.evStart + 6000) {
            const bool ok = g_s.evSeen && g_s.conflict == 0;
            return w4Done(ok,
                "Ereignis gesehen=%d beendet=%d (Dauer %u) Ticks mit Vogel und Ereignis=%u Vogel vor dem Ereignis=%d "
                "Phase am Ende=%s (Log: call rejected (event))",
                g_s.evSeen ? 1 : 0, g_s.evEnded ? 1 : 0, g_s.evEnded ? g_s.evEnd - g_s.evStart : 0, g_s.conflict,
                g_s.birdBeforeEvent ? 1 : 0, phaseName(c.sess.phase));
        }
        return false;
    default:
        return false;
    }
}

// Lage des Test-Tags (TEST, nur Szenario wp4 tags): direkt vor seinem Execute (Pre-Hook oben) 150 Einheiten vor Link und
// 50 darunter, Wartepunkt auf Midnas aktueller Position. So gelten im Execute Midnas Abstand (< 5) und Links Abstand
// (< 200, Gespraechsradius) unabhaengig von der Reihenfolge der Akteure im Frame.
void w4TagPrep(fopAc_ac_c* a) {
    if (g_w4Kind != W4::Tags || a == nullptr || g_s.tagId == kNoProc || fopAcM_GetID(a) != g_s.tagId || g_s.bDone ||
        fopAcM_GetName(a) != fpcNm_Tag_Mwait_e) {
        return;
    }
    daAlink_c* link = liveLink();
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (link == nullptr || midna == nullptr) {
        return;
    }
    daTagMwait_c* tag = static_cast<daTagMwait_c*>(a);
    const float s = cM_ssin(link->shape_angle.y);
    const float co = cM_scos(link->shape_angle.y);
    tag->current.pos.set(link->current.pos.x + s * 150.0f, link->current.pos.y - 50.0f, link->current.pos.z + co * 150.0f);
    tag->mWaitPosition = midna->current.pos;
    ++g_s.tagPrep;
}

// WP4.10: Tag_Mwait im Ritt (TEST: Tag 150 Einheiten vor Link, Wartepunkt an Midna, Schalter an; Lage per w4TagPrep). Ohne
// Absicherung bietet er CANTALK/TALK an und setzt Midna an den Wartepunkt (FLG0_TAG_WAIT). Erwartung: kein CANTALK, kein
// Sprech-Auftrag eines Tags, Midna nie am Wartepunkt, Ritt bleibt bis B.
bool w4Tags(autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 2;
            g_s.t0 = t;
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        if (link == nullptr) {
            return false;
        }
        if (rt >= 100 && g_s.tagRoom < 0) {
            const float s = cM_ssin(link->shape_angle.y);
            const float co = cM_scos(link->shape_angle.y);
            const cXyz ahead(link->current.pos.x + s * 150.0f, link->current.pos.y - 50.0f,
                link->current.pos.z + co * 150.0f);
            g_s.tagRoom = static_cast<int>(fopAcM_GetRoomNo(link));
            dComIfGs_offSwitch(0xC6, g_s.tagRoom);
            dComIfGs_onSwitch(0xC5, g_s.tagRoom);
            const csXyz angle(1, 0, 0);
            const cXyz scale(1.0f, 10.0f, 1.0f);
            g_s.tagId = fopAcM_create(fpcNm_Tag_Mwait_e, kW4TagParam, &ahead, g_s.tagRoom, &angle, &scale, -1);
            logInfo("autotest: wp4 TEST Tag_Mwait im Ritt angelegt (Parameter 0x%08x, Raum %d) -> id=0x%x",
                static_cast<unsigned>(kW4TagParam), g_s.tagRoom, static_cast<unsigned>(g_s.tagId));
        }
        daMidna_c* midna = daPy_py_c::getMidnaActor();
        if (midna != nullptr && midna->checkStateFlg0(daMidna_c::FLG0_TAG_WAIT) != 0) {
            ++g_s.midnaTagWait;
            logEvery(g_logW4, 5, 60, "autotest: wp4 tags: Midna am Wartepunkt eines Tags im Ritt (Ritt-Tick %u), n=%u", rt,
                g_s.midnaTagWait);
        }
        if (rt >= 400 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
            if (g_s.tagId != kNoProc && fopAcM_SearchByID(g_s.tagId) != nullptr) {
                fopAcM_delete(g_s.tagId);
            }
            if (g_s.tagRoom >= 0) {
                dComIfGs_offSwitch(0xC5, g_s.tagRoom);
                dComIfGs_offSwitch(0xC6, g_s.tagRoom);
            }
            const bool tagWait = midna != nullptr && midna->checkStateFlg0(daMidna_c::FLG0_TAG_WAIT) != 0;
            if (tagWait) {
                midna->offTagWaitPos();  // Testaufraeumen: der geloeschte Tag gibt Midna sonst nie frei
            }
            logInfo("autotest: wp4 tags: B, Tag entfernt, Schalter aus, Midna-Wartepunkt %s (Tag-Lage %u Frames gesetzt)",
                tagWait ? "geloest" : "nicht gesetzt", g_s.tagPrep);
        }
        return false;
    }
    default:
        if (t >= g_s.t0 + 120) {
            const uint32_t canTalk = g_ev.tagCanTalk - g_s.ev0.tagCanTalk;
            const uint32_t speak = g_ev.tagSpeak - g_s.ev0.tagSpeak;
            const uint32_t paused = g_ev.tagPaused - g_s.ev0.tagPaused;
            const bool endB = g_w4End.n > g_s.endN0 && g_w4End.last == EndReason::DismountButton;
            const bool ok = g_s.tagId != kNoProc && g_s.tagPrep > 0 && canTalk == 0 && speak == 0 && paused > 0 &&
                            g_s.midnaTagWait == 0 && !g_s.earlyEnd && endB && w4LifeOk();
            return w4Done(ok,
                "Tag angelegt=%d Tag-Lage gesetzt=%u CANTALK/TALK im Ritt=%u Sprech-Auftraege von Tags=%u Midna am "
                "Wartepunkt=%u Ticks Tag ausgesetzt=%u Frames Ritt bis B=%d Ritt-Ende=%s",
                g_s.tagId != kNoProc ? 1 : 0, g_s.tagPrep, canTalk, speak, g_s.midnaTagWait, paused,
                g_s.earlyEnd ? 0 : 1, g_w4End.n > g_s.endN0 ? endReasonName(g_w4End.last) : "keins");
        }
        return false;
    }
}

// WP4.11: R+X im Ritt alle 30 Ticks (Heulen, Ziel waere der eigene Vogel). Erwartung: X konsumiert, kein setEvent am Vogel,
// kein Heul-Proc im Ritt, kein Absturz.
bool w4Howl(autotest::Context& c) {
    const uint32_t t = c.frame;
    switch (g_s.stage) {
    case 0:
        if (w4WaitRide(c, 300)) {
            g_s.stage = 1;
        }
        return false;
    case 1: {
        if (c.sess.phase != Phase::Riding) {
            g_s.earlyEnd = !g_s.bDone;
            g_s.stage = 2;
            g_s.t0 = t;
            return false;
        }
        const uint32_t rt = w4RideTicks(c);
        if (rt >= 60 && rt <= 400) {
            const uint32_t k = (rt - 60) % 30;
            if (k < 6) {
                c.holdR();
            }
            if (k == 3) {
                c.tap(PAD_BUTTON_X);
                ++g_s.xTaps;
                g_w4Pad.xPending = true;
            }
        }
        if (rt >= 450 && !g_s.bDone) {
            g_s.bDone = true;
            c.tapB();
        }
        return false;
    }
    default:
        if (t >= g_s.t0 + 120) {
            const uint32_t setEv = g_ev.setEventInRide - g_s.ev0.setEventInRide;
            const uint32_t howl = g_ev.howlInRide - g_s.ev0.howlInRide;
            const bool ok = !g_s.earlyEnd && setEv == 0 && howl == 0 && g_w4Pad.xLeaked == 0 && g_s.xTaps >= 10 &&
                            g_w4Pad.xConsumed >= 10 && w4LifeOk();
            return w4Done(ok, "R+X im Ritt=%u X konsumiert=%u X durchgelassen=%u setEvent am Vogel im Ritt=%u Heul-Proc im "
                              "Ritt=%u Ritt bis B=%d",
                g_s.xTaps, g_w4Pad.xConsumed, g_w4Pad.xLeaked, setEv, howl, g_s.earlyEnd ? 0 : 1);
        }
        return false;
    }
}

// WP4.9 (Ruf-Fenster, T3/D6): Ereignis zwischen Ruf und Aufsitzen. Sobald die Sitzung in Phase Summoning ist (Vogel gerufen,
// noch nicht im Ritt), ordnet der Test ein Pflicht-Ereignis an Link an (TEST, wie linkdemo). Erwartung: kein Ritt-Start im
// Ereignis, der gerufene Vogel wird geordnet entfernt (Log "ride ended clean (Event, Ereignis vor dem Ritt-Start)"), Vogel
// und Ereignis nie gleichzeitig aktiv, kein Mod-Vogel mehr spaetestens 60 Ticks nach dem Ereignis-Start, das Ereignis endet
// (Testaufraeumen: event_reset nach 120 Ticks), Link frei.
bool w4SummonEvent(autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    const bool evt = dComIfGp_event_runCheck() != 0;
    if (g_s.stage >= 1) {
        const bool active = c.sess.phase == Phase::Summoning || c.sess.phase == Phase::Riding;
        if (evt && !g_s.evSeen) {
            g_s.evSeen = true;
            g_s.evStart = t;
            logInfo("autotest: wp4 summonevent: Ereignis laeuft ab Tick %u (Phase %s, Mod-Voegel %u)", t,
                phaseName(c.sess.phase), w4Birds());
        }
        if (evt && active) {
            ++g_s.conflict;
            logEvery(g_logW4, 5, 60, "autotest: wp4 summonevent: Vogel aktiv und Ereignis bei Tick %u (Phase %s), n=%u", t,
                phaseName(c.sess.phase), g_s.conflict);
        }
        fopAc_ac_c* summoned = nullptr;
        const bool summonAlive = g_s.summonId != kNoProc && fopAcM_SearchByID(g_s.summonId, &summoned) != 0;
        if (g_s.evSeen && !g_s.birdsGone && !summonAlive && w4Birds() == 0) {
            g_s.birdsGone = true;
            g_s.birdsGoneAt = t;
            logInfo("autotest: wp4 summonevent: gerufener Vogel weg bei Tick %u (%u Ticks nach Ereignis-Start, Phase %s)", t,
                t - g_s.evStart, phaseName(c.sess.phase));
        }
        if (g_s.evSeen && !evt && !g_s.evEnded) {
            g_s.evEnded = true;
            g_s.evEnd = t;
            logInfo("autotest: wp4 summonevent: Ereignis endet bei Tick %u (Dauer %u, Phase %s)", t, t - g_s.evStart,
                phaseName(c.sess.phase));
        }
    }
    switch (g_s.stage) {
    case 0:
        w4Call(c, 300);
        if (c.sess.phase == Phase::Riding && !g_s.bDone) {
            // Ritt ohne sichtbare Phase Summoning (Vogel im selben Tick aufgesessen): absteigen, neuer Ruf.
            g_s.bDone = true;
            ++g_s.summonSkipped;
            c.tapB();
            logInfo("autotest: wp4 summonevent: Phase Summoning nicht gesehen (Ruf %u), B und neuer Ruf", g_s.callTries);
        } else if (c.sess.phase == Phase::Idle) {
            g_s.bDone = false;
        }
        if (c.sess.phase == Phase::Summoning && link != nullptr) {
            g_s.summonId = c.sess.birdId;
            g_s.life0 = static_cast<int>(dComIfGs_getLife());
            g_s.ev0 = g_ev;
            g_s.st0 = g_stats;
            g_s.endN0 = g_w4End.n;
            g_s.starts0 = g_w4Start.n;
            g_s.startsInEvent0 = g_w4Start.inEvent;
            fopAc_ac_c* a = nullptr;
            const bool known = fopAcM_SearchByID(g_s.summonId, &a) != 0;
            g_s.order1 = dComIfGp_event_compulsory(link, nullptr, 0xFFFF);
            logInfo("autotest: wp4 TEST Pflicht-Ereignis an Link in Phase Summoning (Ruf %u, Tick %u, Vogel 0x%x %s) -> %d",
                g_s.callTries, t, static_cast<unsigned>(g_s.summonId),
                !known ? "unbekannt" : (a == nullptr ? "im Anlegen" : "angelegt"), g_s.order1);
            g_s.callOn = false;
            g_s.bDone = false;
            g_s.stage = 1;
            g_s.t0 = t;
        }
        return false;
    default:
        if (!g_s.evSeen && t >= g_s.t0 + 90) {
            return w4Done(false, "Pflicht-Ereignis startet nicht (Auftrag %d, Phase %s)", g_s.order1,
                phaseName(c.sess.phase));
        }
        if (evt && !g_s.evReset && t >= g_s.t0 + 120) {
            g_s.evReset = true;
            dComIfGp_event_reset();
            logInfo("autotest: wp4 summonevent: event_reset nach 120 Ticks (Testaufraeumen, Phase %s, Mod-Voegel %u)",
                phaseName(c.sess.phase), w4Birds());
        }
        if (!g_s.cleaned && g_s.evEnded && link != nullptr && !evt && link->mDemo.getDemoType() != 0 &&
            t >= g_s.evEnd + 60) {
            g_s.cleaned = true;
            logInfo("autotest: wp4 TEST Aufraeumen: Demo-Typ %d an Link bleibt ohne Ereignis, zurueckgesetzt",
                link->mDemo.getDemoType());
            link->mDemo.resetDemoType();
            link->mDemo.setDemoMode(0);
        }
        if (c.sess.phase == Phase::Riding && !evt && !g_s.bDone) {
            // Vergleichslauf: der Vogel sitzt nach dem Ereignis auf. Aufraeumen: absteigen.
            g_s.bDone = true;
            c.tapB();
            logInfo("autotest: wp4 summonevent: Ritt nach dem Ereignis bei Tick %u, B", t);
        }
        if ((g_s.evEnded && t >= g_s.evEnd + 300) || t >= g_s.t0 + 1200) {
            const uint32_t starts = g_w4Start.n - g_s.starts0;
            const uint32_t startsInEvent = g_w4Start.inEvent - g_s.startsInEvent0;
            const uint32_t birds = w4Birds();
            const bool linkFree = link != nullptr && !link->checkEventRun();
            const int goneAfter = g_s.birdsGone ? static_cast<int>(g_s.birdsGoneAt - g_s.evStart) : -1;
            const bool ok = g_s.order1 > 0 && g_s.evSeen && g_s.evEnded && !evt && g_s.conflict == 0 && starts == 0 &&
                            startsInEvent == 0 && g_s.birdsGone && goneAfter <= 60 && birds == 0 &&
                            c.sess.phase == Phase::Idle && linkFree && w4LifeOk();
            return w4Done(ok,
                "Auftrag=%d Ereignis gesehen=%d beendet=%d (Dauer %u) Ticks mit Vogel und Ereignis=%u Ritt-Starts "
                "danach=%u davon im Ereignis=%u Vogel weg nach %d Ticks Voegel=%u Phase=%s Link frei=%d Aufraeumen "
                "noetig=%d Summoning verpasst=%u Leben %d->%u (Log: ride ended clean (Event, Ereignis vor dem Ritt-Start))",
                g_s.order1, g_s.evSeen ? 1 : 0, g_s.evEnded ? 1 : 0, g_s.evEnded ? g_s.evEnd - g_s.evStart : 0,
                g_s.conflict, starts, startsInEvent, goneAfter, birds, phaseName(c.sess.phase), linkFree ? 1 : 0,
                g_s.cleaned ? 1 : 0, g_s.summonSkipped, g_s.life0, static_cast<unsigned>(dComIfGs_getLife()));
        }
        return false;
    }
}

bool wp4Tick(autotest::Context& c) {
    g_w4Frame = c.frame;
    if (c.frame == 1) {
        logInfo("autotest: wp4 Teiltest %s%s", g_w4Name, wp4Control() ? " (Vergleichslauf KFF_WP4_AUS=1)" : "");
    }
    if (c.frame >= kW4Limit) {
        return w4Done(false, "Zeitlimit %u Ticks (Stufe %d, Phase %s)", kW4Limit, g_s.stage, phaseName(c.sess.phase));
    }
    switch (g_w4Kind) {
    case W4::Event:
        return w4Event(c);
    case W4::Skip:
        return w4Skip(c);
    case W4::LinkDemo:
        return w4LinkDemo(c);
    case W4::DemoSkip:
        return w4DemoSkip(c);
    case W4::Perch:
        return w4Perch(c);
    case W4::Midna:
        return w4Midna(c);
    case W4::Flags:
        return w4Flags(c);
    case W4::Verwandlung:
        return w4Verwandlung(c);
    case W4::StartDialog:
        return w4StartDialog(c);
    case W4::Tags:
        return w4Tags(c);
    case W4::Howl:
        return w4Howl(c);
    case W4::SummonEvent:
        return w4SummonEvent(c);
    case W4::Unknown:
        break;
    }
    logError("autotest: wp4 unbekannter Teiltest '%s' (KFF_WP4)", g_w4Name);
    return true;
}

const autotest::Scenario kWp4Scenario = {
    "wp4",
    "WP4: Ereignisse, Skip, Midna, Story-Flags, Verwandlung, Heulen (Teiltest per KFF_WP4)",
    wp4Tick,
};

const autotest::Scenario* wp4Scenario() {
    const char* v = std::getenv("KFF_WP4");
    g_w4Kind = W4::Unknown;
    g_w4Name = v != nullptr ? v : "(leer)";
    if (v != nullptr) {
        for (const W4Name& e : kW4Names) {
            if (std::strcmp(v, e.name) == 0) {
                g_w4Kind = e.kind;
                g_w4Name = e.name;
            }
        }
    }
    return &kWp4Scenario;
}
#endif  // KFF_DEV_AUTOTEST (Testszenario wp4)

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------------------------------------
// WP5-Testteil (nur Testbuild): Test-AT an Link (KFF_GRIFF_TESTHIT, Fall schaden), erzwungener Stagger (KFF_TEST_STAGGER),
// Beleg des Szenentyp-Waechters, Ruf-Gate-Proben und das Szenario "wp5" (KFF_AUTOTEST=wp5, Fall ueber KFF_WP5).
// Ergebniszeilen "wp5 RESULT <fall> ok|fail ...", Foto-Marken "wp5shot: <name>" (der Testrunner fotografiert das Fenster).
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kTestHitEvery = 90;  // Abstand der Test-ATs im Ritt (Ritt-Frames)
constexpr uint32_t kTestHitCount = 3;   // hoechstens so viele Test-ATs im Ritt (Leben reicht auch mit 3 Herzen)

struct Wp5Test {
    uint32_t tgHit = 0;             // TG-Treffer an Link (checkDamageAction, Pre)
    uint32_t tgHitRide = 0;         // davon im Ritt
    uint32_t testAt = 0;            // gesetzte Test-ATs (Ritt und Boden)
    uint32_t testAtRide = 0;        // davon im Ritt
    uint32_t lastAtRf = 0xFFFFFFFFu;
    bool hitPending = false;        // Treffer gesehen, der Post-Hook loggt die Folge
    int lifeBefore = 0;
    unsigned procBefore = 0;
    uint32_t procChangeOnHit = 0;   // Treffer im Ritt, nach denen Link nicht mehr in Proc 312 war
    uint32_t staggerForced = 0;     // KFF_TEST_STAGGER: gesetzte STAGGER
    uint32_t lastForceRf = 0xFFFFFFFFu;
    bool groundHitArmed = false;    // Fall schaden: ein Test-AT an Link ausserhalb des Ritts (naechster Link-Frame)
    bool birdHitArmed = false;      // Fall pfeil: ein Pfeil-AT am Vogel (naechster Link-Frame)
    bool birdAtCheck = false;       // Fall pfeil: im Link-Frame danach pruefen, ob der Vogel-AT etwas getroffen hat
    int birdAtHit = -1;             // -1 ungeprueft, 0 kein Treffer, 1 Treffer
    int birdAtHitName = -1;         // Profilname des Getroffenen (falls Treffer)
    bool riverProbe = false;        // Fall szenentyp: Fluss-Bedingungen am Vogel gesetzt
    uint32_t sceneGuardSeen = 0;    // Vogel-Execute mit mSceneType 3 nach dem Kern (Waechter hat gewirkt)
    uint32_t sceneGuardMiss = 0;    // Vogel-Execute mit mSceneType != 3 (Waechter fehlt)
    // Ruf-Gate-Probe: Proc und Reitstatus gelten nur fuer den einen Gate-Aufruf und werden sofort zurueckgesetzt.
    bool probeArmed = false;
    const char* probeLabel = "";
    int probeProc = -1;
    int probeRide = -1;
    uint32_t probeDone = 0;
    const char* probeWhy = nullptr;
    bool probeRestored = false;
    // Letztes Ergebnis des Ruf-Gates (jeder Aufruf aus callBlockReason), fuer die Auswertung im Szenario.
    uint32_t gateCalls = 0;
    const char* lastWhy = nullptr;
};
Wp5Test g_wt;
uint32_t g_logScene = 0;

dCcD_Stts* g_testStts = nullptr;  // einmal angelegt, nie freigegeben (nur Testbuild)
dCcD_Cyl* g_testCyl = nullptr;

// Test-AT: Pfeiltyp, atp 2, AT-SPrm 0x5 (gesetzt, Gruppe 0x4 = trifft den Spieler), Zylinder r 60, h 150.
const dCcD_SrcCyl kTestAtSrc = {
    {
        {0x0, {{AT_TYPE_ARROW, 0x2, 0x5}, {0x0, 0x0}, 0x0}},  // mObj
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                   // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                   // mGObjTg
        {0x0},                                                // mGObjCo
    },
    {
        {{0.0f, 0.0f, 0.0f}, 60.0f, 150.0f}  // mCyl
    }
};

void testPlaceAtPos(fopAc_ac_c* owner, const cXyz& c) {
    if (g_testCyl == nullptr) {
        g_testStts = new dCcD_Stts();
        g_testCyl = new dCcD_Cyl();
    }
    g_testStts->Init(0xFF, 0, owner);
    g_testCyl->Set(kTestAtSrc);
    g_testCyl->ResetAtHit();  // Treffer-Merker des vorigen Test-ATs loeschen (Fall pfeil liest ChkAtHit danach)
    g_testCyl->SetStts(g_testStts);
    g_testCyl->StartCAt(const_cast<cXyz&>(c));
    dComIfG_Ccsp()->Set(g_testCyl);
}

void testPlaceAt(daAlink_c* link, fopAc_ac_c* owner, const char* where) {
    if (link == nullptr || owner == nullptr) {
        return;
    }
    const cXyz c(link->current.pos.x, link->current.pos.y - 30.0f, link->current.pos.z);
    testPlaceAtPos(owner, c);
    ++g_wt.testAt;
    logInfo("griff: TEST AT (Pfeil, atp 2, Gruppe 0x4, Halter %d) an Link gesetzt (%s, Ritt-Frame %u, Proc 0x%x, Leben %d), "
            "n=%u",
        static_cast<int>(fopAcM_GetName(owner)), where, session().rideFrames, static_cast<unsigned>(link->mProcID),
        static_cast<int>(dComIfGs_getLife()), g_wt.testAt);
}

// daAlink_c::checkDamageAction, Pre (vor resolveRideDamage): TG-Treffer an Link zaehlen.
void testHitProbe(daAlink_c* link) {
    if (link == nullptr || link != liveLink()) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        if (link->mTgCyls[i].ChkTgHit() == 0) {
            continue;
        }
        fopAc_ac_c* src = link->mTgCyls[i].GetTgHitAc();
        const bool ride = rideActive();
        ++g_wt.tgHit;
        if (ride) {
            ++g_wt.tgHitRide;
        }
        g_wt.hitPending = true;
        g_wt.lifeBefore = static_cast<int>(dComIfGs_getLife());
        g_wt.procBefore = link->mProcID;
        logInfo("griff: TGHIT #%u an Link (Zylinder %d, Quelle %d, Ritt %d, Ritt-Frame %u, Proc 0x%x, Leben %d, Timer %d)",
            g_wt.tgHit, i, src != nullptr ? static_cast<int>(fopAcM_GetName(src)) : -1, ride ? 1 : 0,
            session().rideFrames, g_wt.procBefore, g_wt.lifeBefore, static_cast<int>(link->mDamageTimer));
        return;
    }
}

// daAlink_c::checkDamageAction, Post: Folge des Treffers (Proc nach dem Original).
void testHitAfter(daAlink_c* link) {
    if (!g_wt.hitPending || link == nullptr || link != liveLink()) {
        return;
    }
    g_wt.hitPending = false;
    const bool ride = rideActive();
    if (ride && link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
        ++g_wt.procChangeOnHit;
    }
    logInfo("griff: TGHIT-Folge: Leben %d -> %d (Abzug kann einen Frame spaeter kommen), Proc 0x%x -> 0x%x, Ritt %d",
        g_wt.lifeBefore, static_cast<int>(dComIfGs_getLife()), g_wt.procBefore, static_cast<unsigned>(link->mProcID),
        ride ? 1 : 0);
}

void onDamageActionTestPost(ModContext*, void* args, void*, void*) {
    try {
        testHitAfter(argOf<daAlink_c*>(args, 0));
    } catch (...) {
        countException("checkDamageAction (Test-Post)");
    }
}

// Link-Frame im Ritt (onFrameLink): Test-ATs ab Ritt-Frame KFF_GRIFF_TESTHIT, alle kTestHitEvery Frames.
void testFrameLink(Session& s, daAlink_c* link, daKago_c* bird) {
    if (g_testHitAt == 0 || g_wt.testAtRide >= kTestHitCount) {
        return;
    }
    const uint32_t rf = s.rideFrames;
    if (rf < g_testHitAt || (rf - g_testHitAt) % kTestHitEvery != 0 || rf == g_wt.lastAtRf) {
        return;
    }
    g_wt.lastAtRf = rf;
    ++g_wt.testAtRide;
    testPlaceAt(link, bird, "Ritt");
}

// Jeder Link-Frame (Anfang von onFrameLink): Test-AT an Link (Fall schaden am Boden, Fall pfeil im Ritt) und am Vogel
// (Fall pfeil), Halter Midna (sonst kein AT).
void testLinkTop(daAlink_c* link) {
    if (g_wt.birdAtCheck && g_testCyl != nullptr) {
        // Ergebnis des Vogel-ATs aus dem letzten Kollisionsdurchlauf.
        g_wt.birdAtCheck = false;
        const bool hit = g_testCyl->ChkAtHit() != 0;
        fopAc_ac_c* hitAc = hit ? g_testCyl->GetAtHitAc() : nullptr;
        g_wt.birdAtHit = hit ? 1 : 0;
        g_wt.birdAtHitName = hitAc != nullptr ? static_cast<int>(fopAcM_GetName(hitAc)) : -1;
        logInfo("griff: TEST Pfeil-AT am Vogel: Treffer %d (Ziel %d)", g_wt.birdAtHit, g_wt.birdAtHitName);
    }
    if (g_wt.birdHitArmed) {
        g_wt.birdHitArmed = false;
        daKago_c* bird = liveBird();
        fopAc_ac_c* owner = daPy_py_c::getMidnaActor();
        if (bird != nullptr && owner != nullptr) {
            // Ueber dem Vogel (Rumpfmitte +100): weit weg von Link, der ~200 unter dem Vogel haengt.
            const cXyz c(bird->current.pos.x, bird->current.pos.y + 100.0f, bird->current.pos.z);
            testPlaceAtPos(owner, c);
            ++g_wt.testAt;
            g_wt.birdAtCheck = true;
            logInfo("griff: TEST AT (Pfeil, atp 2, Halter %d) am Vogel gesetzt (Ritt-Frame %u, Vogel-Aktion %d, Link y %.0f, "
                    "Vogel y %.0f)",
                static_cast<int>(fopAcM_GetName(owner)), session().rideFrames, bird->mAction,
                static_cast<double>(link != nullptr ? link->current.pos.y : 0.0f), static_cast<double>(bird->current.pos.y));
        } else {
            logInfo("griff: TEST Vogel-AT uebersprungen (Vogel %d, Midna %d)", bird != nullptr ? 1 : 0, owner != nullptr ? 1 : 0);
        }
    }
    if (!g_wt.groundHitArmed || link == nullptr) {
        return;
    }
    g_wt.groundHitArmed = false;
    fopAc_ac_c* owner = daPy_py_c::getMidnaActor();
    if (owner == nullptr) {
        logInfo("griff: TEST Boden-AT ohne Halter (keine Midna), uebersprungen");
        return;
    }
    testPlaceAt(link, owner, rideActive() ? "Ritt, Halter Midna" : "Boden");
}

// Vogel-Frame im Ritt (nach keepRideState, vor action()): KFF_TEST_STAGGER erzwingt STAGGER bei Ritt-Frame 150, 300,
// 450, 600 (Modus 0, 1, 0, 3). Mit Griff-Sperre ueberspringt onStaggerPre executeStagger im selben Execute.
void testFrameBird(Session& s, daKago_c* bird) {
    if (!g_testStagger || bird == nullptr) {
        return;
    }
    const uint32_t rf = s.rideFrames;
    int mode = -1;
    if (rf == 150 || rf == 450) {
        mode = 0;
    } else if (rf == 300) {
        mode = 1;
    } else if (rf == 600) {
        mode = 3;
    }
    if (mode < 0 || rf == g_wt.lastForceRf) {
        return;
    }
    g_wt.lastForceRf = rf;
    ++g_wt.staggerForced;
    logInfo("griff: TEST STAGGER erzwungen (Modus %d, Ritt-Frame %u, Aktion vorher %d), n=%u", mode, rf, bird->mAction,
        g_wt.staggerForced);
    bird->setActionMode(daKago_c::ACTION_STAGGER_e, mode);
}

// daKago_c::execute, Pre (nach dem Kern): Beleg des Szenentyp-Waechters, solange der Fall szenentyp die Fluss-Bedingungen
// gesetzt hat (WAIT, 6df 0, lastMode 10, mSceneType 1 vor dem Kern).
HookAction onKagoExecuteTestPre(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (!g_wt.riverProbe || k == nullptr || !isOwnBird(k)) {
            return HOOK_CONTINUE;
        }
        if (k->mSceneType == 3) {
            ++g_wt.sceneGuardSeen;
            logEvery(g_logScene, 5, 30,
                "griff: TEST mSceneType guard: Szenentyp %d vor dem Vogel-Execute (Kern), Aktion %d, 6df %u, lastMode %u, "
                "n=%u",
                k->mSceneType, k->mAction, static_cast<unsigned>(k->field_0x6df), dComIfGs_getLastSceneMode(),
                g_wt.sceneGuardSeen);
        } else {
            ++g_wt.sceneGuardMiss;
            logEvery(g_logScene, 5, 30,
                "griff: TEST mSceneType guard FEHLT: Szenentyp %d vor dem Vogel-Execute, Aktion %d, 6df %u, n=%u",
                k->mSceneType, k->mAction, static_cast<unsigned>(k->field_0x6df), g_wt.sceneGuardMiss);
        }
    } catch (...) {
        countException("daKago_c::execute (Test-Pre)");
    }
    return HOOK_CONTINUE;
}

bool testGateProbeArmed() {
    return g_wt.probeArmed;
}

// Gate-Probe: Proc und Reitstatus nur fuer diesen Aufruf setzen, das Gate auswerten, sofort zuruecksetzen.
const char* testGateProbe(daAlink_c* link) {
    g_wt.probeArmed = false;
    const uint16_t procSaved = link->mProcID;
    const uint8_t rideSaved = link->mRideStatus;
    if (g_wt.probeProc >= 0) {
        link->mProcID = static_cast<uint16_t>(g_wt.probeProc);
    }
    if (g_wt.probeRide >= 0) {
        link->mRideStatus = static_cast<uint8_t>(g_wt.probeRide);
    }
    const char* why = nullptr;
    try {
        why = gateEvaluate(link);
    } catch (...) {
        why = nullptr;
    }
    link->mProcID = procSaved;
    link->mRideStatus = rideSaved;
    g_wt.probeRestored = link->mProcID == procSaved && link->mRideStatus == rideSaved;
    ++g_wt.probeDone;
    g_wt.probeWhy = why;
    g_wt.lastWhy = why;
    ++g_wt.gateCalls;
    logInfo("griff: TEST Gate-Probe %s (Proc 0x%x, Reitstatus %d): Grund %s, danach Proc 0x%x Reitstatus %u, n=%u",
        g_wt.probeLabel, static_cast<unsigned>(g_wt.probeProc >= 0 ? g_wt.probeProc : procSaved), g_wt.probeRide,
        why != nullptr ? why : "(keiner)", static_cast<unsigned>(link->mProcID), static_cast<unsigned>(link->mRideStatus),
        g_wt.probeDone);
    return why;
}

const char* testGateRecord(const char* why) {
    ++g_wt.gateCalls;
    g_wt.lastWhy = why;
    return why;
}

void testLogStats(const char* tag) {
    logInfo("griff: WP5TEST %s tgHit=%u tgHitRide=%u testAt=%u testAtRide=%u procChangeOnHit=%u staggerForced=%u "
            "sceneGuardSeen=%u sceneGuardMiss=%u gateCalls=%u probeDone=%u",
        tag, g_wt.tgHit, g_wt.tgHitRide, g_wt.testAt, g_wt.testAtRide, g_wt.procChangeOnHit, g_wt.staggerForced,
        g_wt.sceneGuardSeen, g_wt.sceneGuardMiss, g_wt.gateCalls, g_wt.probeDone);
}

// ---------------------------------------------------------------------------------------------
// Szenario "wp5"
// ---------------------------------------------------------------------------------------------

enum class W5Case : uint8_t {
    Wasser,      // 5.1/5.1b/5.2 F_SP115: Schweben und Tauchen ueber Wasser, Landeanflug ueber Wasser, Ruf im Wasser
    Seil,        // 5.3 F_SP121: Gate-Proben Seil/Kette/Haengen/Klettern, echter Fall mit Ruf
    Treffer,     // 5.4 F_SP121: Test-ATs im Ritt (KFF_GRIFF_TESTHIT), ohne TESTHIT 60 s Negativkontrolle
    Schaden,     // 5.5 F_SP121: Treffer am Boden + Ruf im Schaden, Wurfschaden im Ritt
    Wand,        // 5.6 F_SP121: Bergwand, Gang 5 + Turbo, 600 Ticks Messung
    Stagger,     // 5.7 F_SP121: KFF_TEST_STAGGER, Erholung, Gier, Hoehe
    Lava,        // 5.8 F_SP114: Eiswasser im Ritt (KFF_WP5_OHNE=boden), Ruecksprung erst nach dem Ritt
    Fluss,       // 5.9 F_SP112: Fluss-/Fall-Tag im Ritt
    Szenentyp,   // 5.10 F_SP121: Fluss-Szenentyp im Abflug (Kern-Waechter)
    Reittiere,   // 5.11 F_SP121: Gate-Proben Epona, Ziege, Boot, Spinner, Wildschwein
    Pfeil,       // 5.12 F_SP121: Gegnerpfeile auf Link und auf den Vogel
    Eym,         // 5.13 F_SP121: E_YM-Lock und mpKago in beide Richtungen
    Mensch,      // 5.14 F_SP103: Ruf als Mensch (mensch_ritt aus)
    Wandabwurf,  // 5.16 F_SP121: KFF_TEST_NOGUARD, STAGGER an der Wand, Ende genau einmal
    Unbekannt,
};

const char* w5CaseName(W5Case k) {
    switch (k) {
    case W5Case::Wasser: return "wasser";
    case W5Case::Seil: return "seil";
    case W5Case::Treffer: return "treffer";
    case W5Case::Schaden: return "schaden";
    case W5Case::Wand: return "wand";
    case W5Case::Stagger: return "stagger";
    case W5Case::Lava: return "lava";
    case W5Case::Fluss: return "fluss";
    case W5Case::Szenentyp: return "szenentyp";
    case W5Case::Reittiere: return "reittiere";
    case W5Case::Pfeil: return "pfeil";
    case W5Case::Eym: return "eym";
    case W5Case::Mensch: return "mensch";
    case W5Case::Wandabwurf: return "wandabwurf";
    case W5Case::Unbekannt: break;
    }
    return "?";
}

W5Case w5CaseFromEnv() {
    const char* v = std::getenv("KFF_WP5");
    if (v == nullptr) {
        return W5Case::Unbekannt;
    }
    for (int i = 0; i < static_cast<int>(W5Case::Unbekannt); ++i) {
        const W5Case k = static_cast<W5Case>(i);
        if (std::strcmp(v, w5CaseName(k)) == 0) {
            return k;
        }
    }
    return W5Case::Unbekannt;
}

struct W5Call {
    uint32_t attempts = 0;
    uint32_t start = 0;
    bool armed = false;
};

struct W5State {
    W5Case kase = W5Case::Unbekannt;
    int step = 0;
    uint32_t stepT = 0;
    W5Call call;
    bool done = false;
    uint32_t rideT = 0;
    // Wasserpunkt (Sonde)
    bool haveWater = false;
    float px = 0.0f, pz = 0.0f, pw = 0.0f, pg = 0.0f;
    // Messwerte (je Fall genutzt)
    float minA = 1.0e9f, minB = 1.0e9f;
    uint32_t overA = 0, overB = 0, swimA = 0, swimB = 0, procOffA = 0, procOffB = 0;
    uint32_t offRun = 0, reTeleport = 0;
    uint32_t bT = 0, endT = 0, goneT = 0;
    uint32_t chords = 0, summonsAfter = 0, birdsAfterMax = 0;
    uint32_t base0 = 0, base1 = 0, base2 = 0, base3 = 0, base4 = 0;
    int lifeA = 0, lifeB = 0, lifeC = 0;
    uint32_t cnt0 = 0, cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0, cnt5 = 0;
    int i0 = 0, i1 = 0;
    float f0 = 0.0f, f1 = 0.0f, f2 = 0.0f;
    cXyz prevPos;
    bool prevValid = false;
    int16_t prevYaw = 0;
    bool flag0 = false, flag1 = false, flag2 = false;
    fpc_ProcID id0 = kNoProc, id1 = kNoProc;
    uint32_t lastTgHit = 0;
    uint32_t hitWatchT = 0;
    int hitLife = 0;
    int lifeLossSum = 0;
    uint32_t hitsMeasured = 0;
    uint32_t probeIdx = 0;
    uint32_t probeOk = 0;
    uint32_t probeT = 0;
    const char* probeExpect = nullptr;
    uint32_t savedLastMode = 0;
    float savedLastSpeed = 0.0f;
    int16_t savedLastAngle = 0;
    uint32_t once = 0;    // Einmal-Ereignisse (Bits), siehe w5Once
    uint32_t nextA = 30;  // Ritt-Frame des naechsten Turbo-Tipps (A)
    uint32_t waterRejects = 0;  // Fall wasser: Rufe mit Gate-Grund "water"
    char note[256] = "";
};
W5State g_w5;
constexpr uint32_t kW5Deadline = 15000;  // Ticks; danach endet das Szenario mit fail

void w5Shot(const char* label) {
    logInfo("wp5shot: %s", label);
}

// Einmal-Ereignis je Bit: true im ersten Tick, in dem cond gilt. Robust, falls Ritt-Frames zwischen zwei Ticks springen.
bool w5Once(uint32_t bit, bool cond) {
    if (cond && (g_w5.once & bit) == 0) {
        g_w5.once |= bit;
        return true;
    }
    return false;
}

float splAt(float x, float y, float z) {
    cXyz p(x, y, z);
    dBgS_ObjGndChk_Spl spl;
    spl.SetPos(&p);
    return dComIfG_Bgsp().GroundCross(&spl);
}

float gndAt(float x, float y, float z) {
    cXyz p(x, y, z);
    dBgS_GndChk gnd;
    gnd.SetPos(&p);
    return dComIfG_Bgsp().GroundCross(&gnd);
}

// Wasserflaeche unter p (Start "above" darueber), -G_CM3D_F_INF ohne Wasser oder wenn Boden ueber dem Wasser liegt.
float waterBelow(const cXyz& p, float above) {
    const float w = splAt(p.x, p.y + above, p.z);
    if (w == -G_CM3D_F_INF) {
        return w;
    }
    const float g = gndAt(p.x, p.y + above, p.z);
    if (g != -G_CM3D_F_INF && g >= w) {
        return -G_CM3D_F_INF;
    }
    return w;
}

int w5CountBirdCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e && static_cast<daKago_c*>(ac)->mType == kFriendlyKagoType) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

uint32_t w5Birds() {
    uint32_t n = 0;
    fopAcIt_Executor(w5CountBirdCb, &n);
    return n;
}

bool w5SwimProc(daAlink_c* link) {
    const unsigned p = link->mProcID;
    return procIn(p, daAlink_c::PROC_WOLF_SWIM_UP, daAlink_c::PROC_WOLF_SWIM_END_WAIT) ||
           procIn(p, daAlink_c::PROC_SWIM_UP, daAlink_c::PROC_SWIM_DAMAGE) || p == daAlink_c::PROC_SWIM_SUBJECTIVITY ||
           link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0;
}

bool w5Calm(autotest::Context& c) {
    return c.sess.phase == Phase::Idle && liveLink() != nullptr && liveBird() == nullptr && !dComIfGp_event_runCheck();
}

// Ruf: R 30 Ticks halten, B bei +15. Wiederholung alle 300 Ticks, hoechstens 6 Versuche. Mensch-Link: autowolf.
void w5CallTick(autotest::Context& c, W5Call& k) {
    if (!k.armed && w5Calm(c) && k.attempts < 6 && (k.attempts == 0 || c.frame >= k.start + 300)) {
        ++k.attempts;
        k.start = c.frame;
        k.armed = true;
        daAlink_c* link = liveLink();
        logInfo("wp5: Ruf-Versuch %u bei Tick %u (Link Wolf %d, Proc 0x%x)", k.attempts, c.frame,
            link != nullptr && link->checkWolf() != 0 ? 1 : 0, link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u);
    }
    if (k.armed) {
        const uint32_t d = c.frame - k.start;
        if (d < 30) {
            c.holdR();
        }
        if (d == 15) {
            c.tapB();
        }
        if (d >= 30) {
            k.armed = false;
        }
    }
}

// Ruf-Kombination in einem Tick (R und B gleichzeitig): der Kern prueft callBlockReason im selben Tick.
void w5Chord(autotest::Context& c) {
    c.holdR();
    c.tapB();
}

void w5Teleport(daKago_c* k, float x, float y, float z) {
    k->current.pos.set(x, y, z);
    k->old.pos.set(x, y, z);
}

void w5Fwd(int16_t yaw, float& dx, float& dz) {
    dx = cM_ssin(yaw);
    dz = cM_scos(yaw);
}

int16_t w5AbsS(int16_t v) {
    return v < 0 ? static_cast<int16_t>(-v) : v;
}

float w5Deg(int v) {
    return static_cast<float>(v) * 360.0f / 65536.0f;
}

void w5Result(bool ok, const char* fmt, ...) KFF_PRINTF(2, 3);
void w5Result(bool ok, const char* fmt, ...) {
    char body[400];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    logInfo("wp5 RESULT %s %s %s", w5CaseName(g_w5.kase), ok ? "ok" : "fail", body);
    logWp5Stats("RESULT");
    testLogStats("RESULT");
    g_w5.done = true;
}

void w5Step(autotest::Context& c, int step) {
    g_w5.step = step;
    g_w5.stepT = c.frame;
}

// Ruf bis zum Ritt; true, sobald der Ritt laeuft. Nach 6 Versuchen ohne Ritt: fail.
bool w5CallUntilRide(autotest::Context& c) {
    if (c.sess.phase == Phase::Riding) {
        g_w5.rideT = c.frame;
        logInfo("wp5: Ritt beginnt bei Tick %u (Ruf-Versuche %u)", c.frame, g_w5.call.attempts);
        return true;
    }
    w5CallTick(c, g_w5.call);
    if (g_w5.call.attempts >= 6 && !g_w5.call.armed && c.frame > g_w5.call.start + 1200) {
        w5Result(false, "kein Ritt nach %u Ruf-Versuchen", g_w5.call.attempts);
    }
    return false;
}

// Nach B: warten, bis der Ritt endet und kein eigener Vogel mehr da ist. true, wenn fertig (oder Zeitlimit).
bool w5WaitGone(autotest::Context& c, uint32_t limit) {
    if (c.sess.phase == Phase::Riding) {
        return false;
    }
    if (g_w5.endT == 0) {
        g_w5.endT = c.frame;
    }
    if (c.sess.phase == Phase::Idle && c.sess.birdId == kNoProc && w5Birds() == 0) {
        if (g_w5.goneT == 0) {
            g_w5.goneT = c.frame;
        }
        return true;
    }
    return c.frame >= g_w5.endT + limit;
}

// Tiefe einer Spalte: Wasserflaeche unter yStart und Boden darunter. false ohne Wasser oder mit Boden ueber dem Wasser.
bool w5Column(float x, float yStart, float z, float& w, float& g) {
    w = splAt(x, yStart, z);
    if (w == -G_CM3D_F_INF) {
        return false;
    }
    g = gndAt(x, yStart, z);
    if (g != -G_CM3D_F_INF && g >= w) {
        return false;
    }
    if (g == -G_CM3D_F_INF) {
        g = w - 5000.0f;
    }
    return true;
}

void w5Consider(float x, float z, float w, float g, float& bestDepth) {
    if (w - g > bestDepth) {
        bestDepth = w - g;
        g_w5.haveWater = true;
        g_w5.px = x;
        g_w5.pz = z;
        g_w5.pw = w;
        g_w5.pg = g;
    }
}

// Wassersonde: 9x9 Raster um Link (Abstand 200, 2000, 5000), 5 Starthoehen; danach zwei feinere 21x21 Raster (Abstand
// 400 und 150) um den besten Punkt, Start 500 ueber dessen Wasserflaeche. Bester Punkt: groesste Tiefe.
bool w5ProbeWater(daAlink_c* link) {
    const float starts[5] = {-500.0f, 0.0f, 500.0f, 2000.0f, 5000.0f};
    const float spacings[3] = {200.0f, 2000.0f, 5000.0f};
    float bestDepth = -1.0f;
    for (float sp : spacings) {
        uint32_t cols = 0;
        for (int ix = -4; ix <= 4; ++ix) {
            for (int iz = -4; iz <= 4; ++iz) {
                const float x = link->current.pos.x + static_cast<float>(ix) * sp;
                const float z = link->current.pos.z + static_cast<float>(iz) * sp;
                bool colWater = false;
                for (float s : starts) {
                    float w = 0.0f;
                    float g = 0.0f;
                    if (!w5Column(x, link->current.pos.y + s + 500.0f, z, w, g)) {
                        continue;
                    }
                    colWater = true;
                    w5Consider(x, z, w, g, bestDepth);
                }
                if (colWater) {
                    ++cols;
                }
            }
        }
        logInfo("wp5: Wassersonde Raster 9x9 Abstand %.0f um Link (%.0f, %.0f, %.0f): %u Spalten mit Wasser, bester Punkt "
                "(%.0f, %.0f) Wasser %.1f Boden %.1f Tiefe %.1f",
            static_cast<double>(sp), static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
            static_cast<double>(link->current.pos.z), cols, static_cast<double>(g_w5.px), static_cast<double>(g_w5.pz),
            static_cast<double>(g_w5.pw), static_cast<double>(g_w5.pg), static_cast<double>(bestDepth));
        if (bestDepth >= 400.0f) {
            break;
        }
    }
    const float fine[2] = {400.0f, 150.0f};
    for (float sp : fine) {
        if (!g_w5.haveWater) {
            break;
        }
        const float cx = g_w5.px;
        const float cz = g_w5.pz;
        const float y0 = g_w5.pw + 500.0f;
        uint32_t cols = 0;
        for (int ix = -10; ix <= 10; ++ix) {
            for (int iz = -10; iz <= 10; ++iz) {
                float w = 0.0f;
                float g = 0.0f;
                const float x = cx + static_cast<float>(ix) * sp;
                const float z = cz + static_cast<float>(iz) * sp;
                if (w5Column(x, y0, z, w, g)) {
                    ++cols;
                    w5Consider(x, z, w, g, bestDepth);
                }
            }
        }
        logInfo("wp5: Wassersonde fein 21x21 Abstand %.0f um (%.0f, %.0f): %u Spalten mit Wasser, bester Punkt (%.0f, %.0f) "
                "Wasser %.1f Boden %.1f Tiefe %.1f",
            static_cast<double>(sp), static_cast<double>(cx), static_cast<double>(cz), cols, static_cast<double>(g_w5.px),
            static_cast<double>(g_w5.pz), static_cast<double>(g_w5.pw), static_cast<double>(g_w5.pg),
            static_cast<double>(bestDepth));
    }
    if (g_w5.haveWater && bestDepth < kSwimWolf + 10.0f) {
        std::snprintf(g_w5.note, sizeof(g_w5.note), "Wasser zu flach fuer Schwimmen (max Tiefe %.0f, Wolf-Schwelle %.0f)",
            static_cast<double>(bestDepth), static_cast<double>(kSwimWolf));
    }
    return g_w5.haveWater;
}

// Nick-Regler fuer den Stick: Nase etwa targetDeg nach unten (Vanilla-Vorzeichen, angle.x > 0 = Nase unten, invert_steer
// aus). Unter dem Ziel drueckt der Stick (gain), darueber ist er neutral und flug trimmt zur Horizontalen.
float w5PitchStick(const daKago_c* bird, float targetDeg, float gain) {
    const float pitchDeg = static_cast<float>(bird->current.angle.x) * 360.0f / 65536.0f;
    return pitchDeg < targetDeg ? gain : 0.0f;
}

// Messung ueber Wasser (Schweben a, Tauchen b): Abstand Links Fuesse zur Wasserflaeche, Schwimm-Procs, Proc != 312.
void w5MeasureWater(autotest::Context& c, bool dive) {
    daAlink_c* link = liveLink();
    daKago_c* bird = liveBird();
    if (link == nullptr || bird == nullptr) {
        return;
    }
    const float w = waterBelow(link->current.pos, 1000.0f);
    float& minV = dive ? g_w5.minB : g_w5.minA;
    uint32_t& over = dive ? g_w5.overB : g_w5.overA;
    uint32_t& swim = dive ? g_w5.swimB : g_w5.swimA;
    uint32_t& off = dive ? g_w5.procOffB : g_w5.procOffA;
    if (w != -G_CM3D_F_INF) {
        ++over;
        g_w5.offRun = 0;
        const float clear = link->current.pos.y - w;
        if (clear < minV) {
            minV = clear;
            if (clear < 60.0f) {
                logInfo("wp5: %s: neues Minimum Links Fuesse %.1f ueber Wasser (Link y %.1f, Wasser %.1f, Vogel y %.1f, "
                        "Tick %u)",
                    dive ? "Tauchen" : "Schweben", static_cast<double>(clear), static_cast<double>(link->current.pos.y),
                    static_cast<double>(w), static_cast<double>(bird->current.pos.y), c.frame);
            }
        }
    } else if (++g_w5.offRun > 30 && g_w5.haveWater) {
        g_w5.offRun = 0;
        ++g_w5.reTeleport;
        w5Teleport(bird, g_w5.px, g_w5.pw + 400.0f, g_w5.pz);
        logInfo("wp5: Vogel zurueck ueber das Wasser gesetzt (%u)", g_w5.reTeleport);
    }
    if (w5SwimProc(link)) {
        if (swim == 0) {
            logInfo("wp5: SCHWIMM-PROC im Ritt: Proc 0x%x, Modus-Schwimmen %d, Tick %u", static_cast<unsigned>(link->mProcID),
                link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0 ? 1 : 0, c.frame);
        }
        ++swim;
    }
    if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
        ++off;
    }
}

// ---- 5.1 / 5.1b / 5.2 wasser (F_SP115)
bool w5Wasser(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame < 120 || link == nullptr || dComIfGp_event_runCheck()) {
            return false;
        }
        if (!w5ProbeWater(link)) {
            w5Result(false, "kein Wasser in der Sonde (9x9 Abstand 200 und 2000, 5 Hoehen)");
            return true;
        }
        w5Step(c, 1);
        return false;
    case 1:
        if (w5CallUntilRide(c)) {
            w5Step(c, 2);
        }
        return g_w5.done;
    case 2: {
        const uint32_t rf = c.sess.rideFrames;
        if (w5Once(0x1, rf >= 5) || w5Once(0x2, rf >= 15)) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));  // Gang 3 -> 1: langsam, bleibt ueber dem See
        }
        daKago_c* bird = liveBird();
        if (rf >= 30 && bird != nullptr && (g_w5.once & 0x2) != 0) {
            w5Teleport(bird, g_w5.px, g_w5.pw + 400.0f, g_w5.pz);
            logInfo("wp5: Vogel ueber den Wasserpunkt gesetzt (%.0f, %.0f, %.0f), Schweben 1200 Ticks (Stick rechts, leicht "
                    "runter)",
                static_cast<double>(g_w5.px), static_cast<double>(g_w5.pw + 400.0f), static_cast<double>(g_w5.pz));
            w5Step(c, 3);
        }
        return false;
    }
    case 3: {
        // Schweben: Vollkreis (Radius etwa 560 bei Gang 1) und Nase etwa 15 Grad nach unten (Nick-Regler), damit der Vogel
        // bis an die Untergrenze sinkt und dort bleibt. Ein gehaltener Stick Y waere eine Schleife (flug: Nick kumuliert).
        daKago_c* hb = liveBird();
        c.stick(1.0f, hb != nullptr ? w5PitchStick(hb, 15.0f, 0.4f) : 0.0f);
        w5MeasureWater(c, false);
        const uint32_t d = c.frame - g_w5.stepT;
        if (d == 20) {
            w5Shot("wasser-start");
        }
        if (d == 600) {
            w5Shot("wasser-schweben");
        }
        if (d >= 1200) {
            logInfo("wp5: Schweben fertig: min Abstand %.1f, ueber Wasser %u Ticks, Schwimm-Procs %u, Proc != 312 %u, "
                    "Ruecksetzungen %u",
                static_cast<double>(g_w5.minA), g_w5.overA, g_w5.swimA, g_w5.procOffA, g_w5.reTeleport);
            w5Step(c, 4);
        }
        return false;
    }
    case 4: {
        // Tauchen: Nase steil nach unten (etwa 75 Grad, voller Stick bis zum Ziel), leichte Kurve, 300 Ticks.
        daKago_c* db = liveBird();
        c.stick(0.5f, db != nullptr ? w5PitchStick(db, 75.0f, 1.0f) : 0.0f);
        w5MeasureWater(c, true);
        const uint32_t d = c.frame - g_w5.stepT;
        if (d == 150) {
            w5Shot("wasser-tauchen");
        }
        if (d >= 300) {
            logInfo("wp5: Tauchen fertig: min Abstand %.1f, ueber Wasser %u Ticks, Schwimm-Procs %u, Proc != 312 %u",
                static_cast<double>(g_w5.minB), g_w5.overB, g_w5.swimB, g_w5.procOffB);
            g_w5.base0 = g_wp5.waterRelease;
            daKago_c* bird = liveBird();
            if (bird != nullptr && g_w5.haveWater) {
                w5Teleport(bird, g_w5.px, g_w5.pw + 400.0f, g_w5.pz);  // B ueber der tiefsten Stelle (Link soll schwimmen)
            }
            c.tapB();
            g_w5.bT = c.frame;
            w5Step(c, 5);
        }
        return false;
    }
    case 5:
        if (c.sess.phase != Phase::Riding) {
            g_w5.endT = c.frame;
            logInfo("wp5: Ritt endet %u Ticks nach B (K17-Freigaben %u)", c.frame - g_w5.bT, g_wp5.waterRelease - g_w5.base0);
            w5Shot("wasser-abgestiegen");
            w5Step(c, 6);
        } else if (c.frame - g_w5.bT > 600) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "Ritt nach B nicht beendet");
            w5Step(c, 6);
        }
        return false;
    case 6:
        if ((c.sess.phase == Phase::Idle && c.sess.birdId == kNoProc && w5Birds() == 0) || c.frame - g_w5.stepT > 900) {
            g_w5.goneT = c.frame;
            g_w5.base1 = g_wt.gateCalls;
            w5Step(c, 7);
        }
        return false;
    case 7: {
        // Ruhe abwarten: kein Ereignis, kein Dialog (eine Spiel-Szene an der Stelle laesst sich nur mit A weiterklicken).
        const uint32_t d = c.frame - g_w5.stepT;
        const bool talk = dMsgObject_isTalkNowCheck() || dComIfGp_event_runCheck() != 0;
        if (talk) {
            g_w5.offRun = 0;
            if (d % 20 == 0) {
                c.tapA();  // Text weiter
                ++g_w5.cnt5;
            }
        } else {
            ++g_w5.offRun;
        }
        if (g_w5.offRun >= 30 || d > 2400) {
            logInfo("wp5: Ruhe nach dem Ritt nach %u Ticks (Text-A %u, Ereignis/Dialog jetzt %d)", d, g_w5.cnt5,
                talk ? 1 : 0);
            w5Step(c, 8);
        }
        return false;
    }
    case 8: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (d == 1 || d == 61 || d == 121) {
            if (link != nullptr) {
                logInfo("wp5: Ruf im Wasser %u: Proc 0x%x, Schwimmen %d, Wasser %.0f ueber Link, Wolf %d, Phase %s, Dialog %d",
                    g_w5.chords + 1, static_cast<unsigned>(link->mProcID), w5SwimProc(link) ? 1 : 0,
                    static_cast<double>(link->mWaterY - link->current.pos.y), link->checkWolf() != 0 ? 1 : 0,
                    phaseName(c.sess.phase), dMsgObject_isTalkNowCheck() ? 1 : 0);
            }
            g_w5.base2 = g_wt.gateCalls;
            w5Chord(c);
            ++g_w5.chords;
        }
        if (d == 2 || d == 62 || d == 122) {
            // Der Kern prueft die Kombination im selben padReadPost nach diesem Szenario-Tick: Ergebnis einen Tick spaeter.
            const bool reached = g_wt.gateCalls != g_w5.base2;
            const char* why = reached ? g_wt.lastWhy : nullptr;
            if (why != nullptr && std::strcmp(why, "water") == 0) {
                ++g_w5.waterRejects;
            }
            logInfo("wp5: Ruf im Wasser %u: Gate erreicht %d, Grund %s", g_w5.chords, reached ? 1 : 0,
                why != nullptr ? why : "(keiner)");
        }
        if (d == 70) {
            w5Shot("wasser-ruf");
        }
        if (d > 1 && c.sess.phase != Phase::Idle) {
            ++g_w5.summonsAfter;
        }
        const uint32_t nb = w5Birds();
        if (nb > g_w5.birdsAfterMax) {
            g_w5.birdsAfterMax = nb;
        }
        if (d >= 300) {
            const bool ohne = unprotected();
            const bool water = g_w5.minA >= 20.0f && g_w5.minB >= 20.0f && g_w5.swimA == 0 && g_w5.swimB == 0;
            const bool calls = g_w5.summonsAfter == 0 && g_w5.birdsAfterMax == 0 && g_w5.waterRejects >= 1;
            const bool ok = ohne ? (g_w5.swimA + g_w5.swimB) > 0 : (water && calls && g_w5.overA > 600);
            w5Result(ok,
                "%s minSchweben=%.1f minTauchen=%.1f ueberWasser=%u/%u schwimmProcs=%u/%u procOff=%u/%u "
                "ruecksetz=%u k17=%u abstiegTicks=%u rufe=%u grundWater=%u phaseNachRuf=%u birdsNachRuf=%u %s",
                ohne ? "[Vergleichslauf ohne Schutz: erwartet Schwimm-Proc]" : "[Schutz an]",
                static_cast<double>(g_w5.minA), static_cast<double>(g_w5.minB), g_w5.overA, g_w5.overB, g_w5.swimA,
                g_w5.swimB, g_w5.procOffA, g_w5.procOffB, g_w5.reTeleport, g_wp5.waterRelease - g_w5.base0,
                g_w5.endT > g_w5.bT ? g_w5.endT - g_w5.bT : 0, g_w5.chords, g_w5.waterRejects, g_w5.summonsAfter,
                g_w5.birdsAfterMax, g_w5.note);
            return true;
        }
        return false;
    }
    default:
        return true;
    }
}

// ---- Gate-Proben (5.3 Seil, 5.11 Reittiere): je Probe ein Ruf (R+B in einem Tick), das Gate setzt Proc/Reitstatus nur
// fuer diesen einen Aufruf.
struct W5Probe {
    const char* label;
    int proc;
    int ride;
    const char* expect;
};

const W5Probe kSeilProbes[] = {
    {"Seil (Wolf, Seilgang)", daAlink_c::PROC_WOLF_ROPE_MOVE, -1, "rope"},
    {"Kette (Wolf, Kette ziehen)", daAlink_c::PROC_WOLF_CHAIN_WAIT, -1, "chain"},
    {"Haengen (Wolf, Kante)", daAlink_c::PROC_WOLF_HANG_READY, -1, "hang"},
    {"Leiter", daAlink_c::PROC_LADDER_MOVE, -1, "climb"},
    {"Ranken/Kletterwand", daAlink_c::PROC_CLIMB_MOVE_SIDE, -1, "climb"},
};

const W5Probe kMountProbes[] = {
    {"Epona (Pferd)", daAlink_c::PROC_HORSE_WAIT, daAlink_c::RIDETYPE_HORSE, "riding"},
    {"Ziege (Goat)", daAlink_c::PROC_GOAT_CATCH, -1, "riding"},
    {"Boot (Kanu)", daAlink_c::PROC_CANOE_RIDE, daAlink_c::RIDETYPE_CANOE, "riding"},
    {"Spinner", daAlink_c::PROC_SPINNER_READY, daAlink_c::RIDETYPE_SPINNER, "riding"},
    {"Wildschwein", daAlink_c::PROC_BOAR_RUN, daAlink_c::RIDETYPE_BOAR, "riding"},
};

// Probenfolge: alle 45 Ticks eine Probe; im Tick danach Auswertung. true, wenn alle Proben durch sind.
bool w5RunProbes(autotest::Context& c, const W5Probe* probes, uint32_t n) {
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return false;
    }
    const uint32_t d = c.frame - g_w5.stepT;
    if (g_w5.probeIdx < n && d == g_w5.probeIdx * 45 + 10) {
        const W5Probe& p = probes[g_w5.probeIdx];
        g_wt.probeArmed = true;
        g_wt.probeLabel = p.label;
        g_wt.probeProc = p.proc;
        g_wt.probeRide = p.ride;
        g_wt.probeWhy = nullptr;
        g_w5.probeExpect = p.expect;
        g_w5.base2 = g_wt.probeDone;
        g_w5.i0 = static_cast<int>(link->mProcID);
        g_w5.i1 = static_cast<int>(link->mRideStatus);
        g_w5.probeT = c.frame;
        w5Chord(c);
        logInfo("wp5: Probe %u (%s) gesendet: Link-Proc vorher 0x%x, Reitstatus %d", g_w5.probeIdx + 1, p.label,
            static_cast<unsigned>(g_w5.i0), g_w5.i1);
        return false;
    }
    if (g_w5.probeIdx < n && g_w5.probeT != 0 && c.frame == g_w5.probeT + 2) {
        const W5Probe& p = probes[g_w5.probeIdx];
        const bool reached = g_wt.probeDone != g_w5.base2;
        const bool why = reached && g_wt.probeWhy != nullptr && std::strcmp(g_wt.probeWhy, p.expect) == 0;
        const bool idle = c.sess.phase == Phase::Idle && w5Birds() == 0;
        const bool okp = reached && why && idle && g_wt.probeRestored;
        if (okp) {
            ++g_w5.probeOk;
        }
        g_wt.probeArmed = false;  // nicht erreicht: Probe verwerfen
        logInfo("wp5: Probe %u (%s): Gate erreicht %d, Grund %s (erwartet %s), Phase %s, Voegel %u, Proc vorher 0x%x jetzt "
                "0x%x, Reitstatus jetzt %u, Ergebnis %s",
            g_w5.probeIdx + 1, p.label, reached ? 1 : 0, g_wt.probeWhy != nullptr ? g_wt.probeWhy : "(keiner)", p.expect,
            phaseName(c.sess.phase), w5Birds(), static_cast<unsigned>(g_w5.i0), static_cast<unsigned>(link->mProcID),
            static_cast<unsigned>(link->mRideStatus), okp ? "ok" : "FEHLER");
        ++g_w5.probeIdx;
        g_w5.probeT = 0;
    }
    return g_w5.probeIdx >= n && c.frame - g_w5.stepT > n * 45 + 20;
}

// ---- 5.3 seil (F_SP121)
bool w5Seil(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame >= 150 && w5Calm(c) && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            w5Step(c, 1);
        }
        return false;
    case 1:
        if (w5RunProbes(c, kSeilProbes, sizeof(kSeilProbes) / sizeof(kSeilProbes[0]))) {
            w5Step(c, 2);
        }
        return false;
    case 2:
        if (link != nullptr && w5Calm(c) && link->mLinkAcch.ChkGroundHit()) {
            link->current.pos.y += 600.0f;  // echter Fall: Link 600 hoch, Schwerkraft und Fall-Proc vom Spiel
            link->old.pos.y += 600.0f;
            g_w5.base3 = g_wt.gateCalls;
            logInfo("wp5: Link 600 Einheiten hoch gesetzt (echter Fall), Rufe bei +4, +8, +12 Ticks");
            w5Step(c, 3);
        } else if (c.frame - g_w5.stepT > 600) {
            w5Step(c, 3);
        }
        return false;
    case 3: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (link != nullptr && (d == 4 || d == 8 || d == 12)) {
            logInfo("wp5: Ruf im Fall (+%u): Proc 0x%x, Boden %d, y %.0f", d, static_cast<unsigned>(link->mProcID),
                link->mLinkAcch.ChkGroundHit() ? 1 : 0, static_cast<double>(link->current.pos.y));
            g_w5.base3 = g_wt.gateCalls;
            w5Chord(c);
            ++g_w5.chords;
        }
        if (d == 6) {
            w5Shot("seil-fall");
        }
        if (d == 5 || d == 9 || d == 13) {
            // Ergebnis des Rufs aus dem Tick davor (der Kern prueft im selben padReadPost nach dem Szenario-Tick).
            const bool reached = g_wt.gateCalls != g_w5.base3;
            const char* why = reached ? g_wt.lastWhy : nullptr;
            if (why != nullptr && std::strcmp(why, "air") == 0) {
                ++g_w5.cnt0;
                g_w5.flag0 = true;
            }
            logInfo("wp5: Gate-Grund im Fall (+%u): erreicht %d, Grund %s", d - 1, reached ? 1 : 0,
                why != nullptr ? why : "(keiner)");
        }
        if (d > 4 && c.sess.phase != Phase::Idle) {
            ++g_w5.summonsAfter;
        }
        if (d >= 200) {
            const uint32_t nProbes = sizeof(kSeilProbes) / sizeof(kSeilProbes[0]);
            const bool ok = g_w5.probeOk == nProbes && g_w5.flag0 && g_w5.summonsAfter == 0 && w5Birds() == 0;
            w5Result(ok, "proben=%u/%u (Seil, Kette, Haengen, Leiter, Ranken), fallGrundAir=%u, rufeImFall=%u, "
                         "phaseNachRuf=%u, birds=%u [Proben: Proc nur im Gate-Aufruf gesetzt; Fall echt]",
                g_w5.probeOk, nProbes, g_w5.cnt0, g_w5.chords, g_w5.summonsAfter, w5Birds());
            return true;
        }
        return false;
    }
    default:
        return true;
    }
}

// ---- 5.11 reittiere (F_SP121)
bool w5Reittiere(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame >= 150 && w5Calm(c) && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            logInfo("wp5: Kanone (Obj_SCannon) ist ein Ereignis: dort lehnt der Kern den Ruf vor dem Gate ab "
                    "('Ereignis laeuft'), keine Probe");
            w5Step(c, 1);
        }
        return false;
    case 1:
        if (w5RunProbes(c, kMountProbes, sizeof(kMountProbes) / sizeof(kMountProbes[0]))) {
            w5Step(c, 2);
        }
        return false;
    case 2:
        // Gegenprobe: normaler Ruf ohne Probe muss angenommen werden (kein falsches "riding").
        if (w5CallUntilRide(c)) {
            g_w5.flag0 = true;
            w5Step(c, 3);
        }
        return g_w5.done;
    case 3:
        if (c.sess.rideFrames >= 120 && !g_w5.flag1) {
            g_w5.flag1 = true;
            c.tapB();
            w5Step(c, 4);
        }
        return false;
    case 4:
        if (w5WaitGone(c, 900)) {
            const uint32_t n = sizeof(kMountProbes) / sizeof(kMountProbes[0]);
            const bool ok = g_w5.probeOk == n && g_w5.flag0 && w5Birds() == 0;
            w5Result(ok, "proben=%u/%u (Epona, Ziege, Boot, Spinner, Wildschwein), gegenprobeRitt=%d, birds=%u "
                         "[Proben: Proc/Reitstatus nur im Gate-Aufruf; Kanone = Ereignis-Sperre des Kerns]",
                g_w5.probeOk, n, g_w5.flag0 ? 1 : 0, w5Birds());
            return true;
        }
        return false;
    default:
        return true;
    }
}

// Lebenszaehlung je TG-Treffer: Leben vor dem Treffer und 20 Ticks danach.
void w5TrackHits(autotest::Context& c) {
    if (g_wt.tgHit != g_w5.lastTgHit) {
        g_w5.lastTgHit = g_wt.tgHit;
        g_w5.hitLife = g_wt.lifeBefore;
        g_w5.hitWatchT = c.frame + 20;
    }
    if (g_w5.hitWatchT != 0 && c.frame == g_w5.hitWatchT) {
        g_w5.hitWatchT = 0;
        const int now = static_cast<int>(dComIfGs_getLife());
        g_w5.lifeLossSum += g_w5.hitLife - now;
        ++g_w5.hitsMeasured;
        logInfo("wp5: Treffer %u: Leben %d -> %d (Abzug %d)", g_w5.hitsMeasured, g_w5.hitLife, now, g_w5.hitLife - now);
    }
}

// ---- 5.4 treffer (F_SP121): KFF_GRIFF_TESTHIT=N (Test-ATs im Ritt), ohne TESTHIT 60 s Negativkontrolle
bool w5Treffer(autotest::Context& c) {
    switch (g_w5.step) {
    case 0:
        if (w5CallUntilRide(c)) {
            g_w5.lifeA = static_cast<int>(dComIfGs_getLife());
            g_w5.base0 = g_wt.tgHit;
            g_w5.base1 = g_stats.procOff;
            g_w5.base2 = g_stats.lostFrames;
            w5Step(c, 1);
        }
        return g_w5.done;
    case 1: {
        const uint32_t rf = c.sess.rideFrames;
        const uint32_t endRf = g_testHitAt > 0 ? (g_testHitAt + kTestHitEvery * kTestHitCount + 120 > 300
                                                      ? g_testHitAt + kTestHitEvery * kTestHitCount + 120
                                                      : 300)
                                               : 1800;  // 60 s bei 30 Hz
        c.stick(0.3f, 0.0f);
        w5TrackHits(c);
        daAlink_c* link = liveLink();
        if (c.sess.phase != Phase::Riding) {
            ++g_w5.cnt0;  // Ritt vor B beendet
        } else if (link != nullptr && link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
            ++g_w5.cnt1;
        }
        if (g_wt.tgHit > g_w5.base0 && !g_w5.flag0) {
            g_w5.flag0 = true;
            g_w5.cnt2 = c.frame;
        }
        if (g_w5.cnt2 != 0 && c.frame == g_w5.cnt2 + 3) {
            w5Shot("treffer-1");
        }
        if (rf >= endRf || c.sess.phase != Phase::Riding) {
            g_w5.lifeB = static_cast<int>(dComIfGs_getLife());
            c.tapB();
            w5Step(c, 2);
        }
        return false;
    }
    case 2:
        w5TrackHits(c);
        if (w5WaitGone(c, 900)) {
            const uint32_t hits = g_wt.tgHit - g_w5.base0;
            const bool neg = g_testHitAt == 0;
            const bool perHit = g_w5.hitsMeasured > 0 && g_w5.lifeLossSum == 2 * static_cast<int>(g_w5.hitsMeasured);
            const bool ok = neg ? hits == 0 && g_w5.cnt0 == 0
                                : hits >= 1 && perHit && g_wt.procChangeOnHit == 0 && g_w5.cnt1 == 0 && g_w5.cnt0 == 0;
            w5Result(ok,
                "%s testAtRitt=%u tgHit=%u gemessen=%u lebenAbzugSumme=%d leben=%d->%d procNicht312=%u "
                "procWechselNachTreffer=%u rittVorB=%u griffProcOff=%u lostFrames=%u birds=%u",
                neg ? "[Negativkontrolle ohne Test-AT, 60 s]" : (unprotected() ? "[ohne WP5-Schutz]" : "[Schutz an]"),
                g_wt.testAtRide, hits, g_w5.hitsMeasured, g_w5.lifeLossSum, g_w5.lifeA, g_w5.lifeB, g_w5.cnt1,
                g_wt.procChangeOnHit, g_w5.cnt0, g_stats.procOff - g_w5.base1, g_stats.lostFrames - g_w5.base2, w5Birds());
            return true;
        }
        return false;
    default:
        return true;
    }
}

bool w5DamageProc(unsigned p) {
    return procIn(p, daAlink_c::PROC_WOLF_DAMAGE, daAlink_c::PROC_WOLF_SCREAM_WAIT) ||
           procIn(p, daAlink_c::PROC_DAMAGE, daAlink_c::PROC_LAND_DAMAGE) || p == daAlink_c::PROC_LARGE_DAMAGE ||
           p == daAlink_c::PROC_POLY_DAMAGE;
}

// ---- 5.5 schaden (F_SP121)
bool w5Schaden(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame >= 150 && w5Calm(c) && link != nullptr && link->mLinkAcch.ChkGroundHit() &&
            !w5DamageProc(link->mProcID)) {
            g_w5.lifeA = static_cast<int>(dComIfGs_getLife());
            g_wt.groundHitArmed = true;
            ++g_w5.cnt0;
            logInfo("wp5: Boden-Treffer %u angefordert (Test-AT an Link, Halter Midna), Leben %d", g_w5.cnt0, g_w5.lifeA);
            w5Step(c, 1);
        }
        return false;
    case 1: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (link != nullptr && w5DamageProc(link->mProcID)) {
            g_w5.i0 = static_cast<int>(link->mProcID);
            g_w5.base3 = g_wt.gateCalls;
            logInfo("wp5: Link im Schadens-Proc 0x%x (Tick %u nach dem AT): Ruf R+B", static_cast<unsigned>(link->mProcID), d);
            w5Chord(c);
            ++g_w5.chords;
            w5Shot("schaden-boden");
            w5Step(c, 2);
        } else if (d > 60) {
            if (g_w5.cnt0 >= 3) {
                std::snprintf(g_w5.note, sizeof(g_w5.note), "kein Schadens-Proc nach 3 Boden-ATs");
                w5Step(c, 3);
            } else {
                w5Step(c, 0);
            }
        }
        return false;
    }
    case 2: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (d == 2) {
            g_w5.flag0 = g_wt.gateCalls != g_w5.base3 && g_wt.lastWhy != nullptr &&
                         std::strcmp(g_wt.lastWhy, "damage") == 0;
            logInfo("wp5: Gate-Grund im Schaden: %s, Phase %s", g_wt.lastWhy != nullptr ? g_wt.lastWhy : "(keiner)",
                phaseName(c.sess.phase));
        }
        if (d > 1 && c.sess.phase != Phase::Idle) {
            ++g_w5.summonsAfter;
        }
        if (d >= 2 && link != nullptr && !w5DamageProc(link->mProcID) && w5Calm(c) && d > 60) {
            g_w5.lifeB = static_cast<int>(dComIfGs_getLife());
            w5Step(c, 3);
        } else if (d > 600) {
            w5Step(c, 3);
        }
        return false;
    }
    case 3:
        if (w5CallUntilRide(c)) {
            g_w5.base0 = g_wp5.dmgThrow;
            g_w5.base1 = g_stats.lostFrames;
            w5Step(c, 4);
        }
        return g_w5.done;
    case 4: {
        const uint32_t rf = c.sess.rideFrames;
        if (link == nullptr) {
            return false;
        }
        if (c.sess.phase != Phase::Riding) {
            ++g_w5.cnt3;  // Ritt vor B beendet
        } else if (rf >= 55 && link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
            ++g_w5.cnt4;
        }
        if (rf >= 60 && !g_w5.flag1) {
            g_w5.flag1 = true;
            g_w5.lifeC = static_cast<int>(dComIfGs_getLife());
            link->setThrowDamage(link->shape_angle.y, 10.0f, 10.0f, 2, 1, 0);
            logInfo("wp5: Wurfschaden 1 im Ritt (setThrowDamage, Grossschaden-Flag 0x40, Schaden 2), Leben %d", g_w5.lifeC);
        }
        if (w5Once(0x20, rf >= 64)) {
            w5Shot("schaden-wurf");
        }
        if (rf >= 120 && !g_w5.flag2) {
            g_w5.flag2 = true;
            link->setThrowDamage(link->shape_angle.y, 10.0f, 10.0f, 2, 0, 0);
            logInfo("wp5: Wurfschaden 2 im Ritt (setThrowDamage, Flag 0x80, Schaden 2), Leben %d",
                static_cast<int>(dComIfGs_getLife()));
        }
        if (rf >= 200) {
            g_w5.cnt5 = static_cast<uint32_t>(dComIfGs_getLife());
            c.tapB();
            w5Step(c, 5);
        }
        return false;
    }
    case 5:
        if (w5WaitGone(c, 900)) {
            const int lifeEnd = static_cast<int>(g_w5.cnt5);
            const bool ground = g_w5.flag0 && g_w5.summonsAfter == 0;
            const bool ride = g_w5.cnt3 == 0 && g_w5.cnt4 == 0 && g_wp5.dmgThrow - g_w5.base0 == 2 && lifeEnd < g_w5.lifeC;
            w5Result(ground && ride && w5Birds() == 0,
                "boden: schadensProc=0x%x grundDamage=%d phaseNachRuf=%u leben=%d->%d | ritt: wurfAufgeloest=%u "
                "leben=%d->%d procNicht312=%u rittVorB=%u lostFrames=%u birds=%u %s",
                static_cast<unsigned>(g_w5.i0), g_w5.flag0 ? 1 : 0, g_w5.summonsAfter, g_w5.lifeA, g_w5.lifeB,
                g_wp5.dmgThrow - g_w5.base0, g_w5.lifeC, lifeEnd, g_w5.cnt4, g_w5.cnt3, g_stats.lostFrames - g_w5.base1,
                w5Birds(), g_w5.note);
            return true;
        }
        return false;
    default:
        return true;
    }
}

// ---- Bergwand (5.6 wand, 5.16 wandabwurf): Kurs auf (-81410, -23950), Gang 5, Turbo, Stick neutral.
constexpr float kWallX = -81410.0f;
constexpr float kWallZ = -23950.0f;

// Anflug; true bei Wandkontakt (Linien-Check 300 voraus trifft oder Acch-Wandtreffer).
bool w5Approach(autotest::Context& c, daKago_c* bird) {
    const uint32_t rf = c.sess.rideFrames;
    if (!g_w5.flag0) {
        g_w5.flag0 = true;
        const float dx = kWallX - bird->current.pos.x;
        const float dz = kWallZ - bird->current.pos.z;
        bird->current.angle.y = cM_atan2s(dx, dz);
        bird->shape_angle.y = bird->current.angle.y;
        logInfo("wp5: Kurs auf die Bergwand (x=%.0f, z=%.0f): Gier %d, Entfernung %.0f", static_cast<double>(kWallX),
            static_cast<double>(kWallZ), static_cast<int>(bird->current.angle.y),
            static_cast<double>(std::sqrt(dx * dx + dz * dz)));
    }
    if (w5Once(0x1, rf >= 5) || w5Once(0x2, rf >= 15)) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));  // Gang 3 -> 5
    }
    if (rf >= g_w5.nextA) {
        c.tapA();  // Turbo (Dash)
        g_w5.nextA = rf + 120;
    }
    float dx = 0.0f;
    float dz = 0.0f;
    w5Fwd(bird->current.angle.y, dx, dz);
    cXyz a(bird->current.pos.x, bird->current.pos.y, bird->current.pos.z);
    cXyz b(a.x + dx * 300.0f, a.y, a.z + dz * 300.0f);
    dBgS_LinChk lin;
    lin.Set(&a, &b, nullptr);
    const bool ahead = dComIfG_Bgsp().LineCross(&lin);
    return ahead || bird->mObjAcch.ChkWallHit();
}

bool w5Wand(autotest::Context& c) {
    daKago_c* bird = liveBird();
    switch (g_w5.step) {
    case 0:
        if (w5CallUntilRide(c)) {
            w5Step(c, 1);
        }
        return g_w5.done;
    case 1:
        if (bird == nullptr) {
            return false;
        }
        if (w5Approach(c, bird)) {
            logInfo("wp5: Wandkontakt bei Ritt-Frame %u, pos=(%.0f,%.0f,%.0f), Aktion %d, Gang %d", c.sess.rideFrames,
                static_cast<double>(bird->current.pos.x), static_cast<double>(bird->current.pos.y),
                static_cast<double>(bird->current.pos.z), bird->mAction, c.sess.gear);
            g_w5.base0 = g_wp5.staggerSkip;
            g_w5.base1 = g_stats.lostFrames;
            g_w5.base2 = g_wp5.letGo;
            g_w5.base3 = g_stats.notFly;
            g_w5.base4 = g_wp5.landingSkip;
            g_w5.prevPos = bird->current.pos;
            g_w5.prevYaw = bird->current.angle.y;
            g_w5.prevValid = true;
            w5Shot("wand-kontakt");
            if (g_w5.kase == W5Case::Wandabwurf) {
                bird->setActionMode(daKago_c::ACTION_STAGGER_e, 0);
                logInfo("wp5: STAGGER Modus 0 an der Wand gesetzt (KFF_TEST_NOGUARD, Vanilla-Absturz), Ritt-Frame %u",
                    c.sess.rideFrames);
                w5Step(c, 10);
            } else {
                w5Step(c, 2);
            }
        } else if (c.sess.rideFrames > 2400) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "kein Wandkontakt bis Ritt-Frame 2400");
            c.tapB();
            w5Step(c, 3);
        }
        return false;
    case 2: {
        const uint32_t d = c.frame - g_w5.stepT;
        const uint32_t rf = c.sess.rideFrames;
        if (rf >= g_w5.nextA) {
            c.tapA();  // Turbo bleibt an
            g_w5.nextA = rf + 120;
        }
        if (bird != nullptr && c.sess.phase == Phase::Riding) {
            const int act = bird->mAction;
            const int16_t dyaw = static_cast<int16_t>(bird->current.angle.y - g_w5.prevYaw);
            const int16_t ady = w5AbsS(dyaw);
            if (act == daKago_c::ACTION_FLY_e) {
                if (ady != 0) {
                    ++g_w5.cnt0;
                }
                if (ady > g_w5.i0) {
                    g_w5.i0 = ady;
                }
            } else if (act == daKago_c::ACTION_ATTACK_e) {
                if (ady != 0) {
                    ++g_w5.cnt1;
                }
                if (ady > g_w5.i1) {
                    g_w5.i1 = ady;
                }
            } else {
                ++g_w5.cnt2;  // weder FLY noch ATTACK
            }
            if (g_w5.prevValid) {
                cXyz a = g_w5.prevPos;
                cXyz b = bird->current.pos;
                const float mx = b.x - a.x;
                const float my = b.y - a.y;
                const float mz = b.z - a.z;
                if (mx * mx + my * my + mz * mz > 1.0f) {
                    dBgS_LinChk lin;
                    lin.Set(&a, &b, nullptr);
                    if (dComIfG_Bgsp().LineCross(&lin)) {
                        ++g_w5.cnt3;
                        if (g_w5.cnt3 <= 3) {
                            const cXyz x = lin.GetCross();
                            logInfo("wp5: Durchdringung? Linie (%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f) schneidet bei "
                                    "(%.0f,%.0f,%.0f)",
                                static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(a.z),
                                static_cast<double>(b.x), static_cast<double>(b.y), static_cast<double>(b.z),
                                static_cast<double>(x.x), static_cast<double>(x.y), static_cast<double>(x.z));
                        }
                    }
                }
            }
            g_w5.prevPos = bird->current.pos;
            g_w5.prevYaw = bird->current.angle.y;
            g_w5.prevValid = true;
        } else {
            ++g_w5.cnt4;  // Ritt vor B beendet
        }
        if (d == 300) {
            w5Shot("wand-300");
        }
        if (d >= 600) {
            c.tapB();
            w5Step(c, 3);
        }
        return false;
    }
    case 3:
        if (w5WaitGone(c, 900)) {
            const uint32_t stag = g_wp5.staggerSkip - g_w5.base0;
            const uint32_t lost = g_stats.lostFrames - g_w5.base1;
            const bool ok = g_w5.note[0] == '\0' && g_w5.cnt2 == 0 && g_w5.cnt3 == 0 && g_w5.cnt4 == 0 && stag == 0 &&
                            lost == 0 && g_w5.cnt0 == 0;
            w5Result(ok,
                "staggerSkip=%u lostFrames(setRideOff)=%u loslassen=%u nichtFlyAttack=%u durchdringung=%u "
                "dyawFLY: ticks!=0 %u max %.2f Grad | dyawATTACK: ticks!=0 %u max %.2f Grad | rittVorB=%u notFly=%u "
                "birds=%u %s",
                stag, lost, g_wp5.letGo - g_w5.base2, g_w5.cnt2, g_w5.cnt3, g_w5.cnt0,
                static_cast<double>(w5Deg(g_w5.i0)), g_w5.cnt1, static_cast<double>(w5Deg(g_w5.i1)), g_w5.cnt4,
                g_stats.notFly - g_w5.base3, w5Birds(), g_w5.note);
            return true;
        }
        return false;
    case 10: {
        // 5.16: nach dem STAGGER bis birds=0 messen.
        const uint32_t d = c.frame - g_w5.stepT;
        if (d == 30) {
            w5Shot("abwurf-30");
        }
        if (d == 150) {
            w5Shot("abwurf-150");
        }
        if (c.sess.phase != Phase::Riding && g_w5.endT == 0) {
            g_w5.endT = c.frame;
            logInfo("wp5: Ritt-Ende %u Ticks nach dem STAGGER (Phase %s)", d, phaseName(c.sess.phase));
        }
        if (g_w5.goneT == 0 && c.sess.phase != Phase::Riding && w5Birds() == 0 && c.sess.birdId == kNoProc) {
            g_w5.goneT = c.frame;
            logInfo("wp5: birds=0 %u Ticks nach dem STAGGER", d);
        }
        if ((g_w5.goneT != 0 && d >= 200) || d >= 900) {
            const uint32_t gone = g_w5.goneT != 0 ? g_w5.goneT - g_w5.stepT : 0;
            const bool ok = g_w5.goneT != 0 && gone <= 150 && g_wp5.letGo - g_w5.base2 <= 1;
            w5Result(ok,
                "rittEndeNach=%u birds0Nach=%u loslassen=%u lostFrames=%u staggerSkip=%u birds=%u (Grenze 150 Ticks)",
                g_w5.endT != 0 ? g_w5.endT - g_w5.stepT : 0, gone, g_wp5.letGo - g_w5.base2,
                g_stats.lostFrames - g_w5.base1, g_wp5.staggerSkip - g_w5.base0, w5Birds());
            return true;
        }
        return false;
    }
    default:
        return true;
    }
}

// ---- 5.7 stagger (F_SP121, KFF_TEST_STAGGER=1)
bool w5Stagger(autotest::Context& c) {
    daKago_c* bird = liveBird();
    static const uint32_t kAt[4] = {150, 300, 450, 600};
    switch (g_w5.step) {
    case 0:
        if (w5CallUntilRide(c)) {
            g_w5.base0 = g_wp5.staggerSkip;
            g_w5.base1 = g_stats.lostFrames;
            w5Step(c, 1);
        }
        return g_w5.done;
    case 1: {
        if (bird == nullptr || c.sess.phase != Phase::Riding) {
            ++g_w5.cnt4;
            if (c.frame - g_w5.stepT > 30 && c.sess.phase != Phase::Riding) {
                w5Step(c, 2);
            }
            return false;
        }
        const uint32_t rf = c.sess.rideFrames;
        // Ereignisse: idx = naechster erzwungener Stagger; Messfenster 30 Ticks danach.
        if (g_w5.probeIdx < 4 && rf >= kAt[g_w5.probeIdx] && g_w5.probeT == 0) {
            g_w5.probeT = c.frame;
            g_w5.prevYaw = bird->current.angle.y;
            g_w5.f0 = bird->current.pos.y;
            g_w5.flag1 = false;  // FLY gesehen
            logInfo("wp5: STAGGER %u (Ritt-Frame %u) beobachtet: Aktion %d Modus %d", g_w5.probeIdx + 1, rf, bird->mAction,
                bird->mMode);
            if (g_w5.probeIdx == 0) {
                w5Shot("stagger-1");
            }
        }
        if (bird->mAction == daKago_c::ACTION_WAIT_e) {
            if (++g_w5.offRun > g_w5.cnt5) {
                g_w5.cnt5 = g_w5.offRun;
            }
        } else {
            g_w5.offRun = 0;
        }
        if (g_w5.probeT != 0) {
            const uint32_t d = c.frame - g_w5.probeT;
            const bool steer = g_w5.probeIdx == 0 || g_w5.probeIdx == 2;
            if (steer) {
                c.stick(1.0f, 0.0f);
            }
            if (!g_w5.flag1 && bird->mAction == daKago_c::ACTION_FLY_e) {
                g_w5.flag1 = true;
                if (d > g_w5.cnt0) {
                    g_w5.cnt0 = d;  // laengste Erholung bis FLY (Ticks)
                }
            }
            if (d >= 30) {
                if (!g_w5.flag1) {
                    g_w5.cnt0 = 999;
                }
                if (steer) {
                    const int16_t dy = w5AbsS(static_cast<int16_t>(bird->current.angle.y - g_w5.prevYaw));
                    const float deg = w5Deg(dy);
                    if (g_w5.cnt1 == 0 || deg < g_w5.f1) {
                        g_w5.f1 = deg;  // kleinste Gieraenderung mit Stick
                    }
                    ++g_w5.cnt1;
                    logInfo("wp5: STAGGER %u: Gieraenderung mit Stick in 30 Ticks %.1f Grad", g_w5.probeIdx + 1,
                        static_cast<double>(deg));
                } else {
                    const float rise = bird->current.pos.y - g_w5.f0;
                    if (g_w5.cnt2 == 0 || rise > g_w5.f2) {
                        g_w5.f2 = rise;  // groesster Anstieg ohne Stick
                    }
                    ++g_w5.cnt2;
                    logInfo("wp5: STAGGER %u: Hoehenaenderung ohne Stick in 30 Ticks %.1f", g_w5.probeIdx + 1,
                        static_cast<double>(rise));
                }
                ++g_w5.probeIdx;
                g_w5.probeT = 0;
            }
        }
        if (rf >= 700) {
            c.tapB();
            w5Step(c, 2);
        }
        return false;
    }
    case 2:
        if (w5WaitGone(c, 900)) {
            const uint32_t skip = g_wp5.staggerSkip - g_w5.base0;
            const bool ok = g_wt.staggerForced == 4 && skip >= 4 && g_w5.cnt0 <= 30 && g_w5.f1 >= 5.0f &&
                            g_w5.f2 <= 1000.0f && g_w5.cnt5 <= 30 && g_w5.cnt4 == 0;
            w5Result(ok,
                "erzwungen=%u staggerSkip=%u erholungMax=%u Ticks gierMinMitStick=%.1f Grad (n=%u) anstiegMaxOhneStick=%.1f "
                "(n=%u) waitMax=%u rittVorB=%u lostFrames=%u birds=%u",
                g_wt.staggerForced, skip, g_w5.cnt0, static_cast<double>(g_w5.f1), g_w5.cnt1,
                static_cast<double>(g_w5.f2), g_w5.cnt2, g_w5.cnt5, g_w5.cnt4, g_stats.lostFrames - g_w5.base1, w5Birds());
            return true;
        }
        return false;
    default:
        return true;
    }
}

// ---- 5.8 lava (F_SP114 Raum 0: Eiswasser-Ruecksprung, KFF_WP5_OHNE=boden: Wasser ist fuer freiheit keine Flaeche)
bool w5Lava(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame < 120 || link == nullptr || dComIfGp_event_runCheck()) {
            return false;
        }
        if (!w5ProbeWater(link)) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "kein Wasser in der Sonde");
        }
        w5Step(c, 1);
        return false;
    case 1:
        if (w5CallUntilRide(c)) {
            g_w5.base1 = g_wp5.lavaSkip;
            g_w5.base2 = g_wp5.swimBlocked;
            w5Step(c, 2);
        }
        return g_w5.done;
    case 2: {
        daKago_c* bird = liveBird();
        if (c.sess.rideFrames >= 30 && bird != nullptr) {
            if (g_w5.haveWater) {
                w5Teleport(bird, g_w5.px, g_w5.pw + 400.0f, g_w5.pz);
            }
            w5Step(c, 3);
        }
        return false;
    }
    case 3: {
        daKago_c* lb = liveBird();
        c.stick(0.5f, lb != nullptr ? w5PitchStick(lb, 75.0f, 1.0f) : 0.0f);  // Tauchen: Nase steil nach unten
        const uint32_t d = c.frame - g_w5.stepT;
        if (link != nullptr && c.sess.phase == Phase::Riding) {
            const float w = waterBelow(link->current.pos, 1000.0f);
            if (w != -G_CM3D_F_INF) {
                const float clear = link->current.pos.y - w;
                if (clear < g_w5.minA) {
                    g_w5.minA = clear;
                }
                if (clear < 0.0f) {
                    ++g_w5.overA;  // Ticks mit Link unter der Wasserflaeche
                }
            }
            if (w5SwimProc(link)) {
                ++g_w5.swimA;
            }
            if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
                ++g_w5.procOffA;
            }
        } else {
            ++g_w5.cnt4;
        }
        if (d == 150) {
            w5Shot("lava-tauchen");
        }
        if (d >= 300) {
            c.tapB();
            g_w5.bT = c.frame;
            w5Step(c, 4);
        }
        return false;
    }
    case 4:
        if (c.sess.phase != Phase::Riding) {
            g_w5.endT = c.frame;
            g_w5.id0 = link != nullptr ? fopAcM_GetID(link) : kNoProc;
            w5Shot("lava-ab");
            w5Step(c, 5);
        } else if (c.frame - g_w5.bT > 600) {
            w5Step(c, 5);
        }
        return false;
    case 5: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (link != nullptr) {
            if (link->checkCargoCarry() || link->mProcID == daAlink_c::PROC_WOLF_CARGO_CARRY) {
                ++g_w5.cnt0;  // Cargo-Zombie nach dem Ritt
            }
            if (link->mProcID == daAlink_c::PROC_SWIM_FREEZE_RETURN && !g_w5.flag1) {
                g_w5.flag1 = true;
                logInfo("wp5: nach dem Ritt: Eiswasser-Ruecksprung (Proc 0x%x) %u Ticks nach dem Abstieg",
                    static_cast<unsigned>(link->mProcID), d);
            }
            if (fopAcM_GetID(link) != g_w5.id0 && !g_w5.flag2) {
                g_w5.flag2 = true;
                logInfo("wp5: nach dem Ritt: neuer Link-Akteur (Raum-Neustart) %u Ticks nach dem Abstieg", d);
            }
        }
        if (d == 200) {
            w5Shot("lava-nach");
        }
        if (d >= 600) {
            // Kein Raum-Neustart im Ritt: der Ritt endet erst durch B (rittVorB 0), Link bleibt im Proc 312, kein Schwimmen,
            // nach dem Ritt kein Cargo-Zombie und kein eigener Vogel. Ruecksprung/Neustart nach dem Ritt ist Vanilla.
            const bool ok = g_w5.cnt4 == 0 && g_w5.swimA == 0 && g_w5.procOffA == 0 && g_w5.cnt0 == 0 && w5Birds() == 0;
            w5Result(ok,
                "wasserpunkt=%d minAbstand=%.1f unterWasserTicks=%u schwimmProcs=%u procNicht312=%u rittVorB=%u "
                "lavaSkip=%u swimBlocked=%u eisRuecksprungNachRitt=%d neustartNachRitt=%d cargoZombie=%u "
                "birds=%u %s",
                g_w5.haveWater ? 1 : 0, static_cast<double>(g_w5.minA), g_w5.overA, g_w5.swimA, g_w5.procOffA, g_w5.cnt4,
                g_wp5.lavaSkip - g_w5.base1, g_wp5.swimBlocked - g_w5.base2,
                g_w5.flag1 ? 1 : 0, g_w5.flag2 ? 1 : 0, g_w5.cnt0, w5Birds(), g_w5.note);
            return true;
        }
        return false;
    }
    default:
        return true;
    }
}

// ---- 5.9 fluss (F_SP112 Raum 1, Ebene 13/14): Tag_KagoFall im Ritt
struct W5Tags {
    fpc_ProcID ids[16];
    uint32_t n;
};

int w5TagCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    W5Tags* t = static_cast<W5Tags*>(data);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_Tag_KagoFall_e && t->n < 16) {
        t->ids[t->n++] = fopAcM_GetID(ac);
    }
    return 1;
}

W5Tags w5FindTags() {
    W5Tags t{};
    t.n = 0;
    fopAcIt_Executor(w5TagCb, &t);
    return t;
}

daTagKagoFall_c* w5Tag(fpc_ProcID id) {
    fopAc_ac_c* ac = nullptr;
    if (id == kNoProc || fopAcM_SearchByID(id, &ac) == 0 || ac == nullptr) {
        return nullptr;
    }
    return static_cast<daTagKagoFall_c*>(ac);
}

// Lage von p im Kasten des Tags (wie actionWaitFall: Translation und Y-Drehung invers).
bool w5InTagBox(daTagKagoFall_c* tag, const cXyz& p) {
    const float dx = p.x - tag->current.pos.x;
    const float dy = p.y - tag->current.pos.y;
    const float dz = p.z - tag->current.pos.z;
    const float s = cM_ssin(tag->shape_angle.y);
    const float co = cM_scos(tag->shape_angle.y);
    const float lx = dx * co - dz * s;
    const float lz = dx * s + dz * co;
    return dy <= tag->scale.y && dy >= 0.0f && std::fabs(lx) <= tag->scale.x && std::fabs(lz) <= tag->scale.z;
}

bool w5Fluss(autotest::Context& c) {
    daAlink_c* link = liveLink();
    static W5Tags tags{};
    // Vorrichtung vor dem Ritt (nur Testbuild): der Zaehler "nicht getragen" der Tags bleibt voll, damit die Szene nicht
    // vor dem Ruf neu startet. Ab Ritt-Start wirkt nur noch der Pre-SKIP des Moduls.
    if (g_w5.step <= 1) {
        for (uint32_t i = 0; i < tags.n; ++i) {
            if (daTagKagoFall_c* t = w5Tag(tags.ids[i])) {
                t->mNoCarryTimer = 150;
            }
        }
    }
    switch (g_w5.step) {
    case 0:
        if (c.frame < 5 || link == nullptr) {
            return false;
        }
        tags = w5FindTags();
        logInfo("wp5: Fluss: %u Tag_KagoFall gefunden, Buehne %s, Link (%.0f,%.0f,%.0f)", tags.n,
            dComIfGp_getStartStageName(), static_cast<double>(link->current.pos.x),
            static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z));
        for (uint32_t i = 0; i < tags.n; ++i) {
            if (daTagKagoFall_c* t = w5Tag(tags.ids[i])) {
                logInfo("wp5: Tag %u: id 0x%x Modus %u Zustand %u pos (%.0f,%.0f,%.0f) Kasten (%.0f,%.0f,%.0f) Gier %d "
                        "Zaehler %d",
                    i, static_cast<unsigned>(tags.ids[i]), static_cast<unsigned>(t->mActionMode),
                    static_cast<unsigned>(t->mActionState), static_cast<double>(t->current.pos.x),
                    static_cast<double>(t->current.pos.y), static_cast<double>(t->current.pos.z),
                    static_cast<double>(t->scale.x), static_cast<double>(t->scale.y), static_cast<double>(t->scale.z),
                    static_cast<int>(t->shape_angle.y), static_cast<int>(t->mNoCarryTimer));
                if (t->mActionMode == daTagKagoFall_c::ACTION_MODE_FALL && g_w5.id0 == kNoProc) {
                    g_w5.id0 = tags.ids[i];
                }
                if (g_w5.id1 == kNoProc) {
                    g_w5.id1 = tags.ids[i];
                }
            }
        }
        if (tags.n == 0) {
            w5Result(false, "kein Tag_KagoFall in %s (Ebene ohne Kargarok-Abschnitt?)", dComIfGp_getStartStageName());
            return true;
        }
        std::snprintf(g_w5.note, sizeof(g_w5.note), "%s", dComIfGp_getStartStageName());
        w5Step(c, 1);
        return false;
    case 1:
        if (w5CallUntilRide(c)) {
            g_w5.base0 = g_wp5.riverTagSkip;
            g_w5.id1 = g_w5.id0 != kNoProc ? g_w5.id0 : g_w5.id1;
            g_w5.i0 = link != nullptr ? static_cast<int>(fopAcM_GetID(link)) : -1;
            w5Step(c, 2);
        }
        return g_w5.done;
    case 2: {
        const uint32_t rf = c.sess.rideFrames;
        if (w5Once(0x1, rf >= 5) || w5Once(0x2, rf >= 15)) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));
        }
        daKago_c* bird = liveBird();
        daTagKagoFall_c* tag = w5Tag(g_w5.id1);
        if (bird == nullptr || link == nullptr || tag == nullptr) {
            ++g_w5.cnt4;
            if (c.frame - g_w5.stepT > 300) {
                w5Step(c, 3);
            }
            return false;
        }
        if (rf < 30) {
            return false;
        }
        // Link in die Kastenmitte: Vogel um Links Versatz verschieben (jeder Tick).
        const float cx = tag->current.pos.x;
        const float cy = tag->current.pos.y + (tag->mActionMode == daTagKagoFall_c::ACTION_MODE_FALL ? tag->scale.y * 0.5f
                                                                                                      : 300.0f);
        const float cz = tag->current.pos.z;
        const float ox = bird->current.pos.x - link->current.pos.x;
        const float oy = bird->current.pos.y - link->current.pos.y;
        const float oz = bird->current.pos.z - link->current.pos.z;
        w5Teleport(bird, cx + ox, cy + oy, cz + oz);
        if (g_w5.cnt0 == 0) {
            g_w5.f0 = static_cast<float>(c.frame);
            logInfo("wp5: Fluss: Vogel so gesetzt, dass Link in der Kastenmitte haengt (%.0f,%.0f,%.0f), Tag Modus %u",
                static_cast<double>(cx), static_cast<double>(cy), static_cast<double>(cz),
                static_cast<unsigned>(tag->mActionMode));
        }
        ++g_w5.cnt0;
        if (tag->mActionMode == daTagKagoFall_c::ACTION_MODE_FALL && w5InTagBox(tag, link->current.pos)) {
            ++g_w5.cnt1;  // Ticks mit Link im Kasten
        }
        if (tag->mActionState != 0) {
            ++g_w5.cnt2;
        }
        if (fopAcM_GetID(link) != static_cast<fpc_ProcID>(g_w5.i0)) {
            ++g_w5.cnt3;
        }
        if (c.sess.phase != Phase::Riding) {
            ++g_w5.cnt4;
        }
        if (g_w5.cnt0 == 75) {
            w5Shot("fluss-tag");
        }
        if (g_w5.cnt0 >= 150) {
            g_w5.flag0 = std::strcmp(dComIfGp_getStartStageName(), g_w5.note) == 0;
            c.tapB();
            w5Step(c, 3);
        }
        return false;
    }
    case 3:
        if (w5WaitGone(c, 900)) {
            const uint32_t skip = g_wp5.riverTagSkip - g_w5.base0;
            const bool ok = g_w5.cnt0 >= 150 && skip > 0 && g_w5.cnt2 == 0 && g_w5.cnt3 == 0 && g_w5.cnt4 == 0 &&
                            g_w5.flag0;
            w5Result(ok,
                "tags=%u gepinnt=%u imKasten=%u tagZustandNicht0=%u tagSkip=%u linkNeu=%u rittVorB=%u "
                "buehneGleich=%d (%s) birds=%u",
                tags.n, g_w5.cnt0, g_w5.cnt1, g_w5.cnt2, skip, g_w5.cnt3, g_w5.cnt4,
                g_w5.flag0 ? 1 : 0, g_w5.note, w5Birds());
            return true;
        }
        return false;
    default:
        return true;
    }
}

// ---- 5.10 szenentyp (F_SP121): Fluss-Bedingungen im Abflug, der Kern-Waechter setzt mSceneType vor executeWait auf 3
bool w5Szenentyp(autotest::Context& c) {
    daAlink_c* link = liveLink();
    daKago_c* bird = liveBird();
    dSv_restart_c& rs = g_dComIfG_gameInfo.info.getRestart();
    switch (g_w5.step) {
    case 0:
        if (w5CallUntilRide(c)) {
            w5Step(c, 1);
        }
        return g_w5.done;
    case 1:
        if (c.sess.rideFrames >= 120) {
            c.tapB();
            w5Step(c, 2);
        }
        return false;
    case 2:
        if (c.sess.phase == Phase::Dismounting && c.sess.departFrames >= 10 && bird != nullptr) {
            const char* env = std::getenv("KFF_TEST_FLAG6DF");
            g_w5.savedLastMode = rs.getLastMode();
            g_w5.savedLastSpeed = rs.getLastSpeedF();
            g_w5.savedLastAngle = rs.getLastAngleY();
            rs.setLastSceneInfo(g_w5.savedLastSpeed, 10, g_w5.savedLastAngle);
            bird->setActionMode(daKago_c::ACTION_WAIT_e, 0);
            bird->field_0x6df = 0;
            bird->mSceneType = 1;
            g_wt.riverProbe = true;
            g_w5.base0 = g_wt.sceneGuardSeen;
            logInfo("wp5: Fluss-Bedingungen gesetzt (KFF_TEST_FLAG6DF=%s): Vogel WAIT, 6df 0, mSceneType 1, lastMode 10 "
                    "(vorher %u), Abflug-Tick %u",
                env != nullptr ? env : "(nicht gesetzt)", g_w5.savedLastMode, c.sess.departFrames);
            w5Shot("szenentyp");
            w5Step(c, 3);
        } else if (c.frame - g_w5.stepT > 900) {
            w5Result(false, "kein Abflug nach B");
            return true;
        }
        return false;
    case 3: {
        const uint32_t d = c.frame - g_w5.stepT;
        if (link != nullptr && link->checkCargoCarry()) {
            ++g_w5.cnt0;  // Vanilla-Ritt (Link wieder im Cargo)
        }
        if (bird != nullptr) {
            if (bird->mAction == daKago_c::ACTION_DEMO_FLY_e || bird->mAction == daKago_c::ACTION_DEMO_FLY2_e ||
                bird->mAction == daKago_c::ACTION_LANDING_e) {
                ++g_w5.cnt1;  // Fluss-/See-Zweig von executeWait gelaufen
            }
            if (bird->mSceneType != 3) {
                ++g_w5.cnt2;
            }
        }
        const bool gone = c.sess.birdId == kNoProc && w5Birds() == 0;
        if ((gone && d >= 30) || d >= 600) {
            g_wt.riverProbe = false;
            rs.setLastSceneInfo(g_w5.savedLastSpeed, g_w5.savedLastMode, g_w5.savedLastAngle);
            const uint32_t seen = g_wt.sceneGuardSeen - g_w5.base0;
            const bool ok = seen > 0 && g_wt.sceneGuardMiss == 0 && g_w5.cnt0 == 0 && g_w5.cnt1 == 0 && gone;
            w5Result(ok,
                "waechterGesehen=%u waechterFehlt=%u vanillaRittTicks=%u flussZweigTicks=%u szenentypNicht3NachTick=%u "
                "birds=%u lastMode zurueck=%u",
                seen, g_wt.sceneGuardMiss, g_w5.cnt0, g_w5.cnt1, g_w5.cnt2, w5Birds(), rs.getLastMode());
            return true;
        }
        return false;
    }
    default:
        return true;
    }
}

// ---- 5.12 pfeil (F_SP121): Pfeiltreffer im Ritt, einmal an Link, einmal am Vogel.
// Echte Gegnerpfeile (E_ARROW) sind im PC-Port als Testmittel nicht nutzbar: w5-pfeil-1 bis -4 endeten beim Zeichnen
// eines E_ARROW ohne Schuetzen mit SIGABRT im Renderer (aurora push_indices/handle_aurora), w5-pfeil-4 auch ohne Ritt
// (Bodenpfeil am Boden). Der Test nimmt deshalb den Kollisionsweg eines Pfeils: einen AT vom Typ AT_TYPE_ARROW (atp 2,
// Gruppe Spieler) wie ein Pfeil im Treffer-Frame, Halter Midna. daKago_c hat keinen TG-Zylinder (nur die AT-Kugel mSph),
// ein Pfeil am Vogel darf also nichts treffen und nichts am Ritt aendern.
bool w5Pfeil(autotest::Context& c) {
    daAlink_c* link = liveLink();
    daKago_c* bird = liveBird();
    switch (g_w5.step) {
    case 0:
        if (c.frame >= 120 && w5Calm(c)) {
            w5Step(c, 1);
        }
        return false;
    case 1:
        if (w5CallUntilRide(c)) {
            g_w5.base1 = g_stats.lostFrames;
            w5Step(c, 2);
        }
        return g_w5.done;
    case 2: {
        const uint32_t rf = c.sess.rideFrames;
        w5TrackHits(c);
        if (bird == nullptr || link == nullptr || c.sess.phase != Phase::Riding) {
            ++g_w5.cnt4;
            if (c.frame - g_w5.stepT > 30 && c.sess.phase != Phase::Riding) {
                w5Step(c, 3);
            }
            return false;
        }
        if (bird->mAction != daKago_c::ACTION_FLY_e && bird->mAction != daKago_c::ACTION_ATTACK_e) {
            ++g_w5.cnt2;
        }
        if (link->mProcID != daAlink_c::PROC_WOLF_CARGO_CARRY) {
            ++g_w5.cnt3;
        }
        if (w5Once(0x200, rf >= 120)) {
            g_w5.lifeA = static_cast<int>(dComIfGs_getLife());
            g_w5.base0 = g_wt.tgHit;
            g_wt.groundHitArmed = true;  // naechster Link-Frame: Pfeil-AT an Link (testLinkTop)
            logInfo("wp5: Pfeil-AT auf Link im Ritt angefordert (Ritt-Frame %u, Leben %d, Proc 0x%x)", rf, g_w5.lifeA,
                static_cast<unsigned>(link->mProcID));
        }
        if (w5Once(0x40, rf >= 124)) {
            w5Shot("pfeil-link");
        }
        if (w5Once(0x80, rf >= 200)) {
            g_w5.lifeB = static_cast<int>(dComIfGs_getLife());
            g_w5.cnt0 = g_wt.tgHit - g_w5.base0;
            logInfo("wp5: Pfeil auf Link: TG-Treffer %u, Leben %d -> %d, Proc 0x%x, Vogel-Aktion %d", g_w5.cnt0, g_w5.lifeA,
                g_w5.lifeB, static_cast<unsigned>(link->mProcID), bird->mAction);
        }
        if (w5Once(0x1000, rf >= 240)) {
            g_w5.base2 = g_wt.tgHit;
            g_w5.base3 = g_wp5.staggerSkip;
            g_w5.i0 = bird->mAction;
            g_wt.birdAtHit = -1;
            g_wt.birdHitArmed = true;  // naechster Link-Frame: Pfeil-AT am Vogel, Frame danach Treffer-Pruefung
            logInfo("wp5: Pfeil-AT auf den Vogel angefordert (Ritt-Frame %u, Vogel-Aktion %d, Leben %d)", rf, bird->mAction,
                static_cast<int>(dComIfGs_getLife()));
        }
        if (w5Once(0x100, rf >= 244)) {
            w5Shot("pfeil-vogel");
        }
        if (w5Once(0x2000, rf >= 300)) {
            logInfo("wp5: Pfeil auf den Vogel: Treffer %d (Ziel %d), Vogel-Aktion %d -> %d, Stagger-Skip %u, Leben %d",
                g_wt.birdAtHit, g_wt.birdAtHitName, g_w5.i0, bird->mAction, g_wp5.staggerSkip - g_w5.base3,
                static_cast<int>(dComIfGs_getLife()));
        }
        if (rf >= 400) {
            g_w5.lifeC = static_cast<int>(dComIfGs_getLife());
            g_w5.cnt1 = g_wt.tgHit - g_w5.base2;
            g_w5.cnt5 = g_wp5.staggerSkip - g_w5.base3;
            c.tapB();
            w5Step(c, 3);
        }
        return false;
    }
    case 3:
        w5TrackHits(c);
        if (w5WaitGone(c, 900)) {
            const bool linkOk = g_w5.cnt0 >= 1 && g_w5.lifeB < g_w5.lifeA;
            const bool birdOk = g_wt.birdAtHit == 0 && g_w5.cnt1 == 0 && g_w5.lifeC == g_w5.lifeB && g_w5.cnt5 == 0;
            const bool rideOk = g_w5.cnt2 == 0 && g_w5.cnt3 == 0 && g_w5.cnt4 == 0 && w5Birds() == 0;
            w5Result(linkOk && birdOk && rideOk,
                "Link: treff=%u leben=%d->%d | Vogel: atTreffer=%d ziel=%d treffLink=%u leben=%d->%d staggerSkip=%u | "
                "vogelNichtFlyAttack=%u linkNicht312=%u rittVorB=%u lostFrames=%u birds=%u (Pfeil-AT statt E_ARROW)",
                g_w5.cnt0, g_w5.lifeA, g_w5.lifeB, g_wt.birdAtHit, g_wt.birdAtHitName, g_w5.cnt1, g_w5.lifeB, g_w5.lifeC,
                g_w5.cnt5, g_w5.cnt2, g_w5.cnt3, g_w5.cnt4, g_stats.lostFrames - g_w5.base1, w5Birds());
            return true;
        }
        return false;
    default:
        return true;
    }
}

// ---- 5.13 eym (F_SP121): E_YM wird geloescht, waehrend der Vogel darauf gesperrt ist; der Vogel wird entfernt, waehrend
// ein E_YM ihn als mpKago haelt.
bool w5Eym(autotest::Context& c) {
    daAlink_c* link = liveLink();
    daKago_c* bird = liveBird();
    switch (g_w5.step) {
    case 0:
        if (w5CallUntilRide(c)) {
            w5Step(c, 1);
        }
        return g_w5.done;
    case 1:
        if (bird != nullptr && link != nullptr && c.sess.rideFrames >= 60) {
            float fx = 0.0f;
            float fz = 0.0f;
            w5Fwd(bird->current.angle.y, fx, fz);
            cXyz p(bird->current.pos.x + fx * 500.0f, bird->current.pos.y - 100.0f, bird->current.pos.z + fz * 500.0f);
            csXyz a(0, 0, 0);
            g_w5.id0 = fopAcM_create(fpcNm_E_YM_e, 0xFFFFFFFFu, &p, fopAcM_GetRoomNo(link), &a, nullptr, -1);
            logInfo("wp5: E_YM #1 angelegt (id 0x%x)", static_cast<unsigned>(g_w5.id0));
            w5Step(c, 2);
        }
        return false;
    case 2: {
        fopAc_ac_c* ym = nullptr;
        if (bird != nullptr && g_w5.id0 != kNoProc && fopAcM_SearchByID(g_w5.id0, &ym) != 0 && ym != nullptr &&
            c.frame - g_w5.stepT >= 10) {
            bird->setLockActor(ym);
            logInfo("wp5: Vogel auf E_YM #1 gesperrt (setLockActor), E_YM #1 wird jetzt geloescht");
            fopAcM_delete(g_w5.id0);
            w5Shot("eym-1");
            w5Step(c, 3);
        } else if (c.frame - g_w5.stepT > 240) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "E_YM #1 nicht erschienen");
            w5Step(c, 4);
        }
        return false;
    }
    case 3: {
        fopAc_ac_c* ym = nullptr;
        const bool exists = fopAcM_SearchByID(g_w5.id0, &ym) != 0 && ym != nullptr;
        if (bird != nullptr && !exists) {
            g_w5.flag0 = bird->getLockActor() == nullptr;
            logInfo("wp5: E_YM #1 entfernt %u Ticks nach fopAcM_delete, Lock des Vogels %s", c.frame - g_w5.stepT,
                g_w5.flag0 ? "geloest" : "HAENGT");
            w5Step(c, 4);
        } else if (c.frame - g_w5.stepT > 120) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "E_YM #1 nicht entfernt");
            w5Step(c, 4);
        }
        return false;
    }
    case 4:
        if (bird != nullptr && link != nullptr) {
            float fx = 0.0f;
            float fz = 0.0f;
            w5Fwd(bird->current.angle.y, fx, fz);
            cXyz p(bird->current.pos.x + fx * 800.0f, bird->current.pos.y - 100.0f, bird->current.pos.z + fz * 800.0f);
            csXyz a(0, 0, 0);
            g_w5.id1 = fopAcM_create(fpcNm_E_YM_e, 0xFFFFFFFFu, &p, fopAcM_GetRoomNo(link), &a, nullptr, -1);
            logInfo("wp5: E_YM #2 angelegt (id 0x%x)", static_cast<unsigned>(g_w5.id1));
        }
        w5Step(c, 5);
        return false;
    case 5: {
        fopAc_ac_c* ac = nullptr;
        if (bird != nullptr && g_w5.id1 != kNoProc && fopAcM_SearchByID(g_w5.id1, &ac) != 0 && ac != nullptr &&
            c.frame - g_w5.stepT >= 10) {
            static_cast<daE_YM_c*>(ac)->mpKago = bird;
            logInfo("wp5: E_YM #2 haelt den Vogel als mpKago, jetzt B (Vogel wird entfernt)");
            c.tapB();
            w5Shot("eym-2");
            w5Step(c, 6);
        } else if (c.frame - g_w5.stepT > 240) {
            std::snprintf(g_w5.note, sizeof(g_w5.note), "E_YM #2 nicht erschienen");
            c.tapB();
            w5Step(c, 6);
        }
        return false;
    }
    case 6:
        if (w5WaitGone(c, 900)) {
            fopAc_ac_c* ac = nullptr;
            bool exists2 = g_w5.id1 != kNoProc && fopAcM_SearchByID(g_w5.id1, &ac) != 0 && ac != nullptr;
            if (exists2) {
                g_w5.flag1 = static_cast<daE_YM_c*>(ac)->mpKago == nullptr;
                fopAcM_delete(g_w5.id1);
            }
            const bool ok = g_w5.flag0 && exists2 && g_w5.flag1 && w5Birds() == 0 && g_w5.note[0] == '\0';
            w5Result(ok, "lockGeloest=%d eym2Da=%d mpKagoGeloescht=%d birds=%u %s", g_w5.flag0 ? 1 : 0, exists2 ? 1 : 0,
                g_w5.flag1 ? 1 : 0, w5Birds(), g_w5.note);
            return true;
        }
        return false;
    default:
        return true;
    }
}

// ---- 5.14 mensch (F_SP103): Ruf als Mensch, mensch_ritt aus (Standard): "call rejected (human)", autowolf verwandelt
bool w5Mensch(autotest::Context& c) {
    daAlink_c* link = liveLink();
    switch (g_w5.step) {
    case 0:
        if (c.frame >= 300 && w5Calm(c) && link != nullptr) {
            g_w5.flag0 = link->checkWolf() == 0;
            logInfo("wp5: Mensch-Ruf: Link Wolf %d, Proc 0x%x", g_w5.flag0 ? 0 : 1, static_cast<unsigned>(link->mProcID));
            w5Step(c, 1);
        }
        return false;
    case 1:
        // Bis zu drei Rufe (300 Ticks auseinander). Ein Ritt ist nicht Pflicht: ohne Midna/Wolf-Faehigkeit lehnt autowolf
        // ab ("human form - ride needs wolf"); mit Verwandlung startet der Ritt und endet mit B.
        if (c.sess.phase == Phase::Riding) {
            g_w5.flag1 = true;
            logInfo("wp5: Mensch-Ruf: Ritt beginnt bei Tick %u (Link Wolf %d, Ruf-Versuche %u)", c.frame,
                link != nullptr && link->checkWolf() != 0 ? 1 : 0, g_w5.call.attempts);
            w5Step(c, 2);
            return false;
        }
        if (g_w5.call.attempts < 3 || g_w5.call.armed) {
            w5CallTick(c, g_w5.call);
        }
        if (c.frame - g_w5.stepT == 45) {
            w5Shot("mensch-ruf");
        }
        if (g_w5.call.attempts >= 3 && !g_w5.call.armed && c.frame > g_w5.call.start + 900 &&
            c.sess.phase == Phase::Idle) {
            logInfo("wp5: Mensch-Ruf: kein Ritt nach %u Rufen (Link Wolf %d)", g_w5.call.attempts,
                link != nullptr && link->checkWolf() != 0 ? 1 : 0);
            w5Step(c, 3);
        }
        return false;
    case 2:
        if (c.sess.rideFrames >= 150) {
            c.tapB();
            w5Step(c, 3);
        }
        return false;
    case 3:
        if (w5WaitGone(c, 900)) {
            w5Result(g_w5.flag0 && w5Birds() == 0 && c.sess.phase == Phase::Idle,
                "linkMenschBeimRuf=%d rufVersuche=%u ritt=%d birds=%u phase=%s (Pflicht im Log: 'mensch: call rejected "
                "(human)' oder 'human form - ride needs wolf')",
                g_w5.flag0 ? 1 : 0, g_w5.call.attempts, g_w5.flag1 ? 1 : 0, w5Birds(), phaseName(c.sess.phase));
            return true;
        }
        return false;
    default:
        return true;
    }
}

bool wp5Tick(autotest::Context& c) {
    if (g_w5.done) {
        return true;
    }
    if (c.frame >= kW5Deadline) {
        w5Result(false, "Zeitlimit %u Ticks (Schritt %d)", kW5Deadline, g_w5.step);
        return true;
    }
    switch (g_w5.kase) {
    case W5Case::Wasser:
        return w5Wasser(c);
    case W5Case::Seil:
        return w5Seil(c);
    case W5Case::Treffer:
        return w5Treffer(c);
    case W5Case::Schaden:
        return w5Schaden(c);
    case W5Case::Wand:
    case W5Case::Wandabwurf:
        return w5Wand(c);
    case W5Case::Stagger:
        return w5Stagger(c);
    case W5Case::Lava:
        return w5Lava(c);
    case W5Case::Fluss:
        return w5Fluss(c);
    case W5Case::Szenentyp:
        return w5Szenentyp(c);
    case W5Case::Reittiere:
        return w5Reittiere(c);
    case W5Case::Pfeil:
        return w5Pfeil(c);
    case W5Case::Eym:
        return w5Eym(c);
    case W5Case::Mensch:
        return w5Mensch(c);
    case W5Case::Unbekannt:
        break;
    }
    w5Result(false, "unbekannter Fall KFF_WP5 (wasser, seil, treffer, schaden, wand, stagger, lava, fluss, szenentyp, "
                    "reittiere, pfeil, eym, mensch, wandabwurf)");
    return true;
}

const autotest::Scenario kWp5Scenario = {
    "wp5",
    "WP5: Wasser, Gegner, Stagger, Procs, Reittiere (Fall ueber KFF_WP5)",
    wp5Tick,
};

void testInstall(Registry& reg, ModResult& worst) {
    g_w5.kase = w5CaseFromEnv();
    const ModResult a = reg.addPost(Hook::LinkCheckDamageAction, &onDamageActionTestPost, prio::kGriff);
    const ModResult b = reg.addPre(Hook::KagoExecute, &onKagoExecuteTestPre, prio::kGriff);
    if (a != MOD_OK) {
        logWarn("griff: Test-Hook checkDamageAction (Post) nicht eingetragen (%s)", modResultName(a));
        worst = a;
    }
    if (b != MOD_OK) {
        logWarn("griff: Test-Hook daKago_c::execute (Pre) nicht eingetragen (%s)", modResultName(b));
        worst = b;
    }
    const ModResult s = autotest::registerScenario(&kWp5Scenario);
    if (s != MOD_OK) {
        logWarn("griff: Szenario wp5 nicht registriert (%s)", modResultName(s));  // nur Test: Modul bleibt aktiv
    }
    logInfo("griff: WP5-Testteil installiert (Fall %s)", w5CaseName(g_w5.kase));
}
#endif  // KFF_DEV_AUTOTEST (WP5-Testteil)

GriffModule g_module;

}  // namespace

// WP5 Ruf-Gate (Whitelist), Aufruf aus ride.cpp callBlockReason nach "Link tot": englischer Grund ("riding", "transform",
// "water", "rope", "chain", "hang", "climb", "hookshot", "damage", "air") oder nullptr. Der Kern loggt
// "call rejected (<Grund>), B stays"; B bleibt beim Spiel. Keine Wirkung ausser Zaehler und Log.
const char* griffCallGateReason(daAlink_c* link) {
    if (link == nullptr) {
        return nullptr;
    }
    try {
#if defined(KFF_DEV_AUTOTEST)
        if (testGateProbeArmed()) {
            return testGateProbe(link);  // Testbuild: Gate-Probe (Proc/Reitstatus nur fuer diesen Aufruf gesetzt)
        }
        return testGateRecord(gateEvaluate(link));  // Testbuild: letzter Grund fuer das Szenario wp5
#else
        return gateEvaluate(link);
#endif
    } catch (...) {
        countException("Ruf-Gate");
        return nullptr;
    }
}

namespace modules {

Module& griff() {
    return g_module;
}

}  // namespace modules
}  // namespace kff
