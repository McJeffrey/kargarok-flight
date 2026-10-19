// Modul "tempo": Gas stufenlos (R und L), Steuerkreuz in 20-Prozent-Schritten, Bremse, Dash, Sichtfeld und Anzeige
// (Version 2.4.0).
//
// Eingaben (kff::input(), Schnappschuss vor dem Konsum des Kerns, siehe src/core/kff/input.hpp):
//   Steuerkreuz oben / unten  Schritt je Flanke, +/- 20 Prozent. Start: config start_gang (1..5 = 30/40/50/75/100 %).
//   R gehalten (bind_schneller) Gas: das Tempo steigt, je Tick 1/90, bei vollem Druck 3 s bis 100 Prozent. Analogwert: je
//                             tiefer, desto schneller. Loslassen: das Tempo bleibt (Tempomat).
//   L gehalten (bind_langsamer) Gas: das Tempo sinkt bis zum Stillstand, das ist das Schweben. Loslassen: das Tempo bleibt.
//   Z gehalten                Bremse: Ziel hoechstens 30 Prozent. Loslassen: zurueck auf den Tempomat. Bricht einen Dash ab.
//   A (Flanke)                Dash: 49 Ticks Ziel 100 Prozent (Hochlauf 14, Schub 35), Anstieg 0,05 je Tick (von 30 Prozent
//                             in 14 Ticks, von 50 Prozent in 10), danach 10 Ticks Ausklang mit 0,05 je Tick zurueck zum Tempomat.
//                             Sperrzeit 85 Ticks ab Start (Vanilla mDashCooldownTime). Bei gehaltener Bremse abgelehnt.
//                             Der Vanilla-Angriff startet nicht: A ist im Ritt vom Kern verbraucht.
//
// Ausgaben an die Sitzung (nur dieses Modul schreibt sie, Felder in src/core/kff/session.hpp):
//   speedScale    Tempo-Faktor 0..1.00 (100 Prozent = Hoechsttempo, 0 = Schweben). Angewendet im Flugmodul (flug):
//                 Weg je Tick = Hoechsttempo * speedScale. Gier- und Nickrate mit 2 * speedScale, damit der Wendekreis
//                 bei jedem Tempo gleich bleibt. Im Landeanflug nach B (Session::landing, E9) geht der Faktor in
//                 kLandRamp Ticks sanft auf 0 und bleibt dort bis zum Ritt-Ende; Gas, Bremse und Dash wirken dann nicht.
//   gear          Gang 1..5 als Zahl, aus dem Tempo abgeleitet (Anzeige, Log).
//   fovExtraDeg   Sichtfeld-Zusatz 0..10 Grad. Angewendet vom Kamera-Modul, dort hoechstens 0,5 Grad je Tick.
//
// Schweben (Tempo 0, Entwurf H1 und Version 2.4.0): entsteht von selbst, wenn das Tempo bei 0 steht (L gehalten bis zum
//   Stillstand, Steuerkreuz runter bis 0). Kein Tippen noetig. R gehalten oder Steuerkreuz hoch beenden es, ein Dash
//   auch (Hochlauf von 0 in 20 Ticks, danach Tempomat 30 Prozent). Der Vogel steht auf der Stelle (Faktor 0, Weg 0), Stick X
//   dreht ihn wie bisher, Stick Y steigt oder sinkt (Schalter lenkung_invertieren). Bremse und Fassrolle wirken im
//   Schweben nicht. Ein Landeanflug nach B schaltet das Schweben aus. Ausgabe session().hover.
//
// Anzeige: eine Zeile im HUD ueber dMeter2Draw_c::draw (Kern-Hook Hook::MeterDraw). Halbtransparent. Config hud_anzeigen
//   schaltet sie ab. km/h ist eine Annahme (1 Einheit = 1 cm, 30 Ticks je Sekunde), nicht gemessen, daher das ~ davor.
//   Im Schweben steht dort "Hover", sonst das Tempo in Prozent ("45%").
//
// Testbau (KFF_DEV_AUTOTEST, KFF_AUTOTEST=tempo): Szenario "tempo" im Kern-Autotest. Es liefert die Eingaben, misst
//   Fenster ("tempo: MESS <name>") und markiert Screenshots ("tempo: SNAP <name>"). Nur dort kompiliert: Testanzeige
//   im HUD und das Szenario.

#include "d/dolzel_rel.h"

#include "core/actions.hpp"
#include "core/kff/actions.hpp"
#include "core/kff/kff.hpp"

#include "m_Do/m_Do_ext.h"
#if defined(KFF_DEV_AUTOTEST)
#include "d/actor/d_a_alink.h"  // nur Test: Link nach der Freigabe (WP8.14), Heulen/Verwandlung (WP8.11)
#include "m_Do/m_Do_controller_pad.h"
#endif
#include "d/actor/d_a_kago.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPrint.h"
#include "JSystem/JUtility/TColor.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "dolphin/gx.h"
#include "dolphin/mtx.h"

#include <algorithm>
#include <chrono>  // Ablauf der Tipp-Anzeige (B4)
#include <cmath>
#include <cstdint>
#include <cstdio>
#if defined(KFF_DEV_AUTOTEST)
#include <cstdlib>  // nur Test: KFF_WB_* im Wasser-Szenario
#include <cstring>  // nur Test: KFF_AUTOTEST-Vergleich fuer die H2-Szenarien
#include <ctime>  // nur Test: Wandzeit der Marken (Zuordnung der Screenshots)
#include "d/d_bg_s.h"  // nur Test: Bodensonde im Wasser-Szenario
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"  // nur Test: H4 Wandprobe
#include "d/d_bg_s_roof_chk.h"  // nur Test: H4 Deckenprobe
#include "SSystem/SComponent/c_math.h"  // nur Test: H4 Ausrichtung (cM_atan2s)
#include "core/kff/panel.hpp"  // nur Test: H4 Mods-Fenster (Panel-Inhalt)
#include "core/kff/services.hpp"  // nur Test: H4 Fenster (window_push)
#endif

namespace kff {
namespace {

// Startstufen 1..5 (start_gang) als Anteil am Hoechsttempo (30/40/50/75/100 Prozent). Stufe 3 (50 Prozent) ist der
// Standard. Der Tempo-Faktor laeuft seit Version 2.4.0 stufenlos von 0 (Schweben) bis 1 (Hoechsttempo).
constexpr int kGearCount = 5;
constexpr float kGearPct[kGearCount] = {0.30f, 0.40f, 0.50f, 0.75f, 1.00f};
constexpr float kScaleMax = 1.00f;

// Gas (Version 2.4.0): gehaltene Taste aendert den Tempomat je Tick. 1/90 je Tick, also 0 bis 100 Prozent in 90 Ticks,
// das sind 3 s bei 30 Ticks je Sekunde. Analogwert (R und L als Trigger): je tiefer, desto langsamer die Aenderung,
// kleinster Faktor kAnalogMin. Gehalten ab kTriggerHold (wie der Ruf in core/input.cpp). Steuerkreuz hoch oder runter:
// ein Schritt von 20 Prozent je Flanke.
constexpr float kThrottleRate = 1.0f / 90.0f;
constexpr float kTriggerHold = 0.61f;
constexpr float kAnalogMin = 0.50f;
constexpr float kStepPct = 0.20f;

// Aenderung des wirksamen Faktors je Tick.
constexpr float kRiseRate = 0.02f;         // Anhebung zum Tempomat: Schritt von 20 Prozent in 10 Ticks
constexpr float kFallRate = 0.03f;         // Absenkung zum Tempomat und Bremse
constexpr float kDashRiseRate = 0.05f;     // Dash-Anstieg je Tick: 30 auf 100 Prozent in 14 Ticks, 50 auf 100 in 10 (Vanilla: Rate 5)
constexpr float kDashReleaseRate = 0.05f;  // Dash-Ausklang je Tick: in kDashRelease Ticks 100 auf 50 Prozent, darunter weiter mit kFallRate

// Dash-Zeiten in Ticks (Vanilla d_a_kago.cpp executeAttack, mDashTime 70).
constexpr uint32_t kDashRamp = 14;
constexpr uint32_t kDashHold = 35;
constexpr uint32_t kDashRelease = 10;
constexpr uint32_t kDashCooldown = 85;  // Sperrzeit ab Start (Vanilla mDashCooldownTime)

// Landeanflug nach B (E9): der Faktor geht in kLandRamp Ticks (1 s bei 30 Hz) sanft auf 0. Kosinus-Kurve, ohne Ruck an
// beiden Enden.
constexpr uint32_t kLandRamp = 30;
constexpr float kPi = 3.14159265f;


constexpr float kFovMaxDeg = 10.0f;     // Sichtfeld-Zusatz bei Hoechsttempo (das Kamera-Modul klemmt ebenfalls)
constexpr float kFlapBoost = 0.5f;      // Fluegelrate 1 + 0,5 * Faktor, also 1,15 bis 1,5
constexpr float kWindFrom = 0.75f;      // Windton ab diesem Faktor (75 Prozent)
constexpr uint32_t kWindEvery = 30;     // Ticks zwischen zwei Windtoenen
constexpr float kKmhPerUnit = 1.08f;    // Annahme: 1 Einheit = 1 cm, 30 Ticks je Sekunde, 3,6 km/h je m/s
#if defined(KFF_DEV_AUTOTEST)
constexpr uint32_t kLogEvery = 30;      // Telemetrie-Zeile alle 30 Ticks (nur Testbuild)
#endif

// HUD: Position der Zeile (Koordinaten des Spiel-2D-Rahmens, 608 x 464 Einheiten). Links oben, rechts neben den Herzen.
// Laengste Zeile "Gear 5/5  ~123 km/h  Dash in 2.8 s" = 410 Einheiten (Schrift rodan_b_24_22: Ziffern je 15, Leerzeichen 8):
// 150 + 410 = 560 < 608. Mit drei Leerzeichen und "Dash ready in" waren es 499 Einheiten, die Zeile lief am Rand ab (B3).
constexpr float kHudX = 150.0f;
constexpr float kHudY = 28.0f;
constexpr uint8_t kHudAlpha = 0xA0;     // halbtransparent

#if defined(KFF_DEV_AUTOTEST)
// Einzelwerte je Messfenster fuer den Median (groesstes Fenster: 90 Ticks).
constexpr uint32_t kMeasCap = 128;
#endif

struct TempoState {
    int gear = 2;                  // Gang 1..5 als Index 0..4, aus dem Tempo abgeleitet (Anzeige und Log)
    float cruise = kGearPct[2];    // Tempomat 0..1: gewuenschtes Tempo (Gas R und L, Steuerkreuz, Startwert)
    float eff = kGearPct[2];       // wirksamer Faktor 0..1, 0 im Schweben
    bool braking = false;          // Z gehalten
    bool dashOn = false;           // Dash laeuft
    bool landOn = false;           // Bremsrampe des Landeanflugs laeuft (E9)
    uint32_t landAge = 0;          // Ticks seit dem Start der Bremsrampe
    float landFrom = 0.0f;         // Faktor beim Start der Bremsrampe
    bool hover = false;            // Schweben aktiv (Tempo 0; die Anzeige-Stufe bleibt 1)
    int halt = 0;                  // gehaltene Gas-Taste: +1 schneller, -1 langsamer, 0 keine (nur Log)
    bool waitRelease = false;      // Ritt-Start: R oder L war schon gehalten (der Ruf), wirkt erst nach dem Loslassen
    uint32_t dashAge = 0;          // Ticks seit dem Dash-Start
    uint32_t cooldown = 0;         // Ticks bis zum naechsten Dash
    uint32_t ticks = 0;            // Ticks dieses Rittes
    bool tip2Due = false;          // zweiter Kurztipp dieses Rittes steht noch aus (Ritt-Frame kTipSecondFrame)
    bool havePrev = false;
    cXyz prevPos;                  // Position am Ende des letzten Ticks
    int16_t prevYaw = 0;           // Kurs am Ende des letzten Ticks
    float measured = 0.0f;         // horizontale Strecke des letzten Ticks (Einheiten)
    float yawStepDeg = 0.0f;       // Gierschritt des letzten Ticks (Grad, Vorzeichen wie angle.y)
    float smoothKmh = 0.0f;        // geglaettete Anzeige
    bool hudOn = false;            // config hud_anzeigen
    bool fovOn = false;            // config sichtfeld_effekt
    // Zaehler und Extremwerte (Log, Auswertung)
    uint32_t gearUps = 0;
    uint32_t gearDowns = 0;
    uint32_t brakes = 0;
    uint32_t dashes = 0;
    uint32_t dashRefused = 0;
    uint32_t windSounds = 0;
    float effMax = 0.0f;
    float fovMax = 0.0f;
    float measMax = 0.0f;
#if defined(KFF_DEV_AUTOTEST)
    // Messfenster (nur Testbuild): Mittel ueber die Ticks zwischen Beginn und Ende
    bool measOn = false;
    uint32_t measN = 0;
    double measWeg = 0.0;
    double measYaw = 0.0;
    double measEff = 0.0;
    float measDh[kMeasCap] = {};  // Einzelwerte je Tick: Weg (Einheiten), nur Testbuild
    float measDy[kMeasCap] = {};  // Einzelwerte je Tick: Gierschritt (Grad), nur Testbuild
#endif
};
TempoState g_t;

uint32_t g_logErrors = 0;   // Drosselung der Ausnahmen in Hooks
J2DPrint* g_hud = nullptr;  // einmal angelegt, in onShutdown freigegeben
bool g_fontTried = false;   // mDoExt_getMesgFont genau einmal aufgerufen
bool g_fontRef = false;     // dieses Modul haelt eine Referenz auf die Schrift (Gegenstueck: mDoExt_removeMesgFont)
bool g_down = false;        // Entladen: keine Anzeige mehr

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Konfiguration beim Ritt-Start. Der Wert wird auf 1..5 geklemmt.
int configStartGear() {
    const int64_t v = config::getInt(config::Key::StartGear);
    return static_cast<int>(std::clamp<int64_t>(v, 1, kGearCount));
}

// Text fuer die Dash-Anzeige (Englisch, Version 2.3.0).
void dashText(char* buf, size_t size) {
    if (g_t.dashOn) {
        std::snprintf(buf, size, "Dash active");
    } else if (g_t.cooldown > 0) {
        std::snprintf(buf, size, "Dash in %.1f s", static_cast<double>(g_t.cooldown) / 30.0);
    } else {
        std::snprintf(buf, size, "Dash ready");
    }
}

// Kurztipps bei den ersten Ritten (Schalter tipps_anzeigen, Zaehler tipps_rest). Tipp 1 beim Ritt-Start (Anzeige 5 s),
// Tipp 2 nach kTipSecondFrame Ritt-Frames (rund 10 s, Anzeige 6 s), nur wenn der Ritt dann noch laeuft.
// Schalter aus oder Zaehler 0: keine Tipps.
constexpr uint32_t kTipSecondFrame = 300;  // 10 s bei 30 Ticks je Sekunde

// Die Host-Anzeige eines Tipps liegt oben rechts ueber der HUD-Zeile (B4). Bis g_tipHideUntil zeichnet die Mod die
// HUD-Zeile nicht. Die Host-Anzeige schliesst nach der Dauer mit 0,2 s Uebergang (overlay.rcss); kTipCloseMs gibt
// diese 0,2 s plus 0,1 s Reserve fuer den Bildtakt.
constexpr uint32_t kTipCloseMs = 300;
std::chrono::steady_clock::time_point g_tipHideUntil{};

// Ein Tipp im Spiel. UiService optional: ohne Dienst bleibt es beim Log.
void pushTip(const char* text, uint32_t durationMs) {
    if (svc_ui == nullptr) {
        logInfo("tempo: Tipp (kein UI-Dienst): %s", text);
        return;
    }
    UiToastDesc d = UI_TOAST_DESC_INIT;
    d.body_rml = text;
    d.duration_ms = durationMs;
    if (svc_ui->push_toast(mod_ctx, &d) != MOD_OK) {
        logWarn("tempo: Tipp abgelehnt");
    } else {
        g_tipHideUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs + kTipCloseMs);
    }
}

// Tipp 1 beim Ritt-Start: Steuerung und Gang. Eine leere Taste steht als "not set" (actions::hintKey).
void startTips() {
    g_t.tip2Due = false;
    if (!config::getBool(config::Key::TipsShow)) {
        return;  // Schalter aus: keine Tipps, der Zaehler bleibt unberuehrt
    }
    const int64_t rest = config::getInt(config::Key::TipsRest);
    if (rest <= 0) {
        return;
    }
    if (!config::setInt(config::Key::TipsRest, rest - 1)) {
        return;  // ohne speicherbaren Zaehler (kein ConfigService) lieber keine Tipps als bei jedem Ritt
    }
    g_t.tip2Due = true;
    static char text[200];
    std::snprintf(text, sizeof text, "Fly with the Control Stick. %s speeds up, %s slows down.",
        actions::hintKey(actions::ActId::Faster, "hold").c_str(),
        actions::hintKey(actions::ActId::Slower, "hold").c_str());
    pushTip(text, 5000);
    logInfo("tempo: Tipp 1 an, Zaehler %lld", static_cast<long long>(rest - 1));
}

// Tipp 2 nach kTipSecondFrame Ritt-Frames: Schweben, Dash und Landung.
void secondTip() {
    static char text[200];
    std::snprintf(text, sizeof text, "Hover: %s until the bird stops in the air. Dash: %s. Land: %s.",
        actions::hintKey(actions::ActId::Slower, "hold").c_str(),
        actions::hintKey(actions::ActId::Dash, "tap").c_str(),
        actions::hintKey(actions::ActId::Dismount, "tap").c_str());
    pushTip(text, 6000);
    logInfo("tempo: Tipp 2 an f=%u", session().rideFrames);
}

#if defined(KFF_DEV_AUTOTEST)
// Telemetrie alle kLogEvery Ticks: alles, was ein Gang-Lauf braucht (Weg, Gierrate, Faktor, Dash, Fluegel).
void logTelemetry(daKago_c* k) {
    const TempoState& t = g_t;
    logInfo("tempo: f=%u gang=%d hover=%d vy=%.2f eff=%.3f weg=%.2f gier=%.4f kmh=%.1f bremse=%d dash=%d cool=%u "
            "fov=%.2f flap=%.3f mFly=%.1f pos=(%.0f,%.0f,%.0f) action=%d",
        session().rideFrames, t.gear + 1, t.hover ? 1 : 0, static_cast<double>(session().att.hoverVy),
        static_cast<double>(t.eff), static_cast<double>(t.measured), static_cast<double>(t.yawStepDeg),
        static_cast<double>(t.smoothKmh), t.braking ? 1 : 0, t.dashOn ? 1 : 0, t.cooldown,
        static_cast<double>(session().fovExtraDeg), static_cast<double>(1.0f + kFlapBoost * t.eff),
        static_cast<double>(k->mFlySpeed), static_cast<double>(k->current.pos.x),
        static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z), static_cast<int>(k->mAction));
}
#endif

