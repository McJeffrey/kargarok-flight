// Modul "kamera": freie Kamera fuer den Ritt, einziger Kamera-Operator des Mods (CameraService).
//
// Aktiv nur im Ritt (Link im Vogel mit Cargo-Carry), ohne Ereignis und ohne Demo-Kamera. Sonst gibt der Operator
// false zurueck, und die Spielkamera rechnet weiter. Den Schalter cam_mode gibt es nicht mehr (Entscheidung D10).
//
// Bezugssystem. Die Kamera haengt am Vogel. Seine gezeichnete Lage (shape_angle, Matrix M = Ry Rx Rz wie
// daKago_c::mtx_set) ergibt die Nase f = M e_z und das Oben u = M e_y, rechts r = u x f. Der Blickpunkt liegt
// kCenterUp ueber dem Vogel in Richtung u, der Vogel bleibt also in der Bildmitte. Das Auge liegt auf einer Kugel um
// den Vogel: rel = Winkel um u (0 = hinter dem Vogel, ohne Anschlag), elev = Hoehe ueber seiner Ebene. Nick, Gier,
// Rolle, Loopings und Fassrollen gehen nur ueber f und u ein, ohne Winkelzerlegung. Ein Nick ueber 90 Grad ist kein
// Sonderfall: am Scheitel bleibt die Kamera hinter dem Vogel.
//
// Oben der Ansicht. cam->mUp bekommt die Mischung aus dem Oben des Vogels ohne Rolle und dem vollen Oben (Gewicht
// kRollGain, stetig ueber die Rolle, sanftes Mitrollen). Dazu loescht der Operator die Flags 0x10 (Oben-Override,
// Up() liest sonst den Override) und 0x1 (Ereignis-Pause, view_setup uebernimmt sonst nichts). Der Abstand von Oben
// und Blickrichtung bleibt D cos(elev), nie null, also kein Flip am Scheitel. Herleitung: spaetfunde/kamera-looping.md.
//
// Griff-Ritt (Mensch-Link, Sitzung rideGrip, Modul mensch): dieselbe Kamera wie beim Wolf-Ritt, Drehpunkt ist der Vogel.
// Link haengt im Griff rund 300 bis 360 Einheiten unter dem Vogel und liegt damit unten im Bild. Gleich im Griff und im
// Wolf-Ritt: Startabstand kamera_abstand (Standard 650), H/J bis 1400, Standardneigung kElevDefault (14 Grad), C-Stick,
// X-Schwenk, Wand, Vorausblick, Ausweichen (K22), Boden, Uebergang. Der frueher eigene Menschen-Ritt (Drehpunkt zwischen Vogel
// und Link, Abstand 1100, Neigung 25 Grad, Kurven-Vorausblick, Ausweichen mit Zusatzneigung) ist entfernt. kamera_abstand_mensch
// wirkt nicht mehr (der Schluessel bleibt lesbar, siehe README). Telemetrie GRIFF: Lage von Link im Bild (Testbuild).
//
// Steuerung im Ritt (Pad 1):
//   C-Stick links/rechts   dreht um den Vogel, ohne Anschlag
//   C-Stick hoch/runter    Neigung ueber dem Vogel, -5 bis 60 Grad (Test mit KFF_KAMERA_TESTLOW=1 bis -70)
//   Steuerkreuz rechts     naeher, links weiter (Abstand 300 bis 1400, im Wolf- und Griff-Ritt gleich)
//   X antippen             Kamera schwenkt hinter den Vogel und auf die Standardneigung
//   120 Ticks ohne C-Stick Rueckschwenk hinter den Vogel
//   Wand                   Vorausblick (kLookAhead) und Liniencheck Vogel -> Auge. Liegt die Wandgrenze (Treffer minus kWallMargin)
//                          naeher als der Abstand, springt er sofort auf sie (harte Grenze). Sonst sinkt er je Tick hoechstens um
//                          kRetractMax und waechst hoechstens um kDistGrowRate.
//   Wand am Vogel          Liegt die Wand direkt hinter dem Vogel (freie Strecke unter kAvoidTouch), schwenkt der Blick sofort zur
//                          naechsten freien Richtung aus und kehrt geglaettet zurueck, sobald die Richtung wieder frei ist (K22)
//   Boden                  das Auge bleibt 70 Einheiten ueber dem Boden unter ihm
//   Sichtfeld              Zusatz Session::fovExtraDeg (Modul tempo, 0 bis 10 Grad, hoechstens 0,5 je Tick).
//   Ende des Ritts         Rueckgabe an die Spielkamera: fovy und Oben zurueck, QuickStart bei eingefrorenem Zustand
//   Stop-Heilung           steht die Spielkamera im Ritt ohne Ereignis 30 Ticks still (dCamera_c::Stop), setzt der
//                          PadRead-Hook sie mit QuickStart zurueck. Hilfsweg. Mit dem Kern-Hook CameraStop (K22, nur bei KFF_CORE_CAMERA_STOP_HOOK) faengt das Modul den Stopp im Ritt ab, die Heilung bleibt Rueckfall. PadRead-Hook erst nach der Operator-Registrierung, Gate ueber moduleActive (HK-03).
//
// Test nur im Testbuild (KFF_DEV_AUTOTEST) mit KFF_AUTOTEST=kamera (Wolf) oder kamera-mensch (Griff, gleicher Zeitplan, D11):
//   KFF_KAMERA_LAUF=wand   Boden- und Wandlauf (Anflug auf die Bergwand F_SP121 bei x=-81410, z=-23950)
//   KFF_KAMERA_TESTLOW=1   Elevation im Test bis -70 Grad
//   KFF_KAMERA_STOPHOOK=0  Stop-Pre-Hook im Test aus, damit die 30-Tick-Stop-Heilung greift (K22)
// Der Test setzt je Tick C-Stick, Steuerkreuz, X und Y ueber autotest::Context wie jedes Szenario. R, A, B und der
// linke Stick kommen ebenfalls von dort (Ruf, Flugsteuerung). Fremdeingaben zaehlt der Kern (foreignInputCount).
// Schritt "stop-test": dCamera_c::Stop im Ritt, danach muss die Stop-Heilung die Spielkamera wieder freigeben.
// Marken "kamera: SHOT <name>" fuer Screenshots, "kamera: SUMMARY ..." am Ende.
// Der Operator liest Steuerkreuz und X aus kff::input() (Schnappschuss vor dem Konsum des Kerns).

#include "d/dolzel_rel.h"

#include "core/kff/actions.hpp"
#include "core/kff/kff.hpp"

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_xyz.h"
#if defined(KFF_DEV_AUTOTEST)
#include "SSystem/SComponent/c_m3d_g_pla.h"  // VORLAUF K22: Normale der Treffer (nur Testbuild)
#endif
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "m_Do/m_Do_controller_pad.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

// ---------------------------------------------------------------- Werte
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
constexpr float kRad2Deg = 180.0f / kPi;
constexpr float kDegPerS16 = 360.0f / 65536.0f;  // s16-Einheiten je Grad, 65536 = 360 Grad
constexpr float kYawRateDeg = 1.5f;              // Grad je Tick bei vollem C-Stick X
constexpr float kPitchRateDeg = 0.7f;            // Grad je Tick bei vollem C-Stick Y
constexpr float kDeadzone = 0.12f;               // kleinere Stickwerte gelten als neutral
constexpr float kElevDefault = 14.0f;            // Grad ueber der Ebene des Vogels
// Griff-Ritt: der Blickpunkt liegt so viele Einheiten unter dem Vogel. Link haengt rund 350 unter dem Vogel, seine Fuesse
// lagen sonst am unteren Bildrand (GRIFF, halbV 28 Grad). Nur im Griff-Ritt, der Wolf-Ritt bleibt unveraendert.
constexpr float kMenschAimDrop = 100.0f;
constexpr float kElevMin = -5.0f;
constexpr float kElevMax = 60.0f;
// Schweben (Entwurf H1 4.5): Neigung bis plus und minus 70 Grad, nicht 90 (die Blickrichtung waere sonst fast parallel zum
// Oben des Vogels, das Bild kann sich drehen). Nach dem Schweben klingt die Neigung mit 1 Grad je Tick auf den
// Normalbereich ab. Im Schweben gibt es keinen Rueckschwenk.
constexpr float kHoverElevMin = -70.0f;
constexpr float kHoverElevMax = 70.0f;
constexpr float kElevRelax = 1.0f;
#if defined(KFF_DEV_AUTOTEST)
constexpr float kElevTestMin = -70.0f;           // nur Test: KFF_KAMERA_TESTLOW=1
#endif
constexpr float kDistDefault = 650.0f;           // Einheiten, Vogel bis Auge (Startwert aus kamera_abstand, Standard 650; Wolf und Griff)
constexpr float kDistMin = 300.0f;
constexpr float kDistMax = 1400.0f;
constexpr float kDistKeyRate = 6.0f;             // je Tick bei gehaltenem Steuerkreuz
constexpr float kDistGrowRate = 26.0f;           // hoechstes Wachstum je Tick nach einer Wand (K22: vorher 6, 650 in 108 Ticks; jetzt rund 25 Ticks)
constexpr float kCenterUp = 40.0f;               // Blickpunkt ueber dem Vogel, in Richtung seines Oben
constexpr float kOrientFollow = 0.5f;            // Glaettung der Ausrichtung je Tick (nur Richtung, nie Position)
constexpr float kRollGain = 0.35f;               // Gewicht des vollen Vogel-Oben im Oben der Ansicht (0 bis 0,5, sanft)
constexpr float kWallMargin = 35.0f;             // Abstand vor der Wand
// Keine Untergrenze mehr fuer den Abstand bei Wand: die Wandgrenze liegt immer vor der Wand (E7-3, throughWall = 0).
constexpr float kRetractStep = 20.0f;            // Abstand sinkt in einem Tick um mehr als das: Wandkorrektur (Zaehler)
constexpr float kRetractMax = 120.0f;            // Abstand sinkt zum Wunschabstand je Tick hoechstens um das; vor einer Wand gilt sofort die Wandgrenze
constexpr float kLookAhead = 900.0f;             // Vorausblick: Wand bis so weit ueber dem Wunschabstand wird schon gesehen
constexpr float kAvoidTouch = 80.0f;             // K22: freie Strecke hinter dem Vogel unter diesem Wert (Wand am Vogel): Blick schwenkt aus
constexpr float kAvoidOk = 200.0f;               // K22: freie Strecke, die eine Ausweichrichtung mindestens braucht
constexpr float kAvoidRelease = 2.0f;            // K22: Rueckschwenk zur gewuenschten Richtung, Grad je Tick (geglaettet)
constexpr float kAvoidStepDeg = 15.0f;           // K22: Suchschritt nach einer freien Richtung, Grad
constexpr int kAvoidSteps = 12;                  // K22: Suche bis 180 Grad
constexpr float kNoWallLimit = 1.0e9f;           // ohne Wand keine Grenze
constexpr float kEyeClearance = 70.0f;           // Auge ueber dem Boden
constexpr uint32_t kIdleFrames = 120;            // ohne C-Stick, dann beginnt der Rueckschwenk
constexpr float kReturnKeep = 0.975f;            // je Tick, Rueckschwenk
constexpr float kSnapRate = 3.0f;                // X: Schwenk hinter den Vogel, Grad je Tick hoechstens (90 Grad in 30 Ticks)
constexpr float kSnapElevRate = 1.0f;            // X: Neigung zurueck auf die Standardhoehe, Grad je Tick hoechstens
constexpr float kSnapDoneDeg = 0.5f;             // X: Ende des Schwenks
constexpr int kCamStateStop = 3;                 // dCamera_c::Stop setzt mCurState = 3 (Start 2, Stay 1: nicht anfassen)
constexpr uint32_t kBlendFrames = 30;            // Uebergang von der Spielkamera
constexpr float kFovStep = 0.5f;                 // Sichtfeld-Zusatz: Grad je Tick hoechstens
constexpr float kFovMax = 10.0f;                 // Sichtfeld-Zusatz: Obergrenze in Grad
constexpr float kJitterStep = 40.0f;             // Auge bewegt sich relativ zum Blickpunkt um mehr als das: Ruck
constexpr float kZigStep = 4.0f;                 // Gegenlauf zweier Schritte, ab dem es als Zittern zaehlt
constexpr float kUpJumpMarginDeg = 3.0f;         // Oben springt mehr als der Vogel-Oben-Schritt plus dies
constexpr uint32_t kStopHealFrames = 30;         // Stillstand der Spielkamera im Ritt, dann QuickStart
#if defined(KFF_DEV_AUTOTEST)
constexpr uint32_t kLogEvery = 60;               // Telemetrie alle 60 Operator-Aufrufe (nur Testbuild)
#endif

// ---------------------------------------------------------------- Hilfsfunktionen

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Winkel in Grad auf (-180, 180].
float wrap180(float deg) {
    while (deg > 180.0f) {
        deg -= 360.0f;
    }
    while (deg <= -180.0f) {
        deg += 360.0f;
    }
    return deg;
}

float stickAxis(float v) {
    if (std::fabs(v) < kDeadzone) {
        return 0.0f;
    }
    return clampf(v, -1.0f, 1.0f);
}

#if defined(KFF_DEV_AUTOTEST)
// Boden senkrecht unter einem Punkt. false, wenn keiner gefunden wird. Nur Test (Anflug).
bool groundUnder(cXyz p, float& outY) {
    dBgS_GndChk gnd;
    gnd.SetPos(&p);
    const f32 gy = dComIfG_Bgsp().GroundCross(&gnd);
    if (gy > -1.0e8f) {
        outY = gy;
        return true;
    }
    return false;
}

// Stick Y fuer eine Zielhoehe: ueber tAlt sinkt der Vogel (positiv = Nase runter), darunter steigt er. Sanft (0,25),
// denn voller Stick Y dauerhaft fuehrt zum Looping.
float altitudeStick(float by, float tAlt) {
    return clampf((by - tAlt) / 600.0f, -0.25f, 0.25f);
}
#endif  // KFF_DEV_AUTOTEST (groundUnder, altitudeStick)

