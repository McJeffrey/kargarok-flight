// Szenenwechsel im Ritt (Entscheidung D1). Der Kern ordnet jeden Szenenwechsel in seinem Pre-Hook ein und beendet
// den Ritt vorher (endRideClean), wenn der Wechsel nicht positionsgetrieben ist: Ereignis, Menue, Titel, Laden,
// Tod, Warp. Das Modul freiheit weist nur positionsgetriebene Wechsel ab (SKIP_ORIGINAL). Ist der Ritt schon vom
// Kern beendet, sieht freiheit rideRunning() == false und laesst den Wechsel durch.
//
// Fuer freiheit (spaeter, Modulaenderung): if (kff::ride::isPositionDriven()) { ... SKIP ... }
#pragma once

#include <cstdint>

namespace kff::ride {

enum class SceneChange : uint8_t {
    None,      // kein Kern-Hook hat den Wechsel gesehen: wie Position behandeln (sicher abweisen)
    Position,  // positionsgetrieben: Ladezone, Ausgangs-Polygon, Void/Fall-Restart, Vogel-eigener Wechsel, Raumneuladen
    Event,     // Ereignis oder Zwischensequenz: Ritt vorher beendet (endRideClean), Wechsel laeuft durch
    Menu,      // Menue, Titel, Laden, Beenden: Ritt vorher beendet, Wechsel laeuft durch
    Warp,      // Warp (Midna, Ooccoo, Portal): Ritt beim Warp-Start beendet, Wechsel laeuft durch
    Death,     // Tod: Ritt vorher beendet, das Spiel verarbeitet den Tod
};

// Art des Wechsels, den der Kern zuletzt eingeordnet hat. Wird in jedem Pad-Tick auf None zurueckgesetzt, damit kein
// alter Wert gilt.
SceneChange sceneChange();

// true: positionsgetrieben oder unbekannt (None), freiheit darf mit SKIP_ORIGINAL abweisen.
// false: der Ritt ist vom Kern bereits beendet, der Wechsel laeuft durch.
bool isPositionDriven();

}  // namespace kff::ride
