// Modul "flug": Flugmodell des freundlichen Kargarok im Ritt (Version 2.3.0).
//
// Drei Haken am Vogel. Sie wirken nur fuer den eigenen Vogel in Phase Riding (kff::isRiding, kff::isOwnBird)
// im Flugmodus (mAction FLY). Sonst bleibt die Vanilla-Lenkung unberuehrt.
//
//   pre  daKago_c::executeFly    Fassrolle (Flanke von Y) und Eingabe dieses Frames. Laeuft vor checkHeight()
//                                und vor dem Modus-Switch. Hoehen gehoeren freiheit (Prioritaet 400, nach flug).
//   pre  daKago_c::flySpeedCalc  HOOK_SKIP_ORIGINAL in Modus 2: Nick, Gier und Rolle je Schritt, kontinuierlich,
//                                ohne die Klemme der Vanilla-Lenkung auf +-0x2aaa. Die Schraeglage folgt der Gierrate.
//                                executeFly ruft flySpeedCalc nur in Modus 2 auf (d_a_kago.cpp:1160).
//   post daKago_c::executeFly    Eigene Geschwindigkeit V und der Weg je Tick: mFlySpeed = V,
//                                speedF = V * cos(Nick) * Gang, speed.y = -V * sin(Nick) * Gang.
//                                Die Geschwindigkeit zeigt in die Nase, auch kopfueber.
//
// Lage. mDoMtx_ZXYrotM (m_Do_mtx.cpp:95, von daKago_c::mtx_set genutzt) bildet M = T * Ry(Gier) * Rx(Nick) * Rz(Rolle).
// Vorwaerts = M * (0,0,1). Nick und Rolle sind fortlaufende s16-Winkel (65536 = 360 Grad), nie zerlegt: der Vogel
// geht ueber die Senkrechte und den Ruecken. Ab 90 Grad Nick ist cos(Nick) < 0 und die Geschwindigkeit zeigt
// rueckwaerts in die Nase (Scheitel einer Schleife). Wolf-Link (Proc 312) haengt an getLegR3Mtx() und folgt der Lage.
//
// Geschwindigkeit. V laeuft mit 2 Einheiten je Frame auf das Hoechsttempo (2 x cruise_speed, Standard 100).
// Der Gang skaliert nur den Weg je Tick (Gang 3 = 0,5, bei Standard 50 Einheiten je Frame). Vanilla zieht mFlySpeed
// in executeFly mit einem eigenen Ziel nach. Deshalb fuehrt flug V selbst und setzt mFlySpeed danach auf V.
//
// Steuerung (Pad 1, Eingaben des Kerns unveraendert, relativ zum Vogel, nie kamerarelativ):
//   linker Stick X   Kurve: Gier und Schraeglage. Voll = 360 Grad in 128 Frames, gehalten = Dauerkreis.
//   linker Stick Y   Nick. Vanilla-Vorzeichen: Y > 0 = Nase nach unten. Gehalten voll = Schleife.
//   neutral          Trimmung: Betrag von Stick Y unter 0,1, dann geht der Nick mit 2,8 Grad je Frame auf die
//                    Horizontale. Der Vogel fliegt ohne Eingabe geradeaus und sinkt nicht. Steigen und Sinken
//                    kommen nur ueber Stick Y oder eine Nicklage.
//   lenkung_invertieren  Nick umgekehrt (Stick hoch = Nase hoch). Gier und Rollrichtung bleiben. Einzige Quelle der
//                    Umkehr (OE8); der Altschluessel invert_steer wirkt als Oder-Alias. Die Dusklight-Option invertAirSwimY
//                    wirkt im Ritt nicht. invertAirSwimX und der Spiegelmodus wirken auf Stick X wie im Spiel (mStickX).
//   Y antippen       Fassrolle, 360 Grad in 64 Frames. Richtung links, wenn der Stick links steht, sonst rechts.
//                    Rollen laeuft bewusst nicht ueber den C-Stick: den besitzt der Kamera-Operator.
//   cruise_speed     Reisegeschwindigkeit (Gang 3). Hoechsttempo = 2 x cruise_speed.
//
// Landeanflug nach B (E9, Session::landing, Kern): der Vogel bleibt waagerecht. Nick, Rolle und Schraeglage gehen weich
// auf 0 (Trimmung im Landeanflug), Stick Y und die Fassrolle wirken nicht. Die Gier bleibt beim Stick X. Der Weg
// je Tick folgt dem Faktor des Moduls tempo, der im Landeanflug auf 0 sinkt: dann steht der Vogel.
//
// Zustand. Phase, Vogel, Link und Ritt-Frames kommen aus der Sitzung des Kerns (kff::session()). Den Lagezustand
// (Nick-, Gier- und Rollsummen, Fassrolle, Trimmung, V) fuehrt g_att, siehe Vertragshinweis dort. onRideStart()
// setzt ihn zurueck.
//
// Hoehen gehoeren dem Modul freiheit, dem einzigen Besitzer von Boden und Decke (Prioritaet 400, laeuft nach flug).
// flug schreibt keine Hoehenwerte, setzt keine Grenzen und liest min_ground_clearance nicht.
//
// Testszenarien (KFF_AUTOTEST=looping, geradeaus, geradeaus2, geradeaus-b, trimm, extrem) und das Messinstrument K18
// (Fremdgier zwischen zwei Flugschritten) nur im Testbuild (KFF_DEV_AUTOTEST). Eingaben nur ueber den Kern-Kontext.
// Im Release ist kein Szenario-Code kompiliert (D11).
//
// Im Ritt schreibt nur dieses Modul die Gier (Stick X, Totzone 0,02). Freiheit dreht den Kurs nicht
// (kreisflug.md, Nachtrag). Der Rueckfall auf die Vanilla-Lenkung im Ritt ist abgeschaltet (HOOK_SKIP_ORIGINAL).
// Die Nicklage bleibt ohne Vanilla-Rueckstellung: field_0x710/712 werden je Schritt genullt (bewusste Vereinfachung).

#include "d/dolzel_rel.h"

#include "core/kff/actions.hpp"
#include "core/kff/kff.hpp"

