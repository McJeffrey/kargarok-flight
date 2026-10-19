# Kargarok Flight 2.1.0: Entwicklerdokumentation

Stand: 12.10.2026. Für Modder und Entwickler. Gilt für Dusklight 2.0.3 (macOS arm64), Mod-ID `com.kaizuki.kargarok_flight`, Version 2.1.0.

Dieses Dokument beschreibt den Code und seine Schnittstellen. Gameplay-Angaben mit Messwerten stehen in der README unter „Bekannte Grenzen“. Zeilenangaben beziehen sich auf die Dateien in `src/` dieses Pakets, sofern nicht anders angegeben.

## 1. Überblick

Kargarok Flight ist ein Mod-Paket für Dusklight 2.0.3. Es besteht aus einem Kern (`src/core/`) und sieben Modulen (`src/modules/`):

- Der Kern verwaltet den Ritt-Zustand, den Ruf, das Absteigen, die Ritt-Enden, die Speichersperre und die Registrierung der Hooks.
- Die Module liefern Flugmodell (`flug`), Höhen- und Wandregeln (`freiheit`), Griff (`griff`), Gang und Tempo (`tempo`), Kamera (`kamera`), den Menschen-Ritt (`mensch`) und die Verwandlung zum Wolf (`autowolf`).

Module sprechen nicht direkt miteinander. Sie teilen sich den Zustand über die Sitzung (`kff::Session`, Abschnitt 6) und die Konfiguration (Abschnitt 9).

## 2. Dateien

| Pfad | Inhalt |
|---|---|
| `mod.json` | Mod-ID, Version, Name, Beschreibung. Die Beschreibung nennt noch „Version 2: modularer Kern“ (veraltet, Abschnitt 16). |
| `CMakeLists.txt` | Build, Testbuild-Option, Strip auf macOS (Abschnitt 12). |
| `res/` | Ressourcen des Pakets (`RES_DIR`). Enthält nur `.gitkeep`. |
| `.gitignore` | Ignoriert `build/` und `bld-*/`. |
| `src/core/kff/` | Schnittstellen-Header: `services.hpp`, `hooks.hpp`, `session.hpp`, `module.hpp`, `config.hpp`, `log.hpp`, `input.hpp`, `ride.hpp`, `kff.hpp`, `fakepath.hpp`, `autotest.hpp`. |
| `src/core/core.cpp` | `mod_initialize`, Startreihenfolge (Abschnitt 3). |
| `src/core/hooks.cpp` | Hook-Ziele (`DEFINE_HOOK`) und `installCoreHooks()`. |
| `src/core/handlers.hpp` | Deklarationen der Kern-Handler für Pre- und Replace-Hooks. |
| `src/core/ride.cpp` | Ritt-Zustandsmaschine: Ruf, Absteigen, Landeanflug, Ritt-Enden, Speichersperre, Schutz-Hooks. |
| `src/core/input.cpp`, `src/core/input_core.hpp` | Eingabe des Kerns (WP3): Pad-Lesung `padReadPost`, Schnappschuss, Ruf-Kombination, Konsum der Ritt-Tasten, Sperrflags, UI-Block, gedrosselte Ablehnungen (Abschnitt 5.6). |
| `src/core/summon.cpp`, `src/core/summon.hpp` | Ruf des Vogels (WP6): `summonBird`, Spawn-Prüfung, Ruf-Gates vor jeder Wirkung, Ritt-Start-Gate, Zeitgrenzen (Abschnitt 5.5). |
| `src/core/session.cpp` | Namen für Phasen und Ritt-Enden (`phaseName`, `endReasonName`). |
| `src/core/log.cpp` | Logzeilen des Pakets. Jede Zeile beginnt mit `kff:`. |
| `src/core/services.cpp` | Zugriff auf die Dienste der Mod-SDK (Schnittstelle in `kff/services.hpp`). |
| `src/core/config.cpp` | Konfigurationstabelle und Lesefunktionen. |
| `src/core/fakepath.cpp` | Ersatzpfad für `mpPath1` (Absturzschutz). |
| `src/core/modules.cpp` | Modulliste, `disableModule`, `moduleActive`. |
| `src/core/autotest.cpp` | Autotest. Wird nur im Testbuild gebaut (Abschnitt 14). |
| `src/core/input_test.cpp` | Testszenarien der Eingabe (WP3). Wird nur im Testbuild gebaut. |
| `src/modules/flug.cpp` | Flugmodell des Vogels im Ritt. |
| `src/modules/freiheit.cpp` | Untergrenze, Wände, Stillstand, Szenen-Vetos. |
| `src/modules/griff.cpp` | Griff des Links am Vogel, geordnetes Absteigen. |
| `src/modules/tempo.cpp` | Gang 1 bis 5, Bremse, Dash, Sichtfeld, HUD. |
| `src/modules/kamera.cpp` | Kamera-Operator im Ritt. |
| `src/modules/mensch.cpp` | Menschen-Ritt (Teilport, Standard aus). |
| `src/modules/autowolf.cpp` | Verwandlung zum Wolf, Ruf des Vogels, Rückverwandlung. |

## 3. Startreihenfolge und Modulliste

`mod_initialize` in `src/core/core.cpp`:

1. Versionszeile im Log (Zeilen 40 und 42). Testbuild: `initialize v2.1.0 (Kargarok Flight, Testbuild), KFF_AUTOTEST=<Wert>`. Release: `initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`.
2. `config::registerVars()` (Zeile 45). Ohne Konfigurationsdienst kommt eine Warnung, und die Standardwerte gelten.
3. Nur im Testbuild: `autotest::init()` (Zeile 47).
4. `installCoreHooks()` (Zeile 49; `src/core/hooks.cpp`, Zeilen 407 bis 457).
5. `install(Registry&)` jedes Moduls in Listenreihenfolge (Zeilen 51 bis 80). Gelingt die Installation, meldet das Log `module <name> installed` (Zeile 64). Schlägt sie fehl, ruft der Kern `disableModule` auf (Zeile 67) und protokolliert den Fehler. Nur im Testbuild registriert der Kern in derselben Schleife das Szenario des Moduls (Zeilen 71 bis 78).
6. Nur im Testbuild: `autotest::selectFromEnvironment()` nach allen Modulen (Zeilen 82 bis 84). Die Auswahl liest `KFF_AUTOTEST`.

Die Modulliste steht in `src/core/modules.cpp` (Zeilen 30 bis 42):

```
flug, freiheit, griff, tempo, kamera, mensch, autowolf
```

Die Reihenfolge ist wichtig. `onHumanCall` läuft in dieser Reihenfolge, und der erste Modul-Treffer (`true`) nimmt den Ruf. `mensch` steht deshalb vor `autowolf`: Mit `mensch_ritt` aus lehnt `mensch` ab, und `autowolf` verwandelt Link.

## 4. Ablauf pro Tick

Ein Tick ist ein Aufruf von `mDoCPd_c::read`. Die Eingabe wird im Post-Hook dieses Aufrufs gelesen, bevor das Spiel sie verbraucht. Die Reihenfolge ergibt sich aus den Prioritäten (Abschnitt 8.2): höhere Werte zuerst.

1. **PadRead Post**, `core::padReadPost` (Priorität 1000, `hooks.cpp` Zeile 411). Der Kern liest den Eingabe-Schnappschuss `kff::input()` mit den Abfragen `isHeld(bits)` und `isPressed(bits)`. Die Felder stehen in `src/core/kff/input.hpp`. Im Handler (`ride.cpp`, Zeilen 840 bis 891) laufen folgende Schritte:
   - Testbuild: Injektion des Autotest-Szenarios (Zeilen 844 bis 846).
   - Erfassen der Eingabe (`captureInput`, Zeile 851).
   - Wiederherstellen des B-Tastenbits bei abgelehnten Rufen (`restoreChordB`, Zeilen 853 bis 856).
   - Zurücksetzen der Tick-Flags (Zeilen 858 bis 859).
   - R+B-Chord: Ist R gehalten und B getippt, wird der Ruf angenommen oder mit Grund abgelehnt (Zeilen 861 bis 878).
   - Im Ritt setzt B `dismountRequested`, und der Kern verbraucht die Tasten (`consumeRide`, Zeilen 882 bis 887).

   Im Ritt verbraucht der Kern die Tasten aus `kRideConsumedButtons` (`src/core/kff/input.hpp`, Zeilen 21 bis 26): A, B, X, Y, Z, L, R und die vier Richtungstasten. START gehört nicht dazu und bleibt bei der Pause. Danach läuft der Kamera-Hook (Schritt 7).

2. **LinkExecute Pre**, `core::linkExecutePre` (Priorität 1000, `hooks.cpp` Zeile 412). Ein angenommener Ruf wird über `callBlockReason` geprüft (Abschnitt 5.4). Bei `ok` legt `summonBird` den Vogel an (seit WP6 in `summon.cpp`, mit eigenen Gates, Abschnitt 5.5). Ohne erfolgreich registrierten LinkExecute-Pre-Hook gibt der Kern keinen Ruf frei (`hooks.cpp`, Zeile 414, `setLinkExecuteReady`). Jeder Link-Frame ruft der Kern `notifyFrameLink` auf (`ride.cpp`, Zeile 990). Diese Funktion ruft `onFrameLink` aller Module auf (`forEachModule`, `ride.cpp`, Zeilen 135 bis 149).

3. **KagoExecute Pre**, `core::kagoExecutePre` (Priorität 1000, `hooks.cpp` Zeile 415). Er verarbeitet einen B-Tipp im Ritt: Absteigen oder Start des Landeanflugs (`dismount`, Abschnitt 5.3), und den Landeanflug je Frame (`ride.cpp`, Zeilen 1036 bis 1067). Danach läuft der Vogel-Frame im Ritt-Zweig. Dort ruft der Kern `notifyFrameBird` auf (`ride.cpp`, Zeile 1104), und das ruft `onFrameBird` aller Module auf.

4. **KagoExecuteFly Pre**. Die Kette läuft in Prioritätsreihenfolge:
   - Kern `kagoExecuteFlyPre` (1000). Sie ruft `guardPathBeforeCall` auf, den Absturzschutz für `mpPath1` (`ride.cpp`, Zeilen 1178 bis 1190).
   - `flug` `onExecuteFlyPre` (500). Im Pre-Hook von `executeFly` wirken die Fassrolle und die Eingabe dieses Frames (Kopf von `flug.cpp`, Zeilen 6 bis 13).
   - `freiheit` `onExecuteFlyPre` (400). Hier greifen die Höhenregeln.

   Danach folgt der Vanilla-Flugschritt. Im Flugmodus des Spiels (`mMode` 2) setzt `flug` den Pre-Hook von `daKago_c::flySpeedCalc` (500, `flug.cpp` Zeilen 322 bis 402) ein. Dort berechnet `flug` Nick, Gier und Rolle selbst und gibt `HOOK_SKIP_ORIGINAL` zurück (`flug.cpp` Zeile 396). Damit entfällt die Vanilla-Lenkung für diesen Schritt. Nach dem Flugschritt setzt `flug` `onExecuteFlyPost` (500, `flug.cpp` ab Zeile 405). Dort werden Geschwindigkeit `V` und Bahn je Tick gesetzt. Der Gang-Faktor geht ein: `speedF = V·cos(Nick)·Gang` und `speed.y = −V·sin(Nick)·Gang` (Kopf von `flug.cpp`).

5. **KagoExecute Post**. `freiheit` `onBirdExecutePost` (400, `freiheit.cpp` Zeile 1747), danach `mensch` `on_kago_execute_post` (150, `mensch.cpp` Zeile 1470).

6. **LinkExecute Post**. `freiheit` `onLinkExecutePost` (400, `freiheit.cpp` Zeile 1748), danach `mensch` `on_link_execute_post` (150, `mensch.cpp` Zeile 1466).

7. **PadRead Post der Kamera**. `kamera` `onPadRead` (300, `kamera.cpp` Zeile 1789; Handler 1703 bis 1712) heilt nur eine gestoppte Spielkamera (`healStoppedCamera`, `kamera.cpp` Zeilen 642 bis 660). Der Kamera-Operator selbst wird über den CameraService registriert (`kamera.cpp` Zeilen 1774 bis 1788). Der Operator ist `cameraOperate` (ab Zeile 710), die Priorität ist `kKamera`.

8. **MeterDraw Post**. `tempo` `onMeterDrawPost` (200, `tempo.cpp` Zeile 513; Registrierung Zeile 1182) zeichnet die HUD-Zeile mit Gang und Geschwindigkeit.

Der Kamera-Stopp-Hook `CameraStop` (`kamera` `onCameraStop`, Pre, Priorität 300) ist im Build fest eingeschaltet. Der Schalter `KFF_CORE_CAMERA_STOP_HOOK` steht in `src/core/kff/hooks.hpp` (Zeile 73) auf 1. Die Registrierung steht in `kamera.cpp` Zeilen 1793 bis 1799.

## 5. Ritt-Zustand

### 5.1 Phasen

`kff::Phase` (`src/core/kff/session.hpp`, Zeilen 27 bis 33):

| Phase | Bedeutung |
|---|---|
| `Idle` | Kein eigener Vogel. |
| `Summoning` | Vogel angelegt (laut Kommentar `fopAcM_create`). Wartet auf den Zustand WAIT ohne Ereignis. |
| `Transforming` | Link verwandelt sich vor dem Ruf (Modul `autowolf`, `autowolf.cpp` Zeilen 311 und 658). Noch kein Vogel. |
| `Riding` | Link sitzt im Vogel (Cargo-Carry). Die Ritt-Frames laufen. |
| `Dismounting` | Abgestiegen, nach Landeanflug oder sofort. Der Vogel fliegt weg und wird danach entfernt. |

Laut Kopf von `session.hpp` (Zeile 5) darf ein Modul vor dem Ruf `Transforming` setzen. Es muss die Phase selbst wieder auflösen, durch `summonBird` oder `resetSession`. Sonst blockiert es jeden Ruf.

### 5.2 Gründe für das Ritt-Ende

`kff::EndReason` (`session.hpp`, Zeilen 36 bis 51). Die Module erhalten den Grund in `onRideEnd`. Die Liste zeigt die Stellen, an denen der Grund gesetzt wird (Datei und Zeile; `ride.cpp` ohne Präfix).

| Grund | Auslöser (Quelle) |
|---|---|
| `DismountButton` | B im Ritt: 1044 (zweites B im Landeanflug), 1056 (Landeanflug fertig oder Abgrund), 1074 (B-Tipp), 1084 (B-Wunsch aus Zwischenaktion). |
| `RiderLost` | 968 (Link ohne Griff). `griff.cpp` 871 (kein Wolf), 883 (Griff nicht zu halten). `mensch.cpp` 996 (Link ist Wolf), 1014 (Griff nicht gestartet), 1021 (Huhn-Instanz fehlt). |
| `Event` | 953 (Ereignis im Ritt), 1141 (Szenenwechsel durch Ereignis, `classifySceneChange`). `griff.cpp` 865 (Ereignis ohne Griff). `mensch.cpp` 1000 (Ereignis im Ritt). WP4: `linkExecutePre` beendet auch die Phase `Summoning`, wenn vor dem Aufsitzen ein Ereignis beginnt (Log `ride ended clean (Event, Ereignis vor dem Ritt-Start)`, Zähler `CORE5 summonEventEnd`); `kagoExecutePre` startet während eines Ereignisses keinen Ritt. |
| `LinkChanged` | 919 (neuer Link-Akteur). |
| `DeleteRequested` | 583 (Summon-Timeout), 1019 (Vogel abgeflogen, nach `kDepartDeleteFrames`). |
| `BirdGone` | 566 und 971 (`notifyRideEnd` ohne Vogel), 1440 (unerwartetes Entfernen im Ritt). |
| `Shutdown` | 1461 (Entladen, `shutdownBird`). `mensch.cpp` 1529 (Ende des Griffs beim Entladen). |
| `Death` | 948 (Link tot), 1282 (`checkDeadAction`), 1292 (Game-Over, `procCoDeadInit`). `griff.cpp` 846 (Tod im Ritt). |
| `Menu` | 1141 (Szenenwechsel aus dem Menü, `classifySceneChange`), 1303 (Titel), 1313 (Laden). |
| `ManualSave` | Kein Aufrufer. |
| `Warp` | 1170 (Warp-Start, `linkProcCoWarpInitPre`). |
| `Transform` | `griff.cpp` `onMetamorphosePre` (Pre auf `daAlink_c::procCoMetamorphoseInit`, WP4): Verwandlung über den Spielweg, solange ein eigener Vogel existiert. Der Ritt endet vor dem Verwandlungs-Proc, das Original läuft danach. Die Verwandlung des Moduls `autowolf` (Phase `Transforming`) ist ausgenommen. |
| `Stuck` | Kein Aufrufer. |
| `Reset` | 1365 (Soft-Reset). |

Bei `endRideClean` hält der Kern die Fallhöhe-Referenz, wenn Link in der Luft ist, der Grund nicht `Death` ist und seit Z1 Boden unter Link liegt (Abschnitt 5.6). Sonst wird sie zurückgesetzt. Der Kern meldet das Ritt-Ende genau einmal (`notifyRideEnd`, `ride.cpp` Zeilen 158 bis 166).

### 5.3 Funktionen des Kerns

Deklariert in `session.hpp`, Zeilen 124 bis 151. Module dürfen sie aufrufen. Zeiger auf Link oder Vogel dürfen nicht gespeichert werden. `liveLink()` und `liveBird()` suchen bei jedem Aufruf neu.

- `summonBird(link)`: legt den freundlichen Vogel 350 Einheiten über Link an (`kSpawnUp`, seit WP6 in `src/core/summon.cpp`; Phase `Summoning`). Nur mit Wolf-Link aufrufen. Seit WP6 prüft die Funktion vor jeder Wirkung ihre Gates (Abschnitt 5.5). Bei einer Sperre liefert sie `MOD_ERROR`, legt keinen Vogel an und lässt die Sitzung unverändert; ein Mensch-Link ohne `mensch_ritt` wird so abgewiesen. Aufrufer: `tryCall` im Kern (`ride.cpp`, gibt B bei einem Fehler an das Spiel zurück), `autowolf` und `mensch`.
- `dismount(bird, reason, why)` (`ride.cpp` Zeilen 723 bis 744): nur in Phase `Riding`. Ein B-Tipp mit Boden unter dem Vogel startet den Landeanflug (Zeile 738). Sonst ruft der Kern `finishDismount` (Zeilen 696 bis 718) auf. Diese Funktion meldet das Ritt-Ende über `onRideEnd`, löst Link vom Vogel (`releaseRider`, Zeile 699), setzt den Vogel in den Wegflug und die Phase auf `Dismounting`. Das Entfernen des Vogels folgt erst später, nach `kDepartDeleteFrames` (`ride.cpp` Zeile 1019).
- `requestDelete(reason, why)` (`ride.cpp` Zeilen 746 bis 774): `notifyRideEnd` nur in Phase `Riding` (danach `Idle`), dann `detachBird`, der Vogel wird in den Zustand WAIT versetzt (`setActionMode(ACTION_WAIT_e)`), Flag 6DF wird auf 1 gesetzt, danach `fopAcM_delete`. Die Kopfkommentare in `session.hpp` (Zeilen 144 bis 145) beschreiben die Reihenfolge ohne die Phasenbedingung von `notifyRideEnd`. Wiederholung und Aufgabe des Entfernens: `retryPendingDelete` (`ride.cpp` Zeile 595) mit `kDeleteRetryFrames` (60) und `kPendingGiveUpTicks` (900).
- `endRideClean(reason, why)` (`ride.cpp` Zeilen 776 bis 793): Ritt-Ende ohne Abflugflug (Tod, Menü, Szenenwechsel durch Ereignis). Ruft `requestDelete` auf, setzt die Fallhöhe-Referenz (Abschnitt 5.2) und dann `resetSession`.
- `releaseRider(bird)` (`ride.cpp` Zeile 795): löst Link aus dem Cargo-Carry, falls er noch darin sitzt. Aufrufe in `ride.cpp` Zeilen 515 und 699.
- `resetSession()`: setzt die Sitzung auf `Idle`. Laut Kommentar in `session.hpp` (Zeile 150) bleiben die Zähler erhalten.
- `isRiding()`, `isOwnBird(k)`, `session()`, `liveLink()`, `liveBird()`: Abfragen.

### 5.4 Ruf und Absteigen

- **Ruf-Gate.** `callBlockReason` (`ride.cpp`, Zeilen 315 bis 398) prüft 25 Bedingungen mit 23 Grund-Texten. Trifft eine zu, läuft der Ruf nicht. B bleibt beim Spiel. Das Log meldet `call rejected (<Grund>), B stays` (`ride.cpp` Zeile 1069, gedrosselt mit `logEvery`). Die Gründe in Prüfreihenfolge: `transforming` (Phase `Transforming` oder Link im Verwandlungs-Proc `PROC_METAMORPHOSE`, WP4); Kern-Hook `daAlink_c::execute` fehlt; Modul `flug` oder `freiheit` abgeschaltet; Startflanke (Menü öffnet in diesem Tick); Pause oder Menü offen; Menü, Karte oder Item-Rad offen; Gespräch offen; `event` (Ereignis läuft); Dusklight-Dokument sichtbar; Kartenzugriff läuft; kein Link; alter Vogel wird noch entfernt; eigener Vogel aktiv; `event` (Link im Ereignis oder in einer Demo); Link reitet bereits; Link tot; Fall; Link im Schaden; Wasser; Klettern; Zielsperre (analoges L); R-Status belegt; Schildangriff läuft; Luft (Sprung oder Fall). Die Texte `transforming` und `event` sind fest (WP4); vorher hießen sie „Verwandlung laeuft“, „Ereignis laeuft“ und „Link im Ereignis“.
- **R-Gate.** R gilt als gehalten, wenn das Digitalbit gesetzt ist oder `triggerR >= 0.61f` (`ride.cpp` Zeile 866). Die Schwelle liegt auf der Skala 0 bis 1. Die Belegungskarte `PS-KARTE.md` (Zeilen 77 und 79) nennt für den Ruf etwa 45 Prozent Achsweg, für den Spiel-Lock etwa 61 Prozent. Die Umrechnung ist nicht geklärt. Am Controller ist die Schwelle nicht gemessen.
- **B-Wunsch.** Wird B im Ritt getippt, während der Vogel weder fliegt noch angreift, zählt `dismountIgnored` hoch (`ride.cpp` Zeile 1077). Dann setzt der Kern `g_dismountWish = kDismountWishTicks` (60, Zeile 119; Zeile 1079). Läuft der Wunsch ab, erscheint der Hinweis „B wirkt nur im Flug: B noch einmal druecken“ (Zeile 1088).
- **Landeanflug.** Er startet beim B-Tipp mit Boden unter dem Vogel (Zeile 738, Log `landing approach (...)`). Je Frame gilt (Zeilen 1038 bis 1067):
  - Ein zweites B gibt sofort frei (`zweites B im Landeanflug`, Zeile 1044).
  - Der Anflug endet, wenn kein Boden mehr unter dem Vogel liegt (Grund `Abgrund im Landeanflug`), wenn der Bodenabstand `kLandClearance` (150) erreicht ist, wenn die Füße den Boden berühren oder wenn `landFrames >= kLandMaxFrames` (240) gilt (Zeile 1052). Das Log meldet dann `landing done: clearance ..., feet ..., frames ..., ground ...`.
  - Die Sinkrate je Tick ist `(Bodenabstand − 150) / max(60 − landFrames, 1)`, begrenzt auf `[kLandSink, kLandSinkMax]` = [8, 60] (Zeilen 1060 bis 1062). Der Vogel sinkt um diesen Betrag (Zeile 1063).
  - Horizontal wird nicht gebremst.
  - Während des Anflugs setzt `freiheit` die Untergrenze aus (`landingActive`, `freiheit.cpp` Zeilen 308 bis 310).
- **Speichersperre.** `saveLockPre` (`ride.cpp` Zeilen 1323 bis 1336) liefert im Ritt `HOOK_SKIP_ORIGINAL` (Zeile 1335). Sie wird von `saveMoveDispPre` („a“, Zeilen 1338 bis 1340) und `saveMoveDisp2Pre` („b“, Zeilen 1342 bis 1344) aufgerufen. Dazu meldet sie den Hinweis „Erst absteigen (B), dann speichern“ über `toastNotice` (Anzeigedauer 3000 ms) und das Log `save locked during ride (path %s), write refused, ride continues`. `cardSavePre` (Zeilen 1349 bis 1359) liefert ebenfalls `HOOK_SKIP_ORIGINAL` (Zeile 1355) und meldet `card write refused during ride (mDoMemCd_Ctrl_c::save), ride continues`. In beiden Fällen läuft der Ritt weiter.
- **Warp.** `linkProcCoWarpInitPre` (`ride.cpp` Zeilen 1165 bis 1170) beendet den Ritt beim Warp-Start (`EndReason::Warp`, Zeile 1170). Der Hook hat kein SKIP. Der Warp selbst läuft weiter.
- **Szenenwechsel.** `classifySceneChange` (Zeilen 1126 bis 1145) beendet den Ritt bei Menü- oder Ereignis-Wechseln (Zeile 1141). Positionsgesteuerte Wechsel bleiben im Ritt. `stageNextStagePre` (Zeilen 1148 bis 1153) und `stageChangeScenePre` (Zeilen 1156 bis 1161) rufen die Funktion auf.

### 5.5 Ruf-Prüfungen, Zeitgrenzen und Wächter (WP6)

