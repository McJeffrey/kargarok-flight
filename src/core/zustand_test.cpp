// Testbuild-Spielstand "wie bei 100 Prozent" (nur KFF_DEV_AUTOTEST, nie im Release). Schalter sind Umgebungsvariablen,
// die beim Start gesetzt werden, z. B.
//   KFF_TEST_UNLOCK=1 KFF_TEST_TIME=18.5 mit --stage F_SP121,0,0,-1
//
//   KFF_TEST_UNLOCK=1   Die Setzungen des Dusklight-Editors (src/dusk/ui/editor.cpp: Twilight Clear Level 3,
//                       Transform Level 4): dComIfGs_onDarkClearLV(0..2) = Faron, Eldin, Lanayru geloest
//                       (mDarkClearLevelFlag 0x07), dComIfGs_onTransformLV(0..3) (mTransformLevelFlag 0x0F) und die
//                       Story-Flags 0x0D04 (M_077, Verwandlung frei), 0x0604 (M_021); mit KFF_TEST_STORY=nach (Standard)
//                       zusaetzlich 0x1E08, 0x0C01, 0x0810 (Hyrule-Feld-Layer 6, Ganon-Wand am Schloss).
//   KFF_TEST_STORY=vor|nach  vor: nur 0x0D04 und 0x0604, Hyrule-Feld in Layer 0 (Schloss ohne Wand). Standard nach.
//                       Gesetzt wird beim Start-Stage-Setzen (dComIfGp_setNextStage, d_s_logo.cpp nach
//                       dComIfGs_init): das ist vor dem Raumladen und damit vor der Layer-Wahl (Layer 14 =
//                       Zwielicht, dKy_darkworld_stage_check). Bei jedem Link-Frame wird geprueft, ob der Stand noch
//                       gilt (z. B. nach dem Laden eines Spielstands).
//   KFF_TEST_TIME=<h>   Tageszeit fest auf h Stunden (0 bis 23,99). Zeit = h * 15 (mTime, wie dKy_getdaytime_hour und
//                       der Editor). Jeder Link-Frame setzt die Zeit auf diesen Wert. 18.5 ist Abend.
//   KFF_TEST_FORM=wolf|mensch  Form beim Start (Editor-Feld Form, setTransformStatus). Ohne Angabe mit KFF_TEST_UNLOCK
//                       Wolf: beim Laden entscheidet die gespeicherte Form (dComIfGs_Wolf_Change_Check, nur mit M_077).
//                       Ohne Wolf kann der Ruf des Kargarok (canCall verlangt Wolf) nicht ausgeloest werden.
//                       Zur Form wird einmal beim Start-Stage gesetzt, nicht laufend.
//   Zufallswetter: mit KFF_TEST_UNLOCK werden die Wuerfelwerte jeden Link-Frame auf 0 gehalten (kein Wetter-Partikel).
//
// Hooks: Besitzer "core", Prioritaet kCore - 1 (nach den Kern-Hooks, kein Konflikt mit den Modulen). Keine Eingaben.
#include "d/dolzel_rel.h"

#include "core/kff/hooks.hpp"
#include "core/kff/log.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(KFF_DEV_AUTOTEST)

