// Die einzige Stelle mit DEFINE_HOOK. Jedes Spielziel hat genau einen Alias. Module haengen sich ueber kff::Registry an,
// nie direkt ueber mods::hook.
//
// Jeder Callback wird ueber eine Huelle registriert (preThunk, postThunk, replaceThunk). Die Huelle prueft den Besitzer
// (ein Modul ohne Aktivitaet bekommt keinen Aufruf), vergibt das SKIP-Recht je Ziel und setzt das Ergebnis eines werfenden
// Replace auf 0 (Regeln in kff/hooks.hpp). Eintraege bleiben fuer die Lebensdauer der Bibliothek stehen, weil das SDK die
// Callbacks nur gemeinsam je Mod entfernt.
#include "d/dolzel_rel.h"

#include "core/handlers.hpp"
#include "core/kff/hooks.hpp"
#include "core/kff/log.hpp"
#include "core/kff/module.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_e_ym.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_tag_hstop.h"
#include "d/actor/d_a_tag_kago_fall.h"
#include "d/actor/d_a_tag_mwait.h"
#include "d/actor/d_a_tag_ret_room.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_menu_save.h"
#include "d/d_meter2_draw.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstring>
#if defined(KFF_DEV_AUTOTEST)
#include <cstdlib>
#include <stdexcept>

#include "core/kff/session.hpp"
#endif

// ---------------------------------------------------------------------------------------------
// Hook-Ziele (globaler Namensraum, Alias mit Praefix Kff). Jedes Ziel ist im Binary v2.0.3 als T-Symbol geprueft.
// ---------------------------------------------------------------------------------------------

// Symbolnamen der beiden Ziele ohne Signatur-Ueberladung. Windows (MSVC) nimmt die dekorierten MSVC-Namen, Schema wie in
// mods/randomizer/src/hooks.cpp. Alle anderen Plattformen behalten die bisherigen Itanium-Namen.
#ifdef _MSVC_LANG
#define KFF_SYM_SET_NEXT_STAGE "?dComIfGp_setNextStage@@YAXPEBDFCC@Z"
#define KFF_SYM_AUTO_SAVE "?updateAutoSave@@YAXXZ"
#else
#define KFF_SYM_SET_NEXT_STAGE "_Z21dComIfGp_setNextStagePKcsaa"
#define KFF_SYM_AUTO_SAVE "_Z14updateAutoSavev"
#endif

