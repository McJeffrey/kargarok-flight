# Changelog

## 2.4.2

One `kargarok_flight.dusk` for every platform Dusklight supports. The mod code and its behavior in the game are unchanged.

### Added

- GitHub Actions build (`.github/workflows/build.yml`, from TwilitRealm/mod-template). It builds the mod for Windows (x64, ARM64), macOS (Apple Silicon, Intel), Linux (x86_64, aarch64), iOS and Android and merges the packages into one `.dusk` with `tools/merge_mod.py`. A pushed tag attaches that file to the GitHub release.
- `cmake/FetchDusklight.cmake` fetches the Dusklight sources (`v2.0.3`) on the first configure. `-DDUSKLIGHT_DIR` is no longer required.

### Fixed

- Windows build: MSVC now gets `/Zc:__cplusplus`. Without it, the build failed with `error C2440` in the standard library.
- README: the four screenshot links pointed to files that do not exist.

### Changed

- README, INSTALL.md and WINDOWS.md describe all platforms. Tested in the game on macOS (Apple Silicon) and Windows (x64).
- `.gitattributes` keeps text files LF, so `mod.json` is byte-identical in every platform package.
- Version 2.4.2.

## 2.4.1

Cleaner Mods panel: one description paragraph, settings right below. New mod ID `com.kaizuki.kargarok_flight`, so the settings of 2.4.0 are not carried over automatically.

### Changed

- The description in `mod.json` is one paragraph about the ride. It says how to call the bird and land, and that speed, camera and every flight button can be adjusted in the Mods panel.
- New mod ID `com.kaizuki.kargarok_flight` (was `dev.local.kargarok_flight`). The config keys now have the prefix `mod.com_kaizuki_kargarok__flight.` (was `mod.dev_local_kargarok__flight.`).
- Settings are not carried over automatically at the first start with 2.4.1. The old keys stay in `config.json`, but the mod does not read them. Set the settings again in the Mods panel, or copy the keys to the new prefix with the migration script `migriere-config.py`, which is not part of the package. The script writes a new file and never changes the original.
- Version 2.4.1.

### Removed

- The section "How to fly" in the Mods panel, with its eight input lines. The Mods panel has no key lines any more. The settings follow the description directly.

## 2.4.0

Speed is now stepless. Hold R (R2 on PlayStation) to speed up and hold L (L2) to slow down. Let go to keep the speed you reached. Slow down to a standstill, and the bird hovers without a tap. The five gears are replaced by a speed from 0 to 100 percent of top speed. D-Pad Up and D-Pad Down still change the speed in steps of 20 percent.

A human ride ("Ride as Human Link") now uses the same camera and the same flight as the wolf ride. Two things still differ: Link hangs at the grip under the bird (grip, hanging pose and pendulum), and the camera aims 100 units lower so that Link stays in the picture. Target platform: macOS arm64 with Dusklight 2.0.3. The region notes are unchanged: only `RZDP` is reported as tested.

### Added

- Stepless speed with R and L. Hold R (R2 on PlayStation) to speed up and hold L (L2) to slow down. The speed changes steadily while you hold: from standstill to top speed in about 3 seconds at full press. A lighter press changes it more slowly, down to half the full rate. Let go, and the bird keeps its speed (cruise control).
- Hover by slowing down. Hold L until the bird stops in the air, and it hovers on its own, without a tap. Hold R, or tap D-Pad Up, to fly on.
- Two new flight buttons: "Faster (hold)" (`bind_schneller`, default R) and "Slower (hold)" (`bind_langsamer`, default L). They can be changed in the Mods panel and in the Flight buttons window, like the other buttons.

### Changed

