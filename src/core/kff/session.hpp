// Sitzung des Rufs: ein Vogel pro Sitzung, Zustand nur ueber Prozess-IDs.
//
// Regeln fuer alle Dateien:
//  - Zeiger auf Link oder Vogel nie speichern. liveLink() und liveBird() suchen jeden Aufruf neu.
//  - Die Phase setzt der Kern. Vor dem Ruf darf ein Modul (geplant autowolf) Transforming setzen und muss es selbst wieder aufloesen (summonBird oder resetSession), sonst blockiert es jeden Ruf.
//  - Aufraeumen: releaseRider() vor jedem Loeschen oder Szenenwechsel.
//  - Schreibrechte der Felder: siehe Kommentar je Feld (Schnittstellenvertrag Teil 2).
#pragma once

#include "core/kff/services.hpp"

#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"

#include <cstdint>

class daAlink_c;
class daKago_c;

namespace kff {

// Ungueltige Prozess-ID (kein Link, kein Vogel).
constexpr fpc_ProcID kNoProc = 0xFFFFFFFFu;
// mType des freundlichen Kargarok (TYPE_NORMAL, d_a_kago.cpp:30). Der Wildvogel hat einen anderen Typ.
inline constexpr uint8_t kFriendlyKagoType = 1;

enum class Phase : uint8_t {
    Idle,          // kein eigener Vogel
    Summoning,     // Vogel angelegt (fopAcM_create), wartet auf WAIT ohne Ereignis
    Transforming,  // Link verwandelt sich vor dem Ruf (Modul autowolf); noch kein Vogel
    Riding,        // Link sitzt im Vogel (Cargo-Carry), Ritt-Frames laufen
    Dismounting,   // abgestiegen (nach Landeanflug oder sofort): Vogel fliegt weg, danach Entfernen
};

// Warum ein Ritt endet. Wird an Module in onRideEnd() uebergeben.
enum class EndReason : uint8_t {
    DismountButton,   // B im Flug
    RiderLost,        // Link ohne Cargo-Carry ueber der Schwelle
    Event,            // Ereignis startet waehrend des Ritts (Kern, linkExecutePre) oder Szenenwechsel per Ereignis
    LinkChanged,      // neuer Link-Akteur (Szenenwechsel) oder Aufraeumen
    DeleteRequested,  // Entfernen durch das Paket nach dem Absteigen
    BirdGone,         // Vogel verschwunden, nicht ueber den eigenen Pfad entfernt
    Shutdown,         // Entladen der Mod
    Death,            // Link tot (Game Over, Tod im Ritt)
    Menu,             // Menue oder Titel: Szenenwechsel aus dem Pausenmenue (Titel, Laden, Beenden)
    Warp,             // Warp (Midna, Ooccoo, Portal): der Ritt endet beim Warp-Start (procCoWarpInit, Entscheidung D1)
    Transform,        // Verwandlung ueber den Spielweg mit eigenem Vogel (Modul griff, Pre procCoMetamorphoseInit, WP4)
    Stuck,            // Stillstands-Waechter (Modul freiheit, WP6): Vogel steht ohne Stick-Eingabe fest
    Reset,            // Soft-Reset im Ritt (Reset-Callback), Ritt-Ende vor dem Titel
};

const char* phaseName(Phase phase);
const char* endReasonName(EndReason reason);

// Lagezustand des Vogels (frueher g_att im Modul flug). Schreibt nur flug; lesen kamera, griff, mensch, tempo.
// Ein Ritt, ein Satz Werte. Zurueckgesetzt beim Ritt-Start (Kern, startRide).
struct AttitudeState {
    long long cumPitch = 0;  // Summe der Nickschritte, + = Nase unten (Vanilla-Vorzeichen)
    long long cumYaw = 0;    // Summe der Gierschritte
    long long cumRoll = 0;   // Summe der Rollschritte
    int16_t roll = 0;        // Rollwinkel des Koerpers, s16, nie geklemmt
    int16_t bank = 0;        // sichtbare Schraeglage, folgt der Gierrate
    int32_t barrelLeft = 0;  // Flugschritte der laufenden Fassrolle, 0 = keine
    int32_t barrelDir = 1;   // +1 rechts, -1 links
    uint32_t barrels = 0;    // begonnene Fassrollen in diesem Ritt
    uint32_t modeAway = 0;   // Flugframes mit mMode != 2 (Anlauf nach dem Ritt-Start)
    uint32_t wfFrames = 0;   // Flugframes im Wasserfall
    float stickX = 0.0f;     // Stick X dieses Frames (Gier und Rollrichtung)
    float stickY = 0.0f;     // Stick Y dieses Frames nach invert_steer (Nick)
    uint32_t trimFrames = 0;  // Flugframes mit neutralem Stick Y (Trimmung aktiv), Schreiber flug
    bool trimming = false;    // Trimmung dieses Frames, Schreiber flug
    float speed = 0.0f;       // V: eigene Geschwindigkeit in Einheiten je Frame, vor dem Gang, Schreiber flug
    int16_t lastDYaw = 0;     // Gierschritt, den flug im letzten Flugschritt gesetzt hat (Messung der Fremdgier)
    float hoverVy = 0.0f;     // senkrechte Rate im Schweben (Einheiten je Frame), Schreiber flug. Ritt-Start setzt sie auf 0
};

struct Session {
    // Identitaet (Prozess-IDs, keine Zeiger).
    fpc_ProcID linkId = kNoProc;   // letzter gesehener Link-Akteur (daAlink_c)
    fpc_ProcID birdId = kNoProc;   // eigener freundlicher Kargarok (mType 1)
    Phase phase = Phase::Idle;

