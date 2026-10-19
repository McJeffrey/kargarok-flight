// Modul "autowolf" (Teil 1 der v2). Ruf als Mensch (R halten, B tippen) verwandelt Link zum Wolf. Nach
// Abschluss der Verwandlung ruft das Modul den freundlichen Kargarok (kff::summonBird), der Kern uebernimmt
// Aufsitzen und Ritt. Nach dem Absteigen (B) und der Landung kehrt Link automatisch in die Menschenform zurueck,
// wenn er vorher Mensch war und M_077 (Schattenkristall, Verwandlung freigeschaltet) nicht gesetzt ist.
// Sonst bleibt Link Wolf.
//
// Verwandlung ohne Pflicht-Ereignis: procCoMetamorphoseInit() startet bei isOrderOK() ein Ereignis mit
// NULL-Namen (d_a_alink.cpp:17421ff). Das Modul setzt dEvt_control_c::mEventStatus nur fuer den synchronen
// Aufruf auf 1 (isOrderOK() ist dann falsch, d_event.h:138) und stellt den alten Wert sofort wieder her.
//
// Stufen (der Kern besitzt Vogel und Ritt, siehe session.hpp):
//   Idle        keine Verwandlung dieses Moduls laeuft
//   ToWolf      Mensch -> Wolf laeuft (Phase Transforming, Rufe blockiert)
//   WolfSettle  Wolf erreicht, kurz stabil, dann kff::summonBird (Kern: Vogel, Aufsitzen)
//   ToHuman     Wolf -> Mensch laeuft (Phase Transforming)
// onRideEnd merkt die Rueckverwandlung vor. onFrameLink fuehrt sie aus, sobald Link am Boden steht und der Kern
// in Phase Idle ist (Vogel weg).
//
// Verbote fuer den Ruf: Ereignis, Dialog (Meldungsfenster), Wasser, Leiter, Klettern, Seil, Reiten
// (MODE_PLAYER_FLY wie bei der Schnellverwandlung), Magnetstiefel, Kettenkugel (Absturz laut
// d_a_alink_dusk.cpp), Sternzelt (R_SP161, ebenfalls dort), Sitz im Vogel, Luft. Eine laufende Verwandlung
// zum Wolf bricht bei Ereignis, Dialog, Wasser oder Klettern ab; die Rueckverwandlung wird dann vorgemerkt.
//
// Bewusst NICHT angewandt: checkMetamorphoseEnableBase() der Spielregeln (verlangt M_077 und Midna-Ritt,
// genau der Grund fuer dieses Modul). Das Modul setzt keine Story-Flags.
//
// Test-Szenarien (KFF_AUTOTEST=ordon bzw. ordon-m077, Stage F_SP103,0,0,-1, frischer Spielstand). Das
// Szenario legt nur Testvorbereitung an: Ordon-Schwert in Slot 0, B-Slot darauf, Schwert-Auswahl gesetzt.
// ordon-m077 setzt zusaetzlich M_077. Das Modul selbst aendert keine Items und keine Story-Flags.
#include "d/dolzel_rel.h"

#include "core/kff/kff.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdint>
#include <cstring>