#if defined(KFF_DEV_AUTOTEST)
// Messfenster: Beginn setzt die Summen zurueck, das Ende schreibt Mittel, Radius und Geschwindigkeit.
void measureBegin() {
    g_t.measOn = true;
    g_t.measN = 0;
    g_t.measWeg = 0.0;
    g_t.measYaw = 0.0;
    g_t.measEff = 0.0;
}

void measureEnd(const char* label) {
    TempoState& t = g_t;
    if (!t.measOn) {
        return;
    }
    t.measOn = false;
    if (t.measN == 0) {
        logInfo("tempo: MESS %s ohne Ticks", label);
        return;
    }
    const double n = static_cast<double>(t.measN);
    const double weg = t.measWeg / n;                                 // Einheiten je Tick
    const double gier = t.measYaw / n;                                // Grad je Tick, mit Vorzeichen
    const double eff = t.measEff / n;
    const double omega = std::fabs(gier) * 3.14159265358979 / 180.0;  // Radiant je Tick
    const double radius = omega > 1.0e-6 ? weg / omega : 0.0;         // Wendekreis in Einheiten (cm)
    // Median je Tick: ein kurzer Wandkontakt verzerrt das Mittel, der Median nicht.
    const uint32_t m = t.measN < kMeasCap ? t.measN : kMeasCap;
    float sh[kMeasCap];
    float sy[kMeasCap];
    std::copy(t.measDh, t.measDh + m, sh);
    std::copy(t.measDy, t.measDy + m, sy);
    std::nth_element(sh, sh + m / 2, sh + m);
    std::nth_element(sy, sy + m / 2, sy + m);
    logInfo("tempo: MESS %s ticks=%u gang=%d eff=%.3f weg=%.3f gier=%.4f radius=%.1f kmh=%.1f wegMed=%.3f gierMed=%.4f",
        label, t.measN, t.gear + 1, eff, weg, gier, radius, weg * static_cast<double>(kKmhPerUnit),
        static_cast<double>(sh[m / 2]), static_cast<double>(sy[m / 2]));
}

// WP8.14 (nur Testbuild): Landeanflug je Tick (Weg, Faktor, Lage, Stick) und Link nach der Freigabe (horizontaler Weg
// je Link-Frame). Der Kern misst "link horizontal" in releaseLandedHorizontal VOR dem Nullen (Weg des letzten Frames am
// Vogel); hier steht der Weg NACH der Freigabe. Zeilen "tempo: E9 ...".
constexpr uint32_t kE9LogAll = 45;      // die ersten Ticks des Anflugs je Tick loggen, danach jeden 15.
constexpr uint32_t kE9LevelFrom = 30;   // ab diesem Anflug-Tick zaehlen Nick, Rolle und Schraege als "waagerecht"
constexpr uint32_t kE9LinkFrames = 10;  // Link-Frames nach der Freigabe fuer das Ergebnis
constexpr uint32_t kE9LinkLog = 15;     // so viele Link-Frames werden geloggt
constexpr float kE9LinkMax = 0.05f;     // Grenze horizontal je Frame nach der Freigabe (Plan WP8.14, [V])

struct E9Meas {
    bool on = false;
    uint32_t ticks = 0;
    uint32_t below1At = 0;
    uint32_t below01At = 0;
    float nickMax = 0.0f;   // Grad, ab kE9LevelFrom
    float rollMax = 0.0f;
    float bankMax = 0.0f;
    bool linkArmed = false;
    bool linkHave = false;
    uint32_t linkFrames = 0;
    cXyz linkPrev{};
    float linkMax = 0.0f;
    double linkSum = 0.0;
};
E9Meas g_e9m;

float degS16(int v) {
    return static_cast<float>(v) * 360.0f / 65536.0f;
}

// Je Tick im Landeanflug, nach der Wegmessung (t.measured = Weg des letzten Ticks).
void e9MeasureTick(daKago_c* k) {
    E9Meas& m = g_e9m;
    if (!m.on) {
        m = E9Meas{};
        m.on = true;
        logInfo("tempo: E9 Anflug beginnt f=%u eff=%.3f weg=%.3f", session().rideFrames, static_cast<double>(g_t.eff),
            static_cast<double>(g_t.measured));
    }
    ++m.ticks;
    const AttitudeState& a = session().att;
    const float nick = degS16(static_cast<int16_t>(k->current.angle.x));
    const float roll = degS16(a.roll);
    const float bank = degS16(a.bank);
    if (m.below1At == 0 && g_t.measured < 1.0f) {
        m.below1At = m.ticks;
    }
    if (m.below01At == 0 && g_t.measured < 0.1f) {
        m.below01At = m.ticks;
    }
    if (m.ticks >= kE9LevelFrom) {
        m.nickMax = std::max(m.nickMax, std::fabs(nick));
        m.rollMax = std::max(m.rollMax, std::fabs(roll));
        m.bankMax = std::max(m.bankMax, std::fabs(bank));
    }
    if (m.ticks <= kE9LogAll || m.ticks % 15 == 0) {
        const Input& in = input();
        logInfo("tempo: E9 t=%u weg=%.3f eff=%.3f nick=%.3f rolle=%.3f schraeg=%.3f fass=%d stick=(%.2f,%.2f) y=%.1f",
            m.ticks, static_cast<double>(g_t.measured), static_cast<double>(g_t.eff), static_cast<double>(nick),
            static_cast<double>(roll), static_cast<double>(bank), a.barrelLeft > 0 ? 1 : 0, static_cast<double>(in.stickX),
            static_cast<double>(in.stickY), static_cast<double>(k->current.pos.y));
    }
}

// Ritt-Ende waehrend eines Landeanflugs: Ergebnis des Anflugs, Link-Messung scharf schalten.
void e9RideEnd() {
    E9Meas& m = g_e9m;
    if (!m.on) {
        return;
    }
    m.on = false;
    logInfo("tempo: E9 ANFLUG ticks=%u weg<1 ab t=%u weg<0,1 ab t=%u nickMax=%.3f rolleMax=%.3f schraegMax=%.3f (ab t=%u)",
        m.ticks, m.below1At, m.below01At, static_cast<double>(m.nickMax), static_cast<double>(m.rollMax),
        static_cast<double>(m.bankMax), kE9LevelFrom);
    m.linkArmed = true;
    m.linkHave = false;
    m.linkFrames = 0;
    m.linkMax = 0.0f;
    m.linkSum = 0.0;
}

// Jeder Link-Frame nach der Freigabe: horizontaler Weg zum vorigen Link-Frame.
void e9LinkFrame(daAlink_c* link) {
    E9Meas& m = g_e9m;
    if (!m.linkArmed || link == nullptr) {
        return;
    }
    const cXyz pos = link->current.pos;
    if (!m.linkHave) {
        m.linkHave = true;
        m.linkPrev = pos;
        logInfo("tempo: E9 LINK Freigabe pos=(%.1f,%.1f,%.1f) speedF=%.3f normal=%.3f v=(%.2f,%.2f) boden=%d proc=%u",
            static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z),
            static_cast<double>(link->speedF), static_cast<double>(link->mNormalSpeed), static_cast<double>(link->speed.x),
            static_cast<double>(link->speed.z), link->mLinkAcch.ChkGroundHit() ? 1 : 0,
            static_cast<unsigned>(link->mProcID));
        return;
    }
    const float dx = pos.x - m.linkPrev.x;
    const float dz = pos.z - m.linkPrev.z;
    const float dh = std::sqrt(dx * dx + dz * dz);
    ++m.linkFrames;
    if (m.linkFrames <= kE9LinkFrames) {
        m.linkMax = std::max(m.linkMax, dh);
        m.linkSum += static_cast<double>(dh);
    }
    if (m.linkFrames <= kE9LinkLog) {
        // normal = daAlink_c::mNormalSpeed, v = speed.x/z, boden = Fuesse am Boden, wand = Wandkontakt (Abgleiten an einer
        // Klippe schiebt Link ohne eigene Geschwindigkeit seitlich).
        logInfo("tempo: E9 LINK %u dh=%.4f dy=%.2f speedF=%.3f normal=%.3f v=(%.2f,%.2f) boden=%d wand=%d proc=%u",
            m.linkFrames, static_cast<double>(dh), static_cast<double>(pos.y - m.linkPrev.y),
            static_cast<double>(link->speedF), static_cast<double>(link->mNormalSpeed), static_cast<double>(link->speed.x),
            static_cast<double>(link->speed.z), link->mLinkAcch.ChkGroundHit() ? 1 : 0,
            link->mLinkAcch.ChkWallHit() ? 1 : 0, static_cast<unsigned>(link->mProcID));
    }
    if (m.linkFrames == kE9LinkFrames) {
        logInfo("tempo: E9 LINK-HORIZONTAL max=%.4f mittel=%.4f in %u Frames nach der Freigabe (Grenze %.2f) -> %s",
            static_cast<double>(m.linkMax), m.linkSum / static_cast<double>(kE9LinkFrames), kE9LinkFrames,
            static_cast<double>(kE9LinkMax), m.linkMax <= kE9LinkMax ? "ok" : "ueber der Grenze");
    }
    if (m.linkFrames >= kE9LinkLog) {
        m.linkArmed = false;
    }
    m.linkPrev = pos;
}

#endif  // KFF_DEV_AUTOTEST (Messfenster)

// Schweben an: das Tempo ist bei 0 (Stillstand, kein Dash). Ohne Rampe, der Stillstand kommt aus dem Tempo selbst.
void enterHover(TempoState& t, daKago_c* k) {
    t.hover = true;
    t.eff = 0.0f;
    k->mSound.startCreatureSound(Z2SE_SY_MENU_CHANGE_WINDOW, 0, -1);
    logInfo("tempo: SCHWEBEN an (Stillstand) f=%u tempomat=%.3f", session().rideFrames, static_cast<double>(t.cruise));
}

// Schweben aus, durch Gas (R), Steuerkreuz hoch oder Dash. Das Tempo steigt dann von 0 aus.
void leaveHover(TempoState& t, daKago_c* k, const char* why) {
    t.hover = false;
    k->mSound.startCreatureSound(Z2SE_SY_MENU_CHANGE_WINDOW, 0, -1);
    logInfo("tempo: SCHWEBEN aus durch %s f=%u", why, session().rideFrames);
}

// Gehaltene Gas-Taste: Digitalbit oder Analogwert ab kTriggerHold (wie der Ruf, core/input.cpp). Ohne Belegung nie.
bool haltOn(const ActRef& r) {
    return r.bit != 0 && (r.held() || actions::analogAbove(r.bit, kTriggerHold));
}

// Druckfaktor der Gas-Rate je Tick. Trigger R oder L mit Analogwert f: je tiefer, desto langsamer, mindestens kAnalogMin.
// Ohne Analogwert (Tastatur, Digitalbit) der volle Schritt.
float druckFaktor(const ActRef& r) {
    const Input& in = input();
    float f = 0.0f;
    if (r.bit == static_cast<uint32_t>(PAD_TRIGGER_R)) {
        f = in.triggerR;
    } else if (r.bit == static_cast<uint32_t>(PAD_TRIGGER_L)) {
        f = in.triggerL;
    }
    return f > 0.0f ? clampf(f, kAnalogMin, 1.0f) : 1.0f;
}

// Ein Tick des Tempo-Modells. Laeuft im Pre-Hook von daKago_c::execute des eigenen Vogels, vor action().
void tick(daKago_c* k) {
    TempoState& t = g_t;
    // Live je Tick: Gang-Zeile und Sichtfeld folgen dem Schalter im Mods-Panel, ohne Neustart des Rittes.
    // Der Stand beim Ritt-Start (onRideStart) bleibt als Anfangswert.
    t.hudOn = config::getBool(config::Key::HudShow);
    t.fovOn = config::getBool(config::Key::FovEffect);
#if defined(KFF_DEV_AUTOTEST)
    const Input& in = input();  // Telemetrie unten; die Tasten selbst liest der Tick ueber kff::act (E-10)
#endif
    // Belegte Flugtasten dieses Ticks (Vorgabe R, L, Steuerkreuz hoch und runter, Z, A; actions.cpp).
    const ActRef schneller = act(Act::Faster);
    const ActRef langsamer = act(Act::Slower);
    const ActRef hoch = act(Act::GearUp);
    const ActRef runter = act(Act::GearDown);
    const ActRef bremse = act(Act::Brake);
    const ActRef dash = act(Act::Dash);
    ++t.ticks;
    const bool landing = session().landing;  // E9: Landeanflug nach B, der Faktor laeuft auf 0

    // Weg und Gierschritt des letzten Ticks: Position und Kurs stehen nach dem Vortick.
    if (t.havePrev) {
        const float dx = k->current.pos.x - t.prevPos.x;
        const float dz = k->current.pos.z - t.prevPos.z;
        t.measured = std::sqrt(dx * dx + dz * dz);
        const int16_t step = static_cast<int16_t>(k->current.angle.y - t.prevYaw);
        t.yawStepDeg = static_cast<float>(step) * 360.0f / 65536.0f;
        t.measMax = std::max(t.measMax, t.measured);
#if defined(KFF_DEV_AUTOTEST)
        if (t.measOn) {
            if (t.measN < kMeasCap) {
                t.measDh[t.measN] = t.measured;
                t.measDy[t.measN] = t.yawStepDeg;
            }
            ++t.measN;
            t.measWeg += static_cast<double>(t.measured);
            t.measYaw += static_cast<double>(t.yawStepDeg);
            t.measEff += static_cast<double>(t.eff);
            logInfo("tempo: MT f=%u dh=%.3f dy=%.3f dyaw=%.4f yaw=%d pitch=%d mStick=%.2f sx=%.2f speedF=%.2f",
                session().rideFrames, static_cast<double>(t.measured),
                static_cast<double>(k->current.pos.y - t.prevPos.y), static_cast<double>(t.yawStepDeg),
                static_cast<int>(k->current.angle.y), static_cast<int>(k->current.angle.x),
                static_cast<double>(k->mStickX), static_cast<double>(in.stickX), static_cast<double>(k->speedF));
        }
#endif  // KFF_DEV_AUTOTEST (Messfenster im Tick)
    }
    t.prevPos = k->current.pos;
    t.prevYaw = k->current.angle.y;
    t.havePrev = true;
    t.smoothKmh += 0.25f * (t.measured * kKmhPerUnit - t.smoothKmh);

    // Gas (Version 2.4.0): gehaltenes R beschleunigt, gehaltenes L bremst, je Tick ein Stueck des Tempomaten. Beim
    // Ritt-Start schon gehaltene Tasten (der Ruf) wirken erst nach dem Loslassen. R und L zugleich: keine Wirkung.
    // Im Landeanflug ohne Wirkung (E9).
    if (t.waitRelease && !haltOn(schneller) && !haltOn(langsamer)) {
        t.waitRelease = false;
    }
    int halt = 0;
    if (!landing && !t.waitRelease && haltOn(schneller) != haltOn(langsamer)) {
        halt = haltOn(schneller) ? 1 : -1;
    }
    if (halt != t.halt) {  // Flanke: Start oder Ende des Haltens, eine Zeile im Log
        if (halt == 0) {
            logInfo("tempo: GAS aus f=%u tempomat=%.0f%%", session().rideFrames, static_cast<double>(t.cruise * 100.0f));
        } else {
            logInfo("tempo: GAS %s f=%u tempomat=%.0f%%", halt > 0 ? "schneller" : "langsamer", session().rideFrames,
                static_cast<double>(t.cruise * 100.0f));
        }
        t.halt = halt;
    }
    if (halt > 0) {
        if (t.hover) {
            leaveHover(t, k, "Gas");
        }
        t.cruise = clampf(t.cruise + kThrottleRate * druckFaktor(schneller), 0.0f, kScaleMax);
    } else if (halt < 0) {
        t.cruise = clampf(t.cruise - kThrottleRate * druckFaktor(langsamer), 0.0f, kScaleMax);
    }

    // Steuerkreuz hoch oder runter (je Flanke): ein Schritt von 20 Prozent des Hoechsttempos. Oben und unten zugleich
    // wirkt nicht. Im Schweben wirkt nur oben, es beendet das Schweben. Im Landeanflug ohne Wirkung (E9).
    const bool up = hoch.pressed();
    const bool down = runter.pressed();
    if (!landing && up != down) {
        if (up) {
            if (t.hover) {
                leaveHover(t, k, "Steuerkreuz hoch");
            }
            t.cruise = clampf(t.cruise + kStepPct, 0.0f, kScaleMax);
            ++t.gearUps;
            logInfo("tempo: SCHRITT hoch -> %.0f%% f=%u", static_cast<double>(t.cruise * 100.0f), session().rideFrames);
            k->mSound.startCreatureSound(Z2SE_SY_MENU_CHANGE_WINDOW, 0, -1);
        } else if (!t.hover) {  // Schweben: Steuerkreuz runter ohne Wirkung
            t.cruise = clampf(t.cruise - kStepPct, 0.0f, kScaleMax);
            ++t.gearDowns;
            logInfo("tempo: SCHRITT runter -> %.0f%% f=%u", static_cast<double>(t.cruise * 100.0f), session().rideFrames);
            k->mSound.startCreatureSound(Z2SE_SY_MENU_CHANGE_WINDOW, 0, -1);
        }
    }

    // Bremse: Z gehalten. Bricht einen laufenden Dash ab.
    const bool braking = bremse.held();
    if (braking && !t.braking) {
        ++t.brakes;
        logInfo("tempo: BREMSE an f=%u eff=%.3f", session().rideFrames, static_cast<double>(t.eff));
    } else if (!braking && t.braking) {
        logInfo("tempo: BREMSE aus f=%u eff=%.3f", session().rideFrames, static_cast<double>(t.eff));
    }
    t.braking = braking;
    if (braking && t.dashOn) {
        t.dashOn = false;
        logInfo("tempo: DASH abgebrochen (Bremse) f=%u", session().rideFrames);
    }

    // Dash: A-Flanke. Sperrzeit ab Start wie in Vanilla.
    if (t.cooldown > 0) {
        --t.cooldown;
    }
    if (!landing && dash.pressed()) {  // kein Dash im Landeanflug (E9)
        if (t.dashOn) {
            // laeuft schon: kein zweiter Start
        } else if (t.braking) {
            ++t.dashRefused;
            logInfo("tempo: DASH abgelehnt (Bremse gehalten) f=%u", session().rideFrames);
        } else if (t.cooldown > 0) {
            ++t.dashRefused;
            logInfo("tempo: DASH abgelehnt (Sperrzeit noch %u Ticks) f=%u", t.cooldown, session().rideFrames);
        } else {
            if (t.hover) {  // Dash aus dem Schweben: Schweben endet, Tempomat 30 Prozent (Gang 1), Hochlauf von 0 auf 100
                t.hover = false;
                t.cruise = kGearPct[0];
                logInfo("tempo: SCHWEBEN aus durch Dash f=%u", session().rideFrames);
            }
            t.dashOn = true;
            t.dashAge = 0;
            t.cooldown = kDashCooldown;
            ++t.dashes;
            k->mSound.startCreatureSound(Z2SE_EN_KC_DASH, 0, -1);
            logInfo("tempo: DASH %u start gang=%d eff=%.3f f=%u", t.dashes, t.gear + 1,
                static_cast<double>(t.eff), session().rideFrames);
        }
    }

    if (landing) {
        // E9: Landeanflug. Der Faktor geht in kLandRamp Ticks sanft (Kosinus) auf 0 und bleibt dort. Gang, Bremse und
        // Dash wirken nicht.
        if (!t.landOn) {
            t.landOn = true;
            t.landAge = 0;
            t.landFrom = t.eff;
            t.dashOn = false;
            t.hover = false;  // Landeanflug hat Vorrang: Schweben aus (session().hover ist ohnehin false)
            logInfo("tempo: LANDUNG Bremsrampe von %.3f auf 0 in %u Ticks, gang=%d f=%u",
                static_cast<double>(t.landFrom), static_cast<unsigned>(kLandRamp), t.gear + 1, session().rideFrames);
        }
        ++t.landAge;
        if (t.landAge < kLandRamp) {
            const float x = static_cast<float>(t.landAge) / static_cast<float>(kLandRamp);
            t.eff = t.landFrom * 0.5f * (1.0f + std::cos(kPi * x));
        } else {
            if (t.landAge == kLandRamp) {
                logInfo("tempo: LANDUNG Bremsrampe fertig, Weg 0 f=%u", session().rideFrames);
            }
            t.eff = 0.0f;
        }
    } else {
        t.landOn = false;
        if (t.hover) {
            // Schweben: der Vogel steht. Bremse und Dash wirken hier nicht.
            t.eff = 0.0f;
        } else {
            // Ziel und Aenderungsrate des wirksamen Faktors. Ziel ist der Tempomat, die Bremse zieht hoechstens auf 30 Prozent.
            float target = t.cruise;
            float riseRate = kRiseRate;
            float fallRate = kFallRate;
            if (t.braking) {
                target = std::min(t.cruise, kGearPct[0]);  // Bremse: hoechstens 30 Prozent
            } else if (t.dashOn) {
                if (t.dashAge < kDashRamp + kDashHold) {
                    target = kScaleMax;  // Hochlauf und Schub auf Hoechsttempo
                    riseRate = kDashRiseRate;
                } else {
                    fallRate = kDashReleaseRate;  // Ausklang auf den Tempomat
                }
                ++t.dashAge;
                if (t.dashAge >= kDashRamp + kDashHold + kDashRelease) {
                    t.dashOn = false;
                    logInfo("tempo: DASH ende f=%u", session().rideFrames);
                }
            }
            const float diff = target - t.eff;
            t.eff += diff > 0.0f ? std::min(diff, riseRate) : std::max(diff, -fallRate);
            t.eff = clampf(t.eff, 0.0f, kScaleMax);
            // Stillstand: Tempo 0 und kein Dash, dann schwebt der Vogel, ohne weiteres Tippen (Version 2.4.0).
            if (t.eff <= 0.0f && t.cruise <= 0.0f && !t.dashOn) {
                enterHover(t, k);
            }
        }
    }
    t.effMax = std::max(t.effMax, t.eff);
    // Gang 1..5 als Zahl, nur fuer Anzeige und Log: aus dem Tempo abgeleitet. Schweben und 0 zaehlen als Gang 1.
    t.gear = std::clamp(static_cast<int>(std::lround(t.eff * static_cast<float>(kGearCount))), 1, kGearCount) - 1;
#if defined(KFF_DEV_AUTOTEST)
    // Test: Gas und Dash je Tick, Schweben alle 30 Ticks eine Zeile.
    if (halt != 0 || t.dashOn || (t.hover && t.ticks % kLogEvery == 0)) {
        logInfo("tempo: RAMP rf=%u eff=%.4f tempomat=%.4f hover=%d halt=%d dash=%d dashAge=%u", session().rideFrames,
            static_cast<double>(t.eff), static_cast<double>(t.cruise), t.hover ? 1 : 0, halt, t.dashOn ? 1 : 0,
            static_cast<unsigned>(t.dashAge));
    }
#endif  // KFF_DEV_AUTOTEST (Rampen-Trace)

    // Ausgaben an die Sitzung.
    Session& s = session();
    s.speedScale = t.eff;
    s.gear = t.gear + 1;
    s.hover = t.hover && !landing;
    const float fovWant = t.fovOn ? kFovMaxDeg * clampf((t.eff - 0.5f) / 0.5f, 0.0f, 1.0f) : 0.0f;
    s.fovExtraDeg = fovWant;
    t.fovMax = std::max(t.fovMax, fovWant);

    // Fluegelrate folgt dem Tempo. Windton in Abstaenden ab Gang 4.
    if (k->mAnm_p != nullptr) {
        k->mAnm_p->setPlaySpeed(1.0f + kFlapBoost * t.eff);
    }
    if (t.eff >= kWindFrom && t.ticks % kWindEvery == 0) {
        ++t.windSounds;
        k->mSound.startCreatureSoundLevel(Z2SE_EN_KC_GLIDE, 0, -1);
    }

#if defined(KFF_DEV_AUTOTEST)
    if (t.ticks % kLogEvery == 0 || t.ticks == 1) {
        logTelemetry(k);
    }
#endif
#if defined(KFF_DEV_AUTOTEST)
    if (landing) {
        e9MeasureTick(k);
    }
#endif
}

