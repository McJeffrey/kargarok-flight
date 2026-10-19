// Testszenarien der Belegung (T3 und T4, nur Testbau): steuerung-s2 bis steuerung-s9 und steuerung-demo. Registriert
// von kff::steuerung::registerTestScenarios (core/kff/steuerung.hpp), nicht in autotest.cpp. Eingaben nur ueber den
// Kontext des Testbaus (Mod-interne Pad-Injektion). Testfenster und Dialog nur im Testbau ueber den UiService.
// Die Belegung der Szenarien kommt aus KFF_CONFIG_EXTRA (Testprofil, siehe BERICHT). Die Logzeilen mit dem Praefix
// "kff: autotest:" sind der Beleg je Szenario.
#include "core/kff/steuerung.hpp"

#include "core/actions.hpp"
#include "core/kff/actions.hpp"
#include "core/kff/autotest.hpp"
#include "core/kff/log.hpp"
#include "core/kff/panel.hpp"
#include "core/kff/services.hpp"
#include "core/kff/session.hpp"

#include <cstdint>
#include <cstdio>

namespace kff::steuerung {
namespace {

using autotest::Context;

constexpr uint32_t kBitA = static_cast<uint32_t>(PAD_BUTTON_A);
constexpr uint32_t kBitB = static_cast<uint32_t>(PAD_BUTTON_B);
constexpr uint32_t kBitX = static_cast<uint32_t>(PAD_BUTTON_X);
constexpr uint32_t kBitY = static_cast<uint32_t>(PAD_BUTTON_Y);
constexpr uint32_t kBitZ = static_cast<uint32_t>(PAD_TRIGGER_Z);

// Eine Notiz mit Praefix, z. B. "steuerung-s2: X getippt, Dash erwartet".
void say(Context& c, const char* who, const char* text) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "%s: %s", who, text);
    c.note(buf);
}

// Ablauf eines Szenarios: Rufversuche, bis der Ritt beginnt, dann die Eingaben im Ritt, danach Ende.
struct Spec {
    const char* name;
    uint32_t startAt;      // frueheste Ruf-Zeit (Ticks seit dem ersten Link-Frame)
    uint32_t callTicks;    // Dauer eines Rufversuchs
    uint32_t retryTicks;   // Pause nach einem Versuch ohne Ritt
    uint32_t maxAttempts;  // danach Ende ohne Ritt
    uint32_t rideTicks;    // Ticks im Ritt, danach Ende
    void (*call)(Context& c, uint32_t n, uint32_t attempt);  // Eingabe eines Rufversuchs, n = Ticks seit Versuchsbeginn
    void (*ride)(Context& c, uint32_t n);                     // Eingabe im Ritt, n = Ticks seit Rittbeginn (kann null sein)
};

struct Run {
    uint32_t attempts = 0;
    bool calling = false;
    uint32_t callStart = 0;
    uint32_t nextAt = 0;
    bool riding = false;
    uint32_t rideStart = 0;
    bool done = false;
};

bool engine(Context& c, Run& r, const Spec& sp) {
    if (r.done) {
        return true;
    }
    if (r.riding && c.sess.phase != Phase::Riding) {
        r.done = true;
        say(c, sp.name, "Ritt zu Ende");
        return true;
    }
    if (c.sess.phase == Phase::Riding) {
        if (!r.riding) {
            r.riding = true;
            r.rideStart = c.frame;
            char buf[80];
            std::snprintf(buf, sizeof buf, "Ritt begonnen nach Versuch %u", r.attempts);
            say(c, sp.name, buf);
        }
        const uint32_t n = c.frame - r.rideStart;
        if (n >= sp.rideTicks) {
            r.done = true;
            say(c, sp.name, "Ende nach Ritt-Zeit");
            return true;
        }
        if (sp.ride != nullptr) {
            sp.ride(c, n);
        }
        return false;
    }
    if (r.calling) {
        const uint32_t n = c.frame - r.callStart;
        if (n < sp.callTicks) {
            sp.call(c, n, r.attempts);
            return false;
        }
        r.calling = false;
        r.nextAt = c.frame + sp.retryTicks;
        return false;
    }
    if (c.frame < sp.startAt || c.frame < r.nextAt || !c.canCall()) {
        return false;
    }
    if (r.attempts >= sp.maxAttempts) {
        r.done = true;
        say(c, sp.name, "kein Ritt nach allen Versuchen");
        return true;
    }
    ++r.attempts;
    r.calling = true;
    r.callStart = c.frame;
    char buf[80];
    std::snprintf(buf, sizeof buf, "Ruf-Versuch %u", r.attempts);
    say(c, sp.name, buf);
    sp.call(c, 0, r.attempts);
    return false;
}