float dist3(const cXyz& a, const cXyz& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ---------------------------------------------------------------- Vektoren (ohne SDK-Abhaengigkeit)

struct V3 {
    float x;
    float y;
    float z;
};

V3 v3add(V3 a, V3 b) { return V3{a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 v3mul(V3 a, float s) { return V3{a.x * s, a.y * s, a.z * s}; }
float v3dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float v3len(V3 a) { return std::sqrt(v3dot(a, a)); }
V3 v3cross(V3 a, V3 b) { return V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

// Einheitsvektor. Bei (fast) Nulllaenge der Ersatzwert.
V3 v3unit(V3 a, V3 fallback) {
    const float l = v3len(a);
    return l > 1.0e-6f ? v3mul(a, 1.0f / l) : fallback;
}

// Winkel zweier Einheitsvektoren in Grad.
float v3ang(V3 a, V3 b) {
    return std::acos(clampf(v3dot(a, b), -1.0f, 1.0f)) * kRad2Deg;
}

V3 fromX(const cXyz& c) { return V3{c.x, c.y, c.z}; }
cXyz toX(V3 v) { return cXyz(v.x, v.y, v.z); }

// Lage des Vogels wie gezeichnet: M = Ry(Gier) Rx(Nick) Rz(Rolle), wie daKago_c::mtx_set (ZXYrotM).
// f = Nase, u = Oben mit voller Rolle. Die Rechte ist r = u x f.
struct BirdFrame {
    V3 f;
    V3 u;
};

BirdFrame birdFrame(const csXyz& a) {
    const float sp = cM_ssin(a.x);
    const float cp = cM_scos(a.x);
    const float sy = cM_ssin(a.y);
    const float cy = cM_scos(a.y);
    const float sr = cM_ssin(a.z);
    const float cr = cM_scos(a.z);
    BirdFrame b;
    b.f = V3{cp * sy, -sp, cp * cy};
    b.u = V3{-sr * cy + cr * sp * sy, cr * cp, sr * sy + cr * sp * cy};
    return b;
}

// Oben des Vogels ohne Rolle (nur Nick und Gier), in derselben Lage wie birdFrame().
V3 upNoRoll(const csXyz& a) {
    const float sp = cM_ssin(a.x);
    const float cp = cM_scos(a.x);
    const float sy = cM_ssin(a.y);
    const float cy = cM_scos(a.y);
    return V3{sp * sy, cp, sp * cy};
}

// Oben der Ansicht: Mischung aus dem Oben ohne Rolle (unr) und dem vollen Oben des Vogels (bu) mit Gewicht w (0 bis 0,5).
// Beide liegen in der Ebene senkrecht zur Nase, die Mischung bleibt also senkrecht dazu. Sie ist stetig ueber die
// Rolle: bei +180 und -180 Grad gleich. Der frueher verwendete Winkel w * Rolle sprang am Umschlag der s16-Rolle um
// 126 Grad (Befund Standardlauf: Oben-Schritt 61 Grad, drei upJumps). Fuer w unter 0,5 ist die Summe nie null.
V3 upWithRoll(V3 unr, V3 bu, float w) {
    return v3unit(v3add(v3mul(unr, 1.0f - w), v3mul(bu, w)), unr);
}

// Richtung vom Vogel zur Kamera im Vogel-Bezugssystem: rel um das Oben, elev ueber seiner Ebene (wie bisher).
V3 orbitDir(V3 fS, V3 rS, V3 uS, float relDeg, float elevDeg) {
    const float re = relDeg * kDeg2Rad;
    const float ee = elevDeg * kDeg2Rad;
    const float ce = std::cos(ee);
    return v3add(v3mul(fS, -std::cos(re) * ce), v3add(v3mul(rS, -std::sin(re) * ce), v3mul(uS, std::sin(ee))));
}

// K22: freie Strecke vom Vogel in Richtung d, hoechstens dist. Trifft die Linie eine Wand, zaehlt der Abstand bis dahin
// minus kWallMargin (wie die Wandgrenze). Ohne Treffer: dist.
float freeRun(cXyz pc, V3 d, float dist) {
    const cXyz end(pc.x + d.x * dist, pc.y + d.y * dist, pc.z + d.z * dist);
    dBgS_LinChk lin;
    lin.Set(&pc, &end, nullptr);
    if (dComIfG_Bgsp().LineCross(&lin)) {
        return std::max(0.0f, dist3(lin.GetCross(), pc) - kWallMargin);
    }
    return dist;
}

// ---------------------------------------------------------------- Zustand (nur Spielthread)

struct CamOrbit {
    bool valid = false;                 // gehoert zum aktuellen Ritt
    fpc_ProcID birdId = kff::kNoProc;   // Vogel, zu dem der Orbit gehoert
    float rel = 0.0f;                   // Grad um den Vogel, 0 = hinter dem Vogel, ohne Anschlag
    float elev = kElevDefault;          // Grad ueber der Ebene des Vogels
    float dist = kDistDefault;          // Wunschabstand (Steuerkreuz)
    float distCur = kDistDefault;       // Abstand nach Wand und Glaettung
    uint32_t idleX = 0;                 // Ticks ohne C-Stick links/rechts
    uint32_t idleY = 0;                 // Ticks ohne C-Stick hoch/runter
    bool snap = false;                  // X: Schwenk hinter den Vogel laeuft
    bool frameInit = false;             // Ausrichtung ist aus dem Vogel initialisiert
    V3 fS{0.0f, 0.0f, 1.0f};            // geglaettete Nase des Vogels
    V3 uS{0.0f, 1.0f, 0.0f};            // geglaettetes Oben des Vogels (Oben der Kamera)
    float fovSm = 0.0f;                 // Sichtfeld-Zusatz, geglaettet (Grad)
    float fovApplied = 0.0f;            // Zusatz, der im letzten Tick in state->fovy steckt (Grad)
    float avoid = 0.0f;                 // K22: Ausweichwinkel (Grad), wenn eine Wand direkt hinter dem Vogel den Blick blockiert; sonst 0
};
CamOrbit g_orbit;

// Ausrichtung nachfuehren (nur die Richtung, nie die Position): Nase und Oben gehen je Tick um kOrientFollow auf
// das Ziel zu und werden danach senkrecht zueinander gehalten. Ein Sprung entsteht nur bei der Initialisierung.
void followFrame(CamOrbit& o, V3 tf, V3 tu) {
    if (!o.frameInit) {
        o.fS = tf;
        o.uS = tu;
        o.frameInit = true;
        return;
    }
    o.fS = v3unit(v3add(o.fS, v3mul(v3add(tf, v3mul(o.fS, -1.0f)), kOrientFollow)), tf);
    const V3 u = v3add(o.uS, v3mul(v3add(tu, v3mul(o.uS, -1.0f)), kOrientFollow));
    o.uS = v3unit(v3add(u, v3mul(o.fS, -v3dot(o.fS, u))), tu);
}

// Laufender Zustand des Operators (Uebergaenge, Telemetrie).
struct CamTelemetry {
    bool operating = false;             // die eigene Kamera hat den letzten Lauf geliefert
    uint32_t blend = 0;                 // Ticks seit dem Start des Uebergangs
    V3 upFromGame{0.0f, 1.0f, 0.0f};    // Oben der Spielkamera beim Start des Uebergangs
    bool upSet = false;                 // mUp wurde vom Operator gesetzt
    bool haveUp = false;                // prevUp und prevBirdUp sind gueltig
    V3 prevUp{0.0f, 1.0f, 0.0f};
    V3 prevBirdUp{0.0f, 1.0f, 0.0f};
    bool havePrev = false;              // prevOff gueltig
    cXyz prevOff{};                     // Auge minus Blickpunkt, letzter Tick
    float prevDx = 0.0f;                // Schritt des Versatzes, letzter Tick
    float prevDy = 0.0f;
    float prevDz = 0.0f;
    float lastOffStep = 0.0f;
    float lastUpStep = 0.0f;
    float lastBirdStep = 0.0f;
    float lastBehind = 1.0f;            // Kosinus Blick-Hinten zu Nase, 1 = genau hinter dem Vogel
    float lastUpY = 1.0f;               // Welt-Y des Oben, negativ = kopfueber
    float lastBirdOff = 0.0f;           // Winkel Vogel zur Bildmitte, Grad
    float lastGap = 0.0f;               // Auge ueber dem Boden, Einheiten
    float lastPitch = 0.0f;             // Grad, gezeichneter Nick
    float lastRoll = 0.0f;              // Grad, gezeichnete Rolle
    int lastHit = 0;
    int lastGround = 0;
    cXyz lastEye{};
    cXyz lastCenter{};
};
CamTelemetry g_tel;

struct CamStats {
    uint32_t calls = 0;          // Operator-Aufrufe
    uint32_t used = 0;           // davon mit eigener Kamera
    uint32_t rides = 0;          // Starts der eigenen Kamera (ON)
    uint32_t handovers = 0;      // Rueckgaben an die Spielkamera (OFF)
    uint32_t quickStarts = 0;    // QuickStart bei der Rueckgabe
    uint32_t stopHeals = 0;      // Stop-Heilung im Ritt
    uint32_t wallHits = 0;
    uint32_t groundClamps = 0;
    uint32_t groundNone = 0;     // kein Boden unter dem Auge gefunden
    uint32_t retracts = 0;
    uint32_t lookHits = 0;       // Vorausblick: Wand zwischen Wunschabstand und kLookAhead, der Abstand folgt ihr
    uint32_t hardClamps = 0;     // harte Wandgrenze kappte den Abstand (Rueckzug allein reichte nicht)
    uint32_t wallFix = 0;        // Wandpruefung nach der Bodenhebung: Auge zurueck vor die Wand gesetzt
    uint32_t stopSkips = 0;      // K22: im Ritt abgefangene Stopps der Spielkamera
    uint32_t stopPass = 0;       // K22: durchgelassene Stopps (kein Ritt, Ereignis/Demo, Testschalter)
    uint32_t avoidCuts = 0;      // K22: Ausweichschwenks (Blick war hinter dem Vogel unter kAvoidTouch an einer Wand)
    uint32_t avoidTicks = 0;     // K22: Ticks mit Ausweichwinkel
    float avoidMaxDeg = 0.0f;    // K22: groesster Ausweichwinkel (Grad)
    uint32_t throughWall = 0;    // Test: Sichtlinie Vogel zum Auge blockiert (Auge hinter der Wand)
    uint32_t stalls = 0;         // Anflug: Stillstand am Wegpunkt (Wand)
    uint32_t snaps = 0;          // X-Schwenks
    uint32_t loopsDone = 0;      // Schleifen-Schritte des Tests, die ihr Ziel erreicht haben
    uint32_t jitterSteps = 0;
    uint32_t zigzag = 0;
    uint32_t upJumps = 0;        // Oben springt mehr als der Vogel plus kUpJumpMarginDeg
    uint32_t flips = 0;          // Oben dreht sich in einem Tick um mehr als 90 Grad
    float maxOffStep = 0.0f;
    float maxUpStepDeg = 0.0f;
    float maxBirdOffDeg = 0.0f;
    float maxFovStep = 0.0f;     // groesste Aenderung des Sichtfeld-Zusatzes je Tick (soll <= 0,5 sein)
    float maxDistStep = 0.0f;    // groesste Aenderung des Abstands je Tick (soll <= kRetractMax sein, sonst Sprung)
    float fovMax = 0.0f;
    float eyeGapMin = 1.0e9f;    // kleinster Abstand Auge zu Boden, wo ein Boden gefunden wurde
    float relMin = 0.0f;
    float relMax = 0.0f;
    float elevMin = kElevDefault;
    float elevMax = kElevDefault;
    float distMin = kDistDefault;
    float distMax = kDistDefault;
    float loopBehindMin = 1.0f;  // in Schleifen: kleinster Kosinus Blick-Hinten zu Nase
    float loopUpYMin = 1.0f;     // in Schleifen: kleinstes Welt-Y des Oben (kopfueber = negativ)
    float loopRelDev = 0.0f;     // in Schleifen: groesste Abweichung des Winkels vom Schleifenbeginn, Grad
    // Griff (Mensch-Link im Ritt, nur Telemetrie): Lage von Vogel und Link im Bild als Winkel zur Blickachse (Grad, + = oben
    // bzw. rechts). Aus = ausserhalb des halben Sichtfelds (vertikal fovy/2, horizontal fuer 4:3). Nur nach dem Uebergang.
    // Die SUMMARY-Zeile "griff" erscheint nur nach einem Ritt im Griff.
    uint32_t menschRides = 0;        // Ritte im Griff (Mensch-Link)
    uint32_t menschTicks = 0;
    uint32_t menschLinkOut = 0;      // Fuesse oder Kopf von Link ausserhalb des Bildes
    uint32_t menschBirdOut = 0;      // Vogelmitte oder Vogelruecken ausserhalb des Bildes
    float menschLinkVMin = 1.0e9f;   // tiefster Punkt von Link (Fuesse), Grad
    float menschBirdVMax = -1.0e9f;  // hoechster Punkt des Vogels (Ruecken), Grad
    float menschHAbsMax = 0.0f;      // groesste seitliche Lage von Vogel oder Link, Grad
    float menschHalfV = 0.0f;        // halbes vertikales Sichtfeld (letzter Wert), Grad
    float menschOffMin = 1.0e9f;     // Laenge des Versatzes Vogel -> Link, Einheiten
    float menschOffMax = 0.0f;
    float menschDistMin = 1.0e9f;    // Abstand nach Wand und Glaettung im Griff-Ritt
    float menschDistMax = 0.0f;
};
CamStats g_stats;

uint32_t g_logSnap = 0;
uint32_t g_logSprung = 0;        // Oben springt im eingeschwungenen Zustand um mehr als 12 Grad je Tick
uint32_t g_logRuck = 0;          // RUCK: Kamera-Sprung je Tick, ratenbegrenzt im Log
uint32_t g_stopFrames = 0;       // Ticks in Folge mit stehender Spielkamera im Ritt
uint32_t g_logAbstand = 0;       // geloggte Abstandssprunge (mehr als kRetractMax je Tick)
uint32_t g_logAusweich = 0;      // K22: geloggte Ausweichschwenks
#if defined(KFF_DEV_AUTOTEST)
// Schritt des Auges je Tick (nur Testbuild, nur Lesen): Auge(t) minus Auge(t-1), Grenze 130 Einheiten. Start = Ritt-Frame 0 bis 60,
// eingeschwungen = ab Frame 61. Zaehlt ueber alle Ritte (Wolf und Griff) der Sitzung; die SUMMARY SCHRITT nennt Frame und Ritt
// des groessten Schritts.
constexpr float kEyeJumpTest = 130.0f;
cXyz g_probeEye{};                 // Auge, letzter Tick (ausgegebene Position)
bool g_probeEyeHave = false;
float g_probeEyeMaxStart = 0.0f;   // groesster Schritt, Ritt-Frame 0 bis 60
uint32_t g_probeEyeFStart = 0;     // dessen Ritt-Frame
uint32_t g_probeEyeRideStart = 0;  // dessen Ritt (Nummer)
uint32_t g_probeEyeOverStart = 0;  // Schritte ueber 130 in Frame 0 bis 60
float g_probeEyeMaxSet = 0.0f;     // groesster Schritt ab Frame 61
uint32_t g_probeEyeFSet = 0;
uint32_t g_probeEyeRideSet = 0;
uint32_t g_probeEyeOverSet = 0;    // Schritte ueber 130 ab Frame 61
#endif
bool g_operatorOn = false;       // eigener Operator registriert (install), bis onShutdown
uint32_t g_logHookErrors = 0;    // gedrosselte Zeile bei einer Ausnahme in PadRead- oder CameraStop-Hook
#if defined(KFF_CORE_CAMERA_STOP_HOOK)
uint32_t g_logStopAbfang = 0;    // K22: geloggte abgefangene Stopps
uint32_t g_logStopPass = 0;      // K22: geloggte durchgelassene Stopps (Original laeuft)

// K22: Stopp durchgelassen (kein Ritt, Ereignis/Demo, Testschalter). Zaehler fuer die SUMMARY, die ersten sechs im Log.
void logStopPass(const char* why) {
    ++g_stats.stopPass;
    if (g_logStopPass < 6) {
        ++g_logStopPass;
        kff::logInfo("kamera: STOP-DURCHLASS (K22) grund=%s ritt=%d f=%u", why, kff::isRiding() ? 1 : 0,
            static_cast<unsigned>(kff::session().rideFrames));
    }
}
#endif

CameraOperatorHandle g_handle = 0;

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------- Test-Zustand (Szenario "kamera", nur Testbuild)

// Schalter aus der Umgebung beim Laden (nur fuer Tests).
bool g_wandLauf = false;     // KFF_KAMERA_LAUF=wand: Boden- und Wandlauf
bool g_wallContact = false;  // Testlauf wand: Kamera-Treffer nahe der Bergwand (x=kWallX), beendet den Anflug
bool g_testLow = false;      // KFF_KAMERA_TESTLOW=1: Elevation bis -70 im Testszenario
uint32_t g_logHitDiag = 0;   // VORLAUF K22: geloggte Diagnose-Treffer (TREFFER-DIAG), nur Testbuild
uint32_t g_logUebergang = 0; // VORLAUF K22: geloggte Uebergangsticks mit Auge unter dem Boden, nur Testbuild
bool g_testNoStopHook = false;  // VORLAUF K22: KFF_KAMERA_STOPHOOK=0 nimmt den Stop-Pre-Hook zurueck (Test der 30-Tick-Heilung)
int g_testZoomDir = 0;          // WP8 (nur Test): Zoom-Richtung des letzten Ticks (+1 naeher, -1 weiter, 0 keiner)
uint32_t g_logZoom = 0;         // WP8 (nur Test): geloggte Zoom-Wechsel
uint32_t g_logSnapDone = 0;     // WP8 (nur Test): geloggte Enden des X-Schwenks

enum class CamStepKind : uint8_t {
    Hold,      // nur warten (linker Stick wie angegeben)
    Sweep,     // C-Stick (cx, cy) fuer frames Ticks
    Seek,      // C-Stick regelt den Relativwinkel auf target, endet nach 20 Ticks im Bereich
    Zoom,      // Steuerkreuz rechts (+1) oder links (-1) fuer frames Ticks
    Loop,      // linker Stick fy, bis der Nick sich um target Grad geaendert hat (oder frames Ticks)
    TapX,      // X, eine Flanke
    TapY,      // Y, eine Flanke (Fassrolle)
    Shot,      // Marke ohne Dauer (Screenshot)
    Anflug,    // Test: Flug auf den Wegpunkt, C-Stick regelt die Kamera auf target
    Dismount,  // B, ohne Dauer
    StopCam,   // Test: dCamera_c::Stop() im Ritt, die Stop-Heilung muss die Kamera wieder freigeben
    Done,      // Ende
};

struct CamStep {
    CamStepKind kind;
    uint32_t frames;   // Dauer in Ticks (0 bei Marken)
    float cx;          // Sweep: C-Stick X
    float cy;          // Sweep: C-Stick Y
    float fx;          // linker Stick X (Flug)
    float fy;          // linker Stick Y (Flug), negativ = steigen (Nase hoch)
    float target;      // Seek: Ziel-Relativwinkel; Loop: Nickaenderung in Grad (negativ = Nase hoch)
    int zoom;          // Zoom: +1 naeher, -1 weiter
    const char* name;  // Marke, Log und Schrittname
    float wx = 0.0f;   // Anflug: Wegpunkt (x, y, z)
    float wy = 0.0f;
    float wz = 0.0f;
};

// Zeitplan in Ticks (30 je Sekunde). Steigen, Orbit, Seite, Neigung, Abstand, X, zwei Loopings bis kopfueber und
// zurueck, Fassrolle, 360-Grad-Kurve, Absteigen per B. Ein Loop-Schritt misst den Nick ab seinem Beginn. Die Schritte
// -60, -120, -150, -30 ergeben eine volle Schleife (-360).
constexpr CamStep kTimeline[] = {
    //  kind                 frames     cx      cy      fx      fy    target  zoom  name
    {CamStepKind::Hold,         150,  0.0f,   0.0f,   0.0f, -0.6f,    0.0f,   0, "steigen"},
    {CamStepKind::Hold,          40,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "geradeaus"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "01-hinter-dem-vogel"},
    {CamStepKind::Sweep,        260, -1.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "orbit-links"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "02-orbit-links"},
    {CamStepKind::Seek,         400,  0.0f,   0.0f,   0.0f,  0.0f,  -90.0f,   0, "seek-seite-a"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "03-seite-a-rel-m90"},
    {CamStepKind::Seek,         400,  0.0f,   0.0f,   0.0f,  0.0f, -180.0f,   0, "seek-vorne"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "04-vorne-rel-m180"},
    {CamStepKind::Sweep,        180,  1.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "orbit-rechts"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "05-seite-b"},
    {CamStepKind::Sweep,        120,  0.0f,   1.0f,   0.0f,  0.0f,    0.0f,   0, "neigung-hoch"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "06-neigung-max"},
    {CamStepKind::Sweep,        190,  0.0f,  -1.0f,   0.0f,  0.0f,    0.0f,   0, "neigung-runter"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "07-neigung-min"},
    {CamStepKind::Zoom,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,  -1, "abstand-weiter"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "08-abstand-weit"},
    {CamStepKind::Zoom,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   1, "abstand-naeher"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "09-abstand-nah"},
    {CamStepKind::Seek,         400,  0.0f,   0.0f,   0.0f,  0.0f,   90.0f,   0, "seek-x-vorher"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "10-x-vorher"},
    {CamStepKind::TapX,           1,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "x-tipp"},
    {CamStepKind::Hold,          40,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "x-einschwenken"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "11-x-nachher"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f,  -60.0f,   0, "loop1-einstieg"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "11-loop1-einstieg"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f, -120.0f,   0, "loop1-scheitel"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "12-loop1-scheitel-kopf-unten"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f, -150.0f,   0, "loop1-ausstieg"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "13-loop1-ausstieg"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f,  -30.0f,   0, "loop1-ende"},
    {CamStepKind::Hold,          40,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "eben"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "14-nach-loop1"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f, -180.0f,   0, "loop2-scheitel"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "15-loop2-scheitel"},
    {CamStepKind::Loop,         260,  0.0f,   0.0f,   0.0f, -1.0f, -180.0f,   0, "loop2-ende"},
    {CamStepKind::Hold,          40,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "eben2"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "16-nach-loop2"},
    {CamStepKind::TapY,           1,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "fass-tipp"},
    {CamStepKind::Hold,          32,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "fass-halb"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "17-fassrolle-mitte"},
    {CamStepKind::Hold,          50,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "fass-ende"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "18-nach-fassrolle"},
    {CamStepKind::StopCam,       60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "stop-test"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "18b-nach-stop"},
    {CamStepKind::Hold,         140,  0.0f,   0.0f,  -1.0f,  0.0f,    0.0f,   0, "kurve-360"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "19-kurve-360"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "eben3"},
    {CamStepKind::Dismount,       0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "absteigen-b"},
    {CamStepKind::Hold,         150,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "nach-absteigen"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "20-spielkamera"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Done,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "ende"},
};
constexpr size_t kStepCount = sizeof(kTimeline) / sizeof(kTimeline[0]);

// Testlauf "wand" (KFF_KAMERA_LAUF=wand): Vogel auf der Flughoehe kWandAlt halten (alle Schritte ausser Anflug und
// Absteigen), Kamera bis -70 Grad (TESTLOW) am Boden pruefen, dann Anflug auf die Bergwand bei x=kWallX, z=-23950
// (Wegpunkt jenseits der Wand) in derselben Hoehe. Kamera vorne (target 180), damit die Wand die Kamera zieht.
// Stillstand des Vogels = Wandkontakt. Marken: ANFLUG-POS (alle 60 Ticks), WAND-TREFFER (erster Kontakt),
// WAND-NAHE (erster Kontakt innerhalb 1500 Einheiten der Bergwand).
constexpr float kWandAlt = -2600.0f;  // Testlauf wand: Flughoehe (Welt-y), knapp ueber dem Gelaende der Startzone
constexpr float kWallX = -81410.0f;   // Testlauf wand: Ebene der Bergwand (x), Bezug fuer den Log
constexpr CamStep kWandTimeline[] = {
    //  kind                 frames     cx      cy      fx      fy    target  zoom  name                     wx         wy        wz
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "tief-sinken"},
    {CamStepKind::Sweep,        120,  0.0f,  -1.0f,   0.0f,  0.0f,    0.0f,   0, "neigung-tief"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "boden-tief"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "01-boden-tief"},
    {CamStepKind::Sweep,        120,  0.0f,   1.0f,   0.0f,  0.0f,    0.0f,   0, "neigung-zurueck"},
    {CamStepKind::Hold,          30,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "02-neigung-zurueck"},
    {CamStepKind::Anflug,       450,  0.0f,   0.0f,   0.0f,  0.0f,  180.0f,   0, "anflug-a",               -85000.0f, 3000.0f, -23950.0f},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "03-anflug-vorne"},
    {CamStepKind::Anflug,      1200,  0.0f,   0.0f,   0.0f,  0.0f,  180.0f,   0, "anflug-b",               -85000.0f, 3000.0f, -23950.0f},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "04-wand-kontakt"},
    {CamStepKind::Hold,          90,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "wand-halten"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "05-wand-halten"},
    {CamStepKind::Dismount,       0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "absteigen-b"},
    {CamStepKind::Hold,         150,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "nach-absteigen"},
    {CamStepKind::Shot,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "06-spielkamera"},
    {CamStepKind::Hold,          60,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "halten"},
    {CamStepKind::Done,           0,  0.0f,   0.0f,   0.0f,  0.0f,    0.0f,   0, "ende"},
};
constexpr size_t kWandCount = sizeof(kWandTimeline) / sizeof(kWandTimeline[0]);

struct TestRunState {
    bool armed = false;            // Ruf eingeleitet
    uint32_t callStart = 0;        // Tick des Rufs
    bool rideSeen = false;         // Ritt erkannt, Zeitplan laeuft
    bool dismounted = false;       // B gedrueckt
    size_t step = 0;               // aktueller Schritt
    uint32_t stepFrame = 0;        // Ticks im aktuellen Schritt
    uint32_t seekStable = 0;       // Ticks im Bereich des Seek-Ziels
    const char* stepName = "-";
    bool finished = false;
    const CamStep* steps = nullptr;  // Zeitplan dieses Laufs
    size_t count = 0;                // Anzahl Schritte des Zeitplans
    cXyz anchor{};                   // Anflug: Ort vor 60 Ticks
    bool stalled = false;            // Anflug: Stillstand erkannt
    bool inLoop = false;             // der aktuelle Schritt ist ein Loop-Schritt
    float loopRel0 = 0.0f;           // Relativwinkel zu Beginn des Loop-Schritts, Grad
    bool pitchInit = false;          // Nick des Vogels gelesen
    int32_t pitchPrev = 0;           // Nick des Vogels im letzten Tick (s16, roh)
    int32_t pitchCum = 0;            // Summe der Nickschritte seit Ritt-Start (s16-Einheiten)
    int32_t stepCumBase = 0;         // pitchCum zu Beginn des Schritts
};
TestRunState g_run;
#endif  // KFF_DEV_AUTOTEST (Test-Zustand)

// Name des Testschritts fuer die Telemetrie. Ohne Testbuild gibt es keinen Zeitplan.
const char* stepLabel() {
#if defined(KFF_DEV_AUTOTEST)
    return g_run.stepName;
#else
    return "-";
#endif
}

// ---------------------------------------------------------------- Kamera-Operator-Helfer

// Der Vogel, solange Link im Ritt sitzt. Sonst nullptr.
daKago_c* freeBird() {
    if (!kff::isRiding()) {
        return nullptr;
    }
    daKago_c* bird = kff::liveBird();
    daAlink_c* link = kff::liveLink();
    if (bird == nullptr || link == nullptr || !link->checkCargoCarry()) {
        return nullptr;
    }
    return bird;
}

// Ereignis oder Demo-Kamera hat die Kamera. Dann bleibt das Ereignis zustaendig.
bool eventOrDemo() {
    return dComIfGp_event_runCheck() != 0 || dDemo_c::getCamera() != nullptr;
}


// Neuer Start der eigenen Kamera. Der Orbit bleibt bei Unterbrechung desselben Vogels erhalten, die Ausrichtung wird immer neu
// aus dem Vogel genommen. Wolf und Mensch-Link im Griff nehmen dieselben Werte: Startabstand kamera_abstand, Standardneigung.
// kamera_abstand_mensch wirkt nicht mehr (der Schluessel bleibt lesbar, siehe README).
void startOperating(daKago_c* bird, dCamera_c* cam) {
    const fpc_ProcID id = fopAcM_GetID(bird);
    if (!g_orbit.valid || g_orbit.birdId != id) {
        g_orbit = CamOrbit{};
        g_orbit.valid = true;
        g_orbit.birdId = id;
        g_orbit.dist = static_cast<float>(kff::config::getFloat(kff::config::Key::KameraAbstand));
        g_orbit.distCur = g_orbit.dist;
        g_orbit.elev = kElevDefault;
        if (kff::session().rideGrip) {
            ++g_stats.menschRides;  // Telemetrie: Ritte mit Mensch-Link im Griff
        }
    }
    g_orbit.frameInit = false;
    g_tel.upFromGame = v3unit(fromX(cam->mUp), V3{0.0f, 1.0f, 0.0f});
    g_tel.operating = true;
    g_tel.blend = 0;
    g_tel.haveUp = false;
    g_tel.havePrev = false;
#if defined(KFF_DEV_AUTOTEST)
    g_probeEyeHave = false;  // Protokoll: jeder Ritt beginnt ohne Vortick
#endif
    ++g_stats.rides;
    kff::logInfo("kamera: ON bird=0x%x griff=%d rel=%.1f elev=%.1f dist=%.0f uebergang=%u Ticks",
        static_cast<unsigned>(id), kff::session().rideGrip ? 1 : 0, g_orbit.rel, g_orbit.elev, g_orbit.dist,
        static_cast<unsigned>(kBlendFrames));
}

// Rueckgabe an die Spielkamera. Nur beim Wechsel von der eigenen Kamera. Der Sichtfeld-Zusatz wird aus der
// Spielkamera herausgerechnet, das Oben auf Welt-Oben gesetzt. Ist der Zustand eingefroren (mCurState != 0) und
// kein Ereignis aktiv, setzt QuickStart ihn zurueck.
void handOver(dCamera_c* cam, bool gameCamera) {
    if (!g_tel.operating) {
        return;
    }
    g_tel.operating = false;
    ++g_stats.handovers;
    bool quick = false;
    if (cam != nullptr) {
        if (g_orbit.fovApplied != 0.0f) {
            cam->mFovy -= g_orbit.fovApplied;
            cam->mViewCache.mFovy -= g_orbit.fovApplied;
        }
        if (g_tel.upSet) {
            cam->mUp = cXyz(0.0f, 1.0f, 0.0f);
        }
        if (!gameCamera && cam->mCurState == kCamStateStop) {
            cam->QuickStart();
            quick = true;
            ++g_stats.quickStarts;
        }
    }
    g_orbit.fovApplied = 0.0f;
    g_orbit.fovSm = 0.0f;
    g_tel.upSet = false;
    kff::logInfo("kamera: OFF (Spielkamera) ereignis=%d quickStart=%d", gameCamera ? 1 : 0, quick ? 1 : 0);
}

// Stop-Heilung (jeder PadRead-Tick, auch ausserhalb des Tests): dCamera_c::Stop() setzt mCurState != 0, danach
// ruft camera_execute den Operator nicht mehr auf. Steht die Kamera im Ritt ohne Ereignis kStopHealFrames lang
// still, setzt QuickStart sie zurueck. Der Hilfsweg ersetzt nicht den Hook auf Stop() im Kern (core_patches).
void healStoppedCamera() {
    if (!kff::isRiding()) {
        g_stopFrames = 0;
        return;
    }
    dCamera_c* cam = dCam_getBody();
    if (cam == nullptr || cam->mCurState != kCamStateStop || eventOrDemo()) {
        g_stopFrames = 0;
        return;
    }
    if (++g_stopFrames < kStopHealFrames) {
        return;
    }
    g_stopFrames = 0;
    cam->QuickStart();
    ++g_stats.stopHeals;
    kff::logInfo("kamera: STOP-HEILUNG f=%u (Spielkamera stand ohne Ereignis, QuickStart)",
        static_cast<unsigned>(kff::session().rideFrames));
}

#if defined(KFF_DEV_AUTOTEST)
// Schritt des Zeitplans in die Marke schreiben (Screenshot durch den Test-Lauf).
void logShot(const CamStep& st) {
    const kff::Session& s = kff::session();
    kff::logInfo("kamera: SHOT %s f=%u rel=%.1f elev=%.1f dist=%.0f distCur=%.0f nick=%.1f rolle=%.1f "
                 "behind=%.3f upY=%.3f birdOff=%.2f fov=%.2f eye=(%.0f,%.0f,%.0f) hit=%d gnd=%d",
        st.name, static_cast<unsigned>(s.rideFrames), g_orbit.rel, g_orbit.elev, g_orbit.dist, g_orbit.distCur,
        g_tel.lastPitch, g_tel.lastRoll, g_tel.lastBehind, g_tel.lastUpY, g_tel.lastBirdOff, g_orbit.fovApplied,
        g_tel.lastEye.x, g_tel.lastEye.y, g_tel.lastEye.z, g_tel.lastHit, g_tel.lastGround);
}

void advanceStep() {
    ++g_run.step;
    g_run.stepFrame = 0;
    g_run.seekStable = 0;
    g_run.stepCumBase = g_run.pitchCum;
    if (g_run.step < g_run.count) {
        g_run.stepName = g_run.steps[g_run.step].name;
        kff::logInfo("kamera: STEP %u %s f=%u", static_cast<unsigned>(g_run.step), g_run.stepName,
            static_cast<unsigned>(kff::session().rideFrames));
    } else {
        g_run.stepName = "ende";
    }
}

// Test beenden: der Kern gibt danach keine Eingaben des Szenarios mehr aus.
bool finishScenario() {
    g_run.finished = true;
    g_run.stepName = "-";
    g_run.inLoop = false;
    return true;
}
#endif  // KFF_DEV_AUTOTEST (Testablauf)

// Untere Grenze der Elevation: im Spiel -5, im Test mit KFF_KAMERA_TESTLOW=1 bis -70 (Boden pruefen).
float elevMinEff() {
#if defined(KFF_DEV_AUTOTEST)
    if (g_testLow && g_run.rideSeen && !g_run.finished) {
        return kElevTestMin;
    }
#endif
    return kElevMin;
}

// Griff (Telemetrie): Lage eines Punkts zur Blickachse in Grad (av vertikal, + = oben; ah horizontal).
void viewAngles(const cXyz& eye, V3 fwd, V3 up, V3 right, const cXyz& p, float& av, float& ah) {
    const V3 v{p.x - eye.x, p.y - eye.y, p.z - eye.z};
    const float z = std::max(1.0f, v3dot(v, fwd));
    av = std::atan2(v3dot(v, up), z) * kRad2Deg;
    ah = std::atan2(v3dot(v, right), z) * kRad2Deg;
}

// ---------------------------------------------------------------- Kamera-Operator

// Operator fuer CameraService, laeuft in dCamera_c::Run() vor der Spielkamera. true = eigene Kamera fuer diesen
// Frame, false = Spielkamera. Der Vogel ist die Richtschnur: Lage, Blickpunkt und Auge kommen aus seinem Rahmen.
bool cameraOperate(ModContext*, CameraOperatorState* state, void*) {
    ++g_stats.calls;
    dCamera_c* cam = dCam_getBody();
    daKago_c* bird = freeBird();
    const bool gameCamera = eventOrDemo();
    if (cam == nullptr || bird == nullptr || gameCamera) {
        handOver(cam, gameCamera);
        return false;
    }
    if (!g_tel.operating) {
        startOperating(bird, cam);
    }
    ++g_stats.used;

    // Spielkamera-Zustand, den der Lauf der Spielkamera sonst pflegt: kein Letterbox.
    cam->SetTrimSize(0);
    cam->mTrimHeight += -cam->mTrimHeight * 0.25f;
    // Oben und Ereignis-Pause: Bit 0x10 liesse Up() den Override lesen, Bit 0x1 laesst view_setup alles verwerfen.
    cam->clrFlag(0x11);

    CamOrbit& o = g_orbit;

    // Eingaben aus dem Schnappschuss des Kerns (kff::input(), vor dem Konsum): C-Stick (Orbit, Neigung), Steuerkreuz
    // rechts/links (Abstand), X (hinter den Vogel). Steuerkreuz und X verbraucht der Kern im Ritt sonst selbst.
    const kff::Input& in = kff::input();
    const float sx = stickAxis(in.cStickX);
    const float sy = stickAxis(in.cStickY);
    const bool zoomIn = kff::act(kff::Act::CameraCloser).held();    // Vorgabe Steuerkreuz rechts (actions.cpp)
    const bool zoomOut = kff::act(kff::Act::CameraFarther).held();  // Vorgabe Steuerkreuz links
    const bool snapNow = kff::act(kff::Act::CameraBehind).pressed();  // Vorgabe X
    // Schweben (Entwurf H1): groesserer Neigungsbereich, kein Rueckschwenk. Nur im Ritt gueltig (ein stehengebliebenes
    // Feld aus einem frueheren Ritt wirkt nicht).
    const bool hover = kff::session().hover && kff::isRiding();

    if (sx != 0.0f) {
        o.rel += sx * kYawRateDeg;
        o.idleX = 0;
    } else if (o.idleX < 0xFFFFFFFFu) {
        ++o.idleX;
    }
    if (sy != 0.0f) {
        o.elev = clampf(o.elev + sy * kPitchRateDeg, hover ? kHoverElevMin : elevMinEff(), hover ? kHoverElevMax : kElevMax);
        o.idleY = 0;
    } else if (o.idleY < 0xFFFFFFFFu) {
        ++o.idleY;
    }
    if (!hover) {
        // Nach dem Schweben: die Neigung geht mit kElevRelax je Tick auf den Normalbereich zurueck.
        if (o.elev > kElevMax) {
            o.elev = std::max(kElevMax, o.elev - kElevRelax);
        } else if (o.elev < elevMinEff()) {
            o.elev = std::min(elevMinEff(), o.elev + kElevRelax);
        }
    }
#if defined(KFF_DEV_AUTOTEST)
    if (hover) {  // Test: Neigung je Tick im Schweben (Pruefkriterium 5)
        kff::logInfo("kamera: SCHWEBEN-T rf=%u rel=%.3f elev=%.3f stickY=%.2f", kff::session().rideFrames,
            static_cast<double>(o.rel), static_cast<double>(o.elev), static_cast<double>(sy));
    }
#endif  // KFF_DEV_AUTOTEST (Neigungs-Trace)
    const float elevDef = kElevDefault;  // Standardneigung, Wolf und Griff gleich
    if (zoomIn) {
        o.dist = std::max(kDistMin, o.dist - kDistKeyRate);
    }
    if (zoomOut) {
        o.dist = std::min(kDistMax, o.dist + kDistKeyRate);
    }
#if defined(KFF_DEV_AUTOTEST)
    // WP8.8/WP8.11 (nur Test): Beginn und Ende eines Steuerkreuz-Zooms im Log.
    {
        const int zoomDir = zoomIn ? 1 : (zoomOut ? -1 : 0);
        if (zoomDir != g_testZoomDir && g_logZoom < 12) {
            ++g_logZoom;
            kff::logInfo("kamera: ZOOM %s dist=%.0f f=%u",
                zoomDir > 0 ? "naeher Beginn" : (zoomDir < 0 ? "weiter Beginn" : "Ende"), static_cast<double>(o.dist),
                static_cast<unsigned>(kff::session().rideFrames));
        }
        g_testZoomDir = zoomDir;
    }
#endif
    if (snapNow) {
        o.snap = true;
        ++g_stats.snaps;
        if (g_logSnap < 6) {
            ++g_logSnap;
            kff::logInfo("kamera: X-Schwenk rel=%.1f elev=%.1f f=%u", o.rel, o.elev,
                static_cast<unsigned>(kff::session().rideFrames));
        }
    }
    if (o.snap) {
        if (sx != 0.0f || sy != 0.0f) {
            o.snap = false;  // der C-Stick hat Vorrang
        } else {
            // Gleichmaessiger Schwenk hinter den Vogel: hoechstens kSnapRate Grad je Tick, die Neigung zurueck auf die Standardhoehe.
            o.rel += clampf(-wrap180(o.rel), -kSnapRate, kSnapRate);
            o.elev += clampf(elevDef - o.elev, -kSnapElevRate, kSnapElevRate);
            if (std::fabs(wrap180(o.rel)) < kSnapDoneDeg && std::fabs(o.elev - elevDef) < kSnapDoneDeg) {
                o.rel = 0.0f;
                o.elev = elevDef;
                o.snap = false;
#if defined(KFF_DEV_AUTOTEST)
                if (g_logSnapDone < 6) {  // WP8.11 (nur Test): X-Schwenk ist hinter dem Vogel angekommen
                    ++g_logSnapDone;
                    kff::logInfo("kamera: X-Schwenk fertig rel=0.0 elev=%.1f f=%u", static_cast<double>(o.elev),
                        static_cast<unsigned>(kff::session().rideFrames));
                }
#endif
            }
        }
    }
    // Rueckschwenk hinter den Vogel nach einer Pause. Der Winkel wird zuerst auf den kurzen Weg gebracht.
    if (o.idleX >= kIdleFrames && !hover) {
        o.rel = wrap180(o.rel) * kReturnKeep;
    }
    if (o.idleY >= kIdleFrames && !hover) {
        o.elev = elevDef + (o.elev - elevDef) * kReturnKeep;
    }
#if defined(KFF_DEV_AUTOTEST)
    // Testbuild: Blickwinkel im Schweben alle 30 Ticks (Entwurf 6.2, Kriterium 5: Neigung 70, keine Aenderung im Stand).
    if (hover && kff::session().rideFrames % 30 == 0) {
        kff::logInfo("kamera: SCHWEBEN f=%u rel=%.2f elev=%.2f", kff::session().rideFrames, static_cast<double>(o.rel),
            static_cast<double>(o.elev));
    }
#endif

    // Sichtfeld-Zusatz aus der Sitzung (schreibt tempo). Hoechstens kFovMax, je Tick hoechstens kFovStep.
    const float fovTarget = clampf(kff::session().fovExtraDeg, 0.0f, kFovMax);
    const float fovPrev = o.fovSm;
    o.fovSm += clampf(fovTarget - o.fovSm, -kFovStep, kFovStep);
    const float fovStep = std::fabs(o.fovSm - fovPrev);
    if (fovStep > g_stats.maxFovStep) {
        g_stats.maxFovStep = fovStep;
    }
    if (o.fovSm > g_stats.fovMax) {
        g_stats.fovMax = o.fovSm;
    }
    const float fovBase = state->fovy - o.fovApplied;  // Spielwert ohne unseren Zusatz des letzten Ticks
    const float fovOut = fovBase + o.fovSm;
    o.fovApplied = o.fovSm;

    // Lage des Vogels, geglaettet (Nase und Oben), Oben mit Rollanteil.
    const BirdFrame bf = birdFrame(bird->shape_angle);
    const V3 tu = upWithRoll(upNoRoll(bird->shape_angle), bf.u, kRollGain);
    followFrame(o, bf.f, tu);
    const V3 fS = o.fS;
    const V3 uS = o.uS;
    const V3 rS = v3cross(uS, fS);  // rechts des Vogels

    // Vogelposition, ohne Verzoegerung. Der Blickpunkt haengt starr an ihm.
    cXyz pc(bird->current.pos.x, bird->current.pos.y, bird->current.pos.z);
    // Drehpunkt der Kamera ist der Vogel selbst, im Wolf- wie im Griff-Ritt. Link haengt im Griff unter dem Vogel und liegt
    // dadurch unten im Bild (Telemetrie GRIFF). Kein eigener Drehpunkt mehr.
    cXyz pv = pc;
    if (kff::session().rideGrip) {
        pv.y -= kMenschAimDrop;  // Griff-Ritt: Blick etwas tiefer, damit Links Fuesse im Bild bleiben (Wolf-Ritt ohne Aenderung)
    }

    // K22 Ausweichen: Liegt eine Wand so dicht hinter dem Vogel, dass die gewuenschte Richtung unter kAvoidTouch frei ist
    // (z. B. beim Start an einer Wand), schwenkt der Blick sofort zur naechsten freien Richtung (Hineinfahren sofort). Zurueck
    // zur gewuenschten Richtung geglaettet, und nur solange sie frei ist. Die Wandgrenze weiter unten gilt unabhaengig davon.
    if (freeRun(pv, orbitDir(fS, rS, uS, o.rel + o.avoid, o.elev), o.dist) < kAvoidTouch) {
        const float pref = o.avoid < 0.0f ? -1.0f : 1.0f;  // zuerst die Seite des bisherigen Ausweichens
        bool found = false;
        float off = 0.0f;
        for (int k = 1; k <= kAvoidSteps && !found; ++k) {
            for (int s = 0; s < 2 && !found; ++s) {
                const float cand = (s == 0 ? pref : -pref) * static_cast<float>(k) * kAvoidStepDeg;
                if (freeRun(pv, orbitDir(fS, rS, uS, o.rel + cand, o.elev), o.dist) >= kAvoidOk) {
                    off = cand;
                    found = true;
                }
            }
        }
        if (found && off != o.avoid) {
            o.avoid = off;
            ++g_stats.avoidCuts;
            if (g_logAusweich < 6) {
                ++g_logAusweich;
                kff::logInfo("kamera: AUSWEICHEN f=%u rel=%.1f ausweich=%.1f",
                    static_cast<unsigned>(kff::session().rideFrames), static_cast<double>(o.rel),
                    static_cast<double>(o.avoid));
            }
        }
    } else if (o.avoid != 0.0f && freeRun(pv, orbitDir(fS, rS, uS, o.rel, o.elev), o.dist) >= kAvoidOk) {
        // Zurueckschwenken: je Tick hoechstens kAvoidRelease, nur solange der Blick in der Zwischenstellung frei bleibt.
        const float cand = o.avoid - clampf(o.avoid, -kAvoidRelease, kAvoidRelease);
        if (freeRun(pv, orbitDir(fS, rS, uS, o.rel + cand, o.elev), o.dist) >= kAvoidTouch) {
            o.avoid = cand;
        }
    }
    if (o.avoid != 0.0f) {
        ++g_stats.avoidTicks;
        if (std::fabs(o.avoid) > g_stats.avoidMaxDeg) {
            g_stats.avoidMaxDeg = std::fabs(o.avoid);
        }
    }
    // Richtung vom Vogel zur Kamera im Vogel-Bezugssystem, mit dem Ausweichwinkel (ohne Ausweichen: die gewuenschte Richtung).
    const V3 dir = orbitDir(fS, rS, uS, o.rel + o.avoid, o.elev);

    cXyz center(pv.x + uS.x * kCenterUp, pv.y + uS.y * kCenterUp, pv.z + uS.z * kCenterUp);

    // Wand: Vorausblick. Die Linie vom Vogel reicht kLookAhead ueber den Wunschabstand, damit eine Wand schon vor dem
    // Auge gesehen wird. Die Wandgrenze (Treffer minus kWallMargin) ist eine harte Schranke: das Auge liegt nie hinter
    // der Wand. Liegt sie naeher als der Abstand, springt er sofort auf sie (Abschnitt Abstand unten).
    const float reach = o.dist + kLookAhead;
    cXyz wanted(pv.x + dir.x * reach, pv.y + dir.y * reach, pv.z + dir.z * reach);
    float wallLimit = kNoWallLimit;
    int hit = 0;
    dBgS_LinChk lin;
    lin.Set(&pv, &wanted, nullptr);
    if (dComIfG_Bgsp().LineCross(&lin)) {
        const cXyz hitPos = lin.GetCross();
        const float hitDist = dist3(hitPos, pv);
#if defined(KFF_DEV_AUTOTEST)
        // VORLAUF K22 Diagnose (nur Testbuild): Normale der getroffenen Flaeche und Boden unter dem Vogel, erste Treffer.
        if (g_logHitDiag < 6u && hitDist <= o.dist) {
            ++g_logHitDiag;
            cM3dGPla diagPla;
            dComIfG_Bgsp().GetTriPla(lin, &diagPla);
            const float diagNl = std::sqrt(diagPla.mNormal.x * diagPla.mNormal.x + diagPla.mNormal.y * diagPla.mNormal.y +
                                           diagPla.mNormal.z * diagPla.mNormal.z);
            const float diagNy = diagNl > 1.0e-6f ? diagPla.mNormal.y / diagNl : 0.0f;
            float diagGnd = -1.0f;
            groundUnder(pc, diagGnd);
            kff::logInfo("kamera: TREFFER-DIAG f=%u dist=%.1f ny=%.2f cross=(%.0f,%.0f,%.0f) bird=(%.0f,%.0f,%.0f) "
                         "nick=%.1f gndBird=%.0f dir=(%.2f,%.2f,%.2f)",
                static_cast<unsigned>(kff::session().rideFrames), static_cast<double>(hitDist),
                static_cast<double>(diagNy), hitPos.x, hitPos.y, hitPos.z, pc.x, pc.y, pc.z,
                static_cast<double>(static_cast<float>(bird->shape_angle.x) * kDegPerS16), static_cast<double>(diagGnd),
                static_cast<double>(dir.x), static_cast<double>(dir.y), static_cast<double>(dir.z));
        }
#endif
        wallLimit = std::max(0.0f, hitDist - kWallMargin);
        if (hitDist <= o.dist) {
            hit = 1;
            ++g_stats.wallHits;
        } else {
            ++g_stats.lookHits;  // Wand im Vorausblick, noch nicht im Wunschabstand
        }
#if defined(KFF_DEV_AUTOTEST)
        // Testlauf wand: erster Kontakt und Kontakt nahe der Bergwand (x=kWallX) fuer das Szenario.
        if (g_stats.wallHits == 1u) {
            kff::logInfo("kamera: WAND-TREFFER erster Kontakt f=%u bird=(%.0f,%.0f,%.0f) Abstand=%.0f dWand=%.0f",
                static_cast<unsigned>(kff::session().rideFrames), pc.x, pc.y, pc.z, hitDist, pc.x - kWallX);
        }
        if (std::fabs(pc.x - kWallX) < 1500.0f) {
            g_wallContact = true;
        }
        static bool wallNearLogged = false;
        if (!wallNearLogged && std::fabs(pc.x - kWallX) < 1500.0f) {
            wallNearLogged = true;
            kff::logInfo("kamera: WAND-NAHE Bergwand f=%u bird=(%.0f,%.0f,%.0f) Abstand=%.0f dWand=%.0f",
                static_cast<unsigned>(kff::session().rideFrames), pc.x, pc.y, pc.z, hitDist, pc.x - kWallX);
        }
#endif
    }

    // Abstand: Ziel ist der Wunschabstand, hoechstens bis zur Wandgrenze. Liegt die Wandgrenze naeher als der Abstand,
    // gilt sie sofort (harte Grenze); mehr als kRetractMax in einem Tick heisst: die Wand war schneller als der Vorausblick
    // (Zaehler hardClamps, Log ABSTANDSSPRUNG). Sonst sinkt der Abstand zum Ziel hin je Tick um hoechstens kRetractMax und
    // waechst nach aussen um hoechstens kDistGrowRate.
    const float distPrev = o.distCur;
    const float target = std::min(o.dist, wallLimit);
    if (wallLimit < o.distCur) {
        // Wand: sofort auf die Wandgrenze, ohne Glaettung. Geglaettet wird nur das Herausfahren (unten).
        o.distCur = wallLimit;
        if (distPrev - wallLimit > kRetractMax) {
            ++g_stats.hardClamps;  // die Wand zog weiter zurueck, als der Rueckzug je Tick erlaubt haette
        }
    } else if (target < o.distCur) {
        o.distCur = std::max(target, o.distCur - kRetractMax);
    } else {
        o.distCur = std::min(target, o.distCur + kDistGrowRate);
    }
    const float distStep = std::fabs(o.distCur - distPrev);
    if (distStep > g_stats.maxDistStep) {
        g_stats.maxDistStep = distStep;
    }
    if (distPrev - o.distCur > kRetractStep) {
        ++g_stats.retracts;
    }
    if (distStep > kRetractMax + 0.5f && g_logAbstand < 6) {
        ++g_logAbstand;
        kff::logInfo("kamera: ABSTANDSSPRUNG f=%u distAlt=%.0f distNeu=%.0f wandgrenze=%.0f hit=%d",
            static_cast<unsigned>(kff::session().rideFrames), distPrev, o.distCur, wallLimit, hit);
    }
    cXyz eye(pv.x + dir.x * o.distCur, pv.y + dir.y * o.distCur, pv.z + dir.z * o.distCur);

    // Boden: das Auge bleibt kEyeClearance ueber dem Boden unter ihm.
    int ground = 0;
    dBgS_GndChk gnd;
    gnd.SetPos(&eye);
    const f32 groundY = dComIfG_Bgsp().GroundCross(&gnd);
    const bool groundFound = groundY > -1.0e8f;
    if (groundFound && eye.y < groundY + kEyeClearance) {
        eye.y = groundY + kEyeClearance;
        ground = 1;
        ++g_stats.groundClamps;
    }
    if (!groundFound) {
        ++g_stats.groundNone;
    }
    // Letzte Wandpruefung nach der Bodenhebung: das Auge liegt nie hinter einer Wand. Der Abstand wird ohne Glaettung
    // auf den Trefferpunkt minus Abstand gesetzt.
    lin.Set(&pv, &eye, nullptr);
    if (dComIfG_Bgsp().LineCross(&lin)) {
        const float hitDistEye = dist3(lin.GetCross(), pv);
        o.distCur = std::max(0.0f, std::min(o.distCur, hitDistEye - kWallMargin));
        eye = cXyz(pv.x + dir.x * o.distCur, pv.y + dir.y * o.distCur, pv.z + dir.z * o.distCur);
        ++g_stats.wallFix;
    }
#if defined(KFF_DEV_AUTOTEST)
    // Test: Auge hinter einer Wand (Sichtlinie vom Drehpunkt zum Auge blockiert). Erwartet nur bei einer Wand naeher als
    // kWallFloor, denn erst dort laesst die Untergrenze des Abstands das Auge hinter die Wand.
    lin.Set(&pv, &eye, nullptr);
    if (dComIfG_Bgsp().LineCross(&lin)) {
        ++g_stats.throughWall;
        if (g_stats.throughWall == 1u) {
            kff::logInfo("kamera: DURCH-WAND erster Fall f=%u abstand=%.0f wandgrenze=%.0f",
                static_cast<unsigned>(kff::session().rideFrames), o.distCur, wallLimit);
        }
    }
#endif

    // Uebergang von der Spielkamera: Auge und Blickpunkt linear, Oben von dem der Spielkamera zu unserem.
    cXyz outEye = eye;
    cXyz outCenter = center;
    V3 upOut = uS;
    // Boden unter dem gezeigten Auge: bei Uebergang unter dem gemischten Auge, sonst unter dem Auge oben.
    f32 groundShown = groundY;
    bool groundShownFound = groundFound;
    if (g_tel.blend < kBlendFrames) {
        const float b = static_cast<float>(g_tel.blend) / static_cast<float>(kBlendFrames);
        // Ausgangspunkt des Uebergangs: die laufende Spielkamera, wie beim Wolf-Ritt.
        const cXyz fromE(state->eye[0], state->eye[1], state->eye[2]);
        const cXyz fromC(state->center[0], state->center[1], state->center[2]);
        outEye = cXyz(fromE.x + (eye.x - fromE.x) * b, fromE.y + (eye.y - fromE.y) * b, fromE.z + (eye.z - fromE.z) * b);
        outCenter = cXyz(fromC.x + (center.x - fromC.x) * b, fromC.y + (center.y - fromC.y) * b,
            fromC.z + (center.z - fromC.z) * b);
        upOut = v3unit(v3add(v3mul(g_tel.upFromGame, 1.0f - b), v3mul(uS, b)), uS);
        // Das gemischte Auge bleibt wie das Auge oben kEyeClearance ueber dem Boden unter ihm. Ohne diese Hebung lag es
        // in den ersten Uebergangsticks unter der Flaeche, wenn das Ziel nahe einer Wand hoeher liegt als der Startpunkt.
        dBgS_GndChk gndShown;
        gndShown.SetPos(&outEye);
        groundShown = dComIfG_Bgsp().GroundCross(&gndShown);
        groundShownFound = groundShown > -1.0e8f;
        if (groundShownFound && outEye.y < groundShown + kEyeClearance) {
#if defined(KFF_DEV_AUTOTEST)
            if (g_logUebergang < 6u) {
                ++g_logUebergang;
                kff::logInfo("kamera: UEBERGANG-UNTER f=%u b=%.2f augeStart=%.0f augeZiel=%.0f augeMisch=%.0f boden=%.0f",
                    static_cast<unsigned>(kff::session().rideFrames), b, state->eye[1], eye.y, outEye.y, groundShown);
            }
#endif
            outEye.y = groundShown + kEyeClearance;
        }
    }
    const bool settled = g_tel.blend >= kBlendFrames;
    if (!settled) {
        ++g_tel.blend;
    }

    // view_setup verwirft die Aktualisierung, wenn Auge und Blickpunkt in x und z gleich sind.
    if (std::fabs(outEye.x - outCenter.x) < 1.0e-3f && std::fabs(outEye.z - outCenter.z) < 1.0e-3f) {
        outEye.x += 0.01f;
    }

    // Ausgabe. Das Oben steht direkt in mUp, der Operator-Pfad von Run() schreibt es nicht neu.
    cam->mUp = toX(upOut);
    g_tel.upSet = true;
    state->eye[0] = outEye.x;
    state->eye[1] = outEye.y;
    state->eye[2] = outEye.z;
    state->center[0] = outCenter.x;
    state->center[1] = outCenter.y;
    state->center[2] = outCenter.z;
    state->fovy = fovOut;
    state->bank_degrees = 0.0f;  // Schraeglage laeuft ueber das Oben, nicht ueber Bank

    // Telemetrie: Blickrichtung, Oben, Abstand des Vogels zur Bildmitte, Schritte.
    const V3 viewDir = v3unit(V3{outCenter.x - outEye.x, outCenter.y - outEye.y, outCenter.z - outEye.z}, fS);
    const V3 toBird = V3{pc.x - outEye.x, pc.y - outEye.y, pc.z - outEye.z};
    const float birdOff = v3ang(viewDir, v3unit(toBird, fS));
    const V3 fromBird = V3{outEye.x - pc.x, outEye.y - pc.y, outEye.z - pc.z};
    const float behind = v3dot(v3unit(fromBird, fS), v3mul(fS, -1.0f));
    if (g_tel.haveUp) {
        const float upStep = v3ang(upOut, g_tel.prevUp);
        const float birdStep = v3ang(tu, g_tel.prevBirdUp);
        g_tel.lastUpStep = upStep;
        g_tel.lastBirdStep = birdStep;
        if (settled) {
            if (upStep > g_stats.maxUpStepDeg) {
                g_stats.maxUpStepDeg = upStep;
            }
            if (upStep > birdStep + kUpJumpMarginDeg) {
                ++g_stats.upJumps;
            }
            if (v3dot(upOut, g_tel.prevUp) < 0.0f) {
                ++g_stats.flips;
            }
            if (upStep > 12.0f) {
                kff::logTelemetry(g_logSprung, 8, 300,
                    "kamera: SPRUNG oben upStep=%.1f birdStep=%.1f f=%u step=%s roll=%.1f nick=%.1f",
                    static_cast<double>(upStep), static_cast<double>(birdStep),
                    static_cast<unsigned>(kff::session().rideFrames), stepLabel(),
                    static_cast<double>(static_cast<float>(bird->shape_angle.z) * kDegPerS16),
                    static_cast<double>(static_cast<float>(bird->shape_angle.x) * kDegPerS16));
            }
        }
    }
    g_tel.prevUp = upOut;
    g_tel.prevBirdUp = tu;
    g_tel.haveUp = true;
    if (settled && birdOff > g_stats.maxBirdOffDeg) {
        g_stats.maxBirdOffDeg = birdOff;
    }

    // Ruck- und Zittermessung: Bewegung des Auges relativ zum Blickpunkt, ohne die Bewegung des Vogels.
    const cXyz off(outEye.x - outCenter.x, outEye.y - outCenter.y, outEye.z - outCenter.z);
    if (g_tel.havePrev) {
        const float dx = off.x - g_tel.prevOff.x;
        const float dy = off.y - g_tel.prevOff.y;
        const float dz = off.z - g_tel.prevOff.z;
        const float stepOff = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float pl = std::sqrt(g_tel.prevDx * g_tel.prevDx + g_tel.prevDy * g_tel.prevDy +
                                   g_tel.prevDz * g_tel.prevDz);
        g_tel.lastOffStep = stepOff;
        if (settled) {
            if (stepOff > g_stats.maxOffStep) {
                g_stats.maxOffStep = stepOff;
            }
            const float dot = dx * g_tel.prevDx + dy * g_tel.prevDy + dz * g_tel.prevDz;
            if (stepOff > kZigStep && pl > kZigStep && dot < -0.5f * stepOff * pl) {
                ++g_stats.zigzag;
            }
            if (stepOff > kJitterStep) {
                ++g_stats.jitterSteps;
            }
            if (stepOff > 100.0f) {
                kff::logTelemetry(g_logRuck, 8, 300,
                    "kamera: RUCK f=%u step=%s off=%.1f distCur=%.0f hit=%d gnd=%d eye=(%.0f,%.0f,%.0f) bird=(%.0f,%.0f,%.0f)",
                    static_cast<unsigned>(kff::session().rideFrames), stepLabel(), stepOff, o.distCur, hit, ground,
                    outEye.x, outEye.y, outEye.z, pc.x, pc.y, pc.z);
            }
        }
        g_tel.prevDx = dx;
        g_tel.prevDy = dy;
        g_tel.prevDz = dz;
    }
    g_tel.prevOff = off;
    g_tel.havePrev = true;

    g_tel.lastBehind = behind;
    g_tel.lastUpY = upOut.y;
    g_tel.lastBirdOff = birdOff;
    g_tel.lastPitch = static_cast<float>(bird->shape_angle.x) * kDegPerS16;
    g_tel.lastRoll = static_cast<float>(bird->shape_angle.z) * kDegPerS16;
    g_tel.lastHit = hit;
    g_tel.lastGround = ground;
    g_tel.lastEye = outEye;
    g_tel.lastCenter = outCenter;
    if (groundShownFound) {
        const float gap = outEye.y - groundShown;
        g_tel.lastGap = gap;
        if (gap < g_stats.eyeGapMin) {
            g_stats.eyeGapMin = gap;
        }
    }

#if defined(KFF_DEV_AUTOTEST)
    // Testlauf: In Schleifen bleibt hinten hinten, das Oben darf nicht kippen, der Winkel bleibt konstant.
    if (g_run.inLoop && settled) {
        g_stats.loopBehindMin = std::min(g_stats.loopBehindMin, behind);
        g_stats.loopUpYMin = std::min(g_stats.loopUpYMin, upOut.y);
        const float dev = std::fabs(wrap180(o.rel - g_run.loopRel0));
        if (dev > g_stats.loopRelDev) {
            g_stats.loopRelDev = dev;
        }
    }
#endif

    if (o.rel < g_stats.relMin) {
        g_stats.relMin = o.rel;
    }
    if (o.rel > g_stats.relMax) {
        g_stats.relMax = o.rel;
    }
    if (o.elev < g_stats.elevMin) {
        g_stats.elevMin = o.elev;
    }
    if (o.elev > g_stats.elevMax) {
        g_stats.elevMax = o.elev;
    }
    if (o.distCur < g_stats.distMin) {
        g_stats.distMin = o.distCur;
    }
    if (o.distCur > g_stats.distMax) {
        g_stats.distMax = o.distCur;
    }

    // Griff (Mensch-Link im Ritt, nur Telemetrie): Lage von Vogel (Mitte, Ruecken) und Link (Fuesse, Kopf) zur Blickachse in
    // Grad. Der Kopf liegt kMenschLinkHead ueber den Fuessen in Richtung Vogel (Link haengt am Griff). Gezaehlt wird nach dem
    // Uebergang. Im Wolf-Ritt liegt kein Link am Griff, dort bleibt diese Telemetrie aus.
    if (kff::session().rideGrip && settled) {
        constexpr float kMenschBirdBack = 60.0f;   // Vogelruecken ueber der Vogelposition (Schaetzung, in Richtung Oben)
        constexpr float kMenschLinkHead = 150.0f;  // Kopf von Link ueber seiner Wurzel (Schaetzung, Richtung Griff)
        const V3 upV = v3unit(v3add(upOut, v3mul(viewDir, -v3dot(viewDir, upOut))), uS);
        const V3 rightV = v3cross(viewDir, upV);
        const float halfV = 0.5f * fovOut;
        const float halfH = std::atan(std::tan(halfV * kDeg2Rad) * (4.0f / 3.0f)) * kRad2Deg;
        float birdV = 0.0f;
        float birdH = 0.0f;
        float backV = 0.0f;
        float backH = 0.0f;
        viewAngles(outEye, viewDir, upV, rightV, pc, birdV, birdH);
        const cXyz back(pc.x + uS.x * kMenschBirdBack, pc.y + uS.y * kMenschBirdBack, pc.z + uS.z * kMenschBirdBack);
        viewAngles(outEye, viewDir, upV, rightV, back, backV, backH);
        float feetV = 0.0f;
        float feetH = 0.0f;
        float headV = 0.0f;
        float headH = 0.0f;
        const daAlink_c* mLink = kff::liveLink();
        if (mLink != nullptr) {
            const cXyz feet(mLink->current.pos.x, mLink->current.pos.y, mLink->current.pos.z);
            const V3 toGrip = v3unit(V3{pc.x - feet.x, pc.y - feet.y, pc.z - feet.z}, V3{0.0f, 1.0f, 0.0f});
            const cXyz head(feet.x + toGrip.x * kMenschLinkHead, feet.y + toGrip.y * kMenschLinkHead,
                feet.z + toGrip.z * kMenschLinkHead);
            viewAngles(outEye, viewDir, upV, rightV, feet, feetV, feetH);
            viewAngles(outEye, viewDir, upV, rightV, head, headV, headH);
            const float offLen = dist3(feet, pc);
            g_stats.menschOffMin = std::min(g_stats.menschOffMin, offLen);
            g_stats.menschOffMax = std::max(g_stats.menschOffMax, offLen);
        }
        ++g_stats.menschTicks;
        g_stats.menschHalfV = halfV;
        if (std::fabs(birdV) > halfV || std::fabs(backV) > halfV || std::fabs(birdH) > halfH || std::fabs(backH) > halfH) {
            ++g_stats.menschBirdOut;
        }
        g_stats.menschBirdVMax = std::max(g_stats.menschBirdVMax, std::max(birdV, backV));
        g_stats.menschHAbsMax = std::max(g_stats.menschHAbsMax, std::max(std::fabs(birdH), std::fabs(backH)));
        if (mLink != nullptr) {
            if (std::fabs(feetV) > halfV || std::fabs(headV) > halfV || std::fabs(feetH) > halfH || std::fabs(headH) > halfH) {
                ++g_stats.menschLinkOut;
            }
            g_stats.menschLinkVMin = std::min(g_stats.menschLinkVMin, std::min(feetV, headV));
            g_stats.menschHAbsMax = std::max(g_stats.menschHAbsMax, std::max(std::fabs(feetH), std::fabs(headH)));
        }
        g_stats.menschDistMin = std::min(g_stats.menschDistMin, o.distCur);
        g_stats.menschDistMax = std::max(g_stats.menschDistMax, o.distCur);
#if defined(KFF_DEV_AUTOTEST)
        if (g_stats.calls % kLogEvery == 0) {
            kff::logInfo("kamera: GRIFF f=%u dist=%.0f distCur=%.0f elev=%.1f vogelV=%.1f rueckenV=%.1f kopfV=%.1f fussV=%.1f "
                         "vogelH=%.1f fussH=%.1f halbV=%.1f halbH=%.1f",
                static_cast<unsigned>(kff::session().rideFrames), static_cast<double>(o.dist),
                static_cast<double>(o.distCur), static_cast<double>(o.elev), static_cast<double>(birdV),
                static_cast<double>(backV), static_cast<double>(headV), static_cast<double>(feetV),
                static_cast<double>(birdH), static_cast<double>(feetH), static_cast<double>(halfV),
                static_cast<double>(halfH));
        }
#endif
    }


#if defined(KFF_DEV_AUTOTEST)
    // Test: Schritt des Auges je Tick (nur Lesen, verhaltensneutral) fuer Wolf und Griff. Start = Ritt-Frame 0 bis 60, eingeschwungen
    // ab 61. Zeile je 10 Ticks (PARITAET): Abstand, Neigung, Drehung, Schritt des Auges, Oben-Schritt.
    {
        const uint32_t fr = kff::session().rideFrames;
        const float eyeStep = g_probeEyeHave ? dist3(outEye, g_probeEye) : 0.0f;
        if (g_probeEyeHave) {
            if (fr <= 60u) {
                if (eyeStep > g_probeEyeMaxStart) {
                    g_probeEyeMaxStart = eyeStep;
                    g_probeEyeFStart = fr;
                    g_probeEyeRideStart = g_stats.rides;
                }
                if (eyeStep > kEyeJumpTest) {
                    ++g_probeEyeOverStart;
                }
            } else {
                if (eyeStep > g_probeEyeMaxSet) {
                    g_probeEyeMaxSet = eyeStep;
                    g_probeEyeFSet = fr;
                    g_probeEyeRideSet = g_stats.rides;
                }
                if (eyeStep > kEyeJumpTest) {
                    ++g_probeEyeOverSet;
                }
            }
        }
        g_probeEye = outEye;
        g_probeEyeHave = true;
        if (fr % 10u == 0u) {
            kff::logInfo("kamera: PARITAET f=%u griff=%d step=%s dist=%.0f distCur=%.0f elev=%.1f rel=%.1f ausweich=%.1f "
                         "augeStep=%.1f upStep=%.2f hit=%d gnd=%d",
                static_cast<unsigned>(fr), kff::session().rideGrip ? 1 : 0, stepLabel(), static_cast<double>(o.dist),
                static_cast<double>(o.distCur), static_cast<double>(o.elev), static_cast<double>(o.rel),
                static_cast<double>(o.avoid), static_cast<double>(eyeStep), static_cast<double>(g_tel.lastUpStep), hit,
                ground);
        }
    }
#endif
#if defined(KFF_DEV_AUTOTEST)
    if (g_stats.calls % kLogEvery == 0) {
        kff::logInfo("kamera: f=%u step=%s bird=(%.0f,%.0f,%.0f) nick=%.1f gier=%.1f rolle=%.1f rel=%.1f elev=%.1f "
                     "dist=%.0f distCur=%.0f hit=%d gnd=%d behind=%.3f upY=%.3f",
            static_cast<unsigned>(kff::session().rideFrames), stepLabel(), pc.x, pc.y, pc.z, g_tel.lastPitch,
            static_cast<float>(bird->current.angle.y) * kDegPerS16, g_tel.lastRoll, o.rel, o.elev, o.dist,
            o.distCur, hit, ground, behind, upOut.y);
        kff::logInfo("kamera: eye=(%.0f,%.0f,%.0f) ctr=(%.0f,%.0f,%.0f) stick=(%.2f,%.2f) dpad=%c%c offStep=%.2f "
                     "upStep=%.2f birdStep=%.2f birdOff=%.2f idleX=%u idleY=%u blend=%u fov=%.2f gap=%.1f",
            outEye.x, outEye.y, outEye.z, outCenter.x, outCenter.y, outCenter.z, sx, sy, zoomIn ? 'R' : '-',
            zoomOut ? 'L' : '-', g_tel.lastOffStep, g_tel.lastUpStep, g_tel.lastBirdStep, birdOff,
            static_cast<unsigned>(o.idleX), static_cast<unsigned>(o.idleY), static_cast<unsigned>(g_tel.blend),
            fovOut, g_tel.lastGap);
    }
#endif
    return true;
}

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------- Test-Szenario "kamera" (nur Testbuild)

// Ruf wie im Szenario zyklus (R halten, B tippen), dann der Zeitplan. C-Stick, Steuerkreuz, X und Y setzt kameraTick
// ueber autotest::Context. Hier kommen die Marken, der linke Stick (Flug) und B.
// Testlauf wand, nur Protokoll: waagerechte Linien quer durch die Ebene der Bergwand (x=kWallX, z=-23950) bei
// verschiedenen Hoehen und Linien entlang z bei x=kWallX. Zeigt, auf welcher Hoehe die Wand Geometrie hat.
void probeWall() {
    static const float kProbeY[] = {-3600.0f, -3300.0f, -3000.0f, -2800.0f, -2700.0f, -2500.0f, -2000.0f,
                                    -1000.0f, 0.0f, 1000.0f, 3000.0f};
    for (float py : kProbeY) {
        cXyz a(kWallX + 600.0f, py, -23950.0f);
        cXyz b(kWallX - 600.0f, py, -23950.0f);
        dBgS_LinChk lin;
        lin.Set(&a, &b, nullptr);
        if (dComIfG_Bgsp().LineCross(&lin)) {
            const cXyz h = lin.GetCross();
            kff::logInfo("kamera: PROBE x-Linie y=%.0f Treffer bei (%.0f,%.0f,%.0f)", py, h.x, h.y, h.z);
        } else {
            kff::logInfo("kamera: PROBE x-Linie y=%.0f kein Treffer", py);
        }
    }
    static const float kProbeZY[] = {-2600.0f, -1000.0f};
    for (float py : kProbeZY) {
        cXyz a(kWallX, py, -23350.0f);
        cXyz b(kWallX, py, -24550.0f);
        dBgS_LinChk lin;
        lin.Set(&a, &b, nullptr);
        if (dComIfG_Bgsp().LineCross(&lin)) {
            const cXyz h = lin.GetCross();
            kff::logInfo("kamera: PROBE z-Linie y=%.0f Treffer bei (%.0f,%.0f,%.0f)", py, h.x, h.y, h.z);
        } else {
            kff::logInfo("kamera: PROBE z-Linie y=%.0f kein Treffer", py);
        }
    }
}

// Hoehe mit Lageregler (nur Anflug): die gewuenschte Nicklage folgt der Hoehendifferenz, hoechstens 15 Grad
// (positiv = Nase runter), der Stick Y regelt die Abweichung der Nicklage. Ein Stick Y ist eine Nickrate, daher
// fuehrt ein dauerhaft voller Stick in den Looping, die Lage als Zwischenziel vermeidet das.
float altitudeStickPitch(daKago_c* b, float tAlt) {
    const float nick = static_cast<float>(b->shape_angle.x) * kDegPerS16;
    const float want = clampf((b->current.pos.y - tAlt) * 0.03f, -15.0f, 15.0f);
    return clampf((want - nick) * 0.03f, -0.25f, 0.25f);
}

// Ruf-Bedingung des Zeitplans (nur Testbuild). Wolf: wie bisher (c.canCall). Griff: Ruf als Mensch wie im Modul mensch (Link
// frei am Boden, kein Vogel, kein Ereignis, Phase Idle).
bool timelineCallOk(kff::autotest::Context& c, bool human) {
    if (!human) {
        return c.canCall();
    }
    daAlink_c* link = kff::liveLink();
    return link != nullptr && link->checkWolf() == 0 && !link->checkCargoCarry() && !link->checkEventRun() &&
           !dComIfGp_event_runCheck() && c.sess.phase == kff::Phase::Idle && c.sess.birdId == kff::kNoProc;
}

// Zeitplan kamera fuer beide Gestalten. human = true: Griff-Ritt (Link als Mensch, mensch_ritt = true).
bool kameraRideTick(kff::autotest::Context& c, bool human) {
    constexpr uint32_t kCallAt = 300;        // erster Ruf, sobald Link bereit ist
    constexpr uint32_t kCallHold = 30;       // R gehalten, Ticks
    constexpr uint32_t kCallTap = 15;        // B-Tipp in der Mitte des R-Halts
    constexpr uint32_t kRideTimeout = 1500;  // Ticks nach dem Ruf bis zum Ritt

    if (g_run.finished) {
        return true;
    }
    if (!g_run.armed && c.frame >= kCallAt && timelineCallOk(c, human)) {
        g_run.armed = true;
        g_run.callStart = c.frame;
        c.note("kamera: Ruf-Kombination (R halten, B tippen)");
    }
    if (g_run.armed && !g_run.rideSeen) {
        if (c.frame >= g_run.callStart && c.frame < g_run.callStart + kCallHold) {
            c.holdR();
        }
        if (c.frame == g_run.callStart + kCallTap) {
            c.tapB();
        }
    }

    if (!g_run.rideSeen) {
        if (c.sess.phase != kff::Phase::Riding) {
            if (g_run.armed && c.frame > g_run.callStart + kRideTimeout) {
                c.note("kamera: kein Ritt nach dem Ruf, Szenario endet");
                return finishScenario();
            }
            return false;
        }
        g_run.rideSeen = true;
        c.note("kamera: Ritt erkannt, Zeitplan startet");
        if (g_wandLauf) {
            g_run.steps = kWandTimeline;
            g_run.count = kWandCount;
        } else {
            g_run.steps = kTimeline;
            g_run.count = kStepCount;
        }
        g_run.step = 0;
        g_run.stepFrame = 0;
        g_run.stepName = g_run.steps[0].name;
        g_run.stepCumBase = g_run.pitchCum;
        kff::logInfo("kamera: STEP 0 %s f=%u", g_run.stepName, static_cast<unsigned>(c.sess.rideFrames));
    } else if (!g_run.dismounted && c.sess.phase != kff::Phase::Riding) {
        c.note("kamera: Ritt vorzeitig beendet, Szenario endet");
        return finishScenario();
    }

    // Nick des Vogels fortschreiben (Summe der Schritte, fuer die Loop-Schritte). Differenzen wrappen als s16.
    daKago_c* bird = kff::liveBird();
    if (bird != nullptr) {
        const int32_t nick = static_cast<int32_t>(static_cast<int16_t>(bird->shape_angle.x));
        if (g_run.pitchInit) {
            g_run.pitchCum += static_cast<int16_t>(nick - g_run.pitchPrev);
        }
        g_run.pitchPrev = nick;
        g_run.pitchInit = true;
    }

    // Marken ohne Dauer laufen in diesem Tick durch, dann hoechstens ein zeitlicher Schritt.
    for (;;) {
        if (g_run.step >= g_run.count) {
            c.note("timeline complete");
            return finishScenario();
        }
        const CamStep& mk = g_run.steps[g_run.step];
        if (mk.kind == CamStepKind::Done) {
            c.note("timeline complete");
            return finishScenario();
        }
        if (mk.kind == CamStepKind::Shot) {
            // Marke: am Anfang des Schritts schreiben. Danach steht die Kamera fuer kShotHold Ticks still (Pad neutral),
            // damit der Screenshot den Zustand der Marke zeigt und nicht eine weiterfliegende Schleife.
            if (g_run.stepFrame == 0) {
                logShot(mk);
            }
            break;
        }
        if (mk.kind == CamStepKind::Dismount) {
            c.tapB();
            g_run.dismounted = true;
            kff::logInfo("kamera: DISMOUNT (B)");
            advanceStep();
            continue;
        }
        break;
    }

    // Ein zeitlicher Schritt: Pad-Werte und linker Stick fuer diesen Tick.
    const CamStep& st = g_run.steps[g_run.step];
    g_run.inLoop = st.kind == CamStepKind::Loop;
    if (g_run.inLoop && g_run.stepFrame == 0) {
        g_run.loopRel0 = g_orbit.rel;
    }
    float fx = st.fx;
    float fy = st.fy;
    static bool wallProbed = false;
    if (g_wandLauf && !wallProbed) {
        wallProbed = true;
        g_wallContact = false;
        probeWall();
    }
    // Testlauf wand: Hoehe auf kWandAlt halten, sofern der Schritt keinen eigenen Flug vorgibt (Anflug, Absteigen).
    if (g_wandLauf && st.kind != CamStepKind::Anflug && st.kind != CamStepKind::Dismount) {
        if (daKago_c* hBird = kff::liveBird()) {
            fy = altitudeStick(hBird->current.pos.y, kWandAlt);
        }
    }
    float cx = 0.0f;
    float cy = 0.0f;
    int zoom = 0;
    bool tapX = false;
    bool tapY = false;
    bool done = false;
    switch (st.kind) {
    case CamStepKind::Sweep:
        cx = st.cx;
        cy = st.cy;
        break;
    case CamStepKind::Seek: {
        const float err = wrap180(st.target - g_orbit.rel);
        cx = std::fabs(err) < 1.2f ? 0.0f : clampf(err * 0.1f, -1.0f, 1.0f);
        g_run.seekStable = std::fabs(err) < 1.5f ? g_run.seekStable + 1u : 0u;
        if (g_run.seekStable >= 20u) {
            done = true;
        }
        break;
    }
    case CamStepKind::Zoom:
        zoom = st.zoom;
        break;
    case CamStepKind::Loop: {
        // Nickaenderung seit Schrittbeginn in Grad. Negativ = Nase hoch (linker Stick Y negativ).
        const float cumDeg = static_cast<float>(g_run.pitchCum - g_run.stepCumBase) * kDegPerS16;
        if ((st.target < 0.0f && cumDeg <= st.target) || (st.target > 0.0f && cumDeg >= st.target)) {
            done = true;
            ++g_stats.loopsDone;
            kff::logInfo("kamera: SCHLEIFE %s Ziel erreicht, nick=%.1f f=%u", st.name, cumDeg,
                static_cast<unsigned>(kff::session().rideFrames));
        }
        break;
    }
    case CamStepKind::TapX:
        tapX = true;
        break;
    case CamStepKind::TapY:
        tapY = true;
        break;
    case CamStepKind::Anflug: {
        // Test: Kurs und Hoehe per linkem Stick zum Wegpunkt, C-Stick regelt die Kamera auf target.
        daKago_c* aBird = kff::liveBird();
        if (aBird == nullptr) {
            done = true;
            break;
        }
        const float bx = aBird->current.pos.x;
        const float by = aBird->current.pos.y;
        const float bz = aBird->current.pos.z;
        const float bearing = std::atan2(st.wx - bx, st.wz - bz) * kRad2Deg;
        const float yawDeg = static_cast<float>(aBird->current.angle.y) * kDegPerS16;
        // Linker Stick X negativ dreht den Kurs nach Plus (Flugtest run1), linker Stick Y negativ steigt.
        fx = clampf(-wrap180(bearing - yawDeg) / 40.0f, -0.6f, 0.6f);
        // Hoehe: auf st.wy halten (Bergwand-Hoehe). Der Boden darunter steht nur im Log.
        float gy = 0.0f;
        const bool hasGround = groundUnder(cXyz(bx, by, bz), gy);
        fy = altitudeStickPitch(aBird, st.wy);
        if (g_wallContact && !done) {
            done = true;
            kff::logInfo("kamera: ANFLUG Wandkontakt %s f=%u bird=(%.0f,%.0f,%.0f) dWand=%.0f", st.name,
                static_cast<unsigned>(kff::session().rideFrames), bx, by, bz, bx - kWallX);
        }
        cx = clampf(wrap180(st.target - g_orbit.rel) * 0.1f, -1.0f, 1.0f);
        // Stillstand (Wand): nach 120 Ticks weniger als 20 Einheiten in 60 Ticks.
        const cXyz now(bx, by, bz);
        if (g_run.stepFrame == 0) {
            g_run.anchor = now;
        } else if (g_run.stepFrame % 60 == 0) {
            const float moved = dist3(now, g_run.anchor);
            kff::logInfo("kamera: ANFLUG-POS %s f=%u bird=(%.0f,%.0f,%.0f) dWand=%.0f dZiel=%.0f gy=%.0f stick=%.2f",
                st.name, static_cast<unsigned>(kff::session().rideFrames), bx, by, bz, bx - kWallX,
                dist3(now, cXyz(st.wx, st.wy, st.wz)), hasGround ? gy : -1.0f, fy);
            if (g_run.stepFrame >= 120 && moved < 20.0f) {
                g_run.stalled = true;
                ++g_stats.stalls;
                done = true;
                kff::logInfo("kamera: ANFLUG Stillstand f=%u pos=(%.0f,%.0f,%.0f) bewegt=%.1f eye=(%.0f,%.0f,%.0f)",
                    static_cast<unsigned>(kff::session().rideFrames), bx, by, bz, moved, g_tel.lastEye.x,
                    g_tel.lastEye.y, g_tel.lastEye.z);
            }
            g_run.anchor = now;
        }
        break;
    }
    case CamStepKind::StopCam:
        // Test: Spielkamera anhalten (Zustand 3). Die Stop-Heilung gibt sie nach kStopHealFrames Ticks frei.
        if (g_run.stepFrame == 0) {
            if (dCamera_c* stopCam = dCam_getBody()) {
                stopCam->Stop();
            }
            kff::logInfo("kamera: TEST-STOP dCamera_c::Stop im Ritt (Stop-Heilung erwartet)");
        }
        break;
    default:
        break;  // Hold
    }
    c.cStick(cx, cy);
    if (zoom > 0) {
        c.hold(static_cast<uint32_t>(PAD_BUTTON_RIGHT));  // Steuerkreuz rechts: naeher
    } else if (zoom < 0) {
        c.hold(static_cast<uint32_t>(PAD_BUTTON_LEFT));   // Steuerkreuz links: weiter
    }
    if (tapX) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_X));
    }
    if (tapY) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));
    }
    c.stick(fx, fy);

    ++g_run.stepFrame;
    if (st.kind == CamStepKind::Seek && !done && g_run.stepFrame >= st.frames) {
        kff::logInfo("kamera: seek %s ohne Einschwingen rel=%.1f ziel=%.1f", st.name, g_orbit.rel, st.target);
    }
    if (st.kind == CamStepKind::Loop && !done && g_run.stepFrame >= st.frames) {
        kff::logInfo("kamera: SCHLEIFE %s Zeitlimit, nick=%.1f", st.name,
            static_cast<float>(g_run.pitchCum - g_run.stepCumBase) * kDegPerS16);
    }
    if (st.kind == CamStepKind::Anflug && (done || g_run.stepFrame >= st.frames)) {
        kff::logInfo("kamera: ANFLUG Ende %s stillstand=%d f=%u rel=%.1f", st.name, g_run.stalled ? 1 : 0,
            static_cast<unsigned>(kff::session().rideFrames), g_orbit.rel);
        g_run.stalled = false;
    }
    constexpr uint32_t kShotHold = 45;  // Standbild nach einer Marke, Screenshot-Zeit (1,5 s bei 30 Ticks)
    const uint32_t need = st.kind == CamStepKind::Shot ? kShotHold : st.frames;
    if (done || g_run.stepFrame >= need) {
        advanceStep();
    }
    return false;
}