// Post-Hook dMeter2Draw_c::draw: eine Zeile HUD in der 2D-Ebene.
void onMeterDrawPost(ModContext*, void*, void*, void*) {
    try {
        if (g_down || !g_t.hudOn || !isRiding()) {
            return;
        }
        if (g_hud == nullptr) {
            if (g_fontTried) {
                return;  // ohne Schrift keine Anzeige und kein zweiter Versuch
            }
            g_fontTried = true;
            // mDoExt_getMesgFont zaehlt die Referenz hoch, auch bei nullptr: danach gilt g_fontRef, Freigabe in onShutdown.
            JUTFont* font = mDoExt_getMesgFont();
            g_fontRef = true;
            if (font == nullptr) {
                return;
            }
            g_hud = new J2DPrint(font, JUtility::TColor(255, 255, 255, 255), JUtility::TColor(0, 0, 0, 160));
        }
        Mtx identity;
        MTXIdentity(identity);
        GXLoadPosMtxImm(identity, GX_PNMTX0);
        GXSetCurrentMtx(0);
        GXSetNumIndStages(0);
        for (int i = 0; i < 16; ++i) {
            GXSetTevDirect(static_cast<GXTevStageID>(i));
        }
        GXSetNumTexGens(1);
        GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 60);
        g_hud->initiate();

        char dash[24];
        dashText(dash, sizeof(dash));
        char gang[24];  // Tempo in Prozent oder Hover. Die Geschwindigkeit in km/h ist eine Schaetzung (~, siehe kKmhPerUnit).
        if (g_t.hover) {
            std::snprintf(gang, sizeof(gang), "Hover");
        } else {
            std::snprintf(gang, sizeof(gang), "%.0f%%", static_cast<double>(g_t.eff * 100.0f));
        }
        // Tipp-Anzeige offen (B4): keine HUD-Zeile, auch keine Testzeile. Der Testbau sieht dann wie das Release aus.
        if (std::chrono::steady_clock::now() >= g_tipHideUntil) {
            g_hud->print(kHudX, kHudY, kHudAlpha, "%s  ~%3.0f km/h  %s", gang,
                static_cast<double>(g_t.smoothKmh), dash);
#if defined(KFF_DEV_AUTOTEST)
            g_hud->print(kHudX, kHudY + 18.0f, 0xFF, "TEST eff=%.2f fov=%.1f weg=%.1f fremd=%u",
                static_cast<double>(g_t.eff), static_cast<double>(session().fovExtraDeg),
                static_cast<double>(g_t.measured), autotest::foreignInputCount());
#endif
        }
        // GX-Zustand zurueck auf den 2D-Standard der Ebene: setup2D() des Grafikports (J2DGrafContext.cpp:33-66), den
        // dMeter2Draw_c::draw vor seinem Zeichnen setzt (d_meter2_draw.cpp:722-723). Der HUD-Code oben setzt Pos-Matrix,
        // Texgen und alle TEV-Stufen um. JUTResFont::setGX (JUTResFont.cpp:206-222) setzt Kanaele, TEV-Stufe 0, Blend und
        // Vertex-Formate. setup2D stellt Matrizen, TEV, Z, Cull, Alpha-Vergleich, Kanaele und Vertex-Deskriptoren her.
        // Der Blend-Modus bleibt SRCALPHA/INVSRCALPHA, den setzen die 2D-Zeichner ohnehin selbst (J2DPicture.cpp:772).
        if (J2DGrafContext* port = dComIfGp_getCurrentGrafPort()) {
            port->setup2D();
        }
    } catch (...) {
        logEvery(g_logErrors, 3, 300, "tempo: Anzeige ausgesetzt (Exception)");
    }
}