// Ruf mit R und B (Vorgabe): R gehalten, B-Tipp in der Mitte.
void callRB(Context& c, uint32_t n, uint32_t) {
    c.holdR();
    if (n == 15) {
        c.tapB();
    }
}

// Ruf mit Z und X (Test S3): Z gehalten, X-Tipp in der Mitte.
void callZX(Context& c, uint32_t n) {
    c.hold(kBitZ);
    if (n == 15) {
        c.tap(kBitX);
    }
}

// S3: erster Versuch mit R und B (darf nicht rufen), danach Z und X (ruft).
void callS3(Context& c, uint32_t n, uint32_t attempt) {
    if (attempt <= 1) {
        callRB(c, n, attempt);
    } else {
        callZX(c, n);
    }
}

// S2: A darf nicht dashen, X dasht (Vorgabe Dash = A, Test: bind_dash = X).
void rideS2(Context& c, uint32_t n) {
    if (n == 60) {
        say(c, "steuerung-s2", "A getippt, Dash mit A nicht erwartet");
        c.tap(kBitA);
    }
    if (n == 120) {
        say(c, "steuerung-s2", "X getippt, Dash mit X erwartet");
        c.tap(kBitX);
    }
}

// S4 und S6: B tippen, im Ritt steigt B ab (Vorgabe, bei S6 nach dem Rueckfall von 0 auf B). Die Logzeile nennt das Szenario.
void rideBAs(Context& c, uint32_t n, const char* who) {
    if (n == 60) {
        say(c, who, "B getippt, Absteigen erwartet");
        c.tap(kBitB);
    }
}

void rideB(Context& c, uint32_t n) {
    rideBAs(c, n, "steuerung-s4");
}

void rideS6(Context& c, uint32_t n) {
    rideBAs(c, n, "steuerung-s6");
}

// S5: Fassrolle behaelt Y, Dash faellt auf A.
void rideS5(Context& c, uint32_t n) {
    if (n == 60) {
        say(c, "steuerung-s5", "Y getippt, Fassrolle erwartet");
        c.tap(kBitY);
    }
    if (n == 120) {
        say(c, "steuerung-s5", "A getippt, Dash erwartet");
        c.tap(kBitA);
    }
}

// S7: ungueltiger Wert 42 faellt auf die Vorgabe A zurueck.
void rideS7(Context& c, uint32_t n) {
    if (n == 60) {
        say(c, "steuerung-s7", "A getippt, Dash erwartet (42 wirkt als A)");
        c.tap(kBitA);
    }
}

// S8: Aenderung waehrend des Rittes ueber denselben Weg wie das Auswahlfeld. Wirkt ab dem naechsten Tick.
void rideS8(Context& c, uint32_t n) {
    if (n == 40) {
        say(c, "steuerung-s8", "Dash auf X geaendert ueber den Auswahl-Weg");
        selectForTest(static_cast<uint8_t>(actions::ActId::Dash), 3);
    }
    if (n == 60) {
        say(c, "steuerung-s8", "A getippt, kein Dash mehr erwartet");
        c.tap(kBitA);
    }
    if (n == 100) {
        say(c, "steuerung-s8", "X getippt, Dash mit X erwartet");
        c.tap(kBitX);
    }
}

// S9: Ein offener Dialog blockiert die Ritt-Tasten. A (Vorgabe Dash) bei offenem Dialog: kein Dash. Nach dem Schliessen: Dash.
UiDialogHandle g_s9Dialog = 0;
UiDialogAction g_s9Action = UI_DIALOG_ACTION_INIT;

