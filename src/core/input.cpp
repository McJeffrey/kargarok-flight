// Eingabe des Kerns: Pad-Lesung (Post-Hook von mDoCPd_c::read), Schnappschuss fuer die Module, Ruf-Kombination (R + B),
// Konsum der Ritt-Tasten und UI-Block (E5). Ausgelagert aus ride.cpp (Welle 2, WP3).
//
// Eingabe (Politik, verbindlich): padReadPost nimmt zuerst den Schnappschuss (kff::input()), dann entscheidet er
// den Ruf (R gehalten + B-Tipp, nur bei angenommenem Ruf verschluckt er B) und das Absteigen (B-Tipp im Ritt),
// danach entzieht er im Ritt A, B, X, Y, Z, L, R und das Steuerkreuz der Spiellogik (Start bleibt).
//
// WP3-Haertung:
//   F02  Jede Ablehnung hat einen Grund. Die Zeilen sind je Grund ratenbegrenzt (erste sofort, danach hoechstens eine je
//        kRejectLogTicks), jede Zeile traegt n = Ablehnungen seit der letzten Zeile dieses Grundes. Die Zeile INPUT der
//        Zusammenfassung gleicht ab: rejected == rejectSum, restored == dropSum.
//   D8   Kein Latch: Ruf und Absteigen gelten nur im Tick ihrer Entstehung. Zuruecksetzen ganz am Anfang des naechsten
//        Pad-Ticks (auch wenn danach eine Ausnahme kommt), B eines nicht ausgefuehrten Rufs geht an das Spiel zurueck,
//        linkExecutePre fuehrt nur einen Ruf aus dem laufenden Tick aus (callRequestFresh).
//   RK (a) Im Ritt sieht das Spiel weder Analogwerte noch Lock-Flaggen von A, B, L und R (auch mHoldLockR). Die falsche
//        Flanke nach dem Ritt (P2-04) verhindert repairLocks mit dem Schattenzustand der Locks.
//   E5   Pause, Menue, Karte, Item-Rad, Dokument, Dialog, Ereignis: kein Konsum, B bedient das Menue (einmal je Tick
//        bestimmt, im Schnappschuss als uiBlocked/uiDocument).
#include "d/dolzel_rel.h"

#include "core/actions.hpp"
#include "core/handlers.hpp"
#include "core/input_core.hpp"
#include "core/kff/autotest.hpp"
#include "core/kff/input.hpp"
#include "core/kff/log.hpp"
#include "core/kff/session.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "f_ap/f_ap_game.h"
#include "m_Do/m_Do_controller_pad.h"

#include <bit>
#include <cstdio>
#include <cstring>