    // Pro Tick vom Kern gesetzt (in padReadPost, nie latchend): Ruf angenommen, B-Tipp im Ritt.
    // callRequested wird im Link-Pre-Hook verarbeitet, dismountRequested im Vogel-Pre-Hook.
    bool callRequested = false;
    bool dismountRequested = false;

    // Mensch-Ritt im Griff (Modul mensch): Link sitzt NICHT im Cargo (kein setPlayerRideOn), sondern haelt den
    // Vogel ueber den Greifslot. Gesetzt direkt nach summonBird, geloescht mit resetSession. Der Kern prueft dann
    // nicht checkCargoCarry, sondern verlaesst sich auf den Griff-Waechter des Moduls.
    bool rideGrip = false;

    // Zaehler und Zeiten (Frames der jeweiligen Hooks).
    uint32_t linkCalls = 0;        // daAlink_c::execute, Pre-Hook, alle Aufrufe
    uint32_t birdAge = 0;          // Kern-Ticks seit dem Ruf (kernTimersTick, WP9 R4)
    bool birdSeen = false;         // der Vogel-Akteur existierte mindestens einmal
    uint32_t birdCalls = 0;        // daKago_c::execute des eigenen Vogels, Pre-Hook
    uint32_t rideFrames = 0;       // Vogel-Frames seit Ritt-Start (Phase Riding)
    uint32_t departFrames = 0;     // Kern-Ticks (Pad-Lesungen) seit dem Absteigen (Phase Dismounting), zaehlt padReadPost
    uint32_t riderLost = 0;        // Kern-Ticks in Folge ohne Cargo-Carry (Phase Riding, kernTimersTick)
    uint32_t deleteWait = 0;       // Frames seit dem letzten Entfern-Wiederholen
    bool deleteRequested = false;  // fopAcM_delete wurde auf den eigenen Vogel angewendet
    int32_t lastAction = -1;       // mAction des Vogels im letzten Ritt-Frame (nur Log)
    uint32_t landFrames = 0;       // Landeanflug nach B mit Boden darunter: Kern-Ticks seit dem B-Tipp (kernTimersTick), 0 = kein Landeanflug
    bool landing = false;  // Kern: B-Landeanflug aktiv – freiheit setzt die Untergrenze aus

    // Kurs- und Hoehenwerte fuer Logs (Yaw ungewickelt in s16-Einheiten).
    bool yawInit = false;
    int16_t yawPrev = 0;
    int64_t yawUnwrapped = 0;
    float yawMinDeg = 0.0f;
    float yawMaxDeg = 0.0f;
    float birdMinY = 0.0f;
    float birdMaxY = 0.0f;

