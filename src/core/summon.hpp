// Interne Schnittstelle des Rufs (WP6): Spawn-Pruefung, Ruf-Gate des Spawns, Ritt-Start-Gate und Zeitgrenzen.
// Nur fuer den Kern (ride.cpp). Module rufen weiter kff::summonBird (core/kff/session.hpp), das hier implementiert ist.
#pragma once

#include <cstdint>

class daAlink_c;

namespace kff::summon {

struct Stats {
    uint32_t summon = 0;            // Voegel erzeugt (SPAWN)
    uint32_t createFailed = 0;      // fopAcM_create meldete einen Fehler
    uint32_t spawnRejected = 0;     // summonBird abgelehnt (Gate vor jeder Wirkung)
    uint32_t roomRejected = 0;      // Spawnpunkt nicht frei ("room"), Pad-Hook und summonBird
    uint32_t strayFound = 0;        // eigener Vogel ohne Sitzung gefunden (Ruf abgelehnt, Vogel entfernt)
    uint32_t rideStartWait = 0;     // Vogel-Frames, in denen der Ritt-Start gesperrt war
    uint32_t summonAborted = 0;     // Summoning wegen Ereignis abgebrochen
    uint32_t summonTimeout = 0;     // Zeitgrenze des Rufs (kein Ritt-Start)
    uint32_t transformTimeout = 0;  // Sicherheitsnetz der Phase Transforming
};

// Weitere Ruf-Sperren nach den Gates des Pad-Hooks (ride.cpp callBlockReason): eigener Vogel ohne Sitzung in der Welt,
// Spawnpunkt ueber Link nicht frei ("room"). nullptr = Ruf erlaubt. Nur Abfragen, keine Wirkung ausser dem Entfernen
// eines verwaisten eigenen Vogels.
const char* callBlockReason(daAlink_c* link);

// Ritt-Start-Gate (kagoExecutePre, erster WAIT-Frame des Vogels): Link lebt und steht in keinem Ereignis.
// Testbuild: KFF_TEST_NORIDE=1 haelt den ersten gerufenen Vogel ohne Ritt-Start fest (WP6.9).
bool rideStartAllowed(daAlink_c* link);

// Je Kern-Tick (padReadPost, nach dem Schnappschuss): Zeitgrenze des Rufs in Kern-Ticks ohne Pause (Summoning ohne
// Ritt-Start), Abbruch des Rufs bei Ereignis, Sicherheitsnetz fuer eine haengende Phase Transforming.
void tick();

// Befund 1 (F2): der wartende Ruf im Abflug. Je Pad-Tick nach retryPendingDelete (ridePadTick): erzeugt den neuen Vogel,
// sobald der abfliegende im _delete-Hook entfernt ist. abortDeferred bricht ihn ab (Link gewechselt).
void deferredTick();
void abortDeferred(const char* why);

const Stats& stats();

}  // namespace kff::summon