namespace kff {
namespace {

core::InputCounts g_counts;
Input g_input;             // Schnappschuss des letzten Ticks (vor dem Konsum)
uint32_t g_tick = 0;       // Zaehler der Pad-Ticks (je mDoCPd_c::read, Post-Hook)
uint32_t g_callTick = 0;   // D8: Pad-Tick, in dem der laufende Ruf angenommen wurde
// Bit, das der letzte angenommene Ruf verschluckt hat (Ruf tippen, E-11). Nur bei einem Ruf gesetzt, am Tickanfang
// NICHT zurueckgesetzt: restoreChordB braucht es noch im naechsten Tick (D8).
uint32_t g_chordBit = 0;
uint32_t g_logStale = 0;
uint32_t g_logRepair = 0;

// ---------------------------------------------------------------------------------------------
// F02: Grundtabelle der Ablehnungen (ratenbegrenzt)
// ---------------------------------------------------------------------------------------------

constexpr size_t kRejectSlots = 64;         // 62 Gruende + je ein Sammelplatz fuer Pad und Link
constexpr uint32_t kRejectLogTicks = 90;    // hoechstens eine Zeile je Grund und 90 Pad-Ticks (1,5 s)
constexpr size_t kWhyLen = 80;

struct RejectSlot {
    char why[kWhyLen];
    bool link;          // false: Pad-Ablehnung ("call rejected"), true: am Link-Frame verworfen ("chord dropped")
    bool logged;        // schon eine Zeile geschrieben (die erste kommt sofort)
    uint32_t pending;   // Ablehnungen seit der letzten Zeile
    uint32_t total;     // Ablehnungen insgesamt (Zusammenfassung)
    uint32_t lastTick;  // Pad-Tick der letzten Zeile
};
RejectSlot g_rejects[kRejectSlots] = {};
size_t g_rejectUsed = 0;       // belegte regulaere Plaetze
uint32_t g_rejectPending = 0;  // Summe aller pending (schneller Test je Tick)
char g_lastReject[kWhyLen] = "";

void emitReject(RejectSlot& r) {
    if (r.pending == 0) {
        return;
    }
    if (r.link) {
        logInfo("chord dropped (%s), B returned to the game (n=%u)", r.why, r.pending);
        ++g_counts.dropLines;
        g_counts.dropSum += r.pending;
    } else {
        logInfo("call rejected (%s), B stays (n=%u)", r.why, r.pending);
        ++g_counts.rejectLines;
        g_counts.rejectSum += r.pending;
    }
    g_rejectPending -= r.pending;
    r.pending = 0;
    r.logged = true;
    r.lastTick = g_tick;
}

RejectSlot& rejectSlot(const char* why, bool link) {
    for (size_t i = 0; i < g_rejectUsed; ++i) {
        RejectSlot& r = g_rejects[i];
        if (r.link == link && std::strncmp(r.why, why, kWhyLen - 1) == 0) {
            return r;
        }
    }
    if (g_rejectUsed < kRejectSlots - 2) {
        RejectSlot& r = g_rejects[g_rejectUsed++];
        std::snprintf(r.why, sizeof r.why, "%s", why);
        r.link = link;
        return r;
    }
    // Sammelplatz (sollte nie noetig sein): ein Platz je Art, damit Zeilenformat und Summen stimmen.
    RejectSlot& r = g_rejects[link ? kRejectSlots - 1 : kRejectSlots - 2];
    if (r.why[0] == '\0') {
        std::snprintf(r.why, sizeof r.why, "%s", "weitere Gruende");
        r.link = link;
    }
    return r;
}

void noteReject(const char* why, bool link) {
    if (why == nullptr) {
        why = "ohne Grund";
    }
    RejectSlot& r = rejectSlot(why, link);
    ++r.pending;
    ++r.total;
    ++g_rejectPending;
    if (!r.logged || g_tick - r.lastTick >= kRejectLogTicks) {
        emitReject(r);
    }
}

// Faellige Zeilen schreiben (all: alles, fuer die Zusammenfassung).
void flushRejects(bool all) {
    if (g_rejectPending == 0) {
        return;
    }
    for (size_t i = 0; i < kRejectSlots; ++i) {
        RejectSlot& r = g_rejects[i];
        if (r.pending > 0 && (all || g_tick - r.lastTick >= kRejectLogTicks)) {
            emitReject(r);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// RK (a) und P2-04: Lock-Flaggen
// ---------------------------------------------------------------------------------------------

// Schattenzustand der Locks: was das Spiel ohne das Nullen im Ritt haette (Vorzustand fuer LRlockCheck).
uint8_t g_shadowHoldL = 0;
uint8_t g_shadowHoldR = 0;
bool g_locksZeroed = false;  // der letzte Tick hat die Lock-Flaggen genullt (Konsum im Ritt)

// Wie mDoCPd_c::LRlockCheck fuer eine Seite, aber mit dem Schatten als Vorzustand. true, wenn sich etwas aendert.
bool relock(float trigger, uint8_t prev, uint8_t& hold, uint8_t& trig) {
    const uint8_t oldHold = hold;
    const uint8_t oldTrig = trig;
    trig = 0;
    if (trigger > fapGmHIO_getLROnValue()) {
        if (prev != 1) {
            trig = 1;
        }
        hold = 1;
    } else if (trigger < fapGmHIO_getLROffValue()) {
        hold = 0;
    } else {
        hold = prev;
    }
    return hold != oldHold || trig != oldTrig;
}

// mDoCPd_c::read rechnet die Locks mit dem Vorzustand im Pad. Hat der Kern ihn im letzten Tick genullt, waere das nach
// dem Ritt eine falsche Flanke (Schildangriff, Zielsperre) oder ein verlorener Lock im Band 0,6..0,9 (P2-04). Hier wird
// mit dem Schatten neu gerechnet. Danach ist der Schatten der Lock dieses Ticks. true = Werte korrigiert.
bool repairLocks(interface_of_controller_pad& p) {
    bool changed = false;
    if (g_locksZeroed) {
        changed = relock(p.mTriggerLeft, g_shadowHoldL, p.mHoldLockL, p.mTrigLockL);
        changed = relock(p.mTriggerRight, g_shadowHoldR, p.mHoldLockR, p.mTrigLockR) || changed;
    }
    g_shadowHoldL = p.mHoldLockL;
    g_shadowHoldR = p.mHoldLockR;
    g_locksZeroed = false;
    return changed;
}

// Konsum im Ritt: A, B, X, Y, Z, L, R und das Steuerkreuz verschwinden aus dem Pad, nur die betroffenen Bits.
// Analogwerte und Lock-Flaggen von A, B, L und R werden IMMER genullt, nicht nur bei gehaltenem Digitalbit: R und L
// loesen auch analog aus (Schwelle 0.61, eingabe-r-lock.md). Ein Analogwert ohne Digitalbit liesse sonst den
// Schildangriff und die Zielsperre des Spiels durch (Teil-2-Review, Befund 1). Stick und C-Stick bleiben.
// RK (a): auch mHoldLockR wird genullt (das Spiel liest getHoldLockR sonst im ganzen Ritt). P2-04 regelt repairLocks.
void consumeRide(interface_of_controller_pad& p) {
    const uint32_t heldNow = p.mButtonFlags & kRideConsumedButtons;
    const uint32_t edgesNow = p.mPressedButtonFlags & kRideConsumedButtons;
    if (heldNow != 0) {
        ++g_counts.consumedHeld;
    }
    g_counts.consumedEdges += static_cast<uint32_t>(std::popcount(edgesNow));
    p.mButtonFlags &= ~kRideConsumedButtons;
    p.mPressedButtonFlags &= ~kRideConsumedButtons;
    p.mAnalogA = 0.0f;
    p.mAnalogB = 0.0f;
    p.mTriggerLeft = 0.0f;
    p.mTriggerRight = 0.0f;
    p.mHoldLockL = 0;
    p.mTrigLockL = 0;
    p.mHoldLockR = 0;
    p.mTrigLockR = 0;
    g_locksZeroed = true;
}

// E5: Solange ein Menue, eine Karte, ein Dialog oder ein Ereignis offen ist, sieht kein Modul die Ritt-Tasten im
// Schnappschuss (Gang ueber das Steuerkreuz, Dash mit A, Fassrolle mit Y). Der Pad selbst bleibt unveraendert, das Menue
// bekommt A, B und das Steuerkreuz. Die Sticks bleiben.
void maskUiSnapshot() {
    g_input.held &= ~kRideConsumedButtons;
    g_input.pressed &= ~kRideConsumedButtons;
    g_input.analogA = 0.0f;
    g_input.analogB = 0.0f;
    g_input.triggerL = 0.0f;
    g_input.triggerR = 0.0f;
}

// Vogel-Schnappschuss des Pad-Ticks in g_input (vor Konsum), Ruf- und Absteige-Flags des Ticks.
void captureInput(const interface_of_controller_pad& p) {
    g_input.frame = g_tick;
    g_input.held = p.mButtonFlags;
    g_input.pressed = p.mPressedButtonFlags;
    g_input.stickX = p.mMainStickPosX;
    g_input.stickY = p.mMainStickPosY;
    g_input.cStickX = p.mCStickPosX;
    g_input.cStickY = p.mCStickPosY;
    g_input.analogA = p.mAnalogA;
    g_input.analogB = p.mAnalogB;
    g_input.triggerL = p.mTriggerLeft;
    g_input.triggerR = p.mTriggerRight;
}

}  // namespace

const Input& input() {
    return g_input;
}

namespace core {

uint32_t padTick() {
    return g_tick;
}

const InputCounts& inputCounts() {
    return g_counts;
}

const char* lastRejectReason() {
    return g_lastReject;
}

// D16, UI-Eingabeblock: ein sichtbares RmlUi-Dokument blockiert die Pad-Eingabe des Spiels (chord-ui.md, 1 (b)).
// Ohne UiService gilt "nicht sichtbar".
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

// E5 (verbindlich): Solange ein Menue, eine Karte, ein Dokument, ein Dialog oder ein Ereignis offen ist, konsumiert der
// Kern B und die Tasten des Ritts nicht. B bedient dann das Menue. Gate fuer den Absteig-Tipp und den Konsum (padReadPost).
// Das Pausen-Flag deckt nur das Start-Menue ab. Item-Rad (Fenster 2), Karte (4, 11) und Untermenues (10) setzen es nicht,
// deshalb das Fenster (dMeter2Info_getWindowStatus, Werte in d_meter2_info.h). Gespraech: dMsgObject_isTalkNowCheck
// (Status des Nachrichtenobjekts ungleich 1). Das alte dComIfGp_getMesgStatus() wird nie geschrieben und entfaellt.
bool uiBlocksRideInput() {
    return dComIfGp_isPauseFlag() != 0 || dMeter2Info_getWindowStatus() != 0 || dMsgObject_isTalkNowCheck() ||
        dComIfGp_event_runCheck() != 0 || dComIfGp_isEnableNextStage() != 0 || uiDocumentVisible();
}

// Der Pad-Hook hat B fuer den Ruf verschluckt. Wird der Ruf erst am Link-Frame verworfen, geht B an das Spiel
// zurueck (D7: ein abgelehnter Ruf behaelt B). Gilt nur fuer den Tick, in dem callRequested gesetzt war.
void restoreChordB(const char* why) {
    interface_of_controller_pad& p = mDoCPd_c::getCpadInfo(PAD_1);
    // Das Bit, das der Ruf verschluckt hat: der Ruf-Tipp des letzten angenommenen Rufs. Ohne Ruf die aktuelle Belegung.
    const uint32_t tipp = g_chordBit != 0 ? g_chordBit : actions::bit(actions::ActId::CallTap);
    p.mPressedButtonFlags |= tipp;
    p.mButtonFlags |= tipp;
    ++g_counts.chordRestored;
    noteReject(why, true);
}

bool callRequestFresh() {
    return g_callTick == g_tick;
}

void dropStaleCall() {
    ++g_counts.staleDropped;
    logEvery(g_logStale, 3, 60, "call request from pad tick %u dropped at link frame (pad tick now %u), no call",
        g_callTick, g_tick);
}

void logInputSummary() {
    flushRejects(true);
    for (size_t i = 0; i < kRejectSlots; ++i) {
        const RejectSlot& r = g_rejects[i];
        if (r.total > 0) {
            logInfo("INPUT reason %s (%s) total=%u", r.link ? "chord dropped" : "call rejected", r.why, r.total);
        }
    }
    logInfo("INPUT rejected=%u rejectLines=%u rejectSum=%u restored=%u dropLines=%u dropSum=%u latchDropped=%u "
            "staleDropped=%u lockRepairs=%u",
        g_counts.callRejected, g_counts.rejectLines, g_counts.rejectSum, g_counts.chordRestored, g_counts.dropLines,
        g_counts.dropSum, g_counts.latchDropped, g_counts.staleDropped, g_counts.lockRepairs);
}

void padReadPost(ModContext*, void*, void*, void*) {
    // D8: Ruf und Absteigen gelten nur fuer den Tick, in dem sie entstanden (kein Latch). Zuerst zuruecksetzen, damit auch
    // eine Ausnahme weiter unten keinen Wunsch in einen spaeteren Link- oder Vogel-Frame traegt.
    Session& s = session();
    const bool callLatched = s.callRequested;
    s.callRequested = false;
    s.dismountRequested = false;
    try {
        interface_of_controller_pad& p = mDoCPd_c::getCpadInfo(PAD_1);
#if defined(KFF_DEV_AUTOTEST)
        autotest::inject(p);
#endif
        ++g_tick;
        // RK (a)/P2-04: Lock-Flaggen nach einem Tick mit Konsum mit dem Schattenzustand neu rechnen.
        const bool locksRepaired = repairLocks(p);
        // ride.cpp: angefordertes Entfernen des Vogels (auch ohne Link und ohne Sitzung), Abflugfrist in Pad-Ticks (D-B),
        // Einordnung des Szenenwechsels gilt nur im selben Tick (D1).
        ridePadTick();
        // Schnappschuss VOR dem Konsum: alles, was Module und die Ruf-Entscheidung lesen.
        captureInput(p);
        // Belegung der Flugtasten (E-7): eine Aenderung wirkt ab diesem Tick. Ohne ConfigService bleibt die Belegung.
        actions::refresh();
        // E5: einmal je Tick, fuer den Konsum und fuer die Module (Schnappschuss).
        const bool uiBlocked = uiBlocksRideInput();
        g_input.uiBlocked = uiBlocked;
        g_input.uiDocument = uiDocumentVisible();

        // P2-11/D8: ein Ruf, den kein Link-Frame verarbeitet hat, gibt B an das Spiel zurueck (D7). Nach dem Schnappschuss,
        // damit das zurueckgegebene B keine neue Ruf-Kombination ausloest.
        if (callLatched) {
            ++g_counts.latchDropped;
            restoreChordB("kein Link-Frame seit dem Ruf");
        }

        // Ruf-Kombination: Ruf halten (Vorgabe R) und Ruf tippen (Vorgabe B), beide frei belegbar (E-10). Der Tipp wird nur
        // verschluckt, wenn der Ruf wirklich angenommen wird. Absteigen ist ebenfalls frei belegbar (Vorgabe B).
        bool chordTaken = false;
        const uint32_t holdBit = actions::bit(actions::ActId::CallHold);
        const uint32_t tippBit = actions::bit(actions::ActId::CallTap);
        const uint32_t dismountBit = actions::bit(actions::ActId::Dismount);
        // R gilt als gehalten bei Digitalbit oder Analogwert f >= 0,61. f ist mTriggerRight = min(1, Post/140); auf den
        // Achsweg umgerechnet sind das ca. 45 % (ps-controller.md, Entscheidung 10). Die Spiel-Locks liegen bei 0,9 an
        // und 0,6 aus (eingabe-r-lock.md).
        // Ruf halten auf L oder R nutzt dieselbe Analogschwelle (E-6). Vorgabe R: wie bisher.
        const bool rHeld = g_input.isHeld(holdBit) || actions::analogAbove(holdBit, 0.61f);
        if (rHeld && g_input.isPressed(tippBit)) {
            if (const char* why = rideCallBlockReason(liveLink())) {
                ++g_counts.callRejected;
                std::snprintf(g_lastReject, sizeof g_lastReject, "%s", why);
                noteReject(why, false);  // F02: Grund ratenbegrenzt, n in der Zeile
            } else {
                s.callRequested = true;
                g_callTick = g_tick;
                chordTaken = true;
                ++g_counts.chordConsumed;
                g_chordBit = tippBit;  // D8/E-11: restoreChordB gibt genau dieses Bit zurueck
                p.mPressedButtonFlags &= ~tippBit;
                p.mButtonFlags &= ~tippBit;
            }
        }

        // Im Ritt: B-Tipp steigt ab, dann Konsum der Tasten. E5: bei offenem Menue, Karte, Dokument, Dialog oder Ereignis
        // bleibt B beim Spiel (uiBlocked).
        bool consumed = false;
        if (s.phase == Phase::Riding && !uiBlocked) {
            if (!chordTaken && g_input.isPressed(dismountBit)) {
                s.dismountRequested = true;
            }
            consumeRide(p);
            consumed = true;
        } else if (uiBlocked) {
            maskUiSnapshot();  // E5: Menue, Karte, Dialog oder Ereignis offen, Module sehen keine Ritt-Tasten
        }
        if (locksRepaired && !consumed) {
            // Erster Tick nach dem Konsum (Ritt-Ende oder Menue im Ritt): ohne Reparatur saehe das Spiel hier die falsche Flanke.
            ++g_counts.lockRepairs;
            logEvery(g_logRepair, 5, 100, "lock flags repaired after ride input consumption: holdL=%u trigL=%u holdR=%u "
                                          "trigR=%u (P2-04)",
                static_cast<unsigned>(p.mHoldLockL), static_cast<unsigned>(p.mTrigLockL),
                static_cast<unsigned>(p.mHoldLockR), static_cast<unsigned>(p.mTrigLockR));
        }
        flushRejects(false);  // F02: faellige Sammelzeilen
#if defined(KFF_DEV_AUTOTEST)
        autotest::observe(p);  // Testbuild: Endzustand des Pads dieses Ticks (Lock-Nachbildung, Messungen)
#endif
    } catch (...) {
        logEvery(coreExceptionCount(), 3, 300, "exception in mDoCPd_c::read (frame skipped)");
    }
}

}  // namespace core
}  // namespace kff
