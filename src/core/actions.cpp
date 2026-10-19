// Belegung der Flugaktionen: Regeln (rein), Aufloesung, Aenderung (E-5) und Zugriff fuer den Kern.
// Die reinen Regeln oben kennen weder Spiel noch Konfiguration und werden in core/actions_test.cpp geprueft.
// Logzeilen auf Deutsch (E-1), Statustexte fuer das Fenster auf Englisch.
#include "core/actions.hpp"

#include "core/kff/actions.hpp"
#include "core/kff/config.hpp"
#include "core/kff/input.hpp"
#include "core/kff/log.hpp"

#include <string>

namespace actions {
namespace {

constexpr uint8_t kBindCount = static_cast<uint8_t>(kff::kBindableCount);  // 12: "Not set" plus 11 Tasten

// Vorgabe = heutige Belegung (E-11), Indizes in kff::kBindable: R 7, B 2, B 2, A 1, Steuerkreuz hoch 8, runter 9,
// Z 5, Y 4, X 3, Steuerkreuz rechts 11, links 10. Ruf tippen und Absteigen teilen sich B (R2). Version 2.4.0: Schneller
// R 7 (teilt R mit Ruf halten), Langsamer L 6.
constexpr Belegung kDefaults = {{7, 2, 2, 1, 8, 9, 5, 4, 3, 11, 10, 7, 6}};

// Reihenfolge der Aufloesung (Entwurf 2.3, siehe BERICHT): Absteigen, Ruf tippen, Ruf halten, dann die Flugaktionen.
constexpr ActId kPrio[kActCount] = {ActId::Dismount, ActId::CallTap, ActId::CallHold, ActId::Dash, ActId::GearUp,
    ActId::GearDown, ActId::Brake, ActId::BarrelRoll, ActId::CameraBehind, ActId::CameraCloser, ActId::CameraFarther,
    ActId::Faster, ActId::Slower};

constexpr kff::config::Key kKeys[kActCount] = {kff::config::Key::BindCallHold, kff::config::Key::BindCallTap,
    kff::config::Key::BindDismount, kff::config::Key::BindDash, kff::config::Key::BindGearUp,
    kff::config::Key::BindGearDown, kff::config::Key::BindBrake, kff::config::Key::BindBarrelRoll,
    kff::config::Key::BindCameraBehind, kff::config::Key::BindCameraCloser, kff::config::Key::BindCameraFarther,
    kff::config::Key::BindFaster, kff::config::Key::BindSlower};

// Wirksame Belegung: zuletzt aufgeloest. Vor dem Laden die Vorgabe.
Belegung g_eff = kDefaults;

size_t at(ActId a) {
    return static_cast<size_t>(a);
}

// Vertraeglich: keine andere bereits angenommene Aktion hat dieselbe Taste, ausser der erlaubten Paarung (R2).
bool vertraeglich(ActId a, uint8_t v, const Belegung& eff, const bool* acc) {
    for (size_t o = 0; o < kActCount; ++o) {
        if (o != at(a) && acc[o] && eff.idx[o] == v && !shareAllowed(a, static_cast<ActId>(o))) {
            return false;
        }
    }
    return true;
}

// Ersatz fuer eine Pflichtaktion ohne passende Vorgabe: erster Index 1..11, mit dem sie vertraeglich ist.
uint8_t ersatz(ActId a, const Belegung& eff, const bool* acc) {
    for (uint8_t v = 1; v < kBindCount; ++v) {
        if (vertraeglich(a, v, eff, acc)) {
            return v;
        }
    }
    return defaultIndex(a);  // nicht erreichbar: elf Tasten reichen fuer zehn verschiedene Aktionen
}

// "A", "A and B", "A, B and C" fuer Statustexte.
std::string joinNames(const ActId* list, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
            s += (i + 1 == n) ? " and " : ", ";
        }
        s += labelEn(list[i]);
    }
    return s;
}

// "on A" oder "not set" fuer Statustexte.
std::string onText(uint8_t index) {
    return index == 0 ? std::string("not set") : std::string("on ") + keyLabel(index);
}

}  // namespace

// ---- Regeln (rein) -----------------------------------------------------------------------------------------------

bool mandatory(ActId a) {
    return a == ActId::CallHold || a == ActId::CallTap || a == ActId::Dismount;
}