Ergänzt 5.2 bis 5.4. Zeilenangaben fehlen hier absichtlich; die Namen sind eindeutig.

- **Weitere Ruf-Gates.** Am Ende von `callBlockReason` ruft der Kern `summon::callBlockReason` (`summon.cpp`): „Verwandlung laeuft (Link)“ (Proc `PROC_METAMORPHOSE` oder `PROC_METAMORPHOSE_ONLY`), „Link im Schaden“ (zusätzlich Lande- und großer Schaden, Wolf und Mensch), „eigener Vogel ohne Sitzung“ (ein Vogel mit Parameter `0x000100FF`, der nicht der Sitzungsvogel ist, wird entfernt; Vanilla-Vögel mit anderem Parameter bleiben) und `room` (Spawn-Prüfung). Der Grund „alter Vogel wird noch entfernt“ heißt jetzt `departing` und gilt auch im Landeanflug, im Abflug und bei angefordertem Entfernen (Schnelles B, D7: B bleibt beim Spiel).
- **Spawn-Prüfung `room`.** Linie von Links Füßen plus 50 bis 600 über den Füßen (Spawnpunkt 350 plus 250), Vorder- und Rückseiten; Decke über dem Spawnpunkt mindestens 250 höher; Boden darunter mindestens 150 tiefer, kein Boden gilt als frei. Log `spawn check: room blocked (...)`. Seitliche Wände prüft sie nicht.
- **Gates in `summonBird`.** Link lebt, keine Verwandlung, kein Schaden, Wolf (Mensch nur mit `mensch_ritt`), kein Vogel, Phase `Idle` oder `Transforming`, kein Ereignis, keine Pause, kein Dusklight-Dokument, kein eigener Vogel ohne Sitzung (hier nur gezählt, nicht entfernt), `room`. Log `call rejected (<Grund>) at spawn`.
- **Ritt-Start-Gate.** `summon::rideStartAllowed` in `kagoExecutePre` vor `startRide`: Link lebt und steht in keinem Ereignis (Log `ride start waiting (<Grund>)`).
- **Zeitgrenzen** in Kern-Ticks ohne Pause (`summon::tick`, je Tick aus `padReadPost`): `kSummonTimeoutTicks` 300 (Vogel in `Summoning` ohne Ritt-Start, Grund `DeleteRequested`, Log `summon timeout: ...`; der Antrag geht bei Tick 299 hinaus, weil `fopAcM_delete` im Folgetick wirkt). Ein Ereignis in `Summoning` entfernt den Vogel sofort (Grund `Event`, Log `summon aborted (event)`). `kTransformLimitTicks` 1800: Phase `Transforming` ohne Vogel gibt die Sitzung frei (Log `transform timeout: ...`). Den Abflug-Antrag stellt der Kern bei `departFrames + 1 >= kDepartDeleteFrames`; der Vogel ist nach 100 Ticks weg.
- **Stillstands-Wächter** (`freiheit.cpp`, `watchStuck`, `kStuckTicks` 300, `kStuckRadius` 150): 300 Ritt-Frames ohne Stick-Eingabe im Umkreis von 150 Einheiten. Stick, Ortswechsel, Landeanflug und Ereignis setzen ihn zurück. Der Vogel-Post setzt nur ein Flag und loggt `watchdog: stuck <n> ticks`; das Ende (`endRideClean(EndReason::Stuck, ...)`) folgt im nächsten Link-Frame (`onFrameLink`) mit einem Hinweis im Spiel.
- **Zählerzeile** `CORE6 spawnRejected=… roomRejected=… strayFound=… rideStartWait=… summonAborted=… transformTimeout=…`. In allen WP6-Läufen blieben `spawnRejected`, `strayFound`, `summonAborted` und `transformTimeout` bei 0; diese Pfade sind nur im Code geprüft.
- **Testbuild.** `KFF_TEST_NORIDE=1`: der erste gerufene Vogel startet keinen Ritt (Prüfung der Zeitgrenze). `KFF_TEST_CRRPOS=1`: CrrPos läuft im Ritt (Kontrolllauf zu WP6.2). Szenarien im Modul `freiheit`: `wp6-decke`, `wp6-wand`, `wp6-rand`, `wp6-ruf`, `wp6-timeout`, `wp6-gate`, `wp6-wasser`, `wp6-doc`.
- **Stillstand unter Decke (E10, Messung WP6).** Höchstens 53 und 48 Ticks Stillstand (Ziel höchstens 30), nach einem Stickwechsel frei nach 46, 8 und 21 Ticks (Ziel höchstens 5). Mit `decke_abstieg = true` startete der Abstieg nie (`kein Ausweg nach unten`), der Schalter bleibt aus. Die Füße bleiben durch die Untergrenze mindestens 50 Einheiten über der höchsten Fläche. Die Ziele sind nicht erreicht; Läufe und Werte stehen in `CHANGELOG.md` (Prüfung WP6).
- **Überholte Angaben in anderen Abschnitten.** Damit sich dieser Nachtrag mit parallelen Änderungen an denselben Tabellenzeilen nicht überschneidet, sind diese Stellen nicht umgeschrieben. Es gilt: 5.2 `DeleteRequested` (Summon-Timeout jetzt in `summon::tick`) und `Stuck` (Aufrufer ist der Wächter oben); 5.4 „alter Vogel wird noch entfernt“ heißt `departing`; 8.4 `autowolf` registriert seit WP6 zwei Hooks (Tabelle dort); 10 `kFriendlyBirdParam` und `kSpawnUp` stehen in `summon.cpp`, `kSummonTimeoutFrames` gibt es nicht mehr (ersetzt durch `kSummonTimeoutTicks`); 16 `Stuck` hat einen Aufrufer, und für den Stillstand gelten die Messwerte im vorigen Punkt. Zeilenangaben zu `freiheit.cpp`, `ride.cpp` und `autowolf.cpp` in den übrigen Abschnitten stammen aus der Fassung vor WP6; die Namen gelten weiter. Die Hooks von `freiheit` registriert `FreiheitModule::install` (ab Zeile 4309).

### 5.6 Zusammenführung Welle 2 (WP1, WP3, WP5, Z1)

Ergänzt die Abschnitte 4, 5.2 bis 5.5 und 8. Zeilenangaben fehlen absichtlich; die Namen sind eindeutig. Zeilenangaben zu `ride.cpp`, `freiheit.cpp` und `griff.cpp` in anderen Abschnitten stammen aus älteren Fassungen.

- **Eingabe (WP3).** Pad-Lesung, Schnappschuss, Ruf-Kombination und Konsum stehen in `src/core/input.cpp` (`padReadPost`, `captureInput`, `consumeRide`, `repairLocks`). `ride.cpp` behält `ridePadTick` (Abflug-Frist und `summon::tick`). Die `ride.cpp`-Zeilen der Eingabe in Abschnitt 4 (Schritt 1) und 5.4 (R-Gate) gelten für die Fassung vor WP3. Ablehnungen stehen in einer Grundtabelle mit 64 Plätzen: je Grund die erste Zeile sofort, danach höchstens eine je 90 Pad-Ticks (`kRejectLogTicks`), jeweils mit `n=<Anzahl>`. D8: Ruf und Absteigen gelten nur im Tick ihrer Entstehung (`dropStaleCall`). RK (a): `consumeRide` nullt im Ritt auch Analogwerte und Sperrflags von A, B, L und R, einschließlich `mHoldLockR`; `repairLocks` verhindert nach dem Ritt eine falsche R-Flanke (P2-04). E5: Bei Pause, Menü, Karte, Item-Rad, Dokument, Dialog, Ereignis oder anstehendem Stage-Wechsel verbraucht der Kern nichts; der Schnappschuss trägt `uiBlocked` und `uiDocument`. Bei `uiDocument` hält `flug` den Vogel an (`ui_halt`).
- **Szenenwechsel im Ritt (WP1).** Besitzer ist `freiheit` (Priorität 400). Im Ritt ohne Ereignis bei lebendem Link liefern diese Pre-Hooks SKIP: `daAlink_c::checkSceneChange`, `checkRestartRoom`, `checkRoofRestart`, `startRestartRoom`, `daTagRetRm_c::execute` (der Zeitgeber wird geleert), `daKago_c::setSceneChange` und `checkFlySceneChange` des eigenen Vogels sowie `dStage_changeScene` ohne Pflichtereignis. Ohne SKIP, mit Ritt-Ende vorher: `dStage_changeScene` mit Pflichtereignis, `dComIfGp_setNextStage` (4 Argumente), `dStage_changeScene4Event` und der Warp-Status (erkannt im Post von `mDoCPd_c::read`). Protokoll: `freiheit: positional scene veto (<Aufrufer>)` je Episode (eine neue Zeile erst nach 30 Ticks ohne Versuch, `kVetoGap`) und `freiheit: scene change allowed after end (<Grund>)` für den ersten Stage-Wechsel nach einem Ritt-Ende (Fenster 5400 Ticks, `kAllowWindow`).
- **Zustände im Ritt (WP5).** `griff` registriert dazu neun Pre-Hooks mit Priorität 350: `daKago_c::executeStagger`, `executeLanding`, `executeLandingLakeHairia`, `executeLandingBoartHouse` (SKIP im Ritt), `daAlink_c::checkSwimAction` (SKIP, Rückgabe 0), `checkDamageAction` (ohne SKIP: Schadenszweige mit Proc-Wechsel werden vorher aufgelöst, der Lebensverlust bleibt), `procWolfDamageInit` (SKIP im Wolfs-Ritt), `procCoLavaReturnInit` und `daTagKagoFall_c::execute` (SKIP im Ritt, außer bei Tod oder Ereignis). Jeder dieser Callbacks fängt seine Ausnahmen selbst (`countException`, das Original läuft). Ruf-Gate: `griffCallGateReason` (aufgerufen aus `callBlockReason` nach „Link tot“) mit den Gründen `riding`, `transform`, `water`, `rope`, `chain`, `hang`, `climb`, `hookshot`, `damage` und `air`. K17: Der Landeanflug über Wasser endet bei 30 Einheiten Fußabstand zur Wasserfläche (`kWaterReleaseClear`).
- **Hook-Besitz nach der Zusammenführung.** Je Ziel höchstens ein SKIP-Besitzer. `startRestartRoom` und `daTagRetRm_c::execute` gehören allein `freiheit`; die gleichlautenden WP5-Hooks in `griff` sind nicht übernommen. Mehrere Pre-Hooks auf einem Ziel gibt es nur mit verschiedenen Prioritäten und höchstens einem SKIP, etwa `KagoExecuteStagger` (Kern 1000 ohne SKIP, `griff` 350 mit SKIP) oder `LinkProcWolfDamageInit` (Kern-Diagnose 1000 nur im Testbuild, `griff` 350). `LinkSetDamagePoint` registrieren Kern (1000) und `freiheit` (400) nur im Testbuild, beide ohne SKIP.
- **Sturzreferenz nach dem Ritt (Z1, OE1).** `finishDismount` setzt die Referenz (`holdFallReference`: `mFallHeight` und `field_0x33c8`) einmal auf die Loslasshöhe. Weiter gehalten wird sie nur, wenn unter Link Boden liegt (`groundBelowLink`, Sonde ab 500 über Link). `endRideClean` wendet dieselbe Bedingung an (zusätzlich Link in der Luft und Grund nicht `Death`). Ohne Boden gilt die Vanilla-Regel: `checkRestartRoom` löst den Void-Out aus, sobald `field_0x33c8 − y > 2000` ohne Boden gilt. Gehalten wird bis zum Bodenkontakt oder höchstens `kFallWatchFrames` (600) Link-Frames, Log `fall reference released (ground touched|watch timeout)`.
- **Abflug und Logzeilen (Z1).** `ridePadTick` stellt den Antrag zum Entfernen bei `departFrames + 1 >= kDepartDeleteFrames` (100). `finishDismount` sichert den Gang vor `notifyRideEnd` (tempo setzt ihn dort zurück) und schreibt `landing release (...) gang <n> ...` nach dem Nullsetzen der Horizontalgeschwindigkeit, danach `dismount (<Grund>, <Anlass>) at ride frame ...` und `ride ended clean (<Grund>, <Anlass>), bird departing, fallHold=<0|1>`.
- **Versatz im ersten Fall-Frame (Z1 (e)).** Nach der Freigabe ohne Boden wechselt Link über Proc 241 zu `PROC_WOLF_FALL` (253). `procWolfFallInit` setzt `field_0x2f99` auf 12 (Wurzel-Z übernehmen, 0x4, und fortsetzen, 0x8). `posMove` addiert die Änderung der Animationswurzel (`field_0x34d4`, gemessen von −27,65 auf 36,63), daher der Sprung um 64,28 Einheiten. Das ist Spielverhalten; die Mod glättet nichts. Die Zeilen `release diag frame +0 (Freigabe)` bis `frame +3 (pre)` gibt es nur im Testbuild.

## 6. Schnittstellenvertrag der Sitzung

Regeln aus dem Kopf von `session.hpp`: Zeiger nie speichern. Die Phase setzt der Kern. Jedes Feld hat einen Schreiber, andere Module lesen nur.

| Feld | Schreiber | Leser | Hinweis |
|---|---|---|---|
| `att` (`AttitudeState`) | `flug` | `kamera`, `griff`, `mensch`, `tempo` (laut Kopfkommentar, `session.hpp` Zeile 56) | Lage des Vogels: Nick- und Gierschritte, Rolle, Fassrolle, Trimmung, Geschwindigkeit `V`. Der Kern setzt sie beim Ritt-Start zurück. |
| `speedScale` (0,3 bis 1,0, Standard 0,5) | `tempo` | `flug` (Anwendung im Flugschritt und im Post-Hook) | Gang-Faktor (`session.hpp` Zeile 119). |
| `gear` (1 bis 5, Standard 3) | `tempo` | Module | Gang für die Anzeige (`session.hpp` Zeile 120). |
| `fovExtraDeg` (0 bis 10) | `tempo` | `kamera` | Sichtfeld-Zusatz in Grad (`session.hpp` Zeile 121). |
| `landing` (bool) | Kern | `freiheit` | Solange der B-Landeanflug läuft, setzt `freiheit` die Untergrenze aus (`session.hpp` Zeile 105, `freiheit.cpp` Zeilen 308 bis 310). |
| `landFrames` | Kern | `freiheit` | Ticks seit dem B-Tipp, 0 ohne Landeanflug. Schreiben nur in `ride.cpp` (Zeilen 712, 735, 828, 1064). |
| `rideGrip` | `mensch` (`mensch.cpp` Zeile 976) | Kern | Kennzeichnet den Menschen-Ritt im Griff. Zurückgesetzt in `ride.cpp` Zeile 830. Der Kern prüft dann nicht `checkCargoCarry` (`ride.cpp` Zeilen 954 bis 957). |
| `callRequested`, `dismountRequested` | Kern (`padReadPost`) | Kern (`linkExecutePre`, `kagoExecutePre`) | Je Tick gesetzt (`session.hpp` Zeilen 83 bis 86). |
| Zähler (`linkCalls`, `birdCalls`, `rideFrames`, `departFrames`, `riderLost`, `deleteWait`, ...) | Kern | Module, Logs | Frame-Zähler seit dem jeweiligen Ereignis (`session.hpp` Zeilen 93 bis 103). |

## 7. Modul-Schnittstelle

`kff::Module` (`src/core/kff/module.hpp`). Ein Modul ist eine Klasse mit einer Factory `kff::modules::<name>()`. Ein Minimalbeispiel steht im Kopf der Datei. Die Modul-Callbacks laufen über `forEachModule`. Eine Ausnahme dort beendet nur den Aufruf in diesem Frame (Abschnitt 8.5).

| Methode | Zeitpunkt |
|---|---|
| `name()` | Kurzname für Logs und Replace-Konflikte, z. B. `flug`. |
| `install(Registry&)` | Einmal aus `mod_initialize`, nach den Kern-Hooks. Hier stehen `addPre`, `addPost`, `replace`. Schlägt die Installation fehl, protokolliert der Kern den Fehler und schaltet das Modul ab (Abschnitt 8.5). Das Modul `mensch` bricht beim ersten fehlgeschlagenen Hook ab (`mensch.cpp` Zeilen 1466 bis 1490). |
| `onHumanCall(Session&, daAlink_c*)` → `bool` | Ruf als Mensch (Wolf-Form fehlt). `true` übernimmt den Ruf, z. B. Verwandlung und danach `summonBird`. Übernimmt kein Modul den Ruf, meldet der Kern `human form - ride needs wolf (no module took the call)` (`ride.cpp` Zeile 433). |
| `onRideStart(Session&, daKago_c*)` | Ritt beginnt, Aufruf aus `notifyRideStart` (`ride.cpp` Zeile 153). |
| `onFrameBird(Session&, daKago_c*)` | Aufruf aus `notifyFrameBird` (`ride.cpp` Zeile 170) im Ritt-Zweig des Vogel-Frames (Abschnitt 4, Schritt 3). |
| `onFrameLink(Session&, daAlink_c*)` | Aufruf aus `notifyFrameLink` (`ride.cpp` Zeile 175) je Link-Frame. |
| `onRideEnd(Session&, daKago_c*, EndReason)` | Ritt endet, Aufruf aus `notifyRideEnd` (`ride.cpp` Zeilen 158 bis 166). Der Vogel kann `nullptr` sein (z. B. `ride.cpp` Zeilen 566 und 971). Die Meldung kommt genau einmal je Ritt. |
| `autotestScenario()` → `const autotest::Scenario*` | Der Kern fragt es nur im Testbuild ab (`core.cpp`, Zeilen 71 bis 78). |
| `onShutdown(Session&)` | Entladen. Der Kern löst den Vogel vorher (`shutdownBird`, dann `shutdownModules`, `core.cpp` Zeilen 92 bis 98). |

`disableModule(m)` und `moduleActive(m)` (`src/core/modules.cpp`, Zeilen 14 bis 28) verwalten die Abschaltung. Die Liste der abgeschalteten Module umfasst höchstens 16 Einträge (`kMaxDisabled`, Zeile 8). Es ist nicht die Modulliste.

## 8. Hooks

### 8.1 Regeln

- Registrierung über `kff::Registry`: `addPre(Hook, Fn, prio)`, `addPost(Hook, Fn, prio)`, `replace(Hook, Fn)`. Die Hook-Namen stehen in `kff::Hook` (`src/core/kff/hooks.hpp`). `Registry::hookName` liefert den Text (`hooks.cpp` Zeile 171).
- Die Hook-Ziele stehen als `DEFINE_HOOK` in `src/core/hooks.cpp` (Zeilen 25 bis 74).
- Mehrere Pre- und Post-Hooks je Ziel sind erlaubt. Die Reihenfolge: höhere Priorität zuerst, bei gleicher Priorität die Registrierungsreihenfolge (SDK, `dusklight/src/dusk/mods/svc/hook.cpp`, Funktion `sort_hooks`, Zeilen 133 bis 141).
- `replace`: höchstens ein Besitzer je Ziel. Ein zweiter `replace` liefert `MOD_CONFLICT` (`src/core/hooks.cpp` Zeile 352, SDK `hook.cpp` Zeile 504). Im Paket besitzt der Kern `KagoCheckWallHitFall` und `KagoExecuteBalloonMenu` (`hooks.cpp` Zeilen 419 und 421).
- Nur Pre-Hooks können mit `HOOK_SKIP_ORIGINAL` das Original unterdrücken (`hooks.hpp` Zeilen 10 und 104). Ein SKIP beendet die übrige Pre-Kette. Der SDK kehrt dann vor dem `replace` zurück (`hook.cpp` Zeilen 605 bis 611). Ein `replace` läuft in diesem Fall also nicht.
- Post-Hooks laufen immer (SDK `hook.cpp` Zeilen 630 bis 647).
- `PadRead` akzeptiert nur Post. `addPre` auf `PadRead` liefert `MOD_UNSUPPORTED` (`src/core/hooks.cpp` Zeilen 309 und 310).
- Der Besitzer eines Hooks ist beim Registrieren über `setOwner` gesetzt. `installCoreHooks` setzt „core“ (`hooks.cpp` Zeile 410) und stellt den vorherigen Besitzer danach wieder her (Zeile 456).

### 8.2 Prioritäten

`prio` in `src/core/kff/hooks.hpp` (Zeilen 76 bis 85). Höhere Werte laufen zuerst.

| Name | Wert | Besitzer |
|---|---|---|
| `kCore` | 1000 | Kern (alle Kern-Hooks) |
| `kFlug` | 500 | `flug` |
| `kFreiheit` | 400 | `freiheit` |
| `kGriff` | 350 | `griff` (Ereignis-Teil, WP4: 15 Hooks, nur Pre und Post, Abschnitt 8.4; Zustands-Teil, WP5: 9 Pre-Hooks, Abschnitt 5.6) |
| `kKamera` | 300 | `kamera` |
| `kAutowolf` | 250 | `autowolf` (ohne eigene Hooks) |
| `kTempo` | 200 | `tempo` (`MeterDraw`; im Testbuild dazu der Beobachter auf `PadRead`) |
| `kMensch` | 150 | `mensch` |

### 8.3 Hook-Besitz des Kerns

Registriert in `installCoreHooks()` (`src/core/hooks.cpp`, Zeilen 407 bis 457), alle mit Besitzer „core“ und Priorität `kCore` (1000). Die Handler stehen in `src/core/ride.cpp`, die Deklarationen in `src/core/handlers.hpp`. Es sind 21 Kern-Hooks.

| Ziel | Art | Handler (`ride.cpp`) | Zweck |
|---|---|---|---|
| `PadRead` | Post | `padReadPost` | Eingabe-Schnappschuss, Ruf- und B-Flags, Verbrauch der Ritt-Tasten (Abschnitt 4). |
| `LinkExecute` | Pre | `linkExecutePre` | Ruf verarbeiten, `onFrameLink` (Abschnitt 4). |
| `KagoExecute` | Pre | `kagoExecutePre` | B-Tipp, Landeanflug, `onFrameBird`. |
| `daE_YM_c::_delete` | Pre | `ymDeletePre` | Löst die Bindung des Vogels an E_YM, wenn E_YM gelöscht wird (Zeilen 1111 bis 1119). |
| `KagoCheckWallHitFall` | Replace | `kagoCheckWallHitFallReplace` | Der eigene Vogel stößt nie an Wänden ab (Zeilen 1376 bis 1393). |
| `KagoExecuteBalloonMenu` | Replace | `kagoExecuteBalloonMenuReplace` | Kein Ballonmenü für den eigenen Vogel (Zeilen 1395 bis 1413). |
| `KagoDraw` | Pre | `kagoDrawPre` | Erzwingt das Zeichnen des eigenen Vogels (`field_0x6de` auf 0xff, Zeilen 1415 bis 1424). |
| `KagoDelete` | Pre | `kagoDeletePre` | Quittiert ein angefordertes Entfernen (`g_pendingBirdId`). Ein unerwartetes Entfernen im Ritt beendet den Ritt mit `BirdGone` (Zeilen 1426 bis 1440). WP4: Vorher löst `birdDeleteHygiene` (`ride.cpp` Zeile 565) für jeden Kargarok einen Skip-Rückruf, der auf ihn zeigt (Log `skip cleared on bird delete`), und beendet beim eigenen Vogel ein Ereignis, an dem er beteiligt ist (Log `event reset on bird delete`). Dieselbe Prüfung läuft in `requestDelete` und beim Entfernen verwaister Vögel. |
| `StageNextStage` | Pre | `stageNextStagePre` | Szenenwechsel durch Menü oder Ereignis beendet den Ritt (`classifySceneChange`). |
| `StageChangeScene` | Pre | `stageChangeScenePre` | Derselbe Schnitt für `dStage_changeScene`. |
| `LinkCheckDeadAction` | Pre, ohne SKIP | `linkCheckDeadActionPre` | Tod: Ritt-Ende vor der Todesaktion (Zeilen 1277 bis 1282). |
| `LinkProcCoDeadInit` | Pre, ohne SKIP | `linkProcCoDeadInitPre` | Zweiter Tod-Pfad (Game-Over, Zeilen 1290 bis 1292). |
| `ChangeOpeningScene` | Pre, ohne SKIP | `openingScenePre` | Titel: Ritt-Ende vorher (Zeilen 1301 bis 1303). |
| `GameStart` | Pre, ohne SKIP | `gameStartPre` | Laden: Ritt-Ende vorher (Zeilen 1311 bis 1313). |
| `SaveMoveDisp` | Pre, SKIP im Ritt | `saveMoveDispPre` | Speichermenü im Ritt über `saveLockPre` (Abschnitt 5.4). |
| `SaveMoveDisp2` | Pre, SKIP im Ritt | `saveMoveDisp2Pre` | Wie `SaveMoveDisp`. |
| `CardSave` | Pre, SKIP im Ritt | `cardSavePre` | Kartenschreiben im Ritt wird abgewiesen, der Ritt läuft weiter (Zeilen 1349 bis 1359). |
| `LinkProcCoWarpInit` | Pre, ohne SKIP | `linkProcCoWarpInitPre` | Warp beendet den Ritt beim Start (Zeilen 1165 bis 1170). |
| `KagoExecuteFly` | Pre | `kagoExecuteFlyPre` | Absturzschutz `guardPathBeforeCall` für `mpPath1` (Zeilen 1178 bis 1190). |
| `KagoExecuteStagger` | Pre | `kagoExecuteStaggerPre` | Absturzschutz wie bei `KagoExecuteFly`. |
| `MainReset` | Pre, ohne SKIP | `mainResetPre` | Soft-Reset: Ritt-Ende vor dem Titel (Zeilen 1363 bis 1365). |