#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "m_Do/m_Do_controller_pad.h"
#if defined(KFF_DEV_AUTOTEST)
#include "d/d_bg_s_gnd_chk.h"  // WP8-Szenarien: Bodenhoehe vor dem Absteigen
#include "d/d_bg_s_lin_chk.h"  // WP8-Szenarien: Sweep des Startpunkts (OE7)
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_iter.h"  // WP8-Szenarien: Voegel zaehlen
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kff {
namespace {

// Raten je Frame bei Vollausschlag, in s16-Einheiten (65536 = 360 Grad). Werte des Looping-Prototyps.
constexpr float kPitchRateMax = 640.0f;    // rund 3,5 Grad je Frame: eine Schleife in 103 Frames
constexpr float kYawRateMax = 512.0f;      // rund 2,8 Grad je Frame: 360 Grad in 128 Frames
constexpr float kRollRateMax = 1024.0f;    // rund 5,6 Grad je Frame
constexpr float kBankMax = 8192.0f;        // sichtbare Schraeglage bei voller Gierrate, 45 Grad
constexpr int32_t kBarrelFrames = 64;      // Fassrolle: 64 * 1024 = 65536 = 360 Grad
constexpr int32_t kBarrelHalfFrames = 32;  // Halbzeit der Fassrolle, 180 Grad
constexpr float kDirDeadZone = 0.2f;       // Fassrolle links, wenn Stick X unter -0,2; sonst rechts
constexpr int32_t kFlyMode = 2;            // mMode der Flugschleife (executeFly, case 2)
#if defined(KFF_DEV_AUTOTEST)
constexpr uint32_t kLogEvery = 20;         // Ritt-Zeile alle 20 Frames (nur Testbuild)
#endif
constexpr float kTrimDeadZone = 0.10f;     // Betrag von Stick Y darunter gilt als neutral: Trimmung
constexpr int32_t kTrimRate = 0x0200;      // Trimmung: 512 Einheiten = 2,8 Grad je Frame, 90 Grad in 32 Frames
constexpr float kSpeedAccel = 2.0f;        // Anlauf von V in Einheiten je Frame
constexpr float kStickDeadZone = 0.02f;    // Totzone des Sticks X, nur fuer die Gier (kreisflug.md 5.3b)
constexpr float kGearDefault = 0.5f;       // Gang 3 (Standard): gilt auch bei NaN und Werten ausserhalb 0..1 (Weg)
constexpr float kGearMinRate = 0.30f;      // Untergrenze der Nick-, Gier- und Schraegenrate: Gang 1 (E9)
// Schweben (Gang 0, Entwurf H1 4.4). Stick Y steigt oder sinkt mit kHoverVy je Frame, Anlauf kHoverAccel je Frame.
constexpr float kHoverVy = 4.0f;          // Einheiten je Frame (1,2 m/s bei 1 Einheit = 1 cm und 30 Frames je Sekunde)
constexpr float kHoverAccel = 0.25f;      // Anlauf der senkrechten Rate je Frame, rund 16 Frames bis zur vollen Rate
constexpr float kHoverBank = 0.30f;       // Schraegelage im Schweben: 30 Prozent der Schraege im Flug (etwa 14 Grad)
constexpr float kHoverBlockShare = 0.5f;  // Steigen gilt als blockiert, wenn je Schritt weniger als die halbe Rate ankommt (H5)
bool g_hoverAnim = false;                 // Animation HOVERING ist im Schweben gesetzt (sonst FLY)
bool g_hoverBlocked = false;              // Steigen an einer Decke blockiert: Rate und Ziel 0, bis der Stick nicht mehr steigt
bool g_hoverMeasured = false;             // der letzte Schritt war ein Schwebeschritt, seine Messung gilt
uint32_t g_hoverFrame = 0;                // Ritt-Frame dieses Schritts
float g_hoverY = 0.0f;                    // Hoehe vor diesem Schritt (vor der Bewegung von flug und freiheit)
float g_hoverRise = 0.0f;                 // Rate, die dieser Schritt gesetzt hat
// Ressourcen-IDs der Vogel-Animationen aus res/Object/E_yc.h (Version RZDE01_00): dRes_ID_E_YC_BCK_YC_FLY_e und
// dRes_ID_E_YC_BCK_YC_HOVERING_e. setBck bildet sie fuer den freundlichen Kargarok (mType 1) ueber daKago_c::getBckName
// auf die KC-Animationen ab (HOVERING ist dort KC 0xD). Die Header liegen nicht im Mod-Include-Pfad, daher die Zahlen.
constexpr int kBckYcFly = 0x9;
constexpr int kBckYcHovering = 0x15;

// Lagezustand des Vogels liegt in kff::Session::att (Schnittstellenvertrag Teil 2, Schreiber: flug).
// Gleiche Feldnamen; g_att ist eine Referenz auf die Sitzung.
using Attitude = AttitudeState;
Attitude& g_att = session().att;

#if defined(KFF_DEV_AUTOTEST)
uint32_t g_logFlight = 0;
#endif
uint32_t g_logErrors = 0;
#if defined(KFF_DEV_AUTOTEST)
uint32_t g_padFrames = 0;     // Frames mit aktivem Testrahmen
bool g_testFinished = false;  // das Szenario dieses Moduls hat "fertig" gemeldet
#endif

// Grad aus s16-Einheiten. Auch fuer die ungeklemmten Summen (long long).
double degOf(long long units) {
    return static_cast<double>(units) * 360.0 / 65536.0;
}

#if defined(KFF_DEV_AUTOTEST)
// Scheitelband des Nicks: wechselt bei jedem Durchgang durch +-180 Grad, also wenn der Vogel kopfueber ist.
long long apexBand(long long cumPitch) {
    return static_cast<long long>(std::floor(static_cast<double>(cumPitch + 32768) / 65536.0));
}
#endif

bool steerInverted() {
    return config::getBool(config::Key::InvertSteer);
}

// Stick Y dieses Frames aus dem Kern-Schnappschuss (eine Quelle der Wahrheit). Der Rohwert ist derselbe wie Vanilla
// mStickY (d_a_kago.cpp:3531, getStickY = mMainStickPosY, ohne Skalierung und Totzone). Vanilla skaliert im
// Wasserfall (x0,5 nach unten, x1,5 nach oben, d_a_kago.cpp:3552-3557): das wird hier nachgebildet. Die Dusklight-Option
// invertAirSwimY (d_a_kago.cpp:3537-3539) wirkt auf den Vogel nicht mehr; gewirkt hat sie bisher nur doppelt mit
// invert_steer (XOR). Umgekehrt wird nur lenkung_invertieren ausgewertet, mit invert_steer als Oder-Alias (OE8).
float stickYFrame(const daKago_c* k) {
    float y = input().stickY;
    if (k->mIsWaterfall) {
        y = y < 0.0f ? y * 0.5f : y * 1.5f;
    }
    return steerInverted() ? -y : y;
}

// cruise_speed ist die Reisegeschwindigkeit (Gang 3). Das Hoechsttempo ist das Doppelte.
float cruiseSpeed() {
    return static_cast<float>(config::getFloat(config::Key::CruiseSpeed));
}

float topSpeed() {
    return 2.0f * cruiseSpeed();
}

// Gang-Faktor des Weges je Tick: Session::speedScale (Modul tempo), Gang 3 = 0,5. Im Landeanflug (E9) sinkt er auf 0,
// dann steht der Vogel. Ein NaN besteht std::clamp unveraendert und wuerde Nick, Gier und Schraege zerstoeren
// (review-flug-tempo.md F2): darum eigener Test. NaN besteht den Vergleich nicht und faellt auf Gang 3.
float wegFactor() {
    const float s = session().speedScale;
    return (s >= 0.0f && s <= 1.0f) ? s : kGearDefault;
}

// Rate fuer Nick, Gier und Schraege: wie der Gang, aber nie unter Gang 1. So bleibt der Kurs im Landeanflug steuerbar,
// auch wenn der Weg auf 0 sinkt.
float rateFactor() {
    return std::max(wegFactor(), kGearMinRate);
}

// Weich auf die Waagerechte (0): Schritt je Frame, ein Achtel des Abstands, hoechstens maxStep, mindestens 16 Einheiten.
// cLib_addCalcAngleS arbeitet in s16 und nimmt den kuerzeren Weg ueber den Scheitel.
s16 levelStep(s16 value, s16 maxStep) {
    s16 next = value;
    cLib_addCalcAngleS(&next, 0, 8, maxStep, 16);
    return static_cast<s16>(next - value);
}

// Eigener Vogel im Ritt mit Flugmodus. Den Modus prueft jeder Hook selbst.
bool isFlyingOwnBird(daKago_c* k) {
    return k != nullptr && isRiding() && isOwnBird(k) && k->mAction == daKago_c::ACTION_FLY_e;
}

// Trimmung: Schritt des Nicks zur Horizontalen (0), begrenzt auf kTrimRate. Nick ist s16, der Abstand ist also
// hoechstens 180 Grad, und die Richtung folgt vorzeichenrichtig dem s16-Wert.
int16_t trimStep(int16_t nick) {
    int32_t step = -static_cast<int32_t>(nick);
    if (step > kTrimRate) {
        step = kTrimRate;
    } else if (step < -kTrimRate) {
        step = -kTrimRate;
    }
    return static_cast<int16_t>(step);
}

// Fassrolle beginnt. Die Flanke von Y startet nur, wenn gerade keine laeuft.
void startBarrel() {
    if (g_att.barrelLeft > 0) {
        logEvery(g_logErrors, 3, 300, "flug: Y-Tipp waehrend einer Fassrolle ignoriert");
        return;
    }
    g_att.barrelDir = g_att.stickX < -kDirDeadZone ? -1 : 1;
    g_att.barrelLeft = kBarrelFrames;
    ++g_att.barrels;
    logInfo("flug: FASSROLLE %u start richtung=%s stickX=%.2f f=%u", g_att.barrels,
        g_att.barrelDir < 0 ? "links" : "rechts", static_cast<double>(g_att.stickX), session().rideFrames);
}

// Schweben, Animation (Entwurf H1 4.4, 5.4): HOVERING (0x15) beim Eintritt, FLY (0x9) beim Austritt. Solange der Vogel
// schwebt, zaehlt field_0x728 nicht auf 0. Dann ruft executeFly setFlyAnime (FLY_WL oder GLIDE_WL, mit dem Gleitgeraeusch)
// nicht auf. field_0x728 wird in daKago_c::execute vor executeFly heruntergezaehlt, deshalb hier je Frame auf 1 gesetzt.
void hoverAnimation(daKago_c* k, bool hover) {
    if (hover && !g_hoverAnim) {
        g_hoverAnim = true;
        k->setBck(kBckYcHovering, 2, 10.0f, 1.0f);
        logInfo("flug: Animation HOVERING (Schweben) f=%u", session().rideFrames);
    } else if (!hover && g_hoverAnim) {
        g_hoverAnim = false;
        k->setBck(kBckYcFly, 2, 10.0f, 1.0f);
        logInfo("flug: Animation FLY (Schweben aus) f=%u", session().rideFrames);
    }
    if (hover && k->field_0x728 == 0) {
        k->field_0x728 = 1;
    }
}

// Schweben, senkrechte Rate: Ziel ist -Stick Y mal kHoverVy (Stick hoch sinkt, Schalter lenkung_invertieren kehrt es um,
// stickYFrame wirkt schon), Totzone kTrimDeadZone. Ohne Schweben ist das Ziel 0, die Rate klingt in rund 16 Frames ab.
// Steigen an einer Decke (H5, Befund H4 F1 B4a): freiheit schiebt den Vogel nach jedem Anstieg wieder unter die Decke
// (slideMove im Post von daKago_c::execute, freiheit.cpp:2083). Die Rate bliebe gesetzt, der Vogel pendelte mit Periode
// 3 Ticks (608, 612, 616, 608 bei Rate 4,00). Kommt vom letzten Schritt weniger als die halbe Rate an, ist das Steigen
// blockiert: Rate und Ziel stehen auf 0, bis der Stick nicht mehr steigt. Sinken bleibt moeglich. Gemessen wird der
// Schritt des Vorframes: Hoehe vor dem Schritt gegen die Hoehe jetzt, also nach dem Gleiten von freiheit.
void hoverVertical(const daKago_c* k) {
    const bool hover = session().hover && !session().landing;
    const bool rising = hover && g_att.stickY <= -kTrimDeadZone;  // Stick verlangt Steigen (Ziel > 0)
    const uint32_t frame = session().rideFrames;
    if (hover && g_hoverMeasured && frame == g_hoverFrame + 1 && g_hoverRise > 0.0f
        && k->current.pos.y - g_hoverY < kHoverBlockShare * g_hoverRise) {
        g_hoverBlocked = true;
        g_att.hoverVy = 0.0f;
        logInfo("flug: Steigen an der Decke blockiert, Rate 0 (Schritt %.2f statt %.2f, Hoehe %.1f, f=%u)",
            static_cast<double>(k->current.pos.y - g_hoverY), static_cast<double>(g_hoverRise),
            static_cast<double>(k->current.pos.y), frame);
    }
    if (!rising) {
        g_hoverBlocked = false;  // Stick steigt nicht (mehr): Steigen darf wieder anlaufen
    }
    float ziel = (hover && std::fabs(g_att.stickY) >= kTrimDeadZone) ? -g_att.stickY * kHoverVy : 0.0f;
    if (g_hoverBlocked) {
        ziel = 0.0f;
    }
    cLib_chaseF(&g_att.hoverVy, ziel, kHoverAccel);
    g_hoverMeasured = hover;
    g_hoverFrame = frame;
    g_hoverY = k->current.pos.y;
    g_hoverRise = g_att.hoverVy;  // die Rate, die der Aufrufer gleich auf speed.y addiert
}

#if defined(KFF_DEV_AUTOTEST)
// Eine Zeile je 20 Frames: Lage, Summen, Eingabe, Trimmung, Geschwindigkeit und Link. Wird nach dem Flugschritt geschrieben.
void logFlight(daKago_c* k) {
    daAlink_c* link = liveLink();
    const unsigned linkProc = link != nullptr ? static_cast<unsigned>(link->mProcID) : 0xFFFFu;
    const int cargo = link != nullptr && link->checkCargoCarry() ? 1 : 0;
    const int inverted = cM_scos(k->current.angle.x) < 0.0f ? 1 : 0;
    logEvery(g_logFlight, 1, kLogEvery,
        "flug: f=%u mode=%d pos=(%.1f,%.1f,%.1f) nick=%.2f gier=%.2f rolle=%.1f schraeg=%.1f kopfueber=%d "
        "cumNick=%.1f cumGier=%.1f cumRolle=%.1f stick=(%.2f,%.2f) trim=%d fass=%d wf=%d speedF=%.2f V=%.2f "
        "link=%u cargo=%d mGround=%.0f mRoof=%.0f",
        session().rideFrames, static_cast<int>(k->mMode), static_cast<double>(k->current.pos.x),
        static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z),
        degOf(k->current.angle.x), degOf(k->current.angle.y), degOf(g_att.roll), degOf(g_att.bank), inverted,
        degOf(g_att.cumPitch), degOf(g_att.cumYaw), degOf(g_att.cumRoll), static_cast<double>(g_att.stickX),
        static_cast<double>(g_att.stickY), g_att.trimming ? 1 : 0, g_att.barrelLeft > 0 ? 1 : 0,
        k->mIsWaterfall ? 1 : 0, static_cast<double>(k->speedF), static_cast<double>(g_att.speed), linkProc, cargo,
        static_cast<double>(k->mGroundHeight), static_cast<double>(k->mRoofHeight));
}
#endif

#if defined(KFF_DEV_AUTOTEST)
// K18 (WP8.9, nur Testbuild): Gier zwischen zwei Flugschritten, die nicht von flug stammt (Fremdgier). Jeder Flugschritt
// merkt sich den Kurs nach seinem eigenen Schreiben, der naechste vergleicht. Eine Abweichung kommt von einem anderen
// Schreiber: freiheit mit dem Testschalter KFF_WALL_STEER (Gleitdrehung 0x600 = 8,4375 Grad, kSlideTurn), Vanilla-Code
// ausserhalb von Modus 2 oder ein Szenario. Fenster Ritt-Frame 250 bis 287 (Gang-1-Gerade im Szenario tempo): eine
// Zeile je Schritt mit Gesamt-, Flug- und Fremdgier.
constexpr uint32_t kK18WinFrom = 250;
constexpr uint32_t kK18WinTo = 287;
constexpr uint32_t kK18LogMax = 20;  // hoechstens so viele FREMDGIER-Zeilen je Ritt
constexpr int kK18SlideStep = 0x0600;  // Gleitdrehung von freiheit (kSlideTurn), nur mit KFF_WALL_STEER=1

struct K18State {
    bool have = false;          // yawAfter gueltig (mindestens ein Flugschritt in diesem Ritt)
    s16 yawAfter = 0;           // Kurs nach dem letzten Schritt von flug
    uint32_t steps = 0;         // Flugschritte mit Messung
    uint32_t foreignSteps = 0;  // davon mit Fremdgier
    long long foreignSum = 0;   // Summe der Fremdgier (s16-Einheiten)
    int foreignMax = 0;         // groesster Betrag eines Fremdschritts
    uint32_t slideSteps = 0;    // Fremdschritte mit Betrag 0x600
    uint32_t winSteps = 0;      // Schritte im Fenster
    uint32_t winForeign = 0;    // davon mit Fremdgier
    uint32_t winFlug = 0;       // davon mit Gier von flug (Stick)
    uint32_t logged = 0;
};
K18State g_k18;

int k18Foreign(const daKago_c* k) {
    return g_k18.have ? static_cast<int>(static_cast<s16>(k->current.angle.y - g_k18.yawAfter)) : 0;
}

void k18Record(const daKago_c* k, int foreign, s16 dYaw, float stickX) {
    K18State& s = g_k18;
    const uint32_t f = session().rideFrames;
    ++s.steps;
    if (foreign != 0) {
        const int mag = foreign < 0 ? -foreign : foreign;
        ++s.foreignSteps;
        s.foreignSum += foreign;
        s.foreignMax = std::max(s.foreignMax, mag);
        if (mag == kK18SlideStep) {
            ++s.slideSteps;
        }
        if (s.logged < kK18LogMax) {
            ++s.logged;
            logInfo("flug: K18 FREMDGIER f=%u schritt=%.4f Grad (%d Einheiten) stickX=%.3f gang=%d", f, degOf(foreign),
                foreign, static_cast<double>(stickX), session().gear);
        }
    }
    if (f >= kK18WinFrom && f <= kK18WinTo) {
        ++s.winSteps;
        if (foreign != 0) {
            ++s.winForeign;
        }
        if (dYaw != 0) {
            ++s.winFlug;
        }
        logInfo("flug: K18 f=%u dGier=%.4f flug=%.4f fremd=%.4f stickX=%.3f gang=%d", f, degOf(foreign + dYaw),
            degOf(dYaw), degOf(foreign), static_cast<double>(stickX), session().gear);
    }
    s.yawAfter = k->current.angle.y;
    s.have = true;
}

// Ein Testszenario setzt den Kurs selbst (Sweep-Start, Test-Teleport). Das ist kein fremder Schreiber: K18 nimmt den
// gesetzten Kurs als neue Basis (Lauf v1-gerade-1 zaehlte den Sweep-Kurs 11,25 Grad sonst als Fremdgier bei f=2).
void k18Rebase(const daKago_c* k) {
    if (g_k18.have) {
        g_k18.yawAfter = k->current.angle.y;
    }
}

void k18Summary(const char* why) {
    const K18State& s = g_k18;
    logInfo("flug: K18 SUMME (%s) schritte=%u fremdSchritte=%u fremdSumme=%.4f Grad maxFremd=%.4f Grad schritte0x600=%u "
            "fenster %u-%u: schritte=%u fremd=%u flugGier=%u",
        why, s.steps, s.foreignSteps, degOf(s.foreignSum), degOf(s.foreignMax), s.slideSteps, kK18WinFrom, kK18WinTo,
        s.winSteps, s.winForeign, s.winFlug);
}
#endif  // KFF_DEV_AUTOTEST (K18)

// Pre von daKago_c::executeFly. Laeuft vor checkHeight() und dem Modus-Switch, also vor dem Flugschritt.
HookAction onExecuteFlyPre(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (!isFlyingOwnBird(k)) {
            return HOOK_CONTINUE;
        }
        if (k->mMode != kFlyMode) {
            ++g_att.modeAway;
        }
        g_att.stickX = k->mStickX;
        g_att.stickY = stickYFrame(k);
        // Y im Landeanflug und im Schweben ohne Wirkung (E9, H1 7.5): der Vogel bleibt waagerecht.
        if (!session().landing && !session().hover && kff::act(kff::Act::BarrelRoll).pressed()) {  // Vorgabe Y (actions.cpp)
            startBarrel();
        }
    } catch (...) {
        logEvery(g_logErrors, 3, 300, "flug: executeFly pre ausgesetzt (Exception, Frame uebersprungen)");
    }
    return HOOK_CONTINUE;
}

