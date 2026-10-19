// Autotest: Szenario-Register, Pad-Injektion (vollstaendiger Ersatz von Port 1) und die eingebauten Szenarien.
// Nur im Testbuild (KFF_DEV_AUTOTEST) kompiliert.
#include "d/dolzel_rel.h"

#include "core/kff/autotest.hpp"

#include "core/kff/log.hpp"
#include "core/kff/session.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_midna.h"  // WP2: Midna-Zeiger (mpKago) in den Logs
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"  // F5 R3-A: dKy_pol_argument_get (Wasser-Attribut in der Diagnosezeile)
#include "d/d_meter2_info.h"  // VORLAUF e1e5-menue
#include "d/d_menu_collect.h"  // VORLAUF e1e5-menue
#include "d/d_menu_save.h"  // VORLAUF e1e5-menue
#include "d/d_menu_window.h"  // VORLAUF e1e5-menue
#include "SSystem/SComponent/c_xyz.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_controller_pad.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <chrono>  // VORLAUF e1e5-menue
#include <cmath>
#include <cstdio>  // VORLAUF e1e5-menue
#include <cstdlib>
#include <cstring>
#include <ctime>  // VORLAUF e1e5-menue

namespace kff::autotest {
namespace {

// Ausreichend fuer die eingebauten Szenarien (4) und alle Modul-Szenarien (flug, freiheit, griff, kamera, autowolf).
// Bei 16 schlug die Installation von autowolf fehl (MOD_ERROR), das Modul war im Testbau abgeschaltet.
// 2.1.0 mit den Vorlauf-Szenarien (e6, e9, e1e5) braucht mehr als 32 Plaetze: bei 32 schlug die Registrierung der
// Modul-Szenarien fehl (freiheit wand5, griff, tempo, kamera, mensch, autowolf), die Module waren abgeschaltet und der Ruf
// wurde mit "Modul flug oder freiheit abgeschaltet" abgewiesen. Nur Testcode, das Release-Paket ist nicht betroffen.
// WP3: 96 Plaetze, die Eingabe-Szenarien (input_test.cpp) kommen hinzu und Welle 2 bringt weitere.
constexpr size_t kMaxScenarios = 96;
const Scenario* g_list[kMaxScenarios] = {};
size_t g_count = 0;
const Scenario* g_active = nullptr;
bool g_finished = false;     // das aktive Szenario hat "fertig" gemeldet
uint32_t g_frame = 0;        // Ticks seit dem ersten Link-Frame
uint32_t g_foreign = 0;      // Fremdeingaben (Roh-Pad ungleich 0 vor der Injektion)
uint32_t g_foreignLog = 0;   // Zaehler fuer die gedrosselte Fremdeingabe-Zeile

// Eingaben eines Ticks (vom Szenario gesetzt, vom Kern angewendet).
struct TickInput {
    uint32_t tapBits = 0;    // Flanke und gehalten
    uint32_t holdBits = 0;   // gehalten ohne Flanke
    bool holdR = false;      // R voll gedrueckt (Flanke bei Beginn, wird aus dem Verlauf erkannt)
    float stickX = 0.0f;
    float stickY = 0.0f;
    float cX = 0.0f;
    float cY = 0.0f;
    float analogR = -1.0f;  // R analog ohne Digitalbit, < 0 = nicht gesetzt (Testfall Ruf 0,65 / 0,50)
    bool gameLocks = false;  // WP3: Lock-Flaggen per LRlockCheck wie im Spiel statt aus den Digitalbits
};
TickInput g_in;
bool g_prevR = false;
interface_of_controller_pad g_lastPad{};  // WP3: Endzustand des Pads im letzten Tick (observe)

constexpr uint32_t kBitA = static_cast<uint32_t>(PAD_BUTTON_A);
constexpr uint32_t kBitB = static_cast<uint32_t>(PAD_BUTTON_B);
constexpr uint32_t kBitR = static_cast<uint32_t>(PAD_TRIGGER_R);
constexpr uint32_t kBitL = static_cast<uint32_t>(PAD_TRIGGER_L);

// ---------------------------------------------------------------------------------------------
// Szenario "zyklus": Ruf, Ritt, Kurve, Dash, Absteigen, erneut Rufen, Absteigen (wie v1).
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kZyCallAt = 300;      // erster Ruf, sobald Link bereit ist
constexpr uint32_t kZyHold = 30;         // R gehalten, Ticks
constexpr uint32_t kZyPressOffset = 15;  // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kZyDismount1 = 1200;  // Absteigen, relativ zum ersten Ruf
constexpr uint32_t kZyRecallGap = 200;   // zweiter Ruf nach dem ersten Absteigen
constexpr uint32_t kZyDismount2 = 400;   // Absteigen, relativ zum zweiten Ruf
constexpr uint32_t kZyEndGap = 100;      // Ende nach dem zweiten Absteigen
constexpr uint32_t kZyDashRide = 900;    // A (Dash) bei Ritt-Frame 900

struct ZyState {
    bool callArmed = false;
    uint32_t callStart = 0;
    bool recallArmed = false;
    uint32_t recallStart = 0;
    bool dashDone = false;
};
ZyState g_zy;

// Stick-Muster der Ritt-Zeitachse: steigen 150, links 400, rechts 200, sinken 100.
// Nur Testbuild: Gegner in der Landezone entfernen, bevor der Landeanflug beginnt. Messung steuerung-r1: Gegner des
// Typs E_YR treffen Link kurz nach dem Absteigen. Das ist Spielverhalten, kein Sturzschaden. Der Test prueft den
// Landeschaden des Kerns deshalb mit leerer Landezone. Die Zahl der entfernten Gegner steht im Log.
constexpr float kLandGegnerRadius = 2500.0f;  // horizontaler Umkreis in Spieleinheiten

bool landGegnerName(s16 name) {
    return name == fpcNm_E_YR_e || name == fpcNm_E_YK_e || name == fpcNm_E_RDY_e || name == fpcNm_E_YD_e ||
           name == fpcNm_E_ARROW_e;
}

struct LandGegnerSuche {
    cXyz center;
    fpc_ProcID ids[64];
    uint32_t n;
};

void* landGegnerJudge(void* actorPtr, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actorPtr);
    LandGegnerSuche* st = static_cast<LandGegnerSuche*>(data);
    if (ac == nullptr || !landGegnerName(fopAcM_GetName(ac))) {
        return nullptr;
    }
    const float dx = ac->current.pos.x - st->center.x;
    const float dz = ac->current.pos.z - st->center.z;
    if (std::sqrt(dx * dx + dz * dz) <= kLandGegnerRadius && st->n < 64) {
        st->ids[st->n++] = fopAcM_GetID(ac);
    }
    return nullptr;  // alle Akteure durchlaufen, nichts vorzeitig melden
}

// Entfernt die Gegner erst nach dem Durchlauf der Liste (kein Entfernen waehrend der Iteration).
uint32_t landGegnerRaeumen(const cXyz& center) {
    LandGegnerSuche st{center, {}, 0};
    (void)fopAcM_Search(landGegnerJudge, &st);
    for (uint32_t i = 0; i < st.n; ++i) {
        fopAcM_delete(st.ids[i]);
    }
    return st.n;
}

void zyStick(uint32_t f, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    if (f < 150u) {
        y = -0.6f;  // steigen (negativer Stick-Y: Nase hoch)
    } else if (f < 550u) {
        x = -1.0f;  // links
    } else if (f < 750u) {
        x = 1.0f;  // rechts
    } else if (f < 850u) {
        y = 0.6f;  // sinken
    }
}

bool zyklusTick(Context& c) {
    const uint32_t t = c.frame;

    if (!g_zy.callArmed && t >= kZyCallAt && c.canCall()) {
        g_zy.callArmed = true;
        g_zy.callStart = t;
        c.note("call chord armed (R halten, B tippen)");
    }
    if (g_zy.callArmed) {
        if (t >= g_zy.callStart && t < g_zy.callStart + kZyHold) {
            c.holdR();
        }
        if (t == g_zy.callStart + kZyPressOffset) {
            c.tapB();
            c.note("B trigger in the R hold (call chord)");
        }
        if (t == g_zy.callStart + kZyDismount1) {
            c.tapB();
            c.note("B trigger (dismount)");
        }
        if (!g_zy.recallArmed && t >= g_zy.callStart + kZyDismount1 + kZyRecallGap && c.canCall()) {
            g_zy.recallArmed = true;
            g_zy.recallStart = t;
            c.note("second call chord armed");
        }
        if (g_zy.recallArmed) {
            if (t >= g_zy.recallStart && t < g_zy.recallStart + kZyHold) {
                c.holdR();
            }
            if (t == g_zy.recallStart + kZyPressOffset) {
                c.tapB();
                c.note("B trigger in the R hold (second call chord)");
            }
            if (t == g_zy.recallStart + kZyDismount2) {
                c.tapB();
                c.note("B trigger (second dismount)");
            }
            if (t == g_zy.recallStart + kZyDismount2 + kZyEndGap) {
                c.note("timeline complete");
                return true;
            }
        }
    }

    if (c.sess.phase == Phase::Riding) {
        if (!g_zy.dashDone && c.sess.rideFrames == kZyDashRide) {
            g_zy.dashDone = true;
            c.tapA();
            c.note("A trigger (dash)");
        }
        float x = 0.0f;
        float y = 0.0f;
        zyStick(c.sess.rideFrames, x, y);
        c.stick(x, y);
    }
    return false;
}

const Scenario kZyklus = {
    "zyklus",
    "Ruf, Ritt, Kurve, Dash, Absteigen, erneut Rufen, Absteigen (rund 2200 Ticks)",
    zyklusTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "ruf-abflug" (N1, nur Testbuild): Ruf, Ritt, Absteigen, dann ein neuer Ruf, waehrend der alte Vogel noch
// wegfliegt. Der neue Ruf kommt in dem Fenster, in dem der Abflug laeuft und 20 bis 70 Pad-Ticks seit dem Absteigen
// vergangen sind (das Entfernen des alten Vogels ist bei 100). Erwartet: Log "call during departure: bird ... removed
// now", ein neuer Ritt startet, danach Absteigen und Ende. Gilt fuer den Wolf (F_SP121) und den Menschen (F_SP103 mit
// mensch_ritt = true). Der Ruf verlangt weder Wolf noch einen leeren Vogelplatz: die Gates des Kerns entscheiden.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kAbCallAt = 300;         // erster Ruf, sobald Link bereit ist
constexpr uint32_t kAbHold = 30;            // R gehalten, Ticks
constexpr uint32_t kAbPressOffset = 15;     // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kAbDismountRide = 200;   // erstes Absteigen bei diesem Ritt-Frame
constexpr uint32_t kAbDepartMin = 20;       // neuer Ruf im Abflug: frueheste Abflug-Stufe (Pad-Ticks seit dem Absteigen)
constexpr uint32_t kAbDepartMax = 70;       // spaeteste Abflug-Stufe (das Entfernen des alten Vogels ist bei 100)
constexpr uint32_t kAbDismount2Ride = 150;  // zweites Absteigen bei diesem Ritt-Frame
constexpr uint32_t kAbEndGap = 120;         // Ende nach dem zweiten Absteigen
constexpr uint32_t kAbTimeout = 900;        // Abbruch, wenn ein Schritt nicht weiterkommt

enum class AbStep : uint8_t { Ruf1, Ruf1Halten, Ritt1, Abflug, Ruf2Halten, Ritt2, Ende };

struct AbState {
    AbStep step = AbStep::Ruf1;
    uint32_t start = 0;  // Frame, seit dem der laufende Schritt besteht
};
AbState g_ab;

// Ruf-Folge (R halten, B tippen) ab dem Frame start. true, sobald die Folge vollstaendig ist.
bool abChord(Context& c, uint32_t start) {
    const uint32_t t = c.frame;
    if (t >= start && t < start + kAbHold) {
        c.holdR();
    }
    if (t == start + kAbPressOffset) {
        c.tapB();
        c.note("B trigger in the R hold (call chord)");
    }
    return t >= start + kAbHold;
}

// Link lebt, hat keine Last und kein Ereignis. Ein eigener Vogel darf noch da sein (Abflug): dann entscheidet der Kern.
bool abBereit(const Context& c) {
    daAlink_c* link = liveLink();
    return link != nullptr && !link->checkCargoCarry() && !link->checkEventRun() && !dComIfGp_event_runCheck() &&
           c.sess.birdId == kNoProc;
}

bool abflugTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_ab.step) {
        case AbStep::Ruf1:
            if (t >= kAbCallAt && abBereit(c)) {
                g_ab.step = AbStep::Ruf1Halten;
                g_ab.start = t;
                c.note("call chord armed (R halten, B tippen)");
                (void)abChord(c, g_ab.start);
            }
            break;
        case AbStep::Ruf1Halten:
            if (abChord(c, g_ab.start)) {
                g_ab.step = AbStep::Ritt1;
                g_ab.start = t;
            }
            break;
        case AbStep::Ritt1:
            if (c.sess.phase == Phase::Riding && c.sess.rideFrames >= kAbDismountRide) {
                c.tapB();
                c.note("B trigger (dismount 1)");
                g_ab.step = AbStep::Abflug;
                g_ab.start = t;
            } else if (t - g_ab.start > kAbTimeout) {
                c.note("ruf-abflug: kein Ritt nach dem ersten Ruf");
                return true;
            }
            break;
        case AbStep::Abflug:
            if (kff::birdDeparting(c.sess) && c.sess.departFrames >= kAbDepartMin &&
                c.sess.departFrames <= kAbDepartMax) {
                g_ab.step = AbStep::Ruf2Halten;
                g_ab.start = t;
                c.note("second call chord in departure (old bird still there)");
                (void)abChord(c, g_ab.start);
            } else if (c.sess.departFrames > kAbDepartMax || c.sess.birdId == kNoProc) {
                c.note("ruf-abflug: Abflug-Fenster verpasst");
                return true;
            }
            break;
        case AbStep::Ruf2Halten:
            if (abChord(c, g_ab.start)) {
                g_ab.step = AbStep::Ritt2;
                g_ab.start = t;
            }
            break;
        case AbStep::Ritt2:
            if (c.sess.phase == Phase::Riding && c.sess.rideFrames >= kAbDismount2Ride) {
                c.tapB();
                c.note("B trigger (dismount 2)");
                g_ab.step = AbStep::Ende;
                g_ab.start = t;
            } else if (t - g_ab.start > kAbTimeout) {
                c.note("ruf-abflug: kein zweiter Ritt nach dem Ruf im Abflug");
                return true;
            }
            break;
        case AbStep::Ende:
            if (t - g_ab.start >= kAbEndGap) {
                c.note("timeline complete");
                return true;
            }
            break;
    }
    return false;
}

const Scenario kRufAbflug = {
    "ruf-abflug",
    "N1: Ruf, Ritt, Absteigen, erneuter Ruf im Abflug (alter Vogel wird sofort entfernt), Ritt, Absteigen",
    abflugTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "konsum": Ruf, dann im Ritt jede Taste einzeln tippen (Y, X, A, Z, L, R, Steuerkreuz), Start
// oeffnet das Menue (Absteigen muss dort ausbleiben), Start schliesst es, B steigt ab.
// Messung: Konsum-Zaehler im Log (SUMMARY/CORE2) und die Menue-Zeile.
// ---------------------------------------------------------------------------------------------

struct KonsumState {
    bool armed = false;
    uint32_t start = 0;
    uint32_t rideAt = 0;     // Ritt-Frame, an dem die Testtasten beginnen
    bool rideSeen = false;
    bool pauseOpened = false;
    uint32_t pauseTick = 0;  // Pad-Tick des START-Drucks (Menue auf)
    bool dismounted = false;
    uint32_t dismountTick = 0;
};
KonsumState g_kon;

bool konsumTick(Context& c) {
    const uint32_t t = c.frame;
    if (!g_kon.armed && t >= kZyCallAt && c.canCall()) {
        g_kon.armed = true;
        g_kon.start = t;
        c.note("konsum: call chord armed");
    }
    if (g_kon.armed && !g_kon.rideSeen) {
        if (t >= g_kon.start && t < g_kon.start + kZyHold) {
            c.holdR();
        }
        if (t == g_kon.start + kZyPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_kon.rideSeen = true;
            g_kon.rideAt = c.sess.rideFrames;
            c.note("konsum: riding");
        }
    }
    if (g_kon.rideSeen && c.sess.phase == Phase::Riding) {
        const uint32_t f = c.sess.rideFrames - g_kon.rideAt;
        switch (f) {
        case 60:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));
            c.note("konsum: Y tap (Fassrolle-Taste)");
            break;
        case 70:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_X));
            c.note("konsum: X tap");
            break;
        case 80:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_A));
            c.note("konsum: A tap (Dash)");
            break;
        case 90:
            c.tap(static_cast<uint32_t>(PAD_TRIGGER_Z));
            c.note("konsum: Z tap");
            break;
        case 100:
            c.tap(static_cast<uint32_t>(PAD_TRIGGER_L));
            c.note("konsum: L tap");
            break;
        case 110:
            c.holdR();
            c.note("konsum: R hold");
            break;
        case 130:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
            c.note("konsum: D-Pad up tap");
            break;
        case 140:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));
            c.note("konsum: D-Pad down tap");
            break;
        case 150:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_LEFT));
            c.note("konsum: D-Pad left tap");
            break;
        case 160:
            c.tap(static_cast<uint32_t>(PAD_BUTTON_RIGHT));
            c.note("konsum: D-Pad right tap");
            break;
        case 200:
            if (!g_kon.pauseOpened) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
                g_kon.pauseOpened = true;
                g_kon.pauseTick = t;
                c.note("konsum: START (Menue auf)");
            }
            break;
        default:
            break;
        }
    }
    // Menue-Phase: im Menue stehen Ritt-Frames und Vogel still (Testlauf konsum1: Ritt-Zaehler blieb bei 200),
    // daher zaehlt hier der Pad-Tick t seit START. Die Zeile "Menue-Stand" zeigt Phase, Ritt-Frames und Pause.
    if (g_kon.pauseOpened && !g_kon.dismounted) {
        const uint32_t d = t - g_kon.pauseTick;
        const bool paused = dComIfGp_isPauseFlag() != 0;
        if (d > 0 && d % 60 == 0) {
            logInfo("konsum: Menue-Stand d=%u phase=%d rideFrames=%u pause=%d", static_cast<unsigned>(d),
                    static_cast<int>(c.sess.phase), static_cast<unsigned>(c.sess.rideFrames), paused ? 1 : 0);
        }
        if (d == 60) {
            c.tapB();
            c.note("konsum: B im Menue (darf nicht absteigen)");
        }
        if (d == 120 && paused) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            c.note("konsum: START (Menue zu)");
        }
        if (d >= 200 && !paused && c.sess.phase == Phase::Riding) {
            c.tapB();
            g_kon.dismounted = true;
            g_kon.dismountTick = t;
            c.note("konsum: B (absteigen)");
        }
        if (d >= 900 && !g_kon.dismounted) {
            c.note("konsum: timeout in Menue-Phase ohne Absteigen");
            return true;
        }
    }
    if (g_kon.dismounted && t == g_kon.dismountTick + 200) {
        c.note("konsum: timeline complete");
        return true;
    }
    if (g_kon.dismounted == false && c.sess.phase == Phase::Riding && c.sess.rideFrames > 900) {
        c.note("konsum: timeout without dismount");
        return true;
    }
    return false;
}