Die Angabe „ohne SKIP“ gilt für alle Kern-Hooks außer `SaveMoveDisp`, `SaveMoveDisp2` und `CardSave`. Nur in `ride.cpp` Zeilen 1335 und 1355 steht `HOOK_SKIP_ORIGINAL`.

Nur im Testbuild (`KFF_DEV_AUTOTEST`, `hooks.cpp` Zeilen 446 bis 455) registriert der Kern vier Diagnose-Hooks: `LinkSetDamagePoint` (Pre), `LinkCheckPolyDamage` (Post), `LinkProcWolfDamageInit` (Pre) und `LinkCheckWolfLandAction` (Post). Ihre Handler sind in dieser Dokumentation nicht beschrieben.

`KagoCheckGroundHeight` und `KagoCheckNextPath` sind als `DEFINE_HOOK` (`hooks.cpp` Zeilen 31 und 37) und als Enum-Werte (`hooks.hpp` Zeilen 30 und 36) vorhanden, aber nicht registriert.

Die Restart-Hooks `AlinkCheckRestartRoom` und `AlinkCheckRoofRestart` registriert `freiheit` nur unter dem Schalter `KFF_CORE_RESTART_HOOKS` (`freiheit.cpp` Zeilen 1743 bis 1746). Der Schalter ist in `hooks.hpp` (Zeile 70) auf 1 gesetzt.

### 8.4 Hook-Besitz der Module

| Ziel | Art | Modul (Priorität) | Handler (Datei:Zeile) | Zweck |
|---|---|---|---|---|
| `KagoCheckRoofHeight` | Pre, mit SKIP | `freiheit` (400) | `onRoofPre` (`freiheit.cpp` 4327; Handler 1304) | Deckenhöhe des Vogels (WP6, vorher Post). Für den eigenen Vogel läuft das Original nicht, wenn `roofKept()` falsch ist: Rückgabewert und `mRoofHeight` sind `kNoRoof` (1e9), `field_0x6e3` und `field_0x6e5` sind 0, in allen Aktionen und Modi gleich. `roofKept()` liefert wegen des entfernten Schlüssels `roof_limit` immer den Standardwert `false`. `checkGroundHeight` bleibt Vanilla. |
| `AlinkStartRestartRoom` | Pre, mit SKIP | `freiheit` (400) | `onStartRestartRoomPre` (4329; Handler 1429) | WP6: Raum-Neustart im Ritt ohne Ereignis bei lebendem Link liefert 0 („nicht gestartet“). Bei Tod, Ereignis oder `isEventRun` und außerhalb des Ritts läuft das Original. |
| `KagoExecute` | Pre, ohne SKIP | `freiheit` (400) | `onBirdExecutePre` (4339; Handler 1751) | WP6: Startposition der Abtastung und CrrPos aus (`field_0x6dc`) auch in Ritt-Frames ohne `onFrameBird`, etwa im Frame des B-Tipps. |
| `ChangeOpeningScene`, `GameStart` | Pre, ohne SKIP | `autowolf` (250) | `awOpeningScenePre`, `awGameStartPre` (`autowolf.cpp` 696 und 697; Handler 673 und 682) | WP6: Titel und Laden verwerfen einen offenen Verwandlungszustand (`awDropForNewGame`). |
| `KagoSetSceneChange` | Pre | `freiheit` (400) | `onSetSceneChangePre` (1738) | Szenenwechsel-Hook für den Vogel. |
| `KagoExecuteFly` | Pre | `freiheit` (400) | `onExecuteFlyPre` (1739) | Höhenregeln (Untergrenze, Wände) vor dem Flugschritt. |
| `StageChangeScene` | Pre | `freiheit` (400) | `onStageChangeScenePre` (1740) | Szenenwechsel-Hook. |
| `StageNextStage` | Pre | `freiheit` (400) | `onStageNextStagePre` (1741) | Szenenwechsel-Hook. |
| `KagoCheckFlySceneChange` | Pre | `freiheit` (400) | `onFlySceneChangePre` (1742) | Szenenwechsel im Flug. |
| `AlinkCheckRestartRoom` | Pre | `freiheit` (400) | `onRestartRoomPre` (854, SKIP 865), nur mit `KFF_CORE_RESTART_HOOKS` | Raumneustart. |
| `AlinkCheckRoofRestart` | Pre | `freiheit` (400) | `onRoofRestartPre` (874, SKIP 882), nur mit `KFF_CORE_RESTART_HOOKS` | Deckenneustart. |
| `KagoExecute` | Post | `freiheit` (400) | `onBirdExecutePost` (1747) | Auswertung nach dem Vogel-Frame. |
| `LinkExecute` | Post | `freiheit` (400) | `onLinkExecutePost` (1748) | Auswertung nach dem Link-Frame. |
| `KagoExecuteFly` | Pre | `flug` (500) | `onExecuteFlyPre` (`flug.cpp` 1657; Handler 300) | Fassrolle und Eingabe des Frames (Abschnitt 4). |
| `KagoExecuteFly` | Post | `flug` (500) | `onExecuteFlyPost` (1659; Handler ab 405) | Geschwindigkeit `V` und Bahn je Tick. |
| `KagoFlySpeedCalc` | Pre | `flug` (500) | `onFlySpeedCalcPre` (1663; Handler 322 bis 402) | Lenkung im Flugmodus des Spiels (`mMode` 2) mit `HOOK_SKIP_ORIGINAL` (Zeile 396). Bei einer Ausnahme SKIP nur für den eigenen Vogel im Ritt (Zeile 400). |
| `PadRead` | Post | `kamera` (300) | `onPadRead` (`kamera.cpp` 1789; Handler 1703 bis 1712) | Nur die Stop-Heilung der Spielkamera. Mit `try`/`catch` (Härtung WP8). |
| `CameraStop` | Pre | `kamera` (300) | `onCameraStop` (1795; Handler 1720 bis 1742) | Im Ritt ohne Ereignis und Demo wird der Kamerastopp abgefangen (SKIP in Zeile 1737), sonst läuft das Original (Log `STOP-DURCHLASS`). Mit `try`/`catch` (Härtung WP8). |
| `MeterDraw` | Post | `tempo` (200) | `onMeterDrawPost` (`tempo.cpp` 1182; Handler 513) | HUD-Zeile mit Gang und Geschwindigkeit. |
| `PadRead` | Post | `tempo` (200), nur Testbuild | `onPadReadObserve` (`tempo.cpp` 1186) | Testbeobachter für WP8.11: Tasten und Sperrflags vor und nach dem Konsum des Kerns. Liest nur. |
| `LinkExecute` | Post | `mensch` (150) | `on_link_execute_post` (`mensch.cpp` 1466) | Griff-Wächter. |
| `KagoExecute` | Post | `mensch` (150) | `on_kago_execute_post` (1470) | Griff-Wächter. |
| `LinkPosMove` | Post | `mensch` (150) | `on_link_pos_move_post` (1474) | Position des Links am Vogel. |
| `LinkSetMatrix` | Pre | `mensch` (150) | `on_link_set_matrix_pre` (1478) | Haltung des Links am Vogel, ohne SKIP. |
| `LinkSetBodyPart` | Post | `mensch` (150) | `on_link_body_part_post` (1482) | Körperteile des Links. |
| `LinkSetArmMatrix` | Pre | `mensch` (150) | `on_link_arm_pre` (1486) | Armhaltung, ohne SKIP. |
| `LinkFreeGrabItem` | Pre | `mensch` (150) | `on_link_free_grab_pre` (1490; SKIP in Zeile 1206) | Verhindert das Loslassen im Griff, solange nur B den Griff löst. |
| `KagoExecute` | Pre | `griff` (350) | `onKagoExecutePre` (`griff.cpp` 378) | Skript-Aktion (EVENT, EVENT2, PERCH, PERCH2) am eigenen oder Mod-Vogel aufheben, bevor `action()` sie ausführt; ohne SKIP. |
| `KagoExecutePerch` | Pre | `griff` (350) | `onExecutePerchPre` (430, über `perchGuard` 400) | Ohne `mpPath2` oder am eigenen/Mod-Vogel SKIP (Log `perch ignored: no path`). |
| `KagoInitPerchDemo` | Pre | `griff` (350) | `onInitPerchDemoPre` (434, über `perchGuard`) | Wie `KagoExecutePerch`; ohne Pfad stürzte das Spiel hier ab. |
| `KagoDemoSkip` | Pre | `griff` (350) | `onDemoSkipPre` (442) | `demo_skip` am eigenen/Mod-Vogel SKIP (Log `demo_skip blocked`); ohne lebenden Kargarok SKIP und Ereignis-Ende. Vanilla-Vögel behalten den Skip. |
| `EventBitOn`, `EventBitOff` | Pre | `griff` (350) | `onEventBitOnPre` (508), `onEventBitOffPre` (512), über `storyFlagGuard` (485) | M_067 und M_077 im Ritt SKIP (Log `story flag blocked: <Flag>`). |
| `OrderPotentialEvent` | Pre | `griff` (350) | `onOrderPotentialPre` (519) | Midna-Auftrag im Ritt SKIP mit Rückgabe 0 (Log `midna order refused`, Zähler `midnaOrderRefused`); die Demo-Folgen nimmt `onPadReadPost` zurück. |
| `OrderTalkEvent` | Pre | `griff` (350) | `onOrderTalkPre` (546) | Gesprächsauftrag mit Midna im Ritt SKIP mit Rückgabe 0. |
| `OrderSpeakEvent` | Pre | `griff` (350) | `onOrderSpeakPre` (571) | Sensor (Sprech-Aufträge im Ritt), ohne SKIP. |
| `TagMwaitExecute` | Pre und Post | `griff` (350) | `onTagMwaitPre` (594), `onTagMwaitPost` (634) | Tag_Mwait im Ritt SKIP (kein CANTALK, kein Sprech-Auftrag, Midna bleibt bei Link), außer ein Gespräch des Tags läuft schon; Post ist ein Sensor. |
| `TagHstopExecute` | Post | `griff` (350) | `onTagHstopPost` (638) | Sensor (CANTALK oder TALK eines Tags im Ritt). |
| `LinkProcCoMetamorphoseInit` | Pre | `griff` (350) | `onMetamorphosePre` (645) | Verwandlung über den Spielweg mit eigenem Vogel: Ritt-Ende `Transform` vor dem Proc, ohne SKIP. |
| `PadRead` | Post | `griff` (350) | `onPadReadPost` (709) | Midna-Demo nach abgelehntem Auftrag zurücknehmen; Sensor Skript-Aktion am eigenen Vogel. |
| `LinkExecute` | Post | `griff` (350) | `onLinkExecutePost` (723) | Sensor Heul-Proc im Ritt. |

Die Hooks von `mensch` sind immer registriert. Ohne Menschen-Ritt greifen die Handler nicht ein: `watch_grip` (`mensch.cpp` Zeile 983) bricht ab, wenn `s.rideGrip` nicht gesetzt ist, die Phase nicht `Riding` ist, kein Link vorliegt oder der Griff aus ist (Zeile 984).

`griff` registriert seine 15 Ereignis-Hooks in `installEventHooks` (`griff.cpp` Zeile 741), alle mit Priorität 350 und ohne Replace. Jeder Callback fängt seine Ausnahmen selbst (`evPre`, `evPost`). Dazu kommen die neun Zustands-Hooks aus WP5 (Abschnitt 5.6). Der Griff selbst läuft weiter über die Modul-Callbacks. `autowolf` registriert seit WP6 zwei Hooks (Titel und Laden). `tempo` registriert `MeterDraw`, im Testbuild dazu den Beobachter auf `PadRead`.

Die SKIP-Stellen in `freiheit.cpp` (Zeilen 822, 865, 882, 911, 929 und 949) gehören zu den Szenenwechsel-Vetos und zu den beiden Restart-Hooks.

### 8.5 Fehlerverhalten bei Hooks

Eine Ausnahme wird in drei Stufen behandelt, bevor der SDK sie sieht:

1. Kern-Hooks laufen durch `guardedPre` (`ride.cpp` Zeilen 180 bis 187). Die Funktion fängt alles, protokolliert `exception in <name> (frame skipped)` und liefert `HOOK_CONTINUE`.
2. Modul-Callbacks laufen durch `forEachModule` (`ride.cpp` Zeilen 135 bis 149, Abfang in den Zeilen 142 bis 147). Eine Ausnahme wird je Modul protokolliert: `module <name> threw in <Stelle> (call skipped)`.
3. Hook-Callbacks der Module haben eigene `try`/`catch`-Blöcke. Die Anzahl beträgt: `flug` 3, `freiheit` 10, `tempo` 1, `mensch` 7, `kamera` 2 (`onPadRead` mit der Stop-Heilung und `onCameraStop`, seit der Härtung WP8). `griff` schützt seine 15 Ereignis-Hooks (WP4) über die Hüllen `evPre` und `evPost` (eine Ausnahme wird gezählt und gedrosselt protokolliert, ein Pre liefert dann `HOOK_CONTINUE`); die Zustands-Hooks aus WP5 fangen ihre Ausnahmen selbst (`countException`, das Original läuft). Im Testbuild kommt in `tempo` der Beobachter `onPadReadObserve` hinzu, ebenfalls mit `try`/`catch`. Jeder Hook, den ein Modul registriert, ist damit abgesichert.

Was danach im SDK geschieht, belegt der Quellcode:

- Eine Ausnahme aus einem Pre-Hook ruft `fail_hook_callback` auf (`dusklight/src/dusk/mods/svc/hook.cpp`, Zeilen 598 bis 601). Die Kette läuft mit dem nächsten Hook weiter (`continue`).
- Eine Ausnahme aus einem Replace ruft `fail_hook_callback` auf und liefert `MOD_ERROR` (Zeilen 616 bis 618).
- Eine Ausnahme aus einem Post-Hook ruft `fail_hook_callback` (Zeile 645) bzw. `fail_unknown_hook_callback` (Zeile 647) auf. Die übrigen Post-Hooks laufen weiter.
- `fail_hook_callback` (`hook.cpp`, Zeile 125) ruft `fail_mod(*mod, MOD_ERROR, ...)` auf. Die Meldung lautet `Exception in <pre|replace|post> hook callback: <Text>`.
- `fail_mod` (`dusklight/src/dusk/mods/loader/context.cpp`, Zeilen 21 bis 37) macht das Paket inaktiv (`active = false`), setzt `loadFailed = true`, speichert die erste Ursache in `failureReason`, entfernt die Dienste des Pakets (`remove_services_for_provider`) und protokolliert `failed: <Text> (<Code>)`. Der Kommentar in Zeilen 30 und 31 sagt: Die vollständige Entladung erfolgt später über `deactivate_mod` an einem sicheren Punkt.

Die Folge für Modder: Eine Ausnahme in einem Hook-Callback, der nicht durch eine der drei Stufen abgefangen wird, deaktiviert das gesamte Paket. Ein neuer Hook braucht deshalb einen eigenen `try`/`catch`-Block.

Fehlgeschlagene Installation: Schlägt `install` eines Moduls fehl, schaltet der Kern nur dieses Modul ab (`core.cpp` Zeilen 63 bis 69). Hooks, die vorher registriert wurden, bleiben aktiv. Die Trampoline prüfen `moduleActive` nicht (Befund F17, offen; `src/core/hooks.cpp` enthält keine Prüfung). Das Log im Modul `freiheit` (Zeile 1733) spricht von „module part inactive“. Tatsächlich wird das ganze Modul abgeschaltet.

## 9. Konfiguration

Namensraum in `config.json`: `mod.com_kaizuki_kargarok__flight.<name>`. Die Schreibweise entsteht durch `escape_mod_id_for_config` im SDK (`dusklight/src/dusk/mods/loader/loader.cpp`): `.` wird zu `_`, `_` wird zu `__`.

- `Key` (`src/core/kff/config.hpp`) und die Tabelle `kDefs` (`src/core/config.cpp`) stehen in derselben Reihenfolge. Ein `static_assert` verbindet die Anzahl mit `Key::Count`.
- Einträge mit `nullptr` als Namen werden nicht registriert: `Mode`, `CamMode`, `EventKeep` und `RoofLimit`. Sie liefern immer den Standardwert. Die Enum-Einträge bleiben im Code.
- Die Schlüssel `mode`, `cam_mode`, `event_keep`, `roof_limit`, `far_plane`, `fog_off`, `landing_frames` und `all_rides` werden nicht gelesen.
- Lesen über `getBool(Key)`, `getInt(Key)` und `getFloat(Key)`. Zahlen werden beim Lesen geklemmt. `getFloat` ersetzt NaN und Werte mit `|v| > 1e300` durch den Standard.
- `getBool(Key::InvertSteer)` verknüpft den Altschlüssel `invert_steer` mit ODER zu `lenkung_invertieren`.
- Nach der Registrierung prüft `checkStoredValues()` jeden INT- und FLOAT-Schlüssel. Liegt ein Wert außerhalb des Bereichs oder ist er nicht endlich, gibt es genau eine Warnzeile mit dem Schlüssel und dem wirksamen Wert. Falsche Typen meldet der Host selbst (`Failed to load key '<Schlüssel>' from config value`); dafür gibt es keine zweite Zeile. Die Zeile `config wirksam: ...` nennt die Werte nach dem Klemmen.
- Bekannte Lücken: Ein falsch geschriebener Schlüssel erzeugt keine Warnung, weil der Host unbekannte Schlüssel nicht meldet. Werte von 0 bis 59 bei `min_ground_clearance` liegen im Bereich der Konfiguration. Angehoben werden sie erst im Modul `freiheit`, deshalb zeigt die Zeile `config wirksam` dann den Wert vor der Anhebung.
- Der Kommentar in `src/core/kff/config.hpp` nennt bei `MinGroundClearance` „-1 = aus“. Richtig ist: -1 ergibt den Vanilla-Wert 300 Einheiten (`freiheit.cpp`, Funktion `clearance()`). Der Kommentar wurde in 2.1.0 nicht geändert, weil die Datei nicht zum Besitz von WP10 gehört.
- Die Namensregel `[A-Za-z0-9_-]` steht nur als Kommentar in `config.cpp`. Der Code prüft sie nicht.
- Ohne Konfigurationsdienst gelten die Standardwerte. Das Log meldet `config service not available, default values active`.

Wirksame Schlüssel:

| Key | Name | Typ | Standard | Bereich |
|---|---|---|---|---|
| `InvertSteer` | `lenkung_invertieren` (Altschlüssel `invert_steer`) | bool | false | |
| `CruiseSpeed` | `cruise_speed` | float | 50 | 0 bis 200 |
| `MinGroundClearance` | `min_ground_clearance` | float | -1 | -1 bis 2000 |
| `StartGear` | `start_gang` | int | 3 | 1 bis 5 |
| `HudShow` | `hud_anzeigen` | bool | false | | (Welle 3, N6: Standard aus. Die Zeile „Gang … km/h“ erscheint nur mit `true`. Die Testzeile „TEST …“ steht nur im Testbuild.) |
| `FovEffect` | `sichtfeld_effekt` | bool | true | |
| `MenschRitt` | `mensch_ritt` | bool | false | |
| `DeckeAbstieg` | `decke_abstieg` | bool | false | |

## 10. Konstanten

Auswahl mit Bedeutung laut Kommentar im Code. Frames sind Ticks, bei 30 je Sekunde.

Ritt (`src/core/ride.cpp`, Zeilen 47 bis 69 und 119):

| Konstante | Wert | Bedeutung |
|---|---|---|
| `kFriendlyBirdParam` | 0x000100FF | Parameter des Vogels: Typ 1 in den Bits 16 bis 23, 0xFF: kein Stage-Pfad (Zeile 49). |
| `kSpawnUp` | 350 | Spawnhöhe über Link (Zeile 50). |
| `kNoRoof` | 1e9 | Keine Decke im Modus frei (Zeile 51). |
| `kDepartDeleteFrames` | 100 | Frames bis zum Entfernen nach dem Absteigen (Zeile 52). Der Kommentar nennt 90 (Abschnitt 16). |
| `kRiderLostFrames` | 90 | Link ohne Cargo-Carry in Folge (Zeile 53). Nach dem Code greift das Griff-Fenster von 60 Ticks (`griff.cpp` Zeilen 84 und 879 bis 883) zuerst. Die Notiz zu P2-08 nennt dagegen ein Ritt-Ende nach 90 Ticks (Abschnitt 17). |
| `kCreateTimeoutFrames` | 900 | Vogel nie gesehen, dann Sitzung zurücksetzen (Zeile 55). |
| `kDeleteRetryFrames` | 60 | Entfernen wiederholen, falls der Vogel lebt (Zeile 56). |
| `kPendingGiveUpTicks` | 900 | Angefordertes Entfernen nach so vielen Pad-Ticks aufgeben (Zeile 57). |
| `kLogEvery`, `kLinkLogEvery` | 60, 150 | Log-Takt in Frames (Zeilen 58, 59). |
| `kSummonTimeoutFrames` | 1800 | Vogel in `Summoning` ohne Ritt-Start entfernen, etwa 60 s (Zeile 60). |
| `kFallWatchFrames` | 600 | Link-Frames, in denen die Fallhöhe nach dem Absteigen gehalten wird (Zeile 61). |
| `kLandMaxFrames` | 240 | Obergrenze des Landeanflugs, 8 s bei 30 Hz (Zeile 62). |
| `kLandPlanFrames` | 60 | Ticks, in denen der Anflug auf `kLandClearance` zielt (Zeile 65). |
| `kLandClearance` | 150 | Bodenabstand, bei dem der Anflug endet. Annahme, im Spiel zu prüfen (Zeile 64). |
| `kLandSink`, `kLandSinkMax` | 8, 60 | Mindest- und Höchstsinkrate je Tick (Zeilen 63, 66). |
| `kNoGroundY` | -1e5 | `checkGroundHeight` liefert darunter „kein Boden“ (Zeile 67). |
| `kDefaultSpeedScale`, `kDefaultGear` | 0,5, 3 | Gang 3 von 5 (30, 40, 50, 75, 100 Prozent; Zeilen 68 und 69). |
| `kDismountWishTicks` | 60 | So lange gilt ein B-Wunsch außerhalb von Flug und Angriff (Zeile 119). |

Flug (`src/modules/flug.cpp`, Konstanten in den Zeilen 82 bis 96): `kPitchRateMax` 640 und `kYawRateMax` 512 (Raten je Schritt, in Vanilla-Einheiten), `kRollRateMax` 1024, `kBankMax` 8192 (45 Grad), `kBarrelFrames` 64 (Fassrolle, 360 Grad), `kDirDeadZone` 0,2 (Fassrolle links, wenn der Stick X unter −0,2 liegt; Zeile 88 und 193), `kTrimDeadZone` 0,10 (neutraler Nick-Stick), `kTrimRate` 0x0200 (2,8 Grad je Frame, 90 Grad in 32 Frames), `kStickDeadZone` 0,02 (nur für die Gier), `kGearMinRate` 0,30 (Untergrenze der Nick-, Gier- und Schräglagenrate, damit die Gier im Landeanflug bei Weg 0 nicht ausfällt, E9).

Gang und Tempo (`src/modules/tempo.cpp`, Konstanten in den Zeilen 54 bis 80): `kGearPct` = {0,30; 0,40; 0,50; 0,75; 1,00}; `kScaleMin` 0,30; `kScaleMax` 1,00; `kRiseRate` 0,02; `kFallRate` 0,03; `kDashRiseRate` 0,05 (Hochlauf um 5 Prozentpunkte je Tick); `kDashReleaseRate` 0,05 (Ausklang); `kDashRamp` 14; `kDashHold` 35; `kDashRelease` 10; `kDashCooldown` 85 (Sperrzeit ab Start, Zeile 69); `kLandRamp` 30 (Bremsrampe im Landeanflug, E9); `kFovMaxDeg` 10; `kWindFrom` 0,75 (Windton ab Gang 4); `kWindEvery` 30; `kKmhPerUnit` 1,08 (km/h je Einheit und Tick, Annahme, Abschnitt 16).

Der Dash-Start steht in `tempo.cpp` Zeilen 412 bis 435 (Sperrzeit ab Start in Zeile 429; im Landeanflug kein Dash, Zeile 417). Bei gehaltener Bremse wird der Dash abgelehnt (Zeile 420). Die Bremsrampe des Landeanflugs steht in den Zeilen 437 bis 456, die Zielregel für den Faktor in den Zeilen 457 bis 481. Der Kommentar bei `kDashRiseRate` nennt beide Fälle: aus Gang 1 dauert der Hochlauf 14 Ticks, aus Gang 3 10 Ticks.

Kamera (`src/modules/kamera.cpp`): `kYawRateDeg` 1,5; `kPitchRateDeg` 0,7; `kDeadzone` 0,12; `kElevDefault` 14; `kElevMin` −5; `kElevMax` 60 (Grad); `kElevTestMin` −70 (nur Testbuild); `kDistDefault` 650; `kDistMin` 300; `kDistMax` 1400; `kDistGrowRate` 26 (Wachstum je Tick, wenn keine Wand den Abstand zwingt; vor dem Vorlauf k22-kamera 6); `kRetractMax` 120 (Schrumpfen je Tick, wenn der Wunschabstand kleiner ist und keine Wand zwingt); `kLookAhead` 900 (Vorausblick der Wandsonde); `kWallMargin` 35; `kEyeClearance` 70; `kIdleFrames` 120 (ohne C-Stick, dann Rückschwenk); `kBlendFrames` 30; `kFovMax` 10; `kStopHealFrames` 30 (Zeile 118); `kLogEvery` 60 (Zeile 119).