// Pre von daKago_c::flySpeedCalc, HOOK_SKIP_ORIGINAL in Modus 2. Nick, Gier und Rolle dieses Flugschritts.
HookAction onFlySpeedCalcPre(ModContext*, void* args, void*, void*) {
    daKago_c* k = nullptr;
    try {
        k = argOf<daKago_c*>(args, 0);
        if (!isFlyingOwnBird(k) || k->mMode != kFlyMode) {
            return HOOK_CONTINUE;
        }
#if defined(KFF_DEV_AUTOTEST)
        const int k18Fremd = k18Foreign(k);  // Fremdgier seit dem letzten Flugschritt (K18)
#endif

        // E9: Landeanflug nach B (Session::landing). Der Vogel bleibt waagerecht: Nick, Rolle und Schraeglage gehen weich
        // auf 0, Stick Y und die Fassrolle wirken nicht. Die Gier bleibt beim Stick X.
        const bool landing = session().landing;
        // Schweben (Gang 0): der Vogel steht. Nick auf die Waagerechte, Rolle waagerecht, keine Fassrolle. Stick X dreht
        // wie in Gang 1 (rateFactor), die Schraege ist auf 30 Prozent begrenzt. Stick Y wirkt senkrecht (hoverVertical).
        const bool hover = session().hover && !landing;
        hoverAnimation(k, hover);

        // Die Fassrolle zaehlt nur in Schritten, die wirklich laufen. 64 Schritte = 360 Grad.
        float rollCmd = 0.0f;
        if (landing || hover) {
            g_att.barrelLeft = 0;  // im Landeanflug und im Schweben abgebrochen
        } else if (g_att.barrelLeft > 0) {
            rollCmd = static_cast<float>(g_att.barrelDir);
            --g_att.barrelLeft;
        }

        // Neutraler Stick Y, Landeanflug oder Schweben: Trimmung auf die Horizontale. Sonst Nick je Stick (+ = Nase unten).
        g_att.trimming = landing || hover || std::fabs(g_att.stickY) < kTrimDeadZone;
        if (g_att.trimming) {
            ++g_att.trimFrames;
        }
        // Nick- und Gierrate mit 2 * Rate (rateFactor): bei jedem Gang bleibt der Wendekreis gleich (Gang 3 = 1,0).
        const float rateScale = 2.0f * rateFactor();
        s16 dPitch = 0;
        if (landing) {
            dPitch = levelStep(k->current.angle.x, static_cast<s16>(kTrimRate));
        } else if (g_att.trimming) {
            dPitch = trimStep(k->current.angle.x);
        } else {
            dPitch = static_cast<s16>(g_att.stickY * kPitchRateMax * rateScale);
        }
        // Gier mit Totzone 0,02 auf dem Stick X (kreisflug.md 5.3b). Bei neutralem Stick bleibt die Gier exakt 0.
        const float stickXn = std::fabs(g_att.stickX) < kStickDeadZone ? 0.0f : g_att.stickX;
        const s16 dYaw = static_cast<s16>(-stickXn * kYawRateMax * rateScale);  // Stick links (X < 0) = Gier +
        const s16 dRoll = (landing || hover) ? levelStep(g_att.roll, static_cast<s16>(kRollRateMax))
                                             : static_cast<s16>(rollCmd * kRollRateMax);
        g_att.lastDYaw = dYaw;  // Messung: der Gierschritt, den flug selbst setzt (Rest ist Fremdgier)

        k->current.angle.x = static_cast<s16>(k->current.angle.x + dPitch);
        k->current.angle.y = static_cast<s16>(k->current.angle.y + dYaw);
        g_att.roll = static_cast<s16>(g_att.roll + dRoll);
        g_att.cumPitch += dPitch;
        g_att.cumYaw += dYaw;
        g_att.cumRoll += dRoll;

        if (rollCmd != 0.0f) {
            if (g_att.barrelLeft == kBarrelHalfFrames) {
                logInfo("flug: MARK rollmid-%u f=%u", g_att.barrels, session().rideFrames);
            } else if (g_att.barrelLeft == 0) {
                logInfo("flug: FASSROLLE %u ende, Rolle=%.1f Grad, cumRolle=%.1f Grad", g_att.barrels,
                    degOf(g_att.roll), degOf(g_att.cumRoll));
            }
        }

        // Sichtbare Schraeglage folgt der Gierrate, geglaettet wie die Vanilla-Schraege. Im Landeanflug waagerecht (E9).
        const float bankTarget = landing ? 0.0f
                                         : -static_cast<float>(dYaw) / (kYawRateMax * rateScale) * kBankMax
                                               * (hover ? kHoverBank : 1.0f);
        cLib_addCalcAngleS(&g_att.bank, static_cast<s16>(bankTarget), 8, 0x400, 0x10);

        k->shape_angle.x = k->current.angle.x;
        k->shape_angle.y = k->current.angle.y;
        k->shape_angle.z = static_cast<s16>(g_att.roll + g_att.bank);
        k->field_0x710 = 0;
        k->field_0x712 = 0;
#if defined(KFF_DEV_AUTOTEST)
        k18Record(k, k18Fremd, dYaw, stickXn);
#endif
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        logEvery(g_logErrors, 3, 300, "flug: flySpeedCalc pre ausgesetzt (Exception, kein Rueckfall auf Vanilla im Ritt)");
        // Im Ritt kein Rueckfall auf die Vanilla-Lenkung: sie kann Nick und Gier je Tick nachfuehren (kreisflug.md 5.3c).
        return (k != nullptr && isOwnBird(k) && isRiding()) ? HOOK_SKIP_ORIGINAL : HOOK_CONTINUE;
    }
}

// ---------------------------------------------------------------------------------------------
// ui_halt (WP3): Ein sichtbares Dusklight-Dokument (Menue, Dialog, Texteingabe) blockiert die Pad-Eingabe des Spiels
// (PADBlockInput), das Spiel laeuft aber weiter. Ohne Halt floege der Vogel ungesteuert mit Reisetempo weiter. Solange
// kff::input().uiDocument gilt, steht er: V = 0, kein Weg, keine Hoehenaenderung durch flug. Nach dem Schliessen laeuft V
// wie beim Ritt-Start mit kSpeedAccel von 0 an, ohne Sprung. Landeanflug und B-Wunsch ruhen im Kern (ride.cpp).
// ---------------------------------------------------------------------------------------------

struct UiHalt {
    bool active = false;
    uint32_t halts = 0;      // Halte in diesem Ritt
    uint32_t frames = 0;     // Frames im Halt in diesem Ritt
    uint32_t runFrames = 0;  // Frames des laufenden Halts
};
UiHalt g_uiHalt;

// true: der Vogel steht in diesem Frame (Geschwindigkeiten genullt).
bool uiHaltStep(daKago_c* k) {
    if (!input().uiDocument) {
        if (g_uiHalt.active) {
            g_uiHalt.active = false;
            logInfo("flug: ui_halt ende nach %u Frames, V laeuft von 0 an (f=%u)", g_uiHalt.runFrames,
                session().rideFrames);
        }
        return false;
    }
    if (!g_uiHalt.active) {
        g_uiHalt.active = true;
        g_uiHalt.runFrames = 0;
        ++g_uiHalt.halts;
        logInfo("flug: ui_halt start, Dusklight-Dokument sichtbar: Vogel steht (V war %.1f, Hoehe %.1f, f=%u)",
            static_cast<double>(g_att.speed), static_cast<double>(k->current.pos.y), session().rideFrames);
    }
    ++g_uiHalt.runFrames;
    ++g_uiHalt.frames;
    g_att.speed = 0.0f;
    g_att.hoverVy = 0.0f;  // Schweben: auch die senkrechte Rate steht
    g_hoverMeasured = false;  // die Messung des Schritts davor gilt nach dem Halt nicht mehr
    k->mFlySpeed = 0.0f;
    k->speedF = 0.0f;
    k->speed.y = 0.0f;
    return true;
}

// Post von daKago_c::executeFly. V laeuft auf das Hoechsttempo, der Gang skaliert nur den Weg je Tick.
void onExecuteFlyPost(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = argOf<daKago_c*>(args, 0);
        if (!isFlyingOwnBird(k) || k->mMode != kFlyMode) {
            return;
        }
        if (k->mIsWaterfall) {
            ++g_att.wfFrames;
        }
        if (uiHaltStep(k)) {
            return;
        }
        cLib_chaseF(&g_att.speed, topSpeed(), kSpeedAccel);
        const float weg = wegFactor();
        k->mFlySpeed = g_att.speed;
        k->speedF = g_att.speed * cM_scos(k->current.angle.x) * weg;
        k->speed.y = -g_att.speed * cM_ssin(k->current.angle.x) * weg;
        // Schweben: senkrechte Rate dazu. Im Schweben ist weg 0, also wirkt nur sie (Entwurf 4.4). Ohne Schweben klingt sie ab.
        hoverVertical(k);
        k->speed.y += g_att.hoverVy;
#if defined(KFF_DEV_AUTOTEST)
        logFlight(k);
#endif
    } catch (...) {
        logEvery(g_logErrors, 3, 300, "flug: executeFly post ausgesetzt (Exception)");
    }
}

// ---------------------------------------------------------------------------------------------
// Szenario "looping" (KFF_AUTOTEST=looping). Ablauf: Ruf (R halten, B tippen), dann der Ritt nach Zeitplan:
// steigen, gleiten (Trimmung), zwei Loopings, Ebene (Trimmung), zwei Fassrollen, 720 Grad Dauerkurve, Schwanz, Absteigen per B,
// danach 600 Frames Nachlauf ohne Eingabe (absturz-mppath1: kein SIGSEGV nach dem B-Absteigen).
// Der Stick laeuft ueber den Pad-Pfad des Kerns (Context::stick). Der Y-Tipp der Fassrolle laeuft ueber den
// Kern-Kontext (Context::tap), startBarrel() liest die Flanke ueber kff::input().
// Marken "flug: MARK <name>" fuer Screenshots, "flug: SZENARIO looping ..." als Abschluss mit Ergebnis.
// ---------------------------------------------------------------------------------------------

#if defined(KFF_DEV_AUTOTEST)
constexpr uint32_t kCallAt = 300;     // erster Ruf, sobald der Link ein Wolf ist und frei steht
constexpr uint32_t kHoldR = 30;       // R gehalten (Ticks)
constexpr uint32_t kTapBAt = 15;      // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kRideWait = 900;   // der Ritt muss innerhalb dieser Ticks nach dem Ruf beginnen
constexpr uint32_t kNachlauf = 600;   // nach dem B-Absteigen laeuft das Spiel so viele Frames weiter (absturz-mppath1)
constexpr uint32_t kGiveUpAt = 1500;  // ohne moeglichen Ruf endet das Szenario

enum class Mark : uint8_t { None, Apex, YawHalf };

struct ScheduleSeg {
    const char* name;
    uint32_t frames;
    float stickX;
    float stickY;
    bool barrel;  // Y-Tipp zu Beginn des Segments
    Mark mark;
    const char* label;
};

// Zeitplan, Index = Ritt-Frame. Neutrale Segmente lassen die Trimmung den Nick auf die Horizontale bringen.
constexpr ScheduleSeg kSchedule[] = {
    {"steigen", 20, 0.0f, -0.5f, false, Mark::None, nullptr},
    {"gleiten", 40, 0.0f, 0.0f, false, Mark::None, nullptr},
    {"loopA", 103, 0.0f, -1.0f, false, Mark::Apex, "loopA-top"},
    {"loopB", 103, 0.0f, -1.0f, false, Mark::Apex, "loopB-top"},
    {"ebene", 20, 0.0f, 0.0f, false, Mark::None, nullptr},
    {"rollA", 64, 0.0f, 0.0f, true, Mark::None, nullptr},
    {"rollB", 64, 0.0f, 0.0f, true, Mark::None, nullptr},
    {"kurve720", 256, -1.0f, 0.0f, false, Mark::YawHalf, "kurve-360"},
    {"schwanz", 40, 0.0f, 0.0f, false, Mark::None, nullptr},
};

constexpr uint32_t scheduleFrames() {
    uint32_t total = 0;
    for (const ScheduleSeg& seg : kSchedule) {
        total += seg.frames;
    }
    return total;
}
constexpr uint32_t kScheduleFrames = scheduleFrames();

struct ScenarioState {
    bool armed = false;
    uint32_t callStart = 0;
    bool rideSeen = false;
    bool dismounted = false;
    uint32_t dismountTick = 0;
    bool birdGone = false;  // der Vogel ist nach dem Absteigen weg (nur Information, das Ende haengt am Nachlauf)
    int seg = -1;       // Index im Zeitplan, -1 = noch keiner
    long long segPitch = 0;
    long long segYaw = 0;
    long long segRoll = 0;
    long long segBand = 0;
    bool segMarked = false;
    bool seen312 = false;
    uint32_t first312 = 0;
    uint32_t ok312 = 0;
    uint32_t bad312 = 0;
    uint32_t firstBad312 = 0;
    uint32_t apexMarks = 0;  // erreichte Schleifenscheitel
};
ScenarioState g_scn;

