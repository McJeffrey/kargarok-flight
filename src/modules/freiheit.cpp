// Modul "freiheit" (Teil 2, Besitzer: Freiheit ohne Clipping): Grenzenlos-Verhalten im Ritt auf dem eigenen Kargarok.
//
// Wirkung, nur im Ritt (Phase Riding) und nur fuer den eigenen Vogel:
//  - Sichtbare feste Geometrie (Gelaende, Felsen, Gebaeude, Raumdecken und -waende) wird nicht durchflogen.
//    Je Frame prueft das Modul den Weg des Vogels von der Startposition zur Endposition mit Linienabfragen
//    (dBgS_LinChk) an sieben Abtastpunkten (Mitte, vier Seiten, oben, unten). Trifft ein Punkt eine Flaeche,
//    bleibt der Vogel knapp davor (kSkin) und gleitet entlang der Flaeche: der Restweg ohne Anteil in die
//    Flaeche wird neu geprueft (hoechstens kSlideIters Schritte). Die Normale kommt aus dBgS::GetTriPla und zeigt
//    immer zum Vogel. Ergebnis: kein Durchflug. Kursaenderung nur bei Wandkontakt in diesem Frame, und zwar zur
//    freien Richtung, die dem Kurs am naechsten liegt. In einer Ecke gilt die Kante zwischen beiden Waenden
//    (Gedaechtnis kWallMemFrames Frames), damit der Kurs nicht zwischen zwei Tangenten pendelt (Befund nf3-innen1).
//    Keine Vorausschau: auf freier Strecke bleibt der Kurs unveraendert (Befund flug: 168 Einheiten vor einer Wand).
//  - Untergrenze wie checkGroundHeight (d_a_kago.cpp:329-381): hoechster Boden (GroundCross) oder hoechste
//    Wasserflaeche (Spl-Gruppe, dBgS_ObjGndChk_Spl) unter dem Vogel, plus min_ground_clearance (Standard 300,
//    wie Vanilla). Ohne beides keine Untergrenze. Gesetzt in executeFly-Pre (vor checkHeight), nach der
//    Bewegung (Post) erneut geprueft. Das Wasser ist wichtig: Link schwimmt, sobald der Wasserspiegel mehr
//    als 90 ueber ihm liegt (wasser.md). Die Untergrenze haelt ihn darueber.
//  - Keine kuenstliche Decke (D9, WP6): checkRoofHeight des eigenen Vogels laeuft nicht (Pre mit SKIP) und liefert
//    einheitlich kNoRoof, in allen Aktionen und Modi (roof_limit=0). Echte Decken stoppt die Abtastung wie jede andere
//    Flaeche. checkGroundHeight bleibt Vanilla.
//  - Acch des Vogels im Ritt (WP6): CrrPos (d_bg_s_acch.cpp ab Zeile 310) laeuft im Ritt nicht. daKago_c::action ruft
//    es nur bei field_0x6dc == 0; freiheit setzt den Wert je Ritt-Frame auf 1 und gibt am Ritt-Ende den Wert davor
//    zurueck (R14). Bewegung und Kollision kommen allein aus der Abtastung und der Untergrenze. Schatten (daKago_c::draw:
//    m_gnd, GetGroundH) und Trefferbits fuehrt refreshAcch an der Endposition des Frames nach. Die Bits WALL_NONE,
//    LINE_CHECK_NONE und ROOF_NONE setzt das Modul weiterhin (nur noch fuer den Kontrolllauf KFF_TEST_CRRPOS=1 wirksam).
//    Nur Acch-Bits, die dieses Modul gesetzt hat (ownBits), nimmt es beim Ritt-Ende und beim Entladen zurueck.
//  - Stillstands-Waechter (WP6, K4/T11): steht der Vogel kStuckTicks Ritt-Frames ohne Stick-Eingabe fest (Umkreis
//    kStuckRadius), endet der Ritt mit Grund Stuck (endRideClean, Hinweis im Spiel). Mit Stick greift er nicht.
//  - Raum-Neustart im Ritt (V12): checkRestartRoom, checkRoofRestart und startRestartRoom liefern im Ritt "kein
//    Neustart" (Pre mit SKIP), ausser bei Tod oder laufendem Ereignis. Nach dem Ritt gilt die Vanilla-Regel.
//  - Link im Wolfs-Ritt (Cargo): WALL_NONE und ROOF_NONE, gleiche Regel fuer die eigenen Bits.
//  - Kein Stagger durch Sondercode-1-Treffer (checkSizeBg setzt field_0x6d9; executeFly-Pre leert es).
//  - Szenenwechsel im Ritt (WP1, Entscheidungen D1, ORCH-2, V13; Abschnitt "Szenenwechsel im Ritt" unten):
//    positionsgetriebene Wechsel werden per Pre-SKIP an der Quelle gesperrt, BEVOR das Spiel ein Pflichtereignis
//    bestellt: daAlink_c::checkSceneChange (Ladezone SCENE_EXIT, Ausgangs-Polygon, Nebel), checkRestartRoom und
//    checkRoofRestart (P-RESTART), startRestartRoom ohne Ereignis, daTagRetRm_c (Rueckkehr-Raum), Vogel-eigene Wechsel
//    (setSceneChange, checkFlySceneChange), dStage_changeScene ohne Ereignis und anstehende Wechsel ohne Ereignis.
//    Log je Episode: "freiheit: positional scene veto (<Aufrufer>) ...". Befund b-ladezone1 (vorher): die Sperre erst
//    in dStage_changeScene liess das von checkSceneChange bestellte Pflichtereignis haengen (Ritt-Ende als Ereignis,
//    danach Link 900 Ticks starr in PROC_WOLF_WAIT, kein Szenenwechsel).
//    Ereignis-, Warp-, Menue- und Ladewechsel werden nicht gesperrt: Kern oder Modul beenden den Ritt vorher
//    (endRideClean), der Wechsel laeuft. Das Modul beendet selbst bei Warp-Status (Karte oder Ooccoo), bei
//    dStage_changeScene mit Pflichtereignis, bei setNextStage mit 4 Argumenten (Menue, Konsole, Warp-Menue),
//    dStage_changeScene4Event und startRestartRoom im Ereignis. Jeder Stage-Wechsel nach einem Ritt-Ende bekommt die
//    Zeile "freiheit: scene change allowed after end (<Grund>) ...". Ein laufendes Ereignis wird nie gesperrt.
//  - Kein Kurswechsel durch freiheit im Ritt (Entscheidung, kreisflug.md Nachtrag): im Ritt aendert nur der Stick die
//    Gier. Gleiten und positionelle Kollision bleiben. Die Kursdrehung an Flaechen gibt es nur im Testbuild mit
//    KFF_WALL_STEER=1 (voll) oder KFF_WALL_STEER=sanft (hoechstens 0,35 Grad je Frame), nur als Vergleichstest.
//  - Bestand: fopAcStts_UNK_0x20_e (Loeschregel bei mehr als 5000 unter Home) wird im Ritt geloescht.
//  - Durchflug-Sperre (E7-2): liegt das Ende eines Frames hinter einer Flaeche, geht der Vogel auf den Punkt kSkin vor
//    dem ersten Treffer zurueck (Zaehler durchflugKorr). Diagnose: durchdringungen (Rest-Durchdringung nach der Sperre, Soll 0), gleiten (Frames mit
//    Gleitschritt), wandSchritte, bodenlift, wasserlift, stillstandAusweg, linkUnterGrund. Log "freiheit: ...".
//  - E6 (Ecken-Stillstand unter Decke oder Ueberhang): Steigt der Vogel 6 Frames ohne Vorwaertsweg nicht frei, weicht er
//    entlang des Hindernisses nach UNTEN aus (hoechstens kSinkStep je Frame, bis die Vorwaertssicht frei ist). Dabei gilt statt der
//    Komfort-Untergrenze nur kEscBodyClear ueber dem Gelaende; Links Fuesse bleiben kLinkFootClear ueber dem Gelaende
//    (Linientest alt -> neu, kein Sinkschritt ohne Freiheit). Danach steigt er weich (kRiseStep je Frame) zur Komfort-
//    Untergrenze, aber nur, wenn Aufstieg und Vorwaertssicht in der hoeheren Lage frei sind (Abklingzeit kEscHold, kein
//    Pendeln). Die Gier bleibt unveraendert (nur der Stick dreht), der Landeanflug hat Vorrang.
//
// Konfiguration (mod.com_kaizuki_kargarok__flight.*): roof_limit (Standard aus), event_keep (ohne Wirkung seit D1),
//   min_ground_clearance (Standard -1 = Vanilla-Wert 300; Werte 60 bis 2000 werden geklemmt, wie config.cpp).
//
// Szenarien (KFF_AUTOTEST): innen (R_SP107 Kanalisation, Steigen unter die Decke, Gleiten, Absteigen, 30 s zu Fuss),
//   fuss (F_SP121: Absteigen am Boden, B, 30 s zu Fuss: Acch-Freigabe, Link bleibt auf dem Boden),
//   grenze (F_SP121 Geradeausflug nach Westen), szene (Vogel-Szenenwechsel im Ritt, abgewiesen), freiheit (F_SP121:
//   Frontalanflug auf eine Bergwand, Gleiten an der Wand, Sturzflug auf den Boden, Abfangen, Geradeausflug nach
//   Westen ueber den Kartenrand hinaus, Absteigen). WP1: ladezone-anflug, event39-kanal, boden-absteigen,
//   void-absteigen, abgrund-faelle, warp-portal, raumwechsel, rueckkehr-raum-tag (Abschnitt wp1). Jedes Szenario
//   endet spaetestens nach kScenarioDeadline Ticks.
// Testschalter (nur Testbuild, KFF_DEV_AUTOTEST): KFF_WALL_STEER=1 erlaubt freiheit die Kursdrehung an Flaechen
//   (Vergleichstest des Flugmodells), KFF_WALL_STEER=sanft nur die sanfte Gleitdrehung ohne Stillstands-Ausweg.
//   KFF_TEST_CRRPOS=1 laesst CrrPos im Ritt laufen (Kontrolllauf WP6.2). Die Szenarien (KFF_AUTOTEST) stehen nur im
//   Testbuild, dazu die WP6-Szenarien wp6-decke, wp6-wand, wp6-rand, wp6-ruf, wp6-timeout, wp6-gate, wp6-wasser und
//   wp6-doc (Beschreibung bei den Szenarien).
#include "d/dolzel_rel.h"

#include "core/kff/config.hpp"
#include "core/kff/kff.hpp"
#include "core/kff/ride.hpp"
#include "core/timers.hpp"  // WP9/R4: Kern-Ticks fuer das Messfenster des Link-Fusses (F06)

#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_m3d.h"
#include "SSystem/SComponent/c_m3d_g_pla.h"
#include "d/d_bg_s_acch.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_bg_s_roof_chk.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_kago.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_meter2_info.h"
#include "d/actor/d_a_scene_exit.h"
#include "d/actor/d_a_tag_ret_room.h"
#include "f_pc/f_pc_layer.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#if defined(KFF_DEV_AUTOTEST)
#include "d/d_camera.h"  // Testszenario raumwechsel (Kontrolle zu Fuss): Kamera-Gier
#include "d/actor/d_a_kytag04.h"  // Testszenario warp-portal: Portal-Markierung (Stage-Nr., Schalter)
#include "JSystem/JKernel/JKRAramArchive.h"  // Testszenario warp-portal: dat/portal.dat aus dem Kartenarchiv
#include "d/d_lib.h"  // Testszenario warp-portal: Groesse von dat/field.dat im Kartenarchiv
#include "d/d_cc_d.h"  // WP6: Schadensquelle (Testbuild)
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

constexpr float kNoRoof = 1.0e9f;             // keine Decke (checkRoofHeight, mRoofHeight)
constexpr float kNoFloor = -1.0e6f;           // keine Untergrenze (kein Gelaende unter dem Vogel)
constexpr float kRealRoofBelow = 1.0e8f;      // alles darueber ist "keine Decke" (G_CM3D_F_INF)
constexpr float kDefaultClearance = 300.0f;   // Vanilla-Mindesthoehe ueber dem Gelaende (mFlightGroundAltitude)
constexpr float kMinClearance = 60.0f;        // Link-Fuesse bleiben darueber
constexpr float kMaxClearance = 2000.0f;  // wie config.cpp (min_ground_clearance -1..2000)

constexpr float kSkin = 12.0f;                // Abstand zur Flaeche nach einem Treffer
constexpr int kSlideIters = 3;                // hoechstens so viele Gleitschritte je Frame
constexpr int kSamples = 7;                   // Abtastpunkte: Mitte, vier Seiten, oben, unten
constexpr float kSideR = 100.0f;              // Abstand der Seitenpunkte (Koerperradius des Vogels)
constexpr float kVertR = 80.0f;               // Abstand oben und unten
constexpr float kOffX[kSamples] = {0.0f, kSideR, -kSideR, 0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kOffY[kSamples] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, kVertR, -kVertR};
constexpr float kOffZ[kSamples] = {0.0f, 0.0f, 0.0f, kSideR, -kSideR, 0.0f, 0.0f};

constexpr int kSlideTurn = 0x0600;            // hoechstens 8,4375 Grad je Frame beim Gleiten an einer Flaeche (nur KFF_WALL_STEER=1)
constexpr int kSlideTurnSoft = 0x0040;        // KFF_WALL_STEER=sanft: hoechstens 0,35 Grad je Frame (kreisflug.md 5, nur Testbuild)
constexpr uint32_t kStallFrames = 6;          // E6: Frames mit Treffer und ohne Vorwaertsweg, bevor ausgewichen wird
constexpr float kStallProbeLen = 180.0f;      // freie Strecke seitlich des Vogels fuer den Stillstands-Ausweg (nur Testschalter)
constexpr float kClimbStep = 20.0f;           // Stillstand in einer Ecke: Ausweichen nach oben (und unten, E6) je Frame, Einheiten
constexpr float kEscBodyClear = kVertR + kSkin;  // E6 tief: Mittelpunkt des Vogels mindestens so hoch ueber dem Gelaende (F1: Abstand der unteren Abtastung plus Haut, sonst liegt sie im Boden)
constexpr float kRiseStep = 10.0f;            // E6: weiches Aufsteigen zur Komfort-Untergrenze, Einheiten je Frame
constexpr float kRiseProbe = 160.0f;          // E6: freie Vorwaertssicht in der hoeheren Lage vor jedem Aufstiegsschritt
constexpr uint32_t kEscHold = 20;             // E6: Frames Abklingzeit nach dem Ausweichen, bevor der Vogel wieder steigt
constexpr float kClimbReach = 120.0f;         // E6: hoechste Hoehe, bis zu der der Vogel nach oben nach freier Sicht sucht
constexpr float kSinkStep = 40.0f;            // E6: hoechster Abstieg je Frame beim Ausweichen nach unten, Einheiten
constexpr float kSinkMax = 800.0f;            // E6: hoechste Suchtiefe nach einer Hoehe mit freier Vorwaertssicht
constexpr float kSinkProbe = 10.0f;           // E6: Schrittweite der Suche nach unten
constexpr float kLinkFootClear = 50.0f;       // Fuesse von Link mindestens so hoch ueber Gelaende oder Wasser (flugbahn (d))
constexpr float kLiftPerTick = 40.0f;         // F1: hoechste Hebung des Vogels je Frame durch die Untergrenze (Einheiten), Korrektur unter 60
constexpr int kLookAhead = 16;                // F1: Vorausschau der Untergrenze in Frames entlang der Bewegung (Link-Fuss, Mindesthoehe)
constexpr float kBrakeCorr = 30.0f;           // F1: hoechste Kuerzung des Weges je Frame durch Bremsen (Einheiten), Korrektur unter 60
constexpr float kWallMaxNy = 0.5f;            // |Normale y| unter 0,5: Wand (steiler als 60 Grad)
constexpr float kFloorMinNy = 0.7f;           // Normale y ueber 0,7 nach oben: Boden (flacher als 45 Grad)
constexpr float kCeilMinNy = -0.7f;           // Normale y unter -0,7: Decke
constexpr int kWallMemSize = 6;               // Gedaechtnis fuer Wandrichtungen (Ecken)
constexpr uint32_t kWallMemFrames = 8;        // so lange gilt eine Wandrichtung nach dem letzten Treffer (Frames)
constexpr float kFeasibleTol = -0.05f;        // Richtung gilt als frei, wenn sie hoechstens ~3 Grad in eine Wand zeigt

// Acch-Bits, die dieses Modul am Vogel und an Link setzt (nur Bits, die vorher nicht gesetzt waren).
constexpr uint32_t kBirdBitsBase = dBgS_Acch::FLAG_WALL_NONE | dBgS_Acch::FLAG_LINE_CHECK_NONE;
constexpr uint32_t kRoofBit = dBgS_Acch::FLAG_ROOF_NONE;

// Zaehler fuer den Abschluss-Log und Log-Takte (Hooks laufen jeden Frame).
struct Stats {
    uint32_t roofOverride = 0;
    uint32_t birdSceneVeto = 0;
    uint32_t stageVeto = 0;
    uint32_t sceneCancel = 0;
    uint32_t frames = 0;          // Vogel-Frames mit Abtastung im Ritt
    uint32_t slideFrames = 0;     // Frames mit mindestens einem Gleitschritt
    uint32_t slideSteps = 0;      // Gleitschritte insgesamt
    uint32_t cornerSteer = 0;     // davon Kursaenderungen mit zwei oder mehr Wandrichtungen (Ecke)
    uint32_t wallSteer = 0;       // Kursaenderungen durch Gleiten an einer Flaeche (Ereignisse)
    uint32_t groundClamp = 0;     // Untergrenze (Gelaende) hat den Vogel angehoben
    uint32_t waterClamp = 0;      // Mindesthoehe ueber Wasser hat den Vogel angehoben
    uint32_t stallBreak = 0;      // Kurswechsel, weil der Vogel mit Treffer am Ort stand
    uint32_t penetration = 0;     // Endpunkt liegt nach der Durchflug-Sperre noch hinter einer Flaeche (Soll 0)
    uint32_t penetrationFix = 0;  // Durchflug erkannt und auf den Punkt vor dem ersten Treffer zurueckgesetzt
    uint32_t linkBelowGround = 0; // Link-Fuss unter dem Gelaende, ohne Ereignis, lebend (Soll 0)
    uint32_t exceptions = 0;
    float maxCorr = 0.0f;         // groesste Abweichung Ergebnis zu Ziel je Vogel-Frame (Einheiten)
    float minBirdGap = 1.0e9f;    // kleinster Abstand Vogel zum Gelaende nach der Korrektur
    float minLinkGap = 1.0e9f;    // kleinster Abstand Link-Fuss zum Gelaende (ohne Ereignis, lebend)
    uint32_t restartVeto = 0;     // checkRestartRoom im Ritt abgewiesen (Kern-Patch P-RESTART)
    uint32_t roofRestartVeto = 0; // checkRoofRestart im Ritt abgewiesen (Kern-Patch P-RESTART)
    uint32_t glideFrames = 0;     // Frames mit Treffer und echter Ortsaenderung entlang der Flaeche (Gleiten)
    uint32_t wallContacts = 0;    // Frames mit Wandkontakt (|Normale y| unter 0,5)
    uint32_t ceilContacts = 0;    // Frames mit Deckenkontakt
    uint32_t floorContacts = 0;   // Frames mit Bodenkontakt
    uint32_t abyssFrames = 0;     // Frames ohne Gelaende unter dem Vogel (Abgrund, Abstand nicht messbar)
    uint32_t steerEvents = 0;     // Kursaenderungen durch freiheit (Gleiten an Flaeche und Stillstand), Anzahl
    float steerDeg = 0.0f;        // Summe der Kursaenderungen durch freiheit in Grad (Betrag)
    uint32_t linkClamp = 0;       // Hoehe des Vogels angehoben, weil Links Fuesse sonst im Gelaende lagen
    uint32_t stallClimb = 0;      // Ausweichschritte nach oben bei Stillstand in einer Ecke (je kClimbStep)
    uint32_t stallSink = 0;       // Ausweichschritte nach unten bei Stillstand unter Decke oder Ueberhang (E6)
    uint32_t stallRunMax = 0;     // laengster Stillstand in Frames (Treffer ohne Vorwaertsweg)
    uint32_t escapeStarts = 0;    // E6: Ausweichen nach unten begonnen (Steigen durch Decke oder Ueberhang blockiert)
    uint32_t escapeBlocked = 0;   // E6: Sinkschritt abgelehnt (Gelaende darunter oder Links Fuss)
    uint32_t riseSteps = 0;       // E6: weiche Aufstiegsschritte zur Komfort-Untergrenze
    uint32_t riseBlocked = 0;     // E6: Aufstieg abgelehnt (Decke oder Vorwaertssicht in der hoeheren Lage)
    float escapeDepthMax = 0.0f;  // E6: groesste Tiefe unter der Hoehe am Beginn eines Ausweichens (Einheiten)
    float escLinkMin = 1.0e9f;    // E6: kleinster Abstand Links Fuss zum Gelaende beim Ausweichen (Soll mindestens 50)
};
Stats g_stats;
uint32_t g_logRoof = 0;
uint32_t g_logSteer = 0;
uint32_t g_logExc = 0;
uint32_t g_logSlide = 0;
uint32_t g_logHit = 0;
uint32_t g_logClamp = 0;
uint32_t g_logStall = 0;
uint32_t g_logPen = 0;
uint32_t g_logLinkGnd = 0;
uint32_t g_logClimb = 0;
uint32_t g_logEsc = 0;
uint32_t g_logRise = 0;
uint32_t g_logLinkLow = 0;

// Zustand von Pre (Startposition des Frames) bis Post (Ergebnis des Frames).
bool g_frameValid = false;
cXyz g_frameStart;
uint32_t g_stallCount = 0;
bool g_stallSink = false;     // E6: Steigen blockiert, der Vogel sinkt weiter, bis die Vorwaertssicht frei ist
bool g_low = false;           // E6: Vogel unter der Komfort-Untergrenze (Tief-Modus), steigt danach weich zurueck
float g_escTop = 0.0f;        // E6: Hoehe des Vogels bei Beginn des Ausweichens (Tiefe = Hoehe minus tiefster Punkt)
uint32_t g_escHold = 0;       // E6: Frames Abklingzeit nach dem Ausweichen, bevor der weiche Aufstieg beginnt
uint32_t g_birdOwned = 0;     // Acch-Bits des Vogels, die dieses Modul gesetzt hat
uint32_t g_linkOwned = 0;     // Acch-Bits von Link, die dieses Modul gesetzt hat
bool g_fotoWand = false;      // Foto-Marke "wand" und "decke" je Ritt nur einmal
bool g_fotoDecke = false;

// WP6: Decke (Pre-SKIP), Acch (CrrPos im Ritt aus), Neustart im Ritt, Stillstands-Waechter. Zaehler fuer SUMMARY6.
struct Wp6Stats {
    uint32_t roofSkip = 0;          // checkRoofHeight des eigenen Vogels per Pre-SKIP mit kNoRoof beantwortet
    uint32_t crrSkipFrames = 0;     // Ritt-Frames mit abgeschaltetem CrrPos (field_0x6dc gesetzt, Acch nachgefuehrt)
    uint32_t crrPosRuns = 0;        // Testbuild: CrrPos lief im Ritt trotzdem (Waechterwert veraendert), Soll 0
    uint32_t restartCalls = 0;      // daAlink_c::startRestartRoom im Ritt aufgerufen
    uint32_t restartVeto = 0;       // davon abgewiesen (Pre-SKIP: kein Ereignis, Link lebt)
    uint32_t restartPassed = 0;     // davon durchgelassen (Ereignis oder Tod), Soll 0 ohne Ereignis
    uint32_t restartOutside = 0;    // startRestartRoom ausserhalb des Rittes (Vanilla)
    uint32_t roofRestartOutside = 0;  // checkRoofRestart ausserhalb des Rittes nach einem Ritt (Vanilla)
    uint32_t stuckEnds = 0;         // Ritt-Enden durch den Stillstands-Waechter (EndReason::Stuck)
    uint32_t stuckRunMax = 0;       // laengster Stillstand ohne Stick (Vogel-Frames = Kern-Ticks ohne Pause)
    uint32_t skipFrames = 0;        // Ritt-Frames ohne Abtastung (kein gueltiger Frame-Start), Soll 0
#if defined(KFF_DEV_AUTOTEST)
    uint32_t roofNotNone = 0;       // Ritt-Frames mit mRoofHeight != kNoRoof nach dem Vogel-Execute (Soll 0)
    uint32_t roofActions = 0;       // Bitmaske der Aktionen (mAction) im Ritt, je Frame mit kNoRoof gesehen
    uint32_t roofModes = 0;         // Bitmaske der Modi (mMode, 0..31) im Ritt, je Frame mit kNoRoof gesehen
    uint32_t tunnelFrames = 0;      // Ritt-Frames mit Tunnel-Zusatzweg (Szenario wp6-wand)
    uint32_t releaseMax = 0;        // E10: Frames vom Stickwechsel im Stillstand bis zur Bewegung (groesster Wert)
    uint32_t releases = 0;          // E10: gemessene Befreiungen
#endif
};
Wp6Stats g_wp6;
uint32_t g_logRoofSkip = 0;
uint32_t g_logRestartWp6 = 0;
uint32_t g_logCrr = 0;
constexpr uint32_t kStuckTicks = 300;   // Waechter: so viele Vogel-Frames (Kern-Ticks ohne Pause) Stillstand ohne Stick
// Stillstand: Vogel bleibt in diesem Abstand zum Ankerpunkt. 150 statt 25 (Lauf wp6p-wand-1): an der Kartenwand zitterte
// der festsitzende Vogel 20 bis 40 Einheiten je Frame (Ausweichen hoch/runter), der kleine Umkreis setzte den Anker
// staendig neu (stuckRunMax 28 bei ueber 1000 Ticks Stillstand). Frei fliegend legt der Vogel auch in Gang 1 mit
// Bremse (30 Prozent von V) 150 Einheiten in wenigen Frames zurueck, der Anker wandert dann mit.
constexpr float kStuckRadius = 150.0f;
constexpr float kStickXDead = 0.02f;    // wie flug: Gier-Totzone
constexpr float kStickYDead = 0.10f;    // wie flug: Trimm-Totzone (darunter gilt Stick Y als neutral)
bool g_crrOwned = false;       // field_0x6dc des Vogels ist von freiheit gesetzt (CrrPos im Ritt aus)
uint8_t g_crrSaved = 0;        // Wert davor, am Ritt-Ende zurueck (R14: nur Eigenes zuruecknehmen)
cXyz g_stuckAnchor;            // Waechter: Ankerpunkt des Stillstands
uint32_t g_stuckTicks = 0;     // Waechter: Vogel-Frames in Folge am Anker ohne Stick
bool g_stuckPending = false;   // Waechter: Ritt-Ende steht an (onFrameLink, Link-Frame wie die Kern-Enden)
bool g_rideEndedOnce = false;  // nach einem Ritt: erster Vanilla-Aufruf von checkRoofRestart/startRestartRoom im Log
bool g_roofRestartLogged = false;
bool g_restartOutsideLogged = false;
#if defined(KFF_DEV_AUTOTEST)
constexpr float kCrrSentinel = -12345678.0f;  // Waechterwert in mObjAcch.field_0xb8 (nur CrrPos schreibt dort pos.y)
bool g_crrArmed = false;
float g_tunnelExtra = 0.0f;    // Szenario wp6-wand: Zusatzweg je Frame in Kursrichtung (Tunnel-Probe 6.7)
uint32_t g_logRoofDiag = 0;
uint32_t g_dmgCount = 0;       // Schadensdiagnose: setDamagePoint-Aufrufe mit Betrag > 0 (Link)
uint32_t g_dmgRide = 0;        // davon im Ritt
uint32_t g_releaseStart = 0;   // E10: Frame-Nummer des Stickwechsels im Stillstand, 0 = keine Messung
float g_lastStickX = 0.0f;
float g_lastStickY = 0.0f;
#endif

// F1: Vanilla checkHeight() (Anfang von executeFly) setzt den Vogel auf mGroundHeight bzw. mRoofHeight, wenn er darunter bzw.
// darueber liegt. Der Pre-Hook legt mGroundHeight auf die volle Untergrenze; dieser Sprung ist keine Vogelbewegung. Der
// Pre-Hook merkt sich die Werte dieses Frames, damit onBirdExecutePost den Sprung herausrechnet. Die Globals gelten auch
// im Release, deshalb ausserhalb des Testblocks.
bool g_snapArmed = false;
float g_snapPreY = 0.0f;
float g_snapGround = kNoFloor;
float g_snapRoof = kNoRoof;
uint32_t g_logBrake = 0;        // F1: Zaehler der Brems-Logzeile
cXyz g_linkPrev;                // F1: Link-Position am Frame-Beginn des vorigen Frames (Geschwindigkeit des Links)
bool g_linkPrevOn = false;
#if defined(KFF_DEV_AUTOTEST)
float g_dbgRaw = 0.0f;   // F1-Diagnose: ungekappte Hebung je Frame (nur Testbuild)
uint32_t g_logDiag3 = 0; // F1-Diagnose: Zaehler der Zeile DIAG (nur Testbuild)
#endif

// ---------------------------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------------------------

bool roofKept() {
    return kff::config::getBool(kff::config::Key::RoofLimit);
}

// Mindest-Bodenfreiheit. Negative Werte ergeben den Vanilla-Wert, sonst geklemmt auf 60 bis 2000.
float clearance() {
    const double c = kff::config::getFloat(kff::config::Key::MinGroundClearance);
    if (c < 0.0) {
        return kDefaultClearance;
    }
    if (c < static_cast<double>(kMinClearance)) {
        return kMinClearance;
    }
    if (c > static_cast<double>(kMaxClearance)) {
        return kMaxClearance;
    }
    return static_cast<float>(c);
}

// Ereignis laeuft: das Ereignis-System oder Link selbst (Demo mit eigener Steuerung, z. B. nach einem Szenenwechsel).
bool inEvent() {
    if (dComIfGp_event_runCheck() != 0) {
        return true;
    }
    const daAlink_c* link = kff::liveLink();
    return link != nullptr && link->checkEventRun() != 0;
}

// Kursdrehung durch freiheit (Gleiten an Flaechen, Stillstands-Ausweg). Entscheidung (kreisflug.md, Nachtrag): im Ritt
// aendert nur der Stick die Gier. Nur das Testbuild kennt den Schalter KFF_WALL_STEER (Vergleichstest WP8.2):
// "1" = voll (kSlideTurn je Frame und Stillstands-Ausweg), "sanft" = hoechstens kSlideTurnSoft je Frame beim Gleiten,
// ohne Stillstands-Ausweg. Im Release gibt es keine Kursdrehung durch freiheit.
enum class WallSteer : uint8_t { Off, Soft, Full };

WallSteer wallSteerMode() {
#if defined(KFF_DEV_AUTOTEST)
    static const WallSteer mode = [] {
        const char* e = std::getenv("KFF_WALL_STEER");
        if (e == nullptr) {
            return WallSteer::Off;
        }
        if (e[0] == '1') {
            return WallSteer::Full;
        }
        return std::strcmp(e, "sanft") == 0 ? WallSteer::Soft : WallSteer::Off;
    }();
    return mode;
#else
    return WallSteer::Off;
#endif
}

bool wallSteerOn() {
    return wallSteerMode() != WallSteer::Off;
}

// Hoechste Kursdrehung je Frame beim Gleiten an einer Flaeche (nur mit wallSteerOn()).
int slideTurnCap() {
    return wallSteerMode() == WallSteer::Soft ? kSlideTurnSoft : kSlideTurn;
}

// Bezeichnung des Schalters fuer das Ritt-Log. Der Testschalter selbst nennt sich nur im Testbuild.
const char* wallSteerLabel() {
#if defined(KFF_DEV_AUTOTEST)
    switch (wallSteerMode()) {
    case WallSteer::Full:
        return "AN voll (nur Testbuild, KFF_WALL_STEER=1)";
    case WallSteer::Soft:
        return "AN sanft (nur Testbuild, KFF_WALL_STEER=sanft)";
    default:
        return "AUS";
    }
#else
    return "AUS";
#endif
}

bool rideRunning() {
    return kff::session().phase == kff::Phase::Riding;
}

// WP6, Testbuild: KFF_TEST_CRRPOS=1 laesst CrrPos im Ritt laufen (Kontrolllauf zu 6.2). Release: immer aus.
bool crrPosKept() {
#if defined(KFF_DEV_AUTOTEST)
    static const bool keep = kff::testFlag("KFF_TEST_CRRPOS");
    return keep;
#else
    return false;
#endif
}

// WP6 (Aufgabe 4): CrrPos des Vogels im Ritt aus. daKago_c::action ruft mObjAcch.CrrPos nur bei field_0x6dc == 0
// (d_a_kago.cpp:3620); sonst liest das Spiel field_0x6dc nirgends. Der Wert davor wird einmal je Ritt gemerkt und am
// Ritt-Ende zurueckgegeben. Die Bewegung prueft freiheit selbst (Abtastung, Untergrenze); den Acch-Zustand, den das
// Spiel liest (Schatten: m_gnd und m_ground_h), fuehrt refreshAcch nach.
void crrPosOff(daKago_c* bird) {
#if defined(KFF_DEV_AUTOTEST)
    // Waechterwert auch im Kontrolllauf: CrrPos setzt field_0xb8 = pos.y (d_bg_s_acch.cpp:354), bevor GroundCheck es
    // liest; sonst liest niemand das Feld. Der Post zaehlt, ob CrrPos lief.
    bird->mObjAcch.field_0xb8 = kCrrSentinel;
    g_crrArmed = true;
#endif
    if (crrPosKept()) {
        return;
    }
    if (!g_crrOwned) {
        g_crrSaved = bird->field_0x6dc;
        g_crrOwned = true;
    }
    bird->field_0x6dc = 1;
}

void crrPosRestore(daKago_c* bird) {
    if (g_crrOwned && bird != nullptr) {
        bird->field_0x6dc = g_crrSaved;
    }
    g_crrOwned = false;
#if defined(KFF_DEV_AUTOTEST)
    g_crrArmed = false;
#endif
}

// Acch-Zustand ohne CrrPos: Bodenpolygon und Bodenhoehe fuer den Schatten (daKago_c::draw), Trefferbits aus (der Vogel
// beruehrt im Flug nichts; Treffer meldet die Abtastung von freiheit).
void refreshAcch(daKago_c* k) {
    if (!g_crrOwned) {
        return;
    }
    dBgS_Acch& a = k->mObjAcch;
    cXyz gp(k->current.pos.x, k->current.pos.y + 50.0f, k->current.pos.z);
    a.m_gnd.SetPos(&gp);
    a.m_ground_h = dComIfG_Bgsp().GroundCross(&a.m_gnd);
    a.m_flags &= ~(static_cast<u32>(dBgS_Acch::FLAG_WALL_HIT) | static_cast<u32>(dBgS_Acch::FLAG_GROUND_HIT) |
                   static_cast<u32>(dBgS_Acch::FLAG_GROUND_LANDING) | static_cast<u32>(dBgS_Acch::FLAG_GROUND_AWAY) |
                   static_cast<u32>(dBgS_Acch::FLAG_ROOF_HIT) | static_cast<u32>(dBgS_Acch::FLAG_LINE_CHECK_HIT));
    ++g_wp6.crrSkipFrames;
}

// Hinweis im Spiel (UiService optional, sonst nur Log).
void notice(const char* text) {
    if (svc_ui == nullptr) {
        kff::logInfo("freiheit: Hinweis (kein UI-Dienst): %s", text);
        return;
    }
    UiToastDesc d = UI_TOAST_DESC_INIT;
    d.body_rml = text;
    d.duration_ms = 4000;
    if (svc_ui->push_toast(mod_ctx, &d) != MOD_OK) {
        kff::logWarn("freiheit: Hinweis abgelehnt: %s", text);
    }
}

// Link fehlt oder ist tot: dann keine Abbruch-Logik, damit der Restart durchlaeuft.
bool linkDead(const daAlink_c* link) {
    return link == nullptr || link->mProcID == daAlink_c::PROC_DEAD;
}

// ---------------------------------------------------------------------------------------------
// Szenenwechsel im Ritt (WP1; Entscheidungen D1, ORCH-2, V13): Protokoll
// ---------------------------------------------------------------------------------------------
// Jeder Stage-Wechsel-Versuch im Ritt bekommt genau eine Logzeile:
//  - "freiheit: positional scene veto (<Aufrufer>) ..." fuer positionsgetriebene Wechsel. Ein Versuch, der sich Frame
//    fuer Frame wiederholt (Link bleibt im Ausgang), ist eine Episode: eine neue Zeile erst nach kVetoGap Ticks ohne
//    Versuch an derselben Stelle. Die Zaehler zaehlen jeden Versuch.
//  - "freiheit: scene change allowed after end (<Grund>) ..." fuer den ersten Stage-Wechsel nach einem Ritt-Ende
//    (Ereignis, Warp, Menue, Laden, Tod, Absteigen). Die Zeile entsteht, sobald der Wechsel ansteht
//    (dComIfGp_isEnableNextStage, steigende Flanke im eigenen Tick).
enum VetoSite : int {
    kVsCheckScene = 0,  // daAlink_c::checkSceneChange: Ladezone (SCENE_EXIT), Ausgangs-Polygon, Nebel
    kVsChangeScene,     // dStage_changeScene ohne Ereignis (anderer Aufrufer)
    kVsRestart,         // daAlink_c::checkRestartRoom: Lava, Neustart-Boden (Attribut 0x80), Abgrund
    kVsRoofRestart,     // daAlink_c::checkRoofRestart: Kopf an der Decke bei Bodenkontakt
    kVsStartRestart,    // daAlink_c::startRestartRoom ohne Ereignis (Sicherheitsnetz)
    kVsRetRm,           // daTagRetRm_c: Rueckkehr-Raum-Quader
    kVsBirdScene,       // daKago_c::setSceneChange des eigenen Vogels
    kVsFlyScene,        // daKago_c::checkFlySceneChange des eigenen Vogels (Pfadende im Flussgebiet)
    kVsPending,         // anstehender Stage-Wechsel im Ritt ohne Ereignis
    kVsCount
};
constexpr const char* kVetoSiteName[kVsCount] = {"daAlink_c::checkSceneChange", "dStage_changeScene",
    "daAlink_c::checkRestartRoom", "daAlink_c::checkRoofRestart", "daAlink_c::startRestartRoom", "daTagRetRm_c",
    "daKago_c::setSceneChange", "daKago_c::checkFlySceneChange", "pending next stage"};
constexpr uint32_t kVetoGap = 30;        // Ticks ohne Versuch, danach beginnt an derselben Stelle eine neue Episode
constexpr uint32_t kAllowWindow = 5400;  // Ticks (3 min) nach einem Ritt-Ende, in denen der naechste Wechsel ihm gilt
constexpr int kRiverSceneType = 1;       // daKago_c::mSceneType SCENE_TYPE_RIVER (d_a_kago.cpp:33)

struct VetoCount {
    uint32_t tries = 0;  // Versuche (Aufrufe mit ausloesender Bedingung)
    uint32_t lines = 0;  // Episoden (Logzeilen)
    uint32_t last = 0;   // eigener Tick des letzten Versuchs
};
VetoCount g_veto[kVsCount];
uint32_t g_tick = 0;             // eigener Tick (mDoCPd_c::read, Post: einmal je Frame)
uint32_t g_vetoLines = 0;        // Zeilen "positional scene veto"
uint32_t g_allowLines = 0;       // Zeilen "scene change allowed after end"
uint32_t g_ownEnds = 0;          // Ritt-Enden durch dieses Modul (Warp, Ereignis, Menue)
uint32_t g_unsorted = 0;         // Stage-Wechsel, die im laufenden Ritt anstanden (Soll 0)
uint32_t g_sceneSkips = 0;       // daAlink_c::checkSceneChange im Ritt uebersprungen (je Frame)
uint32_t g_retRmSkips = 0;       // daTagRetRm_c::execute im Ritt uebersprungen
uint32_t g_restartInRide = 0;    // daAlink_c::startRestartRoom im Ritt aufgerufen (Soll 0 ohne Ereignis)
bool g_nextPrev = false;         // dComIfGp_isEnableNextStage im letzten eigenen Tick
bool g_endOpen = false;          // Ritt-Ende ohne zugeordneten Stage-Wechsel
uint32_t g_endTick = 0;
kff::EndReason g_endWhy = kff::EndReason::DismountButton;
bool g_endTextOwn = false;       // g_endText kommt von endForScene (dieses Modul)
char g_endText[112] = {};

// Neue Episode an dieser Stelle? Zaehlt den Versuch. true: der Aufrufer schreibt die Zeile.
bool vetoTry(VetoSite s) {
    VetoCount& v = g_veto[s];
    const bool fresh = v.tries == 0 || g_tick - v.last > kVetoGap;
    ++v.tries;
    v.last = g_tick;
    if (fresh) {
        ++v.lines;
        ++g_vetoLines;
    }
    return fresh;
}

// Sperre aktiv: Ritt laeuft, Wechsel positionsgetrieben (Kern-Einordnung, V13), kein Ereignis, Link lebt. Ein laufendes
// Ereignis und ein toter Link werden nie gesperrt (kein Veto, das ein Ereignis haengen laesst).
bool sceneVetoActive() {
    return rideRunning() && kff::ride::isPositionDriven() && !inEvent() && !linkDead(kff::liveLink());
}

// Ritt sauber beenden, bevor ein nicht positionsgetriebener Wechsel durchlaeuft (ORCH-2: im Zweifel Ende statt Veto).
// onRideEnd (Modul) merkt Grund und Tick fuer die Zeile "scene change allowed after end".
void endForScene(kff::EndReason why, const char* text) {
    std::snprintf(g_endText, sizeof(g_endText), "%s", text != nullptr ? text : "");
    g_endTextOwn = true;
    ++g_ownEnds;
    kff::endRideClean(why, text != nullptr ? text : "freiheit");
}

// Ein Stage-Wechsel steht an (steigende Flanke). Genau eine Zeile, wenn er einem Ritt-Ende folgt.
void noteStageChange() {
    const char* stage = dComIfGp_getNextStageName();
    const int point = static_cast<int>(dComIfGp_getNextStagePoint());
    const int room = static_cast<int>(dComIfGp_getNextStageRoomNo());
    const int layer = static_cast<int>(dComIfGp_getNextStageLayer());
    if (rideRunning()) {
        // Keine Sperre und kein Ende hat gegriffen (Soll 0). Die Anforderung ist schon gestellt (dScnPly_Draw im selben
        // Frame), eine Sperre kaeme zu spaet: Ritt beenden, damit der Vogel nicht mit dem alten Raum verschwindet.
        ++g_unsorted;
        kff::logWarn("freiheit: Stage-Wechsel im Ritt ohne Einordnung (-> %s), Ritt wird beendet",
            stage != nullptr ? stage : "?");
        endForScene(inEvent() ? kff::EndReason::Event : kff::EndReason::Menu, "Stage-Wechsel ohne Einordnung im Ritt");
    }
    if (g_endOpen && g_tick - g_endTick <= kAllowWindow) {
        ++g_allowLines;
        kff::logInfo("freiheit: scene change allowed after end (%s) %s -> %s Punkt %d Raum %d Layer %d, %u Ticks nach dem "
                     "Ende",
            kff::endReasonName(g_endWhy), g_endText[0] != '\0' ? g_endText : "Ende durch den Kern",
            stage != nullptr ? stage : "?", point, room, layer, static_cast<unsigned>(g_tick - g_endTick));
    }
    g_endOpen = false;
}

// Anstehender Stage-Wechsel im Ritt ohne Ereignis (Vogel- und Link-Post): zuruecknehmen. Hat der Aufrufer in diesem
// Frame ein Pflichtereignis bestellt, wuerde die Ruecknahme es haengen lassen: dann Ritt-Ende statt Sperre (ORCH-2).
void cancelPendingScene(const char* where) {
    if (!rideRunning() || inEvent() || dComIfGp_isEnableNextStage() == 0) {
        return;
    }
    if (linkDead(kff::liveLink())) {
        return;
    }
    const char* stage = dComIfGp_getNextStageName();
    const int point = static_cast<int>(dComIfGp_getNextStagePoint());
    const int room = static_cast<int>(dComIfGp_getNextStageRoomNo());
    dEvt_control_c* ev = dComIfGp_getEvent();
    if (ev != nullptr && ev->checkCompulsory() != 0) {
        char text[96];
        std::snprintf(text, sizeof(text), "anstehender Stage-Wechsel %s mit Pflichtereignis (%s)",
            stage != nullptr ? stage : "?", where);
        endForScene(kff::EndReason::Event, text);
        return;
    }
    dComIfGp_offEnableNextStage();
    ++g_stats.sceneCancel;
    if (vetoTry(kVsPending)) {
        kff::logInfo("freiheit: positional scene veto (%s, %s) -> %s Punkt %d Raum %d, kein Ereignis",
            kVetoSiteName[kVsPending], where, stage != nullptr ? stage : "?", point, room);
    }
}

// Acch-Bits setzen, die noch nicht gesetzt sind, und merken. Vanilla-Bits bleiben unangetastet.
void ownBits(uint32_t& flags, uint32_t& owned, uint32_t want) {
    const uint32_t add = want & ~flags;
    if (add != 0) {
        flags |= add;
        owned |= add;
    }
}

// Nur die selbst gesetzten Bits zuruecknehmen.
void releaseBits(uint32_t& flags, uint32_t& owned) {
    flags &= ~owned;
    owned = 0;
}

// Foto-Marke fuer den Testrunner: Konsolenzeile "kff: freiheit: FOTO <name> ...". Nur bei aktivem Autotest.
void photo(const char* tag, uint32_t f) {
#if defined(KFF_DEV_AUTOTEST)
    if (kff::autotest::active() != nullptr) {
        kff::logInfo("freiheit: FOTO %s (Ritt-Frame %u)", tag, f);
    }
#else
    (void)tag;
    (void)f;
#endif
}

#if defined(KFF_DEV_AUTOTEST)
// WP5 (Testbuild): KFF_WP5_OHNE=1 oder KFF_WP5_OHNE=boden: Wasser zaehlt nicht als Flaeche (Vergleichslauf ohne die
// Wasser-Untergrenze D3/K17, Schwimm-Ping). Im Release gibt es den Schalter nicht.
bool wp5IgnoreWater() {
    static const bool on = [] {
        const char* v = std::getenv("KFF_WP5_OHNE");
        if (v == nullptr) {
            return false;
        }
        const bool one = v[0] == '1' && v[1] == '\0';
        const bool boden = v[0] == 'b' && v[1] == 'o' && v[2] == 'd' && v[3] == 'e' && v[4] == 'n' && v[5] == '\0';
        return one || boden;
    }();
    return on;
}
#endif

// Hoechste Flaeche unter dem Punkt: der hoechste Boden (GroundCross) oder die hoechste Wasserflaeche (Spl-Gruppe,
// dBgS_ObjGndChk_Spl). Der Startpunkt liegt wie im Spiel 500 ueber dem Punkt. -G_CM3D_F_INF, wenn nichts darunter liegt.
// *water wird true, wenn die Wasserflaeche die hoechste ist.
float surfaceAt(const cXyz& pos, bool* water) {
    cXyz start(pos.x, pos.y + 500.0f, pos.z);
    dBgS_GndChk gnd;
    gnd.SetPos(&start);
    const float ground = dComIfG_Bgsp().GroundCross(&gnd);
    dBgS_ObjGndChk_Spl spl;
    spl.SetPos(&start);
    const float wtr = dComIfG_Bgsp().GroundCross(&spl);
    float best = -G_CM3D_F_INF;
    bool isWater = false;
    if (ground != -G_CM3D_F_INF) {
        best = ground;
    }
#if defined(KFF_DEV_AUTOTEST)
    if (wp5IgnoreWater()) {
        if (water != nullptr) {
            *water = false;
        }
        return best;
    }
#endif
    if (wtr != -G_CM3D_F_INF && wtr > best) {
        best = wtr;
        isWater = true;
    }
    if (water != nullptr) {
        *water = isWater;
    }
    return best;
}

// Landeanflug nach B: Session.landing (Kern) oder Session.landFrames > 0 (Modulkern zaehlt den Anflug dort hoch,
// ride.cpp Zeile 704 und 1001). Im Anflug entfaellt die Mindesthoehe.
bool landingActive() {
    const auto& s = kff::session();
    return s.landing || s.landFrames > 0;
}

// Der Aufstieg auf die Komfort-Untergrenze ist frei (keine Decke, kein Ueberhang zwischen Vogel und dieser Hoehe).
// Definition hinter sweep().
bool comfortReachable(const cXyz& pos, float comfortY);

// Abstand eines Punktes zum Gelaende darunter (GroundCross, ohne Wasser). 1e9, wenn kein Gelaende da ist.
float groundGap(const cXyz& p) {
    cXyz start(p.x, p.y + 500.0f, p.z);
    dBgS_GndChk gnd;
    gnd.SetPos(&start);
    const float ground = dComIfG_Bgsp().GroundCross(&gnd);
    if (ground == -G_CM3D_F_INF) {
        return 1.0e9f;
    }
    return p.y - ground;
}

// Untergrenze des Vogels durch Links Fuesse (flugbahn (d)): Link haengt mit dem Versatz dy unter dem Vogel. Seine
// Fuesse sollen kLinkFootClear ueber der hoechsten Flaeche unter Link liegen (Bodensonde unter Link selbst). Umgerechnet
// auf die Hoehe des Vogels. Im Landeanflug ohne Mindesthoehe. kNoFloor, wenn Link nicht im Ritt ist oder nichts darunter liegt.
// Link haengt im Ritt am Vogel: die Fuss-Untergrenze gilt.
bool linkFloorOn(const daAlink_c* link) {
    return link != nullptr && !linkDead(link) && rideRunning() && !inEvent();
}

float linkFloorForBird(const cXyz& pos, const daAlink_c* link) {
    if (!linkFloorOn(link)) {
        return kNoFloor;
    }
    const float surf = surfaceAt(link->current.pos, nullptr);
    if (surf == -G_CM3D_F_INF) {
        return kNoFloor;
    }
    // F1: der Abstand Link-Vogel kommt aus der Position dieses Frames (pos), nicht aus k->current.pos. Dort steht nach
    // checkHeight schon der Sprung, und die Untergrenze haette ihn ein zweites Mal angehoben (Befund 2 und 3).
    const float refY = pos.y;
    const float dy = link->current.pos.y - refY;
    return surf + (landingActive() ? 0.0f : kLinkFootClear) - dy;
}

// Untergrenze des Vogels fuer einen Punkt (K17, wie daKago_c::checkGroundHeight d_a_kago.cpp:329-381), E6: hoechste
// Flaeche plus min_ground_clearance (Standard 300) im Normalbetrieb, solange der Aufstieg dorthin frei ist. Im Tief-Modus
// (Ausweichen nach unten, Abklingzeit, weicher Aufstieg) und wenn Decke oder Ueberhang den Aufstieg versperren, gilt nur
// kEscBodyClear ueber der Flaeche: die Komfort-Untergrenze drueckt den Vogel dann nicht in die Decke. Im Landeanflug ohne
// Mindesthoehe. Die Link-Fuss-Grenze gilt immer. byLink wird true, wenn Links Fuesse die Grenze bestimmen; water, wenn
// Wasser. kNoFloor, wenn keine Flaeche darunter liegt (und Link keine Grenze setzt).
float birdFloor(const cXyz& pos, const daKago_c* k, const daAlink_c* link, bool* water, bool* byLink) {
    bool w = false;
    const float surf = surfaceAt(pos, &w);
    if (water != nullptr) {
        *water = w;
    }
    if (byLink != nullptr) {
        *byLink = false;
    }
    float f = kNoFloor;
    if (surf != -G_CM3D_F_INF) {
        if (landingActive()) {
            f = surf;
        } else if (g_stallSink || g_low || !comfortReachable(pos, surf + clearance())) {
            f = surf + kEscBodyClear;
        } else {
            f = surf + clearance();
        }
    }
    const float lf = linkFloorForBird(pos, link);
    if (lf != kNoFloor && (f == kNoFloor || lf > f)) {
        f = lf;
        if (byLink != nullptr) {
            *byLink = true;
        }
    }
    return f;
}

// F1: Hebung dieses Frames mit Vorausschau. j = 0: die Untergrenze jetzt (birdFloor: Komfort-Untergrenze, Link-Fuss an der
// aktuellen Stelle). Fuer j = 1 .. kLookAhead fliegt der Vogel weiter wie in diesem Frame: Verschiebung v und Hoehe vy je
// Frame, Link haengt mit dem Versatz dLink darunter. Dort muss er hoch genug sein: Mindesthoehe kEscBodyClear bzw. Link-Fuss
// kLinkFootClear. Die Hebung je Frame ist auf kLiftPerTick begrenzt. Also muss er jetzt schon um
// req_j - y - j * (vy + kLiftPerTick) steigen, damit er dort reicht. Rueckgabe: die Hebung dieses Frames (0 bis kLiftPerTick).
float floorLift(const cXyz& q, const cXyz& v, float vy, const daKago_c* k, const daAlink_c* link, const cXyz& dLink,
                const cXyz& linkPos, const cXyz& vL, bool* water, bool* byLink) {
    const float escC = kEscBodyClear;
    const float req0 = birdFloor(q, k, link, water, byLink);
    float lift = req0 != kNoFloor ? req0 - q.y : 0.0f;
    const bool landing = landingActive();
    for (int j = 1; j <= kLookAhead; ++j) {
        const float x = q.x + v.x * static_cast<float>(j);
        const float z = q.z + v.z * static_cast<float>(j);
        float req = kNoFloor;
        const float s = surfaceAt(cXyz(x, q.y, z), nullptr);
        if (s != -G_CM3D_F_INF) {
            req = s + (landing ? 0.0f : escC);
        }
        if (linkFloorOn(link)) {
            const float ls = surfaceAt(cXyz(linkPos.x + vL.x * static_cast<float>(j), q.y + dLink.y,
                                            linkPos.z + vL.z * static_cast<float>(j)),
                                       nullptr);
            if (ls != -G_CM3D_F_INF) {
                const float lf = ls + (landing ? 0.0f : kLinkFootClear) - dLink.y;
                if (lf > req) {
                    req = lf;
                }
            }
        }
        if (req != kNoFloor) {
            lift = std::max(lift, req - q.y - static_cast<float>(j) * (vy + kLiftPerTick));
        }
    }
#if defined(KFF_DEV_AUTOTEST)
    g_dbgRaw = std::max(lift, 0.0f);  // F1-Diagnose: ungekappte Hebung dieses Frames
#endif
    return std::min(std::max(lift, 0.0f), kLiftPerTick);
}

// F1: Bremsprobe. Der Vogel fliegt mit dem Anteil f seines Weges dv je Frame weiter (Hoehe q.y, steigt hoechstens
// kLiftPerTick je Frame, sinkt wie bisher). Links Fuesse (Versatz dLink) und der Vogel muessen dort ueber kLookAhead Frames
// erreichbar bleiben: Hoehe req <= q.y + j * (vy + kLiftPerTick). Die Probe prueft Boden des Vogels und der Fuesse.
bool brakeFeasible(const cXyz& start, const cXyz& q, const cXyz& dv, float f, const cXyz& dLink, const cXyz& vL) {
    const float vy = q.y - start.y;
    const float escC = kEscBodyClear;
    const bool landing = landingActive();
    const float clr = landing ? 0.0f : kLinkFootClear;
    for (int j = 1; j <= kLookAhead; ++j) {
        const float reach = q.y + static_cast<float>(j) * (vy + kLiftPerTick);
        const float x = start.x + dv.x * f * static_cast<float>(j + 1);
        const float z = start.z + dv.z * f * static_cast<float>(j + 1);
        const float s = surfaceAt(cXyz(x, q.y, z), nullptr);
        if (s != -G_CM3D_F_INF && s + (landing ? 0.0f : escC) > reach) {
            return false;
        }
        const float ls = surfaceAt(cXyz(start.x + dLink.x + vL.x * f * static_cast<float>(j + 1), q.y + dLink.y,
                                        start.z + dLink.z + vL.z * f * static_cast<float>(j + 1)),
                                   nullptr);
        if (ls != -G_CM3D_F_INF && ls + clr - dLink.y > reach) {
            return false;
        }
    }
    return true;
}

// Ergebnis einer Abtastung: frueheste Flaeche entlang der Strecke.
struct Contact {
    bool hit = false;
    float t = 1.0f;       // Anteil der Strecke bis zum Treffer, 0 bis 1
    cXyz n;               // Flaechennormale, zeigt zum Vogel (gegen die Bewegung)
    cXyz at;              // Trefferpunkt des Abtastpunkts
    int sample = -1;
    int special = 0;      // Attribute (nur Log und Befundsuche)
    int wallCode = 0;
    int att0 = 0;
    int att1 = 0;
    int ground = 0;
};

// Filter: welche Flaechen den Vogel stoppen. Im Moment alle. Unsichtbare Kartenrand-Wände werden nach den
// Attributen unterschieden, sobald der Befund aus dem Log vorliegt (Log "freiheit: treffer ...").
bool solid(const dBgS_LinChk& lin) {
    (void)lin;
    return true;
}

// Frueheste Flaeche entlang der Verschiebung d, gesucht von allen Abtastpunkten um den Mittelpunkt p0.
void sweep(const cXyz& p0, const cXyz& d, Contact& best) {
    best = Contact();
    const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
    if (len2 < 1.0e-6f) {
        return;
    }
    for (int i = 0; i < kSamples; ++i) {
        cXyz s(p0.x + kOffX[i], p0.y + kOffY[i], p0.z + kOffZ[i]);
        cXyz e(s.x + d.x, s.y + d.y, s.z + d.z);
        dBgS_LinChk lin;
        lin.Set(&s, &e, NULL);
        if (!dComIfG_Bgsp().LineCross(&lin)) {
            continue;
        }
        if (!solid(lin)) {
            continue;
        }
        const cXyz c = lin.GetCross();
        float t = ((c.x - s.x) * d.x + (c.y - s.y) * d.y + (c.z - s.z) * d.z) / len2;
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
        if (t >= best.t && best.hit) {
            continue;
        }
        cM3dGPla pla;
        dComIfG_Bgsp().GetTriPla(lin, &pla);
        const float nl = std::sqrt(pla.mNormal.x * pla.mNormal.x + pla.mNormal.y * pla.mNormal.y +
                                   pla.mNormal.z * pla.mNormal.z);
        if (nl < 1.0e-6f) {
            continue;
        }
        cXyz n(pla.mNormal.x / nl, pla.mNormal.y / nl, pla.mNormal.z / nl);
        if (n.x * d.x + n.y * d.y + n.z * d.z > 0.0f) {
            n.x = -n.x;
            n.y = -n.y;
            n.z = -n.z;
        }
        best.hit = true;
        best.t = t;
        best.n = n;
        best.at = c;
        best.sample = i;
        best.special = dComIfG_Bgsp().GetSpecialCode(lin);
        best.wallCode = dComIfG_Bgsp().GetWallCode(lin);
        best.att0 = dComIfG_Bgsp().GetPolyAtt0(lin);
        best.att1 = dComIfG_Bgsp().GetPolyAtt1(lin);
        best.ground = dComIfG_Bgsp().GetGroundCode(lin);
    }
}

// E6: Aufstieg vom Vogel auf die Komfort-Hoehe comfortY: keine Flaeche im Weg (alle Abtastpunkte, also auch die Decke).
bool comfortReachable(const cXyz& pos, float comfortY) {
    if (pos.y >= comfortY) {
        return true;
    }
    Contact c;
    sweep(pos, cXyz(0.0f, comfortY - pos.y, 0.0f), c);
    return !c.hit;
}

// E6: Vorwaertssicht in Richtung yaw auf der Strecke len: keine Flaeche im Weg (alle Abtastpunkte).
bool forwardFree(const cXyz& p, s16 yaw, float len) {
    Contact c;
    sweep(p, cXyz(cM_ssin(yaw) * len, 0.0f, cM_scos(yaw) * len), c);
    return !c.hit;
}

// E6: Links Fuesse bei einem Sinkschritt dy des Vogels (Link folgt dem Vogel). Der Fusspunkt (kLinkFootClear ueber den
// Fuessen) darf den Weg alt -> neu nicht treffen (Linientest), und der Abstand der Fuesse am Ziel bleibt mindestens
// kLinkFootClear. Ohne Link im Ritt keine Fussgrenze.
bool linkFootStepOk(const daAlink_c* link, float dy) {
    if (link == nullptr || linkDead(link) || !rideRunning()) {
        return true;
    }
    const cXyz p = link->current.pos;
    const cXyz a(p.x, p.y + kLinkFootClear, p.z);
    const cXyz b(p.x, p.y + kLinkFootClear + dy, p.z);
    dBgS_LinChk lin;
    lin.Set(&a, &b, NULL);
    if (dComIfG_Bgsp().LineCross(&lin)) {
        return false;
    }
    return groundGap(cXyz(p.x, p.y + dy, p.z)) >= kLinkFootClear;
}

struct MoveResult {
    cXyz pos;
    bool hit = false;
    int steps = 0;
    cXyz lastN;
    bool wallHit = false;   // mindestens ein Wandkontakt in diesem Frame
    bool ceilHit = false;   // mindestens ein Deckenkontakt
    bool floorHit = false;  // mindestens ein Bodenkontakt
    cXyz hn[kSlideIters];   // horizontale Normalen der Treffer dieses Frames (Wandrichtungen)
    int nHn = 0;
};

// Bewegt den Mittelpunkt um d. Trifft die Strecke eine Flaeche, bleibt der Vogel kSkin davor und gleitet mit dem
// Restweg ohne Anteil in die Flaeche weiter. Hoechstens kSlideIters Schritte.
MoveResult slideMove(const cXyz& start, cXyz d) {
    MoveResult r;
    r.pos = start;
    for (int it = 0; it < kSlideIters; ++it) {
        const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (len2 < 1.0e-6f) {
            return r;
        }
        Contact c;
        sweep(r.pos, d, c);
        if (!c.hit) {
            r.pos.x += d.x;
            r.pos.y += d.y;
            r.pos.z += d.z;
            return r;
        }
        if (c.n.y > kFloorMinNy) {
            r.floorHit = true;
        } else if (c.n.y < kCeilMinNy) {
            r.ceilHit = true;
        } else if (std::fabs(c.n.y) < kWallMaxNy) {
            r.wallHit = true;
        }
        // Steile Flaeche (|Normale y| unter 0,7, also Wand und schraege Flaeche): der Vogel gleitet nur horizontal. Der
        // Abstand (kSkin) und die Gleitprojektion wirken nur in der Horizontalen, die Hoehe bleibt. Sonst wanderte der
        // Vogel durch die Normale nach unten oder oben (flugbahn: 444 Einheiten Absinken an einer Schraegwand, +92 beim
        // Kurveneintritt). Boden und Decke (|n.y| ab 0,7) bleiben bei der vollen Projektion wie bisher.
        const bool steep = std::fabs(c.n.y) < kFloorMinNy;
        cXyz np = steep ? cXyz(r.pos.x + d.x * c.t + c.n.x * kSkin, r.pos.y + d.y * c.t,
                               r.pos.z + d.z * c.t + c.n.z * kSkin)
                        : cXyz(r.pos.x + d.x * c.t + c.n.x * kSkin, r.pos.y + d.y * c.t + c.n.y * kSkin,
                               r.pos.z + d.z * c.t + c.n.z * kSkin);
        cXyz rest(d.x * (1.0f - c.t), d.y * (1.0f - c.t), d.z * (1.0f - c.t));
        const float rn = rest.x * c.n.x + rest.y * c.n.y + rest.z * c.n.z;
        if (rn < 0.0f) {
            if (steep) {
                const float nh2 = c.n.x * c.n.x + c.n.z * c.n.z;   // mindestens 0,51 bei |n.y| unter 0,7
                const float hn = rest.x * c.n.x + rest.z * c.n.z;  // horizontaler Anteil in die Flaeche
                if (hn < 0.0f) {
                    rest.x -= c.n.x * (hn / nh2);
                    rest.z -= c.n.z * (hn / nh2);
                }
            } else {
                rest.x -= c.n.x * rn;
                rest.y -= c.n.y * rn;
                rest.z -= c.n.z * rn;
            }
        }
        r.pos = np;
        d = rest;
        r.hit = true;
        ++r.steps;
        r.lastN = c.n;
        const float nhLen = std::sqrt(c.n.x * c.n.x + c.n.z * c.n.z);
        if (nhLen >= 0.2f && r.nHn < kSlideIters) {
            r.hn[r.nHn] = cXyz(c.n.x / nhLen, 0.0f, c.n.z / nhLen);
            ++r.nHn;
        }
        kff::logTelemetry(g_logHit, 40, 400,
            "freiheit: treffer sample=%d t=%.3f n=(%.2f,%.2f,%.2f) Sondercode=%d Wandcode=%d att0=%d att1=%d "
            "gnd=%d pos=(%.0f,%.0f,%.0f)",
            c.sample, static_cast<double>(c.t), static_cast<double>(c.n.x), static_cast<double>(c.n.y),
            static_cast<double>(c.n.z), c.special, c.wallCode, c.att0, c.att1, c.ground,
            static_cast<double>(r.pos.x), static_cast<double>(r.pos.y), static_cast<double>(r.pos.z));
    }
    return r;
}

// Gedaechtnis der Wandrichtungen (horizontale Normalen, Einheitslaenge, Frame des letzten Treffers). In einer Ecke
// trifft der Vogel jeden Frame die andere Wand. Ohne Gedaechtnis wechselte der Kurs dann zwischen den beiden
// Wandtangenten (Zittern, Befund nf3-innen1: 54 Kursaenderungen zu je 8,4 Grad, Ziel 0 und -16384 im Wechsel).
struct WallDir {
    float x = 0.0f;
    float z = 0.0f;
    uint32_t frame = 0;
    bool used = false;
};
WallDir g_wallDir[kWallMemSize];
uint32_t g_frameNo = 0;       // Frame-Zaehler des Vogels fuer das Gedaechtnis (Ritt-Start setzt zurueck)
// Link-Fuss-Messung (groundGap je Link-Frame, F06 WP9): im Ritt und noch kLinkWatchTicks Kern-Ticks nach dem Ritt-Ende,
// danach aus (kein Dauerbetrieb). Zeit in Kern-Ticks (R4), nicht in Execute-Aufrufen.
constexpr uint32_t kLinkWatchTicks = 600;  // rund 20 s bei 30 Kern-Ticks je Sekunde
bool g_linkWatchArmed = false;             // mindestens ein Ritt ist zu Ende gegangen (onRideEnd)
uint32_t g_linkWatchEnd = 0;               // Kern-Tick des letzten Ritt-Endes

// Messfenster des Link-Fusses: im Ritt, sonst noch kLinkWatchTicks nach dem letzten Ritt-Ende.
bool linkWatching() {
    if (rideRunning()) {
        return true;
    }
    return g_linkWatchArmed && kff::timers::elapsed(g_linkWatchEnd) < kLinkWatchTicks;
}

void forgetWalls() {
    for (WallDir& w : g_wallDir) {
        w.used = false;
    }
    g_frameNo = 0;
}

void rememberWall(float nx, float nz) {
    for (WallDir& w : g_wallDir) {
        if (w.used && w.x * nx + w.z * nz > 0.98f) {
            w.frame = g_frameNo;  // dieselbe Richtung: nur auffrischen
            return;
        }
    }
    int slot = -1;
    for (int i = 0; i < kWallMemSize; ++i) {
        if (!g_wallDir[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;  // voll: aeltesten Eintrag ersetzen
        for (int i = 1; i < kWallMemSize; ++i) {
            if (g_wallDir[i].frame < g_wallDir[slot].frame) {
                slot = i;
            }
        }
    }
    WallDir& w = g_wallDir[slot];
    w.x = nx;
    w.z = nz;
    w.frame = g_frameNo;
    w.used = true;
}

// Wandrichtungen, die noch gelten (hoechstens kWallMemFrames Frames seit dem letzten Treffer).
int activeWalls(float* ax, float* az) {
    int n = 0;
    for (const WallDir& w : g_wallDir) {
        if (w.used && g_frameNo - w.frame <= kWallMemFrames) {
            ax[n] = w.x;
            az[n] = w.z;
            ++n;
        }
    }
    return n;
}

// Kurs an Wandkontakten ausrichten (nur mit wallSteerOn(), also nur Testbuild mit KFF_WALL_STEER=1 oder sanft). Trifft
// der Vogel in diesem Frame eine Wand und zeigt sein Kurs in eine Wandrichtung aus dem Gedaechtnis, dreht er um hoechstens
// slideTurnCap() je Frame (kSlideTurn, sanft kSlideTurnSoft) zu der freien Richtung, die dem Kurs am naechsten liegt. Frei heisst: in keine Wandrichtung
// hinein. Bei einer Ecke ist das die Kante zwischen beiden Waenden, sonst die Wandtangente auf der Seite des Kurses.
void slideHeading(daKago_c* k, const MoveResult& r) {
    if (!wallSteerOn()) {
        return;  // Release und Standard: keine Kursdrehung durch freiheit (kreisflug.md, Nachtrag)
    }
    if (r.nHn == 0) {
        return;  // kein Wandkontakt in diesem Frame (Boden oder Decke): kein Kurswechsel
    }
    float ax[kWallMemSize];
    float az[kWallMemSize];
    const int na = activeWalls(ax, az);
    const s16 yaw = k->current.angle.y;
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    bool blocked = false;
    for (int i = 0; i < na; ++i) {
        if (hx * ax[i] + hz * az[i] < kFeasibleTol) {
            blocked = true;
        }
    }
    if (!blocked) {
        return;  // der Kurs zeigt in keine Wand, also kein Eingriff
    }
    float bestX = 0.0f;
    float bestZ = 0.0f;
    float bestDot = -2.0f;
    bool bestFree = false;
    for (int i = 0; i < na; ++i) {
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const float cx = -az[i] * static_cast<float>(sgn);  // Tangente der Wand i
            const float cz = ax[i] * static_cast<float>(sgn);
            bool frei = true;
            for (int j = 0; j < na; ++j) {
                if (cx * ax[j] + cz * az[j] < kFeasibleTol) {
                    frei = false;
                }
            }
            const float dot = cx * hx + cz * hz;
            if ((frei && !bestFree) || (frei == bestFree && dot > bestDot)) {
                bestX = cx;
                bestZ = cz;
                bestDot = dot;
                bestFree = frei;
            }
        }
    }
    const s16 target = cM_atan2s(bestX, bestZ);
    int step = static_cast<int>(static_cast<s16>(target - yaw));
    const int cap = slideTurnCap();
    if (step > cap) {
        step = cap;
    } else if (step < -cap) {
        step = -cap;
    }
    if (step == 0) {
        return;
    }
    k->current.angle.y = static_cast<s16>(yaw + step);
    k->shape_angle.y = k->current.angle.y;
    ++g_stats.wallSteer;
    ++g_stats.steerEvents;
    g_stats.steerDeg += std::fabs(static_cast<float>(step)) * 360.0f / 65536.0f;
    if (na >= 2) {
        ++g_stats.cornerSteer;
    }
    kff::logTelemetry(g_logSteer, 40, 20,
        "freiheit: Gleiten an Flaeche: Kurs %d -> %d (Ziel %d, Gier %.2f Grad, Wandrichtungen %d, frei %d)",
        static_cast<int>(yaw), static_cast<int>(k->current.angle.y), static_cast<int>(target),
        static_cast<double>(step) * 360.0 / 65536.0, na, bestFree ? 1 : 0);
}

// Kein Vorausschau-Ausweichen mehr: freiheit dreht den Kurs erst, wenn die Bewegung dieses Frames tatsaechlich
// auf eine Flaeche trifft (slideHeading). Eine Linie 180 Einheiten voraus hat den Vogel 168 Einheiten vor einer Wand
// abgedreht (Befund flug, Lauf geradeaus2) und ist deshalb entfallen.

// Freie Sichtlinie vom Vogel in Richtung yaw (Laenge length): true, wenn keine Flaeche im Weg liegt.
bool clearAhead(const cXyz& from, s16 yaw, float length) {
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    cXyz to(from.x + hx * length, from.y, from.z + hz * length);
    dBgS_LinChk lin;
    lin.Set(&from, &to, NULL);
    return !dComIfG_Bgsp().LineCross(&lin);
}

// Stillstand mit Treffer (Ecke, keine Gleitrichtung), nur Testschalter KFF_WALL_STEER: Kurs um 45 Grad nach der freien
// Seite drehen, sonst umkehren. Im Standard und im Release geschieht hier nichts, der Stillstand loest avoidStall() aus.
void breakStall(daKago_c* k) {
    if (wallSteerMode() != WallSteer::Full) {
        return;  // nur KFF_WALL_STEER=1; "sanft" dreht nur beim Gleiten
    }
    const s16 yaw = k->current.angle.y;
    const bool left = clearAhead(k->current.pos, static_cast<s16>(yaw + 0x4000), kStallProbeLen);
    const bool right = clearAhead(k->current.pos, static_cast<s16>(yaw - 0x4000), kStallProbeLen);
    int off = 0x8000;
    if (left && !right) {
        off = 0x3000;
    } else if (right && !left) {
        off = -0x3000;
    } else if (left && right) {
        off = 0x3000;
    }
    k->current.angle.y = static_cast<s16>(yaw + off);
    k->shape_angle.y = k->current.angle.y;
    ++g_stats.steerEvents;
    g_stats.steerDeg += std::fabs(static_cast<float>(off)) * 360.0f / 65536.0f;
    kff::logTelemetry(g_logStall, 40, 20, "freiheit: Stillstand an einer Flaeche: Kurs %d -> %d (Gier %.2f Grad)",
        static_cast<int>(yaw), static_cast<int>(k->current.angle.y), static_cast<double>(off) * 360.0 / 65536.0);
}

// Ecken-Stillstand ohne Gieraenderung (flugbahn (b)) und E6 (verbindlich): Ab kStallFrames blockierten Frames weicht der
// Vogel aus. Ist das Steigen frei, steigt er je Frame um kClimbStep. Ist es durch Decke oder Ueberhang blockiert, weicht
// er entlang des Hindernisses nach UNTEN aus, je Frame hoechstens kSinkStep, bis die Vorwaertssicht frei ist (g_stallSink bleibt
// gesetzt, bis der Stillstand endet). Der Sinkschritt entfaellt, wenn der Vogel auf dem Gelaende aufsaesse oder Links
// Fuesse unter kLinkFootClear kaemen (Linientest alt -> neu). Die Gier bleibt unveraendert (nur der Stick dreht).
#if defined(KFF_DEV_AUTOTEST)
uint32_t g_logDiag = 0;
uint32_t g_logDiag2 = 0;
// Diagnose des Ausweichens (nur Testbuild): Vorwaertssicht (200 Einheiten) in Schritten von 40 von -240 bis +120 relativ zum
// Vogel ("1" frei, "0" Flaeche im Weg), Hoehe des Vogels, Gelaende darunter, Abstand Link zum Vogel und Link-Fuss-Abstand.
void logEscapeDiag(const cXyz& pos, s16 yaw, const daAlink_c* link, const char* what) {
    char row[24] = {};
    int n = 0;
    for (int off = -400; off <= 200; off += 40) {
        const cXyz p(pos.x, pos.y + static_cast<float>(off), pos.z);
        row[n++] = forwardFree(p, yaw, 200.0f) ? '1' : '0';
    }
    const float surf = surfaceAt(pos, nullptr);
    const float dyLink = link != nullptr ? link->current.pos.y - pos.y : 0.0f;
    const float footGap = link != nullptr ? groundGap(link->current.pos) : -1.0f;
    kff::logEvery(g_logDiag, 12, 25,
        "freiheit: DIAG %s y=%.1f gelaende=%.1f linkDy=%.1f linkFuss=%.1f fwd(-400..200, Schritt 40)=%s",
        what, static_cast<double>(pos.y), static_cast<double>(surf), static_cast<double>(dyLink),
        static_cast<double>(footGap), row);
}
#endif

// E6: freie Vorwaertssicht in der Hoehe pos.y + dy (alle Abtastpunkte, Strecke kRiseProbe).
bool forwardFreeAt(const cXyz& pos, float dy, s16 yaw) {
    return forwardFree(cXyz(pos.x, pos.y + dy, pos.z), yaw, kRiseProbe);
}

// E6: tiefste zulaessige Hoehe des Vogels beim Ausweichen. Maximum aus der Tief-Untergrenze (kEscBodyClear ueber dem
// Gelaende unter dem Vogel) und der Hoehe, bei der Links Fuesse kLinkFootClear ueber dem Gelaende unter Link bleiben.
// kNoEscapeY, wenn kein Gelaende darunter liegt (dann gibt es kein Ausweichen nach unten).
float escapeFloorY(const cXyz& pos, const daAlink_c* link) {
    constexpr float kNoEscapeY = 1.0e9f;
    const float surf = surfaceAt(pos, nullptr);
    if (surf == -G_CM3D_F_INF) {
        return kNoEscapeY;
    }
    float low = surf + kEscBodyClear;
    if (link != nullptr && !linkDead(link) && rideRunning()) {
        const float linkSurf = surfaceAt(link->current.pos, nullptr);
        if (linkSurf != -G_CM3D_F_INF) {
            const float dyLink = link->current.pos.y - pos.y;
            low = std::max(low, linkSurf + kLinkFootClear - dyLink);
        }
    }
    return low;
}

void avoidStall(cXyz& pos, const daAlink_c* link, s16 yaw) {
    if (!g_stallSink) {
        // Steigen ist frei, wenn der Weg nach oben frei bleibt und die Vorwaertssicht in hoechstens kClimbReach Hoehe frei wird.
        bool climbFree = false;
        for (float dy = kClimbStep; dy <= kClimbReach; dy += kClimbStep) {
            Contact up;
            sweep(pos, cXyz(0.0f, dy, 0.0f), up);
            if (up.hit) {
                break;  // Decke oder Ueberhang blockiert das Steigen
            }
            if (forwardFreeAt(pos, dy, yaw)) {
                climbFree = true;
                break;
            }
        }
        if (climbFree) {
            pos.y += kClimbStep;
            ++g_stats.stallClimb;
            return;
        }
    }
    // E10: Der Abstieg ist per Konfig decke_abstieg standardmaessig AUS. Der Pfad ist im Test nicht belegt (ausweichStarts 0).
    // Ohne Schalter bleibt der Vogel unter Decke oder Ueberhang stehen, ohne nach unten auszuweichen (Gier unveraendert).
    if (!kff::config::getBool(kff::config::Key::DeckeAbstieg)) {
        ++g_stats.escapeBlocked;
        kff::logTelemetry(g_logClimb, 40, 20, "freiheit: Ausweichen nach unten aus (Konfig decke_abstieg AUS), Hoehe bleibt");
        return;
    }
    // Ausweichen nach unten nur, wenn unterhalb bis zur tiefsten zulaessigen Hoehe eine Hoehe mit freier Vorwaertssicht
    // liegt. Sonst ist das Hindernis eine Wand: der Vogel behaelt seine Hoehe, ein Abstieg brachte keinen Vorwaertsweg.
    const float low = escapeFloorY(pos, link);
    float freeDy = -1.0f;
    for (float dy = kSinkProbe; dy <= kSinkMax && pos.y - dy >= low; dy += kSinkProbe) {
        if (forwardFreeAt(pos, -dy, yaw)) {
            freeDy = dy;
            break;
        }
    }
    if (freeDy < 0.0f) {
        ++g_stats.escapeBlocked;
        kff::logTelemetry(g_logClimb, 40, 20, "freiheit: kein Ausweg nach unten (Wand oder Untergrenze), Hoehe bleibt");
        return;
    }
    if (!g_stallSink) {
        g_stallSink = true;  // Steigen blockiert, unten gibt es freie Sicht: ab jetzt nach unten, bis vorwaerts frei
        g_low = true;        // Tief-Modus: die Komfort-Untergrenze drueckt den Vogel nicht in die Decke
        g_escTop = pos.y;
        ++g_stats.escapeStarts;
        kff::logEvery(g_logEsc, 3, 60, "freiheit: Steigen durch Decke oder Ueberhang blockiert: Ausweichen nach unten, Gier bleibt");
    }
    const float drop = std::min(kSinkStep, freeDy);
    Contact down;
    sweep(pos, cXyz(0.0f, -drop, 0.0f), down);
    const bool footOk = down.hit ? true : linkFootStepOk(link, -drop);
    if (down.hit || !footOk) {
        ++g_stats.escapeBlocked;
        kff::logTelemetry(g_logClimb, 40, 20, "freiheit: Ausweichen nach unten blockiert: %s",
            down.hit ? "Flaeche unter dem Vogel" : "Links Fuss am Gelaende");
        return;
    }
    pos.y -= drop;
    const float depth = g_escTop - pos.y;
    if (depth > g_stats.escapeDepthMax) {
        g_stats.escapeDepthMax = depth;
    }
    ++g_stats.stallSink;
}

// E6, Hoehe nach dem Stillstand (je Frame vor der Untergrenze). Ausweichen laeuft: nichts tun. Abklingzeit: halten. Unter
// der Komfort-Untergrenze gilt: ist der Aufstieg frei, hebt die Untergrenze den Vogel wie bisher. Ist er durch Decke oder
// Ueberhang blockiert, bleibt der Vogel im Tief-Modus (g_low) und steigt nur weich (kRiseStep je Frame), solange Aufstieg
// und Vorwaertssicht in der hoeheren Lage frei sind. So entsteht kein Pendeln zwischen Decke und Boden.
void settleHeight(cXyz& pos, const daKago_c* k) {
    if (landingActive()) {
        g_low = false;  // Landeanflug (Session.landing) hat Vorrang
        return;
    }
    if (g_stallSink) {
        return;
    }
    if (g_escHold > 0) {
        --g_escHold;
        return;
    }
    const float surf = surfaceAt(pos, nullptr);
    if (surf == -G_CM3D_F_INF) {
        g_low = false;  // kein Gelaende darunter: keine Komfort-Untergrenze
        return;
    }
    const float comfort = surf + clearance();
    if (pos.y >= comfort) {
        g_low = false;
        return;
    }
    if (!g_low) {
        if (comfortReachable(pos, comfort)) {
            return;  // frei: die Untergrenze hebt den Vogel wie bisher auf die Komfort-Hoehe
        }
        g_low = true;  // Decke oder Ueberhang versperrt den Aufstieg: Tief-Modus
    }
    const cXyz next(pos.x, pos.y + kRiseStep, pos.z);
    Contact up;
    sweep(pos, cXyz(0.0f, kRiseStep, 0.0f), up);
    if (up.hit || !forwardFree(next, k->current.angle.y, kRiseProbe)) {
        ++g_stats.riseBlocked;
        kff::logEvery(g_logRise, 3, 120, "freiheit: Aufstieg zur Komfort-Hoehe wartet (Decke oder Vorwaertssicht in der hoeheren Lage)");
        return;
    }
    pos.y = std::min(comfort, next.y);
    ++g_stats.riseSteps;
    if (pos.y >= comfort) {
        g_low = false;
    }
}

// Pruefung nach der Korrektur: liegt der Endpunkt hinter einer getroffenen Flaeche, ist ein Durchflug passiert. Dann
// liefert die Funktion true und setzt out auf den Punkt kSkin vor dem ersten Treffer. Die Strecke bis dorthin ist frei.
bool passedThrough(const cXyz& start, const cXyz& end, cXyz& out) {
    const cXyz d(end.x - start.x, end.y - start.y, end.z - start.z);
    Contact c;
    sweep(start, d, c);
    if (!c.hit || c.t <= 0.0f || c.t >= 1.0f) {
        return false;
    }
    const float side = (end.x - c.at.x) * c.n.x + (end.y - c.at.y) * c.n.y + (end.z - c.at.z) * c.n.z;
    if (side >= -0.5f * kSkin) {
        return false;
    }
    out = cXyz(c.at.x + c.n.x * kSkin, c.at.y + c.n.y * kSkin, c.at.z + c.n.z * kSkin);
    return true;
}

// WP6 (Aufgabe 5, K4/T11): Stillstands-Waechter. Zaehlt Ritt-Frames des Vogels (gleich Kern-Ticks ohne Pause, in der
// Pause laeuft kein Vogel-Execute), in denen der Vogel ohne Stick-Eingabe im Umkreis kStuckRadius um einen Ankerpunkt
// bleibt. Stick ueber den Totzonen von flug, Ortswechsel, Landeanflug und Ereignis setzen zurueck. Nach kStuckTicks:
// Log "watchdog: stuck <n> ticks", das Ritt-Ende (endRideClean, Grund Stuck) folgt im naechsten Link-Frame
// (onFrameLink, wie die Ritt-Enden des Kerns). Mit Stick-Eingabe greift der Waechter nicht.
void watchStuck(const cXyz& pos) {
    if (g_stuckPending) {
        return;
    }
    const kff::Input& in = kff::input();
    const bool stick = std::fabs(in.stickX) > kStickXDead || std::fabs(in.stickY) > kStickYDead;
    const float dx = pos.x - g_stuckAnchor.x;
    const float dy = pos.y - g_stuckAnchor.y;
    const float dz = pos.z - g_stuckAnchor.z;
    const bool moved = dx * dx + dy * dy + dz * dz > kStuckRadius * kStuckRadius;
    // Schweben (Gang 0, Entwurf H1 4.7) und ein offenes Dokument (H1 7.13): der Vogel steht dann absichtlich. Der
    // Vogel-Execute laeuft im Dokument weiter (ride.cpp, uiHalt im Vogel-Pre-Hook), deshalb ist die Ausnahme noetig.
    const bool halt = (kff::session().hover && kff::isRiding()) || in.uiDocument;
    if (stick || moved || landingActive() || inEvent() || halt) {
        g_stuckAnchor = pos;
        g_stuckTicks = 0;
        return;
    }
    ++g_stuckTicks;
    if (g_stuckTicks > g_wp6.stuckRunMax) {
        g_wp6.stuckRunMax = g_stuckTicks;
    }
    if (g_stuckTicks == kStuckTicks) {
        g_stuckPending = true;
        kff::logWarn("watchdog: stuck %u ticks (Vogel ohne Stick im Umkreis %.0f um (%.0f,%.0f,%.0f), Ritt-Frame %u), "
                     "Ritt endet (Stuck)",
            g_stuckTicks, static_cast<double>(kStuckRadius), static_cast<double>(g_stuckAnchor.x),
            static_cast<double>(g_stuckAnchor.y), static_cast<double>(g_stuckAnchor.z), kff::session().rideFrames);
    }
}

void resetStuck(const cXyz& pos) {
    g_stuckAnchor = pos;
    g_stuckTicks = 0;
    g_stuckPending = false;
}

#if defined(KFF_DEV_AUTOTEST)
// E10-Messung (nur Testbuild): Ritt-Frames vom Stickwechsel im Stillstand (ab kStallFrames) bis der Stillstand endet.
// Hoechstens 150 Frames je Messung (dann gilt der Wechsel als ohne Befreiung).
void e10Release(bool stalledBefore, bool stalledNow) {
    const kff::Input& in = kff::input();
    const bool change = std::fabs(in.stickX - g_lastStickX) > 0.1f || std::fabs(in.stickY - g_lastStickY) > 0.1f;
    g_lastStickX = in.stickX;
    g_lastStickY = in.stickY;
    if (g_releaseStart == 0 && stalledBefore && change) {
        g_releaseStart = g_frameNo;
    }
    if (g_releaseStart == 0) {
        return;
    }
    const uint32_t n = g_frameNo - g_releaseStart;
    if (!stalledNow || n >= 150) {
        ++g_wp6.releases;
        g_wp6.releaseMax = std::max(g_wp6.releaseMax, n);
        kff::logInfo("freiheit: E10 Stickwechsel im Stillstand: %s nach %u Frames (Stick %.2f,%.2f)",
            stalledNow ? "KEINE Befreiung" : "befreit", n, static_cast<double>(in.stickX), static_cast<double>(in.stickY));
        g_releaseStart = 0;
    }
}

// WP6.1/6.8 (nur Testbuild): mRoofHeight nach dem Vogel-Execute muss kNoRoof sein (alle Aktionen und Modi). Alle 60
// Ritt-Frames: echte Decke ueber dem Vogel (RoofChk ab y-500 wie das Original) und der Vanilla-Grenzwert (Decke - 300).
void roofDiag(daKago_c* k) {
    const uint32_t f = kff::session().rideFrames;
    if (k->mRoofHeight != kNoRoof) {
        ++g_wp6.roofNotNone;
        kff::logEvery(g_logRoofDiag, 5, 120, "freiheit: DACH mRoofHeight=%.1f statt kNoRoof (Ritt-Frame %u, Aktion %d Modus %d)",
            static_cast<double>(k->mRoofHeight), f, k->mAction, k->mMode);
    } else {
        g_wp6.roofActions |= 1u << (static_cast<unsigned>(k->mAction) & 31u);
        g_wp6.roofModes |= 1u << (static_cast<unsigned>(k->mMode) & 31u);
    }
    if (f % 60 != 0) {
        return;
    }
    dBgS_RoofChk rc;
    const cXyz p(k->current.pos.x, k->current.pos.y - 500.0f, k->current.pos.z);
    rc.SetPos(p);
    const float roof = dComIfG_Bgsp().RoofChk(&rc);
    const bool has = roof != G_CM3D_F_INF;
    kff::logInfo("freiheit: DACH f=%u Aktion %d Modus %d y=%.1f mRoofHeight=%s echteDecke=%.1f vanillaGrenze=%.1f "
                 "roofSkip=%u",
        f, k->mAction, k->mMode, static_cast<double>(k->current.pos.y),
        k->mRoofHeight == kNoRoof ? "kNoRoof" : "WERT", static_cast<double>(has ? roof : -1.0f),
        static_cast<double>(has ? roof - 300.0f : -1.0f), g_wp6.roofSkip);
}

// Schadensquelle (nur Testbuild; Auftrag: Schaden durch Gegner von Mod-Fehlern trennen). daAlink_c::setDamagePoint,
// Pre ohne SKIP (greift nicht ein). Im Aufruf aus checkDamageAction sind die Trefferbits (mTgCyls, mAtSph) und die
// Acch-Treffer dieses Frames noch gesetzt. Quelle: Akteur-Treffer (Profilname, Abstand), Wand- oder Bodenpolygon mit
// Schadensbits 0x60 (wie checkPolyDamage, d_a_alink_damage.inc:315-379), sonst unbekannt (z. B. Test-API).
uint32_t g_logDmg = 0;

HookAction onLinkSetDamagePointPre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        const int amount = kff::argOf<int>(args, 1);
        if (link == nullptr || link != kff::liveLink() || amount <= 0) {
            return HOOK_CONTINUE;
        }
        ++g_dmgCount;
        const bool ride = kff::session().phase == kff::Phase::Riding;
        if (ride) {
            ++g_dmgRide;
        }
        int tgIndex = -1;
        int prof = -1;
        double dist = -1.0;
        auto hitActor = [&](dCcD_GObjInf& obj, int index) {
            if (tgIndex >= 0 || !obj.ChkTgHit()) {
                return;
            }
            tgIndex = index;
            if (fopAc_ac_c* a = obj.GetTgHitAc()) {
                prof = static_cast<int>(fopAcM_GetProfName(a));
                const double dx = static_cast<double>(a->current.pos.x - link->current.pos.x);
                const double dy = static_cast<double>(a->current.pos.y - link->current.pos.y);
                const double dz = static_cast<double>(a->current.pos.z - link->current.pos.z);
                dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
        };
        for (int i = 0; i < 3; ++i) {
            hitActor(link->mTgCyls[i], i);
        }
        hitActor(link->mAtSph, 3);
        int wallArg = -1;  // Argument des ersten Wandpolygons mit Schadensbits, sonst des ersten Wandpolygons
        bool wallDmg = false;
        for (int i = 0; i < 3; ++i) {
            dBgS_AcchCir& cir = link->mAcchCir[i];
            if (!cir.ChkWallHit() || !dComIfG_Bgsp().ChkPolySafe(cir)) {
                continue;
            }
            const int arg = static_cast<int>(dKy_pol_argument_get(&cir));
            if ((arg & 0x60) != 0) {
                wallArg = arg;
                wallDmg = true;
                break;
            }
            if (wallArg < 0) {
                wallArg = arg;
            }
        }
        int gndArg = -1;
        if (link->mLinkAcch.ChkGroundHit() && dComIfG_Bgsp().ChkPolySafe(link->mLinkAcch.m_gnd)) {
            gndArg = static_cast<int>(dKy_pol_argument_get(&link->mLinkAcch.m_gnd));
        }
        const bool gndDmg = gndArg >= 0 && (gndArg & 0x60) != 0;
        const char* src = prof >= 0 ? "Akteur-Treffer (Gegner oder Objekt)"
                          : wallDmg  ? "Wandpolygon mit Schadensbits (Umgebung)"
                          : gndDmg   ? "Bodenpolygon mit Schadensbits (Umgebung)"
                          : tgIndex >= 0 ? "Treffer ohne Akteur"
                                         : "unbekannt (kein Treffer, kein Schadenspolygon)";
        kff::logEvery(g_logDmg, 40, 20,
            "freiheit: SCHADEN betrag=%d leben=%u proc=%d ritt=%d quelle=%s tgIndex=%d tgProf=%d tgAbstand=%.0f "
            "wandArg=%d bodenArg=%d link=(%.0f,%.0f,%.0f)",
            amount, static_cast<unsigned>(dComIfGs_getLife()), static_cast<int>(link->mProcID), ride ? 1 : 0, src, tgIndex,
            prof, dist, wallArg, gndArg, static_cast<double>(link->current.pos.x),
            static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z));
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in setDamagePoint diagnosis (ignored)");
    }
    return HOOK_CONTINUE;
}
#endif

#if defined(KFF_DEV_AUTOTEST)
void logDiagBird(const char* tag, daKago_c* k, uint32_t f) {
    kff::logInfo("freiheit: %s f=%u pos=(%.1f,%.1f,%.1f) speedF=%.2f flySpeed=%.2f yaw=%d floor=%.1f "
                 "acchFlags=0x%x owned=0x%x",
        tag, f, static_cast<double>(k->current.pos.x), static_cast<double>(k->current.pos.y),
        static_cast<double>(k->current.pos.z), static_cast<double>(k->speedF),
        static_cast<double>(k->mFlySpeed), static_cast<int>(k->current.angle.y),
        static_cast<double>(k->mGroundHeight), k->mObjAcch.m_flags, g_birdOwned);
}

// Messzeile fuer den Test: Zaehler und Mindestwerte bis zu diesem Zeitpunkt (Log "freiheit: MESS ...").
void logMess(const char* tag) {
    const Stats& st = g_stats;
    kff::logInfo("freiheit: MESS %s frames=%u gleiten=%u glideFrames=%u schritte=%u durchdringungen=%u "
                 "maxKorrektur=%.1f minBodenVogel=%.1f minLinkFuss=%.1f linkUnterGrund=%u abgrund=%u",
        tag, st.frames, st.slideFrames, st.glideFrames, st.slideSteps, st.penetration,
        static_cast<double>(st.maxCorr), static_cast<double>(st.minBirdGap), static_cast<double>(st.minLinkGap),
        st.linkBelowGround, st.abyssFrames);
    kff::logInfo("freiheit: MESS2 %s wandKontakt=%u deckenKontakt=%u bodenKontakt=%u bodenlift=%u wasserlift=%u "
                 "ausweichEreignisse=%u ausweichGrad=%.1f stillstand=%u wandSteer=%u ecken=%u",
        tag, st.wallContacts, st.ceilContacts, st.floorContacts, st.groundClamp, st.waterClamp, st.steerEvents,
        static_cast<double>(st.steerDeg), st.stallBreak, st.wallSteer, st.cornerSteer);
    kff::logInfo("freiheit: MESS3 %s stageVeto=%u sceneCancel=%u birdSceneVeto=%u restartVeto=%u "
                 "roofVeto=%u exc=%u wallSteer=%d",
        tag, st.stageVeto, st.sceneCancel, st.birdSceneVeto, st.restartVeto, st.roofRestartVeto,
        st.exceptions, wallSteerOn() ? 1 : 0);
}
#endif  // KFF_DEV_AUTOTEST (Messzeilen nur fuer Szenarien)

// ---------------------------------------------------------------------------------------------
// Hook-Callbacks (Besitzer "freiheit", Prioritaet kff::prio::kFreiheit)
// ---------------------------------------------------------------------------------------------

// daKago_c::checkRoofHeight, Pre mit SKIP (WP6, D9): fuer den eigenen Vogel laeuft das Original nicht. Die Antwort ist
// die des Originals ohne Decke (RoofChk liefert G_CM3D_F_INF, d_a_kago.cpp:395-439): Rueckgabe und mRoofHeight kNoRoof,
// field_0x6e3 = 0 (kein Deckenruf NAKU), field_0x6e5 = 0 (kein Flussfels-Stagger). Einheitlich in allen Aktionen und
// Modi, weil jeder Aufrufer (checkMoveHeight und die Aktionen) denselben Wert bekommt. Vorher (Post) lief das Original
// erst vollstaendig: "roof=400" im Log war dessen Wert (Decke 700 minus mFlightCeilingAltitude 300), kein Fehlerwert.
// checkGroundHeight bleibt unveraendert (Vanilla).
HookAction onRoofPre(ModContext*, void* args, void* retval, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        if (k == nullptr || !kff::isOwnBird(k) || roofKept()) {
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<float*>(retval) = kNoRoof;
        }
        k->mRoofHeight = kNoRoof;
        k->field_0x6e3 = 0;
        k->field_0x6e5 = 0;
        ++g_stats.roofOverride;
        ++g_wp6.roofSkip;
        kff::logTelemetry(g_logRoofSkip, 3, 600,
            "freiheit: Decke per Pre-SKIP aus (kNoRoof, Original laeuft nicht), Aktion %d Modus %d, Vogel y=%.1f",
            static_cast<int>(k->mAction), static_cast<int>(k->mMode), static_cast<double>(k->current.pos.y));
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in checkRoofHeight (original runs)");
        return HOOK_CONTINUE;
    }
}

// daKago_c::setSceneChange, Pre: der eigene Vogel wechselt im Ritt nicht die Szene (Rueckgabe 0 = kein Wechsel).
// setSceneChange bestellt kein Ereignis (d_a_kago.cpp:668-698), die Sperre laesst nichts haengen.
HookAction onSetSceneChangePre(ModContext*, void* args, void* retval, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        if (k == nullptr || !kff::isOwnBird(k) || !sceneVetoActive()) {
            return HOOK_CONTINUE;
        }
        const int mode = kff::argOf<int>(args, 1);
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        ++g_stats.birdSceneVeto;
        if (vetoTry(kVsBirdScene)) {
            kff::logInfo("freiheit: positional scene veto (%s) mode %d, Vogel bei (%.0f,%.0f,%.0f)",
                kVetoSiteName[kVsBirdScene], mode, static_cast<double>(k->current.pos.x),
                static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z));
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in setSceneChange (original runs)");
        return HOOK_CONTINUE;
    }
}

// daKago_c::executeFly, Pre (nach checkMoveHeight und checkSizeBg, vor checkHeight): Untergrenze, Decke und
// Stagger-Bits dieses Frames setzen.
HookAction onExecuteFlyPre(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        g_snapArmed = false;
        if (k == nullptr || !kff::isOwnBird(k) || !rideRunning()) {
            return HOOK_CONTINUE;
        }
        k->mGroundHeight = birdFloor(k->current.pos, k, kff::liveLink(), nullptr, nullptr);
        if (!roofKept()) {
            k->mRoofHeight = kNoRoof;
        }
        // F1: Stand vor checkHeight, damit onBirdExecutePost den Sprung dieser Vanilla-Funktion herausrechnet.
        g_snapArmed = true;
        g_snapPreY = k->current.pos.y;
        g_snapGround = k->mGroundHeight;
        g_snapRoof = k->mRoofHeight;
        k->field_0x6d9 = 0;  // checkSizeBg hat Sondercode-1-Treffer gemeldet: kein Stagger
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in executeFly (heights unchanged)");
    }
    return HOOK_CONTINUE;
}

// Kern-Patch P-RESTART (nur wenn KFF_CORE_RESTART_HOOKS gesetzt ist): daAlink_c::checkRestartRoom (BOOL), Pre.
#if defined(KFF_CORE_RESTART_HOOKS)
// Wuerde checkRestartRoom jetzt neu starten (d_a_alink.cpp:13662-13760)? Nur fuer die Logzeile: die Sperre gilt im Ritt
// in jedem Frame. Im Ritt traegt der Vogel Link (Cargo), der Abgrund-Zweig des Spiels ist dann ohnehin aus.
bool restartWouldTrigger(daAlink_c* l) {
    if (l->mLinkAcch.ChkGroundHit() &&
        (l->mGndPolyAtt1 == 2 || (dKy_pol_argument_get(&l->mLinkAcch.m_gnd) & 0x80) != 0)) {
        return true;  // Lava oder Neustart-Boden (Attribut 0x80)
    }
    return !l->checkCargoCarry() && (l->mGroundCode == 4 || l->mGroundCode == 10);  // Abgrund- oder Ausgangsboden
}

// Der Neustart (Void, Fall, Lava, Neustart-Boden) wird im Ritt nicht ausgeloest (ORCH-5, V12). Rueckgabe 0, Original
// uebersprungen, bevor es ein Pflichtereignis bestellt. Nach dem Absteigen gilt die Vanilla-Regel wieder (OE1).
HookAction onRestartRoomPre(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr || !rideRunning() || linkDead(link)) {
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<BOOL*>(retval) = 0;
        }
        ++g_stats.restartVeto;
        if (restartWouldTrigger(link) && vetoTry(kVsRestart)) {
            kff::logInfo("freiheit: positional scene veto (%s) Boden-Code %d Attribut %d Bodenkontakt %d bei (%.0f,%.0f,%.0f)",
                kVetoSiteName[kVsRestart], link->mGroundCode, static_cast<int>(link->mGndPolyAtt1),
                link->mLinkAcch.ChkGroundHit() ? 1 : 0, static_cast<double>(link->current.pos.x),
                static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z));
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in checkRestartRoom (original runs)");
        return HOOK_CONTINUE;
    }
}

// Kern-Patch P-RESTART: daAlink_c::checkRoofRestart (void), Pre. Kopf-Neustart im Ritt aus. WP6 (6.4): nach dem Ritt
// laeuft die Vanilla-Pruefung wieder; der erste Aufruf nach einem Ritt steht im Log, die Aufrufe zaehlen fuer SUMMARY6.
HookAction onRoofRestartPre(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (!rideRunning() && g_rideEndedOnce) {
            ++g_wp6.roofRestartOutside;
            if (!g_roofRestartLogged) {
                g_roofRestartLogged = true;
                kff::logInfo("freiheit: checkRoofRestart wieder aktiv nach dem Ritt (Vanilla, Link y=%.1f)",
                    static_cast<double>(link != nullptr ? link->current.pos.y : 0.0f));
            }
        }
        if (link == nullptr || !rideRunning() || linkDead(link)) {
            return HOOK_CONTINUE;
        }
        ++g_stats.roofRestartVeto;
        if (link->mLinkAcch.ChkRoofHit() && link->mLinkAcch.ChkGroundHit() && vetoTry(kVsRoofRestart)) {
            kff::logInfo("freiheit: positional scene veto (%s) Decke und Boden beruehrt bei (%.0f,%.0f,%.0f)",
                kVetoSiteName[kVsRoofRestart], static_cast<double>(link->current.pos.x),
                static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z));
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in checkRoofRestart (original runs)");
        return HOOK_CONTINUE;
    }
}

#endif  // KFF_CORE_RESTART_HOOKS

// Wuerde daAlink_c::checkSceneChange jetzt wechseln (d_a_alink.cpp:13798-13820)? Nebel, Ladezone mit Kontakt
// (Boden, Schwimmen, Reiten, Tuer, Warp, Graben, Fall auf Code 5) oder Sprung-Ladezone (0x8000) im Sprungmodus.
bool sceneChangeWouldTrigger(daAlink_c* l, int exitId) {
    const bool fog = l->mProcID != daAlink_c::PROC_FOG_DEAD &&
                     (l->mGroundCode == 9 || l->checkNoResetFlg2(daPy_py_c::FLG2_FOG_FADE));
    if (l->mGroundCode == 4 || l->mGroundCode == 10) {
        exitId = 0x3F;
    }
    const bool exitSet = exitId != 0x3F || l->mExitID != 0x3F;
    const bool contact = l->checkModeFlg(0x40000) || l->checkModeFlg(0x400) || l->mLinkAcch.ChkGroundHit() ||
                         l->mProcID == daAlink_c::PROC_DOOR_OPEN || l->mProcID == daAlink_c::PROC_WARP ||
                         l->mProcID == daAlink_c::PROC_WOLF_DIG_THROUGH ||
                         (l->mGroundCode == 5 && l->field_0x33c8 - l->current.pos.y > 500.0f);
    const bool jump = (l->mExitID & 0x8000) != 0 && l->checkModeFlg(2);
    return fog || (exitSet && contact) || jump;
}

// daAlink_c::checkSceneChange, Pre: im Ritt uebersprungen (Rueckgabe 0). Das Original bestellt vor dStage_changeScene ein
// Pflichtereignis (d_a_alink.cpp:13895); eine Sperre erst in dStage_changeScene liesse dieses Ereignis haengen (Befund
// b-ladezone1). Ohne ausloesende Bedingung tut das Original nichts, das Ueberspringen ist dann gleichwertig.
HookAction onCheckSceneChangePre(ModContext*, void* args, void* retval, void*) {
    try {
        if (!sceneVetoActive()) {
            return HOOK_CONTINUE;
        }
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr) {
            return HOOK_CONTINUE;
        }
        const int exitId = kff::argOf<int>(args, 1);
        ++g_sceneSkips;
        if (sceneChangeWouldTrigger(link, exitId)) {
            ++g_stats.stageVeto;
            if (vetoTry(kVsCheckScene)) {
                kff::logInfo("freiheit: positional scene veto (%s) Ausgang %d (Ladezone 0x%x, Richtung %d, Boden-Code %d, "
                             "Bodenkontakt %d) Link bei (%.0f,%.0f,%.0f)",
                    kVetoSiteName[kVsCheckScene], exitId, static_cast<unsigned>(link->mExitID),
                    static_cast<int>(link->mExitDirection), link->mGroundCode, link->mLinkAcch.ChkGroundHit() ? 1 : 0,
                    static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                    static_cast<double>(link->current.pos.z));
            }
        }
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in checkSceneChange (original runs)");
        return HOOK_CONTINUE;
    }
}

// daAlink_c::startRestartRoom(u32 mode, int param, int dmg, BOOL isEventRun), Pre. Einziger Besitzer dieses Ziels (Welle 2:
// WP1 und WP6 zusammengefuehrt, griff registriert hier nichts). Gemeinsamer Ausgang aller Raum-Neustarts aus der
// Spielerlage (checkRoofRestart, checkRestartRoom, daTagRetRm_c ueber forceRestartRoom). Im Ritt ohne Ereignis mit
// lebendem Link: Rueckgabe 0 ("nicht gestartet"), Original uebersprungen, bevor ein Pflichtereignis bestellt wird. Bei
// Tod (auch Leben 0, checkDeadHP), laufendem Ereignis oder isEventRun (Demo- und Schadenspfade d_a_alink_demo.inc:3133,
// d_a_alink_damage.inc:2049) endet der Ritt vorher geordnet, und der Neustart laeuft: kein Ereignis und kein Game-Over
// haengt. daTagRetRm_c zaehlt danach neu herunter (30 Ticks). Ausserhalb des Rittes laeuft das Original immer (OE1).
HookAction onStartRestartRoomPre(ModContext*, void* args, void* retval, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        const u32 mode = kff::argOf<u32>(args, 1);
        const int param = kff::argOf<int>(args, 2);
        const int dmg = kff::argOf<int>(args, 3);
        const int eventRun = kff::argOf<int>(args, 4);
        if (!rideRunning()) {
            ++g_wp6.restartOutside;
            if (g_rideEndedOnce && !g_restartOutsideLogged) {
                g_restartOutsideLogged = true;
                kff::logInfo("freiheit: startRestartRoom nach dem Ritt (Vanilla, laeuft durch): mode %u param 0x%x dmg %d "
                             "eventRun %d", static_cast<unsigned>(mode), static_cast<unsigned>(param), dmg, eventRun);
            }
            return HOOK_CONTINUE;
        }
        ++g_restartInRide;
        ++g_wp6.restartCalls;
        const bool dead = linkDead(link) || link->checkDeadHP();
        if (dead || eventRun != 0 || inEvent()) {
            ++g_wp6.restartPassed;
            kff::logEvery(g_logRestartWp6, 3, 60,
                "freiheit: startRestartRoom im Ritt durchgelassen (%s): mode %u param 0x%x dmg %d eventRun %d",
                dead ? "Link tot" : "Ereignis", static_cast<unsigned>(mode), static_cast<unsigned>(param), dmg, eventRun);
            char text[96];
            std::snprintf(text, sizeof(text), "startRestartRoom (mode %u, Schaden %d) im %s", static_cast<unsigned>(mode),
                dmg, dead ? "Tod" : "Ereignis");
            endForScene(dead ? kff::EndReason::Death : kff::EndReason::Event, text);
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        ++g_wp6.restartVeto;
        if (vetoTry(kVsStartRestart)) {
            kff::logInfo("freiheit: positional scene veto (%s) mode %u Schaden %d, kein Ereignis",
                kVetoSiteName[kVsStartRestart], static_cast<unsigned>(mode), dmg);
        }
        kff::logEvery(g_logRestartWp6, 3, 60,
            "freiheit: startRestartRoom im Ritt abgewiesen (mode %u param 0x%x dmg %d), Ritt laeuft weiter",
            static_cast<unsigned>(mode), static_cast<unsigned>(param), dmg);
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in startRestartRoom (original runs)");
        return HOOK_CONTINUE;
    }
}

// daTagRetRm_c::execute, Pre: der Rueckkehr-Raum-Quader zaehlt im Ritt nicht (sonst forceRestartRoom nach 30 Frames im
// Volumen, d_a_tag_ret_room.cpp:101-113). Der Zeitgeber wird geleert: nach dem Ritt beginnt die Vanilla-Frist neu.
HookAction onTagRetRmPre(ModContext*, void* args, void* retval, void*) {
    try {
        if (!sceneVetoActive()) {
            return HOOK_CONTINUE;
        }
        daTagRetRm_c* tag = kff::argOf<daTagRetRm_c*>(args, 0);
        if (tag == nullptr) {
            return HOOK_CONTINUE;
        }
        ++g_retRmSkips;
        const bool inside = tag->chkPlyrInTag();
        if ((inside || tag->field_0x578 != 0) && vetoTry(kVsRetRm)) {
            kff::logInfo("freiheit: positional scene veto (%s) Link im Rueckkehr-Quader %d (Zeitgeber %d), Quader bei "
                         "(%.0f,%.0f,%.0f)",
                kVetoSiteName[kVsRetRm], inside ? 1 : 0, static_cast<int>(tag->field_0x578),
                static_cast<double>(tag->current.pos.x), static_cast<double>(tag->current.pos.y),
                static_cast<double>(tag->current.pos.z));
        }
        tag->field_0x578 = 0;
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 1;
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in daTagRetRm_c::execute (original runs)");
        return HOOK_CONTINUE;
    }
}

// dStage_changeScene (global), Pre. Ohne Ereignis gesperrt (Rueckgabe 0). Hat der Aufrufer in diesem Frame ein
// Pflichtereignis bestellt (z. B. Ooccoo-Warp, d_a_alink_demo.inc:4472), endet der Ritt und der Wechsel laeuft: eine
// Sperre liesse das Ereignis haengen (ORCH-2).
HookAction onStageChangeScenePre(ModContext*, void* args, void* retval, void*) {
    try {
        if (!sceneVetoActive()) {
            return HOOK_CONTINUE;
        }
        const int exitId = kff::argOf<int>(args, 0);
        dEvt_control_c* ev = dComIfGp_getEvent();
        if (ev != nullptr && ev->checkCompulsory() != 0) {
            char text[96];
            std::snprintf(text, sizeof(text), "dStage_changeScene (Ausgang %d) mit Pflichtereignis", exitId);
            endForScene(kff::EndReason::Event, text);
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<int*>(retval) = 0;
        }
        ++g_stats.stageVeto;
        if (vetoTry(kVsChangeScene)) {
            kff::logInfo("freiheit: positional scene veto (%s) Ausgang %d Raum %d, kein Ereignis",
                kVetoSiteName[kVsChangeScene], exitId, static_cast<int>(kff::argOf<s8>(args, 3)));
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in dStage_changeScene (original runs)");
        return HOOK_CONTINUE;
    }
}

// dComIfGp_setNextStage (4 Argumente), Pre: nur Menue, Konsole, Warp-Menue (Dusklight) und Skripte rufen diese Form
// (kein Ladezonen-Pfad). Kein Veto: Ritt-Ende, dann laeuft der Wechsel (ORCH-2).
HookAction onStageNextStagePre(ModContext*, void* args, void*, void*) {
    try {
        if (!rideRunning()) {
            return HOOK_CONTINUE;
        }
        const char* stage = kff::argOf<const char*>(args, 0);
        char text[96];
        std::snprintf(text, sizeof(text), "setNextStage %s (Menue, Konsole, Warp-Menue oder Skript)",
            stage != nullptr ? stage : "?");
        endForScene(inEvent() ? kff::EndReason::Event : kff::EndReason::Menu, text);
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in dComIfGp_setNextStage (original runs)");
    }
    return HOOK_CONTINUE;
}

// dStage_changeScene4Event, Pre: Szenenwechsel durch ein Ereignis. Kein Veto: Ritt-Ende, falls der Kern es noch nicht
// beendet hat.
HookAction onChangeScene4EventPre(ModContext*, void* args, void*, void*) {
    try {
        if (rideRunning()) {
            char text[96];
            std::snprintf(text, sizeof(text), "dStage_changeScene4Event (Ausgang %d)", kff::argOf<int>(args, 0));
            endForScene(kff::EndReason::Event, text);
        }
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in dStage_changeScene4Event (original runs)");
    }
    return HOOK_CONTINUE;
}

// daKago_c::checkFlySceneChange, Pre: der eigene Vogel wechselt im Ritt nicht die Szene (Pfadende im Flussgebiet).
HookAction onFlySceneChangePre(ModContext*, void* args, void* retval, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        if (k == nullptr || !kff::isOwnBird(k) || !sceneVetoActive()) {
            return HOOK_CONTINUE;
        }
        if (retval != nullptr) {
            *static_cast<bool*>(retval) = false;
        }
        if (k->mSceneType == kRiverSceneType && k->mpPath1 != nullptr) {
            const bool end = (k->mPathStep > 0 && k->mPathCurrentPointNo >= k->mpPath1->m_num - 1) ||
                             (k->mPathStep < 0 && k->mPathCurrentPointNo < 1);
            if (end) {
                ++g_stats.stageVeto;
                if (vetoTry(kVsFlyScene)) {
                    kff::logInfo("freiheit: positional scene veto (%s) Pfadende im Flussgebiet, Vogel bei (%.0f,%.0f,%.0f)",
                        kVetoSiteName[kVsFlyScene], static_cast<double>(k->current.pos.x),
                        static_cast<double>(k->current.pos.y), static_cast<double>(k->current.pos.z));
                }
            }
        }
        return HOOK_SKIP_ORIGINAL;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in checkFlySceneChange (original runs)");
        return HOOK_CONTINUE;
    }
}

// mDoCPd_c::read, Post (einmal je Frame, vor den Akteuren): eigener Tick, Warp-Erkennung und Stage-Wechsel-Protokoll.
// Warp: Karte (Status 3) und Ooccoo (Status 1, 2) setzen den Warp-Status, Link startet den Warp erst in seinem Execute
// (checkWarpStart). Der Ritt endet hier vorher als Warp (sonst hielte der Kern ihn fuer ein Ereignis).
void onPadReadPost(ModContext*, void*, void*, void*) {
    try {
        ++g_tick;
        if (rideRunning()) {
            const unsigned ws = dMeter2Info_getWarpStatus();
            if (ws != 0) {
                char text[64];
                std::snprintf(text, sizeof(text), "Warp angefordert (Warp-Status %u)", ws);
                endForScene(kff::EndReason::Warp, text);
            }
        }
        const bool next = dComIfGp_isEnableNextStage() != 0;
        if (next && !g_nextPrev) {
            noteStageChange();
        }
        g_nextPrev = next;
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in mDoCPd_c::read post (scene protocol skipped)");
    }
}

#if defined(KFF_DEV_AUTOTEST)
uint32_t g_logDecomp = 0;  // F1-Messung: Zeilen KORR (Korrektur je Vogel-Frame ueber 60)
#endif

// daKago_c::execute, Post: Bewegung dieses Frames gegen Flaechen pruefen und korrigieren, Untergrenze, Szenen-
// Antrag abbrechen. Danach Matrix und Sphaere neu rechnen, damit das Modell an der korrigierten Stelle steht.
void onBirdExecutePost(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        const bool snapOn = g_snapArmed;  // F1: Pre-Hook dieses Frames hat den Stand vor checkHeight gemerkt
        g_snapArmed = false;
        const bool valid = g_frameValid;
        g_frameValid = false;
        if (k == nullptr || !kff::isOwnBird(k) || !rideRunning()) {
            return;
        }
#if defined(KFF_DEV_AUTOTEST)
        // WP6.2: CrrPos schreibt in jedem Lauf pos.y nach mObjAcch.field_0xb8 (d_bg_s_acch.cpp:354). Steht dort nicht mehr
        // der Waechterwert aus onFrameBird, lief CrrPos in diesem Ritt-Frame (Soll 0, Kontrolllauf KFF_TEST_CRRPOS=1).
        if (g_crrArmed) {
            g_crrArmed = false;
            if (k->mObjAcch.field_0xb8 != kCrrSentinel) {
                ++g_wp6.crrPosRuns;
                kff::logEvery(g_logCrr, 3, 300, "freiheit: CrrPos lief im Ritt (Ritt-Frame %u, Kontrolle %s)",
                    kff::session().rideFrames, crrPosKept() ? "KFF_TEST_CRRPOS" : "nein");
            }
        }
#endif
        cancelPendingScene("Vogel");
        if (!valid) {
            ++g_wp6.skipFrames;  // Ritt-Frame ohne Startposition: keine Abtastung (Soll 0)
            return;
        }
        ++g_stats.frames;
        ++g_frameNo;

        const cXyz start = g_frameStart;
        // F1: Vorgabe = Vogelbewegung ohne den Sprung von checkHeight (Anfang von executeFly). Der Sprung ist die Untergrenze
        // des Pre-Hooks, die sonst unbegrenzt in einem Frame wirkte. Wirksam wird er nur, wenn der Pre-Hook dieses Frames lief.
        const cXyz wantedRaw = k->current.pos;
        cXyz wanted = wantedRaw;
        if (snapOn) {
            float y1 = g_snapPreY;  // checkHeight: erst die Decke, sonst der Boden
            if (y1 > g_snapRoof) {
                y1 = g_snapRoof;
            } else if (y1 < g_snapGround) {
                y1 = g_snapGround;
            }
            wanted.y = wantedRaw.y - (y1 - g_snapPreY);
        }
        cXyz d(wanted.x - start.x, wanted.y - start.y, wanted.z - start.z);
#if defined(KFF_DEV_AUTOTEST)
        // WP6.7 Tunnel-Probe (nur Testbuild): Zusatzweg in Kursrichtung, damit die Abtastung sehr lange Schritte pruefen muss.
        if (g_tunnelExtra > 0.0f) {
            d.x += cM_ssin(k->current.angle.y) * g_tunnelExtra;
            d.z += cM_scos(k->current.angle.y) * g_tunnelExtra;
            // Messung: die Testsonde gehoert zur Vorgabe, sonst zaehlt ihr Weg als Korrektur (corr, KORR, MESS).
            wanted.x += cM_ssin(k->current.angle.y) * g_tunnelExtra;
            wanted.z += cM_scos(k->current.angle.y) * g_tunnelExtra;
            ++g_wp6.tunnelFrames;
        }
        const bool stalledBefore = g_stallCount >= kStallFrames;
#endif
        MoveResult r = slideMove(start, d);
#if defined(KFF_DEV_AUTOTEST)
        const cXyz dbgSlide = r.pos;  // F1-Messung: Zerlegung der Korrektur (nur Testbuild)
#endif
        for (int i = 0; i < r.nHn; ++i) {
            rememberWall(r.hn[i].x, r.hn[i].z);
        }

        // E6: Stillstand (Treffer, keine Vorwaertsbewegung in der Ebene) und Hoehe. Vor der Untergrenze, damit die
        // Untergrenze den aktuellen Modus (Tief oder Komfort) sieht. Nur die Horizontale zaehlt, sonst gilt das
        // Ausweichen selbst als Fortschritt. Ab kStallFrames weicht der Vogel aus (avoidStall), die Gier bleibt.
        const daAlink_c* linkE6 = kff::liveLink();
        if (landingActive()) {
            g_stallSink = false;  // Landeanflug hat Vorrang (Session.landing): kein Ausweichen, keine Tief-Hoehe
            g_low = false;
            g_escHold = 0;
        }
        const float movedXZ = std::sqrt((r.pos.x - start.x) * (r.pos.x - start.x) +
                                        (r.pos.z - start.z) * (r.pos.z - start.z));
        const float wantedXZ = std::sqrt(d.x * d.x + d.z * d.z);
        if (r.hit && movedXZ < 0.5f && wantedXZ > 5.0f && k->speedF > 5.0f) {
            if (g_stallCount < 0xFFFFFFFFu) {
                ++g_stallCount;
            }
            if (g_stallCount > g_stats.stallRunMax) {
                g_stats.stallRunMax = g_stallCount;
            }
            if (g_stallCount == kStallFrames) {
                ++g_stats.stallBreak;  // ein Stillstandsereignis je Stillstand
                kff::logInfo("freiheit: Stillstand an einer Flaeche (%u Frames ohne Vorwaertsweg): Ausweichen, Gier bleibt",
                    static_cast<unsigned>(g_stallCount));
            }
            if (g_stallCount >= kStallFrames && !landingActive()) {
                if (wallSteerOn()) {
                    breakStall(k);  // nur Testschalter KFF_WALL_STEER
                }
#if defined(KFF_DEV_AUTOTEST)
                logEscapeDiag(r.pos, k->current.angle.y, linkE6, "stall");
                {
                    const float yawDeg = static_cast<float>(k->current.angle.y) * 360.0f / 65536.0f;
                    const float dirDeg = std::atan2(d.x, d.z) * 180.0f / 3.14159265f;
                    kff::logEvery(g_logDiag2, 12, 25,
                        "freiheit: DIAG2 yawGrad=%.1f bewegungGrad=%.1f bewegt=%.2f gewollt=%.2f normY=%.2f speedF=%.1f",
                        static_cast<double>(yawDeg), static_cast<double>(dirDeg), static_cast<double>(movedXZ),
                        static_cast<double>(wantedXZ), static_cast<double>(r.lastN.y), static_cast<double>(k->speedF));
                }
#endif
                avoidStall(r.pos, linkE6, k->current.angle.y);
            }
        } else {
            if (g_stallSink) {
                g_stallSink = false;
                g_low = true;           // Tief-Modus bleibt, bis der weiche Aufstieg oben ist
                g_escHold = kEscHold;   // Abklingzeit, damit der Vogel nicht sofort wieder in die Ecke steigt
                kff::logEvery(g_logEsc, 3, 60, "freiheit: Ausweichen beendet (Vorwaertsweg frei), Abklingzeit %u Frames",
                    static_cast<unsigned>(kEscHold));
            }
            if (g_stallCount >= kStallFrames) {
                kff::logInfo("freiheit: Stillstand beendet nach %u Frames", static_cast<unsigned>(g_stallCount));
            }
            g_stallCount = 0;
        }
#if defined(KFF_DEV_AUTOTEST)
        e10Release(stalledBefore, g_stallCount >= kStallFrames);
#endif
#if defined(KFF_DEV_AUTOTEST)
        const cXyz dbgE6 = r.pos;  // F1-Messung (nur Testbuild)
#endif
        settleHeight(r.pos, k);
#if defined(KFF_DEV_AUTOTEST)
        const cXyz dbgSet = r.pos;  // F1-Messung (nur Testbuild)
#endif

        bool water = false;
        bool byLink = false;
        // F1: Geschwindigkeit des Links je Frame. Der Link folgt dem Vogel mit Verzoegerung, sein Weg ist nicht der des Vogels.
        // Vorgabe ist der Weg des Vogels; sonst der Link-Weg aus dem vorigen Frame (ohne Spruenge).
        const daAlink_c* linkV = kff::liveLink();
        cXyz vLink(r.pos.x - start.x, 0.0f, r.pos.z - start.z);
        cXyz linkPosNow(0.0f, 0.0f, 0.0f);
        if (linkFloorOn(linkV)) {
            linkPosNow = linkV->current.pos;
            if (g_linkPrevOn) {
                const cXyz lv(linkPosNow.x - g_linkPrev.x, 0.0f, linkPosNow.z - g_linkPrev.z);
                if (std::fabs(lv.x) <= 150.0f && std::fabs(lv.z) <= 150.0f) {
                    vLink = lv;
                }
            }
            g_linkPrev = linkPosNow;
            g_linkPrevOn = true;
        } else {
            g_linkPrevOn = false;
        }
        // F1: Bremsen, wenn Links Fuesse bei diesem Tempo ueber die naechsten Frames nicht mehr hoch genug kommen. Der Weg je
        // Frame wird hoechstens um kBrakeCorr gekuerzt; der Rest kommt in den naechsten Frames.
        {
            const daAlink_c* linkB = kff::liveLink();
            // Nur bremsen, wenn Links Fuesse jetzt schon ueber dem Gelaende sind: liegen sie darunter, hilft Bremsen nicht mehr.
            if (linkFloorOn(linkB) && groundGap(linkB->current.pos) >= kLinkFootClear - 1.0f) {
                const cXyz dLinkB(linkB->current.pos.x - start.x, linkB->current.pos.y - start.y,
                                  linkB->current.pos.z - start.z);
                const cXyz dv(r.pos.x - start.x, 0.0f, r.pos.z - start.z);
                const float dxz = std::sqrt(dv.x * dv.x + dv.z * dv.z);
                if (dxz > 1.0f && !brakeFeasible(start, r.pos, dv, 1.0f, dLinkB, vLink)) {
                    const float fMin = std::max(0.1f, 1.0f - kBrakeCorr / dxz);
                    float fUse = fMin;
                    for (float f = 0.9f; f >= fMin - 1.0e-3f; f -= 0.1f) {
                        if (brakeFeasible(start, r.pos, dv, f, dLinkB, vLink)) {
                            fUse = f;
                            break;
                        }
                    }
                    r.pos.x = start.x + dv.x * fUse;
                    r.pos.z = start.z + dv.z * fUse;
                    kff::logTelemetry(g_logBrake, 40, 200, "freiheit: Bremsen auf %.2f des Weges (Weg %.1f, Link-Fuss erreichbar)",
                        static_cast<double>(fUse), static_cast<double>(dxz));
                }
            }
        }
        // F1: vorausschauende Hebung, hoechstens kLiftPerTick je Frame. Die Link-Position ist die von Frame-Beginn (der Link folgt
        // erst nach dem Vogel), der Versatz dLink bleibt im Frame gleich. Die Hebung endet an einer Decke (senkrechte Abtastung),
        // damit der Durchflug-Schutz nicht erst hinterher den ganzen Schritt zuruecksetzt. Die Unterseite (Abtastpunkt 6) liegt
        // beim Steigen aus dem Boden heraus und blockiert nicht.
        const daAlink_c* linkNow = kff::liveLink();
        const bool linkOn = linkFloorOn(linkNow);
        const cXyz vxz(r.pos.x - start.x, 0.0f, r.pos.z - start.z);
        const cXyz dLink = linkOn ? cXyz(linkNow->current.pos.x - start.x, linkNow->current.pos.y - start.y,
                                         linkNow->current.pos.z - start.z)
                                  : cXyz(0.0f, 0.0f, 0.0f);
        const float vy = r.pos.y - start.y;
        float lift = 0.0f;
        lift = floorLift(r.pos, vxz, vy, k, linkOn ? linkNow : nullptr, dLink, linkPosNow, vLink, &water, &byLink);
        const float fl = lift > 0.0f ? r.pos.y + lift : kNoFloor;
        if (lift > 0.0f) {
            Contact up;
            sweep(r.pos, cXyz(0.0f, lift, 0.0f), up);
            if (up.hit && up.sample != 6) {
                lift = std::max(0.0f, lift * up.t - kSkin);
            }
        }
        if (lift > 0.0f) {
            r.pos.y += lift;
            if (byLink) {
                ++g_stats.linkClamp;
            } else if (water) {
                ++g_stats.waterClamp;
            } else {
                ++g_stats.groundClamp;
            }
            kff::logTelemetry(g_logClamp, 3, 300, "freiheit: Untergrenze (%s) hebt den Vogel um %.1f auf y=%.1f (Ziel %.1f)",
                byLink ? "Link-Fuss" : (water ? "Wasser" : "Gelaende"), static_cast<double>(lift),
                static_cast<double>(r.pos.y), static_cast<double>(fl));
        }

#if defined(KFF_DEV_AUTOTEST)
        const cXyz dbgFloor = r.pos;  // F1-Messung (nur Testbuild)
        const float dbgFl = fl;
#endif
        // E6-Messung: Links Fuesse waehrend des Ausweichens, tatsaechlicher Abstand nach dem Frame (nicht Hypothesen).
        if (g_stallSink && linkNow != nullptr) {
            const float lg = groundGap(linkNow->current.pos);
            if (lg < g_stats.escLinkMin) {
                g_stats.escLinkMin = lg;
            }
            if (lg < kLinkFootClear) {
                kff::logEvery(g_logLinkLow, 3, 30,
                    "freiheit: Link-Fuss %.1f ueber Gelaende im Ausweichen (Link y=%.1f, Vogel y=%.1f, Untergrenze %.1f)",
                    static_cast<double>(lg), static_cast<double>(linkNow->current.pos.y), static_cast<double>(r.pos.y),
                    static_cast<double>(fl));
            }
        }

        const float corr = std::sqrt((r.pos.x - wanted.x) * (r.pos.x - wanted.x) +
                                     (r.pos.y - wanted.y) * (r.pos.y - wanted.y) +
                                     (r.pos.z - wanted.z) * (r.pos.z - wanted.z));
        if (corr > g_stats.maxCorr) {
            g_stats.maxCorr = corr;
        }
#if defined(KFF_DEV_AUTOTEST)
        // Nur fuer die Testzeile logMess (D11): im Release kein zusaetzlicher GroundCross je Ritt-Frame.
        const float bodenAbstand = groundGap(r.pos);
        if (bodenAbstand >= 1.0e8f) {
            ++g_stats.abyssFrames;  // kein Gelaende unter dem Vogel (Abgrund oder Luecke in der Karte)
        } else if (bodenAbstand < g_stats.minBirdGap) {
            g_stats.minBirdGap = bodenAbstand;
        }
#endif

        if (r.hit) {
            ++g_stats.slideFrames;
            g_stats.slideSteps += static_cast<uint32_t>(r.steps);
            kff::logTelemetry(g_logSlide, 40, 100, "freiheit: Gleiten: %d Schritte, Ende (%.0f,%.0f,%.0f)", r.steps,
                static_cast<double>(r.pos.x), static_cast<double>(r.pos.y), static_cast<double>(r.pos.z));
            slideHeading(k, r);
        }
        if (r.wallHit) {
            ++g_stats.wallContacts;
            if (!g_fotoWand) {
                g_fotoWand = true;
                photo("wand", kff::session().rideFrames);
            }
        }
        if (r.ceilHit) {
            ++g_stats.ceilContacts;
            if (!g_fotoDecke) {
                g_fotoDecke = true;
                photo("decke", kff::session().rideFrames);
            }
        }
        if (r.floorHit) {
            ++g_stats.floorContacts;
        }

        const float moved = std::sqrt((r.pos.x - start.x) * (r.pos.x - start.x) +
                                      (r.pos.y - start.y) * (r.pos.y - start.y) +
                                      (r.pos.z - start.z) * (r.pos.z - start.z));
        if (r.hit && moved >= 5.0f) {
            ++g_stats.glideFrames;  // Treffer und echte Ortsaenderung entlang der Flaeche
        }
        // Durchflug-Sperre (E7-2): liegt das Ende hinter einer Flaeche, geht der Vogel auf den Punkt vor dem ersten
        // Treffer zurueck. Danach zaehlt nur ein Rest-Durchflug als Durchdringung (Soll 0).
        cXyz kept = r.pos;
        if (passedThrough(start, r.pos, kept)) {
            ++g_stats.penetrationFix;
            kff::logEvery(g_logPen, 3, 60,
                "freiheit: Durchflug verhindert (Ende hinter Flaeche), Vogel auf den Punkt vor der Flaeche gesetzt");
            r.pos = kept;
            cXyz again(0.0f, 0.0f, 0.0f);
            if (passedThrough(start, kept, again)) {
                ++g_stats.penetration;
                kff::logEvery(g_logPen, 3, 60, "freiheit: DURCHDRINGUNG Rest nach Korrektur pos=(%.0f,%.0f,%.0f)",
                    static_cast<double>(kept.x), static_cast<double>(kept.y), static_cast<double>(kept.z));
            }
        }
#if defined(KFF_DEV_AUTOTEST)
        // F1-Diagnose (nur Testbuild): je Vogel-Frame mit Link-Abstand unter 60 oder gekappter Hebung eine Zeile DIAG.
        // Link-Hoehe und Abstand gelten nach dem Vogelschritt, vor dem Link-Schritt dieses Frames.
        if (linkNow != nullptr && !linkDead(linkNow)) {
            const float dGap = groundGap(linkNow->current.pos);
            if (dGap < 60.0f || g_dbgRaw > kLiftPerTick) {
                kff::logEvery(g_logDiag3, 400, 0,
                    "freiheit: DIAG f=%u y0=%.1f wanted=%.1f fin=%.1f | Link y=%.1f gap=%.1f dLink=(%.1f,%.1f,%.1f) | raw=%.1f lift=%.1f",
                    kff::session().rideFrames, static_cast<double>(start.y), static_cast<double>(wanted.y),
                    static_cast<double>(r.pos.y), static_cast<double>(linkNow->current.pos.y), static_cast<double>(dGap),
                    static_cast<double>(linkNow->current.pos.x - start.x), static_cast<double>(linkNow->current.pos.y - start.y),
                    static_cast<double>(linkNow->current.pos.z - start.z), static_cast<double>(g_dbgRaw),
                    static_cast<double>(lift));
            }
        }
#endif
#if defined(KFF_DEV_AUTOTEST)
        // F1-Messung (nur Testbuild): Zerlegung der Korrektur je Vogel-Frame, Zeile KORR ab 60 Einheiten.
        {
            const float corrFin = std::sqrt((r.pos.x - wanted.x) * (r.pos.x - wanted.x) +
                                            (r.pos.y - wanted.y) * (r.pos.y - wanted.y) +
                                            (r.pos.z - wanted.z) * (r.pos.z - wanted.z));
            if (corrFin > 60.0f) {
                const float corrSlide = std::sqrt((dbgSlide.x - wanted.x) * (dbgSlide.x - wanted.x) +
                                                  (dbgSlide.y - wanted.y) * (dbgSlide.y - wanted.y) +
                                                  (dbgSlide.z - wanted.z) * (dbgSlide.z - wanted.z));
                const float corrE6 = std::sqrt((dbgE6.x - wanted.x) * (dbgE6.x - wanted.x) +
                                               (dbgE6.y - wanted.y) * (dbgE6.y - wanted.y) +
                                               (dbgE6.z - wanted.z) * (dbgE6.z - wanted.z));
                const float corrSet = std::sqrt((dbgSet.x - wanted.x) * (dbgSet.x - wanted.x) +
                                                (dbgSet.y - wanted.y) * (dbgSet.y - wanted.y) +
                                                (dbgSet.z - wanted.z) * (dbgSet.z - wanted.z));
                const float corrFloor = std::sqrt((dbgFloor.x - wanted.x) * (dbgFloor.x - wanted.x) +
                                                  (dbgFloor.y - wanted.y) * (dbgFloor.y - wanted.y) +
                                                  (dbgFloor.z - wanted.z) * (dbgFloor.z - wanted.z));
                float linkGap = 1.0e9f;
                float linkPred = 1.0e9f;
                if (linkNow != nullptr && !linkDead(linkNow)) {
                    linkGap = groundGap(linkNow->current.pos);
                    const cXyz lp(linkNow->current.pos.x + (r.pos.x - start.x),
                                  linkNow->current.pos.y + (r.pos.y - start.y),
                                  linkNow->current.pos.z + (r.pos.z - start.z));
                    linkPred = groundGap(lp);
                }
                kff::logEvery(g_logDecomp, 400, 0,
                    "freiheit: KORR f=%u corr=%.1f |d|=%.1f slide=%.1f e6=%.1f set=%.1f floor=%.1f fin=%.1f fl=%.1f "
                    "y0=%.1f yW=%.1f yF=%.1f hit=%d wand=%d decke=%d boden=%d stall=%u sink=%d low=%d tunnel=%.0f "
                    "linkGap=%.1f linkPred=%.1f byLink=%d water=%d",
                    kff::session().rideFrames, static_cast<double>(corrFin),
                    static_cast<double>(std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z)), static_cast<double>(corrSlide),
                    static_cast<double>(corrE6), static_cast<double>(corrSet), static_cast<double>(corrFloor),
                    static_cast<double>(corrFin), static_cast<double>(dbgFl), static_cast<double>(start.y),
                    static_cast<double>(wanted.y), static_cast<double>(r.pos.y), r.hit ? 1 : 0, r.wallHit ? 1 : 0,
                    r.ceilHit ? 1 : 0, r.floorHit ? 1 : 0, g_stallCount, g_stallSink ? 1 : 0, g_low ? 1 : 0,
                    static_cast<double>(g_tunnelExtra), static_cast<double>(linkGap), static_cast<double>(linkPred),
                    byLink ? 1 : 0, water ? 1 : 0);
            }
        }
#endif
        const bool changed = r.pos.x != wantedRaw.x || r.pos.y != wantedRaw.y || r.pos.z != wantedRaw.z;
        if (changed) {
            k->current.pos = r.pos;
            k->mtx_set();
            k->cc_set();
        }
        refreshAcch(k);  // WP6: CrrPos lief nicht, Schatten und Trefferbits an der Endposition dieses Frames
        watchStuck(k->current.pos);
#if defined(KFF_DEV_AUTOTEST)
        roofDiag(k);
#endif
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in bird execute post (frame skipped)");
    }
}

// daKago_c::execute, Pre (nach dem Kern-Pre, Prioritaet kFreiheit, kein SKIP): Startposition fuer die Abtastung auch in
// Ritt-Frames ohne onFrameBird. Im Frame des B-Tipps beginnt der Kern den Landeanflug und kehrt vor notifyFrameBird
// zurueck; ohne diesen Pre liefe der Vogel diesen Frame ohne Abtastung und (WP6) ohne CrrPos (Befund skipFrames=1 je
// Landeanflug, wp6p-ruf-1 und wp6p-timeout-1).
HookAction onBirdExecutePre(ModContext*, void* args, void*, void*) {
    try {
        daKago_c* k = kff::argOf<daKago_c*>(args, 0);
        if (k != nullptr && !g_frameValid && rideRunning() && kff::isOwnBird(k)) {
            crrPosOff(k);
            g_frameStart = k->current.pos;
            g_frameValid = true;
        }
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in bird execute pre (frame skipped)");
    }
    return HOOK_CONTINUE;
}

// daAlink_c::execute, Post: Szenen-Antrag von Link (Ladezone, Restart) abbrechen.
void onLinkExecutePost(ModContext*, void* args, void*, void*) {
    try {
        daAlink_c* link = kff::argOf<daAlink_c*>(args, 0);
        if (link == nullptr) {
            return;
        }
        cancelPendingScene("Link");
    } catch (...) {
        ++g_stats.exceptions;
        kff::logEvery(g_logExc, 3, 300, "freiheit: exception in link execute post (frame skipped)");
    }
}

// Acch-Bits des Vogels fuer den Ritt. LINE_CHECK ist der Teil, der den Vogel an geneigten Decken festhielt.
uint32_t birdBits() {
    return kBirdBitsBase | (roofKept() ? 0u : kRoofBit);
}

// ---------------------------------------------------------------------------------------------
// Szenarien: nur Eingaben und Zeitplan, keine Tasten- oder Mausereignisse. Nur im Testbuild (D11).
// ---------------------------------------------------------------------------------------------

#if defined(KFF_DEV_AUTOTEST)
using kff::autotest::Context;

constexpr uint32_t kCallAt = 300;  // erster Ruf, sobald Link bereit ist
constexpr uint32_t kHold = 30;     // R gehalten, Ticks
constexpr uint32_t kPress = 15;    // B-Tipp in der Mitte des R-Halts

struct Call {
    bool armed = false;
    bool pressed = false;
    uint32_t start = 0;
};

// Ruf: R gehalten, B im Halt. Fertig, wenn der Ruf angelaufen ist.
void runCall(Context& c, Call& call) {
    if (!call.armed && c.frame >= kCallAt && c.canCall()) {
        call.armed = true;
        call.start = c.frame;
        c.note("freiheit: Ruf (R halten, B tippen)");
    }
    if (call.armed && c.frame >= call.start && c.frame < call.start + kHold) {
        c.holdR();
    }
    if (call.armed && !call.pressed && c.frame == call.start + kPress) {
        call.pressed = true;
        c.tapB();
    }
}

void logBird(const char* tag, uint32_t rideFrame) {
    daKago_c* k = kff::liveBird();
    if (k == nullptr) {
        kff::logInfo("freiheit: %s f=%u Vogel fehlt", tag, rideFrame);
        return;
    }
    logDiagBird(tag, k, rideFrame);
}

// Ereignis-Texte mit A weiterschalten, sonst haengt der Ablauf in einem Ereignis mit "Next A".
void advanceEvent(Context& c) {
    if (inEvent() && c.frame % 20 == 0) {
        c.tapA();
    }
}

// Ende nach dem Ritt: 200 Ticks nach dem Ritt-Ende ohne laufendes Ereignis, dann fertig.
bool rideFinished(Context& c, bool& rode, uint32_t& endFrame) {
    if (c.sess.phase == kff::Phase::Riding) {
        rode = true;
        return false;
    }
    if (!rode) {
        return false;
    }
    if (endFrame == 0) {
        endFrame = c.frame + 200;
        c.note("freiheit: Ritt beendet (Absteigen oder Ereignis)");
    }
    if (inEvent() || c.frame < endFrame) {
        return false;
    }
    c.note("autotest: timeline complete");
    return true;
}

bool g_grenzeRode = false;
uint32_t g_grenzeEnd = 0;
bool g_szeneRode = false;
uint32_t g_szeneEnd = 0;
bool g_freiRode = false;
uint32_t g_freiEnd = 0;

// Fussphase nach dem Ritt (innen, fuss): 30 s zu Fuss, sobald der Ritt beendet ist und Link frei steht (kein Ereignis,
// keine Demo). Link darf nicht durch den Boden fallen, die Acch-Bits des Vogels sind zurueckgenommen. Ohne Start nach
// kInnenWalkDeadline Ticks bricht das Szenario ab, es haengt nicht.
constexpr uint32_t kScenarioDeadline = 15000;  // Ticks (ca. 500 s): danach endet jedes Szenario
constexpr uint32_t kInnenWalkDelay = 240;      // Ticks nach dem Ritt-Ende, bis Vogel weg und Link gelandet ist
constexpr uint32_t kInnenWalkFrames = 900;     // 30 s zu Fuss bei 30 Ticks je Sekunde
constexpr uint32_t kInnenWalkDeadline = 3000;  // Ticks nach dem Ritt-Ende; ohne Fussphase bis dahin Abbruch

struct WalkState {
    bool rode = false;      // der Ritt hat begonnen
    uint32_t endFrame = 0;  // Frame des Ritt-Endes (0: noch nicht festgestellt)
    bool started = false;
    uint32_t start = 0;
    float minGap = 1.0e9f;  // kleinster Abstand Link-Fuss zum Gelaende in der Fussphase
};
WalkState g_innenWalk;
WalkState g_fussWalk;
Call g_innenCall;
Call g_fussCall;
bool g_innenDismounted = false;  // B zum Absteigen gedrueckt (innen)
bool g_fussDismounted = false;   // B zum Absteigen gedrueckt (fuss)

// Fussphase: geradeaus, Bogen links, Bogen rechts, geradeaus; alle 150 Ticks ein Sprung (A).
void walkStep(Context& c, uint32_t t, WalkState& w, const char* tag) {
    if (daAlink_c* link = kff::liveLink()) {
        w.minGap = std::min(w.minGap, groundGap(link->current.pos));
    }
    float x = 0.0f;
    float y = -1.0f;  // Stick nach oben: geradeaus
    if (t >= 300 && t < 450) {
        x = -0.7f;
        y = -0.7f;
    } else if (t >= 450 && t < 600) {
        x = 0.7f;
        y = -0.7f;
    }
    c.stick(x, y);
    if (t % 150 == 75) {
        c.tapA();
    }
    if (t % 150 == 0) {
        if (daAlink_c* link = kff::liveLink()) {
            kff::logInfo("freiheit: %s Fuss t=%u link=(%.1f,%.1f,%.1f) proc=%u eventRun=%d gap=%.1f", tag, t,
                static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                static_cast<double>(link->current.pos.z), static_cast<unsigned>(link->mProcID),
                link->checkEventRun() != 0 ? 1 : 0, static_cast<double>(groundGap(link->current.pos)));
        }
        logMess(tag);
    }
}

// Nach dem Ritt: warten, bis Link frei steht, dann die Fussphase. Rueckgabe true: Szenario fertig.
bool walkAfterRide(Context& c, WalkState& w, const char* tag) {
    if (!w.rode) {
        return false;
    }
    if (!w.started) {
        if (w.endFrame == 0) {
            w.endFrame = c.frame;
            c.note("freiheit: Ritt beendet (Absteigen oder Ereignis), Fussphase folgt");
        }
        if (c.frame >= w.endFrame + kInnenWalkDeadline) {
            c.note("freiheit: Fussphase nicht gestartet (Zeitlimit nach dem Ritt)");
            logMess(tag);
            return true;
        }
        if (c.frame < w.endFrame + kInnenWalkDelay) {
            return false;
        }
        w.started = true;
        w.start = c.frame;
        c.note("freiheit: Fussphase (30 s Laufen und Springen)");
    }
    const uint32_t t = c.frame - w.start;
    walkStep(c, t, w, tag);
    if (t >= kInnenWalkFrames) {
        kff::logInfo("freiheit: %s Fussphase Ende: minLinkFussGelaende=%.1f (Link ueber Boden, Acch-Bits zurueck)",
            tag, static_cast<double>(w.minGap));
        c.note("autotest: timeline complete");
        return true;
    }
    return false;
}

// innen: Kanalisation R_SP107. Steigen bis unter die Decke, Kurven, Absteigen nahe am Boden, dann Fussphase.
// Ereignis bei Ritt-Frame 211: der Ritt endet vor dem Szenenwechsel, die Fussphase folgt nach dem Ende der Demo.
bool innenTick(Context& c) {
    if (c.frame >= kScenarioDeadline) {
        c.note("autotest: Zeitlimit des Szenarios innen erreicht (kein Ende gemeldet)");
        return true;
    }
    advanceEvent(c);
    if (inEvent()) {
        return false;
    }
    runCall(c, g_innenCall);
    if (c.sess.phase == kff::Phase::Riding) {
        g_innenWalk.rode = true;
        const uint32_t f = c.sess.rideFrames;
        float x = 0.0f;
        float y = 0.0f;
        if (f < 240) {
            y = -0.6f;  // Nase hoch: steigen, bis unter die Decke
        } else if (f >= 420 && f < 520) {
            x = -1.0f;
        } else if (f >= 520 && f < 620) {
            x = 1.0f;
        } else if (f >= 620) {
            y = 0.6f;  // Nase runter: absinken bis zum Boden (Untergrenze 300 ueber Gelaende)
        }
        c.stick(x, y);
        if (f % 120 == 0) {
            logBird("innen", f);
        }
        // B zum Absteigen, sobald der Vogel nahe am Boden ist (Abstand unter 350), spaetestens bei Ritt-Frame 900.
        float bodenAbstand = 1.0e9f;
        if (daKago_c* k = kff::liveBird()) {
            bodenAbstand = groundGap(k->current.pos);
        }
        if (!g_innenDismounted && ((f >= 640 && bodenAbstand < 350.0f) || f >= 900)) {
            g_innenDismounted = true;
            c.tapB();
            kff::logInfo("freiheit: innen B (Absteigen) bei Ritt-Frame %u, Bodenabstand Vogel %.1f", f,
                static_cast<double>(bodenAbstand >= 1.0e8f ? -1.0f : bodenAbstand));
        }
        return false;
    }
    return walkAfterRide(c, g_innenWalk, "innen");
}

// fuss: Acch-Freigabe nach dem Absteigen am Boden (F_SP121, dort ohne Ereignis). Ritt: Nase runter bis knapp ueber den
// Boden, B, dann die Fussphase. Link darf nicht durch den Boden fallen.
bool fussTick(Context& c) {
    if (c.frame >= kScenarioDeadline) {
        c.note("autotest: Zeitlimit des Szenarios fuss erreicht (kein Ende gemeldet)");
        return true;
    }
    advanceEvent(c);
    if (inEvent()) {
        return false;
    }
    runCall(c, g_fussCall);
    if (c.sess.phase == kff::Phase::Riding) {
        g_fussWalk.rode = true;
        const uint32_t f = c.sess.rideFrames;
        c.stick(0.0f, 0.6f);  // Nase runter bis knapp ueber den Boden
        if (f % 120 == 0) {
            logBird("fuss", f);
        }
        float bodenAbstand = 1.0e9f;
        if (daKago_c* k = kff::liveBird()) {
            bodenAbstand = groundGap(k->current.pos);
        }
        if (!g_fussDismounted && ((f >= 120 && bodenAbstand < 350.0f) || f >= 1500)) {
            g_fussDismounted = true;
            c.tapB();
            kff::logInfo("freiheit: fuss B (Absteigen) bei Ritt-Frame %u, Bodenabstand Vogel %.1f", f,
                static_cast<double>(bodenAbstand >= 1.0e8f ? -1.0f : bodenAbstand));
        }
        return false;
    }
    return walkAfterRide(c, g_fussWalk, "fuss");
}

// grenze: Geradeausflug nach Westen (-X) ueber Bergwand und Kartenrand, ohne Stick. Hoechsttempo ueber cruise_speed.
Call g_grenzeCall;
bool g_grenzeFaced = false;
uint32_t g_grenzeDismountFrame = 0;
bool g_grenzeDismounted = false;
constexpr uint32_t kGrenzeRideEnd = 2600;

bool grenzeTick(Context& c) {
    if (c.frame >= kScenarioDeadline) {
        c.note("autotest: Zeitlimit des Szenarios grenze erreicht (kein Ende gemeldet)");
        return true;
    }
    advanceEvent(c);
    if (inEvent()) {
        return false;
    }
    if (!g_grenzeFaced && c.frame + 10 >= kCallAt && !g_grenzeCall.armed) {
        g_grenzeFaced = true;
        if (daAlink_c* link = kff::liveLink()) {
            link->current.angle.y = static_cast<s16>(-0x4000);  // Blick nach Westen (-X)
            c.note("freiheit: grenze, Link blickt nach Westen");
        }
    }
    runCall(c, g_grenzeCall);
    if (rideFinished(c, g_grenzeRode, g_grenzeEnd)) {
        logMess("grenze-ende");
        return true;
    }
    if (c.sess.phase != kff::Phase::Riding) {
        return false;
    }
    const uint32_t f = c.sess.rideFrames;
    c.stick(0.0f, 0.0f);
    if (f % 300 == 0) {
        logBird("grenze", f);
    }
    if (!g_grenzeDismounted && f == kGrenzeRideEnd) {
        g_grenzeDismounted = true;
        g_grenzeDismountFrame = c.frame;
        c.tapB();
        c.note("freiheit: grenze B (Absteigen)");
    }
    if (g_grenzeDismounted && c.frame >= g_grenzeDismountFrame + 120) {
        logMess("grenze");
        c.note("autotest: timeline complete");
        return true;
    }
    return false;
}

// szene: Vogel-Szenenwechsel im Ritt, soll abgewiesen werden (Ladezonen: Szenario ladezone-anflug).
Call g_szeneCall;
bool g_szeneDone = false;
uint32_t g_szeneDismountFrame = 0;
bool g_szeneDismounted = false;

bool szeneTick(Context& c) {
    if (c.frame >= kScenarioDeadline) {
        c.note("autotest: Zeitlimit des Szenarios szene erreicht (kein Ende gemeldet)");
        return true;
    }
    advanceEvent(c);
    if (inEvent()) {
        return false;
    }
    runCall(c, g_szeneCall);
    if (rideFinished(c, g_szeneRode, g_szeneEnd)) {
        logMess("szene-ende");
        return true;
    }
    if (c.sess.phase != kff::Phase::Riding) {
        return false;
    }
    const uint32_t f = c.sess.rideFrames;
    c.stick(0.0f, 0.0f);
    // WP1: kein kuenstlicher Ladezonen-Antrag mehr (setNextStage mit 4 Argumenten ist der Menue- und Konsolenpfad und
    // beendet jetzt den Ritt). Echte Ladezonen prueft das Szenario ladezone-anflug.
    if (f == 160) {
        kff::logInfo("freiheit: szene Antrag nach dem Frame: pending=%d", dComIfGp_isEnableNextStage() != 0 ? 1 : 0);
    }
    if (f == 220) {
        if (daKago_c* k = kff::liveBird()) {
            const int r = k->setSceneChange(3);  // Vogel-Szenenwechsel, soll abgewiesen werden
            kff::logInfo("freiheit: szene Vogel setSceneChange(3) -> %d", r);
        }
    }
    if (f % 150 == 0 && f > 0) {
        logBird("szene", f);
    }
    if (!g_szeneDismounted && f == 400) {
        g_szeneDismounted = true;
        g_szeneDismountFrame = c.frame;
        c.tapB();
        c.note("freiheit: szene B (Absteigen)");
    }
    if (g_szeneDismounted && c.frame >= g_szeneDismountFrame + 120) {
        g_szeneDone = true;
        logMess("szene");
        c.note("autotest: timeline complete");
        return true;
    }
    return false;
}

// freiheit: F_SP121, Hoechsttempo (cruise_speed). Frontalanflug auf die Bergwand (x=-81410, z=-23950) mit Gleiten an
// der Wand, Sturzflug bis zum Boden (Untergrenze), Abfangen bis waagerecht, Geradeausflug nach Westen ueber den
// Kartenrand hinaus (220000 Einheiten), Absteigen. Eingaben nur Stick und B.
enum class FreiPhase : uint8_t { Approach, Glide, Dive, Pull, Straight, Done };
FreiPhase g_freiPhase = FreiPhase::Approach;
uint32_t g_freiPhaseFrame = 0;
Call g_freiCall;
bool g_freiStartSet = false;
bool g_freiDismounted = false;
uint32_t g_freiDismountFrame = 0;
bool g_freiFaced = false;
uint32_t g_freiWallBase = 0;                  // Wandkontakt-Frames vor dem Anflug (Vergleich am Ende des Anflugs)
float g_freiStartX = 0.0f;                    // Strecke des Geradeausflugs ab hier (nicht ab Ritt-Start)
float g_freiStartZ = 0.0f;
float g_freiGlideX0 = 0.0f;                   // Gleiten: Startpunkt und Ausdehnung der Bewegung
float g_freiGlideZ0 = 0.0f;
float g_freiGlideMinX = 0.0f;
float g_freiGlideMaxX = 0.0f;
float g_freiGlideMinZ = 0.0f;
float g_freiGlideMaxZ = 0.0f;
bool g_freiEdgeMarked = false;
constexpr uint32_t kFreiApproachMax = 1800;   // Zeitlimit des Anflugs in Ritt-Frames
constexpr uint32_t kFreiGlideFrames = 300;    // Gleiten an der Wand in Ritt-Frames
constexpr uint32_t kFreiDiveMax = 400;        // Zeitlimit des Sturzflugs
constexpr uint32_t kFreiPullMax = 300;        // Zeitlimit des Abfangens
constexpr float kFreiEdgeMark = 20000.0f;     // Foto-Marke jenseits des Kartenrands
constexpr float kFreiDistance = 220000.0f;    // Geradeausflug in Einheiten
constexpr uint32_t kFreiStraightMax = 9000;   // Zeitlimit des Geradeausflugs

bool freiheitTick(Context& c) {
    if (c.frame >= kScenarioDeadline) {
        c.note("autotest: Zeitlimit des Szenarios freiheit erreicht (kein Ende gemeldet)");
        return true;
    }
    advanceEvent(c);
    if (inEvent()) {
        return false;
    }
    if (!g_freiFaced && c.frame + 10 >= kCallAt && !g_freiCall.armed) {
        g_freiFaced = true;
        // Anflugkurs wird am Ritt-Start auf den Kurs des VOGELS gesetzt (siehe unten). Bis dahin stand er hier auf dem
        // Link, den der Ritt nicht uebernimmt (Befund flugbahn: Vogel flog mit Gier 0 nach +Z).
        c.note("freiheit: Anflugkurs auf die Bergwand wird am Ritt-Start gesetzt");
    }
    runCall(c, g_freiCall);
    if (rideFinished(c, g_freiRode, g_freiEnd)) {
        logMess("freiheit-ende");
        return true;
    }
    if (c.sess.phase != kff::Phase::Riding) {
        return false;
    }
    const uint32_t f = c.sess.rideFrames;
    daKago_c* k = kff::liveBird();
    if (k == nullptr) {
        return false;
    }
    if (!g_freiStartSet) {
        g_freiStartSet = true;
        // Anflugkurs auf die Bergwand bei (-81410, -23950) (Auftrag freiheit): Gier aus der Position des Vogels am Ritt-Start.
        const float dxA = -81410.0f - k->current.pos.x;
        const float dzA = -23950.0f - k->current.pos.z;
        k->current.angle.y = cM_atan2s(dxA, dzA);
        k->shape_angle.y = k->current.angle.y;
        kff::logInfo("freiheit: Anflugkurs (Vogel) auf die Bergwand (x=-81410, z=-23950): Gier %d, Entfernung %.0f",
            static_cast<int>(k->current.angle.y), static_cast<double>(std::sqrt(dxA * dxA + dzA * dzA)));
        g_freiWallBase = g_stats.wallContacts;
        g_freiStartX = k->current.pos.x;
        g_freiStartZ = k->current.pos.z;
        g_freiPhaseFrame = f;
        c.note("freiheit: Anflug auf die Bergwand (Stick neutral, Hoechsttempo)");
    }
    float x = 0.0f;
    float y = 0.0f;
    const uint32_t inPhase = f - g_freiPhaseFrame;
    const cXyz pos = k->current.pos;
    switch (g_freiPhase) {
    case FreiPhase::Approach:
        if (g_stats.wallContacts > g_freiWallBase || inPhase >= kFreiApproachMax) {
            const bool contact = g_stats.wallContacts > g_freiWallBase;
            g_freiPhase = FreiPhase::Glide;
            g_freiPhaseFrame = f;
            g_freiGlideX0 = g_freiGlideMinX = g_freiGlideMaxX = pos.x;
            g_freiGlideZ0 = g_freiGlideMinZ = g_freiGlideMaxZ = pos.z;
            kff::logInfo("freiheit: Anflug Ende (Ritt-Frame %u, Wandkontakt %s) pos=(%.0f,%.0f,%.0f)", f,
                contact ? "ja" : "nein", static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z));
            logMess("anflug");
            c.note("freiheit: Gleiten an der Wand (300 Frames, Stick neutral)");
        }
        break;
    case FreiPhase::Glide: {
        g_freiGlideMinX = std::min(g_freiGlideMinX, pos.x);
        g_freiGlideMaxX = std::max(g_freiGlideMaxX, pos.x);
        g_freiGlideMinZ = std::min(g_freiGlideMinZ, pos.z);
        g_freiGlideMaxZ = std::max(g_freiGlideMaxZ, pos.z);
        if (inPhase == kFreiGlideFrames / 2) {
            photo("gleiten", f);
        }
        if (inPhase >= kFreiGlideFrames) {
            const float dx = pos.x - g_freiGlideX0;
            const float dz = pos.z - g_freiGlideZ0;
            kff::logInfo("freiheit: Gleiten Ende: %u Frames, Weg dx=%.0f dz=%.0f, x %.0f..%.0f, z %.0f..%.0f, "
                         "Kurs %d, Ende pos=(%.0f,%.0f,%.0f)",
                kFreiGlideFrames, static_cast<double>(dx), static_cast<double>(dz),
                static_cast<double>(g_freiGlideMinX), static_cast<double>(g_freiGlideMaxX),
                static_cast<double>(g_freiGlideMinZ), static_cast<double>(g_freiGlideMaxZ),
                static_cast<int>(k->current.angle.y), static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z));
            g_freiPhase = FreiPhase::Dive;
            g_freiPhaseFrame = f;
            logMess("gleiten");
            c.note("freiheit: Sturzflug (Stick nach unten)");
        }
        break;
    }
    case FreiPhase::Dive:
        y = 1.0f;  // Nase runter
        if ((k->mGroundHeight != kNoFloor && pos.y - k->mGroundHeight <= 5.0f) || inPhase >= kFreiDiveMax) {
            g_freiPhase = FreiPhase::Pull;
            g_freiPhaseFrame = f;
            kff::logInfo("freiheit: Sturzflug Ende (Ritt-Frame %u) pos=(%.0f,%.0f,%.0f) Untergrenze=%.1f", f,
                static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z),
                static_cast<double>(k->mGroundHeight));
            logMess("sturzflug");
            c.note("freiheit: Bodenkontakt (Untergrenze), Abfangen");
        }
        break;
    case FreiPhase::Pull: {
        y = -0.6f;  // Nase hoch, bis die Lage wieder waagerecht ist
        int nick = static_cast<int>(k->current.angle.x);
        if (nick < 0) {
            nick = -nick;
        }
        if (nick < 1000 || inPhase >= kFreiPullMax) {
            g_freiPhase = FreiPhase::Straight;
            g_freiPhaseFrame = f;
            g_freiStartX = pos.x;  // Strecke des Geradeausflugs ab hier (nicht ab Ritt-Start)
            g_freiStartZ = pos.z;
            logMess("abfangen");
            c.note("freiheit: Geradeausflug in Kursrichtung ueber den Kartenrand");
        }
        break;
    }
    case FreiPhase::Straight: {
        const float dx = pos.x - g_freiStartX;
        const float dz = pos.z - g_freiStartZ;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (!g_freiEdgeMarked && dist >= kFreiEdgeMark) {
            g_freiEdgeMarked = true;
            kff::logInfo("freiheit: Kartenrand-Marke: %.0f Einheiten jenseits des Strecken-Starts, pos=(%.0f,%.0f,%.0f), "
                         "Bodenabstand %.0f", static_cast<double>(dist), static_cast<double>(pos.x),
                static_cast<double>(pos.y), static_cast<double>(pos.z), static_cast<double>(groundGap(pos)));
            photo("kartenrand", f);
        }
        if (dist >= kFreiDistance || inPhase >= kFreiStraightMax) {
            g_freiPhase = FreiPhase::Done;
            g_freiPhaseFrame = f;
            g_freiDismounted = true;
            g_freiDismountFrame = c.frame;
            c.tapB();
            kff::logInfo("freiheit: Geradeausflug beendet, Strecke %.0f, Ritt-Frame %u pos=(%.0f,%.0f,%.0f)",
                static_cast<double>(dist), f, static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z));
            logMess("geradeaus");
            c.note("freiheit: B (Absteigen)");
        }
        break;
    }
    case FreiPhase::Done:
        break;
    }
    c.stick(x, y);
    if (f % 300 == 0) {
        logBird("freiheit", f);
    }
    return false;
}

const kff::autotest::Scenario kScenFuss = {
    "fuss",
    "freiheit: F_SP121 Absteigen am Boden (B), 30 s zu Fuss (Acch-Freigabe, Link bleibt auf dem Boden)",
    fussTick,
};

const kff::autotest::Scenario kScenInnen = {
    "innen",
    "freiheit: Kanalisation R_SP107, steigen durch die Decke, Kurven, Absteigen",
    innenTick,
};

const kff::autotest::Scenario kScenGrenze = {
    "grenze",
    "freiheit: F_SP121 Geradeausflug nach Westen ueber Bergwand und Kartenrand",
    grenzeTick,
};

const kff::autotest::Scenario kScenSzene = {
    "szene",
    "freiheit: F_SP121 Vogel-Szenenwechsel im Ritt (abgewiesen)",
    szeneTick,
};

const kff::autotest::Scenario kScenFreiheit = {
    "freiheit",
    "freiheit: F_SP121 Frontalanflug Bergwand, Sturzflug auf den Boden, Steigen, Flug ueber den Kartenrand",
    freiheitTick,
};

// Testlauf "wand5" (KFF_AUTOTEST=wand5, nur Testbuild, Werkzeugkopie flugbahn): Anflug und Ablauf wie "freiheit",
// aber im Ritt Gang 5 (Steuerkreuz oben je Flanke bei Ritt-Frame 5 und 15, Gang 3 -> 5) und Turbo (Dash, Taste A)
// im Anflug: bei Ritt-Frame 30 und danach alle 120 Frames (Sperrzeit 85 Frames). Der Stick kommt unveraendert
// von freiheitTick (Anflug neutral). Context::tap ODER-verknuepft die Tasten, daher bleibt die Logik unberuehrt.
bool wand5Tick(Context& c) {
    if (c.sess.phase == kff::Phase::Riding) {
        const uint32_t f = c.sess.rideFrames;
        if (f == 5 || f == 15) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
            c.note("wand5: Gang hoch (Steuerkreuz oben)");
        }
        if (g_freiPhase == FreiPhase::Approach && f >= 30 && (f - 30) % 120 == 0) {
            c.tapA();
            c.note("wand5: Turbo (Dash, A)");
        }
    }
    return freiheitTick(c);
}

const kff::autotest::Scenario kScenWand5 = {
    "wand5",
    "freiheit (Werkzeugkopie): F_SP121 Frontalanflug Bergwand in Gang 5 mit Turbo (Dash), danach wie freiheit",
    wand5Tick,
};

// Testlauf "wand5s" (KFF_AUTOTEST=wand5s, nur Testbuild, Werkzeugkopie flugbahn): wie "wand5", aber 20 Frames nach
// dem Wandkontakt (Beginn der Gleitphase) Stick nach rechts (x = 0,5). Der Stick wird NACH freiheitTick gesetzt
// (letzter Schreibzugriff gewinnt). Prueft, ob der Vogel mit Stick aus einem frontalen Stillstand herauskommt.
bool wand5sTick(Context& c) {
    const bool done = wand5Tick(c);
    if (c.sess.phase == kff::Phase::Riding && g_freiPhase == FreiPhase::Glide) {
        const uint32_t f = c.sess.rideFrames;
        if (f >= g_freiPhaseFrame && f - g_freiPhaseFrame < 20) {
            c.stick(0.5f, 0.0f);
            c.note("wand5s: Stick rechts (Ausweichen, 20 Frames)");
        }
    }
    return done;
}

const kff::autotest::Scenario kScenWand5s = {
    "wand5s",
    "freiheit (Werkzeugkopie): wie wand5, nach dem Wandkontakt 20 Frames Stick rechts (Stillstand mit Stick)",
    wand5sTick,
};

// =============================================================================================
// WP1-Szenarien (nur Testbuild): Szenenwechsel, Ladezonen, Void, Absteigen, Rueckkehr-Raum (PLAN-v2.1 WP1.1 bis WP1.8).
// Eingaben nur ueber die Pad-Injektion des Kerns (Ruf, Stick, B, A). Test-Teleports nur fuer die Ausgangslage (wie e9):
// Vorpositionierung vor einem Ausgang, Verschieben ueber den Abgrund, Anheben ueber den Boden. Den Ausgang, den Abgrund und
// den Boden erreicht der Vogel danach selbst. Messzeilen "wp1: ...", Ergebnis "wp1: RESULT <szenario> ok|fail <gruende>".
// =============================================================================================
namespace wp1 {

constexpr uint32_t kHoldR = 30;          // R gehalten beim Ruf
constexpr uint32_t kPressB = 15;         // B-Tipp in der Mitte des R-Halts
constexpr uint32_t kStatusEvery = 30;    // Statuszeile alle 30 Ticks (1 s)
constexpr uint32_t kRetryCall = 300;     // Ruf ohne Ritt: nach so vielen Ticks erneut
constexpr uint32_t kDeadline = 9000;     // spaetestens dann endet jedes WP1-Szenario (5 min)
constexpr float kNoGround = -1.0e8f;

// Zustand, den onRideEnd fuer die Szenarien festhaelt (Grund des letzten Ritt-Endes).
bool g_endSeen = false;
kff::EndReason g_endWhy = kff::EndReason::DismountButton;

int countBirdCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e &&
        static_cast<daKago_c*>(ac)->mType == kff::kFriendlyKagoType) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

uint32_t birds() {
    uint32_t n = 0;
    fopAcIt_Executor(countBirdCb, &n);
    return n;
}

int layerOf(const void* p) {
    if (p == nullptr) {
        return -1;
    }
    const base_process_class* b = static_cast<const base_process_class*>(p);
    return b->layer_tag.layer != nullptr ? static_cast<int>(b->layer_tag.layer->layer_id) : -1;
}

const char* stageName() {
    const char* s = dComIfGp_getStartStageName();
    return s != nullptr ? s : "?";
}

// Oberste Flaeche an (x, z) unterhalb von yTop, kNoGround ohne Boden.
float groundAt(float x, float yTop, float z, int* exitId = nullptr, int* code = nullptr, int* arg = nullptr) {
    cXyz start(x, yTop, z);
    dBgS_GndChk gnd;
    gnd.SetPos(&start);
    const float g = dComIfG_Bgsp().GroundCross(&gnd);
    if (g == -G_CM3D_F_INF) {
        return kNoGround;
    }
    if (exitId != nullptr) {
        *exitId = static_cast<int>(dComIfG_Bgsp().GetExitId(gnd));
    }
    if (code != nullptr) {
        *code = dComIfG_Bgsp().GetGroundCode(gnd);
    }
    if (arg != nullptr) {
        *arg = static_cast<int>(dKy_pol_argument_get(&gnd));
    }
    return g;
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Stick X fuer Kurs auf (tx, tz). flug: Stick links (X < 0) = Gier +.
float steerTo(const daKago_c* k, float tx, float tz) {
    const s16 want = cM_atan2s(tx - k->current.pos.x, tz - k->current.pos.z);
    const s16 err = static_cast<s16>(want - k->current.angle.y);
    return clampf(-static_cast<float>(err) / 2048.0f, -1.0f, 1.0f);
}

// Stick Y fuer die Hoehe des Vogels (y > 0 = Nase runter). PD auf Hoehe und Steigrate.
float holdHeight(const daKago_c* k, float targetY, float& lastY) {
    const float err = k->current.pos.y - targetY;
    const float vy = k->current.pos.y - lastY;
    lastY = k->current.pos.y;
    const float y = err / 500.0f + vy / 25.0f;
    return std::fabs(y) < 0.05f ? 0.0f : clampf(y, -0.7f, 0.7f);
}

void placeBird(daKago_c* k, const cXyz& at, s16 yaw) {
    k->current.pos = at;
    k->old.pos = at;
    k->current.angle.y = yaw;
    k->shape_angle.y = yaw;
}

void status(kff::autotest::Context& c, const char* tag) {
    daAlink_c* link = kff::liveLink();
    daKago_c* k = kff::liveBird();
    const cXyz lp = link != nullptr ? link->current.pos : cXyz(0.0f, 0.0f, 0.0f);
    const cXyz bp = k != nullptr ? k->current.pos : cXyz(0.0f, 0.0f, 0.0f);
    kff::logInfo("wp1: ST %s t=%u ph=%s rf=%u birds=%u stage=%s room=%d life=%u ev=%d next=%d link=(%.1f,%.1f,%.1f) "
                 "proc=%d cargo=%d gndHit=%d code=%d bird=(%.1f,%.1f,%.1f) a6de=%d lay=%d/%d landF=%u",
        tag, c.frame, kff::phaseName(c.sess.phase), c.sess.rideFrames, birds(), stageName(),
        dComIfGp_roomControl_getStayNo(), static_cast<unsigned>(dComIfGs_getLife()), inEvent() ? 1 : 0,
        dComIfGp_isEnableNextStage() != 0 ? 1 : 0, static_cast<double>(lp.x), static_cast<double>(lp.y),
        static_cast<double>(lp.z), link != nullptr ? static_cast<int>(link->mProcID) : -1,
        link != nullptr && link->checkCargoCarry() ? 1 : 0, link != nullptr && link->mLinkAcch.ChkGroundHit() ? 1 : 0,
        link != nullptr ? link->mGroundCode : -9, static_cast<double>(bp.x), static_cast<double>(bp.y),
        static_cast<double>(bp.z), k != nullptr ? static_cast<int>(k->field_0x6de) : -1, layerOf(k), layerOf(link),
        c.sess.landFrames);
}

// Gemeinsamer Ablauf: Ruf, Ritt-Start, Ritt-Ende, Vogelzahl, Link-Wechsel, Statuszeilen.
struct Base {
    uint32_t callAt = 300;
    bool calling = false;
    uint32_t callStart = 0;
    uint32_t calls = 0;
    bool rode = false;
    uint32_t rideTick = 0;
    bool released = false;
    uint32_t releaseTick = 0;
    uint32_t releaseRideFrame = 0;
    kff::EndReason why = kff::EndReason::DismountButton;
    bool whyKnown = false;
    bool zero = false;
    uint32_t zeroTick = 0;
    uint32_t maxBirds = 0;
    uint32_t maxBirdsAfter = 0;
    fpc_ProcID linkId = kff::kNoProc;
    uint32_t linkChanges = 0;
    uint32_t firstLinkChange = 0;
    char stage0[16] = {};
    int room0 = -1;
    unsigned life0 = 0;
    int birdLayer0 = -2;
    uint32_t layerChanges = 0;
    uint32_t lastStatus = 0;
    uint32_t vetoAtRide = 0;
    uint32_t vetoAtEnd = 0;
    bool resultDone = false;
    bool stageSameAtEnd = true;     // Stage beim Ritt-Ende gleich der Stage beim Ritt-Start
    int roomAtEnd = -1;             // Raum beim Ritt-Ende
    uint32_t linkChangesAtEnd = 0;  // Link-Wechsel zwischen Ritt-Start und Ritt-Ende
};

uint32_t vetoTotal();

void track(kff::autotest::Context& c, Base& b, const char* tag) {
    const uint32_t n = birds();
    if (n > b.maxBirds) {
        b.maxBirds = n;
    }
    if (b.released && n > b.maxBirdsAfter) {
        b.maxBirdsAfter = n;
    }
    if (c.sess.linkId != kff::kNoProc) {
        if (b.linkId == kff::kNoProc) {
            b.linkId = c.sess.linkId;
        } else if (c.sess.linkId != b.linkId) {
            ++b.linkChanges;
            if (b.firstLinkChange == 0) {
                b.firstLinkChange = c.frame;
            }
            kff::logInfo("wp1: %s Link gewechselt t=%u (0x%x -> 0x%x) stage=%s room=%d", tag, c.frame,
                static_cast<unsigned>(b.linkId), static_cast<unsigned>(c.sess.linkId), stageName(),
                dComIfGp_roomControl_getStayNo());
            b.linkId = c.sess.linkId;
        }
    }
    if (!b.rode && !b.calling && c.frame >= b.callAt && c.canCall() &&
        (b.calls == 0 || c.frame >= b.callStart + kRetryCall)) {
        b.calling = true;
        b.callStart = c.frame;
        ++b.calls;
        kff::logInfo("wp1: %s Ruf %u bei t=%u (R halten, B tippen)", tag, b.calls, c.frame);
    }
    if (b.calling) {
        const uint32_t d = c.frame - b.callStart;
        if (d < kHoldR) {
            c.holdR();
        }
        if (d == kPressB) {
            c.tapB();
        }
        if (d >= kHoldR) {
            b.calling = false;
        }
    }
    if (!b.rode && c.sess.phase == kff::Phase::Riding) {
        b.rode = true;
        b.rideTick = c.frame;
        std::snprintf(b.stage0, sizeof(b.stage0), "%s", stageName());
        b.room0 = dComIfGp_roomControl_getStayNo();
        b.life0 = dComIfGs_getLife();
        b.birdLayer0 = layerOf(kff::liveBird());
        b.vetoAtRide = vetoTotal();
        g_endSeen = false;
        // Link-Wechsel vor dem Ritt (Stage-Start mit Ereignis, z. B. R_SP107) zaehlen nicht.
        b.linkChanges = 0;
        b.firstLinkChange = 0;
        b.linkId = c.sess.linkId;
        daAlink_c* link = kff::liveLink();
        kff::logInfo("wp1: %s Ritt-Start t=%u stage=%s room=%d life=%u birdLayer=%d linkLayer=%d", tag, c.frame, b.stage0,
            b.room0, b.life0, b.birdLayer0, layerOf(link));
    }
    if (b.rode && !b.released && c.sess.phase == kff::Phase::Riding) {
        const int lay = layerOf(kff::liveBird());
        if (lay != b.birdLayer0 && lay != -1) {
            ++b.layerChanges;
            kff::logInfo("wp1: %s Vogel-Layer gewechselt %d -> %d t=%u", tag, b.birdLayer0, lay, c.frame);
            b.birdLayer0 = lay;
        }
        b.releaseRideFrame = c.sess.rideFrames;
    }
    if (b.rode && !b.released && c.sess.phase != kff::Phase::Riding) {
        b.released = true;
        b.releaseTick = c.frame;
        b.whyKnown = g_endSeen;
        b.why = g_endWhy;
        b.vetoAtEnd = vetoTotal();
        b.stageSameAtEnd = std::strncmp(b.stage0, stageName(), sizeof(b.stage0)) == 0;
        b.roomAtEnd = dComIfGp_roomControl_getStayNo();
        b.linkChangesAtEnd = b.linkChanges;
        daAlink_c* link = kff::liveLink();
        kff::logInfo("wp1: %s Ritt-Ende t=%u (Ritt-Frame %u) phase=%s grund=%s birds=%u link=(%.1f,%.1f,%.1f) life=%u "
                     "veto=%u", tag, c.frame, b.releaseRideFrame, kff::phaseName(c.sess.phase),
            b.whyKnown ? kff::endReasonName(b.why) : "?", n, link ? static_cast<double>(link->current.pos.x) : 0.0,
            link ? static_cast<double>(link->current.pos.y) : 0.0, link ? static_cast<double>(link->current.pos.z) : 0.0,
            static_cast<unsigned>(dComIfGs_getLife()), b.vetoAtEnd - b.vetoAtRide);
    }
    if (b.released && !b.zero && n == 0) {
        b.zero = true;
        b.zeroTick = c.frame;
        kff::logInfo("wp1: %s birds=0 t=%u (%u Ticks nach dem Ritt-Ende)", tag, c.frame, c.frame - b.releaseTick);
    }
    if (c.frame >= b.lastStatus + kStatusEvery) {
        b.lastStatus = c.frame;
        status(c, tag);
    }
}

// Ereignis-Texte und Game-Over-Auswahl mit A weiterschalten (nur wo das Szenario es verlangt).
void tapAInEvent(kff::autotest::Context& c) {
    if (inEvent() && c.frame % 20 == 0) {
        c.tapA();
    }
}

bool sameStage(const Base& b) {
    return std::strncmp(b.stage0, stageName(), sizeof(b.stage0)) == 0;
}

// Leeres Gebiet (kein Boden) ab from in Richtung +X, -X, +Z, -Z (Abstand dist, 1x, 1,5x, 2x). Rueckgabe: Punkt.
bool findVoid(const cXyz& from, float dist, cXyz* out) {
    const float dirs[4][2] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}};
    const float mults[3] = {1.0f, 1.5f, 2.0f};
    for (const auto& d : dirs) {
        for (float m : mults) {
            const float x = from.x + d[0] * dist * m;
            const float z = from.z + d[1] * dist * m;
            bool empty = true;
            for (int i = -1; i <= 1 && empty; ++i) {
                for (int j = -1; j <= 1 && empty; ++j) {
                    if (groundAt(x + 800.0f * i, 60000.0f, z + 800.0f * j) != kNoGround) {
                        empty = false;
                    }
                }
            }
            if (empty) {
                *out = cXyz(x, from.y, z);
                return true;
            }
        }
    }
    return false;
}

// ------------------------------------------------------------------------- Ausgaenge (SCENE_EXIT)
struct Exit {
    fpc_ProcID id = kff::kNoProc;
    cXyz pos;
    s16 yaw = 0;
    float hx = 0.0f;
    float hy = 0.0f;
    float hz = 0.0f;
    int exitId = 0;
    int type = 0;
    int sw = 0;
    bool active = false;
};
constexpr int kMaxExits = 64;
Exit g_exits[kMaxExits];
int g_nExits = 0;
int g_nExits2 = 0;
int g_nRetRm = 0;
int g_nKagoFall = 0;

int sweepCb(void* actor, void*) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac == nullptr) {
        return 1;
    }
    const s16 name = fopAcM_GetName(ac);
    if (name == fpcNm_SCENE_EXIT_e && g_nExits < kMaxExits) {
        daScex_c* sx = static_cast<daScex_c*>(ac);
        Exit& e = g_exits[g_nExits++];
        e.id = fopAcM_GetID(ac);
        e.pos = ac->current.pos;
        e.yaw = ac->shape_angle.y;
        e.hx = ac->scale.x;
        e.hy = ac->scale.y;
        e.hz = ac->scale.z;
        e.exitId = sx->getArg0();
        e.type = sx->getArg1();
        e.sw = sx->getSwNo();
        e.active = sx->checkWork() != 0;
    } else if (name == fpcNm_SCENE_EXIT2_e) {
        ++g_nExits2;
        kff::logInfo("wp1: SWEEP SCENE_EXIT2 pos=(%.0f,%.0f,%.0f) param=0x%08x scale=(%.1f,%.1f,%.1f)",
            static_cast<double>(ac->current.pos.x), static_cast<double>(ac->current.pos.y),
            static_cast<double>(ac->current.pos.z), static_cast<unsigned>(fopAcM_GetParam(ac)),
            static_cast<double>(ac->scale.x), static_cast<double>(ac->scale.y), static_cast<double>(ac->scale.z));
    } else if (name == fpcNm_Tag_RetRoom_e) {
        ++g_nRetRm;
        kff::logInfo("wp1: SWEEP Tag_RetRoom pos=(%.0f,%.0f,%.0f) scale=(%.1f,%.1f,%.1f) yaw=%d",
            static_cast<double>(ac->current.pos.x), static_cast<double>(ac->current.pos.y),
            static_cast<double>(ac->current.pos.z), static_cast<double>(ac->scale.x), static_cast<double>(ac->scale.y),
            static_cast<double>(ac->scale.z), static_cast<int>(ac->current.angle.y));
    } else if (name == fpcNm_Tag_KagoFall_e) {
        ++g_nKagoFall;
    }
    return 1;
}

void sweepExits(const cXyz& ref) {
    g_nExits = 0;
    g_nExits2 = 0;
    g_nRetRm = 0;
    g_nKagoFall = 0;
    fopAcIt_Executor(sweepCb, nullptr);
    for (int i = 0; i < g_nExits; ++i) {
        const Exit& e = g_exits[i];
        const float dx = e.pos.x - ref.x;
        const float dz = e.pos.z - ref.z;
        kff::logInfo("wp1: SWEEP EXIT %d id=0x%x pos=(%.0f,%.0f,%.0f) yaw=%d halb=(%.0f,%.0f) hoehe=%.0f ausgang=%d typ=%d "
                     "sw=%d aktiv=%d abstand=%.0f",
            i, static_cast<unsigned>(e.id), static_cast<double>(e.pos.x), static_cast<double>(e.pos.y),
            static_cast<double>(e.pos.z), static_cast<int>(e.yaw), static_cast<double>(e.hx), static_cast<double>(e.hz),
            static_cast<double>(e.hy), e.exitId, e.type, e.sw, e.active ? 1 : 0,
            static_cast<double>(std::sqrt(dx * dx + dz * dz)));
    }
    kff::logInfo("wp1: SWEEP Summe SCENE_EXIT=%d SCENE_EXIT2=%d Tag_RetRoom=%d TagKagoFall=%d stage=%s room=%d", g_nExits,
        g_nExits2, g_nRetRm, g_nKagoFall, stageName(), dComIfGp_roomControl_getStayNo());
}

// Ortskoordinaten von p im Ausgangsquader (wie daScex_c::execute). Abstand zum Quader, 0 innen.
float exitDistance(const Exit& e, const cXyz& p, bool* inside) {
    const float dx = p.x - e.pos.x;
    const float dz = p.z - e.pos.z;
    const float s = cM_ssin(e.yaw);
    const float co = cM_scos(e.yaw);
    // Inverse Drehung um Y (mDoMtx_inverse von YrotM).
    const float lx = co * dx - s * dz;
    const float lz = s * dx + co * dz;
    const float ly = p.y - e.pos.y;
    const float ox = std::fabs(lx) > e.hx ? std::fabs(lx) - e.hx : 0.0f;
    const float oz = std::fabs(lz) > e.hz ? std::fabs(lz) - e.hz : 0.0f;
    const float oy = ly < 0.0f ? -ly : (ly > e.hy ? ly - e.hy : 0.0f);
    if (inside != nullptr) {
        *inside = ox == 0.0f && oy == 0.0f && oz == 0.0f;
    }
    return std::sqrt(ox * ox + oy * oy + oz * oz);
}

bool exitJumpType(const Exit& e) {
    return e.type == 0 || e.type == 2 || e.type == 3 || e.type == 4;
}

// Bodensuche nach Ausgangs-Polygonen (Exit-ID ungleich 0x3F) und Codes 4, 5, 9, 10 auf einem Raster um ref.
void sweepGround(const cXyz& ref, float radius, float step) {
    int found = 0;
    int samples = 0;
    for (float x = -radius; x <= radius && found < 24; x += step) {
        for (float z = -radius; z <= radius && found < 24; z += step) {
            int ex = 0x3F;
            int code = 0;
            int arg = 0;
            const float g = groundAt(ref.x + x, 60000.0f, ref.z + z, &ex, &code, &arg);
            ++samples;
            if (g == kNoGround) {
                continue;
            }
            if (ex != 0x3F || code == 4 || code == 5 || code == 9 || code == 10 || (arg & 0x80) != 0) {
                ++found;
                kff::logInfo("wp1: SWEEP BODEN (%.0f,%.1f,%.0f) exitId=%d code=%d arg=0x%02x", static_cast<double>(ref.x + x),
                    static_cast<double>(g), static_cast<double>(ref.z + z), ex, code, arg);
            }
        }
    }
    kff::logInfo("wp1: SWEEP BODEN Summe %d Treffer in %d Proben (Radius %.0f, Schritt %.0f)", found, samples,
        static_cast<double>(radius), static_cast<double>(step));
}

// ------------------------------------------------------------------------- WP1.1 ladezone-anflug
struct Ladezone {
    Base b;
    bool swept = false;
    int target = -1;
    bool placed = false;
    float targetLinkY = 0.0f;
    float linkDy = -150.0f;
    float lastY = 0.0f;
    bool inside = false;
    uint32_t insideTicks = 0;
    uint32_t entries = 0;
    uint32_t firstInside = 0;
    float dist900 = -1.0f;
    float dist900c = -1.0f;
    bool bDone = false;
    uint32_t bTick = 0;
    cXyz walkLast;
    uint32_t walkWindows = 0;
    uint32_t walkStuck = 0;
    uint32_t walkStart = 0;
    uint32_t walkSkipped = 0;   // Fenster mit Ereignis oder Link-Wechsel (nicht gewertet)
    uint32_t eventTicks = 0;    // Ticks der Probe im Ereignis
    uint32_t winStart = 0;
    bool winEvent = false;
    fpc_ProcID winLink = kff::kNoProc;
    cXyz walkCheck;
    uint32_t walkCheckTick = 0;
    float walkAng = 3.1416f;    // zuerst Stick nach hinten
    uint32_t turns = 0;
    uint32_t endAt = 0;
};
Ladezone g_lz;

uint32_t kLzPlaceAt = 860;   // Vorpositionierung, Einflug um Tick 900
uint32_t kLzAwayAt = 1380;   // weg vom Ausgang
uint32_t kLzBAt = 1500;      // B

bool ladezoneTick(kff::autotest::Context& c) {
    Ladezone& s = g_lz;
    Base& b = s.b;
    b.callAt = 300;
    track(c, b, "ladezone");
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT ladezone-anflug fail Zeitlimit (rode=%d released=%d)", b.rode ? 1 : 0, b.released ? 1 : 0);
        return true;
    }
    daKago_c* k = kff::liveBird();
    daAlink_c* link = kff::liveLink();
    if (b.rode && !b.released && k != nullptr && link != nullptr) {
        if (!s.swept) {
            s.swept = true;
            sweepExits(link->current.pos);
            sweepGround(link->current.pos, 16000.0f, 1000.0f);
            s.linkDy = link->current.pos.y - k->current.pos.y;
            // Ziel: aktiver Sprung-Ausgang (loest im Flug aus), sonst aktiver Lauf-Ausgang; jeweils der naechste.
            float best = 1.0e30f;
            for (int pass = 0; pass < 2 && s.target < 0; ++pass) {
                for (int i = 0; i < g_nExits; ++i) {
                    const Exit& e = g_exits[i];
                    if (!e.active || (pass == 0 && !exitJumpType(e))) {
                        continue;
                    }
                    const float dx = e.pos.x - link->current.pos.x;
                    const float dz = e.pos.z - link->current.pos.z;
                    const float d2 = dx * dx + dz * dz;
                    if (d2 < best) {
                        best = d2;
                        s.target = i;
                    }
                }
            }
            if (s.target < 0) {
                kff::logInfo("wp1: RESULT ladezone-anflug fail kein aktiver Ausgang (SCENE_EXIT) geladen");
                return true;
            }
            const Exit& e = g_exits[s.target];
            s.targetLinkY = e.pos.y + clampf(e.hy * 0.5f, 40.0f, 400.0f);
            kff::logInfo("wp1: ladezone Ziel EXIT %d ausgang=%d typ=%d (%s) pos=(%.0f,%.0f,%.0f) Link-Zielhoehe %.0f, "
                         "Versatz Link-Vogel %.1f",
                s.target, e.exitId, e.type, exitJumpType(e) ? "Sprung, loest im Flug aus" : "Lauf, braucht Bodenkontakt",
                static_cast<double>(e.pos.x), static_cast<double>(e.pos.y), static_cast<double>(e.pos.z),
                static_cast<double>(s.targetLinkY), static_cast<double>(s.linkDy));
        }
        const Exit& e = g_exits[s.target];
        if (!s.placed && c.frame >= kLzPlaceAt) {
            s.placed = true;
            float dx = link->current.pos.x - e.pos.x;
            float dz = link->current.pos.z - e.pos.z;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len > 1.0f) {
                dx /= len;
                dz /= len;
            } else {
                dx = 0.0f;
                dz = 1.0f;
            }
            const float farDist = 1000.0f + std::max(e.hx, e.hz);
            const cXyz at(e.pos.x + dx * farDist, s.targetLinkY - s.linkDy, e.pos.z + dz * farDist);
            placeBird(k, at, cM_atan2s(e.pos.x - at.x, e.pos.z - at.z));
            s.lastY = at.y;
            kff::logInfo("wp1: ladezone Vorpositionierung (Test-Teleport) t=%u Vogel=(%.0f,%.0f,%.0f), %.0f vor dem Ausgang",
                c.frame, static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
                static_cast<double>(farDist));
        }
        bool in = false;
        const float dist = exitDistance(e, link->current.pos, &in);
        if (in && !s.inside) {
            ++s.entries;
            if (s.firstInside == 0) {
                s.firstInside = c.frame;
            }
            kff::logInfo("wp1: ladezone Link IM Ausgang t=%u (Eintritt %u) link=(%.0f,%.0f,%.0f) ausgangsId=0x%x", c.frame,
                s.entries, static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                static_cast<double>(link->current.pos.z), static_cast<unsigned>(link->mExitID));
        } else if (!in && s.inside) {
            kff::logInfo("wp1: ladezone Link verlaesst den Ausgang t=%u", c.frame);
        }
        s.inside = in;
        if (in) {
            ++s.insideTicks;
        }
        if (c.frame == 900) {
            const float cx = link->current.pos.x - e.pos.x;
            const float cz = link->current.pos.z - e.pos.z;
            s.dist900 = dist;
            s.dist900c = std::sqrt(cx * cx + cz * cz);
            kff::logInfo("wp1: ladezone Abstand bei Tick 900: Quader %.1f, Mitte (eben) %.1f", static_cast<double>(dist),
                static_cast<double>(s.dist900c));
        }
        if (s.placed && !s.bDone) {
            float sx = 0.0f;
            float sy = 0.0f;
            if (c.frame < kLzAwayAt) {
                sx = steerTo(k, e.pos.x, e.pos.z);
                sy = holdHeight(k, s.targetLinkY - s.linkDy, s.lastY);
            } else {
                const float ax = k->current.pos.x + (k->current.pos.x - e.pos.x);
                const float az = k->current.pos.z + (k->current.pos.z - e.pos.z);
                sx = steerTo(k, ax, az);
                sy = holdHeight(k, s.targetLinkY - s.linkDy + 300.0f, s.lastY);
            }
            c.stick(sx, sy);
        }
        if (!s.bDone && c.frame >= kLzBAt) {
            s.bDone = true;
            s.bTick = c.frame;
            c.tapB();
            kff::logInfo("wp1: ladezone B t=%u Abstand zum Ausgang %.0f", c.frame, static_cast<double>(dist));
        }
    }
    if (b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + 900;
            s.walkStart = b.releaseTick + 150;
        }
        if (c.frame >= s.walkStart && c.frame < s.endAt) {
            // Bewegungsprobe (Softlock-Nachweis): zu Fuss laufen, an Waenden die Richtung um 135 Grad drehen. Gemessen
            // wird je 60 Ticks, nur Fenster ganz ohne Ereignis und ohne Link-Wechsel (ein Szenenwechsel nach dem Ende ist
            // Vanilla und zaehlt nicht als Stillstand). Ein Dauer-Ereignis (Softlock wie b-ladezone1) ergibt 0 Fenster.
            const bool ev = link == nullptr || inEvent();
            if (ev) {
                ++s.eventTicks;
                s.winEvent = true;
            } else {
                if (s.walkCheckTick == 0 || c.frame - s.walkCheckTick >= 20) {
                    const float cx = link->current.pos.x - s.walkCheck.x;
                    const float cz = link->current.pos.z - s.walkCheck.z;
                    if (s.walkCheckTick != 0 && cx * cx + cz * cz < 100.0f) {
                        s.walkAng += 2.356f;
                        ++s.turns;
                    }
                    s.walkCheck = link->current.pos;
                    s.walkCheckTick = c.frame;
                }
                c.stick(std::sin(s.walkAng), std::cos(s.walkAng));
            }
            const fpc_ProcID lid = link != nullptr ? fopAcM_GetID(link) : kff::kNoProc;
            if (s.winStart == 0 || c.frame - s.winStart >= 60) {
                if (s.winStart != 0 && !s.winEvent && !ev && lid == s.winLink) {
                    const float mx = link->current.pos.x - s.walkLast.x;
                    const float mz = link->current.pos.z - s.walkLast.z;
                    const float m = std::sqrt(mx * mx + mz * mz);
                    ++s.walkWindows;
                    if (m < 1.0f) {
                        ++s.walkStuck;
                    }
                    kff::logInfo("wp1: ladezone Fuss t=%u Weg in 60 Ticks %.1f link=(%.0f,%.0f,%.0f) proc=%d stage=%s "
                                 "Drehungen=%u", c.frame, static_cast<double>(m), static_cast<double>(link->current.pos.x),
                        static_cast<double>(link->current.pos.y), static_cast<double>(link->current.pos.z),
                        static_cast<int>(link->mProcID), stageName(), s.turns);
                } else if (s.winStart != 0) {
                    ++s.walkSkipped;
                }
                s.winStart = c.frame;
                s.winEvent = ev;
                s.winLink = lid;
                if (link != nullptr) {
                    s.walkLast = link->current.pos;
                }
            }
        }
        if (c.frame >= s.endAt) {
            const uint32_t vetos = b.vetoAtEnd - b.vetoAtRide;
            const bool stageOk = b.stageSameAtEnd && b.linkChangesAtEnd == 0;  // Stage im Ritt unveraendert
            const bool endOk = s.bDone && b.releaseTick >= s.bTick && b.releaseTick - s.bTick <= 200 &&
                               b.whyKnown && b.why == kff::EndReason::DismountButton;
            const bool birdsOk = birds() == 0;
            const bool walkOk = s.walkWindows >= 5 && s.walkStuck == 0;
            const bool distOk = s.dist900 >= 0.0f && s.dist900 < 300.0f;
            const bool ok = stageOk && vetos >= 1 && endOk && birdsOk && walkOk && s.entries > 0;
            kff::logInfo("wp1: RESULT ladezone-anflug %s stageImRitt=%s(%s) stageJetzt=%s linkWechselImRitt=%u linkWechsel=%u "
                         "veto=%u erlaubtNachEnde=%u eintritte=%u imAusgang=%u Ticks abstand900=%.1f(%s) ende=%s nach %d Ticks "
                         "birds=%u fuss=%u/%u Fenster ohne Bewegung (nicht gewertet %u, Ereignis-Ticks %u, Drehungen %u) typ=%d",
                ok ? "ok" : "fail", b.stage0, stageOk ? "gleich" : "GEWECHSELT", stageName(), b.linkChangesAtEnd,
                b.linkChanges, vetos, g_allowLines, s.entries, s.insideTicks, static_cast<double>(s.dist900),
                distOk ? "<300" : ">=300", b.whyKnown ? kff::endReasonName(b.why) : "?",
                static_cast<int>(b.releaseTick - s.bTick), birds(), s.walkStuck, s.walkWindows, s.walkSkipped, s.eventTicks,
                s.turns, s.target >= 0 ? g_exits[s.target].type : -1);
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------- WP1.2 event39-kanal
struct Event39 {
    Base b;
    float lastY = 0.0f;
    uint32_t endAt = 0;
};
Event39 g_ev;

bool event39Tick(kff::autotest::Context& c) {
    Event39& s = g_ev;
    Base& b = s.b;
    b.callAt = 60;
    track(c, b, "event39");
    tapAInEvent(c);
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT event39-kanal fail Zeitlimit (rode=%d released=%d)", b.rode ? 1 : 0, b.released ? 1 : 0);
        return true;
    }
    if (b.rode && !b.released) {
        const uint32_t f = c.sess.rideFrames;
        c.stick(0.0f, f < 240 ? -0.6f : 0.0f);  // wie innen: steigen
        if (f >= 1500) {
            c.tapB();  // ohne Ereignis nach 1500 Ritt-Frames absteigen
        }
    }
    if (b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + 1500;
        }
        if (c.frame >= s.endAt) {
            const bool whyOk = b.whyKnown && b.why == kff::EndReason::Event;
            const bool orderOk = b.firstLinkChange == 0 || b.firstLinkChange > b.releaseTick;
            const bool birdsOk = b.maxBirds <= 1;
            const bool zeroOk = b.zero && b.zeroTick - b.releaseTick <= 100;
            const int fr = static_cast<int>(b.releaseRideFrame);
            const bool frameOk = fr >= 191 && fr <= 231;
            const bool ok = whyOk && orderOk && birdsOk && zeroOk;
            kff::logInfo("wp1: RESULT event39-kanal %s grund=%s ritt-frame=%d(%s) ende t=%u linkWechsel t=%u (%s) maxBirds=%u "
                         "birds0 nach %d Ticks stage=%s",
                ok ? "ok" : "fail", b.whyKnown ? kff::endReasonName(b.why) : "?", fr, frameOk ? "211+-20" : "ausserhalb",
                b.releaseTick, b.firstLinkChange, orderOk ? "danach" : "DAVOR", b.maxBirds,
                b.zero ? static_cast<int>(b.zeroTick - b.releaseTick) : -1, stageName());
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------- Absteigen: Boden, Void, Abgrund, Hoehe, 0x60
enum class Drop : uint8_t { Boden, Void, Abgrund12, Abgrund4, Boden60, Hoch };

struct Absteigen {
    Base b;
    float lastY = 0.0f;
    bool prepared = false;
    uint32_t prepTick = 0;
    bool bDone = false;
    uint32_t bTick = 0;
    unsigned lifeAtB = 0;
    uint32_t approaches = 0;
    uint32_t prevLand = 0;
    uint32_t maxLand = 0;
    float releaseLinkY = 0.0f;
    bool touched = false;
    uint32_t touchTick = 0;
    float touchY = 0.0f;
    int last6de = -1;
    uint32_t jumps6de = 0;
    uint32_t endAt = 0;
    unsigned lifeMin = 999;
    bool gameOver = false;
    uint32_t gameOverTick = 0;  // Z1 (b): Tick des ersten Game-Over-Zustands (PROC_DEAD oder FORCE_GAMEOVER)
    uint32_t restartLink = 0;
    int target60 = 0;
    cXyz spot;
    cXyz relPos;            // Link-Position im ersten Tick nach der Freigabe (E9: Horizontalbewegung danach)
    float relMove1 = -1.0f; // horizontale Strecke im ersten ganzen Tick nach der Freigabe
    bool lifeLandSet = false;
    unsigned lifeLand = 0;  // Leben 90 Ticks nach der Bodenberuehrung (Sturzschaden; spaeterer Gegnerschaden zaehlt nicht)
    bool fallDamage = false;   // Sturzschaden-Proc (PROC_WOLF_LAND_DAMAGE, PROC_LAND_DAMAGE) nach der Freigabe
    uint32_t otherDamage = 0;  // andere Schadens-Procs nach der Freigabe (Bodenpolygon 0x60/Eis, Gegner)
    int lastProc = -1;
};
Absteigen g_ab;

const char* dropName(Drop d) {
    switch (d) {
    case Drop::Boden: return "boden-absteigen";
    case Drop::Void: return "void-absteigen";
    case Drop::Abgrund12: return "abgrund-12";
    case Drop::Abgrund4: return "abgrund-4";
    case Drop::Boden60: return "boden-0x60";
    case Drop::Hoch: return "landung-hoch";
    }
    return "?";
}

float voidDist() {
    const char* e = std::getenv("KFS_VOID_DIST");
    const float v = e != nullptr ? static_cast<float>(std::atof(e)) : 0.0f;
    return v > 1000.0f ? v : 30000.0f;
}

// Bodenpunkt mit Polygon-Attribut 0x60 (Schaden, daAlink_c::checkPolyDamage) in der Naehe: Raster um ref. f-abgrund-c
// fand mit Schritt 400 nichts, f-abgrund-d bekam am Landepunkt (-50138,-2975,-33549) Eis-Bodenschaden: Schritt 200.
bool find60(const cXyz& ref, cXyz* out) {
    float best = 1.0e30f;
    bool found = false;
    for (float x = -10000.0f; x <= 10000.0f; x += 200.0f) {
        for (float z = -10000.0f; z <= 10000.0f; z += 200.0f) {
            int arg = 0;
            const float g = groundAt(ref.x + x, 60000.0f, ref.z + z, nullptr, nullptr, &arg);
            if (g == kNoGround || (arg & 0x60) == 0) {
                continue;
            }
            const float d = x * x + z * z;
            if (d < best) {
                best = d;
                *out = cXyz(ref.x + x, g, ref.z + z);
                found = true;
            }
        }
    }
    return found;
}

bool absteigenTick(kff::autotest::Context& c, Drop d) {
    Absteigen& s = g_ab;
    Base& b = s.b;
    const bool voidCase = d == Drop::Void || d == Drop::Abgrund12 || d == Drop::Abgrund4;
    // Kontrolle ohne Ritt (KFS_OHNE_RITT=1, nur Void und Abgrund): Link zu Fuss an dieselbe Stelle ueber dem Abgrund.
    static const bool ohneEnv = [] {
        const char* e = std::getenv("KFS_OHNE_RITT");
        return e != nullptr && e[0] == '1';
    }();
    const bool ohne = ohneEnv && voidCase;
    b.callAt = ohne ? 0xFFFFFFFFu : 300;
    static char ohneName[32] = {};
    if (ohne && ohneName[0] == '\0') {
        std::snprintf(ohneName, sizeof(ohneName), "%s-ohne", dropName(d));
    }
    const char* name = ohne ? ohneName : dropName(d);
    track(c, b, name);
    if (voidCase && (b.released || (ohne && s.prepared))) {
        tapAInEvent(c);  // Game-Over-Auswahl und Neustart-Texte
    }
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT %s fail Zeitlimit (rode=%d released=%d)", name, b.rode ? 1 : 0, b.released ? 1 : 0);
        return true;
    }
    daKago_c* k = kff::liveBird();
    daAlink_c* link = kff::liveLink();
    // Landeanflug zaehlen (Kern: Session.landFrames 0 -> 1).
    if (c.sess.landFrames > 0 && s.prevLand == 0) {
        ++s.approaches;
        kff::logInfo("wp1: %s Landeanflug beginnt t=%u (Nr. %u)", name, c.frame, s.approaches);
    }
    if (c.sess.landFrames > s.maxLand) {
        s.maxLand = c.sess.landFrames;
    }
    s.prevLand = c.sess.landFrames;
    if (k != nullptr && k->mType == kff::kFriendlyKagoType) {
        const int a = static_cast<int>(k->field_0x6de);
        if (s.last6de == 0 && a == 255) {
            ++s.jumps6de;
            kff::logInfo("wp1: %s field_0x6de springt 0 -> 255 t=%u", name, c.frame);
        }
        s.last6de = a;
    } else {
        s.last6de = -1;
    }
    if (link != nullptr) {
        const unsigned life = dComIfGs_getLife();
        if (life < s.lifeMin) {
            s.lifeMin = life;
        }
        if (link->mProcID == daAlink_c::PROC_DEAD || link->checkNoResetFlg2(daPy_py_c::FLG2_FORCE_GAMEOVER)) {
            if (!s.gameOver) {
                kff::logInfo("wp1: %s Link tot oder Game Over t=%u life=%u", name, c.frame, life);
                s.gameOverTick = c.frame;
            }
            s.gameOver = true;
        }
    }
    if (ohne) {
        // Kontrolle ohne Ritt: zur Zeit des Verschiebens im Ritt (Ruf 300, Ritt-Start ~318, 150 Ticks steigen) Link zu Fuss
        // 1500 ueber seiner Hoehe an die Abgrundstelle (Test-Teleport wie beim Vogel), gleiches Leben. Zeigt, ob der Void-Out
        // ohne Ritt genauso ausgeht (OE1: Void-Out ist Vanilla; checkRestartDead: Schaden 4 >= Leben 4 ergibt Game Over).
        if (!s.prepared && c.frame >= 470 && link != nullptr && !inEvent()) {
            s.prepared = true;
            s.prepTick = c.frame;
            cXyz to;
            if (!findVoid(link->current.pos, voidDist(), &to)) {
                kff::logInfo("wp1: RESULT %s fail kein Abgrund im Abstand %.0f", name, static_cast<double>(voidDist()));
                return true;
            }
            to.y = link->current.pos.y + 1500.0f;
            if (d == Drop::Abgrund12) {
                dComIfGs_setLife(12);
            } else if (d == Drop::Abgrund4) {
                dComIfGs_setLife(4);
            }
            s.lifeAtB = dComIfGs_getLife();
            const cXyz from = link->current.pos;
            link->current.pos = to;
            link->old.pos = to;
            // Link-Wechsel vor dem Versetzen zaehlen nicht (wie beim Ritt-Start).
            b.linkChanges = 0;
            b.firstLinkChange = 0;
            b.linkId = c.sess.linkId;
            kff::logInfo("wp1: %s Link verschoben (Test-Teleport, Kontrolle ohne Ritt) t=%u von (%.0f,%.0f,%.0f) nach "
                         "(%.0f,%.0f,%.0f), Boden darunter: %s, life=%u",
                name, c.frame, static_cast<double>(from.x), static_cast<double>(from.y), static_cast<double>(from.z),
                static_cast<double>(to.x), static_cast<double>(to.y), static_cast<double>(to.z),
                groundAt(to.x, 60000.0f, to.z) == kNoGround ? "keiner" : "ja", s.lifeAtB);
        }
        if (s.prepared) {
            if (b.firstLinkChange != 0 && s.restartLink == 0) {
                s.restartLink = b.firstLinkChange;
            }
            if (c.frame >= s.prepTick + 1200) {
                const bool fell = s.restartLink != 0 || s.gameOver;
                kff::logInfo("wp1: RESULT %s %s Kontrolle ohne Ritt: life vorher=%u min=%u jetzt=%u gameOver=%d "
                             "neustart(Link-Wechsel) t=%u (%d Ticks nach dem Versetzen) stage=%s birds=%u",
                    name, fell ? "ok" : "fail", s.lifeAtB, s.lifeMin, static_cast<unsigned>(dComIfGs_getLife()),
                    s.gameOver ? 1 : 0, s.restartLink,
                    s.restartLink != 0 ? static_cast<int>(s.restartLink - s.prepTick) : -1, stageName(), birds());
                return true;
            }
        }
        return false;
    }
    if (b.rode && !b.released && k != nullptr && link != nullptr) {
        const uint32_t f = c.sess.rideFrames;
        float sx = 0.0f;
        float sy = 0.0f;
        switch (d) {
        case Drop::Boden:
            // 421 Ritt-Frames geradeaus (Stick neutral), dann B ueber Boden. Basislauf b-boden1 und f-boden1/2: geradeaus
            // liegt bei Frame 421 kein Boden unter dem Vogel (Schlucht mit Wasser, Link schwimmt). Daher merkt sich das
            // Szenario die Stelle mit Boden beim Ritt-Start (unter Link, offenes Feld) und setzt den Vogel bei Frame 400
            // dorthin zurueck (Test-Teleport, 1200 ueber dem Boden), falls unter ihm oder 1500 voraus kein Boden liegt.
            if (f <= 5 && s.target60 == 0) {
                const float g = groundAt(link->current.pos.x, link->current.pos.y + 500.0f, link->current.pos.z);
                if (g != kNoGround) {
                    s.spot = cXyz(link->current.pos.x, g, link->current.pos.z);
                    s.target60 = 1;
                }
            }
            if (f == 400 && !s.prepared) {
                s.prepared = true;
                const float ax = k->current.pos.x + cM_ssin(k->current.angle.y) * 1500.0f;
                const float az = k->current.pos.z + cM_scos(k->current.angle.y) * 1500.0f;
                const float g = groundAt(k->current.pos.x, k->current.pos.y + 500.0f, k->current.pos.z);
                const float ga = groundAt(ax, k->current.pos.y + 500.0f, az);
                kff::logInfo("wp1: %s Boden bei Frame 400 unter dem Vogel %.1f, 1500 voraus %.1f, Startstelle %s", name,
                    static_cast<double>(g == kNoGround ? -1.0e9f : g), static_cast<double>(ga == kNoGround ? -1.0e9f : ga),
                    s.target60 == 1 ? "gemerkt" : "fehlt");
                if ((g == kNoGround || ga == kNoGround) && s.target60 == 1) {
                    const cXyz to(s.spot.x, s.spot.y + 1200.0f, s.spot.z);
                    placeBird(k, to, k->current.angle.y);
                    kff::logInfo("wp1: %s Vogel ueber Boden zurueckgesetzt (Test-Teleport) t=%u nach (%.0f,%.0f,%.0f), Boden %.1f",
                        name, c.frame, static_cast<double>(to.x), static_cast<double>(to.y), static_cast<double>(to.z),
                        static_cast<double>(s.spot.y));
                }
            }
            if (!s.bDone && f >= 421) {
                s.bDone = true;
            }
            break;
        case Drop::Void:
        case Drop::Abgrund12:
        case Drop::Abgrund4:
            if (f < 150) {
                sy = -0.6f;  // 150 Ticks steigen
            } else if (!s.prepared) {
                s.prepared = true;
                s.prepTick = c.frame;
                cXyz to;
                if (!findVoid(k->current.pos, voidDist(), &to)) {
                    kff::logInfo("wp1: RESULT %s fail kein Abgrund im Abstand %.0f", name, static_cast<double>(voidDist()));
                    return true;
                }
                const cXyz from = k->current.pos;
                placeBird(k, to, k->current.angle.y);
                if (d == Drop::Abgrund12) {
                    dComIfGs_setLife(12);
                } else if (d == Drop::Abgrund4) {
                    dComIfGs_setLife(4);
                }
                kff::logInfo("wp1: %s Vogel verschoben (Test-Teleport) t=%u von (%.0f,%.0f,%.0f) nach (%.0f,%.0f,%.0f), "
                             "Boden darunter: %s, life=%u",
                    name, c.frame, static_cast<double>(from.x), static_cast<double>(from.y), static_cast<double>(from.z),
                    static_cast<double>(to.x), static_cast<double>(to.y), static_cast<double>(to.z),
                    groundAt(to.x, 60000.0f, to.z) == kNoGround ? "keiner" : "ja",
                    static_cast<unsigned>(dComIfGs_getLife()));
            } else if (!s.bDone && c.frame >= s.prepTick + 2) {
                s.bDone = true;  // B zwei Ticks nach dem Verschieben
            }
            break;
        case Drop::Hoch:
            if (!s.prepared && f >= 120) {
                s.prepared = true;
                s.prepTick = c.frame;
                const float g = groundAt(k->current.pos.x, k->current.pos.y + 500.0f, k->current.pos.z);
                if (g == kNoGround) {
                    kff::logInfo("wp1: RESULT %s fail kein Boden unter dem Vogel", name);
                    return true;
                }
                cXyz to = k->current.pos;
                to.y = g + 5200.0f;
                placeBird(k, to, k->current.angle.y);
                s.lastY = to.y;
                kff::logInfo("wp1: %s Vogel auf 5200 ueber den Boden (%.1f) gehoben (Test-Teleport) t=%u", name,
                    static_cast<double>(g), c.frame);
            } else if (s.prepared && !s.bDone && c.frame >= s.prepTick + 30) {
                s.bDone = true;
            }
            if (s.prepared) {
                const float g = groundAt(k->current.pos.x, k->current.pos.y + 500.0f, k->current.pos.z);
                sy = g == kNoGround ? 0.0f : holdHeight(k, g + 5200.0f, s.lastY);
            }
            break;
        case Drop::Boden60:
            if (!s.prepared && f >= 120) {
                s.prepared = true;
                s.prepTick = c.frame;
                if (!find60(k->current.pos, &s.spot)) {
                    kff::logInfo("wp1: RESULT %s fail kein Boden mit Attribut 0x60 im Umkreis 12000", name);
                    return true;
                }
                const cXyz to(s.spot.x, s.spot.y + 600.0f, s.spot.z);
                placeBird(k, to, k->current.angle.y);
                kff::logInfo("wp1: %s Vogel ueber Boden mit Attribut 0x60 bei (%.0f,%.1f,%.0f) gesetzt (Test-Teleport) t=%u", name,
                    static_cast<double>(s.spot.x), static_cast<double>(s.spot.y), static_cast<double>(s.spot.z), c.frame);
                s.bDone = true;  // B im selben Tick: Landeanflug auf die Stelle
            }
            break;
        }
        c.stick(sx, sy);
        if (s.bDone && s.bTick == 0) {
            s.bTick = c.frame;
            s.lifeAtB = dComIfGs_getLife();
            c.tapB();
            const float g = groundAt(k->current.pos.x, k->current.pos.y + 500.0f, k->current.pos.z);
            kff::logInfo("wp1: %s B t=%u Ritt-Frame %u Vogel=(%.0f,%.0f,%.0f) Boden %.1f life=%u", name, c.frame, f,
                static_cast<double>(k->current.pos.x), static_cast<double>(k->current.pos.y),
                static_cast<double>(k->current.pos.z), static_cast<double>(g == kNoGround ? -1.0e9f : g), s.lifeAtB);
        }
        if (s.bTick != 0 && c.frame > s.bTick) {
            kff::logInfo("wp1: %s Anflug t=%u landF=%u Vogel y=%.1f Link y=%.1f Boden %.1f Fuesse=%d a6de=%d", name, c.frame,
                c.sess.landFrames, static_cast<double>(k->current.pos.y), static_cast<double>(link->current.pos.y),
                static_cast<double>(groundAt(link->current.pos.x, link->current.pos.y + 500.0f, link->current.pos.z)),
                link->mLinkAcch.ChkGroundHit() ? 1 : 0, static_cast<int>(k->field_0x6de));
        }
    }
    if (b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + (voidCase ? 1200 : 600);
            if (link != nullptr) {
                s.releaseLinkY = link->current.pos.y;
            }
            kff::logInfo("wp1: %s Freigabe t=%u Link y=%.1f (%u Ticks nach B)", name, c.frame, static_cast<double>(s.releaseLinkY),
                s.bTick != 0 ? c.frame - s.bTick : 0);
        }
        // E9: Horizontalbewegung von Link in den Ticks nach der Freigabe (Tick+1 = erster ganzer Frame danach).
        if (link != nullptr) {
            const uint32_t dt = c.frame - b.releaseTick;
            if (dt == 0) {
                s.relPos = link->current.pos;
            } else if (dt <= 3) {
                const float dx = link->current.pos.x - s.relPos.x;
                const float dz = link->current.pos.z - s.relPos.z;
                const float h = std::sqrt(dx * dx + dz * dz);
                if (dt == 1) {
                    s.relMove1 = h;
                }
                kff::logInfo("wp1: %s Horizontal nach Freigabe Tick+%u: %.3f (speedF %.3f speed.x %.3f speed.z %.3f) y=%.1f "
                             "proc=%d",
                    name, dt, static_cast<double>(h), static_cast<double>(link->speedF),
                    static_cast<double>(link->speed.x), static_cast<double>(link->speed.z),
                    static_cast<double>(link->current.pos.y), static_cast<int>(link->mProcID));
                s.relPos = link->current.pos;
            }
        }
        if (link != nullptr && !s.touched && link->mLinkAcch.ChkGroundHit() && !voidCase) {
            s.touched = true;
            s.touchTick = c.frame;
            s.touchY = link->current.pos.y;
            kff::logInfo("wp1: %s Boden beruehrt t=%u y=%.1f Loslasshoehe %.1f Differenz %.1f mFallHeight %.1f field_0x33c8 %.1f",
                name, c.frame, static_cast<double>(s.touchY), static_cast<double>(s.releaseLinkY),
                static_cast<double>(s.touchY - s.releaseLinkY), static_cast<double>(link->mFallHeight),
                static_cast<double>(link->field_0x33c8));
        }
        // Schadensquelle nach der Freigabe bis 90 Ticks nach der Bodenberuehrung: Sturzschaden (Land-Damage-Proc) oder
        // andere Quelle (Damage-Procs: Bodenpolygon mit Attribut 0x60 inkl. Eis, Gegner; Diagnosezeilen "DMG").
        if (link != nullptr && !voidCase && (!s.touched || c.frame < s.touchTick + 90)) {
            const int p = static_cast<int>(link->mProcID);
            if (p != s.lastProc) {
                if (p == daAlink_c::PROC_WOLF_LAND_DAMAGE || p == daAlink_c::PROC_LAND_DAMAGE) {
                    s.fallDamage = true;
                    kff::logInfo("wp1: %s Sturzschaden-Proc %d t=%u life=%u", name, p, c.frame,
                        static_cast<unsigned>(dComIfGs_getLife()));
                } else if (p == daAlink_c::PROC_WOLF_DAMAGE || p == daAlink_c::PROC_WOLF_LARGE_DAMAGE_UP ||
                           p == daAlink_c::PROC_LARGE_DAMAGE || p == daAlink_c::PROC_DAMAGE) {
                    ++s.otherDamage;
                    kff::logInfo("wp1: %s Schaden anderer Quelle (Proc %d) t=%u life=%u Bodenattribut 0x%x", name, p, c.frame,
                        static_cast<unsigned>(dComIfGs_getLife()),
                        static_cast<unsigned>(dKy_pol_argument_get(&link->mLinkAcch.m_gnd)));
                }
                s.lastProc = p;
            }
        }
        if (s.touched && !s.lifeLandSet && c.frame >= s.touchTick + 90) {
            s.lifeLandSet = true;
            s.lifeLand = dComIfGs_getLife();
            kff::logInfo("wp1: %s Leben 90 Ticks nach der Bodenberuehrung %u (vor B %u)", name, s.lifeLand, s.lifeAtB);
        }
        if (b.firstLinkChange != 0 && s.restartLink == 0) {
            s.restartLink = b.firstLinkChange;
        }
        if (c.frame >= s.endAt) {
            const unsigned lifeNow = dComIfGs_getLife();
            const bool zeroOk = b.zero && b.zeroTick - b.releaseTick <= 100;
            const bool whyOk = b.whyKnown && b.why == kff::EndReason::DismountButton;
            bool ok = false;
            // Liste der verfehlten Kriterien fuer die RESULT-Zeile.
            char miss[128] = {};
            auto fehlt = [&miss](bool good, const char* what) {
                if (!good) {
                    const size_t n = std::strlen(miss);
                    std::snprintf(miss + n, sizeof(miss) - n, "%s%s", n > 0 ? "," : "", what);
                }
            };
            if (voidCase) {
                // Void-Out ist Vanilla (OE1, Z1 b): Void-Out (Link-Wechsel) oder Game Over hoechstens 150 Ticks nach der
                // Freigabe. E12 (Z2): Game Over beim Abgrund mit Leben 4 ist Vanilla (Kontrolle abgrund-4-ohne) und kein
                // Fehler; Leben und Game Over stehen als Ist-Werte im Log (Vergleich mit dem Kontrolllauf ohne Ritt).
                uint32_t outTick = s.restartLink;
                if (s.gameOverTick != 0 && (outTick == 0 || s.gameOverTick < outTick)) {
                    outTick = s.gameOverTick;
                }
                const int outTicks = outTick != 0 ? static_cast<int>(outTick - b.releaseTick) : -1;
                const bool outOk = outTicks >= 0 && outTicks <= 150;
                ok = whyOk && zeroOk && outOk;
                fehlt(whyOk, "grund");
                fehlt(zeroOk, "birds0<=100");
                fehlt(outOk, "voidout<=150");
                kff::logInfo("wp1: RESULT %s %s grund=%s birds0 nach %d Ticks life vorB=%u min=%u jetzt=%u gameOver=%d "
                             "neustart(Link-Wechsel) t=%u voidOut nach %d Ticks stage=%s landeanfluege=%u verfehlt=%s",
                    name, ok ? "ok" : "fail", b.whyKnown ? kff::endReasonName(b.why) : "?",
                    b.zero ? static_cast<int>(b.zeroTick - b.releaseTick) : -1, s.lifeAtB, s.lifeMin, lifeNow,
                    s.gameOver ? 1 : 0, s.restartLink, outTicks, stageName(), s.approaches, miss[0] != 0 ? miss : "-");
            } else {
                const bool oneApproach = s.approaches == 1;
                const bool approachOk = s.maxLand <= 240;
                const bool stageOk = sameStage(b) && b.linkChanges == 0 &&
                                     dComIfGp_roomControl_getStayNo() == b.room0;
                // Leben: 90 Ticks nach der Bodenberuehrung (Sturzschaden). Spaeterer Schaden stammt von Gegnern im
                // Feld (f-boden3: E_YR, Profil 506, DMG-Zeilen der Diagnose) und wird nur berichtet.
                // Verlust mit beobachteter anderer Quelle (Damage-Proc: Eis-/Schadenspolygon, Gegner) ist kein Sturzschaden;
                // ein Land-Damage-Proc ist immer ein Fehler.
                const unsigned lifeCheck = s.lifeLandSet ? s.lifeLand : lifeNow;
                const bool lifeOk = !s.fallDamage && (lifeCheck >= s.lifeAtB || s.otherDamage > 0);
                const bool touchOk = s.touched && std::fabs(s.touchY - s.releaseLinkY) <= 10.0f;
                ok = whyOk && oneApproach && approachOk && zeroOk && stageOk && lifeOk && s.jumps6de == 0 &&
                     (d != Drop::Boden || touchOk);
                fehlt(whyOk, "grund");
                fehlt(oneApproach, "einAnflug");
                fehlt(approachOk, "anflug<=240");
                fehlt(zeroOk, "birds0<=100");
                fehlt(stageOk, "stage");
                fehlt(lifeOk, "leben");
                fehlt(s.jumps6de == 0, "6de");
                fehlt(d != Drop::Boden || touchOk, "loslasshoehe");
                kff::logInfo("wp1: RESULT %s %s grund=%s landeanfluege=%u anflug=%u Ticks birds0 nach %d Ticks stage=%s room=%d(%s) "
                             "life vorB=%u nachLandung=%u(%s) jetzt=%u sturzschaden=%d andererSchaden=%u boden-beruehrt=%d "
                             "dy=%.1f(%s) 6de-spruenge=%u linkWechsel=%u horizontalTick1=%.3f verfehlt=%s",
                    name, ok ? "ok" : "fail", b.whyKnown ? kff::endReasonName(b.why) : "?", s.approaches, s.maxLand,
                    b.zero ? static_cast<int>(b.zeroTick - b.releaseTick) : -1, stageName(),
                    dComIfGp_roomControl_getStayNo(), stageOk ? "gleich" : "GEWECHSELT", s.lifeAtB, lifeCheck,
                    lifeOk ? (lifeCheck >= s.lifeAtB ? "gleich" : "Verlust andere Quelle") : "VERLUST", lifeNow,
                    s.fallDamage ? 1 : 0, s.otherDamage, s.touched ? 1 : 0, static_cast<double>(s.touchY - s.releaseLinkY),
                    touchOk ? "<=10" : ">10", s.jumps6de, b.linkChanges, static_cast<double>(s.relMove1),
                    miss[0] != 0 ? miss : "-");
            }
            return true;
        }
    }
    return false;
}

bool bodenTick(kff::autotest::Context& c) { return absteigenTick(c, Drop::Boden); }
bool voidTick(kff::autotest::Context& c) { return absteigenTick(c, Drop::Void); }

// WP1.5 abgrund-faelle: Fall aus KFF_WP1_FALL (a Abgrund bei Leben 12, b Abgrund bei Leben 4, c Boden mit Attribut 0x60,
// d Landung aus 5200 ueber Boden). Ohne Angabe Fall a.
bool abgrundFaelleTick(kff::autotest::Context& c) {
    static const Drop d = [] {
        const char* e = std::getenv("KFF_WP1_FALL");
        const char f = e != nullptr ? e[0] : 'a';
        return f == 'b' ? Drop::Abgrund4 : (f == 'c' ? Drop::Boden60 : (f == 'd' ? Drop::Hoch : Drop::Abgrund12));
    }();
    return absteigenTick(c, d);
}

// ------------------------------------------------------------------------- WP1.6 warp-portal
struct Warp {
    Base b;
    bool set = false;
    uint32_t setTick = 0;
    cXyz home;
    int homeRoom = 0;
    uint32_t endAt = 0;
    uint32_t newLinkTick = 0;  // erster execute des neuen Links nach der Warp-Anforderung
    bool invalid = false;      // Testaufbau ohne gueltiges Ziel: Szenario endet ohne Warp
    char dest[9] = {};         // Ziel-Stage des Warps (Kartendaten)
    bool newLinkLogged = false;
    cXyz newLinkPos;           // Position des ersten neuen Links (Startpunkt im Zielraum)
    int newLinkRoom = -1;
    float tagGap = -1.0f;      // XZ-Abstand des ersten neuen Links zum naechsten kytag04 (Vanilla-create verlangt < 1)
};
Warp g_wp;

// Nach der Warp-Anforderung: laeuft der neue Link (execute), oder bleibt der Szenenaufbau stehen? (f-warp1: schwarz,
// kein execute des neuen Links ueber 1400 Ticks.)
void warpProbe(kff::autotest::Context& c, Warp& s) {
    if (!s.set) {
        return;
    }
    if (s.newLinkTick == 0 && s.b.linkChanges > 0) {
        s.newLinkTick = c.frame;
    }
    if ((c.frame - s.setTick) % 60 == 0) {
        daAlink_c* pl = daAlink_getAlinkActorClass();
        const fpc_ProcID id = pl != nullptr ? fopAcM_GetID(pl) : kff::kNoProc;
        kff::logInfo("wp1: warp Probe t=%u (+%u) alink=%s id=0x%x executing=%d creating=%d linkCalls=%u ev=%d next=%d "
                     "stage=%s room=%d linkWechsel=%u",
            c.frame, c.frame - s.setTick, pl != nullptr ? "ja" : "nein", static_cast<unsigned>(id),
            id != kff::kNoProc ? (fopAcM_IsExecuting(id) ? 1 : 0) : -1, id != kff::kNoProc ? (fpcM_IsCreating(id) ? 1 : 0) : -1,
            c.sess.linkCalls, inEvent() ? 1 : 0, dComIfGp_isEnableNextStage() != 0 ? 1 : 0, stageName(),
            dComIfGp_roomControl_getStayNo(), s.b.linkChanges);
    }
}

struct PortalList {
    fopAc_ac_c* ac[16] = {};
    int n = 0;
};

int findPortalCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    PortalList* list = static_cast<PortalList*>(data);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KYTAG04_e && list->n < 16) {
        list->ac[list->n++] = ac;
    }
    return 1;
}

// Big-Endian-Werte aus den Kartendateien.
uint32_t beU32(const u8* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

float beF32(const u8* p) {
    const uint32_t v = beU32(p);
    float f = 0.0f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
}

// Eintrag aus dat/portal.dat (dMenu_Fmap_portal_data_c::data, 0x1C Bytes, Big Endian) plus Freischaltung und Weltposition.
struct PortalEntry {
    bool found = false;
    int index = -1;
    char stage[9] = {};
    u8 selectWarpPt = 0;
    u8 regionNo = 0;
    u8 roomNo = 0;
    u8 warpPlayerNo = 0;
    u8 stageNo = 0;
    u8 switchNo = 0;
    bool open = false;
    // Kartenposition + Region-Ursprung (wie dMenu_Fmap_c::portal_warp_select_proc). Nur Protokoll: am Ziel setzt
    // dStage_playerInit Link bei Punkt -4 auf den Startpunkt mit der Nummer TurnRestart-Winkel = mWarpPlayerNo
    // (skipPortalObjWarp), nicht auf diese Position (f-warp7: Startpunkt 1 in F_SP121 Raum 3 bei x-100000).
    cXyz world;
};

// Region-Urspruenge aus dat/field.dat (dMenu_Fmap_field_region_data_c, 0x28 Bytes je Region). Die Karte setzt als Warp-Ziel
// Portalposition + Ursprung der Region unter dem Cursor (portal_warp_select_proc); waehlbar sind dort nur Portale mit
// mRegionNo == Cursor + 1 (portalWarpMapMove), und der Ursprung von Cursor j stammt aus der Region mit
// mTextureReadNum == j + 1 (decodeFieldMapData). Also: Ursprung der Region mit mTextureReadNum == mRegionNo des Portals.
struct RegionOrigins {
    bool ok = false;
    bool have[16] = {};
    float x[16] = {};
    float z[16] = {};
};

RegionOrigins readRegionOrigins(JKRAramArchive* arc) {
    RegionOrigins r;
    alignas(32) static u8 buf[0x40000];
    const u32 need = dLib_getExpandSizeFromAramArchive(arc, "dat/field.dat");
    if (need == 0 || need > sizeof(buf)) {
        kff::logInfo("wp1: warp field.dat: Groesse %u passt nicht in den Puffer (%u)", need, static_cast<unsigned>(sizeof(buf)));
        return r;
    }
    const u32 got = static_cast<JKRArchive*>(arc)->readResource(buf, sizeof(buf), "dat/field.dat");
    if (got < 0x14) {
        kff::logInfo("wp1: warp field.dat: nicht gelesen (%u Bytes)", got);
        return r;
    }
    const u32 off = beU32(buf + 0x08);
    if (off + 4 > got) {
        kff::logInfo("wp1: warp field.dat: Region-Tabelle bei 0x%x ausserhalb (%u Bytes)", off, got);
        return r;
    }
    const int count = buf[off];
    kff::logInfo("wp1: warp field.dat: %u Bytes, Region-Tabelle bei 0x%x, %d Regionen", got, off, count);
    for (int i = 0; i < count; ++i) {
        const u32 e = off + 4 + static_cast<u32>(i) * 0x28;
        if (e + 0x28 > got) {
            break;
        }
        const int tex = buf[e];
        const float ox = beF32(buf + e + 0x04);
        const float oz = beF32(buf + e + 0x08);
        kff::logInfo("wp1: warp field.dat Region[%d] Textur=%d Ursprung=(%.1f,%.1f)", i, tex, static_cast<double>(ox),
            static_cast<double>(oz));
        if (tex >= 0 && tex < 16 && !r.have[tex]) {
            r.have[tex] = true;
            r.x[tex] = ox;
            r.z[tex] = oz;
        }
    }
    r.ok = true;
    return r;
}

// Portalwahl wie die Karte: nur freigeschaltete Portale (checkDrawPortalIcon: dComIfGs_isStageSwitch(Stage-Nr., Schalter)).
// Bevorzugt ein Portal einer anderen Stage (echter Stage-Wechsel), sonst eines dieser Stage; KFF_WP1_PORTAL=<n> erzwingt
// Eintrag n. f-warp6: Stage-Start per --stage ohne Spielfortschritt, alle 15 Portale gesperrt. Dann das erste Portal dieser
// Stage (Freischaltung als Testaufbau in warpRequest). Formelkontrolle im Log: Abstand jeder Weltposition zum naechsten
// kytag04 der geladenen Stage.
PortalEntry pickPortal(const char* stage, const PortalList& tags) {
    PortalEntry best;
    PortalEntry same;
    PortalEntry firstSame;
    JKRAramArchive* arc = dComIfGp_getFieldMapArchive2();
    if (arc == nullptr) {
        kff::logInfo("wp1: warp Kartenarchiv nicht geladen");
        return best;
    }
    alignas(32) static u8 buf[0x2000];
    const u32 got = static_cast<JKRArchive*>(arc)->readResource(buf, sizeof(buf), "dat/portal.dat");
    if (got < 0x0C) {
        kff::logInfo("wp1: warp portal.dat: nicht gelesen (%u Bytes)", got);
        return best;
    }
    const RegionOrigins org = readRegionOrigins(arc);
    if (!org.ok) {
        return best;
    }
    static const int forced = [] {
        const char* e = std::getenv("KFF_WP1_PORTAL");
        return e != nullptr ? std::atoi(e) : -1;
    }();
    const int count = buf[0x08];
    kff::logInfo("wp1: warp portal.dat: %u Bytes, %d Eintraege, erzwungen=%d", got, count, forced);
    for (int i = 0; i < count; ++i) {
        const u32 off = 0x0C + static_cast<u32>(i) * 0x1C;
        if (off + 0x1C > got) {
            break;
        }
        const u8* e = buf + off;
        PortalEntry p;
        p.index = i;
        std::memcpy(p.stage, e + 0x02, 8);
        p.selectWarpPt = e[0x00];
        p.regionNo = e[0x01];
        p.roomNo = e[0x0A];
        p.warpPlayerNo = e[0x0B];
        p.stageNo = e[0x0E];
        p.switchNo = e[0x0F];
        p.open = dComIfGs_isStageSwitch(p.stageNo, p.switchNo) != 0;
        const bool haveOrg = p.regionNo < 16 && org.have[p.regionNo];
        p.world = cXyz(beF32(e + 0x10) + (haveOrg ? org.x[p.regionNo] : 0.0f), beF32(e + 0x14),
            beF32(e + 0x18) + (haveOrg ? org.z[p.regionNo] : 0.0f));
        p.found = haveOrg;
        float nearest = -1.0f;
        for (int t = 0; t < tags.n; ++t) {
            const float dx = tags.ac[t]->current.pos.x - p.world.x;
            const float dz = tags.ac[t]->current.pos.z - p.world.z;
            const float d = std::sqrt(dx * dx + dz * dz);
            if (nearest < 0.0f || d < nearest) {
                nearest = d;
            }
        }
        const bool sameStage = std::strncmp(p.stage, stage, 8) == 0;
        kff::logInfo("wp1: warp portal.dat[%d] %s Raum=%d Spieler=%d Auswahl=%d Region=%d StageNr=%d Schalter=%d frei=%d "
                     "Welt=(%.1f,%.1f,%.1f)%s%s naechster kytag04 hier %.1f",
            i, p.stage, p.roomNo, p.warpPlayerNo, p.selectWarpPt, p.regionNo, p.stageNo, p.switchNo, p.open ? 1 : 0,
            static_cast<double>(p.world.x), static_cast<double>(p.world.y), static_cast<double>(p.world.z),
            haveOrg ? "" : " (kein Region-Ursprung)", sameStage ? " (diese Stage)" : "", static_cast<double>(nearest));
        if (!p.found) {
            continue;
        }
        if (sameStage && !firstSame.found) {
            firstSame = p;
        }
        if (forced >= 0) {
            if (i == forced) {
                best = p;
            }
            continue;
        }
        if (!p.open) {
            continue;
        }
        if (!sameStage && !best.found) {
            best = p;
        } else if (sameStage && !same.found) {
            same = p;
        }
    }
    if (forced < 0 && !best.found) {
        best = same.found ? same : firstSame;
    }
    return best;
}

void warpRequest(kff::autotest::Context& c, Warp& s) {
    // Spielweg nach der Kartenwahl (dMenu_Fmap_c::portal_warp_select_proc): Warp-Ziel und Status 3 (entschieden).
    // Danach startet Link selbst den Warp (checkWarpStart, Ereignis POTAL_WARPIN, procCoWarpInit, Szenenwechsel).
    // Am Ziel setzt dStage_playerInit Link bei Punkt -4 auf den Startpunkt Nr. mWarpPlayerNo des Zielraums (als
    // TurnRestart-Winkel uebergeben), und der create wartet, bis Boden unter ihm liegt und ein kytag04 naeher als 1 steht
    // (daAlink_searchPortal). f-warp1/f-warp-ohne1 (Startpunkt ohne Portal): neuer Link blieb im create, mit und ohne Ritt
    // gleich. f-warp2 (Raum -1): Absturz in isReadRoom. f-warp3: Boden nicht geladen. f-warp4/5: kein kytag04 dieser Stage
    // mit eigenem Karteneintrag. Daher wie die Karte selbst: Portal aus dat/portal.dat (Stage, Raum, Spieler-Nr.,
    // Auswahlpunkt), Position wie die Karte (Kartenposition + Region-Ursprung aus dat/field.dat, am Ziel ungenutzt).
    PortalList list;
    fopAcIt_Executor(findPortalCb, &list);
    for (int i = 0; i < list.n; ++i) {
        kytag04_class* portal = static_cast<kytag04_class*>(list.ac[i]);
        kff::logInfo("wp1: warp kytag04 %d/%d hier pos=(%.1f,%.1f,%.1f) Typ=%d StageNr=%d Schalter=%d", i + 1, list.n,
            static_cast<double>(portal->current.pos.x), static_cast<double>(portal->current.pos.y),
            static_cast<double>(portal->current.pos.z), static_cast<int>(portal->field_0x5b4), portal->mStageNo,
            static_cast<int>(portal->field_0x5b5));
    }
    const PortalEntry e = pickPortal(stageName(), list);
    if (!e.found || e.roomNo >= 64) {
        kff::logInfo("wp1: RESULT warp-portal fail Testaufbau: kein freigeschaltetes Portal mit Kartendaten, Warp nicht "
                     "ausgeloest");
        s.invalid = true;
        return;
    }
    if (!e.open) {
        // Testaufbau: Portal freischalten wie nach dem Sieg ueber die Schattenwesen (Schalter im Spielstand).
        dComIfGs_onStageSwitch(e.stageNo, e.switchNo);
        kff::logInfo("wp1: warp Testaufbau: Portal[%d] %s freigeschaltet (Stage-Nr. %d, Schalter %d) -> frei=%d", e.index,
            e.stage, e.stageNo, e.switchNo, dComIfGs_isStageSwitch(e.stageNo, e.switchNo) != 0 ? 1 : 0);
    }
    s.home = e.world;
    s.homeRoom = e.roomNo;
    std::memcpy(s.dest, e.stage, sizeof(s.dest));
    dMeter2Info_setWarpInfo(e.stage, e.world, 0, e.roomNo, 0, e.warpPlayerNo);
    dComIfGp_SelectWarpPt_set(e.selectWarpPt);
    dMeter2Info_setWarpStatus(WARP_STATUS_DECIDED_e);
    s.set = true;
    s.setTick = c.frame;
    kff::logInfo("wp1: warp Warp-Ziel gesetzt t=%u Portal[%d] %s Kartenpos=(%.1f,%.1f,%.1f) room=%d Spieler(Startpunkt)=%d "
                 "Auswahl=%d frei=%d (von %s), Status 3 (Kartenwahl)",
        c.frame, e.index, e.stage, static_cast<double>(e.world.x), static_cast<double>(e.world.y),
        static_cast<double>(e.world.z), e.roomNo, e.warpPlayerNo, e.selectWarpPt, e.open ? 1 : 0, stageName());
}

// Erster neuer Link nach dem Warp: Position, Raum und XZ-Abstand zum naechsten kytag04. Der Vanilla-create wartet bei
// Startpunkt -4, bis ein kytag04 naeher als 1 an Link steht (daAlink_searchPortal); Abstand < 1 heisst Ankunft am Portal.
void warpArrival(kff::autotest::Context& c, Warp& s) {
    if (s.newLinkTick == 0 || s.newLinkLogged) {
        return;
    }
    const daAlink_c* link = kff::liveLink();
    if (link == nullptr) {
        return;
    }
    s.newLinkLogged = true;
    s.newLinkPos = link->current.pos;
    s.newLinkRoom = dComIfGp_roomControl_getStayNo();
    PortalList list;
    fopAcIt_Executor(findPortalCb, &list);
    for (int i = 0; i < list.n; ++i) {
        const float dx = list.ac[i]->current.pos.x - s.newLinkPos.x;
        const float dz = list.ac[i]->current.pos.z - s.newLinkPos.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (s.tagGap < 0.0f || d < s.tagGap) {
            s.tagGap = d;
        }
    }
    kff::logInfo("wp1: warp neuer Link t=%u (%u Ticks nach dem Warp-Status) pos=(%.1f,%.1f,%.1f) stage=%s Raum=%d naechster "
                 "kytag04 XZ %.2f (%d kytag04)",
        c.frame, c.frame - s.setTick, static_cast<double>(s.newLinkPos.x), static_cast<double>(s.newLinkPos.y),
        static_cast<double>(s.newLinkPos.z), stageName(), s.newLinkRoom, static_cast<double>(s.tagGap), list.n);
}

bool warpTick(kff::autotest::Context& c) {
    static const bool ohne = [] {
        const char* e = std::getenv("KFS_OHNE_RITT");
        return e != nullptr && e[0] == '1';
    }();
    Warp& s = g_wp;
    Base& b = s.b;
    b.callAt = ohne ? 0xFFFFFFFFu : 300;
    track(c, b, ohne ? "warp-ohne" : "warp");
    warpProbe(c, s);
    warpArrival(c, s);
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT warp-portal fail Zeitlimit (rode=%d released=%d set=%d)", b.rode ? 1 : 0, b.released ? 1 : 0,
            s.set ? 1 : 0);
        return true;
    }
    if (ohne) {
        // Kontrolle ohne Ritt: gleicher Warp zu Fuss bei t=500, Bewertung 1500 Ticks danach.
        if (!s.set && c.frame >= 500 && !inEvent()) {
            warpRequest(c, s);
            if (s.invalid) {
                return true;
            }
            // Link-Wechsel vor dem Warp zaehlen nicht.
            b.linkChanges = 0;
            b.firstLinkChange = 0;
            b.linkId = c.sess.linkId;
        }
        if (s.set && c.frame >= s.setTick + 1500) {
            const bool reached = std::strncmp(stageName(), s.dest, 8) == 0 && s.newLinkRoom == s.homeRoom;
            const bool atPortal = s.tagGap >= 0.0f && s.tagGap < 1.0f;
            const bool ok = b.linkChanges > 0 && reached && atPortal;
            kff::logInfo("wp1: RESULT warp-ohne %s Kontrolle ohne Ritt: neuer Link t=%u (%d Ticks nach dem Warp-Status) "
                         "linkWechsel=%u ziel=%s/%d erreicht=%d kytag04AmStart=%.2f stage=%s room=%d",
                ok ? "ok" : "fail", s.newLinkTick, s.newLinkTick != 0 ? static_cast<int>(s.newLinkTick - s.setTick) : -1,
                b.linkChanges, s.dest, s.homeRoom, reached ? 1 : 0, static_cast<double>(s.tagGap), stageName(),
                dComIfGp_roomControl_getStayNo());
            return true;
        }
        return false;
    }
    if (s.invalid) {
        // Testaufbau ohne gueltiges Ziel: Ritt per B beenden (sonst fliegt der Vogel ohne Eingabe geradeaus weiter).
        if (b.rode && !b.released && c.frame % 30 == 0) {
            c.tapB();
        }
        return b.released || !b.rode;
    }
    if (b.rode && !b.released && !s.set && c.sess.rideFrames >= 200) {
        warpRequest(c, s);
    }
    if (s.set && b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + 1500;
        }
        if (c.frame >= s.endAt) {
            const bool whyOk = b.whyKnown && b.why == kff::EndReason::Warp;
            const bool orderOk = b.firstLinkChange == 0 || b.firstLinkChange > b.releaseTick;
            const bool zeroOk = b.zero && b.zeroTick - b.releaseTick <= 60;
            const bool noVeto = b.vetoAtEnd == b.vetoAtRide && vetoTotal() == b.vetoAtRide;
            const bool changed = b.linkChanges > 0;
            const bool reached = std::strncmp(stageName(), s.dest, 8) == 0 && s.newLinkRoom == s.homeRoom;
            const bool atPortal = s.tagGap >= 0.0f && s.tagGap < 1.0f;
            const bool ok = whyOk && orderOk && zeroOk && noVeto && changed && reached && atPortal;
            kff::logInfo("wp1: RESULT warp-portal %s grund=%s ende t=%u (%u Ticks nach dem Warp-Status) linkWechsel t=%u (%s) "
                         "neuer Link %d Ticks nach dem Warp-Status birds0 nach %d Ticks veto=%u ziel=%s/%d erreicht=%d "
                         "kytag04AmStart=%.2f stage=%s room=%d birds=%u linkWechsel=%u",
                ok ? "ok" : "fail", b.whyKnown ? kff::endReasonName(b.why) : "?", b.releaseTick, b.releaseTick - s.setTick,
                b.firstLinkChange, orderOk ? "danach" : "DAVOR",
                s.newLinkTick != 0 ? static_cast<int>(s.newLinkTick - s.setTick) : -1,
                b.zero ? static_cast<int>(b.zeroTick - b.releaseTick) : -1, vetoTotal() - b.vetoAtRide, s.dest, s.homeRoom,
                reached ? 1 : 0, static_cast<double>(s.tagGap), stageName(), dComIfGp_roomControl_getStayNo(), birds(),
                b.linkChanges);
            return true;
        }
    } else if (b.released && !s.set) {
        kff::logInfo("wp1: RESULT warp-portal fail Ritt endete vor dem Warp (grund=%s)",
            b.whyKnown ? kff::endReasonName(b.why) : "?");
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------- WP1.7 raumwechsel-unter-vogel
// Befund f-raum1 / f-raum-ohne1: geradeaus nach Westen (26000 Einheiten) und im Kreis zu Fuss bleibt der Aufenthaltsraum 10;
// die "Raumwechsel" waren nur die Ladeuebergaenge bei t=0. Daher sucht das Szenario jetzt per Bodensonde (Polygon-Raum-ID,
// dBgS::GetRoomId) den naechsten Boden eines anderen Raums und fliegt (bzw. laeuft in der Kontrolle) dorthin. Gezaehlt
// werden nur Raumwechsel nach dem Ritt-Start (Kontrolle: nach dem Start der Fussstrecke).
struct Raum {
    Base b;
    int lastRoom = -99;
    int startRoom = -1;
    uint32_t roomChanges = 0;
    uint32_t firstChange = 0;
    int firstNewRoom = -1;
    bool searched = false;
    bool haveTarget = false;
    cXyz target;
    int targetRoom = -1;
    float holdY = 0.0f;
    float wantY = 0.0f;
    float lastY = 0.0f;
    int birdLayerAtChange = -2;
    uint32_t layerBad = 0;      // Ticks nach dem Raumwechsel mit anderem Vogel-Layer
    uint32_t birdsMaxAfter = 0; // birds nach dem Raumwechsel (Ritt)
    bool bDone = false;
    uint32_t endAt = 0;
    bool walking = false;
    uint32_t walkStart = 0;
    bool ohneRitt = false;
    cXyz progressPos;
    bool progressSet = false;
    bool moved = false;  // Kontrolle: Link auf den Zielboden versetzt
};
Raum g_raum;

// Bodenpunkt eines anderen Raums als stay (Raster 1000, Radius 40000). Gewaehlt wird der guenstigste nach
// XZ-Abstand + 4 x Hoehenunterschied (f-raum2: der naechste Punkt lag 4000 hoeher, zu Fuss nicht erreichbar).
bool findOtherRoom(const cXyz& ref, int stay, cXyz* out, int* room) {
    float best = 1.0e30f;
    bool found = false;
    int counts[64] = {};
    float roomBest[64];
    cXyz roomPos[64];
    for (int r = 0; r < 64; ++r) {
        roomBest[r] = 1.0e30f;
    }
    int samples = 0;
    for (float x = -40000.0f; x <= 40000.0f; x += 1000.0f) {
        for (float z = -40000.0f; z <= 40000.0f; z += 1000.0f) {
            dBgS_GndChk gnd;
            cXyz p(ref.x + x, 60000.0f, ref.z + z);
            gnd.SetPos(&p);
            const float g = dComIfG_Bgsp().GroundCross(&gnd);
            if (g == -G_CM3D_F_INF) {
                continue;
            }
            ++samples;
            const int r = dComIfG_Bgsp().GetRoomId(gnd);
            if (r < 0 || r >= 64) {
                continue;
            }
            ++counts[r];
            if (r == stay) {
                continue;
            }
            const float cost = std::sqrt(x * x + z * z) + 4.0f * std::fabs(g - ref.y);
            if (cost < roomBest[r]) {
                roomBest[r] = cost;
                roomPos[r] = cXyz(ref.x + x, g, ref.z + z);
            }
            if (cost < best) {
                best = cost;
                *out = cXyz(ref.x + x, g, ref.z + z);
                *room = r;
                found = true;
            }
        }
    }
    char list[512] = {};
    for (int r = 0; r < 64; ++r) {
        if (counts[r] > 0) {
            const size_t n = std::strlen(list);
            if (r != stay && roomBest[r] < 1.0e29f) {
                std::snprintf(list + n, sizeof(list) - n, "%sR%d:%d bester (%.0f,%.0f,%.0f) Kosten %.0f", n > 0 ? "; " : "", r,
                    counts[r], static_cast<double>(roomPos[r].x), static_cast<double>(roomPos[r].y),
                    static_cast<double>(roomPos[r].z), static_cast<double>(roomBest[r]));
            } else {
                std::snprintf(list + n, sizeof(list) - n, "%sR%d:%d", n > 0 ? "; " : "", r, counts[r]);
            }
        }
    }
    kff::logInfo("wp1: raum Bodensonde %d Treffer, Polygon-Raeume %s, Aufenthaltsraum %d", samples, list[0] != 0 ? list : "-",
        stay);
    return found;
}

// Stick fuer eine Laufrichtung in Weltkoordinaten (Fuss): mMoveAngle = Stick-Winkel + 0x8000 + Kamera-Gier
// (d_a_alink.cpp:9468-9472). Die Injektion setzt mMainStickAngle = atan2(y, x).
void stickToward(kff::autotest::Context& c, const daAlink_c* link, const cXyz& to) {
    const s16 want = cM_atan2s(to.x - link->current.pos.x, to.z - link->current.pos.z);
    const s16 cam = dCam_getControledAngleY(dComIfGp_getCamera(0));
    const s16 a = static_cast<s16>(want - cam - 0x8000);
    const double rad = static_cast<double>(a) * 3.14159265358979323846 / 32768.0;
    c.stick(static_cast<float>(std::cos(rad)), static_cast<float>(std::sin(rad)));
}

bool raumTick(kff::autotest::Context& c) {
    Raum& s = g_raum;
    Base& b = s.b;
    b.callAt = s.ohneRitt ? 0xFFFFFFFFu : 300;
    track(c, b, s.ohneRitt ? "raum-ohne" : "raum");
    const int room = dComIfGp_roomControl_getStayNo();
    const bool counting = s.ohneRitt ? s.walking : b.rode;
    if (room != s.lastRoom) {
        if (counting && s.lastRoom != -99) {
            ++s.roomChanges;
            if (s.firstChange == 0) {
                s.firstChange = c.frame;
                s.firstNewRoom = room;
                s.birdLayerAtChange = layerOf(kff::liveBird());
            }
        }
        kff::logInfo("wp1: raum Raum %d -> %d t=%u phase=%s birds=%u birdLayer=%d linkLayer=%d%s", s.lastRoom, room, c.frame,
            kff::phaseName(c.sess.phase), birds(), layerOf(kff::liveBird()), layerOf(kff::liveLink()),
            counting ? "" : " (Laden, nicht gezaehlt)");
        s.lastRoom = room;
    }
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT raumwechsel fail Zeitlimit");
        return true;
    }
    daAlink_c* link = kff::liveLink();
    if (s.ohneRitt) {
        // Kontrolle ohne Ritt: zu Fuss zum naechsten Boden eines anderen Raums.
        if (!s.searched && c.frame >= 300 && link != nullptr && !inEvent()) {
            s.searched = true;
            s.startRoom = room;
            s.haveTarget = findOtherRoom(link->current.pos, room, &s.target, &s.targetRoom);
            if (!s.haveTarget) {
                kff::logInfo("wp1: RESULT raumwechsel-ohne fail kein Boden eines anderen Raums im Radius 40000");
                return true;
            }
            s.walking = true;
            s.walkStart = c.frame;
            kff::logInfo("wp1: raum-ohne Ziel Raum %d bei (%.0f,%.0f,%.0f), Abstand %.0f", s.targetRoom,
                static_cast<double>(s.target.x), static_cast<double>(s.target.y), static_cast<double>(s.target.z),
                static_cast<double>(std::sqrt((s.target.x - link->current.pos.x) * (s.target.x - link->current.pos.x) +
                                              (s.target.z - link->current.pos.z) * (s.target.z - link->current.pos.z))));
        }
        if (s.walking && link != nullptr && !inEvent() && (s.firstChange == 0 || c.frame < s.firstChange + 300)) {
            stickToward(c, link, s.target);
            // f-raum-ohne2: zu Fuss nach 22000 Einheiten an einem Hang stecken geblieben (Ziel 1200 hoeher). Kein Fortschritt
            // ueber 300 Ticks: Link auf den Zielboden versetzen (Kontrolle des Raumwechsels ohne Ritt, kein Szenenausloeser).
            if (c.frame % 300 == 0) {
                const float dx = link->current.pos.x - s.progressPos.x;
                const float dz = link->current.pos.z - s.progressPos.z;
                if (s.progressSet && dx * dx + dz * dz < 100.0f * 100.0f && s.firstChange == 0 && !s.moved) {
                    s.moved = true;
                    const cXyz to(s.target.x, s.target.y + 100.0f, s.target.z);
                    kff::logInfo("wp1: raum-ohne kein Fortschritt bei (%.0f,%.0f,%.0f), Link auf den Zielboden versetzt (%.0f,%.0f,%.0f)",
                        static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                        static_cast<double>(link->current.pos.z), static_cast<double>(to.x), static_cast<double>(to.y),
                        static_cast<double>(to.z));
                    link->current.pos = to;
                    link->old.pos = to;
                }
                s.progressPos = link->current.pos;
                s.progressSet = true;
            }
        }
        if (s.walking && (c.frame >= s.walkStart + 2400 || (s.firstChange != 0 && c.frame >= s.firstChange + 300))) {
            const bool ok = s.roomChanges > 0 && b.linkChanges == 0;
            kff::logInfo("wp1: RESULT raumwechsel-ohne %s Kontrolle ohne Ritt: Raumwechsel=%u (erster t=%u %d -> %d) Ziel Raum %d "
                         "linkWechsel=%u stage=%s birds=%u versetzt=%d",
                ok ? "ok" : "fail", s.roomChanges, s.firstChange, s.startRoom, s.firstNewRoom, s.targetRoom, b.linkChanges,
                stageName(), birds(), s.moved ? 1 : 0);
            return true;
        }
        return false;
    }
    daKago_c* k = kff::liveBird();
    if (b.rode && !b.released && k != nullptr && link != nullptr) {
        if (!s.searched && c.sess.rideFrames >= 5) {
            s.searched = true;
            s.startRoom = room;
            s.haveTarget = findOtherRoom(link->current.pos, room, &s.target, &s.targetRoom);
            if (!s.haveTarget) {
                kff::logInfo("wp1: RESULT raumwechsel fail kein Boden eines anderen Raums im Radius 40000");
                return true;
            }
            // f-raum2: Sollhoehe sofort 5500 ueber dem Vogel -> Dauerzug -0,7 -> Loopings auf der Stelle (kein Raumwechsel).
            // Daher Sollhoehe in Rampe (8 Einheiten je Tick) anheben.
            s.wantY = std::max(k->current.pos.y, s.target.y + 1500.0f);
            s.holdY = k->current.pos.y;
            s.lastY = k->current.pos.y;
            kff::logInfo("wp1: raum Ziel Raum %d bei (%.0f,%.0f,%.0f), Flughoehe %.0f (Rampe 8/Tick ab %.0f)", s.targetRoom,
                static_cast<double>(s.target.x), static_cast<double>(s.target.y), static_cast<double>(s.target.z),
                static_cast<double>(s.wantY), static_cast<double>(s.holdY));
        }
        if (s.haveTarget) {
            s.holdY = std::min(s.wantY, s.holdY + 8.0f);
            c.stick(steerTo(k, s.target.x, s.target.z), holdHeight(k, s.holdY, s.lastY));
            if (c.sess.rideFrames % 150 == 0) {
                const float dx = s.target.x - k->current.pos.x;
                const float dz = s.target.z - k->current.pos.z;
                kff::logInfo("wp1: raum Flug t=%u Vogel=(%.0f,%.0f,%.0f) Abstand=%.0f Sollhoehe=%.0f Raum=%d", c.frame,
                    static_cast<double>(k->current.pos.x), static_cast<double>(k->current.pos.y),
                    static_cast<double>(k->current.pos.z), static_cast<double>(std::sqrt(dx * dx + dz * dz)),
                    static_cast<double>(s.holdY), room);
            }
        }
        if (s.firstChange != 0) {
            const int lay = layerOf(k);
            if (lay != s.birdLayerAtChange) {
                ++s.layerBad;
            }
            const uint32_t n = birds();
            if (n > s.birdsMaxAfter) {
                s.birdsMaxAfter = n;
            }
        }
        // B 300 Ticks nach dem ersten Raumwechsel, spaetestens nach 2400 Ritt-Frames.
        if (!s.bDone && ((s.firstChange != 0 && c.frame >= s.firstChange + 300) || c.sess.rideFrames >= 2400)) {
            s.bDone = true;
            c.tapB();
            kff::logInfo("wp1: raum B t=%u Raumwechsel bisher %u", c.frame, s.roomChanges);
        }
    }
    if (b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + 600;
        }
        if (c.frame >= s.endAt) {
            // Pass: Raumwechsel unter dem Vogel und danach 300 Ticks Vogel-Layer gleich bei birds <= 1 (Ritt endet durch B),
            // oder Ritt-Ende mit Grund vor dem Neuladen (dann kein Vogel im alten Raum).
            const bool whyOk = b.whyKnown;
            const bool layerOk = s.firstChange != 0 && s.layerBad == 0 && b.why == kff::EndReason::DismountButton &&
                                 b.releaseTick >= s.firstChange + 300;
            const bool endBeforeReload = s.firstChange != 0 && b.releaseTick <= s.firstChange && b.why != kff::EndReason::DismountButton;
            const bool ok = s.roomChanges > 0 && b.maxBirds <= 1 && whyOk && (layerOk || endBeforeReload) && b.linkChanges == 0;
            kff::logInfo("wp1: RESULT raumwechsel %s Raumwechsel=%u (erster t=%u %d -> %d) Ziel Raum %d maxBirds=%u "
                         "Vogel-Layer bei Wechsel=%d abweichende Ticks=%u grund=%s ende t=%u ritt-frames=%u linkWechsel=%u "
                         "stage=%s",
                ok ? "ok" : "fail", s.roomChanges, s.firstChange, s.startRoom, s.firstNewRoom, s.targetRoom, b.maxBirds,
                s.birdLayerAtChange, s.layerBad, b.whyKnown ? kff::endReasonName(b.why) : "?", b.releaseTick,
                b.releaseRideFrame, b.linkChanges, stageName());
            return true;
        }
    }
    return false;
}

// WP1.7 mit KFS_OHNE_RITT=1: Kontrolle ohne Ritt (gleiche Strecke zu Fuss).
bool raumwechselTick(kff::autotest::Context& c) {
    static const bool ohne = [] {
        const char* e = std::getenv("KFS_OHNE_RITT");
        return e != nullptr && e[0] == '1';
    }();
    g_raum.ohneRitt = ohne;
    return raumTick(c);
}

// ------------------------------------------------------------------------- WP1.8 rueckkehr-raum-tag
struct RetRm {
    Base b;
    bool swept = false;
    bool created = false;
    fpc_ProcID tagId = kff::kNoProc;
    cXyz tagPos;
    s16 tagYaw = 0;
    float hx = 0.0f;
    float hy = 0.0f;
    float hz = 0.0f;
    uint32_t insideTicks = 0;
    uint32_t maxTimer = 0;
    bool bDone = false;
    uint32_t restartsAtRide = 0;
    uint32_t endAt = 0;
    float lastY = 0.0f;
    float holdY = 0.0f;
    bool inNow = false;       // Link in diesem Tick im Quader
    uint32_t birdsNot1 = 0;   // Ritt-Ticks mit birds != 1
    bool extLogged = false;
};
RetRm g_rr;

uint32_t restartCalls();

int findRetRmCb(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    fpc_ProcID* found = static_cast<fpc_ProcID*>(data);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_Tag_RetRoom_e && *found == kff::kNoProc) {
        *found = fopAcM_GetID(ac);
    }
    return 1;
}

bool retRmTick(kff::autotest::Context& c) {
    RetRm& s = g_rr;
    Base& b = s.b;
    b.callAt = 300;
    track(c, b, "retrm");
    if (c.frame >= kDeadline) {
        kff::logInfo("wp1: RESULT rueckkehr-raum-tag fail Zeitlimit");
        return true;
    }
    daKago_c* k = kff::liveBird();
    daAlink_c* link = kff::liveLink();
    if (b.rode && !b.released && k != nullptr && link != nullptr) {
        if (!s.swept) {
            s.swept = true;
            s.restartsAtRide = restartCalls();
            sweepExits(link->current.pos);
            s.lastY = k->current.pos.y;
            s.holdY = k->current.pos.y;
            fpc_ProcID found = kff::kNoProc;
            fopAcIt_Executor(findRetRmCb, &found);
            if (found == kff::kNoProc) {
                // Keine Rueckkehr-Raum-Markierung geladen: Testaufbau, ein echter daTagRetRm_c vor dem Vogel (Profil
                // Tag_RetRoom). Er prueft selbst, ob der Spieler im Quader ist, und loest dann forceRestartRoom aus.
                // Die Erzeugungsparameter speichern die Skala als u8 (Wert x 10, hoechstens 25,5): f-retrm1 mit z=60 ergab
                // nur 18 Ticks im Quader. Daher 25,5 x 25,5 (halb 1275) und Quader um 45 Grad gedreht: der Vogel fliegt
                // die Diagonale (rund 3600 Einheiten, ueber 60 Ticks bei Gang 3).
                const s16 yaw = k->current.angle.y;
                const float fx = cM_ssin(yaw);
                const float fz = cM_scos(yaw);
                const cXyz scale(25.5f, 10.0f, 25.5f);  // halb 1275 x 1275, Hoehe 1000
                s.tagPos = cXyz(k->current.pos.x + fx * 3400.0f, link->current.pos.y - 500.0f, k->current.pos.z + fz * 3400.0f);
                s.tagYaw = static_cast<s16>(yaw + 0x2000);
                const csXyz ang(0, s.tagYaw, 0);
                s.tagId = fopAcM_create(fpcNm_Tag_RetRoom_e, 0, &s.tagPos, dComIfGp_roomControl_getStayNo(), &ang, &scale, -1);
                s.created = true;
                kff::logInfo("wp1: retrm Testaufbau daTagRetRm_c id=0x%x pos=(%.0f,%.0f,%.0f) Flug-yaw=%d Quader-yaw=%d Skala (25.5,10,25.5)",
                    static_cast<unsigned>(s.tagId), static_cast<double>(s.tagPos.x), static_cast<double>(s.tagPos.y),
                    static_cast<double>(s.tagPos.z), static_cast<int>(yaw), static_cast<int>(s.tagYaw));
            } else {
                s.tagId = found;
                kff::logInfo("wp1: retrm Rueckkehr-Raum-Markierung im Spiel gefunden id=0x%x", static_cast<unsigned>(found));
            }
        }
        // Kurs halten, Hoehe halten: Link fliegt laengs durch den Quader.
        c.stick(0.0f, holdHeight(k, s.holdY, s.lastY));
        fopAc_ac_c* tagAc = nullptr;
        if (s.tagId != kff::kNoProc && fopAcM_SearchByID(s.tagId, &tagAc) != 0 && tagAc != nullptr) {
            daTagRetRm_c* tag = static_cast<daTagRetRm_c*>(tagAc);
            s.tagPos = tag->current.pos;
            s.hx = tag->field_0x56c;
            s.hy = tag->field_0x570;
            s.hz = tag->field_0x574;
            if (!s.extLogged && s.hx > 0.0f) {
                s.extLogged = true;
                kff::logInfo("wp1: retrm Quader aus dem Tag: halb x=%.0f z=%.0f Hoehe %.0f yaw=%d", static_cast<double>(s.hx),
                    static_cast<double>(s.hz), static_cast<double>(s.hy), static_cast<int>(tag->current.angle.y));
            }
            if (static_cast<uint32_t>(tag->field_0x578) > s.maxTimer) {
                s.maxTimer = static_cast<uint32_t>(tag->field_0x578);
            }
            const float dx = link->current.pos.x - tag->current.pos.x;
            const float dz = link->current.pos.z - tag->current.pos.z;
            const float sn = cM_ssin(tag->current.angle.y);
            const float cs = cM_scos(tag->current.angle.y);
            const float lx = cs * dx - sn * dz;
            const float lz = sn * dx + cs * dz;
            const float ly = link->current.pos.y - tag->current.pos.y;
            const bool in = ly > 0.0f && ly < s.hy && std::fabs(lx) < s.hx && std::fabs(lz) < s.hz;
            s.inNow = in;
            if (in) {
                ++s.insideTicks;
                if (s.insideTicks == 1 || s.insideTicks % 30 == 0) {
                    kff::logInfo("wp1: retrm Link im Quader t=%u Ticks=%u timer=%d birds=%u", c.frame, s.insideTicks,
                        static_cast<int>(tag->field_0x578), birds());
                }
            }
        }
        if (birds() != 1) {
            ++s.birdsNot1;
        }
        // B erst nach dem Durchflug (mindestens 60 Ticks im Quader, Link wieder draussen): sonst landet Link im Quader und
        // der Vanilla-Rueckkehr-Raum loest nach dem Ende aus (das ist erlaubt, prueft aber nicht den Ritt).
        if (!s.bDone && ((s.insideTicks >= 60 && !s.inNow) || c.sess.rideFrames >= 1500)) {
            s.bDone = true;
            c.tapB();
            kff::logInfo("wp1: retrm B t=%u Ticks im Quader %u", c.frame, s.insideTicks);
        }
    }
    if (b.released) {
        if (s.endAt == 0) {
            s.endAt = b.releaseTick + 300;
        }
        if (c.frame >= s.endAt) {
            const uint32_t restarts = restartCalls() - s.restartsAtRide;
            const bool whyOk = b.whyKnown && b.why == kff::EndReason::DismountButton;
            const bool birdsOk = b.maxBirds <= 1 && s.birdsNot1 == 0;
            const bool ok = s.insideTicks >= 60 && restarts == 0 && whyOk && b.linkChangesAtEnd == 0 && birdsOk;
            kff::logInfo("wp1: RESULT rueckkehr-raum-tag %s Ticks im Quader=%u startRestartRoom im Ritt=%u timerMax=%u grund=%s "
                         "ritt-frames=%u linkWechselImRitt=%u linkWechsel=%u birds!=1 im Ritt=%u maxBirds=%u "
                         "retRm uebersprungen=%u testaufbau=%d stage=%s",
                ok ? "ok" : "fail", s.insideTicks, restarts, s.maxTimer, b.whyKnown ? kff::endReasonName(b.why) : "?",
                b.releaseRideFrame, b.linkChangesAtEnd, b.linkChanges, s.birdsNot1, b.maxBirds, g_retRmSkips,
                s.created ? 1 : 0, stageName());
            if (s.created && s.tagId != kff::kNoProc) {
                fopAcM_delete(s.tagId);  // Testaufbau wieder entfernen
            }
            return true;
        }
    }
    return false;
}

}  // namespace wp1

const kff::autotest::Scenario kScenWp1Ladezone = {"ladezone-anflug",
    "WP1.1: F_SP121, Ausgang (SCENE_EXIT) per Sweep, Anflug mit Autopilot, B bei Tick 1500, danach zu Fuss",
    wp1::ladezoneTick};
const kff::autotest::Scenario kScenWp1Event39 = {"event39-kanal",
    "WP1.2: R_SP107, Ruf bei Tick 60, Ereignis im Ritt (Ritt-Frame ~211), Ende vor dem Link-Wechsel", wp1::event39Tick};
const kff::autotest::Scenario kScenWp1Boden = {"boden-absteigen",
    "WP1.3: F_SP121, 421 Ritt-Frames, B ueber Boden, Landeanflug, Fall-Referenz, field_0x6de", wp1::bodenTick};
const kff::autotest::Scenario kScenWp1Void = {"void-absteigen",
    "WP1.4: F_SP121, 150 Ticks steigen, Vogel um KFS_VOID_DIST (30000) an den Abgrund, B nach 2 Ticks, 1200 Ticks", wp1::voidTick};
const kff::autotest::Scenario kScenWp1Abgrund = {"abgrund-faelle",
    "WP1.5: F_SP121, KFF_WP1_FALL a (Abgrund, Leben 12), b (Abgrund, Leben 4), c (Boden 0x60), d (B aus 5200 ueber Boden); "
    "KFS_OHNE_RITT=1: a/b zu Fuss (Kontrolle)",
    wp1::abgrundFaelleTick};
const kff::autotest::Scenario kScenWp1Warp = {"warp-portal",
    "WP1.6: F_SP121, Ritt, Warp ueber den Spielweg (freigeschaltetes Portal aus den Kartendaten, Status 3 wie nach der "
    "Kartenwahl); KFS_OHNE_RITT=1: gleicher Warp zu Fuss (Kontrolle)", wp1::warpTick};
const kff::autotest::Scenario kScenWp1Raum = {"raumwechsel",
    "WP1.7: F_SP121, Geradeausflug nach Westen ueber Raumgrenzen, Vogelzahl und Layer (KFS_OHNE_RITT=1: Kontrolle zu Fuss)",
    wp1::raumwechselTick};
const kff::autotest::Scenario kScenWp1RetRm = {"rueckkehr-raum-tag",
    "WP1.8: F_SP121, Flug durch einen daTagRetRm_c-Quader (Sweep, sonst Testaufbau), 60+ Ticks im Volumen", wp1::retRmTick};

// WP1-Zaehler fuer die Szenarien: Zeilen "positional scene veto" und startRestartRoom-Aufrufe im Ritt (Protokoll oben).
namespace wp1 {
uint32_t vetoTotal() {
    return g_vetoLines;
}
uint32_t restartCalls() {
    return g_restartInRide;
}
}  // namespace wp1

// ---------------------------------------------------------------------------------------------
// WP6-Szenarien (nur Testbuild). Eingaben nur ueber Context (Pad-Injektion). Messzeilen "freiheit: WP6.<n> ...".
// Positionswechsel von Link (Teleport) sind nur Aufbau: Decke, Wasser und Fall wirken danach ueber das Spiel selbst.
// ---------------------------------------------------------------------------------------------

constexpr uint32_t kWp6FriendlyParam = 0x000100FFu;  // eigener Vogel (Kern, summon.cpp)

int wp6CountBird(void* actor, void* data) {
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(actor);
    if (ac != nullptr && fopAcM_GetName(ac) == fpcNm_KAGO_e && fopAcM_GetParam(ac) == kWp6FriendlyParam) {
        ++*static_cast<uint32_t*>(data);
    }
    return 1;
}

// Eigene Voegel in der Welt (Parameter 0x000100FF, mit oder ohne Sitzung).
uint32_t wp6Birds() {
    uint32_t n = 0;
    fopAcIt_Executor(wp6CountBird, &n);
    return n;
}

float wp6RoofAbove(const cXyz& p) {
    dBgS_RoofChk rc;
    const cXyz q(p.x, p.y - 500.0f, p.z);
    rc.SetPos(q);
    return dComIfG_Bgsp().RoofChk(&rc);
}

// Ruf-Versuch in einem Tick: R gehalten und B-Flanke (wie die Ruf-Kombination des Kerns).
void wp6Chord(Context& c) {
    c.holdR();
    c.tapB();
}

void wp6Teleport(daAlink_c* link, const cXyz& p) {
    link->current.pos = p;
    link->old.pos = p;
    link->speed.set(0.0f, 0.0f, 0.0f);
    link->speedF = 0.0f;
}

// Zustand von Link und Sitzung bei einem Ruf-Versuch (fuer die Gate-Pruefung 6.11/6.12).
void wp6LogAttempt(const char* tag, Context& c) {
    daAlink_c* link = kff::liveLink();
    kff::logInfo("freiheit: %s Versuch: voegel=%u phase=%s proc=%d RStatus=%u pause=%d ereignis=%d schwimmt=%d boden=%d "
                 "zielsperre=%d frame=%u",
        tag, wp6Birds(), kff::phaseName(c.sess.phase), link != nullptr ? static_cast<int>(link->mProcID) : -1,
        static_cast<unsigned>(dComIfGp_getRStatus()), dComIfGp_isPauseFlag() != 0 ? 1 : 0, inEvent() ? 1 : 0,
        link != nullptr && link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0 ? 1 : 0,
        link != nullptr && link->mLinkAcch.ChkGroundHit() ? 1 : 0,
        link != nullptr && link->mAttention != nullptr && link->checkAttentionLock() ? 1 : 0, c.frame);
}

// Nicksteuerung der Szenarien: Nase etwa auf target (s16, negativ = hoch) halten. Ein fester Stick Y dreht den Vogel im
// freien Flugmodell weiter (Looping, Lauf wp6p-decke-1: Kopfueber ab Ritt-Frame 60), darum regelt der Pilot die Nase.
float wp6PitchStick(const daKago_c* k, int target) {
    const int pitch = static_cast<int>(k->current.angle.x);
    if (pitch > target + 0x400) {
        return -0.5f;  // Nase hoch
    }
    if (pitch < target - 0x400) {
        return 0.5f;
    }
    return 0.0f;
}

// ---- wp6-decke (R_SP107, Zelle, echte Decke y 700) ----
// 6.2: Steigen (Nase etwa 22 Grad hoch, geregelt) bis y >= 550 (Vanilla-Grenze 400 plus 150) innerhalb von 300
//      Ritt-Ticks, CrrPos-Laeufe 0.
// 6.1: Dash unter der Decke: A alle 20 Ticks, Stick X wechselnd +-0,8 alle 30 Ticks, Stick Y aus der Nickregelung
//      (Nase hoch gegen die Decke), 120 Ticks. Danach |y - yZiel| <= 50 mit yZiel = echte Decke - 92 (Abtastpunkt oben
//      80 plus kSkin 12), Durchdringungen 0, DACH-Log in allen gesehenen Aktionen und Modi kNoRoof. 6.4: B,
//      Landeanflug, Link am Boden unter der Decke; startRestartRoom im Ritt nie ausgefuehrt, checkRoofRestart laeuft nach
//      dem Ritt wieder (Vanilla).
struct Wp6Decke {
    Call call;
    bool rode = false;
    uint32_t y550 = 0;
    float yMax = -1.0e9f;
    float yTarget = 0.0f;
    uint32_t dashTaps = 0;
    uint32_t penBase = 0;
    bool dashDone = false;
    bool bDone = false;
    uint32_t endFrame = 0;
    float dashMin = 1.0e9f;      // y im Dash-Fenster
    float dashMax = -1.0e9f;
    double dashSum = 0.0;
    uint32_t dashFrames = 0;
    uint32_t dashNear = 0;       // Dash-Frames mit |y - yZiel| <= 50
};
Wp6Decke g_wd;
constexpr uint32_t kWdDashStart = 100;
constexpr uint32_t kWdDashEnd = 220;  // 120 Dash-Ticks (Vorgabe 6.1)
constexpr uint32_t kWdB = 230;
constexpr int kWdPitch = -0x1000;     // Nase etwa 22 Grad hoch

bool wp6DeckeTick(Context& c) {
    Wp6Decke& w = g_wd;
    if (c.frame >= kScenarioDeadline) {
        c.note("wp6-decke: Zeitlimit erreicht");
        return true;
    }
    advanceEvent(c);
    if (c.sess.phase == kff::Phase::Riding) {
        w.rode = true;
        const uint32_t f = c.sess.rideFrames;
        daKago_c* k = kff::liveBird();
        if (k == nullptr) {
            return false;
        }
        const cXyz pos = k->current.pos;
        w.yMax = std::max(w.yMax, pos.y);
        if (w.y550 == 0 && pos.y >= 550.0f) {
            w.y550 = f;
            kff::logInfo("freiheit: WP6.2 y>=550 bei Ritt-Frame %u (y=%.1f; Vanilla-Grenze 400 + 150), CrrPos-Laeufe %u",
                f, static_cast<double>(pos.y), g_wp6.crrPosRuns);
        }
        float x = 0.0f;
        float y = wp6PitchStick(k, kWdPitch);
        if (f == kWdDashStart) {
            // 6.2 hier auswerten (der Ritt endet mit B vor Ritt-Frame 300).
            kff::logInfo("freiheit: WP6.2 Ergebnis: y550Frame=%u yMax=%.1f crrPosRuns=%u crrSkipFrames=%u (Soll: Frame <= 300, "
                         "CrrPos 0)", w.y550, static_cast<double>(w.yMax), g_wp6.crrPosRuns, g_wp6.crrSkipFrames);
            const float roof = wp6RoofAbove(pos);
            w.yTarget = roof != G_CM3D_F_INF ? roof - kVertR - kSkin : pos.y;
            w.penBase = g_stats.penetration;
            kff::logInfo("freiheit: WP6.1 Dash-Phase Start f=%u y=%.1f echteDecke=%.1f yZiel=%.1f", f,
                static_cast<double>(pos.y), static_cast<double>(roof != G_CM3D_F_INF ? roof : -1.0f),
                static_cast<double>(w.yTarget));
            c.note("wp6-decke: Dash unter der Decke (A alle 20 Ticks, Stick X wechselnd, Nase hoch geregelt)");
        }
        if (f >= kWdDashStart && f < kWdDashEnd) {
            x = ((f - kWdDashStart) / 30) % 2 == 0 ? 0.8f : -0.8f;
            if ((f - kWdDashStart) % 20 == 0) {
                c.tapA();
                ++w.dashTaps;
            }
            w.dashMin = std::min(w.dashMin, pos.y);
            w.dashMax = std::max(w.dashMax, pos.y);
            w.dashSum += static_cast<double>(pos.y);
            ++w.dashFrames;
            if (std::fabs(pos.y - w.yTarget) <= 50.0f) {
                ++w.dashNear;
            }
            if (f == kWdDashStart + 60) {
                photo("decke-dash-mitte", f);
            }
        }
        if (f >= kWdDashEnd && !w.dashDone) {
            w.dashDone = true;
            const float roofHere = wp6RoofAbove(pos);
            const float targetHere = roofHere != G_CM3D_F_INF ? roofHere - kVertR - kSkin : w.yTarget;
            kff::logInfo("freiheit: WP6.1 Ergebnis nach %u Dash-Ticks: y=%.1f yZiel=%.1f diff=%.1f yZielHier=%.1f "
                         "diffHier=%.1f dashTaps=%u durchdringungen=%u roofNotNone=%u roofSkip=%u kNoRoofAktionen=0x%x "
                         "kNoRoofModi=0x%x (Soll |diff| <= 50)",
                kWdDashEnd - kWdDashStart, static_cast<double>(pos.y), static_cast<double>(w.yTarget),
                static_cast<double>(pos.y - w.yTarget), static_cast<double>(targetHere),
                static_cast<double>(pos.y - targetHere), w.dashTaps, g_stats.penetration - w.penBase,
                g_wp6.roofNotNone, g_wp6.roofSkip, g_wp6.roofActions, g_wp6.roofModes);
            kff::logInfo("freiheit: WP6.1 Dash-Fenster: yMin=%.1f yMax=%.1f yMittel=%.1f Frames=%u davon |y-yZiel|<=50: %u",
                static_cast<double>(w.dashMin), static_cast<double>(w.dashMax),
                w.dashFrames > 0 ? w.dashSum / static_cast<double>(w.dashFrames) : 0.0, w.dashFrames, w.dashNear);
            photo("decke-dash", f);
        }
        if (f >= kWdDashEnd) {
            y = 0.0f;
        }
        if (f >= kWdB && !w.bDone) {
            w.bDone = true;
            c.tapB();
            c.note("wp6-decke: B (Absteigen unter der Decke)");
            return false;
        }
        c.stick(x, y);
        return false;
    }
    if (inEvent()) {
        return false;
    }
    if (!w.rode) {
        runCall(c, w.call);
        return false;
    }
    if (w.endFrame == 0) {
        w.endFrame = c.frame;
        c.note("wp6-decke: Ritt beendet, Link unter der Decke");
    }
    if (c.frame == w.endFrame + 240) {
        daAlink_c* link = kff::liveLink();
        kff::logInfo("freiheit: WP6.4 Ergebnis: startRestartRoom im Ritt aufgerufen=%u abgewiesen=%u durchgelassen=%u, "
                     "checkRoofRestart im Ritt abgewiesen=%u, nach dem Ritt aufgerufen=%u, startRestartRoom nach dem Ritt=%u, "
                     "Link y=%.1f Boden=%d Decke ueber Link=%.1f",
            g_wp6.restartCalls, g_wp6.restartVeto, g_wp6.restartPassed, g_stats.roofRestartVeto,
            g_wp6.roofRestartOutside, g_wp6.restartOutside,
            static_cast<double>(link != nullptr ? link->current.pos.y : 0.0f),
            link != nullptr && link->mLinkAcch.ChkGroundHit() ? 1 : 0,
            static_cast<double>(link != nullptr ? wp6RoofAbove(link->current.pos) : -1.0f));
        logMess("wp6-decke");
        c.note("autotest: timeline complete");
        return true;
    }
    return false;
}

// ---- wp6-wand (F_SP121): 6.5 Haengen an der Bergwand, 6.7 Kartenwand-Probe und Tunnel-Probe ----
// Anflug mit Kurs -X (Vorgabe, Neuausrichtung alle 30 Ticks), Gang 5, Stick neutral (Lauf wp6p-wand-1 flog schraeg auf
// (-81509, -23950) und blieb an einer Kartenwand bei x -70590 haengen). An der Wand (in 30 Ticks weniger als 600 nach -X
// mit Wandkontakt), bei x <= -81509 oder nach Zeitlimit: Kurs -X und 800 Ritt-Ticks Stick neutral (Haengen): Gleiten,
// Stillstand, Waechter. Endet der Ritt durch den
// Waechter (Grund Stuck), ruft das Szenario neu (hoechstens 4 Rufe, Link am Boden) und das Haengen laeuft weiter (die
// Ticks zaehlen ueber die Ritte). Beim ersten Wandkontakt im Haengen: Wandprofil (Linien in Kursrichtung von 0 bis
// 50000 ueber dem Kontakt, Flaechen-Attribute, Boden vor und hinter der Wand). Danach Tunnel-Probe: Zusatzweg 300, dann
// 600 Einheiten je Tick in Kursrichtung (je 150 Ticks, Stick X 0,05, Kurs alle 30 Ticks auf den Kontaktpunkt).
// Soll: Durchdringungen 0, skipFrames 0, Stillstand ohne Stick hoechstens 300 Ticks, sonst Waechter-Ende mit Grund.
enum class WandStep : uint8_t { Approach, Hang, Tunnel300, Tunnel600, Done };
struct Wp6Wand {
    Call call;
    uint32_t calls = 0;
    WandStep step = WandStep::Approach;
    uint32_t stepTicks = 0;    // Ritt-Ticks im aktuellen Schritt, ueber Ritte hinweg
    bool rideSeen = false;     // Ritt-Start dieses Ritts schon verarbeitet
    uint32_t rides = 0;
    uint32_t wallBase = 0;
    uint32_t glideBase = 0;
    uint32_t penBase = 0;
    uint32_t fixBase = 0;
    uint32_t stuckBase = 0;
    bool firstContact = false;
    bool contact = false;      // Wandkontakt im Haengen (Ziel der Tunnel-Probe)
    cXyz contactPos;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    uint32_t endFrame = 0;
    bool bDone = false;
    float approachX = 0.0f;        // x beim letzten 30-Tick-Schritt des Anflugs
    bool approachContact = false;  // Wandkontakt seit dem letzten 30-Tick-Schritt
    uint32_t stuckPhotos = 0;      // Waechter-Enden mit Foto (Hinweis im Spiel, 4 s sichtbar)
};
Wp6Wand g_ww;
constexpr float kWwTargetX = -81509.0f;  // Anflug endet spaetestens hier (Bergwand laut Vorbefund bei x -81410)
constexpr uint32_t kWwApproachMax = 2400;
constexpr uint32_t kWwHangTicks = 800;
constexpr uint32_t kWwTunnelTicks = 150;
constexpr uint32_t kWwMaxCalls = 4;

void wp6WallProfile(const cXyz& at, s16 yaw, const char* tag) {
    static constexpr float kDy[] = {0.0f, 250.0f, 500.0f, 1000.0f, 1500.0f, 2000.0f, 3000.0f, 4000.0f, 6000.0f, 8000.0f,
                                    12000.0f, 16000.0f, 24000.0f, 32000.0f, 50000.0f};
    constexpr int kN = static_cast<int>(sizeof(kDy) / sizeof(kDy[0]));
    char row[kN + 1] = {};
    const float hx = cM_ssin(yaw);
    const float hz = cM_scos(yaw);
    int att0 = -1;
    int wallCode = -1;
    int special = -1;
    int roomId = -1;
    int moveBg = -1;
    float crossX = 0.0f;
    float crossZ = 0.0f;
    for (int i = 0; i < kN; ++i) {
        const cXyz s(at.x - hx * 300.0f, at.y + kDy[i], at.z - hz * 300.0f);
        const cXyz e(at.x + hx * 600.0f, at.y + kDy[i], at.z + hz * 600.0f);
        dBgS_LinChk lin;
        lin.Set(&s, &e, nullptr);
        const bool hit = dComIfG_Bgsp().LineCross(&lin);
        row[i] = hit ? '1' : '0';
        if (hit && i == 0) {
            att0 = dComIfG_Bgsp().GetPolyAtt0(lin);
            wallCode = dComIfG_Bgsp().GetWallCode(lin);
            special = dComIfG_Bgsp().GetSpecialCode(lin);
            roomId = dComIfG_Bgsp().GetRoomId(lin);
            moveBg = dComIfG_Bgsp().ChkMoveBG(lin) ? 1 : 0;
            crossX = lin.GetCross().x;
            crossZ = lin.GetCross().z;
        }
    }
    const cXyz behind(at.x + hx * 800.0f, at.y + 60000.0f, at.z + hz * 800.0f);
    dBgS_GndChk g;
    g.SetPos(&behind);
    const float gb = dComIfG_Bgsp().GroundCross(&g);
    const cXyz front(at.x - hx * 400.0f, at.y + 60000.0f, at.z - hz * 400.0f);
    dBgS_GndChk g2;
    g2.SetPos(&front);
    const float gf = dComIfG_Bgsp().GroundCross(&g2);
    kff::logInfo("freiheit: WP6.7 Wandprofil (%s) bei (%.0f,%.0f,%.0f) Kurs %d: Treffer je Hoehe ueber dem Kontakt "
                 "(0,250,500,1k,1.5k,2k,3k,4k,6k,8k,12k,16k,24k,32k,50k) %s; Flaeche att0=%d wallCode=%d special=%d raum=%d "
                 "bewegt=%d Treffer bei (%.0f,%.0f); oberste Flaeche vor der Wand %.1f, 800 hinter der Wand %.1f",
        tag, static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z), static_cast<int>(yaw), row,
        att0, wallCode, special, roomId, moveBg, static_cast<double>(crossX), static_cast<double>(crossZ),
        static_cast<double>(gf == -G_CM3D_F_INF ? -1.0e9f : gf), static_cast<double>(gb == -G_CM3D_F_INF ? -1.0e9f : gb));
}

void wp6WandAim(daKago_c* k, float tx, float tz) {
    k->current.angle.y = cM_atan2s(tx - k->current.pos.x, tz - k->current.pos.z);
    k->shape_angle.y = k->current.angle.y;
}

void wp6WandReport(const Wp6Wand& w) {
    kff::logInfo("freiheit: WP6.5/6.7 Ende: Rufe=%u Ritte=%u stuckEnds=%u stuckRunMax=%u skipFrames=%u tunnelFrames=%u "
                 "durchdringungen=%u durchflugKorr=%u gleitFrames=%u",
        w.calls, w.rides, g_wp6.stuckEnds, g_wp6.stuckRunMax, g_wp6.skipFrames, g_wp6.tunnelFrames, g_stats.penetration,
        g_stats.penetrationFix, g_stats.glideFrames);
    logMess("wp6-wand");
}

bool wp6WandTick(Context& c) {
    Wp6Wand& w = g_ww;
    if (c.frame >= kScenarioDeadline) {
        g_tunnelExtra = 0.0f;
        wp6WandReport(w);
        c.note("wp6-wand: Zeitlimit erreicht");
        return true;
    }
    advanceEvent(c);
    if (c.sess.phase != kff::Phase::Riding) {
        g_tunnelExtra = 0.0f;
        w.rideSeen = false;
        if (g_wp6.stuckEnds > w.stuckPhotos) {
            w.stuckPhotos = g_wp6.stuckEnds;  // erster Tick nach dem Waechter-Ende: Hinweis im Spiel ins Bild
            photo("wp6-stuck-hinweis", c.frame);
        }
        if (w.step == WandStep::Done) {
            if (w.endFrame == 0) {
                w.endFrame = c.frame;
            }
            if (c.frame >= w.endFrame + 150 && !inEvent()) {
                wp6WandReport(w);
                c.note("autotest: timeline complete");
                return true;
            }
            return false;
        }
        if (inEvent()) {
            return false;
        }
        // Erster Ruf, oder neuer Ruf nach einem Ritt-Ende durch den Waechter (Stuck): Link am Boden, kein Vogel.
        daAlink_c* link = kff::liveLink();
        if (w.call.armed && c.frame > w.call.start + kHold + 300) {
            w.call = Call();
        }
        if (!w.call.armed && c.frame >= kCallAt && c.canCall() && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            if (w.calls >= kWwMaxCalls) {
                kff::logInfo("freiheit: WP6.5 keine weiteren Rufe (%u), Schritt %d bleibt unvollstaendig", w.calls,
                    static_cast<int>(w.step));
                w.step = WandStep::Done;
                return false;
            }
            ++w.calls;
            w.call.armed = true;
            w.call.pressed = false;
            w.call.start = c.frame;
            c.note(w.calls == 1 ? "wp6-wand: Ruf" : "wp6-wand: neuer Ruf nach dem Ritt-Ende");
        }
        if (w.call.armed && c.frame < w.call.start + kHold) {
            c.holdR();
        }
        if (w.call.armed && !w.call.pressed && c.frame == w.call.start + kPress) {
            w.call.pressed = true;
            c.tapB();
        }
        return false;
    }
    const uint32_t f = c.sess.rideFrames;
    daKago_c* k = kff::liveBird();
    if (k == nullptr) {
        return false;
    }
    const cXyz pos = k->current.pos;
    if (!w.rideSeen) {
        w.rideSeen = true;
        ++w.rides;
        w.wallBase = g_stats.wallContacts;
        w.approachX = pos.x;
        w.approachContact = false;
        if (w.step == WandStep::Approach) {
            k->current.angle.y = static_cast<s16>(-0x4000);
            k->shape_angle.y = k->current.angle.y;
        } else if (w.contact) {
            wp6WandAim(k, w.contactPos.x, w.contactPos.z);
        } else {
            k->current.angle.y = static_cast<s16>(-0x4000);
            k->shape_angle.y = k->current.angle.y;
        }
        kff::logInfo("freiheit: WP6.5 Ritt %u beginnt (Ruf %u, Schritt %d, Schritt-Ticks %u) pos=(%.0f,%.0f,%.0f) Kurs %d",
            w.rides, w.calls, static_cast<int>(w.step), w.stepTicks, static_cast<double>(pos.x), static_cast<double>(pos.y),
            static_cast<double>(pos.z), static_cast<int>(k->current.angle.y));
    }
    if (f == 5 || f == 15) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));  // Gang 3 -> 5
    }
    const uint32_t in = w.stepTicks++;
    const bool newContact = g_stats.wallContacts > w.wallBase;
    w.wallBase = g_stats.wallContacts;
    if (newContact && !w.firstContact) {
        w.firstContact = true;
        kff::logInfo("freiheit: WP6.7 erster Wandkontakt Ritt-Frame %u pos=(%.0f,%.0f,%.0f) Kurs %d (Schritt %d)", f,
            static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z),
            static_cast<int>(k->current.angle.y), static_cast<int>(w.step));
        wp6WallProfile(pos, k->current.angle.y, "erster Kontakt");
    }
    float sx = 0.0f;
    switch (w.step) {
    case WandStep::Approach: {
        // Kurs -X (Vorgabe 6.5), alle 30 Ticks neu gesetzt. Die Wand ist erreicht, wenn der Vogel in 30 Ticks weniger als
        // 600 nach -X kam und in dieser Zeit eine Wand beruehrte (Gelaendekanten im Feld bremsen ihn nicht so lange).
        if (newContact) {
            w.approachContact = true;
        }
        bool blocked = false;
        if (in % 30 == 0) {
            if (in > 0) {
                blocked = w.approachContact && w.approachX - pos.x < 600.0f;
            }
            w.approachX = pos.x;
            w.approachContact = false;
            k->current.angle.y = static_cast<s16>(-0x4000);
            k->shape_angle.y = k->current.angle.y;
        }
        if (in % 300 == 0) {
            kff::logInfo("freiheit: WP6.5 Anflug t=%u pos=(%.0f,%.0f,%.0f) Kurs -X", in, static_cast<double>(pos.x),
                static_cast<double>(pos.y), static_cast<double>(pos.z));
        }
        const bool nearTarget = blocked || pos.x <= kWwTargetX;
        if (nearTarget || in + 1 >= kWwApproachMax) {
            w.step = WandStep::Hang;
            w.stepTicks = 0;
            w.glideBase = g_stats.glideFrames;
            w.penBase = g_stats.penetration;
            w.fixBase = g_stats.penetrationFix;
            w.stuckBase = g_wp6.stuckEnds;
            w.minX = w.maxX = pos.x;
            w.minZ = w.maxZ = pos.z;
            k->current.angle.y = static_cast<s16>(-0x4000);  // Kurs -X gegen die Wand
            k->shape_angle.y = k->current.angle.y;
            kff::logInfo("freiheit: WP6.5 Anflug Ende (%s) Ritt-Frame %u pos=(%.0f,%.0f,%.0f), Haengen %u Ticks Kurs -X, "
                         "Stick neutral",
                nearTarget ? "Ziel erreicht" : "Zeitlimit", f, static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z), kWwHangTicks);
            c.note("wp6-wand: Haengen an der Wand (Stick neutral)");
        }
        break;
    }
    case WandStep::Hang:
        w.minX = std::min(w.minX, pos.x);
        w.maxX = std::max(w.maxX, pos.x);
        w.minZ = std::min(w.minZ, pos.z);
        w.maxZ = std::max(w.maxZ, pos.z);
        if (newContact && !w.contact) {
            w.contact = true;
            w.contactPos = pos;
            kff::logInfo("freiheit: WP6.5 Wandkontakt im Haengen t=%u pos=(%.0f,%.0f,%.0f)", in, static_cast<double>(pos.x),
                static_cast<double>(pos.y), static_cast<double>(pos.z));
            wp6WallProfile(pos, k->current.angle.y, "Haengen");
            photo("wp6-wand", f);
        }
        if (in % 100 == 0) {
            kff::logInfo("freiheit: WP6.5 Haengen t=%u pos=(%.0f,%.0f,%.0f) stuckTicks=%u stuckRunMax=%u gleiten=%u",
                in, static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z), g_stuckTicks,
                g_wp6.stuckRunMax, g_stats.glideFrames - w.glideBase);
        }
        if (in == kWwHangTicks / 2) {
            photo("wp6-haengen", f);
        }
        if (in + 1 >= kWwHangTicks) {
            kff::logInfo("freiheit: WP6.5 Ergebnis: %u Ticks Haengen, gleitFrames=%u, Weg x %.0f..%.0f z %.0f..%.0f, "
                         "stuckRunMax=%u stuckEnds=%u (im Haengen %u) durchdringungen=%u durchflugKorr=%u Ritte=%u",
                kWwHangTicks, g_stats.glideFrames - w.glideBase, static_cast<double>(w.minX), static_cast<double>(w.maxX),
                static_cast<double>(w.minZ), static_cast<double>(w.maxZ), g_wp6.stuckRunMax, g_wp6.stuckEnds,
                g_wp6.stuckEnds - w.stuckBase, g_stats.penetration - w.penBase, g_stats.penetrationFix - w.fixBase,
                w.rides);
            if (!w.contact && w.firstContact) {
                w.contact = true;
                w.contactPos = pos;  // kein Kontakt im Haengen: Tunnel-Probe in Kursrichtung von hier
            }
            w.step = w.contact ? WandStep::Tunnel300 : WandStep::Done;
            w.stepTicks = 0;
            if (w.contact) {
                g_tunnelExtra = 300.0f;
                w.penBase = g_stats.penetration;
                w.fixBase = g_stats.penetrationFix;
                wp6WandAim(k, w.contactPos.x - 2000.0f, w.contactPos.z);
                c.note("wp6-wand: Tunnel-Probe +300 Einheiten je Tick");
            }
        }
        break;
    case WandStep::Tunnel300:
    case WandStep::Tunnel600:
        sx = 0.05f;  // kleine Stick-Eingabe: der Waechter bleibt still, der Kurs dreht kaum
        if (in % 30 == 0) {
            wp6WandAim(k, w.contactPos.x - 2000.0f, w.contactPos.z);  // Ziel hinter der Wand (Kurs -X durch den Kontakt)
        }
        if (in + 1 >= kWwTunnelTicks) {
            kff::logInfo("freiheit: WP6.7 Tunnel %.0f Ergebnis: %u Ticks, pos=(%.0f,%.0f,%.0f), Kontakt x=%.0f, "
                         "durchdringungen=%u durchflugKorr=%u tunnelFrames=%u skipFrames=%u",
                static_cast<double>(g_tunnelExtra), kWwTunnelTicks, static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z), static_cast<double>(w.contactPos.x), g_stats.penetration - w.penBase,
                g_stats.penetrationFix - w.fixBase, g_wp6.tunnelFrames, g_wp6.skipFrames);
            w.penBase = g_stats.penetration;
            w.fixBase = g_stats.penetrationFix;
            w.stepTicks = 0;
            if (w.step == WandStep::Tunnel300) {
                w.step = WandStep::Tunnel600;
                g_tunnelExtra = 600.0f;
                c.note("wp6-wand: Tunnel-Probe +600 Einheiten je Tick");
            } else {
                g_tunnelExtra = 0.0f;
                w.step = WandStep::Done;
            }
        }
        break;
    case WandStep::Done:
        if (!w.bDone) {
            w.bDone = true;
            c.tapB();
            c.note("wp6-wand: B (Absteigen)");
        }
        break;
    }
    c.stick(sx, 0.0f);
    return false;
}

// ---- wp6-rand (F_SP121): 6.6 Kartenrand nach -X bis x <= -289170 in Gang 5 mit Turbo, 6.8 Hoehe ueber 100000 ----
// Erst Steigen auf etwa 5000 ueber dem Start (Nicksteuerung), dann waagerecht Kurs -X, Turbo (A) alle 120 Ticks. Je Tick:
// eigene Voegel = 1 (Soll), Szene und Raum unveraendert. Nach x <= -289170: Steigen mit Nase etwa 45 Grad hoch bis
// y > 100000, danach 60 Ticks waagerecht stellen und 120 Ticks Stick neutral (Schwingung: Vorzeichenwechsel der
// Hoehenaenderung zaehlen).
enum class RandStep : uint8_t { Climb, West, Up, Hold, Done };
struct Wp6Rand {
    Call call;
    RandStep step = RandStep::Climb;
    uint32_t stepFrame = 0;
    bool started = false;
    float y0 = 0.0f;
    uint32_t birdsMax = 0;
    uint32_t birdsMin = 99;
    uint32_t samples = 0;
    float edgeX = 0.0f;
    bool edgeLogged = false;
    float lastY = 0.0f;
    int lastSign = 0;
    uint32_t signChanges = 0;
    float holdMin = 1.0e9f;
    float holdMax = -1.0e9f;
    int roomNo = -1;
    uint32_t penBase = 0;
    bool reported = false;
    float westX = 0.0f;         // x beim letzten 30-Tick-Schritt nach -X
    bool westContact = false;   // Wandkontakt seit dem letzten 30-Tick-Schritt
    uint32_t wallBase = 0;
    uint32_t climbLeft = 0;     // Ticks Steigen ueber ein Hindernis (nach einem Halt)
    uint32_t halts = 0;         // Halte nach -X (T1: dokumentieren)
};
Wp6Rand g_wr;
constexpr float kWrEdgeX = -269170.0f;
constexpr float kWrGoalX = -289170.0f;
constexpr float kWrHighY = 100000.0f;

bool wp6RandTick(Context& c) {
    Wp6Rand& w = g_wr;
    if (c.frame >= 20000) {
        c.note("wp6-rand: Zeitlimit erreicht");
        return true;
    }
    advanceEvent(c);
    if (c.sess.phase != kff::Phase::Riding) {
        if (w.started && !w.reported) {
            w.reported = true;
            kff::logInfo("freiheit: WP6.6 Ritt endete vor dem Ziel (Schritt %d), voegelMax=%u", static_cast<int>(w.step),
                w.birdsMax);
        }
        if (inEvent()) {
            return false;
        }
        if (!w.started) {
            runCall(c, w.call);
        }
        return w.reported;
    }
    const uint32_t f = c.sess.rideFrames;
    daKago_c* k = kff::liveBird();
    daAlink_c* link = kff::liveLink();
    if (k == nullptr) {
        return false;
    }
    const cXyz pos = k->current.pos;
    if (!w.started) {
        w.started = true;
        w.y0 = pos.y;
        w.stepFrame = f;
        w.penBase = g_stats.penetration;
        w.roomNo = link != nullptr ? static_cast<int>(fopAcM_GetRoomNo(link)) : -1;
        k->current.angle.y = static_cast<s16>(-0x4000);
        k->shape_angle.y = k->current.angle.y;
        kff::logInfo("freiheit: WP6.6 Start pos=(%.0f,%.0f,%.0f) Raum %d, Kurs -X, Gang 5, Turbo", static_cast<double>(pos.x),
            static_cast<double>(pos.y), static_cast<double>(pos.z), w.roomNo);
    }
    const uint32_t n = wp6Birds();
    w.birdsMax = std::max(w.birdsMax, n);
    w.birdsMin = std::min(w.birdsMin, n);
    ++w.samples;
    if (f == 5 || f == 15) {
        c.tap(static_cast<uint32_t>(PAD_BUTTON_UP));
    }
    if (f >= 30 && (f - 30) % 120 == 0 && w.step != RandStep::Hold) {
        c.tapA();  // Turbo (Dash)
    }
    if (k->current.angle.y != static_cast<s16>(-0x4000) && (w.step == RandStep::West || w.step == RandStep::Climb)) {
        k->current.angle.y = static_cast<s16>(-0x4000);  // Kurs -X halten (Gleiten an Huegeln dreht nicht, nur zur Sicherheit)
        k->shape_angle.y = k->current.angle.y;
    }
    float y = 0.0f;
    const uint32_t in = f - w.stepFrame;
    switch (w.step) {
    case RandStep::Climb:
        y = wp6PitchStick(k, -0x2000);
        if (pos.y >= w.y0 + 5000.0f || in >= 900) {
            w.step = RandStep::West;
            w.stepFrame = f;
            kff::logInfo("freiheit: WP6.6 Steigen Ende f=%u y=%.1f, waagerecht nach -X", f, static_cast<double>(pos.y));
        }
        break;
    case RandStep::West: {
        // Halt (T1): in 30 Ticks weniger als 600 nach -X und dabei eine Wand beruehrt. Dokumentieren (Position,
        // Wandprofil), dann 300 Ticks mit Nase etwa 67 Grad hoch ueber das Hindernis steigen und weiter nach -X.
        if (g_stats.wallContacts > w.wallBase) {
            w.westContact = true;
        }
        w.wallBase = g_stats.wallContacts;
        if (in % 30 == 0) {
            if (in > 0 && w.climbLeft == 0 && w.westContact && w.westX - pos.x < 600.0f) {
                ++w.halts;
                kff::logInfo("freiheit: WP6.6 Halt %u bei f=%u pos=(%.0f,%.0f,%.0f): in 30 Ticks %.0f nach -X mit "
                             "Wandkontakt, Boden %.1f; Steigen ueber das Hindernis",
                    w.halts, f, static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z),
                    static_cast<double>(w.westX - pos.x),
                    static_cast<double>(groundGap(pos) >= 1.0e8f ? -1.0f : groundGap(pos)));
                if (w.halts <= 3) {
                    wp6WallProfile(pos, k->current.angle.y, "Kartenrand-Halt");
                }
                if (w.halts == 1) {
                    photo("wp6-rand-halt", f);
                }
                w.climbLeft = 300;
            }
            w.westX = pos.x;
            w.westContact = false;
        }
        if (w.climbLeft > 0) {
            --w.climbLeft;
            y = wp6PitchStick(k, -0x3000);
        } else {
            y = wp6PitchStick(k, 0);
        }
        if (in % 300 == 0) {
            kff::logInfo("freiheit: WP6.6 f=%u pos=(%.0f,%.0f,%.0f) voegel=%u boden=%.1f stageVeto=%u sceneCancel=%u "
                         "restartVeto=%u",
                f, static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z), n,
                static_cast<double>(groundGap(pos) >= 1.0e8f ? -1.0f : groundGap(pos)), g_stats.stageVeto,
                g_stats.sceneCancel, g_stats.restartVeto);
        }
        if (!w.edgeLogged && pos.x <= kWrEdgeX) {
            w.edgeLogged = true;
            w.edgeX = pos.x;
            kff::logInfo("freiheit: WP6.6 Kartenrand x<=%.0f erreicht f=%u pos=(%.0f,%.0f,%.0f) boden=%.1f",
                static_cast<double>(kWrEdgeX), f, static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z), static_cast<double>(groundGap(pos) >= 1.0e8f ? -1.0f : groundGap(pos)));
            photo("wp6-kartenrand", f);
        }
        if (pos.x <= kWrGoalX || in >= 9000) {
            kff::logInfo("freiheit: WP6.6 Ergebnis (%s): f=%u pos=(%.0f,%.0f,%.0f) voegel min/max=%u/%u Proben=%u Raum %d->%d "
                         "durchdringungen=%u stageVeto=%u sceneCancel=%u restartVeto=%u roofRestartVeto=%u Halte=%u",
                pos.x <= kWrGoalX ? "Ziel erreicht" : "Zeitlimit", f, static_cast<double>(pos.x),
                static_cast<double>(pos.y), static_cast<double>(pos.z), w.birdsMin, w.birdsMax, w.samples, w.roomNo,
                link != nullptr ? static_cast<int>(fopAcM_GetRoomNo(link)) : -1, g_stats.penetration - w.penBase,
                g_stats.stageVeto, g_stats.sceneCancel, g_stats.restartVeto, g_stats.roofRestartVeto, w.halts);
            w.step = RandStep::Up;
            w.stepFrame = f;
            c.note("wp6-rand: Steigen auf y > 100000 (6.8)");
        }
        break;
    }
    case RandStep::Up:
        y = wp6PitchStick(k, -0x2000);
        if (in % 300 == 0) {
            kff::logInfo("freiheit: WP6.8 Steigen f=%u y=%.1f mRoofHeight=%s", f, static_cast<double>(pos.y),
                k->mRoofHeight == kNoRoof ? "kNoRoof" : "WERT");
        }
        if (pos.y > kWrHighY || in >= 4000) {
            w.step = RandStep::Hold;
            w.stepFrame = f;
            w.lastY = pos.y;
            w.lastSign = 0;
            kff::logInfo("freiheit: WP6.8 Hoehe erreicht (%s) f=%u y=%.1f, 180 Ticks waagerecht",
                pos.y > kWrHighY ? "y > 100000" : "Zeitlimit", f, static_cast<double>(pos.y));
            photo("wp6-hoehe", f);
        }
        break;
    case RandStep::Hold: {
        y = in < 60 ? wp6PitchStick(k, 0) : 0.0f;  // erst waagerecht stellen, dann Stick neutral (Trimmung)
        const float dy = pos.y - w.lastY;
        const int sign = dy > 0.5f ? 1 : (dy < -0.5f ? -1 : 0);
        if (in > 60 && sign != 0 && w.lastSign != 0 && sign != w.lastSign) {
            ++w.signChanges;
        }
        if (sign != 0) {
            w.lastSign = sign;
        }
        w.lastY = pos.y;
        if (in > 60) {
            w.holdMin = std::min(w.holdMin, pos.y);
            w.holdMax = std::max(w.holdMax, pos.y);
        }
        if (in >= 180) {
            kff::logInfo("freiheit: WP6.8 Ergebnis: y=%.1f (min %.1f max %.1f in den letzten 120 Ticks), Vorzeichenwechsel=%u, "
                         "mRoofHeight=%s roofNotNone=%u voegel max=%u",
                static_cast<double>(pos.y), static_cast<double>(w.holdMin), static_cast<double>(w.holdMax), w.signChanges,
                k->mRoofHeight == kNoRoof ? "kNoRoof" : "WERT", g_wp6.roofNotNone, w.birdsMax);
            w.step = RandStep::Done;
            w.reported = true;
            logMess("wp6-rand");
            c.note("autotest: timeline complete (Ritt laeuft weiter bis zum Ende des Laufs)");
            return true;
        }
        break;
    }
    case RandStep::Done:
        return true;
    }
    c.stick(0.0f, y);
    return false;
}

// ---- wp6-ruf (F_SP121; in R_SP107 holt die Kette den Wolf zurueck): 6.3 Spawn nur im offenen Luftraum, 6.11 Ruf-Spam
// unter Decke, im Wasser und im Ritt ----
// Suche (je Tick kWfBudget Punkte je Raster): Ort mit Decke 200..590 ueber einem flachen Boden (Raster 250 in +-25000,
// bis zu drei Bodenebenen je Punkt; Link passt darunter, der Spawnraum 350 + 250 ueber Link nicht) und Wasser mit
// mindestens 200 Tiefe (Raster 500 in +-30000). Link wird dorthin gesetzt, die Pruefung macht danach das Spiel (Boden,
// Decke, Schwimmen). Je Ort 5 Ruf-Versuche (R+B), danach Ruf am Startort (frei), im Ritt R+B alle 20 Ticks fuer 600 Ticks
// (B steigt ab, danach "departing", danach neuer Ruf). Je Tick: Voegel <= 1.
enum class RufStep : uint8_t { Wait, Search, LowGo, LowSettle, LowSpam, WaterGo, WaterSettle, WaterSpam, BackGo,
                               BackSettle, Call, RideSpam, Finish, Done };
struct Wp6Ruf {
    RufStep step = RufStep::Wait;
    uint32_t t0 = 0;
    cXyz home;
    int ix = 0;
    int iw = 0;
    bool lowFound = false;
    cXyz low;
    float lowH = 0.0f;
    float lowD2 = 1.0e30f;
    bool waterFound = false;
    cXyz water;
    float waterDepth = 0.0f;
    float waterD2 = 1.0e30f;
    uint32_t candidates = 0;
    uint32_t attempts = 0;
    uint32_t birdsMax = 0;
    uint32_t rideSeen = 0;
    Call call;
    uint32_t spamStart = 0;
};
Wp6Ruf g_wf;
constexpr int kWfHalf = 100;          // Raster +-25000 um Link (Schritt 250)
constexpr float kWfStep = 250.0f;
constexpr int kWfBudget = 100;        // Rasterpunkte je Tick und Suche (Last im Spiel klein halten)

// Niedrige Decke per Sweep: je Rasterpunkt bis zu drei Bodenebenen von oben (Start y Link + 8000, danach knapp unter
// dem letzten Boden, z. B. Gelaende unter einer Bruecke oder einem Ueberhang). Je Ebene: Boden flach (Normale y >= 0,8),
// kein Wasser darueber, Decke (RoofChk ab Boden + 30) 200..590 ueber dem Boden: Link passt darunter, der Spawnraum
// (350 ueber Link plus 250 Vogelraum) nicht. Der naechste Ort zu Link gewinnt.
void wp6RufSearchLow(Wp6Ruf& w, int budget) {
    constexpr int kSide = 2 * kWfHalf + 1;
    for (int b = 0; b < budget && w.ix < kSide * kSide; ++b, ++w.ix) {
        const float x = w.home.x + static_cast<float>(w.ix % kSide - kWfHalf) * kWfStep;
        const float z = w.home.z + static_cast<float>(w.ix / kSide - kWfHalf) * kWfStep;
        const float d2 = (x - w.home.x) * (x - w.home.x) + (z - w.home.z) * (z - w.home.z);
        if (d2 >= w.lowD2) {
            continue;
        }
        float startY = w.home.y + 8000.0f;
        for (int layer = 0; layer < 3; ++layer) {
            cXyz top(x, startY, z);
            dBgS_GndChk gnd;
            gnd.SetPos(&top);
            const float g = dComIfG_Bgsp().GroundCross(&gnd);
            if (g == -G_CM3D_F_INF) {
                break;
            }
            startY = g - 40.0f;
            cM3dGPla pla;
            dComIfG_Bgsp().GetTriPla(gnd, &pla);
            if (pla.mNormal.y < 0.8f) {
                continue;
            }
            cXyz wtop(x, g + 1500.0f, z);
            dBgS_ObjGndChk_Spl spl;
            spl.SetPos(&wtop);
            const float wy = dComIfG_Bgsp().GroundCross(&spl);
            if (wy != -G_CM3D_F_INF && wy > g + 20.0f) {
                continue;  // Wasser: nicht fuer den Decken-Test
            }
            cXyz rp(x, g + 30.0f, z);
            dBgS_RoofChk rc;
            rc.SetPos(rp);
            const float r = dComIfG_Bgsp().RoofChk(&rc);
            if (r == G_CM3D_F_INF) {
                continue;
            }
            const float h = r - g;
            if (h >= 200.0f && h <= 590.0f) {
                ++w.candidates;
                w.lowFound = true;
                w.low = cXyz(x, g, z);
                w.lowH = h;
                w.lowD2 = d2;
                break;
            }
        }
    }
}

// Wasser: Raster 500 in +-30000 um Link, Wasserflaeche (Spl-Gruppe) mindestens 200 ueber dem Grund, hoechstens 3000
// ueber oder unter Link. Der naechste Ort gewinnt.
constexpr int kWfWaterHalf = 60;
constexpr float kWfWaterStep = 500.0f;
void wp6RufSearchWater(Wp6Ruf& w, int budget) {
    constexpr int kSide = 2 * kWfWaterHalf + 1;
    for (int b = 0; b < budget && w.iw < kSide * kSide; ++b, ++w.iw) {
        const float x = w.home.x + static_cast<float>(w.iw % kSide - kWfWaterHalf) * kWfWaterStep;
        const float z = w.home.z + static_cast<float>(w.iw / kSide - kWfWaterHalf) * kWfWaterStep;
        const float d2 = (x - w.home.x) * (x - w.home.x) + (z - w.home.z) * (z - w.home.z);
        if (d2 >= w.waterD2) {
            continue;
        }
        cXyz top(x, w.home.y + 3000.0f, z);
        dBgS_ObjGndChk_Spl spl;
        spl.SetPos(&top);
        const float wy = dComIfG_Bgsp().GroundCross(&spl);
        if (wy == -G_CM3D_F_INF || std::fabs(wy - w.home.y) > 3000.0f) {
            continue;
        }
        cXyz wtop(x, wy + 10.0f, z);
        dBgS_GndChk gnd;
        gnd.SetPos(&wtop);
        const float g = dComIfG_Bgsp().GroundCross(&gnd);
        if (g == -G_CM3D_F_INF || wy - g < 200.0f) {
            continue;
        }
        w.waterFound = true;
        w.water = cXyz(x, wy, z);
        w.waterDepth = wy - g;
        w.waterD2 = d2;
    }
}

bool wp6RufTick(Context& c) {
    Wp6Ruf& w = g_wf;
    if (c.frame >= kScenarioDeadline) {
        c.note("wp6-ruf: Zeitlimit erreicht");
        return true;
    }
    const uint32_t n = wp6Birds();
    w.birdsMax = std::max(w.birdsMax, n);
    daAlink_c* link = kff::liveLink();
    const uint32_t t = c.frame;
    advanceEvent(c);
    if (link == nullptr) {
        return false;
    }
    switch (w.step) {
    case RufStep::Wait:
        if (t >= kCallAt && c.canCall()) {
            w.home = link->current.pos;
            w.step = RufStep::Search;
            w.t0 = t;
            kff::logInfo("freiheit: WP6.3 Suche nach niedriger Decke und Wasser um (%.0f,%.0f,%.0f)",
                static_cast<double>(w.home.x), static_cast<double>(w.home.y), static_cast<double>(w.home.z));
        }
        return false;
    case RufStep::Search:
        wp6RufSearchLow(w, kWfBudget);
        wp6RufSearchWater(w, kWfBudget);
        if (w.ix >= (2 * kWfHalf + 1) * (2 * kWfHalf + 1) && w.iw >= (2 * kWfWaterHalf + 1) * (2 * kWfWaterHalf + 1)) {
            kff::logInfo("freiheit: WP6.3 Suche fertig: niedrige Decke %s (%.0f,%.0f,%.0f) Hoehe %.0f, Kandidaten %u; Wasser %s "
                         "(%.0f,%.0f,%.0f) Tiefe %.0f",
                w.lowFound ? "ja" : "nein", static_cast<double>(w.low.x), static_cast<double>(w.low.y),
                static_cast<double>(w.low.z), static_cast<double>(w.lowH), w.candidates, w.waterFound ? "ja" : "nein",
                static_cast<double>(w.water.x), static_cast<double>(w.water.y), static_cast<double>(w.water.z),
                static_cast<double>(w.waterDepth));
            w.step = w.lowFound ? RufStep::LowGo : (w.waterFound ? RufStep::WaterGo : RufStep::Call);
            w.t0 = t;
        }
        return false;
    case RufStep::LowGo:
        wp6Teleport(link, cXyz(w.low.x, w.low.y + 5.0f, w.low.z));
        w.step = RufStep::LowSettle;
        w.t0 = t;
        c.note("wp6-ruf: Link unter der niedrigen Decke");
        return false;
    case RufStep::LowSettle:
        if (t >= w.t0 + 60) {
            kff::logInfo("freiheit: WP6.3 Link unter Decke: pos=(%.0f,%.0f,%.0f) boden=%d Decke ueber Link=%.1f proc=%d",
                static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                static_cast<double>(link->current.pos.z), link->mLinkAcch.ChkGroundHit() ? 1 : 0,
                static_cast<double>(wp6RoofAbove(link->current.pos) - link->current.pos.y),
                static_cast<int>(link->mProcID));
            w.step = RufStep::LowSpam;
            w.t0 = t;
            w.attempts = 0;
        }
        return false;
    case RufStep::LowSpam:
        if ((t - w.t0) % 20 == 0 && w.attempts < 5) {
            ++w.attempts;
            wp6Chord(c);
            wp6LogAttempt("WP6.3 Decke", c);
        }
        if (t >= w.t0 + 120) {
            kff::logInfo("freiheit: WP6.3 Ergebnis Decke: %u Versuche, Voegel jetzt %u, max %u (Soll 0)", w.attempts, n,
                w.birdsMax);
            w.step = w.waterFound ? RufStep::WaterGo : RufStep::BackGo;
            w.t0 = t;
        }
        return false;
    case RufStep::WaterGo:
        wp6Teleport(link, cXyz(w.water.x, w.water.y + 80.0f, w.water.z));
        w.step = RufStep::WaterSettle;
        w.t0 = t;
        c.note("wp6-ruf: Link ueber dem Wasser");
        return false;
    case RufStep::WaterSettle:
        if (t >= w.t0 + 90) {
            w.step = RufStep::WaterSpam;
            w.t0 = t;
            w.attempts = 0;
        }
        return false;
    case RufStep::WaterSpam:
        if ((t - w.t0) % 20 == 0 && w.attempts < 5) {
            ++w.attempts;
            wp6Chord(c);
            wp6LogAttempt("WP6.11 Wasser", c);
        }
        if (t >= w.t0 + 120) {
            kff::logInfo("freiheit: WP6.11 Ergebnis Wasser: %u Versuche, Voegel jetzt %u, max %u (Soll 0)", w.attempts, n,
                w.birdsMax);
            w.step = RufStep::BackGo;
            w.t0 = t;
        }
        return false;
    case RufStep::BackGo:
        wp6Teleport(link, cXyz(w.home.x, w.home.y + 5.0f, w.home.z));
        w.step = RufStep::BackSettle;
        w.t0 = t;
        return false;
    case RufStep::BackSettle:
        if (t >= w.t0 + 90 && c.canCall()) {
            w.step = RufStep::Call;
            w.t0 = t;
            w.call = Call();
        } else if (t >= w.t0 + 900) {
            c.note("wp6-ruf: Link wurde am Startort nicht ruffaehig");
            w.step = RufStep::Finish;
            w.t0 = t;
        }
        return false;
    case RufStep::Call:
        if (!w.call.armed) {
            w.call.armed = true;
            w.call.start = t;
            c.note("wp6-ruf: Ruf am freien Startort (Decke 700)");
        }
        if (t < w.call.start + kHold) {
            c.holdR();
        }
        if (!w.call.pressed && t == w.call.start + kPress) {
            w.call.pressed = true;
            c.tapB();
        }
        if (c.sess.phase == kff::Phase::Riding) {
            w.step = RufStep::RideSpam;
            w.t0 = t;
            w.spamStart = t + 30;
            kff::logInfo("freiheit: WP6.3 Ruf am freien Ort angenommen, Ritt laeuft; Spam im Ritt folgt");
        } else if (t >= w.call.start + 600) {
            c.note("wp6-ruf: kein Ritt am Startort");
            w.step = RufStep::Finish;
            w.t0 = t;
        }
        return false;
    case RufStep::RideSpam:
        if (c.sess.phase == kff::Phase::Riding) {
            ++w.rideSeen;
            c.stick(0.0f, -0.3f);
        }
        if (t >= w.spamStart && t % 20 == 10 && t < w.spamStart + 600) {  // nicht im Tick der A-Taste (advanceEvent)
            wp6Chord(c);
            wp6LogAttempt("WP6.11 Ritt-Spam", c);
        }
        if (t >= w.spamStart + 900) {
            w.step = RufStep::Finish;
            w.t0 = t;
        }
        return false;
    case RufStep::Finish:
        if (c.sess.phase == kff::Phase::Riding && t == w.t0 + 1) {
            c.tapB();
        }
        if (t >= w.t0 + 400 || (c.sess.phase == kff::Phase::Idle && n == 0 && t >= w.t0 + 120)) {
            kff::logInfo("freiheit: WP6.11 Ergebnis: Voegel max=%u (Soll <= 1), Ritt-Frames im Spam=%u, Voegel jetzt %u", w.birdsMax,
                w.rideSeen, n);
            logMess("wp6-ruf");
            c.note("autotest: timeline complete");
            w.step = RufStep::Done;
            return true;
        }
        return false;
    case RufStep::Done:
        return true;
    }
    return false;
}

// ---- wp6-timeout (F_SP121, KFF_TEST_NORIDE=1): 6.9 Ruf ohne Ritt-Start, 6.10 schnelles B ----
// 6.9: der erste Vogel startet keinen Ritt (Testschalter im Kern). Erwartet: "summon timeout: no ride start after", der
// Vogel ist hoechstens 300 Ticks nach dem Ruf weg, der naechste Ruf wird angenommen (Ritt). 6.10: Ritt 10 Ticks, B,
// 50 Ticks spaeter R+B: angenommen oder "call rejected (departing)" (B bleibt). Voegel <= 1, der Vogel ist hoechstens
// 100 Ticks nach Beginn des Abflugs weg. Danach noch ein Ruf (angenommen) und B.
enum class ToStep : uint8_t { Call1, WaitGone, Call2, Ride2, AfterB, WaitGone2, Call3, Ride3, End, Done };
struct Wp6Timeout {
    ToStep step = ToStep::Call1;
    Call call;
    uint32_t t0 = 0;
    uint32_t spawnTick = 0;
    uint32_t bTick = 0;
    uint32_t departTick = 0;
    uint32_t birdsMax = 0;
    fpc_ProcID firstBird = kff::kNoProc;  // Vogel des Ritts, der mit B endet (6.10)
    uint32_t firstGone = 0;
    uint32_t chordTick = 0;
    bool secondRide = false;
    uint32_t secondB = 0;
};
Wp6Timeout g_wt;

// 6.10: Abflugbeginn und Verschwinden des ersten Vogels (Prozess-ID) messen.
void wp6TimeoutTrack(Context& c, Wp6Timeout& w, uint32_t t) {
    if (w.departTick == 0 && c.sess.phase == kff::Phase::Dismounting && c.sess.birdId == w.firstBird) {
        w.departTick = t;
        kff::logInfo("freiheit: WP6.10 Abflug beginnt %u Ticks nach B", t - w.bTick);
    }
    if (w.firstGone == 0 && w.firstBird != kff::kNoProc && fopAcM_SearchByID(w.firstBird) == nullptr) {
        w.firstGone = t;
        kff::logInfo("freiheit: WP6.10 erster Vogel weg: %u Ticks nach Abflugbeginn (Soll <= 100), %u Ticks nach B, "
                     "%u Ticks nach R+B",
            w.departTick != 0 ? t - w.departTick : 0, t - w.bTick, w.chordTick != 0 ? t - w.chordTick : 0);
    }
}

bool wp6TimeoutTick(Context& c) {
    Wp6Timeout& w = g_wt;
    const uint32_t t = c.frame;
    if (t >= kScenarioDeadline) {
        c.note("wp6-timeout: Zeitlimit erreicht");
        return true;
    }
    const uint32_t n = wp6Birds();
    w.birdsMax = std::max(w.birdsMax, n);
    advanceEvent(c);
    switch (w.step) {
    case ToStep::Call1:
        runCall(c, w.call);
        if (c.sess.birdId != kff::kNoProc && c.sess.phase == kff::Phase::Summoning && w.spawnTick == 0) {
            w.spawnTick = t;
            w.step = ToStep::WaitGone;
            kff::logInfo("freiheit: WP6.9 erster Vogel gerufen (Tick %u), Ritt-Start gesperrt (KFF_TEST_NORIDE)", t);
        }
        return false;
    case ToStep::WaitGone:
        if (c.sess.phase == kff::Phase::Riding) {
            kff::logInfo("freiheit: WP6.9 FEHLER: Ritt trotz KFF_TEST_NORIDE");
        }
        if (n == 0) {
            kff::logInfo("freiheit: WP6.9 Ergebnis: erster Vogel weg nach %u Ticks (Soll <= 300), Phase %s", t - w.spawnTick,
                kff::phaseName(c.sess.phase));
            w.step = ToStep::Call2;
            w.call = Call();
            w.t0 = t;
        } else if (t - w.spawnTick > 900) {
            kff::logInfo("freiheit: WP6.9 FEHLER: Vogel nach 900 Ticks noch da");
            w.step = ToStep::Call2;
            w.call = Call();
            w.t0 = t;
        }
        return false;
    case ToStep::Call2:
        if (!w.call.armed && c.canCall() && t >= w.t0 + 30) {
            w.call.armed = true;
            w.call.start = t;
            c.note("wp6-timeout: naechster Ruf");
        }
        if (w.call.armed && t < w.call.start + kHold) {
            c.holdR();
        }
        if (w.call.armed && !w.call.pressed && t == w.call.start + kPress) {
            w.call.pressed = true;
            c.tapB();
        }
        if (c.sess.phase == kff::Phase::Riding) {
            kff::logInfo("freiheit: WP6.9 naechster Ruf angenommen, Ritt laeuft (Tick %u)", t);
            w.step = ToStep::Ride2;
        } else if (w.call.armed && t > w.call.start + 300) {
            kff::logInfo("freiheit: WP6.9 FEHLER: naechster Ruf ohne Ritt");
            w.step = ToStep::End;
            w.t0 = t;
        }
        return false;
    case ToStep::Ride2:
        if (c.sess.phase == kff::Phase::Riding && c.sess.rideFrames >= 10) {
            c.tapB();
            w.bTick = t;
            w.firstBird = c.sess.birdId;
            w.step = ToStep::AfterB;
            kff::logInfo("freiheit: WP6.10 B bei Ritt-Frame %u (Tick %u)", c.sess.rideFrames, t);
        }
        return false;
    case ToStep::AfterB:
        wp6TimeoutTrack(c, w, t);
        if (t == w.bTick + 50) {
            w.chordTick = t;
            wp6Chord(c);
            wp6LogAttempt("WP6.10 R+B 50 Ticks nach B", c);
        }
        if (t > w.bTick + 50) {
            w.step = ToStep::WaitGone2;
        }
        return false;
    case ToStep::WaitGone2:
        wp6TimeoutTrack(c, w, t);
        if (c.sess.phase == kff::Phase::Riding && c.sess.birdId != w.firstBird && !w.secondRide) {
            w.secondRide = true;
            w.secondB = t + 10;
            kff::logInfo("freiheit: WP6.10 R+B nach B wurde als neuer Ruf angenommen (zweiter Vogel, Ritt laeuft)");
        }
        if (w.secondRide && t == w.secondB) {
            c.tapB();
        }
        if (w.firstGone != 0 && n == 0 && c.sess.phase == kff::Phase::Idle) {
            kff::logInfo("freiheit: WP6.10 Ergebnis: erster Vogel weg %u Ticks nach Abflugbeginn (Soll <= 100), %u Ticks "
                         "nach B, zweiter Ritt %s, Voegel max=%u (Soll <= 1)",
                w.departTick != 0 ? w.firstGone - w.departTick : 0, w.firstGone - w.bTick, w.secondRide ? "ja" : "nein",
                w.birdsMax);
            w.step = ToStep::Call3;
            w.call = Call();
            w.t0 = t;
        } else if (t > w.bTick + 1200) {
            kff::logInfo("freiheit: WP6.10 FEHLER: Vogel 1200 Ticks nach B noch da (Phase %s)", kff::phaseName(c.sess.phase));
            w.step = ToStep::End;
            w.t0 = t;
        }
        return false;
    case ToStep::Call3:
        if (!w.call.armed && c.canCall() && t >= w.t0 + 30) {
            w.call.armed = true;
            w.call.start = t;
            c.note("wp6-timeout: Ruf nach dem Abflug");
        }
        if (w.call.armed && t < w.call.start + kHold) {
            c.holdR();
        }
        if (w.call.armed && !w.call.pressed && t == w.call.start + kPress) {
            w.call.pressed = true;
            c.tapB();
        }
        if (c.sess.phase == kff::Phase::Riding) {
            kff::logInfo("freiheit: WP6.10 Ruf nach dem Abflug angenommen (Tick %u)", t);
            w.step = ToStep::Ride3;
            w.t0 = t;
        } else if (w.call.armed && t > w.call.start + 300) {
            kff::logInfo("freiheit: WP6.10 FEHLER: Ruf nach dem Abflug ohne Ritt");
            w.step = ToStep::End;
            w.t0 = t;
        }
        return false;
    case ToStep::Ride3:
        if (t == w.t0 + 60) {
            c.tapB();
        }
        if (t >= w.t0 + 61) {
            w.step = ToStep::End;
            w.t0 = t;
        }
        return false;
    case ToStep::End:
        if ((c.sess.phase == kff::Phase::Idle && n == 0) || t >= w.t0 + 900) {
            kff::logInfo("freiheit: WP6.9/6.10 Ende: Voegel max=%u (Soll <= 1), Voegel jetzt %u", w.birdsMax, n);
            c.note("autotest: timeline complete");
            w.step = ToStep::Done;
            return true;
        }
        return false;
    case ToStep::Done:
        return true;
    }
    return false;
}

// ---- wp6-gate (R_SP107 oder F_SP121): 6.12 Ruf-Gates mit echten Ausloesern ----
// Ereignis beim Laden (R+B alle 40 Ticks), Startflanke (START, R und B im selben Tick), Pause (START allein, warten bis
// das Menue offen ist, dann R+B), Luft (Link 300 hoeher gesetzt, faellt; R+B im Fall), Sturz (nur unter freiem Himmel:
// Link 2500 hoeher, R+B im Fall und nach der Landung, Proc im Log), Zielsperre (L gehalten, dann R+B), Reiten (Ruf, im
// Ritt R+B), zuletzt Schaden durch Gegner (Link wartet, R+B beim Eintritt in einen Schaden-Proc). Je Versuch: Voegel,
// Grund (Kern-Log "call rejected (...), B stays"), R-Status. Soll: Voegel 0 bei jeder Ablehnung.
// Ein Ruf, den ein Gate haette ablehnen muessen, steht als "WP6.12 FEHLER" im Log; der Ritt endet per B und das Szenario
// setzt mit dem naechsten Schritt fort (Lauf wp6p2-gate-f: Menue nicht offen, Ruf angenommen, Szenario hing im Ritt).
enum class GateStep : uint8_t { Event, Calm, StartEdge, StartEdgeClose, Pause, PauseClose, Air, AirLand, Fall, FallLand,
                                Lock, Ride, RideChord, Finish, Hurt, Done };
struct Wp6Gate {
    GateStep step = GateStep::Event;
    uint32_t t0 = 0;
    uint32_t eventTries = 0;
    uint32_t tries = 0;
    uint32_t birdsMaxRejected = 0;
    bool bigFall = false;
    Call call;
    uint32_t lastTry = 0;
    const char* lastTag = nullptr;
    const char* lastTagSeen = nullptr;  // letzter Versuch (bleibt fuer die Fehlermeldung stehen)
    uint32_t landTries = 0;
    int landProcMax = -1;
    bool menuSeen = false;     // Startflanke: das Menue war offen
    uint32_t openAt = 0;       // Pause: Tick, ab dem das Menue offen ist (0 = noch nicht)
    bool unexpected = false;   // ein Ruf wurde vor dem Reiten-Test angenommen
    GateStep resume = GateStep::Done;
    uint32_t unexpectedCount = 0;
    bool hurtDone = false;
    uint32_t hurtTries = 0;
    bool hurtPrev = false;
};
Wp6Gate g_wg;

void wp6GateTry(Context& c, Wp6Gate& w, const char* tag) {
    wp6Chord(c);
    ++w.tries;
    w.lastTry = c.frame;
    w.lastTag = tag;
    w.lastTagSeen = tag;
    wp6LogAttempt(tag, c);
}

bool wp6MenuOpen() {
    return dComIfGp_isPauseFlag() != 0 || dMeter2Info_getWindowStatus() != 0;
}

// Schritt nach einem unerwartet angenommenen Ruf (kein Endlos-Kreislauf ueber denselben Versuch).
GateStep wp6GateResume(GateStep s) {
    switch (s) {
    case GateStep::Event:
    case GateStep::Calm:
    case GateStep::StartEdge:
    case GateStep::StartEdgeClose:
    case GateStep::Pause:
    case GateStep::PauseClose:
        return GateStep::PauseClose;
    case GateStep::Air:
    case GateStep::AirLand:
        return GateStep::AirLand;
    case GateStep::Fall:
    case GateStep::FallLand:
    case GateStep::Lock:
        return GateStep::Lock;
    case GateStep::Hurt:
        return GateStep::Finish;
    default:
        return GateStep::Ride;
    }
}

// Schritte, in denen jeder Ruf abgelehnt werden muss (Reiten-Test und Abschluss ausgenommen).
bool wp6GateMustReject(GateStep s) {
    return s < GateStep::Ride || s == GateStep::Hurt;
}

// Schadens- und Lande-Procs von Link (Wolf und Mensch), nur fuer die Sturz-Pruefung des Szenarios.
bool wp6LinkHurtProc(const daAlink_c* link) {
    const int p = static_cast<int>(link->mProcID);
    return p == daAlink_c::PROC_WOLF_LAND_DAMAGE || p == daAlink_c::PROC_WOLF_DAMAGE ||
           p == daAlink_c::PROC_WOLF_LARGE_DAMAGE_UP || p == daAlink_c::PROC_LAND_DAMAGE || p == daAlink_c::PROC_DAMAGE ||
           p == daAlink_c::PROC_POLY_DAMAGE || p == daAlink_c::PROC_LARGE_DAMAGE;
}

bool wp6GateTick(Context& c) {
    Wp6Gate& w = g_wg;
    const uint32_t t = c.frame;
    if (t >= kScenarioDeadline) {
        c.note("wp6-gate: Zeitlimit erreicht");
        return true;
    }
    daAlink_c* link = kff::liveLink();
    const uint32_t n = wp6Birds();
    if (wp6GateMustReject(w.step) && !w.unexpected) {
        w.birdsMaxRejected = std::max(w.birdsMaxRejected, n);
    }
    if (w.lastTag != nullptr && t == w.lastTry + 3) {
        kff::logInfo("freiheit: %s Ergebnis 3 Ticks spaeter: voegel=%u phase=%s", w.lastTag, n, kff::phaseName(c.sess.phase));
        w.lastTag = nullptr;
    }
    // Vor dem Reiten-Test darf kein Ruf angenommen werden. Sonst: Fehler protokollieren, Ritt per B beenden, warten bis der
    // Vogel fort ist, dann mit dem Folgeschritt weiter.
    if (wp6GateMustReject(w.step) && c.sess.phase != kff::Phase::Idle) {
        if (!w.unexpected) {
            w.unexpected = true;
            ++w.unexpectedCount;
            w.resume = wp6GateResume(w.step);
            kff::logInfo("freiheit: WP6.12 FEHLER: Ruf angenommen bei \"%s\" (Phase %s, Schritt %u), Ritt endet per B",
                w.lastTagSeen != nullptr ? w.lastTagSeen : "?", kff::phaseName(c.sess.phase),
                static_cast<unsigned>(w.step));
        }
        if (c.sess.phase == kff::Phase::Riding && t % 30 == 0) {
            c.tapB();
        }
        return false;
    }
    if (w.unexpected) {
        if (n > 0 && t < w.t0 + 1200) {
            return false;  // Vogel fliegt noch ab
        }
        w.unexpected = false;
        w.step = w.resume;
        w.t0 = t;
        kff::logInfo("freiheit: WP6.12 weiter mit Schritt %u nach dem Fehler", static_cast<unsigned>(w.step));
        return false;
    }
    switch (w.step) {
    case GateStep::Event:
        if (inEvent()) {
            if (t % 20 == 0) {
                c.tapA();
            } else if (t % 40 == 10 && w.eventTries < 5) {
                ++w.eventTries;
                wp6GateTry(c, w, "WP6.12 Ereignis");
            }
            return false;
        }
        if (t >= kCallAt && c.canCall()) {
            kff::logInfo("freiheit: WP6.12 Ereignis-Versuche %u, Voegel max %u", w.eventTries, w.birdsMaxRejected);
            w.step = GateStep::StartEdge;
            w.t0 = t;
        }
        return false;
    case GateStep::Calm:
        return false;
    case GateStep::StartEdge:
        if (t == w.t0 + 10) {
            c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
            wp6GateTry(c, w, "WP6.12 Startflanke");
            w.step = GateStep::StartEdgeClose;
            w.t0 = t;
            w.menuSeen = false;
        }
        return false;
    case GateStep::StartEdgeClose:
        // Oeffnete die Startflanke ein Menue, schliesst es dieser Schritt (START und B im Wechsel), dann folgt die Pause.
        if (wp6MenuOpen()) {
            w.menuSeen = true;
            if ((t - w.t0) % 60 == 30) {
                c.tap(static_cast<uint32_t>(((t - w.t0) / 60) % 2 == 0 ? PAD_BUTTON_START : PAD_BUTTON_B));
            }
            if (t - w.t0 > 900) {
                c.note("wp6-gate: Menue schliesst nicht");
                w.step = GateStep::Finish;
                w.t0 = t;
            }
            return false;
        }
        if (t >= w.t0 + 60) {
            kff::logInfo("freiheit: WP6.12 Startflanke: Menue %s", w.menuSeen ? "war offen, jetzt zu" : "blieb zu");
            w.step = GateStep::Pause;
            w.t0 = t;
            w.openAt = 0;
        }
        return false;
    case GateStep::Pause:
        // START allein; der Ruf erst, wenn das Menue wirklich offen ist (Pause-Flag oder Fenster-Status).
        if (!wp6MenuOpen()) {
            if (w.openAt != 0) {
                kff::logInfo("freiheit: WP6.12 Pause: Menue wieder zu nach %u Ticks", t - w.openAt);
                w.step = GateStep::PauseClose;
                w.t0 = t;
                return false;
            }
            if (t == w.t0 + 10 || t == w.t0 + 100 || t == w.t0 + 190) {
                c.tap(static_cast<uint32_t>(PAD_BUTTON_START));
                kff::logInfo("freiheit: WP6.12 Pause: START (Tick %u des Schritts, proc=%d)", t - w.t0,
                    link != nullptr ? static_cast<int>(link->mProcID) : -1);
            }
            if (t >= w.t0 + 280) {
                kff::logInfo("freiheit: WP6.12 Pause: Menue oeffnet nicht (3x START), Pause-Versuch entfaellt");
                w.step = GateStep::PauseClose;
                w.t0 = t;
            }
            return false;
        }
        if (w.openAt == 0) {
            w.openAt = t;
            kff::logInfo("freiheit: WP6.12 Pause: Menue offen (pause=%d fenster=%d)", dComIfGp_isPauseFlag() != 0 ? 1 : 0,
                static_cast<int>(dMeter2Info_getWindowStatus()));
            photo("wp6-pause", t);
        }
        if (t == w.openAt + 30) {
            wp6GateTry(c, w, "WP6.12 Pause");
        }
        if (t >= w.openAt + 60) {
            w.step = GateStep::PauseClose;
            w.t0 = t;
        }
        return false;
    case GateStep::PauseClose:
        if (dComIfGp_isPauseFlag() != 0 || dMeter2Info_getWindowStatus() != 0) {
            if ((t - w.t0) % 60 == 0) {
                c.tap(static_cast<uint32_t>(((t - w.t0) / 60) % 2 == 0 ? PAD_BUTTON_START : PAD_BUTTON_B));
            }
            if (t - w.t0 > 900) {
                c.note("wp6-gate: Menue schliesst nicht");
                w.step = GateStep::Finish;
                w.t0 = t;
            }
            return false;
        }
        if (t >= w.t0 + 30 && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            const float roof = wp6RoofAbove(link->current.pos);
            const float room = roof == G_CM3D_F_INF ? 1.0e9f : roof - link->current.pos.y;
            w.bigFall = room > 3500.0f;
            kff::logInfo("freiheit: WP6.12 Menue zu, Luft-Test (Raum ueber Link %.0f, Sturz-Test %s)",
                static_cast<double>(room >= 1.0e8f ? -1.0f : room), w.bigFall ? "ja" : "nein");
            wp6Teleport(link, cXyz(link->current.pos.x, link->current.pos.y + 300.0f, link->current.pos.z));
            w.step = GateStep::Air;
            w.t0 = t;
        }
        return false;
    case GateStep::Air:
        if (t == w.t0 + 3 || t == w.t0 + 6 || t == w.t0 + 9) {
            wp6GateTry(c, w, "WP6.12 Luft");
        }
        if (t >= w.t0 + 12) {
            w.step = GateStep::AirLand;
            w.t0 = t;
        }
        return false;
    case GateStep::AirLand:
        if (link != nullptr && link->mLinkAcch.ChkGroundHit() && t >= w.t0 + 60) {
            if (w.bigFall) {
                wp6Teleport(link, cXyz(link->current.pos.x, link->current.pos.y + 2500.0f, link->current.pos.z));
                w.step = GateStep::Fall;
            } else {
                w.step = GateStep::Lock;
            }
            w.t0 = t;
        } else if (t >= w.t0 + 600) {
            w.step = GateStep::Lock;
            w.t0 = t;
        }
        return false;
    case GateStep::Fall:
        if ((t - w.t0) % 10 == 5) {
            wp6GateTry(c, w, "WP6.12 Sturz (Fall)");
        }
        if (link != nullptr && link->mLinkAcch.ChkGroundHit() && t > w.t0 + 5) {
            kff::logInfo("freiheit: WP6.12 Sturz gelandet nach %u Ticks, proc=%d leben=%u", t - w.t0,
                static_cast<int>(link->mProcID), static_cast<unsigned>(dComIfGs_getLife()));
            w.step = GateStep::FallLand;
            w.t0 = t;
        } else if (t > w.t0 + 400) {
            w.step = GateStep::FallLand;
            w.t0 = t;
        }
        return false;
    case GateStep::FallLand:
        // Nur im Schaden-Proc rufen (Landeschaden); ohne Schaden ist ein Ruf am Boden gueltig und gehoert nicht hierher.
        if (link != nullptr && t <= w.t0 + 60) {
            w.landProcMax = std::max(w.landProcMax, static_cast<int>(link->mProcID));
            if ((t - w.t0) % 4 == 1 && wp6LinkHurtProc(link)) {
                ++w.landTries;
                wp6GateTry(c, w, "WP6.12 Sturz (Landeschaden)");
            }
        }
        if (t >= w.t0 + 120) {
            kff::logInfo("freiheit: WP6.12 Sturz-Landung: %u Versuche im Schaden-Proc%s", w.landTries,
                w.landTries == 0 ? " (kein Schaden-Proc nach der Landung)" : "");
            w.step = GateStep::Lock;
            w.t0 = t;
        }
        return false;
    case GateStep::Lock:
        if (t < w.t0 + 40) {
            c.hold(static_cast<uint32_t>(PAD_TRIGGER_L));
        }
        if (t == w.t0 + 20) {
            wp6GateTry(c, w, "WP6.12 Zielsperre (L gehalten)");
        }
        if (t >= w.t0 + 90) {
            kff::logInfo("freiheit: WP6.12 Ergebnis Gates: Versuche %u, Voegel max bei Ablehnung %u (Soll 0), Phase %s",
                w.tries, w.birdsMaxRejected, kff::phaseName(c.sess.phase));
            w.step = GateStep::Ride;
            w.t0 = t;
            w.call = Call();
        }
        return false;
    case GateStep::Ride:
        if (c.sess.phase == kff::Phase::Riding) {
            w.step = GateStep::RideChord;
            w.t0 = t;
            return false;
        }
        if (!w.call.armed && c.canCall()) {
            w.call.armed = true;
            w.call.start = t;
            c.note("wp6-gate: Ruf fuer den Reiten-Test");
        }
        if (w.call.armed && t < w.call.start + kHold) {
            c.holdR();
        }
        if (w.call.armed && !w.call.pressed && t == w.call.start + kPress) {
            w.call.pressed = true;
            c.tapB();
        }
        if (t > w.t0 + 1500) {
            c.note("wp6-gate: kein Ritt fuer den Reiten-Test");
            w.step = GateStep::Finish;
            w.t0 = t;
        }
        return false;
    case GateStep::RideChord:
        if (t == w.t0 + 30) {
            wp6GateTry(c, w, "WP6.12 Reiten (R+B im Ritt)");
        }
        if (t >= w.t0 + 31) {
            w.step = GateStep::Finish;
            w.t0 = t;
        }
        return false;
    case GateStep::Finish:
        if (c.sess.phase == kff::Phase::Riding && t == w.t0 + 300) {
            c.tapB();
        }
        if (!w.hurtDone && c.sess.phase == kff::Phase::Idle && n == 0 && t >= w.t0 + 60) {
            // Zuletzt Schaden durch Gegner (kann Herzen kosten, darum am Ende).
            w.hurtDone = true;
            w.hurtPrev = false;
            w.step = GateStep::Hurt;
            w.t0 = t;
            kff::logInfo("freiheit: WP6.12 Schaden-Test: Link wartet auf einen Gegnertreffer (hoechstens 1800 Ticks)");
            return false;
        }
        if ((c.sess.phase == kff::Phase::Idle && n == 0 && t >= w.t0 + 60) || t >= w.t0 + 1200) {
            kff::logInfo("freiheit: WP6.12 Ende: Versuche %u, Voegel max bei Ablehnung %u, Voegel jetzt %u, Fehler %u", w.tries,
                w.birdsMaxRejected, n, w.unexpectedCount);
            c.note("autotest: timeline complete");
            w.step = GateStep::Done;
            return true;
        }
        return false;
    case GateStep::Hurt: {
        // Echter Ausloeser: ein Gegner trifft Link (Twilight-Feld). R+B beim Eintritt in einen Schaden-Proc, hoechstens 3x.
        const bool hurt = link != nullptr && wp6LinkHurtProc(link);
        if (hurt && !w.hurtPrev) {
            ++w.hurtTries;
            kff::logInfo("freiheit: WP6.12 Schaden-Proc %d (0x%x) leben=%u", static_cast<int>(link->mProcID),
                static_cast<unsigned>(link->mProcID), static_cast<unsigned>(dComIfGs_getLife()));
            wp6GateTry(c, w, "WP6.12 Schaden (Gegner)");
        }
        w.hurtPrev = hurt;
        if (w.hurtTries >= 3 || t >= w.t0 + 1800 || (link != nullptr && linkDead(link))) {
            kff::logInfo("freiheit: WP6.12 Schaden-Test Ende: %u Versuche im Schaden-Proc, Voegel max bei Ablehnung %u",
                w.hurtTries, w.birdsMaxRejected);
            w.step = GateStep::Finish;
            w.t0 = t;
        }
        return false;
    }
    case GateStep::Done:
        return true;
    }
    return false;
}

// ---- wp6-wasser (Stage mit Wasser, z. B. F_SP115; Link als Wolf oder Mensch): 6.11/6.12 Ruf im Wasser ----
// Suche wie wp6-ruf (Wasserflaeche mindestens 200 tief, Raster 500 in +-30000 um Link). Link wird 80 ueber die
// Wasserflaeche gesetzt (Test-Teleport); ins Wasser faellt er und schwimmt nach den Regeln des Spiels. Im Schwimmen 5
// Ruf-Versuche (R+B alle 20 Ticks). Soll: "call rejected (Wasser), B stays", Voegel 0, keine Verwandlung (Mensch).
// Danach zurueck an den Startort. Ohne Wasser im Suchgebiet endet das Szenario mit einer Notiz.
enum class WasserStep : uint8_t { Wait, Search, Go, Swim, Spam, Back, Done };
struct Wp6Wasser {
    WasserStep step = WasserStep::Wait;
    uint32_t t0 = 0;
    Wp6Ruf search;  // Suchzustand (Startort, Raster, Fund)
    uint32_t attempts = 0;
    uint32_t birdsMax = 0;
    uint32_t swimTicks = 0;
    bool wolfAtStart = false;
};
Wp6Wasser g_wsw;

bool wp6WasserTick(Context& c) {
    Wp6Wasser& w = g_wsw;
    const uint32_t t = c.frame;
    if (t >= kScenarioDeadline) {
        c.note("wp6-wasser: Zeitlimit erreicht");
        return true;
    }
    const uint32_t n = wp6Birds();
    w.birdsMax = std::max(w.birdsMax, n);
    daAlink_c* link = kff::liveLink();
    advanceEvent(c);
    if (link == nullptr) {
        return false;
    }
    switch (w.step) {
    case WasserStep::Wait:
        if (t >= kCallAt && !link->checkCargoCarry() && !link->checkEventRun() && !dComIfGp_event_runCheck() &&
            c.sess.birdId == kff::kNoProc && link->mLinkAcch.ChkGroundHit()) {
            w.search = Wp6Ruf();
            w.search.home = link->current.pos;
            w.wolfAtStart = link->checkWolf() != 0;
            w.step = WasserStep::Search;
            w.t0 = t;
            kff::logInfo("freiheit: WP6.11 Wasser-Suche um (%.0f,%.0f,%.0f), Link %s",
                static_cast<double>(w.search.home.x), static_cast<double>(w.search.home.y),
                static_cast<double>(w.search.home.z), w.wolfAtStart ? "Wolf" : "Mensch");
        }
        return false;
    case WasserStep::Search:
        wp6RufSearchWater(w.search, kWfBudget);
        if (w.search.iw >= (2 * kWfWaterHalf + 1) * (2 * kWfWaterHalf + 1)) {
            kff::logInfo("freiheit: WP6.11 Wasser-Suche fertig: %s (%.0f,%.0f,%.0f) Tiefe %.0f",
                w.search.waterFound ? "gefunden" : "kein Wasser", static_cast<double>(w.search.water.x),
                static_cast<double>(w.search.water.y), static_cast<double>(w.search.water.z),
                static_cast<double>(w.search.waterDepth));
            if (!w.search.waterFound) {
                c.note("wp6-wasser: kein Wasser im Suchgebiet");
                w.step = WasserStep::Done;
                return true;
            }
            w.step = WasserStep::Go;
            w.t0 = t;
        }
        return false;
    case WasserStep::Go:
        wp6Teleport(link, cXyz(w.search.water.x, w.search.water.y + 80.0f, w.search.water.z));
        c.note("wp6-wasser: Link ueber dem Wasser (Test-Teleport, das Schwimmen kommt vom Spiel)");
        w.step = WasserStep::Swim;
        w.t0 = t;
        w.swimTicks = 0;
        return false;
    case WasserStep::Swim: {
        const bool swim = link->checkModeFlg(daAlink_c::MODE_SWIMMING) != 0;
        w.swimTicks = swim ? w.swimTicks + 1 : 0;
        if (w.swimTicks >= 30 || t >= w.t0 + 300) {
            kff::logInfo("freiheit: WP6.11 Link %s (proc=%d, pos=(%.0f,%.0f,%.0f), Wasserflaeche %.0f), Ruf-Versuche folgen",
                swim ? "schwimmt" : "schwimmt NICHT", static_cast<int>(link->mProcID),
                static_cast<double>(link->current.pos.x), static_cast<double>(link->current.pos.y),
                static_cast<double>(link->current.pos.z), static_cast<double>(w.search.water.y));
            photo("wp6-wasser", t);
            w.step = WasserStep::Spam;
            w.t0 = t;
            w.attempts = 0;
        }
        return false;
    }
    case WasserStep::Spam:
        if ((t - w.t0) % 20 == 0 && w.attempts < 5) {
            ++w.attempts;
            wp6Chord(c);
            wp6LogAttempt("WP6.11 Wasser", c);
        }
        if (t >= w.t0 + 120) {
            kff::logInfo("freiheit: WP6.11 Ergebnis Wasser: %u Versuche, Voegel jetzt %u, max %u (Soll 0), Link %s -> %s, "
                         "Phase %s",
                w.attempts, n, w.birdsMax, w.wolfAtStart ? "Wolf" : "Mensch", link->checkWolf() ? "Wolf" : "Mensch",
                kff::phaseName(c.sess.phase));
            w.step = WasserStep::Back;
            w.t0 = t;
        }
        return false;
    case WasserStep::Back:
        if (t == w.t0 + 1) {
            wp6Teleport(link, cXyz(w.search.home.x, w.search.home.y + 5.0f, w.search.home.z));
        }
        if (t >= w.t0 + 120) {
            kff::logInfo("freiheit: WP6.11 Wasser Ende: Voegel max %u, Phase %s, Link %s", w.birdsMax,
                kff::phaseName(c.sess.phase), link->checkWolf() ? "Wolf" : "Mensch");
            c.note("autotest: timeline complete");
            w.step = WasserStep::Done;
            return true;
        }
        return false;
    case WasserStep::Done:
        return true;
    }
    return false;
}

// ---- wp6-doc (F_SP121; Link Wolf am Boden): 6.12 Ruf bei sichtbarem Dusklight-Dokument ----
// Ein modaler Dialog ueber den UiService (dialog_push) liegt auf dem Dokumentstapel des Hosts. is_any_document_visible
// meldet ihn wie jedes Host- oder Mod-Dokument, und der Host sperrt die Pad-Eingabe des Spiels (PADBlockInput). Die
// Injektion des Testbuilds wirkt nach dem Pad-Lesen und erreicht den Kern trotzdem. 3 Ruf-Versuche (R+B alle 30 Ticks).
// Soll: "call rejected (Dusklight-Dokument sichtbar), B stays", Voegel 0. Danach schliesst das Szenario den Dialog
// (dialog_close) und ruft erneut (Gegenprobe: Ritt), dann B und Abflug.
enum class DocStep : uint8_t { Wait, Spam, Close, Control, Ride, Depart, Done };
struct Wp6Doc {
    DocStep step = DocStep::Wait;
    uint32_t t0 = 0;
    UiDialogHandle dlg = 0;
    uint32_t attempts = 0;
    uint32_t visibleAttempts = 0;
    uint32_t birdsMax = 0;
    uint32_t controlTries = 0;
};
Wp6Doc g_wdoc;

bool wp6DocVisible() {
    bool v = false;
    return svc_ui != nullptr && svc_ui->is_any_document_visible(mod_ctx, &v) == MOD_OK && v;
}

// Knopf und Abbruch des Dialogs: im Test ohne echte Eingabe nicht erwartet, nur protokolliert.
void wp6DocPressed(ModContext*, UiDialogHandle, void*) {
    kff::logInfo("freiheit: WP6.12 Dokument: Knopf gedrueckt (unerwartet)");
    g_wdoc.dlg = 0;
}

void wp6DocDismiss(ModContext*, UiDialogHandle, void*) {
    kff::logInfo("freiheit: WP6.12 Dokument: per Abbruch geschlossen (unerwartet)");
    g_wdoc.dlg = 0;
}

void wp6DocClose(Wp6Doc& w) {
    if (w.dlg != 0 && svc_ui != nullptr) {
        const ModResult r = svc_ui->dialog_close(mod_ctx, w.dlg);
        kff::logInfo("freiheit: WP6.12 Dokument geschlossen: dialog_close -> %d", static_cast<int>(r));
    }
    w.dlg = 0;
}

bool wp6DocTick(Context& c) {
    Wp6Doc& w = g_wdoc;
    const uint32_t t = c.frame;
    if (t >= kScenarioDeadline) {
        wp6DocClose(w);
        c.note("wp6-doc: Zeitlimit erreicht");
        return true;
    }
    const uint32_t n = wp6Birds();
    daAlink_c* link = kff::liveLink();
    switch (w.step) {
    case DocStep::Wait:
        advanceEvent(c);
        if (t >= kCallAt && c.canCall() && link != nullptr && link->mLinkAcch.ChkGroundHit()) {
            if (svc_ui == nullptr) {
                c.note("wp6-doc: kein UiService, Szenario endet");
                w.step = DocStep::Done;
                return true;
            }
            static UiDialogAction act = UI_DIALOG_ACTION_INIT;
            act.label = "OK";
            act.on_pressed = &wp6DocPressed;
            UiDialogDesc d = UI_DIALOG_DESC_INIT;
            d.title = "Kargarok Testbuild";
            d.body_rml = "WP6.12: Ruf bei sichtbarem Dokument. Der Dialog schliesst sich selbst.";
            d.actions = &act;
            d.action_count = 1;
            d.on_dismiss = &wp6DocDismiss;
            const ModResult r = svc_ui->dialog_push(mod_ctx, &d, &w.dlg);
            kff::logInfo("freiheit: WP6.12 Dokument geoeffnet: dialog_push -> %d, sichtbar %d, voegel=%u, Link proc=%d",
                static_cast<int>(r), wp6DocVisible() ? 1 : 0, n, static_cast<int>(link->mProcID));
            if (r != MOD_OK) {
                w.dlg = 0;
                c.note("wp6-doc: dialog_push fehlgeschlagen");
                w.step = DocStep::Done;
                return true;
            }
            w.step = DocStep::Spam;
            w.t0 = t;
        }
        return false;
    case DocStep::Spam:
        w.birdsMax = std::max(w.birdsMax, n);
        if (t == w.t0 + 15) {
            photo("wp6-dokument", t);
        }
        if ((t - w.t0) % 30 == 20 && w.attempts < 3) {
            ++w.attempts;
            const bool vis = wp6DocVisible();
            if (vis) {
                ++w.visibleAttempts;
            }
            kff::logInfo("freiheit: WP6.12 Dokument sichtbar=%d vor dem Ruf", vis ? 1 : 0);
            wp6Chord(c);
            wp6LogAttempt("WP6.12 Dokument", c);
        }
        if (t >= w.t0 + 140) {
            kff::logInfo("freiheit: WP6.12 Ergebnis Dokument: %u Versuche (%u bei sichtbarem Dokument), Voegel jetzt %u, "
                         "max %u (Soll 0), Phase %s",
                w.attempts, w.visibleAttempts, n, w.birdsMax, kff::phaseName(c.sess.phase));
            w.step = DocStep::Close;
            w.t0 = t;
        }
        return false;
    case DocStep::Close:
        wp6DocClose(w);
        kff::logInfo("freiheit: WP6.12 Dokument nach dem Schliessen sichtbar=%d", wp6DocVisible() ? 1 : 0);
        w.step = DocStep::Control;
        w.t0 = t;
        return false;
    case DocStep::Control:
        advanceEvent(c);
        if (c.sess.phase == kff::Phase::Riding) {
            kff::logInfo("freiheit: WP6.12 Gegenprobe ohne Dokument: Ruf angenommen, Ritt laeuft (%u Ticks nach dem "
                         "Schliessen, Versuch %u)",
                t - w.t0, w.controlTries);
            w.step = DocStep::Ride;
            w.t0 = t;
            return false;
        }
        if (c.sess.phase == kff::Phase::Idle && (t - w.t0) % 60 == 59 && w.controlTries < 5 && c.canCall()) {
            ++w.controlTries;
            wp6Chord(c);
            wp6LogAttempt("WP6.12 Gegenprobe", c);
        }
        if (t >= w.t0 + 900) {
            kff::logInfo("freiheit: WP6.12 Gegenprobe: kein Ritt nach %u Versuchen", w.controlTries);
            w.step = DocStep::Depart;
            w.t0 = t;
        }
        return false;
    case DocStep::Ride:
        if (c.sess.phase != kff::Phase::Riding) {
            w.step = DocStep::Depart;
            w.t0 = t;
            return false;
        }
        if (t == w.t0 + 60 || (t > w.t0 + 400 && t % 60 == 0)) {
            c.tapB();
        }
        return false;
    case DocStep::Depart:
        if ((n == 0 && c.sess.phase == kff::Phase::Idle) || t >= w.t0 + 900) {
            kff::logInfo("freiheit: WP6.12 Dokument Ende: Voegel jetzt %u, Phase %s", n, kff::phaseName(c.sess.phase));
            c.note("autotest: timeline complete");
            w.step = DocStep::Done;
            return true;
        }
        return false;
    case DocStep::Done:
        return true;
    }
    return false;
}

const kff::autotest::Scenario kScenWp6Decke = {
    "wp6-decke",
    "WP6.1/6.2/6.4: R_SP107 Steigen ueber die Vanilla-Grenze, Dash unter der Decke, Absteigen, Neustart-Zaehler",
    wp6DeckeTick,
};
const kff::autotest::Scenario kScenWp6Wand = {
    "wp6-wand",
    "WP6.5/6.7: F_SP121 Haengen an der Bergwand (Stick neutral, Waechter), Wandprofil, Tunnel-Probe +300/+600",
    wp6WandTick,
};
const kff::autotest::Scenario kScenWp6Rand = {
    "wp6-rand",
    "WP6.6/6.8: F_SP121 Kartenrand nach -X in Gang 5 mit Turbo, danach Steigen auf y > 100000",
    wp6RandTick,
};
const kff::autotest::Scenario kScenWp6Ruf = {
    "wp6-ruf",
    "WP6.3/6.11: Ruf unter niedriger Decke und im Wasser (abgelehnt), Ruf am freien Ort, Ruf-Spam im Ritt",
    wp6RufTick,
};
const kff::autotest::Scenario kScenWp6Timeout = {
    "wp6-timeout",
    "WP6.9/6.10: Ruf ohne Ritt-Start (KFF_TEST_NORIDE=1), Zeitgrenze, schnelles B mit R+B danach",
    wp6TimeoutTick,
};
const kff::autotest::Scenario kScenWp6Gate = {
    "wp6-gate",
    "WP6.12: Ruf-Gates mit echten Ausloesern (Ereignis, Startflanke, Pause, Luft, Sturz, Zielsperre, Reiten)",
    wp6GateTick,
};
const kff::autotest::Scenario kScenWp6Wasser = {
    "wp6-wasser",
    "WP6.11/6.12: Ruf im Wasser (Link schwimmt, Wolf oder Mensch), Soll call rejected (Wasser)",
    wp6WasserTick,
};
const kff::autotest::Scenario kScenWp6Doc = {
    "wp6-doc",
    "WP6.12: Ruf bei sichtbarem Dusklight-Dokument (Dialog ueber den UiService), Gegenprobe nach dem Schliessen",
    wp6DocTick,
};

#endif  // KFF_DEV_AUTOTEST (Szenarien)

// ---------------------------------------------------------------------------------------------
// Modul
// ---------------------------------------------------------------------------------------------

#if defined(KFF_DEV_AUTOTEST)
// Testbuild WP7.8: Platzhalter fuer das Ziel mit fehlendem Symbol. Wird nie eingetragen (Installation bricht vorher ab).
HookAction onTestBadNamePre(ModContext*, void*, void*, void*) {
    return HOOK_CONTINUE;
}
#endif

class FreiheitModule final : public kff::Module {
public:
    const char* name() const override { return "freiheit"; }

    ModResult install(kff::Registry& reg) override {
        ModResult worst = MOD_OK;
        auto note = [&worst](const char* what, ModResult r) {
            if (r != MOD_OK) {
                kff::logWarn("freiheit: %s not installed (%d), module disabled by core", what, static_cast<int>(r));
                worst = r;
            }
        };
#if defined(KFF_DEV_AUTOTEST)
        // Testbuild WP7.8 (KFF_TEST_BADNAME=1): das erste Ziel hat kein Symbol. Die Installation bricht hier ab, damit kein
        // halber Satz Hooks eingetragen wird. Das Modul wird abgeschaltet, sein Zaehler bleibt 0.
        if (kff::testFlag("KFF_TEST_BADNAME")) {
            const ModResult bad = reg.addPre(kff::Hook::TestBadName, &onTestBadNamePre, kff::prio::kFreiheit);
            kff::logError("MOD_ERROR: freiheit install abgebrochen (Testbuild, fehlendes Symbol) -> %s",
                kff::modResultName(bad));
            return MOD_ERROR;
        }
#endif
        note("checkRoofHeight pre (SKIP, kNoRoof)", reg.addPre(kff::Hook::KagoCheckRoofHeight, &onRoofPre, kff::prio::kFreiheit));
        note("setSceneChange pre", reg.addPre(kff::Hook::KagoSetSceneChange, &onSetSceneChangePre, kff::prio::kFreiheit));
        note("executeFly pre (Hoehen)", reg.addPre(kff::Hook::KagoExecuteFly, &onExecuteFlyPre, kff::prio::kFreiheit));
        note("dStage_changeScene pre", reg.addPre(kff::Hook::StageChangeScene, &onStageChangeScenePre, kff::prio::kFreiheit));
        note("dComIfGp_setNextStage pre", reg.addPre(kff::Hook::StageNextStage, &onStageNextStagePre, kff::prio::kFreiheit));
        note("checkFlySceneChange pre", reg.addPre(kff::Hook::KagoCheckFlySceneChange, &onFlySceneChangePre, kff::prio::kFreiheit));
        note("checkSceneChange pre (WP1)", reg.addPre(kff::Hook::AlinkCheckSceneChange, &onCheckSceneChangePre, kff::prio::kFreiheit));
        note("startRestartRoom pre (WP1/WP6, einziger Besitzer)",
            reg.addPre(kff::Hook::AlinkStartRestartRoom, &onStartRestartRoomPre, kff::prio::kFreiheit));
        note("daTagRetRm_c::execute pre (WP1)", reg.addPre(kff::Hook::TagRetRmExecute, &onTagRetRmPre, kff::prio::kFreiheit));
        note("dStage_changeScene4Event pre (WP1)", reg.addPre(kff::Hook::StageChangeScene4Event, &onChangeScene4EventPre, kff::prio::kFreiheit));
        note("mDoCPd_c::read post (WP1)", reg.addPost(kff::Hook::PadRead, &onPadReadPost, kff::prio::kFreiheit));
#if defined(KFF_CORE_RESTART_HOOKS)
        note("checkRestartRoom pre (Kern-Patch P-RESTART)", reg.addPre(kff::Hook::AlinkCheckRestartRoom, &onRestartRoomPre, kff::prio::kFreiheit));
        note("checkRoofRestart pre (Kern-Patch P-RESTART)", reg.addPre(kff::Hook::AlinkCheckRoofRestart, &onRoofRestartPre, kff::prio::kFreiheit));
#endif
        note("daKago_c::execute pre (Frame-Start)", reg.addPre(kff::Hook::KagoExecute, &onBirdExecutePre, kff::prio::kFreiheit));
        note("daKago_c::execute post", reg.addPost(kff::Hook::KagoExecute, &onBirdExecutePost, kff::prio::kFreiheit));
        note("daAlink_c::execute post", reg.addPost(kff::Hook::LinkExecute, &onLinkExecutePost, kff::prio::kFreiheit));
#if defined(KFF_DEV_AUTOTEST)
        note("daAlink_c::setDamagePoint pre (Schadensquelle, Testbuild)",
            reg.addPre(kff::Hook::LinkSetDamagePoint, &onLinkSetDamagePointPre, kff::prio::kFreiheit));
        note("scenario innen", kff::autotest::registerScenario(&kScenInnen));
        note("scenario fuss", kff::autotest::registerScenario(&kScenFuss));
        note("scenario grenze", kff::autotest::registerScenario(&kScenGrenze));
        note("scenario szene", kff::autotest::registerScenario(&kScenSzene));
        note("scenario freiheit", kff::autotest::registerScenario(&kScenFreiheit));
        note("scenario wand5", kff::autotest::registerScenario(&kScenWand5));
        note("scenario wand5s", kff::autotest::registerScenario(&kScenWand5s));
        note("scenario ladezone-anflug", kff::autotest::registerScenario(&kScenWp1Ladezone));
        note("scenario event39-kanal", kff::autotest::registerScenario(&kScenWp1Event39));
        note("scenario boden-absteigen", kff::autotest::registerScenario(&kScenWp1Boden));
        note("scenario void-absteigen", kff::autotest::registerScenario(&kScenWp1Void));
        note("scenario abgrund-faelle", kff::autotest::registerScenario(&kScenWp1Abgrund));
        note("scenario warp-portal", kff::autotest::registerScenario(&kScenWp1Warp));
        note("scenario raumwechsel", kff::autotest::registerScenario(&kScenWp1Raum));
        note("scenario rueckkehr-raum-tag", kff::autotest::registerScenario(&kScenWp1RetRm));
        note("scenario wp6-decke", kff::autotest::registerScenario(&kScenWp6Decke));
        note("scenario wp6-wand", kff::autotest::registerScenario(&kScenWp6Wand));
        note("scenario wp6-rand", kff::autotest::registerScenario(&kScenWp6Rand));
        note("scenario wp6-ruf", kff::autotest::registerScenario(&kScenWp6Ruf));
        note("scenario wp6-timeout", kff::autotest::registerScenario(&kScenWp6Timeout));
        note("scenario wp6-gate", kff::autotest::registerScenario(&kScenWp6Gate));
        note("scenario wp6-wasser", kff::autotest::registerScenario(&kScenWp6Wasser));
        note("scenario wp6-doc", kff::autotest::registerScenario(&kScenWp6Doc));
#endif
        return worst;
    }

    void onRideStart(kff::Session& s, daKago_c* bird) override {
        (void)s;
        g_frameValid = false;
        if (bird != nullptr) {
            // WP6: schon der erste Ritt-Frame laeuft ohne CrrPos und mit Abtastung (Start im Pre des Vogel-Execute).
            crrPosOff(bird);
            g_frameStart = bird->current.pos;
            g_frameValid = true;
            resetStuck(bird->current.pos);
        }
#if defined(KFF_DEV_AUTOTEST)
        g_releaseStart = 0;
#endif
        g_stallCount = 0;
        g_stallSink = false;
        g_low = false;
        g_escHold = 0;
        g_birdOwned = 0;
        g_linkOwned = 0;
#if defined(KFF_DEV_AUTOTEST)
        g_freiStartSet = false;  // Zustand des Testszenarios freiheit
        g_freiPhase = FreiPhase::Approach;
        g_freiEdgeMarked = false;
#endif
        g_fotoWand = false;
        g_fotoDecke = false;
        forgetWalls();
        kff::logInfo("freiheit: Ritt beginnt (Decke %s, Gleiten an Flaechen, Untergrenze %.0f ueber Gelaende, "
                     "Szenen im Ritt gesperrt, Kursdrehung durch freiheit %s)",
            roofKept() ? "bleibt" : "aus", static_cast<double>(clearance()),
            wallSteerLabel());
    }

    // Kein Ritt-Ende durch freiheit: Ereignisse beendet der Kern (kff::core, endRideClean). Hier nur die Acch-Bits.
    void onFrameBird(kff::Session& s, daKago_c* bird) override {
        (void)s;
        if (bird == nullptr) {
            return;
        }
        g_frameStart = bird->current.pos;
        g_frameValid = true;
        ownBits(bird->mObjAcch.m_flags, g_birdOwned, birdBits());
        crrPosOff(bird);  // WP6: CrrPos im Ritt aus (field_0x6dc), Wert davor wird am Ritt-Ende zurueckgegeben
        fopAcM_OffStatus(bird, fopAcStts_UNK_0x20_e);  // keine Loeschung durch die 5000-Regel
    }

    void onFrameLink(kff::Session& s, daAlink_c* link) override {
        (void)s;
        if (link == nullptr) {
            return;
        }
        // WP6: der Stillstands-Waechter hat angeschlagen (Vogel-Post). Ritt-Ende im Link-Frame wie die Kern-Enden.
        if (g_stuckPending) {
            g_stuckPending = false;
            if (rideRunning() && !linkDead(link)) {
                ++g_wp6.stuckEnds;
                notice("The bird got stuck and the ride ended. Keep the Control Stick moving to avoid this.");
                kff::endRideClean(kff::EndReason::Stuck, "watchdog: Stillstand ohne Stick");
            }
        }
        if (rideRunning() && link->checkCargoCarry()) {
            uint32_t bits = dBgS_Acch::FLAG_WALL_NONE;
            if (!roofKept()) {
                bits |= kRoofBit;
            }
            ownBits(link->mLinkAcch.m_flags, g_linkOwned, bits);
        } else {
            releaseBits(link->mLinkAcch.m_flags, g_linkOwned);
        }
        // Link-Fuss gegen das Gelaende, auch nach dem Absteigen (Acch-Bits zurueckgenommen, Link muss stehen).
        // Messfenster linkWatching(): Ritt und kLinkWatchTicks Kern-Ticks danach (F06).
        // y = 0: Link im ersten Ritt-Frame noch nicht gesetzt, dieser Wert ist kein Messwert.
        if (linkWatching() && !inEvent() && !linkDead(link) && link->current.pos.y != 0.0f) {
            const float gap = groundGap(link->current.pos);
            if (gap < g_stats.minLinkGap) {
                g_stats.minLinkGap = gap;
            }
            if (gap < -2.0f) {
                ++g_stats.linkBelowGround;
                kff::logEvery(g_logLinkGnd, 3, 120, "freiheit: Link unter dem Gelaende (y=%.1f, Abstand %.1f, Ritt %s)",
                    static_cast<double>(link->current.pos.y), static_cast<double>(gap),
                    rideRunning() ? "ja" : "nein");
            }
        }
    }

    void onRideEnd(kff::Session& s, daKago_c* bird, kff::EndReason why) override {
        (void)s;
#if defined(KFF_DEV_AUTOTEST)
        wp1::g_endSeen = true;  // Grund des Ritt-Endes fuer die WP1-Szenarien
        wp1::g_endWhy = why;
#endif
        // Protokoll (WP1): der naechste Stage-Wechsel gilt diesem Ende ("scene change allowed after end").
        g_endOpen = true;
        g_endTick = g_tick;
        g_endWhy = why;
        g_linkWatchArmed = true;            // F06: Link-Fuss-Messung laeuft noch kLinkWatchTicks Kern-Ticks nach diesem Ende
        g_linkWatchEnd = kff::timers::now();
        if (!g_endTextOwn) {
            g_endText[0] = '\0';
        }
        g_endTextOwn = false;
        if (bird != nullptr) {
            const float gap = groundGap(bird->current.pos);
            kff::logInfo("freiheit: Ritt-Ende (%s) Endposition Vogel=(%.1f,%.1f,%.1f) Bodenabstand %.1f",
                kff::endReasonName(why), static_cast<double>(bird->current.pos.x),
                static_cast<double>(bird->current.pos.y), static_cast<double>(bird->current.pos.z),
                static_cast<double>(gap >= 1.0e8f ? -1.0f : gap));
            releaseBits(bird->mObjAcch.m_flags, g_birdOwned);
        } else {
            g_birdOwned = 0;
        }
        crrPosRestore(bird);  // WP6 (R14): nur den eigenen Wert von field_0x6dc zuruecknehmen
        if (daAlink_c* link = kff::liveLink()) {
            releaseBits(link->mLinkAcch.m_flags, g_linkOwned);
        } else {
            g_linkOwned = 0;
        }
        g_frameValid = false;
        g_stallCount = 0;
        g_stallSink = false;
        g_low = false;
        g_escHold = 0;
        g_stuckPending = false;
        g_stuckTicks = 0;
        g_rideEndedOnce = true;          // WP6.4: erster Vanilla-Neustart-Aufruf nach diesem Ritt kommt ins Log
        g_roofRestartLogged = false;
        g_restartOutsideLogged = false;
    }

    void onShutdown(kff::Session& s) override {
        (void)s;
        if (daKago_c* bird = kff::liveBird()) {
            releaseBits(bird->mObjAcch.m_flags, g_birdOwned);
            crrPosRestore(bird);
        } else {
            g_birdOwned = 0;
            crrPosRestore(nullptr);
        }
        if (daAlink_c* link = kff::liveLink()) {
            releaseBits(link->mLinkAcch.m_flags, g_linkOwned);
        } else {
            g_linkOwned = 0;
        }
        kff::logInfo("freiheit: SUMMARY roof=%u birdSceneVeto=%u sceneCancel=%u stageVeto=%u restartVeto=%u "
                     "roofRestartVeto=%u",
            g_stats.roofOverride, g_stats.birdSceneVeto, g_stats.sceneCancel, g_stats.stageVeto, g_stats.restartVeto,
            g_stats.roofRestartVeto);
        kff::logInfo("freiheit: SUMMARY2 frames=%u gleiten=%u glideFrames=%u schritte=%u wandSteer=%u "
                     "ausweichGrad=%.1f bodenlift=%u wasserlift=%u stillstand=%u durchdringungen=%u linkUnterGrund=%u "
                     "exceptions=%u eckenLenkung=%u",
            g_stats.frames, g_stats.slideFrames, g_stats.glideFrames, g_stats.slideSteps, g_stats.wallSteer,
            static_cast<double>(g_stats.steerDeg), g_stats.groundClamp, g_stats.waterClamp, g_stats.stallBreak,
            g_stats.penetration, g_stats.linkBelowGround, g_stats.exceptions, g_stats.cornerSteer);
        kff::logInfo("freiheit: SUMMARY3 stillstandMaxFrames=%u stallClimb=%u stallSink=%u linkHebung=%u durchflugKorr=%u",
            g_stats.stallRunMax, g_stats.stallClimb, g_stats.stallSink, g_stats.linkClamp, g_stats.penetrationFix);
        kff::logInfo("freiheit: SUMMARY4 e6 ausweichStarts=%u sinkSchritte=%u ausweichBlockiert=%u riseSchritte=%u "
                     "riseBlockiert=%u linkFussMinAusweichen=%.1f ausweichTiefeMax=%.1f wandKontakt=%u deckenKontakt=%u",
            g_stats.escapeStarts, g_stats.stallSink, g_stats.escapeBlocked, g_stats.riseSteps, g_stats.riseBlocked,
            static_cast<double>(g_stats.escLinkMin), static_cast<double>(g_stats.escapeDepthMax), g_stats.wallContacts,
            g_stats.ceilContacts);
        kff::logInfo("freiheit: SUMMARY5 szenen vetoZeilen=%u erlaubtNachEnde=%u eigeneEnden=%u ohneEinordnung=%u "
                     "checkSceneChange=%u/%u restart=%u/%u roofRestart=%u/%u startRestart=%u/%u(imRitt %u) "
                     "retRm=%u/%u(uebersprungen %u) changeScene=%u/%u vogel=%u/%u flug=%u/%u anstehend=%u/%u "
                     "sceneSkips=%u ticks=%u",
            g_vetoLines, g_allowLines, g_ownEnds, g_unsorted, g_veto[kVsCheckScene].lines, g_veto[kVsCheckScene].tries,
            g_veto[kVsRestart].lines, g_veto[kVsRestart].tries, g_veto[kVsRoofRestart].lines,
            g_veto[kVsRoofRestart].tries, g_veto[kVsStartRestart].lines, g_veto[kVsStartRestart].tries, g_restartInRide,
            g_veto[kVsRetRm].lines, g_veto[kVsRetRm].tries, g_retRmSkips, g_veto[kVsChangeScene].lines,
            g_veto[kVsChangeScene].tries, g_veto[kVsBirdScene].lines, g_veto[kVsBirdScene].tries,
            g_veto[kVsFlyScene].lines, g_veto[kVsFlyScene].tries, g_veto[kVsPending].lines, g_veto[kVsPending].tries,
            g_sceneSkips, g_tick);
        const Wp6Stats& w = g_wp6;
        kff::logInfo("freiheit: SUMMARY6 roofSkip=%u crrSkipFrames=%u crrPosRuns=%u restartCalls=%u restartVeto=%u "
                     "restartPassed=%u restartOutside=%u roofRestartOutside=%u stuckEnds=%u stuckRunMax=%u skipFrames=%u",
            w.roofSkip, w.crrSkipFrames, w.crrPosRuns, w.restartCalls, w.restartVeto, w.restartPassed, w.restartOutside,
            w.roofRestartOutside, w.stuckEnds, w.stuckRunMax, w.skipFrames);
#if defined(KFF_DEV_AUTOTEST)
        kff::logInfo("freiheit: SUMMARY6T roofNotNone=%u kNoRoofAktionen=0x%x kNoRoofModi=0x%x tunnelFrames=%u "
                     "e10Befreiungen=%u e10MaxFrames=%u schaden=%u schadenRitt=%u",
            w.roofNotNone, w.roofActions, w.roofModes, w.tunnelFrames, w.releases, w.releaseMax, g_dmgCount,
            g_dmgRide);
#endif
    }
};

FreiheitModule g_module;

}  // namespace

namespace kff::modules {

Module& freiheit() {
    return g_module;
}

}  // namespace kff::modules