void rideS9(Context& c, uint32_t n) {
    if (n == 40 && g_s9Dialog == 0 && svc_ui != nullptr) {
        g_s9Action = UI_DIALOG_ACTION_INIT;
        g_s9Action.label = "Close";
        UiDialogDesc d = UI_DIALOG_DESC_INIT;
        d.title = "steuerung-s9";
        d.body_rml = "Test dialog: a menu is open.";
        d.actions = &g_s9Action;
        d.action_count = 1;
        const ModResult r = svc_ui->dialog_push(mod_ctx, &d, &g_s9Dialog);
        say(c, "steuerung-s9", r == MOD_OK ? "Dialog offen" : "Dialog nicht geoeffnet");
    }
    if (n == 60) {
        say(c, "steuerung-s9", "A bei offenem Dialog getippt, kein Dash erwartet");
        c.tap(kBitA);
    }
    if (n == 120 && g_s9Dialog != 0 && svc_ui != nullptr) {
        (void)svc_ui->dialog_close(mod_ctx, g_s9Dialog);
        g_s9Dialog = 0;
        say(c, "steuerung-s9", "Dialog geschlossen");
    }
    if (n == 150) {
        say(c, "steuerung-s9", "A getippt, Dash erwartet");
        c.tap(kBitA);
    }
}

// Vorfuehrung (steuerung-demo): im Ritt X tippen, X dasht.
void rideDemo(Context& c, uint32_t n) {
    if (n == 60) {
        say(c, "steuerung-demo", "A getippt, kein Dash erwartet");
        c.tap(kBitA);
    }
    if (n == 150) {
        say(c, "steuerung-demo", "X getippt, Dash mit X erwartet");
        c.tap(kBitX);
    }
}

const Spec kS2 = {"steuerung-s2", 300, 40, 150, 6, 200, callRB, rideS2};
const Spec kS3 = {"steuerung-s3", 300, 40, 60, 4, 150, callS3, nullptr};
const Spec kS4 = {"steuerung-s4", 300, 40, 150, 6, 200, callRB, rideB};
const Spec kS5 = {"steuerung-s5", 300, 40, 150, 6, 200, callRB, rideS5};
const Spec kS6 = {"steuerung-s6", 300, 40, 150, 6, 200, callRB, rideS6};
const Spec kS7 = {"steuerung-s7", 300, 40, 150, 6, 200, callRB, rideS7};
const Spec kS8 = {"steuerung-s8", 300, 40, 150, 6, 200, callRB, rideS8};
const Spec kS9 = {"steuerung-s9", 300, 40, 150, 6, 200, callRB, rideS9};
// steuerung-demo (K8): drei Phasen zu je 450 Ticks, also 15 s bei 30 Ticks je Sekunde. Die Zahlen sind Ticks seit dem
// ersten Link-Frame (f). Phase 1: Testfenster mit dem Panel-Inhalt allein ab f=240. Phase 2: dazu das Fenster Flight
// buttons ab f=690. Phase 3: nach Dash auf X ab f=1140, beide Fenster ab f=1590 zu. Der Ruf beginnt frueheste bei f=1650.
// K10: ab Phase 2 setzt der Lauf den Fokus auf die Zeile Dash (Start f=690 und f=1140).
constexpr uint32_t kDemoPhase = 450;
constexpr uint32_t kDemoOpen = 240;
constexpr uint32_t kDemoButtons = kDemoOpen + kDemoPhase;
constexpr uint32_t kDemoSet = kDemoButtons + kDemoPhase;
constexpr uint32_t kDemoClose = kDemoSet + kDemoPhase;
constexpr uint32_t kDemoRide = kDemoClose + 60;
const Spec kDemo = {"steuerung-demo", kDemoRide, 40, 150, 6, 300, callRB, rideDemo};

bool tickS2(Context& c) {
    static Run r;
    return engine(c, r, kS2);
}

bool tickS3(Context& c) {
    static Run r;
    return engine(c, r, kS3);
}

bool tickS4(Context& c) {
    static Run r;
    return engine(c, r, kS4);
}

bool tickS5(Context& c) {
    static Run r;
    return engine(c, r, kS5);
}

bool tickS6(Context& c) {
    static Run r;
    return engine(c, r, kS6);
}

bool tickS7(Context& c) {
    static Run r;
    return engine(c, r, kS7);
}

bool tickS8(Context& c) {
    static Run r;
    return engine(c, r, kS8);
}

bool tickS9(Context& c) {
    static Run r;
    return engine(c, r, kS9);
}

