// Kern: Ruf des Vogels (WP6, ORCH-6). summonBird ist aus ride.cpp hierher verschoben; die Wirkung ist unveraendert
// (fopAcM_create 350 ueber Link, Gier aus der Blickrichtung, Raum von Link). Neu sind nur Pruefungen VOR jeder Wirkung:
//
//  - Spawnpunkt frei ("room", Abnahme WP6.3): (a) die Linie von Link (+kProbeFoot) bis ueber den Spawnpunkt
//    (+kSpawnUp+kRoomAbove) trifft keine Flaeche, auch keine Rueckseite (keine Decke und kein Boden einer oberen Ebene
//    zwischen Link und Vogel), (b) die Decke ueber dem Spawnpunkt (RoofChk) liegt mindestens kRoomAbove hoeher
//    (Vogelhoehe 150 plus 100 Reserve, Annahme [V]), (c) der Boden unter dem Spawnpunkt liegt mindestens kRoomBelow
//    tiefer. Kein Boden darunter gilt als frei (Abgrund: der Vogel fliegt). Seitenwaende prueft der Spawn nicht: der Vogel
//    erscheint senkrecht ueber Link, seitlich prueft erst die Abtastung von freiheit im Ritt (bekannte Grenze).
//  - Eigener Vogel ohne Sitzung (Parameter 0x000100FF, nicht die Sitzung): Ruf abgelehnt, der Vogel wird entfernt.
//    Vanilla-Voegel (anderer Parameter) bleiben unberuehrt.
//  - summonBird selbst prueft vor fopAcM_create: Link lebt, Wolf (Mensch nur mit mensch_ritt), kein Vogel und keine
//    Ritt-Phase, kein Ereignis, keine Pause, kein Dusklight-Dokument, kein eigener Vogel ohne Sitzung, Spawnpunkt frei.
//    Module (autowolf, mensch) rufen summonBird ohne den Pad-Hook; die Gates gelten deshalb auch fuer sie.
//  - Ruf im Abflug (Befund 1, F2): der abfliegende Vogel wird zum Entfernen vorgemerkt (requestDelete). Der neue Vogel
//    entsteht erst, wenn der alte im _delete-Hook entfernt ist, frueheststens im Folgetick (deferredTick). Bis dahin
//    wird ein weiterer Ruf angenommen, erzeugt aber keinen zweiten Vogel. Ueberschreitet das Warten kDeferGiveUpTicks,
//    gibt es ein logWarn und B geht zurueck.
//
// Zeitgrenzen in Kern-Ticks (Pad-Lesungen, R4), Pausen-Ticks zaehlen nicht (Akteure stehen in der Pause):
//  - Ruf ohne Ritt-Start: nach kSummonTimeoutTicks ist der Vogel entfernt (Log "summon timeout: no ride start after").
//    Der Antrag geht einen Tick vorher hinaus, denn fopAcM_delete wirkt im Folgetick (gemessen: Antrag bei
//    departFrames 100, _delete bei 101).
//  - Ereignis im Ruf (Phase Summoning): der Vogel wird sofort entfernt ("summon aborted (event)").
//  - Phase Transforming ohne Vogel: Sicherheitsnetz nach kTransformLimitTicks (autowolf hat seinen eigenen Waechter).
#include "d/dolzel_rel.h"

#include "core/summon.hpp"

#include "core/input_core.hpp"
#include "core/kff/config.hpp"
#include "core/kff/hooks.hpp"
#include "core/kff/log.hpp"
#include "core/kff/services.hpp"
#include "core/kff/session.hpp"

#include "SSystem/SComponent/c_m3d.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_bg_s_roof_chk.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

