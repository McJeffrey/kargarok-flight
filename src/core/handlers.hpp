// Interne Schnittstelle: hooks.cpp registriert die Kern-Handler, ride.cpp implementiert sie.
// Nicht fuer Module gedacht.
#pragma once

#include "core/kff/services.hpp"

namespace kff {

// hooks.cpp: registriert alle Kern-Hooks unter dem Besitzer "core".
void installCoreHooks();

namespace core {

// mDoCPd_c::read, Post: Autotest-Eingaben, Ruf-Kombination (R + B).
void padReadPost(ModContext* ctx, void* args, void* retval, void* userdata);
// Befund 2 (F2): Post-Hook daAlink_c::posMove, nimmt den Wurzelversatz der Fallanimation nach dem Loslassen zurueck.
void linkPosMovePost(ModContext* ctx, void* args, void* retval, void* userdata);
// daAlink_c::execute, Pre: Link-Wechsel, Ruf, Ritt-Wache, Modul-Aufrufe onFrameLink.
HookAction linkExecutePre(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::execute, Pre: Ersatzpfad, Start des Ritts, Absteige-Zaehler, Modul-Aufrufe onFrameBird.
HookAction kagoExecutePre(ModContext* ctx, void* args, void* retval, void* userdata);
// daE_YM_c::_delete, Pre: haelt der eigene Vogel einen Lock auf diesen E_YM, wird er geloest.
HookAction ymDeletePre(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::checkWallHitFall, Replace: der eigene Vogel stoesst nie ab.
void kagoCheckWallHitFallReplace(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::executeBalloonMenu, Replace: B im Flug steigt ab (eigener Vogel).
void kagoExecuteBalloonMenuReplace(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::draw, Pre: der eigene Vogel bleibt sichtbar.
HookAction kagoDrawPre(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::_delete, Pre: vor dem Freigeben den Ritt loesen, Sitzung zuruecksetzen.
HookAction kagoDeletePre(ModContext* ctx, void* args, void* retval, void* userdata);
// dComIfGp_setNextStage, Pre: Szenenwechsel durch Ereignis oder Menue beendet den Ritt sauber.
HookAction stageNextStagePre(ModContext* ctx, void* args, void* retval, void* userdata);
// daAlink_c::checkDeadAction, Pre (kein SKIP): Tod im Ritt beendet den Ritt, bevor das Spiel den Tod verarbeitet.
HookAction linkCheckDeadActionPre(ModContext* ctx, void* args, void* retval, void* userdata);
// daAlink_c::procCoDeadInit, Pre (kein SKIP): zweites Netz fuer den Tod (Zwangs-Game-Over ohne HP).
HookAction linkProcCoDeadInitPre(ModContext* ctx, void* args, void* retval, void* userdata);
// dComIfG_changeOpeningScene, Pre (kein SKIP): Titel beendet den Ritt sauber.
HookAction openingScenePre(ModContext* ctx, void* args, void* retval, void* userdata);
// dComIfGs_gameStart, Pre (kein SKIP): Laden beendet den Ritt (Sicherheitsnetz).
HookAction gameStartPre(ModContext* ctx, void* args, void* retval, void* userdata);
// dMenu_save_c::saveMoveDisp / saveMoveDisp2, Pre: im Ritt Speichern sperren (SKIP), nur wenn die Schreibbedingung
// des Spiels (Warnfenster Status 1) erfuellt ist. Abbruch mit Hinweis.
HookAction saveMoveDispPre(ModContext* ctx, void* args, void* retval, void* userdata);
HookAction saveMoveDisp2Pre(ModContext* ctx, void* args, void* retval, void* userdata);
// mDoMemCd_Ctrl_c::save, Pre (kein SKIP): Kartenschreiben im Ritt beendet den Ritt zuerst (Sicherheitsnetz).
HookAction cardSavePre(ModContext* ctx, void* args, void* retval, void* userdata);
// updateAutoSave, Pre und Post (E11): markieren den Autosave-Aufruf, damit cardSavePre dessen Schreiben nie verweigert.
HookAction autoSavePre(ModContext* ctx, void* args, void* retval, void* userdata);
void autoSavePost(ModContext* ctx, void* args, void* retval, void* userdata);
// dStage_changeScene, Pre (kein SKIP): ordnet den Wechsel ein (kff::ride); Ereignis und Menue beenden den Ritt vorher.
HookAction stageChangeScenePre(ModContext* ctx, void* args, void* retval, void* userdata);
// daAlink_c::procCoWarpInit, Pre (kein SKIP): Warp (Midna, Ooccoo, Portal) beendet den Ritt beim Start (D1).
HookAction linkProcCoWarpInitPre(ModContext* ctx, void* args, void* retval, void* userdata);
// daKago_c::executeFly und executeStagger, Pre (K6): fehlt mpPath1 des eigenen Vogels, wird der Ersatzpfad gesetzt.
HookAction kagoExecuteFlyPre(ModContext* ctx, void* args, void* retval, void* userdata);
HookAction kagoExecuteStaggerPre(ModContext* ctx, void* args, void* retval, void* userdata);
#if defined(KFF_DEV_AUTOTEST)
// Schadensdiagnose (lebensverlust-zyklus3.md, Fix C): nur Testbuild, loggt Quelle und Ergebnis, greift nicht ins Spiel ein.
HookAction linkSetDamagePointPre(ModContext* ctx, void* args, void* retval, void* userdata);
void linkCheckPolyDamagePost(ModContext* ctx, void* args, void* retval, void* userdata);
HookAction linkProcWolfDamageInitPre(ModContext* ctx, void* args, void* retval, void* userdata);
void linkCheckWolfLandActionPost(ModContext* ctx, void* args, void* retval, void* userdata);
#endif

// Region-Hinweis beim Start (region.cpp, WP10.9): liest die Disc-ID, meldet alles ausser PAL (GZ2P01).
void logRegion();
// Startpruefung beim Laden: entfernt verwaiste eigene Voegel (hoechstens einer ist erlaubt, sonst Warnung).
void startupBirdCheck();
// Entladen: Ritt beenden (endRideClean, Grund Shutdown), eigener Vogel per fopAcM_delete (v1-Paritaet, WP7 Lebenszyklus).
void shutdownBird();
// Zusammenfassung der Zaehler in den Log.
void logSummary();

}  // namespace core
}  // namespace kff