Abstandsregel (`kamera.cpp` Zeilen 939 bis 951): Liegt eine Wand näher als der Abstand, springt der Abstand sofort auf die Wandgrenze. Sonst schrumpft er je Tick um höchstens `kRetractMax` in Richtung des Wunschabstands und wächst um höchstens `kDistGrowRate`. Die Kommentare im Kopf von `kamera.cpp`, bei `kRetractMax`, an der Wandsonde und im Abschnitt Abstand beschreiben diese Regel (berichtigt in der Härtung WP8).

Höhe und Wände (`src/modules/freiheit.cpp`): `kNoRoof` 1e9 (Zeile 85); `kNoFloor` −1e6 (Zeile 86); `kRealRoofBelow` 1e8 (Zeile 87); `kDefaultClearance` 300 (Zeile 88); `kMinClearance` 60 (Zeile 89); `kMaxClearance` 2000 (Zeile 90); `kSkin` 12 (Zeile 92); `kSlideIters` 3 (Zeile 93); `kSideR` 100 (Zeile 95); `kVertR` 80 (Zeile 96); `kSlideTurn` 0x0600 (Zeile 101, 8,4375 Grad je Frame, nur im Testbuild mit `KFF_WALL_STEER=1`); `kSlideTurnSoft` 0x0040 (Zeile 102, 0,35 Grad je Frame, nur Testbuild mit `KFF_WALL_STEER=sanft`; im Release dreht `freiheit` den Kurs nie, `wallSteerMode` liefert dort `Off`); `kStallFrames` 6 (Zeile 103); `kStallProbeLen` 180 (Zeile 104, nur Testbuild); `kClimbStep` 20 (Zeile 105); `kLinkFootClear` 50 (Zeile 114, Füße von Link mindestens so hoch über Gelände oder Wasser); `kWallMaxNy` 0,5 (Zeile 115: Wand, wenn die Normale-y unter 0,5 liegt, also Flächen steiler als 60 Grad; Kommentar in der Härtung WP8 berichtigt); `kFloorMinNy` 0,7 (Boden); `kCeilMinNy` −0,7 (Decke).

Die Klemmung der Untergrenze (`freiheit.cpp`, Zeilen 179 bis 192): Ein negativer Wert ergibt 300. Werte unter 60 ergeben 60. Werte über 2000 ergeben 2000.

## 11. Fehler- und Gating-Regeln

- Der einzige Schalter für den Build ist `KFF_DEV_AUTOTEST` (CMake-Option, Abschnitt 12.3). Die Schalter `KFF_CORE_RESTART_HOOKS` und `KFF_CORE_CAMERA_STOP_HOOK` stehen in `hooks.hpp` (Zeilen 70 und 73) fest auf 1 und gelten in beiden Builds.
- Die Umgebungsvariablen werden nur im Testbuild gelesen (Abschnitt 14.2). Alle Lesestellen liegen in Blöcken unter `#if defined(KFF_DEV_AUTOTEST)` (Kern: `core.cpp`, `input_test.cpp`, `autotest.cpp`; Module: `kamera.cpp`, `freiheit.cpp` mit `wallSteerMode`, `wp5IgnoreWater` und den WP1-/WP6-Szenarien, `griff.cpp` mit `envSet`, `testFlag`, `wp4Scenario` und dem WP5-Testteil). `autotest.cpp` und `input_test.cpp` werden im Release nicht gebaut. Geprüft am Release-Paket (WP10.2): `nm -u` zeigt kein `getenv`, `strings` findet kein `KFF_` oder `KFS_`. Das Gate in `make-dist.sh` prüft `KFF_AUTOTEST` und Autotest-Symbole (Abschnitt 12.4).
- Im Release gibt es keine Kursdrehung durch `freiheit`. `wallSteerMode()` liefert dort `Off`, `wallSteerOn()` also `false` (`freiheit.cpp` Zeilen 239 bis 259). `slideHeading` (ab Zeile 692) prüft `wallSteerOn()`, `breakStall` (ab Zeile 778) wird nur bei `wallSteerOn()` aufgerufen (Zeile 1244) und wirkt nur im Modus `Full`. Beide sind deshalb im Release aus. Gemessen (Härtung WP8): ohne Schalter keine Fremdgier im Flug (K18, Abschnitt 14.5).
- Das Ausweichen im Stillstand (`avoidStall`, ab Zeile 853) läuft dagegen auch im Release, aufgerufen aus Zeile 1257. Es ändert die Höhe, nicht den Kurs.

## 12. Build und Paket

### 12.1 Voraussetzungen

- CMake 3.25 oder neuer.
- Ein Dusklight-Quellbaum: `-DDUSKLIGHT_DIR=<Pfad>`. Die Angabe ist Pflicht. Ohne sie bricht CMake im Standalone-Build mit `FATAL_ERROR` ab (`DUSKLIGHT_DIR fehlt: ...`). Der Standardwert ist leer.
- Das Dusklight-Binary für den Link: `-DDUSK_GAME_EXE=<Pfad>`. Laut Kommentar in `CMakeLists.txt` vermeidet die Angabe den Download eines Link-Stubs in `cmake/ModSDK.cmake`.
- Zielplattform: macOS arm64 mit Dusklight 2.0.3.

### 12.2 Release-Build

```
cmake -S . -B build -DDUSKLIGHT_DIR=<Dusklight-Quellbaum> -DDUSK_GAME_EXE=<Dusklight-Binary> -DCMAKE_BUILD_TYPE=Release -DKFF_DEV_AUTOTEST=OFF
cmake --build build --target kargarok_flight_package --parallel 2
```

Das Ziel `kargarok_flight_package` erzeugt `build/mods/kargarok_flight.dusk`. Das Ziel `kargarok_flight` baut nur die Bibliothek.

Auf macOS läuft bei Release, RelWithDebInfo und MinSizeRel nach dem Bau `strip -S` auf der Bibliothek (`CMakeLists.txt`). Der Schritt entfernt die Debug-Stabs mit Entwicklerpfaden und lässt die Export-Symbole stehen. Der SDK-Strip wirkt nach dem Kommentar in `CMakeLists.txt` nur unter Linux.

Mit `KFF_DEV_AUTOTEST=OFF` (Standard) werden `src/core/autotest.cpp` und `src/core/input_test.cpp` nicht übersetzt. Die Schnittstelle im Header bleibt als leere Stubs erhalten.

### 12.3 Testbuild

```
cmake -S . -B build-test -DDUSKLIGHT_DIR=<Dusklight-Quellbaum> -DDUSK_GAME_EXE=<Dusklight-Binary> -DCMAKE_BUILD_TYPE=Release -DKFF_DEV_AUTOTEST=ON
cmake --build build-test --target kargarok_flight_package --parallel 2
```

`KFF_DEV_AUTOTEST=ON` übersetzt `src/core/autotest.cpp` und `src/core/input_test.cpp` und setzt die Compile-Definition `KFF_DEV_AUTOTEST=1`. Das Testbuild gehört in einen eigenen Ordner und wird nie als Release-Paket abgelegt. Das Release-Gate in `make-dist.sh` bricht ab, wenn `KFF_AUTOTEST` oder Autotest-Symbole im Paket stehen (Code 5).

### 12.4 Paketierung mit make-dist.sh

Aufruf (Kopf des Skripts `kff-dist-tools/make-dist.sh`):

```
make-dist.sh [QUELLORDNER] [--expect-version V] [--zip] [--jobs N] [--build-dir ORDNER] [--ausgabe ORDNER]
```

- `--expect-version`: erwartete Version in `mod.json`, Standard 2.1.0.
- `--zip`: wird aus Kompatibilitätsgründen angenommen. Die Zip-Datei entsteht in jedem Fall.
- `--jobs`: Parallelität des Builds, Standard 2.
- `--build-dir`: Build-Ordner, Standard `<Skriptordner>/build-<Quellname>`.
- `--ausgabe`: Ausgabeordner, Standard `<Skriptordner>/dist`.

Umgebungsvariablen: `KFF_SRC`, `KFF_DUSKLIGHT_DIR`, `KFF_GAME_EXE`, `KFF_EXPECT_VERSION`, `KFF_JOBS` und `KFF_MAX_LOAD` (höchste erlaubte 1-Minuten-Last, Standard 20). Liegt die Last darüber, bricht der Lauf mit Code 2 ab, bevor er baut. Die Standardpfade zeigen auf den Entwicklungsrechner und sind auf anderen Rechnern zu überschreiben.

Ausgabe in `dist/`:

- `kargarok_flight.dusk`: das Paket. Es ist ein Zip-Archiv mit `mod.json`, `lib/macos-arm64/mod.so`, `res/icon.png` und `res/banner.png`.
- `kargarok_flight.dusk.sha256`: Prüfsumme des Pakets.
- `kargarok-flight-<V>.zip`: Zip-Archiv, das nur `kargarok_flight.dusk` enthält.

Rückgabecodes laut Kopf des Skripts:

| Code | Bedeutung |
|---|---|
| 0 | ok |
| 1 | Aufruf falsch |
| 2 | Vorbedingung nicht erfüllt (Werkzeug fehlt, Version falsch, Last zu hoch, Quelle oder Dusklight fehlt) |
| 3 | Konfiguration oder Build fehlgeschlagen |
| 4 | Paketinhalt unzulässig |
| 5 | Autotest-Code im Paket |
| 6 | Quelle oder Dusklight-SDK wurde während des Baus verändert |

### 12.5 Region

`src/core/region.cpp` liest beim Start die Kennung der eingelegten Disc über das exportierte Symbol `DVDGetCurrentDiskID`. Die ersten vier Bytes sind der Spielcode. `GZ2P` (PAL) wird als getestet gemeldet. `GZ2E` (USA), `GZ2J` (JPN) und alle anderen Kennungen gelten als nicht getestet und bekommen eine Warnzeile. Ist die Kennung nicht lesbar, gibt es ebenfalls eine Warnung.

`dusk::version::getGameVersion` ist im Dusklight-Binary nicht exportiert und wird deshalb nicht aufgerufen. Die Zweige für USA und JPN sind im Code getrennt und nur eine Meldung, sie ändern das Verhalten der Mod nicht.

Stand der Abnahme 2.1.0: Die Disc aus dem Testprofil (EUR-Datei) meldet die Kennung `RZDP`. Das Log zeigt dafür `region: RZDP nicht getestet ...` als Warnung. `GZ2P01` ist in diesem Lauf nicht belegt, obwohl ältere Notizen diese Region nennen.

## 13. Installationsskripte (kff-dist-tools)

Die Skripte liegen im Werkzeugordner `kff-dist-tools` des Entwicklers. Sie gehören nicht zum Paket `kargarok-flight-v2`, das nur die Mod enthält. Gemeinsame Funktionen stehen in `kff-lib.sh`. Anleitung für Spieler: `README-INSTALL.md` (im Werkzeugordner, nicht im Paket).

| Skript | Aufgabe | Rückgabecodes |
|---|---|---|
| `install.sh` | Plan (`--dry-run`), Status (`--status`), Installation (`--paket DATEI`). | 0 ok. 1 Aufruf. 2 Dusklight läuft (bei Installation, `--status` und `--dry-run`; der Trockenlauf ändert in jedem Fall nichts). 4 Paketname endet nicht auf `.dusk`, Paket-ID oder Version passt nicht, Zielordner ist kein Ordner, Sicherungsordner liegt im Mods-Ordner, neuere Version vorhanden, fremde Datei belegt den Zielnamen. 7 Fehler, vorheriger Zustand zurückgeholt. |
| `uninstall.sh` | Verschiebt alle Pakete mit der Mod-ID in die Sicherung (`--dry-run`). Ist nichts installiert, endet es mit 0. | 0. 1 Aufruf. 2 Dusklight läuft. 4 Mods-Pfad ist kein Ordner. 7 Verschieben oder Sichern fehlgeschlagen. |
| `restore.sh` | Stellt eine Sicherung wieder her (`--liste`, `--dry-run`, `--backup ORDNER`, `--ohne-config`). | 0. 1 Aufruf. 2 Dusklight läuft. 4 keine Sicherung, Manifest fehlt, Sicherung unvollständig, fremde Dateien im Mods-Ordner. 7 Wiederherstellung fehlgeschlagen, der Stand liegt im Ordner `<Zeitstempel>-vor-restore`. |

Der Installationsname im Mods-Ordner ist `kargarok_flight.dusk` (`KFF_DESTNAME` in `kff-lib.sh`). Ältere Installationen unter dem früheren Namen `dev.local.kargarok_flight.dusk` werden über die Mod-ID erkannt und in die Sicherung verschoben.

Umgebungsvariablen (in `kff-lib.sh`):

- `KFF_TARGET_DIR`: Mods-Ordner. Standard `~/Library/Application Support/TwilitRealm/Dusklight/mods`.
- `KFF_CONFIG_FILE`: Standard `<Dusklight-Ordner>/config.json`. Der Dusklight-Ordner ist der übergeordnete Ordner von `KFF_TARGET_DIR`.
- `KFF_BACKUP_ROOT`: Standard `<Dusklight-Ordner>/kff-backups`.
- `KFF_EXPECT_VERSION`: Standard 2.1.0.
- `KFF_PROCESS_NAME`: Standard `Dusklight`. Der Wert wirkt nur zusammen mit `KFF_INSTALLER_TEST=1`. Ohne diesen Schalter wird er ignoriert, und das Skript meldet das auf stderr.
- `KFF_INSTALLER_TEST=1`: Testschalter. Nur die Testskripte setzen ihn.
- `KFF_PACKAGE`: Vorgabe für den Paketpfad (`--paket`).

Die Prozessprüfung läuft mit `pgrep -i -x`. Eine echte Installation prüft vor jeder Änderung, ob Dusklight läuft.

Der Ablauf der Installation (`install.sh`): Das Paket wird als `.kff-install.dusk.part` in den Mods-Ordner kopiert, mit `unzip -tq` geprüft, per `mv -f` auf den Zielnamen umbenannt und zuletzt mit `cmp -s` gegen das Paket verglichen. Vor der Änderung wird die `config.json` gesichert. Die Sicherung liegt unter `<KFF_BACKUP_ROOT>/<Zeitstempel>-<Laufnummer>-<Anlass>/` mit `mods/`, `manifest.txt` und einer Kopie von `config.json`.

Pakete mit der Mod-ID `dev.local.kargarok_flight` werden in die Sicherung verschoben, gleich welche Version und ob Datei oder Ordner. Fremde Pakete mit den Präfixen `dev.local.kargarok*` oder `dev.local.kff*` werden nur gemeldet.

Tests:

- `test-installer.sh`: Fixture-Tests für `install.sh`, `uninstall.sh` und `restore.sh`. Das Ergebnis muss `PASS: 74 FAIL: 0` sein. Das Skript arbeitet nur unter `testziel/` und startet zwei eigene Stellvertreter-Prozesse für die Prozessprüfung.
- `test-dist-zusatz.sh`: Tests für die Abbruchcodes 2, 3, 4, 5 und 6 von `make-dist.sh` mit einem Stellvertreter-`cmake` auf dem PATH, für die Ausgabenamen (`kargarok_flight.dusk`, `.sha256`, Zip) und für den Schalter `KFF_PROCESS_NAME`. Das Skript arbeitet nur unter `testzusatz/`.

## 14. Autotest (nur Testbuild)

### 14.1 Aktivierung

- Nur im Testbuild (Abschnitt 12.3). Die Auswahl liefert die Umgebungsvariable `KFF_AUTOTEST=<Name>`. Leer, nicht gesetzt oder `0` bedeutet aus (`autotest.cpp` Zeilen 2051 bis 2064).
- Unbekannter Name: Log-Fehler `autotest: unknown scenario '<Name>', injection off. Available: ...`. Es erfolgt keine Eingabe (Zeile 2073).
- Bekannter Name: Log `autotest: scenario <Name> active (<Beschreibung>)` (Zeile 2060).

### 14.2 Umgebungsvariablen

Alle Lesungen stehen im Testbuild-Gate (Abschnitt 11).

| Variable | Wirkung | Ort |
|---|---|---|
| `KFF_AUTOTEST=<Name>` | Szenario wählen. | `core.cpp` 39, `autotest.cpp` 2053 |
| `KFF_GRIFF_AUS=1` | Testschalter des Griffs. | `griff.cpp` (Hilfsfunktion `envSet`) |
| `KFF_GRIFF_KURS=<Name>` | Testkurs des Griffs: `wand`, `dauer`, `boden`, `decke`, `schlag`, `kreis` oder `dash` (Kopf `griff.cpp`). | `griff.cpp` |
| `KFF_KAMERA_LAUF=wand` | Testlauf der Kamera: Anflug auf die Bergwand von F_SP121 (Kopf `kamera.cpp`, Zeile 36). | `kamera.cpp` 1760 |
| `KFF_KAMERA_TESTLOW=1` | Elevation im Test bis −70 Grad. | `kamera.cpp` 1762 |
| `KFF_WALL_STEER=1` oder `=sanft` | Erlaubt die Kursdrehung beim Gleiten an Flächen: `1` bis 8,4375 Grad je Frame (`kSlideTurn`) samt Stillstands-Ausweg, `sanft` bis 0,35 Grad je Frame (`kSlideTurnSoft`) ohne Ausweg. Nur Vergleichstest (WP8.2); ohne Schalter dreht nur der Stick. | `freiheit.cpp`, Funktion `wallSteerMode` |
| `KFF_KAMERA_STOPHOOK=0` | Der Stop-Abfang (K22) lässt jeden `dCamera_c::Stop` durch, damit die 30-Tick-Heilung greift. | `kamera.cpp` 1764, Wirkung in `onCameraStop` |
| `KFF_WP4=<Teiltest>` | Teiltest des Szenarios `wp4` (WP4): `event`, `skip`, `linkdemo`, `demoskip`, `perch`, `midna`, `flags`, `verwandlung`, `startdialog`, `summonevent`, `tags` oder `howl`. Ergebniszeile `wp4 RESULT <Teiltest> ok|fail ...`. | `griff.cpp` (Funktion `wp4Scenario`) |
| `KFF_WP4_AUS=1` | Vergleichslauf ohne die WP4-Absicherungen in `griff` und im Kern (Skip-Hygiene, Ereignis-Sperre beim Ruf); die Sensoren zählen weiter. | `griff.cpp` (`install`), `ride.cpp` (`wp4Off`), beide über `testFlag` |
| `KFF_WP5=<Fall>` | Fall des Szenarios `wp5` (WP5): `wasser`, `seil`, `reittiere`, `treffer`, `schaden`, `wand`, `wandabwurf`, `stagger`, `lava`, `fluss`, `szenentyp`, `pfeil`, `eym` oder `mensch`. Ergebniszeile `wp5 RESULT <Fall> ok|fail ...`. | `griff.cpp` (WP5-Testteil) |
| `KFF_WP5_OHNE=1` oder `=boden` | Vergleichslauf ohne den WP5-Schutz; `boden`: Wasser zählt für `freiheit` nicht als Fläche. | `griff.cpp`, `freiheit.cpp` (`wp5IgnoreWater`) |
| `KFF_TEST_NOGUARD=1`, `KFF_TEST_STAGGER=1`, `KFF_GRIFF_TESTHIT=<Frame>` | WP5: Griff-Sperren aus, erzwungener Stagger am Vogel, Test-Treffer an Link ab diesem Ritt-Frame. | `ride.cpp`, `griff.cpp` |
| `KFF_TEST_FLAG6DF` | WP5, Fall `szenentyp`: Variante der Fluss-Bedingungen im Abflug. | `griff.cpp` |
| `KFF_TEST_OHNE_AUTOWOLF=1` | WP3: schaltet `autowolf` für den Test ab, damit der Ruf als Mensch von keinem Modul übernommen wird (Szenarien `wp3-chord-b`, `wp3-schild`). | `input_test.cpp` (`autowolfSwitch`) |
| `KFF_WP1_FALL=a|b|c|d` | WP1, Szenario `abgrund-faelle`: a Abgrund bei Leben 12, b Abgrund bei Leben 4, c Boden mit Attribut 0x60, d Landung aus 5200 über Boden (Standard a). | `freiheit.cpp` |
| `KFF_WP1_PORTAL=<Nr>` | WP1, Szenario `warp-portal`: Portal fest wählen. | `freiheit.cpp` |
| `KFS_VOID_DIST=<Einheiten>` | WP1, Absteige-Szenarien: Weg des Test-Teleports zum Abgrund (Standard 30000). | `freiheit.cpp` (`voidDist`) |
| `KFS_OHNE_RITT=1` | WP1, Void- und Abgrund-Fälle: Vergleichslauf zu Fuß an derselben Stelle ohne Ritt. | `freiheit.cpp` (`absteigenTick`) |

### 14.3 Szenarien

75 Szenarien, nach Modul (Stand Zusammenführung Welle 2). Die Namen sind die Zeichenketten in den Definitionen der Szenarien.

- Kern, Eingabe (7, registriert in `input_test.cpp`, WP3): `wp3-karte-ritt`, `wp3-ui-halt`, `wp3-chord-b`, `wp3-dialog-b`, `wp3-schild`, `wp3-schild-wolf`, `wp3-r-analog`.
- Kern (29, registriert in `autotest.cpp`): `zyklus`, `ruf-abflug` (Welle 3, N1: erneuter Ruf im Abflug, Wolf und Mensch), `konsum`, `ruf-spam`, `leben3`, `politik-zyklus`, `politik-titel`, `politik-tod`, `politik-menue`, `politik-speichern`, `politik-speichern-ja`, `spiel-speichern`, `politik-schreiben`, `landung-hoch`, `steuerung`, `decke6`, `e9-klippe`, `e9-mittel`, `e9-b2`, `e9-abgrund`, `e9-stick`, `e5-ritt`, `e5-aussen`, `e5-ritt-karte`, `e1-speichern`, `wp2-tod-abstieg`, `wp2-game-over`, `wp2-b701`, `wp2-spawn-tod`.
- `flug` (6): `looping`, `geradeaus`, `geradeaus2`, `geradeaus-b` (WP8.1/8.4: Startpunkt per Sweep, 1800 Ticks neutral, dann 900 Ticks Dauerkreis), `trimm` (WP8.3), `extrem` (WP8.10).
- `kamera` (2): `kamera`, `stick-relativ` (WP8.7: Kurve mit Kamera hinter und neben dem Vogel, Nick-Vorzeichen).
- `freiheit` (23): `fuss`, `innen`, `grenze`, `szene`, `freiheit`, `wand5`, `wand5s`; WP1: `ladezone-anflug`, `event39-kanal`, `boden-absteigen`, `void-absteigen`, `abgrund-faelle`, `warp-portal`, `raumwechsel`, `rueckkehr-raum-tag`; WP6: `wp6-decke`, `wp6-wand`, `wp6-rand`, `wp6-ruf`, `wp6-timeout`, `wp6-gate`, `wp6-wasser`, `wp6-doc`.
- `tempo` (2): `tempo` (WP8.8/8.9), `eingabe-konsum` (WP8.11: jede Ritt-Taste einzeln, R+Y, R+X, Pausenmenü).
- `griff` (3): `griff`, `wp4` (WP4: Ereignisse, Skip, Midna, Story-Flags, Verwandlung, Heulen; Teiltest über `KFF_WP4`, Abschnitt 14.2), `wp5` (WP5: Zustände im Ritt; Fall über `KFF_WP5`).
- `autowolf` (3): `ordon`, `ordon-m077`, `ordon-event`.
- `mensch` (1): `mensch`.

Das Limit liegt bei `kMaxScenarios` = 64 (`autotest.cpp`). Ein doppelter Name liefert `MOD_CONFLICT`.

### 14.4 Schnittstelle

`kff::autotest` (`src/core/kff/autotest.hpp`):

- `Scenario` mit den Feldern `name`, `description` und `tick` (`TickFn` = `bool (*)(Context&)`, Zeile 47).
- `Context`: Felder `frame` (Frame-Zähler des Szenarios) und `sess` (Sitzung, nur lesen). Methoden: `holdR()`, `tapB()`, `tapA()`, `tap(buttons)` (Flanke und gehalten), `hold(buttons)` (gehalten ohne Flanke), `holdRAnalog(v)` (ab 0,61 gilt R als gehalten), `stick(x, y)`, `cStick(x, y)` (je −1 bis 1), `note(text)` (Log `kff: autotest: <text>`), `canCall()` (Link ist Wolf, sitzt nicht, kein Ereignis, kein eigener Vogel).
- Registrierung: `registerScenario(&szenario)` in `install()`, oder `autotestScenario()` liefert das Szenario des Moduls.

Der Kopf von `autotest.hpp` (Zeile 57) nennt nur fünf der Kernszenarien. Die Liste in Abschnitt 14.3 nennt alle 75.

### 14.5 Regeln für Testläufe