DEFINE_HOOK(&mDoCPd_c::read, KffPadRead);
DEFINE_HOOK(&daAlink_c::execute, KffLinkExecute);
DEFINE_HOOK(&daKago_c::execute, KffKagoExecute);
DEFINE_HOOK(&daKago_c::executeFly, KffKagoExecuteFly);
DEFINE_HOOK(&daKago_c::flySpeedCalc, KffKagoFlySpeedCalc);
DEFINE_HOOK(&daKago_c::checkRoofHeight, KffKagoCheckRoofHeight);
DEFINE_HOOK(&daKago_c::checkGroundHeight, KffKagoCheckGroundHeight);
DEFINE_HOOK(&daKago_c::checkWallHitFall, KffKagoCheckWallHitFall);
DEFINE_HOOK(&daKago_c::executeBalloonMenu, KffKagoExecuteBalloonMenu);
DEFINE_HOOK(&daKago_c::draw, KffKagoDraw);
DEFINE_HOOK(&daKago_c::_delete, KffKagoDelete);
DEFINE_HOOK(&daKago_c::setSceneChange, KffKagoSetSceneChange);
DEFINE_HOOK(&daKago_c::checkNextPath, KffKagoCheckNextPath);
DEFINE_HOOK(&daKago_c::checkFlySceneChange, KffKagoCheckFlySceneChange);
DEFINE_HOOK(&dStage_changeScene, KffStageChangeScene);
DEFINE_HOOK(&dMeter2Draw_c::draw, KffMeterDraw);
// dComIfGp_setNextStage ist ueberladen: Ziel ueber den dekorierten Namen (wie dlsym, macOS: ein Unterstrich vor _Z).
DEFINE_HOOK_SYMBOL(KFF_SYM_SET_NEXT_STAGE, void(char const*, s16, s8, s8), KffStageNextStage);
// E_YM-Lock (v1-Paritaet): nur der Kern, kein Modul-Hook.
DEFINE_HOOK(&daE_YM_c::_delete, KffYmDelete);
// Ritt-Ende bei Tod, Titel, Laden und Speichern (speichern-titel.md, tod-hp.md). Ein Besitzer pro Ziel.
DEFINE_HOOK(&daAlink_c::checkDeadAction, KffLinkCheckDeadAction);
DEFINE_HOOK(&daAlink_c::procCoDeadInit, KffLinkProcCoDeadInit);
DEFINE_HOOK(&dComIfG_changeOpeningScene, KffChangeOpeningScene);
DEFINE_HOOK(&dComIfGs_gameStart, KffGameStart);
DEFINE_HOOK(&dMenu_save_c::saveMoveDisp, KffSaveMoveDisp);
DEFINE_HOOK(&dMenu_save_c::saveMoveDisp2, KffSaveMoveDisp2);
DEFINE_HOOK(&mDoMemCd_Ctrl_c::save, KffCardSave);
// E11: updateAutoSave (Autosave-Automat, dusk/autosave.cpp). Pre und Post setzen ein Flag fuer genau diesen Aufruf.
// Ziel ueber den Namen (wie dComIfGp_setNextStage oben): dusk/autosave.h liegt nicht im SDK, und ein Adressbezug
// wuerde das Symbol ins Modul binden (undefined symbol beim Link). Signatur: global, ohne Argumente.
DEFINE_HOOK_SYMBOL(KFF_SYM_AUTO_SAVE, void(), KffAutoSaveUpdate);
// Ersatzpfad-Schutz (griff-Befund K6) und Warp-Start (D1).
DEFINE_HOOK(&daKago_c::executeStagger, KffKagoExecuteStagger);
DEFINE_HOOK(&daAlink_c::procCoWarpInit, KffLinkProcCoWarpInit);
// P-RESTART (freiheit-nacharbeit.md): Void- und Kopf-Neustart des Links im Ritt abweisbar machen (Modul freiheit).
DEFINE_HOOK(&daAlink_c::checkRestartRoom, KffAlinkCheckRestartRoom);
DEFINE_HOOK(&daAlink_c::checkRoofRestart, KffAlinkCheckRoofRestart);
DEFINE_HOOK(&dCamera_c::Stop, KffCameraStop);
DEFINE_HOOK(&mDoRst_resetCallBack, KffMainReset);
// Griff des Mensch-Ritts (Modul mensch).
DEFINE_HOOK(&daAlink_c::posMove, KffLinkPosMove);
DEFINE_HOOK(&daAlink_c::setMatrix, KffLinkSetMatrix);
DEFINE_HOOK(&daAlink_c::setBodyPartPos, KffLinkSetBodyPart);
DEFINE_HOOK(&daAlink_c::setArmMatrix, KffLinkSetArmMatrix);
DEFINE_HOOK(&daAlink_c::freeGrabItem, KffLinkFreeGrabItem);
// WP7 Hook-Ziele fuer die Wellen 2 und 3.
DEFINE_HOOK(&daKago_c::executeLanding, KffKagoExecuteLanding);
DEFINE_HOOK(&daKago_c::executeLandingLakeHairia, KffKagoExecuteLandingLakeHairia);
DEFINE_HOOK(&daKago_c::executeLandingBoartHouse, KffKagoExecuteLandingBoartHouse);
DEFINE_HOOK(&daKago_c::demo_skip, KffKagoDemoSkip);
DEFINE_HOOK(&daKago_c::executePerch, KffKagoExecutePerch);
DEFINE_HOOK(&daKago_c::initPerchDemo, KffKagoInitPerchDemo);
DEFINE_HOOK(&dComIfGs_onEventBit, KffEventBitOn);
DEFINE_HOOK(&dComIfGs_offEventBit, KffEventBitOff);
DEFINE_HOOK(&fopAcM_orderSpeakEvent, KffOrderSpeakEvent);
DEFINE_HOOK(&fopAcM_orderPotentialEvent, KffOrderPotentialEvent);
DEFINE_HOOK(&fopAcM_orderChangeEventId, KffOrderChangeEventId);
DEFINE_HOOK(&fopAcM_orderOtherEventId, KffOrderOtherEventId);
DEFINE_HOOK(&fopAcM_orderDoorEvent, KffOrderDoorEvent);
DEFINE_HOOK(&fopAcM_orderItemEvent, KffOrderItemEvent);
DEFINE_HOOK(&fopAcM_orderTalkEvent, KffOrderTalkEvent);
DEFINE_HOOK(&dStage_changeScene4Event, KffStageChangeScene4Event);
DEFINE_HOOK(&dEvt_control_c::sceneChange, KffEvtControlSceneChange);
DEFINE_HOOK(&daAlink_c::checkSceneChange, KffAlinkCheckSceneChange);
DEFINE_HOOK(&daAlink_c::startRestartRoom, KffAlinkStartRestartRoom);
DEFINE_HOOK(&dStage_restartRoom, KffStageRestartRoom);
DEFINE_HOOK(&daTagRetRm_c::execute, KffTagRetRmExecute);
DEFINE_HOOK(&daTagKagoFall_c::execute, KffTagKagoFallExecute);
DEFINE_HOOK(&daTagMwait_c::execute, KffTagMwaitExecute);
DEFINE_HOOK(&daTagHstop_c::execute, KffTagHstopExecute);
DEFINE_HOOK(&daAlink_c::checkSwimAction, KffLinkCheckSwimAction);
DEFINE_HOOK(&daAlink_c::checkDamageAction, KffLinkCheckDamageAction);
DEFINE_HOOK(&daAlink_c::procWolfDamageInit, KffLinkProcWolfDamageInit);
DEFINE_HOOK(&daAlink_c::procWolfCargoCarry, KffLinkProcWolfCargoCarry);
DEFINE_HOOK(&daAlink_c::procCoLavaReturnInit, KffLinkProcCoLavaReturnInit);
DEFINE_HOOK(&daAlink_c::procCoMetamorphoseInit, KffLinkProcCoMetamorphoseInit);
DEFINE_HOOK(&daAlink_c::checkHorseRide, KffLinkCheckHorseRide);
DEFINE_HOOK(&daAlink_c::checkSpinnerRide, KffLinkCheckSpinnerRide);
#if defined(KFF_DEV_AUTOTEST)
// Schadensdiagnose (lebensverlust-zyklus3.md, Fix C). Nur im Testbuild.
DEFINE_HOOK(&daAlink_c::setDamagePoint, KffLinkSetDamagePoint);
DEFINE_HOOK(&daAlink_c::checkPolyDamage, KffLinkCheckPolyDamage);
DEFINE_HOOK(&daAlink_c::checkWolfLandAction, KffLinkCheckWolfLandAction);
// Testbuild WP7.8: Symbol, das es im Binary nicht gibt. Die Installation muss MOD_UNAVAILABLE liefern.
DEFINE_HOOK_SYMBOL("_Z28kff_wp7_testmissing_symbol_v", void(), KffTestBadName);
#endif

namespace kff {

const char* modResultName(ModResult r) {
    switch (r) {
    case MOD_OK:
        return "MOD_OK";
    case MOD_ERROR:
        return "MOD_ERROR";
    case MOD_UNAVAILABLE:
        return "MOD_UNAVAILABLE";
    case MOD_UNSUPPORTED:
        return "MOD_UNSUPPORTED";
    case MOD_CONFLICT:
        return "MOD_CONFLICT";
    case MOD_INVALID_ARGUMENT:
        return "MOD_INVALID_ARGUMENT";
    default:
        break;
    }
    return "MOD_?";
}

#if defined(KFF_DEV_AUTOTEST)
bool testFlag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] == '1';
}
#endif

