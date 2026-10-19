// Konfiguration des Pakets ueber den ConfigService (optional). Schluessel im Namensraum
// "mod.com_kaizuki_kargarok__flight.<name>" in config.json (Escaping laut SDK: "." wird "_", "_" wird "__").
// Ohne Dienst gelten die Standardwerte. Jeder Wert wird beim Lesen auf seinen Bereich geklemmt.
// Entfernt (Entscheidung D10, ohne Wirkung): far_plane, fog_off, landing_frames, all_rides, cam_mode.
// Umbenannt (D10): invert_steer heisst jetzt lenkung_invertieren. invert_steer bleibt als Altlast-Alias lesbar.
#pragma once

#include <cstdint>

namespace kff::config {

enum class Key : uint8_t {
    Mode,               // int: 0 frei (einziger Modus, 0..0). Modi 1 und 2 gibt es nicht (Entscheidung D9)
    InvertSteer,        // bool: Nick umkehren (Modul flug). Schluessel lenkung_invertieren, Alias invert_steer
    CamMode,            // int, kein Schluessel mehr (D10): liefert immer 0 = frei. Nur fuer das Modul kamera erhalten
    CruiseSpeed,        // float 0..200: Reisegeschwindigkeit (Modul flug)
    EventKeep,          // bool: Szenenwechsel waehrend Ereignissen erlauben (Modul freiheit)
    RoofLimit,          // bool: Decke ueberhaupt setzen (Modul freiheit, Standard aus)
    MinGroundClearance, // float -1..2000: Mindest-Bodenfreiheit, -1 = aus (Modul freiheit)
    StartGear,          // int 1..5: Startgang des Rittes (Modul tempo, Standard 3)
    HudShow,            // bool: Gang- und Tempozeile im HUD (Modul tempo)
    FovEffect,          // bool: Sichtfeld-Zusatz mit dem Tempo (Modul tempo, Kamera)
    MenschRitt,         // bool: Ruf als Mensch = Ritt als Mensch im Griff (Modul mensch). Standard AUS (Entscheidung D4)
    DeckeAbstieg,       // bool: Ausweichen nach unten unter Decke oder Ueberhang (Modul freiheit, E6). Standard AUS (E10)
    KameraAbstand,      // float 300..1400: Startabstand der Kamera im Wolf-Ritt (Modul kamera, Standard 650 wie bisher)
    KameraAbstandMensch,  // float 300..2000: Startabstand der Kamera im Menschen-Ritt (Modul kamera, Standard 1100)
    // Flugtasten (E-10): int 0..11, Index in kff::kBindable (0 = Not set). Strict: ein ungueltiger Wert faellt auf die
    // Vorgabe zurueck, nicht auf den Klemmwert. Die Vorgaben sind die heutigen Tasten (E-11), siehe core/config.cpp.
    BindCallHold,        // bind_ruf_halten (Vorgabe R)
    BindCallTap,         // bind_ruf_tipp (Vorgabe B)
    BindDismount,        // bind_absteigen (Vorgabe B)
    BindDash,            // bind_dash (Vorgabe A)
    BindGearUp,          // bind_gang_hoch (Vorgabe Steuerkreuz hoch)
    BindGearDown,        // bind_gang_runter (Vorgabe Steuerkreuz runter)
    BindBrake,           // bind_bremse (Vorgabe Z)
    BindBarrelRoll,      // bind_fassrolle (Vorgabe Y)
    BindCameraBehind,    // bind_kamera_hinten (Vorgabe X)
    BindCameraCloser,    // bind_kamera_naeher (Vorgabe Steuerkreuz rechts)
    BindCameraFarther,   // bind_kamera_weiter (Vorgabe Steuerkreuz links)
    BindFaster,          // bind_schneller (Vorgabe R): halten = stufenlos schneller (Modul tempo, Version 2.4.0, neu)
    BindSlower,          // bind_langsamer (Vorgabe L): halten = stufenlos langsamer bis zum Schweben (Version 2.4.0, neu)
    TipsShow,           // bool: Kurztipps bei den ersten Ritten (Modul tempo). Standard AN (Version 2.3.0, neu)
    TipsRest,            // int 0..3: Ritte mit Tipps, die noch uebrig sind (Modul tempo). Standard 3 (Version 2.3.0, neu)
    Count
};

// Ritte mit Tipps je Einschalten des Schalters tipps_anzeigen (Zaehler tipps_rest wird dann auf diesen Wert gesetzt).
inline constexpr int64_t kTipRides = 3;

// Registriert die wirksamen Variablen beim ConfigService. Ohne Dienst nur eine Warnung im Log.
void registerVars();

// Handle der Variablen fuer das Einstellungsfeld (panel.cpp). 0: ohne Wirkung oder nicht registriert.
uint64_t handleOf(Key key);

// Liest den aktuellen Wert, geklemmt auf den Bereich. Fehlt der Dienst oder die Registrierung, kommt der Standardwert.
bool getBool(Key key);
int64_t getInt(Key key);
double getFloat(Key key);

// Schreibt einen Wert in den ConfigService (Speicherung in config.json). false: ohne Dienst, ohne Registrierung
// oder bei einem Fehler des Dienstes.
bool setBool(Key key, bool value);
bool setInt(Key key, int64_t value);

}  // namespace kff::config
