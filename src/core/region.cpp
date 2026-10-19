// Region-Hinweis beim Start (WP10.9). Dusklight leitet die Spielversion aus der Disc-ID ab
// (src/dusk/version.cpp: GZ2E = USA, GZ2P = PAL, GZ2J = JPN). Die Funktion dusk::version::getGameVersion
// ist nicht exportiert. Exportiert ist DVDGetCurrentDiskID, also dieselbe Quelle. Diese Datei liest nur
// und schreibt nichts. Getestet, weil in eigenen Laeufen gefahren und im Log belegt ("region: RZDP"), ist nur
// RZDP (PAL-Disc der Abnahme). GZ2P, GZ2E und GZ2J sind nicht gefahren. Andere Regionen bekommen einen Hinweis im Log.
#include "core/kff/log.hpp"

#include <cstring>

// Die Deklaration steht allein in dieser Datei, damit kein Spielheader mit anderem Rueckgabetyp in die
// Uebersetzungseinheit kommt. Der Zeiger zeigt auf die Disc-ID; ihre ersten vier Bytes sind der Spielcode.
extern "C" const void* DVDGetCurrentDiskID(void);

namespace kff::core {

void logRegion() {
    const unsigned char* id = static_cast<const unsigned char*>(DVDGetCurrentDiskID());
    if (id == nullptr) {
        logWarn("region: Disc-ID nicht lesbar, Region unbekannt (getestet ist nur RZDP, PAL)");
        return;
    }
    char game[5] = {};
    std::memcpy(game, id, 4);
    for (size_t i = 0; i < 4; ++i) {
        if (game[i] < 0x20 || game[i] > 0x7e) {
            logWarn("region: Disc-ID nicht lesbar (Region unbekannt, getestet ist nur RZDP, PAL)");
            return;
        }
    }
    if (std::strcmp(game, "RZDP") == 0) {
        logInfo("region: %s (PAL, Abnahme-Disc, getestet)", game);
    } else if (std::strcmp(game, "GZ2P") == 0) {
        logWarn("region: %s (GameCube PAL), nicht getestet, Verhalten nicht belegt", game);
    } else if (std::strcmp(game, "GZ2E") == 0) {
        logWarn("region: %s (GameCube USA), nicht getestet, Verhalten nicht belegt", game);
    } else if (std::strcmp(game, "GZ2J") == 0) {
        logWarn("region: %s (GameCube JPN), nicht getestet, Verhalten nicht belegt", game);
    } else {
        logWarn("region: %s nicht getestet, Verhalten nicht belegt (getestet ist nur RZDP, PAL)", game);
    }
}

}  // namespace kff::core