namespace {

constexpr size_t kHookCount = static_cast<size_t>(Hook::kCount);
constexpr size_t kMaxCallRecs = 256;  // je Registrierung ein Eintrag; Eintraege werden nie wiederverwendet

enum class Kind : uint8_t { Pre, Post, Replace };

// Ein registrierter Callback. Das SDK speichert nur den Zeiger auf diesen Eintrag (userdata), daher bleibt er stehen.
struct CallRec {
    const Module* owner = nullptr;  // nullptr: Kern (immer aktiv)
    const char* ownerName = "core";
    Hook hook = Hook::PadRead;
    Kind kind = Kind::Pre;
    int32_t priority = 0;
    PreFn pre = nullptr;
    PostFn post = nullptr;
    ReplaceFn repl = nullptr;
    void* user = nullptr;
};

CallRec g_recs[kMaxCallRecs];
size_t g_recCount = 0;

bool g_installed[kHookCount] = {};
bool g_replaced[kHookCount] = {};
const char* g_replaceOwner[kHookCount] = {};
const char* g_skipOwner[kHookCount] = {};  // der einzige Besitzer mit SKIP-Recht je Ziel (erster SKIP-Liefernde)
const char* g_owner = "core";
bool g_ownerIsCore = true;
const Module* g_ownerModule = nullptr;     // gesetzt, wenn g_owner ein registriertes Modul ist
uint32_t g_conflicts = 0;                  // Registrierungs- und Laufzeitkonflikte (fuer den Startbericht)
uint32_t g_ownerFailures = 0;              // Fehlschlaege von Registrierungen eines Moduls (vom Kern nach install() geprueft)
uint32_t g_replaceExceptions = 0;          // werfende Replace-Aufrufe (begrenzt geloggt)

size_t idx(Hook h) {
    return static_cast<size_t>(h);
}

// Ein Besitzer darf nur registrieren, wenn er der Kern oder ein bekanntes Modul ist.
bool ownerKnown() {
    return g_ownerIsCore || g_ownerModule != nullptr;
}

// Zaehlt Fehlschlaege von Modul-Registrierungen und gibt das Ergebnis unveraendert zurueck.
ModResult countFailure(ModResult r) {
    if (r != MOD_OK && !g_ownerIsCore) {
        ++g_ownerFailures;
    }
    return r;
}

// Aktiv = Kern, oder Modul, das nicht abgeschaltet ist.
bool recActive(const CallRec& rec) {
    return rec.owner == nullptr || moduleActive(rec.owner);
}

CallRec* newRec(Hook h, Kind kind, int32_t priority, void* user) {
    if (g_recCount >= kMaxCallRecs) {
        logError("MOD_ERROR: hook callback table full (%u entries), %s refused for %s",
            static_cast<unsigned>(kMaxCallRecs), Registry::hookName(h), g_owner);
        return nullptr;
    }
    CallRec* rec = &g_recs[g_recCount++];
    rec->owner = g_ownerModule;
    rec->ownerName = g_owner;
    rec->hook = h;
    rec->kind = kind;
    rec->priority = priority;
    rec->user = user;
    return rec;
}

// Nimmt den zuletzt angelegten Eintrag zurueck, wenn die Installation scheitert. Er ist dann nie benutzt worden.
void dropRec(const CallRec* rec) {
    if (g_recCount > 0 && rec == &g_recs[g_recCount - 1]) {
        --g_recCount;
    }
}

// Eindeutige Prioritaet: gleiche Art (Pre oder Post), gleiche Prioritaet, anderer Besitzer = Konflikt.
ModResult checkPriority(Hook h, Kind kind, int32_t priority) {
    for (size_t i = 0; i < g_recCount; ++i) {
        const CallRec& other = g_recs[i];
        if (other.hook != h || other.kind != kind || other.priority != priority ||
            std::strcmp(other.ownerName, g_owner) == 0) {
            continue;
        }
        ++g_conflicts;
        logError("MOD_CONFLICT: priority %d on %s is taken by %s, %s refused", static_cast<int>(priority),
            Registry::hookName(h), other.ownerName, g_owner);
        return MOD_CONFLICT;
    }
    return MOD_OK;
}

// Hoechstens ein Besitzer je Ziel darf HOOK_SKIP_ORIGINAL liefern. Der erste SKIP-Liefernde behaelt das Recht. Ein anderer
// Besitzer, der ebenfalls SKIP liefert, wird abgewiesen (das Original laeuft), als MOD_CONFLICT gemeldet und, wenn er ein
// Modul ist, abgeschaltet. Der Kern kann nicht abgeschaltet werden: bei ihm bleibt es bei der Meldung.
bool claimSkip(const CallRec& rec) {
    const size_t i = idx(rec.hook);
    if (g_skipOwner[i] == nullptr || std::strcmp(g_skipOwner[i], rec.ownerName) == 0) {
        g_skipOwner[i] = rec.ownerName;
        return true;
    }
    ++g_conflicts;
    logError("MOD_CONFLICT: SKIP on %s by %s refused (only %s may skip this target)", Registry::hookName(rec.hook),
        rec.ownerName, g_skipOwner[i]);
    if (rec.owner != nullptr) {
        disableModule(rec.owner);
        logError("module %s disabled after MOD_CONFLICT on %s", rec.ownerName, Registry::hookName(rec.hook));
    }
    return false;
}

#if defined(KFF_DEV_AUTOTEST)
// Testbuild WP7.5: KFF_TEST_THROW=<Modulname> wirft genau einmal aus einem Pre-Callback dieses Besitzers, sobald ein Ritt
// laeuft. Die Ausnahme geht wie im Produktivcode ungefangen an das SDK (fail_mod, CONTINUE).
void testThrowPoint(const CallRec& rec) {
    static const char* target = std::getenv("KFF_TEST_THROW");
    static bool fired = false;
    if (fired || target == nullptr || std::strcmp(target, rec.ownerName) != 0 || !isRiding()) {
        return;
    }
    fired = true;
    logWarn("KFF_TEST_THROW: Ausnahme im Pre-Callback von %s (Testbuild)", rec.ownerName);
    throw std::runtime_error("KFF_TEST_THROW");
}
#endif

// Pre-Huelle. Eine Ausnahme wird NICHT gefangen: das SDK wertet sie als Modulfehler (fail_mod) und behandelt die
// Pre-Kette als CONTINUE, das Original laeuft.
HookAction preThunk(ModContext* ctx, void* args, void* retval, void* userdata) {
    CallRec* rec = static_cast<CallRec*>(userdata);
    if (rec == nullptr || rec->pre == nullptr || !recActive(*rec)) {
        return HOOK_CONTINUE;
    }
#if defined(KFF_DEV_AUTOTEST)
    testThrowPoint(*rec);
#endif
    const HookAction action = rec->pre(ctx, args, retval, rec->user);
    if (action == HOOK_SKIP_ORIGINAL && !claimSkip(*rec)) {
        return HOOK_CONTINUE;
    }
    return action;
}

// Post-Huelle. Eine Ausnahme wird wie bei Pre vom SDK als Modulfehler gewertet.
void postThunk(ModContext* ctx, void* args, void* retval, void* userdata) {
    CallRec* rec = static_cast<CallRec*>(userdata);
    if (rec == nullptr || rec->post == nullptr || !recActive(*rec)) {
        return;
    }
    rec->post(ctx, args, retval, rec->user);
}

// Setzt das Ergebnis eines werfenden Replace auf 0. Der Rueckgabetyp ist je Ziel fest (siehe Registry::replace).
void zeroReplaceResult(const CallRec& rec, void* retval) {
    if (retval == nullptr) {
        return;
    }
    switch (rec.hook) {
    case Hook::KagoCheckWallHitFall:
        *static_cast<bool*>(retval) = false;
        break;
    case Hook::KagoExecuteBalloonMenu:
        *static_cast<int*>(retval) = 0;
        break;
    default:
        break;
    }
}

// Replace-Huelle (nur Kern). Wirft der Replace, wird das Ergebnis auf 0 gesetzt und die Ausnahme gemeldet. Die Huelle endet
// dann normal, daher nimmt das SDK den Replace als gelaufen an: das Original laeuft in diesem Aufruf nicht.
void replaceThunk(ModContext* ctx, void* args, void* retval, void* userdata) {
    CallRec* rec = static_cast<CallRec*>(userdata);
    if (rec == nullptr || rec->repl == nullptr) {
        return;
    }
    try {
        rec->repl(ctx, args, retval, rec->user);
    } catch (...) {
        ++g_replaceExceptions;
        zeroReplaceResult(*rec, retval);
        if (g_replaceExceptions <= 3) {
            logError("replace %s threw (result 0, original not run, count %u)", Registry::hookName(rec->hook),
                static_cast<unsigned>(g_replaceExceptions));
        }
    }
}

// Installiert das Ziel genau einmal (Detour), danach nur noch Callbacks anhaengen.
template <class E>
ModResult ensureInstalled(Hook h) {
    const size_t i = idx(h);
    if (g_installed[i]) {
        return MOD_OK;
    }
    const ModResult r = mods::hook::install<E>();
    if (r == MOD_OK) {
        g_installed[i] = true;
        logInfo("install %s: ok", Registry::hookName(h));
    } else {
        logWarn("install %s -> %s (target unresolved or host refused)", Registry::hookName(h), modResultName(r));
    }
    return r;
}

template <class E>
ModResult preOn(Hook h, PreFn fn, const HookOptions& o) {
    const ModResult r = ensureInstalled<E>(h);
    if (r != MOD_OK) {
        return r;
    }
    return svc_hook->add_pre(mod_ctx, E::target, fn, &o);
}

template <class E>
ModResult postOn(Hook h, PostFn fn, const HookOptions& o) {
    const ModResult r = ensureInstalled<E>(h);
    if (r != MOD_OK) {
        return r;
    }
    return svc_hook->add_post(mod_ctx, E::target, fn, &o);
}

template <class E>
ModResult replaceOn(Hook h, ReplaceFn fn, const HookOptions& o) {
    const ModResult r = ensureInstalled<E>(h);
    if (r != MOD_OK) {
        return r;
    }
    return svc_hook->replace(mod_ctx, E::target, fn, &o);
}

void report(const char* what, ModResult r) {
    if (r == MOD_OK) {
        logInfo("hook %s: ok", what);
    } else {
        logError("hook %s: failed (%s)", what, modResultName(r));
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------------------------

void Registry::setOwner(const char* owner) {
    g_owner = owner != nullptr ? owner : "?";
    g_ownerIsCore = std::strcmp(g_owner, "core") == 0;
    g_ownerModule = nullptr;
    if (g_ownerIsCore) {
        return;
    }
    size_t count = 0;
    Module* const* list = moduleList(count);
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(list[i]->name(), g_owner) == 0) {
            g_ownerModule = list[i];
            return;
        }
    }
    logError("MOD_UNAVAILABLE: %s is not a registered module, its registrations are refused", g_owner);
}

const char* Registry::owner() const {
    return g_owner;
}

const char* Registry::hookName(Hook h) {
    switch (h) {
    case Hook::PadRead:
        return "mDoCPd_c::read";
    case Hook::LinkExecute:
        return "daAlink_c::execute";
    case Hook::KagoExecute:
        return "daKago_c::execute";
    case Hook::KagoExecuteFly:
        return "daKago_c::executeFly";
    case Hook::KagoFlySpeedCalc:
        return "daKago_c::flySpeedCalc";
    case Hook::KagoCheckRoofHeight:
        return "daKago_c::checkRoofHeight";
    case Hook::KagoCheckGroundHeight:
        return "daKago_c::checkGroundHeight";
    case Hook::KagoCheckWallHitFall:
        return "daKago_c::checkWallHitFall";
    case Hook::KagoExecuteBalloonMenu:
        return "daKago_c::executeBalloonMenu";
    case Hook::KagoDraw:
        return "daKago_c::draw";
    case Hook::KagoDelete:
        return "daKago_c::_delete";
    case Hook::KagoSetSceneChange:
        return "daKago_c::setSceneChange";
    case Hook::KagoCheckNextPath:
        return "daKago_c::checkNextPath";
    case Hook::KagoCheckFlySceneChange:
        return "daKago_c::checkFlySceneChange";
    case Hook::StageChangeScene:
        return "dStage_changeScene";
    case Hook::StageNextStage:
        return "dComIfGp_setNextStage";
    case Hook::MeterDraw:
        return "dMeter2Draw_c::draw";
    case Hook::LinkCheckDeadAction:
        return "daAlink_c::checkDeadAction";
    case Hook::LinkProcCoDeadInit:
        return "daAlink_c::procCoDeadInit";
    case Hook::ChangeOpeningScene:
        return "dComIfG_changeOpeningScene";
    case Hook::GameStart:
        return "dComIfGs_gameStart";
    case Hook::SaveMoveDisp:
        return "dMenu_save_c::saveMoveDisp";
    case Hook::SaveMoveDisp2:
        return "dMenu_save_c::saveMoveDisp2";
    case Hook::CardSave:
        return "mDoMemCd_Ctrl_c::save";
    case Hook::AutoSaveUpdate:
        return "updateAutoSave";
    case Hook::KagoExecuteStagger:
        return "daKago_c::executeStagger";
    case Hook::LinkProcCoWarpInit:
        return "daAlink_c::procCoWarpInit";
    case Hook::AlinkCheckRestartRoom:
        return "daAlink_c::checkRestartRoom";
    case Hook::AlinkCheckRoofRestart:
        return "daAlink_c::checkRoofRestart";
    case Hook::CameraStop:
        return "dCamera_c::Stop";
    case Hook::MainReset:
        return "mDoRst_resetCallBack";
    case Hook::LinkPosMove:
        return "daAlink_c::posMove";
    case Hook::LinkSetMatrix:
        return "daAlink_c::setMatrix";
    case Hook::LinkSetBodyPart:
        return "daAlink_c::setBodyPartPos";
    case Hook::LinkSetArmMatrix:
        return "daAlink_c::setArmMatrix";
    case Hook::LinkFreeGrabItem:
        return "daAlink_c::freeGrabItem";
    case Hook::YmDelete:
        return "daE_YM_c::_delete";
    case Hook::KagoExecuteLanding:
        return "daKago_c::executeLanding";
    case Hook::KagoExecuteLandingLakeHairia:
        return "daKago_c::executeLandingLakeHairia";
    case Hook::KagoExecuteLandingBoartHouse:
        return "daKago_c::executeLandingBoartHouse";
    case Hook::KagoDemoSkip:
        return "daKago_c::demo_skip";
    case Hook::KagoExecutePerch:
        return "daKago_c::executePerch";
    case Hook::KagoInitPerchDemo:
        return "daKago_c::initPerchDemo";
    case Hook::EventBitOn:
        return "dComIfGs_onEventBit";
    case Hook::EventBitOff:
        return "dComIfGs_offEventBit";
    case Hook::OrderSpeakEvent:
        return "fopAcM_orderSpeakEvent";
    case Hook::OrderPotentialEvent:
        return "fopAcM_orderPotentialEvent";
    case Hook::OrderChangeEventId:
        return "fopAcM_orderChangeEventId";
    case Hook::OrderOtherEventId:
        return "fopAcM_orderOtherEventId";
    case Hook::OrderDoorEvent:
        return "fopAcM_orderDoorEvent";
    case Hook::OrderItemEvent:
        return "fopAcM_orderItemEvent";
    case Hook::OrderTalkEvent:
        return "fopAcM_orderTalkEvent";
    case Hook::StageChangeScene4Event:
        return "dStage_changeScene4Event";
    case Hook::EvtControlSceneChange:
        return "dEvt_control_c::sceneChange";
    case Hook::AlinkCheckSceneChange:
        return "daAlink_c::checkSceneChange";
    case Hook::AlinkStartRestartRoom:
        return "daAlink_c::startRestartRoom";
    case Hook::StageRestartRoom:
        return "dStage_restartRoom";
    case Hook::TagRetRmExecute:
        return "daTagRetRm_c::execute";
    case Hook::TagKagoFallExecute:
        return "daTagKagoFall_c::execute";
    case Hook::TagMwaitExecute:
        return "daTagMwait_c::execute";
    case Hook::TagHstopExecute:
        return "daTagHstop_c::execute";
    case Hook::LinkCheckSwimAction:
        return "daAlink_c::checkSwimAction";
    case Hook::LinkCheckDamageAction:
        return "daAlink_c::checkDamageAction";
    case Hook::LinkProcWolfDamageInit:
        return "daAlink_c::procWolfDamageInit";
    case Hook::LinkProcWolfCargoCarry:
        return "daAlink_c::procWolfCargoCarry";
    case Hook::LinkProcCoLavaReturnInit:
        return "daAlink_c::procCoLavaReturnInit";
    case Hook::LinkProcCoMetamorphoseInit:
        return "daAlink_c::procCoMetamorphoseInit";
    case Hook::LinkCheckHorseRide:
        return "daAlink_c::checkHorseRide";
    case Hook::LinkCheckSpinnerRide:
        return "daAlink_c::checkSpinnerRide";
#if defined(KFF_DEV_AUTOTEST)
    case Hook::LinkSetDamagePoint:
        return "daAlink_c::setDamagePoint";
    case Hook::LinkCheckPolyDamage:
        return "daAlink_c::checkPolyDamage";
    case Hook::LinkCheckWolfLandAction:
        return "daAlink_c::checkWolfLandAction";
    case Hook::TestBadName:
        return "kff_wp7_testmissing_symbol (Testbuild)";
#endif
    case Hook::kCount:
        break;
    }
    return "?";
}

#define KFF_PRE_CASE(H, E)                                                                         \
    case Hook::H:                                                                                  \
        r = preOn<E>(h, fn, o);                                                                    \
        break
#define KFF_POST_CASE(H, E)                                                                        \
    case Hook::H:                                                                                  \
        r = postOn<E>(h, fn, o);                                                                   \
        break

ModResult Registry::addPre(Hook h, PreFn userFn, int32_t priority, void* userdata) {
    if (userFn == nullptr) {
        return countFailure(MOD_INVALID_ARGUMENT);
    }
    if (!ownerKnown()) {
        return countFailure(MOD_UNAVAILABLE);
    }
    if (const ModResult tie = checkPriority(h, Kind::Pre, priority); tie != MOD_OK) {
        return countFailure(tie);
    }
    CallRec* rec = newRec(h, Kind::Pre, priority, userdata);
    if (rec == nullptr) {
        return countFailure(MOD_ERROR);
    }
    rec->pre = userFn;
    HookOptions o = HOOK_OPTIONS_INIT;
    o.priority = priority;
    o.userdata = rec;
    const PreFn fn = preThunk;
    ModResult r = MOD_UNSUPPORTED;
    switch (h) {
        KFF_PRE_CASE(LinkExecute, KffLinkExecute);
        KFF_PRE_CASE(KagoExecute, KffKagoExecute);
        KFF_PRE_CASE(KagoExecuteFly, KffKagoExecuteFly);
        KFF_PRE_CASE(KagoFlySpeedCalc, KffKagoFlySpeedCalc);
        KFF_PRE_CASE(KagoCheckRoofHeight, KffKagoCheckRoofHeight);
        KFF_PRE_CASE(KagoCheckGroundHeight, KffKagoCheckGroundHeight);
        KFF_PRE_CASE(KagoCheckWallHitFall, KffKagoCheckWallHitFall);
        KFF_PRE_CASE(KagoExecuteBalloonMenu, KffKagoExecuteBalloonMenu);
        KFF_PRE_CASE(KagoDraw, KffKagoDraw);
        KFF_PRE_CASE(KagoDelete, KffKagoDelete);
        KFF_PRE_CASE(KagoSetSceneChange, KffKagoSetSceneChange);
        KFF_PRE_CASE(KagoCheckNextPath, KffKagoCheckNextPath);
        KFF_PRE_CASE(KagoCheckFlySceneChange, KffKagoCheckFlySceneChange);
        KFF_PRE_CASE(StageChangeScene, KffStageChangeScene);
        KFF_PRE_CASE(StageNextStage, KffStageNextStage);
        KFF_PRE_CASE(LinkCheckDeadAction, KffLinkCheckDeadAction);
        KFF_PRE_CASE(LinkProcCoDeadInit, KffLinkProcCoDeadInit);
        KFF_PRE_CASE(ChangeOpeningScene, KffChangeOpeningScene);
        KFF_PRE_CASE(GameStart, KffGameStart);
        KFF_PRE_CASE(SaveMoveDisp, KffSaveMoveDisp);
        KFF_PRE_CASE(SaveMoveDisp2, KffSaveMoveDisp2);
        KFF_PRE_CASE(CardSave, KffCardSave);
        KFF_PRE_CASE(AutoSaveUpdate, KffAutoSaveUpdate);
        KFF_PRE_CASE(KagoExecuteStagger, KffKagoExecuteStagger);
        KFF_PRE_CASE(LinkProcCoWarpInit, KffLinkProcCoWarpInit);
        KFF_PRE_CASE(AlinkCheckRestartRoom, KffAlinkCheckRestartRoom);
        KFF_PRE_CASE(AlinkCheckRoofRestart, KffAlinkCheckRoofRestart);
        KFF_PRE_CASE(CameraStop, KffCameraStop);
        KFF_PRE_CASE(MainReset, KffMainReset);
        KFF_PRE_CASE(LinkSetMatrix, KffLinkSetMatrix);
        KFF_PRE_CASE(LinkSetArmMatrix, KffLinkSetArmMatrix);
        KFF_PRE_CASE(LinkFreeGrabItem, KffLinkFreeGrabItem);
        KFF_PRE_CASE(YmDelete, KffYmDelete);
        KFF_PRE_CASE(KagoExecuteLanding, KffKagoExecuteLanding);
        KFF_PRE_CASE(KagoExecuteLandingLakeHairia, KffKagoExecuteLandingLakeHairia);
        KFF_PRE_CASE(KagoExecuteLandingBoartHouse, KffKagoExecuteLandingBoartHouse);
        KFF_PRE_CASE(KagoDemoSkip, KffKagoDemoSkip);
        KFF_PRE_CASE(KagoExecutePerch, KffKagoExecutePerch);
        KFF_PRE_CASE(KagoInitPerchDemo, KffKagoInitPerchDemo);
        KFF_PRE_CASE(EventBitOn, KffEventBitOn);
        KFF_PRE_CASE(EventBitOff, KffEventBitOff);
        KFF_PRE_CASE(OrderSpeakEvent, KffOrderSpeakEvent);
        KFF_PRE_CASE(OrderPotentialEvent, KffOrderPotentialEvent);
        KFF_PRE_CASE(OrderChangeEventId, KffOrderChangeEventId);
        KFF_PRE_CASE(OrderOtherEventId, KffOrderOtherEventId);
        KFF_PRE_CASE(OrderDoorEvent, KffOrderDoorEvent);
        KFF_PRE_CASE(OrderItemEvent, KffOrderItemEvent);
        KFF_PRE_CASE(OrderTalkEvent, KffOrderTalkEvent);
        KFF_PRE_CASE(StageChangeScene4Event, KffStageChangeScene4Event);
        KFF_PRE_CASE(EvtControlSceneChange, KffEvtControlSceneChange);
        KFF_PRE_CASE(AlinkCheckSceneChange, KffAlinkCheckSceneChange);
        KFF_PRE_CASE(AlinkStartRestartRoom, KffAlinkStartRestartRoom);
        KFF_PRE_CASE(StageRestartRoom, KffStageRestartRoom);
        KFF_PRE_CASE(TagRetRmExecute, KffTagRetRmExecute);
        KFF_PRE_CASE(TagKagoFallExecute, KffTagKagoFallExecute);
        KFF_PRE_CASE(TagMwaitExecute, KffTagMwaitExecute);
        KFF_PRE_CASE(TagHstopExecute, KffTagHstopExecute);
        KFF_PRE_CASE(LinkCheckSwimAction, KffLinkCheckSwimAction);
        KFF_PRE_CASE(LinkCheckDamageAction, KffLinkCheckDamageAction);
        KFF_PRE_CASE(LinkProcWolfDamageInit, KffLinkProcWolfDamageInit);
        KFF_PRE_CASE(LinkProcWolfCargoCarry, KffLinkProcWolfCargoCarry);
        KFF_PRE_CASE(LinkProcCoLavaReturnInit, KffLinkProcCoLavaReturnInit);
        KFF_PRE_CASE(LinkProcCoMetamorphoseInit, KffLinkProcCoMetamorphoseInit);
        KFF_PRE_CASE(LinkCheckHorseRide, KffLinkCheckHorseRide);
        KFF_PRE_CASE(LinkCheckSpinnerRide, KffLinkCheckSpinnerRide);
#if defined(KFF_DEV_AUTOTEST)
        KFF_PRE_CASE(LinkSetDamagePoint, KffLinkSetDamagePoint);
        KFF_PRE_CASE(TestBadName, KffTestBadName);
#endif
    default:
        logWarn("addPre on %s refused for %s (no pre slot on this target)", hookName(h), g_owner);
        break;
    }
    if (r != MOD_OK) {
        dropRec(rec);
    }
    return countFailure(r);
}

ModResult Registry::addPost(Hook h, PostFn userFn, int32_t priority, void* userdata) {
    if (userFn == nullptr) {
        return countFailure(MOD_INVALID_ARGUMENT);
    }
    if (!ownerKnown()) {
        return countFailure(MOD_UNAVAILABLE);
    }
    if (const ModResult tie = checkPriority(h, Kind::Post, priority); tie != MOD_OK) {
        return countFailure(tie);
    }
    CallRec* rec = newRec(h, Kind::Post, priority, userdata);
    if (rec == nullptr) {
        return countFailure(MOD_ERROR);
    }
    rec->post = userFn;
    HookOptions o = HOOK_OPTIONS_INIT;
    o.priority = priority;
    o.userdata = rec;
    const PostFn fn = postThunk;
    ModResult r = MOD_UNSUPPORTED;
    switch (h) {
        KFF_POST_CASE(PadRead, KffPadRead);
        KFF_POST_CASE(AutoSaveUpdate, KffAutoSaveUpdate);
        KFF_POST_CASE(LinkExecute, KffLinkExecute);
        KFF_POST_CASE(KagoExecute, KffKagoExecute);
        KFF_POST_CASE(KagoExecuteFly, KffKagoExecuteFly);
        KFF_POST_CASE(KagoFlySpeedCalc, KffKagoFlySpeedCalc);
        KFF_POST_CASE(KagoCheckRoofHeight, KffKagoCheckRoofHeight);
        KFF_POST_CASE(KagoCheckGroundHeight, KffKagoCheckGroundHeight);
        KFF_POST_CASE(KagoCheckWallHitFall, KffKagoCheckWallHitFall);
        KFF_POST_CASE(KagoExecuteBalloonMenu, KffKagoExecuteBalloonMenu);
        KFF_POST_CASE(KagoDraw, KffKagoDraw);
        KFF_POST_CASE(KagoDelete, KffKagoDelete);
        KFF_POST_CASE(LinkPosMove, KffLinkPosMove);
        KFF_POST_CASE(LinkSetBodyPart, KffLinkSetBodyPart);
        KFF_POST_CASE(KagoSetSceneChange, KffKagoSetSceneChange);
        KFF_POST_CASE(KagoCheckNextPath, KffKagoCheckNextPath);
        KFF_POST_CASE(MeterDraw, KffMeterDraw);
        KFF_POST_CASE(KagoExecuteLanding, KffKagoExecuteLanding);
        KFF_POST_CASE(KagoExecuteLandingLakeHairia, KffKagoExecuteLandingLakeHairia);
        KFF_POST_CASE(KagoExecuteLandingBoartHouse, KffKagoExecuteLandingBoartHouse);
        KFF_POST_CASE(KagoDemoSkip, KffKagoDemoSkip);
        KFF_POST_CASE(KagoExecutePerch, KffKagoExecutePerch);
        KFF_POST_CASE(KagoInitPerchDemo, KffKagoInitPerchDemo);
        KFF_POST_CASE(EventBitOn, KffEventBitOn);
        KFF_POST_CASE(EventBitOff, KffEventBitOff);
        KFF_POST_CASE(OrderSpeakEvent, KffOrderSpeakEvent);
        KFF_POST_CASE(OrderPotentialEvent, KffOrderPotentialEvent);
        KFF_POST_CASE(OrderChangeEventId, KffOrderChangeEventId);
        KFF_POST_CASE(OrderOtherEventId, KffOrderOtherEventId);
        KFF_POST_CASE(OrderDoorEvent, KffOrderDoorEvent);
        KFF_POST_CASE(OrderItemEvent, KffOrderItemEvent);
        KFF_POST_CASE(OrderTalkEvent, KffOrderTalkEvent);
        KFF_POST_CASE(StageChangeScene4Event, KffStageChangeScene4Event);
        KFF_POST_CASE(EvtControlSceneChange, KffEvtControlSceneChange);
        KFF_POST_CASE(AlinkCheckSceneChange, KffAlinkCheckSceneChange);
        KFF_POST_CASE(AlinkStartRestartRoom, KffAlinkStartRestartRoom);
        KFF_POST_CASE(StageRestartRoom, KffStageRestartRoom);
        KFF_POST_CASE(TagRetRmExecute, KffTagRetRmExecute);
        KFF_POST_CASE(TagKagoFallExecute, KffTagKagoFallExecute);
        KFF_POST_CASE(TagMwaitExecute, KffTagMwaitExecute);
        KFF_POST_CASE(TagHstopExecute, KffTagHstopExecute);
        KFF_POST_CASE(LinkCheckSwimAction, KffLinkCheckSwimAction);
        KFF_POST_CASE(LinkCheckDamageAction, KffLinkCheckDamageAction);
        KFF_POST_CASE(LinkProcWolfDamageInit, KffLinkProcWolfDamageInit);
        KFF_POST_CASE(LinkProcWolfCargoCarry, KffLinkProcWolfCargoCarry);
        KFF_POST_CASE(LinkProcCoLavaReturnInit, KffLinkProcCoLavaReturnInit);
        KFF_POST_CASE(LinkProcCoMetamorphoseInit, KffLinkProcCoMetamorphoseInit);
        KFF_POST_CASE(LinkCheckHorseRide, KffLinkCheckHorseRide);
        KFF_POST_CASE(LinkCheckSpinnerRide, KffLinkCheckSpinnerRide);
#if defined(KFF_DEV_AUTOTEST)
        KFF_POST_CASE(LinkCheckPolyDamage, KffLinkCheckPolyDamage);
        KFF_POST_CASE(LinkCheckWolfLandAction, KffLinkCheckWolfLandAction);
#endif
    default:
        logWarn("addPost on %s refused for %s (no post slot on this target)", hookName(h), g_owner);
        break;
    }
    if (r != MOD_OK) {
        dropRec(rec);
    }
    return countFailure(r);
}

#undef KFF_PRE_CASE
#undef KFF_POST_CASE

ModResult Registry::replace(Hook h, ReplaceFn userFn, void* userdata) {
    if (userFn == nullptr) {
        return countFailure(MOD_INVALID_ARGUMENT);
    }
    if (!ownerKnown()) {
        return countFailure(MOD_UNAVAILABLE);
    }
    const size_t i = idx(h);
    if (!g_ownerIsCore) {
        ++g_conflicts;
        logError("MOD_CONFLICT: replace on %s by %s refused (only the core replaces)", hookName(h), g_owner);
        return countFailure(MOD_CONFLICT);
    }
    if (g_replaced[i]) {
        ++g_conflicts;
        logError("MOD_CONFLICT: replace on %s refused: %s already replaces it (requested by %s)", hookName(h),
            g_replaceOwner[i], g_owner);
        return MOD_CONFLICT;
    }
    CallRec* rec = newRec(h, Kind::Replace, 0, userdata);
    if (rec == nullptr) {
        return MOD_ERROR;
    }
    rec->repl = userFn;
    HookOptions o = HOOK_OPTIONS_INIT;
    o.userdata = rec;
    ModResult r = MOD_UNSUPPORTED;
    switch (h) {
    case Hook::KagoCheckWallHitFall:
        r = replaceOn<KffKagoCheckWallHitFall>(h, replaceThunk, o);
        break;
    case Hook::KagoExecuteBalloonMenu:
        r = replaceOn<KffKagoExecuteBalloonMenu>(h, replaceThunk, o);
        break;
    default:
        logWarn("replace on %s refused for %s (no replace slot on this target)", hookName(h), g_owner);
        break;
    }
    if (r != MOD_OK) {
        dropRec(rec);
        return r;
    }
    g_replaced[i] = true;
    g_replaceOwner[i] = g_owner;
    return MOD_OK;
}

size_t hookCallbackCount(const Module* m) {
    size_t n = 0;
    for (size_t i = 0; i < g_recCount; ++i) {
        if (g_recs[i].owner == m) {
            ++n;
        }
    }
    return n;
}

void registryResetOwnerFailures() {
    g_ownerFailures = 0;
}

uint32_t registryOwnerFailures() {
    return g_ownerFailures;
}

// Startbericht nach der Modulinstallation: Anzahl der Callbacks, Replaces, Konflikte und abgeschalteten Module.
void registryCheck() {
    size_t count = 0;
    Module* const* list = moduleList(count);
    size_t disabled = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!moduleActive(list[i])) {
            ++disabled;
        }
    }
    size_t replaces = 0;
    for (size_t i = 0; i < kHookCount; ++i) {
        if (g_replaced[i]) {
            ++replaces;
        }
    }
    logInfo("registry check: callbacks=%u replaces=%u modules=%u disabled=%u conflicts=%u",
        static_cast<unsigned>(g_recCount), static_cast<unsigned>(replaces), static_cast<unsigned>(count),
        static_cast<unsigned>(disabled), static_cast<unsigned>(g_conflicts));
}