- Gears replaced by speed. The speed is a percentage of top speed, from 0 (hover) to 100 percent. D-Pad Up and D-Pad Down still work: one tap adds or takes away 20 percent.
- Hover: it starts when the speed reaches 0 percent, by holding L or by tapping D-Pad Down until the speed is 0. Before, one tap of D-Pad Down in gear 1 started it. D-Pad Up or holding R now ends it: D-Pad Up brings the speed to 20 percent, and holding R speeds up.
- Display: "Show gear and speed" is now "Show speed and hover". It shows the speed in percent, or Hover while the bird stands still.
- Start gear is now "Start speed (1 to 5)". The values are unchanged: 30, 40, 50, 75 and 100 percent.
- Human ride camera: the camera turns around the bird, as in the wolf ride. Before, it turned around a point between the bird and Link. The start distance is `kamera_abstand` (650 units, as in the wolf ride), the maximum is 1400 units, and the default tilt is 14 degrees. Before, the maximum was 2000 units and the default tilt 25 degrees. The distance changes by at most 120 units per tick, as in the wolf ride (before, the human ride allowed 60 units per tick). It no longer adds tilt when it avoids a wall and no longer looks ahead into curves. The one difference: the aim point lies 100 units under the bird, so that Link's feet stay in the picture (Link hangs about 350 units below the bird).
- Human ride flight: the same flight as in the wolf ride, including the height limit per frame, the height snap, braking and the lowest height. Before, the human ride used a separate base for these.
- The panel text of "Ride as Human Link" says that the ride has the same camera and controls as the wolf ride.
- Version 2.4.0.

### Removed

- The five fixed gears as the speed model. Their percentages, 30, 40, 50, 75 and 100, remain as the start speeds of `start_gang`.
- The distance chosen with the D-Pad is no longer kept for the next human ride. Every ride starts at `kamera_abstand`.

### Configuration

- `bind_schneller` (default 7, R) and `bind_langsamer` (default 6, L) are new. `bind_gang_hoch` and `bind_gang_runter` keep their names, and each tap now takes one step of 20 percent. `start_gang` keeps its name and now sets the start speed. `hud_anzeigen` keeps its name and now shows the speed in percent, or Hover.
- `kamera_abstand_mensch` (default 1100) has no effect any more. The key stays readable, so existing configuration files still load.

### Developers