// steuerung-demo, Phasen 1 bis 3 vor dem Ruf (K8): Testfenster mit dem Panel-Inhalt, Fenster Flight buttons, Auswahl Dash auf X
// Jede Phase beginnt mit einer Logzeile (Phase und Tick f). Danach ein Ritt, in dem X dasht.
UiWindowHandle g_demoWindow = 0;
UiTabDesc g_demoTab = UI_TAB_DESC_INIT;
bool g_dOpen = false;
bool g_dButtons = false;
bool g_dSet = false;
bool g_dClosed = false;

ModResult demoBuild(ModContext*, UiWindowHandle, UiElementHandle left, UiElementHandle, void*, ModError*) {
    return panel::buildContents(left);
}

ModResult demoUpdate(ModContext*, void*, ModError*) {
    panel::updateContents();
    return MOD_OK;
}

// Logzeile am Beginn einer Phase: Phase, Tick f und Text, z. B. "steuerung-demo: phase 2 start f=690: ...".
void phaseNote(Context& c, int phase, const char* text) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "phase %d start f=%u: %s", phase, c.frame, text);
    say(c, "steuerung-demo", buf);
}

// K10: Fokus auf die Zeile Dash in Phase 2 und Phase 3 (elem_focus). Solange der Dienst MOD_UNAVAILABLE meldet, wird je Tick
// erneut versucht, hoechstens kFocusTicks lang. Das Ergebnis steht in einer Logzeile.
constexpr uint32_t kFocusTicks = 90;
int g_focusPhase = 0;       // Phase, deren Fokus noch aussteht (0 = keiner)
uint32_t g_focusStart = 0;  // Tick des ersten Versuchs

void focusDashStart(int phase, uint32_t frame) {
    g_focusPhase = phase;
    g_focusStart = frame;
}

void focusDashStep(Context& c) {
    if (g_focusPhase == 0) {
        return;
    }
    const ModResult r = focusForTest(static_cast<uint8_t>(actions::ActId::Dash));
    if (r == MOD_UNAVAILABLE && c.frame - g_focusStart < kFocusTicks) {
        return;
    }
    const char* ergebnis = r == MOD_OK ? "MOD_OK" : (r == MOD_UNAVAILABLE ? "MOD_UNAVAILABLE" : "anderer Fehler");
    char buf[200];
    std::snprintf(buf, sizeof buf, "phase %d focus Dash f=%u nach %u Ticks: elem_focus %s", g_focusPhase, c.frame,
        c.frame - g_focusStart, ergebnis);
    say(c, "steuerung-demo", buf);
    g_focusPhase = 0;
}

// K11: false im Vorgabe-Lauf (steuerung-demo-vorgabe): gleicher Zeitplan, aber ohne Dash auf X (Belegung Vorgabe A/B).
bool g_demoMitSet = true;

void demoSteps(Context& c) {
    if (!g_dOpen && c.frame >= kDemoOpen) {
        g_dOpen = true;
        if (svc_ui != nullptr) {
            g_demoTab = UI_TAB_DESC_INIT;
            g_demoTab.title = "Kargarok Flight (Demo)";
            g_demoTab.build = demoBuild;
            g_demoTab.update = demoUpdate;
            UiWindowDesc desc = UI_WINDOW_DESC_INIT;
            desc.tabs = &g_demoTab;
            desc.tab_count = 1;
            const ModResult r = svc_ui->window_push(mod_ctx, &desc, &g_demoWindow);
            phaseNote(c, 1, r == MOD_OK ? "Testfenster mit dem Panel-Inhalt allein offen"
                                                 : "Testfenster nicht geoeffnet");
        }
    }
    if (!g_dButtons && c.frame >= kDemoButtons) {
        g_dButtons = true;
        (void)openWindow();
        phaseNote(c, 2, "Fenster Flight buttons dazu, Testfenster bleibt offen");
        focusDashStart(2, c.frame);
    }
    if (!g_dSet && c.frame >= kDemoSet) {
        g_dSet = true;
        if (g_demoMitSet) {
            selectForTest(static_cast<uint8_t>(actions::ActId::Dash), 3);
            phaseNote(c, 3, "Dash auf X gesetzt ueber den Auswahl-Weg, Statuszeile sichtbar");
        } else {
            phaseNote(c, 3, "Vorgabe ohne Set: Dash bleibt auf A, Absteigen auf B");
        }
        focusDashStart(3, c.frame);
    }
    focusDashStep(c);
    if (!g_dClosed && c.frame >= kDemoClose) {
        g_dClosed = true;
        closeForTest();
        if (svc_ui != nullptr && g_demoWindow != 0) {
            (void)svc_ui->window_close(mod_ctx, g_demoWindow);
        }
        char buf[120];
        std::snprintf(buf, sizeof buf, "phase 3 ende f=%u: beide Fenster geschlossen", c.frame);
        say(c, "steuerung-demo", buf);
    }
}