const Scenario kKonsum = {
    "konsum",
    "Ruf, Ritt, jede Ritt-Taste einzeln tippen, Menue mit START, B im Menue ohne Absteigen, dann B",
    konsumTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "ruf-spam": Ruf, Absteigen per B, danach R+B-Chords in kurzen Abstaenden, solange der Vogel noch
// fliegt. Jeder Chord muss abgelehnt werden (B bleibt), es darf kein zweiter Vogel entstehen.
// ---------------------------------------------------------------------------------------------

struct SpamState {
    bool armed = false;
    uint32_t start = 0;
    bool rideSeen = false;
    uint32_t rideAt = 0;
    bool dismounted = false;
    uint32_t dismountTick = 0;
    uint32_t chords = 0;
};
SpamState g_spam;

bool rufSpamTick(Context& c) {
    const uint32_t t = c.frame;
    if (!g_spam.armed && t >= kZyCallAt && c.canCall()) {
        g_spam.armed = true;
        g_spam.start = t;
        c.note("ruf-spam: call chord armed");
    }
    if (g_spam.armed && !g_spam.rideSeen) {
        if (t >= g_spam.start && t < g_spam.start + kZyHold) {
            c.holdR();
        }
        if (t == g_spam.start + kZyPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_spam.rideSeen = true;
            g_spam.rideAt = c.sess.rideFrames;
        }
    }
    if (g_spam.rideSeen && !g_spam.dismounted && c.sess.phase == Phase::Riding &&
        c.sess.rideFrames >= g_spam.rideAt + 60) {
        c.tapB();
        g_spam.dismounted = true;
        g_spam.dismountTick = t;
        c.note("ruf-spam: dismount");
    }
    if (g_spam.dismounted && t >= g_spam.dismountTick + 10 && t < g_spam.dismountTick + 80 &&
        (t - g_spam.dismountTick) % 6 == 0) {
        c.holdR();
        c.tapB();
        ++g_spam.chords;
        c.note("ruf-spam: R+B chord while the bird is still active");
    }
    if (g_spam.dismounted && t == g_spam.dismountTick + 200) {
        c.note("ruf-spam: timeline complete");
        return true;
    }
    return false;
}

const Scenario kRufSpam = {
    "ruf-spam",
    "Ruf, Absteigen, dann R+B-Chords waehrend der Vogel noch fliegt (alle muessen abgelehnt werden)",
    rufSpamTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "leben3": dreimal rufen, 240 Ritt-Frames fliegen, mit B absteigen, 160 Frames warten. Herzen vor und
// nach jedem Zyklus in das Log (dComIfGs_getLife). Nur Messung, keine Wertung im Szenario.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kLebenRideFrames = 240;
constexpr uint32_t kLebenWaitTicks = 160;
constexpr uint32_t kLebenCycles = 3;

struct LebenState {
    uint32_t cycle = 0;
    int stage = 0;  // 0 Ruf vorbereiten, 1 Chord, 2 Ritt, 3 Warten
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
};
LebenState g_leb;

bool leben3Tick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_leb.stage) {
    case 0:
        if (c.canCall() && t >= kZyCallAt) {
            g_leb.t0 = t;
            g_leb.stage = 1;
            c.note("leben3: call");
            logInfo("autotest: LEBEN vor Zyklus %u life=%u/%u", g_leb.cycle + 1,
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
        }
        break;
    case 1:
        if (t >= g_leb.t0 && t < g_leb.t0 + kZyHold) {
            c.holdR();
        }
        if (t == g_leb.t0 + kZyPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_leb.stage = 2;
            g_leb.rideAt = c.sess.rideFrames;
        }
        break;
    case 2:
        if (c.sess.phase == Phase::Riding && c.sess.rideFrames >= g_leb.rideAt + kLebenRideFrames) {
            c.tapB();
            c.note("leben3: B (absteigen)");
            logInfo("autotest: LEBEN nach Ritt vor Abstieg life=%u/%u",
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
            g_leb.t0 = t;
            g_leb.stage = 3;
        } else if (c.sess.phase != Phase::Riding && c.sess.phase != Phase::Dismounting) {
            c.note("leben3: ritt verloren, zyklus abgebrochen");
            return true;
        }
        break;
    case 3:
        if (t >= g_leb.t0 + kLebenWaitTicks) {
            ++g_leb.cycle;
            logInfo("autotest: LEBEN nach Zyklus %u life=%u/%u phase=%s", g_leb.cycle,
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()),
                phaseName(c.sess.phase));
            if (g_leb.cycle >= kLebenCycles) {
                c.note("leben3: complete");
                return true;
            }
            g_leb.stage = 0;
        }
        break;
    default:
        return true;
    }
    return false;
}

const Scenario kLeben3 = {
    "leben3",
    "Dreimal rufen, 240 Ritt-Frames, B, 160 Frames warten; Herzen je Zyklus im Log",
    leben3Tick,
};

// ---------------------------------------------------------------------------------------------
// Politik-Szenarien (Auftrag politik, Teil 2). Nur Testbuild. Gemessen wird ueber das Log (Leben, Bodenabstand, Phase).
// Das Ritt-Ende und alle Sperren macht der Kern. Schaden kommt aus dem Spiel (Fix-C-Diagnose "DMG ...") oder ueber
// die Test-API (politik-tod), damit Umgebungs- und Testschaden getrennt ausgewiesen sind.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kPolCallAt = 300;        // erster Ruf, sobald Link bereit ist
constexpr uint32_t kPolHold = 30;           // R gehalten, Ticks
constexpr uint32_t kPolPressOffset = 15;    // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kPolCallTimeout = 900;   // Wartezeit auf Ruf-Bereitschaft nach einem Zyklus
constexpr uint32_t kPolRideTimeout = 700;   // Obergrenze je Zyklus im Ritt (Ritt-Frames)
constexpr uint32_t kPolEndTimeout = 480;    // Obergrenze bis zum Ende nach B
constexpr uint32_t kPolIdle = 1800;         // 60 s ohne Eingabe (30 Hz)
constexpr uint32_t kPolCycles = 3;          // hoch, knapp, Abgrund
constexpr uint32_t kPolLogEvery = 60;       // Log-Takt im Ritt
constexpr float kPolNoGround = -1.0e5f;     // wie ride: checkGroundHeight darunter = kein Boden

// Bodenabstand des eigenen Vogels. false: kein Boden darunter (Abgrund).
bool polClearance(float* clearance) {
    daKago_c* bird = liveBird();
    if (bird == nullptr) {
        return false;
    }
    const float ground = bird->checkGroundHeight(bird->current.pos, nullptr);
    if (ground <= kPolNoGround) {
        return false;
    }
    *clearance = bird->current.pos.y - ground;
    return true;
}

const char* polCycleName(uint32_t cycle) {
    switch (cycle) {
    case 0:
        return "hoch";
    case 1:
        return "knapp";
    default:
        return "Abgrund";
    }
}

// Absteig-Bedingung je Zyklus: true, wenn jetzt mit B abgestiegen werden soll.
bool polWantDismount(uint32_t cycle, uint32_t ride, float clearance, bool hasGround) {
    switch (cycle) {
    case 0:  // hoch: steigen, bis ueber Boden etwa 1000 Einheiten erreicht sind
        return (hasGround && clearance >= 1000.0f) || ride >= kPolRideTimeout;
    case 1:  // knapp: sinken, bis unter 300 Einheiten ueber dem Boden
        return (hasGround && clearance <= 300.0f) || ride >= kPolRideTimeout;
    default:  // Abgrund: steigen, bis kein Boden mehr darunter liegt oder der Abstand 2000 erreicht
        return !hasGround || clearance >= 2000.0f || ride >= kPolRideTimeout;
    }
}

enum class PolStage : uint8_t { Call, Chord, Ride, Wait, Idle };

struct PolZyState {
    uint32_t cycle = 0;      // 0 hoch, 1 knapp, 2 Abgrund
    PolStage stage = PolStage::Call;
    uint32_t t0 = 0;         // Frame des Rufs
    uint32_t rideAt = 0;     // Ritt-Frame beim Ritt-Start
    uint32_t stageAt = 0;    // Frame des letzten Stufenwechsels
    uint32_t bTries = 0;     // wiederholte B-Tipps, falls der Vogel nicht absteigt
    uint32_t lifeAtB = 0;
};
PolZyState g_polZy;

// politik-zyklus: drei Zyklen Ruf, Ritt, B auf drei Hoehen, danach 60 s ohne Eingabe. Log: Leben vor/nach, Abstand.
bool politikZyklusTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_polZy.stage) {
    case PolStage::Call:
        if (c.canCall() && t >= kPolCallAt) {
            g_polZy.t0 = t;
            g_polZy.stage = PolStage::Chord;
            logInfo("politik: zyklus %u (%s) Ruf, Leben %u/%u", g_polZy.cycle + 1, polCycleName(g_polZy.cycle),
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
            c.note("politik: Ruf-Kombination (R halten, B tippen)");
        } else if (g_polZy.cycle > 0 && t - g_polZy.stageAt > kPolCallTimeout) {
            c.note("politik: kein Ruf moeglich (Link nicht bereit), Szenario endet");
            return true;
        }
        break;
    case PolStage::Chord: {
        if (t >= g_polZy.t0 && t < g_polZy.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polZy.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polZy.rideAt = c.sess.rideFrames;
            g_polZy.stage = PolStage::Ride;
            logInfo("politik: zyklus %u Ritt-Start, Leben %u", g_polZy.cycle + 1,
                static_cast<unsigned>(dComIfGs_getLife()));
        } else if (t > g_polZy.t0 + kPolHold + 600) {
            c.note("politik: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    }
    case PolStage::Ride: {
        if (c.sess.phase != Phase::Riding) {
            c.note("politik: Ritt endete vor dem B-Tipp");
            g_polZy.stage = PolStage::Wait;
            g_polZy.stageAt = t;
            break;
        }
        const uint32_t ride = c.sess.rideFrames - g_polZy.rideAt;
        float clearance = 0.0f;
        const bool hasGround = polClearance(&clearance);
        if (ride % kPolLogEvery == 0) {
            logInfo("politik: zyklus %u (%s) ritt f=%u boden=%d abstand=%.1f leben=%u", g_polZy.cycle + 1,
                polCycleName(g_polZy.cycle), static_cast<unsigned>(ride), hasGround ? 1 : 0,
                static_cast<double>(clearance), static_cast<unsigned>(dComIfGs_getLife()));
        }
        if (!polWantDismount(g_polZy.cycle, ride, clearance, hasGround)) {
            c.stick(0.0f, g_polZy.cycle == 1 ? 0.6f : -0.6f);  // knapp sinkt, hoch und Abgrund steigen
            break;
        }
        c.tapB();
        g_polZy.lifeAtB = dComIfGs_getLife();
        logInfo("politik: zyklus %u (%s) B-Tipp bei Ritt-Frame %u, boden=%d, abstand=%.1f, leben=%u",
            g_polZy.cycle + 1, polCycleName(g_polZy.cycle), static_cast<unsigned>(ride), hasGround ? 1 : 0,
            static_cast<double>(clearance), static_cast<unsigned>(g_polZy.lifeAtB));
        g_polZy.stage = PolStage::Wait;
        g_polZy.stageAt = t;
        g_polZy.bTries = 0;
        break;
    }
    case PolStage::Wait: {
        const bool ended = c.sess.phase == Phase::Idle;
        const bool timeout = t - g_polZy.stageAt > kPolEndTimeout;
        if (!ended && !timeout) {
            if (c.sess.phase == Phase::Riding && g_polZy.bTries < 3 &&
                t - g_polZy.stageAt == 60 * (g_polZy.bTries + 1)) {
                ++g_polZy.bTries;
                c.tapB();
                logInfo("politik: B-Tipp wiederholt (%u), Ritt laeuft noch", g_polZy.bTries);
            }
            break;
        }
        if (timeout && !ended) {
            c.note("politik: Zeitlimit nach B, Phase laeuft noch");
        }
        logInfo("politik: zyklus %u Ende nach %u Ticks, Leben vorher %u nachher %u/%u, phase=%s", g_polZy.cycle + 1,
            static_cast<unsigned>(t - g_polZy.stageAt), static_cast<unsigned>(g_polZy.lifeAtB),
            static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()),
            phaseName(c.sess.phase));
        ++g_polZy.cycle;
        g_polZy.stageAt = t;
        if (g_polZy.cycle >= kPolCycles) {
            g_polZy.stage = PolStage::Idle;
            logInfo("politik: 60 s ohne Eingabe, Leben %u", static_cast<unsigned>(dComIfGs_getLife()));
        } else {
            g_polZy.stage = PolStage::Call;
        }
        break;
    }
    case PolStage::Idle:
        if (t - g_polZy.stageAt >= kPolIdle) {
            logInfo("politik: 60 s ohne Eingabe beendet, Leben %u/%u", static_cast<unsigned>(dComIfGs_getLife()),
                static_cast<unsigned>(dComIfGs_getMaxLife()));
            c.note("politik: Zyklen und Leerlauf beendet");
            return true;
        }
        if ((t - g_polZy.stageAt) % 300 == 0) {
            logInfo("politik: Leerlauf %u Ticks, leben=%u phase=%s", static_cast<unsigned>(t - g_polZy.stageAt),
                static_cast<unsigned>(dComIfGs_getLife()), phaseName(c.sess.phase));
        }
        break;
    }
    return false;
}

const Scenario kPolZyklus = {
    "politik-zyklus",
    "Drei Zyklen Ruf, Ritt, B: hoch, knapp, Abgrund; danach 60 s Leerlauf; Leben und Abstand im Log",
    politikZyklusTick,
};

// politik-titel: Ritt, dann Home-Reset (BTN2 = mDoRst_resetCallBack) im Ritt. Erwartet: Ritt endet mit Titel, kein Haenger.
constexpr uint32_t kPolTitelRide = 150;     // Ritt-Frames vor dem Reset
constexpr uint32_t kPolTitelAfter = 1200;   // Beobachtung nach dem Reset (40 s)

struct PolTiState {
    int stage = 0;           // 0 Ruf, 1 Chord, 2 Ritt, 3 nach dem Reset
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t resetAt = 0;
    uint32_t starts = 0;     // START-Tipps nach dem Reset
};
PolTiState g_polTi;

bool politikTitelTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_polTi.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g_polTi.t0 = t;
            g_polTi.stage = 1;
            c.note("politik-titel: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_polTi.t0 && t < g_polTi.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polTi.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polTi.rideAt = c.sess.rideFrames;
            g_polTi.stage = 2;
            logInfo("politik-titel: Ritt-Start, Leben %u", static_cast<unsigned>(dComIfGs_getLife()));
        } else if (t > g_polTi.t0 + kPolHold + 600) {
            c.note("politik-titel: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("politik-titel: Ritt endete vor dem Reset, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g_polTi.rideAt >= kPolTitelRide) {
            logInfo("politik-titel: Reset im Ritt (Home-Menue BTN2 = mDoRst_resetCallBack), Leben %u/%u, phase=%s",
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()),
                phaseName(c.sess.phase));
            mDoRst_resetCallBack(-1, nullptr);
            g_polTi.resetAt = t;
            g_polTi.stage = 3;
        }
        break;
    case 3:
        if (t - g_polTi.resetAt >= kPolTitelAfter) {
            c.note("politik-titel: Beobachtung nach dem Reset beendet");
            return true;
        }
        if ((t - g_polTi.resetAt) % 60 == 0) {
            logInfo("politik-titel: nach Reset %u Ticks phase=%s link=%d cargo=%d midnaKago=%d",
                static_cast<unsigned>(t - g_polTi.resetAt), phaseName(c.sess.phase), liveLink() != nullptr ? 1 : 0,
                liveLink() != nullptr && liveLink()->checkCargoCarry() ? 1 : 0,
                daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr ? 1 : 0);
        }
        // Kartenabfrage des Spiels ("Would you like to create a save file", Standard Nein) mit A bestaetigen, dann
        // Titel (START) und Dateiwahl (A) bedienen. Beleg: die Menues reagieren nach dem Ritt-Ende.
        {
            const uint32_t d = t - g_polTi.resetAt;
            if (d == 120) {
                c.tapA();
                logInfo("politik-titel: A-Tipp 1 (Kartenabfrage, Standard Nein)");
            } else if (d == 300) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
                logInfo("politik-titel: START-Tipp 1 (Titel)");
            } else if (d == 420) {
                c.tapA();
                logInfo("politik-titel: A-Tipp 2 (Dateiwahl)");
            } else if (d == 540) {
                c.tapA();
                logInfo("politik-titel: A-Tipp 3 (Bestaetigung)");
            } else if (d == 660) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
                logInfo("politik-titel: START-Tipp 2");
            }
        }
        break;
    }
    return false;
}

const Scenario kPolTitel = {
    "politik-titel",
    "Ritt, nach 150 Ritt-Frames Home-Reset (BTN2) im Ritt; Ritt-Ende ueber Titel, danach START-Tipps",
    politikTitelTick,
};

// politik-tod: Ritt, dann Leben auf 1 und Test-Schaden 2 (setDamagePoint). Erwartet: Ritt endet vor dem Tod sauber.
constexpr uint32_t kPolTodRide = 120;      // Ritt-Frames vor dem Test-Schaden
constexpr uint32_t kPolTodAfter = 900;     // Beobachtung nach dem Ritt-Ende

struct PolToState {
    int stage = 0;           // 0 Ruf, 1 Chord, 2 Ritt, 3 Tod abwarten, 4 Beobachtung
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t hitAt = 0;
    uint32_t endAt = 0;
};
PolToState g_polTo;

bool politikTodTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_polTo.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g_polTo.t0 = t;
            g_polTo.stage = 1;
            c.note("politik-tod: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_polTo.t0 && t < g_polTo.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polTo.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polTo.rideAt = c.sess.rideFrames;
            g_polTo.stage = 2;
            logInfo("politik-tod: Ritt-Start, Leben %u", static_cast<unsigned>(dComIfGs_getLife()));
        } else if (t > g_polTo.t0 + kPolHold + 600) {
            c.note("politik-tod: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2: {
        if (c.sess.phase != Phase::Riding) {
            c.note("politik-tod: Ritt endete vor dem Test-Schaden, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g_polTo.rideAt < kPolTodRide) {
            break;
        }
        daAlink_c* link = liveLink();
        if (link == nullptr) {
            c.note("politik-tod: kein Link, Szenario endet");
            return true;
        }
        dComIfGs_setLife(1);
        const int r = link->setDamagePoint(2, 0, 1, 0);  // Quelle: Test-API, nicht das Spiel
        logInfo("politik-tod: Test-Schaden 2 bei Leben 1 (Quelle Test-API setDamagePoint), Rueckgabe %d, leben=%u, "
                "proc=%u",
            r, static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(link->mProcID));
        g_polTo.hitAt = t;
        g_polTo.stage = 3;
        break;
    }
    case 3: {
        daAlink_c* link = liveLink();
        if ((t - g_polTo.hitAt) % 30 == 0) {
            logInfo("politik-tod: %u Ticks nach Schaden, leben=%u phase=%s proc=%u link=%d",
                static_cast<unsigned>(t - g_polTo.hitAt), static_cast<unsigned>(dComIfGs_getLife()),
                phaseName(c.sess.phase), link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u,
                link != nullptr ? 1 : 0);
        }
        if (c.sess.phase == Phase::Idle) {
            g_polTo.endAt = t;
            g_polTo.stage = 4;
            logInfo("politik-tod: Ritt beendet (Phase Idle) %u Ticks nach Schaden, leben=%u",
                static_cast<unsigned>(t - g_polTo.hitAt), static_cast<unsigned>(dComIfGs_getLife()));
        } else if (t - g_polTo.hitAt > 600) {
            c.note("politik-tod: Zeitlimit ohne Ritt-Ende");
            return true;
        }
        break;
    }
    case 4:
        if (t - g_polTo.endAt >= kPolTodAfter) {
            c.note("politik-tod: Beobachtung nach Ritt-Ende beendet");
            return true;
        }
        if ((t - g_polTo.endAt) % 120 == 0) {
            logInfo("politik-tod: nach Ritt-Ende %u Ticks leben=%u", static_cast<unsigned>(t - g_polTo.endAt),
                static_cast<unsigned>(dComIfGs_getLife()));
        }
        break;
    }
    return false;
}

const Scenario kPolTod = {
    "politik-tod",
    "Ritt, dann Leben 1 und Test-Schaden 2: Ritt-Ende vor dem Tod, danach Beobachtung",
    politikTodTick,
};

// politik-menue: Erkundung des Pausenmenues im Ritt (START, dann R-Tipps). Nur Beobachtung ueber Screenshots und Log.
constexpr uint32_t kPolMenuRide = 120;

struct PolMenuState {
    int stage = 0;           // 0 Ruf, 1 Chord, 2 Ritt, 3 Menue
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t menuAt = 0;
    uint32_t presses = 0;
};
PolMenuState g_polMn;

