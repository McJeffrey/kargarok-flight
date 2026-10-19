// Belegung der Flugaktionen (Kern). Die Regeln sind rein (ohne Spiel, ohne Konfiguration) und werden in
// core/actions_test.cpp geprueft. Der Zugriff der Module laeuft ueber kff::act (core/kff/actions.hpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace actions {

// Aktionen mit fester Reihenfolge. Die Reihenfolge entspricht den Konfig-Schluesseln bind_* (core/config.cpp).
enum class ActId : uint8_t {
    CallHold,      // bind_ruf_halten, Pflicht (E-4)
    CallTap,       // bind_ruf_tipp, Pflicht (E-4)
    Dismount,      // bind_absteigen, Pflicht (E-4)
    Dash,          // bind_dash
    GearUp,        // bind_gang_hoch
    GearDown,      // bind_gang_runter
    Brake,         // bind_bremse
    BarrelRoll,    // bind_fassrolle
    CameraBehind,  // bind_kamera_hinten
    CameraCloser,  // bind_kamera_naeher
    CameraFarther, // bind_kamera_weiter
    Faster,        // bind_schneller (Vorgabe R, teilt R mit Ruf halten)
    Slower,        // bind_langsamer (Vorgabe L)
    Count
};
inline constexpr size_t kActCount = static_cast<size_t>(ActId::Count);

// Belegung: je Aktion der Index in kff::kBindable (0 = Not set).
struct Belegung {
    uint8_t idx[kActCount];
};

// ---- Regeln (rein) ---------------------------------------------------------------------------------------------------

bool mandatory(ActId a);              // Ruf halten, Ruf tippen, Absteigen (E-4)
bool shareAllowed(ActId a, ActId b);  // Ruf tippen und Absteigen teilen B (R2); Ruf halten teilt R mit Schneller
uint8_t defaultIndex(ActId a);        // Vorgabe als Index in kBindable
Belegung defaults();                  // Vorgabe aller Aktionen (heutiges Verhalten)
const char* labelEn(ActId a);         // Oberflaechentext (Englisch, E-1)
const char* nameDe(ActId a);          // Name in Logzeilen (Deutsch)
const char* configKey(ActId a);       // Konfig-Schluessel bind_*
const char* keyLabel(uint8_t index);  // GameCube-Name zu einem Index (E-2)
// Taste einer Aktion fuer Hilfetexte in Saetzen (Englisch): "A", oder mit Verb davor "hold Z" und "tap Y". Ist die
// Aktion nicht belegt, steht nur "not set" und das Verb entfaellt. Liest die wirksame Belegung (indexOf).
std::string hintKey(ActId a, const char* verb);

// Auswahlliste je Aktion (K1): Pflichtaktionen haben keine Option "Not set". Index i der Liste ist Belegungsindex i+1.
int optionCount(ActId a);                 // 11 bei Pflichtaktionen, sonst 12
int uiIndexOf(ActId a, uint8_t bindIdx);  // Belegungsindex -> Index der Auswahlliste
int bindIndexOf(ActId a, int uiIdx);      // Index der Auswahlliste -> Belegungsindex

// Verstoss gegen die Regeln. leer: Pflichtaktion a ohne Taste. Sonst teilen die Aktionen a und b dieselbe Taste.
struct Verstoss {
    bool leer;
    ActId a;
    ActId b;
};
bool pruefen(const Belegung& b, Verstoss* v);

// Aufloesung roher Werte (Laden und je Pad-Tick, Entwurf 2.3, Reihenfolge und Vorrang in BERICHT.md).
enum class Fall : uint8_t {
    Keiner,           // Rohwert uebernommen
    Bereich,          // Index ausserhalb von kBindable, Vorgabe
    PflichtVorgabe,   // Pflichtaktion ohne Taste, Vorgabe
    KonfliktVorgabe,  // Rohwert doppelt belegt, Vorgabe
    KonfliktLeer,     // Rohwert und Vorgabe doppelt belegt, ohne Taste
};
Belegung aufloesen(const Belegung& roh, Fall* fall);

// Aenderung (E-5). Gesetzt: nur die Aktion. Getauscht: die kollidierenden Aktionen bekommen die bisherige Taste.
enum class Ergebnis : uint8_t { Unveraendert, Gesetzt, Getauscht, Abgelehnt };
struct Aenderung {
    Belegung neu;
    Ergebnis ergebnis = Ergebnis::Unveraendert;
    std::string text;  // englische Statuszeile des Fensters
};
Aenderung applyChange(const Belegung& b, ActId a, uint8_t wert);

// ---- Laufzeit (Kern) -------------------------------------------------------------------------------------------------

uint32_t bit(ActId a);                       // belegtes Bit der Aktion nach dem letzten Pad-Tick (0 = unbelegt)
bool analogAbove(uint32_t mask, float threshold);  // Analogwert von L oder R erreicht die Schwelle, sonst false
void load();                                 // einmal beim Laden (aus config::registerVars)
void refresh();                              // je Pad-Tick (aus padReadPost), wirkt ab dem naechsten Tick (E-7)
Belegung current();                          // wirksame Belegung
uint8_t indexOf(ActId a);                    // wirksamer Index einer Aktion
int changedCount();                          // Anzahl Aktionen ungleich Vorgabe
Aenderung setFromUi(ActId a, uint8_t wert);  // Weg der Auswahlliste (set()): pruefen, speichern, Ereigniszeile
void resetAll();                             // Zuruecksetzen auf die Vorgabe (Dialog)

#if defined(KFF_DEV_AUTOTEST)
// Reine Tests (core/actions_test.cpp, nur Testbau). Gibt die Anzahl fehlgeschlagener Pruefungen zurueck.
int runPureTests();
#endif

}  // namespace actions