- Not yet tested in the game: the R and L holds, the speed change with the trigger pressure, and the hover at standstill. The runtime test is pending.
- Test build: the pure rules of the flight buttons (`core/actions_test.cpp`) cover the two new actions, including the shared button R for Call: hold and Faster (hold).
- Test build only: the scenario `kamera-mensch` (human ride with the timeline of `kamera`), and the log lines `kamera: GRIFF` (position of Link and the bird in the picture) and `freiheit: DIAG` (lift and distance to Link's feet, for every ride).

## 2.3.0

All text the player sees is now in English: the mod description, the Mods panel, the Flight buttons window, the on-screen display, the messages during a ride, and the documentation. The panel has a new section "How to fly". It shows the flight buttons in use and explains calling, speed and gears, hovering in place, dash, brake, barrel roll, camera and landing. Target platform: macOS arm64 with Dusklight 2.0.3. The region notes are unchanged from 2.2.0: only `RZDP` is reported as tested. Existing log lines are unchanged, except for the version number. New log lines cover the tips.

### Added

- Section "How to fly" at the top of the Mods panel. Its eight lines show the flight buttons in use, and they update when you change them.
- Setting "Show tips on your first rides" (on by default). It shows two short tips on each of your first three rides. New configuration keys: `tipps_anzeigen` (default `true`) and `tipps_rest` (default 3, internal counter).
- Help texts in the Flight buttons window for each action, including the conditions. For example, no dash while braking or landing, and no barrel roll while hovering.
- New icon and banner (`res/icon.png`, `res/banner.png`).
- Images for the README in `docs/images/`.
- Quick start at the top of the README, and a section on going to other regions with the Dusklight warp.
- Documentation in English: README.md, INSTALL.md and this changelog. DEVELOPMENT.md stays in German.

### Changed

- Panel texts in English, with more detail: "Start gear (1 to 5)", "Invert pitch (stick up lifts the nose)", "Show gear and speed" (shows Hover and the dash status too), "Wider view at high speed" (in gears 4 and 5), and "Change flight buttons (default)".
- The line that shows the call and dismount buttons moved from the Controls section to "How to fly".
- HUD texts: "Hover" instead of "Schweben", "Gear 3/5" instead of "Gang 3/5", and the dash status as "Dash active", "Dash in 2.3 s" or "Dash ready". The speed in km/h is marked with "~" as an estimate.
- The HUD line is hidden while a tip box is on screen, because the box covers it at the top right. It returns when the box closes.
- Flight buttons window: new labels "Reset flight buttons to default" and the dialog title "Reset flight buttons", new status text "All flight buttons are back on the default layout.", and new help texts for all actions.
- The mod description in `mod.json` is in English. Version 2.3.0.

### Fixed

- On-screen messages during a ride named the B button, even when dismounting had another button. They now show the button you have set.
- The message for a stuck bird said that the ride continues. In this case, the ride ends, and the message now says so.

### Known limitations

- See the README under "Known limits". The limitations of 2.2.0 apply unchanged.
- Over deep water far from the shore, a landing can end with a reset to the start point of the room and the loss of 4 life points. This was seen in one test.
- Log lines stay in German, as before.

## 2.2.0

Settings in the Mods panel, freely assignable flight buttons, hovering and corrections to the ride. Target platform: macOS arm64 with Dusklight 2.0.3. Tested with disc ID `RZDP`. `GZ2P`, `GZ2E` and `GZ2J` are not tested.

### Added

- Settings in the Mods panel: The panel "Kargarok Flight" has the sections Flight, Display, Controls and Experimental. Toggle and number settings: Start gear (1 to 5), Stick up lifts the nose, Show gear and speed, Wider view at high speed, Ride as Human Link. The button "Change buttons (default)" opens the window with the tab "Flight buttons" (`core/panel.cpp`, `core/steuerung.cpp`).
- Freely assignable flight buttons in the "Flight buttons" window: Dash, Barrel roll, Brake (hold), Gear up, Gear down, Camera behind bird, Camera closer (hold), Camera farther (hold), Call: hold, Call: then tap and Dismount. A button can serve only one action, except Call: then tap and Dismount, which share B. If exactly one collision occurs, the two actions swap. Call: hold, Call: then tap and Dismount must be bound (`core/steuerung.cpp`, `core/actions.cpp`).
- Configuration keys `bind_*` for each flight button (`core/config.cpp`).
- The in-ride display follows the bindings: the button used for Dash shows "Dash", and the button used for dismounting shows "Quit". This applies to A, B, X, Y and Z. R has a field without text (`modules/flug.cpp`).
- A call with a dead Link is refused (reason `Link tot`, `core/summon.cpp`).
- If Link dies between the call and the ride start, the called bird is removed (log `summon aborted (Link tot)`). This case is not tested (`core/summon.cpp`).
- Hovering: Below gear 1, the bird stands still in place. Gear down in gear 1 starts the hover. Gear up or Dash brings the bird back to gear 1. The Control Stick turns the bird and raises or lowers it. The C Stick tilts the camera up to 70 degrees without swinging back. With "Show gear and speed", the display shows the German word "Schweben" (replaced by "Hover" in 2.3.0). The floor is the minimum height (`modules/freiheit.cpp`). The standstill watchdog does not apply while hovering (`modules/freiheit.cpp`). If the bird bumps into a ceiling while rising, it stays below the ceiling until the stick is released or moved the other way (`modules/flug.cpp`).

### Changed

- Region notice at start: Only `RZDP` is reported as tested. `GZ2P`, `GZ2E` and `GZ2J` count as not tested. In 2.1.0, `GZ2P` was reported as tested. Now the code reports only `RZDP` as tested (`core/region.cpp`).
- Telemetry: The lines from `logTelemetry`, for example `kamera: RUCK`, now run only in the test build. In the release build, the info, warning and error lines remain, along with the rate-limited lines (`logEvery`) (`core/log.cpp`). In the release, this includes, among others, the start line `kff: initialize v2.2.0 (Kargarok Flight, Release, ohne Autotest)` (`core/core.cpp`), the line `registry check: callbacks=… replaces=… modules=… disabled=… conflicts=…` (`core/hooks.cpp`), the ride line `ride f=…` every 600 ticks (20 seconds) (`core/ride.cpp`), `ride ended clean (…)`, the lines about calls, rides and saving, and all warnings and errors.
- Camera in the human ride: The camera turns around a point halfway between the bird and Link, which fades in over 45 ticks (1.5 s) (`modules/kamera.cpp`). Start distance 1100 (`kamera_abstand_mensch`), default tilt 25 degrees (`modules/kamera.cpp`), maximum distance 2000 when set with the D-Pad (`modules/kamera.cpp`). The distance last chosen with the D-Pad applies to the next human ride for as long as Dusklight runs. Near walls and fences, the camera moves closer. The wall limit takes effect at once. The control is described in `modules/kamera.cpp`. The orientation jumps only at initialization (`modules/kamera.cpp`).
- Camera in the human ride, measured in test runs: In the settled ride, the largest camera jump is 117.8 units (previously 311.3), and there are no jumps above 130 units any more. At the start, a jump of 1251.0 units (previously 1587.4) remains if Link stands close to a wall or fence. In the settled state, the zoom has 4 jumps above 130 units, the largest 134.5 (previously 3 jumps, the largest 214.1).
- Log texts: New or changed are `call rejected (…), B stays (n=…)` (`core/input.cpp`), `call during departure: bird 0x… removal requested, the new bird follows its _delete hook` (`core/summon.cpp`), `deferred call: new bird created … core ticks after the call, old bird removed, … more call(s) accepted` (`core/summon.cpp`), `summon aborted (Link tot): …` (`core/summon.cpp`), `unload landing: Link von … auf den Boden … gesetzt` (`core/ride.cpp`) and `save refused during ride …` (`core/ride.cpp`).

### Fixed

- Saving during a ride: The mod refuses saving while a ride runs. The save menu shows the note "Erst absteigen (B), dann speichern" (German text in 2.2.0). The menu returns to the overview, and the ride continues (`core/ride.cpp`). When the game writes the card, it shows an error. After that, dismounting with B is possible (`core/ride.cpp`). Only this route through the card menu is tested. If the card layer is busy, the save is refused without completing (log `save refused during ride, card layer busy (…)`, `core/ride.cpp`). This case is not tested. Autosave is excluded. Whether it writes during a ride is not tested (`core/ride.cpp`).
- Falling when the mod unloads during a ride over ground: Link is placed on the ground below him, without a fall from ride height (log `unload landing: …`, `core/ride.cpp`). During the dismount (phase Dismounting), unloading also sets the fall reference to the ground (`core/ride.cpp`). Unloading over water and over a chasm is not tested. See "Known limits" in the README.
- The in-ride display no longer stays on screen when the mod unloads. The initial state of the buttons is restored on unload (`modules/flug.cpp`, `restoreIfActive`). Not tested in the game.
- Call during departure: A call while the bird is still flying away removes the old bird and creates the new one as soon as the old one is removed. There are never two of your own birds. In the measurement, the new bird appeared 3 ticks after the call (counted in pad ticks). If the call waits longer than 300 ticks (10 s), the mod gives up, and B goes back to the game (`core/summon.cpp`, `kDeferGiveUpTicks`).
- Steep walls: In the measurements, the bird no longer stays stuck on steep walls. The standstill at the test wall dropped from 297 to 0 ticks (`modules/freiheit.cpp`). When it hits a steep wall, it can still be pushed, up to about 680 units. In other measurements, the push was smaller (`modules/freiheit.cpp`).
- Landing approach after the second B: Link jumped 53.11 units at the transition to the fall, because the game added the root offset of the animation. The mod now removes this offset after letting go, for at most 90 ticks (3 s) (`core/ride.cpp`; hook in `core/hooks.cpp`). Even when Link touches the ground or water on release, he no longer jumps sideways in the measurements (previously up to about 68 units). To prevent this, the mod holds him horizontal in place during the first 10 ticks (1/3 s) after release (`core/ride.cpp`).
- Dismounting over water: The mod previously kept the fall reference only above ground. When Link fell over water, the vanilla rule reset him before the water surface was detected. The mod now keeps the fall reference above ground or water (`core/ride.cpp`). The bird releases Link as soon as his feet are at most 30 units above the water (`modules/griff.cpp`). Over water, Link falls in and swims, without restart and without heart loss. Real chasms keep the vanilla rule (`core/ride.cpp`).

### Known limitations

- Unloading over water and over a chasm is not tested.
- If a human ride starts directly in front of a wall or a fence, the camera jumps sideways once.
- Hovering directly against a wall is not tested. Over a chasm, B releases Link at once. He falls as in the original game, and the bird disappears 100 ticks later.
- On very steep walls, the bird can fly lower than the minimum height, down to about 120 units above the ground.
- Landing after the second B: in single cases, a sideways jump remains. When the fall starts at a wall, it is up to about 28 units. When the bird touches the ground after the minimum duration, it is up to about 14 units (`core/ride.cpp`).
- The complete list is in the README under "Known limits".

## 2.1.0

Expansion and hardening of the ride, plus a package for distribution. Target platform: macOS arm64 with Dusklight 2.0.3. Acceptance testing used an EUR disc that reports itself as ID `RZDP`. `GZ2P01` is not verified. The test records for this state are in `DEVELOPMENT.md`, appendix 18.

### Added

- Module `griff`: If Link loses his grip on the bird, it is attached again. If Link has no grip for more than 60 ticks in a row, the bird descends cleanly (log: `griff: <n> Frames ohne Griff (<m> Kern-Ticks), geordnetes Absteigen`). Death and an event without grip end the ride at once.
- Module `tempo`: five gears with 30, 40, 50, 75 and 100 percent of top speed. The ride starts in gear `start_gang` (default 3). Gear up and down with the D-Pad, brake with Z, dash with A. In addition, a field-of-view extension of up to 10 degrees (`sichtfeld_effekt`) and a HUD line (`hud_anzeigen`).
- Module `flug`: barrel roll with Y (360 degrees in 64 ticks). With the nose stick in neutral, the nose slowly levels out.
- Landing approach after B: The bird brakes and sinks until it is about 150 units above the ground, or until Link's feet touch the ground. After 240 ticks at the latest, Link is released. A second B tap and a chasm release immediately.
- Module `freiheit`: minimum distance above the highest surface under the bird (`min_ground_clearance`), gliding along walls and floors, standstill avoidance and a standstill watchdog. The watchdog ends the ride after 300 ticks without stick input (reason `Stuck`).
- Module `kamera`: wall limit with look-ahead. If the game camera stops during a ride without an event, the module starts it again.
- Module `mensch` as a partial port, off by default (`mensch_ritt`).
- Module `kamera`: new key `kamera_abstand` for the wolf ride (default 650, 300 to 1400). The wolf ride is unchanged. The camera in the human ride is described under 2.2.0.
- Test scenario `ruf-abflug` (test build only): call, ride, dismount, a new call while the bird is still flying away, ride, dismount. It applies to the wolf (`F_SP121`) and to the human (`F_SP103` with `mensch_ritt = true`).
- Save lock during a ride: writing the card and the save menus are refused during a ride. The ride continues.
- Call with a reason: If a call is not possible, B stays with the game, and the log gives the reason (`call rejected (<reason>), B stays`). Checks include transformation, event, departure, room above Link (reason `room`), pause and menu, open documents, water, climbing, falling, damage and a running shield attack.
- Ride ends with a reason: in addition, `Death`, `Menu`, `Warp`, `Reset` and `Transform`.
- Hook registry with owner, priority and conflict check. If the installation of a module fails or reports a registration conflict, only that module is switched off. The core keeps running.
- Startup check on load: own birds without a session, for example from an earlier load, are removed (log: `startup check: <n> own bird(s) removed`).
- Region notice at start: GZ2P (PAL) is reported as tested, GZ2E (USA) and GZ2J (JPN) as not tested. If the ID cannot be read, a warning follows.
- Configuration: `lenkung_invertieren`, `start_gang`, `hud_anzeigen`, `sichtfeld_effekt`, `mensch_ritt` and `decke_abstieg`. Each value is clamped to its range when it is read. For each key outside its range, there is exactly one warning line, plus a line with the effective values.
- Package: icon (`res/icon.png`, 512 x 512) and banner (`res/banner.png`, 1120 x 320) are listed in `mod.json`.
- Distribution: `install.sh`, `uninstall.sh`, `restore.sh`, `make-dist.sh` and `test-installer.sh` are in the developer's tools folder, not in the package. The package contains only the mod.
- Release gate in the tool `make-dist.sh`: The release package is refused (return code 5) if the library contains a test line (`TEST `, `fremd=` or `autotest`; case-sensitive). The message "ohne Autotest" in the start log is allowed.

### Changed

- `invert_steer` is now called `lenkung_invertieren`. The old key remains readable as a legacy alias and takes effect in an OR combination. The pitch inversion in the ride therefore comes only from the mod setting. Dusklight's option "Invert Air/Swim Y Axis" no longer has an effect in the ride.
- `min_ground_clearance` has the range -1 to 2000 and the default -1. The value -1 gives the vanilla value of 300 units. Values from 0 to 59 are raised to 60. In 2.0.0, there was no lower limit for -1.
- The camera is always free. The key `cam_mode` is removed. In 2.0.0, `cam_mode = 1` gave the game camera.
- Module `autowolf`: The watchdogs for transformation and bird call trigger after 600 ticks instead of 900. An event before the bird call or during the transformation back cancels the sequence.
- The call has a time limit of 300 core ticks without pause. If the ride does not start by then, the bird is removed.
- When the mod unloads, the ride ends cleanly first (reason `Shutdown`), and then the bird is removed.
- The start marker reads `kff: initialize v2.1.0 (Kargarok Flight, Release, ohne Autotest)`.
- `hud_anzeigen` is off by default (previously on). Without this setting, the game shows no text from the mod. With `hud_anzeigen = true`, the line with gear and speed appears.
- Time limits of the ride count core ticks (once per pad reading), also during pause and events, instead of link frames. This affects the landing approach, the fall reference, reattachment, the grip window and the call time. The ride line in the log appears every 600 core ticks (about 20 seconds), and the camera line `RUCK` is rate-limited.
- README: The installation is manual; there is no script. The mods folders for macOS, Windows and Linux are in the installation section. The README describes dismounting with F or ⌥+F, and the enemies on the Twilight steppe (`F_SP121`).

### Removed

- Configuration keys without effect. They are no longer registered: `mode`, `cam_mode`, `event_keep`, `roof_limit`, `far_plane`, `fog_off`, `landing_frames` and `all_rides`.
- The ceiling limit (`roof_limit`) and the mode key. There is only the free mode.
- Ride end `ManualSave`. It had no trigger, because saving during a ride is locked.

### Fixed

- Crash protection: If the path pointer `mpPath1` is empty, the package sets a substitute path and logs it (log: `mpPath1 was null before flight or stagger, fake path set (count <n>)`).
- A call during damage or a fall is refused. Previously, the ride started during damage.
- The bird takes over Link's facing direction at the start (`shape_angle.y`).
- If `flug` or `freiheit` is switched off, the call is refused. If the core hook `daAlink_c::execute` is missing, B also stays with the game.
- If Link dies during the departure, the bird is removed at once (reason `Death`).
- If the bird disappears without taking its own removal path, the core releases Link and Midna anyway (log: `bird gone: Link and Midna released without the bird`).
- A call under a low ceiling is refused with `room`, without creating the bird.
- If the bird hangs on a wall without stick input, the ride ends with `Stuck` after 300 ticks.
- A call shortly after dismounting was refused for up to 100 ticks with `departing`, while the old bird was still flying away. Now a new call removes the departing bird at once and calls a new one (log: `call during departure: bird 0x... removed now (requestDelete), new call`). `departing` now applies only during the landing approach after B. This applies to both the wolf and the human.

### Known limitations

The complete list is in the README under "Known limits". Acceptance testing used the ID `RZDP`, which the region check classifies as not tested. The package is built only for macOS arm64. A portability patch for Windows and Linux is not included.

## 2.0.0

Integrated state of part 1: core with the modules flug, freiheit, kamera and autowolf.

### Added

- Module `flug`: looping steering with pitch, yaw and barrel roll (Y tap). Configuration keys `invert_steer` and `cruise_speed`.
- Module `freiheit`: free mode without ceiling and without floor (`mode`, `roof_limit`, `min_ground_clearance`), wall gliding with a probe and an emergency exit, a block on loading zones and scene changes during a ride, a safe descent during events (`event_keep`), fall damage from the dismount height, and removal of the distance culling rule for far objects during a ride.
- Module `kamera`: free camera via the C Stick and the D-Pad. Configuration key `cam_mode`, ground protection.
- Module `autowolf`: transformation into the wolf by a call as a human, then the call of the Kargarok, and transformation back after landing if M_077 is not set.
- Core: end reason `EndReason::Event`, summoning timeout after 1800 frames, immediate end of the ride when Link dies, and abort of a running transformation when Link changes.
- Core: pre-hooks for `dStage_changeScene`, `dComIfGp_setNextStage` (4-argument form) and `daKago_c::checkFlySceneChange`, and a hook for `daE_YM_c::_delete`.
- Core: cleanup of pointers left behind after the bird is deleted (cargo keep, Midna, E_YM), taken over from version 1.
- Log line `CORE2` with the new counters.
- Autotest scenarios: `looping` (flug), `kamera` (kamera), `ordon`, `ordon-m077`, `ordon-event` (autowolf), `innen`, `grenze` and `szene` (freiheit).

### Changed

- Ground and ceiling belong exclusively to the module `freiheit`. The core sets the ceiling at ride start only if `roof_limit` is not set. The core no longer has its own `executeFly` pre-hook.
- The event descent uses `EndReason::Event` instead of `RiderLost`.
- Module priorities: flug 500, freiheit 400, griff 350, kamera 300, autowolf 250, tempo 200, mensch 150.

### Configuration

- Effective: `mode` (0), `invert_steer`, `cam_mode` (0), `cruise_speed`, `roof_limit`, `event_keep` and `min_ground_clearance`.
- Without effect: `far_plane`, `fog_off`, `landing_frames` and `all_rides`.

### Known limitations

- The modules `griff`, `tempo` and `mensch` are stubs (part 2). The core rule to dismount after 30 frames without cargo carry is still active.
- Modes 1 and 2 are not implemented.
- An event that starts directly after dismounting does not lift the scene lock (smoke test: event type 39, the bird disappears).
- Trigger volumes (tags) and the room reload after a deep fall are not blocked.
- In free mode without `min_ground_clearance`, the bird can sink below the ground.
- Smoke tests only on the stages F_SP121, F_SP103 and R_SP107.
- The long timeline `KFF_AUTOTEST_LONG` from version 1 is not ported.