bool kameraTick(kff::autotest::Context& c) {
    return kameraRideTick(c, false);
}

const kff::autotest::Scenario kKameraScenario = {
    "kamera",
    "Kamera: Ruf, Ritt, C-Stick-Orbit, Neigung, Abstand, X-Schwenk, zwei Loopings, Fassrolle, Kurve, Absteigen",
    &kameraTick,
};

// Griff-Ritt: derselbe Zeitplan wie kamera, Link als Mensch (mensch_ritt = true, Stage F_SP103). So lassen sich Abstand,
// Neigung, Sprung des Auges und die Lage von Link im Bild direkt mit dem Wolf-Ritt vergleichen.
bool kameraMenschTick(kff::autotest::Context& c) {
    return kameraRideTick(c, true);
}

const kff::autotest::Scenario kKameraMenschScenario = {
    "kamera-mensch",
    "Griff-Ritt (Mensch-Link) mit dem Zeitplan von kamera: Abstand, Neigung, Sprung des Auges, Lage von Link im Bild",
    &kameraMenschTick,
};

// ---------------------------------------------------------------- Szenario "stick-relativ" (WP8.7, nur Testbuild)
// Entscheidung D14: der linke Stick steuert relativ zum Vogel, die Kamera folgt dem Vogel. Zeitplan in Kern-Ticks seit dem
// Ritt-Beginn (nicht in Ritt-Frames):
//   A  60..119    Stick X -1, Kamera hinter dem Vogel (rel 0): Kursaenderung dA
//   B  300..359   C-Stick X +1, Kamera 90 Grad zur Seite; 360..419 Stick X -1: Kursaenderung dB (muss dA gleichen)
//   H  420..1019  600 Ticks Stick neutral, davon 420..719 C-Stick X +0,5 (Kamera kreist): Kurs bleibt (hoechstens 10 Grad)
//   N  1040..1059 Stick Y -0,5: Nickaenderung dN. Vorzeichen: ohne Umkehr Nase hoch (dN < 0), mit lenkung_invertieren oder
//                 dem Alias invert_steer Nase runter (dN > 0); game.invertAirSwimY darf daran nichts aendern (OE8).
//   B-Tipp 1120, Ende nach dem Abflug. Ergebnis: "kamera: STICK-RELATIV ... -> BESTANDEN".
constexpr uint32_t kSrA0 = 60;
constexpr uint32_t kSrA1 = 120;
constexpr uint32_t kSrC0 = 300;
constexpr uint32_t kSrB0 = 360;
constexpr uint32_t kSrB1 = 420;
constexpr uint32_t kSrOrbit1 = 720;
constexpr uint32_t kSrH1 = 1020;
constexpr uint32_t kSrN0 = 1040;
constexpr uint32_t kSrN1 = 1060;
constexpr uint32_t kSrB = 1120;
constexpr uint32_t kSrDepart = 100;

