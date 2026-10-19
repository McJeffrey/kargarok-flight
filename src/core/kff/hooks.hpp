// Hook-Registrierung des Kerns. Module hängen sich nur hierüber an Spielfunktionen.
//
// Regeln (WP7, Lebenszyklus und Koexistenz):
//  - Jedes Spielziel hat genau einen DEFINE_HOOK-Alias, und zwar in src/core/hooks.cpp.
//  - Pre- und Post-Hooks: beliebig viele. Reihenfolge: hoehere Prioritaet zuerst, bei Gleichstand
//    die Registrierungsreihenfolge. Zwei VERSCHIEDENE Besitzer mit gleicher Prioritaet auf demselben Ziel und
//    derselben Art (Pre oder Post) sind ein Konflikt: MOD_CONFLICT, der zweite Besitzer wird nicht eingetragen.
//  - Replace: pro Ziel hoechstens ein Besitzer, und nur der Kern (Besitzer "core"). Ein zweiter Replace wird mit
//    MOD_CONFLICT abgelehnt und im Log vermerkt (Kern: checkWallHitFall, executeBalloonMenu).
//  - Hoechstens EIN Besitzer je Ziel darf HOOK_SKIP_ORIGINAL liefern. Der Rueckgabewert steht erst im Callback fest,
//    deshalb wird die Pruefung zur Laufzeit gemacht: der erste SKIP-Liefernde behaelt das Recht. Ein anderer
//    Besitzer, der ebenfalls SKIP liefert, wird abgewiesen (das Original laeuft), als MOD_CONFLICT geloggt und sein
//    Modul wird abgeschaltet. Der Kern wird nicht abgeschaltet, dort bleibt es bei der Meldung.
//  - Modul inaktiv: Jeder Callback laeuft ueber eine Huelle, die den Besitzer prueft. Ein Modul, dessen install()
//    fehlschlaegt, oder das per fail_mod abgeschaltet wurde, bekommt keine Aufrufe mehr (auch nicht ueber
//    registrierte Hooks). Der SDK-Dispatch kann einzelne Callbacks nicht entfernen, daher bleibt die Huelle stehen.
//  - Registrierungsfehler (Konflikt, Ziel nicht aufloesbar, ungueltige Argumente, unbekannter Besitzer) zaehlen als
//    Fehler des Moduls. Der Kern schaltet ein Modul ab, dessen install() fehlschlaegt oder dessen Registrierungen
//    Fehler melden. Der Kern laeuft in jedem Fall weiter.
//
// Ausnahmen (festgelegt, Entscheidung D-WP7-EX):
//  - Pre-Callback wirft: die Huelle laesst die Ausnahme zum SDK durch. Das SDK wertet das als Modulfehler
//    (fail_mod, Log "Exception in pre hook callback") und behandelt die Pre-Kette als CONTINUE: das Original laeuft.
//    Das Modul wird beim naechsten Tick entladen (mod_shutdown beendet vorher den Ritt).
//  - Post-Callback wirft: wie Pre, nur ohne Original-Wirkung (Modulfehler, Entladen beim naechsten Tick).
//  - Replace wirft (nur Kern): die Huelle faengt die Ausnahme, loggt sie (begrenzt) und setzt das Ergebnis auf 0.
//    Das Original laeuft in diesem Aufruf NICHT. Die Huelle endet normal, damit das SDK den Replace als gelaufen
//    annimmt; sonst wuerde es nach einem Fehler das Original starten.
//  - Ein Callback wirft nie absichtlich. Ausnahme: der Testbuild (KFF_DEV_AUTOTEST) wirft unter KFF_TEST_UNLOAD und
//    KFF_TEST_THROW genau einmal, um Entladen und den SDK-Fehlerpfad zu pruefen (WP7-Tests, nie im Release).
//
// Ziele ohne Ergebnis im Binary (Dusklight v2.0.3, release): daKago_c::setEvent und daKago_c::setEvent2 sind
// Inline-Methoden im Header (d_a_kago.h:108, :114) und haben kein Symbol. Sie sind nicht hookbar. Ihr Zweck ist
// ueber den Ritt-Zustand (Session) und endRideClean/Eventgrenzen abgedeckt.
#pragma once