#if defined(KFF_DEV_AUTOTEST)
// ---------------------------------------------------------------------------------------------
// Szenario "tempo" (KFF_AUTOTEST=tempo). Laeuft im Kern-Autotest: je Tick ersetzt die Injektion den gesamten
// Port-1-Zustand, der Kern zaehlt Fremdeingaben. Ablauf: Ruf (R halten, B tippen), Ritt mit Zeitplan nach
// Ritt-Frames: Gaenge hoch und runter, Dash mit Sperrzeit, Kurven je Gang, Bremse, Absteigen per B.
// ---------------------------------------------------------------------------------------------
namespace test {

enum class Stage : uint8_t { Wait, Call, Ride, Done };

constexpr uint32_t kCallAt = 300;      // frueheste Ruf-Zeit (Ticks seit dem ersten Link-Frame)
constexpr uint32_t kCallHold = 30;     // R gehalten, Ticks
constexpr uint32_t kCallTap = 15;      // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kCallRetry = 150;   // ohne Ritt nach so vielen Ticks neuer Ruf
constexpr uint32_t kMaxCalls = 6;
constexpr uint32_t kGiveUpTicks = 4000;
// Abstieg fertig nach so vielen Abflug-Ticks. Muss unter 100 liegen: der Kern entfernt den Vogel nach 100 Abflug-Ticks
// (ride.cpp kDepartDeleteFrames), danach ist die Phase Idle (Lauf v1-tempo meldete deshalb "Ritt ohne Abstieg beendet").
constexpr uint32_t kDepartEnd = 95;

constexpr uint32_t kBitUp = static_cast<uint32_t>(PAD_BUTTON_UP);
constexpr uint32_t kBitDown = static_cast<uint32_t>(PAD_BUTTON_DOWN);
constexpr uint32_t kBitA = static_cast<uint32_t>(PAD_BUTTON_A);
constexpr uint32_t kBitB = static_cast<uint32_t>(PAD_BUTTON_B);
constexpr uint32_t kBitZ = static_cast<uint32_t>(PAD_TRIGGER_Z);
constexpr uint32_t kBitY = static_cast<uint32_t>(PAD_BUTTON_Y);
constexpr uint32_t kBitX = static_cast<uint32_t>(PAD_BUTTON_X);
constexpr uint32_t kBitL = static_cast<uint32_t>(PAD_TRIGGER_L);
constexpr uint32_t kBitR = static_cast<uint32_t>(PAD_TRIGGER_R);
constexpr uint32_t kBitLeft = static_cast<uint32_t>(PAD_BUTTON_LEFT);
constexpr uint32_t kBitRight = static_cast<uint32_t>(PAD_BUTTON_RIGHT);
constexpr uint32_t kBitStart = static_cast<uint32_t>(PAD_BUTTON_START);

struct Tap {
    uint32_t at;
    uint32_t bits;
    const char* note;
};
constexpr Tap kTaps[] = {
    {70, kBitUp, "gang 3 -> 4"},
    {100, kBitUp, "gang 4 -> 5"},
    {200, kBitDown, "gang 5 -> 4"},
    {215, kBitDown, "gang 4 -> 3"},
    {230, kBitDown, "gang 3 -> 2"},
    {245, kBitDown, "gang 2 -> 1"},
    {300, kBitA, "dash 1 bei gang 1"},
    {372, kBitA, "dash 2 nach dem Dash-Ende in der Sperrzeit (muss abgelehnt werden)"},
    {400, kBitA, "dash 3 nach der Sperrzeit"},
    {640, kBitUp, "gang 1 -> 2"},
    {655, kBitUp, "gang 2 -> 3"},
    {820, kBitUp, "gang 3 -> 4"},
    {835, kBitUp, "gang 4 -> 5"},
    {1010, kBitA, "dash bei gehaltener Bremse (muss abgelehnt werden)"},
    {1105, kBitY, "Y: Fassrolle (WP8.8)"},
    {1180, kBitB, "B: absteigen"},
};

struct Hold {
    uint32_t from;
    uint32_t to;
    uint32_t bits;
    float stickX;
    const char* note;
};
constexpr Hold kHolds[] = {
    {500, 620, 0u, -1.0f, "kurve links bei gang 1"},
    {680, 800, 0u, -1.0f, "kurve links bei gang 3"},
    {860, 980, 0u, -1.0f, "kurve links bei gang 5"},
    {1000, 1040, kBitZ, 0.0f, "Bremse Z gehalten"},
    {1060, 1075, kBitRight, 0.0f, "Steuerkreuz rechts: Kamera naeher (WP8.8)"},
    {1080, 1095, kBitLeft, 0.0f, "Steuerkreuz links: Kamera weiter (WP8.8)"},
};

struct Snap {
    uint32_t at;
    const char* name;
};
constexpr Snap kSnaps[] = {
    {60, "g3"},
    {150, "g5"},
    {190, "top"},
    {285, "g1"},
    {306, "dash1"},
    {322, "dash1-top"},
    {380, "dash1-ende"},
    {420, "dash3"},
    {560, "kurve-g1"},
    {740, "kurve-g3"},
    {940, "kurve-g5"},
    {1025, "bremse"},
    {1095, "top-nach-bremse"},
    {1175, "vor-absteigen"},
};

struct Meas {
    uint32_t begin;
    uint32_t end;
    const char* name;
};
constexpr Meas kMeas[] = {
    {55, 70, "g3-gerade"},
    {120, 190, "g5-gerade"},
    {270, 300, "g1-gerade"},
    {520, 600, "kurve-g1"},
    {700, 790, "kurve-g3"},
    {890, 975, "kurve-g5"},
    {1028, 1040, "bremse"},
};

struct State {
    Stage stage = Stage::Wait;
    uint32_t armAt = 0;
    uint32_t calls = 0;
    uint32_t base = 0;  // Kern-Tick, an dem der Ritt-Tick 0 lag (Zeitplan in Kern-Ticks, WP8)
    bool humanCall = false;  // nur H4 W1: Ruf auch aus Menschform (autowolf verwandelt Link, wie F6 W1)
};
State g_state;

void finish(const char* why) {
    g_state.stage = Stage::Done;
    logInfo("tempo: SZENARIO ende (%s) fremdeingabe=%u dash=%u abgelehnt=%u gangHoch=%u gangRunter=%u bremse=%u "
            "effMax=%.3f fovMax=%.2f measMax=%.2f",
        why, autotest::foreignInputCount(), g_t.dashes, g_t.dashRefused, g_t.gearUps, g_t.gearDowns, g_t.brakes,
        static_cast<double>(g_t.effMax), static_cast<double>(g_t.fovMax), static_cast<double>(g_t.measMax));
}

// Nur H4 W1: Ruf auch aus Menschform (F6 W1, canCallHumanOrWolf in core/autotest.cpp). Mensch: Bodenkontakt, Phase Idle,
// kein Ereignis, kein eigener Vogel; dann verwandelt autowolf Link und ruft den Vogel. Wolf: wie canCall().
bool h4CanCallHuman(const autotest::Context& c) {
    daAlink_c* link = liveLink();
    if (link == nullptr || link->checkCargoCarry() || link->checkEventRun() || dComIfGp_event_runCheck() ||
        c.sess.birdId != kff::kNoProc) {
        return false;
    }
    if (link->checkWolf() != 0) {
        return true;
    }
    return c.sess.phase == Phase::Idle && link->mLinkAcch.ChkGroundHit() && link->mProcID != daAlink_c::PROC_DEAD &&
           link->mProcID != daAlink_c::PROC_METAMORPHOSE && link->mProcID != daAlink_c::PROC_METAMORPHOSE_ONLY;
}

// Ruf-Phase der Szenarien dieses Moduls (R halten, B tippen; Wiederholung ohne Vogel). Rueckgabe: 0 = warten (keine
// Eingabe des Zeitplans in diesem Tick), 1 = Ritt laeuft (Zeitplan), 2 = Szenario zu Ende.
// Zeitbezug: Ritt-Tick = Kern-Tick seit dem Ritt-Beginn (st.base). Ritt-Frames taugen nicht als Uhr: im Tick des
// Absteige-B zaehlt der Kern sie nicht hoch (ride.cpp kagoExecutePre, Ruecksprung vor ++rideFrames). Ein Zeitplan nach
// Ritt-Frames tippte B dort zweimal (Lauf base-tempo: "TASTE B" zweimal bei f=1150, zweites B = sofortige Freigabe).
int callPhase(autotest::Context& c, State& st, const char* tag, void (*fin)(const char*)) {
    const uint32_t t = c.frame;
    char buf[96];
    switch (st.stage) {
    case Stage::Wait:
        if (c.sess.phase == Phase::Riding) {
            st.stage = Stage::Ride;
            st.base = t - c.sess.rideFrames;
            std::snprintf(buf, sizeof(buf), "%s: ritt beginnt", tag);
            c.note(buf);
            return 0;
        }
        if (t >= kCallAt && (c.canCall() || (st.humanCall && h4CanCallHuman(c)))) {
            st.stage = Stage::Call;
            st.armAt = t;
            ++st.calls;
            std::snprintf(buf, sizeof(buf), "%s: ruf armiert (R halten, B tippen)", tag);
            c.note(buf);
        } else if (t > kGiveUpTicks) {
            fin("kein Ruf moeglich");
            return 2;
        }
        return 0;
    case Stage::Call: {
        const uint32_t since = t - st.armAt;
        if (c.sess.phase == Phase::Riding) {
            st.stage = Stage::Ride;
            st.base = t - c.sess.rideFrames;
            std::snprintf(buf, sizeof(buf), "%s: ritt beginnt", tag);
            c.note(buf);
            return 0;
        }
        if (since < kCallHold) {
            c.holdR();
            if (since == kCallTap) {
                c.tapB();
            }
        } else if (since > kGiveUpTicks) {
            fin("Ruf haengt ohne Ritt");
            return 2;
        } else if (since >= kCallRetry && c.sess.phase == Phase::Idle) {
            // Nur ohne laufenden Vogel neu rufen. Summoning wartet, bis der Ritt beginnt.
            if (st.calls >= kMaxCalls) {
                fin("Ruf verweigert");
                return 2;
            }
            st.stage = Stage::Wait;
        }
        return 0;
    }
    case Stage::Ride:
        return 1;
    case Stage::Done:
        return 2;
    }
    return 2;
}

bool scenarioTick(autotest::Context& c) {
    State& st = g_state;
    const int phase = callPhase(c, st, "tempo", &finish);
    if (phase != 1) {
        return phase == 2;
    }

    // Ritt-Phase: Zeitplan nach Ritt-Ticks (Kern-Ticks seit dem Ritt-Beginn).
    if (c.sess.phase == Phase::Dismounting) {
        if (c.sess.departFrames >= kDepartEnd) {
            finish("Abstieg abgeschlossen");
            return true;
        }
        return false;
    }
    if (c.sess.phase != Phase::Riding) {
        finish("Ritt ohne Abstieg beendet");
        return true;
    }

    const uint32_t f = c.frame - st.base;
    char buf[200];
    for (const Tap& tap : kTaps) {
        if (tap.at == f) {
            c.tap(tap.bits);
            std::snprintf(buf, sizeof(buf), "tempo: TASTE %s f=%u gang=%d", tap.note, f, g_t.gear + 1);
            c.note(buf);
        }
    }
    float stickX = 0.0f;
    uint32_t held = 0;
    for (const Hold& h : kHolds) {
        if (f == h.from) {
            std::snprintf(buf, sizeof(buf), "tempo: BEGINN %s f=%u", h.note, f);
            c.note(buf);
        }
        if (f == h.to) {
            std::snprintf(buf, sizeof(buf), "tempo: ENDE %s f=%u", h.note, f);
            c.note(buf);
        }
        if (f >= h.from && f < h.to) {
            stickX = h.stickX;
            held |= h.bits;
        }
    }
    if (held != 0) {
        c.hold(held);
    }
    c.stick(stickX, 0.0f);  // Nick 0: der Flug trimmt selbst auf die Horizontale

    for (const Meas& m : kMeas) {
        if (f == m.begin) {
            measureBegin();
        }
        if (f == m.end) {
            measureEnd(m.name);
        }
    }
    for (const Snap& s : kSnaps) {
        if (f == s.at) {
            std::snprintf(buf, sizeof(buf), "tempo: SNAP %s f=%u gang=%d eff=%.3f fov=%.2f", s.name, f, g_t.gear + 1,
                static_cast<double>(g_t.eff), static_cast<double>(session().fovExtraDeg));
            c.note(buf);
        }
    }
    return false;
}

const autotest::Scenario kScenario = {
    "tempo",
    "Gang 1-5, Dash mit Sperrzeit, Bremse, Kurven je Gang, Kamera-Abstand, Fassrolle, Sichtfeld, Absteigen (rund 1200 Ritt-Ticks)",
    &scenarioTick,
};

// ---------------------------------------------------------------------------------------------
// WP8.11 Konsum-Beobachter (nur Testbuild): Post-Hook von mDoCPd_c::read mit Prioritaet kTempo (200), also NACH dem Kern
// (1000, Injektion, Schnappschuss, Konsum) und nach kamera (300). Er sieht den Pad so, wie ihn danach Dusk
// (processGameCombos: R+Y Schnellverwandlung, R+X Heulen, m_Do_main.cpp) und die Spiellogik lesen. Je Ritt-Tick:
// Bit im Schnappschuss vor dem Konsum (kff::input(), bei offener Oberflaeche vom Kern maskiert) gegen das Bit im Pad danach.
// Lock-Flags: vor dem Konsum setzt die Injektion sie aus den Bits (autotest.cpp replacePad: Halten L/R, Flanke L/R),
// danach stehen sie hier im Pad. mHoldLockR bleibt bewusst (Kern P2-04).
// ---------------------------------------------------------------------------------------------
struct KonsumKey {
    uint32_t bit;
    const char* name;
};
constexpr KonsumKey kKonsumKeys[] = {
    {kBitA, "A"},          {kBitB, "B"},         {kBitX, "X"},         {kBitY, "Y"},
    {kBitZ, "Z"},          {kBitL, "L"},         {kBitR, "R"},         {kBitUp, "hoch"},
    {kBitDown, "runter"},  {kBitLeft, "links"},  {kBitRight, "rechts"}, {kBitStart, "Start"},
};
constexpr size_t kKonsumKeyN = sizeof(kKonsumKeys) / sizeof(kKonsumKeys[0]);
constexpr size_t kKonsumStartIdx = kKonsumKeyN - 1;  // Start bleibt beim Spiel (Pause, Menue)
constexpr uint32_t kKonsumLogMax = 80;

struct KonsumObs {
    uint32_t ticks = 0;               // Ritt-Ticks
    uint32_t vor[kKonsumKeyN] = {};   // Ticks mit dem Bit vor dem Konsum (gehalten oder Flanke)
    uint32_t nach[kKonsumKeyN] = {};  // davon Ticks mit dem Bit nach dem Konsum
    uint32_t lockVor[4] = {};         // vor dem Konsum: HoldL, TrigL, HoldR, TrigR (Injektion)
    uint32_t lockNach[4] = {};        // nach dem Konsum
    uint32_t analogVor = 0;           // Ticks mit Analog A, B, L oder R ungleich 0 vor dem Konsum
    uint32_t analogNach = 0;          // davon Ticks, in denen nach dem Konsum ein Analogwert bleibt
    // Ticks, in denen der Pad nach dem Kern Eingaben traegt, die der Schnappschuss nicht zeigt: offene Oberflaeche
    // (Menue, Karte, Dialog), der Kern maskiert den Schnappschuss und laesst die Tasten dem Spiel (E5). Kein Leck.
    uint32_t uiPass = 0;
    uint32_t lastVor = 0;             // Bits des vorigen Ticks (Log nur bei Aenderung)
    uint32_t logged = 0;
};
KonsumObs g_kobs;
uint32_t g_kobsErrors = 0;

void onPadReadObserve(ModContext*, void*, void*, void*) {
    try {
        if (session().phase != Phase::Riding) {
            g_kobs.lastVor = 0;
            return;
        }
        KonsumObs& o = g_kobs;
        const Input& in = input();
        const interface_of_controller_pad& p = mDoCPd_c::getCpadInfo(PAD_1);
        ++o.ticks;
        const uint32_t vorBits = in.held | in.pressed;
        const uint32_t nachBits = p.mButtonFlags | p.mPressedButtonFlags;
        for (size_t i = 0; i < kKonsumKeyN; ++i) {
            const uint32_t b = kKonsumKeys[i].bit;
            if ((vorBits & b) != 0) {
                ++o.vor[i];
                if ((nachBits & b) != 0) {
                    ++o.nach[i];
                }
            }
        }
        const bool lv[4] = {in.isHeld(kBitL), in.isPressed(kBitL), in.isHeld(kBitR), in.isPressed(kBitR)};
        const bool ln[4] = {p.mHoldLockL != 0, p.mTrigLockL != 0, p.mHoldLockR != 0, p.mTrigLockR != 0};
        for (int i = 0; i < 4; ++i) {
            o.lockVor[i] += lv[i] ? 1u : 0u;
            o.lockNach[i] += ln[i] ? 1u : 0u;
        }
        const bool av = in.analogA != 0.0f || in.analogB != 0.0f || in.triggerL != 0.0f || in.triggerR != 0.0f;
        const bool an = p.mAnalogA != 0.0f || p.mAnalogB != 0.0f || p.mTriggerLeft != 0.0f || p.mTriggerRight != 0.0f;
        // Wie bei den Tasten zaehlt "nach" nur Ticks, in denen der Kern den Wert im Schnappschuss gesehen hat. Analog ohne
        // Schnappschuss-Wert (Lauf v1-konsum: ein Tick, B im Pausenmenue) gehoert der offenen Oberflaeche.
        o.analogVor += av ? 1u : 0u;
        o.analogNach += (av && an) ? 1u : 0u;
        const bool uiTick = (!av && an) || (nachBits & ~vorBits) != 0;
        o.uiPass += uiTick ? 1u : 0u;
        if ((vorBits != o.lastVor || (uiTick && o.uiPass <= 8)) && o.logged < kKonsumLogMax) {
            ++o.logged;
            logInfo("tempo: KONSUM f=%u vor=0x%04x nach=0x%04x lockVor=%d%d%d%d lockNach=%d%d%d%d analogVor=(%.2f,%.2f,%.2f,%.2f) "
                    "analogNach=(%.2f,%.2f,%.2f,%.2f) getHold=0x%04x getTrig=0x%04x",
                session().rideFrames, static_cast<unsigned>(vorBits), static_cast<unsigned>(nachBits), lv[0] ? 1 : 0,
                lv[1] ? 1 : 0, lv[2] ? 1 : 0, lv[3] ? 1 : 0, ln[0] ? 1 : 0, ln[1] ? 1 : 0, ln[2] ? 1 : 0, ln[3] ? 1 : 0,
                static_cast<double>(in.analogA), static_cast<double>(in.analogB), static_cast<double>(in.triggerL),
                static_cast<double>(in.triggerR), static_cast<double>(p.mAnalogA), static_cast<double>(p.mAnalogB),
                static_cast<double>(p.mTriggerLeft), static_cast<double>(p.mTriggerRight),
                static_cast<unsigned>(mDoCPd_c::getHold(PAD_1) & 0xFFFFu),
                static_cast<unsigned>(mDoCPd_c::getTrig(PAD_1) & 0xFFFFu));
        }
        o.lastVor = vorBits;
    } catch (...) {
        logEvery(g_kobsErrors, 3, 300, "tempo: Konsum-Beobachter ausgesetzt (Exception)");
    }
}

// Ergebnis des Beobachters. true, wenn alle Ritt-Tasten vor dem Konsum gesehen wurden (sofern verlangt), nach dem Konsum
// 0 sind, Start unveraendert durchgeht, die Locks L und TrigR nach dem Konsum 0 sind und kein Analogwert bleibt.
bool konsumSummary(const char* where, bool requireAll) {
    const KonsumObs& o = g_kobs;
    char line[400];
    size_t used = 0;
    bool ok = true;
    for (size_t i = 0; i < kKonsumKeyN; ++i) {
        const bool isStart = i == kKonsumStartIdx;
        const bool keyOk = isStart ? o.nach[i] == o.vor[i] : o.nach[i] == 0;
        const bool seen = o.vor[i] > 0;
        if (!keyOk || (requireAll && !seen)) {
            ok = false;
        }
        const int n = std::snprintf(line + used, sizeof(line) - used, "%s=%u/%u ", kKonsumKeys[i].name, o.vor[i], o.nach[i]);
        if (n > 0 && static_cast<size_t>(n) < sizeof(line) - used) {
            used += static_cast<size_t>(n);
        }
    }
    const bool locksOk = o.lockNach[0] == 0 && o.lockNach[1] == 0 && o.lockNach[3] == 0;
    const bool analogOk = o.analogNach == 0;
    ok = ok && locksOk && analogOk;
    logInfo("tempo: KONSUM SUMMARY (%s) ticks=%u Taste=vor/nach: %s", where, o.ticks, line);
    logInfo("tempo: KONSUM SUMMARY (%s) locks vor HoldL=%u TrigL=%u HoldR=%u TrigR=%u nach HoldL=%u TrigL=%u HoldR=%u "
            "TrigR=%u (HoldR bleibt, P2-04) analog vor=%u nach=%u oberflaeche=%u -> %s",
        where, o.lockVor[0], o.lockVor[1], o.lockVor[2], o.lockVor[3], o.lockNach[0], o.lockNach[1], o.lockNach[2],
        o.lockNach[3], o.analogVor, o.analogNach, o.uiPass, ok ? "ok" : "ABWEICHUNG");
    return ok;
}

// ---------------------------------------------------------------------------------------------
// Szenario "eingabe-konsum" (WP8.11, KFF_AUTOTEST=eingabe-konsum). Ruf, dann im Ritt jede Ritt-Taste einzeln, R+Y und
// R+X (mit game.enableQuickTransform und game.sunsSong in der Testkonfiguration), Start oeffnet das Menue, B im Menue
// darf nicht absteigen, Start schliesst es, B steigt ab (Landeanflug). Zeitplan in Kern-Ticks seit dem Ritt-Beginn.
// Wirkung im Log: "flug: FASSROLLE", "tempo: DASH", "tempo: BREMSE", "tempo: GANG", "kamera: ZOOM", "kamera: X-Schwenk".
// Ergebnis: "tempo: SZENARIO eingabe-konsum ... -> BESTANDEN".
// ---------------------------------------------------------------------------------------------
struct KStep {
    uint32_t from;
    uint32_t to;      // exklusiv
    uint32_t tap;     // Flanke bei from
    uint32_t hold;    // gehalten von from bis to (R ueber holdR: Flanke am Anfang, Analogwert 1)
    float cStickX;
    const char* note;
};
constexpr KStep kKonsumSteps[] = {
    {60, 61, kBitY, 0u, 0.0f, "Y: Fassrolle"},
    {100, 101, kBitA, 0u, 0.0f, "A: Dash"},
    {140, 170, 0u, 0u, 1.0f, "C-Stick rechts: Kamera 45 Grad aus der Mitte"},
    {180, 181, kBitX, 0u, 0.0f, "X: Kamera hinter den Vogel"},
    {230, 246, 0u, kBitZ, 0.0f, "Z gehalten: Bremse"},
    {270, 271, kBitL, 0u, 0.0f, "L getippt"},
    {280, 301, 0u, kBitL, 0.0f, "L gehalten"},
    {330, 351, 0u, kBitR, 0.0f, "R gehalten"},
    {380, 381, kBitUp, 0u, 0.0f, "Steuerkreuz oben: Gang hoch"},
    {400, 401, kBitDown, 0u, 0.0f, "Steuerkreuz unten: Gang runter"},
    {430, 461, 0u, kBitRight, 0.0f, "Steuerkreuz rechts gehalten: Kamera naeher"},
    {480, 511, 0u, kBitLeft, 0.0f, "Steuerkreuz links gehalten: Kamera weiter"},
    {540, 576, 0u, kBitR, 0.0f, "R gehalten (R+Y = Dusk-Schnellverwandlung)"},
    {550, 551, kBitY, 0u, 0.0f, "Y bei gehaltenem R: keine Schnellverwandlung"},
    {600, 636, 0u, kBitR, 0.0f, "R gehalten (R+X = Dusk-Heulen)"},
    {610, 611, kBitX, 0u, 0.0f, "X bei gehaltenem R: kein Heulen"},
};
constexpr uint32_t kKonsumStartAt = 680;     // Start: Menue auf
constexpr uint32_t kKonsumMenuB = 60;        // Kern-Ticks nach Start: B im Menue
constexpr uint32_t kKonsumMenuClose = 120;   // Start schliesst das Menue (falls noch offen)
constexpr uint32_t kKonsumDismount = 200;    // B (Absteigen), sobald das Menue zu ist
constexpr uint32_t kKonsumMenuTimeout = 900;
constexpr float kKonsumCircle = -0.25f;      // sanfter Dauerkreis (Radius rund 4000), damit der Vogel im Feld bleibt

struct KonsumRun {
    State st;
    uint32_t startTick = 0;
    bool startDone = false;
    bool closeDone = false;
    bool menuBDone = false;
    bool dismountDone = false;  // Absteige-B des Tests gegeben (Landeanflug begonnen oder Ritt beendet)
    uint32_t bTries = 0;        // Absteige-B, solange kein Landeanflug laeuft (Oberflaeche kann B noch halten)
    uint32_t lastBTick = 0;
    uint32_t pausedTicks = 0;
    uint32_t menuDismount = 0;  // Ritt-Ende waehrend der Menue-Phase vor dem eigenen B (darf nicht sein)
    int wolf0 = -1;
    uint32_t wolfChanged = 0;
    uint32_t howlTicks = 0;
    uint32_t metaTicks = 0;
    bool done = false;
};
KonsumRun g_kr;

void konsumFinish(const char* why) {
    KonsumRun& r = g_kr;
    if (r.done) {
        return;
    }
    r.done = true;
    r.st.stage = Stage::Done;
    const bool keysOk = konsumSummary("eingabe-konsum", true);
    const bool linkOk = r.wolfChanged == 0 && r.howlTicks == 0 && r.metaTicks == 0;
    const bool menuOk = r.startDone && r.pausedTicks > 0 && r.menuDismount == 0;
    const bool ok = keysOk && linkOk && menuOk && r.dismountDone && autotest::foreignInputCount() == 0;
    logInfo("tempo: SZENARIO eingabe-konsum ende (%s) wolf0=%d wolfWechsel=%u heulenTicks=%u verwandlungTicks=%u "
            "pauseTicks=%u menueAbstieg=%u absteigen=%d dash=%u bremse=%u gangHoch=%u gangRunter=%u fremdeingabe=%u -> %s",
        why, r.wolf0, r.wolfChanged, r.howlTicks, r.metaTicks, r.pausedTicks, r.menuDismount, r.dismountDone ? 1 : 0,
        g_t.dashes, g_t.brakes, g_t.gearUps, g_t.gearDowns, autotest::foreignInputCount(),
        ok ? "BESTANDEN" : "NICHT BESTANDEN");
}

// Link-Zustand je Tick: Wolf-Form und Proc (Heulen 0xED/0xFB, Verwandlung 0x14D/0x14E).
void konsumWatchLink(KonsumRun& r) {
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return;
    }
    const int wolf = link->checkWolf() ? 1 : 0;
    if (r.wolf0 < 0) {
        r.wolf0 = wolf;
    } else if (wolf != r.wolf0) {
        ++r.wolfChanged;
    }
    const int proc = static_cast<int>(link->mProcID);
    if (proc == daAlink_c::PROC_WOLF_HOWL_DEMO || proc == daAlink_c::PROC_WOLF_HOWL) {
        ++r.howlTicks;
    }
    if (proc == daAlink_c::PROC_METAMORPHOSE || proc == daAlink_c::PROC_METAMORPHOSE_ONLY) {
        ++r.metaTicks;
    }
}