// nachlauf = Frames nach dem B-Absteigen, die ohne Absturz gelaufen sind (0, wenn kein Absteigen stattfand).
void scenarioSummary(const char* why, uint32_t nachlauf) {
    const ScenarioState& s = g_scn;
    const bool regel = s.apexMarks == 2 && g_att.barrels == 2 && std::fabs(degOf(g_att.cumYaw) - 720.0) < 2.0 &&
                       s.seen312 && s.bad312 == 0 && autotest::foreignInputCount() == 0 && nachlauf >= kNachlauf;
    logInfo("flug: SZENARIO looping %s proc312 gesehen=%d seit f=%u ok=%u bruch=%u (erster bei f=%u) "
            "cumNick=%.1f cumGier=%.1f cumRolle=%.1f fassrollen=%u schleifen=%u modusAbweichend=%u wasserfall=%u "
            "trimFrames=%u fremdeingabe=%u nachlauf=%u -> %s",
        why, s.seen312 ? 1 : 0, s.first312, s.ok312, s.bad312, s.firstBad312, degOf(g_att.cumPitch),
        degOf(g_att.cumYaw), degOf(g_att.cumRoll), g_att.barrels, s.apexMarks, g_att.modeAway, g_att.wfFrames,
        g_att.trimFrames, autotest::foreignInputCount(), nachlauf, regel ? "BESTANDEN" : "NICHT BESTANDEN");
}

// Ritt nach Zeitplan. Jeder Tick mit Ritt: Link-Proc pruefen, Segment waehlen, Stick setzen, Marken, Absteigen am Ende.
void playTimeline(autotest::Context& c) {
    ScenarioState& s = g_scn;
    const uint32_t f = c.sess.rideFrames;  // Index des kommenden Ritt-Frames

    if (daAlink_c* link = liveLink()) {
        if (link->mProcID == daAlink_c::PROC_WOLF_CARGO_CARRY) {
            if (!s.seen312) {
                s.seen312 = true;
                s.first312 = f;
                logInfo("flug: Link in PROC_WOLF_CARGO_CARRY (312) ab f=%u", f);
            }
            ++s.ok312;
        } else if (s.seen312) {
            if (s.bad312 == 0) {
                s.firstBad312 = f;
                logInfo("flug: Link verlaesst Proc 312 bei f=%u (proc=%u)", f, static_cast<unsigned>(link->mProcID));
            }
            ++s.bad312;
        }
    }

    if (f >= kScheduleFrames) {
        c.tapB();
        s.dismounted = true;
        s.dismountTick = c.frame;
        c.note("B-Tipp: Absteigen am Ende des Zeitplans");
        logInfo("flug: ZEITPLAN fertig f=%u, Absteigen per B, danach %u Frames Nachlauf", f, kNachlauf);
        return;
    }

    uint32_t start = 0;
    uint32_t idx = 0;
    while (f >= start + kSchedule[idx].frames) {  // f < kScheduleFrames, die Suche endet im Plan
        start += kSchedule[idx].frames;
        ++idx;
    }

    if (static_cast<int>(idx) != s.seg) {
        if (s.seg >= 0) {
            const ScheduleSeg& prev = kSchedule[s.seg];
            logInfo("flug: SEG %s END f=%u dNick=%.1f dGier=%.1f dRolle=%.1f", prev.name, f,
                degOf(g_att.cumPitch - s.segPitch), degOf(g_att.cumYaw - s.segYaw), degOf(g_att.cumRoll - s.segRoll));
        }
        s.seg = static_cast<int>(idx);
        s.segPitch = g_att.cumPitch;
        s.segYaw = g_att.cumYaw;
        s.segRoll = g_att.cumRoll;
        s.segBand = apexBand(g_att.cumPitch);
        s.segMarked = false;
        logInfo("flug: SEG %s BEGIN f=%u", kSchedule[idx].name, f);
        if (kSchedule[idx].barrel) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));
            c.note("Y-Tipp fuer die Fassrolle");
        }
    }

    const ScheduleSeg& seg = kSchedule[idx];
    c.stick(seg.stickX, seg.stickY);

    if (!s.segMarked && seg.mark != Mark::None) {
        const bool hit = seg.mark == Mark::Apex ? apexBand(g_att.cumPitch) != s.segBand
                                                : (g_att.cumYaw - s.segYaw) >= 65536LL;
        if (hit) {
            s.segMarked = true;
            if (seg.mark == Mark::Apex) {
                ++s.apexMarks;
            }
            logInfo("flug: MARK %s f=%u cumNick=%.1f cumGier=%.1f cumRolle=%.1f", seg.label, f,
                degOf(g_att.cumPitch), degOf(g_att.cumYaw), degOf(g_att.cumRoll));
        }
    }
}

bool loopingTick(autotest::Context& c) {
    ScenarioState& s = g_scn;
    const uint32_t t = c.frame;

    if (!s.armed) {
        if (t >= kCallAt && c.canCall()) {
            s.armed = true;
            s.callStart = t;
            c.note("Ruf armiert (R halten, B tippen)");
        } else if (t >= kGiveUpAt) {
            c.note("kein Ruf moeglich, Szenario endet");
            return true;
        }
        return false;
    }

    if (!s.rideSeen && t >= s.callStart && t < s.callStart + kHoldR) {
        c.holdR();
    }
    if (!s.rideSeen && t == s.callStart + kTapBAt) {
        c.tapB();
        c.note("B-Tipp im R-Halt (Ruf)");
    }

    if (s.dismounted) {
        if (!s.birdGone && c.sess.birdId == kNoProc) {
            s.birdGone = true;
            c.note("Vogel nach dem Absteigen entfernt");
        }
        if (t >= s.dismountTick + kNachlauf) {
            c.note("Nachlauf ohne Absturz, Szenario endet");
            scenarioSummary("Zeitplan fertig", t - s.dismountTick);
            return true;
        }
        return false;
    }

    if (c.sess.phase == Phase::Riding) {
        if (!s.rideSeen) {
            s.rideSeen = true;
            c.note("Ritt erkannt, Zeitplan startet");
        }
        playTimeline(c);
        return false;
    }

    if (s.rideSeen) {
        scenarioSummary("Ritt vor dem Zeitplan beendet", 0);
        c.note("Ritt endete vor dem Zeitplan, Szenario endet");
        return true;
    }
    if (t >= s.callStart + kRideWait) {
        c.note("Ritt beginnt nicht, Szenario endet");
        return true;
    }
    return false;
}

// Die Ruf-Logik ist gemeinsam; der Zeitplan oben liefert nur den Ritt.
bool loopingRun(autotest::Context& c) {
    ++g_padFrames;
    const bool done = loopingTick(c);
    if (done) {
        g_testFinished = true;
    }
    return done;
}

const autotest::Scenario kLoopingScenario = {
    "looping",
    "Ruf, Steigen, zwei Loopings, Ebene, zwei Fassrollen, 720 Grad Kurve, Schwanz, Absteigen",
    loopingRun,
};

// ---------------------------------------------------------------------------------------------
// Szenario "geradeaus" (KFF_AUTOTEST=geradeaus). Ruf wie bei looping, dann im Ritt:
//   Phase A  1800 Frames (60 s bei 30 Ticks je Sekunde), Stick neutral. Erwartung: Hoehe aendert sich um weniger als 50 Einheiten, der
//            Kurs um weniger als 2 Grad. Die Trimmung haelt den Nick auf der Horizontalen.
//   Phase B  900 Frames (30 s), Stick X voll rechts. Erwartung: sauberer Dauerkreis. Jeder Frame dreht um genau
//            512 Einheiten, der Nick bleibt auf null, die Hoehe konstant.
//   Ende     B zum Absteigen, danach 600 Frames Nachlauf ohne Eingabe (absturz-mppath1). Dann endet das Szenario.
//   geradeaus2 wie geradeaus, aber Startkurs 11,25 Grad (kGeraVersatzYaw). Die Wandausweichung von freiheit
//            greift bei Kurs 0 nach rund 5 s an einer Wand; der Versuch zeigt, ob der Kurs danach allein vom
//            Stick abhaengt. Fremde Gierschritte (freiheit) werden je Frame gemessen und getrennt ausgewiesen.
// Marken: "flug: MARK geradeaus-30s" (Mitte Phase A) und "flug: MARK kreis-15s" (Mitte Phase B).
// Ergebnis: "flug: SZENARIO geradeaus ..." mit Messwerten, fremdeingabe und BESTANDEN oder NICHT BESTANDEN.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kGeraPhaseA = 1800;               // 60 s neutraler Stick (30 Ticks je Sekunde, game_clock.h kSimPeriod)
constexpr uint32_t kGeraPhaseB = 900;                // 30 s Vollausschlag rechts
constexpr uint32_t kGeraEnd = kGeraPhaseA + kGeraPhaseB;
constexpr uint32_t kGeraMarkA = 900;                 // Mitte von Phase A
constexpr uint32_t kGeraMarkB = kGeraPhaseA + 450;   // Mitte von Phase B
constexpr float kGeraHeightTol = 50.0f;              // Einheiten
constexpr double kGeraCourseTolDeg = 2.0;            // Grad
constexpr double kGeraNickTolDeg = 1.0;              // Grad
constexpr int16_t kGeraYawStep = -512;               // Vollausschlag rechts: dYaw = -stickX * kYawRateMax
constexpr int16_t kGeraVersatzYaw = 0x0800;          // Startkurs von geradeaus2: 11,25 Grad, umgeht die Wandausweichung bei Kurs 0

struct GeraState {
    bool armed = false;
    uint32_t callStart = 0;
    bool rideSeen = false;
    bool havePos = false;
    float y0 = 0.0f;           // Hoehe am Start des Ritts
    int16_t yaw0 = 0;          // Kurs am Start des Ritts
    int16_t yawPrev = 0;       // Kurs im vorherigen Tick
    float dyMaxA = 0.0f;       // groesste Hoehenabweichung in Phase A, Einheiten
    double courseMaxA = 0.0;   // groesste Kursabweichung in Phase A, Grad
    double nickMaxA = 0.0;     // groesster Nick in Phase A, Grad
    int64_t yawSumA = 0;       // Summe der Gierschritte in Phase A, s16-Einheiten
    int64_t yawSumB = 0;       // Summe der Gierschritte in Phase B, s16-Einheiten
    int64_t flugGierA = 0;     // davon in Phase A von flug gesetzt (muss null sein, Stick X ist neutral)
    int64_t fremdGierA = 0;    // davon in Phase A von anderen Modulen gesetzt (freiheit: Wandausweichen)
    uint32_t fremdFramesA = 0; // Frames in Phase A mit solchem Fremdschritt
    int16_t startYaw = 0;      // Startkurs dieses Versuchs, 0 = unveraendert
    const char* name = "geradeaus";  // Name des Versuchs fuer die Ergebniszeile
    bool birdGone = false;     // der Vogel ist nach dem Absteigen weg (nur Information)
    uint32_t unsauberB = 0;    // Frames in Phase B mit anderem Gierschritt als kGeraYawStep
    bool yBSeen = false;
    float yMinB = 0.0f;
    float yMaxB = 0.0f;
    double nickMaxB = 0.0;     // groesster Nick in Phase B, Grad
    bool okA = false;
    bool okB = false;
    bool dismounted = false;
    uint32_t dismountTick = 0;
};
GeraState g_gera;

// nachlauf = Frames nach dem B-Absteigen ohne Absturz (0, wenn der Vogel nicht abstieg).
void scenarioGeraFinal(const char* why, uint32_t nachlauf) {
    const GeraState& s = g_gera;
    const bool gesamt = s.okA && s.okB && autotest::foreignInputCount() == 0 && nachlauf >= kNachlauf;
    logInfo("flug: SZENARIO %s %s A=%s B=%s fremdeingabe=%u testFrames=%u trimFrames=%u nachlauf=%u startKurs=%.2f "
            "gierSummeA=%.2f gierSummeB=%.1f -> %s",
        s.name, why, s.okA ? "ok" : "fail", s.okB ? "ok" : "fail", autotest::foreignInputCount(), g_padFrames, g_att.trimFrames, nachlauf,
        degOf(s.startYaw), degOf(s.yawSumA), degOf(s.yawSumB), gesamt ? "BESTANDEN" : "NICHT BESTANDEN");
}