bool politikMenueTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_polMn.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g_polMn.t0 = t;
            g_polMn.stage = 1;
            c.note("politik-menue: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_polMn.t0 && t < g_polMn.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polMn.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polMn.rideAt = c.sess.rideFrames;
            g_polMn.stage = 2;
        } else if (t > g_polMn.t0 + kPolHold + 600) {
            c.note("politik-menue: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.rideFrames - g_polMn.rideAt >= kPolMenuRide) {
            g_polMn.menuAt = t;
            g_polMn.stage = 3;
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("politik-menue: START im Ritt gedrueckt, pause=%u phase=%s",
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        break;
    case 3: {
        const uint32_t m = t - g_polMn.menuAt;
        if (m > 0 && m % 60 == 0 && g_polMn.presses < 6) {
            c.tap(static_cast<uint32_t>(PAD_TRIGGER_R));
            ++g_polMn.presses;
            logInfo("politik-menue: R-Tipp %u, pause=%u phase=%s", g_polMn.presses,
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        if (m >= 900) {
            c.note("politik-menue: Erkundung beendet");
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kPolMenue = {
    "politik-menue",
    "Ritt, dann START und R-Tipps im Pausenmenue (Erkundung fuer den Speichertest)",
    politikMenueTick,
};

// politik-speichern: Ritt, dann START und Pausenmenue. Cursor per D-Pad zur Speichern-Schaltflaeche, A, danach R
// (Menue bedienbar?) und B (Menue zu). Erwartet: Speichern im Ritt abgelehnt (Hinweis), kein Kartenschreiben.
constexpr uint32_t kPolSpRide = 120;   // Ritt-Frames vor dem START
constexpr uint32_t kPolSpEnd = 420;    // Ticks nach dem START: Ende

struct PolSpState {
    int stage = 0;           // 0 Ruf, 1 Chord, 2 Ritt, 3 Menue
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t menuAt = 0;
};
PolSpState g_polSp;
bool g_polSpJa = false;     // politik-speichern-ja: im Kartenabfragefenster "Ja" waehlen

bool politikSpeichernTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_polSp.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g_polSp.t0 = t;
            g_polSp.stage = 1;
            c.note("politik-speichern: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_polSp.t0 && t < g_polSp.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polSp.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polSp.rideAt = c.sess.rideFrames;
            g_polSp.stage = 2;
        } else if (t > g_polSp.t0 + kPolHold + 600) {
            c.note("politik-speichern: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("politik-speichern: Ritt endete vor dem Menue, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g_polSp.rideAt >= kPolSpRide) {
            g_polSp.menuAt = t;
            g_polSp.stage = 3;
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("politik-speichern: START im Ritt gedrueckt, pause=%u phase=%s",
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        break;
    case 3: {
        const uint32_t m = t - g_polSp.menuAt;
        // Cursor per D-Pad nach unten zur Speichern-Schaltflaeche (drei Tipps), dann A (Speichern-Auswahl).
        if (m == 30 || m == 60 || m == 90) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));
            logInfo("politik-speichern: runter-Tipp m=%u pause=%u", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m == 120) {
            c.tapA();
            logInfo("politik-speichern: A-Tipp (Speichern-Auswahl) m=%u pause=%u phase=%s", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        if (m == 180 && !g_polSpJa) {
            c.tap(static_cast<uint32_t>(PAD_TRIGGER_R));
            logInfo("politik-speichern: R-Tipp (Menue bedienbar?) m=%u pause=%u", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m == 360 && !g_polSpJa) {
            c.tapB();
            logInfo("politik-speichern: B-Tipp (Menue zu) m=%u pause=%u phase=%s", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        if (g_polSpJa && (m == 720 || m == 750 || m == 780)) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_LEFT));
            logInfo("politik-speichern-ja: LEFT-Tipp (Ja waehlen) m=%u pause=%u", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (g_polSpJa && m == 810) {
            c.tapA();
            logInfo("politik-speichern-ja: A-Tipp (Bestaetigen) m=%u pause=%u phase=%s", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
        }
        if (m > 360 && m % 30 == 0) {
            logInfo("politik-speichern: nach B m=%u pause=%u phase=%s link=%d", static_cast<unsigned>(m),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase), liveLink() != nullptr ? 1 : 0);
        }
        if (m >= (g_polSpJa ? 960u : kPolSpEnd)) {
            c.note("politik-speichern: Beobachtung beendet");
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kPolSpeichern = {
    "politik-speichern",
    "Ritt, dann START, Speichern-Auswahl im Pausenmenue (Ablehnung im Ritt), danach R und B",
    politikSpeichernTick,
};

bool politikSpeichernJaTick(Context& c) {
    g_polSpJa = true;
    return politikSpeichernTick(c);
}

const Scenario kPolSpeichernJa = {
    "politik-speichern-ja",
    "wie politik-speichern, aber im Kartenabfragefenster Ja waehlen (Schreibpfad im Ritt)",
    politikSpeichernJaTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "spiel-speichern" (nur Testbuild, E3). Speichern AUSSERHALB des Ritts: START, Speichern-Eintrag (drei Tipps
// nach unten, A), Ja (drei LEFT-Tipps, damit die Auswahl sicher auf Ja steht, dann A). Erzeugt die Karte im Testordner.
// Der Auswerter kopiert den Testordner und laesst "politik-speichern-ja" mit dieser Kopie laufen (sha256 muss gleich bleiben).
constexpr uint32_t kSpielSpStart = 300;     // Ticks bis zum START (Link bereit, kein Ritt)

// E3 (nur Testbuild): Kartenschreiben ueber denselben Schreibweg wie der Autosave (dusk/autosave.cpp, writeAutoSave):
// Spielstand in den Puffer, Pruefsummen, dann mDoMemCd_Ctrl_c::save. Der Kern verweigert diesen Aufruf im Ritt
// (cardSavePre); ausserhalb des Ritts schreibt er die Karte. Der Test ruft den Weg gezielt auf, das Menue bleibt aus.
uint8_t g_testSaveBuf[QUEST_LOG_SIZE * 3];
bool g_testWritePending = false;  // Schreibauftrag gestellt, Ergebnis noch offen
uint32_t g_testWriteTicks = 0;    // Ticks seit dem Auftrag

bool testCardWrite(Context& c, const char* where) {
    stage_stag_info_class* stagInfo = dComIfGp_getStageStagInfo();
    if (stagInfo == nullptr) {
        c.note("kartentest: kein Stage-Info, kein Schreibauftrag");
        return false;
    }
    const int stageNo = dStage_stagInfo_GetSaveTbl(stagInfo);
    dComIfGs_putSave(stageNo);
    dComIfGs_setMemoryToCard(g_testSaveBuf, dComIfGs_getDataNum());
    mDoMemCdRWm_SetCheckSumGameData(g_testSaveBuf, dComIfGs_getDataNum());
    uint8_t* save = g_testSaveBuf;
    for (int i = 0; i < 3; i++) {
        mDoMemCdRWm_TestCheckSumGameData(save);
        save += QUEST_LOG_SIZE;
    }
    g_mDoMemCd_control.save(g_testSaveBuf, sizeof(g_testSaveBuf), 0);
    g_testWritePending = true;
    g_testWriteTicks = 0;
    logInfo("kartentest: Schreibauftrag %s, Phase %s, Stage %d", where, phaseName(c.sess.phase), stageNo);
    return true;
}

// Ergebnis des Schreibauftrags (SaveSync: 0 = laeuft, sonst fertig oder Fehler). Zeitlimit 1200 Ticks.
void pollTestWrite() {
    if (!g_testWritePending) {
        return;
    }
    ++g_testWriteTicks;
    const int32_t st = g_mDoMemCd_control.SaveSync();
    if (st == 0 && g_testWriteTicks < 1200) {
        return;
    }
    g_testWritePending = false;
    if (st == 0) {
        logInfo("kartentest: Schreiben ohne Ergebnis nach %u Ticks", static_cast<unsigned>(g_testWriteTicks));
    } else {
        logInfo("kartentest: Schreibergebnis SaveSync=%d nach %u Ticks", static_cast<int>(st),
            static_cast<unsigned>(g_testWriteTicks));
    }
}

struct SpielSpState {
    bool started = false;
    uint32_t t0 = 0;
};
SpielSpState g_spielSp;

bool spielSpeichernTick(Context& c) {
    const uint32_t t = c.frame;
    pollTestWrite();
    if (t == kSpielSpStart - 60) {
        testCardWrite(c, "ausserhalb des Ritts (vor START)");  // E3: Karte anlegen, Schreibweg des Autosave
    }
    if (!g_spielSp.started) {
        if (t < kSpielSpStart) {
            return false;
        }
        g_spielSp.started = true;
        g_spielSp.t0 = t;
        c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
        c.note("spiel-speichern: START ohne Ritt (Speichern ausserhalb des Ritts)");
        return false;
    }
    const uint32_t m = t - g_spielSp.t0;
    if (m == 30 || m == 60 || m == 90) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));
    }
    if (m == 120) {
        c.tapA();
        c.note("spiel-speichern: A (Speichern-Eintrag)");
    }
    if (m == 150 || m == 180 || m == 210) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_LEFT));
    }
    if (m == 240) {
        c.tapA();
        c.note("spiel-speichern: A (Ja bestaetigen)");
    }
    if (m >= 420) {
        logInfo("spiel-speichern: Ende, Leben %u/%u, Phase %s", static_cast<unsigned>(dComIfGs_getLife()),
            static_cast<unsigned>(dComIfGs_getMaxLife()), phaseName(c.sess.phase));
        return true;
    }
    return false;
}

const Scenario kSpielSpeichern = {
    "spiel-speichern",
    "Speichern ausserhalb des Ritts (START, Speichern, Ja): erzeugt die Karte fuer den Speichertest (E3)",
    spielSpeichernTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "politik-schreiben" (nur Testbuild, E3). Ritt, danach im Ritt der kartentest-Schreibweg (Autosave-Pfad).
// Erwartet: Der Kern verweigert das Schreiben (Hinweis), der Ritt laeuft weiter. Die Karte bleibt unveraendert
// (sha256 vor und nach dem Lauf, Auswertung ausserhalb des Spiels).
// ---------------------------------------------------------------------------------------------
constexpr uint32_t kPolWrRide = 120;   // Ritt-Frames vor dem Schreibversuch
constexpr uint32_t kPolWrAfter = 420;  // Ticks nach dem Schreibversuch: Ende

struct PolWrState {
    int stage = 0;  // 0 Ruf, 1 Chord, 2 Ritt, 3 nach dem Schreibversuch
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t writeAt = 0;
};
PolWrState g_polWr;

bool politikSchreibenTick(Context& c) {
    const uint32_t t = c.frame;
    pollTestWrite();
    switch (g_polWr.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g_polWr.t0 = t;
            g_polWr.stage = 1;
            c.note("politik-schreiben: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_polWr.t0 && t < g_polWr.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_polWr.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_polWr.rideAt = c.sess.rideFrames;
            g_polWr.stage = 2;
        } else if (t > g_polWr.t0 + kPolHold + 600) {
            c.note("politik-schreiben: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("politik-schreiben: Ritt endete vor dem Schreibversuch, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g_polWr.rideAt >= kPolWrRide) {
            g_polWr.writeAt = t;
            g_polWr.stage = 3;
            testCardWrite(c, "im Ritt");
        }
        break;
    case 3: {
        const uint32_t m = t - g_polWr.writeAt;
        if (m > 0 && m % 60 == 0) {
            logInfo("politik-schreiben: nach dem Schreibversuch m=%u phase=%s link=%d", static_cast<unsigned>(m),
                phaseName(c.sess.phase), liveLink() != nullptr ? 1 : 0);
        }
        if (m >= kPolWrAfter) {
            c.note("politik-schreiben: Beobachtung beendet");
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kPolSchreiben = {
    "politik-schreiben",
    "Ritt, dann im Ritt der Kartenschreibweg (Autosave-Pfad): Ablehnung im Ritt, Ritt laeuft weiter (E3)",
    politikSchreibenTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "landung-hoch" (nur Testbuild, Auftrag Landeanflug aus > 8000). Ruf, Ritt, 90 Ritt-Frames, dann wird der
// Vogel fuer diesen Test um 8000 Einheiten angehoben (Testeingriff nur im Testbuild), 60 Ticks Flug, danach B
// (Landeanflug). Gemessen im Log: Hoehe vor und nach dem Anheben, Leben vor dem Absteigen und nach dem Ritt-Ende,
// Freigabe (ride.cpp: landing done, clearance).
constexpr uint32_t kLandHochRideFrames = 90;
constexpr float kLandHochLift = 8000.0f;
constexpr uint32_t kLandHochHold = 60;
constexpr uint32_t kLandHochMaxAfterB = 900;
constexpr uint32_t kLandHochAfterB2 = 300;  // F5 R2-A: Beobachtung nach dem zweiten B (Ticks), Abflug 100, Reserve

// F5 R2-A (nur Testcode): KFF_TEST_B2_AFTER=<n> drueckt B ein zweites Mal n Ticks nach dem ersten (Landeanflug in der
// Luft = sofort los, Phase Dismounting). 0 = aus.
uint32_t landHochB2After() {
    const char* v = std::getenv("KFF_TEST_B2_AFTER");
    const unsigned long n = v != nullptr ? std::strtoul(v, nullptr, 10) : 0UL;
    return (n >= 1UL && n < 100000UL) ? static_cast<uint32_t>(n) : 0u;
}

struct LandHochState {
    int stage = 0;          // 0 Ruf, 1 Chord, 2 Ritt, 3 Anheben und Flug, 4 B und Landeanflug, 5 nur Beobachten (ordon-hoch)
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t liftAt = 0;
    uint32_t bAt = 0;
    uint32_t b2At = 0;      // F5 R2-A: Tick des zweiten B (0 = keiner)
    bool b2Done = false;
    bool noB = false;       // F5 R2-C (ordon-hoch): kein B nach dem Anheben
};
LandHochState g_landHoch;

// F5 R2-C (nur Testbuild): Ruf auch als Mensch fuer ordon-hoch. Das Modul autowolf verwandelt Link dann und ruft den Vogel
// (onHumanCall). Gleiche Bedingungen wie dort: Mensch, Phase Idle, Boden, kein Ereignis, kein Vogel, kein Verwandlungsproc.
bool canCallHumanOrWolf(const Context& c) {
    daAlink_c* link = liveLink();
    if (link == nullptr || link->checkCargoCarry() || link->checkEventRun() || dComIfGp_event_runCheck() ||
        c.sess.birdId != kNoProc) {
        return false;
    }
    if (link->checkWolf() != 0) {
        return true;
    }
    return c.sess.phase == Phase::Idle && link->mLinkAcch.ChkGroundHit() && link->mProcID != daAlink_c::PROC_DEAD &&
           link->mProcID != daAlink_c::PROC_METAMORPHOSE && link->mProcID != daAlink_c::PROC_METAMORPHOSE_ONLY;
}

bool landungHochTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_landHoch.stage) {
    case 0:
        if ((g_landHoch.noB ? canCallHumanOrWolf(c) : c.canCall()) && t >= kPolCallAt) {
            g_landHoch.t0 = t;
            g_landHoch.stage = 1;
            logInfo("landung-hoch: Ruf, Leben %u/%u", static_cast<unsigned>(dComIfGs_getLife()),
                static_cast<unsigned>(dComIfGs_getMaxLife()));
            c.note("landung-hoch: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_landHoch.t0 && t < g_landHoch.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g_landHoch.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_landHoch.rideAt = c.sess.rideFrames;
            g_landHoch.stage = 2;
            logInfo("landung-hoch: Ritt gestartet, Leben %u/%u", static_cast<unsigned>(dComIfGs_getLife()),
                static_cast<unsigned>(dComIfGs_getMaxLife()));
        } else if (t > g_landHoch.t0 + kPolHold + (g_landHoch.noB ? 2400u : 600u)) {  // F5 R2-C: Verwandlung als Mensch braucht laenger
            c.note("landung-hoch: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("landung-hoch: Ritt endete vor dem Anheben, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g_landHoch.rideAt >= kLandHochRideFrames) {
            if (daKago_c* k = liveBird()) {
                const float y0 = k->current.pos.y;
                k->current.pos.y += kLandHochLift;
                logInfo("landung-hoch: Vogel um %.0f angehoben, y %.1f -> %.1f", static_cast<double>(kLandHochLift),
                    static_cast<double>(y0), static_cast<double>(k->current.pos.y));
            } else {
                logInfo("landung-hoch: kein Vogel zum Anheben");
            }
            g_landHoch.liftAt = t;
            g_landHoch.stage = 3;
        }
        break;
    case 3:
        if (c.sess.phase != Phase::Riding) {
            c.note("landung-hoch: Ritt endete vor dem B, Szenario endet");
            return true;
        }
        if (t >= g_landHoch.liftAt + kLandHochHold) {
            if (g_landHoch.noB) {  // F5 R2-C (ordon-hoch): Vogel oben, kein B
                c.note("ordon-hoch: Vogel oben, kein B, Entladen im Ritt (KFF_TEST_UNLOAD)");
                logInfo("ordon-hoch: Vogel oben, kein B (Test), Leben %u/%u", static_cast<unsigned>(dComIfGs_getLife()),
                    static_cast<unsigned>(dComIfGs_getMaxLife()));
                g_landHoch.stage = 5;
                break;
            }
            if (daAlink_c* link = liveLink()) {
                const uint32_t n = landGegnerRaeumen(link->current.pos);
                logInfo("landung-hoch: Landezone geraeumt (nur Testbuild): %u Gegner im Umkreis %.0f entfernt", n,
                    static_cast<double>(kLandGegnerRadius));
            }
            g_landHoch.bAt = t;
            c.tapB();
            c.note("landung-hoch: B (Landeanflug aus grosser Hoehe)");
            logInfo("landung-hoch: B gedrueckt, Leben %u/%u", static_cast<unsigned>(dComIfGs_getLife()),
                static_cast<unsigned>(dComIfGs_getMaxLife()));
            g_landHoch.stage = 4;
        }
        break;
    case 4: {
        const uint32_t b2 = landHochB2After();  // F5 R2-A (nur Testcode): zweites B nach so vielen Ticks
        if (b2 > 0) {
            if (!g_landHoch.b2Done && t - g_landHoch.bAt >= b2) {
                g_landHoch.b2Done = true;
                g_landHoch.b2At = t;
                c.tapB();
                float birdY = 0.0f;
                if (daKago_c* k = liveBird()) {
                    birdY = k->current.pos.y;
                }
                logInfo("landung-hoch: zweites B (Test) nach %u Ticks, phase=%s, Vogel y %.1f, Leben %u/%u",
                    static_cast<unsigned>(t - g_landHoch.bAt), phaseName(c.sess.phase), static_cast<double>(birdY),
                    static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
            }
            if (g_landHoch.b2Done && t - g_landHoch.b2At >= kLandHochAfterB2) {
                logInfo("landung-hoch: Zeitlimit nach dem zweiten B, phase=%s", phaseName(c.sess.phase));
                return true;
            }
            break;
        }
        if (c.sess.phase != Phase::Riding) {
            logInfo("landung-hoch: Ritt beendet nach B nach %u Ticks, Leben %u/%u", static_cast<unsigned>(t - g_landHoch.bAt),
                static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
            return true;
        }
        if (t - g_landHoch.bAt >= kLandHochMaxAfterB) {
            logInfo("landung-hoch: Zeitlimit nach B, Ritt noch aktiv");
            return true;
        }
        break;
    }
    case 5:
        break;  // F5 R2-C (ordon-hoch): Vogel oben, nur beobachten; das Entladen kommt aus KFF_TEST_UNLOAD
    default:
        return true;
    }
    return false;
}

const Scenario kLandungHoch = {
    "landung-hoch",
    "Ruf, Ritt, Vogel um 8000 angehoben (Test), B: Landeanflug aus grosser Hoehe (Freigabe, Herzen)",
    landungHochTick,
};

// F5 R2-C (nur Testbuild): wie landung-hoch, aber ohne B. Der Vogel bleibt oben, Entladen im Ritt per KFF_TEST_UNLOAD=1.
bool ordonHochTick(Context& c) {
    g_landHoch.noB = true;
    return landungHochTick(c);
}

const Scenario kOrdonHoch = {
    "ordon-hoch",
    "Ruf, Ritt, Vogel um 8000 angehoben, kein B (F5 R2-C: Entladen im Ritt auf der Hoehe, Ordon)",
    ordonHochTick,
};

// F5 R2-B Kontrolle (nur Testbuild): kein Ruf, keine Eingabe, Link steht im Stage-Start. Endet nur mit der Dauer des Laufs.
bool idleStartTick(Context& c) {
    if (c.frame == 1) {
        c.note("idle-start: keine Eingabe (Kontrolle R2-B)");
    }
    return false;
}

const Scenario kIdleStart = {
    "idle-start",
    "F5 R2-B Kontrolle: Link steht ohne Eingabe im Stage-Start (kein Ritt, Lebenslog)",
    idleStartTick,
};

// ---------------------------------------------------------------------------------------------
// Szenario "steuerung" (nur Testbuild). Pruefung der Ritt-Steuerung ueber den Kern-Autotest-Kontext:
//   1. Ruf mit digitalem R (R halten, B tippen).
//   2. Ritt 1: gerade Gaenge 1..5 (Weg je Tick aus der Vogelposition), danach Kreisflug links mit Gang 1..5
//      (Wendekreis je Gang, Radius = Weg/Gierwinkel), Gang hoch und runter, Dash (A) mit Sperrzeit (Ablehnung bei
//      f=470), Bremse (Z) auf Gang 5 mit Dash-Versuch, Kamera-Zoom (Steuerkreuz rechts/links), C-Stick-Orbit,
//      X-Schwenk, Fassrolle (Y), dann Absteigen mit Landeanflug (B) auf Gang 1. Herzen vor und nach dem Absteigen.
//   3. Ruf ablehnen im Schaden-Proc (Fix A, R+B-Chords) und im Fall-Proc (procWolfFallInit und R+B im selben
//      Tick). Danach Heilung auf das Maximum (nur Test), damit der Link am Eis nicht stirbt.
//   4. Ruf mit Analog-R 0,65 (muss angenommen werden), Ritt 2 mit Absteigen per B.
//   5. Ruf mit Analog-R 0,50 (darf nicht angenommen werden).
// Eingaben nur ueber Context (Kern-Autotest). Schaden und Fall loesen Spielfunktionen aus (setDamagePoint,
// procWolfDamageInit, procWolfFallInit); das ist kein Eingabepfad.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kSteuCallAt = 300;     // fruehester Ruf (Ticks seit dem ersten Link-Frame)
constexpr uint32_t kSteuCallHold = 30;    // R gehalten, Ticks
constexpr uint32_t kSteuCallTap = 15;     // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kSteuCallRetry = 150;  // ohne Ritt nach so vielen Ticks neuer Versuch (nur im Leerlauf)
constexpr uint32_t kSteuCallTries = 4;
constexpr uint32_t kSteuNegWait = 90;     // Analog-R 0,50: so lange ohne Vogel warten
constexpr uint32_t kSteuGiveUp = 4000;    // kein Ruf moeglich bis hierhin
constexpr uint32_t kSteuRest = 900;       // Zeitlimit fuer Abstieg, Ruhe und Teilversuche
constexpr uint32_t kSteuCalm = 60;        // Ticks in Folge in Ruhe, bevor der naechste Teil startet
constexpr uint32_t kSteuAfterLand = 150;  // nach dem Absteigen: Ticks bis zur Herz-Messung
constexpr uint32_t kSteuChordEvery = 5;   // R+B-Chord alle so vielen Ticks waehrend des Procs
constexpr uint32_t kSteuProcWait = 20;    // Schaden: so lange auf den Proc warten, dann procWolfDamageInit
constexpr uint32_t kSteuProcMax = 40;     // Teilversuch: Proc muss in dieser Zeit erreicht sein
constexpr uint32_t kSteuProcRun = 300;    // Zeitlimit der Chord-Phase
constexpr uint32_t kSteuRide1Pre = 915;   // Zustand vor dem Absteigen loggen
constexpr uint32_t kSteuRide1B = 920;     // B: Absteigen aus Ritt 1 (Gang 1, Landeanflug)
constexpr uint32_t kSteuStallFrom = 80;   // Stillstand-Ausweg ab diesem Ritt-Frame
constexpr uint32_t kSteuStallTicks = 20;  // so viele Ticks unter 0,5 Weg je Tick gelten als Stillstand
constexpr uint32_t kSteuEscapeTicks = 40; // Ausweichen: Steuerung rechts fuer so viele Ticks
constexpr uint32_t kSteuRide2B = 150;     // B: Absteigen aus Ritt 2
constexpr uint32_t kSteuNever = 0xFFFFFFFFu;

constexpr uint32_t kSUp = static_cast<uint32_t>(PAD_BUTTON_UP);
constexpr uint32_t kSDown = static_cast<uint32_t>(PAD_BUTTON_DOWN);
constexpr uint32_t kSLeft = static_cast<uint32_t>(PAD_BUTTON_LEFT);
constexpr uint32_t kSRight = static_cast<uint32_t>(PAD_BUTTON_RIGHT);
constexpr uint32_t kSA = static_cast<uint32_t>(PAD_BUTTON_A);
constexpr uint32_t kSB = static_cast<uint32_t>(PAD_BUTTON_B);
constexpr uint32_t kSX = static_cast<uint32_t>(PAD_BUTTON_X);
constexpr uint32_t kSY = static_cast<uint32_t>(PAD_BUTTON_Y);
constexpr uint32_t kSZ = static_cast<uint32_t>(PAD_TRIGGER_Z);

struct SteuTap {
    uint32_t at;
    uint32_t bits;
    const char* note;
};

constexpr SteuTap kSteuRideTaps[] = {
    {60, kSUp, "Gang 3 -> 4"},
    {90, kSUp, "Gang 4 -> 5"},
    {120, kSDown, "Gang 5 -> 4"},
    {126, kSDown, "Gang 4 -> 3"},
    {132, kSDown, "Gang 3 -> 2"},
    {165, kSDown, "Gang 2 -> 1"},
    {200, kSUp, "Gang 1 -> 2 (Kreisflug beginnt)"},
    {230, kSUp, "Gang 2 -> 3"},
    {260, kSUp, "Gang 3 -> 4"},
    {290, kSUp, "Gang 4 -> 5"},
    {325, kSDown, "Gang 5 -> 4"},
    {331, kSDown, "Gang 4 -> 3"},
    {337, kSDown, "Gang 3 -> 2"},
    {343, kSDown, "Gang 2 -> 1"},
    {400, kSA, "Dash 1 bei Gang 1"},
    {470, kSA, "Dash 2 in der Sperrzeit (muss abgelehnt werden)"},
    {500, kSA, "Dash 3 nach der Sperrzeit"},
    {560, kSUp, "Gang 1 -> 2"},
    {565, kSUp, "Gang 2 -> 3"},
    {570, kSUp, "Gang 3 -> 4"},
    {575, kSUp, "Gang 4 -> 5"},
    {650, kSA, "Dash bei gehaltener Bremse (muss abgelehnt werden)"},
    {835, kSX, "X-Schwenk hinter den Vogel"},
    {850, kSY, "Fassrolle (Y-Tipp)"},
    {870, kSDown, "Gang 5 -> 4"},
    {876, kSDown, "Gang 4 -> 3"},
    {882, kSDown, "Gang 3 -> 2"},
    {888, kSDown, "Gang 2 -> 1"},
    {kSteuRide1B, kSB, "B: Landeanflug und Absteigen (Gang 1)"},
};

struct SteuHold {
    uint32_t from;
    uint32_t to;
    uint32_t bits;
    float stickX;
    float cStickX;
    const char* note;
};

constexpr SteuHold kSteuHolds[] = {
    {200, 915, 0u, -1.0f, 0.0f, "Kreisflug links (Wendekreis, Gaenge 1-5)"},
    {640, 700, kSZ, -1.0f, 0.0f, "Bremse Z gehalten (Gang 5)"},
    {750, 770, kSRight, -1.0f, 0.0f, "Steuerkreuz rechts (Kamera naeher)"},
    {780, 800, kSLeft, -1.0f, 0.0f, "Steuerkreuz links (Kamera weiter)"},
    {810, 825, 0u, -1.0f, 1.0f, "C-Stick rechts (Kamera-Orbit)"},
};

struct SteuWin {
    uint32_t from;
    uint32_t to;
    const char* name;
};

constexpr SteuWin kSteuWins[] = {
    {40, 60, "gerade-g3"},      {70, 90, "gerade-g4"},      {100, 120, "gerade-g5"},
    {142, 162, "gerade-g2"},    {175, 195, "gerade-g1"},
    {210, 230, "kreis-g2"},     {240, 260, "kreis-g3"},     {270, 290, "kreis-g4"},
    {300, 320, "kreis-g5"},     {353, 373, "kreis-g1"},
    {670, 690, "bremse-g5"},    {720, 740, "nach-bremse-g5"},
};

// Messung aus der Vogel-Position (unabhaengig vom Modul tempo): Weg je Tick, Gierschritt, Gang-Faktor.
struct SteuMeter {
    bool on = false;
    bool havePrev = false;
    uint32_t n = 0;
    double weg = 0.0;
    double gier = 0.0;
    double eff = 0.0;
    double lastStep = 0.0;  // Weg des letzten Ticks (Stillstand-Erkennung)
    float px = 0.0f;
    float pz = 0.0f;
    int16_t pyaw = 0;
};
SteuMeter g_meter;

void steuMeterUpdate(const Session& s) {
    daKago_c* k = liveBird();
    if (k == nullptr) {
        g_meter.havePrev = false;
        return;
    }
    if (g_meter.havePrev) {
        const double dx = static_cast<double>(k->current.pos.x) - static_cast<double>(g_meter.px);
        const double dz = static_cast<double>(k->current.pos.z) - static_cast<double>(g_meter.pz);
        const int16_t step = static_cast<int16_t>(k->current.angle.y - g_meter.pyaw);
        g_meter.lastStep = std::sqrt(dx * dx + dz * dz);
        if (g_meter.on) {
            g_meter.weg += g_meter.lastStep;
            g_meter.gier += std::fabs(static_cast<double>(step) * 360.0 / 65536.0);
            g_meter.eff += static_cast<double>(s.speedScale);
            ++g_meter.n;
        }
    }
    g_meter.px = k->current.pos.x;
    g_meter.pz = k->current.pos.z;
    g_meter.pyaw = k->current.angle.y;
    g_meter.havePrev = true;
}

void steuMeterBegin() {
    g_meter.on = true;
    g_meter.n = 0;
    g_meter.weg = 0.0;
    g_meter.gier = 0.0;
    g_meter.eff = 0.0;
}

void steuMeterEnd(const char* name, const Session& s) {
    g_meter.on = false;
    if (g_meter.n == 0) {
        logInfo("steuerung: MESS %s ohne Ticks", name);
        return;
    }
    const double n = static_cast<double>(g_meter.n);
    const double weg = g_meter.weg / n;
    const double gier = g_meter.gier / n;
    const double eff = g_meter.eff / n;
    const double omega = gier * 3.14159265358979323846 / 180.0;
    const double radius = omega > 1.0e-6 ? weg / omega : 0.0;
    logInfo("steuerung: MESS %s ticks=%u gang=%d speedScale=%.3f weg=%.3f gierGrad=%.4f radius=%.1f", name,
        static_cast<unsigned>(g_meter.n), s.gear, eff, weg, gier, radius);
}

enum class SteuStage : uint8_t { Wait, Call, Ride1, Ride2, Land, Settle, Calm, Trial, Done };

struct SteuState {
    SteuStage stage = SteuStage::Wait;
    SteuStage afterCall = SteuStage::Ride1;  // Stufe nach einem angenommenen Ruf
    SteuStage afterCalm = SteuStage::Trial;  // Stufe nach der Ruhephase
    uint32_t t0 = 0;         // Beginn der Stufe (Kern-Ticks)
    uint32_t calmStart = 0;  // Beginn der Ruhephase
    uint32_t calm = 0;       // Ruhe-Ticks in Folge
    uint32_t calls = 0;      // Rufversuche der aktuellen Folge
    float callAnalog = 0.0f;     // 0 = digitales R, sonst Analogwert
    bool expectNoRide = false;   // Analog-R 0,50: der Ruf darf nicht angenommen werden
    bool trialDamage = true;     // Teilversuch: true = Schaden, false = Fall
    bool trialFallback = false;  // Schaden: procWolfDamageInit als Ersatz schon benutzt
    uint32_t sub = 0;            // Teilversuch: 0 ausloesen, 1 warten, 2 Chords
    uint32_t lastChord = kSteuNever;
    uint32_t chords = 0;
    int lifeBefore = 0;
    int lifeAfter = 0;
    uint32_t stuck = 0;   // Stillstand-Zaehler (Ritt 1)
    uint32_t escape = 0;  // Ausweichen: verbleibende Ticks mit Steuerung rechts
};
SteuState g_steu;

const char* steuStageName(SteuStage s) {
    switch (s) {
    case SteuStage::Wait: return "warten";
    case SteuStage::Call: return "ruf";
    case SteuStage::Ride1: return "ritt1";
    case SteuStage::Ride2: return "ritt2";
    case SteuStage::Land: return "landung";
    case SteuStage::Settle: return "nachlauf";
    case SteuStage::Calm: return "ruhe";
    case SteuStage::Trial: return "teilversuch";
    case SteuStage::Done: return "ende";
    }
    return "?";
}

int steuLife() {
    return static_cast<int>(dComIfGs_getLife());
}

// Link ruhig: Wolf, auf dem Boden, in keinem Schadens-, Fall- oder Todes-Proc.
bool steuLinkCalm(daAlink_c* link) {
    return link != nullptr && link->checkWolf() != 0 && link->mLinkAcch.ChkGroundHit() &&
           link->mProcID != daAlink_c::PROC_WOLF_DAMAGE && link->mProcID != daAlink_c::PROC_WOLF_FALL &&
           link->mProcID != daAlink_c::PROC_FALL && link->mProcID != daAlink_c::PROC_DEAD;
}

void steuFinish(const char* why) {
    g_steu.stage = SteuStage::Done;
    logInfo("steuerung: SZENARIO ende (%s) fremdeingabe=%u", why, foreignInputCount());
}

void steuEnterCalm(const Context& c, SteuStage next) {
    g_steu.afterCalm = next;
    g_steu.stage = SteuStage::Calm;
    g_steu.calm = 0;
    g_steu.calmStart = c.frame;
}

void steuLogState(const char* label, const Context& c) {
    daAlink_c* link = liveLink();
    daKago_c* k = liveBird();
    if (link == nullptr) {
        logInfo("steuerung: ZUSTAND %s: kein Link (phase=%s)", label, phaseName(c.sess.phase));
        return;
    }
    if (k == nullptr) {
        logInfo("steuerung: ZUSTAND %s: kein Vogel, phase=%s, linkProc=%d, linkBoden=%d, leben=%d", label,
            phaseName(c.sess.phase), static_cast<int>(link->mProcID), link->mLinkAcch.ChkGroundHit() ? 1 : 0,
            steuLife());
        return;
    }
    const float ground = k->checkGroundHeight(k->current.pos, nullptr);
    logInfo("steuerung: ZUSTAND %s f=%u phase=%s vogelY=%.1f boden=%.1f abstand=%.1f linkProc=%d linkBoden=%d "
            "leben=%d",
        label, static_cast<unsigned>(c.sess.rideFrames), phaseName(c.sess.phase), static_cast<double>(k->current.pos.y),
        static_cast<double>(ground), static_cast<double>(k->current.pos.y - ground), static_cast<int>(link->mProcID),
        link->mLinkAcch.ChkGroundHit() ? 1 : 0, steuLife());
}

// Ruf: R (digital oder analog) halten und B tippen. Ohne Ritt nach kSteuCallRetry Ticks neuer Versuch im Leerlauf.
bool steuCall(Context& c) {
    SteuState& st = g_steu;
    const uint32_t t = c.frame;
    if (st.expectNoRide && c.sess.phase != Phase::Idle) {
        logInfo("steuerung: FEHLER Analog-R 0,50: der Ruf wurde angenommen (phase=%s)", phaseName(c.sess.phase));
        steuFinish("Analog-R 0,50 angenommen");
        return true;
    }
    if (c.sess.phase == Phase::Riding) {
        logInfo("steuerung: Ruf angenommen (R %s), Ritt beginnt nach %u Ticks", st.callAnalog > 0.0f ? "analog" : "digital",
            static_cast<unsigned>(t - st.t0));
        st.stage = st.afterCall;
        st.t0 = t;
        return false;
    }
    const uint32_t since = t - st.t0;
    if (since < kSteuCallHold) {
        if (st.callAnalog > 0.0f) {
            c.holdRAnalog(st.callAnalog);
        } else {
            c.holdR();
        }
        if (since == kSteuCallTap) {
            c.tapB();
        }
        return false;
    }
    if (st.expectNoRide) {
        if (since >= kSteuCallHold + kSteuNegWait) {
            logInfo("steuerung: ANALOG-R 0,50: kein Vogel nach %u Ticks, Ruf abgelehnt (B bleibt beim Spiel)",
                static_cast<unsigned>(since));
            steuFinish("Analog-R 0,50 korrekt abgelehnt");
            return true;
        }
        return false;
    }
    if (c.sess.phase == Phase::Idle && since >= kSteuCallRetry) {
        if (st.calls >= kSteuCallTries) {
            steuFinish("Ruf verweigert");
            return true;
        }
        ++st.calls;
        st.t0 = t;
        logInfo("steuerung: Ruf wiederholt (Versuch %u)", st.calls);
    }
    return false;
}

// Ritt 1: Zeitplan nach Ritt-Frames (Gaenge, Kurven, Dash, Bremse, Kamera, Fassrolle, Absteigen).
bool steuRide1(Context& c) {
    SteuState& st = g_steu;
    const uint32_t f = c.sess.rideFrames;
    if (c.sess.phase != Phase::Riding) {
        steuFinish("Ritt 1 endet ungeplant");
        return true;
    }
    steuMeterUpdate(c.sess);
    for (const SteuWin& w : kSteuWins) {
        if (f == w.from) {
            steuMeterBegin();
        }
        if (f == w.to) {
            steuMeterEnd(w.name, c.sess);
        }
    }
    uint32_t bits = 0;
    float sx = 0.0f;
    float cx = 0.0f;
    for (const SteuHold& h : kSteuHolds) {
        if (f == h.from) {
            logInfo("steuerung: BEGINN %s f=%u", h.note, static_cast<unsigned>(f));
        }
        if (f == h.to) {
            logInfo("steuerung: ENDE %s f=%u", h.note, static_cast<unsigned>(f));
        }
        if (f >= h.from && f < h.to) {
            bits |= h.bits;
            sx = h.stickX;
            cx = h.cStickX;
        }
    }
    if (bits != 0) {
        c.hold(bits);
    }
    // Stillstand-Ausweg (nur Test): steht der Vogel ab kSteuStallFrom kSteuStallTicks lang fast still, wird
    // kSteuEscapeTicks lang nach rechts gelenkt (wie ein Spieler). Jede Ausloesung wird geloggt.
    if (f >= kSteuStallFrom && f < kSteuRide1B) {
        st.stuck = g_meter.lastStep < 0.5 ? st.stuck + 1 : 0;
        if (st.stuck >= kSteuStallTicks && st.escape == 0) {
            st.escape = kSteuEscapeTicks;
            st.stuck = 0;
            logInfo("steuerung: STILLSTAND f=%u gang=%d -> Ausweichen (Steuerung rechts, %u Ticks)",
                static_cast<unsigned>(f), c.sess.gear, static_cast<unsigned>(kSteuEscapeTicks));
        }
    }
    if (st.escape > 0) {
        sx = 1.0f;
        --st.escape;
    }
    c.stick(sx, 0.0f);
    c.cStick(cx, 0.0f);
    for (const SteuTap& tap : kSteuRideTaps) {
        if (tap.at == f) {
            c.tap(tap.bits);
            logInfo("steuerung: TASTE %s f=%u gang=%d speedScale=%.3f", tap.note, static_cast<unsigned>(f), c.sess.gear,
                static_cast<double>(c.sess.speedScale));
        }
    }
    if (f == kSteuRide1Pre) {
        steuLogState("vor Abstieg 1", c);
        if (daAlink_c* link = liveLink()) {
            const uint32_t n = landGegnerRaeumen(link->current.pos);
            logInfo("steuerung: Landezone geraeumt (nur Testbuild): %u Gegner im Umkreis %.0f entfernt", n,
                static_cast<double>(kLandGegnerRadius));
        }
    }
    if (f == kSteuRide1B) {
        st.lifeBefore = steuLife();
        st.trialDamage = true;
        st.afterCalm = SteuStage::Trial;
        st.t0 = c.frame;
        st.stage = SteuStage::Land;
        logInfo("steuerung: ABSTIEG 1 (B) bei f=%u, Herzen vorher=%d", static_cast<unsigned>(f), st.lifeBefore);
    }
    return false;
}

// Ritt 2 (Analog-R 0,65): einfache Kurve, danach Absteigen per B; danach Ruf mit Analog-R 0,50 (kein Ruf erwartet).
bool steuRide2(Context& c) {
    SteuState& st = g_steu;
    const uint32_t f = c.sess.rideFrames;
    if (c.sess.phase != Phase::Riding) {
        steuFinish("Ritt 2 endet ungeplant");
        return true;
    }
    if (f >= 20 && f < 80) {
        c.stick(-1.0f, 0.0f);
    }
    if (f == 20) {
        logInfo("steuerung: RITT 2 (Ruf mit Analog-R 0,65) Kurve links f=20");
    }
    if (f == kSteuRide2B) {
        c.tapB();
        st.lifeBefore = steuLife();
        st.callAnalog = 0.50f;
        st.expectNoRide = true;
        st.afterCalm = SteuStage::Call;
        st.calls = 1;
        st.t0 = c.frame;
        st.stage = SteuStage::Land;
        logInfo("steuerung: ABSTIEG 2 (B) bei f=%u, Herzen vorher=%d", static_cast<unsigned>(f), st.lifeBefore);
    }
    return false;
}

// Nach B: warten, bis Vogel weg ist (Dismounting endet, Idle).
bool steuLand(Context& c) {
    SteuState& st = g_steu;
    if (c.sess.phase == Phase::Idle && c.sess.birdId == kNoProc) {
        logInfo("steuerung: Vogel weg nach %u Ticks seit B", static_cast<unsigned>(c.frame - st.t0));
        st.t0 = c.frame;
        st.stage = SteuStage::Settle;
        return false;
    }
    if (c.frame - st.t0 > kSteuRest) {
        steuFinish("Vogel nach dem Absteigen nicht weg");
        return true;
    }
    return false;
}

// Nach dem Absteigen: Link steht, Herzen nachher messen.
bool steuSettle(Context& c) {
    SteuState& st = g_steu;
    if (c.frame - st.t0 < kSteuAfterLand) {
        return false;
    }
    if (c.sess.phase != Phase::Idle) {
        if (c.frame - st.t0 > kSteuAfterLand + kSteuRest) {
            steuFinish("Nachlauf nach dem Absteigen haengt");
            return true;
        }
        return false;
    }
    st.lifeAfter = steuLife();
    logInfo("steuerung: HERZEN nach dem Absteigen vorher=%d nachher=%d differenz=%d", st.lifeBefore, st.lifeAfter,
        st.lifeAfter - st.lifeBefore);
    steuLogState("nach Absteigen", c);
    steuEnterCalm(c, st.afterCalm);
    return false;
}

// Ruhe: Vogel weg, Link auf dem Boden, kein Schaden- oder Fall-Proc, mindestens kSteuCalm Ticks.
bool steuCalm(Context& c) {
    SteuState& st = g_steu;
    daAlink_c* link = liveLink();
    const bool ok = c.sess.phase == Phase::Idle && c.sess.birdId == kNoProc && steuLinkCalm(link);
    st.calm = ok ? st.calm + 1 : 0;
    if (st.calm >= kSteuCalm) {
        st.t0 = c.frame;
        st.stage = st.afterCalm;
        logInfo("steuerung: RUHE (%u Ticks), weiter mit %s", static_cast<unsigned>(st.calm),
            steuStageName(st.afterCalm));
        return false;
    }
    if (c.frame - st.calmStart > kSteuRest) {
        steuFinish("Wolf kommt nicht zur Ruhe");
        return true;
    }
    return false;
}

// Nur Test: Leben auf das Maximum setzen, bevor Ritt 2 beginnt (Eisschaden am Landeort hatte den Link in der
// ersten Testfassung auf 2 Herzen gedrueckt). Die Herzen vor und nach Ritt 2 werden trotzdem gemessen.
void steuHeal() {
    const int before = steuLife();
    dComIfGs_setLife(dComIfGs_getMaxLife());
    logInfo("steuerung: HEILUNG (nur Test) vor Ritt 2: Leben %d -> %d", before, steuLife());
}

// Naechster Teilversuch oder Ende: Schaden -> Fall -> Ruf mit Analog-R 0,65 (Ritt 2).
bool steuTrialNext(const Context& c) {
    SteuState& st = g_steu;
    if (st.trialDamage) {
        st.trialDamage = false;
        steuEnterCalm(c, SteuStage::Trial);
    } else {
        steuHeal();
        st.callAnalog = 0.65f;
        st.expectNoRide = false;
        st.afterCall = SteuStage::Ride2;
        st.calls = 1;
        steuEnterCalm(c, SteuStage::Call);
    }
    st.sub = 0;
    st.trialFallback = false;
    st.lastChord = kSteuNever;
    st.chords = 0;
    return false;
}

// Teilversuch Ruf-Ablehnung (Fix A): Schaden (setDamagePoint, Fallback procWolfDamageInit) oder Fall
// (procWolfFallInit). Sobald der Proc laeuft, R+B alle kSteuChordEvery Ticks. Ein Ruf darf nicht entstehen.
bool steuTrial(Context& c) {
    SteuState& st = g_steu;
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        steuFinish("Teilversuch ohne Link");
        return true;
    }
    const int want = st.trialDamage ? static_cast<int>(daAlink_c::PROC_WOLF_DAMAGE)
                                    : static_cast<int>(daAlink_c::PROC_WOLF_FALL);
    const char* name = st.trialDamage ? "Schaden" : "Fall";
    if (st.sub == 0) {
        if (st.trialDamage) {
            const int life = steuLife();
            if (life < 8) {
                logInfo("steuerung: FIXA Schaden uebersprungen, Leben %d zu niedrig", life);
                return steuTrialNext(c);
            }
            logInfo("steuerung: FIXA Schaden: setDamagePoint(4) = 1 Herz, Leben vorher %d, Proc vorher %d", life,
                static_cast<int>(link->mProcID));
            link->setDamagePoint(4, 0, 1, 0);
            st.sub = 1;
            st.t0 = t;
            return false;
        }
        // Fall: procWolfFallInit und R+B im SELBEN Tick. Der Pad-Hook prueft die Annahme vor dem Link-Execute;
        // beim naechsten Tick ist der Fall-Proc schon in der Landung (Proc 254) und nicht mehr sichtbar.
        logInfo("steuerung: FIXA Fall: procWolfFallInit(2) und R+B im selben Tick, Proc vorher %d",
            static_cast<int>(link->mProcID));
        link->procWolfFallInit(2, 0.0f);
        logInfo("steuerung: FIXA Fall: Proc direkt nach Init %d (erwartet %d)", static_cast<int>(link->mProcID), want);
        c.holdR();
        c.tapB();
        st.lastChord = t;
        ++st.chords;
        st.sub = 2;
        st.t0 = t;
        return false;
    }
    if (st.sub == 1) {
        if (static_cast<int>(link->mProcID) == want) {
            st.sub = 2;
            logInfo("steuerung: FIXA %s-Proc erreicht nach %u Ticks (Proc %d)", name, static_cast<unsigned>(t - st.t0),
                want);
        } else if (st.trialDamage && !st.trialFallback && t - st.t0 >= kSteuProcWait) {
            st.trialFallback = true;
            logInfo("steuerung: FIXA Schaden-Proc nach %u Ticks nicht erreicht (Proc %d), procWolfDamageInit(nullptr)",
                static_cast<unsigned>(t - st.t0), static_cast<int>(link->mProcID));
            link->procWolfDamageInit(nullptr);
            st.t0 = t;
            return false;
        } else if (t - st.t0 >= kSteuProcMax) {
            logInfo("steuerung: FIXA %s-Proc NICHT erreicht (Proc %d), Teilversuch uebersprungen", name,
                static_cast<int>(link->mProcID));
            return steuTrialNext(c);
        } else {
            return false;
        }
    }
    if (st.sub == 2) {
        if (static_cast<int>(link->mProcID) != want) {
            logInfo("steuerung: FIXA %s-Proc beendet nach %u R+B-Versuchen, Vogel=%d, phase=%s, Proc %d", name,
                st.chords, c.sess.birdId != kNoProc ? 1 : 0, phaseName(c.sess.phase),
                static_cast<int>(link->mProcID));
            return steuTrialNext(c);
        }
        if (st.lastChord == kSteuNever || t - st.lastChord >= kSteuChordEvery) {
            c.holdR();
            c.tapB();
            st.lastChord = t;
            ++st.chords;
            logInfo("steuerung: FIXA R+B im %s-Proc (Versuch %u, Proc %d, Vogel=%d)", name, st.chords, want,
                c.sess.birdId != kNoProc ? 1 : 0);
        }
        if (t - st.t0 > kSteuProcRun) {
            logInfo("steuerung: FIXA %s-Chord-Phase Zeitlimit erreicht", name);
            return steuTrialNext(c);
        }
    }
    return false;
}

bool steuTick(Context& c) {
    SteuState& st = g_steu;
    switch (st.stage) {
    case SteuStage::Wait:
        if (c.frame >= kSteuCallAt && c.canCall()) {
            st.stage = SteuStage::Call;
            st.t0 = c.frame;
            st.calls = 1;
            st.callAnalog = 0.0f;
            st.expectNoRide = false;
            st.afterCall = SteuStage::Ride1;
            logInfo("steuerung: Ruf 1 armiert (digitales R halten, B tippen)");
        } else if (c.frame > kSteuGiveUp) {
            steuFinish("kein Ruf moeglich");
            return true;
        }
        return false;
    case SteuStage::Call:
        return steuCall(c);
    case SteuStage::Ride1:
        return steuRide1(c);
    case SteuStage::Ride2:
        return steuRide2(c);
    case SteuStage::Land:
        return steuLand(c);
    case SteuStage::Settle:
        return steuSettle(c);
    case SteuStage::Calm:
        return steuCalm(c);
    case SteuStage::Trial:
        return steuTrial(c);
    case SteuStage::Done:
        return true;
    }
    return false;
}

const Scenario kSteuerung = {
    "steuerung",
    "Ritt-Steuerung: Ruf, Gaenge 1-5 gerade und Kurve, Fassrolle, Dash mit Sperrzeit, Bremse, Kamera, Absteigen, "
    "Analog-R 0,65 und 0,50, Ruf-Ablehnung im Schaden und Fall (Fix A), rund 2400 Ticks",
    steuTick,
};

// VORLAUF e9-landung (nur Testbuild, E9): Landeanflug mit horizontaler Bremse. Vier Szenarien:
//   e9-klippe   Kante mit Bodenabfall ueber 1000 suchen (16 Richtungen, bis 16000, Plateau 4500 vor der Kante eben). Vogel auf Gang 5, 1200
//               ueber dem Plateau, 2500 vor die Kante (Test-Teleport), geradeaus, B. Ohne Bremse landete die Freigabe
//               hinter der Kante, mit Bremse davor.
//   e9-mittel   Vogel 1200 ueber den Boden heben (Test-Teleport), kurzer Nick nach unten (12 Ticks, Stick Y 0,25), dann
//               neutral, B bei Tick 32. Im Anflug 30 Ticks Stick X: Gier bleibt, Nick und Schraege gehen auf 0.
//   e9-b2       wie e9-mittel, zweites B 20 Ticks nach dem ersten: sofortige Freigabe.
//   e9-abgrund  Stelle ohne Boden unter dem Vogel suchen (Raster 400, Umkreis 8000), dorthin setzen, B: sofort los.
//   e9-stick    wie e9-mittel, im Anflug Stick Y 0,8 und zwei Y-Tipps (Kontrolle: Y-Tipp vor dem B startet eine Fassrolle).
//               Erwartet: Nick und Fassrolle bleiben im Anflug ohne Wirkung, die Gier folgt dem Stick X.
// Gemessen im Log: "landing release ... strecke" (ride.cpp), Leben vor und nach dem Ritt, Abstand zur Kante (Klippe).
constexpr float kE9Lift = 1200.0f;        // Hoehe ueber dem Boden fuer die Landetests (Einheiten)
constexpr float kE9EdgeB = 2500.0f;       // Abstand von B bis zur Kante (Klippe)
constexpr float kE9Margin = 4000.0f;      // Vorlauf vor dem B-Punkt, damit der Vogel auf Gang 5 Fahrt hat
constexpr float kE9DropMin = 1000.0f;     // Bodenabfall je 100 Einheiten, ab dem eine Kante zaehlt
constexpr float kE9SearchMax = 16000.0f;  // Suchweite je Richtung
constexpr float kE9Fenster = 4500.0f;     // Plateau-Fenster vor der Kante (B-Punkt und Bremsweg liegen darin)
constexpr float kE9Pi = 3.14159265f;
constexpr uint32_t kE9RideWait = 90;      // Ticks nach dem Ritt-Start, bis der Vogel Hoechsttempo hat
constexpr uint32_t kE9Timeout = 600;      // Ticks ab Positionierung, danach endet das Szenario
constexpr uint32_t kE9Anflug = 90;        // Ticks nach B, danach ist der Anflug ueberfaellig

enum class E9Plan : uint8_t { Klippe, Mittel, B2, Abgrund, Stick };

struct E9Cliff {
    float dx = 0.0f;
    float dz = 0.0f;  // Richtung zur Kante (xz), Vorwaerts des Vogels = (sin, cos)
    float ex = 0.0f;
    float ez = 0.0f;  // letzter Punkt auf dem Plateau vor der Kante
    float upper = 0.0f;
    bool ok = false;
};

struct E9State {
    E9Plan plan = E9Plan::Klippe;
    int stage = 0;  // 0 Ruf, 1 Chord, 2 Ritt abwarten, 3 Anflug, 4 nach B
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t posAt = 0;
    uint32_t bAt = 0;
    float life0 = 0.0f;
    E9Cliff cliff;
};
E9State g_e9;

const char* e9PlanName(E9Plan plan) {
    switch (plan) {
    case E9Plan::Klippe:
        return "klippe";
    case E9Plan::Mittel:
        return "mittel";
    case E9Plan::B2:
        return "b2";
    case E9Plan::Abgrund:
        return "abgrund";
    case E9Plan::Stick:
        return "stick";
    }
    return "?";
}

// Bodenhoehe unter dem Punkt, Sonde ab 500 ueber dem Punkt wie im Kern (groundBelow). false: kein Boden (Abgrund).
bool e9Ground(float x, float y, float z, float* gy) {
    dBgS_GndChk chk;
    cXyz p(x, y + 500.0f, z);
    chk.SetPos(&p);
    *gy = dComIfG_Bgsp().GroundCross(&chk);
    return *gy > -1.0e5f;
}

// Yaw (s16) fuer die Richtung (dx, dz), dieselbe Konvention wie der Vogel: Vorwaerts = (sin, cos).
int16_t e9Yaw(float dx, float dz) {
    return static_cast<int16_t>(std::atan2(dx, dz) * 32768.0f / kE9Pi);
}

// Erste Kante in 16 Richtungen: Bodenabfall ueber kE9DropMin je 100 Einheiten, bis kE9SearchMax. Der Anflugweg (Start bis
// Kante) darf kein Loch, keinen zweiten Abfall und keinen Hang ueber 800 ueber dem Startboden haben; das Plateau der letzten
// kE9Fenster Einheiten vor der Kante muss eben sein (Abweichung hoechstens 400). Sonst wird die Richtung verworfen.
E9Cliff e9FindCliff(float x0, float y0, float z0) {
    E9Cliff best;
    float bestDist = 1.0e9f;
    for (int k = 0; k < 16; ++k) {
        const float th = static_cast<float>(k) * (2.0f * kE9Pi / 16.0f);
        const float dx = std::sin(th);
        const float dz = std::cos(th);
        float gPrev = 0.0f;
        if (!e9Ground(x0, y0, z0, &gPrev)) {
            logInfo("e9: Suche %d: kein Boden am Startpunkt", k);
            continue;
        }
        float dHit = -1.0f;
        float drop = 0.0f;  // Hoehenverlust an der Kante; 1e6 = Abgrund
        for (float s = 100.0f; s <= kE9SearchMax; s += 100.0f) {
            float g = 0.0f;
            if (!e9Ground(x0 + dx * s, y0, z0 + dz * s, &g)) {
                dHit = s - 100.0f;  // Kante in den Abgrund
                drop = 1.0e6f;
                break;
            }
            if (gPrev - g > kE9DropMin) {
                dHit = s - 100.0f;  // letzter Punkt auf dem oberen Niveau
                drop = gPrev - g;
                break;
            }
            gPrev = g;
        }
        if (dHit < 0.0f) {
            logInfo("e9: Suche %d: keine Kante bis %.0f", k, static_cast<double>(kE9SearchMax));
            continue;
        }
        // Startpunkt: kE9EdgeB + kE9Margin vor der Kante (kann hinter dem Suchursprung liegen, fuer den Teleport egal).
        const float dStart = dHit - (kE9EdgeB + kE9Margin);
        float gS0 = 0.0f;
        bool frei = e9Ground(x0 + dx * dStart, y0, z0 + dz * dStart, &gS0);
        // Anflugweg frei: kein Loch, kein zweiter Abfall und kein Hang ueber 800 ueber dem Startboden.
        float hoch = gS0;
        float gLetzt = gS0;
        for (float u = dStart + 100.0f; frei && u <= dHit; u += 100.0f) {
            float g = 0.0f;
            frei = e9Ground(x0 + dx * u, y0, z0 + dz * u, &g);
            if (frei) {
                frei = gLetzt - g <= kE9DropMin;
                if (g > hoch) {
                    hoch = g;
                }
                gLetzt = g;
            }
        }
        frei = frei && hoch - gS0 <= 800.0f;
        // Plateau eben: die letzten kE9Fenster Einheiten vor der Kante (B-Punkt und Bremsweg), Abweichung hoechstens 400.
        const float wStart = dHit - kE9Fenster;
        float gUp = 0.0f;
        bool eben = e9Ground(x0 + dx * wStart, y0, z0 + dz * wStart, &gUp);
        for (float u = wStart + 100.0f; eben && u <= dHit; u += 100.0f) {
            float g = 0.0f;
            eben = e9Ground(x0 + dx * u, y0, z0 + dz * u, &g) && std::fabs(g - gUp) <= 400.0f;
        }
        const bool flat = frei && eben;
        logInfo("e9: Suche %d: Kante bei %.0f, Abfall %.0f (1e6 = Abgrund), Plateau %.1f, Hang %.0f, Weg frei %d, Plateau eben %d",
            k, static_cast<double>(dHit), static_cast<double>(drop), static_cast<double>(gUp),
            static_cast<double>(hoch - gS0), frei ? 1 : 0, eben ? 1 : 0);
        if (!flat || dHit >= bestDist) {
            continue;
        }
        bestDist = dHit;
        best.dx = dx;
        best.dz = dz;
        best.ex = x0 + dx * dHit;
        best.ez = z0 + dz * dHit;
        best.upper = gUp;
        best.ok = true;
    }
    return best;
}

// Stelle ohne Boden unter dem Vogel: Raster 400 bis 8000 um den Vogel, auf der Hoehe des Vogels.
bool e9FindVoid(float x0, float y0, float z0, float* vx, float* vz) {
    for (int ix = -20; ix <= 20; ++ix) {
        for (int iz = -20; iz <= 20; ++iz) {
            const float x = x0 + static_cast<float>(ix) * 400.0f;
            const float z = z0 + static_cast<float>(iz) * 400.0f;
            float g = 0.0f;
            if (!e9Ground(x, y0, z, &g)) {
                *vx = x;
                *vz = z;
                return true;
            }
        }
    }
    return false;
}

// Vogel an die Testposition setzen (nur Testbuild). false: keine Position gefunden, das Szenario endet.
bool e9Setup(Context& c) {
    E9State& g = g_e9;
    daKago_c* k = liveBird();
    if (k == nullptr) {
        c.note("e9: kein Vogel fuer die Position");
        return false;
    }
    g.life0 = static_cast<float>(dComIfGs_getLife());
    if (g.plan == E9Plan::Klippe) {
        g.cliff = e9FindCliff(k->current.pos.x, k->current.pos.y, k->current.pos.z);
        if (!g.cliff.ok) {
            c.note("e9-klippe: keine Kante im Suchbereich (Bodenabfall > 1000, flaches Plateau)");
            return false;
        }
        float gs = 0.0f;
        const float sx = g.cliff.ex - g.cliff.dx * (kE9EdgeB + kE9Margin);
        const float sz = g.cliff.ez - g.cliff.dz * (kE9EdgeB + kE9Margin);
        if (!e9Ground(sx, k->current.pos.y, sz, &gs)) {
            c.note("e9-klippe: Startpunkt ohne Boden");
            return false;
        }
        const cXyz at(sx, gs + kE9Lift, sz);
        k->current.pos = at;
        k->old.pos = at;
        k->current.angle.y = e9Yaw(g.cliff.dx, g.cliff.dz);
        k->shape_angle.y = k->current.angle.y;
        logInfo("e9: Klippe gefunden: Kante bei (%.1f, %.1f), Plateau %.1f, Start (%.1f, %.1f) Hoehe %.1f, Kurs %d",
            static_cast<double>(g.cliff.ex), static_cast<double>(g.cliff.ez), static_cast<double>(g.cliff.upper),
            static_cast<double>(sx), static_cast<double>(sz), static_cast<double>(at.y), k->current.angle.y);
    } else if (g.plan == E9Plan::Abgrund) {
        float vx = 0.0f;
        float vz = 0.0f;
        if (!e9FindVoid(k->current.pos.x, k->current.pos.y, k->current.pos.z, &vx, &vz)) {
            c.note("e9-abgrund: keine Stelle ohne Boden im Umkreis 8000");
            return false;
        }
        k->current.pos.x = vx;
        k->current.pos.z = vz;
        k->old.pos = k->current.pos;
        logInfo("e9: Abgrund gefunden bei (%.1f, %.1f), Hoehe %.1f", static_cast<double>(vx), static_cast<double>(vz),
            static_cast<double>(k->current.pos.y));
    } else {
        float gb = 0.0f;
        if (!e9Ground(k->current.pos.x, k->current.pos.y, k->current.pos.z, &gb)) {
            c.note("e9: kein Boden unter dem Vogel fuer die Hoehe");
            return false;
        }
        k->current.pos.y = gb + kE9Lift;
        k->old.pos.y = k->current.pos.y;
        logInfo("e9: Vogel auf %.1f ueber den Boden (%.1f) gehoben", static_cast<double>(kE9Lift),
            static_cast<double>(gb));
    }
    return true;
}

// B-Tipp des Landetests. Vorher Gegner in der Landezone entfernen (wie landung-hoch), damit der Anflug frei ist.
void e9FireB(Context& c, daKago_c* k) {
    E9State& g = g_e9;
    const uint32_t removed = (k != nullptr && g.plan != E9Plan::Abgrund) ? landGegnerRaeumen(k->current.pos) : 0u;
    const float along = g.plan == E9Plan::Klippe && k != nullptr
                            ? (g.cliff.ex - k->current.pos.x) * g.cliff.dx + (g.cliff.ez - k->current.pos.z) * g.cliff.dz
                            : 0.0f;
    c.tapB();
    g.bAt = c.frame;
    g.stage = 4;
    logInfo("e9: B (%s), Leben %.0f/%.0f, Gegner geraeumt %u, Abstand zur Kante %.1f, gang %d",
        e9PlanName(g.plan), static_cast<double>(dComIfGs_getLife()), static_cast<double>(dComIfGs_getMaxLife()), removed,
        static_cast<double>(along), c.sess.gear);
}

bool e9Tick(Context& c, E9Plan plan) {
    E9State& g = g_e9;
    g.plan = plan;
    const uint32_t t = c.frame;
    switch (g.stage) {
    case 0:
        if (c.canCall() && t >= kPolCallAt) {
            g.t0 = t;
            g.stage = 1;
            c.note("e9: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g.t0 && t < g.t0 + kPolHold) {
            c.holdR();
        }
        if (t == g.t0 + kPolPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g.rideAt = c.sess.rideFrames;
            g.stage = 2;
            logInfo("e9: Ritt gestartet (%s), Leben %.0f/%.0f", e9PlanName(plan),
                static_cast<double>(dComIfGs_getLife()), static_cast<double>(dComIfGs_getMaxLife()));
        } else if (t > g.t0 + kPolHold + 600) {
            c.note("e9: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("e9: Ritt endete vor der Positionierung, Szenario endet");
            return true;
        }
        if (c.sess.rideFrames - g.rideAt >= kE9RideWait) {
            if (!e9Setup(c)) {
                return true;
            }
            g.posAt = t;
            g.stage = 3;
        }
        break;
    case 3: {
        if (c.sess.phase != Phase::Riding) {
            c.note("e9: Ritt endete vor dem B, Szenario endet");
            return true;
        }
        daKago_c* k = liveBird();
        if (k == nullptr) {
            c.note("e9: Vogel fehlt vor dem B, Szenario endet");
            return true;
        }
        const uint32_t dt = t - g.posAt;
        if (plan == E9Plan::Klippe) {
            if (dt == 2 || dt == 6) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));  // Gang 3 auf 5
            }
            const float along = (g.cliff.ex - k->current.pos.x) * g.cliff.dx + (g.cliff.ez - k->current.pos.z) * g.cliff.dz;
            if (dt >= 40 && along <= kE9EdgeB) {
                e9FireB(c, k);
            }
        } else if (plan == E9Plan::Abgrund) {
            if (dt >= 10) {
                e9FireB(c, k);
            }
        } else {
            // Kurzer Nick nach unten (12 Ticks, Stick Y 0,25): der Anflug muss ihn ausgleichen. B bei 1200 Hoehe.
            if (plan == E9Plan::Stick && dt == 4) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));  // Kontrolle: vor dem B startet Y eine Fassrolle
                logInfo("e9: Y getippt vor dem B (Kontrolle), dt 4");
            }
            if (dt >= 15 && dt < 27) {
                c.stick(0.0f, 0.25f);
            }
            if (dt == 32) {
                e9FireB(c, k);
            }
        }
        if (dt > kE9Timeout) {
            c.note("e9: Zeitlimit vor dem B, Szenario endet");
            return true;
        }
        break;
    }
    case 4: {
        const uint32_t dt = t - g.bAt;
        if (plan == E9Plan::Mittel && dt >= 1 && dt <= 30) {
            c.stick(0.6f, 0.0f);  // Gier im Anflug: der Stick X bleibt wirksam
        }
        if (plan == E9Plan::Stick && dt >= 1 && dt <= 30) {
            c.stick(0.6f, 0.8f);  // Stick Y nach unten (0,8) wirkt im Anflug nicht, Stick X schon
        }
        if (plan == E9Plan::Stick && (dt == 5 || dt == 12)) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_Y));  // Fassrolle im Anflug: ohne Wirkung
            logInfo("e9: Y getippt (Fassrolle), dt %u", static_cast<unsigned>(dt));
        }
        if (plan == E9Plan::B2 && dt == 20) {
            c.tapB();
            logInfo("e9: zweites B 20 Ticks nach dem ersten (Anflug)");
        }
        if (c.sess.phase != Phase::Riding) {
            if (plan == E9Plan::Klippe) {
                // Positiv: die Freigabe liegt vor der Kante (auf dem Plateau), negativ: dahinter.
                daAlink_c* link = liveLink();
                if (link != nullptr) {
                    const float vor = (g.cliff.ex - link->current.pos.x) * g.cliff.dx +
                                      (g.cliff.ez - link->current.pos.z) * g.cliff.dz;
                    logInfo("e9: Freigabe %.1f vor der Kante (positiv = davor), Link (%.1f, %.1f) Hoehe %.1f",
                        static_cast<double>(vor), static_cast<double>(link->current.pos.x),
                        static_cast<double>(link->current.pos.z), static_cast<double>(link->current.pos.y));
                }
            }
            logInfo("e9: ende (%s), Ticks nach B %u, Leben vor %.0f nach %.0f", e9PlanName(plan),
                static_cast<unsigned>(dt), static_cast<double>(g.life0), static_cast<double>(dComIfGs_getLife()));
            return true;
        }
        if (dt > kE9Anflug + 300) {
            logInfo("e9: Zeitlimit nach B, Ritt noch aktiv (%s)", e9PlanName(plan));
            return true;
        }
        break;
    }
    default:
        return true;
    }
    return false;
}

bool e9KlippeTick(Context& c) {
    return e9Tick(c, E9Plan::Klippe);
}
bool e9MittelTick(Context& c) {
    return e9Tick(c, E9Plan::Mittel);
}
bool e9B2Tick(Context& c) {
    return e9Tick(c, E9Plan::B2);
}
bool e9AbgrundTick(Context& c) {
    return e9Tick(c, E9Plan::Abgrund);
}
bool e9StickTick(Context& c) {
    return e9Tick(c, E9Plan::Stick);
}

const Scenario kE9Klippe = {
    "e9-klippe",
    "Landeanflug an einer Klippe: Gang 5, B 2500 vor der Kante, Freigabe muss davor liegen (E9)",
    e9KlippeTick,
};
const Scenario kE9Mittel = {
    "e9-mittel",
    "Landeanflug aus 1200 Hoehe: Nick nach unten vor B, Stick X im Anflug (E9)",
    e9MittelTick,
};
const Scenario kE9B2 = {
    "e9-b2",
    "Zweites B 20 Ticks nach dem ersten im Landeanflug: sofortige Freigabe (E9)",
    e9B2Tick,
};
const Scenario kE9Abgrund = {
    "e9-abgrund",
    "B ueber einer Stelle ohne Boden: sofort los (E9)",
    e9AbgrundTick,
};
const Scenario kE9Stick = {
    "e9-stick",
    "Landeanflug mit Stick Y nach unten und Y-Tipps: ohne Wirkung, Gier bleibt beim Stick X (E9)",
    e9StickTick,
};

// VORLAUF e1e5-menue: E5 (ein offenes Menue bedient B, der Kern verbraucht B nicht) und E1 (Speichern im Ritt ueber den
// Menuepfad wird verweigert, der Ritt laeuft weiter). Nur Eingabe und Beobachtung, keine Kernlogik.
struct VorState {
    bool ready = false;     // Ritt steht (Vorlauf abgeschlossen)
    int callStage = 0;      // Ruf-Vorlauf
    int stage = 0;          // Stufe des Szenarios
    uint32_t t0 = 0;        // Beginn der Stufe (Frame)
    uint32_t saveStart = 0; // Beginn der Speichermenue-Stufe (Frame)
    uint32_t rideAt = 0;    // Ritt-Frames zum Vergleich
    int proc = -1;          // letzter Zustand des Speichermenues
    bool saveSeen = false;  // Speichermenue hat den Schreibzustand erreicht
};
VorState g_vor;

// Ortszeit mit Millisekunden, damit Logzeilen zu den Screenshots (Sekunden nach dem Start) passen.
void vorClock(char* buf, size_t n) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &tt);  // MSVC: erst der Zielpuffer, dann die Zeit (umgekehrt zu localtime_r)
#else
    localtime_r(&tt, &tmv);
#endif
    std::snprintf(buf, n, "%02d:%02d:%02d.%03d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, static_cast<int>(ms));
}

int vorWin() {
    return static_cast<int>(dMeter2Info_getWindowStatus());
}

dMenu_save_c* vorSave() {
    dMw_c* mw = dMeter2Info_getMenuWindowClass();
    return mw != nullptr ? mw->mpMenuSave : nullptr;
}

// Cursor im Sammelbildschirm (Speichern: Spalte 0, Zeile 5). -1, solange der Sammelbildschirm fehlt.
int vorCursor(bool wantY) {
    dMw_c* mw = dMeter2Info_getMenuWindowClass();
    if (mw == nullptr || mw->mpMenuCollect == nullptr || mw->mpMenuCollect->mpCollect2D == nullptr) {
        return -1;
    }
    dMenu_Collect2D_c* sammel = mw->mpMenuCollect->mpCollect2D;
    return static_cast<int>(wantY ? sammel->getCursorY() : sammel->getCursorX());
}

// Ruf wie politik-menue (R halten, B tippen). 1 = Ritt steht, 0 = warten, -1 = kein Ritt (Szenario endet).
int vorRide(Context& c) {
    const uint32_t t = c.frame;
    if (g_vor.callStage == 0) {
        if (c.canCall() && t >= kPolCallAt) {
            g_vor.t0 = t;
            g_vor.callStage = 1;
            c.note("vor: Ruf-Kombination (R halten, B tippen)");
        }
        return 0;
    }
    if (t >= g_vor.t0 && t < g_vor.t0 + kPolHold) {
        c.holdR();
    }
    if (t == g_vor.t0 + kPolPressOffset) {
        c.tapB();
    }
    if (c.sess.phase == Phase::Riding) {
        return 1;
    }
    if (t > g_vor.t0 + kPolHold + 600) {
        c.note("vor: Ritt startet nicht, Szenario endet");
        return -1;
    }
    return 0;
}

// e5-ritt: Ritt, START, B-Tipp im Pausenmenue. B muss das Menue schliessen (B kommt beim Spiel an), danach fliegt der Ritt weiter.
bool e5RittTick(Context& c) {
    const uint32_t t = c.frame;
    if (!g_vor.ready) {
        const int r = vorRide(c);
        if (r < 0) {
            return true;
        }
        if (r == 0) {
            return false;
        }
        g_vor.ready = true;
        g_vor.rideAt = c.sess.rideFrames;
        g_vor.t0 = t;
    }
    const uint32_t m = t - g_vor.t0;
    switch (g_vor.stage) {
    case 0:
        if (c.sess.rideFrames - g_vor.rideAt >= 120) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("e5-ritt: START im Ritt, Fenster=%d pause=%u phase=%s", vorWin(),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
            g_vor.stage = 1;
            g_vor.t0 = t;
        }
        break;
    case 1:
        if (m == 60) {
            logInfo("e5-ritt: Menue offen, Fenster=%d pause=%u phase=%s rideFrames=%u", vorWin(),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase), c.sess.rideFrames);
            c.tapB();
            c.note("e5-ritt: B-Tipp bei offenem Menue");
        }
        if (m == 120) {
            logInfo("e5-ritt: nach B, Fenster=%d (0 = B kam beim Spiel an), phase=%s", vorWin(), phaseName(c.sess.phase));
            g_vor.rideAt = c.sess.rideFrames;
            g_vor.stage = 2;
            g_vor.t0 = t;
        }
        break;
    case 2:
        if (m >= 240) {
            logInfo("e5-ritt: Ende, phase=%s Ritt-Frames seit dem Schliessen=%u", phaseName(c.sess.phase),
                c.sess.rideFrames - g_vor.rideAt);
            return true;
        }
        break;
    }
    return false;
}

// e5-aussen: ohne Ritt (Wolf, F_SP121). START, R+B im Pausenmenue (verweigert, B kommt beim Spiel an). Dann D-Pad oben
// (Item-Rad, Fenster 2) und R+B dort.
bool e5AussenTick(Context& c) {
    const uint32_t t = c.frame;
    const uint32_t m = t - g_vor.t0;
    switch (g_vor.stage) {
    case 0:
        if (t >= 150) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("e5-aussen: START ohne Ritt, Fenster=%d", vorWin());
            g_vor.stage = 1;
            g_vor.t0 = t;
        }
        break;
    case 1:
        if (m >= 60 && m < 80) {
            c.holdR();
        }
        if (m == 70) {
            c.tapB();
            c.note("e5-aussen: R+B bei offenem Pausenmenue");
        }
        if (m == 120) {
            logInfo("e5-aussen: nach R+B, Fenster=%d (0 = B kam beim Spiel an), phase=%s", vorWin(), phaseName(c.sess.phase));
            g_vor.stage = 2;
            g_vor.t0 = t;
        }
        break;
    case 2:
        if (m == 30) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
        }
        if (m == 90) {
            logInfo("e5-aussen: nach D-Pad oben, Fenster=%d pause=%u", vorWin(),
                static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m >= 100 && m < 120 && vorWin() == 2) {
            c.holdR();
        }
        if (m == 110 && vorWin() == 2) {
            c.tapB();
            c.note("e5-aussen: R+B bei offenem Item-Rad");
        }
        if (m == 180) {
            logInfo("e5-aussen: nach R+B am Item-Rad, Fenster=%d (0 = B kam beim Spiel an), phase=%s", vorWin(),
                phaseName(c.sess.phase));
            g_vor.stage = 3;
            g_vor.t0 = t;
        }
        break;
    case 3:  // Karte per D-Pad rechts (HUD: MAP). Erst nach dem Oeffnen warten (ein B in der Oeffnungsanimation waere
             // wirkungslos), dann R+B, danach B allein als Kontrolle ohne Ruf-Kombination.
        if (m == 30) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_RIGHT));
        }
        if (m == 200) {
            logInfo("e5-aussen: nach D-Pad rechts (Karte offen), Fenster=%d mapStatus=%u pause=%u", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m >= 210 && m < 230 && vorWin() != 0) {
            c.holdR();  // R allein, ohne B
        }
        if (m == 240) {
            logInfo("e5-aussen: nach R allein (ohne B), Fenster=%d mapStatus=%u pause=%u", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m == 260 && vorWin() != 0) {
            c.tapB();
            c.note("e5-aussen: B nach dem Loslassen von R (ohne Ruf-Kombination)");
        }
        if (m == 300) {
            logInfo("e5-aussen: 40 Frames nach B ohne R, Fenster=%d mapStatus=%u pause=%u", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()));
        }
        if (m >= 310 && m < 330 && vorWin() != 0) {
            c.holdR();
        }
        if (m == 320 && vorWin() != 0) {
            c.tapB();
            c.note("e5-aussen: R+B gleichzeitig (zweiter Versuch, falls Karte offen)");
        }
        if (m == 360) {
            logInfo("e5-aussen: 40 Frames nach dem zweiten R+B-Versuch, Fenster=%d mapStatus=%u pause=%u", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()));
            return true;
        }
        break;
    }
    return false;
}

// e5-ritt-karte: Ritt, START, D-Pad rechts (Karte im Pausenmenue). Sechs B-Tipps bei offenem Menue, abwechselnd B allein
// und R+B. Ohne offenes Menue kein Tipp (sonst Absteigen). Danach fliegt der Ritt weiter (Ritt-Frames waechst).
bool e5RittKarteTick(Context& c) {
    static const uint32_t kAttempt[6] = {220, 300, 380, 460, 540, 620};
    const uint32_t t = c.frame;
    if (!g_vor.ready) {
        const int r = vorRide(c);
        if (r < 0) {
            return true;
        }
        if (r == 0) {
            return false;
        }
        g_vor.ready = true;
        g_vor.rideAt = c.sess.rideFrames;
        g_vor.t0 = t;
    }
    const uint32_t m = t - g_vor.t0;
    switch (g_vor.stage) {
    case 0:
        if (c.sess.rideFrames - g_vor.rideAt >= 120) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("e5-ritt-karte: START im Ritt, Fenster=%d pause=%u phase=%s", vorWin(),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
            g_vor.stage = 1;
            g_vor.t0 = t;
        }
        break;
    case 1:  // Pausenmenue offen. D-Pad rechts bewegt nur den Cursor (Fenster bleibt 3): R-Tipp wechselt die Seite, sonst A
        if (m == 60) {
            c.tap(static_cast<uint32_t>(PAD_TRIGGER_R));
            c.note("e5-ritt-karte: R-Tipp im Pausenmenue (Seitenwechsel)");
        }
        if (m == 120) {
            logInfo("e5-ritt-karte: nach R-Tipp, Fenster=%d mapStatus=%u pause=%u phase=%s", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()),
                phaseName(c.sess.phase));
            if (vorWin() != 4) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_A));
                c.note("e5-ritt-karte: A-Tipp im Pausenmenue (Karte waehlen)");
            }
        }
        if (m == 200) {
            logInfo("e5-ritt-karte: vor den B-Versuchen, Fenster=%d mapStatus=%u pause=%u phase=%s rideFrames=%u", vorWin(),
                static_cast<unsigned>(dMeter2Info_getMapStatus()), static_cast<unsigned>(dComIfGp_isPauseFlag()),
                phaseName(c.sess.phase), c.sess.rideFrames);
            g_vor.stage = 2;
            g_vor.t0 = t;
            g_vor.proc = 0;
        }
        break;
    case 2: {
        const int i = g_vor.proc;
        if (i >= 6) {
            g_vor.stage = 3;
            g_vor.t0 = t;
            break;
        }
        const uint32_t at = kAttempt[i];
        const bool chord = (i % 2) == 1;
        if (chord && m + 10 >= at && m < at + 10 && vorWin() != 0) {
            c.holdR();
        }
        if (m == at) {
            logInfo("e5-ritt-karte: Versuch %d (%s), Fenster=%d mapStatus=%u pause=%u phase=%s", i + 1,
                chord ? "R+B" : "B", vorWin(), static_cast<unsigned>(dMeter2Info_getMapStatus()),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
            if (vorWin() != 0) {
                c.tapB();
                c.note(chord ? "e5-ritt-karte: R+B im offenen Menue" : "e5-ritt-karte: B allein im offenen Menue");
            }
        }
        if (m == at + 40) {
            logInfo("e5-ritt-karte: Versuch %d nach 40 Frames, Fenster=%d mapStatus=%u pause=%u phase=%s", i + 1,
                vorWin(), static_cast<unsigned>(dMeter2Info_getMapStatus()),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase));
            g_vor.proc = i + 1;
        }
        break;
    }
    case 3:
        if (m == 20) {
            g_vor.rideAt = c.sess.rideFrames;
            logInfo("e5-ritt-karte: Kontrolle nach allen Versuchen, Fenster=%d pause=%u phase=%s rideFrames=%u", vorWin(),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase), c.sess.rideFrames);
        }
        if (m == 180) {
            logInfo("e5-ritt-karte: 160 Frames spaeter, Fenster=%d pause=%u phase=%s Ritt-Frames seit der Kontrolle=%u",
                vorWin(), static_cast<unsigned>(dComIfGp_isPauseFlag()), phaseName(c.sess.phase),
                c.sess.rideFrames - g_vor.rideAt);
            return true;
        }
        break;
    }
    return false;
}