bool konsumTick(autotest::Context& c) {
    KonsumRun& r = g_kr;
    if (r.done) {
        return true;
    }
    const int phase = callPhase(c, r.st, "eingabe-konsum", &konsumFinish);
    if (phase != 1) {
        return phase == 2;
    }
    konsumWatchLink(r);
    const uint32_t t = c.frame;
    const bool paused = dComIfGp_isPauseFlag() != 0;
    if (paused) {
        ++r.pausedTicks;
    }

    if (c.sess.phase == Phase::Dismounting) {
        if (r.bTries == 0) {
            ++r.menuDismount;  // Ritt endete ohne das eigene B
            c.note("eingabe-konsum: Ritt endete ohne das Absteige-B des Tests");
        }
        r.dismountDone = true;
        if (c.sess.departFrames >= kDepartEnd) {
            konsumFinish("Abstieg abgeschlossen");
            return true;
        }
        return false;
    }
    if (c.sess.phase != Phase::Riding) {
        konsumFinish(r.bTries > 0 ? "Ritt beendet" : "Ritt ohne Abstieg beendet");
        return true;
    }
    if (c.sess.landing) {
        r.dismountDone = true;  // Landeanflug laeuft: kein weiteres B (zweites B = sofortige Freigabe)
    }

    char buf[200];
    if (!r.startDone) {
        const uint32_t f = t - r.st.base;
        float cx = 0.0f;
        uint32_t held = 0;
        bool holdR = false;
        for (const KStep& s : kKonsumSteps) {
            if (f == s.from) {
                std::snprintf(buf, sizeof(buf), "eingabe-konsum: %s f=%u", s.note, f);
                c.note(buf);
                if (s.tap != 0) {
                    c.tap(s.tap);
                }
            }
            if (f >= s.from && f < s.to) {
                if (s.hold == kBitR) {
                    holdR = true;
                } else {
                    held |= s.hold;
                }
                if (s.cStickX != 0.0f) {
                    cx = s.cStickX;
                }
            }
        }
        if (holdR) {
            c.holdR();
        }
        if (held != 0) {
            c.hold(held);
        }
        c.cStick(cx, 0.0f);
        c.stick(kKonsumCircle, 0.0f);
        if (f == kKonsumStartAt) {
            c.tap(kBitStart);
            r.startDone = true;
            r.startTick = t;
            c.note("eingabe-konsum: START (Menue auf, Start bleibt beim Spiel)");
        }
        return false;
    }

    // Menue-Phase in Kern-Ticks seit Start: im Menue stehen Vogel und Ritt-Frames still.
    const uint32_t d = t - r.startTick;
    if (d > 0 && d % 60 == 0 && !r.dismountDone) {
        logInfo("eingabe-konsum: Menue-Stand d=%u phase=%d rideFrames=%u pause=%d", d, static_cast<int>(c.sess.phase),
            c.sess.rideFrames, paused ? 1 : 0);
    }
    if (d == kKonsumMenuB) {
        c.tapB();
        r.menuBDone = true;
        std::snprintf(buf, sizeof(buf), "eingabe-konsum: B im Menue (darf nicht absteigen) pause=%d", paused ? 1 : 0);
        c.note(buf);
    }
    if (d == kKonsumMenuClose && paused) {
        c.tap(kBitStart);
        r.closeDone = true;
        c.note("eingabe-konsum: START (Menue zu)");
    }
    // Absteige-B, sobald das Menue zu ist. Haelt die Oberflaeche B noch (Fenster, E5), folgt alle 30 Ticks ein neues B,
    // aber nie waehrend eines Landeanflugs.
    if (!r.dismountDone && d >= kKonsumDismount && !paused && (r.bTries == 0 || t - r.lastBTick >= 30)) {
        c.tapB();
        ++r.bTries;
        r.lastBTick = t;
        std::snprintf(buf, sizeof(buf), "eingabe-konsum: B (absteigen, Landeanflug) Versuch %u", r.bTries);
        c.note(buf);
    }
    if (!r.dismountDone && d >= kKonsumMenuTimeout) {
        konsumFinish("Menue-Phase ohne Absteigen (Zeitlimit)");
        return true;
    }
    c.stick(0.0f, 0.0f);
    return false;
}

const autotest::Scenario kKonsumScenario = {
    "eingabe-konsum",
    "WP8.11: Ritt-Tasten vor und nach dem Konsum, Fassrolle, Dash, Bremse, Gang, Kamera, R+Y, R+X, Start-Menue, B",
    &konsumTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "schweben-demo" (KFF_AUTOTEST=schweben-demo, Entwurf H1 6.1, Zeitplan in Ritt-Ticks ab dem Ritt-Start wie
// "tempo"). Gang 3, dann Gang 1 und Schweben (Tick 100). Bremse (Z, Tick 110 bis 170), A und Y im Schweben ohne Wirkung.
// Stand, Drehen, Sinken und Steigen mit dem linken Stick Y. C-Stick rundum, hoch auf 70 und tief auf -70 Grad, dann kein
// Rueckschwenk. Gang hoch (Austritt in Gang 1), wieder Schweben, Dash aus dem Schweben, Sperrzeit, B zum Absteigen.
// Marken: "tempo: SNAP <name>" (Screenshots), "tempo: MESS <name>", Ende "schweben-demo: SZENARIO ende (...)".
// ---------------------------------------------------------------------------------------------
constexpr float kSwLift = 1500.0f;  // nach dem Ritt-Start angehoben (Muster landung-hoch, dort 8000)
constexpr uint32_t kSwLiftAt = 30;
constexpr uint32_t kSwZFrom = 110;  // Bremse gehalten im Schweben
constexpr uint32_t kSwZTo = 170;

struct SwTap {
    uint32_t at;
    uint32_t bits;
    const char* note;
};
constexpr SwTap kSwTaps[] = {
    {40, kBitDown, "gang 3 -> 2"},
    {60, kBitDown, "gang 2 -> 1"},
    {100, kBitDown, "gang 1 -> Schweben"},
    {120, kBitA, "A bei gehaltener Bremse (muss abgelehnt werden)"},
    {125, kBitY, "Y im Schweben (keine Fassrolle)"},
    {1140, kBitUp, "gang hoch: Schweben aus, Gang 1"},
    {1200, kBitDown, "gang runter: wieder Schweben"},
    {1260, kBitA, "A im Schweben: Dash aus dem Schweben"},
    {1330, kBitA, "A in der Sperrzeit (muss abgelehnt werden)"},
    {1420, kBitB, "B: absteigen"},
};

struct SwSeg {
    uint32_t from;
    uint32_t to;  // exklusiv
    float stickX;
    float stickY;
    float cStickX;
    float cStickY;
    const char* note;
};
constexpr SwSeg kSwSegs[] = {
    {160, 280, -1.0f, 0.0f, 0.0f, 0.0f, "Stick links: Drehen auf der Stelle"},
    {280, 340, 0.0f, 1.0f, 0.0f, 0.0f, "Stick hoch (Schalter aus): sinken"},
    {340, 400, 0.0f, -1.0f, 0.0f, 0.0f, "Stick runter (Schalter aus): steigen"},
    {420, 660, 0.0f, 0.0f, 1.0f, 0.0f, "C-Stick rechts: Orbit 360 Grad"},
    // Haltezeiten: die Neigung aendert sich um 0,7 Grad je Ritt-Tick. Im Lauf h2-t1-a erreichte die Haltezeit 660 bis 740
    // nur 76 Ritt-Ticks (67,2 Grad), weil Szenario-Frame und Ritt-Tick auseinanderlaufen. Daher bis 765 und ab 775.
    {660, 765, 0.0f, 0.0f, 0.0f, 1.0f, "C-Stick hoch: Neigung bis 70 Grad (Klemme ab etwa 745)"},
    {775, 1000, 0.0f, 0.0f, 0.0f, -1.0f, "C-Stick runter: Neigung bis -70 Grad (Klemme ab etwa 975)"},
};

struct SwMeas {
    uint32_t begin;
    uint32_t end;
    const char* name;
};
constexpr SwMeas kSwMeas[] = {
    {135, 150, "stand"},  // nach dem Uebergang (Ende Tick 130)
    {160, 280, "dreht"},
    {300, 338, "sinkt"},  // nach dem Anlauf der senkrechten Rate (16 Ticks)
    {372, 400, "steigt"},
    {1000, 1140, "kein-rueckschwenk"},
};

struct SwSnap {
    uint32_t at;
    const char* name;
};
constexpr SwSnap kSwSnaps[] = {
    {150, "stand"},          {280, "dreht-ende"},      {320, "sinkt"},        {385, "steigt"},
    {480, "orbit-90"},       {540, "orbit-180"},       {660, "orbit-360"},    {760, "oben-70"},
    {1010, "unten-70"},      {1080, "kein-rueckschwenk"}, {1170, "austritt"}, {1280, "dash"},
    {1330, "dash-sperre"},
};

State g_sw;  // Zustand des Szenarios schweben-demo

void swFinish(const char* why) {
    g_sw.stage = Stage::Done;
    logInfo("schweben-demo: SZENARIO ende (%s) fremdeingabe=%u dash=%u abgelehnt=%u gangHoch=%u gangRunter=%u "
            "bremse=%u effMax=%.3f fovMax=%.2f measMax=%.2f",
        why, autotest::foreignInputCount(), g_t.dashes, g_t.dashRefused, g_t.gearUps, g_t.gearDowns, g_t.brakes,
        static_cast<double>(g_t.effMax), static_cast<double>(g_t.fovMax), static_cast<double>(g_t.measMax));
}

bool schwebenTick(autotest::Context& c) {
    State& st = g_sw;
    const int phase = callPhase(c, st, "schweben-demo", &swFinish);
    if (phase != 1) {
        return phase == 2;
    }

    // Ritt-Phase: Zeitplan nach Ritt-Ticks (Kern-Ticks seit dem Ritt-Beginn).
    if (c.sess.phase == Phase::Dismounting) {
        if (c.sess.departFrames >= kDepartEnd) {
            swFinish("Abstieg abgeschlossen");
            return true;
        }
        return false;
    }
    if (c.sess.phase != Phase::Riding) {
        swFinish("Ritt ohne Abstieg beendet");
        return true;
    }

    const uint32_t f = c.frame - st.base;
    char buf[220];
    if (f == kSwLiftAt) {
        if (daKago_c* k = liveBird()) {
            const float y0 = k->current.pos.y;
            k->current.pos.y += kSwLift;
            logInfo("schweben-demo: Vogel um %.0f angehoben, y %.1f -> %.1f", static_cast<double>(kSwLift),
                static_cast<double>(y0), static_cast<double>(k->current.pos.y));
        } else {
            logInfo("schweben-demo: kein Vogel zum Anheben");
        }
    }
    for (const SwTap& tap : kSwTaps) {
        if (tap.at == f) {
            c.tap(tap.bits);
            std::snprintf(buf, sizeof(buf), "schweben-demo: TASTE %s f=%u gang=%d hover=%d", tap.note, f, g_t.gear + 1,
                g_t.hover ? 1 : 0);
            c.note(buf);
        }
    }
    float sx = 0.0f;
    float sy = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    for (const SwSeg& s : kSwSegs) {
        if (f == s.from) {
            std::snprintf(buf, sizeof(buf), "schweben-demo: BEGINN %s f=%u", s.note, f);
            c.note(buf);
        }
        if (f == s.to) {
            std::snprintf(buf, sizeof(buf), "schweben-demo: ENDE %s f=%u", s.note, f);
            c.note(buf);
        }
        if (f >= s.from && f < s.to) {
            sx = s.stickX;
            sy = s.stickY;
            cx = s.cStickX;
            cy = s.cStickY;
        }
    }
    if (f >= kSwZFrom && f < kSwZTo) {
        c.hold(kBitZ);
    }
    c.stick(sx, sy);
    c.cStick(cx, cy);

    for (const SwMeas& m : kSwMeas) {
        if (f == m.begin) {
            measureBegin();
        }
        if (f == m.end) {
            measureEnd(m.name);
        }
    }
    for (const SwSnap& s : kSwSnaps) {
        if (f == s.at) {
            std::snprintf(buf, sizeof(buf), "tempo: SNAP %s f=%u gang=%d hover=%d eff=%.3f fov=%.2f wall=%lld", s.name, f,
                g_t.gear + 1, g_t.hover ? 1 : 0, static_cast<double>(g_t.eff),
                static_cast<double>(session().fovExtraDeg), static_cast<long long>(std::time(nullptr)));
            c.note(buf);
        }
    }
    return false;
}

const autotest::Scenario kSchwebenScenario = {
    "schweben-demo",
    "Schweben (Entwurf H1): Stand, Drehen, Sinken und Steigen, C-Stick rundum, hoch und tief, Austritt, Dash, B",
    &schwebenTick,
};

// Nur Testbuild: die H2-Szenarien werden nur registriert, wenn KFF_AUTOTEST sie nennt. Die Szenarienliste hat 96 Plaetze;
// mit allen drei zusaetzlich schlug die Registrierung des Moduls autowolf (zuletzt installiert) fehl (MOD_ERROR).
bool h2Wants(const char* name) {
    const char* s = std::getenv("KFF_AUTOTEST");
    return s != nullptr && std::strcmp(s, name) == 0;
}

// Szenario "schweben-wasser" (H2, Test T4): Gang 3 fliegt geradeaus (Flug wie wp2-b701, dort liegt bei Ritt-Frame 701 ein
// See ohne Grund). Ausloeser der drei Tasten Gang 3 -> 2 -> 1 -> Schweben (Abstand kWbGap Ticks), danach 90 Ticks ohne
// Stick und B (Landeanflug). Nur Beobachtung. Ausloeser per Umgebungsvariable (nur Testbuild):
//   KFF_WB_LOOK=<Einheiten>  Die Bodensonde <Einheiten> voraus (Blickrichtung des Vogels) findet keinen Boden.
//   KFF_WB_T0=<Tick>         Fester Zeitpunkt (Vorgabe 600).
// Protokoll: POS-Zeilen alle 2 Ticks ab Tick 300 (Lage, Boden unter dem Vogel, Gang, Faktor).
constexpr uint32_t kWbGap = 20;      // Abstand der drei Tasten
constexpr uint32_t kWbHold = 90;     // Ticks ohne Stick im Schweben, danach B
constexpr uint32_t kWbLimit = 1500;  // ohne Ausloeser bis hier: Abbruch

struct WbRun {
    bool started = false;    // Ritt-Beginn protokolliert
    bool armed = false;      // Ausloeser erreicht, erste Taste gesetzt
    uint32_t t0 = 0;         // Tick der ersten Taste
    uint32_t bAt = 0;        // Tick der B-Taste, relativ zu t0
    float look = 0.0f;       // KFF_WB_LOOK (0 = fester Zeitpunkt)
    uint32_t fixedT0 = 600;  // KFF_WB_T0
};
WbRun g_wbr;

float wbEnv(const char* name, float dflt) {
    const char* s = std::getenv(name);
    return (s != nullptr && *s != '\0') ? static_cast<float>(std::atof(s)) : dflt;
}

// Bodenhoehe unter einem Punkt wie groundBelow (Sonde 500 darueber). false: kein Boden (Wasser ohne Grund oder Abgrund).
bool wbGroundAt(float x, float y, float z, float* gy) {
    dBgS_GndChk chk;
    cXyz p(x, y + 500.0f, z);
    chk.SetPos(&p);
    *gy = dComIfG_Bgsp().GroundCross(&chk);
    return *gy > -1.0e5f;
}

void wbNote(autotest::Context& c, const char* what, uint32_t f) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "schweben-wasser: TASTE %s f=%u gang=%d hover=%d", what, f, g_t.gear + 1,
        g_t.hover ? 1 : 0);
    c.note(buf);
}

void wbSnap(autotest::Context& c, const char* name, uint32_t f) {
    char buf[220];
    std::snprintf(buf, sizeof(buf), "tempo: SNAP %s f=%u gang=%d hover=%d eff=%.3f fov=%.2f wall=%lld", name, f,
        g_t.gear + 1, g_t.hover ? 1 : 0, static_cast<double>(g_t.eff), static_cast<double>(session().fovExtraDeg),
        static_cast<long long>(std::time(nullptr)));
    c.note(buf);
}

State g_wb;  // Zustand des Szenarios schweben-wasser

void wbFinish(const char* why) {
    g_wb.stage = Stage::Done;
    logInfo("schweben-wasser: SZENARIO ende (%s) fremdeingabe=%u hover=%d", why, autotest::foreignInputCount(),
        g_t.hover ? 1 : 0);
}