// Erlaubt: Ruf tippen und Absteigen (B, R2). Ruf halten und Schneller teilen R (Vorgabe, Version 2.4.0): R allein
// gehalten ist Gas, R gehalten plus B getippt ist der Ruf.
bool shareAllowed(ActId a, ActId b) {
    if ((a == ActId::CallTap && b == ActId::Dismount) || (a == ActId::Dismount && b == ActId::CallTap)) {
        return true;
    }
    return (a == ActId::CallHold && b == ActId::Faster) || (a == ActId::Faster && b == ActId::CallHold);
}

uint8_t defaultIndex(ActId a) {
    return kDefaults.idx[at(a)];
}

Belegung defaults() {
    return kDefaults;
}

// Statusname (Englisch, E-1): so heisst die Aktion in den Statuszeilen des Fensters.
const char* labelEn(ActId a) {
    switch (a) {
    // K12: dieselben Namen wie die Zeilen im Fenster (steuerung.cpp, kFlying/kCamera/kCall).
    case ActId::CallHold: return "Call: hold";
    case ActId::CallTap: return "Call: then tap";
    case ActId::Dismount: return "Dismount";
    case ActId::Dash: return "Dash";
    case ActId::GearUp: return "Speed up (tap)";
    case ActId::GearDown: return "Speed down (tap)";
    case ActId::Brake: return "Brake (hold)";
    case ActId::BarrelRoll: return "Barrel roll";
    case ActId::CameraBehind: return "Camera behind bird";
    case ActId::CameraCloser: return "Camera closer (hold)";
    case ActId::CameraFarther: return "Camera farther (hold)";
    case ActId::Faster: return "Faster (hold)";
    case ActId::Slower: return "Slower (hold)";
    case ActId::Count: break;
    }
    return "";
}

// Name in den Logzeilen (Deutsch, ohne Leerzeichen).
const char* nameDe(ActId a) {
    switch (a) {
    case ActId::CallHold: return "RufHalten";
    case ActId::CallTap: return "RufTippen";
    case ActId::Dismount: return "Absteigen";
    case ActId::Dash: return "Dash";
    case ActId::GearUp: return "GangHoch";
    case ActId::GearDown: return "GangRunter";
    case ActId::Brake: return "Bremse";
    case ActId::BarrelRoll: return "Fassrolle";
    case ActId::CameraBehind: return "KameraHinten";
    case ActId::CameraCloser: return "KameraNaeher";
    case ActId::CameraFarther: return "KameraWeiter";
    case ActId::Faster: return "Schneller";
    case ActId::Slower: return "Langsamer";
    case ActId::Count: break;
    }
    return "";
}

const char* configKey(ActId a) {
    switch (a) {
    case ActId::CallHold: return "bind_ruf_halten";
    case ActId::CallTap: return "bind_ruf_tipp";
    case ActId::Dismount: return "bind_absteigen";
    case ActId::Dash: return "bind_dash";
    case ActId::GearUp: return "bind_gang_hoch";
    case ActId::GearDown: return "bind_gang_runter";
    case ActId::Brake: return "bind_bremse";
    case ActId::BarrelRoll: return "bind_fassrolle";
    case ActId::CameraBehind: return "bind_kamera_hinten";
    case ActId::CameraCloser: return "bind_kamera_naeher";
    case ActId::CameraFarther: return "bind_kamera_weiter";
    case ActId::Faster: return "bind_schneller";
    case ActId::Slower: return "bind_langsamer";
    case ActId::Count: break;
    }
    return "";
}

const char* keyLabel(uint8_t index) {
    return index < kBindCount ? kff::kBindable[index].label : "?";
}

int optionCount(ActId a) {
    return mandatory(a) ? static_cast<int>(kBindCount) - 1 : static_cast<int>(kBindCount);
}

int uiIndexOf(ActId a, uint8_t bindIdx) {
    if (!mandatory(a)) {
        return bindIdx;
    }
    return bindIdx == 0 ? 0 : bindIdx - 1;  // Pflicht hat nie 0 (E-4); der Fall ist nur ein Schutz
}

int bindIndexOf(ActId a, int uiIdx) {
    return mandatory(a) ? uiIdx + 1 : uiIdx;
}