// Messung und Eingabe je Tick im Ritt. Die Lage stammt vom vorherigen Frame, weil der Kern die Eingabe erst im
// folgenden Frame liest. Der Gierschritt eines Ticks ist also der Schritt des Frames, dessen Stick im vorherigen
// Tick stand.
bool geraTimeline(autotest::Context& c) {
    GeraState& s = g_gera;
    const uint32_t f = c.sess.rideFrames;  // Index des kommenden Ritt-Frames
    daKago_c* k = liveBird();
    if (k == nullptr) {
        c.note("Vogel fehlt im Ritt, Geradeaus-Test endet");
        scenarioGeraFinal("Vogel fehlt", 0);
        return true;
    }
    if (!s.havePos && s.startYaw != 0) {
        k->current.angle.y = s.startYaw;  // Startkurs vor der ersten Messung (nur geradeaus2)
        k18Rebase(k);
    }
    const float y = k->current.pos.y;
    const int16_t yaw = k->current.angle.y;
    const double nick = std::fabs(degOf(k->current.angle.x));
    if (!s.havePos) {
        s.havePos = true;
        s.y0 = y;
        s.yaw0 = yaw;
        s.yawPrev = yaw;
        logInfo("flug: GERADE Start f=%u y=%.2f kurs=%.2f nick=%.3f", f, static_cast<double>(y), degOf(yaw), nick);
    }
    if (f > 0) {
        const int16_t dYaw = static_cast<int16_t>(yaw - s.yawPrev);
        // Der Schritt, den flug selbst gesetzt hat, steht in g_att.lastDYaw. Der Rest kommt von anderen Modulen.
        const int16_t fremd = static_cast<int16_t>(dYaw - g_att.lastDYaw);
        if (f <= kGeraPhaseA) {
            s.yawSumA += dYaw;
            s.flugGierA += g_att.lastDYaw;
            s.fremdGierA += fremd;
            if (fremd != 0) {
                if (s.fremdFramesA < 20) {
                    logInfo("flug: FREMDGIER f=%u Schritt=%.3f Grad (nicht von flug), Kurs danach %.2f Grad", f,
                        degOf(fremd), degOf(yaw));
                }
                ++s.fremdFramesA;
            }
            s.dyMaxA = std::max(s.dyMaxA, std::fabs(y - s.y0));
            s.courseMaxA = std::max(s.courseMaxA, std::fabs(degOf(static_cast<int16_t>(yaw - s.yaw0))));
            s.nickMaxA = std::max(s.nickMaxA, nick);
        } else if (f <= kGeraEnd) {
            s.yawSumB += dYaw;
            if (dYaw != kGeraYawStep) {
                ++s.unsauberB;
            }
            if (!s.yBSeen) {
                s.yBSeen = true;
                s.yMinB = y;
                s.yMaxB = y;
            }
            s.yMinB = std::min(s.yMinB, y);
            s.yMaxB = std::max(s.yMaxB, y);
            s.nickMaxB = std::max(s.nickMaxB, nick);
        }
    }
    s.yawPrev = yaw;

    if (f > 0 && f % 30 == 0) {  // eine Zeile je Sekunde (30 Ticks)
        logInfo("flug: GERADE t=%u s f=%u %s y=%.2f dy=%.2f kursRel=%.3f nick=%.3f rolle=%.2f schraeg=%.2f V=%.2f "
                "trim=%d gierA=%.1f gierB=%.1f fremdA=%.2f",
            f / 30, f, f <= kGeraPhaseA ? "A-neutral" : "B-rechts", static_cast<double>(y),
            static_cast<double>(y - s.y0), degOf(static_cast<int16_t>(yaw - s.yaw0)), nick, degOf(g_att.roll),
            degOf(g_att.bank), static_cast<double>(g_att.speed), g_att.trimming ? 1 : 0, degOf(s.yawSumA),
            degOf(s.yawSumB), degOf(s.fremdGierA));
    }
    if (f == kGeraMarkA) {
        logInfo("flug: MARK geradeaus-30s f=%u y=%.2f dy=%.3f kursRel=%.3f nick=%.3f V=%.2f", f,
            static_cast<double>(y), static_cast<double>(y - s.y0), degOf(static_cast<int16_t>(yaw - s.yaw0)), nick,
            static_cast<double>(g_att.speed));
    }
    if (f == kGeraMarkB) {
        logInfo("flug: MARK kreis-15s f=%u y=%.2f nick=%.3f gierSummeB=%.1f", f, static_cast<double>(y), nick,
            degOf(s.yawSumB));
    }

    if (f == kGeraPhaseA) {
        s.okA = s.dyMaxA < kGeraHeightTol && s.courseMaxA < kGeraCourseTolDeg;
        logInfo("flug: GERADE A (60 s neutral): dHoehe=%.2f (Grenze 50) dKurs=%.3f Grad (Grenze 2) nickMax=%.3f Grad "
                "gierSumme=%.2f Grad (flug %.2f, fremd %.2f Grad in %u Frames) -> %s",
            static_cast<double>(s.dyMaxA), s.courseMaxA, s.nickMaxA, degOf(s.yawSumA), degOf(s.flugGierA),
            degOf(s.fremdGierA), s.fremdFramesA, s.okA ? "ok" : "fehlgeschlagen");
    }

    if (f < kGeraPhaseA) {
        c.stick(0.0f, 0.0f);
    } else if (f < kGeraEnd) {
        c.stick(1.0f, 0.0f);
    }

    if (f == kGeraEnd) {
        const double erwartet = degOf(static_cast<long long>(kGeraPhaseB) * kGeraYawStep);
        const double gedreht = degOf(s.yawSumB);
        const double hoehe = static_cast<double>(s.yMaxB - s.yMinB);
        s.okB = s.unsauberB == 0 && std::fabs(gedreht - erwartet) < 2.0 && s.nickMaxB < kGeraNickTolDeg &&
                hoehe < static_cast<double>(kGeraHeightTol);
        logInfo("flug: GERADE B (30 s rechts): gedreht=%.1f Grad (erwartet %.1f) unsauber=%u hoehenBereich=%.3f "
                "nickMax=%.3f Grad -> %s",
            gedreht, erwartet, s.unsauberB, hoehe, s.nickMaxB, s.okB ? "ok" : "fehlgeschlagen");
        c.tapB();
        s.dismounted = true;
        s.dismountTick = c.frame;
        c.note("B-Tipp: Absteigen am Ende des Geradeaus-Tests");
        return false;
    }
    return false;
}

bool geradeausTick(autotest::Context& c) {
    GeraState& s = g_gera;
    const uint32_t t = c.frame;

    if (!s.armed) {
        if (t >= kCallAt && c.canCall()) {
            s.armed = true;
            s.callStart = t;
            c.note("Ruf armiert (R halten, B tippen)");
        } else if (t >= kGiveUpAt) {
            c.note("kein Ruf moeglich, Szenario endet");
            scenarioGeraFinal("kein Ruf", 0);
            return true;
        }
        return false;
    }

    if (!s.rideSeen && t >= s.callStart && t < s.callStart + kHoldR) {
        c.holdR();
    }
    if (!s.rideSeen && t == s.callStart + kTapBAt) {
        c.tapB();
        c.note("B-Tipp im R-Halt (Ruf)");
    }

    if (s.dismounted) {
        if (!s.birdGone && c.sess.birdId == kNoProc) {
            s.birdGone = true;
            c.note("Vogel nach dem Absteigen entfernt");
        }
        if (t >= s.dismountTick + kNachlauf) {
            c.note("Nachlauf ohne Absturz, Geradeaus-Test endet");
            scenarioGeraFinal("Ende", t - s.dismountTick);
            return true;
        }
        return false;
    }

    if (c.sess.phase == Phase::Riding) {
        if (!s.rideSeen) {
            s.rideSeen = true;
            c.note("Ritt erkannt, Geradeaus-Test startet");
        }
        return geraTimeline(c);
    }

    if (s.rideSeen) {
        c.note("Ritt endete vor dem Ende des Geradeaus-Tests, Szenario endet");
        scenarioGeraFinal("Ritt vorzeitig beendet", 0);
        return true;
    }
    if (t >= s.callStart + kRideWait) {
        c.note("Ritt beginnt nicht, Szenario endet");
        scenarioGeraFinal("kein Ritt", 0);
        return true;
    }
    return false;
}

bool geradeausRun(autotest::Context& c) {
    ++g_padFrames;
    const bool done = geradeausTick(c);
    if (done) {
        g_testFinished = true;
    }
    return done;
}

bool geradeausRunStrict(autotest::Context& c) {
    g_gera.startYaw = 0;
    g_gera.name = "geradeaus";
    return geradeausRun(c);
}

bool geradeausRunVersatz(autotest::Context& c) {
    g_gera.startYaw = kGeraVersatzYaw;
    g_gera.name = "geradeaus2";
    return geradeausRun(c);
}

const autotest::Scenario kGeradeausScenario = {
    "geradeaus",
    "Ruf, 60 s neutraler Stick (Hoehe und Kurs halten), 30 s Vollausschlag rechts (Dauerkreis), Absteigen, Nachlauf",
    geradeausRunStrict,
};

const autotest::Scenario kGeradeaus2Scenario = {
    "geradeaus2",
    "Wie geradeaus, Startkurs 11,25 Grad: umgeht die Wandausweichung von freiheit bei Kurs 0",
    geradeausRunVersatz,
};

// ---------------------------------------------------------------------------------------------
// WP8 (Haertung Eingabe, Flugsteuerung, Trimm, Tempo): Szenarien geradeaus-b, trimm und extrem. Gemeinsam:
//   Ablauf   Ruf wie looping (R halten, B tippen), Ritt nach Zeitplan, B (Landeanflug), Nachlauf kWp8Nachlauf Ticks.
//   Start    OE7: Startpunkt mit Sweep-Beleg. Ab der natuerlichen Ritt-Startposition prueft ein Liniencheck
//            (dBgS_LinChk, Versatzpunkte um den Vogel und 300 darunter fuer Link) den Weg des Szenarios. Ist er nicht
//            frei, folgen hoehere Startpunkte (bis +12000) und andere Kurse (je 45 Grad). Der Vogel wird dorthin gesetzt
//            (Test-Teleport wie e9-klippe: current und old). Sichtbar sind nur geladene Raeume.
//   Ende     Liegt der Vogel mehr als kWp8LowerAbove ueber dem Boden, setzt der Test ihn vor dem B auf kWp8LowerTo ueber den
//            Boden, damit der Landeanflug (hoechstens 240 Ticks) Link nicht hoch in der Luft freigibt.
// Ergebnis je Szenario: "flug: SZENARIO <name> ... -> BESTANDEN" oder "NICHT BESTANDEN". Fremdgier misst K18.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kWp8Nachlauf = 300;      // Ticks nach dem B bis zum Szenario-Ende
constexpr float kWp8LowerAbove = 3000.0f;   // vor dem B: hoeher ueber dem Boden, dann absenken
constexpr float kWp8LowerTo = 1200.0f;      // vor dem B: Hoehe ueber dem Boden nach dem Absenken
constexpr float kSweepSeg = 2000.0f;        // Laenge eines Linienchecks
constexpr int kSweepCircleN = 48;           // Ecken je Kreis
constexpr int kSweepOffN = 8;               // Versatzpunkte: Mitte, vier Seiten, oben, unten, Link unter dem Vogel
constexpr float kSweepOffX[kSweepOffN] = {0.0f, 100.0f, -100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kSweepOffY[kSweepOffN] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 80.0f, -80.0f, -300.0f};
constexpr float kSweepOffZ[kSweepOffN] = {0.0f, 0.0f, 0.0f, 100.0f, -100.0f, 0.0f, 0.0f, 0.0f};
constexpr float kSweepLifts[] = {0.0f, 500.0f, 1000.0f, 1500.0f, 2000.0f, 3000.0f, 4000.0f, 6000.0f, 8000.0f,
    10000.0f, 12000.0f};
constexpr float kWp8Pi = 3.14159265f;

// Ein Liniencheck mit allen Versatzpunkten. true = frei.
bool sweepSeg(const cXyz& a, const cXyz& b) {
    for (int i = 0; i < kSweepOffN; ++i) {
        cXyz from(a.x + kSweepOffX[i], a.y + kSweepOffY[i], a.z + kSweepOffZ[i]);
        cXyz to(b.x + kSweepOffX[i], b.y + kSweepOffY[i], b.z + kSweepOffZ[i]);
        dBgS_LinChk lin;
        lin.Set(&from, &to, nullptr);
        if (dComIfG_Bgsp().LineCross(&lin)) {
            return false;
        }
    }
    return true;
}

// Freie Strecke geradeaus ab p mit Kurs yaw, hoechstens len.
float sweepStraight(const cXyz& p, s16 yaw, float len) {
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    for (float d = 0.0f; d < len; d += kSweepSeg) {
        const float e = std::min(d + kSweepSeg, len);
        if (!sweepSeg(cXyz(p.x + hx * d, p.y, p.z + hz * d), cXyz(p.x + hx * e, p.y, p.z + hz * e))) {
            return d;
        }
    }
    return len;
}

// Rechtskreis (Stick X rechts, die Gier faellt) ab p mit Kurs yaw und Radius r.
bool sweepCircleRight(const cXyz& p, s16 yaw, float r) {
    const s16 toCenter = static_cast<s16>(yaw - 0x4000);
    const float cx = p.x + cM_ssin(toCenter) * r;
    const float cz = p.z + cM_scos(toCenter) * r;
    const int a0 = static_cast<int>(yaw) + 0x4000;  // Richtung Mittelpunkt -> p
    cXyz prev = p;
    for (int i = 1; i <= kSweepCircleN; ++i) {
        const s16 a = static_cast<s16>(a0 - i * (65536 / kSweepCircleN));
        const cXyz q(cx + cM_ssin(a) * r, p.y, cz + cM_scos(a) * r);
        if (!sweepSeg(prev, q)) {
            return false;
        }
        prev = q;
    }
    return true;
}

// Senkrechte Schleife (Nase hoch) ab p mit Kurs yaw und Radius r, in der Ebene von Kurs und Hochachse.
bool sweepLoopUp(const cXyz& p, s16 yaw, float r) {
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    cXyz prev = p;
    for (int i = 1; i <= kSweepCircleN; ++i) {
        const float phi = 2.0f * kWp8Pi * static_cast<float>(i) / static_cast<float>(kSweepCircleN);
        const float fwd = r * std::sin(phi);
        const float up = r * (1.0f - std::cos(phi));
        const cXyz q(p.x + hx * fwd, p.y + up, p.z + hz * fwd);
        if (!sweepSeg(prev, q)) {
            return false;
        }
        prev = q;
    }
    return true;
}

using SweepCheck = bool (*)(const cXyz& p, s16 yaw);

struct Wp8Start {
    bool ok = false;
    float lift = 0.0f;
    s16 yaw = 0;
    cXyz pos{};
};

