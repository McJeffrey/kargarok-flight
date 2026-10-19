// Ersatzpfad: der Vogel bekommt jeden Frame einen synthetischen Pfad in Richtung seines Kurses.
// Die Flugsteuerung liest dann "geradeaus", springt nie auf dem Pfad weiter und dereferenziert
// keinen NULL-Pfad. Bewiesen im MVP (kff-mvp).
#pragma once

class daKago_c;

namespace kff::fakepath {

// Vor jedem action()-Aufruf des eigenen Vogels im Ritt und im Absteigen.
void refresh(daKago_c* bird);

}  // namespace kff::fakepath
