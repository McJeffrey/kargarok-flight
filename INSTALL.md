# Installation: Kargarok Flight 2.4.1

Fly anytime, anywhere on the friendly Kargarok! Call your ride and it swoops down, grabs Wolf Link in its talons and carries you high above Hyrule. Glide across Hyrule Field toward the castle, race through the canyon into Kakariko, circle the smoke of Death Mountain or sweep over the Gerudo Desert – the whole world is yours to explore from the sky. Speed up and slow down smoothly, hang perfectly still in the air to take in the view, burst ahead with a dash, or throw in loops and barrel rolls just for fun. Hold R and tap B to call the bird, tap B to land. Speed, camera and every flight button can be adjusted in the Mods panel.

Kargarok Flight is a mod for Dusklight 2.0.3. These instructions apply only to macOS on Apple Silicon Macs (arm64).

## What you need

- Dusklight 2.0.3 for macOS arm64, with your own copy of *The Legend of Zelda: Twilight Princess*. The tested disc ID is `RZDP` (PAL).
- The file `kargarok_flight.dusk`. The package contains only the mod.

## Mods folder

Dusklight loads mod files (`.dusk`) from the `mods` folder in its user folder. On the Mac, this is:

`~/Library/Application Support/TwilitRealm/Dusklight/mods`

## Installation

Installation is manual. There is no installer script.

1. Quit Dusklight completely.
2. Copy `kargarok_flight.dusk` into the mods folder. Keep the file name unchanged.
3. If the mods folder contains an older version, such as `kargarok_flight.dusk` from 2.2.0, move it out of the folder first, for example to your Desktop.
4. Start Dusklight.

On start, the log contains the line `kff: initialize v2.4.1 (Kargarok Flight, Release, ohne Autotest)`. The log lines are in German, as the mod writes them. If the line is missing, the mod is not active. Then check that the file is in the mods folder and that it was copied completely. The footer of the Mods panel shows Version 2.4.1.

## Log

The log file is in the `logs` folder in the Dusklight user folder. On the Mac, this is `~/Library/Application Support/TwilitRealm/Dusklight/logs`. Its file name starts with `dusklight`. All messages from the mod start with `kff:`.

## Uninstall

1. Quit Dusklight completely.
2. Take `kargarok_flight.dusk` out of the mods folder, for example by moving it to another folder.

Settings are stored in `config.json` in the Dusklight user folder. Each key has the form `mod.com_kaizuki_kargarok__flight.<name>`. Delete these settings by hand only if you really want to remove them, and only while Dusklight is closed.