namespace kff {
namespace {

constexpr uint32_t kFriendlyBirdParam = 0x000100FFu;  // Typ 1 in Bits 16-23, 0xFF: kein Stage-Pfad
constexpr float kSpawnUp = 350.0f;                    // Spawnhoehe ueber Link
constexpr float kBirdHeight = 150.0f;                 // Hoehe des Vogels ueber seinem Ursprung (Acch-Deckenhoehe) [V]
constexpr float kRoomAbove = kBirdHeight + 100.0f;    // Decke mindestens so hoch ueber dem Spawnpunkt
constexpr float kRoomBelow = 150.0f;                  // Boden mindestens so tief unter dem Spawnpunkt
constexpr float kProbeFoot = 50.0f;                   // Start der Linienpruefung ueber Links Fuessen
constexpr uint32_t kSummonTimeoutTicks = 300;         // Entscheidung: Vogel ohne Ritt-Start ist danach weg (Kern-Ticks)
constexpr uint32_t kTransformLimitTicks = 1800;       // Sicherheitsnetz Phase Transforming ohne Vogel (Kern-Ticks)
constexpr uint32_t kDeferGiveUpTicks = 300;           // Befund 1: Obergrenze des Wartens auf das Entfernen (Pad-Ticks)

summon::Stats g_stats;
uint32_t g_logRoom = 0;
uint32_t g_logStray = 0;
uint32_t g_logSpawnReject = 0;
uint32_t g_logRideWait = 0;
uint32_t g_logDeferred = 0;
fpc_ProcID g_summonId = kNoProc;  // Vogel, dessen Ruf gerade gezaehlt wird
uint32_t g_summonTicks = 0;       // Kern-Ticks ohne Pause seit dem Ruf (Phase Summoning)
uint32_t g_transformTicks = 0;    // Kern-Ticks ohne Pause in Phase Transforming ohne Vogel
#if defined(KFF_DEV_AUTOTEST)
fpc_ProcID g_noRideBird = kNoProc;  // KFF_TEST_NORIDE=1: dieser (erste) Vogel startet keinen Ritt
bool g_noRideUsed = false;
#endif

// Befund 1 (F2): angenommener Ruf im Abflug. Der neue Vogel entsteht erst, wenn der abfliegende Vogel entfernt ist:
// isPendingBird ist geloescht (_delete-Hook) und unter seiner Prozess-ID existiert kein Akteur mehr. Ticks sind Pad-Ticks.
struct Deferred {
    bool active = false;
    fpc_ProcID oldBird = kNoProc;  // abfliegender Vogel, dessen Entfernen abgewartet wird
    uint32_t ticks = 0;            // Pad-Ticks seit dem angenommenen Ruf
    uint32_t calls = 0;            // weitere angenommene Rufe waehrend des Wartens (ohne zweiten Vogel)
};
Deferred g_deferred;

// D16, UI-Eingabeblock: wie core::uiDocumentVisible (input.cpp, WP3). Ohne UiService gilt "nicht sichtbar".
bool uiDocumentVisible() {
    if (svc_ui == nullptr) {
        return false;
    }
    bool visible = false;
    if (svc_ui->is_any_document_visible(mod_ctx, &visible) != MOD_OK) {
        return false;
    }
    return visible;
}

bool linkInEvent(daAlink_c* link) {
    return dComIfGp_event_runCheck() != 0 || (link != nullptr && link->checkEventRun());
}

// B1 (F4): Link ist tot, auch wenn der Proc noch nicht PROC_DEAD ist. Leben 0 setzt den Tod, PROC_DEAD folgt erst danach im
// Link-Execute (gemessen: Ruf bei Leben 0 im Proc 239 WOLF_TIRED_WAIT, Befund D5). Ein Ruf in dieser Zeit erzeugt keinen Vogel.
bool linkDead(daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_DEAD || link->mProcID == daAlink_c::PROC_FOG_DEAD || dComIfGs_getLife() == 0;
}

// Link verwandelt sich gerade (Spielweg ueber Midna oder autowolf): kein Ruf, bis die Verwandlung fertig ist.
bool linkTransforming(daAlink_c* link) {
    return link->mProcID == daAlink_c::PROC_METAMORPHOSE || link->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY;
}

// Schadens-Procs (WP6.12): der Kern-Gate in ride.cpp kennt PROC_WOLF_DAMAGE und PROC_POLY_DAMAGE. Landeschaden und
// grosser Schaden (Wolf und Mensch) fehlen dort; ein Ruf darin startete den Ritt im Schaden (wie Fix A).
bool linkHurt(daAlink_c* link) {
    switch (link->mProcID) {
    case daAlink_c::PROC_WOLF_DAMAGE:
    case daAlink_c::PROC_WOLF_LARGE_DAMAGE_UP:
    case daAlink_c::PROC_WOLF_LAND_DAMAGE:
    case daAlink_c::PROC_POLY_DAMAGE:
    case daAlink_c::PROC_DAMAGE:
    case daAlink_c::PROC_LAND_DAMAGE:
    case daAlink_c::PROC_LARGE_DAMAGE:
    case daAlink_c::PROC_LARGE_DAMAGE_UP:
    case daAlink_c::PROC_LARGE_DAMAGE_WALL:
        return true;
    default:
        return false;
    }
}

// Eigene Voegel ohne Sitzung: Parameter genau 0x000100FF und nicht der Vogel der Sitzung.
struct StrayScan {
    fpc_ProcID keep = kNoProc;
    bool remove = false;  // true: gefundene Voegel entfernen (Pad-Hook), false: nur zaehlen (summonBird)
    uint32_t found = 0;
    fpc_ProcID first = kNoProc;
    cXyz firstPos;
};

int strayBird(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac == nullptr || fopAcM_GetName(ac) != fpcNm_KAGO_e) {
        return 1;
    }
    if (fopAcM_GetParam(ac) != kFriendlyBirdParam) {
        return 1;  // Vanilla-Vogel (anderer Parameter): nie anfassen
    }
    StrayScan* scan = static_cast<StrayScan*>(data);
    const fpc_ProcID id = fopAcM_GetID(ac);
    if (id == scan->keep || isPendingBird(id)) {  // N1: zum Entfernen vorgemerkt, kein Stray
        return 1;
    }
    if (scan->found == 0) {
        scan->first = id;
        scan->firstPos = ac->current.pos;
    }
    ++scan->found;
    if (scan->remove) {
        fopAcM_delete(id);
    }
    return 1;
}

// Anzahl eigener Voegel ohne Sitzung. remove: diese Voegel entfernen (Log je Fund, ratenbegrenzt).
uint32_t strayBirds(bool remove) {
    StrayScan scan;
    scan.keep = session().birdId;
    scan.remove = remove;
    fopAcIt_Executor(strayBird, &scan);
    if (scan.found > 0) {
        g_stats.strayFound += scan.found;
        logEvery(g_logStray, 10, 60, "call check: %u own bird(s) without session (first 0x%x at (%.0f,%.0f,%.0f))%s",
            scan.found, static_cast<unsigned>(scan.first), static_cast<double>(scan.firstPos.x),
            static_cast<double>(scan.firstPos.y), static_cast<double>(scan.firstPos.z),
            remove ? ", removed" : "");
    }
    return scan.found;
}

// Spawnpunkt pruefen (siehe Dateikopf). nullptr: frei. Sonst "room", Einzelheiten im Log (ratenbegrenzt).
const char* roomBlockReason(daAlink_c* link) {
    const cXyz l = link->current.pos;
    const cXyz spawn(l.x, l.y + kSpawnUp, l.z);
    const char* what = nullptr;
    float roof = G_CM3D_F_INF;
    float ground = -G_CM3D_F_INF;
    cXyz cross(0.0f, 0.0f, 0.0f);

    // (a) Linie von Link bis ueber den Spawnpunkt, Vorder- und Rueckseiten.
    const cXyz a(l.x, l.y + kProbeFoot, l.z);
    const cXyz b(l.x, spawn.y + kRoomAbove, l.z);
    dBgS_LinChk lin;
    lin.Set(&a, &b, nullptr);
    lin.OnBackFlag();
    if (dComIfG_Bgsp().LineCross(&lin)) {
        cross = lin.GetCross();
        what = "Flaeche zwischen Link und Spawnpunkt";
    }
    // (b) Decke ueber dem Spawnpunkt.
    if (what == nullptr) {
        dBgS_RoofChk rc;
        rc.SetPos(spawn);
        roof = dComIfG_Bgsp().RoofChk(&rc);
        if (roof != G_CM3D_F_INF && roof < spawn.y + kRoomAbove) {
            what = "Decke ueber dem Spawnpunkt zu tief";
        }
    }
    // (c) Boden unter dem Spawnpunkt.
    if (what == nullptr) {
        dBgS_GndChk gc;
        cXyz gp = spawn;
        gc.SetPos(&gp);
        ground = dComIfG_Bgsp().GroundCross(&gc);
        if (ground != -G_CM3D_F_INF && spawn.y - ground < kRoomBelow) {
            what = "Boden unter dem Spawnpunkt zu hoch";
        }
    }
    if (what == nullptr) {
        return nullptr;
    }
    ++g_stats.roomRejected;
    logEvery(g_logRoom, 20, 30,
        "spawn check: room blocked (%s) link=(%.1f,%.1f,%.1f) spawn y=%.1f roof=%.1f ground=%.1f cross=(%.1f,%.1f,%.1f) "
        "need above %.0f below %.0f",
        what, static_cast<double>(l.x), static_cast<double>(l.y), static_cast<double>(l.z), static_cast<double>(spawn.y),
        static_cast<double>(roof == G_CM3D_F_INF ? -1.0f : roof),
        static_cast<double>(ground == -G_CM3D_F_INF ? -1.0f : ground), static_cast<double>(cross.x),
        static_cast<double>(cross.y), static_cast<double>(cross.z), static_cast<double>(kRoomAbove),
        static_cast<double>(kRoomBelow));
    return "room";
}

// Gates von summonBird (vor jeder Wirkung). nullptr: der Vogel darf entstehen.
const char* spawnBlockReason(daAlink_c* link) {
    const Session& s = session();
    if (linkDead(link)) {
        return "Link tot";
    }
    if (linkTransforming(link)) {
        return "Verwandlung laeuft (Link)";
    }
    if (linkHurt(link)) {
        return "Link im Schaden";
    }
    if (link->checkWolf() == 0 && !config::getBool(config::Key::MenschRitt)) {
        return "Menschform ohne mensch_ritt";
    }
    // N1: ein abfliegender eigener Vogel blockiert hier nicht; summonBird entfernt ihn erst nach allen Gates.
    const bool departing = birdDeparting(s);
    if (s.birdId != kNoProc && !departing) {
        return "eigener Vogel aktiv";
    }
    if (s.phase != Phase::Idle && s.phase != Phase::Transforming && !departing) {
        return "Ritt-Phase laeuft";
    }
    if (linkInEvent(link)) {
        return "Ereignis laeuft";
    }
    if (dComIfGp_isPauseFlag() != 0) {
        return "Pause oder Menue offen";
    }
    if (uiDocumentVisible()) {
        return "Dusklight-Dokument sichtbar";
    }
    if (strayBirds(false) > 0) {
        return "eigener Vogel ohne Sitzung";
    }
    return roomBlockReason(link);
}

// Erzeugt den Vogel neben Link (fopAcM_create) und setzt die Sitzung auf Summoning. resetFirst: die Sitzung vorher leeren
// (sofortiger Ruf). Der verzoegerte Ruf (Befund 1) leert sie nicht, denn mensch setzt rideGrip direkt nach summonBird.
ModResult createBird(daAlink_c* link, bool resetFirst) {
    Session& s = session();
    const cXyz pos(link->current.pos.x, link->current.pos.y + kSpawnUp, link->current.pos.z);
    // Fix B (lebensverlust-zyklus3.md): Spawn-Gier aus der Blickrichtung (shape_angle), nicht aus current.angle. Nach
    // einem Schaden setzt current.angle den Schadenswinkel, die Blickrichtung bleibt erhalten.
    const csXyz angle(0, link->shape_angle.y, 0);
    const int room = static_cast<int>(fopAcM_GetRoomNo(link));
    const fpc_ProcID id = fopAcM_create(fpcNm_KAGO_e, kFriendlyBirdParam, &pos, room, &angle, nullptr, -1);
    if (id == fpcM_ERROR_PROCESS_ID_e) {
        ++g_stats.createFailed;
        logError("SPAWN failed, fopAcM_create returned an error");
        return MOD_ERROR;
    }
    if (resetFirst) {
        resetSession();
    }
    s.birdId = id;
    s.phase = Phase::Summoning;
    ++g_stats.summon;
    g_summonId = id;
    g_summonTicks = 0;
    logInfo("SPAWN fopAcM_create(fpcNm_KAGO_e, 0x%08x) -> id=0x%x pos=(%.1f,%.1f,%.1f) room=%d yaw=%d "
            "linkId=0x%x linkCall=%u",
        static_cast<unsigned>(kFriendlyBirdParam), static_cast<unsigned>(id), pos.x, pos.y, pos.z, room,
        static_cast<int>(angle.y), static_cast<unsigned>(fopAcM_GetID(link)), s.linkCalls);
#if defined(KFF_DEV_AUTOTEST)
    if (!g_noRideUsed && testFlag("KFF_TEST_NORIDE")) {
        g_noRideUsed = true;
        g_noRideBird = id;
        logInfo("KFF_TEST_NORIDE: bird 0x%x will not start a ride (summon timeout test)", static_cast<unsigned>(id));
    }
#endif
    return MOD_OK;
}

// Wartenden Ruf (Befund 1) beenden. giveUp: die Frist ist abgelaufen, dann geht B zurueck wie bei jeder Ablehnung.
void cancelDeferred(const char* why, bool giveUp) {
    if (giveUp) {
        logWarn("deferred call given up after %u core ticks: %s, B returned", g_deferred.ticks, why);
    } else {
        logInfo("deferred call dropped after %u core ticks: %s", g_deferred.ticks, why);
    }
    g_deferred = Deferred{};
    if (giveUp) {
        core::restoreChordB("deferred call given up");
    }
}

}  // namespace