- **Fremdeingabe.** Die Abschlussmeldung eines Szenarios zeigt die Zahl der Fremdeingaben, etwa `steuerung: SZENARIO ende (...) fremdeingabe=<n>` (`autotest.cpp` Zeile 1562; `tempo.cpp` Zeilen 684 und 1019 ebenso). Ein Lauf mit Fremdeingabe ist nicht sauber.
- **Zähler.** Bleibt `birdCalls` (Zähler in `session.hpp` Zeile 97) bei 0, hat kein Ritt stattgefunden. Ein solcher Lauf beweist nichts.
- **Zeitbezug.** Zeitpläne im Ritt zählen Kern-Ticks seit dem Ritt-Beginn, nicht Ritt-Frames: Im Tick des Absteige-B zählt der Kern keinen Ritt-Frame (`kagoExecutePre` kehrt vorher zurück), ein Zeitplan nach Ritt-Frames tippte B dort zweimal. Das Abstiegsende eines Szenarios muss unter `kDepartDeleteFrames` (100) liegen, sonst ist die Phase schon `Idle`.
- **Fremdgier (K18, WP8.9).** Das Modul flug misst im Testbuild jede Kursänderung zwischen zwei eigenen Flugschritten (Log `flug: K18 FREMDGIER` und `flug: K18 SUMME`). Setzt ein Szenario des Moduls flug den Kurs selbst, ruft es `k18Rebase`. Ohne `KFF_WALL_STEER` war die Fremdgier in den WP8-Läufen 0. Einzige Ausnahme: Das Kern-Szenario `e9-klippe` setzt beim Versetzen an den Startpunkt den Kurs ohne `k18Rebase` (Log `e9: Klippe gefunden ... Kurs 28672`); K18 zählt das als einen Fremdschritt von 157,5 Grad im selben Frame (f=92). Das ist Testaufbau, keine Drehung im Flug.
- **Gemessene Stages.** Bisher nur `F_SP121`, `R_SP107`, `F_SP103`, `F_SP115` (WP6) und `F_SP124` (WP4, WP3).
- **Speichersperre.** Empfohlenes Vorgehen für einen Nachweis: Den Speicherstand außerhalb des Rittes per SHA-256 festhalten, im Ritt speichern, Hash vergleichen. Der Ritt muss weiterlaufen. Der Test sollte in einem eigenen Benutzerordner laufen (`--user-dir`). Ob `--user-dir` den Log-Ort ändert, ist nicht geprüft.
- **Logs.** Die Logdatei und ihren Ort siehe README, Abschnitt „Fehlerbehebung“. Das Log-Verzeichnis ist der Ordner `logs` im Cache-Verzeichnis (`dusklight/src/dusk/logging.cpp` Zeile 44).

## 15. Konfliktregeln (Zusammenfassung)

- Ein Pre-Hook mit SKIP beendet die Kette. Das Original und ein `replace` laufen dann nicht (Abschnitt 8.1).
- Ein zweiter `replace` auf dasselbe Ziel liefert `MOD_CONFLICT`.
- Eine Ausnahme in einem Hook-Callback ohne Absicherung deaktiviert das gesamte Paket (`fail_mod`, Abschnitt 8.5).
- `addPre` auf `PadRead` liefert `MOD_UNSUPPORTED`.

## 16. Bekannte Lücken im Code

| Stelle | Befund | Folge |
|---|---|---|
| `hooks.hpp` 30 und 36, `hooks.cpp` 31 und 37 | `KagoCheckGroundHeight` und `KagoCheckNextPath`: definiert, aber nicht registriert. | Tote Einträge. |
| `session.hpp` | `ManualSave` ist entfallen. `Transform` löst seit WP4 das Modul `griff` aus, `Stuck` seit WP6 der Stillstands-Wächter in `freiheit`. | Kein Grund mehr ohne Aufrufer. |
| `config.hpp` 17 | Enum-Eintrag `EventKeep` ohne Schlüssel und ohne Wirkung. | Toter Code (Befund F01, teilweise behoben). |
| `session.hpp` 139 bis 140 | Kommentar nennt „hoechstens ~2 s“ und „Steuerungstabelle Zeile 24“. | Der Code hat 240 Ticks (8 s). |
| `session.hpp` 5 und 6 | Kommentar „(geplant autowolf)“ ist veraltet. Der Satz „releaseRider() vor jedem Loeschen“ trifft nicht zu: `releaseRider` wird nur in `ride.cpp` Zeilen 515 und 699 aufgerufen. | Kommentar falsch. |
| `session.hpp` 70 | Feld `stickY` nennt den alten Schlüssel `invert_steer`. | Kommentar veraltet. |
| `session.hpp` 144 bis 145 | Reihenfolge im Kommentar ohne die Phasenbedingung von `notifyRideEnd`. | Kommentar unvollständig. |
| `ride.cpp` 52 | `kDepartDeleteFrames` = 100, der Kommentar nennt 90. | Kommentar falsch. |
| `freiheit.cpp` 1733 | Logtext „module part inactive“ (Befund F09). | Irreführend: Das ganze Modul ist abgeschaltet. |
| `mod.json` | Beschreibung „Version 2: modularer Kern“. | Veraltet. |
| `ride.cpp` 53 und 54 | Die Kern-Schwelle für Griff ist 90 Frames (`kRiderLostFrames`). Der Kommentar nennt das Griff-Fenster von 60 Ticks (`griff.cpp` Zeile 84) als die früher greifende Grenze. | Befund P2-08, offen (`spaetfunde/uebernahme-zusammenfuehrung.md`, Zeile 60). |
| `ride.cpp` Zeilen 1038 bis 1067 | Der Landeanflug hat keine Reichweitenprüfung. Im Testlauf szene-r1 (Testbuild-Kopie, `spaetfunde/probe2-ergebnisse.json`, Punkt 5) startete er bei einem Bodenabstand von 1593 Einheiten. Das Szenario `landung-hoch` existiert; sein Ergebnis ist nicht dokumentiert. | Entscheidung offen. |
| `tempo.cpp` `kKmhPerUnit` | 1,08 km/h je Einheit und Tick ist eine Annahme (1 Einheit = 1 cm, 30 Ticks je Sekunde). | Anzeige nicht gemessen (Befund P2-13). |
| `mensch.cpp` | Teilport. Der Krallen-Roll-Fix fehlt. In der Messreihe von `spaetfunde/uebernahme-mensch.md` (Zeilen 54, 56 und 58) fiel `clawGap` (tiefste Kralle minus Kopfoberkante) in Kurven bis auf −16,4; das Ziel aus dem Gerüst (mindestens 20) wurde nicht erreicht, der Höchstwert war 9,4. Gelenk 0x23 fehlte im Modell mit 35 Gelenken. Bei Frame 30 eines Laufs sprang die Kameradistanz bei Wandkontakt von 650 auf 0 und erholte sich nach rund 4,6 Sekunden. | Nur mit `mensch_ritt`. Standard aus. |
| `kamera.cpp` Stop-Abfang und Stop-Heilung (K22) | Belegt nur mit einem im Test ausgelösten `dCamera_c::Stop` im Ritt (Härtung WP8): Abfang in drei Läufen des Szenarios `kamera` (`stopSkips=1`, Log `STOP-ABFANG`), mit `KFF_KAMERA_STOPHOOK=0` QuickStart nach 29 Ticks (Log `STOP-HEILUNG`). Ein natürlicher Auslöser im Ritt ohne Ereignis (etwa der Elektroschaden, `d_a_alink_damage.inc` Zeile 1721) ist auf den Test-Stages nicht erreichbar. Ballonmenü (`d_a_kago.cpp` Zeile 246) und Ereignis-Stopps rufen `Stop` erst nach angenommener Demo; dann läuft das Original (`eventOrDemo`). | Abfang bleibt (keine Regression beobachtet), die Heilung ist Rückfall. Ballonspiel und Schadenskamera sind im Spiel nicht geprüft. |
| `freiheit.cpp` Stillstand | Eckenstillstand unter einer Decke oder einem Überhang: 148 Frames statt höchstens 30 (Messlauf innen-r5). Die Untergrenze stoppt das Sinken bei 380 Einheiten (`spaetfunde/uebernahme-zusammenfuehrung.md`, Zeilen 16 und 80). | Entscheidung zur Untergrenze im Stillstand offen. |
| Gegner | Gegnertreffer im Ritt (Profil 506, E_YR) sind nicht geprüft; in Testläufen traten sie auf (`spaetfunde/uebernahme-zusammenfuehrung.md`, Zeile 88; Lauf kamera-wand-r6, Zeile 31). | Offen. |
| Landung auf schädlichem Boden | Im Lauf zyklus-r4 gab es einen Schadenspunkt durch eine Bodenfläche (`spaetfunde/uebernahme-zusammenfuehrung.md`, Zeile 70). | Offen. |
| Landeanflug an steilem Gelände | Der Anflug bremst in 30 Ticks auf null (`tempo` `kLandRamp`, Härtung WP8). Steigt das Gelände unter dem Vogel steil an, ist die Freigabehöhe aber schon nach wenigen Frames erreicht (Lauf stick-relativ: Freigabe nach 2 Frames bei Vogeltempo 49,9). Die Horizontalgeschwindigkeit von Link setzt der Kern bei der Freigabe trotzdem auf null (`releaseLandedHorizontal`). | Kein Schaden beobachtet; offen. |

In der Härtung WP8 behoben und deshalb nicht mehr in der Tabelle: `try`/`catch` in beiden Hooks von `kamera` (Abschnitt 8.5), die Abstands-Kommentare in `kamera.cpp`, der Winkel-Kommentar zu `kWallMaxNy` in `freiheit.cpp` (60 statt 30 Grad), der Kopf von `flug.cpp` (`lenkung_invertieren`, `invert_steer` nur als ODER-Alias; XOR nennt der Kommentar nur noch für das alte Verhalten) und der Dash-Kommentar in `tempo.cpp`.

## 17. Offene Prüfpunkte und Quellen

Dieser Abschnitt ordnet die Kennungen aus dem Prüfprotokoll (`spaetfunde/probe2-review.json`) ein. Der Status ist, soweit nicht anders vermerkt, nur übernommen und nicht nachgeprüft.

- F01: teilweise behoben. Der Enum-Eintrag `EventKeep` steht noch im Code. `mode` und `event_keep` sind nicht registriert.
- F02: behoben (WP3). „call rejected“ und „chord dropped“ sind je Grund gedrosselt und tragen `n=<Anzahl>` (`input.cpp`, Abschnitt 5.6).
- F10: betrifft den GX-Zustand nach dem HUD (`tempo.cpp` Zeilen 552 bis 559). Der Status ist nicht geprüft.
- F17: offen. Hooks, die vor einem Installationsfehler registriert wurden, laufen weiter (Abschnitt 8.5). Dies wurde im Code bestätigt.

Statusübersicht. Quelle für „offen“ ist `spaetfunde/uebernahme-zusammenfuehrung.md` (Zeilen 84, 85 und 72); der Code-Stand von v21-final wurde nur für die genannten Stellen geprüft.
- Erledigt: F05 (in v21-final fehlt `verifyEnd` in `freiheit.cpp`; die Zusammenführung führt F05 noch als offen, Zeile 85), F11, F18 (`spaetfunde/INDEX.md`, Zeile 49), F16 (Abschnitt 2.1.0 in `CHANGELOG.md` vorhanden), HK-01 bis HK-03 (`INDEX.md`, Zeilen 45 und 49; `kamera.cpp` Zeile 33).
- Teilweise: F01 (`EventKeep` im Enum, `config.hpp` Zeile 17), P2-07 (`mode` und `event_keep` nicht registriert; die Einträge in `config.cpp` Zeilen 29 und 33 stehen noch).
- Offen: F03, F06 (`freiheit.cpp` Zeilen 540, 1777 und 1811), F07 (`kamera.cpp` Zeile 963), F09 (`freiheit.cpp` Zeile 1733), F10, F17, P2-03, P2-08, P2-10, P2-12, P2-13, P2-14.
- Nicht nachgeprüft: P2-11, HK-04.
- P2-04 (Szenario `eingabe-konsum`): In der Härtung WP8 blieb `mHoldLockR` im Ritt gesetzt, solange R gehalten wurde (93 von 93 Ticks), damit nach dem Ritt keine falsche R-Flanke entsteht. Seit der Zusammenführung mit WP3 (RK (a), `src/core/input.cpp`) setzt der Kern im Ritt auch `mHoldLockR` auf 0. Die falsche R-Flanke nach dem Ritt verhindert jetzt `repairLocks` mit einem Schattenzustand der Sperrflags. Lauf w2r-konsum: vor dem Konsum HoldR 93, TrigR 3, HoldL 22 und TrigL 1, danach alle vier 0, Ergebnis `BESTANDEN`, `lockRepairs=0`. Die Zusammenfassung im Testbuild trägt noch den alten Zusatz „(HoldR bleibt, P2-04)“; es gelten die gemessenen Werte. R+Y und R+X lösten im Ritt weder Verwandlung noch Heulen aus.
- P2-03: `callBlockReason` sperrt den Ruf, solange ein Kartenschreiben läuft (Speichern oder Autosave, `cardWriteBusy`, höchstens 600 Pad-Ticks, `kCardBusyTicks`). Im Test nicht gemessen.
- P2-08: Griff-Fenster (60 Ticks, `griff.cpp` Zeile 84) und Kern-Schwelle (90 Frames, `ride.cpp` Zeile 53) sind nicht abgestimmt. Die Zusammenführung (Zeile 60) nennt als Folge ein Ritt-Ende nach 90 Ticks statt nach 60.
- P2-10: B-Wunsch außerhalb von Flug und Angriff (Abschnitt 5.4).
- P2-13: km/h-Anzeige ist eine Annahme.

Flugmessungen, die sich widersprechen. Lauf geradeaus-r4 (`spaetfunde/uebernahme-module.md`, Zeile 37): Phase A mit dHoehe 0,00 und nickMax 0 ist als ok vermerkt, ohne Dauerangabe in dieser Zeile. `spaetfunde/MERGE-PLAN.md`, Zeile 10, nennt für den Befund 60 Sekunden bei der Trimmrate 0x0200; der Lauf ist dort nicht namentlich benannt. Lauf final-geradeaus-r1 (`spaetfunde/probe2-test-flugbahn.md`, Zeilen 13, 21 und 22) schlug fehl: Der Höhenverlauf sank nach etwa 20 Sekunden in einer Wandecke um 444,34 Einheiten. Laut Vorbefund lag die Ursache im Gleiten an einer schrägen Wand; das ist nicht isoliert. Härtung WP8: Auf freier Strecke (Szenario `geradeaus-b`, Startpunkt per Suche ohne Hindernis) lag die Abweichung in drei Läufen bei 0,000 Einheiten Höhe und 0,0000 Grad Kurs in 1800 Ticks, die Fremdgier (K18) war 0. Ohne Wand weicht der Vogel also nicht ab; der Einbruch in der Wandecke bleibt als Wandfall offen.

Gameplay, nicht gemessen: Toast und Menüpfad der Speichersperre, Karte, Dokument und Dialog, ein natürlicher Kamera-Stopp im Ritt (Abfang und Heilung sind nur mit einem Test-Stopp belegt), Verhalten nach dem Abgrund-Anflug, Schildangriff beim Ruf. B im Pausenmenü ist gemessen (Szenarien `e5-ritt` und `eingabe-konsum`: B schließt das Menü, der Ritt läuft weiter).

Außerhalb des Pakets: Die Belegungskarten `TASTENKARTE.md` (Ordner `tastatur-belegung`, Zeile 122) und `PS-KARTE.md` (Ordner `ps-controller-belegung`, Zeile 66) nennen den Landeanflug mit „höchstens etwa 2 s“. Das widerspricht dem Code (240 Ticks, etwa 8 s). Die Karten sind nicht Teil dieses Pakets und wurden nicht geändert.

SDK-Basis: Der Commit `34a144a` gegen den Tag `v2.0.3` (`40457c6`) ist in dieser Prüfung nicht verifiziert, weil keine Versionsverwaltung benutzt werden durfte.

Integrationsdokumente (nicht Teil des Pakets, im Ordner `spaetfunde`): `INDEX.md`, `MERGE-PLAN.md`, `orchestrator-entscheidungen-uebernahme.md`, `uebernahme-zusammenfuehrung.md` (Gesamturteil „teilweise“), `probe2-*.md`, `probe2-review.json`, `absturz-mppath1.md`.

## 18. Anhang: Prüfprotokolle zu 2.1.0 (aus CHANGELOG verschoben)

Die folgenden Abschnitte stammen aus dem früheren CHANGELOG-Abschnitt 2.1.0. Sie beschreiben Läufe und Messwerte. Den Leistungsumfang nennt CHANGELOG.md.


Ausbau und Härtung des Rittes. Neu sind die Module griff und tempo, ein Landeanflug nach B, eine Untergrenze und Gleiten im Modul freiheit, eine Wandgrenze für die Kamera und eine Sperre für das Speichern im Ritt. Das Modul mensch ist ein Teilport und standardmäßig aus. Die Konfiguration ist bereinigt. Ein Tick ist ein Spielschritt. Die Sekundenangaben rechnen mit 30 Ticks je Sekunde (Annahme). Gemessene Werte stammen aus den Testläufen; offene Punkte stehen unter „Bekannte Einschränkungen“. Eingearbeitet ist die Härtung Welle 1: Ritt-Ende (WP2) und Hook-Register mit Lebenszyklus (WP7). Die Prüfläufe dazu stehen unter „Prüfung Härtung Welle 1“. Dazu kommt WP6 (Decke, Wände, Kartenrand, Spawn-Prüfung, Ruf-Gates, Zeitgrenzen, schnelles B); die Prüfläufe stehen unter „Prüfung WP6“. Die Härtung Welle 2 führt WP1 (Szenenwechsel im Ritt), WP3 (Eingabe), WP4 (Ereignisse im Ritt), WP5 (Zustände im Ritt), WP6 und WP8 (Flug, Kamera und Eingabe nachgemessen) zusammen, dazu die Kernaufträge Z1 (Absteigen über einem Abgrund, Abflug, Logzeilen). Die Prüfläufe stehen unter „Prüfung Härtung Welle 2 (Zusammenführung)“.

#### Hinzugefügt