// OE7: erster freier Start ab der Vogelposition. Reihenfolge: Kurs kurs0 mit steigender Hoehe, dann die Kurse je 45 Grad.
Wp8Start wp8FindStart(const daKago_c* k, s16 kurs0, SweepCheck check, const char* tag) {
    Wp8Start r;
    const cXyz base = k->current.pos;
    for (int d = 0; d < 8; ++d) {
        const s16 yaw = static_cast<s16>(kurs0 + d * 0x2000);
        for (const float lift : kSweepLifts) {
            const cXyz p(base.x, base.y + lift, base.z);
            if (lift > 0.0f && !sweepSeg(base, p)) {
                continue;  // der Weg nach oben ist nicht frei
            }
            if (check(p, yaw)) {
                r.ok = true;
                r.lift = lift;
                r.yaw = yaw;
                r.pos = p;
                logInfo("flug: SWEEP %s frei: Start (%.1f, %.1f, %.1f) Kurs %d (%.2f Grad), Hoehe +%.0f, Versuch %d", tag,
                    static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z), static_cast<int>(yaw),
                    degOf(yaw), static_cast<double>(lift), d);
                return r;
            }
        }
        logInfo("flug: SWEEP %s Kurs %d (%.2f Grad): kein freier Start bis +%.0f", tag, static_cast<int>(yaw), degOf(yaw),
            static_cast<double>(kSweepLifts[sizeof(kSweepLifts) / sizeof(kSweepLifts[0]) - 1]));
    }
    logInfo("flug: SWEEP %s ohne freien Start, der Test laeuft vom natuerlichen Startpunkt", tag);
    return r;
}

// Vogel auf den gefundenen Start setzen (Test-Teleport wie e9-klippe).
void wp8Place(daKago_c* k, const Wp8Start& st) {
    if (!st.ok) {
        return;
    }
    k->current.pos = st.pos;
    k->old.pos = st.pos;
    k->current.angle.y = st.yaw;
    k->shape_angle.y = st.yaw;
    k18Rebase(k);
}

// Vor dem B: hoch ueber dem Boden auf kWp8LowerTo setzen. Liefert die Hoehe ueber dem Boden vor dem Absenken (-1 ohne Boden).
float wp8LowerForLanding(daKago_c* k) {
    dBgS_GndChk chk;
    cXyz probe(k->current.pos.x, k->current.pos.y + 500.0f, k->current.pos.z);
    chk.SetPos(&probe);
    const float gy = dComIfG_Bgsp().GroundCross(&chk);
    if (gy <= -1.0e5f) {
        return -1.0f;
    }
    const float above = k->current.pos.y - gy;
    if (above > kWp8LowerAbove) {
        const cXyz to(k->current.pos.x, gy + kWp8LowerTo, k->current.pos.z);
        k->current.pos = to;
        k->old.pos = to;
        logInfo("flug: WP8 Vogel vor dem B von %.0f auf %.0f ueber den Boden gesetzt", static_cast<double>(above),
            static_cast<double>(kWp8LowerTo));
    }
    return above;
}

int wp8CountBirdCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e && static_cast<daKago_c*>(ac)->mType == kFriendlyKagoType) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

// Eigene Voegel (mType 1) im Spiel, unabhaengig von der Sitzung.
uint32_t wp8CountBirds() {
    uint32_t n = 0;
    fopAcIt_Executor(wp8CountBirdCb, &n);
    return n;
}

// Gemeinsamer Ablauf: Ruf, Ritt (ride liefert true, wenn der Zeitplan fertig ist: dann B), Nachlauf, Ergebnis (end).
struct Wp8Run {
    bool armed = false;
    uint32_t callStart = 0;
    bool rideSeen = false;
    bool dismounted = false;
    uint32_t dismountTick = 0;
};

using Wp8RideFn = bool (*)(autotest::Context& c, daKago_c* k);
using Wp8EndFn = void (*)(const char* why, uint32_t nachlauf);

bool wp8Drive(autotest::Context& c, Wp8Run& r, Wp8RideFn ride, Wp8EndFn end) {
    const uint32_t t = c.frame;
    if (!r.armed) {
        if (t >= kCallAt && c.canCall()) {
            r.armed = true;
            r.callStart = t;
            c.note("wp8: Ruf armiert (R halten, B tippen)");
        } else if (t >= kGiveUpAt) {
            end("kein Ruf", 0);
            return true;
        }
        return false;
    }
    if (!r.rideSeen && t >= r.callStart && t < r.callStart + kHoldR) {
        c.holdR();
    }
    if (!r.rideSeen && t == r.callStart + kTapBAt) {
        c.tapB();
        c.note("wp8: B-Tipp im R-Halt (Ruf)");
    }
    if (r.dismounted) {
        if (t >= r.dismountTick + kWp8Nachlauf) {
            end("Ende", t - r.dismountTick);
            return true;
        }
        return false;
    }
    if (c.sess.phase == Phase::Riding) {
        if (!r.rideSeen) {
            r.rideSeen = true;
            c.note("wp8: Ritt erkannt, Zeitplan startet");
        }
        daKago_c* k = liveBird();
        if (k == nullptr) {
            end("Vogel fehlt im Ritt", 0);
            return true;
        }
        if (ride(c, k)) {
            c.tapB();
            r.dismounted = true;
            r.dismountTick = t;
            c.note("wp8: B-Tipp, Absteigen (Landeanflug)");
        }
        return false;
    }
    if (r.rideSeen) {
        end("Ritt vorzeitig beendet", 0);
        return true;
    }
    if (t >= r.callStart + kRideWait) {
        end("kein Ritt", 0);
        return true;
    }
    return false;
}

// ---- geradeaus-b (WP8.1 und WP8.4) -------------------------------------------------------------
//   Start    Kurs 2048 (11,25 Grad) an der natuerlichen Ritt-Startposition, per Sweep belegt (Phase A gerade, Phase B Kreis).
//   Phase A  1800 Ticks Stick neutral, Gang 3. Pass: |dy| < 50, Kursabweichung < 2 Grad, flug-Gier 0,00, Fremdgier 0,
//            Voegel = 1 (gezaehlt bei f 1, 900, 1800).
//   Wechsel  Steuerkreuz oben bei f 1800 und 1806 (Gang 5), neutral bis f 1840.
//   Phase B  900 Ticks Stick X voll rechts, Gang 5 (1024 Einheiten = 5,625 Grad je Tick). Pass: jeder Umlauf (64 Ticks)
//            360 Grad +-5, Summe -5062,5 Grad +-1, Hoehenbereich unter 1 Einheit, kein Schritt ungleich -1024.
constexpr s16 kGbKurs = 0x0800;                 // 11,25 Grad (Plan WP8.1)
constexpr uint32_t kGbPhaseA = 1800;
constexpr uint32_t kGbGearUp1 = 1800;
constexpr uint32_t kGbGearUp2 = 1806;
constexpr uint32_t kGbPhaseBFrom = 1840;
constexpr uint32_t kGbPhaseBTicks = 900;
constexpr uint32_t kGbPhaseBTo = kGbPhaseBFrom + kGbPhaseBTicks;
constexpr uint32_t kGbRevTicks = 64;            // ein Umlauf bei Gang 5
constexpr int kGbStepB = -1024;                 // Gierschritt je Tick in Phase B
constexpr float kGbStraightLen = 96000.0f;      // 1800 Ticks * 50 plus Wechsel, mit Reserve
constexpr float kGbCircleAt = 93000.0f;         // erwarteter Beginn des Kreises auf der Geraden
constexpr float kGbRadius = 1018.6f;            // Wendekreis (Gang 5: 100 / (1024 * 2 pi / 65536))

struct GbState {
    Wp8Run run;
    bool placed = false;
    Wp8Start start;
    float y0 = 0.0f;
    s16 yaw0 = 0;
    float dyMaxA = 0.0f;
    double courseMaxA = 0.0;
    double nickMaxA = 0.0;
    long long cumYawA0 = 0;
    long long fremdA0 = 0;
    long long flugA = 0;
    long long fremdA = 0;
    uint32_t birdsMin = 0xFFFFFFFFu;
    uint32_t birdsMax = 0;
    bool okA = false;
    long long cumYawB0 = 0;
    long long cumYawRev = 0;
    long long fremdB0 = 0;
    uint32_t revs = 0;
    double revMaxErr = 0.0;
    uint32_t unsauberB = 0;
    s16 yawPrevB = 0;
    bool yBSeen = false;
    float yMinB = 0.0f;
    float yMaxB = 0.0f;
    double nickMaxB = 0.0;
    int gearB = 0;
    bool okB = false;
    bool done = false;
};
GbState g_gb;

bool gbCheck(const cXyz& p, s16 yaw) {
    if (sweepStraight(p, yaw, kGbStraightLen) < kGbStraightLen) {
        return false;
    }
    const cXyz c(p.x + cM_ssin(yaw) * kGbCircleAt, p.y, p.z + cM_scos(yaw) * kGbCircleAt);
    return sweepCircleRight(c, yaw, kGbRadius + 150.0f);
}

void gbEnd(const char* why, uint32_t nachlauf) {
    const GbState& s = g_gb;
    const bool ok = s.okA && s.okB && autotest::foreignInputCount() == 0 && nachlauf >= kWp8Nachlauf;
    logInfo("flug: SZENARIO geradeaus-b %s A=%s B=%s sweep=%d start=(%.1f,%.1f,%.1f) kurs=%.2f hoehe+%.0f fremdeingabe=%u "
            "nachlauf=%u -> %s",
        why, s.okA ? "ok" : "fail", s.okB ? "ok" : "fail", s.start.ok ? 1 : 0, static_cast<double>(s.start.pos.x),
        static_cast<double>(s.start.pos.y), static_cast<double>(s.start.pos.z), degOf(s.start.yaw),
        static_cast<double>(s.start.lift), autotest::foreignInputCount(), nachlauf, ok ? "BESTANDEN" : "NICHT BESTANDEN");
}

bool gbRide(autotest::Context& c, daKago_c* k) {
    GbState& s = g_gb;
    const uint32_t f = c.sess.rideFrames;
    if (!s.placed) {
        s.placed = true;
        s.start = wp8FindStart(k, kGbKurs, &gbCheck, "geradeaus-b");
        if (s.start.ok) {
            wp8Place(k, s.start);
        } else {
            k->current.angle.y = kGbKurs;
            k->shape_angle.y = kGbKurs;
            k18Rebase(k);
        }
        s.y0 = k->current.pos.y;
        s.yaw0 = k->current.angle.y;
        s.cumYawA0 = g_att.cumYaw;
        s.fremdA0 = g_k18.foreignSum;
        logInfo("flug: GERADE-B Start f=%u pos=(%.1f,%.1f,%.1f) kurs=%.2f gang=%d", f, static_cast<double>(k->current.pos.x),
            static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z), degOf(k->current.angle.y),
            c.sess.gear);
        c.stick(0.0f, 0.0f);
        return false;
    }
    const float y = k->current.pos.y;
    const s16 yaw = k->current.angle.y;
    const double nick = std::fabs(degOf(k->current.angle.x));

    if (f <= kGbPhaseA) {
        s.dyMaxA = std::max(s.dyMaxA, std::fabs(y - s.y0));
        s.courseMaxA = std::max(s.courseMaxA, std::fabs(degOf(static_cast<s16>(yaw - s.yaw0))));
        s.nickMaxA = std::max(s.nickMaxA, nick);
        if (f == 1 || f == 900 || f == kGbPhaseA) {
            const uint32_t n = wp8CountBirds();
            s.birdsMin = std::min(s.birdsMin, n);
            s.birdsMax = std::max(s.birdsMax, n);
        }
        if (f % 300 == 0) {
            logInfo("flug: GERADE-B A f=%u y=%.2f dy=%.2f kursRel=%.3f nick=%.3f V=%.2f gang=%d", f, static_cast<double>(y),
                static_cast<double>(y - s.y0), degOf(static_cast<s16>(yaw - s.yaw0)), nick,
                static_cast<double>(g_att.speed), c.sess.gear);
        }
        if (f == 900) {
            logInfo("flug: MARK geradeaus-b-30s f=%u dy=%.3f kursRel=%.3f", f, static_cast<double>(y - s.y0),
                degOf(static_cast<s16>(yaw - s.yaw0)));
        }
        if (f == kGbPhaseA) {
            s.flugA = g_att.cumYaw - s.cumYawA0;
            s.fremdA = g_k18.foreignSum - s.fremdA0;
            s.okA = s.dyMaxA < 50.0f && s.courseMaxA < 2.0 && s.flugA == 0 && s.fremdA == 0 && s.birdsMin == 1 &&
                    s.birdsMax == 1;
            logInfo("flug: GERADE-B A (1800 Ticks neutral): dHoehe=%.3f (Grenze 50) dKurs=%.4f Grad (Grenze 2) nickMax=%.3f "
                    "flugGier=%.4f Grad fremdGier=%.4f Grad voegel=%u..%u -> %s",
                static_cast<double>(s.dyMaxA), s.courseMaxA, s.nickMaxA, degOf(s.flugA), degOf(s.fremdA), s.birdsMin,
                s.birdsMax, s.okA ? "ok" : "fehlgeschlagen");
        }
    }
    if (f == kGbGearUp1 || f == kGbGearUp2) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
        c.note("wp8: Steuerkreuz oben (Gang hoch)");
    }
    if (f == kGbPhaseBFrom) {
        s.cumYawB0 = g_att.cumYaw;
        s.cumYawRev = g_att.cumYaw;
        s.fremdB0 = g_k18.foreignSum;
        s.yawPrevB = yaw;
        s.gearB = c.sess.gear;
        logInfo("flug: GERADE-B B beginnt f=%u gang=%d eff=%.3f y=%.2f", f, c.sess.gear,
            static_cast<double>(c.sess.speedScale), static_cast<double>(y));
    }
    if (f > kGbPhaseBFrom && f <= kGbPhaseBTo) {
        const s16 step = static_cast<s16>(yaw - s.yawPrevB);
        if (step != kGbStepB) {
            ++s.unsauberB;
            if (s.unsauberB <= 5) {
                logInfo("flug: GERADE-B B unsauberer Schritt f=%u schritt=%d", f, static_cast<int>(step));
            }
        }
        s.yawPrevB = yaw;
        if (!s.yBSeen) {
            s.yBSeen = true;
            s.yMinB = y;
            s.yMaxB = y;
        }
        s.yMinB = std::min(s.yMinB, y);
        s.yMaxB = std::max(s.yMaxB, y);
        s.nickMaxB = std::max(s.nickMaxB, nick);
        if ((f - kGbPhaseBFrom) % kGbRevTicks == 0) {
            ++s.revs;
            const double rev = degOf(g_att.cumYaw - s.cumYawRev);
            s.revMaxErr = std::max(s.revMaxErr, std::fabs(std::fabs(rev) - 360.0));
            s.cumYawRev = g_att.cumYaw;
            logInfo("flug: GERADE-B UMLAUF %u f=%u gier=%.3f Grad (Soll -360 +-5) y=%.2f", s.revs, f, rev,
                static_cast<double>(y));
        }
        if (f == kGbPhaseBFrom + 450) {
            logInfo("flug: MARK geradeaus-b-kreis f=%u y=%.2f", f, static_cast<double>(y));
        }
    }
    if (f == kGbPhaseBTo) {
        const double sum = degOf(g_att.cumYaw - s.cumYawB0);
        const double fremd = degOf(g_k18.foreignSum - s.fremdB0);
        const double range = static_cast<double>(s.yMaxB - s.yMinB);
        s.okB = s.revs >= 14 && s.revMaxErr <= 5.0 && std::fabs(sum + 5062.5) <= 1.0 && range < 1.0 && s.unsauberB == 0 &&
                s.gearB == 5;
        logInfo("flug: GERADE-B B (900 Ticks rechts, Gang %d): summe=%.2f Grad (Soll -5062,5) umlaeufe=%u maxFehler=%.3f Grad "
                "hoehenBereich=%.4f (Grenze 1) nickMax=%.3f unsauber=%u fremd=%.4f -> %s",
            s.gearB, sum, s.revs, s.revMaxErr, range, s.nickMaxB, s.unsauberB, fremd, s.okB ? "ok" : "fehlgeschlagen");
    }
    if (f >= kGbPhaseBFrom && f < kGbPhaseBTo) {
        c.stick(1.0f, 0.0f);
    } else {
        c.stick(0.0f, 0.0f);
    }
    if (f == kGbPhaseBTo + 2) {
        wp8LowerForLanding(k);
    }
    return f >= kGbPhaseBTo + 4;
}