// e1-speichern: Ritt, START, Cursor auf Speichern (Spalte 0, Zeile 5), A, Dateiwahl und Ja. Im Ritt verweigert der Kern das
// Schreiben (SKIP an saveMoveDisp): das Menue geht ins Pausenmenue zurueck, der Ritt laeuft weiter.
bool e1SpeichernTick(Context& c) {
    const uint32_t t = c.frame;
    if (!g_vor.ready) {
        const int r = vorRide(c);
        if (r < 0) {
            return true;
        }
        if (r == 0) {
            return false;
        }
        g_vor.ready = true;
        g_vor.rideAt = c.sess.rideFrames;
        g_vor.t0 = t;
    }
    const uint32_t m = t - g_vor.t0;
    switch (g_vor.stage) {
    case 0:
        if (c.sess.rideFrames - g_vor.rideAt >= 120) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("e1-speichern: START im Ritt, Fenster=%d", vorWin());
            g_vor.stage = 1;
            g_vor.t0 = t;
        }
        break;
    case 1: {
        if (m % 20 != 0) {
            break;
        }
        const int x = vorCursor(false);
        const int y = vorCursor(true);
        if (x < 0) {
            break;
        }
        if (x == 0 && y == 5) {
            c.tapA();
            c.note("e1-speichern: A auf Speichern");
            g_vor.stage = 2;
            g_vor.t0 = t;
            g_vor.saveStart = t;
            g_vor.proc = -1;
            break;
        }
        if (m > 1200) {
            logInfo("e1-speichern: Cursor erreicht Speichern nicht (x=%d y=%d)", x, y);
            return true;
        }
        c.tap(static_cast<uint32_t>(x != 0 ? PAD_BUTTON_LEFT : (y < 5 ? PAD_BUTTON_DOWN : PAD_BUTTON_UP)));
        break;
    }
    case 2: {
        dMenu_save_c* sm = vorSave();
        if (sm == nullptr) {
            break;
        }
        const int proc = static_cast<int>(sm->mMenuProc);
        if (proc != g_vor.proc) {
            logInfo("e1-speichern: Speichermenue proc=0x%02X status=%u", proc, static_cast<unsigned>(sm->mSaveStatus));
            g_vor.proc = proc;
            g_vor.t0 = t;  // Eingaben je Zustand neu zaehlen
        }
        const uint32_t k = t - g_vor.t0;
        if (proc == dMenu_save_c::PROC_SAVE_MOVE_DISP) {
            g_vor.saveSeen = true;
        }
        if (proc == dMenu_save_c::PROC_SAVE_SELECT && k % 90 == 30) {
            c.tapA();
        }
        if ((proc == dMenu_save_c::PROC_SAVE_YES_NO_SELECT || proc == dMenu_save_c::PROC_SAVE_QUESTION ||
                proc == dMenu_save_c::PROC_SAVE_QUESTION2) && k % 90 == 30) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_LEFT));
        }
        if ((proc == dMenu_save_c::PROC_SAVE_YES_NO_SELECT || proc == dMenu_save_c::PROC_SAVE_QUESTION ||
                proc == dMenu_save_c::PROC_SAVE_QUESTION2) && k % 90 == 60) {
            c.tapA();
        }
        if (proc == dMenu_save_c::PROC_SAVE_WAIT && g_vor.saveSeen) {
            char clk[24];
            vorClock(clk, sizeof clk);
            logInfo("e1-speichern: Speichermenue beendet, kein Schreiben, Fenster=%d clock=%s", vorWin(), clk);
            g_vor.stage = 3;
            g_vor.t0 = t;
            break;
        }
        if (t - g_vor.saveStart > 1500) {
            logInfo("e1-speichern: Speichermenue erreicht die Verweigerung nicht (proc=0x%02X)", proc);
            return true;
        }
        break;
    }
    case 3:
        if (m == 60) {
            logInfo("e1-speichern: nach Verweigerung, Fenster=%d phase=%s rideFrames=%u", vorWin(), phaseName(c.sess.phase),
                c.sess.rideFrames);
            c.tapB();
            c.note("e1-speichern: B schliesst das Pausenmenue");
            g_vor.rideAt = c.sess.rideFrames;
        }
        if (m == 240) {
            logInfo("e1-speichern: Ende, Fenster=%d phase=%s Ritt-Frames seit Verweigerung=%u", vorWin(),
                phaseName(c.sess.phase), c.sess.rideFrames - g_vor.rideAt);
            return true;
        }
        break;
    }
    return false;
}