bool wasserTick(autotest::Context& c) {
    State& st = g_wb;
    const int phase = callPhase(c, st, "schweben-wasser", &wbFinish);
    if (phase != 1) {
        return phase == 2;
    }
    if (c.sess.phase == Phase::Dismounting) {
        if (c.sess.departFrames >= kDepartEnd) {
            wbFinish("Abstieg abgeschlossen");
            return true;
        }
        return false;
    }
    if (c.sess.phase != Phase::Riding) {
        wbFinish("Ritt ohne Abstieg beendet");
        return true;
    }
    const uint32_t f = c.frame - st.base;
    if (!g_wbr.started) {
        g_wbr.started = true;
        g_wbr.look = wbEnv("KFF_WB_LOOK", 0.0f);
        g_wbr.fixedT0 = static_cast<uint32_t>(wbEnv("KFF_WB_T0", 600.0f));
        logInfo("schweben-wasser: START modus=%s look=%.1f t0=%u", g_wbr.look > 0.0f ? "sonde" : "fest",
            static_cast<double>(g_wbr.look), g_wbr.fixedT0);
    }
    c.stick(0.0f, 0.0f);
    c.cStick(0.0f, 0.0f);
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const float px = k->current.pos.x;
    const float py = k->current.pos.y;
    const float pz = k->current.pos.z;
    const float yaw = static_cast<float>(k->current.angle.y) * (kPi / 32768.0f);
    if (!g_wbr.armed) {
        bool trig = false;
        if (g_wbr.look > 0.0f) {
            float gyAhead = 0.0f;
            trig = f >= 60 && !wbGroundAt(px + std::sin(yaw) * g_wbr.look, py, pz + std::cos(yaw) * g_wbr.look, &gyAhead);
        } else {
            trig = f >= g_wbr.fixedT0;
        }
        if (trig) {
            float gyB = 0.0f;
            const bool bodenU = wbGroundAt(px, py, pz, &gyB);
            g_wbr.armed = true;
            g_wbr.t0 = f;
            g_wbr.bAt = 2 * kWbGap + 30 + kWbHold;
            logInfo("schweben-wasser: AUSLOESER f=%u pos=(%.1f,%.1f,%.1f) yaw=%d boden_unter_vogel=%d gy=%.1f modus=%s", f,
                static_cast<double>(px), static_cast<double>(py), static_cast<double>(pz),
                static_cast<int>(k->current.angle.y), bodenU ? 1 : 0, static_cast<double>(gyB),
                g_wbr.look > 0.0f ? "sonde" : "fest");
        } else if (f >= kWbLimit) {
            wbFinish("kein Ausloeser bis Tick 1500");
            return true;
        }
    }
    if (f >= 300 && (f % 2) == 0) {
        float gy = 0.0f;
        const bool boden = wbGroundAt(px, py, pz, &gy);
        logInfo("schweben-wasser: POS f=%u x=%.1f y=%.1f z=%.1f yaw=%d boden=%d gy=%.1f gang=%d hover=%d eff=%.3f", f,
            static_cast<double>(px), static_cast<double>(py), static_cast<double>(pz),
            static_cast<int>(k->current.angle.y), boden ? 1 : 0, static_cast<double>(gy), g_t.gear + 1,
            g_t.hover ? 1 : 0, static_cast<double>(g_t.eff));
    }
    if (g_wbr.armed) {
        const uint32_t d = f - g_wbr.t0;
        if (d == 0) {
            c.tap(kBitDown);
            wbNote(c, "gang 3 -> 2 (Ausloeser)", f);
            wbSnap(c, "see-flug", f);
        } else if (d == kWbGap) {
            c.tap(kBitDown);
            wbNote(c, "gang 2 -> 1", f);
        } else if (d == 2 * kWbGap) {
            c.tap(kBitDown);
            wbNote(c, "gang 1 -> Schweben", f);
        } else if (d == 2 * kWbGap + 60) {
            wbSnap(c, "see-schweben", f);
        } else if (d == g_wbr.bAt - 10) {
            wbSnap(c, "see-vor-b", f);
        } else if (d == g_wbr.bAt) {
            c.tap(kBitB);
            wbNote(c, "B: absteigen", f);
        } else if (d == g_wbr.bAt + 60) {
            wbSnap(c, "see-abstieg", f);
        }
    }
    return false;
}

const autotest::Scenario kWasserScenario = {
    "schweben-wasser",
    "Schweben ueber dem See (Test T4): Ausloeser KFF_WB_LOOK (Sonde ohne Boden) oder KFF_WB_T0 (Zeit), 90 Ticks ohne Stick, dann B. Nur Beobachtung",
    &wasserTick,
};

// Szenario "schweben-dokument" (H2, Test Nr. 12 aus Entwurf 6.2): Schweben ab Tick 100, Pausenmenue (START) von Tick 200
// bis 520 offen (320 Ticks), danach B zum Schliessen. Schweben soll danach unveraendert sein, kein Stillstandsabbruch.
// Beleg fuer Frage 7.13: Im Menue laufen keine Vogel-Ticks (MESS "dok-menue" ticks) oder doch (MT-Zeilen im Menue).
constexpr SwTap kDokTaps[] = {
    {40, kBitDown, "gang 3 -> 2"},
    {60, kBitDown, "gang 2 -> 1"},
    {100, kBitDown, "gang 1 -> Schweben"},
    {200, kBitStart, "START: Pausenmenue auf (Dokument)"},
    {520, kBitB, "B: Pausenmenue zu"},
    {1100, kBitB, "B: absteigen"},
};
constexpr SwMeas kDokMeas[] = {
    {120, 150, "dok-vor"},
    {200, 520, "dok-menue"},
    {540, 600, "dok-nach"},
};
constexpr SwSnap kDokSnaps[] = {
    {150, "dok-vor"}, {300, "dok-menue"}, {530, "dok-nach-menue"}, {700, "dok-hover"},
};

State g_dk;  // Zustand des Szenarios schweben-dokument

void dokFinish(const char* why) {
    g_dk.stage = Stage::Done;
    logInfo("schweben-dokument: SZENARIO ende (%s) fremdeingabe=%u hover=%d", why, autotest::foreignInputCount(),
        g_t.hover ? 1 : 0);
}

bool dokumentTick(autotest::Context& c) {
    State& st = g_dk;
    const int phase = callPhase(c, st, "schweben-dokument", &dokFinish);
    if (phase != 1) {
        return phase == 2;
    }
    if (c.sess.phase == Phase::Dismounting) {
        if (c.sess.departFrames >= kDepartEnd) {
            dokFinish("Abstieg abgeschlossen");
            return true;
        }
        return false;
    }
    if (c.sess.phase != Phase::Riding) {
        dokFinish("Ritt ohne Abstieg beendet");
        return true;
    }
    const uint32_t f = c.frame - st.base;
    char buf[220];
    for (const SwTap& tap : kDokTaps) {
        if (tap.at == f) {
            c.tap(tap.bits);
            std::snprintf(buf, sizeof(buf), "schweben-dokument: TASTE %s f=%u gang=%d hover=%d", tap.note, f, g_t.gear + 1,
                g_t.hover ? 1 : 0);
            c.note(buf);
        }
    }
    for (const SwMeas& m : kDokMeas) {
        if (f == m.begin) {
            measureBegin();
        }
        if (f == m.end) {
            measureEnd(m.name);
        }
    }
    for (const SwSnap& s : kDokSnaps) {
        if (f == s.at) {
            std::snprintf(buf, sizeof(buf), "tempo: SNAP %s f=%u gang=%d hover=%d eff=%.3f fov=%.2f wall=%lld", s.name, f,
                g_t.gear + 1, g_t.hover ? 1 : 0, static_cast<double>(g_t.eff),
                static_cast<double>(session().fovExtraDeg), static_cast<long long>(std::time(nullptr)));
            c.note(buf);
        }
    }
    c.stick(0.0f, 0.0f);
    c.cStick(0.0f, 0.0f);
    return false;
}

const autotest::Scenario kDokumentScenario = {
    "schweben-dokument",
    "Schweben, Pausenmenue (Dokument) 320 Ticks offen, danach Schweben weiter, B",
    &dokumentTick,
};

// ---------------------------------------------------------------------------------------------
// H4 (Schweben, Testluecken B1L bis B14). Nur Testbuild, Auswahl ueber KFF_AUTOTEST=h4-<name>.
// Je Ritt-Tick zwei Zeilen: "h4: TR <name> ..." (Vogel: Lage, Gang, Faktor, Sonden) und "h4: IN <name> ..."
// (Eingaben, Link, Leben, Fremdeingaben). Nach dem Ritt "h4: POST <name> ..." bis kH4PostTicks.
// Sonden: Boden und Wasser ab y+500 (groundBelowLink und waterBelowPoint in ride.cpp), Decke ab y-500
// (wp6RoofAbove in freiheit.cpp), Wand per LinChk (Wandprofil in freiheit.cpp). Kein Treffer: kH4None.
// ---------------------------------------------------------------------------------------------
constexpr float kH4None = -1.0e8f;      // wie kNoGround (freiheit.cpp)
constexpr float kH4Lift = 1500.0f;      // Anhebung wie schweben-demo (kSwLift)
constexpr float kH4Pi = 3.14159265f;
constexpr uint32_t kH4PostTicks = 400;  // Nachlauf nach dem Ende des Ritts (Abstieg, Vogel weg, Void-Out)
constexpr uint32_t kH4W2Max = 1500;     // B12 W2: ohne Wasser ohne Grund bis hier, dann Schweben trotzdem

// Ein Szenario laeuft je Prozess (eine KFF_AUTOTEST-Auswahl), daher ein gemeinsamer Zustand.
struct H4St {
    State st;            // Ruf und Ritt (wie schweben-demo)
    uint32_t post = 0;   // Ticks nach dem Ritt
    uint32_t mode = 0;   // Ablaufmodus des Szenarios
    uint32_t t1 = 0;     // Marken in Ritt-Ticks
    uint32_t t2 = 0;
    uint32_t t3 = 0;
    uint32_t still = 0;  // Ticks ohne Hoehenaenderung (B9)
    float fx = 0.0f;     // Richtung zur Wand (B4b)
    float fz = 0.0f;
    float fy = 0.0f;     // letzte Hoehe (B9)
    bool flag = false;   // Ausloeser erreicht (B6M Fenster offen, B12 W2)
};
H4St g_h4;

// ---- Sonden
// Boden ab dem Start startY (GndChk). kH4None ohne Boden.
float h4GroundFrom(float x, float startY, float z) {
    dBgS_GndChk gnd;
    const cXyz p(x, startY, z);
    gnd.SetPos(&p);
    const float g = dComIfG_Bgsp().GroundCross(&gnd);
    return g > -1.0e5f ? g : kH4None;
}

// Wasserflaeche ab dem Start startY (ObjGndChk_Spl). kH4None ohne Wasser.
float h4WaterFrom(float x, float startY, float z) {
    dBgS_ObjGndChk_Spl wsp;
    const cXyz p(x, startY, z);
    wsp.SetPos(&p);
    const float w = dComIfG_Bgsp().GroundCross(&wsp);
    return w > -1.0e5f ? w : kH4None;
}

float h4Ground(float x, float y, float z) {
    return h4GroundFrom(x, y + 500.0f, z);
}

float h4Water(float x, float y, float z) {
    return h4WaterFrom(x, y + 500.0f, z);
}

// Decke ueber dem Punkt: RoofChk ab der Hoehe y selbst. Frueher ab y-500 (wie wp6RoofAbove): liegt der Vogel mehr als 500
// ueber dem Gelaende, trifft der Strahl dann den Boden und meldet ihn als Decke (Lauf h4-b4a-r1). kH4None ohne Decke.
float h4Roof(float x, float y, float z) {
    dBgS_RoofChk rc;
    const cXyz p(x, y, z);
    rc.SetPos(p);
    const float r = dComIfG_Bgsp().RoofChk(&rc);
    return (r < 1.0e5f && r > -1.0e5f) ? r : kH4None;
}

// Kleinster Abstand Decke minus y: nur der Mittelpunkt (rad 0) oder acht Punkte im Kreis mit Radius rad um den Vogel. Der
// Kreis erfasst die Randlage des Vogel-Zylinders, die der Mittelpunkt-Strahl nicht sieht (Befund B4a). kH4None ohne Decke.
float h4CeilGap(float x, float y, float z, float rad) {
    static const float kDirX[8] = {1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f};
    static const float kDirZ[8] = {0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f};
    float best = kH4None;
    const int n = rad > 0.0f ? 8 : 1;
    for (int i = 0; i < n; ++i) {
        const float r = rad > 0.0f ? h4Roof(x + rad * kDirX[i], y, z + rad * kDirZ[i]) : h4Roof(x, y, z);
        if (r != kH4None && (best == kH4None || r < best)) {
            best = r;
        }
    }
    return best == kH4None ? kH4None : best - y;
}

// Abstand zur Wand von (x, y, z) in Richtung (dx, dz), hoechstens len. kH4None ohne Treffer (LinChk wie freiheit.cpp).
float h4Wall(float x, float y, float z, float dx, float dz, float len) {
    const cXyz s(x, y, z);
    const cXyz e(x + dx * len, y, z + dz * len);
    dBgS_LinChk lin;
    lin.Set(&s, &e, nullptr);
    if (!dComIfG_Bgsp().LineCross(&lin)) {
        return kH4None;
    }
    const cXyz q = lin.GetCross();
    return std::sqrt((q.x - x) * (q.x - x) + (q.z - z) * (q.z - z));
}

// Abgrund wie findVoid (freiheit.cpp): vier Richtungen, Abstaende dist, 1,5 dist und 2 dist, Umfeld 3 x 3 mal 800 ohne Boden.
bool h4FindVoid(const cXyz& from, float dist, cXyz* out) {
    const float dirs[4][2] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
    const float mults[3] = {1.0f, 1.5f, 2.0f};
    for (const auto& d : dirs) {
        for (const float m : mults) {
            const float x = from.x + d[0] * dist * m;
            const float z = from.z + d[1] * dist * m;
            bool empty = true;
            for (int i = -1; i <= 1 && empty; ++i) {
                for (int j = -1; j <= 1 && empty; ++j) {
                    const float gy = h4GroundFrom(x + 800.0f * static_cast<float>(i), 60000.0f, z + 800.0f * static_cast<float>(j));
                    if (gy != kH4None) {
                        empty = false;
                    }
                }
            }
            if (empty) {
                *out = cXyz(x, from.y, z);
                return true;
            }
        }
    }
    return false;
}

struct H4Spot {
    bool found = false;
    float x = 0.0f;
    float z = 0.0f;
    float waterY = 0.0f;
    float groundY = 0.0f;
};

// Naechster Wasserpunkt um from (Raster 30 x 1000, wie f6FindLake in autotest.cpp). wantGround: Grund unter der
// Flaeche verlangt (W1), sonst kein Grund darunter (W2).
H4Spot h4FindWater(const cXyz& from, bool wantGround) {
    H4Spot best;
    int bestD = 1 << 30;
    constexpr int kN = 30;
    constexpr float kStep = 1000.0f;
    for (int ix = -kN; ix <= kN; ++ix) {
        for (int iz = -kN; iz <= kN; ++iz) {
            const int d = ix * ix + iz * iz;
            if (d >= bestD) {
                continue;
            }
            const float x = from.x + static_cast<float>(ix) * kStep;
            const float z = from.z + static_cast<float>(iz) * kStep;
            const float wy = h4WaterFrom(x, from.y + 20000.0f, z);
            if (wy == kH4None) {
                continue;
            }
            const float gy = h4GroundFrom(x, wy - 1.0f, z);
            const bool ok = wantGround ? (gy != kH4None && wy - gy >= 100.0f) : (gy == kH4None);
            if (!ok) {
                continue;
            }
            best.found = true;
            best.x = x;
            best.z = z;
            best.waterY = wy;
            best.groundY = gy;
            bestD = d;
        }
    }
    return best;
}

// ---- Ablauf
// Ruf-Ende ohne Ritt (callPhase): Szenario beenden und die Ursache protokollieren.
void h4Fin(const char* why) {
    g_h4.st.stage = Stage::Done;
    logInfo("h4: ABBRUCH (%s) fremdeingabe=%u", why, autotest::foreignInputCount());
}

enum class H4Step : uint8_t { Wait, Ride, End };

// Ruf (callPhase), danach Nachlauf nach dem Ritt. Rueckgabe Ride: der Zeitplan laeuft in diesem Tick.
H4Step h4Step(autotest::Context& c, const char* tag) {
    const int ph = callPhase(c, g_h4.st, tag, &h4Fin);
    if (ph == 2) {
        return H4Step::End;
    }
    if (ph == 0) {
        return H4Step::Wait;
    }
    if (c.sess.phase == Phase::Riding) {
        return H4Step::Ride;
    }
    ++g_h4.post;
    daAlink_c* link = liveLink();
    const float lky = link != nullptr ? link->current.pos.y : 0.0f;
    const float lkgy = link != nullptr ? h4Ground(link->current.pos.x, link->current.pos.y, link->current.pos.z) : kH4None;
    const int wet = (link != nullptr && link->mLinkAcch.ChkWaterHit()) ? 1 : 0;
    const int swim = (link != nullptr && link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0) ? 1 : 0;
    logInfo("h4: POST %s n=%u ph=%d vogel=%d leben=%d lky=%.1f lkgy=%.1f wet=%d swim=%d ev=%d fremd=%u", tag, g_h4.post,
        static_cast<int>(c.sess.phase), liveBird() != nullptr ? 1 : 0, static_cast<int>(dComIfGs_getLife()),
        static_cast<double>(lky), static_cast<double>(lkgy), wet, swim, dComIfGp_event_runCheck() != 0 ? 1 : 0,
        autotest::foreignInputCount());
    if (g_h4.post >= kH4PostTicks) {
        g_h4.st.stage = Stage::Done;
        logInfo("%s: SZENARIO ende (nach Ritt, POST %u) fremdeingabe=%u", tag, g_h4.post, autotest::foreignInputCount());
        return H4Step::End;
    }
    return H4Step::Wait;
}

struct H4In {
    uint32_t tap = 0;   // Flanken in diesem Tick
    uint32_t hold = 0;  // gehaltene Tasten
    float sx = 0.0f;
    float sy = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
};

void h4Apply(autotest::Context& c, const H4In& in) {
    if (in.tap != 0) {
        c.tap(in.tap);
    }
    if (in.hold != 0) {
        c.hold(in.hold);
    }
    c.stick(in.sx, in.sy);
    c.cStick(in.cx, in.cy);
}

void h4Say(autotest::Context& c, const char* tag, uint32_t f, const char* text) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%s: %s f=%u", tag, text, f);
    c.note(buf);
}

struct H4Ev {
    uint32_t at;
    uint32_t bits;
    const char* note;
};

template <unsigned N>
void h4Events(autotest::Context& c, const char* tag, uint32_t f, const H4Ev (&ev)[N], H4In& in) {
    for (const H4Ev& e : ev) {
        if (e.at == f) {
            in.tap |= e.bits;
            h4Say(c, tag, f, e.note);
        }
    }
}

struct H4Seg {
    uint32_t from;
    uint32_t to;  // exklusiv
    float sx;
    float sy;
    float cx;
    float cy;
    const char* note;
};

template <unsigned N>
void h4Segs(autotest::Context& c, const char* tag, uint32_t f, const H4Seg (&segs)[N], H4In& in) {
    for (const H4Seg& g : segs) {
        if (f == g.from) {
            h4Say(c, tag, f, g.note);
        }
        if (f >= g.from && f < g.to) {
            in.sx = g.sx;
            in.sy = g.sy;
            in.cx = g.cx;
            in.cy = g.cy;
        }
    }
}