- Modul `griff`: Fehlt Link der Griff am Vogel, wird er wieder angebunden. Hat Link mehr als 60 Ticks in Folge keinen Griff, steigt der Vogel geordnet ab (Log: `griff: <n> Frames ohne Griff, geordnetes Absteigen`). Tod und ein Ereignis ohne Griff beenden den Ritt sofort.
- Modul `tempo`: Fünf Gänge mit 30, 40, 50, 75 und 100 Prozent des Höchsttempos. Start in Gang `start_gang` (Standard 3). Gang hoch und runter mit dem Steuerkreuz, Bremse mit Z (zieht auf Gang 1 und bricht einen Dash ab), Dash mit A. Der Dash steigt je Tick um 5 Prozentpunkte auf 100 Prozent (aus Gang 1 in 14 Ticks), hält 35 Ticks und sinkt danach 10 Ticks lang je Tick um 5 Prozentpunkte zurück auf den Gang. Die Sperrzeit beträgt 85 Ticks ab Start. Dazu kommen ein Sichtfeld-Zusatz von höchstens 10 Grad (`sichtfeld_effekt`), eine HUD-Zeile (`hud_anzeigen`) und ein Windton ab Gang 4.
- Modul `flug`: Fassrolle mit Y, 360 Grad in 64 Ticks. Die Rolle geht nach links, wenn der Stick beim Antippen unter −0,2 nach links steht, sonst nach rechts. Trimmung: Bei neutralem Nick-Stick (Betrag unter 0,1) geht die Nase mit 2,8 Grad je Tick zur Waagerechten.
- Landeanflug nach B (`src/core/ride.cpp`): Ein B-Tipp über Gelände startet den Anflug. Der Vogel sinkt, bis er etwa 150 Einheiten über dem Boden ist oder Links Füße den Boden berühren; spätestens nach 240 Ticks gibt Link frei. Die Sinkrate je Tick ist der Bodenabstand minus 150, geteilt durch die Ticks bis 60, begrenzt auf 8 bis 60 Einheiten. Ein zweiter B-Tipp und ein Abgrund geben sofort frei. Der Kern ändert im Anflug nur die Höhe; Bremsen und Waagerechte kommen aus den Modulen tempo und flug (nächster Punkt).
- Vorlauf e9-landung: Der Landeanflug bremst die Vorwärtsbewegung auf null, und der Vogel hält die Waagerechte. Die Freigabe erfolgt bei Vorwärtsgeschwindigkeit null des Vogels. Gemessen im Lauf e9-klippe: Freigabe 975,6 Einheiten vor der Kante, Vorwärtsgeschwindigkeit des Vogels 0,000. Nachmessung Härtung WP8 (zwei Läufe e9-klippe, dazu e9-mittel und e9-stick): Weg je Tick unter 1 ab Anflug-Tick 29 bis 30, null ab Tick 31; Nase, Rolle und Schräglage ab Tick 30 bei 0 Grad, auch mit Stick hoch/runter und Y im Anflug; Stick links/rechts dreht weiter. In den ersten 10 Frames nach der Freigabe bewegte sich Link horizontal nicht (0,0000 Einheiten je Frame, Grenze 0,05), die Herzen blieben gleich. Der Logwert `link horizontal` (0,662 bis 4,627) ist die Bewegung des Reiters an den Krallen im letzten Frame vor der Freigabe, keine Geschwindigkeit danach.
- Gemessene Freigaben des Landeanflugs: bei 135,3 und 142,8 Einheiten (Lauf zyklus-r6, kein Schaden), bei 148,8 Einheiten (Lauf innen-r5, kein Schaden) und bei 181,1 Einheiten mit Füßen am Boden.
- Speichersperre im Ritt: Das Schreiben der Karte (`mDoMemCd_Ctrl_c::save`) und die Menüfunktionen `saveMoveDisp` und `saveMoveDisp2` werden im Ritt abgewiesen. Der Ritt läuft weiter (Log: `card write refused during ride (mDoMemCd_Ctrl_c::save), ride continues`). Vorlauf e1e5-menue, gemessen im Lauf e1-speichern am Menüeintrag (Log `save locked during ride (path a)`): Die Kartendatei blieb unverändert (SHA-256 wie im Startstand), der Ritt lief 166 Frames nach der Verweigerung weiter. Die Hinweismeldung ist nicht belegt. Autosave-Schreibvorgänge werden nicht verweigert (Hook `updateAutoSave` setzt ein Flag; im Code belegt, im Test nicht ausgelöst).
- Ruf-Sperre mit Grund (`callBlockReason` in `src/core/ride.cpp`, dazu `summon::callBlockReason` und das Ruf-Gate des Moduls griff): Ist der Ruf nicht möglich, bleibt B beim Spiel, und das Log nennt den Grund (`call rejected (<Grund>), B stays`). Gründe sind unter anderem laufende Verwandlung (`transforming`), Ereignis (`event`), Abflug des letzten Vogels (`departing`), kein Platz über Link (`room`), Pause oder Menü, offenes Dusklight-Dokument, Wasser, Klettern, Fall, Schaden, Zielsperre und laufender Schildangriff.
- Ritt-Enden (`EndReason`): 13 statt 7 Gründe. Neu sind Tod (`Death`), Menü, Titel und Laden (`Menu`), Warp (der Ritt endet beim Warp-Start) und Reset (`Reset`). `Transform` beendet den Ritt bei einer Verwandlung über den Spielweg (Härtung Welle 2, WP4). `Stuck` löst seit WP6 der Stillstands-Wächter des Moduls freiheit aus. `ManualSave` ist entfallen (siehe „Entfernt“).
- Modul `freiheit`: Untergrenze über der höchsten Fläche unter dem Vogel (Gelände oder Wasser), gesteuert über `min_ground_clearance`. Die Füße von Link sollen mindestens 50 Einheiten über der höchsten Fläche unter Link liegen; daraus folgt die Untergrenze des Vogels. Gleiten an Wänden und Böden mit höchstens drei Gleitschritten je Tick. Durchflug-Sperre: Endet ein Frame hinter einer Fläche, geht der Vogel auf den Punkt 12 Einheiten vor dem ersten Treffer zurück. Stillstandsausweichen: Hat der Vogel 6 Ticks in Folge einen Treffer ohne Ortsänderung (`kStallFrames`), steigt er je Tick um 20 Einheiten, solange der Weg nach oben frei ist. Sonst sinkt er mit derselben Schrittweite, bis die Vorwärtssicht frei ist.
- Modul `kamera`: Wandgrenze mit Vorausblick von 900 Einheiten. Die Grenze liegt 35 Einheiten vor dem Treffer; liegt eine Wand näher, springt der Abstand sofort dorthin. Ohne Wand bewegt sich der Abstand zum Wunschabstand hin: verkürzen um höchstens 120 Einheiten je Tick, verlängern um höchstens 26 (Vorlauf k22-kamera, vorher 6). Stop-Heilung: Steht die Spielkamera im Ritt ohne Ereignis 30 Ticks still, setzt das Modul sie mit QuickStart zurück. Die Wirkung ist mit einem im Test ausgelösten Stopp belegt (siehe „Bekannte Einschränkungen“).
- Modul `mensch` (Teilport, standardmäßig aus): Mit `mensch_ritt = true` nimmt das Modul den Ruf als Mensch an, und Link sitzt im Griff des Vogels. Im Testlauf liefen Ruf, Griff und B-Landung durch.
- Kern-Hooks: Der Kern registriert im Release 21 Hooks (2.0.0: 7), darunter für Warp, Tod, Titel, Laden, Speichern und Stagger. Dazu kommen die zwei Replace-Hooks auf `daKago_c::checkWallHitFall` und `daKago_c::executeBalloonMenu`. Vier weitere Hooks gibt es nur im Testbuild (Diagnose von Schaden und Landung). Mit `KFF_TEST_CONFLICT` versucht der Testbuild zusätzlich einen zweiten Replace, den das Register abweist. Den Kamera-Stopp-Hook (`CameraStop`) registriert das Modul kamera, der Kern bietet ihn an. Die Schalter `KFF_CORE_CAMERA_STOP_HOOK` und `KFF_CORE_RESTART_HOOKS` stehen in `src/core/kff/hooks.hpp` und sind eingeschaltet.
- Konfiguration: `lenkung_invertieren` (umbenannt aus `invert_steer`), `start_gang`, `hud_anzeigen`, `sichtfeld_effekt`, `mensch_ritt` und `decke_abstieg`.
- Testbuild (`KFF_DEV_AUTOTEST=ON`): 75 Testszenarien (Stand Zusammenführung Welle 2: 35 im Kern, davon 7 aus WP3 in `src/core/input_test.cpp`; 40 in den Modulen), ausgewählt über `KFF_AUTOTEST=<Name>`. Neu seit Welle 1: in WP8 `geradeaus-b`, `trimm`, `extrem`, `stick-relativ` und `eingabe-konsum`, in WP6 acht und in WP1 acht Szenarien im Modul freiheit, in WP4 `wp4`, in WP5 `wp5` (Modul griff). Das Release-Paket enthält den Testcode nicht.
- Installer-Skripte im Ordner `kff-dist-tools`: `install.sh` verweigert die Installation, solange Dusklight läuft (Rückgabe 2). Vorher sichert es alte Kargarok-Pakete mit derselben Mod-ID und die `config.json` unter `kff-backups/`; alte Pakete werden verschoben, nicht gelöscht. Dazu kommen `uninstall.sh`, `restore.sh` und `make-dist.sh`, das das Release-Paket baut und prüft.
- Hook-Register (`src/core/hooks.cpp`, Härtung Welle 1): Jeder Callback läuft über eine Hülle, die seinen Besitzer prüft. Ein abgeschaltetes Modul bekommt keine Aufrufe mehr. Höchstens ein Besitzer je Ziel darf das Original überspringen (SKIP). Liefert ein zweiter Besitzer ebenfalls SKIP, wird er abgewiesen, das Original läuft, das Log meldet `MOD_CONFLICT`, und sein Modul wird abgeschaltet (beim Kern bleibt es bei der Meldung). Zwei Besitzer mit gleicher Priorität auf demselben Ziel und derselben Art (Pre oder Post) sind ebenfalls `MOD_CONFLICT`. Replace-Hooks setzt nur der Kern. Schlägt die Installation oder eine Registrierung eines Moduls fehl, wird das Modul abgeschaltet (Log: `module <Name> install -> <Ergebnis>, <n> registration failure(s), module disabled, <n> callback(s) inert`). Der Kern läuft weiter.
- Startbericht des Registers (Härtung Welle 1): `registry check: callbacks=<n> replaces=<n> modules=<n> disabled=<n> conflicts=<n>`. Gemessen im Release: `callbacks=46 replaces=2 modules=7 disabled=0 conflicts=0`; im Testbuild 50 Callbacks. Härtung WP8: Testbuild 51 (Testbeobachter des Moduls tempo auf `mDoCPd_c::read`). Mit WP6: Release `callbacks=50` (Lauf wp6p3-release-smoke), Testbuild 55. Neu sind im Modul freiheit die Pre-Hooks auf `daKago_c::checkRoofHeight` (statt des Post), `daAlink_c::startRestartRoom` und `daKago_c::execute`, im Modul autowolf die Pre-Hooks auf Titel und Laden, im Testbuild zusätzlich die Schadensquelle (`daAlink_c::setDamagePoint`). Stand Zusammenführung Welle 2 (WP1 bis WP8): Release `callbacks=78 replaces=2 modules=7 disabled=0 conflicts=0` (Lauf w2r-release), Testbuild `callbacks=86`.
- Startprüfung beim Laden (`startupBirdCheck`, Härtung Welle 1): Eigene Vögel ohne Sitzung, etwa aus einer früheren Ladung der Mod, werden entfernt (Log: `startup check: <n> own bird(s) removed`). Mehr als ein solcher Vogel wird als Warnung gemeldet.
- Logzeile `bird deleted id=… riding=… departing=… rideFrames=… departFrames=…` auch dann, wenn die Sitzung beim Entfernen schon leer ist (Härtung Welle 1). Die Werte stammen aus einem Schnappschuss beim Löschauftrag.
- Testbuild (Härtung Welle 1): die Szenarien `wp2-tod-abstieg` (Tod im Abflug), `wp2-game-over` (Ruf im Game-Over), `wp2-b701` (B-Tipp bei Ritt-Frame 701) und `wp2-spawn-tod` (Tod direkt nach dem Ruf). Dazu die Testschalter `KFF_TEST_UNLOAD` (`1`: Ausnahme im Ritt, `summon`: Ausnahme nach dem Ruf), `KFF_TEST_THROW=<Modul>`, `KFF_TEST_BADNAME` und `KFF_TEST_CONFLICT`. Sie prüfen das Entladen, den Fehlerpfad des SDK, eine fehlgeschlagene Registrierung und einen Replace-Konflikt. Im Release sind sie nicht übersetzt.
- Spawn-Prüfung beim Ruf (WP6, `src/core/summon.cpp`): Der Vogel erscheint 350 Einheiten über Link. Der Ruf wird mit dem Grund `room` abgewiesen, wenn eine Fläche (auch eine Rückseite) zwischen Links Füßen und 600 Einheiten darüber liegt, die Decke über dem Spawnpunkt weniger als 250 Einheiten höher liegt oder der Boden unter dem Spawnpunkt weniger als 150 Einheiten tiefer. Kein Boden darunter gilt als frei. Die Zeile davor nennt die Fläche (`spawn check: room blocked (...)`). Seitliche Wände prüft der Ruf nicht.
- Ruf-Gates vor jeder Wirkung (WP6): `kff::summonBird` prüft vor dem Erzeugen des Vogels, dass Link lebt, Wolf ist (Mensch nur mit `mensch_ritt`), nicht verwandelt wird und nicht im Schaden ist, dass kein Vogel und keine Ritt-Phase bestehen, kein Ereignis, keine Pause und kein Dusklight-Dokument offen sind, kein eigener Vogel ohne Sitzung in der Welt ist und Platz für den Spawn ist. Damit gelten die Sperren auch für Rufe aus Modulen (autowolf, mensch). Ein abgewiesener Spawn ändert nichts (Log `call rejected (<Grund>) at spawn`). Ein eigener Vogel ohne Sitzung (Parameter `0x000100FF`) wird beim Ruf entfernt; Vögel des Spiels bleiben unberührt.
- Ritt-Start-Gate (WP6): Der Ritt startet erst, wenn Link lebt und in keinem Ereignis steht (Log `ride start waiting (<Grund>)`).
- Stillstands-Wächter (WP6, Modul freiheit): Steht der Vogel 300 Ritt-Frames ohne Stick-Eingabe im Umkreis von 150 Einheiten, endet der Ritt mit `Stuck` (Log `watchdog: stuck 300 ticks ...`, dazu ein Hinweis im Spiel). Stick-Eingabe, Ortswechsel, Landeanflug und Ereignis setzen ihn zurück. Mit Stick-Eingabe greift er nicht.
- Logzeile `CORE6 spawnRejected=… roomRejected=… strayFound=… rideStartWait=… summonAborted=… transformTimeout=…` (WP6) und im Modul freiheit `SUMMARY5` (Decke, CrrPos, Neustart, Wächter).
- Testbuild (WP6): die Szenarien `wp6-decke`, `wp6-wand`, `wp6-rand`, `wp6-ruf`, `wp6-timeout`, `wp6-gate`, `wp6-wasser` und `wp6-doc` sowie die Testschalter `KFF_TEST_NORIDE=1` (der erste gerufene Vogel startet keinen Ritt) und `KFF_TEST_CRRPOS=1` (Kontrolllauf mit CrrPos im Ritt). Im Release sind sie nicht übersetzt.

#### Geändert

- Die Module `griff` und `tempo` sind umgesetzt. In 2.0.0 waren sie Stubs mit je 24 Zeilen ohne Wirkung.
- Modul `mensch`: In 2.0.0 ein Stub ohne Wirkung, der den Ruf als Mensch ablehnte. Jetzt ein Teilport, standardmäßig aus.
- Modul `flug`: Die Nickumkehr folgt `lenkung_invertieren`. Der Altschlüssel `invert_steer` wirkt zusätzlich, wenn er true ist (ODER-Verknüpfung). Entscheidung OE8: Das ist die einzige Nickumkehr im Ritt. Dusklights „Invert Air/Swim Y Axis“ (`game.invertAirSwimY`) wirkt im Ritt nicht; vorher wirkte sie per XOR mit `invert_steer` (beide gesetzt ergab keine Umkehr). „Invert Air/Swim X Axis“ und „Mirror Mode“ wirken unverändert auf die Kurve. Gemessen im Lauf stick-relativ (Stick Y −0,5): Standard und nur `invertAirSwimY` Nase −35,16 Grad (hoch), `lenkung_invertieren` und nur `invert_steer` +35,16 Grad (runter).
- Modul `freiheit`: `min_ground_clearance` hat den Bereich -1 bis 2000 und den Standard -1. Bei -1 gilt der Vanilla-Wert 300 Einheiten; Werte von 0 bis 59 werden auf 60 angehoben. In 2.0.0 gab -1 keine Untergrenze (Modus frei: kein Boden).
- Modul `kamera`: Die Kamera ist immer frei. Der Schalter `cam_mode` entfällt; in 2.0.0 gab `cam_mode = 1` die Spielkamera.
- Modul `autowolf`: Das Verhalten ist bis auf die Grenzen aus WP6 (siehe unten) unverändert. Die Testszenarien sind nur im Testbuild übersetzt.
- Konfiguration: `invert_steer` heißt jetzt `lenkung_invertieren`. `invert_steer` bleibt als Altschlüssel lesbar.
- Vorlauf k22-kamera (Modul kamera): Die Kamera weicht Decke und Wänden aus. Die Abstandsrückkehr nach einer Wand ist schneller (kDistGrowRate 26 statt 6). Die Stop-Heilung läuft über einen Stop-Hook; ohne Hook übernimmt das Modul die Heilung. Gemessen: im Lauf kamera ein Stop-Eingriff (stopSkips 1) und sechs Loopings, im Lauf kamerastop0 eine Heilung ohne Hook (stopHeals 1), im Lauf kamerawand ein Ausweichen über 92 Ticks bis 45 Grad und ein kleinster Abstand von 85 Einheiten. In allen drei Läufen throughWall 0.
- Vorlauf e6-decke (Modul freiheit): Der Vogel steigt nur, wenn der Weg nach oben frei ist. Das Ausweichen nach unten unter Decke oder Überhang ist standardmäßig aus (Konfig `decke_abstieg`), weil der Pfad in keinem geprüften Lauf ausgelöst wurde (ausweichStarts 0). Gemessen im Lauf decke6: 2704 Frames, durchdringungen 0, throughWall 0.
- Testbuild: Das Szenarienregister hat 64 statt 32 Plätze (`kMaxScenarios`). Mit 32 Plätzen fielen die Modulregistrierungen aus. Das Release ist nicht betroffen.
- Entladen (Härtung Welle 1): `shutdownBird` beendet den Ritt über denselben Weg wie jedes andere Ende (`endRideClean`, Grund `Shutdown`) und entfernt danach den Vogel. Das gilt auch, wenn das SDK die Mod nach einer Ausnahme in einem Callback entlädt. Gemessen in den Läufen w1-entladen und w1-throw: genau eine Zeile `ride ended clean (Shutdown, Entladen)`, genau eine Zeile `shutdown, bird set to WAIT and removed (…)`, danach `kff: unloaded`. Das Spiel lief weiter.
- Abflug (Härtung Welle 1): Die Frist bis zum Entfernen des Vogels nach dem Absteigen zählt Kern-Ticks (Pad-Lesungen) statt Ausführungen des Vogels. Steht der Vogel still, etwa im Game-Over, wird er trotzdem entfernt.
- Pfadschutz (Härtung Welle 1): Jede Ausführung des eigenen Vogels prüft `mpPath1`, nicht nur Ritt und Abflug (Log: `mpPath1 was null at bird execute, fake path set (count <n>)`). Die Prüfung vor Flug und Taumeln bleibt bestehen.
- Zeitgrenze des Rufs (WP6): 300 Kern-Ticks ohne Pause statt 1800 Link-Frames. Startet der Ritt bis dahin nicht, ist der Vogel nach 300 Ticks entfernt (Log `summon timeout: no ride start after 299 core ticks, bird removed (gone at tick 300)`), danach geht der nächste Ruf. Beginnt vor dem Ritt-Start ein Ereignis, wird der Vogel sofort entfernt (`summon aborted (event)`). Sicherheitsnetz: Hängt die Phase `Transforming` 1800 Kern-Ticks ohne Vogel, wird die Sitzung frei (`transform timeout`). `summonBird` steht jetzt in `src/core/summon.cpp`; die Wirkung des Spawns ist unverändert.
- Schnelles B (WP6, D7): Solange der Landeanflug läuft, der Vogel nach dem Absteigen wegfliegt oder entfernt wird, weist der Ruf mit `departing` ab, und B bleibt beim Spiel. Der Antrag zum Entfernen geht einen Tick früher hinaus, damit der Vogel 100 Ticks nach dem Abflugbeginn weg ist (vorher 101).
- Modul `freiheit`, Decke (WP6): Ein Pre-Hook mit SKIP auf `daKago_c::checkRoofHeight` liefert für den eigenen Vogel einheitlich `kNoRoof` (auch `mRoofHeight`), in allen Aktionen und Modi; das Original läuft nicht. Vorher lief es vollständig, und ein Post überschrieb den Wert; `roof=400` im Log war dessen Ergebnis (Decke 700 minus 300). `checkGroundHeight` bleibt Vanilla.
- Modul `freiheit`, Acch (WP6): Im Ritt läuft `CrrPos` des Vogels nicht (`field_0x6dc` je Ritt-Frame gesetzt, der Wert davor kommt am Ritt-Ende zurück). Bewegung und Kollision kommen allein aus der Abtastung und der Untergrenze; Schatten und Trefferbits führt das Modul an der Endposition nach. Zurückgenommen werden nur Werte und Acch-Bits, die das Modul selbst gesetzt hat (R14).
- Modul `freiheit`, Raum-Neustart (WP6): `daAlink_c::checkRoofRestart` und `daAlink_c::startRestartRoom` lösen im Ritt keinen Neustart aus (Pre mit SKIP), außer bei Tod oder laufendem Ereignis. Nach dem Ritt gilt die Vanilla-Regel.
- Modul `autowolf` (WP6): Die Wächter der Verwandlung und des Vogelrufs greifen nach 600 statt 900 Ticks. Ein Ereignis vor dem Vogelruf oder während der Rückverwandlung bricht ab; die Rückverwandlung bleibt vorgemerkt, ebenso nach dem Wächter der Rückverwandlung. Im Tod beginnt keine Verwandlung. Titel und Laden verwerfen eine vorgemerkte Rückverwandlung (neuer Spielstand). Die Sitzung gibt das Modul nur frei, wenn sie noch seine eigene Verwandlung ist.

#### Entfernt

- Konfigurationsschlüssel ohne Wirkung, nicht mehr registriert: `mode`, `cam_mode`, `event_keep`, `roof_limit`, `far_plane`, `fog_off`, `landing_frames` und `all_rides`.
- Der Modus-Schlüssel entfällt; es gibt nur den freien Modus. Die Modi 1 und 2 waren schon in 2.0.0 nicht umgesetzt.
- Die Deckenbegrenzung entfällt (`roof_limit`). Der Landeanflug hat eine feste Obergrenze von 240 Ticks.
- Ritt-Ende `ManualSave` (Härtung Welle 1): Es hatte keinen Auslöser, denn das Speichern im Ritt ist gesperrt.

#### Behoben

- Absturzschutz: In einer Nachprüfung des Tempo-Prototyps stürzte der Vogel nach dem Absteigen ab (SIGSEGV in `executeFly` und `executeStagger`). Die wahrscheinliche Ursache ist ein leerer Zeiger `mpPath1`, den Flug und Taumeln ungeprüft lesen. Ist der Zeiger leer, setzt das Paket einen Ersatzpfad (`src/core/fakepath.cpp`) und protokolliert das (Log: `mpPath1 was null before flight or stagger, fake path set (count <n>)`).
- Ein Ruf während eines Schadens oder eines Falls wird abgewiesen (Gründe „Link im Schaden“ und „Fall“). Ohne diese Sperre startete der Ritt im Schaden.
- Der Vogel übernimmt beim Start die Blickrichtung des Links (`shape_angle.y`) statt `current.angle`.
- Ist `flug` oder `freiheit` abgeschaltet, wird der Ruf abgewiesen (Grund „Modul flug oder freiheit abgeschaltet“). Fehlt der Kern-Hook `daAlink_c::execute`, bleibt B beim Spiel (Grund „Kern-Hook daAlink_c::execute fehlt“).
- Tod im Abflug (Härtung Welle 1): Stirbt Link, während der Vogel nach dem Absteigen wegfliegt, wird der Vogel sofort entfernt (Grund `Death`). Vorher blieb er im Game-Over stehen, weil seine Ausführung dort ruht. Gemessen im Lauf w1-tod-abstieg: Vogel spätestens 3 Ticks nach dem Tod weg (Grenze 150). Im Lauf w1-spawn-tod (Tod direkt nach dem Ruf) ebenfalls nach 3 Ticks. Im Lauf w1-game-over erschien in 460 Game-Over-Ticks kein Vogel; die Rufe im Game-Over wurden mit dem Grund „Ereignis läuft“ (seit WP4 `event`) abgewiesen. Nach dem Neustart lief ein normaler Ritt.
- Verschwundener Vogel (Härtung Welle 1): Ist der Vogel weg, ohne über den eigenen Entfernen-Pfad gegangen zu sein, löst der Kern Link und Midna trotzdem (Log: `bird gone: Link and Midna released without the bird`, Zähler `CORE5 goneRelease`). Vorher konnten Cargo-Zustand und Midna-Zeiger stehen bleiben.
- Ruf unter einer Decke (WP6.3): Vorher prüfte der Ruf den Platz über Link nicht. Jetzt weist er unter einer niedrigen Decke mit `room` ab, ohne Vogel (Lauf wp6p2-ruf-1: 5 Versuche, 5 Ablehnungen, Vögel 0).
- Hängen an einer Wand (WP6.5): Vorher gab es keinen Wächter (`Stuck` war reserviert); ohne Stick-Eingabe blieb der Ritt an einer Wand bestehen. Jetzt endet er nach 300 Ticks mit `Stuck` (Läufe wp6p2-wand-1, wp6p3-wand-2, wp6p3-wand-3 und wp6p6-wand-4).

#### Bekannte Einschränkungen

- Stillstand unter Decke oder Überhang: Im Lauf innen-r5 blieb der Vogel bis zu 148 Ticks stehen; das Ziel lag bei höchstens 30. Die Untergrenze stoppte das Sinken bei 380 Einheiten. Der Abstiegspfad wurde in den geprüften Läufen nie ausgelöst (ausweichStarts 0) und ist standardmäßig aus (`decke_abstieg`). Messung WP6 (E10): höchstens 53 Ticks (wp6p2-decke-1) und 48 Ticks (wp6p3-decke6-std und wp6p3-decke6-ab, je 7 Stillstände); nach einem Stickwechsel kam der Vogel nach 46, 8 und 21 Ticks frei (Ziel höchstens 5). Mit `decke_abstieg = true` startete der Abstieg nicht (42-mal `kein Ausweg nach unten`, ausweichStarts 0); der Pfad bleibt aus. An der senkrechten Bergwand hing der Vogel ruhig: In den Wandläufen war die Position bei den Proben nach 0, 100 und 200 Ticks dieselbe, bis der Wächter den Ritt beendete. Eine Ecke aus zwei Wänden ist nicht gesondert gemessen. Die Füße liegen durch die Untergrenze mindestens 50 Einheiten über der höchsten Fläche (linkUnterGrund 0); eine Messung im Abstieg gibt es nicht, weil er nie startete.
- Gegnertreffer im Ritt sind nicht gezielt geprüft (Gegner E_YR, Profil 506). Beobachtet im Lauf w1-zyklus: ein Pfeiltreffer im Ritt, der Ritt lief bis zur Landung weiter (griff: lostFrames 0). Ob und wann Gegner treffen, schwankt von Lauf zu Lauf. Eine Landung auf schädlichem Boden ist möglich: Im Lauf zyklus-r4 gab es dabei einen Schadenspunkt.
- Andere Mods mit Pad-Eingabe: Der Kern liest das Pad im Post-Hook von `mDoCPd_c::read` mit Priorität 1000. Der Testtreiber `dev.local.kff_szenario_treiber` 0.2.0 speist seine Tasten mit derselben Priorität ein; im Lauf w1-treiber sah der Kern dessen Ruf nicht (chord 0, kein Ritt im Treiber-Szenario `tod`). Wahrscheinliche Ursache ist die Reihenfolge gleich priorisierter Post-Hooks; der Treiber war für Version 1 (Priorität 0) geschrieben. Der Zustand bestand schon vor der Härtung Welle 1.
- Der Landeanflug prüft die Reichweite nicht. Er bremst die Vorwärtsbewegung auf null (Vorlauf e9-landung, Nachmessung Härtung WP8). Steigt das Gelände unter dem Vogel steil an, ist die Freigabehöhe schon nach wenigen Frames erreicht, und Link wird frei, bevor der Vogel gebremst hat (Lauf stick-relativ: Freigabe nach 2 Frames bei Vogeltempo 49,9; Link rutschte an einer steilen Wand ab, ohne Schaden).
- Speichern: Die Hinweismeldung „Erst absteigen (B), dann speichern“ ist nicht belegt. Ohne vorhandene Kartendatei zeigt das Spiel seine eigene Meldung (Unable to save) und schreibt nichts. Der Ja-Pfad zum Anlegen einer Karte ist nicht getestet; ob ein Abbruch das Speichermenü offen lässt, ist ungeprüft (Vermutung aus dem Code).
- Ein Ruf während eines Kartenschreibens ist über `cardWriteBusy` gesperrt; das ist im Test nicht gemessen. Ein Autosave im Ritt wird nicht verweigert (E11): im Code belegt, im Spiel praktisch nicht auslösbar und im Test nicht ausgelöst.
- Pause, Karte, Dokument und Dialog: Im Pausenmenü schließt B das Menü, und der Ritt läuft weiter (Läufe e5-ritt und eingabe-konsum). Karte, Item-Rad, Dokument und NPC-Dialog sind nicht gemessen.
- Kamera-Stopp (K22): Abfang und Stop-Heilung sind nur mit einem im Test ausgelösten `dCamera_c::Stop` im Ritt belegt (Härtung WP8: Abfang in drei Läufen kamera, die Kamera lief ohne Pause weiter; mit `KFF_KAMERA_STOPHOOK=0` Heilung nach 29 Ticks). Ein natürlicher Auslöser im Ritt (etwa Elektroschaden) ist auf den Test-Stages nicht erreichbar. Ballonspiel und Schadenskamera außerhalb des Rittes lässt der Abfang laut Code unverändert durch; im Spiel ist das nicht geprüft. Der Abfang bleibt, weil keine Regression beobachtet wurde.
- Mensch: Der Krallen-Roll-Fix fehlt (Abstand der Krallen bis −16,4 in Kurven). Die Kamera springt beim Wandkontakt am Rittbeginn auf Abstand 0 und braucht rund 4,6 Sekunden zur Erholung; die Ursache ist nicht geprüft. Einzelne Abbruchfälle sind nicht portiert.
- Abgrund: Steigt Link über einer Stelle ohne Boden ab, fällt er wie nach einem Sprung, und der Void-Out des Spiels greift wie ohne Ritt (Z1, Lauf w2-f-void1: 86 Ticks nach der Freigabe statt vorher 670, ein Herz Abzug wie im Vergleich ohne Ritt). Mit wenig Leben endet das wie im Spiel mit Game Over (Entscheidung E12, kein Neustart statt Game Over). Gemessen ist das nur außerhalb der Karte, nach einem Test-Teleport über den Abgrund. Im ersten Frame nach der Freigabe versetzt die Wurzelbewegung der Fall-Animation des Spiels Link um rund 64 Einheiten (`posMove`, Proc 241 nach 253); das kommt nicht von der Mod und wird nicht geglättet.
- Kartenrand (WP6.6/6.7): Über die Randberge von F_SP121 hinaus fliegt der Vogel weiter, gemessen bis x = −289181 und bis über 100000 Höhe, ohne Szenenwechsel und ohne Halt (wp6p2-rand-1). Dort liegt kein Boden. Im Ritt verhindert die Mod den Void-Out; B gibt Link frei, er fällt, und das Spiel setzt ihn mit einem Herz Abzug am Neustartpunkt des Raums ab (Vanilla). Der Halt bei x ≈ −81410 aus dem Vorlauf ist die Bergflanke selbst (normales Gelände, Wandcode 0): In niedriger Höhe stoppt sie den Vogel wie jede Fläche, über dem Grat gibt es keinen Halt.
- Stillstands-Wächter (WP6.5): Endet der Ritt mit `Stuck`, fällt Link aus der Höhe des Vogels. In den Wandläufen (rund 300 Einheiten über dem Boden) gab es keinen Schaden; aus größerer Höhe ist Fallschaden möglich. Der Wächter greift nur ohne Stick-Eingabe. Die Vorgabe „der Vogel lässt nur über B los“ gilt damit für den Ritt mit Stick-Eingabe; ein Ritt ohne Eingabe an einer Wand endet nach 300 Ticks (Vorgabe WP6, Aufgabe 5). Der Hinweis im Spiel ist im Bild belegt (Lauf wp6p6-wand-4).
- Platzprüfung beim Ruf: nur senkrecht über Link. Seitliche Wände prüft sie nicht; daneben greift erst die Abtastung im Ritt.
- Ruf-Gates „Klettern“ und Bodenschaden (`PROC_POLY_DAMAGE`) sind im Spiel nicht ausgelöst: Der Wolf klettert nicht, und in den geprüften Stages gab es keinen Schadensboden. Die Bedingung für `PROC_POLY_DAMAGE` steht in derselben Abfrage wie `PROC_WOLF_DAMAGE` (0x107, geprüft). Die Positivliste der Link-Procs aus WP5 (Modul griff, Grund `climb` für Klettern, Ranken und Leiter) ist seit der Zusammenführung Welle 2 enthalten; im Spiel ausgelöst ist auch sie nicht.
- Nur im Code geprüft, im Spiel nicht ausgelöst (WP6): `summon aborted (event)` und `transform timeout` (Zählerzeile `CORE6`: summonAborted 0 und transformTimeout 0 in allen Läufen), „eigener Vogel ohne Sitzung“ (strayFound 0) und die Gates in `summonBird` selbst (spawnRejected 0; die Ruf-Sperre griff jeweils vorher). Ebenso die WP6-Grenzen von autowolf (Abbruch bei Ereignis, Wächter nach 600 Ticks, Verwerfen bei Titel und Laden) und die Rückverwandlung nach dem Tod. Der Normalablauf von autowolf lief im Lauf wp6p3-ordon: Ruf als Mensch, Verwandlung mit abgewiesenem Probe-Ruf („Verwandlung laeuft“), Ritt, B, Rückverwandlung.
- Flug: Im Lauf final-geradeaus-r1 (60 Sekunden neutraler Stick über F_SP121) sank der Vogel nach etwa 20 Sekunden in einer Wandecke um 444 Einheiten (Grenze 50). Laut Vorbefund kommt das vom Gleiten an einer schrägen Wand; das ist nicht isoliert. Auf freier Strecke (Startpunkt per Suche, Szenario geradeaus-b) lag die Abweichung in drei Läufen der Härtung WP8 bei 0,000 Einheiten und 0,0000 Grad in 1800 Ticks.
- Geschwindigkeit in km/h ist eine Annahme (1 Einheit = 1 cm, 30 Ticks je Sekunde) und nicht gemessen.
- Tastatur- und Controller-Belegung sind im Spiel nicht geprüft. Die Belegungsdatei `out/keyboard_bindings.dat` im Ordner `tastatur-belegung` stimmt Taste für Taste mit der Tabelle im README überein (Härtung WP8).
- Getestet wurden nur die Stages F_SP121, R_SP107, F_SP103, F_SP115 (WP6, Ruf im Wasser) und F_SP124 (WP4, Startdialog).