bool pruefen(const Belegung& b, Verstoss* v) {
    for (size_t i = 0; i < kActCount; ++i) {
        const ActId a = static_cast<ActId>(i);
        if (mandatory(a) && b.idx[i] == 0) {
            if (v != nullptr) {
                *v = Verstoss{true, a, a};
            }
            return false;
        }
    }
    for (size_t p = 0; p < kActCount; ++p) {
        for (size_t q = p + 1; q < kActCount; ++q) {
            const ActId a = static_cast<ActId>(p);
            const ActId c = static_cast<ActId>(q);
            if (b.idx[p] != 0 && b.idx[p] == b.idx[q] && !shareAllowed(a, c)) {
                if (v != nullptr) {
                    *v = Verstoss{false, a, c};
                }
                return false;
            }
        }
    }
    return true;
}

// Aufloesung (Entwurf 2.3): Stufe 1 nimmt alle Aktionen mit ihrer Vorgabe. Stufe 2 geht die uebrigen in fester
// Reihenfolge durch: Rohwert, sonst Vorgabe, sonst (nur Pflicht) ein freier Index, sonst ohne Taste.
Belegung aufloesen(const Belegung& roh, Fall* fall) {
    Belegung eff = roh;
    bool acc[kActCount] = {};
    for (size_t i = 0; i < kActCount; ++i) {
        fall[i] = Fall::Keiner;
    }
    for (size_t i = 0; i < kActCount; ++i) {
        if (roh.idx[i] == kDefaults.idx[i]) {
            acc[i] = true;
        }
    }
    for (ActId a : kPrio) {
        const size_t i = at(a);
        if (acc[i]) {
            continue;
        }
        const uint8_t v0 = roh.idx[i];
        const bool rawOk = v0 < kBindCount && (v0 != 0 || !mandatory(a)) && (v0 == 0 || vertraeglich(a, v0, eff, acc));
        if (rawOk) {
            eff.idx[i] = v0;
            acc[i] = true;
            continue;
        }
        fall[i] = v0 >= kBindCount ? Fall::Bereich : (v0 == 0 ? Fall::PflichtVorgabe : Fall::KonfliktVorgabe);
        const uint8_t vd = defaultIndex(a);
        if (vertraeglich(a, vd, eff, acc)) {
            eff.idx[i] = vd;
        } else {
            fall[i] = Fall::KonfliktLeer;
            eff.idx[i] = mandatory(a) ? ersatz(a, eff, acc) : 0;
        }
        acc[i] = true;
    }
    return eff;
}

// Aenderung (E-5). Die Tasten werden nur getauscht, wenn genau eine andere Aktion die neue Taste hat und die beiden
// sie nicht teilen duerfen. Verletzt das Ergebnis eine Regel, bleibt der alte Wert.
Aenderung applyChange(const Belegung& b, ActId a, uint8_t wert) {
    Aenderung r;
    r.neu = b;
    r.ergebnis = Ergebnis::Abgelehnt;
    const size_t ia = at(a);
    if (wert >= kBindCount) {
        r.text = "Not possible: unknown button.";
        return r;
    }
    if (wert == b.idx[ia]) {
        r.ergebnis = Ergebnis::Unveraendert;
        r.text = std::string(labelEn(a)) + " is already " + onText(wert) + ".";
        return r;
    }
    if (wert == 0 && mandatory(a)) {
        r.text = std::string("Not possible: ") + labelEn(a) + " needs a button.";
        return r;
    }
    // Aktionen, die die neue Taste schon haben und sie nicht teilen duerfen.
    ActId kollision[kActCount] = {};
    size_t nk = 0;
    if (wert != 0) {
        for (size_t o = 0; o < kActCount; ++o) {
            if (o != ia && b.idx[o] == wert && !shareAllowed(a, static_cast<ActId>(o))) {
                kollision[nk++] = static_cast<ActId>(o);
            }
        }
    }
    if (nk > 1) {
        r.text = std::string("Not possible: ") + keyLabel(wert) + " is also used by " + joinNames(kollision, nk) +
            ". Change them first.";
        return r;
    }
    Belegung nb = b;
    nb.idx[ia] = wert;
    if (nk == 1) {
        nb.idx[at(kollision[0])] = b.idx[ia];  // Tausch: die andere Aktion bekommt die bisherige Taste
    }
    Verstoss v{false, a, a};
    if (!pruefen(nb, &v)) {
        if (v.leer) {
            // Die Tauschpartnerin wuerde ohne Taste, obwohl sie Pflicht ist: erst umbelegen lassen.
            r.text = std::string("Not possible: ") + keyLabel(wert) + " is also used by " + labelEn(v.a) +
                ". Change " + labelEn(v.a) + " first.";
        } else {
            // Die Partnerin bekommt die bisherige Taste der Aenderung und trifft dort eine dritte Aktion.
            const ActId partner = nk == 1 ? kollision[0] : a;
            const ActId blocker = v.a == partner ? v.b : v.a;
            r.text = std::string("Not possible: ") + keyLabel(nb.idx[at(v.a)]) + " is also used by " +
                labelEn(blocker) + ". Change " + labelEn(blocker) + " first.";
        }
        return r;
    }
    r.neu = nb;
    if (nk == 0) {
        r.ergebnis = Ergebnis::Gesetzt;
        r.text = std::string(labelEn(a)) + " is now " + onText(wert) + ".";
    } else {
        r.ergebnis = Ergebnis::Getauscht;
        r.text = std::string("Swapped: ") + labelEn(a) + " is now " + onText(wert) + ". " + labelEn(kollision[0]) +
            " is now " + onText(b.idx[ia]) + ".";
    }
    return r;
}