Registry& registry() {
    static Registry instance;
    return instance;
}

// ---------------------------------------------------------------------------------------------
// Original-Aufruf (fuer Replace-Hooks)
// ---------------------------------------------------------------------------------------------

bool origCheckWallHitFall(daKago_c* k, int arg) {
    if (KffKagoCheckWallHitFall::g_orig == nullptr) {
        return false;
    }
    return KffKagoCheckWallHitFall::g_orig(k, arg);
}

int origExecuteBalloonMenu(daKago_c* k) {
    if (KffKagoExecuteBalloonMenu::g_orig == nullptr) {
        return 0;
    }
    return KffKagoExecuteBalloonMenu::g_orig(k);
}

// ---------------------------------------------------------------------------------------------
// Kern-Registrierung
// ---------------------------------------------------------------------------------------------

namespace core {
void setLinkExecuteReady(bool ready);                       // ride.cpp (HK-02)
HookAction mainResetPre(ModContext*, void*, void*, void*);  // ride.cpp (K4)
}  // namespace core

void installCoreHooks() {
    Registry& reg = registry();
    const char* previous = reg.owner();
    reg.setOwner("core");
    report("mDoCPd_c::read (post)", reg.addPost(Hook::PadRead, core::padReadPost, prio::kCore));
    const ModResult linkExecuteRes = reg.addPre(Hook::LinkExecute, core::linkExecutePre, prio::kCore);
    report("daAlink_c::execute (pre)", linkExecuteRes);
    core::setLinkExecuteReady(linkExecuteRes == MOD_OK);  // HK-02: ohne diesen Hook kein Ruf, B bleibt beim Spiel
    report("daKago_c::execute (pre)", reg.addPre(Hook::KagoExecute, core::kagoExecutePre, prio::kCore));
    report("daE_YM_c::_delete (pre, core)", reg.addPre(Hook::YmDelete, core::ymDeletePre, prio::kCore));
    report("daKago_c::checkWallHitFall (replace)",
        reg.replace(Hook::KagoCheckWallHitFall, core::kagoCheckWallHitFallReplace));
    report("daKago_c::executeBalloonMenu (replace)",
        reg.replace(Hook::KagoExecuteBalloonMenu, core::kagoExecuteBalloonMenuReplace));
#if defined(KFF_DEV_AUTOTEST)
    if (testFlag("KFF_TEST_CONFLICT")) {
        // Testbuild WP7.4-Ersatz: zweiter Replace auf dasselbe Ziel. Erwartet MOD_CONFLICT, der erste Replace bleibt aktiv.
        report("daKago_c::checkWallHitFall (replace, zweiter, Testbuild)",
            reg.replace(Hook::KagoCheckWallHitFall, core::kagoCheckWallHitFallReplace));
    }
#endif
    report("daKago_c::draw (pre)", reg.addPre(Hook::KagoDraw, core::kagoDrawPre, prio::kCore));
    report("daKago_c::_delete (pre)", reg.addPre(Hook::KagoDelete, core::kagoDeletePre, prio::kCore));
    report("daAlink_c::posMove (post, Befund 2)", reg.addPost(Hook::LinkPosMove, core::linkPosMovePost, prio::kCore));
    report("dComIfGp_setNextStage (pre, core)", reg.addPre(Hook::StageNextStage, core::stageNextStagePre, prio::kCore));
    report("daAlink_c::checkDeadAction (pre, core)",
        reg.addPre(Hook::LinkCheckDeadAction, core::linkCheckDeadActionPre, prio::kCore));
    report("daAlink_c::procCoDeadInit (pre, core)",
        reg.addPre(Hook::LinkProcCoDeadInit, core::linkProcCoDeadInitPre, prio::kCore));
    report("dComIfG_changeOpeningScene (pre, core)",
        reg.addPre(Hook::ChangeOpeningScene, core::openingScenePre, prio::kCore));
    report("dComIfGs_gameStart (pre, core)", reg.addPre(Hook::GameStart, core::gameStartPre, prio::kCore));
    report("dMenu_save_c::saveMoveDisp (pre, core)",
        reg.addPre(Hook::SaveMoveDisp, core::saveMoveDispPre, prio::kCore));
    report("dMenu_save_c::saveMoveDisp2 (pre, core)",
        reg.addPre(Hook::SaveMoveDisp2, core::saveMoveDisp2Pre, prio::kCore));
    report("mDoMemCd_Ctrl_c::save (pre, core)", reg.addPre(Hook::CardSave, core::cardSavePre, prio::kCore));
    report("updateAutoSave (pre, core, E11)", reg.addPre(Hook::AutoSaveUpdate, core::autoSavePre, prio::kCore));
    report("updateAutoSave (post, core, E11)", reg.addPost(Hook::AutoSaveUpdate, core::autoSavePost, prio::kCore));
    report("dStage_changeScene (pre, core, D1)",
        reg.addPre(Hook::StageChangeScene, core::stageChangeScenePre, prio::kCore));
    report("daAlink_c::procCoWarpInit (pre, core, D1)",
        reg.addPre(Hook::LinkProcCoWarpInit, core::linkProcCoWarpInitPre, prio::kCore));
    report("daKago_c::executeFly (pre, core, K6)",
        reg.addPre(Hook::KagoExecuteFly, core::kagoExecuteFlyPre, prio::kCore));
    report("mDoRst_resetCallBack (pre, core, K4)", reg.addPre(Hook::MainReset, core::mainResetPre, prio::kCore));
    report("daKago_c::executeStagger (pre, core, K6)",
        reg.addPre(Hook::KagoExecuteStagger, core::kagoExecuteStaggerPre, prio::kCore));
#if defined(KFF_DEV_AUTOTEST)
    report("daAlink_c::setDamagePoint (pre, Diagnose)",
        reg.addPre(Hook::LinkSetDamagePoint, core::linkSetDamagePointPre, prio::kCore));
    report("daAlink_c::checkPolyDamage (post, Diagnose)",
        reg.addPost(Hook::LinkCheckPolyDamage, core::linkCheckPolyDamagePost, prio::kCore));
    report("daAlink_c::procWolfDamageInit (pre, Diagnose)",
        reg.addPre(Hook::LinkProcWolfDamageInit, core::linkProcWolfDamageInitPre, prio::kCore));
    report("daAlink_c::checkWolfLandAction (post, Diagnose)",
        reg.addPost(Hook::LinkCheckWolfLandAction, core::linkCheckWolfLandActionPost, prio::kCore));
#endif
    reg.setOwner(previous);
}

}  // namespace kff