namespace kff::zustand {
namespace {

// Story-Flags aus der Editor-Liste (src/dusk/imgui/ImGuiEventFlags.hpp).
constexpr uint16_t kBasisFlags[] = {
    0x0D04,  // M_077: Schattenkristall, Verwandlung frei (dComIfGs_Wolf_Change_Check)
    0x0604,  // M_021: erster Portal-Warp
};
// Nur mit KFF_TEST_STORY=nach (Standard): Hyrule-Feld-Layer 6 (nach Midnas verzweifelter Stunde, mit Ganon-Wand am
// Schloss). Ohne sie bleibt das Hyrule-Feld in Layer 0 (Schloss ohne Wand, F_SP121 in d_com_inf_game.cpp).
constexpr uint16_t kNachFlags[] = {
    0x1E08,  // F_0250: Midna wiederbelebt, Ganon-Wand erscheint (Layer 6)
    0x0C01,  // M_071: Zant erscheint (Midnas verzweifelte Stunde)
    0x0810,  // M_035: nach der Kutschen-Szene
};

bool g_unlock = false;      // KFF_TEST_UNLOCK=1
bool g_storyNach = true;    // KFF_TEST_STORY: nach (Standard) oder vor
bool g_timeLock = false;    // KFF_TEST_TIME gueltig
float g_timeTarget = 0.0f;  // Tageszeit in Einheiten (Stunde * 15)
bool g_timeLogged = false;
int g_form = -1;            // KFF_TEST_FORM: 0 Mensch, 1 Wolf, -1 unveraendert
bool g_formDone = false;    // Form beim Start-Stage gesetzt

bool g_stateLogged = false;

bool unlockPresent() {
    return dComIfGs_isDarkClearLV(0) && dComIfGs_isDarkClearLV(1) && dComIfGs_isDarkClearLV(2) &&
           dComIfGs_isTransformLV(3) && dComIfGs_isEventBit(0x0D04);
}

void logState(const char* wo) {
    kff::logInfo("zustand: %s: Zwielicht Faron/Eldin/Lanayru=%d%d%d, Verwandlung 0..3=%d%d%d%d, M_077=%d, Form=%s, "
                 "Tageszeit=%.2f",
        wo, dComIfGs_isDarkClearLV(0) ? 1 : 0, dComIfGs_isDarkClearLV(1) ? 1 : 0, dComIfGs_isDarkClearLV(2) ? 1 : 0,
        dComIfGs_isTransformLV(0) ? 1 : 0, dComIfGs_isTransformLV(1) ? 1 : 0, dComIfGs_isTransformLV(2) ? 1 : 0,
        dComIfGs_isTransformLV(3) ? 1 : 0, dComIfGs_isEventBit(0x0D04) ? 1 : 0,
        dComIfGs_getTransformStatus() == 1 ? "Wolf" : "Mensch", static_cast<double>(dComIfGs_getTime()));
}

void applyUnlock() {
    for (int i = 0; i < 3; ++i) {
        dComIfGs_onDarkClearLV(i);
    }
    for (int i = 0; i < 4; ++i) {
        dComIfGs_onTransformLV(i);
    }
    for (uint16_t flag : kBasisFlags) {
        dComIfGs_onEventBit(flag);
    }
    if (g_storyNach) {
        for (uint16_t flag : kNachFlags) {
            dComIfGs_onEventBit(flag);
        }
    }
}

// Start-Stage (--stage) und Warp: vor dem Raumladen, nach dComIfGs_init.
HookAction stageNextStagePre(ModContext*, void*, void*, void*) {
    if (g_unlock && !unlockPresent()) {
        applyUnlock();
        logState("Start-Stage gesetzt");
    }
    // Die gespeicherte Form entscheidet beim Laden (d_a_alink.cpp, dComIfGs_Wolf_Change_Check): Wolf nur mit M_077.
    if (g_form >= 0 && !g_formDone) {
        g_formDone = true;
        dComIfGs_setTransformStatus(static_cast<u8>(g_form));
        kff::logInfo("zustand: Form beim Start gesetzt: %s", g_form == 1 ? "Wolf" : "Mensch");
    }
    return HOOK_CONTINUE;
}

// Jeder Link-Frame: Stand nachpruefen (z. B. Spielstand geladen), Zufallswetter aus und die Tageszeit festhalten.
HookAction linkExecutePre(ModContext*, void*, void*, void*) {
    if (g_unlock && !unlockPresent()) {
        applyUnlock();
        logState("Link-Frame nachgesetzt");
    }
    if (g_unlock) {
        // Kein Zufallswetter auf dem Hyrule-Feld (Partikel, dKyw_wether_proc): die Wuerfelwerte bleiben auf dem Stand
        // nach dem Spielstart (dice_wether_time 0 laesst den Block fuer F_SP121 nicht laufen, Modus sonnig).
        dScnKy_env_light_c* env = dKy_getEnvlight();
        env->dice_wether_time = 0.0f;
        env->dice_wether_mode = 0;
    }
    if (g_timeLock && dComIfGs_getTime() != g_timeTarget) {
        dComIfGs_setTime(g_timeTarget);
        if (!g_timeLogged) {
            g_timeLogged = true;
            kff::logInfo("zustand: Tageszeit fest auf %.2f Einheiten (Stunde %.2f)", static_cast<double>(g_timeTarget),
                static_cast<double>(g_timeTarget / 15.0f));
        }
    }
    if (g_unlock && !g_stateLogged) {
        g_stateLogged = true;
        logState("erster Link-Frame");
        kff::logInfo("zustand: Layer %d, Start-Stage %s, Raum %d", dComIfG_play_c::getLayerNo(0),
            dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
    }
    return HOOK_CONTINUE;
}

void readEnvironment() {
    const char* unlock = std::getenv("KFF_TEST_UNLOCK");
    g_unlock = unlock != nullptr && unlock[0] == '1' && unlock[1] == '\0';

    // KFF_TEST_STORY=nach|vor. Ohne Angabe nach (Layer 6 am Hyrule-Feld); vor laesst die Ganon-Wand weg (Layer 0).
    const char* story = std::getenv("KFF_TEST_STORY");
    if (story != nullptr && std::strcmp(story, "vor") == 0) {
        g_storyNach = false;
    } else if (story != nullptr && story[0] != '\0' && std::strcmp(story, "nach") != 0) {
        kff::logWarn("zustand: KFF_TEST_STORY=%s unbekannt (nach oder vor), es gilt nach", story);
    }

    // KFF_TEST_FORM=wolf|mensch. Ohne Angabe: mit KFF_TEST_UNLOCK Wolf (der Kargarok-Ritt startet als Wolf-Link).
    const char* form = std::getenv("KFF_TEST_FORM");
    if (form == nullptr || form[0] == '\0') {
        g_form = g_unlock ? 1 : -1;
    } else if (std::strcmp(form, "wolf") == 0) {
        g_form = 1;
    } else if (std::strcmp(form, "mensch") == 0) {
        g_form = 0;
    } else {
        kff::logWarn("zustand: KFF_TEST_FORM=%s unbekannt (wolf oder mensch), Form bleibt unveraendert", form);
    }

    const char* time = std::getenv("KFF_TEST_TIME");
    if (time != nullptr && time[0] != '\0') {
        char* end = nullptr;
        const double hours = std::strtod(time, &end);
        if (end != time && *end == '\0' && hours >= 0.0 && hours < 24.0) {
            g_timeTarget = static_cast<float>(hours * 15.0);
            g_timeLock = true;
        } else {
            kff::logWarn("zustand: KFF_TEST_TIME=%s ungueltig (0 bis 24 Stunden), Zeit bleibt frei", time);
        }
    }
}

}  // namespace

// Aufruf aus mod_initialize (core.cpp), nur Testbuild, nach installCoreHooks().
void install() {
    readEnvironment();
    if (!g_unlock && !g_timeLock && g_form < 0) {
        return;
    }
    Registry& reg = registry();
    reg.setOwner("core");
    const ModResult a = reg.addPre(Hook::StageNextStage, stageNextStagePre, prio::kCore - 1);
    const ModResult b = reg.addPre(Hook::LinkExecute, linkExecutePre, prio::kCore - 1);
    kff::logInfo("zustand: Testbuild-Stand: unlock=%d, Tageszeit=%s, Hooks StageNextStage %s, LinkExecute %s",
        g_unlock ? 1 : 0, g_timeLock ? "fest" : "frei", modResultName(a), modResultName(b));
}

}  // namespace kff::zustand

#endif
