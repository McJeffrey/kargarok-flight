# Kargarok Flight 2.4.1 on Windows: build and test

## Kurzfassung (Deutsch)

- Dieses Repo baut den Mod unter Windows mit CMake. Die Grundlage ist der öffentliche Dusklight-Quellcode, Tag `v2.0.3`, mit Submodulen. Ein eigener Dusklight-Fork ist nicht nötig.
- Du brauchst Visual Studio (Workload "Desktop-Entwicklung mit C++"), CMake 3.25 oder neuer, Ninja und Git. Python brauchst du für den Mod nicht.
- Alle Befehle laufen in der **Developer PowerShell for VS** (Startmenü). Ergebnis: `build\mods\kargarok_flight.dusk`.
- Dusklight vollständig beenden, dann die `.dusk` in `%APPDATA%\TwilitRealm\Dusklight\mods` kopieren.
- Die Test-Checkliste steht in Abschnitt 5.
- Stand: Der Build ist für Windows vorbereitet, aber noch auf keinem Windows-PC gebaut. Der erste Lauf auf Windows ist der eigentliche Test.

The README and INSTALL.md describe macOS only. This file is the Windows guide.

---

## 1. Requirements

- Windows 10 or 11, 64-bit.
- Visual Studio with the workload **Desktop development with C++**, including the Windows 11 SDK and CMake tools for Windows. The Dusklight documentation recommends Visual Studio 2026 Community. Visual Studio 2022 has not been tested for this mod.
- CMake 3.25 or newer. It is included in the Visual Studio CMake tools, or install it with `winget install Kitware.CMake`.
- Ninja. It is usually included with the Visual Studio CMake tools. Otherwise install it with `winget install Ninja-build.Ninja`.
- Git for Windows.
- Python is not needed for the mod. Only Dusklight's own build uses it.

To **play** the mod you also need:

- Dusklight 2.0.3 for Windows x64: `Dusklight-v2.0.3-win32-x86_64.zip` from https://github.com/TwilitRealm/dusklight/releases/tag/v2.0.3
- Your own legally obtained copy of *The Legend of Zelda: Twilight Princess*. The game is not part of this repository.

## 2. Get the sources

Use PowerShell. The examples use `C:\dev`.

```powershell
cd C:\dev
git clone https://github.com/McJeffrey/kargarok-flight.git kargarok-flight
git clone --recursive --branch v2.0.3 https://github.com/TwilitRealm/dusklight.git dusklight-2.0.3
```

- The mod repository is private. Git asks you to sign in on the first clone. Git Credential Manager opens a browser window for that.
- The second clone is the public Dusklight repository at tag `v2.0.3`, with its submodules. Check it with `git -C C:\dev\dusklight-2.0.3 describe --tags`. The output must be `v2.0.3`.
- Keep the Dusklight clone outside the mod repository. Do not commit it.

## 3. Build the mod (Release, without test code)

1. Start **Developer PowerShell for VS** from the Start menu. This shell provides the MSVC compiler. In a normal PowerShell, CMake may not find the compiler.
2. Run these commands:

```powershell
cd C:\dev\kargarok-flight
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKFF_DEV_AUTOTEST=OFF -DDUSKLIGHT_DIR=C:/dev/dusklight-2.0.3
cmake --build build --target kargarok_flight_package
```

Notes:

- `-DKFF_DEV_AUTOTEST=OFF` builds the release without the test code. Do not change it for the normal test.
- `-DDUSKLIGHT_DIR` must point to the folder that contains `sdk`. Use forward slashes (`C:/dev/...`).
- If you do not pass `-DDUSK_GAME_EXE`, CMake downloads the link stub `windows-amd64.lib` (about 9 MB) during configure. The source is `https://github.com/encounter/dusklight/releases/download/sdk/windows-amd64.lib`. If the download fails, for example behind a proxy or firewall, download that file in a browser. Then add `-DDUSK_GAME_EXE=C:/path/to/windows-amd64.lib` to the configure command. The file name must end in `.lib`.
- After a change in the CMake files, run `cmake -S . -B build` again. The cache keeps the other settings.

The result is `build\mods\kargarok_flight.dusk`. It is a zip archive. It must contain `mod.json`, `lib\windows-amd64\mod.dll`, `res\icon.png` and `res\banner.png`. The platform folder is `windows-amd64`, as ModSDK.cmake names it.

## 4. Install into Dusklight