struct SrState {
    bool armed = false;
    uint32_t callStart = 0;
    bool ride = false;
    uint32_t base = 0;
    bool havePrev = false;
    int16_t prevYaw = 0;
    int16_t prevNick = 0;
    double cumYaw = 0.0;   // Grad
    double cumNick = 0.0;  // Grad, + = Nase unten
    double yawA0 = 0.0;
    double dA = 0.0;
    double yawB0 = 0.0;
    double dB = 0.0;
    float relB = 0.0f;
    float relBMin = 1.0e9f;
    float relBMax = -1.0e9f;
    double yawH0 = 0.0;
    double holdMaxDev = 0.0;
    double holdEnd = 0.0;
    float relHoldMax = 0.0f;
    float relEnd = 0.0f;
    double nick0 = 0.0;
    double dN = 0.0;
    bool dismounted = false;
    bool done = false;
};
SrState g_sr;

bool srFinish(const char* why) {
    SrState& s = g_sr;
    if (s.done) {
        return true;
    }
    s.done = true;
    const bool inv = kff::config::getBool(kff::config::Key::InvertSteer);
    const bool turnOk = std::fabs(s.dA) >= 30.0 && std::fabs(s.dB) >= 30.0 && (s.dA > 0.0) == (s.dB > 0.0) &&
                        std::fabs(s.dA - s.dB) <= 2.0;
    const bool camSide = s.relB >= 80.0f && s.relB <= 100.0f && s.relBMin >= 60.0f;
    const bool holdOk = s.holdMaxDev <= 10.0 && s.relHoldMax >= 30.0f;
    const bool followOk = std::fabs(s.relEnd) <= 5.0f;
    const bool nickOk = std::fabs(s.dN) >= 5.0 && (inv ? s.dN > 0.0 : s.dN < 0.0);
    const bool ok = turnOk && camSide && holdOk && followOk && nickOk && s.dismounted &&
                    kff::autotest::foreignInputCount() == 0;
    kff::logInfo("kamera: STICK-RELATIV (%s) A: dA=%.2f | B: relB=%.1f relB min/max=%.1f/%.1f dB=%.2f | HALT 600: maxAbw=%.2f "
                 "ende=%.2f relMax=%.1f relEnde=%.2f | NICK: dN=%.2f lenkung_invertieren=%d | kurve=%d kameraSeite=%d halt=%d "
                 "folgt=%d nick=%d fremdeingabe=%u -> %s",
        why, s.dA, static_cast<double>(s.relB), static_cast<double>(s.relBMin), static_cast<double>(s.relBMax), s.dB,
        s.holdMaxDev, s.holdEnd, static_cast<double>(s.relHoldMax), static_cast<double>(s.relEnd), s.dN, inv ? 1 : 0,
        turnOk ? 1 : 0, camSide ? 1 : 0, holdOk ? 1 : 0, followOk ? 1 : 0, nickOk ? 1 : 0,
        static_cast<unsigned>(kff::autotest::foreignInputCount()), ok ? "BESTANDEN" : "NICHT BESTANDEN");
    return true;
}