// ---- Konfigzugriff und Laufzeit ----------------------------------------------------------------------------------

namespace {

bool haveConfig() {
    return kff::config::handleOf(kff::config::Key::BindCallHold) != 0;
}

// Rohwerte aus der Konfiguration. getInt wirkt strict: ein ungueltiger Wert kommt als Vorgabe an (E-10).
Belegung readRaw() {
    Belegung r = {};
    for (size_t i = 0; i < kActCount; ++i) {
        r.idx[i] = static_cast<uint8_t>(kff::config::getInt(kKeys[i]));
    }
    return r;
}

// Eine Zeile mit der wirksamen Belegung: "(Vorgabe)" oder "(N geaendert)".
void logBelegung() {
    std::string s = "steuerung: Belegung";
    for (size_t i = 0; i < kActCount; ++i) {
        s += ' ';
        s += nameDe(static_cast<ActId>(i));
        s += '=';
        s += keyLabel(g_eff.idx[i]);
    }
    const int n = changedCount();
    s += n == 0 ? std::string(" (Vorgabe)") : " (" + std::to_string(n) + " geaendert)";
    kff::logInfo("%s", s.c_str());
}

}  // namespace

uint32_t bit(ActId a) {
    const uint8_t i = g_eff.idx[at(a)];
    return i < kBindCount ? kff::kBindable[i].bit : 0u;
}

bool analogAbove(uint32_t mask, float threshold) {
    const kff::Input& in = kff::input();
    if (mask == static_cast<uint32_t>(PAD_TRIGGER_R)) {
        return in.triggerR >= threshold;
    }
    if (mask == static_cast<uint32_t>(PAD_TRIGGER_L)) {
        return in.triggerL >= threshold;
    }
    return false;
}

void load() {
    Fall fall[kActCount];
    const Belegung roh = readRaw();
    const Belegung eff = aufloesen(roh, fall);
    for (size_t i = 0; i < kActCount; ++i) {
        const char* grund = nullptr;
        switch (fall[i]) {
        case Fall::Keiner: break;
        case Fall::Bereich: grund = "ausserhalb der Tabelle"; break;
        case Fall::PflichtVorgabe: grund = "Pflichtaktion ohne Taste"; break;
        case Fall::KonfliktVorgabe: grund = "Taste doppelt belegt"; break;
        case Fall::KonfliktLeer: grund = "Taste und Vorgabe doppelt belegt"; break;
        }
        if (grund != nullptr) {
            kff::logWarn("steuerung: %s = %s ungueltig (%s), wirksam %s", configKey(static_cast<ActId>(i)),
                keyLabel(roh.idx[i]), grund, keyLabel(eff.idx[i]));
        }
    }
    g_eff = eff;
    logBelegung();
}