const Scenario kVorE5RittKarte = {
    "e5-ritt-karte",
    "VORLAUF e1e5-menue: Ritt, START, Karte per D-Pad rechts, B-Tipps bei offenem Menue (B allein und R+B), danach fliegt der Ritt weiter",
    e5RittKarteTick,
};

const Scenario kVorE5Ritt = {
    "e5-ritt",
    "VORLAUF e1e5-menue: Ritt, START, B-Tipp im Pausenmenue (B schliesst das Menue, der Ritt fliegt weiter)",
    e5RittTick,
};

const Scenario kVorE5Aussen = {
    "e5-aussen",
    "VORLAUF e1e5-menue: ohne Ritt, R+B im Pausenmenue und am Item-Rad (Ruf verweigert, B kommt beim Spiel an)",
    e5AussenTick,
};

const Scenario kVorE1 = {
    "e1-speichern",
    "VORLAUF e1e5-menue: Ritt, START, Speichern ueber das Menue, Ja: Schreiben im Ritt verweigert, Ritt laeuft weiter",
    e1SpeichernTick,
};

}  // namespace

// ---------------------------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------------------------

void Context::holdR() {
    g_in.holdR = true;
}

void Context::tapB() {
    g_in.tapBits |= kBitB;
}

void Context::tapA() {
    g_in.tapBits |= kBitA;
}

void Context::tap(uint32_t buttons) {
    g_in.tapBits |= buttons;
}

void Context::hold(uint32_t buttons) {
    g_in.holdBits |= buttons;
}

void Context::holdRAnalog(float v) {
    g_in.analogR = v;
}

void Context::stick(float x, float y) {
    g_in.stickX = x;
    g_in.stickY = y;
}

void Context::cStick(float x, float y) {
    g_in.cX = x;
    g_in.cY = y;
}

void Context::note(const char* text) {
    logInfo("autotest: %s", text);
}

bool Context::canCall() const {
    daAlink_c* link = liveLink();
    return link != nullptr && link->checkWolf() != 0 && !link->checkCargoCarry() && !link->checkEventRun() &&
           !dComIfGp_event_runCheck() && sess.birdId == kNoProc;
}

void Context::gameLocks() {
    g_in.gameLocks = true;
}

// ---------------------------------------------------------------------------------------------
// VORLAUF e6-decke: Szenario "decke6" (R_SP107 Kanalisation). Ruf, dann Ritt im festen Stickplan (je 150 Ritt-Ticks ein
// Muster: Nase hoch unter die Decke, Kurven links und rechts, Nase runter), bis 2700 Ritt-Ticks (90 s) erreicht sind.
// Ein Ereignis beendet den Ritt sauber, danach wird erneut gerufen. Dann B zum Absteigen. Eingaben nur Stick, Ruf, B
// (und A in Ereignistexten). Die Gier aendert nur der Stick.
// ---------------------------------------------------------------------------------------------

struct Decke6State {
    bool callOn = false;        // Ruf laeuft (R gehalten, B-Tipp in der Mitte)
    bool callPressed = false;
    uint32_t callStart = 0;
    uint32_t rideTicks = 0;     // Ticks in der Phase Riding, ueber alle Ritte
    bool dismounted = false;
    uint32_t dismountFrame = 0;
};
Decke6State g_d6;

// Stickplan: je 150 Ritt-Ticks ein Muster, acht Muster im Kreis.
void decke6Stick(uint32_t f, float& x, float& y) {
    static constexpr float kPat[8][2] = {
        {0.0f, -0.6f}, {0.8f, -0.6f}, {-0.8f, 0.0f}, {-0.8f, -0.6f},
        {0.0f, 0.6f},  {0.8f, 0.0f},  {0.6f, -0.6f}, {-0.6f, 0.3f},
    };
    const uint32_t p = (f / 150u) % 8u;
    x = kPat[p][0];
    y = kPat[p][1];
}

bool decke6Tick(Context& c) {
    const uint32_t t = c.frame;
    Decke6State& s = g_d6;
    if (t >= 20000) {
        c.note("decke6: Zeitlimit erreicht (kein Ende gemeldet)");
        return true;
    }
    if (dComIfGp_event_runCheck() != 0) {
        if (t % 20 == 0) {
            c.tapA();
        }
        return false;
    }
    if (s.dismounted) {
        if (t >= s.dismountFrame + 400) {
            c.note("autotest: timeline complete");
            return true;
        }
        return false;
    }
    if (c.sess.phase == kff::Phase::Riding) {
        s.callOn = false;
        ++s.rideTicks;
        if (s.rideTicks >= 2700) {
            s.dismounted = true;
            s.dismountFrame = t;
            c.tapB();
            c.note("decke6: B (Absteigen) nach 90 s Ritt");
            return false;
        }
        float x = 0.0f;
        float y = 0.0f;
        decke6Stick(c.sess.rideFrames, x, y);
        c.stick(x, y);
        return false;
    }
    // Kein Ritt: Ruf, auch erneut nach einem Ereignis, das den Ritt beendet hat.
    if (!s.callOn && t >= kZyCallAt && c.canCall()) {
        s.callOn = true;
        s.callPressed = false;
        s.callStart = t;
        c.note("decke6: Ruf (R halten, B tippen)");
    }
    if (s.callOn) {
        if (t >= s.callStart && t < s.callStart + kZyHold) {
            c.holdR();
        }
        if (!s.callPressed && t == s.callStart + kZyPressOffset) {
            s.callPressed = true;
            c.tapB();
        }
        if (t >= s.callStart + kZyHold + 120) {  // kein Ritt zustande gekommen: spaeter erneut rufen
            s.callOn = false;
        }
    }
    return false;
}

const Scenario kDecke6 = {
    "decke6",
    "VORLAUF e6-decke: R_SP107, 90 s Ritt unter Decke und Ueberhang (Stickplan), dann B",
    decke6Tick,
};

// ---------------------------------------------------------------------------------------------
// WP2 (Tod, Ritt-Ende): Szenarien "wp2-tod-abstieg" und "wp2-game-over" (nur Testbuild). Sie zaehlen die eigenen Voegel
// im Spiel direkt (ueber den Akteur-Iterator, nicht ueber die Sitzung). So wird ein Zombie-Vogel sichtbar, den die
// Sitzung schon vergessen hat. Ergebnis: Logzeile "wp2 RESULT <name> ok|fail <grund>".
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kWpCallAt = 300;         // erster Ruf, sobald Link bereit ist
constexpr uint32_t kWpHold = 30;            // R gehalten, Ticks
constexpr uint32_t kWpPressOffset = 15;     // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kWpRideFrames = 200;     // Ritt-Frames vor dem Absteigen
constexpr uint32_t kWpDepartWait = 30;      // Ticks nach Beginn des Abflugs bis zum Tod
constexpr uint32_t kWpDismountLimit = 900;  // Obergrenze, bis der Abflug (Phase Dismounting) beginnt
constexpr uint32_t kWpWatch = 600;          // Beobachtung nach dem Tod (Ticks)
constexpr uint32_t kWpGoWatch = 900;        // Beobachtung im Game-Over (Ticks)
constexpr uint32_t kWpGoneTicks = 150;      // WP2.1: Vogel spaetestens so viele Ticks nach Leben 0 weg (Plan)

// Eigene Voegel im Spiel (mType 1, Kargarok), unabhaengig von der Sitzung.
int wpCountBirdCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e && static_cast<daKago_c*>(ac)->mType == kFriendlyKagoType) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

uint32_t wpCountBirds() {
    uint32_t n = 0;
    fopAcIt_Executor(wpCountBirdCb, &n);
    return n;
}

