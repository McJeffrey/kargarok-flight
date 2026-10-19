// Interne Schnittstelle zwischen input.cpp (Pad-Lesung, Ruf-Kombination, Konsum, UI-Block) und ride.cpp (Ruf, Ritt).
// Nicht fuer Module gedacht. Module lesen den Schnappschuss ueber kff::input() (core/kff/input.hpp).
#pragma once

#include <cstdint>

class daAlink_c;

namespace kff {
namespace core {

// ---------------------------------------------------------------------------------------------
// input.cpp
// ---------------------------------------------------------------------------------------------

// Zaehler der Pad-Ticks (je mDoCPd_c::read, Post-Hook).
uint32_t padTick();
// D16, UI-Eingabeblock: ein sichtbares RmlUi-Dokument von Dusklight blockiert die Pad-Eingabe des Spiels. Ohne UiService
// gilt "nicht sichtbar".
bool uiDocumentVisible();
// E5: Pause, Menue, Karte, Item-Rad, Dokument, Dialog, Ereignis oder Szenenwechsel offen. Dann verbraucht der Kern B und
// die Ritt-Tasten nicht, B bedient das Menue.
bool uiBlocksRideInput();
// D7: B des am Link-Frame verworfenen Rufs geht an das Spiel zurueck (Pad von Port 1, Flanke und gehalten). Der Grund
// laeuft ueber die ratenbegrenzte Grundtabelle (F02, Zeile "chord dropped (<Grund>), B returned to the game (n=..)").
void restoreChordB(const char* why);
// D8: true, wenn der Ruf (Session::callRequested) aus dem laufenden Pad-Tick stammt. linkExecutePre fuehrt nur diesen aus.
bool callRequestFresh();
// D8: ein veralteter Ruf am Link-Frame (Pad-Hook seit dem Ruf nicht gelaufen) wird verworfen, gezaehlt und geloggt.
void dropStaleCall();
// F02: noch nicht geloggte Ablehnungen schreiben (Zusammenfassung) und die Zeile INPUT mit dem Abgleich
// rejected == rejectSum, restored == dropSum ausgeben.
void logInputSummary();
// Testbuild und Log: Grund der letzten Pad-Ablehnung ("" = noch keine).
const char* lastRejectReason();

// Zaehler der Eingabe fuer die Zusammenfassung (Zeilen COUNTS, CORE2, CORE3 in ride.cpp und INPUT in input.cpp).
struct InputCounts {
    uint32_t callRejected = 0;   // Ruf-Kombination abgelehnt (B bleibt erhalten), ratenbegrenzt geloggt
    uint32_t chordConsumed = 0;  // Ruf-Kombination angenommen, B verschluckt
    uint32_t consumedEdges = 0;  // Tastenflanken, die der Konsum im Ritt entzogen hat
    uint32_t consumedHeld = 0;   // gehaltene Tasten, die der Konsum im Ritt entzogen hat (Ticks)
    uint32_t chordRestored = 0;  // B-Tipp des verworfenen Rufs an das Spiel zurueckgegeben (D7)
    uint32_t rejectLines = 0;    // F02: Logzeilen "call rejected (..)"
    uint32_t rejectSum = 0;      // F02: Summe der n in diesen Zeilen (== callRejected nach logInputSummary)
    uint32_t dropLines = 0;      // F02: Logzeilen "chord dropped (..)"
    uint32_t dropSum = 0;        // F02: Summe der n in diesen Zeilen (== chordRestored nach logInputSummary)
    uint32_t latchDropped = 0;   // D8: Ruf ohne Link-Frame bis zum naechsten Pad-Tick verworfen (B zurueck)
    uint32_t staleDropped = 0;   // D8: veralteter Ruf am Link-Frame verworfen (erwartet 0)
    uint32_t lockRepairs = 0;    // RK (a)/P2-04: Lock-Flaggen nach dem Ritt mit dem Schattenzustand korrigiert
};
const InputCounts& inputCounts();

// ---------------------------------------------------------------------------------------------
// ride.cpp
// ---------------------------------------------------------------------------------------------

// Je Pad-Tick vor dem Schnappschuss: angefordertes Entfernen nachverfolgen, Abflugfrist (D-B), Einordnung des
// Szenenwechsels zuruecksetzen (D1).
void ridePadTick();
// Grund, warum der Ruf jetzt nicht geht, oder nullptr, wenn er geht (callBlockReason).
const char* rideCallBlockReason(daAlink_c* link);
// Gemeinsamer Ausnahmezaehler des Kerns (COUNTS exceptions=).
uint32_t& coreExceptionCount();

}  // namespace core
}  // namespace kff