bool stickRelTick(kff::autotest::Context& c) {
    SrState& s = g_sr;
    if (s.done) {
        return true;
    }
    constexpr uint32_t kCallAt = 300;
    constexpr uint32_t kCallHold = 30;
    constexpr uint32_t kCallTap = 15;
    if (!s.ride) {
        if (c.sess.phase == kff::Phase::Riding) {
            s.ride = true;
            s.base = c.frame;
            c.note("stick-relativ: Ritt erkannt, Zeitplan startet");
            return false;
        }
        if (!s.armed && c.frame >= kCallAt && c.canCall()) {
            s.armed = true;
            s.callStart = c.frame;
            c.note("stick-relativ: Ruf-Kombination (R halten, B tippen)");
        }
        if (s.armed) {
            if (c.frame < s.callStart + kCallHold) {
                c.holdR();
            }
            if (c.frame == s.callStart + kCallTap) {
                c.tapB();
            }
            if (c.frame > s.callStart + 1500) {
                return srFinish("kein Ritt nach dem Ruf");
            }
        }
        return false;
    }
    if (c.sess.phase == kff::Phase::Dismounting) {
        return c.sess.departFrames >= kSrDepart ? srFinish("Abstieg abgeschlossen") : false;
    }
    if (c.sess.phase != kff::Phase::Riding) {
        return srFinish(s.dismounted ? "Ritt beendet" : "Ritt ohne Abstieg beendet");
    }
    daKago_c* bird = kff::liveBird();
    if (bird == nullptr) {
        return false;
    }
    // Kurs und Nick als Summe der s16-Schritte (Wrap-sicher), in Grad.
    const int16_t yaw = static_cast<int16_t>(bird->current.angle.y);
    const int16_t nick = static_cast<int16_t>(bird->current.angle.x);
    if (s.havePrev) {
        s.cumYaw += static_cast<double>(static_cast<int16_t>(yaw - s.prevYaw)) * 360.0 / 65536.0;
        s.cumNick += static_cast<double>(static_cast<int16_t>(nick - s.prevNick)) * 360.0 / 65536.0;
    }
    s.prevYaw = yaw;
    s.prevNick = nick;
    s.havePrev = true;

    const uint32_t f = c.frame - s.base;
    const float rel = wrap180(g_orbit.rel);
    float sx = 0.0f;
    float sy = 0.0f;
    float cx = 0.0f;
    if (f == kSrA0) {
        s.yawA0 = s.cumYaw;
        kff::logInfo("kamera: STICK-RELATIV A Beginn f=%u rel=%.1f kurs=%.2f", f, static_cast<double>(rel), s.cumYaw);
    }
    if (f >= kSrA0 && f < kSrA1) {
        sx = -1.0f;
    }
    if (f == kSrA1) {
        s.dA = s.cumYaw - s.yawA0;
        kff::logInfo("kamera: STICK-RELATIV A Ende f=%u dA=%.2f rel=%.1f", f, s.dA, static_cast<double>(rel));
    }
    if (f >= kSrC0 && f < kSrB0) {
        cx = 1.0f;
    }
    if (f == kSrB0) {
        s.yawB0 = s.cumYaw;
        s.relB = rel;
        kff::logInfo("kamera: SHOT sr-kamera-seite f=%u rel=%.1f", f, static_cast<double>(rel));
    }
    if (f >= kSrB0 && f < kSrB1) {
        sx = -1.0f;
        s.relBMin = std::min(s.relBMin, rel);
        s.relBMax = std::max(s.relBMax, rel);
        if (f == kSrB0 + 30) {
            kff::logInfo("kamera: SHOT sr-kurve-seite f=%u rel=%.1f", f, static_cast<double>(rel));
        }
    }
    if (f == kSrB1) {
        s.dB = s.cumYaw - s.yawB0;
        s.yawH0 = s.cumYaw;
        kff::logInfo("kamera: STICK-RELATIV B Ende f=%u dB=%.2f rel=%.1f", f, s.dB, static_cast<double>(rel));
    }
    if (f > kSrB1 && f <= kSrH1) {
        s.holdMaxDev = std::max(s.holdMaxDev, std::fabs(s.cumYaw - s.yawH0));
        if (f < kSrOrbit1) {
            s.relHoldMax = std::max(s.relHoldMax, std::fabs(rel));
        }
    }
    if (f >= kSrB1 && f < kSrOrbit1) {
        cx = 0.5f;
    }
    if (f == kSrH1) {
        s.holdEnd = s.cumYaw - s.yawH0;
        s.relEnd = rel;
        kff::logInfo("kamera: SHOT sr-halt-ende f=%u kursAbw=%.2f maxAbw=%.2f rel=%.1f", f, s.holdEnd, s.holdMaxDev,
            static_cast<double>(rel));
    }
    if (f == kSrN0) {
        s.nick0 = s.cumNick;
    }
    if (f >= kSrN0 && f < kSrN1) {
        sy = -0.5f;
    }
    if (f == kSrN1) {
        s.dN = s.cumNick - s.nick0;
        kff::logInfo("kamera: STICK-RELATIV N f=%u dN=%.2f (Stick Y -0,5, + = Nase unten)", f, s.dN);
    }
    if (f == kSrB && !s.dismounted) {
        c.tapB();
        s.dismounted = true;
        c.note("stick-relativ: B (absteigen)");
    }
    if (f > kSrB + 600) {
        return srFinish("Zeitlimit nach dem B");
    }
    c.stick(sx, sy);
    c.cStick(cx, 0.0f);
    return false;
}