struct WpAbState {
    int stage = 0;           // 0 Ruf, 1 Chord, 2 Ritt, 3 Abflug abwarten, 4 Tod im Abflug, 5 Beobachtung
    uint32_t t0 = 0;         // Tick des Rufs
    uint32_t rideAt = 0;     // Ritt-Frame bei Ritt-Start
    uint32_t tapAt = 0;      // Tick des B-Tipps zum Absteigen
    uint32_t departAt = 0;   // Tick, in dem die Phase Dismounting beginnt
    uint32_t hitAt = 0;      // Tick des Todes
    uint32_t birdTicks = 0;  // Ticks nach dem Tod mit mindestens einem eigenen Vogel im Spiel (Uebergang, Information)
    uint32_t birdMax = 0;    // hoechste Vogelzahl nach dem Tod
    uint32_t lastBirdSince = 0;  // Ticks nach dem Tod, als zuletzt ein eigener Vogel da war
    uint32_t birdLate = 0;       // Ticks mit Vogel mehr als kWpGoneTicks nach dem Tod (WP2.1 verletzt)
};
WpAbState g_wpAb;

bool wp2TodAbstiegTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_wpAb.stage) {
    case 0:
        if (c.canCall() && t >= kWpCallAt) {
            g_wpAb.t0 = t;
            g_wpAb.stage = 1;
            c.note("wp2-tod-abstieg: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_wpAb.t0 && t < g_wpAb.t0 + kWpHold) {
            c.holdR();
        }
        if (t == g_wpAb.t0 + kWpPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_wpAb.rideAt = c.sess.rideFrames;
            g_wpAb.stage = 2;
        } else if (t > g_wpAb.t0 + kWpHold + 600) {
            logInfo("wp2 RESULT wp2-tod-abstieg fail Ritt startet nicht");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            logInfo("wp2 RESULT wp2-tod-abstieg fail Ritt endete vor dem Absteigen");
            return true;
        }
        if (c.sess.rideFrames - g_wpAb.rideAt >= kWpRideFrames) {
            c.tapB();
            g_wpAb.tapAt = t;
            g_wpAb.stage = 3;
            logInfo("wp2-tod-abstieg: B-Tipp zum Absteigen bei Ritt-Frame %u, Voegel im Spiel %u", c.sess.rideFrames,
                wpCountBirds());
        }
        break;
    case 3:
        if (c.sess.phase == Phase::Dismounting) {
            g_wpAb.departAt = t;
            g_wpAb.stage = 4;
            logInfo("wp2-tod-abstieg: Abflug (Phase Dismounting) nach %u Ticks", t - g_wpAb.tapAt);
        } else if (t - g_wpAb.tapAt > kWpDismountLimit) {
            logInfo("wp2 RESULT wp2-tod-abstieg fail kein Abflug nach B, phase=%s", phaseName(c.sess.phase));
            return true;
        }
        break;
    case 4:
        if (t - g_wpAb.departAt >= kWpDepartWait) {
            daAlink_c* link = liveLink();
            if (link == nullptr) {
                logInfo("wp2 RESULT wp2-tod-abstieg fail kein Link im Abflug");
                return true;
            }
            dComIfGs_setLife(1);
            const int r = link->setDamagePoint(2, 0, 1, 0);  // Quelle: Test-API, Tod im Abflug
            logInfo("wp2-tod-abstieg: Tod im Abflug (Test-Schaden 2 bei Leben 1), Rueckgabe %d, phase=%s, Voegel %u",
                r, phaseName(c.sess.phase), wpCountBirds());
            g_wpAb.hitAt = t;
            g_wpAb.stage = 5;
        }
        break;
    case 5: {
        const uint32_t n = wpCountBirds();
        const uint32_t since = t - g_wpAb.hitAt;
        if (n > 0) {
            ++g_wpAb.birdTicks;  // Ticks mit Vogel nach dem Tod (Uebergang, nur Information)
            g_wpAb.lastBirdSince = since;
            if (since > kWpGoneTicks) {
                ++g_wpAb.birdLate;  // Vogel nach der Frist von WP2.1
            }
            if (n > g_wpAb.birdMax) {
                g_wpAb.birdMax = n;
            }
        }
        if (since % 30 == 0) {
            logInfo("wp2-tod-abstieg: %u Ticks nach Tod, Voegel im Spiel %u, Sitzung phase=%s, leben=%u, cargo=%d midnaKago=%d",
                since, n, phaseName(c.sess.phase), static_cast<unsigned>(dComIfGs_getLife()),
                liveLink() != nullptr && liveLink()->checkCargoCarry() ? 1 : 0,
                daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr ? 1 : 0);
        }
        if (since >= kWpWatch) {
            // WP2.1 (Plan): birds=0 hoechstens kWpGoneTicks Ticks nach Leben 0, danach kein Vogel mehr.
            if (n == 0 && g_wpAb.birdLate == 0) {
                logInfo("wp2 RESULT wp2-tod-abstieg ok Vogel nach dem Tod spaetestens nach %u Ticks weg (Grenze %u), danach keiner, Uebergangs-Ticks %u",
                    g_wpAb.lastBirdSince, kWpGoneTicks, g_wpAb.birdTicks);
            } else {
                logInfo("wp2 RESULT wp2-tod-abstieg fail Vogel bis Tick %u nach dem Tod (Grenze %u), spaet %u Ticks, am Ende %u Voegel",
                    g_wpAb.lastBirdSince, kWpGoneTicks, g_wpAb.birdLate, n);
            }
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kWp2TodAbstieg = {
    "wp2-tod-abstieg",
    "WP2: Ruf, Ritt, B zum Absteigen, Tod im Abflug (Phase Dismounting), Zaehlung der Voegel im Spiel",
    wp2TodAbstiegTick,
};

struct WpGoState {
    int stage = 0;           // 0 Ruf-Bereitschaft, 1 Tod am Boden und Game-Over-Beobachtung
    uint32_t hitAt = 0;      // Tick des Todes
    uint32_t birdTicks = 0;  // Ticks im Game-Over (Leben 0) mit mindestens einem eigenen Vogel im Spiel
    uint32_t birdMax = 0;
    uint32_t goTicks = 0;    // Ticks im Game-Over (Leben 0), in denen beobachtet wurde
    uint32_t rejects = 0;    // Ruf-Versuche (R+B) im Game-Over
};
WpGoState g_wpGo;

bool wp2GameOverTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_wpGo.stage) {
    case 0:
        if (c.canCall() && t >= kWpCallAt) {
            daAlink_c* link = liveLink();
            if (link == nullptr) {
                break;
            }
            dComIfGs_setLife(1);
            const int r = link->setDamagePoint(2, 0, 1, 0);  // Tod am Boden, ohne Ritt
            logInfo("wp2-game-over: Tod am Boden (Test-Schaden 2 bei Leben 1), Rueckgabe %d, proc=%u", r,
                static_cast<unsigned>(link->mProcID));
            g_wpGo.hitAt = t;
            g_wpGo.stage = 1;
        }
        break;
    case 1: {
        const uint32_t d = t - g_wpGo.hitAt;
        const uint32_t n = wpCountBirds();
        // Game-Over ist Leben 0, bis "weiter" das Leben wiederherstellt. Nur dieser Zeitraum zaehlt. Ein Ruf nach dem
        // Weiterspielen ist ein legitimer Ritt und kein Befund.
        if (dComIfGs_getLife() == 0) {
            ++g_wpGo.goTicks;
            if (n > 0) {
                ++g_wpGo.birdTicks;
                if (n > g_wpGo.birdMax) {
                    g_wpGo.birdMax = n;
                }
            }
        }
        // Ruf-Versuche im Game-Over: R gehalten, B-Tipp in der Mitte, alle 120 Ticks ab Tick 30.
        if (d >= 30 && (d - 30) % 120 < kWpHold) {
            c.holdR();
        }
        if (d >= 30 && (d - 30) % 120 == kWpPressOffset) {
            c.tapB();
            ++g_wpGo.rejects;
        }
        // Game-Over-Bildschirm: A bestaetigt "weiter", damit der Link wieder lebt.
        if (d >= 60 && d % 90 == 0) {
            c.tapA();
        }
        if (d % 60 == 0) {
            logInfo("wp2-game-over: %u Ticks nach Tod, Voegel im Spiel %u, phase=%s, leben=%u, Ruf-Versuche %u, cargo=%d midnaKago=%d",
                d, n, phaseName(c.sess.phase), static_cast<unsigned>(dComIfGs_getLife()), g_wpGo.rejects,
                liveLink() != nullptr && liveLink()->checkCargoCarry() ? 1 : 0,
                daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr ? 1 : 0);
        }
        if (d >= kWpGoWatch) {
            if (g_wpGo.goTicks > 0 && g_wpGo.birdTicks == 0) {
                logInfo("wp2 RESULT wp2-game-over ok kein Vogel in %u Game-Over-Ticks (Leben 0), Ruf-Versuche %u",
                    g_wpGo.goTicks, g_wpGo.rejects);
            } else {
                logInfo("wp2 RESULT wp2-game-over fail Vogel %u Ticks im Game-Over (Game-Over-Ticks %u, max %u)",
                    g_wpGo.birdTicks, g_wpGo.goTicks, g_wpGo.birdMax);
            }
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kWp2GameOver = {
    "wp2-game-over",
    "WP2: Tod am Boden ohne Ritt, Ruf-Versuche (R+B) im Game-Over, Zaehlung der Voegel im Spiel",
    wp2GameOverTick,
};

// WP2.13: B-Tipp genau bei Ritt-Frame 701, kein Tod. Erwartung: Absteigen (ggf. Landeanflug), der Vogel fliegt weg und wird
// entfernt, Link lebt, kein Cargo und kein Midna-Zeiger bleiben. Ergebnis: Logzeile "wp2 RESULT wp2-b701 ok|fail <grund>".
constexpr uint32_t kWpB701Frame = 701;  // Ritt-Frame des B-Tipps (WP2.13)
constexpr uint32_t kWpB701Watch = 900;  // Beobachtung nach dem B-Tipp (Ticks): Landeanflug bis 240, Abflug 100, Reserve
// F5 (nur Testcode): KFF_TEST_B_FRAME=<n> setzt den Ritt-Frame des B-Tipps (gleicher Ort, anderer Zeitpunkt).
uint32_t wpB701Frame() {
    static uint32_t frame = 0;
    if (frame == 0) {
        const char* v = std::getenv("KFF_TEST_B_FRAME");
        const unsigned long n = v != nullptr ? std::strtoul(v, nullptr, 10) : 0UL;
        frame = (n >= 20UL && n < 100000UL) ? static_cast<uint32_t>(n) : kWpB701Frame;
    }
    return frame;
}

struct WpB701State {
    int stage = 0;          // 0 Ruf, 1 Chord, 2 Ritt bis Frame 701, 3 Abstieg und Entfernen beobachten
    uint32_t t0 = 0;        // Tick des Rufs
    uint32_t tapAt = 0;     // Tick des B-Tipps
    bool departed = false;  // Phase Dismounting (Abflug) gesehen
    uint32_t birdTicks = 0; // Ticks nach dem B-Tipp mit einem eigenen Vogel im Spiel (Information)
    bool b2Done = false;    // F7 (nur Testcode): zweites B nach KFF_TEST_B2_AFTER schon gedrueckt
};
WpB701State g_wpB701;

bool wp2B701Tick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_wpB701.stage) {
    case 0:
        if (c.canCall() && t >= kWpCallAt) {
            g_wpB701.t0 = t;
            g_wpB701.stage = 1;
            c.note("wp2-b701: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_wpB701.t0 && t < g_wpB701.t0 + kWpHold) {
            c.holdR();
        }
        if (t == g_wpB701.t0 + kWpPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_wpB701.stage = 2;
        } else if (t > g_wpB701.t0 + kWpHold + 600) {
            logInfo("wp2 RESULT wp2-b701 fail Ritt startet nicht");
            return true;
        }
        break;
    case 2:
        if (c.sess.phase != Phase::Riding) {
            logInfo("wp2 RESULT wp2-b701 fail Ritt endete vor Ritt-Frame %u", wpB701Frame());
            return true;
        }
        if (c.sess.rideFrames >= wpB701Frame()) {
            c.tapB();
            g_wpB701.tapAt = t;
            g_wpB701.stage = 3;
            logInfo("wp2-b701: B-Tipp zum Absteigen bei Ritt-Frame %u, Voegel im Spiel %u", c.sess.rideFrames,
                wpCountBirds());
        }
        break;
    case 3: {
        const uint32_t since = t - g_wpB701.tapAt;
        const uint32_t n = wpCountBirds();
        if (c.sess.phase == Phase::Dismounting) {
            g_wpB701.departed = true;
        }
        if (n > 0) {
            ++g_wpB701.birdTicks;
        }
        if (since % 60 == 0) {
            logInfo("wp2-b701: %u Ticks nach B-Tipp, Voegel im Spiel %u, phase=%s, leben=%u, cargo=%d midnaKago=%d", since, n,
                phaseName(c.sess.phase), static_cast<unsigned>(dComIfGs_getLife()),
                liveLink() != nullptr && liveLink()->checkCargoCarry() ? 1 : 0,
                daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr ? 1 : 0);
        }
        // F7 (nur Testcode): KFF_TEST_B2_AFTER=<n> drueckt B ein zweites Mal n Ticks nach dem ersten, also die Freigabe im
        // Landeanflug (Vorlage landung-hoch, landHochB2After). Ohne die Variable bleibt das Szenario unveraendert.
        const uint32_t b2 = landHochB2After();
        if (b2 > 0 && !g_wpB701.b2Done && since >= b2) {
            g_wpB701.b2Done = true;
            c.tapB();
            logInfo("wp2-b701: zweites B (Test) nach %u Ticks, phase=%s, Ritt-Frame %u, Leben %u", since, phaseName(c.sess.phase),
                c.sess.rideFrames, static_cast<unsigned>(dComIfGs_getLife()));
        }
        if (since >= kWpB701Watch) {
            const bool cargo = liveLink() != nullptr && liveLink()->checkCargoCarry();
            const bool midna = daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr;
            if (g_wpB701.departed && n == 0 && !cargo && !midna && dComIfGs_getLife() > 0) {
                logInfo("wp2 RESULT wp2-b701 ok B bei Ritt-Frame %u, Abflug, Vogel weg (Ticks mit Vogel %u), Link lebt, kein Cargo, kein Midna-Zeiger",
                    c.sess.rideFrames, g_wpB701.birdTicks);
            } else {
                logInfo("wp2 RESULT wp2-b701 fail abgeflogen=%d voegel=%u cargo=%d midnaKago=%d leben=%u",
                    g_wpB701.departed ? 1 : 0, n, cargo ? 1 : 0, midna ? 1 : 0, static_cast<unsigned>(dComIfGs_getLife()));
            }
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kWp2B701 = {
    "wp2-b701",
    "WP2.13: B-Tipp bei Ritt-Frame 701, Absteigen, Entfernen des Vogels, kein Cargo und kein Midna-Zeiger",
    wp2B701Tick,
};

// F6 W1 (nur Testcode): B ueber Wasser mit Grund darunter (Hylia-See). Ablauf: Ruf, Ritt, Vogel ueber den naechsten Punkt
// mit Wasserflaeche und Seegrund setzen (Gitter um Link), 40 Ticks warten, B-Tipp, dann beobachten. Ergebnis: Logzeile
// "wp2 RESULT hylia-wasser ok|fail <grund>". Gemessen: Ticks bis zum Wasserkontakt, Schwimm-Ticks, Leben, groesster Schritt
// von Link je Tick (Neustart = Sprung), Ticks von der Freigabe bis zum Entfernen des Vogels.
struct F6LakeSpot {
    bool found = false;
    float x = 0.0f;
    float z = 0.0f;
    float waterY = 0.0f;
    float groundY = 0.0f;
};

F6LakeSpot f6FindLake(const cXyz& from) {
    F6LakeSpot best;
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
            dBgS_ObjGndChk_Spl wsp;
            cXyz wstart(x, from.y + 20000.0f, z);
            wsp.SetPos(&wstart);
            const float wy = dComIfG_Bgsp().GroundCross(&wsp);
            if (!(wy > -1.0e5f)) {
                continue;
            }
            dBgS_GndChk gnd;
            cXyz gstart(x, wy - 1.0f, z);
            gnd.SetPos(&gstart);
            const float gy = dComIfG_Bgsp().GroundCross(&gnd);
            if (!(gy > -1.0e5f) || wy - gy < 100.0f) {
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

// F6 (nur Testcode): naechster Punkt mit Boden und ohne Wasser darueber (Link soll nicht im See stehen).
bool f6FindDry(const cXyz& from, cXyz* out) {
    int bestD = 1 << 30;
    bool found = false;
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
            dBgS_GndChk gnd;
            cXyz gstart(x, from.y + 20000.0f, z);
            gnd.SetPos(&gstart);
            const float gy = dComIfG_Bgsp().GroundCross(&gnd);
            if (!(gy > -1.0e5f)) {
                continue;
            }
            dBgS_ObjGndChk_Spl wsp;
            cXyz wstart(x, from.y + 20000.0f, z);
            wsp.SetPos(&wstart);
            const float wy = dComIfG_Bgsp().GroundCross(&wsp);
            if (wy > -1.0e5f && wy > gy + 10.0f) {
                continue;
            }
            out->x = x;
            out->y = gy + 10.0f;
            out->z = z;
            found = true;
            bestD = d;
        }
    }
    return found;
}

struct F6HyliaState {
    int stage = 0;          // 0 Ruf, 1 Ritt, 2 See suchen, 3 Vogel dort warten, 4 B-Tipp und Beobachtung
    uint32_t t0 = 0;        // Tick des Rufs
    uint32_t teleportAt = 0;
    uint32_t tapAt = 0;     // Tick des B-Tipps
    uint32_t departAt = 0;  // erster Tick in Phase Dismounting
    uint32_t wetAt = 0;     // erster Tick mit Wasserkontakt von Link
    uint32_t goneAt = 0;    // erster Tick ohne Vogel nach der Freigabe
    uint32_t swimTicks = 0; // Ticks mit Wasserkontakt
    uint32_t birdTicks = 0; // Ticks mit einem Vogel im Spiel
    uint32_t life0 = 0;     // Leben beim B-Tipp
    uint32_t rideAtTap = 0; // Ritt-Frame beim B-Tipp
    float maxStep = 0.0f;   // groesster Schritt von Link je Tick nach dem B-Tipp
    cXyz lastPos;
    bool havePos = false;
    F6LakeSpot spot;
};
F6HyliaState g_f6Hylia;

bool f6HyliaTick(Context& c) {
    F6HyliaState& st = g_f6Hylia;
    const uint32_t t = c.frame;
    switch (st.stage) {
    case 0: {
        daAlink_c* link = liveLink();
        if (link != nullptr && t == 30 && (link->checkModeFlg(0x40000) != 0 || link->mLinkAcch.ChkWaterHit())) {
            cXyz dry(0.0f, 0.0f, 0.0f);
            if (f6FindDry(link->current.pos, &dry)) {
                link->current.pos = dry;
                link->old.pos = dry;
                logInfo("hylia-wasser: Link aus dem Wasser auf trockenen Boden (%.1f, %.1f, %.1f) gesetzt", static_cast<double>(dry.x),
                    static_cast<double>(dry.y), static_cast<double>(dry.z));
            } else {
                logInfo("wp2 RESULT hylia-wasser fail kein trockener Boden im Umkreis von 30000 Einheiten");
                return true;
            }
        }
        if (link != nullptr && t % 150 == 0) {
            logInfo("hylia-wasser: Warten auf Ruf, proc=%u swim=%d ground=%d water=%d eventLink=%d eventRun=%d wolf=%d cargo=%d bird=%u",
                static_cast<unsigned>(link->mProcID), link->checkModeFlg(0x40000) != 0 ? 1 : 0,
                link->mLinkAcch.ChkGroundHit() ? 1 : 0, link->mLinkAcch.ChkWaterHit() ? 1 : 0,
                link->checkEventRun() != 0 ? 1 : 0, dComIfGp_event_runCheck() != 0 ? 1 : 0, link->checkWolf() != 0 ? 1 : 0,
                link->checkCargoCarry() != 0 ? 1 : 0, static_cast<unsigned>(c.sess.birdId));
        }
        if (canCallHumanOrWolf(c) && t >= kWpCallAt) {  // F6: Mensch-Ruf, die Verwandlung uebernimmt autowolf
            st.t0 = t;
            st.stage = 1;
            c.note("hylia-wasser: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    }
    case 1:
        if (t >= st.t0 && t < st.t0 + kWpHold) {
            c.holdR();
        }
        if (t == st.t0 + kWpPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            st.stage = 2;
        } else if (t > st.t0 + kWpHold + 600) {
            logInfo("wp2 RESULT hylia-wasser fail Ritt startet nicht");
            return true;
        }
        break;
    case 2: {
        if (c.sess.phase != Phase::Riding) {
            logInfo("wp2 RESULT hylia-wasser fail Ritt endete vor dem Seeflug");
            return true;
        }
        daAlink_c* link = liveLink();
        if (c.sess.rideFrames < 5 || link == nullptr) {
            break;
        }
        st.spot = f6FindLake(link->current.pos);
        if (!st.spot.found) {
            logInfo("wp2 RESULT hylia-wasser fail kein Wasser mit Grund im Umkreis von 30000 Einheiten um Link");
            return true;
        }
        fopAc_ac_c* actor = nullptr;
        if (fopAcM_SearchByID(c.sess.birdId, &actor) == 0 || actor == nullptr) {
            logInfo("wp2 RESULT hylia-wasser fail kein Vogel beim Seeflug");
            return true;
        }
        daKago_c* k = static_cast<daKago_c*>(actor);
        const cXyz target(st.spot.x, st.spot.waterY + 400.0f, st.spot.z);
        k->current.pos = target;
        k->old.pos = target;
        logInfo("hylia-wasser: Vogel auf (%.1f, %.1f, %.1f) gesetzt, Wasser y %.1f, Grund y %.1f, Link (%.1f, %.1f, %.1f)",
            static_cast<double>(target.x), static_cast<double>(target.y), static_cast<double>(target.z),
            static_cast<double>(st.spot.waterY), static_cast<double>(st.spot.groundY),
            static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
            static_cast<double>(link->current.pos.z));
        st.teleportAt = t;
        st.stage = 3;
        break;
    }
    case 3:
        if (c.sess.phase != Phase::Riding) {
            logInfo("wp2 RESULT hylia-wasser fail Ritt endete vor dem B-Tipp");
            return true;
        }
        if (t >= st.teleportAt + 40) {
            if (daAlink_c* link = liveLink()) {
                st.life0 = static_cast<uint32_t>(dComIfGs_getLife());
                st.rideAtTap = c.sess.rideFrames;
                st.tapAt = t;
                c.tapB();
                st.stage = 4;
                logInfo("hylia-wasser: B-Tipp bei Ritt-Frame %u, Leben %u, Link y %.1f, Wasserkontakt %d",
                    st.rideAtTap, st.life0, static_cast<double>(link->current.pos.y), link->mLinkAcch.ChkWaterHit() ? 1 : 0);
            }
        }
        break;
    case 4: {
        const uint32_t since = t - st.tapAt;
        daAlink_c* link = liveLink();
        if (c.sess.phase == Phase::Dismounting && st.departAt == 0) {
            st.departAt = t;
        }
        if (link != nullptr) {
            if (st.havePos) {
                const float dx = link->current.pos.x - st.lastPos.x;
                const float dy = link->current.pos.y - st.lastPos.y;
                const float dz = link->current.pos.z - st.lastPos.z;
                const float step = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (step > st.maxStep) {
                    st.maxStep = step;
                }
            }
            st.lastPos = link->current.pos;
            st.havePos = true;
            if (link->mLinkAcch.ChkWaterHit()) {
                if (st.wetAt == 0) {
                    st.wetAt = t;
                }
                ++st.swimTicks;
            }
        }
        const uint32_t n = wpCountBirds();
        if (n > 0) {
            ++st.birdTicks;
        }
        if (st.departAt != 0 && n == 0 && st.goneAt == 0) {
            st.goneAt = t;
        }
        if (since % 30 == 0) {
            logInfo("hylia-wasser: %u Ticks nach B, Voegel %u, phase=%s, leben=%u, link y %.1f, wet %d, groesster Schritt %.1f",
                since, n, phaseName(c.sess.phase), static_cast<unsigned>(dComIfGs_getLife()),
                link != nullptr ? static_cast<double>(link->current.pos.y) : 0.0,
                link != nullptr && link->mLinkAcch.ChkWaterHit() ? 1 : 0, static_cast<double>(st.maxStep));
        }
        const bool done = (st.goneAt != 0 && t >= st.goneAt + 60) || since >= 900;
        if (done) {
            const uint32_t life = static_cast<uint32_t>(dComIfGs_getLife());
            const bool ok = st.wetAt != 0 && st.swimTicks >= 30 && life == st.life0 && life > 0 && st.maxStep < 2000.0f &&
                            st.goneAt != 0 && st.departAt != 0 && (st.goneAt - st.departAt) <= 100;
            logInfo("wp2 RESULT hylia-wasser %s B bei Ritt-Frame %u, Wasserkontakt nach %u Ticks, Schwimm-Ticks %u, Leben %u "
                    "(vorher %u), groesster Schritt %.1f, Vogel weg %u Ticks nach der Freigabe (Ticks mit Vogel %u)",
                ok ? "ok" : "fail", st.rideAtTap, st.wetAt != 0 ? st.wetAt - st.tapAt : 0u, st.swimTicks, life, st.life0,
                static_cast<double>(st.maxStep), st.goneAt != 0 && st.departAt != 0 ? st.goneAt - st.departAt : 0u,
                st.birdTicks);
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kHyliaWasser = {
    "hylia-wasser",
    "F6 W1: B ueber Wasser mit Seegrund (Hylia), Link im Wasser, kein Neustart",
    f6HyliaTick,
};

// WP2.14: Tod im Spawn-Fenster (Phase Summoning: Vogel da, Link am Boden, noch kein Ritt). Erwartung: der Vogel verschwindet
// nach dem Tod (Frist wie WP2.1), keine Absturz, Sitzung frei. Ergebnis: Logzeile "wp2 RESULT wp2-spawn-tod ok|fail <grund>".
constexpr uint32_t kWpSpawnWatch = 600;  // Beobachtung nach dem Tod (Ticks)

struct WpSpawnState {
    int stage = 0;               // 0 Ruf, 1 Chord bis Phase Summoning, 2 Tod und Beobachtung
    uint32_t t0 = 0;             // Tick des Rufs
    uint32_t hitAt = 0;          // Tick des Todes
    uint32_t birdTicks = 0;      // Ticks nach dem Tod mit einem eigenen Vogel (Information)
    uint32_t lastBirdSince = 0;  // Ticks nach dem Tod, als zuletzt ein eigener Vogel da war
    uint32_t birdLate = 0;       // Ticks mit Vogel mehr als kWpGoneTicks nach dem Tod
};
WpSpawnState g_wpSp;

bool wp2SpawnTodTick(Context& c) {
    const uint32_t t = c.frame;
    switch (g_wpSp.stage) {
    case 0:
        if (c.canCall() && t >= kWpCallAt) {
            g_wpSp.t0 = t;
            g_wpSp.stage = 1;
            c.note("wp2-spawn-tod: Ruf-Kombination (R halten, B tippen)");
        }
        break;
    case 1:
        if (t >= g_wpSp.t0 && t < g_wpSp.t0 + kWpHold) {
            c.holdR();
        }
        if (t == g_wpSp.t0 + kWpPressOffset) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Summoning) {
            daAlink_c* link = liveLink();
            if (link == nullptr) {
                logInfo("wp2 RESULT wp2-spawn-tod fail kein Link im Spawn-Fenster");
                return true;
            }
            dComIfGs_setLife(1);
            const int r = link->setDamagePoint(2, 0, 1, 0);  // Tod im Spawn-Fenster (Test-API)
            logInfo("wp2-spawn-tod: Tod im Spawn-Fenster (Phase Summoning, Voegel %u), Rueckgabe %d", wpCountBirds(), r);
            g_wpSp.hitAt = t;
            g_wpSp.stage = 2;
        } else if (c.sess.phase == Phase::Riding) {
            logInfo("wp2 RESULT wp2-spawn-tod fail Ritt begann, bevor der Spawn-Tick beobachtet wurde");
            return true;
        } else if (t > g_wpSp.t0 + kWpHold + 600) {
            logInfo("wp2 RESULT wp2-spawn-tod fail kein Spawn nach dem Ruf");
            return true;
        }
        break;
    case 2: {
        const uint32_t since = t - g_wpSp.hitAt;
        const uint32_t n = wpCountBirds();
        if (n > 0) {
            ++g_wpSp.birdTicks;
            g_wpSp.lastBirdSince = since;
            if (since > kWpGoneTicks) {
                ++g_wpSp.birdLate;
            }
        }
        if (since % 60 == 0) {
            logInfo("wp2-spawn-tod: %u Ticks nach Tod, Voegel im Spiel %u, phase=%s, leben=%u, cargo=%d midnaKago=%d", since, n,
                phaseName(c.sess.phase), static_cast<unsigned>(dComIfGs_getLife()),
                liveLink() != nullptr && liveLink()->checkCargoCarry() ? 1 : 0,
                daPy_py_c::getMidnaActor() != nullptr && daPy_py_c::getMidnaActor()->mpKago != nullptr ? 1 : 0);
        }
        if (since >= kWpSpawnWatch) {
            if (n == 0 && g_wpSp.birdLate == 0 && c.sess.phase == Phase::Idle) {
                logInfo("wp2 RESULT wp2-spawn-tod ok Vogel nach Tod im Spawn-Fenster spaetestens nach %u Ticks weg (Grenze %u), Sitzung frei",
                    g_wpSp.lastBirdSince, kWpGoneTicks);
            } else {
                logInfo("wp2 RESULT wp2-spawn-tod fail Vogel bis Tick %u nach dem Tod (Grenze %u), spaet %u, am Ende %u Voegel, phase=%s",
                    g_wpSp.lastBirdSince, kWpGoneTicks, g_wpSp.birdLate, n, phaseName(c.sess.phase));
            }
            return true;
        }
        break;
    }
    }
    return false;
}

const Scenario kWp2SpawnTod = {
    "wp2-spawn-tod",
    "WP2.14: Tod im Spawn-Fenster (Phase Summoning), Entfernen des Vogels, Sitzung frei",
    wp2SpawnTodTick,
};

// ---------------------------------------------------------------------------------------------
// Register und Auswahl
// ---------------------------------------------------------------------------------------------

// F2 Befund 7: Vorwaertsdeklaration des Szenarios "f2-void-ruf" (Definition am Dateiende, Block "F2 Befund 7").
extern const Scenario kF2VoidRuf;

void init() {
    (void)registerScenario(&kZyklus);
    (void)registerScenario(&kRufAbflug);
    (void)registerScenario(&kKonsum);
    (void)registerScenario(&kRufSpam);
    (void)registerScenario(&kLeben3);
    (void)registerScenario(&kPolZyklus);
    (void)registerScenario(&kPolTitel);
    (void)registerScenario(&kPolTod);
    (void)registerScenario(&kPolMenue);
    (void)registerScenario(&kPolSpeichern);
    (void)registerScenario(&kPolSpeichernJa);
    (void)registerScenario(&kSpielSpeichern);
    (void)registerScenario(&kPolSchreiben);
    (void)registerScenario(&kLandungHoch);
    (void)registerScenario(&kOrdonHoch);   // F5 R2-C
    (void)registerScenario(&kIdleStart);   // F5 R2-B Kontrolle
    (void)registerScenario(&kSteuerung);
    (void)registerScenario(&kDecke6);  // VORLAUF e6-decke
    // VORLAUF e9-landung
    (void)registerScenario(&kE9Klippe);
    (void)registerScenario(&kE9Mittel);
    (void)registerScenario(&kE9B2);
    (void)registerScenario(&kE9Abgrund);
    (void)registerScenario(&kE9Stick);
    (void)registerScenario(&kVorE5Ritt);    // VORLAUF e1e5-menue
    (void)registerScenario(&kVorE5Aussen);  // VORLAUF e1e5-menue
    (void)registerScenario(&kVorE5RittKarte);  // VORLAUF e1e5-menue
    (void)registerScenario(&kVorE1);        // VORLAUF e1e5-menue
    (void)registerScenario(&kWp2TodAbstieg);  // WP2 (Tod im Abflug, Zaehlung der Voegel)
    (void)registerScenario(&kWp2GameOver);    // WP2 (Ruf im Game-Over)
    (void)registerScenario(&kWp2B701);        // WP2.13 (B bei Ritt-Frame 701)
    (void)registerScenario(&kHyliaWasser);    // F6 W1: B ueber Wasser mit Seegrund
    (void)registerScenario(&kWp2SpawnTod);    // WP2.14 (Tod im Spawn-Fenster)
    (void)registerScenario(&kF2VoidRuf);      // F2 Befund 7 (Ruf, Void-Out, Ruf im Neustart, zweiter Ruf)
    registerInputScenarios();                 // WP3 (input_test.cpp)
}

ModResult registerScenario(const Scenario* scenario) {
    if (scenario == nullptr || scenario->name == nullptr || scenario->tick == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < g_count; ++i) {
        if (std::strcmp(g_list[i]->name, scenario->name) == 0) {
            logWarn("autotest scenario %s already registered (conflict)", scenario->name);
            return MOD_CONFLICT;
        }
    }
    if (g_count >= kMaxScenarios) {
        return MOD_ERROR;
    }
    g_list[g_count++] = scenario;
    return MOD_OK;
}

void selectFromEnvironment() {
    g_active = nullptr;
    const char* env = std::getenv("KFF_AUTOTEST");
    if (env == nullptr || env[0] == '\0' || std::strcmp(env, "0") == 0) {
        return;
    }
    for (size_t i = 0; i < g_count; ++i) {
        if (std::strcmp(g_list[i]->name, env) == 0) {
            g_active = g_list[i];
            logInfo("autotest: scenario %s active (%s)", g_active->name, g_active->description);
            return;
        }
    }
    char names[256] = "";
    size_t used = 0;
    for (size_t i = 0; i < g_count; ++i) {
        const int n = std::snprintf(names + used, sizeof(names) - used, "%s ", g_list[i]->name);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(names) - used) {
            break;
        }
        used += static_cast<size_t>(n);
    }
    logError("autotest: unknown scenario '%s', injection off. Available: %s", env, names);
}

const Scenario* active() {
    return g_active;
}

uint32_t foreignInputCount() {
    return g_foreign;
}

void observe(const interface_of_controller_pad& pad) {
    g_lastPad = pad;
}

const interface_of_controller_pad& lastPad() {
    return g_lastPad;
}

// ---------------------------------------------------------------------------------------------
// Injektion (Post-Hook mDoCPd_c::read)
// ---------------------------------------------------------------------------------------------

namespace {

float absOf(float v) {
    return v < 0.0f ? -v : v;
}

float maxAbsOf(float a, float b) {
    return absOf(a) > absOf(b) ? absOf(a) : absOf(b);
}

// Winkel in s16-Einheiten (65536 = 360 Grad), wie der Pad-Konverter sie liefert.
int16_t angleOf(float x, float y) {
    if (absOf(x) < 1.0e-6f && absOf(y) < 1.0e-6f) {
        return 0;
    }
    const double a = std::atan2(static_cast<double>(y), static_cast<double>(x));
    return static_cast<int16_t>(static_cast<int>(a * 32768.0 / 3.14159265358979323846));
}

// Roh-Pad vor der Injektion: alles ungleich Null ist eine Fremdeingabe (Tastatur, Maus, Controller).
bool rawForeign(const interface_of_controller_pad& p) {
    return p.mButtonFlags != 0 || absOf(p.mMainStickPosX) > 0.02f || absOf(p.mMainStickPosY) > 0.02f ||
           absOf(p.mCStickPosX) > 0.02f || absOf(p.mCStickPosY) > 0.02f || p.mTriggerLeft > 0.02f ||
           p.mTriggerRight > 0.02f || p.mAnalogA > 0.02f || p.mAnalogB > 0.02f;
}

// Ersetzt den GESAMTEN Zustand von Port 1 durch die Eingaben dieses Ticks.
void replacePad(interface_of_controller_pad& p, const TickInput& in) {
    uint32_t held = in.holdBits | in.tapBits;
    uint32_t edges = in.tapBits;
    if (in.holdR) {
        held |= kBitR;
        if (!g_prevR) {
            edges |= kBitR;  // Flanke: R wurde in diesem Tick gedrueckt
        }
    }
    g_prevR = in.holdR;

    p.mButtonFlags = held;
    p.mPressedButtonFlags = edges;
    p.mMainStickPosX = in.stickX;
    p.mMainStickPosY = in.stickY;
    p.mMainStickValue = maxAbsOf(in.stickX, in.stickY);
    p.mMainStickAngle = angleOf(in.stickX, in.stickY);
    p.mCStickPosX = in.cX;
    p.mCStickPosY = in.cY;
    p.mCStickValue = maxAbsOf(in.cX, in.cY);
    p.mCStickAngle = angleOf(in.cX, in.cY);
    p.mAnalogA = (held & kBitA) != 0 ? 1.0f : 0.0f;
    p.mAnalogB = (held & kBitB) != 0 ? 1.0f : 0.0f;
    p.mTriggerLeft = (held & kBitL) != 0 ? 1.0f : 0.0f;
    p.mTriggerRight = in.analogR >= 0.0f ? in.analogR : ((held & kBitR) != 0 ? 1.0f : 0.0f);
    if (in.gameLocks) {
        // WP3: wie mDoCPd_c::read. Der Vorzustand der Locks ist der Endzustand des letzten Ticks (inklusive Konsum
        // des Kerns), LRlockCheck rechnet daraus mit den Analogwerten dieses Ticks die Locks und die Flanken.
        p.mHoldLockL = g_lastPad.mHoldLockL;
        p.mHoldLockR = g_lastPad.mHoldLockR;
        mDoCPd_c::LRlockCheck(&p);
    } else {
        p.mHoldLockL = (held & kBitL) != 0 ? 1 : 0;
        p.mTrigLockL = (edges & kBitL) != 0 ? 1 : 0;
        p.mHoldLockR = (held & kBitR) != 0 ? 1 : 0;
        p.mTrigLockR = (edges & kBitR) != 0 ? 1 : 0;
    }
    p.mGamepadErrorFlags = 0;
}

// ---------------------------------------------------------------------------------------------
// F5 R2-B (nur Testbuild, KFF_TEST_LIFELOG=1): Lebenslog je Pad-Lesung (ein Tick). Protokolliert jede Lebensaenderung und
// jeden Wechsel in eine Schadensprozedur mit Tick, Prozedur, Position, Bodenabstand (Link-Acch), Cargo, Phase, Voegeln und
// den Gegnern im Umkreis von 2000 (3D, mit Name, Profil und Abstand). Alle 30 Ticks eine Herzzeile ohne Gegner. Nach dem
// Entladen (mod_shutdown) laeuft dieser Code nicht mehr, danach gibt es keine Zeilen.
constexpr float kLlEnemyRadius = 2000.0f;
constexpr uint32_t kLlHeartbeat = 30;
// Schadensprozeduren (d_a_alink.h): PROC_DAMAGE 50, PROC_LARGE_DAMAGE_UP 51, PROC_LAND_DAMAGE 52, PROC_HORSE_DAMAGE 68,
// PROC_SWIM_DAMAGE 127, PROC_WOLF_DAMAGE 263, PROC_WOLF_LARGE_DAMAGE_UP 264, PROC_WOLF_LAND_DAMAGE 265,
// PROC_POLY_DAMAGE 321, PROC_ELEC_DAMAGE 322, PROC_LARGE_DAMAGE 345, PROC_LARGE_DAMAGE_WALL 346; Tod: PROC_DEAD 336.
constexpr int kLlDamageProcs[] = {50, 51, 52, 68, 127, 263, 264, 265, 321, 322, 345, 346, 336};

bool llIsDamageProc(int p) {
    for (const int d : kLlDamageProcs) {
        if (p == d) {
            return true;
        }
    }
    return false;
}

struct LlState {
    bool init = false;
    uint32_t tick = 0;
    int life = 0;
    bool dmg = false;
};
LlState g_ll;

struct LlScan {
    cXyz center;
    uint32_t n = 0;
    uint32_t logged = 0;
};

void* llEnemyLog(void* actorPtr, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actorPtr);
    LlScan* st = static_cast<LlScan*>(data);
    if (ac == nullptr || fopAcM_GetGroup(ac) != fopAc_ENEMY_e) {
        return nullptr;
    }
    const float dx = ac->current.pos.x - st->center.x;
    const float dy = ac->current.pos.y - st->center.y;
    const float dz = ac->current.pos.z - st->center.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (d <= kLlEnemyRadius) {
        ++st->n;
        if (st->logged < 12) {
            ++st->logged;
            logInfo("LL gegner name=%d profil=%d abstand=%.0f id=0x%x", static_cast<int>(fopAcM_GetName(ac)),
                static_cast<int>(fopAcM_GetProfName(ac)), static_cast<double>(d),
                static_cast<unsigned>(fopAcM_GetID(ac)));
        }
    }
    return nullptr;  // alle Akteure durchlaufen
}

// Eine Lebenslog-Zeile fuer Link (what = Ereignis). withEnemies: zusaetzlich die Gegner im Umkreis.
void llLine(const char* what, daAlink_c* link, bool withEnemies) {
    const daMidna_c* midna = daPy_py_c::getMidnaActor();
    const float ground = link->mLinkAcch.GetGroundH();
    logInfo("LL %s tick=%u leben=%u/%u proc=%u pos=(%.1f,%.1f,%.1f) boden=%.1f abstand=%.1f cargo=%d phase=%s voegel=%u "
            "midnaKago=%d",
        what, g_ll.tick, static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()),
        static_cast<unsigned>(link->mProcID), static_cast<double>(link->current.pos.x),
        static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z), static_cast<double>(ground),
        static_cast<double>(link->current.pos.y - ground), link->checkCargoCarry() ? 1 : 0, phaseName(session().phase),
        static_cast<unsigned>(wpCountBirds()), midna != nullptr && midna->mpKago != nullptr ? 1 : 0);
    if (withEnemies) {
        LlScan st;
        st.center = link->current.pos;
        (void)fopAcM_Search(llEnemyLog, &st);
        logInfo("LL gegner im Umkreis %.0f: %u", static_cast<double>(kLlEnemyRadius), st.n);
    }
}

void llTick() {
    static const bool enabled = std::getenv("KFF_TEST_LIFELOG") != nullptr;
    if (!enabled) {
        return;
    }
    ++g_ll.tick;
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return;
    }
    const int life = static_cast<int>(dComIfGs_getLife());
    const bool dmg = llIsDamageProc(static_cast<int>(link->mProcID));
    if (!g_ll.init) {
        g_ll.init = true;
        g_ll.life = life;
        g_ll.dmg = dmg;
        llLine("start", link, false);
        return;
    }
    if (life != g_ll.life) {
        char what[48];
        std::snprintf(what, sizeof what, "leben %d->%d", g_ll.life, life);
        llLine(what, link, true);
        g_ll.life = life;
    }
    if (dmg && !g_ll.dmg) {
        char what[48];
        std::snprintf(what, sizeof what, "schaden-proc %u", static_cast<unsigned>(link->mProcID));
        llLine(what, link, true);
    }
    g_ll.dmg = dmg;
    if (g_ll.tick % kLlHeartbeat == 0) {
        llLine("herz", link, false);
    }
}

// F5 R3-A (nur Testbuild, KFF_TEST_DTRACE=1): Spurprotokoll je Frame vom Absteigen bis zur Landung. Protokolliert Lage und
// Geschwindigkeit von Link, die Bodensonde unter Link (wie groundBelowLink: Start 500 ueber der Position) mit Polygon und
// Hintergrundindex, die Raeume (roomNo von Link und vom Vogel, Stay-Raum), den Bodenkontakt (acch), die Sturzreferenz, Proc
// und Leben. Beginnt im ersten Frame in Phase Dismounting, endet nach drei Frames Bodenkontakt oder nach 400 Frames.
constexpr float kDtNoGround = -1.0e5f;  // wie kNoGroundY in ride.cpp
struct DtState {
    bool active = false;
    bool done = false;
    uint32_t frames = 0;       // Frames seit Beginn des Absteigens
    uint32_t touch = 0;        // Frames in Folge mit Bodenkontakt
    uint32_t firstGround = 0;  // Frame (ab Beginn), in dem die Sonde zum ersten Mal Boden findet; 0 = noch keiner
};
DtState g_dt;

void dismountTraceTick() {
    static const bool enabled = std::getenv("KFF_TEST_DTRACE") != nullptr;
    if (!enabled || g_dt.done) {
        return;
    }
    daAlink_c* link = liveLink();
    if (link == nullptr) {
        return;
    }
    if (!g_dt.active) {
        if (session().phase != Phase::Dismounting) {
            return;
        }
        g_dt.active = true;
        logInfo("DTR start: t=%u, Link (%.1f, %.1f, %.1f), proc %u, roomNo %d, Stay %d, Vogel %s", g_frame,
            static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
            static_cast<double>(link->current.pos.z), static_cast<unsigned>(link->mProcID), static_cast<int>(link->current.roomNo),
            dComIfGp_roomControl_getStayNo(), liveBird() != nullptr ? "da" : "keiner");
    }
    ++g_dt.frames;
    dBgS_GndChk gndChk;
    cXyz probe(link->current.pos.x, link->current.pos.y + 500.0f, link->current.pos.z);
    gndChk.SetPos(&probe);
    const float ground = dComIfG_Bgsp().GroundCross(&gndChk);
    const bool hit = ground > kDtNoGround;
    if (hit && g_dt.firstGround == 0) {
        g_dt.firstGround = g_dt.frames;
        logInfo("DTR erster Bodenfund: f=%u t=%u, Boden %.1f, Link y %.1f, Poly %d, BG %d, roomNo %d, Stay %d", g_dt.frames,
            g_frame, static_cast<double>(ground), static_cast<double>(link->current.pos.y), gndChk.GetPolyIndex(),
            gndChk.GetBgIndex(), static_cast<int>(link->current.roomNo), dComIfGp_roomControl_getStayNo());
    }
    logInfo("DTR f=%u t=%u phase=%s proc=%u leben=%u pos=(%.1f,%.1f,%.1f) old=(%.1f,%.1f,%.1f) speed=(%.2f,%.2f,%.2f) "
            "speedF=%.2f",
        g_dt.frames, g_frame, phaseName(session().phase), static_cast<unsigned>(link->mProcID),
        static_cast<unsigned>(dComIfGs_getLife()), static_cast<double>(link->current.pos.x),
        static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z),
        static_cast<double>(link->old.pos.x), static_cast<double>(link->old.pos.y), static_cast<double>(link->old.pos.z),
        static_cast<double>(link->speed.x), static_cast<double>(link->speed.y), static_cast<double>(link->speed.z),
        static_cast<double>(link->speedF));
    char sonde[48];
    if (hit) {
        std::snprintf(sonde, sizeof sonde, "%.1f poly %d bg %d", static_cast<double>(ground), gndChk.GetPolyIndex(),
            gndChk.GetBgIndex());
    } else {
        std::snprintf(sonde, sizeof sonde, "kein Boden");
    }
    char vogel[48];
    if (daKago_c* bird = liveBird()) {
        std::snprintf(vogel, sizeof vogel, "y %.1f roomNo %d speedF %.2f", static_cast<double>(bird->current.pos.y),
            static_cast<int>(bird->current.roomNo), static_cast<double>(bird->speedF));
    } else {
        std::snprintf(vogel, sizeof vogel, "keiner");
    }
    logInfo("DTR f=%u sonde=%s roomNo=%d stay=%d acch=%d/%.1f fallRef=%.1f/%.1f vogel=%s", g_dt.frames, sonde,
        static_cast<int>(link->current.roomNo), dComIfGp_roomControl_getStayNo(), link->mLinkAcch.ChkGroundHit() ? 1 : 0,
        static_cast<double>(link->mLinkAcch.GetGroundH()), static_cast<double>(link->mFallHeight),
        static_cast<double>(link->field_0x33c8), vogel);
    // F5 R3-A (nur Testcode): Wasser unter Link (Schwimm-Modus, Wasserhoehe, Wasser-Attribut 0x80 = Neustart)
    logInfo("DTR wasser f=%u wasserUnter=%d mWaterY=%.1f tiefe=%.1f wasserAtt0=%u wtr80=%d noReset80=%d proc=%u m40000=%d m2=%d",
        g_dt.frames, link->checkWaterPolygonUnder() ? 1 : 0, static_cast<double>(link->mWaterY),
        static_cast<double>(link->mWaterY - link->current.pos.y), static_cast<unsigned>(link->field_0x2fbc),
        (dKy_pol_argument_get(&link->mLinkAcch.m_wtr) & 0x80) != 0 ? 1 : 0,
        link->checkNoResetFlg0(daPy_py_c::FLG0_UNK_80) ? 1 : 0, static_cast<unsigned>(link->mProcID),
        link->checkModeFlg(0x40000) ? 1 : 0, link->checkModeFlg(2) ? 1 : 0);
    // F6 (nur Testcode): Eingaben der Neustart-Zweige von daAlink_c::checkRestartRoom (d_a_alink.cpp 13662-13760) je Frame.
    // outer1 = Zweig mit Abgrund-/Ausgangsboden oder ohne Boden und nicht schwimmend; innerA = Schwimmen mit Wasser oder
    // Fallhoehe > 2000 ohne Wasser (Modusflagge 2 oder FLG3 0x80000); innerB = Code 10 mit Fallhoehe <= 3000; innerD = Bodentreffer.
    {
        const float gh = link->mLinkAcch.GetGroundH();
        const bool swimF = link->checkModeFlg(0x40000) != 0;
        const bool noGnd = gh < -5.0e8f;
        const bool wet = link->mWaterY > gh;
        const float varF31 = link->mGroundCode == 10 ? link->current.pos.y - gh : link->field_0x33c8 - link->current.pos.y;
        const bool cargo = link->checkCargoCarry() != 0;
        const bool hookshot = link->mProcID == daAlink_c::PROC_HOOKSHOT_FLY;
        const bool outer1 = !cargo && (link->mGroundCode == 4 || link->mGroundCode == 10 || (noGnd && !swimF));
        const bool flag3 = link->checkNoResetFlg3(daPy_py_c::FLG3_UNK_80000) != 0;
        const bool innerA = link->mGroundCode != 10 &&
            ((wet && swimF) || (!wet && (link->checkModeFlg(2) != 0 || flag3) && !hookshot && !link->checkModeFlg(0x800) &&
                                varF31 > 2000.0f));
        const bool innerB = link->mGroundCode == 10 && varF31 <= 3000.0f;
        const bool innerD = link->mLinkAcch.ChkGroundHit() && !hookshot;
        logInfo("DTR neustart f=%u: gc=%d gh=%.1f wy=%.1f wet=%d swim=%d m2=%d flg3=%d var=%.1f cargo=%d proc=%u outer1=%d "
                "innerA=%d innerB=%d innerD=%d",
            g_dt.frames, link->mGroundCode, static_cast<double>(gh), static_cast<double>(link->mWaterY), wet ? 1 : 0,
            swimF ? 1 : 0, link->checkModeFlg(2) ? 1 : 0, flag3 ? 1 : 0, static_cast<double>(varF31), cargo ? 1 : 0,
            static_cast<unsigned>(link->mProcID), outer1 ? 1 : 0, innerA ? 1 : 0, innerB ? 1 : 0, innerD ? 1 : 0);
        // F6 (nur Testcode): Zweig 4 von checkRestartRoom (d_a_alink.cpp 13749): Modus 0x40 und Wasserpruefpunkt (fopAcM_wt_c).
        const bool m40 = link->checkModeFlg(0x40) != 0;
        const bool wtAtt80 = (link->field_0x2fc6 & 0x80) != 0;
        const bool br4iii = m40 && link->field_0x33bc < link->field_0x33b8 && link->field_0x33b8 != -G_CM3D_F_INF && wtAtt80;
        logInfo("DTR neustart4 f=%u: m40=%d wt33bc=%.1f wt33b8=%.1f wt2fc6=%u wt2fc5=%u wtAtt80=%d br4iii=%d",
            g_dt.frames, m40 ? 1 : 0, static_cast<double>(link->field_0x33bc), static_cast<double>(link->field_0x33b8),
            static_cast<unsigned>(link->field_0x2fc6), static_cast<unsigned>(link->field_0x2fc5), wtAtt80 ? 1 : 0,
            br4iii ? 1 : 0);
    }
    if (link->mLinkAcch.ChkGroundHit()) {
        ++g_dt.touch;
    } else {
        g_dt.touch = 0;
    }
    if (g_dt.touch >= 3 || g_dt.frames >= 400) {
        g_dt.done = true;
        logInfo("DTR ende: f=%u, erster Bodenfund f=%u, Leben %u/%u", g_dt.frames, g_dt.firstGround,
            static_cast<unsigned>(dComIfGs_getLife()), static_cast<unsigned>(dComIfGs_getMaxLife()));
    }
}

}  // namespace

void inject(interface_of_controller_pad& p) {
    llTick();  // F5 R2-B (nur mit KFF_TEST_LIFELOG): laeuft auch nach dem Ende des Szenarios
    dismountTraceTick();  // F5 R3-A (nur mit KFF_TEST_DTRACE): Spur des Absteigens je Frame
    if (g_active == nullptr || g_finished) {
        return;
    }
    if (session().linkCalls > 0) {
        ++g_frame;
    }
    g_in = TickInput{};
    Context ctx{g_frame, session()};
    const bool done = g_active->tick(ctx);

    // Roh-Pad vor der Ersetzung pruefen: echte Eingaben im Testfenster machen den Lauf ungueltig.
    if (rawForeign(p)) {
        ++g_foreign;
        logEvery(g_foreignLog, 10, 100,
            "autotest: FREMDEINGABE frame=%u buttons=0x%x stick=(%.2f,%.2f) c=(%.2f,%.2f) trig=(%.2f,%.2f) "
            "fremdeingabe=%u",
            g_frame, static_cast<unsigned>(p.mButtonFlags), static_cast<double>(p.mMainStickPosX),
            static_cast<double>(p.mMainStickPosY), static_cast<double>(p.mCStickPosX),
            static_cast<double>(p.mCStickPosY), static_cast<double>(p.mTriggerLeft),
            static_cast<double>(p.mTriggerRight), g_foreign);
    }
    replacePad(p, g_in);

    if (done) {
        g_finished = true;
        logInfo("autotest: scenario %s finished, fremdeingabe=%u, no further input", g_active->name, g_foreign);
    }
}

}  // namespace kff::autotest

// ---------------------------------------------------------------------------------------------
// F2 Befund 7: Szenario "f2-void-ruf" (nur Testbuild, KFF_AUTOTEST=f2-void-ruf).
// Ablauf: Ruf, Ritt (60 Ticks neutraler Stick), Void-Out (Vogel per Test-Teleport in einen leeren Bereich, B-Tipp
// 2 Ticks spaeter), Ruf waehrend des Neustarts, Neustart abwarten, zweiter Ruf.
// Erwartung: Der Ruf im Neustart wird vom Kern abgewiesen (der Grund steht im Kern-Log, nicht geprueft). Nach dem
// Neustart startet der zweite Ruf wieder einen Ritt.
// Void-Out wie WP1.4 (freiheit.cpp, void-absteigen, Drop::Void): findVoid, voidDist, Test-Teleport, B 2 Ticks spaeter.
// Die Hilfsfunktionen sind hier unter f2-Namen neu geschrieben, freiheit.cpp bleibt unberuehrt.
// Ergebnis: genau eine Zeile "f2: void-ruf ERGEBNIS ok ritt2=<Ticks>" oder "f2: void-ruf ERGEBNIS fail <Grund>".
// ---------------------------------------------------------------------------------------------
namespace kff::autotest {
namespace {

constexpr uint32_t kF2StartFrist = 600;           // Start: Link bereit, Frist ab dem ersten Tick (Ticks)
constexpr uint32_t kF2RufFrist = 600;             // Ritt 1 nach dem ersten Ruf (Ticks)
constexpr uint32_t kF2Ritt1Frist = 900;           // Ritt 1: 60 Ritt-Ticks spaetestens so viele Ticks nach dem Ruf
constexpr uint32_t kF2RittTicks = 60;             // Ritt 1: so viele Ritt-Ticks mit neutralem Stick, dann Void-Out
constexpr uint32_t kF2BNachVerschieben = 2;       // B-Tipp zwei Ticks nach dem Verschieben (wie void-absteigen)
constexpr uint32_t kF2RufImNeustartFenster = 60;  // Ruf im Neustart: spaetestens so viele Ticks nach dem B-Tipp
constexpr uint32_t kF2NeustartFrist = 2400;       // Neustart: Frist ab dem B-Tipp (Ticks)
constexpr uint32_t kF2NeustartStabil = 30;        // "Neustart vorbei" muss so viele Ticks ununterbrochen gelten
constexpr uint32_t kF2Ritt2Frist = 300;           // zweiter Ritt: Frist ab dem zweiten Ruf (10 s bei 30 Hz)
constexpr uint32_t kF2StatusEvery = 60;           // Statuszeile im Neustart alle so vielen Ticks
constexpr float kF2VoidDistStandard = 30000.0f;   // Abstand zum Abgrund ohne KFS_VOID_DIST (wie voidDist())
constexpr float kF2BodenGrenze = -1.0e5f;         // Bodenhoehe ueber diesem Wert: Boden vorhanden (wie e9Ground)

// Ereignis laeuft: das Ereignis-System oder Link selbst (wie inEvent() in freiheit.cpp).
bool f2InEvent() {
    if (dComIfGp_event_runCheck() != 0) {
        return true;
    }
    daAlink_c* link = liveLink();
    return link != nullptr && link->checkEventRun() != 0;
}

// Start-Bedingung: Link lebt, steht am Boden, ist in keinem Ereignis und hat keinen eigenen Vogel.
bool f2LinkReady(const Context& c) {
    daAlink_c* link = liveLink();
    return link != nullptr && link->mLinkAcch.ChkGroundHit() && !f2InEvent() && c.sess.birdId == kNoProc;
}

// Ereignis-Texte und Game-Over-Auswahl mit A weiterschalten (wie tapAInEvent in freiheit.cpp).
void f2TapAInEvent(Context& c) {
    if (f2InEvent() && c.frame % 20 == 0) {
        c.tapA();
    }
}

// Abstand zum Abgrund wie voidDist() in freiheit.cpp: KFS_VOID_DIST, wenn der Wert ueber 1000 liegt, sonst 30000.
float f2VoidDist() {
    const char* e = std::getenv("KFS_VOID_DIST");
    const float v = e != nullptr ? static_cast<float>(std::atof(e)) : 0.0f;
    return v > 1000.0f ? v : kF2VoidDistStandard;
}

// Boden unter (x, 60000, z) wie groundAt() in freiheit.cpp; Sonde und Grenze wie e9Ground.
bool f2HasGround(float x, float z) {
    dBgS_GndChk chk;
    cXyz p(x, 60000.0f, z);
    chk.SetPos(&p);
    return dComIfG_Bgsp().GroundCross(&chk) > kF2BodenGrenze;
}

// Leeres Gebiet wie findVoid() in freiheit.cpp: ab from in +X, -X, +Z, -Z in den Abstaenden dist, 1,5x, 2x.
// Kein Boden in einem 3x3-Raster mit 800 Abstand. Rueckgabe: Punkt auf der Hoehe von from.
bool f2FindVoid(const cXyz& from, float dist, cXyz* out) {
    const float dirs[4][2] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
    const float mults[3] = {1.0f, 1.5f, 2.0f};
    for (const auto& d : dirs) {
        for (const float m : mults) {
            const float x = from.x + d[0] * dist * m;
            const float z = from.z + d[1] * dist * m;
            bool leer = true;
            for (int i = -1; i <= 1 && leer; ++i) {
                for (int j = -1; j <= 1 && leer; ++j) {
                    if (f2HasGround(x + 800.0f * static_cast<float>(i), z + 800.0f * static_cast<float>(j))) {
                        leer = false;
                    }
                }
            }
            if (leer) {
                *out = cXyz(x, from.y, z);
                return true;
            }
        }
    }
    return false;
}

// Test-Teleport des Vogels wie placeBird() in freiheit.cpp (Position, alte Position, Gierwinkel).
void f2PlaceBird(daKago_c* k, const cXyz& at, s16 yaw) {
    k->current.pos = at;
    k->old.pos = at;
    k->current.angle.y = yaw;
    k->shape_angle.y = yaw;
}

// Schluss mit Fehler: Zeile FAIL mit dem Grund, danach die eine ERGEBNIS-Zeile. Rueckgabe true: Szenario fertig.
bool f2Fail(const char* grund) {
    logInfo("f2: void-ruf FAIL %s", grund);
    logInfo("f2: void-ruf ERGEBNIS fail %s", grund);
    return true;
}

enum class F2Stage : uint8_t { Start, Ruf1, Ritt1, VoidOut, Neustart, Ruf2 };

struct F2VoidRufState {
    bool begonnen = false;             // Zustand beim ersten Tick des Szenarios zurueckgesetzt
    F2Stage stage = F2Stage::Start;
    uint32_t t0 = 0;                   // erster Tick des Szenarios (Startfrist)
    uint32_t rufTick = 0;              // Chord-Start des ersten Rufs
    uint32_t verschobenTick = 0;       // Tick des Test-Teleports (Void-Out)
    uint32_t bTick = 0;                // Tick des B-Tipps: Beginn des Neustarts
    bool neustartRufGedrueckt = false; // Ruf waehrend des Neustarts ist gedrueckt (genau einmal)
    bool neustartRufAktiv = false;     // dessen Chord laeuft noch
    uint32_t neustartRufStart = 0;     // Chord-Start dieses Rufs
    bool bereitGilt = false;           // "Neustart vorbei" gilt seit bereitSeit
    uint32_t bereitSeit = 0;
    uint32_t ruf2Tick = 0;             // Chord-Start des zweiten Rufs
};
F2VoidRufState g_f2vr;

bool f2VoidRufTick(Context& c) {
    const uint32_t t = c.frame;
    F2VoidRufState& s = g_f2vr;
    if (!s.begonnen) {
        s = F2VoidRufState{};
        s.begonnen = true;
        s.t0 = t;
    }
    // Ruf waehrend des Neustarts: sein Chord laeuft zu Ende, auch wenn die Stufe schon gewechselt hat.
    if (s.neustartRufAktiv && abChord(c, s.neustartRufStart)) {
        s.neustartRufAktiv = false;
    }
    switch (s.stage) {
    case F2Stage::Start:
        if (t >= kZyCallAt && f2LinkReady(c)) {
            s.stage = F2Stage::Ruf1;
            s.rufTick = t;
            logInfo("f2: void-ruf Ruf 1 t=%u (R halten, B tippen), canCall=%d", t, c.canCall() ? 1 : 0);
            (void)abChord(c, s.rufTick);
        } else if (t - s.t0 >= kF2StartFrist) {
            return f2Fail("Start: Link nicht bereit innerhalb 600 Ticks");
        }
        break;
    case F2Stage::Ruf1:
        if (c.sess.phase == Phase::Riding) {
            s.stage = F2Stage::Ritt1;
            logInfo("f2: void-ruf ritt1 start t=%u", t);
        } else {
            (void)abChord(c, s.rufTick);
            if (t - s.rufTick >= kF2RufFrist) {
                return f2Fail("ritt1 nicht innerhalb 600 Ticks");
            }
        }
        break;
    case F2Stage::Ritt1: {
        if (c.sess.phase != Phase::Riding) {
            return f2Fail("Ritt 1 endete vor dem Void-Out");
        }
        c.stick(0.0f, 0.0f);  // neutral bis zum Void-Out
        if (c.sess.rideFrames < kF2RittTicks) {
            if (t - s.rufTick >= kF2Ritt1Frist) {
                return f2Fail("Ritt 1 ohne 60 Ritt-Ticks innerhalb 900 Ticks");
            }
            break;
        }
        daKago_c* k = kff::liveBird();
        if (k == nullptr) {
            return f2Fail("Vogel fehlt beim Void-Out");
        }
        cXyz ziel;
        if (!f2FindVoid(k->current.pos, f2VoidDist(), &ziel)) {
            char grund[96];
            std::snprintf(grund, sizeof(grund), "kein Abgrund im Abstand %.0f", static_cast<double>(f2VoidDist()));
            return f2Fail(grund);
        }
        const cXyz von = k->current.pos;
        f2PlaceBird(k, ziel, k->current.angle.y);
        s.verschobenTick = t;
        s.stage = F2Stage::VoidOut;
        logInfo("f2: void-ruf Vogel in den Abgrund versetzt (Test-Teleport) t=%u Ritt-Frame %u von (%.0f,%.0f,%.0f) "
                "nach (%.0f,%.0f,%.0f), Boden darunter: %s",
            t, c.sess.rideFrames, static_cast<double>(von.x), static_cast<double>(von.y), static_cast<double>(von.z),
            static_cast<double>(ziel.x), static_cast<double>(ziel.y), static_cast<double>(ziel.z),
            f2HasGround(ziel.x, ziel.z) ? "ja" : "keiner");
        break;
    }
    case F2Stage::VoidOut:
        if (t >= s.verschobenTick + kF2BNachVerschieben) {
            c.tapB();
            s.bTick = t;
            s.stage = F2Stage::Neustart;
            logInfo("f2: void-ruf B t=%u (Void-Out, %u Ticks nach dem Verschieben)", t, t - s.verschobenTick);
        }
        break;
    case F2Stage::Neustart: {
        f2TapAInEvent(c);
        const bool ereignis = f2InEvent();
        // Ruf waehrend des Neustarts: einmal, sobald ein Ereignis laeuft, spaetestens so viele Ticks nach dem B-Tipp.
        if (!s.neustartRufGedrueckt && (ereignis || t >= s.bTick + kF2RufImNeustartFenster)) {
            s.neustartRufGedrueckt = true;
            s.neustartRufAktiv = true;
            s.neustartRufStart = t;
            logInfo("f2: void-ruf Ruf waehrend Neustart t=%u", t);
            (void)abChord(c, s.neustartRufStart);
        }
        daAlink_c* link = liveLink();
        const bool boden = link != nullptr && link->mLinkAcch.ChkGroundHit();
        const bool bereit = boden && !ereignis && c.sess.birdId == kNoProc && c.sess.phase == Phase::Idle;
        if ((t - s.bTick) % kF2StatusEvery == 0) {
            logInfo("f2: void-ruf Neustart t=%u, %u Ticks nach B, phase=%s, eigener Vogel=%d, Ereignis=%d, "
                    "Boden=%d, leben=%u",
                t, t - s.bTick, phaseName(c.sess.phase), c.sess.birdId != kNoProc ? 1 : 0, ereignis ? 1 : 0,
                boden ? 1 : 0, static_cast<unsigned>(dComIfGs_getLife()));
        }
        if (!bereit) {
            s.bereitGilt = false;
        } else if (!s.bereitGilt) {
            s.bereitGilt = true;
            s.bereitSeit = t;
        }
        if (s.bereitGilt && t - s.bereitSeit >= kF2NeustartStabil) {
            logInfo("f2: void-ruf Neustart vorbei t=%u", t);
            s.ruf2Tick = t;
            s.stage = F2Stage::Ruf2;
            logInfo("f2: void-ruf Ruf 2 t=%u (R halten, B tippen), canCall=%d", t, c.canCall() ? 1 : 0);
            (void)abChord(c, s.ruf2Tick);
        } else if (t - s.bTick >= kF2NeustartFrist) {
            return f2Fail("Zeitlimit Neustart");
        }
        break;
    }
    case F2Stage::Ruf2:
        if (c.sess.phase == Phase::Riding) {
            const uint32_t nach = t - s.ruf2Tick;
            logInfo("f2: void-ruf ritt2 start t=%u nach %u Ticks", t, nach);
            logInfo("f2: void-ruf ERGEBNIS ok ritt2=%u", nach);
            return true;
        }
        (void)abChord(c, s.ruf2Tick);
        if (t - s.ruf2Tick >= kF2Ritt2Frist) {
            return f2Fail("ritt2 nicht innerhalb 300 Ticks");
        }
        break;
    }
    return false;
}

}  // namespace

const Scenario kF2VoidRuf = {
    "f2-void-ruf",
    "F2 Befund 7: Ruf, Ritt, Void-Out, Ruf im Neustart, Neustart abwarten, zweiter Ruf (Ritt2 muss starten)",
    f2VoidRufTick,
};

}  // namespace kff::autotest

// ---------------------------------------------------------------------------------------------
// F4 (Restbefunde WP2+7): Testszenarien fuer B1 (Ruf bei Leben 0 vor PROC_DEAD) und B2 (verweigertes Speichern im Ritt).
// Nur im Testbuild, nur ueber KFF_AUTOTEST. Die Eingaben folgen dem Zustand (Menue-Proc, Cursor, Ritt-Phase).
// ---------------------------------------------------------------------------------------------
namespace kff::autotest {
namespace {

constexpr uint32_t kF4Start = 300;          // frueheste Ruf-Zeit (Ticks), danach sobald canCall() gilt
constexpr uint32_t kF4Hold = 40;            // R gehalten (Ruf-Kombination wie in den Politik-Szenarien)
constexpr uint32_t kF4Press = 20;           // B-Tipp in diesem Tick (B1: Leben auf 0 im selben Tick)
constexpr uint32_t kF4B1Beobachtung = 240;  // B1: Beobachtung nach dem Ruf (Ticks)
constexpr uint32_t kF4B2Ritt = 120;         // B2: Ritt-Ticks vor dem START
constexpr uint32_t kF4B2Menue = 900;        // B2: Ticks im Speichermenue, danach wird das Menue geschlossen
constexpr uint32_t kF4B2Frist = 4200;       // B2: Gesamtfrist ab dem Ruf (Ticks)

struct F4B1State {
    int stage = 0;  // 0 warten, 1 Ruf-Kombination mit Leben 0, 2 Beobachtung
    uint32_t t0 = 0;
    uint32_t callAt = 0;
};
F4B1State g_f4b1;

struct F4B2State {
    int stage = 0;  // 0 warten, 1 Ruf, 2 Ritt, 3 Speichermenue (Ja), 4 Menue schliessen, 5 Abstieg, 6 Ende
    uint32_t t0 = 0;
    uint32_t rideAt = 0;
    uint32_t menuAt = 0;
    uint32_t closeAt = 0;
    uint32_t dismountAt = 0;
    uint32_t lastProc = 0xFFFFFFFFu;
    uint32_t lastA = 0;
    int jaCount = 0;
    bool fehlerGesehen = false;  // das Menue hat ein Fehlerergebnis gelesen (SaveSync 2)
};
F4B2State g_f4b2;

// Menue-Zustand fuer die Tests (nullptr, wenn kein Speichermenue offen ist).
dMenu_save_c* f4SaveMenu() {
    dMw_c* window = dMeter2Info_getMenuWindowClass();
    return window != nullptr ? window->mpMenuSave : nullptr;
}

void f4LogMenu(const char* tag, const Context& c, const dMenu_save_c* menu, uint32_t m) {
    logInfo("%s: m=%u proc=0x%02X cursor=%d status=%u cmd=%d card=%d field=%d pause=%u window=%u phase=%s link=%d", tag,
        static_cast<unsigned>(m), menu != nullptr ? static_cast<unsigned>(menu->mMenuProc) : 0xFFu,
        menu != nullptr ? static_cast<int>(menu->mYesNoCursor) : -1,
        menu != nullptr ? static_cast<unsigned>(menu->mSaveStatus) : 0xFFu,
        menu != nullptr ? menu->mCmdState : -1, static_cast<int>(g_mDoMemCd_control.mCardState),
        static_cast<int>(g_mDoMemCd_control.field_0x1fc8), static_cast<unsigned>(dComIfGp_isPauseFlag()),
        static_cast<unsigned>(dMeter2Info_getWindowStatus()), phaseName(c.sess.phase), liveLink() != nullptr ? 1 : 0);
}

bool f4B1Tick(Context& c) {
    const uint32_t t = c.frame;
    daAlink_c* link = liveLink();
    if (g_f4b1.stage == 0) {
        if (t >= kF4Start && c.canCall()) {
            g_f4b1.t0 = t;
            g_f4b1.stage = 1;
            logInfo("f4-b1: Vorbedingung erfuellt, Leben=%u proc=0x%x", static_cast<unsigned>(dComIfGs_getLife()),
                link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u);
        } else if (t > kF4Start + 3000) {
            c.note("f4-b1: kein Ruf moeglich, Szenario endet");
            return true;
        }
        return false;
    }
    if (g_f4b1.stage == 1) {
        const uint32_t m = t - g_f4b1.t0;
        if (m < kF4Hold) {
            c.holdR();
        }
        if (m == kF4Press) {
            dComIfGs_setLife(0);  // Leben 0, der Proc ist noch nicht PROC_DEAD (der Tod folgt im Link-Execute)
            logInfo("f4-b1: Leben auf 0 gesetzt, Ruf im selben Tick, Leben=%u proc=0x%x",
                static_cast<unsigned>(dComIfGs_getLife()), link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u);
            c.tapB();
            g_f4b1.callAt = t;
            g_f4b1.stage = 2;
        }
        return false;
    }
    if (t - g_f4b1.callAt >= kF4B1Beobachtung) {
        logInfo("f4-b1: Ende, Leben=%u proc=0x%x phase=%s", static_cast<unsigned>(dComIfGs_getLife()),
            link != nullptr ? static_cast<unsigned>(link->mProcID) : 0u, phaseName(c.sess.phase));
        c.note("f4-b1: Beobachtung beendet");
        return true;
    }
    return false;
}

bool f4B2Tick(Context& c) {
    const uint32_t t = c.frame;
    dMenu_save_c* menu = f4SaveMenu();
    if (t - g_f4b2.t0 > kF4B2Frist && g_f4b2.stage != 0) {
        f4LogMenu("f4-b2: Frist erreicht", c, menu, t - g_f4b2.menuAt);
        c.note("f4-b2: Frist erreicht, Szenario endet");
        return true;
    }
    switch (g_f4b2.stage) {
    case 0:
        if (t >= kF4Start && c.canCall()) {
            g_f4b2.t0 = t;
            g_f4b2.stage = 1;
        } else if (t > kF4Start + 3000) {
            c.note("f4-b2: kein Ruf moeglich, Szenario endet");
            return true;
        }
        break;
    case 1: {
        const uint32_t m = t - g_f4b2.t0;
        if (m < kF4Hold) {
            c.holdR();
        }
        if (m == kF4Press) {
            c.tapB();
        }
        if (c.sess.phase == Phase::Riding) {
            g_f4b2.rideAt = t;
            g_f4b2.stage = 2;
            logInfo("f4-b2: Ritt gestartet, Ruf-Ticks=%u", m);
        } else if (m > kF4Hold + 600) {
            c.note("f4-b2: Ritt startet nicht, Szenario endet");
            return true;
        }
        break;
    }
    case 2:
        if (c.sess.phase != Phase::Riding) {
            c.note("f4-b2: Ritt endete vor dem Menue, Szenario endet");
            return true;
        }
        if (t - g_f4b2.rideAt >= kF4B2Ritt) {
            g_f4b2.menuAt = t;
            g_f4b2.stage = 3;
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            logInfo("f4-b2: START im Ritt gedrueckt");
        }
        break;
    case 3: {
        const uint32_t m = t - g_f4b2.menuAt;
        // Cursor zur Speichern-Schaltflaeche (drei Tipps nach unten), dann A (Speichern-Auswahl).
        if (m == 30 || m == 60 || m == 90) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_DOWN));
        }
        if (m == 120) {
            c.tapA();
        }
        // Danach steuert der Zustand: Frage mit Cursor auf Nein nach links, Cursor auf Ja dann A, Fehlertext mit A.
        if (menu != nullptr && m >= 150 && g_f4b2.jaCount < 12) {
            // Erst "Ja" (Cursor 1) bis zum Fehlerergebnis (mCmdState 2), danach "Nein" (Cursor 0), damit das Menue schliesst.
            if (menu->mCmdState == 2) {
                g_f4b2.fehlerGesehen = true;
            }
            const uint32_t proc = static_cast<uint32_t>(menu->mMenuProc);
            const bool frage = proc == dMenu_save_c::PROC_SAVE_QUESTION || proc == dMenu_save_c::PROC_SAVE_YES_NO_SELECT ||
                proc == dMenu_save_c::PROC_MAKE_GAME_FILE_SEL_DISP || proc == dMenu_save_c::PROC_MEMCARD_MAKE_GAME_FILE_SEL;
            const bool taste = proc == dMenu_save_c::PROC_MEMCARD_ERRMSG_WAIT_KEY;
            const int ziel = g_f4b2.fehlerGesehen ? 0 : 1;
            if (taste || (frage && menu->mYesNoCursor == ziel)) {
                if (g_f4b2.jaCount == 0 || m - g_f4b2.lastA >= 30) {
                    c.tapA();
                    g_f4b2.lastA = m;
                    ++g_f4b2.jaCount;
                    logInfo("f4-b2: A (%d) m=%u proc=0x%02X cursor=%d ziel=%d", g_f4b2.jaCount, m, proc, menu->mYesNoCursor,
                        ziel);
                }
            } else if (frage && m % 60 == 0) {
                c.tap(static_cast<uint32_t>(ziel == 1 ? PAD_BUTTON_LEFT : PAD_BUTTON_RIGHT));
                logInfo("f4-b2: Cursor %s m=%u cursor=%d", ziel == 1 ? "LEFT" : "RIGHT", m, menu->mYesNoCursor);
            }
        }
        if (menu != nullptr && (static_cast<uint32_t>(menu->mMenuProc) != g_f4b2.lastProc || m % 60 == 0)) {
            g_f4b2.lastProc = static_cast<uint32_t>(menu->mMenuProc);
            f4LogMenu("f4-b2", c, menu, m);
        }
        if (m >= kF4B2Menue) {
            g_f4b2.closeAt = t;
            g_f4b2.stage = 4;
        }
        break;
    }
    case 4: {
        const uint32_t m = t - g_f4b2.closeAt;
        const bool offen = dComIfGp_isPauseFlag() != 0 || dMeter2Info_getWindowStatus() != 0;
        if (!offen) {
            f4LogMenu("f4-b2: Menue zu", c, menu, m);
            g_f4b2.dismountAt = t;
            g_f4b2.stage = 5;
            break;
        }
        if (m > 0 && m % 60 == 0) {
            c.tapB();  // B schliesst das offene Menue (nur solange es offen ist, sonst waere es der Abstieg)
            f4LogMenu("f4-b2: B (Menue zu)", c, menu, m);
        }
        if (m >= 900) {
            f4LogMenu("f4-b2: Menue schliesst nicht", c, menu, m);
            c.note("f4-b2: Menue schliesst nicht, Szenario endet");
            return true;
        }
        break;
    }
    case 5:
        if (t - g_f4b2.dismountAt >= 30) {
            c.tapB();  // Abstieg: prueft, dass die Eingabe nach dem Menue wieder wirkt
            logInfo("f4-b2: B nach dem Menue (Abstieg), phase=%s", phaseName(c.sess.phase));
            g_f4b2.stage = 6;
            g_f4b2.dismountAt = t;
        }
        break;
    case 6:
        if (t - g_f4b2.dismountAt >= 150) {
            logInfo("f4-b2: Ende, phase=%s pause=%u window=%u", phaseName(c.sess.phase),
                static_cast<unsigned>(dComIfGp_isPauseFlag()), static_cast<unsigned>(dMeter2Info_getWindowStatus()));
            c.note("f4-b2: Beobachtung beendet");
            return true;
        }
        break;
    }
    return false;
}

const Scenario kF4B1DeadCall = {
    "f4-b1-dead-call",
    "F4 B1: Leben 0 bei Proc vor PROC_DEAD, Ruf im selben Tick (Erwartung: abgelehnt, kein Vogel)",
    f4B1Tick,
};

const Scenario kF4B2SaveJa = {
    "f4-b2-save-ja",
    "F4 B2: Ritt, START, Speichern, Ja (zustandsgesteuert), Fehlerabschluss im Ritt, Menue zu, Abstieg",
    f4B2Tick,
};

struct F4Registrar {
    F4Registrar() {
        (void)registerScenario(&kF4B1DeadCall);
        (void)registerScenario(&kF4B2SaveJa);
    }
};
const F4Registrar g_f4Registrar;

}  // namespace

}  // namespace kff::autotest