#include "core/kff/services.hpp"

#include <cstddef>
#include <cstdint>

class daKago_c;

namespace kff {

class Module;

// Spielfunktionen, die das Paket hookt. Die Reihenfolge ist nur Dokumentation.
enum class Hook : uint8_t {
    PadRead,                 // mDoCPd_c::read                       (nur Post)
    LinkExecute,             // daAlink_c::execute                   (Pre, Post)
    KagoExecute,             // daKago_c::execute                    (Pre, Post)
    KagoExecuteFly,          // daKago_c::executeFly                 (Pre, Post)
    KagoFlySpeedCalc,        // daKago_c::flySpeedCalc               (Pre, Post)
    KagoCheckRoofHeight,     // daKago_c::checkRoofHeight            (Pre, Post)
    KagoCheckGroundHeight,   // daKago_c::checkGroundHeight          (Pre, Post)
    KagoCheckWallHitFall,    // daKago_c::checkWallHitFall           (Pre, Post, Replace: Owner core)
    KagoExecuteBalloonMenu,  // daKago_c::executeBalloonMenu         (Pre, Post, Replace: Owner core)
    KagoDraw,                // daKago_c::draw                       (Pre, Post)
    KagoDelete,              // daKago_c::_delete                    (Pre, Post)
    KagoSetSceneChange,      // daKago_c::setSceneChange             (Pre, Post)
    KagoCheckNextPath,       // daKago_c::checkNextPath              (Pre, Post)
    KagoCheckFlySceneChange, // daKago_c::checkFlySceneChange        (Pre)
    StageChangeScene,        // dStage_changeScene (global)          (Pre)
    StageNextStage,          // dComIfGp_setNextStage, 4 Argumente   (Pre)
    MeterDraw,               // dMeter2Draw_c::draw                  (Post, Anzeige des Moduls tempo)
    LinkCheckDeadAction,     // daAlink_c::checkDeadAction           (Pre, kein SKIP: Tod beendet den Ritt)
    LinkProcCoDeadInit,      // daAlink_c::procCoDeadInit            (Pre, kein SKIP: zweites Netz fuer den Tod)
    ChangeOpeningScene,      // dComIfG_changeOpeningScene (Titel)   (Pre, kein SKIP)
    GameStart,               // dComIfGs_gameStart (Laden)           (Pre, kein SKIP)
    SaveMoveDisp,            // dMenu_save_c::saveMoveDisp           (Pre, E1: SKIP im Ritt bei jedem Speicher-Eintrag)
    SaveMoveDisp2,           // dMenu_save_c::saveMoveDisp2          (Pre, E1: SKIP im Ritt bei jedem Speicher-Eintrag)
    CardSave,                // mDoMemCd_Ctrl_c::save                (Pre, E1: SKIP im Ritt, Sicherheitsnetz, Ritt laeuft weiter)
    AutoSaveUpdate,          // updateAutoSave (dusk/autosave.cpp)   (Pre, Post, E11: Flag fuer den Autosave-Schreibvorgang)
    KagoExecuteStagger,      // daKago_c::executeStagger             (Pre, K6: Ersatzpfad fuer den eigenen Vogel)
    LinkProcCoWarpInit,      // daAlink_c::procCoWarpInit            (Pre, kein SKIP: Warp beendet den Ritt, D1)
    AlinkCheckRestartRoom,   // daAlink_c::checkRestartRoom          (Pre, P-RESTART: Void-/Fall-Neustart im Ritt, Modul freiheit)
    AlinkCheckRoofRestart,   // daAlink_c::checkRoofRestart          (Pre, P-RESTART: Kopf-Neustart im Ritt, Modul freiheit)
    CameraStop,              // dCamera_c::Stop                      (Pre, K22: Kamera-Stop im Ritt, Modul kamera)
    MainReset,               // mDoRst_resetCallBack (Soft-Reset)    (Pre, kein SKIP, K4: Ritt-Ende vor dem Titel)
    LinkPosMove,             // daAlink_c::posMove                   (Post)  - Modul mensch: Griff-Pin
    LinkSetMatrix,           // daAlink_c::setMatrix                 (Pre)   - Modul mensch: Pose
    LinkSetBodyPart,         // daAlink_c::setBodyPartPos            (Post)  - Modul mensch: Messung Haende
    LinkSetArmMatrix,        // daAlink_c::setArmMatrix              (Pre)   - Modul mensch: Ellbogen
    LinkFreeGrabItem,        // daAlink_c::freeGrabItem              (Pre)   - Modul mensch: Loslassen sperren
    YmDelete,                    // daE_YM_c::_delete                    (Pre, nur Kern: E_YM-Lock, v1-Paritaet)
    // WP7 Hook-Ziele fuer die Wellen 2 und 3 (Hookbarkeit gegen das Binary v2.0.3 geprueft, alle als T-Symbol).
    KagoExecuteLanding,          // daKago_c::executeLanding             (Pre, Post; Landeanflug)
    KagoExecuteLandingLakeHairia,   // daKago_c::executeLandingLakeHairia   (Pre, Post)
    KagoExecuteLandingBoartHouse,   // daKago_c::executeLandingBoartHouse   (Pre, Post)
    KagoDemoSkip,                // daKago_c::demo_skip(int)             (Pre, Post)
    KagoExecutePerch,            // daKago_c::executePerch               (Pre, Post)
    KagoInitPerchDemo,           // daKago_c::initPerchDemo              (Pre, Post)
    EventBitOn,                  // dComIfGs_onEventBit                  (Pre, Post)
    EventBitOff,                 // dComIfGs_offEventBit                 (Pre, Post)
    OrderSpeakEvent,             // fopAcM_orderSpeakEvent               (Pre, Post)
    OrderPotentialEvent,         // fopAcM_orderPotentialEvent           (Pre, Post)
    OrderChangeEventId,          // fopAcM_orderChangeEventId            (Pre, Post)
    OrderOtherEventId,           // fopAcM_orderOtherEventId             (Pre, Post)
    OrderDoorEvent,              // fopAcM_orderDoorEvent                (Pre, Post)
    OrderItemEvent,              // fopAcM_orderItemEvent                (Pre, Post)
    OrderTalkEvent,              // fopAcM_orderTalkEvent                (Pre, Post)
    StageChangeScene4Event,      // dStage_changeScene4Event             (Pre, Post)
    EvtControlSceneChange,       // dEvt_control_c::sceneChange          (Pre, Post)
    AlinkCheckSceneChange,       // daAlink_c::checkSceneChange          (Pre, Post)
    AlinkStartRestartRoom,       // daAlink_c::startRestartRoom          (Pre, Post)
    StageRestartRoom,            // dStage_restartRoom                   (Pre, Post)
    TagRetRmExecute,             // daTagRetRm_c::execute                (Pre, Post)
    TagKagoFallExecute,          // daTagKagoFall_c::execute             (Pre, Post)
    TagMwaitExecute,             // daTagMwait_c::execute                (Pre, Post)
    TagHstopExecute,             // daTagHstop_c::execute                (Pre, Post)
    LinkCheckSwimAction,         // daAlink_c::checkSwimAction           (Pre, Post)
    LinkCheckDamageAction,       // daAlink_c::checkDamageAction         (Pre, Post)
    LinkProcWolfDamageInit,      // daAlink_c::procWolfDamageInit        (Pre, Post; Schadensdiagnose nur im Testbuild)
    LinkProcWolfCargoCarry,      // daAlink_c::procWolfCargoCarry        (Pre, Post)
    LinkProcCoLavaReturnInit,    // daAlink_c::procCoLavaReturnInit      (Pre, Post)
    LinkProcCoMetamorphoseInit,  // daAlink_c::procCoMetamorphoseInit    (Pre, Post)
    LinkCheckHorseRide,          // daAlink_c::checkHorseRide (const)    (Pre, Post)
    LinkCheckSpinnerRide,        // daAlink_c::checkSpinnerRide (const)  (Pre, Post)
#if defined(KFF_DEV_AUTOTEST)
    LinkSetDamagePoint,      // daAlink_c::setDamagePoint            (Pre, nur Testbuild: Schadensdiagnose, Fix C)
    LinkCheckPolyDamage,     // daAlink_c::checkPolyDamage           (Post, nur Testbuild: Schadensdiagnose)
    LinkCheckWolfLandAction, // daAlink_c::checkWolfLandAction       (Post, nur Testbuild: Schadensdiagnose)
    TestBadName,             // Testbuild WP7.8: Symbol, das es nicht gibt (nur KFF_TEST_BADNAME, Modul freiheit)
#endif
    kCount
};

// P-RESTART (freiheit-nacharbeit.md): der Kern bietet die Pre-Hooks checkRestartRoom und checkRoofRestart an. Das Modul
// freiheit registriert sie nur unter dieser Bedingung.
#define KFF_CORE_RESTART_HOOKS 1

// K22 (Kamera-Stop): der Kern bietet Hook::CameraStop an. Das Modul kamera registriert ihn nur unter dieser Bedingung.
#define KFF_CORE_CAMERA_STOP_HOOK 1

// Prioritaeten (hoeher = frueher). Der Kern laeuft vor allen Modulen.
namespace prio {
constexpr int32_t kCore = 1000;
constexpr int32_t kFlug = 500;
constexpr int32_t kFreiheit = 400;
constexpr int32_t kGriff = 350;
constexpr int32_t kKamera = 300;
constexpr int32_t kAutowolf = 250;
constexpr int32_t kTempo = 200;
constexpr int32_t kMensch = 150;
}  // namespace prio

// Die Callback-Typen des SDK. Argumente: args[0] ist bei Methoden `this`, Parameter folgen.
using PreFn = HookPreFn;
using PostFn = HookPostFn;
using ReplaceFn = HookReplaceFn;

// Typisierter Zugriff auf ein Argument, z. B. argOf<daKago_c*>(args, 0).
template <class T>
T argOf(void* args, int n) {
    return mods::arg<T>(args, n);
}

class Registry {
public:
    // Besitzer fuer Logs und Replace-Konflikte. Der Kern setzt ihn auf den Modulnamen vor jedem install().
    void setOwner(const char* owner);
    const char* owner() const;