const kff::autotest::Scenario kStickRelScenario = {
    "stick-relativ",
    "WP8.7: Kurve mit Kamera hinter und 90 Grad neben dem Vogel, 600 Ticks Halt mit kreisender Kamera, Nick-Schritt",
    &stickRelTick,
};

#endif  // KFF_DEV_AUTOTEST (Test-Szenario "kamera")

// ---------------------------------------------------------------- Hooks

// PadRead-Hook (nach dem Autotest, vor dem Konsum des Kerns): Stop-Heilung. Die Tasten des Operators liest cameraOperate
// aus kff::input().
// Modul-Gate (HK-03/P2-09): der Hook wirkt nur, solange der eigene Operator laeuft und das Modul aktiv ist.
// Keine Ausnahme verlaesst einen Hook (eine Ausnahme im Callback schaltet sonst die ganze Mod ab).
void onPadRead(ModContext*, void*, void*, void*) {
    try {
        if (!g_operatorOn || !kff::moduleActive(&kff::modules::kamera())) {
            return;
        }
        healStoppedCamera();
    } catch (...) {
        kff::logEvery(g_logHookErrors, 3, 300, "kamera: PadRead-Hook ausgesetzt (Exception, Tick uebersprungen)");
    }
}

#if defined(KFF_CORE_CAMERA_STOP_HOOK)
// K22: Kern-Hook dCamera_c::Stop (Pre). Im Ritt ohne Ereignis und Demo wird der Stopp abgefangen (HOOK_SKIP_ORIGINAL),
// die Spielkamera bleibt frei. Sonst laeuft das Original (stopPass). Die Stop-Heilung im PadRead-Hook bleibt als Rueckfall.
// Ballonmenue (d_a_kago.cpp:246) und die Ereignis-Stopps des Vogels rufen Stop erst nach angenommener Demo (eventOrDemo:
// Original laeuft); fuer den eigenen Vogel ersetzt der Kern executeBalloonMenu. Die Schadenskamera (d_a_alink_damage.inc:1721)
// laeuft ausserhalb des Ritts unveraendert durch; im Ritt faengt der Hook ihren Stopp ab (Absicht K22).
HookAction onCameraStop(ModContext*, void*, void*, void*) {
    try {
#if defined(KFF_DEV_AUTOTEST)
        if (g_testNoStopHook) {
            logStopPass("Test KFF_KAMERA_STOPHOOK=0");
            return HOOK_CONTINUE;  // VORLAUF K22 (nur Test): Stopp durchlassen, damit die 30-Tick-Heilung im PadRead-Hook greift
        }
#endif
        if (!g_operatorOn || !kff::moduleActive(&kff::modules::kamera()) || !kff::isRiding() || eventOrDemo()) {
            logStopPass(!kff::isRiding() ? "kein Ritt" : (eventOrDemo() ? "Ereignis oder Demo" : "Operator aus"));
            return HOOK_CONTINUE;
        }
        ++g_stats.stopSkips;
        if (g_logStopAbfang < 6) {
            ++g_logStopAbfang;
            kff::logInfo("kamera: STOP-ABFANG (K22) f=%u", static_cast<unsigned>(kff::session().rideFrames));
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        kff::logEvery(g_logHookErrors, 3, 300, "kamera: CameraStop-Hook ausgesetzt (Exception), Stopp laeuft");
        return HOOK_CONTINUE;
    }
}
#endif

// ---------------------------------------------------------------- Modul

#if defined(KFF_DEV_AUTOTEST)
// Testwert aus der Umgebung: "1" = Elevation bis -70 im Test.
bool parseTestLow(const char* text) {
    return text != nullptr && text[0] == '1' && text[1] == '\0';
}
#endif

class KameraModule final : public kff::Module {
public:
    const char* name() const override { return "kamera"; }

    ModResult install(kff::Registry& reg) override {
#if defined(KFF_DEV_AUTOTEST)
        const char* lauf = std::getenv("KFF_KAMERA_LAUF");  // nur Test: "wand" = Boden- und Wandlauf
        g_wandLauf = lauf != nullptr && std::strcmp(lauf, "wand") == 0;
        g_testLow = parseTestLow(std::getenv("KFF_KAMERA_TESTLOW"));
        kff::logInfo("kamera: Testlauf=%s testLow=%d", g_wandLauf ? "wand" : "kamera", g_testLow ? 1 : 0);
        const char* stopHook = std::getenv("KFF_KAMERA_STOPHOOK");  // VORLAUF K22 (nur Test): "0" = Stop-Pre-Hook aus
        g_testNoStopHook = stopHook != nullptr && std::strcmp(stopHook, "0") == 0;
        kff::logInfo("kamera: Stop-Pre-Hook im Test %s", g_testNoStopHook ? "aus (KFF_KAMERA_STOPHOOK=0)" : "an");
        const ModResult srR = kff::autotest::registerScenario(&kStickRelScenario);  // WP8.7, nur Testbuild
        if (srR != MOD_OK) {
            kff::logWarn("kamera: Szenario stick-relativ nicht registriert (%d)", static_cast<int>(srR));
        }
        const ModResult kmR = kff::autotest::registerScenario(&kKameraMenschScenario);  // Griff-Ritt mit dem Zeitplan von kamera, nur Testbuild
        if (kmR != MOD_OK) {
            kff::logWarn("kamera: Szenario kamera-mensch nicht registriert (%d)", static_cast<int>(kmR));
        }
#endif
        // HK-03/P2-09: erst die Dienstpruefung, dann der Operator, erst danach die Hooks. Ohne Operator bleibt die
        // Stop-Heilung aus, und kein Hook des Moduls ist registriert.
        if (svc_camera == nullptr) {
            kff::logWarn("kamera: CameraService fehlt, die Spielkamera bleibt");
            return MOD_UNAVAILABLE;
        }
        CameraOperatorDesc desc = CAMERA_OPERATOR_DESC_INIT;
        desc.debug_name = "kff-kamera";
        desc.priority = kff::prio::kKamera;
        desc.operate = &cameraOperate;
        desc.user_data = nullptr;
        const ModResult r = svc_camera->register_camera_operator(mod_ctx, &desc, &g_handle);
        kff::logInfo("kamera: Kamera-Operator registriert -> %d", static_cast<int>(r));
        if (r != MOD_OK) {
            return r;
        }
        g_operatorOn = true;
        const ModResult padR = reg.addPost(kff::Hook::PadRead, &onPadRead, kff::prio::kKamera);
        if (padR != MOD_OK) {
            kff::logWarn("kamera: PadRead-Hook nicht registriert (%d)", static_cast<int>(padR));
        }
#if defined(KFF_CORE_CAMERA_STOP_HOOK)
        // K22: Stopp im Ritt abfangen. Fehlt der Kern-Hook, bleibt die Stop-Heilung im PadRead-Hook.
        const ModResult stopR = reg.addPre(kff::Hook::CameraStop, &onCameraStop, kff::prio::kKamera);
        if (stopR != MOD_OK) {
            kff::logWarn("kamera: CameraStop-Hook nicht registriert (%d), die Stop-Heilung bleibt", static_cast<int>(stopR));
        }
#endif
        return r;
    }

    void onRideEnd(kff::Session& s, daKago_c* bird, kff::EndReason why) override {
        (void)s;
        (void)bird;
        // Rueckgabe an die Spielkamera: fovy und Oben zurueck, QuickStart bei eingefrorenem Zustand ohne Ereignis.
        handOver(dCam_getBody(), eventOrDemo());
        // Eine im Stop-Zustand eingefrorene Spielkamera auch dann freigeben, wenn der Operator nicht mehr lief.
        if (dCamera_c* cam = dCam_getBody()) {
            if (!eventOrDemo() && cam->mCurState == kCamStateStop) {
                cam->QuickStart();
                ++g_stats.quickStarts;
                kff::logInfo("kamera: Ritt-Ende, Spielkamera im Stop-Zustand, QuickStart");
            }
        }
        g_orbit.valid = false;
        g_stopFrames = 0;
        kff::logInfo("kamera: Ritt-Ende (%s), Orbit verworfen", kff::endReasonName(why));
    }

#if defined(KFF_DEV_AUTOTEST)
    const kff::autotest::Scenario* autotestScenario() const override { return &kKameraScenario; }
#endif

    void onShutdown(kff::Session& s) override {
        (void)s;
        g_operatorOn = false;
        kff::logInfo("kamera: SUMMARY calls=%u used=%u rides=%u handovers=%u quickStarts=%u stopHeals=%u "
                     "wallHits=%u groundClamps=%u groundNone=%u retracts=%u snaps=%u",
            static_cast<unsigned>(g_stats.calls), static_cast<unsigned>(g_stats.used),
            static_cast<unsigned>(g_stats.rides), static_cast<unsigned>(g_stats.handovers),
            static_cast<unsigned>(g_stats.quickStarts), static_cast<unsigned>(g_stats.stopHeals),
            static_cast<unsigned>(g_stats.wallHits), static_cast<unsigned>(g_stats.groundClamps),
            static_cast<unsigned>(g_stats.groundNone), static_cast<unsigned>(g_stats.retracts),
            static_cast<unsigned>(g_stats.snaps));
        kff::logInfo("kamera: SUMMARY upJumps=%u flips=%u jitterSteps=%u zigzag=%u maxOffStep=%.2f "
                     "maxUpStepDeg=%.2f maxBirdOffDeg=%.2f eyeGapMin=%.1f",
            static_cast<unsigned>(g_stats.upJumps), static_cast<unsigned>(g_stats.flips),
            static_cast<unsigned>(g_stats.jitterSteps), static_cast<unsigned>(g_stats.zigzag),
            static_cast<double>(g_stats.maxOffStep), static_cast<double>(g_stats.maxUpStepDeg),
            static_cast<double>(g_stats.maxBirdOffDeg), static_cast<double>(g_stats.eyeGapMin));
        kff::logInfo("kamera: SUMMARY relMin=%.1f relMax=%.1f elevMin=%.1f elevMax=%.1f distMin=%.0f distMax=%.0f "
                     "fovMax=%.2f maxFovStep=%.2f",
            static_cast<double>(g_stats.relMin), static_cast<double>(g_stats.relMax),
            static_cast<double>(g_stats.elevMin), static_cast<double>(g_stats.elevMax),
            static_cast<double>(g_stats.distMin), static_cast<double>(g_stats.distMax),
            static_cast<double>(g_stats.fovMax), static_cast<double>(g_stats.maxFovStep));
        kff::logInfo("kamera: SUMMARY abstand lookHits=%u hardClamps=%u maxDistStep=%.1f stopSkips=%u stopPass=%u wallFix=%u",
            static_cast<unsigned>(g_stats.lookHits), static_cast<unsigned>(g_stats.hardClamps),
            static_cast<double>(g_stats.maxDistStep), static_cast<unsigned>(g_stats.stopSkips),
            static_cast<unsigned>(g_stats.stopPass), static_cast<unsigned>(g_stats.wallFix));
        kff::logInfo("kamera: SUMMARY ausweichen cuts=%u ticks=%u maxDeg=%.1f",
            static_cast<unsigned>(g_stats.avoidCuts), static_cast<unsigned>(g_stats.avoidTicks),
            static_cast<double>(g_stats.avoidMaxDeg));
        if (g_stats.menschRides > 0u) {
            // Nur nach einem Ritt im Griff (Mensch-Link): Lage von Link und Vogel im Bild, Versatz und Abstand.
            kff::logInfo("kamera: SUMMARY griff rides=%u ticks=%u linkOut=%u birdOut=%u linkVmin=%.1f birdVmax=%.1f "
                         "hAbsMax=%.1f halbV=%.1f versatz=%.0f..%.0f dist=%.0f..%.0f",
                static_cast<unsigned>(g_stats.menschRides), static_cast<unsigned>(g_stats.menschTicks),
                static_cast<unsigned>(g_stats.menschLinkOut), static_cast<unsigned>(g_stats.menschBirdOut),
                static_cast<double>(g_stats.menschLinkVMin), static_cast<double>(g_stats.menschBirdVMax),
                static_cast<double>(g_stats.menschHAbsMax), static_cast<double>(g_stats.menschHalfV),
                static_cast<double>(g_stats.menschOffMin), static_cast<double>(g_stats.menschOffMax),
                static_cast<double>(g_stats.menschDistMin), static_cast<double>(g_stats.menschDistMax));
        }
#if defined(KFF_DEV_AUTOTEST)
        kff::logInfo("kamera: SUMMARY TEST loopBehindMin=%.3f loopUpYMin=%.3f loopRelDev=%.2f loopsDone=%u stalls=%u "
                     "foreign=%u wandLauf=%d testLow=%d",
            static_cast<double>(g_stats.loopBehindMin), static_cast<double>(g_stats.loopUpYMin),
            static_cast<double>(g_stats.loopRelDev), static_cast<unsigned>(g_stats.loopsDone),
            static_cast<unsigned>(g_stats.stalls), static_cast<unsigned>(kff::autotest::foreignInputCount()),
            g_wandLauf ? 1 : 0, g_testLow ? 1 : 0);
        kff::logInfo("kamera: SUMMARY TEST throughWall=%u", static_cast<unsigned>(g_stats.throughWall));
        kff::logInfo("kamera: SUMMARY SCHRITT start f0-60 max=%.1f f=%u ritt=%u n130=%u | eingeschwungen f61+ max=%.1f f=%u "
                     "ritt=%u n130=%u",
            static_cast<double>(g_probeEyeMaxStart), static_cast<unsigned>(g_probeEyeFStart),
            static_cast<unsigned>(g_probeEyeRideStart), static_cast<unsigned>(g_probeEyeOverStart),
            static_cast<double>(g_probeEyeMaxSet), static_cast<unsigned>(g_probeEyeFSet),
            static_cast<unsigned>(g_probeEyeRideSet), static_cast<unsigned>(g_probeEyeOverSet));
#endif
    }
};

KameraModule g_module;

}  // namespace

namespace kff::modules {

Module& kamera() {
    return g_module;
}

}  // namespace kff::modules