bool tickDemo(Context& c) {
    static Run r;
    g_demoMitSet = true;
    if (c.frame < kDemo.startAt) {
        demoSteps(c);
        return false;
    }
    return engine(c, r, kDemo);
}

// K11: Vorgabe-Lauf fuer das Bild mit Standardbelegung (Dash A, Absteigen B). Gleiche Phasen 1 bis 3, nur ohne Set.
const Spec kDemoVorgabe = {"steuerung-demo-vorgabe", kDemoRide, 40, 150, 6, 300, callRB, rideDemo};

bool tickDemoVorgabe(Context& c) {
    static Run r;
    g_demoMitSet = false;
    if (c.frame < kDemoVorgabe.startAt) {
        demoSteps(c);
        return false;
    }
    return engine(c, r, kDemoVorgabe);
}

const autotest::Scenario kScS2 = {"steuerung-s2",
    "Belegung Dash auf X (bind_dash=3): X dasht im Ritt, A nicht", tickS2};
const autotest::Scenario kScS3 = {"steuerung-s3",
    "Belegung Ruf halten Z, Ruf tippen X (bind_ruf_halten=5, bind_ruf_tipp=3): Z+X ruft, R+B ruft nicht", tickS3};
const autotest::Scenario kScS4 = {"steuerung-s4",
    "Vorgabe: im Ritt steigt B allein ab, R+B ruft", tickS4};
const autotest::Scenario kScS5 = {"steuerung-s5",
    "Konflikt in der Konfiguration (bind_dash=Y, bind_fassrolle=Y): Dash faellt auf A, Fassrolle bleibt Y", tickS5};
const autotest::Scenario kScS6 = {"steuerung-s6",
    "Absteigen ohne Taste (bind_absteigen=0): Vorgabe B, B steigt ab", tickS6};
const autotest::Scenario kScS7 = {"steuerung-s7",
    "Ungueltiger Wert (bind_dash=42): Vorgabe A, kein Absturz", tickS7};
const autotest::Scenario kScS8 = {"steuerung-s8",
    "Aenderung im Ritt ueber den Auswahl-Weg (Dash auf X): wirkt ab dem naechsten Tick, der Ritt laeuft weiter", tickS8};
const autotest::Scenario kScS9 = {"steuerung-s9",
    "Dialog offen: A (Vorgabe Dash) loest im Ritt kein Dash aus, nach dem Schliessen wieder", tickS9};
const autotest::Scenario kScDemo = {"steuerung-demo",
    "Vorfuehrung fuer S4: Testfenster mit Panel, Fenster Flight buttons, Dash auf X, Ritt mit X-Dash", tickDemo};
const autotest::Scenario kScDemoVorgabe = {"steuerung-demo-vorgabe",
    "Vorfuehrung K11: wie steuerung-demo, aber Vorgabe-Belegung (Dash A, Absteigen B), Ritt mit Vorgabe", tickDemoVorgabe};

}  // namespace

void registerTestScenarios() {
    (void)actions::runPureTests();
    (void)autotest::registerScenario(&kScS2);
    (void)autotest::registerScenario(&kScS3);
    (void)autotest::registerScenario(&kScS4);
    (void)autotest::registerScenario(&kScS5);
    (void)autotest::registerScenario(&kScS6);
    (void)autotest::registerScenario(&kScS7);
    (void)autotest::registerScenario(&kScS8);
    (void)autotest::registerScenario(&kScS9);
    (void)autotest::registerScenario(&kScDemo);
    (void)autotest::registerScenario(&kScDemoVorgabe);
}

}  // namespace kff::steuerung