#### Prüfung Härtung Welle 1

Release (`KFF_DEV_AUTOTEST=OFF`) und Testbuild (`ON`) wurden aus demselben Quellstand gebaut, ohne Warnungen aus `src/`. Jeder Lauf hatte einen eigenen Benutzerordner. Alle Läufe endeten mit Rückgabe 0, ohne Absturzbericht und ohne Ausnahme im Mod (außer den absichtlichen Testausnahmen).

- Regression gegen Referenzläufe vor der Welle: `zyklus` (F_SP121: zwei Ritte, zwei Landungen, zwei entfernte Vögel, Kamerawerte gleich), `innen` (R_SP107: Kamerazähler gleich), `mensch` (F_SP103 mit `mensch_ritt = true`: Werte gleich) und `e9-klippe` (Freigabe 975,6 Einheiten vor der Kante, wie vorher). Der zweite Ritt in `zyklus` startete an einer anderen Stelle als in der Referenz, weil Link vorher am Boden von einem Pfeil getroffen wurde. Dasselbe Muster zeigen ältere Läufe.
- Ein Szenario `crash` gibt es im Testbuild nicht. Ersatz waren `wp2-b701` (B-Tipp bei Ritt-Frame 701, Absteigen und Entfernen) und `e9-klippe`. Beide liefen ohne Absturz.
- Tod und Game Over: `wp2-tod-abstieg`, `wp2-game-over` und `wp2-spawn-tod` (Werte unter „Behoben“).
- Titel und Laden: `politik-titel` mit einer Spielstand-Kopie. Ein Soft-Reset im Ritt beendete den Ritt mit `Reset`, der Vogel wurde entfernt. Titel, Dateiauswahl und Laden liefen ohne Vogel und ohne Cargo-Zustand.
- Speichern: `e1-speichern` mit einer Spielstand-Kopie. Das Speichern im Ritt wurde abgewiesen, die Kartendatei blieb unverändert (SHA-256), und der Ritt lief weiter.
- Lebenszyklus: Entladen im Ritt (`KFF_TEST_UNLOAD=1`) und eine Ausnahme im Modul freiheit (`KFF_TEST_THROW=freiheit`) beendeten den Ritt genau einmal mit `Shutdown`. Eine fehlgeschlagene Registrierung (`KFF_TEST_BADNAME`) schaltete nur das Modul freiheit ab; danach wurde der Ruf abgewiesen. Ein zweiter Replace (`KFF_TEST_CONFLICT`) wurde mit `MOD_CONFLICT` abgewiesen, der Ritt lief. Eine zweite Paketdatei mit derselben Mod-ID (Version 1.0.0) wurde vom Loader verdeckt; die Mod startete genau einmal.
- Release-Paket: Die Startzeile lautet `initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`, auch mit gesetztem `KFF_AUTOTEST`. Es gab keine Autotest-Zeilen, alle sieben Module waren installiert, der Startbericht meldete `callbacks=46`.

#### Härtung Welle 2: Ereignisse im Ritt (WP4)

Ereignisse, Dialoge, Skip, Midna, Story-Flags, Verwandlung und Heulen im Ritt. Das Modul `griff` registriert dafür 15 Hooks, nur Pre und Post mit Priorität 350 und ohne Replace; jeder Hook fängt seine Ausnahmen selbst. Im Kern kommen die Skip-Hygiene beim Entfernen eines Vogels und eine Ereignis-Sperre zwischen Ruf und Aufsitzen dazu. Der Startbericht meldet im Release `callbacks=61 replaces=2 modules=7 disabled=0 conflicts=0` (Härtung Welle 1: 46 Callbacks), im Testbuild 65.

- Skip-Hygiene (Befund P0, `birdDeleteHygiene` in `src/core/ride.cpp`): Zeigt der Skip-Rückruf des Ereignissystems auf einen Vogel, der entfernt wird, leert der Kern vorher das Skip-System (Log: `skip cleared on bird delete (<Stelle>, id=…)`, Zähler `CORE5 skipCleared`). Vorher fiel der Skipper nach dem Entfernen auf Link zurück und rief `daKago_c::DemoSkipCallBack` mit Link als Vogel auf. Ist der eigene Vogel an einem laufenden Ereignis beteiligt, endet das Ereignis mit ihm, und die Kamera startet wieder (Log: `event reset on bird delete (…)`, Zähler `CORE5 eventReset`). Ein schon angefordertes Ende wird nicht wiederholt (Log-Zusatz `already requested`). Die Prüfung läuft in `requestDelete`, beim Entfernen verwaister Vögel und als letztes Netz im Pre von `daKago_c::_delete`. Sie vergleicht nur Prozess-IDs.
- Demo-Skip (Befund P0): `daKago_c::demo_skip` läuft am eigenen Vogel und an Mod-Vögeln nicht mehr (Log: `demo_skip blocked (…)`). Die Fälle 7 und 9 riefen vorher `setRideOff` und setzten Link ohne Ritt-Ende ab. Ohne lebenden Kargarok, etwa wenn der Skipper Link übergibt, läuft das Original ebenfalls nicht, und das Ereignis endet (Log: `demo_skip without a live Kargarok (…)`). Vanilla-Vögel behalten ihren Skip.
- Skript-Aufträge am Vogel: Ein Ereignis- oder Perch-Auftrag am eigenen oder an einem Mod-Vogel wird vor der Ausführung abgefangen. Im Ritt fliegt der Vogel weiter, im Abflug fliegt er weg, ein verwaister Mod-Vogel wartet (Log: `griff: script action … intercepted (…)`). Ein Ereignis, an dem der Vogel beteiligt ist, endet dabei (Log: `griff: event of bird … ended (script intercepted), camera started`).
- Perch ohne Pfad: `daKago_c::executePerch` und `daKago_c::initPerchDemo` laufen nicht, wenn `mpPath2` fehlt oder der Vogel ein eigener oder Mod-Vogel ist (Log: `perch ignored: no path (…)` oder `perch ignored: own bird (…)`). Vorher stürzte das Spiel ab (Vergleichslauf: SIGSEGV in `daKago_c::initPerchDemo`).
- Ereignis zwischen Ruf und Aufsitzen (T3, D6): Beginnt in der Phase `Summoning` ein Ereignis, endet die Sitzung geordnet, und der gerufene Vogel wird entfernt (Log: `ride ended clean (Event, Ereignis vor dem Ritt-Start)`, Zähler `CORE5 summonEventEnd`). `kagoExecutePre` startet während eines Ereignisses keinen Ritt. Vorher saß Link im Vergleichslauf während des Ereignisses auf, und der Ritt endete im ersten Ritt-Frame mit `Event`.
- Midna-Aufträge im Ritt (Befund P2): `fopAcM_orderPotentialEvent` und `fopAcM_orderTalkEvent` mit Midna werden im Ritt mit Rückgabe 0 übersprungen (Log: `midna order refused (…)`, Zähler `midnaOrderRefused`). Den Demo-Modus, den der Midna-Ablauf danach trotzdem an Link und Midna setzt, nimmt das Modul im nächsten Pad-Tick zurück (Log: `midna demo reset (order refused)`). Vorher lief der Auftrag, und Link stand im Demo-Typ 3.
- Midna-Wartepunkt: `daTagMwait_c::execute` ist im Ritt ausgesetzt, außer ein Gespräch des Tags läuft schon (Log: `griff: tag_mwait paused during ride (…)`). Vorher bot der Tag im Ritt ein Gespräch an (CANTALK und TALK), und Midna stand am Wartepunkt.
- Story-Flags: `dComIfGs_onEventBit` und `dComIfGs_offEventBit` ändern M_067 und M_077 im Ritt nicht (Log: `story flag blocked: <Flag> (on|off)`). Der Spielstand-Editor von Dusklight (F6) umgeht die Sperre (README, „Bekannte Grenzen“).
- Verwandlung über den Spielweg: Der Pre auf `daAlink_c::procCoMetamorphoseInit` beendet den Ritt mit `Transform`, bevor die Verwandlung beginnt, solange ein eigener Vogel existiert (Log: `griff: transformation via game path with own bird (…)`, danach `ride ended clean (Transform, Verwandlung (Spielweg))`). Die Verwandlung des Moduls `autowolf` ist ausgenommen. Ein Ruf während der Verwandlung wird abgewiesen (`call rejected (transforming)`), auch ohne Phase `Transforming`. Vorher blieb der Vogel im Vergleichslauf 79 Ticks, und der Ritt endete ohne `Transform`.
- Ruf-Sperre: Die Grund-Texte heißen `transforming` (vorher „Verwandlung laeuft“) und `event` (vorher „Ereignis laeuft“ und „Link im Ereignis“).
- Sensoren ohne Eingriff, Zähler in der Zeile `griff: SUMMARY WP4`: Sprech-Aufträge im Ritt, Gesprächsangebote von Tags, Skript-Aktion am eigenen Vogel im Ritt (`setEventInRide`) und Heul-Proc im Ritt (`howlInRide`).
- Testbuild: Szenario `wp4` mit zwölf Teiltests über `KFF_WP4=<Teiltest>` und dem Vergleichslauf `KFF_WP4_AUS=1` ohne die WP4-Absicherungen (DEVELOPMENT.md, Abschnitt 14.2). Im Release nicht übersetzt.

#### Prüfung Härtung Welle 2 (WP4)

Testbuild und Release wurden aus demselben Quellstand gebaut, ohne Warnungen aus `src/`. Jeder Lauf hatte einen eigenen Benutzerordner. Alle zu Ende gelaufenen Läufe meldeten `fremdeingabe=0` und endeten mit Rückgabe 0, ohne Absturzbericht. Nur der Vergleichslauf `perch` stürzte ab (SIGSEGV), wie erwartet. Ausgelöst wurde über Pad-Eingaben der Mod. Auslöser, die im Ritt mit Spieleingaben nicht erreichbar sind, stellt der Testbuild nach (Log `TEST`). Die Vergleichsläufe mit `KFF_WP4_AUS=1` zeigen den Fehler ohne die Absicherung. Die Werte der Prüfläufe stammen aus dem letzten Testpaket, die der Vergleichsläufe zum Teil aus früheren Testpaketen mit demselben Vergleichsschalter.

- `event` (Sprech-Aufträge am eigenen Vogel, TEST): Ritt-Ende `Event`, das Ereignis endete nach 2 Ticks, kein Vogel übrig, Link frei. `event reset on bird delete` setzte das Ereignis genau einmal zurück, die zweite Stelle meldete `already requested`. Vergleich: Das Ereignis lief weiter, Link war nicht frei.
- `skip` (Skip-Rückruf auf den eigenen Vogel, danach Entfernen, TEST): `skip cleared on bird delete (_delete, id=0x121)` genau einmal, danach kein Aufruf von `demo_skip`, weder mit dem Vogelzeiger noch ohne Vogel. Vergleich: ein `demo_skip` ohne Vogel (Rückfall auf Link).
- `linkdemo` (Pflicht-Ereignis und Original-Demo an Link im Ritt, TEST): `overrideFrames=0`, Ritt-Ende `Event`, Link frei.
- `demoskip` (`DemoSkipCallBack` mit den Fällen 7 und 9 im Ritt, TEST): Der Ritt lief bis B, `setRideOff`-Wirkung 0, Teleport 0, Ticks ohne Cargo 0, zwei Skips blockiert. Vergleich: `setRideOff`-Wirkung 2.
- `perch` (Heul-Wirkung auf den eigenen Vogel, einen verwaisten Mod-Vogel und einen Vanilla-Vogel ohne Pfad, TEST; der verwaiste Mod-Vogel ersetzt das Neuladen der Mod): `perch ignored` 3, `setEvent` im Ritt 0, Heul-Proc im Ritt 0, R+X 8-mal ohne durchgelassenes X. Vergleich: SIGSEGV in `daKago_c::initPerchDemo`.
- `midna` (Midna-Auftrag mit event_id 4 im Ritt, TEST): `midnaOrderRefused=2`, beide Rückgaben 0, Demo-Typ von Link und Midna 0, der Ritt lief bis B weiter. Vergleich: Aufträge angenommen, Link im Demo-Typ 3.
- `flags` (M_067 und M_077 im Ritt umschalten, TEST): `story flag blocked` 6-mal, keine Änderung im Ritt, außerhalb des Ritts wirksam, Endwerte unverändert. Vergleich: 4 Änderungen im Ritt.
- `verwandlung` (Schnellverwandlung von Dusklight mit `enableQuickTransform` und `canTransformAnywhere`; F_SP103 aus Menschengestalt, F_SP121 aus Wolfsgestalt): Ritt-Ende `Transform`, Vögel nach 1 Tick weg, Verwandlung nach 49 Ticks fertig, `call rejected (transforming)`, R+Y im Ritt ohne Verwandlung. Die Verwandlung begann im Abflug nach B. Vergleich: kein `Transform`, der Vogel blieb 79 Ticks.
- `startdialog` (F_SP124, Startdialog): `call rejected (event)`, kein Tick mit Vogel und Ereignis, Phase am Ende `Idle`.
- `summonevent` (Pflicht-Ereignis an Link in der Phase `Summoning`, TEST): Vogel nach 2 Ticks weg, kein Tick mit Vogel und Ereignis, kein Ritt-Start. Vergleich: Ritt-Start während des Ereignisses.
- `tags` (Midna-Wartepunkt in Gesprächsreichweite, TEST): Gesprächsangebote im Ritt 0, Midna am Wartepunkt 0. Vergleich: je 300 Ticks.
- `howl` (R+X mit dem Vogel als Heul-Ziel): R+X 12-mal, X 12-mal verbraucht und nie durchgelassen, kein `setEvent` am Vogel, kein Heul-Proc.
- Release: Die Startzeile lautet `initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`, auch mit gesetztem `KFF_AUTOTEST=wp4`. Es gab keine Autotest-Zeilen. Der Startbericht meldete `callbacks=61 replaces=2 modules=7 disabled=0 conflicts=0`. Das Paket enthält weder `getenv` noch die Zeichenfolgen `KFF_WP4`, `autotest` oder `Vergleichslauf`.

#### Prüfung WP6

Testbuild und Release wurden mit eigenen Build-Ordnern gebaut, ohne Warnungen aus `src/`. Der Produktionscode war in allen Testbuilds der Läufe unten gleich; zwischen ihnen änderten sich nur Testszenarien. Jeder Lauf hatte einen eigenen Benutzerordner. Alle Läufe endeten mit Rückgabe 0, ohne Absturzbericht, ohne Ausnahme im Mod und ohne Fremdeingabe. Vor den Änderungen liefen Referenzläufe mit dem Stand davor (`wp6-base-innen`, `wp6-base-rufspam`, `wp6-base-wand5`).

- WP6.1, 6.2 und 6.4 (`wp6p2-decke-1`, R_SP107, Zelle mit Decke bei y 700): Der Vogel lag schon im ersten Ritt-Frame bei y 550 (Vanilla-Grenze 400 plus 150), höchstens bei 619,9. CrrPos lief im Ritt nie (crrPosRuns 0). Beim Dash unter der Decke lagen 120 von 120 Frames bei y 608 (Band ±50), ohne Durchdringung. `startRestartRoom` wurde im Ritt nicht aufgerufen; `checkRoofRestart` wurde im Ritt 276-mal abgewiesen und lief nach dem Ritt wieder (226 Aufrufe).
- WP6.3 und 6.11 (`wp6p2-ruf-1`): unter einer niedrigen Decke 5 Rufe, 5-mal `call rejected (room)`, Vögel 0. Ruf-Spam im Ritt: abgewiesen mit `eigener Vogel aktiv` und `departing`, höchstens ein Vogel. Im Wasser (`wp6p4-wasser`, F_SP115, Link als Mensch schwimmend, Proc 0x7A): 5 Rufe, 5-mal `call rejected (Wasser)`, Vögel 0, keine Verwandlung.
- WP6.5 (`wp6p2-wand-1`, `wp6p3-wand-2`, `wp6p3-wand-3`, dazu `wp6p6-wand-4` mit dem letzten Testbuild, F_SP121): Der Vogel hing mit neutralem Stick an der Bergwand bei x −76440. Nach 300 Ticks `watchdog: stuck 300 ticks`, Ritt-Ende `Stuck`, Link fiel rund 300 Einheiten ohne Schaden. Der Hinweis im Spiel ist im Bild zu sehen (wp6p6-wand-4). Durchdringungen 0.
- WP6.6 und 6.8 (`wp6p2-rand-1`): Kartenrand nach −X in Gang 5 mit Dash bis x = −289181, danach Steigen auf über 100000 Höhe. Kein Szenenwechsel, kein Halt, Decke im Ritt immer `kNoRoof`.
- WP6.7 (in den Wandläufen): Tunnel-Probe mit 300 und 600 Einheiten Zusatzweg je Tick gegen den Fels: kein Durchflug (Durchdringungen 0, durchflugKorr 0, skipFrames 0). Der Halt bei x ≈ −81410 ist die Bergflanke (normales Gelände, Wandcode 0, Raum 10).
- WP6.9 und 6.10 (`wp6p4-timeout`, `KFF_TEST_NORIDE=1`): Der Vogel ohne Ritt-Start war nach 300 Ticks weg, der nächste Ruf startete einen Ritt. B bei Ritt-Frame 10, R+B 50 Ticks später: `call rejected (departing), B stays`. Der erste Vogel war 100 Ticks nach dem Abflugbeginn weg, höchstens ein Vogel; der Ruf danach wurde angenommen.
- WP6.12 (`wp6p3-gate-f` in F_SP121, `wp6p3-gate-r` in R_SP107, `wp6p5-doc`, `wp6p4-wasser`, `wp6p3-ordon`, `wp6p3-ordonev`): abgewiesen mit Grund im Log und ohne Vogel wurden Ereignis (5-mal), Startflanke, Pause, Sprung und Sturz (als `Fall`, Proc 0xFD), Zielsperre, Schaden durch Gegner (Proc 0x107, 3-mal), Reiten (`eigener Vogel aktiv`), Wasser, sichtbares Dusklight-Dokument (Dialog über den UiService, 3-mal; nach dem Schließen startete der Ruf einen Ritt) und laufende Verwandlung. Der R-Status war bei allen Versuchen 0, B blieb beim Spiel. Klettern und Bodenschaden wurden nicht ausgelöst (siehe Einschränkungen).
- E10 (`wp6p2-decke-1`, `wp6p3-decke6-std`, `wp6p3-decke6-ab`): Werte unter „Bekannte Einschränkungen“, Stillstand unter Decke.
- Release (`wp6p3-release-smoke`): `initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`, `registry check: callbacks=50 replaces=2 modules=7 disabled=0 conflicts=0`.

#### Härtung Welle 2: Szenenwechsel im Ritt (WP1)

- Positionsgetriebene Szenenwechsel sperrt das Modul `freiheit` im Ritt an der Quelle, bevor das Spiel ein Pflichtereignis bestellt (Pre mit SKIP): `daAlink_c::checkSceneChange` (Ladezone, Ausgangs-Polygon, Nebel), `checkRestartRoom`, `checkRoofRestart`, `startRestartRoom` ohne Ereignis, `daTagRetRm_c::execute` (Rückkehr-Raum), die Wechsel des eigenen Vogels (`setSceneChange`, `checkFlySceneChange`), `dStage_changeScene` ohne Ereignis und ein anstehender Wechsel ohne Ereignis. Log je Episode: `freiheit: positional scene veto (<Aufrufer>) ...`. Vorher (Befund b-ladezone1) griff die Sperre erst in `dStage_changeScene`. Das von `checkSceneChange` bestellte Pflichtereignis blieb dann hängen: Der Ritt endete als Ereignis, danach stand Link 900 Ticks starr, und die Szene wechselte nicht.
- Ereignis-, Warp-, Menü- und Ladewechsel sperrt die Mod nicht. Der Ritt endet vorher (`endRideClean`), dann läuft der Wechsel. `freiheit` beendet den Ritt selbst bei Warp-Status (Karte oder Ooccoo), bei `dStage_changeScene` mit Pflichtereignis, bei `dComIfGp_setNextStage` mit 4 Argumenten (Menü, Konsole, Warp-Menü), bei `dStage_changeScene4Event` und bei `startRestartRoom` im Ereignis. Der erste Stage-Wechsel nach einem Ritt-Ende bekommt die Zeile `freiheit: scene change allowed after end (<Grund>) ...`. Ein laufendes Ereignis wird nie gesperrt.
- Testbuild: die Szenarien `ladezone-anflug`, `event39-kanal`, `boden-absteigen`, `void-absteigen`, `abgrund-faelle`, `warp-portal`, `raumwechsel` und `rueckkehr-raum-tag` (Ergebniszeile `wp1: RESULT <Szenario> ok|fail ...`) mit den Schaltern `KFF_WP1_FALL`, `KFF_WP1_PORTAL`, `KFS_VOID_DIST` und `KFS_OHNE_RITT` (Vergleich zu Fuß ohne Ritt). Im Release nicht übersetzt.

#### Härtung Welle 2: Eingabe (WP3)

- Die Eingabe des Kerns steht jetzt in `src/core/input.cpp` (vorher `ride.cpp`): Pad-Lesung im Post-Hook von `mDoCPd_c::read`, Schnappschuss für die Module, Ruf-Kombination R+B, Konsum der Ritt-Tasten und UI-Block.
- Ablehnungen (F02): Jede Ablehnung hat einen Grund. Die Zeilen `call rejected (<Grund>), B stays (n=<Anzahl>)` und `chord dropped (<Grund>), B returned to the game (n=<Anzahl>)` sind je Grund gedrosselt: Die erste Zeile kommt sofort, danach höchstens eine je 90 Pad-Ticks. `n` zählt die Ablehnungen seit der letzten Zeile dieses Grundes. Die Zusammenfassung `INPUT` gleicht die Summen ab.
- Kein Latch (D8): Ruf und Absteigen gelten nur im Tick ihrer Entstehung. B eines nicht ausgeführten Rufs geht an das Spiel zurück.
- Sperrflags (RK (a)): Im Ritt sieht das Spiel weder Analogwerte noch Sperrflags von A, B, L und R, auch `mHoldLockR` nicht. Eine falsche R-Flanke nach dem Ritt (P2-04) verhindert `repairLocks` mit einem Schattenzustand der Sperrflags.
- UI-Block (E5): Bei Pause, Menü, Karte, Item-Rad, Dusklight-Dokument, Dialog oder Ereignis verbraucht der Kern keine Tasten, und B bedient das Menü. Ist im Ritt ein Dusklight-Dokument sichtbar, steht der Vogel und hält die Höhe (Log `flug: ui_halt start` und `ui_halt ende`), danach beschleunigt er von 0.
- Testbuild: die Szenarien `wp3-karte-ritt`, `wp3-ui-halt`, `wp3-chord-b`, `wp3-dialog-b`, `wp3-schild`, `wp3-schild-wolf` und `wp3-r-analog` (`src/core/input_test.cpp`, Ergebniszeile `wp3: ERGEBNIS <Szenario> ok|fail ...`), Schalter `KFF_TEST_OHNE_AUTOWOLF`. Im Release nicht übersetzt.