bool gbRun(autotest::Context& c) {
    ++g_padFrames;
    const bool done = wp8Drive(c, g_gb.run, &gbRide, &gbEnd);
    if (done) {
        g_testFinished = true;
    }
    return done;
}

const autotest::Scenario kGeradeausBScenario = {
    "geradeaus-b",
    "WP8.1/8.4: Sweep-Start Kurs 11,25 Grad, 1800 Ticks neutral (Hoehe, Kurs, Gier 0), dann Gang 5 und 900 Ticks Dauerkreis",
    gbRun,
};

// ---- trimm (WP8.3) ------------------------------------------------------------------------------
//   Stick Y -0,6 bis Ritt-Frame 150 (wie der Kamera-Test: fast eine volle Schleife), dann neutral.
//   Pass: |Nick| <= 0,5 Grad hoechstens 120 Ticks nach dem Loslassen und danach dauerhaft; in den 300 Ticks ab dem
//   Erreichen der Waagerechten |dy| <= 20. Zur Information: dy in 300 Ticks ab dem Loslassen.
constexpr uint32_t kTrRelease = 150;
constexpr float kTrStickY = -0.6f;
constexpr double kTrLevelDeg = 0.5;
constexpr uint32_t kTrLevelMax = 120;
constexpr uint32_t kTrHold = 300;
constexpr float kTrDyMax = 20.0f;

struct TrState {
    Wp8Run run;
    bool placed = false;
    Wp8Start start;
    double nickAtRelease = 0.0;
    float yRelease = 0.0f;
    bool level = false;
    uint32_t levelAt = 0;     // Ritt-Frame, ab dem |Nick| <= 0,5
    float yLevel = 0.0f;
    float dyMaxLevel = 0.0f;  // groesste |dy| in kTrHold Ticks ab levelAt
    uint32_t relapse = 0;     // Ticks nach levelAt mit |Nick| > 0,5
    float dyRelease300 = 0.0f;
    bool ok = false;
    bool evaluated = false;
};
TrState g_tr;

bool trCheck(const cXyz& p, s16 yaw) {
    if (!sweepLoopUp(p, yaw, 1500.0f)) {
        return false;
    }
    const cXyz back(p.x - cM_ssin(yaw) * 1500.0f, p.y + 100.0f, p.z - cM_scos(yaw) * 1500.0f);
    return sweepStraight(back, yaw, 22000.0f) >= 22000.0f;
}

void trEnd(const char* why, uint32_t nachlauf) {
    const TrState& s = g_tr;
    const bool ok = s.ok && autotest::foreignInputCount() == 0 && nachlauf >= kWp8Nachlauf;
    logInfo("flug: SZENARIO trimm %s nickBeimLoslassen=%.2f ticksBisWaagerecht=%u (Grenze 120) rueckfaelle=%u "
            "dyNachTrimm300=%.3f (Grenze 20) dyAbLoslassen300=%.2f sweep=%d hoehe+%.0f fremdeingabe=%u nachlauf=%u -> %s",
        why, s.nickAtRelease, s.level ? s.levelAt - kTrRelease : 0u, s.relapse, static_cast<double>(s.dyMaxLevel),
        static_cast<double>(s.dyRelease300), s.start.ok ? 1 : 0, static_cast<double>(s.start.lift),
        autotest::foreignInputCount(), nachlauf, ok ? "BESTANDEN" : "NICHT BESTANDEN");
}

bool trRide(autotest::Context& c, daKago_c* k) {
    TrState& s = g_tr;
    const uint32_t f = c.sess.rideFrames;
    if (!s.placed) {
        s.placed = true;
        s.start = wp8FindStart(k, k->current.angle.y, &trCheck, "trimm");
        wp8Place(k, s.start);
        logInfo("flug: TRIMM Start f=%u pos=(%.1f,%.1f,%.1f) kurs=%.2f", f, static_cast<double>(k->current.pos.x),
            static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z), degOf(k->current.angle.y));
    }
    const double nick = degOf(k->current.angle.x);
    const float y = k->current.pos.y;
    if (f < kTrRelease) {
        c.stick(0.0f, kTrStickY);
        return false;
    }
    c.stick(0.0f, 0.0f);
    if (f == kTrRelease) {
        s.nickAtRelease = nick;
        s.yRelease = y;
        logInfo("flug: TRIMM Loslassen f=%u nick=%.2f cumNick=%.1f y=%.2f", f, nick, degOf(g_att.cumPitch),
            static_cast<double>(y));
        logInfo("flug: MARK trimm-loslassen f=%u", f);
    }
    if (f > kTrRelease) {
        if (!s.level) {
            if (std::fabs(nick) <= kTrLevelDeg) {
                s.level = true;
                s.levelAt = f;
                s.yLevel = y;
                logInfo("flug: TRIMM waagerecht f=%u nach %u Ticks nick=%.3f y=%.2f", f, f - kTrRelease, nick,
                    static_cast<double>(y));
            } else if (f - kTrRelease > kTrLevelMax + 60) {
                logInfo("flug: TRIMM nicht waagerecht nach %u Ticks, nick=%.2f", f - kTrRelease, nick);
                s.evaluated = true;
                s.ok = false;
                return true;
            }
        } else if (f <= s.levelAt + kTrHold) {
            if (std::fabs(nick) > kTrLevelDeg) {
                ++s.relapse;
            }
            s.dyMaxLevel = std::max(s.dyMaxLevel, std::fabs(y - s.yLevel));
            if (f == s.levelAt + 150) {
                logInfo("flug: MARK trimm-gerade f=%u dy=%.3f", f, static_cast<double>(y - s.yLevel));
            }
        }
        if (f == kTrRelease + kTrHold) {
            s.dyRelease300 = y - s.yRelease;
        }
    }
    if (s.level && f == s.levelAt + kTrHold && !s.evaluated) {
        s.evaluated = true;
        s.ok = s.levelAt - kTrRelease <= kTrLevelMax && s.relapse == 0 && s.dyMaxLevel <= kTrDyMax;
        logInfo("flug: TRIMM Ergebnis waagerecht nach %u Ticks, rueckfaelle=%u, dyMax=%.3f in %u Ticks -> %s",
            s.levelAt - kTrRelease, s.relapse, static_cast<double>(s.dyMaxLevel), kTrHold, s.ok ? "ok" : "fehlgeschlagen");
    }
    if (s.evaluated && f >= std::max(s.levelAt + kTrHold, kTrRelease + kTrHold) + 2) {
        if (f == std::max(s.levelAt + kTrHold, kTrRelease + kTrHold) + 2) {
            wp8LowerForLanding(k);
            return false;
        }
        return true;
    }
    return false;
}

bool trRun(autotest::Context& c) {
    ++g_padFrames;
    const bool done = wp8Drive(c, g_tr.run, &trRide, &trEnd);
    if (done) {
        g_testFinished = true;
    }
    return done;
}

const autotest::Scenario kTrimmScenario = {
    "trimm",
    "WP8.3: Stick Y -0,6 bis Ritt-Frame 150, dann neutral: Nick 0 in hoechstens 120 Ticks, |dy| <= 20 in 300 Ticks",
    trRun,
};

// ---- extrem (WP8.10) ----------------------------------------------------------------------------
//   Gang 5 (Steuerkreuz oben bei f 5 und 12), ab f 40 Stick Y voll (Nase hoch) bis fuenf Schleifen gezaehlt sind, Dash
//   (Turbo) bei f 45, 145, 245 und 345, zwei Fassrollen bei f 700 und 900, sonst Dauerkreis rechts bis f 1500.
//   Pass hier: fuenf Schleifen bis f 1500, Link durchgehend in Proc 312, Szenario ohne Absturz zu Ende. Sichtfeld
//   (kamera: SUMMARY fovMax, maxFovStep) und Durchdringungen (freiheit: SUMMARY2) stehen in den Modulzeilen.
constexpr uint32_t kExGear1 = 5;
constexpr uint32_t kExGear2 = 12;
constexpr uint32_t kExLoopFrom = 40;
constexpr uint32_t kExEnd = 1500;
constexpr uint32_t kExLoops = 5;
constexpr uint32_t kExDash[] = {45, 145, 245, 345};
constexpr uint32_t kExRoll[] = {700, 900};

struct ExState {
    Wp8Run run;
    bool placed = false;
    Wp8Start start;
    long long pitch0 = 0;
    uint32_t loops = 0;
    uint32_t loopsDoneAt = 0;
    bool seen312 = false;
    uint32_t ok312 = 0;
    uint32_t bad312 = 0;
    float fovMax = 0.0f;
    float fovStepMax = 0.0f;
    float fovPrev = 0.0f;
    bool ok = false;
};
ExState g_ex;

bool exCheck(const cXyz& p, s16 yaw) {
    return sweepLoopUp(p, yaw, 1000.0f) && sweepCircleRight(p, yaw, kGbRadius + 250.0f);
}

void exEnd(const char* why, uint32_t nachlauf) {
    const ExState& s = g_ex;
    const bool ok = s.ok && autotest::foreignInputCount() == 0 && nachlauf >= kWp8Nachlauf;
    logInfo("flug: SZENARIO extrem %s schleifen=%u (fertig bei f=%u) proc312 gesehen=%d ok=%u bruch=%u fassrollen=%u "
            "fovZielMax=%.2f fovZielSchrittMax=%.2f sweep=%d hoehe+%.0f fremdeingabe=%u nachlauf=%u -> %s",
        why, s.loops, s.loopsDoneAt, s.seen312 ? 1 : 0, s.ok312, s.bad312, g_att.barrels, static_cast<double>(s.fovMax),
        static_cast<double>(s.fovStepMax), s.start.ok ? 1 : 0, static_cast<double>(s.start.lift),
        autotest::foreignInputCount(), nachlauf, ok ? "BESTANDEN" : "NICHT BESTANDEN");
}

