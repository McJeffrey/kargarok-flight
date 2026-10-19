// Liste der Module in Aufrufreihenfolge. Neues Modul: Datei in src/modules/, Factory in
// core/kff/module.hpp deklarieren, hier eintragen und in CMakeLists.txt aufnehmen.
#include "core/kff/module.hpp"

namespace kff {
namespace {

constexpr size_t kMaxDisabled = 16;
const Module* g_disabled[kMaxDisabled] = {};
size_t g_disabledCount = 0;

}  // namespace

void disableModule(const Module* m) {
    if (m == nullptr || g_disabledCount >= kMaxDisabled || !moduleActive(m)) {
        return;
    }
    g_disabled[g_disabledCount++] = m;
}

bool moduleActive(const Module* m) {
    for (size_t i = 0; i < g_disabledCount; ++i) {
        if (g_disabled[i] == m) {
            return false;
        }
    }
    return true;
}

Module* const* moduleList(size_t& count) {
    static Module* const kList[] = {
        &modules::flug(),
        &modules::freiheit(),
        &modules::griff(),
        &modules::tempo(),
        &modules::kamera(),
        &modules::mensch(),  // vor autowolf: der erste onHumanCall-Treffer nimmt den Ruf (mensch_ritt AUS lehnt ab)
        &modules::autowolf(),
    };
    count = sizeof(kList) / sizeof(kList[0]);
    return kList;
}

}  // namespace kff
