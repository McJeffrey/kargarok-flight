// Reine Tests der Belegung (T1, nur Testbau): Regeln R1 bis R3, Aufloesung, Aenderung nach E-5 und Invarianten ueber
// Zufallsbelegungen. Kein Spiel, keine Konfiguration. Aufruf beim Start (steuerung::registerTestScenarios), die Zahl
// der fehlgeschlagenen Pruefungen steht in der Logzeile "steuerung: reine Tests ...".
#include "core/actions.hpp"

#include "core/kff/actions.hpp"
#include "core/kff/input.hpp"
#include "core/kff/log.hpp"

#include <cstdint>
#include <string>

namespace actions {
namespace {

int g_checks = 0;
int g_fails = 0;

void check(bool ok, const char* was) {
    ++g_checks;
    if (!ok) {
        ++g_fails;
        kff::logWarn("steuerung: reine Tests FEHLER: %s", was);
    }
}

bool eq(const Belegung& a, const Belegung& b) {
    for (size_t i = 0; i < kActCount; ++i) {
        if (a.idx[i] != b.idx[i]) {
            return false;
        }
    }
    return true;
}

Belegung mit(Belegung b, ActId a, uint8_t v) {
    b.idx[static_cast<size_t>(a)] = v;
    return b;
}

// Deterministischer Zufall (kein Standardbibliotheks-Generator, damit der Lauf gleich bleibt).
struct Rng {
    uint32_t s;
    uint32_t next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    }
};

// Vorgaben entsprechen dem heutigen Verhalten (E-11): Ruf R, Ruf tippen B, Absteigen B, Dash A, Fassrolle Y, Bremse Z,
// Kamera X, Kamera naeher und weiter an den Steuerkreuz-Tasten.
void testVorgabe() {
    const Belegung d = defaults();
    Verstoss v{};
    check(pruefen(d, &v), "Vorgabe erfuellt die Regeln");
    Fall fall[kActCount];
    const Belegung e = aufloesen(d, fall);
    check(eq(e, d), "aufloesen(Vorgabe) ist die Vorgabe");
    bool ohneFall = true;
    for (size_t i = 0; i < kActCount; ++i) {
        ohneFall = ohneFall && fall[i] == Fall::Keiner;
    }
    check(ohneFall, "Vorgabe ohne Fallcode");
    struct Soll {
        ActId a;
        uint32_t bit;
    };
    const Soll soll[] = {
        {ActId::CallHold, static_cast<uint32_t>(PAD_TRIGGER_R)},
        {ActId::CallTap, static_cast<uint32_t>(PAD_BUTTON_B)},
        {ActId::Dismount, static_cast<uint32_t>(PAD_BUTTON_B)},
        {ActId::Dash, static_cast<uint32_t>(PAD_BUTTON_A)},
        {ActId::GearUp, static_cast<uint32_t>(PAD_BUTTON_UP)},
        {ActId::GearDown, static_cast<uint32_t>(PAD_BUTTON_DOWN)},
        {ActId::Brake, static_cast<uint32_t>(PAD_TRIGGER_Z)},
        {ActId::BarrelRoll, static_cast<uint32_t>(PAD_BUTTON_Y)},
        {ActId::CameraBehind, static_cast<uint32_t>(PAD_BUTTON_X)},
        {ActId::CameraCloser, static_cast<uint32_t>(PAD_BUTTON_RIGHT)},
        {ActId::CameraFarther, static_cast<uint32_t>(PAD_BUTTON_LEFT)},
        {ActId::Faster, static_cast<uint32_t>(PAD_TRIGGER_R)},
        {ActId::Slower, static_cast<uint32_t>(PAD_TRIGGER_L)},
    };
    for (const Soll& s : soll) {
        check(kff::kBindable[d.idx[static_cast<size_t>(s.a)]].bit == s.bit, "Vorgabe-Taste stimmt mit dem Code ueberein");
    }
    check(kff::bindableMask() == kff::kRideConsumedButtons, "Maske der belegbaren Tasten == Konsum-Maske des Rittes");
    check(kff::kBindableCount == 12, "kBindable hat 12 Eintraege");
    check(mandatory(ActId::CallHold) && mandatory(ActId::CallTap) && mandatory(ActId::Dismount), "Pflichtaktionen (E-4)");
    check(!mandatory(ActId::Dash) && !mandatory(ActId::Brake), "uebrige Aktionen duerfen Not set sein");
}

// R1 bis R3: fuer jedes Paar (a, b) gilt, dass b die Taste von a bekommt. Erlaubt nur fuer {Ruf tippen, Absteigen}.
void testPaare() {
    const Belegung d = defaults();
    for (size_t p = 0; p < kActCount; ++p) {
        for (size_t q = 0; q < kActCount; ++q) {
            if (p == q) {
                continue;
            }
            const ActId a = static_cast<ActId>(p);
            const ActId b = static_cast<ActId>(q);
            const Belegung x = mit(d, b, d.idx[p]);
            check(pruefen(x, nullptr) == shareAllowed(a, b), "Paarregel: nur Ruf tippen und Absteigen teilen");
        }
    }
    check(!shareAllowed(ActId::CallHold, ActId::CallTap), "Ruf halten teilt nicht");
    check(!shareAllowed(ActId::Dash, ActId::Dismount), "Dash teilt nicht mit Absteigen");
    Verstoss v{};
    check(!pruefen(mit(d, ActId::Dismount, 0), &v) && v.leer && v.a == ActId::Dismount, "Pflicht ohne Taste (E-4)");
}

void testAufloesen() {
    const Belegung d = defaults();
    Fall fall[kActCount];
    // S5: Dash = Y bei Fassrolle = Y. Fassrolle behaelt Y, Dash faellt auf A.
    Belegung e = aufloesen(mit(d, ActId::Dash, 4), fall);
    check(e.idx[static_cast<size_t>(ActId::Dash)] == 1 && e.idx[static_cast<size_t>(ActId::BarrelRoll)] == 4,
        "S5-Regel: Dash faellt auf A, Fassrolle bleibt Y");
    check(fall[static_cast<size_t>(ActId::Dash)] == Fall::KonfliktVorgabe, "S5: Fallcode Konflikt mit Vorgabe");
    // Pflicht ohne Taste: Absteigen faellt auf B.
    e = aufloesen(mit(d, ActId::Dismount, 0), fall);
    check(e.idx[static_cast<size_t>(ActId::Dismount)] == 2, "Absteigen ohne Taste -> Vorgabe B");
    check(fall[static_cast<size_t>(ActId::Dismount)] == Fall::PflichtVorgabe, "Absteigen: Fallcode Pflicht");
    // Ausserhalb der Tabelle: Vorgabe.
    e = aufloesen(mit(d, ActId::Dash, 42), fall);
    check(e.idx[static_cast<size_t>(ActId::Dash)] == 1 && fall[static_cast<size_t>(ActId::Dash)] == Fall::Bereich,
        "Index 42 -> Vorgabe A");
    // Ruf halten ohne Taste: Pflichtaktion, Vorgabe R. R teilt Ruf halten mit Schneller (erlaubt), also kein Konflikt.
    e = aufloesen(mit(d, ActId::CallHold, 0), fall);
    check(e.idx[static_cast<size_t>(ActId::CallHold)] == 7 &&
        fall[static_cast<size_t>(ActId::CallHold)] == Fall::PflichtVorgabe,
        "Ruf halten ohne Taste -> Vorgabe R (teilt R mit Schneller)");
    // Absteigen auf L: L hat Langsamer (Vorgabe), das teilt Absteigen nicht, also faellt Absteigen auf die Vorgabe B.
    e = aufloesen(mit(d, ActId::Dismount, 6), fall);
    check(e.idx[static_cast<size_t>(ActId::Dismount)] == 2 &&
        fall[static_cast<size_t>(ActId::Dismount)] == Fall::KonfliktVorgabe, "Absteigen auf L -> Vorgabe B");
    check(pruefen(e, nullptr), "Ersatz: Ergebnis erfuellt die Regeln");
    // Ruf tippen auf B, obwohl Dash B hat: Dash weicht nicht, Ruf tippen fuehrt die Vorgabe B weiter.
    e = aufloesen(mit(d, ActId::Dash, 2), fall);
    check(e.idx[static_cast<size_t>(ActId::Dash)] == 1, "Dash mit doppeltem B faellt auf A");
    // Gueltige Belegung bleibt unveraendert.
    const Belegung g = mit(mit(d, ActId::Slower, 0), ActId::Brake, 6);  // Bremse auf L, Langsamer ohne Taste
    check(eq(aufloesen(g, fall), g), "gueltige Belegung bleibt");
}

void testAenderung() {
    const Belegung d = defaults();
    const size_t dash = static_cast<size_t>(ActId::Dash);
    const size_t fass = static_cast<size_t>(ActId::BarrelRoll);
    const size_t tipp = static_cast<size_t>(ActId::CallTap);
    const size_t absteig = static_cast<size_t>(ActId::Dismount);

    Aenderung r = applyChange(d, ActId::Dash, 4);
    check(r.ergebnis == Ergebnis::Getauscht && r.neu.idx[dash] == 4 && r.neu.idx[fass] == 1, "Tausch Dash auf Y");
    check(r.text == "Swapped: Dash is now on Y. Barrel roll is now on A.", "Text Tausch Dash");

    r = applyChange(d, ActId::BarrelRoll, 1);
    check(r.ergebnis == Ergebnis::Getauscht && r.neu.idx[fass] == 1 && r.neu.idx[dash] == 4, "Tausch Fassrolle auf A");
    check(r.text == "Swapped: Barrel roll is now on A. Dash is now on Y.", "Text Tausch Fassrolle (E-5-Beispiel)");

    r = applyChange(d, ActId::Dismount, 0);
    check(r.ergebnis == Ergebnis::Abgelehnt && eq(r.neu, d), "Absteigen auf Not set: abgelehnt");
    check(r.text == "Not possible: Dismount needs a button.", "Text Pflicht ohne Taste");

    r = applyChange(d, ActId::CallHold, 0);
    check(r.ergebnis == Ergebnis::Abgelehnt, "Ruf halten auf Not set: abgelehnt");

    // Ruf tippen auf A: A ist Dash, und Dash bekaeme B, dort steht Absteigen.
    r = applyChange(d, ActId::CallTap, 1);
    check(r.ergebnis == Ergebnis::Abgelehnt && eq(r.neu, d), "Ruf tippen auf Ritt-Taste A: abgelehnt");
    check(r.text == "Not possible: B is also used by Dismount. Change Dismount first.", "Text Ruf tippen auf A");

    // Dash auf B: B haben Ruf tippen und Absteigen, beide duerfen nicht zu Dash.
    r = applyChange(d, ActId::Dash, 2);
    check(r.ergebnis == Ergebnis::Abgelehnt && eq(r.neu, d), "Dash auf B: abgelehnt (zwei Partner)");
    check(r.text == "Not possible: B is also used by Call: then tap and Dismount. Change them first.", "Text Dash auf B");

    // R haben Ruf halten und Schneller (beide ohne Teilen mit Dash): Dash auf R wird abgelehnt, beide muessen zuerst weg.
    const Belegung ohne = mit(d, ActId::Dash, 0);
    r = applyChange(ohne, ActId::Dash, 7);
    check(r.ergebnis == Ergebnis::Abgelehnt, "Dash auf R: abgelehnt (Ruf halten und Schneller)");
    check(r.text == "Not possible: R is also used by Call: hold and Faster (hold). Change them first.", "Text Dash auf R");

    r = applyChange(d, ActId::Dash, 0);
    check(r.ergebnis == Ergebnis::Gesetzt && r.neu.idx[dash] == 0, "Dash auf Not set");
    check(r.text == "Dash is now not set.", "Text Not set");

    r = applyChange(d, ActId::Dash, 1);
    check(r.ergebnis == Ergebnis::Unveraendert && eq(r.neu, d), "gleiche Taste: unveraendert");

    r = applyChange(d, ActId::Dash, 42);
    check(r.ergebnis == Ergebnis::Abgelehnt && eq(r.neu, d), "Index 42: abgelehnt");

    r = applyChange(d, ActId::Brake, 7);
    check(r.ergebnis == Ergebnis::Abgelehnt && eq(r.neu, d), "Bremse auf R: abgelehnt (Ruf halten und Schneller)");
    r = applyChange(d, ActId::Brake, 6);
    check(r.ergebnis == Ergebnis::Getauscht && r.neu.idx[static_cast<size_t>(ActId::Slower)] == 5,
        "Bremse auf L: Langsamer bekommt Z");
    check(r.text == "Swapped: Brake (hold) is now on L. Slower (hold) is now on Z.", "Text Bremse auf L");

    r = applyChange(d, ActId::Dash, 3);
    check(r.ergebnis == Ergebnis::Getauscht && r.neu.idx[tipp] == 2 && r.neu.idx[absteig] == 2,
        "Dash auf X: X ist Kamera hinten, Tausch");
}

// Zufall: jede aufgeloeste Belegung erfuellt die Regeln, aufloesen ist idempotent, und jede Aenderung ist entweder
// abgelehnt (Belegung unveraendert) oder gueltig.
void testZufall() {
    Rng rng{987654u};
    for (int k = 0; k < 300; ++k) {
        Belegung roh = {};
        for (size_t i = 0; i < kActCount; ++i) {
            roh.idx[i] = static_cast<uint8_t>(rng.next() % 14);
        }
        Fall fall[kActCount];
        const Belegung eff = aufloesen(roh, fall);
        check(pruefen(eff, nullptr), "Zufall: aufloesen liefert eine gueltige Belegung");
        Fall f2[kActCount];
        check(eq(aufloesen(eff, f2), eff), "Zufall: aufloesen ist idempotent");
        for (size_t a = 0; a < kActCount; ++a) {
            for (uint8_t w = 0; w < 12; ++w) {
                const Aenderung r = applyChange(eff, static_cast<ActId>(a), w);
                if (r.ergebnis == Ergebnis::Abgelehnt) {
                    check(eq(r.neu, eff), "Zufall: abgelehnt laesst die Belegung unveraendert");
                } else {
                    check(pruefen(r.neu, nullptr), "Zufall: angenommene Aenderung ist gueltig");
                }
            }
        }
    }
}

// K1: Auswahlliste je Aktion. Pflichtaktionen haben 11 Optionen (A bis D-Pad Right) ohne "Not set". Die uebrigen haben
// 12 Optionen mit "Not set" an Position 0. Die Abbildung zwischen Auswahl und Belegung ist umkehrbar.
void testOptionen() {
    for (size_t p = 0; p < kActCount; ++p) {
        const ActId a = static_cast<ActId>(p);
        const int n = optionCount(a);
        check(n == (mandatory(a) ? 11 : 12), "Anzahl der Auswahl-Optionen (K1)");
        bool notSet = false;
        for (int i = 0; i < n; ++i) {
            notSet = notSet || std::string(kff::kBindable[bindIndexOf(a, i)].label) == "Not set";
        }
        check(notSet == !mandatory(a), "'Not set' nur bei den uebrigen Aktionen (K1)");
        check(std::string(keyLabel(static_cast<uint8_t>(bindIndexOf(a, 0)))) == (mandatory(a) ? "A" : "Not set"),
            "erste Option: A bei Pflichtaktionen, sonst Not set (K1)");
        check(std::string(keyLabel(static_cast<uint8_t>(bindIndexOf(a, n - 1)))) == "D-Pad Right",
            "letzte Option: D-Pad Right (K1)");
        bool umkehrbar = true;
        for (int b = mandatory(a) ? 1 : 0; b < static_cast<int>(kff::kBindableCount); ++b) {
            const int ui = uiIndexOf(a, static_cast<uint8_t>(b));
            umkehrbar = umkehrbar && ui == (mandatory(a) ? b - 1 : b) && bindIndexOf(a, ui) == b;
        }
        check(umkehrbar, "Abbildung Auswahl <-> Belegung ist umkehrbar (K1)");
    }
}

}  // namespace

int runPureTests() {
    g_checks = 0;
    g_fails = 0;
    testVorgabe();
    testPaare();
    testAufloesen();
    testAenderung();
    testZufall();
    testOptionen();
    kff::logInfo("steuerung: reine Tests Pruefungen=%d Fehler=%d", g_checks, g_fails);
    return g_fails;
}

}  // namespace actions