bool exRide(autotest::Context& c, daKago_c* k) {
    ExState& s = g_ex;
    const uint32_t f = c.sess.rideFrames;
    if (!s.placed) {
        s.placed = true;
        s.start = wp8FindStart(k, k->current.angle.y, &exCheck, "extrem");
        wp8Place(k, s.start);
        logInfo("flug: EXTREM Start f=%u pos=(%.1f,%.1f,%.1f) kurs=%.2f", f, static_cast<double>(k->current.pos.x),
            static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z), degOf(k->current.angle.y));
    }
    if (daAlink_c* link = liveLink()) {
        if (link->mProcID == daAlink_c::PROC_WOLF_CARGO_CARRY) {
            s.seen312 = true;
            ++s.ok312;
        } else if (s.seen312) {
            if (s.bad312 == 0) {
                logInfo("flug: EXTREM Link verlaesst Proc 312 bei f=%u (proc=%u)", f, static_cast<unsigned>(link->mProcID));
            }
            ++s.bad312;
        }
    }
    const float fov = c.sess.fovExtraDeg;
    s.fovMax = std::max(s.fovMax, fov);
    s.fovStepMax = std::max(s.fovStepMax, std::fabs(fov - s.fovPrev));
    s.fovPrev = fov;

    if (f == kExGear1 || f == kExGear2) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
    }
    for (const uint32_t at : kExDash) {
        if (f == at) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_A));
            c.note("wp8: A (Dash, Turbo)");
        }
    }
    for (const uint32_t at : kExRoll) {
        if (f == at) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));
            c.note("wp8: Y (Fassrolle)");
        }
    }
    if (f == kExLoopFrom) {
        s.pitch0 = g_att.cumPitch;
        logInfo("flug: EXTREM Schleifen beginnen f=%u gang=%d eff=%.3f", f, c.sess.gear,
            static_cast<double>(c.sess.speedScale));
    }
    if (f >= kExLoopFrom && s.loops < kExLoops) {
        const long long turned = s.pitch0 - g_att.cumPitch;  // Nase hoch: cumPitch faellt
        const uint32_t loops = static_cast<uint32_t>(turned / 65536LL);
        if (loops > s.loops) {
            s.loops = loops;
            logInfo("flug: EXTREM Schleife %u fertig f=%u cumNick=%.1f y=%.1f", s.loops, f, degOf(g_att.cumPitch),
                static_cast<double>(k->current.pos.y));
            if (s.loops == 3) {
                logInfo("flug: MARK extrem-schleife3 f=%u", f);
            }
            if (s.loops == kExLoops) {
                s.loopsDoneAt = f;
            }
        }
        c.stick(0.0f, s.loops < kExLoops ? -1.0f : 0.0f);
    } else if (f >= kExLoopFrom) {
        c.stick(1.0f, 0.0f);  // Dauerkreis rechts, bleibt im gepruften Raum
    } else {
        c.stick(0.0f, 0.0f);
    }
    if (f == 1000) {
        logInfo("flug: MARK extrem-kreis f=%u", f);
    }
    if (f == kExEnd) {
        s.ok = s.loops >= kExLoops && s.loopsDoneAt <= kExEnd && s.seen312 && s.bad312 == 0;
        logInfo("flug: EXTREM Ergebnis schleifen=%u bis f=%u proc312 ok=%u bruch=%u -> %s", s.loops, s.loopsDoneAt,
            s.ok312, s.bad312, s.ok ? "ok" : "fehlgeschlagen");
    }
    if (f == kExEnd + 2) {
        wp8LowerForLanding(k);
    }
    return f >= kExEnd + 4;
}

bool exRun(autotest::Context& c) {
    ++g_padFrames;
    const bool done = wp8Drive(c, g_ex.run, &exRide, &exEnd);
    if (done) {
        g_testFinished = true;
    }
    return done;
}

const autotest::Scenario kExtremScenario = {
    "extrem",
    "WP8.10: Gang 5, Dash, fuenf Schleifen, zwei Fassrollen, Dauerkreis bis f 1500: kein Absturz, Proc 312",
    exRun,
};
#endif  // KFF_DEV_AUTOTEST

ModResult report(const char* what, ModResult r) {
    if (r == MOD_OK) {
        logInfo("flug: install %s ok", what);
    } else {
        logError("flug: install %s -> %d", what, static_cast<int>(r));
    }
    return r;
}

// K11 (NACHARBEIT 3, Bau G): Spielanzeige im Ritt folgt der Belegung.
// Vanilla (daKago_c::executeFly): je Tick DoStatusForce = Dash (UNK_75, nur bei mDashCooldownTime == 0) und
// AStatusForce = Quit (QUIT; Vogeltyp ungleich NORMAL: UNK_39). dMeter2_c::moveButtonA/B/R uebernehmen einen
// Force-Wert != 0 einmal in das Feld und setzen ihn auf 0. Force 0 heisst "kein Force" und loescht kein Feld.
// Ein leeres Feld hat Status 0 (getActionString: i_action_num[0] = 0, kein Text). Das HUD zeichnet ein Feld neu,
// wenn sein Status sich aendert (moveButtonA/B/XY/Z, mItemStatus fuer X und Y).
// Hier, nur im Ritt (kff::isRiding: Phase Riding, Landeanflug inklusive): LinkExecute post und KagoExecute post
// setzen nach den Vanilla-Setzern die Forces von Do, A, X, Y, Z, R auf 0 und die Felder auf den Sollwert. Im
// Frame-Anfang (PadRead post, vor den Akteuren) kommen nach dem Ritt die gesicherten Vanilla-Werte zurueck.
namespace label {
enum Field : uint8_t { kLblDo, kLblA, kLblX, kLblY, kLblZ, kLblR, kLblCount };

// X: Text vorlaeufig zeigen. Zeigt das Bild X nicht sauber, bleibt X leer (false).
constexpr bool kLabelOnX = true;

bool g_active = false;
bool g_written[kLblCount] = {};
uint8_t g_writtenVal[kLblCount] = {};
uint8_t g_saved[kLblCount] = {};

uint8_t getField(int f) {
    switch (f) {
    case kLblDo: return dComIfGp_getDoStatus();
    case kLblA: return dComIfGp_getAStatus();
    case kLblX: return dComIfGp_getXStatus();
    case kLblY: return dComIfGp_getYStatus();
    case kLblZ: return dComIfGp_getZStatus();
    default: return dComIfGp_getRStatus();
    }
}

void setField(int f, uint8_t v) {
    switch (f) {
    case kLblDo: dComIfGp_setDoStatus(v, 0); break;
    case kLblA: dComIfGp_setAStatus(v, 0); break;
    case kLblX: dComIfGp_setXStatus(v, 0); break;
    case kLblY: dComIfGp_setYStatus(v, 0); break;
    case kLblZ: dComIfGp_setZStatus(v, 0); break;
    default: dComIfGp_setRStatus(v, 0); break;
    }
}

void zeroForce(int f) {
    switch (f) {
    case kLblDo: dComIfGp_setDoStatusForce(0, 0); break;
    case kLblA: dComIfGp_setAStatusForce(0, 0); break;
    case kLblX: dComIfGp_setXStatusForce(0, 0); break;
    case kLblY: dComIfGp_setYStatusForce(0, 0); break;
    case kLblZ: dComIfGp_setZStatusForce(0, 0); break;
    default: dComIfGp_setRStatusForce(0, 0); break;
    }
}

// Knopf-Bit je Feld (kff::kHudFieldBits in core/kff/actions.hpp): Do = A, AStatus = B, X, Y, Z, R.
uint32_t fieldBit(int f) { return kff::kHudFieldBits[f]; }

// Sollwert je Feld im Ritt: Dash = UNK_75 ("Dash"), Dismount = QUIT ("Quit"), sonst 0 (leer).
// R bleibt leer: drawButtonR schreibt keinen Text und zeigt das Textfeld von Z mit (mpTextXY[2]).
uint8_t desired(int f) {
    if (f == kLblR) return 0;
    if (f == kLblX && !kLabelOnX) return 0;
    const uint32_t bit = fieldBit(f);
    if (kff::act(kff::Act::Dash).bit == bit) return static_cast<uint8_t>(BUTTON_STATUS_UNK_75);
    if (kff::act(kff::Act::Dismount).bit == bit) return static_cast<uint8_t>(BUTTON_STATUS_QUIT);
    return 0;
}

void save() {
    for (int f = 0; f < kLblCount; ++f) {
        g_saved[f] = getField(f);
        g_written[f] = false;
    }
}

void restore() {
    for (int f = 0; f < kLblCount; ++f) {
        setField(f, g_saved[f]);
    }
    g_active = false;
}

void apply() {
    for (int f = 0; f < kLblCount; ++f) {
        zeroForce(f);
        const uint8_t cur = getField(f);
        if (g_written[f] && cur != g_writtenVal[f]) {
            g_saved[f] = cur;  // ein Vanilla-Setzer hat das Feld im Ritt geaendert: das gilt nach dem Ritt
        }
        const uint8_t want = desired(f);
        if (cur != want) {
            setField(f, want);
        }
        g_written[f] = true;
        g_writtenVal[f] = want;
    }
}

void tickAfterVanilla() {
    if (!isRiding()) {
        if (g_active) restore();
        return;
    }
    if (!g_active) {
        save();
        g_active = true;
    }
    apply();
}

void tickFrameStart() {
    if (g_active && !isRiding()) restore();
}

void onLinkExecutePost(ModContext*, void*, void*, void*) { tickAfterVanilla(); }
void onKagoExecutePost(ModContext*, void*, void*, void*) { tickAfterVanilla(); }
void onPadReadPost(ModContext*, void*, void*, void*) { tickFrameStart(); }
void restoreIfActive() {
    if (g_active) restore();
}
}  // namespace label

class FlugModule final : public Module {
public:
    const char* name() const override { return "flug"; }

    void onShutdown(kff::Session&) override {
        label::restoreIfActive();
    }

    ModResult install(Registry& reg) override {
        ModResult r = report("daKago_c::executeFly pre",
            reg.addPre(Hook::KagoExecuteFly, &onExecuteFlyPre, prio::kFlug));
        if (r == MOD_OK) {
            r = report("daKago_c::executeFly post", reg.addPost(Hook::KagoExecuteFly, &onExecuteFlyPost, prio::kFlug));
        }
        if (r == MOD_OK) {
            r = report("daKago_c::flySpeedCalc pre",
                reg.addPre(Hook::KagoFlySpeedCalc, &onFlySpeedCalcPre, prio::kFlug));
        }
        if (r == MOD_OK) {
            r = report("daAlink_c::execute post (Anzeige im Ritt)",
                reg.addPost(Hook::LinkExecute, &label::onLinkExecutePost, prio::kFlug));
        }
        if (r == MOD_OK) {
            r = report("daKago_c::execute post (Anzeige im Ritt)",
                reg.addPost(Hook::KagoExecute, &label::onKagoExecutePost, prio::kFlug));
        }
        if (r == MOD_OK) {
            r = report("mDoCPd_c::read post (Anzeige, Rittende)",
                reg.addPost(Hook::PadRead, &label::onPadReadPost, prio::kFlug));
        }
#if defined(KFF_DEV_AUTOTEST)
        if (r == MOD_OK) {
            r = report("autotest geradeaus", autotest::registerScenario(&kGeradeausScenario));
        }
        if (r == MOD_OK) {
            r = report("autotest geradeaus2", autotest::registerScenario(&kGeradeaus2Scenario));
        }
        if (r == MOD_OK) {
            r = report("autotest geradeaus-b", autotest::registerScenario(&kGeradeausBScenario));
        }
        if (r == MOD_OK) {
            r = report("autotest trimm", autotest::registerScenario(&kTrimmScenario));
        }
        if (r == MOD_OK) {
            r = report("autotest extrem", autotest::registerScenario(&kExtremScenario));
        }
#endif
        return r;
    }

    void onRideStart(Session&, daKago_c* bird) override {
        g_att = Attitude{};
        g_uiHalt = UiHalt{};
        g_hoverAnim = false;
        g_hoverBlocked = false;
        g_hoverMeasured = false;
#if defined(KFF_DEV_AUTOTEST)
        g_k18 = K18State{};
#endif
        if (bird != nullptr && bird->mFlySpeed > 0.0f) {
            g_att.speed = bird->mFlySpeed;
        }
        logInfo("flug: Ritt start, Lage auf null, V=%.1f Hoechsttempo=%.1f Gang=%.2f lenkung_invertieren=%d cruise=%.1f "
                "nick=%.1f",
            static_cast<double>(g_att.speed), static_cast<double>(topSpeed()), static_cast<double>(wegFactor()),
            steerInverted() ? 1 : 0, static_cast<double>(cruiseSpeed()),
            bird != nullptr ? degOf(bird->current.angle.x) : 0.0);
    }

    void onRideEnd(Session&, daKago_c*, EndReason why) override {
        logInfo("flug: Ritt Ende (%s) cumNick=%.1f cumGier=%.1f cumRolle=%.1f fassrollen=%u "
                "modusAbweichend=%u wasserfall=%u trimFrames=%u",
            endReasonName(why), degOf(g_att.cumPitch), degOf(g_att.cumYaw), degOf(g_att.cumRoll), g_att.barrels,
            g_att.modeAway, g_att.wfFrames, g_att.trimFrames);
        if (g_uiHalt.halts > 0) {
            logInfo("flug: ui_halt im Ritt %u mal, %u Frames", g_uiHalt.halts, g_uiHalt.frames);
        }
        g_uiHalt.active = false;
        g_hoverAnim = false;
        g_hoverBlocked = false;
        g_hoverMeasured = false;
#if defined(KFF_DEV_AUTOTEST)
        k18Summary(endReasonName(why));
#endif
    }

#if defined(KFF_DEV_AUTOTEST)
    const autotest::Scenario* autotestScenario() const override { return &kLoopingScenario; }
#endif
};

FlugModule g_module;

}  // namespace

namespace modules {

Module& flug() {
    return g_module;
}

}  // namespace modules
}  // namespace kff