1. Quit Dusklight completely, including the tray icon. Dusklight loads mods at start, so change the file only while Dusklight is closed.
2. Open `%APPDATA%\TwilitRealm\Dusklight\mods` (Win+R, paste the path). Create the folder `mods` if it does not exist.
3. If the folder contains an older `kargarok_flight.dusk`, for example from 2.2.0, move that file out of the folder first.
4. Copy `build\mods\kargarok_flight.dusk` into that folder. Keep the file name unchanged.
5. Start Dusklight with your game disc. The Dusklight documentation describes the `--dvd <path>` option.

Other Dusklight paths on Windows:

- Log: `%APPDATA%\TwilitRealm\Dusklight\logs\dusklight*.log`. Lines from the mod start with `kff:`.
- Settings: `%APPDATA%\TwilitRealm\Dusklight\config.json`. Keys have the form `mod.com_kaizuki_kargarok__flight.<name>`. Change them only while Dusklight is closed.
- When a mod loads, Dusklight unpacks the mod DLL into `%APPDATA%\TwilitRealm\Dusklight\mods\.cache`.

## 5. Test checklist

The button names are the GameCube names used in the Mods panel. On a DualSense, R is R2, L is L2, and Z is R1. Other controllers and the Windows button mapping are not tested. Check the mapping in the Flight buttons window.

Log checks (open the newest `dusklight*.log` after the start):

- [ ] `kff: initialize v2.4.1 (Kargarok Flight, Release, ohne Autotest)` is present. If it is missing, the mod is not active.
- [ ] `kff: region: RZDP (PAL ...)` is present for the test disc. Another disc gives a warning. That is expected.
- [ ] No line contains `hook ... failed`. Two of the hook lines need special attention on Windows, because their symbol names are the MSVC form: `kff: hook dComIfGp_setNextStage (pre, core): ok` and `kff: hook updateAutoSave (pre, core, E11): ok`. Send the log if one of them says `failed`.
- [ ] `registry check: ... modules=7 disabled=0 conflicts=0` is present, and no line says `module ... disabled`.

Game checks:

- [ ] **Mods panel (Mods-Fenster):** "Kargarok Flight" is listed. Its text shows `Version 2.4.1`.
- [ ] **Call (Ruf R+B):** Hold R (R2), then tap B (Circle). The bird comes down above Link, grabs Link, and the ride starts.
- [ ] **Speed (Gas R/L, R2/L2):** Hold R (R2): the speed rises. Hold L (L2): the speed falls to a hover at 0 percent. D-Pad Up and D-Pad Down change the speed in steps of 20 percent.
- [ ] **Hover (Schweben):** Hold L until the bird stops in the air. It then hovers by itself. Push the Control Stick up to sink and down to climb. To fly on, hold R, tap D-Pad Up, or tap A.
- [ ] **Dash:** Tap A (Cross). The bird bursts ahead.
- [ ] **Landing (Landung):** Tap B (Circle). The bird slows down and sinks. Link lets go when the bird reaches the ground or water.
- [ ] **Quit:** Dusklight closes without a crash report.

Optional: barrel roll (Y), brake (Z), camera (X, D-Pad Left and Right), pause (Start).

If the bird does not start a ride, search the log for `call rejected`. The reason in brackets is explained in the "Log messages" section of the README.

## 6. Troubleshooting

- **`DUSKLIGHT_DIR fehlt`:** the `-DDUSKLIGHT_DIR=...` option is missing or empty.
- **`Mod SDK: failed to download`:** see the note on `-DDUSK_GAME_EXE` in section 3.
- **No compiler found, or `cl` not recognized:** you are not in a Developer PowerShell. Open it from the Start menu.
- **`ninja` not found:** install Ninja (section 1).
- **The mod is not active:** the log has no `kff: initialize` line. Check that the file is in the `mods` folder, that its name is unchanged, and that the copy finished while Dusklight was closed.
- **Build error in `src\`:** send the first 30 lines of the compiler output, starting at the first `error`.

## 7. Status and developer notes

- The Windows build is not yet tested on a Windows PC.
- The package uses the ModSDK convention `lib/<platform>/mod.dll`. For x64, `<platform>` is `windows-amd64`, not `windows-x86_64`.
- The mod uses the game headers from the Dusklight tree. Game data is accessed through `DUSK_GAME_DATA` (`__declspec(dllimport)` on Windows), which the headers already provide.
- Two hook targets use MSVC decorated symbol names (`#ifdef _MSVC_LANG` in `src\core\hooks.cpp`): `dComIfGp_setNextStage` (four-argument overload) and `updateAutoSave`. They are checked in the log (section 5).