#### Härtung Welle 2: Zustände im Ritt (WP5)

- Modul `griff`, Zustands-Whitelist im Ritt (Pre mit Priorität 350, nur Pre-SKIP, kein Replace): `daKago_c::executeStagger`, `executeLanding`, `executeLandingLakeHairia` und `executeLandingBoartHouse` laufen im Ritt nicht. Der Vogel bleibt im Flug, gelandet wird nur über den Landeanflug des Kerns. `daAlink_c::checkSwimAction` liefert im Ritt 0 (kein Schwimm-Proc). In `checkDamageAction` laufen Wurf-, Groß-, Sandwand-, Quetsch- und Rückstoßschaden ohne Proc-Wechsel ab, der Lebensverlust bleibt. `procWolfDamageInit` läuft im Wolfs-Ritt nicht. `procCoLavaReturnInit` und `daTagKagoFall_c::execute` (Fluss- und Fall-Tag in F_SP112) laufen im Ritt nicht, außer Link ist tot oder ein Ereignis läuft.
- Ruf-Gate (Positivliste der Link-Zustände, `griffCallGateReason`, aufgerufen aus `callBlockReason`): abgewiesen mit den Gründen `riding` (Epona, Wildschwein, Boot, Board, Spinner, Ziege, Gorone), `transform`, `water`, `rope`, `chain`, `hang`, `climb` (Leiter, Kletterwand, Ranken), `hookshot`, `damage` und `air`. B bleibt beim Spiel.
- Landeanflug über Wasser (K17): Der Anflug endet, sobald Links Füße 30 Einheiten über dem Wasser sind. Link fällt ins Wasser und schwimmt, statt bis zur Zeitgrenze an der Oberfläche zu schweben.
- Lässt der Vogel Link los (Stagger, Landung oder Warten ohne Griff, nur ohne diesen Schutz denkbar), endet der Ritt sofort geordnet.
- Modul `mensch`: Mit `mensch_ritt = false` (Standard) meldet das Modul den Ruf als Mensch im Log (`mensch: call rejected (human)`), danach entscheidet `autowolf` wie bisher. Sonst ändert sich nichts.
- Testbuild: Szenario `wp5` mit dem Fall über `KFF_WP5` (`wasser`, `seil`, `reittiere`, `treffer`, `schaden`, `wand`, `wandabwurf`, `stagger`, `lava`, `fluss`, `szenentyp`, `pfeil`, `eym`, `mensch`) und den Schaltern `KFF_WP5_OHNE`, `KFF_TEST_NOGUARD`, `KFF_GRIFF_TESTHIT`, `KFF_TEST_STAGGER` und `KFF_TEST_FLAG6DF`. Im Release nicht übersetzt.

#### Härtung Welle 2: Zusammenführung und Kernaufträge Z1

- Hook-Besitz: Je Ziel gibt es höchstens einen SKIP-Besitzer. Den Raum-Neustart im Ritt (`daAlink_c::startRestartRoom`, `daTagRetRm_c::execute`) besitzt allein das Modul `freiheit` (WP1 und WP6). Die gleichlautenden Pre-Hooks aus WP5 im Modul `griff` sind nicht übernommen. Die Prüfung der Hüllen meldete in allen Läufen `conflicts=0`.
- Ruf-Abbruch durch ein Ereignis: Der Link-Frame des Kerns (WP4) und die Zeitgrenzen-Prüfung aus WP6 (`summon::tick`) beenden die Sitzung mit demselben Aufruf (`endRideClean`, Grund `Event`, Anlass „Ereignis vor dem Ritt-Start“). Wer das Ereignis zuerst sieht, beendet. Danach gibt es keine Phase `Summoning` mehr, und der andere Pfad tut nichts.
- Absteigen über einem Abgrund (Z1, OE1): Der Kern hält die Sturzreferenz (`mFallHeight`, `field_0x33c8`) nach der Freigabe nur noch, wenn unter Link Boden liegt (`groundBelowLink`, Bodensonde ab 500 Einheiten über Link). Ohne Boden fällt Link wie nach einem Sprung von der Loslasshöhe, und der Void-Out des Spiels greift. Lauf w2-f-void1: Void-Out 86 Ticks nach der Freigabe (vorher 670, Befund WP1: 619 Ticks nach dem Ende), Leben von 12 auf 8 Viertelherzen, wie im Vergleichslauf zu Fuß. Mit Boden bleibt es beim Halten bis zur Landung (Lauf w2-f-boden1: `fallHold=1`, Freigabe der Referenz beim Bodenkontakt, kein Sturzschaden). Mit dem letzten Testpaket ergaben w2r-void und w2r-boden dieselben Werte. Dieselbe Regel gilt für `endRideClean` (Ritt-Ende durch Ereignis, Stillstand, Menü). Dort ist sie nur mit Boden geprüft: `Stuck` an der Bergwand (w2r-wp6-wand) und `Event` in der Kanalisation (w2r-innen) endeten mit `fallHold=1`, die Referenz wurde beim Bodenkontakt freigegeben, ohne Sturzschaden. Ein Game Over bei wenig Leben bleibt wie im Spiel (Entscheidung E12).
- Abflug (Z1): Der Vogel ist 100 Ticks nach der Freigabe entfernt. Der Antrag geht bei Tick 99 hinaus (WP6.10), die Frist `kDepartDeleteFrames` bleibt 100 (Lauf w2-f-boden1: Vogelzahl 0 nach 100 Ticks).
- Logzeilen (Z1): `dismount (<Grund>, <Anlass>) at ride frame ...` nennt den Grundnamen, dazu kommt die Ende-Zeile `ride ended clean (<Grund>, <Anlass>), bird departing, fallHold=<0|1>`, so dass jedes Ritt-Ende eine Zeile `ride ended clean (` hat. Die Zeile `landing release (...)` nennt den Gang vor dem Ritt-Ende (vorher stand dort immer `gang 3`, weil tempo den Gang in `onRideEnd` zurücksetzt) und die Restgeschwindigkeit nach dem Nullsetzen.
- Versatz im ersten Fall-Frame (Z1 (e)): Im ersten Frame nach der Freigabe springt Link ohne Boden um rund 64 Einheiten. Ursache ist die Wurzelbewegung der Fall-Animation des Spiels (`procWolfFallInit` setzt den Wurzelmodus 0xC, `posMove` übernimmt die Wurzel-Z-Änderung von −27,65 auf 36,63). Die Mod verursacht ihn nicht und glättet ihn nicht. Diagnosezeilen `release diag` gibt es nur im Testbuild.

#### Prüfung Härtung Welle 2 (Zusammenführung)

Testbuild (`KFF_DEV_AUTOTEST=ON`) und Release (`OFF`) wurden aus demselben Quellstand in eigenen Build-Ordnern gebaut, ohne Warnungen aus `src/`. Die Läufe liefen nacheinander mit dem letzten Testpaket, jeder mit eigenem Benutzerordner. Jeder Lauf endete mit Rückgabe 0, ohne Absturzbericht und ohne Ausnahme im Mod. Alle Läufe mit dem Testbuild meldeten `fremdeingabe=0` und `registry check: callbacks=86 replaces=2 modules=7 disabled=0 conflicts=0`.

- Regression gegen Welle 1: `zyklus` (F_SP121) mit einem Ritt, Landeanflug, Ende `DismountButton`, `fallHold=1` und entferntem Vogel. Der zweite Ruf wurde mit `call rejected (damage)` abgewiesen, weil kurz vorher ein Pfeil (`E_ARROW`) Link getroffen hatte (Proc 0x108, `PROC_WOLF_LARGE_DAMAGE_UP`). In der Referenz der Welle 1 begann der zweite Ritt in genau diesem Proc. Das Ruf-Gate aus WP5 weist den Ruf jetzt ab, wie vorgesehen. Die Untergrenze (Link-Fuß) hob den Vogel schon im ersten Ritt-Frame an (in Welle 1 erst im zweiten). Der Vogel flog deshalb 50 Einheiten höher, und der Landeanflug dauerte 60 statt 3 Frames. `innen` (R_SP107) endete wie in der Referenz mit `Event` bei Ritt-Frame 364. `mensch` (F_SP103, `mensch_ritt = true`) hatte nach der Landung Leben 12. `e9-klippe` gab Link 954 Einheiten vor der Kante frei (Referenz 975,6), die Zeile `landing release` nennt jetzt `gang 5`. Ersatz für `crash` war `wp2-b701`: Der Vogel war nach 63 Ticks weg, und Link lebte. Über dem Abgrund ergab B `fallHold=0`, und der Void-Out des Spiels kam 35 Ticks nach dem Ende (Leben 12 auf 8). In Welle 1 hielt der Kern die Referenz an dieser Stelle bis zur Zeitgrenze.
- WP1: `void-absteigen` (Vögel nach 63 Ticks weg, Void-Out nach 86 Ticks, Leben 12 auf 8, kein Game Over), `boden-absteigen` (Landeanflug 60 Ticks, Vögel nach 100 Ticks weg, kein Sturzschaden, `dy` 0), `ladezone-anflug` (Stage im Ritt gleich, 4 Veto-Zeilen; 7 Ticks nach dem Ritt-Ende wechselte das Spiel nach D_SB00, Zeile `scene change allowed after end`) und `rueckkehr-raum-tag` (71 Ticks im Quader, `startRestartRoom` im Ritt 0, mit Testaufbau).
- WP3 (Szenario-Treiber, ein Lauf je Szenario): `karte` und `pause` (Ritt danach intakt), `pause-ruf` (`call rejected (Pause oder Menue offen)`, danach kein Vogel) und `ruf-spam` (60 Kombinationen, höchstens ein Vogel, 4 Ritte, 56 Ablehnungen mit Grund, am Ende weder Vogel noch Cargo). Laufen die Szenarien in einem gemeinsamen Lauf, meldet der Treiber nach `karte` „nicht bereit“, weil der Ritt weiterläuft. Dasselbe zeigt der WP3-Lauf wp3h-base-treiber. Die WP3-Autotests `wp3-karte-ritt`, `wp3-ui-halt`, `wp3-chord-b`, `wp3-schild`, `wp3-schild-wolf` und `wp3-r-analog` meldeten `ok` mit denselben Prüfwerten wie die WP3-Läufe. In `r-analog` gab es nach dem Ritt weder Spiel-Lock noch R-Flanke (`lockReparaturen=2`). Länger dauerten nur die Landeanflüge, weil der Vogel beim B-Tipp 50 Einheiten höher flog (siehe `zyklus`): in `ui-halt` 54 statt 48 Frames, im ersten Ritt von `r-analog` 58 statt 52. `wp3-dialog-b` braucht F_SP124 und lief nicht.
- WP4: `summonevent` (Vogel nach 2 Ticks weg, kein Ritt-Start, `CORE5 summonEventEnd=1`) und `verwandlung` (F_SP121, mit `game.enableQuickTransform` und `game.canTransformAnywhere` wie im WP4-Lauf verwandlung-4). In `verwandlung` startete R+Y im Abflug die Verwandlung bei Tick 449, das Ende war `Transform`, der Vogel nach 1 Tick weg, Link nach 49 Ticks Mensch, der Ruf danach `call rejected (transforming)`; Ergebnis `ok` mit denselben Werten wie im WP4-Lauf. Vor der Zeile `ride ended clean (Transform, …), session freed` steht jetzt die B-Ende-Zeile `ride ended clean (DismountButton, …)` (Z1). Ohne die beiden Einstellungen verwandelt das Spiel nicht; der erste Lauf (w2r-wp4-verw) hatte sie nicht und zählt nicht.
- WP5: `wasser` (F_SP115: niedrigstes Schweben 187,8 Einheiten, drei Rufe im Wasser abgewiesen, K17 einmal) und `mensch` (F_SP103: `mensch: call rejected (human)`, danach Ritt über autowolf).
- WP6: `wp6-wand` (Stillstand an der Bergwand, `watchdog: stuck 300 ticks`, Ende `Stuck` mit `fallHold=1`, Freigabe beim Bodenkontakt, kein Schaden; die Tunnel-Probe ergab keine Durchdringung) und `wp6-timeout` (`KFF_TEST_NORIDE=1`: Vogel nach 300 Ticks weg, der zweite 100 Ticks nach Abflugbeginn, höchstens ein Vogel). Dazu `wp6-gate` in R_SP107 (11 Versuche, davon 5 im Ereignis, bei jeder Ablehnung kein Vogel, `Fehler 0`, wie wp6p3-gate-r) und `wp6-ruf` in F_SP121 (unter der Decke 5 Versuche ohne Vogel, im Ruf-Spam höchstens ein Vogel bei 166 Ritt-Frames, Referenz wp6p2-ruf-1: 168).
- WP8: `eingabe-konsum`, `stick-relativ` und `geradeaus-b` jeweils `BESTANDEN`. In `eingabe-konsum` waren die Sperrflags vor dem Konsum HoldR 93, TrigR 3, HoldL 22 und TrigL 1, danach alle 0 (P2-04 in DEVELOPMENT.md).
- Nach dem Szenario-Ende stand Link ohne Eingabe im Zwielicht von F_SP121. Zwielicht-Kargoroks (`E_YR`), Schatten-Bulblins (`E_RDY`) und Pfeile trafen ihn, in sieben Läufen bis zum Game Over (`wp6-wand`, `rueckkehr-raum-tag`, `wp6-timeout`, `eingabe-konsum`, `stick-relativ`, `wp6-ruf` und `verwandlung`, dort als Mensch). Diese Treffer kamen außerhalb des Ritts, im Log als Akteur-Treffer mit Quelle. Der WP6-Referenzlauf wp6p6-wand-4 und der WP4-Lauf verwandlung-4 zeigen dasselbe. Im Ritt trafen Pfeile (`E_ARROW`) Link in `ui-halt` zweimal und in `r-analog` einmal: Das Leben sank, Link blieb im Griff (`letGo=0`), und der Ritt endete regulär mit B (WP5).
- Release (w2r-release): Die Startzeile lautet `initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`, auch mit gesetztem `KFF_AUTOTEST=zyklus`. Es gab keine Autotest-Zeilen, alle sieben Module waren installiert, und der Startbericht meldete `callbacks=78 replaces=2 modules=7 disabled=0 conflicts=0`. `mod.so` enthält weder `getenv` noch Testzeichenfolgen. Das Paket enthält nur `mod.json`, `lib/macos-arm64/mod.so` und `res/`.


## 19. Anhang: Prüfprotokoll Welle 3 (Zusammenführung 2.1.0)

Welle 3 führt die Patches p9 (Kern-Tick-Zeitbasis) und p10 (Paketpflege) zusammen und setzt N1, N2, N3 und N6 um. N4 und N5 sind nicht umgesetzt, weil keine Freigabe vorlag. N7 dient als Regressionsprüfung; Zeitgeber wurden nicht zurückgenommen. N8 (Portabilität) ist nicht angewendet. Die geprüften Pakete liegen eingefroren unter `merge-w3/pakete/w3g-test` und `merge-w3/pakete/w3g-release`. Ticks sind Kern-Ticks, sofern nicht anders angegeben.

#### Änderungen

- **p9, Kern-Tick-Zeitbasis** (`src/core/timers.hpp`, neu): Ein Kern-Tick ist eine Pad-Lesung. Alle Zeitgrenzen des Pakets (Abflug, Ruf, Wiederanbindung, Landefenster, Fallreferenz) zählen Kern-Ticks, auch in Pause und Ereignis. Gezählt wird in `core::ridePadTick`, geprüft in den Execute-Hooks. Zeitstempel sind Zahlen, keine Zeiger.
- **p10, Paketpflege**: Region-Hinweis beim Start (`src/core/region.cpp`, neu, WP10.9). Die Datei liest nur die Disc-ID und schreibt nichts. Im Paket liegen `res/icon.png` und `res/banner.png`; `res/.gitkeep` entfällt. Das Release-Gate in `make-dist.sh` (Rückgabe 5) lehnt Pakete mit Testzeilen ab.
- **N1, Ruf während des Abflugs** (`src/core/ride.cpp`, `src/core/summon.cpp`, `src/core/session.cpp`, `src/modules/mensch.cpp`): Ein Ruf entfernt einen noch abfliegenden Vogel sofort (`requestDelete`) und ruft danach einen neuen. Logzeile: `call during departure: bird <id> removed now (requestDelete), new call`. Die Sperre `eigener Vogel aktiv` in `callBlockReason` ignoriert einen abfliegenden Vogel (`birdDeparting`). Im Zwischenstand vor diesem Fix wurde der Zweitruf im Abflug in beiden Varianten mit `eigener Vogel aktiv` abgewiesen.
- **N2, README**: Installation von Hand mit den Mods-Pfaden für macOS, Windows und Linux (nur macOS arm64 geprüft). Keine Verweise auf Installationsskripte. Absteigen mit `F` oder `⌥+F`, mit Hinweis zur Wahltaste. Zwielicht-Gegnerhinweis. Der Vogel wird 100 Ticks nach dem Absteigen entfernt.
- **N3, Kamera im Menschen-Ritt** (`src/modules/kamera.cpp`): Bei `mensch_ritt` dreht die Kamera um einen Punkt auf halbem Weg zwischen Vogel und Link. Logzeile `MENSCH-RAHMEN an: dist=1100 …`. Startabstand `kamera_abstand_mensch` 1100 (Bereich 300 bis 2000), Einblendung in 45 Ticks.
- **N6, kein Text ohne Einstellung**: `hud_anzeigen` ist standardmäßig aus (`src/core/config.cpp`). Das Release-Gate prüft die Bibliothek im Zip (siehe unten).

#### Build und Paket

- Testbuild `bld-test3` (`KFF_DEV_AUTOTEST=ON`, fertig 22:14:28) und Release `bld-release3` (`OFF`, fertig 22:22:36), jeweils im eigenen Build-Ordner außerhalb des Quellordners. Warnungen aus `src/`: 0 in beiden. Insgesamt meldete der Compiler 163 (Test) und 143 (Release) Warnungen, alle aus Dusklight-Headern (`m_Do_ext.h`, `JAUSoundTable.h`, `JORServer.h`, `linklist.h`, `stb.h`). Keine Fehler. Keine Datei unter `src/` ist jünger als das Testbuild-Log.
- `make-dist.sh` mit Release (`KFF_DEV_AUTOTEST=OFF`) in einem frischen Build-Ordner: Rückgabe 0. Autotest-Gate und Testtext-Gate bestanden. Paket `kargarok_flight.dusk`, 7 Einträge, 175822 Bytes, SHA-256 `c2b0fffa36727191fe36003ccec481cc20d816924327c5f6cb89eaac2b23d23f`.
- Testtext-Gate im Detail: `strings` auf `lib/macos-arm64/mod.so` im Zip liefert für die Release-Bibliothek 0 Treffer für `TEST `, `fremd=` und `autotest`. Gegenprobe mit der Testbibliothek: 315 Treffer. Der erste Versuch mit `strings` direkt auf die `.dusk`-Datei war ein Fehlschluss, weil das Paket ein Zip-Archiv ist. Die Prüfung muss den Inhalt des Archivs lesen. Die Release-Bibliothek enthält `Autotest` einmal, in der Startzeile `ohne Autotest`. Das ist erlaubt.
- Release-Startbericht: `callbacks=78 replaces=2 modules=7 disabled=0 conflicts=0`. Testbuild: `callbacks=86`, ebenfalls ohne Konflikte.

#### Läufe

Alle Läufe endeten mit Rückgabe 0, ohne Absturzbericht und ohne Ausnahme im Mod. Jeder Lauf hatte einen eigenen Benutzerordner. In allen Läufen steht `fremdeingabe=0`.

- `w3g-abflug-wolf` (F_SP121, `ruf-abflug`): Ritt, Absteigen, Ruf während des Abflugs. Logzeile `call during departure: bird 0x121 removed now`, danach zweiter Ritt mit Vogel 0x123. N1 belegt.
- `w3g-abflug-mensch` (F_SP103, `mensch_ritt = true`, `ruf-abflug`): gleiche Folge. Vogel 0x10e entfernt, zweiter Ritt mit Vogel 0x110. N1 belegt.
- `w3g-zyklus` (F_SP121): Ritt, Landeanflug, Absteigen, Zweitruf mit `call rejected (damage), B stays (n=1)`. Derselbe Verlauf wie die Referenz `w2r-zyklus`. Ein Zähler weicht um einen Frame ab (1243 gegen 1244). Die Abweisung wegen Schaden ist das Ruf-Gate aus WP5 (ein Pfeiltreffer kurz vorher). Im früheren Zwischenstand S1 war der Zweitruf angenommen; die Rückkehr zur Welle-2-Regel ist bewusst.
- `w3g-b701` (F_SP121, `wp2-b701`): `wp2 RESULT wp2-b701 ok`, Vogel nach 63 Ticks weg, Link lebt, kein Cargo, kein Midna-Zeiger. Der Zähler `birdFrames=701` belegt den B-Tipp bei Ritt-Frame 701. Die Ergebniszeile nennt `B bei Ritt-Frame 0`, weil sie nach dem Ritt-Ende ausgegeben wird (seit Welle 2 so). Alle Ritt-Zähler sind identisch zu `w2r-b701`; abweichend sind nur `roofRestartOutside` und `kamera calls`, die von der Laufdauer abhängen (100 s gegen 150 s).
- `w3g-mensch` (F_SP103, `mensch_ritt = true`): Ritt, Linkskreis, Rechtskreis, Absteigen, Landung, Leben 12. Der Höhenverlauf (`vogelY`, `baseY`, `nick`) ist identisch zu `w2r-mensch`. Die x/z-Positionen von Link weichen ab; die Gehstrecke in 600 Ticks beträgt 45,6 gegen 22,9 Einheiten. Vermutlich wirkt die kamerarelative Steuerung aus N3. Das ist nicht gesondert belegt.
- `w3g-mensch-foto` (N3-Nachweis): Screenshot `shot-50pct.png` zeigt den Vogel und Link vollständig im Bild. Log `MENSCH-RAHMEN an: dist=1100`.
- `w3g-innen` (R_SP107): Ritt-Zähler identisch zu `w2r-innen` (birdFrames 364). Laufdauer 180 s gegen 150 s; deshalb weichen `roofRestartOutside` und `kamera calls` ab.
- `w3g-hud-an` (zyklus, `hud_anzeigen = true`): Screenshot zeigt `Gang 3/5 … 54 km/h … Dash bereit` und die Testzeile `TEST eff=0.50 …`.
- `w3g-zyklus` mit Standardeinstellung: Screenshot ohne Mod-Text.
- `w3g-release` (Release): Startzeile `kff: initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`, keine Testzeilen.
- Treiber 0.2.1, Testbuild: `w3d-void-absteigen` ok, Vogel nach 64 Ticks weg (Grenze 100), Void-Neustart nach 81 Ticks (Grenze 150). `w3d-void` ok, Vogel über Nichts verschoben, Ritt bleibt bestehen (`Ritt=1`). `w3d-dauer2` ok, 2 Minuten, 3081 Ritt-Ticks, Leben min 12.
- Void-Out mit dem Flug-Autotest und den Parametern von Welle 2 (`w3h-void-wp1`, Gegenstück zu `w2r-void`): Ergebniszeile wörtlich gleich zu W2: `birds0 nach 63 Ticks life vorB=12 min=8 jetzt=8 … neustart(Link-Wechsel) t=556 voidOut nach 86 Ticks`. Der Sturzschaden 12 auf 8 bleibt erhalten.

#### Bekannte Lücken

- Die volle Dauer-30-Prüfung aus N7 ist nicht gelaufen. Gelaufen ist die Zwei-Minuten-Variante `w3d-dauer2`.
- Der Treiber `w3d-void-absteigen` meldete `Leben 12`. Er liest das Leben beim Link-Wechsel. Die Ursache der Abweichung zum Flug-Autotest ist nicht abschließend geklärt. Maßgeblich ist `w3h-void-wp1`, der W2 wörtlich entspricht.
- Im Paketordner liegen Build-Ordner, die Welle 3 nicht angelegt hat: `bld-release`, `bld-release-v`, `bld-test`, `bld-test-v` und `build`. Sie sind nicht verändert und nicht Teil des Zip. Zum Aufräumen durch den Nutzer.
- Ein paralleler Hintergrundagent arbeitet an Kamera-Ruckeln in Mensch-Kurven, in einer eigenen Kopie. Am Paketstand dieses Anhangs ist `src/modules/kamera.cpp` unverändert (21:52). Eine Übernahme würde die N3-Nachweise ungültig machen.
- Die Region-Zeile zeigt `RZDP nicht getestet`. Das Verhalten für andere Regionen ist nicht belegt.
- Nicht geprüft: Windows, Linux und andere Dusklight-Versionen als 2.0.3.