namespace {

// Ticks = Link-Frames (ca. 30 pro Sekunde). Zaehlbasis (WP6, R4 geprueft): ein Link-Frame je Kern-Tick ohne Pause; in der
// Pause stehen die Akteure, dort zaehlt kein Waechter. Gemessen dauert die Verwandlung zum Wolf rund 70, die
// Rueckverwandlung rund 50 Ticks (ordon-2).
constexpr uint32_t kGroundSettleTicks = 30;   // Boden-Ticks, bevor die Rueckverwandlung startet
constexpr uint32_t kWolfSettleTicks = 30;     // Wolf stabil, bevor der Vogel gerufen wird
constexpr uint32_t kTransformWatchdog = 600;  // Verwandlung dauert zu lange: Abbruch (WP6: 900 -> 600)
constexpr uint32_t kWaitWatchdog = 600;       // Vogelruf wartet auf Dialog oder Boden: Abbruch (WP6: Phase Transforming
                                              // je Stufe hoechstens 600 Ticks; ein Ereignis bricht sofort ab)
constexpr uint32_t kRetryTicks = 60;          // Pause nach einer nicht gestarteten Rueckverwandlung
constexpr uint32_t kBlockLogEvery = 120;      // Log-Takt fuer "warte auf ..."

enum class AwStage : uint8_t { Idle, ToWolf, WolfSettle, ToHuman };

AwStage g_stage = AwStage::Idle;
uint32_t g_stageTicks = 0;             // Link-Ticks in der aktuellen Stufe
uint32_t g_groundTicks = 0;            // zusammenhaengende Link-Ticks auf dem Boden
uint32_t g_retryWait = 0;              // Link-Ticks bis zum naechsten Rueckverwandlungsversuch
uint32_t g_logBlock = 0;               // logEvery-Zaehler fuer Wartemeldungen
bool g_wasHuman = false;               // die laufende Sitzung hat als Mensch begonnen
bool g_revertPending = false;          // nach Ritt oder Abbruch zurueck in Menschenform
fpc_ProcID g_linkId = kff::kNoProc;    // Link, zu dem die laufende Verwandlung gehoert
uint32_t g_transforms = 0;             // gestartete Verwandlungen (beide Richtungen)
uint32_t g_summons = 0;                // Vogelrufe nach einer Verwandlung
uint32_t g_reverts = 0;                // abgeschlossene Rueckverwandlungen

const char* awStageName(AwStage stage) {
    switch (stage) {
    case AwStage::Idle:
        return "Idle";
    case AwStage::ToWolf:
        return "ToWolf";
    case AwStage::WolfSettle:
        return "WolfSettle";
    case AwStage::ToHuman:
        return "ToHuman";
    }
    return "?";
}

bool awM077() {
    return dComIfGs_isEventBit(dSv_event_flag_c::M_077) != 0;
}

// Ereignis laeuft (Zwischensequenz oder Ereignis-Modus, wie checkEventRun() in der Schnellverwandlung).
bool awEventBusy(daAlink_c* link) {
    return link->checkEventRun() || dComIfGp_event_runCheck();
}

// Meldungsfenster offen (Dialog). Status ungleich 0, wie in d_gameover.cpp:135 und touch_controls.cpp:201.
bool awDialogOpen() {
    return dComIfGp_getMesgStatus() != 0;
}

bool awBusy(daAlink_c* link) {
    return awEventBusy(link) || awDialogOpen();
}

// Proc-Bereiche Schwimmen, Leiter und Klettern (Enum daAlink_PROC, d_a_alink.h:1123-1153).
bool awProcBlock(daAlink_c* link) {
    const int proc = static_cast<int>(link->mProcID);
    return (proc >= daAlink_c::PROC_SWIM_UP && proc <= daAlink_c::PROC_SWIM_DAMAGE) ||
           (proc >= daAlink_c::PROC_LADDER_UP_START && proc <= daAlink_c::PROC_LADDER_MOVE) ||
           (proc >= daAlink_c::PROC_CLIMB_UP_START && proc <= daAlink_c::PROC_CLIMB_TO_ROOF);
}

// Gruende, die eine Verwandlung zum Wolf oder zurueck verbieten (nullptr = erlaubt). Die Liste folgt der
// Schnellverwandlung (d_a_alink_dusk.cpp handleQuickTransform) plus Ereignis und Dialog.
const char* awTransformBlock(daAlink_c* link) {
    if (link->mProcID == daAlink_c::PROC_DEAD) {
        return "Link tot";  // WP6: keine Verwandlung im Tod (Game Over)
    }
    if (awEventBusy(link)) {
        return "Ereignis";
    }
    if (awDialogOpen()) {
        return "Dialog";
    }
    if (!link->checkSwimUp()) {  // FLG0_SWIM_UP ist ueber Wasser gesetzt, siehe d_a_alink_dusk.cpp "not underwater"
        return "unter Wasser";
    }
    if (link->checkModeFlg(daAlink_c::MODE_PLAYER_FLY)) {
        return "Schwimmen, Seil, Klettern oder Reiten";
    }
    if (awProcBlock(link)) {
        return "Schwimmen, Leiter oder Klettern";
    }
    if (link->checkMagneBootsOn()) {
        return "Magnetstiefel";
    }
    if (link->mEquipItem == dItemNo_IRONBALL_e) {
        return "Kettenkugel (Absturz laut d_a_alink_dusk.cpp)";
    }
    if (std::strcmp(dComIfGp_getStartStageName(), "R_SP161") == 0) {
        return "Sternzelt";
    }
    if (link->checkCargoCarry()) {
        return "sitzt im Vogel";
    }
    if (link->checkMetamorphose()) {  // PROC_METAMORPHOSE und nicht in der Animations-Schleife
        return "Verwandlung laeuft";
    }
    if (!link->mLinkAcch.ChkGroundHit()) {
        return "in der Luft";
    }
    return nullptr;
}

// Gruende, die eine laufende Verwandlung zum Wolf abbrechen. Die Verwandlung selbst (PROC_METAMORPHOSE)
// zaehlt hier nicht, sonst wuerde jede Verwandlung sich selbst abbrechen.
const char* awInterruptBlock(daAlink_c* link) {
    if (awEventBusy(link)) {
        return "Ereignis";
    }
    if (awDialogOpen()) {
        return "Dialog";
    }
    if (!link->checkSwimUp()) {
        return "unter Wasser";
    }
    if (awProcBlock(link)) {
        return "Schwimmen, Leiter oder Klettern";
    }
    return nullptr;
}

// Gruende, die die Rueckverwandlung aufhalten (nullptr = jetzt moeglich).
const char* awRevertBlock(daAlink_c* link) {
    if (const char* why = awTransformBlock(link)) {
        return why;
    }
    if (g_groundTicks < kGroundSettleTicks) {
        return "Landung";
    }
    return nullptr;
}

// Verwandlung ohne Pflicht-Ereignis (Kopfkommentar). true = Link ist danach in PROC_METAMORPHOSE.
bool awStartTransform(daAlink_c* link) {
    dEvt_control_c* evt = dComIfGp_getEvent();
    const u8 saved = evt->mEventStatus;
    evt->mEventStatus = 1;  // isOrderOK() == false: kein Ereignis mit NULL-Namen
    link->procCoMetamorphoseInit();
    evt->mEventStatus = saved;
    return link->mProcID == daAlink_c::PROC_METAMORPHOSE;
}

void awLogLink(const char* tag, daAlink_c* link) {
    if (link == nullptr) {
        kff::logInfo("autowolf: %s link=NULL", tag);
        return;
    }
    kff::logInfo("autowolf: %s proc=%d wolf=%d cargo=%d event=%d dlg=%d ground=%d gndTicks=%u equip=%u m077=%d "
                 "phase=%s pos=(%.0f,%.0f,%.0f)",
        tag, static_cast<int>(link->mProcID), link->checkWolf() ? 1 : 0, link->checkCargoCarry() ? 1 : 0,
        awEventBusy(link) ? 1 : 0, awDialogOpen() ? 1 : 0, link->mLinkAcch.ChkGroundHit() ? 1 : 0,
        static_cast<unsigned>(g_groundTicks), static_cast<unsigned>(link->mEquipItem), awM077() ? 1 : 0,
        kff::phaseName(kff::session().phase), link->current.pos.x, link->current.pos.y, link->current.pos.z);
}

void awBegin(AwStage stage) {
    g_stage = stage;
    g_stageTicks = 0;
}

// WP6: die Sitzung nur freigeben, wenn sie noch die eigene Verwandlung ist (Phase Transforming, kein Vogel). Sonst
// wuerde ein Vogel, den der Kern inzwischen fuehrt, ohne Sitzung zurueckbleiben (verwaist).
void awReleaseSession() {
    const kff::Session& s = kff::session();
    if (s.phase == kff::Phase::Transforming && s.birdId == kff::kNoProc) {
        kff::resetSession();
    }
}

// Stufe ohne Vogel beenden. keepRevert: Link ist Wolf und war Mensch -> Rueckverwandlung vormerken.
void awAbort(const char* why, bool keepRevert) {
    kff::logInfo("autowolf: Abbruch (%s), Stufe %s nach %u Ticks", why, awStageName(g_stage), g_stageTicks);
    awBegin(AwStage::Idle);
    awReleaseSession();  // kein Vogel in diesen Stufen, Phase wird Idle
    g_revertPending = keepRevert && g_wasHuman;
    if (!g_revertPending) {
        g_wasHuman = false;
    }
}

void awTickToWolf(daAlink_c* link) {
    ++g_stageTicks;
    if (fopAcM_GetID(link) != g_linkId) {
        awAbort("Link gewechselt", true);
        return;
    }
    if (const char* why = awInterruptBlock(link)) {
        awAbort(why, true);
        return;
    }
    if (link->mProcID != daAlink_c::PROC_METAMORPHOSE) {
        if (link->checkWolf()) {
            awBegin(AwStage::WolfSettle);
            awLogLink("Wolf erreicht:", link);
        } else {
            awAbort("Verwandlung ohne Wolf beendet", true);
        }
        return;
    }
    if (g_stageTicks > kTransformWatchdog) {
        awAbort("Watchdog Verwandlung zum Wolf", true);
    }
}

void awTickWolfSettle(daAlink_c* link) {
    ++g_stageTicks;
    if (fopAcM_GetID(link) != g_linkId) {
        awAbort("Link gewechselt", true);
        return;
    }
    if (!link->checkWolf()) {
        awAbort("Wolf verloren", false);
        return;
    }
    if (link->mProcID == daAlink_c::PROC_METAMORPHOSE) {
        return;
    }
    if (awEventBusy(link)) {
        awAbort("Ereignis vor dem Vogelruf", true);  // WP6: kein Vogel in ein Ereignis, Rueckverwandlung danach
        return;
    }
    const bool ready = !awBusy(link) && !awProcBlock(link) && link->checkSwimUp() &&
                       link->mLinkAcch.ChkGroundHit() && g_stageTicks >= kWolfSettleTicks;
    if (!ready) {
        if (g_stageTicks > kWaitWatchdog) {
            awAbort("Watchdog: Vogelruf wartet auf Ereignis, Dialog oder Boden", true);
        }
        return;
    }
    if (kff::summonBird(link) != MOD_OK) {
        awAbort("Vogel nicht erzeugt", true);
        return;
    }
    ++g_summons;
    awBegin(AwStage::Idle);
    awLogLink("Vogel gerufen (Wolf stabil), Kern setzt Aufsitzen:", link);
}

void awTickToHuman(daAlink_c* link) {
    ++g_stageTicks;
    if (fopAcM_GetID(link) != g_linkId) {
        awAbort("Link gewechselt waehrend der Rueckverwandlung", true);
        return;
    }
    if (!link->checkWolf() && link->mProcID != daAlink_c::PROC_METAMORPHOSE) {
        ++g_reverts;
        awBegin(AwStage::Idle);
        awReleaseSession();
        g_revertPending = false;
        g_wasHuman = false;
        awLogLink("Rueckverwandlung abgeschlossen:", link);
        return;
    }
    // WP6: Ereignis oder Waechter brechen ab, die Rueckverwandlung bleibt vorgemerkt (sonst bliebe Link ohne M_077 Wolf).
    if (awEventBusy(link)) {
        awAbort("Ereignis waehrend der Rueckverwandlung", true);
        g_retryWait = kRetryTicks;
        return;
    }
    if (g_stageTicks > kTransformWatchdog) {
        awAbort("Watchdog Rueckverwandlung", true);
        g_retryWait = kRetryTicks;
    }
}

void awTickRevert(daAlink_c* link) {
    if (!link->checkWolf()) {
        g_revertPending = false;
        g_wasHuman = false;
        kff::logInfo("autowolf: Link ist bereits Mensch, keine Rueckverwandlung noetig");
        return;
    }
    if (awM077()) {
        g_revertPending = false;
        g_wasHuman = false;
        kff::logInfo("autowolf: M_077 gesetzt, Link bleibt Wolf");
        return;
    }
    if (g_retryWait > 0) {
        return;
    }
    if (const char* why = awRevertBlock(link)) {
        kff::logEvery(g_logBlock, 1, kBlockLogEvery, "autowolf: warte auf Rueckverwandlung (%s)", why);
        return;
    }
    g_linkId = fopAcM_GetID(link);
    kff::session().phase = kff::Phase::Transforming;
    awBegin(AwStage::ToHuman);
    ++g_transforms;
    if (!awStartTransform(link)) {
        awBegin(AwStage::Idle);
        awReleaseSession();
        g_retryWait = kRetryTicks;
        awLogLink("Rueckverwandlung nicht gestartet:", link);
        return;
    }
    awLogLink("Rueckverwandlung Wolf -> Mensch gestartet:", link);
}

void awFrameLink(kff::Session& s, daAlink_c* link) {
    if (g_retryWait > 0) {
        --g_retryWait;
    }
    if (link->mLinkAcch.ChkGroundHit()) {
        if (g_groundTicks < 0xFFFFFFFFu) {
            ++g_groundTicks;
        }
    } else {
        g_groundTicks = 0;
    }
    switch (g_stage) {
    case AwStage::ToWolf:
        awTickToWolf(link);
        break;
    case AwStage::WolfSettle:
        awTickWolfSettle(link);
        break;
    case AwStage::ToHuman:
        awTickToHuman(link);
        break;
    case AwStage::Idle:
        break;
    }
    if (g_stage == AwStage::Idle && g_revertPending && s.phase == kff::Phase::Idle) {
        awTickRevert(link);
    }
}

#if defined(KFF_DEV_AUTOTEST)
// ---- Test-Szenarien "ordon" und "ordon-m077" (KFF_AUTOTEST, Stage F_SP103,0,0,-1, frischer Spielstand) ----
// Ablauf: Mensch ruft (R halten, B tippen), Probe-Ruf waehrend der Verwandlung (muss blockiert sein), Wolf,
// Ritt (Stick), B zum Absteigen, Landung, Rueckverwandlung zum Menschen, B-Tipp in Menschenform (Schwert-Test).
// Variante ordon-m077: M_077 vorher gesetzt, Link muss nach dem Ritt Wolf bleiben (keine Rueckverwandlung).
enum class OrdonStep : uint8_t { Wait, Call, WaitRide, Fly, WaitHuman, WaitStay, SwordTest, Done };

constexpr uint32_t kOArmFrame = 240;     // frueheste Ruf-Pruefung (Autotest-Frames)
constexpr uint32_t kOWaitLimit = 6000;   // Wartezeit auf "bereit" (Szenario-Aufrufe)
constexpr uint32_t kOCallHold = 30;      // erster Ruf: R gehalten (Ticks)
constexpr uint32_t kOCallPress = 15;     // erster Ruf: B-Tipp in der Mitte des R-Halts
constexpr uint32_t kOProbeAt = 40;       // Probe-Ruf waehrend der Verwandlung: R-Halt Start
constexpr uint32_t kOProbePress = 45;    // Probe-Ruf: B-Tipp
constexpr uint32_t kOCallEnd = 60;       // ab hier wartet das Szenario auf den Ritt
constexpr uint32_t kORideLimit = 1500;   // Ritt muss innerhalb dieser Zeit starten
constexpr uint32_t kOStickRise = 150;    // Ritt-Frames: steigen
constexpr uint32_t kOStickLeft = 300;    // Ritt-Frames: links
constexpr uint32_t kODismount = 420;     // Ritt-Frame des Absteigens
constexpr uint32_t kOHumanLimit = 3600;  // Rueckverwandlung muss innerhalb dieser Zeit fertig sein
constexpr uint32_t kOHumanStable = 60;   // Ticks als Mensch, bevor der Schwert-Test beginnt
constexpr uint32_t kOStayTicks = 300;    // Variante M_077: so lange muss Link Wolf bleiben
constexpr uint32_t kOSwordFrames = 120;  // Beobachtung nach dem ersten B-Tipp in Menschenform
constexpr uint32_t kOSwordSecond = 60;   // zweiter B-Tipp (Schwertangriff), Ticks nach dem ersten

OrdonStep g_oStep = OrdonStep::Wait;
uint32_t g_oStart = 0;        // Autotest-Frame des Schrittbeginns
uint32_t g_oTicks = 0;        // Aufrufe des Szenarios (zaehlt auch ohne Link, Autotest-Frame bleibt dann 0)
uint32_t g_oHumanTicks = 0;   // Ticks in Folge als Mensch im Zustand Idle
uint32_t g_oStayTicks = 0;    // Ticks in Folge als Wolf im Zustand Idle (Variante M_077)
bool g_oVariantM077 = false;  // aktuelle Variante, von den TickFn-Huellen gesetzt
bool g_oSwordTapped = false;  // ersten B-Tipp des Schwert-Tests schon gesendet (Test beginnt bei k=1, nicht 0)

bool ordonHumanReady(kff::autotest::Context& c) {
    daAlink_c* link = kff::liveLink();
    return link != nullptr && !link->checkWolf() && c.sess.phase == kff::Phase::Idle &&
           c.sess.birdId == kff::kNoProc && awTransformBlock(link) == nullptr;
}

bool ordonStayWolf(kff::autotest::Context& c, daAlink_c* link) {
    if (link != nullptr && !link->checkWolf()) {
        awLogLink("FAIL (M_077):", link);
        c.note("FAIL: Rueckverwandlung trotz M_077");
        g_oStep = OrdonStep::Done;
        return true;
    }
    const bool settled = c.sess.phase == kff::Phase::Idle && link != nullptr && link->mLinkAcch.ChkGroundHit();
    g_oStayTicks = settled ? g_oStayTicks + 1 : 0;
    if (g_oStayTicks >= kOStayTicks) {
        awLogLink("Wolf bleibt:", link);
        c.note("PASS: Link bleibt Wolf nach dem Ritt (M_077 gesetzt)");
        c.note("timeline complete");
        g_oStep = OrdonStep::Done;
        return true;
    }
    if (c.frame - g_oStart > kOHumanLimit) {
        c.note("TIMEOUT: Wolf-Warten nach dem Ritt");
        awLogLink("Timeout:", link);
        g_oStep = OrdonStep::Done;
        return true;
    }
    return false;
}

bool ordonStep(kff::autotest::Context& c) {
    ++g_oTicks;
    const uint32_t t = c.frame;
    daAlink_c* link = kff::liveLink();
    switch (g_oStep) {
    case OrdonStep::Wait:
        if (t >= kOArmFrame && ordonHumanReady(c)) {
            if (g_oVariantM077) {
                dComIfGs_onEventBit(dSv_event_flag_c::M_077);
            }
            // Nur Test (frischer Spielstand hat kein Schwert). dComIfGs_setItem legt das Ordon-Schwert in Slot 0,
            // dComIfGs_setSelectEquipSword waehlt es als Schwert (Menue-Weg, setzt auch das Sammel-Bit),
            // dComIfGs_setSelectItemIndex legt den B-Slot auf Slot 0 und aktualisiert den Spielzwischenspeicher.
            dComIfGs_setItem(SLOT_0, dItemNo_SWORD_e);
            dComIfGs_setSelectEquipSword(dItemNo_SWORD_e);
            dComIfGs_setSelectItemIndex(SELECT_ITEM_B, SLOT_0);
            kff::logInfo("autowolf-test: Setup variant=%s m077=%d getSelectItem(B)=%u selectEquipSword=%u",
                g_oVariantM077 ? "m077" : "ordon", awM077() ? 1 : 0,
                static_cast<unsigned>(dComIfGp_getSelectItem(SELECT_ITEM_B)),
                static_cast<unsigned>(dComIfGs_getSelectEquipSword()));
            g_oStep = OrdonStep::Call;
            g_oStart = t;
            c.note("bereit: Mensch, Boden, kein Ereignis, kein Vogel. Ruf folgt");
            return false;
        }
        if (g_oTicks > kOWaitLimit) {
            c.note("TIMEOUT: Mensch wurde nicht bereit");
            return true;
        }
        if (g_oTicks % 300 == 0) {
            awLogLink("warte auf bereit:", link);
        }
        return false;
    case OrdonStep::Call: {
        const uint32_t k = t - g_oStart;
        if (k < kOCallHold) {
            c.holdR();
        }
        if (k == kOCallPress) {
            c.tapB();
            c.note("B-Tipp im R-Halt (Ruf als Mensch)");
        }
        if (k >= kOProbeAt && k < kOProbeAt + 10) {
            c.holdR();
        }
        if (k == kOProbePress) {
            c.tapB();
            c.note("Probe-Ruf waehrend der Verwandlung (muss blockiert sein)");
        }
        if (k >= kOCallEnd) {
            g_oStep = OrdonStep::WaitRide;
            g_oStart = t;
        }
        return false;
    }
    case OrdonStep::WaitRide:
        if (c.sess.phase == kff::Phase::Riding) {
            g_oStep = OrdonStep::Fly;
            g_oStart = t;
            c.note("Ritt gestartet");
            awLogLink("Ritt-Start:", link);
            return false;
        }
        if (t - g_oStart > kORideLimit) {
            c.note("TIMEOUT: kein Ritt gestartet");
            awLogLink("Timeout:", link);
            return true;
        }
        if (g_oTicks % 150 == 0) {
            kff::logInfo("autowolf-test: warte auf Ritt phase=%s", kff::phaseName(c.sess.phase));
            awLogLink("  Zustand:", link);
        }
        return false;
    case OrdonStep::Fly: {
        if (c.sess.phase != kff::Phase::Riding) {
            c.note("Ritt endet ohne B-Tipp (Kern)");
            g_oStep = g_oVariantM077 ? OrdonStep::WaitStay : OrdonStep::WaitHuman;
            g_oStart = t;
            return false;
        }
        const uint32_t rf = c.sess.rideFrames;
        float x = 0.0f;
        float y = 0.0f;
        if (rf < kOStickRise) {
            y = -0.6f;
        } else if (rf < kOStickLeft) {
            x = -1.0f;
        } else if (rf < kODismount) {
            x = 1.0f;
        }
        c.stick(x, y);
        if (rf >= kODismount) {
            c.tapB();
            c.note("B-Tipp (Absteigen)");
            g_oStep = g_oVariantM077 ? OrdonStep::WaitStay : OrdonStep::WaitHuman;
            g_oStart = t;
        }
        return false;
    }
    case OrdonStep::WaitHuman: {
        const bool human = c.sess.phase == kff::Phase::Idle && link != nullptr && !link->checkWolf();
        g_oHumanTicks = human ? g_oHumanTicks + 1 : 0;
        if (g_oHumanTicks >= kOHumanStable) {
            g_oStep = OrdonStep::SwordTest;
            g_oStart = t;
            g_oSwordTapped = false;
            c.note("Mensch stabil: Schwert-Test (B-Tipp in Menschenform)");
            awLogLink("Mensch:", link);
            return false;
        }
        if (t - g_oStart > kOHumanLimit) {
            c.note("TIMEOUT: keine Rueckverwandlung zum Menschen");
            awLogLink("Timeout:", link);
            return true;
        }
        if (g_oTicks % 150 == 0) {
            kff::logInfo("autowolf-test: warte auf Mensch phase=%s", kff::phaseName(c.sess.phase));
            awLogLink("  Zustand:", link);
        }
        return false;
    }
    case OrdonStep::WaitStay:
        return ordonStayWolf(c, link);
    case OrdonStep::SwordTest: {
        const uint32_t k = t - g_oStart;
        if (!g_oSwordTapped) {
            g_oSwordTapped = true;
            awLogLink("Schwert vor Tipp:", link);
            kff::logInfo("autowolf-test: getSelectItem(B)=%u selectEquipSword=%u",
                static_cast<unsigned>(dComIfGp_getSelectItem(SELECT_ITEM_B)),
                static_cast<unsigned>(dComIfGs_getSelectEquipSword()));
            c.tapB();
            c.note("B-Tipp als Mensch (Schwert ziehen oder Angriff)");
        } else if (k == kOSwordSecond) {
            c.tapB();
            c.note("B-Tipp 2 (Schwertangriff)");
        } else if (k % 10 == 0) {
            awLogLink("Schwert-Test:", link);
        }
        if (k >= kOSwordFrames) {
            awLogLink("Schwert-Test Ende:", link);
            c.note("timeline complete");
            g_oStep = OrdonStep::Done;
            return true;
        }
        return false;
    }
    case OrdonStep::Done:
        return true;
    }
    return true;
}

bool ordonTickMain(kff::autotest::Context& c) {
    g_oVariantM077 = false;
    return ordonStep(c);
}

bool ordonTickM077(kff::autotest::Context& c) {
    g_oVariantM077 = true;
    return ordonStep(c);
}

const kff::autotest::Scenario kOrdon = {
    "ordon",
    "Mensch ruft (Probe-Ruf in der Verwandlung), Wolf, Ritt, Absteigen, Rueckverwandlung, Schwert-Test",
    ordonTickMain,
};

const kff::autotest::Scenario kOrdonM077 = {
    "ordon-m077",
    "wie ordon, aber M_077 gesetzt: Link muss nach dem Ritt Wolf bleiben",
    ordonTickM077,
};

// ---- Test-Szenario "ordon-event": Ruf waehrend des Intro-Ereignisses (Link ist Mensch, eventRun=1) ----
// Erwartung: keine Verwandlung. Pruefung nach dem Intro: Link ist Mensch, Phase Idle, kein Vogel.
// Das Intro-Ereignis laeuft in ordon-2 etwa bis Link-Frame 550 (eventRun=1 bis 450, 0 ab 600).
constexpr uint32_t kEvChordAt = 10;    // R-Halt beginnt (Link-Frames nach dem ersten Link-Aufruf)
constexpr uint32_t kEvChordEnd = 40;   // R-Halt endet, B-Tipp in der Mitte
constexpr uint32_t kEvCheckAt = 700;   // Pruefung nach dem Intro

bool ordonEventTick(kff::autotest::Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = kff::liveLink();
    if (t >= kEvChordAt && t < kEvChordEnd) {
        c.holdR();
    }
    if (t == kEvChordAt + 15) {
        c.tapB();
        c.note("Ruf waehrend des Intro-Ereignisses (muss abgelehnt werden)");
    }
    if (t == kEvCheckAt) {
        awLogLink("Nach Intro:", link);
        const bool human = link != nullptr && !link->checkWolf();
        if (human && c.sess.phase == kff::Phase::Idle && c.sess.birdId == kff::kNoProc) {
            c.note("PASS: keine Verwandlung waehrend des Ereignisses");
            c.note("timeline complete");
        } else {
            c.note("FAIL: Verwandlung waehrend des Ereignisses oder Phase nicht Idle");
        }
        return true;
    }
    return false;
}

const kff::autotest::Scenario kOrdonEvent = {
    "ordon-event",
    "Ruf waehrend des Intro-Ereignisses: keine Verwandlung, Link bleibt Mensch",
    ordonEventTick,
};

#endif  // KFF_DEV_AUTOTEST (Test-Szenarien)

// WP6: Titel (dComIfG_changeOpeningScene) und Laden (dComIfGs_gameStart) beginnen einen neuen Spielstand. Eine
// vorgemerkte Rueckverwandlung und eine laufende Verwandlung dieses Moduls gelten dort nicht mehr (sonst verwandelte das
// Modul einen geladenen Wolf-Link zum Menschen). Pre ohne SKIP, nach dem Kern (der den Ritt vorher beendet).
// Nach dem Tod (Fortsetzen) laufen beide Ziele nicht: die Rueckverwandlung bleibt dort vorgemerkt (kein Wolf ohne M_077).
void awDropForNewGame(const char* where) {
    if (g_stage == AwStage::Idle && !g_revertPending && !g_wasHuman) {
        return;
    }
    kff::logInfo("autowolf: %s, Verwandlungszustand verworfen (Stufe %s, Rueckverwandlung %s)", where,
        awStageName(g_stage), g_revertPending ? "war vorgemerkt" : "keine");
    awBegin(AwStage::Idle);
    awReleaseSession();
    g_revertPending = false;
    g_wasHuman = false;
    g_retryWait = 0;
}

HookAction awOpeningScenePre(ModContext*, void*, void*, void*) {
    try {
        awDropForNewGame("Titel");
    } catch (...) {
        kff::logWarn("autowolf: exception in dComIfG_changeOpeningScene (ignored)");
    }
    return HOOK_CONTINUE;
}

HookAction awGameStartPre(ModContext*, void*, void*, void*) {
    try {
        awDropForNewGame("Laden");
    } catch (...) {
        kff::logWarn("autowolf: exception in dComIfGs_gameStart (ignored)");
    }
    return HOOK_CONTINUE;
}

class AutowolfModule final : public kff::Module {
public:
    const char* name() const override { return "autowolf"; }

