# Input Display 5

A controller input overlay for speedrunners. It shows the stick, buttons and
triggers live, lights up when the stick is in a game-specific zone (M-speed in
Sonic Generations), and has a transparent background for OBS.

This is a rebuild of Gens Input Display 4.43 (itself based on TurtleMan64's
Sonic Input Display v2.2). It reads your old `settings.ini` and uses the same
button sets and toons.

## What changed from 4.43

- **Builds anywhere with one command.** CMake downloads pinned, checksummed
  copies of SDL3, Dear ImGui, stb and nlohmann/json. No DLLs, no Visual Studio
  project, no hand-copied libraries. One ~3 MB executable plus `assets/`.
- **M-speed is checked on every controller report**, not once per drawn frame.
  A hit that lasts a single report is shown on the next frame and then cleared.
  Nothing is held longer unless you set "Extra hold after a hit".
- **No phantom hits.** X and Y from the same report are combined before
  checking, so the display never tests a position the stick never had.
- **Same M-speed answer as 4.43.** A test compares the new code with a copy of
  the 4.43 `speedCheck` on all 554,743,809 stick positions that can hit. The
  results match exactly.
- **Thresholds live in a profile file** (`assets/profiles/*.json`). A new game
  or sensitivity setting is a new file, and you can edit regions live in the
  settings window and save them.
- **Layouts live in `assets/layouts.json`**, so a new arrangement needs no
  recompile.
- **The drawn stick is exact by default.** 4.43 always smoothed the dot; that
  is now an option and off by default.
- **Windows: reads Xbox pads through XInput**, the same API the game uses. Can
  be turned off in settings.

## Build

Needs CMake 3.24+ and a C++20 compiler (Visual Studio 2022, GCC 11+, Clang 14+).

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release      # unit tests + the 4.43 equivalence check
```

Linux also needs the usual SDL3 development packages; see
`.github/workflows/build.yml` for the exact list.

The app is `build/Release/InputDisplay.exe` (Windows) or `build/InputDisplay`,
with `assets/` copied next to it. Put your old `settings.ini` beside the
executable to keep your settings.

`InputDisplay --demo` adds a virtual controller that circles the M-speed band.
Use it to check the display or preview skins without a controller.

## Hotkeys (overlay window focused)

| Key | Action |
|---|---|
| F1 | Coordinates |
| F2 | Toon |
| F3 | M-speed light mode |
| F4 | D-pad moves the stick |
| F5 | Next ring shape |
| F6 | Ghost buttons |
| F7 | Next layout |
| F8 | Draw the game's stick value (read from game memory) instead of the pad's |
| Left / Right | Button set |
| Up / Down | Toon |
| M | Settings window |

## OBS

- **Windows:** Game Capture → "Capture specific window" → `Controller Input`,
  and tick **Allow Transparency**. The background colour you pick is what you
  see; OBS sees it as transparent.
- **macOS / Linux:** tick "See-through window" in settings (restart needed) and
  use a window capture. If your capture method drops transparency, set a
  solid background colour and add a Chroma Key filter. (Not yet tested on
  macOS/Linux.)

## Reading the game's own stick value (Windows)

Input Display can read the stick value the game itself stored, the same
read-only way LiveSplit autosplitters read game memory. Supported games are
JSON files in `assets/games/` (currently Sonic X Shadow Generations, Steam exe
built 2024-12-05).

- **F8** switches the drawn stick between the pad and the game. If the game
  isn't running, the overlay keeps drawing the pad and says
  `pad (game not found)` in the top-left corner.
- The M-speed light follows whichever stick is drawn, and each uses its own
  rule:
  - **Pad:** the v4.43 boxes from the rule profile (measured in pad units).
  - **Game:** one ring on the value the game stored: M-speed when the
    distance from centre is between *ring min* and *ring max* (default
    0.03..0.077). This is a hypothesis under test; see
    `../mspeed-research/NOTES.md`.
- **Settings → Game stick → Apply deadzone to drawn game stick** (on by
  default) adds the game's deadzone back for drawing only, so the game stick
  lines up with the pad: `p = |g| × (1 - d) + d` per axis, with d = 0.265. Inside
  the deadzone the game stores 0, so the dot stays centred until the stick
  leaves it. It doesn't change the M-speed check.
- **Settings → Game probe** shows the game, its exact build, and the pad and
  game values side by side. **Log pad + game values to CSV** writes
  `probe-<date>-<time>.csv` next to the exe: one row whenever either value
  changes, with every value the game definition lists.
- A game update changes the build ID; Input Display then refuses to read
  rather than read the wrong memory, and the Game probe shows the new
  build ID to add to the definition.
- If the game runs as administrator, Input Display must too.

## Layout of the code

```
core/      input normalization, rule regions, settings schema, layouts (no SDL, unit tested)
app/       SDL3 window, drawing (ported from 4.43), ImGui settings window, main loop
tests/     unit tests + legacy_v443.hpp, a verbatim copy of the 4.43 math used as reference
assets/    sets/, toons/, profiles/, games/ (game memory definitions), layouts.json, font
```

To add a setting, add a member to `Settings` and a row to `settingsSchema()`.
Loading, saving and the settings UI pick it up from there.

## Credits

Kyrrone (M/D Speed template), Khroma (updated template, 100 sensitivity
testing), Starlight (Remastered button prompts), Ranbin and Avikaielef
(Unleashed button sets), TurtleMan64 (original Sonic Input Display).

The bundled font `ARLRDBD.TTF` (Arial Rounded MT Bold) came with 4.43. It is a
commercial font; replace it with an openly licensed one before distributing
builds.