namespace summon {

const char* callBlockReason(daAlink_c* link) {
    if (link == nullptr) {
        return nullptr;  // ride.cpp prueft Link vorher
    }
    if (linkDead(link)) {
        return "Link tot";  // B1 (F4): Leben 0 vor PROC_DEAD
    }
    if (linkTransforming(link)) {
        return "Verwandlung laeuft (Link)";
    }
    if (linkHurt(link)) {
        return "Link im Schaden";
    }
    if (strayBirds(true) > 0) {
        return "eigener Vogel ohne Sitzung";
    }
    return roomBlockReason(link);
}

bool rideStartAllowed(daAlink_c* link) {
    const char* why = nullptr;
    if (link == nullptr) {
        why = "kein Link";
    } else if (linkDead(link)) {
        why = "Link tot";
    } else if (linkInEvent(link)) {
        why = "Ereignis";
    }
#if defined(KFF_DEV_AUTOTEST)
    if (why == nullptr && g_noRideBird != kNoProc && session().birdId == g_noRideBird) {
        why = "Testschalter KFF_TEST_NORIDE (erster Vogel)";
    }
#endif
    if (why == nullptr) {
        return true;
    }
    ++g_stats.rideStartWait;
    logEvery(g_logRideWait, 3, 120, "ride start waiting (%s)", why);
    return false;
}

void tick() {
    Session& s = session();
    const bool paused = dComIfGp_isPauseFlag() != 0;

    // Sicherheitsnetz: Phase Transforming ohne Vogel haengt (Modul ausgefallen oder Waechter verpasst).
    if (s.phase == Phase::Transforming && s.birdId == kNoProc) {
        if (!paused) {
            ++g_transformTicks;
        }
        if (g_transformTicks >= kTransformLimitTicks) {
            ++g_stats.transformTimeout;
            logWarn("transform timeout: phase Transforming for %u core ticks without a bird, session freed",
                g_transformTicks);
            g_transformTicks = 0;
            resetSession();
        }
    } else {
        g_transformTicks = 0;
    }

    if (s.phase != Phase::Summoning || s.birdId == kNoProc || s.deleteRequested) {
        g_summonId = kNoProc;
        g_summonTicks = 0;
        return;
    }
    if (s.birdId != g_summonId) {
        g_summonId = s.birdId;
        g_summonTicks = 0;
    }
    daAlink_c* deadLink = liveLink();
    if (deadLink != nullptr && linkDead(deadLink)) {
        // B1 (F4): Link stirbt in der Ruf-Phase: der Vogel geht sofort, es entsteht kein Ritt-Start mehr.
        ++g_stats.summonAborted;
        logInfo("summon aborted (Link tot): bird removed before the ride start, %u core ticks after the call", g_summonTicks);
        endRideClean(EndReason::Death, "Link tot vor dem Ritt-Start");
        return;
    }
    if (linkInEvent(liveLink())) {
        // Welle 2: dieselbe Wirkung wie der Link-Frame-Pfad des Kerns (linkExecutePre, WP4 T3/D6): Sitzung sofort frei
        // (endRideClean), damit Vogel und Ereignis nie gleichzeitig aktiv sind. Wer zuerst sieht, beendet; der andere
        // findet danach keine Phase Summoning mehr.
        ++g_stats.summonAborted;
        logInfo("summon aborted (event): bird removed before the ride start, %u core ticks after the call", g_summonTicks);
        endRideClean(EndReason::Event, "Ereignis vor dem Ritt-Start");
        return;
    }
    if (!paused) {
        ++g_summonTicks;
    }
    // Antrag einen Tick vor der Grenze: fopAcM_delete wirkt im Folgetick, der Vogel ist nach kSummonTimeoutTicks weg.
    if (g_summonTicks + 1 >= kSummonTimeoutTicks) {
        ++g_stats.summonTimeout;
        logInfo("summon timeout: no ride start after %u core ticks, bird removed (gone at tick %u)", g_summonTicks,
            kSummonTimeoutTicks);
        requestDelete(EndReason::DeleteRequested, "summon timeout");
    }
}

// Je Pad-Tick (ridePadTick, nach retryPendingDelete): der wartende Ruf (Befund 1) erzeugt den neuen Vogel, sobald der
// abfliegende Vogel entfernt ist. Das geschieht frueheststens im Folgetick nach dem Ruf.
void deferredTick() {
    if (!g_deferred.active) {
        return;
    }
    ++g_deferred.ticks;
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        cancelDeferred("kein Link", false);
        return;
    }
    if (linkDead(link)) {
        cancelDeferred("Link tot", false);
        return;
    }
    fopAc_ac_c* old = nullptr;
    const bool removed = !isPendingBird(g_deferred.oldBird) && fopAcM_SearchByID(g_deferred.oldBird, &old) == 0;
    if (!removed) {
        if (g_deferred.ticks >= kDeferGiveUpTicks) {
            cancelDeferred("abfliegender Vogel nicht entfernt", true);
        }
        return;
    }
    if (const char* why = spawnBlockReason(link)) {
        if (g_deferred.ticks >= kDeferGiveUpTicks) {
            cancelDeferred(why, true);
        }
        return;
    }
    const uint32_t waited = g_deferred.ticks;
    const uint32_t calls = g_deferred.calls;
    g_deferred = Deferred{};
    if (createBird(link, false) != MOD_OK) {
        logWarn("deferred call: new bird not created, B returned");
        core::restoreChordB("spawn failed (deferred)");
        return;
    }
    logInfo("deferred call: new bird created %u core ticks after the call, old bird removed, %u more call(s) accepted",
        waited, calls);
}