    ModResult install(kff::Registry& reg) override {
        ModResult worst = reg.addPre(kff::Hook::ChangeOpeningScene, &awOpeningScenePre, kff::prio::kAutowolf);
        const ModResult load = reg.addPre(kff::Hook::GameStart, &awGameStartPre, kff::prio::kAutowolf);
        if (worst == MOD_OK) {
            worst = load;
        }
#if defined(KFF_DEV_AUTOTEST)
        // Weitere Szenarien: der Kern nimmt nur das eine aus autotestScenario() mit.
        const ModResult m077 = kff::autotest::registerScenario(&kOrdonM077);
        const ModResult event = kff::autotest::registerScenario(&kOrdonEvent);
        if (worst == MOD_OK) {
            worst = m077 != MOD_OK ? m077 : event;
        }
#endif
        return worst;
    }

    bool onHumanCall(kff::Session& s, daAlink_c* link) override {
        if (s.phase != kff::Phase::Idle) {
            kff::logInfo("autowolf: Ruf als Mensch abgelehnt, Phase %s", kff::phaseName(s.phase));
            return false;
        }
        if (const char* why = awTransformBlock(link)) {
            kff::logInfo("autowolf: Verwandlung abgelehnt (%s)", why);
            return false;
        }
        g_wasHuman = true;
        g_revertPending = false;
        g_linkId = fopAcM_GetID(link);
        s.phase = kff::Phase::Transforming;
        awBegin(AwStage::ToWolf);
        ++g_transforms;
        if (!awStartTransform(link)) {
            awBegin(AwStage::Idle);
            awReleaseSession();
            g_wasHuman = false;
            awLogLink("Verwandlung nicht gestartet:", link);
            return false;
        }
        awLogLink("Ruf als Mensch -> Verwandlung zum Wolf gestartet:", link);
        return true;
    }