// Anhebung um kH4Lift (Muster schweben-demo).
void h4Lift(autotest::Context& c, daKago_c* k, const char* tag, uint32_t f) {
    const float y0 = k->current.pos.y;
    k->current.pos.y += kH4Lift;
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%s: Vogel um %.0f angehoben, y %.1f -> %.1f f=%u", tag, static_cast<double>(kH4Lift),
        static_cast<double>(y0), static_cast<double>(k->current.pos.y), f);
    c.note(buf);
}

// Je Ritt-Tick: TR (Vogel) und IN (Eingaben, Link, Leben, Fremdeingaben, Ereignis). Nur Logzeilen, keine Eingabe.
void h4Trace(const char* tag, uint32_t f, daKago_c* k, const H4In& in) {
    const cXyz p = k->current.pos;
    const float th = static_cast<float>(k->current.angle.y) * kH4Pi / 32768.0f;
    const float wd = h4Wall(p.x, p.y, p.z, std::sin(th), std::cos(th), 1500.0f);
    logInfo("h4: TR %s f=%u rf=%u ph=%d hov=%d sg=%d gang=%d eff=%.4f vy=%.3f x=%.1f y=%.1f z=%.1f yaw=%d pit=%d "
            "gy=%.1f ry=%.1f wy=%.1f wd=%.1f",
        tag, f, session().rideFrames, static_cast<int>(session().phase), g_t.hover ? 1 : 0, session().gear, g_t.gear + 1,
        static_cast<double>(g_t.eff), static_cast<double>(session().att.hoverVy), static_cast<double>(p.x),
        static_cast<double>(p.y), static_cast<double>(p.z), static_cast<int>(k->current.angle.y),
        static_cast<int>(k->current.angle.x), static_cast<double>(h4Ground(p.x, p.y, p.z)),
        static_cast<double>(h4Roof(p.x, p.y, p.z)), static_cast<double>(h4Water(p.x, p.y, p.z)), static_cast<double>(wd));
    daAlink_c* link = liveLink();
    const float lky = link != nullptr ? link->current.pos.y : 0.0f;
    const float lkgy = link != nullptr ? h4Ground(link->current.pos.x, link->current.pos.y, link->current.pos.z) : kH4None;
    const int wet = (link != nullptr && link->mLinkAcch.ChkWaterHit()) ? 1 : 0;
    const int swim = (link != nullptr && link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0) ? 1 : 0;
    logInfo("h4: IN %s f=%u s=%.2f,%.2f c=%.2f,%.2f tap=%u hold=%u zb=%d lky=%.1f lkgy=%.1f wet=%d swim=%d leben=%d "
            "fremd=%u land=%d ev=%d",
        tag, f, static_cast<double>(in.sx), static_cast<double>(in.sy), static_cast<double>(in.cx),
        static_cast<double>(in.cy), in.tap, in.hold, g_t.braking ? 1 : 0, static_cast<double>(lky),
        static_cast<double>(lkgy), wet, swim, static_cast<int>(dComIfGs_getLife()), autotest::foreignInputCount(),
        session().landing ? 1 : 0, dComIfGp_event_runCheck() != 0 ? 1 : 0);
}

// Mods-Fenster wie steuerung-demo (window_push mit dem Panel-Inhalt).
UiWindowHandle g_h4Window = 0;
UiTabDesc g_h4Tab = UI_TAB_DESC_INIT;

ModResult h4Build(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle, void*, ModError*) {
    return panel::buildContents(left);
}

ModResult h4Update(ModContext*, void*, ModError*) {
    panel::updateContents();
    return MOD_OK;
}

bool h4OpenWindow() {
    if (svc_ui == nullptr) {
        return false;
    }
    g_h4Tab = UI_TAB_DESC_INIT;
    g_h4Tab.title = "Kargarok Flight (H4)";
    g_h4Tab.build = h4Build;
    g_h4Tab.update = h4Update;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &g_h4Tab;
    desc.tab_count = 1;
    return svc_ui->window_push(mod_ctx, &desc, &g_h4Window) == MOD_OK;
}

void h4CloseWindow() {
    if (svc_ui != nullptr && g_h4Window != 0) {
        (void)svc_ui->window_close(mod_ctx, g_h4Window);
        g_h4Window = 0;
    }
}

// ---- B1L: Langes Schweben. Lift bei 30, Gang 3 -> 2 -> 1 (40, 60), Schweben (80), Austritt bei 1880, B bei 1960.
// Hover von 80 bis 1880 sind 1800 Ticks (60 s). Ruhephasen ohne Eingabe: 110 bis 400, 1200 bis 1300, 1340 bis 1500,
// 1560 bis 1880.
constexpr H4Ev kH4B1Ev[] = {
    {40, kBitDown, "Gang runter (3 nach 2)"},
    {60, kBitDown, "Gang runter (2 nach 1)"},
    {80, kBitDown, "Gang 1 runter: Schweben"},
    {1880, kBitUp, "Gang hoch: Austritt in Gang 1"},
    {1960, kBitB, "B: absteigen"},
};
constexpr H4Seg kH4B1Segs[] = {
    {400, 520, -1.0f, 0.0f, 0.0f, 0.0f, "Stick links: Drehen"},
    {560, 640, 0.0f, 0.0f, 1.0f, 0.0f, "C-Stick rechts"},
    {640, 700, 0.0f, 0.0f, 0.0f, 1.0f, "C-Stick hoch"},
    {800, 860, 0.0f, 0.0f, 0.0f, -1.0f, "C-Stick runter"},
    {1000, 1100, 0.0f, 1.0f, 0.0f, 0.0f, "Stick hoch: sinken"},
    {1100, 1200, 0.0f, -1.0f, 0.0f, 0.0f, "Stick runter: steigen"},
    {1300, 1340, 1.0f, 0.0f, 0.0f, 0.0f, "Stick rechts: Drehen"},
    {1500, 1560, 0.0f, 0.0f, -1.0f, 0.0f, "C-Stick links"},
};

bool h4b1Tick(autotest::Context& c) {
    const char* tag = "h4-b1l";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f == 30) {
        h4Lift(c, k, tag, f);
    }
    h4Events(c, tag, f, kH4B1Ev, in);
    h4Segs(c, tag, f, kH4B1Segs, in);
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B1Scenario = {
    "h4-b1l",
    "H4 B1L: langes Schweben 1800 Ticks, C-Stick und Stick, Ruhephasen ohne Eingabe, dann B",
    &h4b1Tick,
};

// ---- B2: zehn Ein- und Austritte im Abstand von 40 Ticks. Lift bei 30, Gang 3 -> 1 (40, 60), dann ab 80 alle 40 Ticks
// abwechselnd Gang runter (Eintritt) und Gang hoch (Austritt, Gang 1). Letzter Austritt bei 840, B bei 900.
constexpr H4Ev kH4B2Ev[] = {
    {40, kBitDown, "Gang runter (3 nach 2)"},
    {60, kBitDown, "Gang runter (2 nach 1)"},
    {900, kBitB, "B: absteigen"},
};