    // Pre-Hook. Rueckgabe HOOK_SKIP_ORIGINAL unterdrueckt das Original und die restlichen Pre-Hooks.
    ModResult addPre(Hook h, PreFn fn, int32_t priority, void* userdata = nullptr);
    // Post-Hook. Laeuft immer, auch nach SKIP. Schreibt er in *retval, aendert er den Rueckgabewert.
    ModResult addPost(Hook h, PostFn fn, int32_t priority, void* userdata = nullptr);
    // Replace. Ersetzt das Original. Pro Ziel hoechstens einer, sonst MOD_CONFLICT. Nur der Kern (Besitzer "core").
    ModResult replace(Hook h, ReplaceFn fn, void* userdata = nullptr);

    // Qualifizierter Name des Ziels fuer Logs, z. B. "daKago_c::executeFly".
    static const char* hookName(Hook h);
};

Registry& registry();

// Ruft das Original auf (nur gueltig nach der Installation durch den Kern).
bool origCheckWallHitFall(daKago_c* k, int arg);
int origExecuteBalloonMenu(daKago_c* k);

// Anzahl der Callbacks, die ein Modul registriert hat (fuer den Log beim Abschalten, WP7.8).
size_t hookCallbackCount(const Module* m);

// Fehlschlaege der Registrierungen des Moduls, das gerade install() ausfuehrt. Der Kern setzt den Zaehler vor install()
// zurueck und schaltet das Modul ab, wenn er danach nicht 0 ist.
void registryResetOwnerFailures();
uint32_t registryOwnerFailures();

// Startbericht nach der Modulinstallation (Callbacks, Replaces, Konflikte, abgeschaltete Module).
void registryCheck();

// Klartext eines ModResult, z. B. "MOD_CONFLICT".
const char* modResultName(ModResult r);

#if defined(KFF_DEV_AUTOTEST)
// Testbuild: Umgebungsschalter der WP7-Tests. true, wenn die Variable den Wert "1" hat.
bool testFlag(const char* name);
#endif

}  // namespace kff