// Der Ruf wartet nicht mehr auf einen Vogel, der zu einem anderen Link gehoerte (Szenenwechsel).
void abortDeferred(const char* why) {
    if (g_deferred.active) {
        cancelDeferred(why, false);
    }
}

const Stats& stats() {
    return g_stats;
}

}  // namespace summon

// ---------------------------------------------------------------------------------------------
// Aktion des Kerns (siehe core/kff/session.hpp)
// ---------------------------------------------------------------------------------------------

ModResult summonBird(daAlink_c* link) {
    if (link == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    // WP6 (R6): alle Gates vor jeder Wirkung. Ein abgelehnter Spawn aendert nichts (keine Sitzung, kein Vogel).
    if (const char* why = spawnBlockReason(link)) {
        ++g_stats.spawnRejected;
        logEvery(g_logSpawnReject, 20, 30, "call rejected (%s) at spawn", why);
        return MOD_ERROR;
    }
    if (g_deferred.active) {
        // Befund 1: der neue Vogel ist schon angefordert. Ein weiterer angenommener Ruf erzeugt keinen zweiten Vogel.
        ++g_deferred.calls;
        logEvery(g_logDeferred, 5, 60, "call accepted, the new bird waits until the departing bird is removed (%u)",
            g_deferred.calls);
        return MOD_OK;
    }
    Session& s = session();
    // N1 (Befund 1, F2): ein Ruf im Abflug nimmt den abfliegenden Vogel jetzt weg (requestDelete ueber die Prozess-ID,
    // derselbe Pfad wie am Abflug-Ende). Der neue Vogel entsteht erst nach dessen _delete-Hook (deferredTick): nie zwei
    // eigene Voegel zugleich. mensch setzt danach rideGrip.
    if (s.birdId != kNoProc) {
        const fpc_ProcID old = s.birdId;
        logInfo("call during departure: bird 0x%x removal requested, the new bird follows its _delete hook",
            static_cast<unsigned>(old));
        requestDelete(EndReason::DeleteRequested, "neuer Ruf im Abflug");
        resetSession();
        fopAc_ac_c* actor = nullptr;
        const bool alive = fopAcM_SearchByID(old, &actor) != 0;
        if (isPendingBird(old) || alive) {
            g_deferred.active = true;
            g_deferred.oldBird = old;
            g_deferred.ticks = 0;
            g_deferred.calls = 0;
            return MOD_OK;
        }
    }
    return createBird(link, true);
}

}  // namespace kff