    // Schnittstellenvertrag Teil 2 (Schreiber je Feld). Der Kern setzt att, speedScale, gear und fovExtraDeg beim Ritt-Start
    // zurueck (startRide); danach schreiben flug bzw. tempo. Module lesen nur.
    AttitudeState att;         // schreibt flug
    float speedScale = 0.5f;   // Tempo-Faktor 0..1.0 (0 = Schweben, 1.0 = Hoechsttempo); schreibt tempo, Anwendung im Flug/Post-Hook
    int gear = 3;              // Anzeige-Stufe 1..5, aus dem Tempo abgeleitet; schreibt tempo
    float fovExtraDeg = 0.0f;  // Sichtfeld-Zusatz 0..10 Grad; schreibt tempo, wendet kamera an
    // Schweben (Tempo 0) aktiv und nicht im Landeanflug. Schreibt tempo (Eintritt, Austritt, Ritt-Start und -Ende), lesen
    // flug, kamera und freiheit. resetSession setzt das Feld nicht; tempo setzt es in onRideStart und onRideEnd zurueck.
    bool hover = false;
};

// Die eine Sitzung dieser Mod-Instanz.
Session& session();

// Frische Suche nach den Akteuren. nullptr, wenn der Akteur fehlt oder der Typ nicht passt.
daAlink_c* liveLink();
daKago_c* liveBird();
// true, wenn k der eigene Vogel ist (Prozess-ID und mType == kFriendlyKagoType).
bool isOwnBird(daKago_c* k);
// true, solange Link im Vogel sitzt (Phase Riding).
bool isRiding();

// Aktionen des Kerns (v1-Paritaet). Module duerfen sie aufrufen, keine Zeiger speichern.

// Legt den freundlichen Vogel neben Link an (Phase Summoning). Nur mit Wolf-Link aufrufen. Ruf im Abflug (N1, Befund 1 F2):
// der abfliegende Vogel wird entfernt, der neue entsteht erst nach dessen _delete-Hook. MOD_OK heisst dann angenommen.
ModResult summonBird(daAlink_c* link);
// Absteigen. Nur in Phase Riding. Mit B und Boden darunter zuerst Landeanflug (der Vogel sinkt, hoechstens ~2 s,
// Steuerungstabelle Zeile 24). Sonst, bei Abgrund oder fuer andere Gruende, sofort: onRideEnd, Link und Midna
// loesen, Vogel fliegt weg, danach requestDelete (Phase Dismounting).
void dismount(daKago_c* bird, EndReason reason, const char* why);
// Entfernen des eigenen Vogels per fopAcM_delete. Ist Link noch im Ritt, wird er vorher geloest.
// Reihenfolge: onRideEnd, Link loesen, Vogel in WAIT mit Flag 6DF = 1, dann fopAcM_delete.
void requestDelete(EndReason reason, const char* why);
// Zentrales Ritt-Ende ohne Abflugflug (Tod, Menue, Szenenwechsel durch Ereignis): requestDelete, dann resetSession.
void endRideClean(EndReason reason, const char* why);
// setRideOff(), wenn Link noch im Cargo-Carry sitzt.
void releaseRider(daKago_c* bird);
// Sitzung auf Idle setzen (Zaehler der Sitzung bleiben). Vorher onRideEnd selbst aufrufen, falls noetig.
void resetSession();
// N1 (12.10.2026): eigener Vogel fliegt ab. true bei Phase Dismounting oder bei angefordertem Entfernen des eigenen
// Vogels, nicht im Landeanflug (landFrames == 0). Ein angenommener Ruf nimmt den abfliegenden Vogel dann sofort weg
// (summonBird, requestDelete ueber die Prozess-ID), statt bis zum Ende des Abflugs zu warten.
bool birdDeparting(const Session& s);
// true, wenn dieser Vogel zum Entfernen vorgemerkt ist (requestDelete bis zum _delete-Hook). Solche Voegel gelten nicht
// als verwaiste eigene Voegel (Stray) und blockieren keinen Ruf.
bool isPendingBird(fpc_ProcID id);

}  // namespace kff