    void onRideStart(kff::Session& s, daKago_c* bird) override {
        (void)s;
        (void)bird;
        kff::logInfo("autowolf: Ritt aktiv (Sitzung nach Mensch-Ruf=%d)", g_wasHuman ? 1 : 0);
    }

    void onFrameLink(kff::Session& s, daAlink_c* link) override {
        awFrameLink(s, link);
    }

    void onRideEnd(kff::Session& s, daKago_c* bird, kff::EndReason why) override {
        (void)s;
        (void)bird;
        if (!g_wasHuman) {
            return;
        }
        // WP6 (geprueft): auch nach dem Tod vorgemerkt. Link steht nach dem Fortsetzen als Wolf auf und kehrt am Boden in
        // die Menschenform zurueck; ohne Vormerkung bliebe er ohne M_077 Wolf. Titel und Laden verwerfen die Vormerkung.
        g_revertPending = !awM077();
        kff::logInfo("autowolf: Ritt beendet (%s), Rueckverwandlung %s", kff::endReasonName(why),
            g_revertPending ? "vorgemerkt" : "entfaellt, M_077 gesetzt");
    }

#if defined(KFF_DEV_AUTOTEST)
    const kff::autotest::Scenario* autotestScenario() const override { return &kOrdon; }
#endif

    void onShutdown(kff::Session& s) override {
        (void)s;
        kff::logInfo("autowolf: Entladen (Verwandlungen %u, Rufe %u, Rueckverwandlungen %u)", g_transforms,
            g_summons, g_reverts);
        awBegin(AwStage::Idle);
        g_revertPending = false;
        g_wasHuman = false;
    }
};

AutowolfModule g_module;

}  // namespace

namespace kff::modules {

Module& autowolf() {
    return g_module;
}

}  // namespace kff::modules