bool h4b2Tick(autotest::Context& c) {
    const char* tag = "h4-b2";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f == 30) {
        h4Lift(c, k, tag, f);
    }
    h4Events(c, tag, f, kH4B2Ev, in);
    if (f >= 80 && f <= 840 && (f - 80) % 40 == 0) {
        const uint32_t i = (f - 80) / 40;  // gerade i: Eintritt, ungerade i: Austritt
        if (i % 2 == 0) {
            in.tap |= kBitDown;
            h4Say(c, tag, f, "Eintritt Schweben");
        } else {
            in.tap |= kBitUp;
            h4Say(c, tag, f, "Austritt Schweben (Gang 1)");
        }
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B2Scenario = {
    "h4-b2",
    "H4 B2: zehn Ein- und Austritte Schweben im Abstand von 40 Ticks, danach Gang 1, B",
    &h4b2Tick,
};

// ---- B4a: Schweben unter der Decke (Ort wie wp6-decke, Stage R_SP107,0,24,-1). Gang 3 -> 0 in drei Schritten (f 2, 4, 6),
// steigen (Stick runter) ab Tick 20, bis eine Decke weniger als 30 ueber dem Vogel liegt (Mitte oder Kreis mit Radius 60).
// Dann 120 Ticks weiter steigen, 40 Ticks Rast, B. Modi: 0 steigen, 1 an der Decke, 2 Rast, 3 B.
// Der Anflug bleibt nah am Startpunkt. Im Lauf h4-b4a-r1 endete der Ritt bei (-35, 616, -854) durch ein Spielereignis mit
// Szenenwechsel (Befund, siehe BERICHT.md).
bool h4b4aTick(autotest::Context& c) {
    const char* tag = "h4-b4a";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    const cXyz p = k->current.pos;
    static float hist[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // Hoehe der letzten 8 Ticks (Ringspeicher)
    if (f <= 2) {
        for (float& v : hist) {
            v = p.y;
        }
    }
    hist[f % 8] = p.y;
    H4In in;
    if (f == 2 || f == 4 || f == 6) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, f == 6 ? "Gang 0: Schweben" : "Gang runter");
    }
    if (g_h4.mode == 0 && f >= 20) {
        in.sy = -1.0f;  // Stick runter: steigen
        const float dC = h4CeilGap(p.x, p.y, p.z, 0.0f);
        const float d60 = h4CeilGap(p.x, p.y, p.z, 60.0f);
        const float dMin = std::min(dC != kH4None ? dC : 1.0e9f, d60 != kH4None ? d60 : 1.0e9f);
        logInfo("h4: RING %s f=%u y=%.1f dC=%.1f d60=%.1f (-1 = keine Decke)", tag, f, static_cast<double>(p.y),
            static_cast<double>(dC != kH4None ? dC : -1.0f), static_cast<double>(d60 != kH4None ? d60 : -1.0f));
        // Decke erreicht: Abstand unter 30 (Mitte oder Kreis) ODER Aufstieg blockiert. Die Decke trifft die Vogelspitze (etwa
        // 80 ueber der Mitte, Lauf h4-b4a-r2: Mitte 608 bis 620 bei Decke 700), daher meldet die Mitte-Sonde dann etwa 80.
        const bool blockiert = f >= 60 && p.y - hist[(f + 1) % 8] < 8.0f;  // Hoehe 7 Ticks zuvor
        if (dMin < 30.0f || blockiert) {
            g_h4.mode = 1;
            g_h4.t1 = f;
            char buf[200];
            std::snprintf(buf, sizeof(buf), "%s: Decke erreicht y=%.1f Abstand=%.1f (Mitte %.1f, Kreis60 %.1f) blockiert=%d f=%u",
                tag, static_cast<double>(p.y), static_cast<double>(dMin), static_cast<double>(dC != kH4None ? dC : -1.0f),
                static_cast<double>(d60 != kH4None ? d60 : -1.0f), blockiert ? 1 : 0, f);
            c.note(buf);
        } else if (f >= 1000) {
            g_h4.mode = 2;
            g_h4.t2 = f;
            h4Say(c, tag, f, "keine Decke bis Tick 1000, Rast");
        }
    }
    if (g_h4.mode == 1) {
        in.sy = -1.0f;
        if (f >= g_h4.t1 + 120) {
            g_h4.mode = 2;
            g_h4.t2 = f;
            h4Say(c, tag, f, "120 Ticks an der Decke vorbei, Rast");
        }
    }
    if (g_h4.mode == 2 && f >= g_h4.t2 + 40) {
        in.tap |= kBitB;
        g_h4.mode = 3;
        h4Say(c, tag, f, "B: absteigen");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B4aScenario = {
    "h4-b4a",
    "H4 B4a: Schweben unter der Decke (Ort wie wp6-decke, R_SP107), 120 Ticks steigen an der Decke, B",
    &h4b4aTick,
};

// ---- B4b: Schweben an der Wand (Ort wie wp6-wand, Stage F_SP121,10,23,-1). Bei Tick 5 Kurs auf die naechste Wand
// mit Abstand >= 300 (16 Richtungen). Gang 3 -> 1 (10, 30), Flug auf die Wand. Ab 600 Abstand Schweben (Gang runter),
// 70 Ticks Schweben, Austritt, Flug bis 40 vor die Wand oder 200 Ticks, dann Schweben an der Wand. Dort Drehen
// (links, rechts), Steigen, Sinken, B. Modi: 0 Anflug, 1 Schweben vor der Wand, 2 Austritt, 3 an der Wand, 4 B.
// B4b: Suchweite der Wandsuche je Richtung und Abstand des Vogels vor der Wand nach dem Test-Teleport. Im Lauf h4-b4b-r1
// lag am Ritt-Start keine Wand im Umkreis 1500 (naechste Wand etwa 18000 entfernt, Befund), daher Suche bis 20000 und
// Teleport auf 700 Einheiten vor die gefundene Wand (Kurs unveraendert).
constexpr float kH4WallSearch = 20000.0f;
constexpr float kH4WallStart = 700.0f;

bool h4b4bTick(autotest::Context& c) {
    const char* tag = "h4-b4b";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    const cXyz p = k->current.pos;
    H4In in;
    if (f == 5) {
        float best = -1.0f;
        int bi = -1;
        for (int i = 0; i < 16; ++i) {
            const float a = static_cast<float>(i) * 0.39269908f;
            const float d = h4Wall(p.x, p.y, p.z, std::sin(a), std::cos(a), kH4WallSearch);
            if (d >= 300.0f && (bi < 0 || d < best)) {
                best = d;
                bi = i;
            }
        }
        if (bi < 0) {
            logInfo("%s: ABBRUCH keine Wand mit Abstand >= 300 im Umkreis %.0f um (%.1f,%.1f,%.1f)", tag,
                static_cast<double>(kH4WallSearch),
                static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z));
            g_h4.st.stage = Stage::Done;
            return true;
        }
        const float a = static_cast<float>(bi) * 0.39269908f;
        g_h4.fx = std::sin(a);
        g_h4.fz = std::cos(a);
        if (best > kH4WallStart) {  // Test-Teleport: Vogel steht kH4WallStart vor der Wand, Kurs unveraendert
            const float s = best - kH4WallStart;
            k->current.pos.x = p.x + g_h4.fx * s;
            k->current.pos.z = p.z + g_h4.fz * s;
            k->old.pos = k->current.pos;
        }
        k->current.angle.y = cM_atan2s(g_h4.fx, g_h4.fz);
        k->shape_angle.y = k->current.angle.y;
        char buf[200];
        std::snprintf(buf, sizeof(buf), "%s: Kurs auf die Wand (Richtung %d, Abstand %.1f) f=%u", tag, bi,
            static_cast<double>(best), f);
        c.note(buf);
    }
    if (f == 10 || f == 30) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, "Gang runter (Anflug)");
    }
    if (g_h4.mode == 0 && f >= 40) {
        const float d = h4Wall(p.x, p.y, p.z, g_h4.fx, g_h4.fz, 1500.0f);
        if (d >= 0.0f && d <= 600.0f) {
            in.tap |= kBitDown;
            g_h4.mode = 1;
            g_h4.t1 = f;
            char buf[200];
            std::snprintf(buf, sizeof(buf), "%s: Schweben vor der Wand, Abstand %.1f f=%u", tag, static_cast<double>(d), f);
            c.note(buf);
        } else if (f >= 1000) {
            g_h4.mode = 4;
            in.tap |= kBitB;
            h4Say(c, tag, f, "keine Wand erreicht bis Tick 1000, B");
        }
    }
    if (g_h4.mode == 1 && f >= g_h4.t1 + 70) {
        in.tap |= kBitUp;
        g_h4.mode = 2;
        g_h4.t2 = f;
        h4Say(c, tag, f, "Austritt, Flug zur Wand");
    }
    if (g_h4.mode == 2) {
        const float d = h4Wall(p.x, p.y, p.z, g_h4.fx, g_h4.fz, 1500.0f);
        if ((d >= 0.0f && d <= 40.0f) || f >= g_h4.t2 + 200) {
            in.tap |= kBitDown;
            g_h4.mode = 3;
            g_h4.t3 = f;
            char buf[200];
            std::snprintf(buf, sizeof(buf), "%s: an der Wand, Abstand %.1f f=%u", tag, static_cast<double>(d), f);
            c.note(buf);
        }
    }
    if (g_h4.mode == 3) {
        const uint32_t t = g_h4.t3;
        if (f >= t && f < t + 100) {
            in.sx = -1.0f;
        } else if (f >= t + 100 && f < t + 200) {
            in.sx = 1.0f;
        } else if (f >= t + 200 && f < t + 300) {
            in.sy = -1.0f;  // steigen
        } else if (f >= t + 300 && f < t + 400) {
            in.sy = 1.0f;  // sinken
        }
        if (f == t + 100) {
            h4Say(c, tag, f, "Drehen (Gegenrichtung)");
        }
        if (f == t + 200) {
            h4Say(c, tag, f, "an der Wand steigen");
        }
        if (f == t + 300) {
            h4Say(c, tag, f, "an der Wand sinken");
        }
        if (f == t + 460) {
            in.tap |= kBitB;
            g_h4.mode = 4;
            h4Say(c, tag, f, "B: absteigen");
        }
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B4bScenario = {
    "h4-b4b",
    "H4 B4b: Schweben an der Wand (Ort wie wp6-wand, F_SP121), Drehen, Steigen und Sinken an der Wand, B",
    &h4b4bTick,
};

// ---- B5+: bind_gang_runter=6 (L) im Testprofil (KFF_CONFIG_EXTRA). L schaltet Gang runter und Schweben, D-Pad unten nicht.
constexpr H4Ev kH4B5Ev[] = {
    {40, kBitL, "L: Gang runter (3 nach 2)"},
    {60, kBitL, "L: Gang runter (2 nach 1)"},
    {80, kBitDown, "D-Pad unten in Gang 1 (ohne Wirkung erwartet)"},
    {100, kBitL, "L in Gang 1: Schweben an"},
    {160, kBitUp, "D-Pad oben: Austritt in Gang 1"},
    {180, kBitL, "L in Gang 1: Schweben wieder an"},
    {240, kBitL, "L im Schweben (ohne Wirkung erwartet)"},
    {260, kBitDown, "D-Pad unten im Schweben (ohne Wirkung erwartet)"},
    {320, kBitB, "B: absteigen"},
};

bool h4b5Tick(autotest::Context& c) {
    const char* tag = "h4-b5";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    h4Events(c, tag, f, kH4B5Ev, in);
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B5Scenario = {
    "h4-b5",
    "H4 B5+: bind_gang_runter=6 (L): L schaltet Gang runter und Schweben, D-Pad unten ohne Wirkung",
    &h4b5Tick,
};

// ---- B6M: Mods-Fenster im Schweben, 320 Ticks offen (150 bis 470), B bei 700. Lift 30, Gang 3 -> 1 (40, 60, 80).
bool h4b6Tick(autotest::Context& c) {
    const char* tag = "h4-b6";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f == 30) {
        h4Lift(c, k, tag, f);
    }
    if (f == 40 || f == 60 || f == 80) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, f == 80 ? "Gang 1 runter: Schweben" : "Gang runter");
    }
    if (f == 150) {
        g_h4.flag = h4OpenWindow();
        h4Say(c, tag, f, g_h4.flag ? "Mods-Fenster offen (window_push MOD_OK)" : "Mods-Fenster NICHT geoeffnet");
    }
    if (f == 470) {
        h4CloseWindow();
        h4Say(c, tag, f, "Mods-Fenster zu (320 Ticks offen)");
    }
    if (f == 700) {
        in.tap |= kBitB;
        h4Say(c, tag, f, "B: absteigen");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B6Scenario = {
    "h4-b6",
    "H4 B6M: Mods-Fenster (window_push, Panel-Inhalt) 320 Ticks offen im Schweben, danach B",
    &h4b6Tick,
};

// ---- B8: Bremse Z 60 Ticks (135 bis 195) im eingeschwungenen Schweben (Eintritt 80, Rampe bis etwa 110), B bei 260.
constexpr H4Ev kH4B8Ev[] = {
    {40, kBitDown, "Gang runter"},
    {60, kBitDown, "Gang runter"},
    {80, kBitDown, "Gang 1 runter: Schweben"},
    {260, kBitB, "B: absteigen"},
};

bool h4b8Tick(autotest::Context& c) {
    const char* tag = "h4-b8";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f == 30) {
        h4Lift(c, k, tag, f);
    }
    h4Events(c, tag, f, kH4B8Ev, in);
    if (f == 135) {
        h4Say(c, tag, f, "Z gehalten (Bremse im Schweben), 60 Ticks");
    }
    if (f >= 135 && f < 195) {
        in.hold |= kBitZ;
    }
    if (f == 195) {
        h4Say(c, tag, f, "Z losgelassen");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B8Scenario = {
    "h4-b8",
    "H4 B8: Bremse Z 60 Ticks ab Ritt-Tick 135 im eingeschwungenen Schweben, eff bleibt 0, dann B",
    &h4b8Tick,
};

// ---- B9: Untergrenze. Lift 30, Gang 3 -> 1 (40, 60), Schweben (80). Sinken (Stick hoch) ab 120, bis der Vogel 20 Ticks
// nicht mehr sinkt (hoechstens bis 900). 100 Ticks Rast, 150 Ticks steigen (Stick runter), 60 Ticks Rast, B.
// Modi: 0 warten, 1 sinken, 2 Rast, 3 steigen, 4 Rast, 5 B.
bool h4b9Tick(autotest::Context& c) {
    const char* tag = "h4-b9";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    const cXyz p = k->current.pos;
    H4In in;
    if (f == 30) {
        h4Lift(c, k, tag, f);
    }
    if (f == 40 || f == 60 || f == 80) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, f == 80 ? "Gang 1 runter: Schweben" : "Gang runter");
    }
    if (g_h4.mode == 0 && f >= 120) {
        g_h4.mode = 1;
        g_h4.t1 = f;
        g_h4.fy = p.y;
        g_h4.still = 0;
        h4Say(c, tag, f, "sinken beginnt (Stick hoch)");
    }
    if (g_h4.mode == 1) {
        in.sy = 1.0f;
        if (f > g_h4.t1 + 5) {
            g_h4.still = (std::fabs(p.y - g_h4.fy) < 0.01f) ? g_h4.still + 1 : 0;
        }
        g_h4.fy = p.y;
        if (g_h4.still >= 20 || f >= 900) {
            g_h4.mode = 2;
            g_h4.t2 = f;
            const float gy = h4Ground(p.x, p.y, p.z);
            const float wy = h4Water(p.x, p.y, p.z);
            char buf[240];
            std::snprintf(buf, sizeof(buf), "%s: Anschlag y=%.1f gy=%.1f wy=%.1f keine Bewegung %u Ticks f=%u", tag,
                static_cast<double>(p.y), static_cast<double>(gy), static_cast<double>(wy), g_h4.still, f);
            c.note(buf);
        }
    }
    if (g_h4.mode == 2 && f >= g_h4.t2 + 100) {
        g_h4.mode = 3;
        g_h4.t3 = f;
        h4Say(c, tag, f, "steigen (Stick runter)");
    }
    if (g_h4.mode == 3) {
        in.sy = -1.0f;
        if (f >= g_h4.t3 + 150) {
            g_h4.mode = 4;
            g_h4.t2 = f;
            h4Say(c, tag, f, "Rast");
        }
    }
    if (g_h4.mode == 4 && f >= g_h4.t2 + 60) {
        in.tap |= kBitB;
        g_h4.mode = 5;
        h4Say(c, tag, f, "B: absteigen");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B9Scenario = {
    "h4-b9",
    "H4 B9: Untergrenze, sinken bis zum Anschlag, 100 Ticks Rast, steigen, B",
    &h4b9Tick,
};

// ---- B12 W1: Schweben ueber See mit Grund (F6 W1, Stage F_SP115,0,0,-1). Bei Tick 3 wird der Vogel 400 ueber die
// Wasserflaeche des naechsten Sees mit Grund gesetzt (wie hylia-wasser in F6). Gang 3 -> 1 (5, 7), Schweben (9), B bei 130.
bool h4b12w1Tick(autotest::Context& c) {
    const char* tag = "h4-b12w1";
    g_h4.st.humanCall = true;  // Ruf aus Menschform (F6 W1): Stage F_SP115 startet als Mensch, autowolf verwandelt Link
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f == 3) {
        const H4Spot sp = h4FindWater(k->current.pos, true);
        if (!sp.found) {
            logInfo("%s: ABBRUCH kein See mit Grund im Umkreis 30000", tag);
            g_h4.st.stage = Stage::Done;
            return true;
        }
        const cXyz target(sp.x, sp.waterY + 400.0f, sp.z);
        k->current.pos = target;
        k->old.pos = target;
        char buf[240];
        std::snprintf(buf, sizeof(buf), "%s: Vogel ueber den See gesetzt (%.1f, %.1f, %.1f) Wasser y %.1f Grund y %.1f f=%u",
            tag, static_cast<double>(target.x), static_cast<double>(target.y), static_cast<double>(target.z),
            static_cast<double>(sp.waterY), static_cast<double>(sp.groundY), f);
        c.note(buf);
    }
    if (f == 5 || f == 7) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, "Gang runter");
    }
    if (f == 9) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, "Gang 1 runter: Schweben ueber dem See");
    }
    if (f == 130) {
        in.tap |= kBitB;
        h4Say(c, tag, f, "B: absteigen ueber Wasser");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B12w1Scenario = {
    "h4-b12w1",
    "H4 B12 W1: Schweben ueber See mit Grund (F6 W1, F_SP115), dann B, Landeanflug auf die Wasserflaeche",
    &h4b12w1Tick,
};

// ---- B12 W2: Schweben ueber Wasser ohne Grund (F6 W2, wp2-b701, Stage F_SP121,10,23,-1). Geradeaus fliegen ohne Eingabe.
// Ausloeser: erster Tick mit Wasser und ohne Grund unter dem Vogel (das ist der Flug wie wp2-b701). Dann Gang 3 -> 1
// (Ausloeser, +2), Schweben (+4), B bei +120. Ohne solchen Punkt bis kH4W2Max: Schweben trotzdem, Befund vermerken.
bool h4b12w2Tick(autotest::Context& c) {
    const char* tag = "h4-b12w2";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    const cXyz p = k->current.pos;
    H4In in;
    if (!g_h4.flag) {
        if (h4Water(p.x, p.y, p.z) != kH4None && h4Ground(p.x, p.y, p.z) == kH4None) {
            g_h4.flag = true;
            g_h4.t1 = f;
            char buf[240];
            std::snprintf(buf, sizeof(buf), "%s: Wasser ohne Grund unter dem Vogel (y %.1f, wy %.1f) f=%u", tag,
                static_cast<double>(p.y), static_cast<double>(h4Water(p.x, p.y, p.z)), f);
            c.note(buf);
        } else if (f >= kH4W2Max) {
            g_h4.flag = true;
            g_h4.t1 = f;
            h4Say(c, tag, f, "kein Wasser ohne Grund bis Tick 1500, Schweben trotzdem");
        }
    }
    if (g_h4.flag) {
        const uint32_t r = f - g_h4.t1;
        if (r == 0 || r == 2 || r == 4) {
            in.tap |= kBitDown;
        }
        if (r == 4) {
            h4Say(c, tag, f, "Schweben (Gang 1 runter)");
        }
        if (r == 120) {
            in.tap |= kBitB;
            h4Say(c, tag, f, "B: absteigen ueber Wasser");
        }
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B12w2Scenario = {
    "h4-b12w2",
    "H4 B12 W2: Schweben ueber Wasser ohne Grund (F6 W2, wp2-b701, F_SP121), dann B",
    &h4b12w2Tick,
};

// ---- B14: Schweben ueber dem Abgrund (Ort wie void-absteigen, Stage F_SP121,10,23,-1). 150 Ticks steigen, dann Vogel um
// 30000 zum Abgrund (findVoid), Gang 3 -> 1 (152, 154), Schweben (156), B bei 260. Danach POST bis 400 Ticks.
bool h4b14Tick(autotest::Context& c) {
    const char* tag = "h4-b14";
    const H4Step s = h4Step(c, tag);
    if (s != H4Step::Ride) {
        return s == H4Step::End;
    }
    daKago_c* k = liveBird();
    if (k == nullptr) {
        return false;
    }
    const uint32_t f = c.frame - g_h4.st.base;
    H4In in;
    if (f < 150) {
        in.sy = -1.0f;  // steigen
    }
    if (f == 150) {
        cXyz to;
        if (!h4FindVoid(k->current.pos, 30000.0f, &to)) {
            logInfo("%s: ABBRUCH kein Abgrund im Abstand 30000", tag);
            g_h4.st.stage = Stage::Done;
            return true;
        }
        k->current.pos = to;
        k->old.pos = to;
        char buf[240];
        std::snprintf(buf, sizeof(buf), "%s: Vogel an den Abgrund gesetzt (%.1f, %.1f, %.1f) f=%u", tag,
            static_cast<double>(to.x), static_cast<double>(to.y), static_cast<double>(to.z), f);
        c.note(buf);
    }
    if (f == 152 || f == 154) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, "Gang runter (Schweben ueber dem Abgrund)");
    }
    if (f == 156) {
        in.tap |= kBitDown;
        h4Say(c, tag, f, "Gang 1 runter: Schweben ueber dem Abgrund");
    }
    if (f == 260) {
        in.tap |= kBitB;
        h4Say(c, tag, f, "B: absteigen ueber dem Abgrund");
    }
    h4Apply(c, in);
    h4Trace(tag, f, k, in);
    return false;
}

const autotest::Scenario kH4B14Scenario = {
    "h4-b14",
    "H4 B14: Schweben ueber dem Abgrund (Ort wie void-absteigen, F_SP121), dann B, Vogel weg und Void-Out",
    &h4b14Tick,
};

}  // namespace test
#endif  // KFF_DEV_AUTOTEST

ModResult report(const char* what, ModResult r) {
    if (r == MOD_OK) {
        logInfo("tempo: install %s ok", what);
    } else {
        logWarn("tempo: install %s -> %d", what, static_cast<int>(r));
    }
    return r;
}

class TempoModule final : public Module {
public:
    const char* name() const override { return "tempo"; }

    ModResult install(Registry& reg) override {
        // Die Anzeige ist nur Zugabe: fehlt der Hook, wirken Gang, Dash und Sichtfeld trotzdem.
        report("dMeter2Draw_c::draw post (Anzeige)", reg.addPost(Hook::MeterDraw, &onMeterDrawPost, prio::kTempo));
#if defined(KFF_DEV_AUTOTEST)
        // Nur Testbuild (WP8.11): Konsum-Beobachter nach dem Kern und das Szenario eingabe-konsum. Fehlt eines, laeuft das
        // Modul trotzdem (nur Log).
        report("mDoCPd_c::read post (Testbeobachter Konsum)", reg.addPost(Hook::PadRead, &test::onPadReadObserve, prio::kTempo));
        report("autotest eingabe-konsum", autotest::registerScenario(&test::kKonsumScenario));
        if (test::h2Wants("schweben-demo")) report("autotest schweben-demo", autotest::registerScenario(&test::kSchwebenScenario));
        if (test::h2Wants("schweben-wasser")) report("autotest schweben-wasser", autotest::registerScenario(&test::kWasserScenario));
        if (test::h2Wants("schweben-dokument")) report("autotest schweben-dokument", autotest::registerScenario(&test::kDokumentScenario));
        if (test::h2Wants("h4-b1l")) report("autotest h4-b1l", autotest::registerScenario(&test::kH4B1Scenario));
        if (test::h2Wants("h4-b2")) report("autotest h4-b2", autotest::registerScenario(&test::kH4B2Scenario));
        if (test::h2Wants("h4-b4a")) report("autotest h4-b4a", autotest::registerScenario(&test::kH4B4aScenario));
        if (test::h2Wants("h4-b4b")) report("autotest h4-b4b", autotest::registerScenario(&test::kH4B4bScenario));
        if (test::h2Wants("h4-b5")) report("autotest h4-b5", autotest::registerScenario(&test::kH4B5Scenario));
        if (test::h2Wants("h4-b6")) report("autotest h4-b6", autotest::registerScenario(&test::kH4B6Scenario));
        if (test::h2Wants("h4-b8")) report("autotest h4-b8", autotest::registerScenario(&test::kH4B8Scenario));
        if (test::h2Wants("h4-b9")) report("autotest h4-b9", autotest::registerScenario(&test::kH4B9Scenario));
        if (test::h2Wants("h4-b12w1")) report("autotest h4-b12w1", autotest::registerScenario(&test::kH4B12w1Scenario));
        if (test::h2Wants("h4-b12w2")) report("autotest h4-b12w2", autotest::registerScenario(&test::kH4B12w2Scenario));
        if (test::h2Wants("h4-b14")) report("autotest h4-b14", autotest::registerScenario(&test::kH4B14Scenario));
#endif
        return MOD_OK;
    }

#if defined(KFF_DEV_AUTOTEST)
    const autotest::Scenario* autotestScenario() const override { return &test::kScenario; }
#endif

    void onRideStart(Session& s, daKago_c* bird) override {
        (void)bird;
        g_t = TempoState{};
        g_t.gear = configStartGear() - 1;
        g_t.cruise = kGearPct[g_t.gear];
        g_t.eff = g_t.cruise;
        g_t.halt = 0;
        // Ritt-Start: R oder L ist vom Ruf noch gehalten (Ruf halten teilt R mit Schneller). Wirkt erst nach dem Loslassen.
        g_t.waitRelease = haltOn(act(Act::Faster)) || haltOn(act(Act::Slower));
        g_t.hudOn = config::getBool(config::Key::HudShow);
        g_t.fovOn = config::getBool(config::Key::FovEffect);
        s.speedScale = g_t.eff;
        s.gear = g_t.gear + 1;
        s.fovExtraDeg = 0.0f;
        s.hover = false;  // ein Ritt beginnt nie im Schweben (resetSession setzt das Feld nicht)
        logInfo("tempo: ritt start gang=%d eff=%.3f hud=%d fov=%d", g_t.gear + 1, static_cast<double>(g_t.eff),
            g_t.hudOn ? 1 : 0, g_t.fovOn ? 1 : 0);
        startTips();
    }

    void onFrameBird(Session& s, daKago_c* bird) override {
        // Tipp 2 nur, solange der Ritt laeuft (diese Funktion laeuft nur im Ritt).
        if (g_t.tip2Due && s.rideFrames >= kTipSecondFrame) {
            g_t.tip2Due = false;
            secondTip();
        }
        if (bird != nullptr) {
            tick(bird);
        }
    }

#if defined(KFF_DEV_AUTOTEST)
    // Nur Testbuild (WP8.14): Link nach der Freigabe aus dem Landeanflug.
    void onFrameLink(Session& s, daAlink_c* link) override {
        (void)s;
        e9LinkFrame(link);
    }
#endif

    void onRideEnd(Session& s, daKago_c* bird, EndReason why) override {
        (void)bird;
        logInfo("tempo: ritt ende (%s) ticks=%u gangHoch=%u gangRunter=%u bremse=%u dash=%u abgelehnt=%u "
                "effMax=%.3f fovMax=%.2f measMax=%.2f",
            endReasonName(why), g_t.ticks, g_t.gearUps, g_t.gearDowns, g_t.brakes, g_t.dashes, g_t.dashRefused,
            static_cast<double>(g_t.effMax), static_cast<double>(g_t.fovMax), static_cast<double>(g_t.measMax));
#if defined(KFF_DEV_AUTOTEST)
        e9RideEnd();
#endif
        // Neutral nach dem Ritt: Gang 3, kein Sichtfeld-Zusatz, keine Anzeige, kein Schweben.
        s.speedScale = 0.5f;
        s.gear = 3;
        s.fovExtraDeg = 0.0f;
        s.hover = false;
        g_t.hudOn = false;
        g_t.dashOn = false;
        g_t.hover = false;
        g_t.tip2Due = false;
    }

    void onShutdown(Session&) override {
        g_down = true;
        delete g_hud;  // J2DPrint::~J2DPrint ist leer. Der Zeichenpuffer mStrBuff ist statisch und bleibt bestehen.
        g_hud = nullptr;
        const bool released = g_fontRef;
        if (g_fontRef) {
            g_fontRef = false;
            // Gegenstueck zu mDoExt_getMesgFont. Die Schrift wird erst beim letzten Verweis des Spiels frei.
            mDoExt_removeMesgFont();
        }
        logInfo("tempo: Entladen (HUD-Schrift-Referenz freigegeben=%d)", released ? 1 : 0);
#if defined(KFF_DEV_AUTOTEST)
        (void)test::konsumSummary("Lauf", false);
#endif
    }
};

TempoModule g_module;

}  // namespace

namespace modules {

Module& tempo() {
    return g_module;
}

}  // namespace modules
}  // namespace kff