void refresh() {
    if (!haveConfig()) {
        return;  // ohne ConfigService bleibt die zuletzt gesetzte Belegung im Speicher
    }
    Fall fall[kActCount];
    g_eff = aufloesen(readRaw(), fall);  // keine Zeile je Tick (Release)
}

Belegung current() {
    return g_eff;
}

uint8_t indexOf(ActId a) {
    return g_eff.idx[at(a)];
}

std::string hintKey(ActId a, const char* verb) {
    const uint8_t i = indexOf(a);
    if (i == 0 || i >= kBindCount) {
        return "not set";  // leere Belegung: das Verb davor entfaellt
    }
    std::string s = verb != nullptr ? std::string(verb) + " " : std::string();
    s += kff::kBindable[i].label;
    return s;
}

int changedCount() {
    int n = 0;
    for (size_t i = 0; i < kActCount; ++i) {
        if (g_eff.idx[i] != kDefaults.idx[i]) {
            ++n;
        }
    }
    return n;
}

Aenderung setFromUi(ActId a, uint8_t wert) {
    const Belegung alt = g_eff;
    const size_t ia = at(a);
    Aenderung r = applyChange(alt, a, wert);
    if (r.ergebnis == Ergebnis::Abgelehnt) {
        kff::logInfo("steuerung: abgelehnt %s: %s", nameDe(a), r.text.c_str());
        return r;
    }
    if (r.ergebnis == Ergebnis::Unveraendert) {
        return r;
    }
    if (haveConfig()) {
        for (size_t i = 0; i < kActCount; ++i) {
            if (r.neu.idx[i] != alt.idx[i] && !kff::config::setInt(kKeys[i], r.neu.idx[i])) {
                kff::logWarn("steuerung: %s nicht gespeichert", configKey(static_cast<ActId>(i)));
            }
        }
        refresh();  // liest die gespeicherten Werte zurueck (E-7: wirkt ab dem naechsten Pad-Tick)
    } else {
        g_eff = r.neu;
    }
    kff::logInfo("steuerung: Aenderung %s: %s -> %s", nameDe(a), keyLabel(alt.idx[ia]), keyLabel(wert));
    for (size_t i = 0; i < kActCount; ++i) {
        if (i != ia && r.neu.idx[i] != alt.idx[i]) {
            kff::logInfo("steuerung: getauscht %s: %s -> %s", nameDe(static_cast<ActId>(i)),
                keyLabel(alt.idx[i]), keyLabel(r.neu.idx[i]));
        }
    }
    return r;
}

void resetAll() {
    const Belegung alt = g_eff;
    int n = 0;
    for (size_t i = 0; i < kActCount; ++i) {
        if (alt.idx[i] != kDefaults.idx[i]) {
            ++n;
            if (haveConfig() && !kff::config::setInt(kKeys[i], kDefaults.idx[i])) {
                kff::logWarn("steuerung: %s nicht zurueckgesetzt", configKey(static_cast<ActId>(i)));
            }
        }
    }
    if (haveConfig()) {
        refresh();
    } else {
        g_eff = kDefaults;
    }
    kff::logInfo("steuerung: Zuruecksetzen auf die Vorgabe, %d Aktion(en) geaendert", n);
}

}  // namespace actions

namespace kff {
namespace {

actions::ActId toId(Act a) {
    switch (a) {
    case Act::Dismount: return actions::ActId::Dismount;
    case Act::Dash: return actions::ActId::Dash;
    case Act::GearUp: return actions::ActId::GearUp;
    case Act::GearDown: return actions::ActId::GearDown;
    case Act::Brake: return actions::ActId::Brake;
    case Act::BarrelRoll: return actions::ActId::BarrelRoll;
    case Act::CameraBehind: return actions::ActId::CameraBehind;
    case Act::CameraCloser: return actions::ActId::CameraCloser;
    case Act::CameraFarther: return actions::ActId::CameraFarther;
    case Act::Faster: return actions::ActId::Faster;
    case Act::Slower: return actions::ActId::Slower;
    case Act::Count: break;
    }
    return actions::ActId::Dismount;
}

}  // namespace

bool ActRef::pressed() const {
    return bit != 0 && input().isPressed(bit);
}

bool ActRef::held() const {
    return bit != 0 && input().isHeld(bit);
}

ActRef act(Act a) {
    return ActRef{actions::bit(toId(a))};
}

}  // namespace kff
