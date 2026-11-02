# Gens Input Display 5.0
Based on TurtleMan64's Sonic Input Display v2.2.

Gens Input Display is a controller input overlay for Sonic Generations
speedrunners. It shows your stick, buttons, triggers and d-pad live, lights up
when your stick is in the M-Speed zone, and has a transparent background so you
can put it straight onto your stream in OBS.

5.0 is a full rebuild of Gens Input Display 4.43. It looks the same out of the
box, uses the same button sets, and reads your old settings.ini, but nearly
everything else is new.


## Getting started

1. Unzip the folder anywhere and run SonicInputDisplay.exe.
2. Plug in your controller. Xbox controllers are read the same way the game
   reads them.
3. To open the settings, hover over the overlay and click the gear in the top
   right corner.
4. Set your stick sensitivity to 50 in-game for the M-Speed light, same as
   before.

Every change in the settings window shows on the overlay right away. Click
"Save settings" at the bottom to keep it for next time.

Hotkeys are gone in 5.0. Everything they used to do is now in the settings
window, so you can't toggle something by accident mid-run.


## What's new in 5.0

### More accurate M-Speed light
- The M-Speed check is now a single ring around the stick's center, measured
  against what the game itself does. In testing it matched the game.
- Your stick is checked every time the controller reports (up to about 1000
  times a second) instead of once per drawn frame, so even a very short M-Speed
  hit shows up.
- "Keep lit after a hit" lets you hold the light on a little longer if a hit is
  too quick to see.
- The deadzone and ring sizes can be adjusted under M-Speed > Advanced if you
  ever need to.

### Reading the game's own stick value
- The display can read the stick value Sonic X Shadow Generations (Steam)
  stores, so you see exactly what the game sees. Pick "Game value" under
  Settings > Input.
- It only reads, the same way LiveSplit autosplitters do, and it never changes
  anything in the game.
- If the game isn't open, the display just keeps showing your controller. It can
  show a small "no supported games detected" note if you'd like to know.
- If the game gets an update, the display stops reading it instead of showing
  wrong values.
- If you run the game as administrator, run the display as administrator too.

### A new settings window
- Practice: a live preview of your stick with the M-Speed zone drawn on it,
  and the M-Speed light, so you can practice the input. Click it to open it.
- Display studio: ring shape and colors, stick dot, ghost buttons, button set,
  layout, coordinates and the window.
- Input: pad or game value, D-pad moves the stick, and which controller is
  connected.
- M-Speed: turn the light on or off, and the advanced ring settings.
- Research: shows your controller and the game's values side by side, and can
  log them to a CSV file next to the exe.

### Layouts you can edit
- Five ready-made layouts: Buttons left, Buttons right, Split with the ring
  centered, Diamond right and Diamond left.
- Click "Duplicate and edit" on any of them to make your own. In the layout
  editor you can:
  - drag and resize every button, the ring, the triggers and the coordinates
  - snap to a grid or line things up with each other, or hold Alt to place
    freely
  - type exact positions and sizes
  - hide anything you don't use
  - undo and redo with Ctrl+Z and Ctrl+Y
  - change the canvas size
- Add your own PNG images: a background, a frame, a custom ring design. They're
  copied into the images folder next to the exe, and you can move them above or
  below everything else with the layer buttons.
- Show the right stick too. Each stick can have its own shape, and you can pick
  which ones light up for M-Speed.
- Your layouts are saved in layouts.user.json next to the exe.

### Resizable overlay
- Drag the edge of the overlay window to make it any size you want. It scales
  evenly, and it stays crisp at 200%, 300% and other whole sizes.
- Display studio > Window shows the current size, with a "Reset to 100%"
  button.

### Shapes, colors and the rest
- 8 ring shapes: Rays, Rays + cross, Octagon, Plain, Ring, Clock, Dot only, and
  Target, a new design the community asked for.
- "Dot only" hides the ring and shows just the stick dot, which is handy
  together with your own ring image.
- The drawn stick shows exactly where your stick is. Older versions smoothed it
  out a little.
- "Keep the drawn stick inside the ring" and "Draw stick dots above images" are
  both on by default and can be turned off.
- Ghost buttons now show their opacity as a percentage.

### Removed
- Toons have been removed.
- Hotkeys have been removed (see Getting started).


## OBS

### Windows
1. Add a Game Capture source.
2. Set Mode to "Capture specific window" and pick "Controller Input".
3. Tick "Allow Transparency".

The background color you pick is only for you to see. OBS sees it as
transparent, so there's nothing to key out.

### Transparent window
Turn on "Transparent Window" in Display studio > Window and restart the display.
The window itself becomes see-through on your desktop, which also helps with
capture methods that don't support "Allow Transparency". If yours still shows a
background, pick a solid background color and add a Chroma Key filter.


## Button sets
Button sets live in assets/sets/, one numbered folder each. Pick one in Display
studio > Button set. To add your own, make a new folder in assets/sets/ with
images that use the same file names (buttA.png, buttB.png and so on).


# Credits

Kyrrone: Original creator of the M/D Speed template
https://www.youtube.com/@Kyrrone

Starlight: The remaster button prompts were taken from the Remastered GUI Mod
https://www.youtube.com/@StarlightDX

Khroma: Creating the updated M/D Speed template and testing 100 stick sensitivity mode.
https://www.youtube.com/@SlavicSnowball

TurtleMan64: Original Sonic Input Display v2.2
https://github.com/TurtleMan64/SADX-SA2-Input-Display/releases/tag/v2.2

Ranbin: Unleashed Button Set
https://www.youtube.com/@R_A_N_B_I_N

Avikaielef: Unleashed Button Sets
https://www.youtube.com/@Avikaielef

Built with SDL3, Dear ImGui, stb and nlohmann/json, and uses the M PLUS Rounded
1c font. Their licenses are in THIRD-PARTY-NOTICES.txt.


# Change Log

<5.0>   - Full rebuild (see What's new in 5.0)
        - New M-Speed ring check that matches the game, checked on every controller report
        - Reads the game's own stick value (Sonic X Shadow Generations, Steam)
        - New settings window, layout editor, custom images, right stick and resizable overlay
        - New ring shapes: Dot only and Target
        - Removed toons and hotkeys
<4.43>  - Button shadows, button layouts and more ring shapes
        - M-Speed thresholds can be changed per direction in the menu
<4.0>   - Massive overhaul
        - D-pad and triggers separated from the stick and shoulder buttons
<2.3>   - Fixed crash with Coordinates switch
        - Capped framerate to help with CPU usage
        - Adjusted MSpeed equations to include edge cases
<2.2>   - Fixed massive memory leak
        - Added MSpeed equations for both regular and 100 Stick Sensitivity Mode


# For the technical folks

Made in C++20 with SDL3, Dear ImGui, stb_image and nlohmann/json. CMake
downloads pinned, checksummed copies of all of them, so there are no DLLs or
hand-copied libraries: one ~3 MB exe plus the `assets/` folder.

## Build

Needs CMake 3.24+ and a C++20 compiler (Visual Studio 2022, GCC 11+, Clang 14+).

```sh
cmake -S . -B build                      # first time only: sets up build/ and downloads the libraries
cmake --build build --config Release     # every time after a change
ctest --test-dir build -C Release        # unit tests
```

The app is `build/Release/InputDisplay.exe` on Windows (`build/InputDisplay`
elsewhere), with `assets/` copied next to it. Close the app before rebuilding,
or Windows can't replace the exe. Linux also needs the usual SDL3 development
packages; see `.github/workflows/build.yml` for the list.

`InputDisplay --demo` adds a virtual controller that circles the M-Speed zone.

## How the M-Speed check works

- One ring on the game's stick value: M-Speed when the distance from center is
  between *ring min* and *ring max* (default 0.030 and 0.077), after the game's
  per-axis deadzone (default 0.265).
- The controller's value goes through the same deadzone first, so the pad and
  the game value use one rule. The old 4.43 boxes are kept only in the tests.
- The check runs on every controller report, not once per frame, and X and Y
  from the same report are checked together, so it never tests a position the
  stick never had.
- Stick sensitivity changes whether M-Speed happens in-game. The defaults are
  for sensitivity 50; other sensitivities are still being measured.

## Reading the game's stick value (Windows)

- Supported games are JSON files in `assets/games/` (currently Sonic X Shadow
  Generations, Steam exe built 2024-12-05). The display opens the game with
  read-only access and follows pointer paths like a Cheat Engine table or a
  LiveSplit autosplitter.
- Each definition pins the exe's build ID. After a game update the display
  refuses to read, and the Research panel shows the new build ID to add.
- "Draw the game value with the deadzone added back" (on by default) maps the
  game value back to pad space for drawing only: `p = |g| × (1 - d) + d` per
  axis.
- Research > "Pad and game values to CSV" writes `probe-<date>-<time>.csv` next
  to the exe, one row whenever either value changes.

## Files next to the exe

- `settings.ini`: all settings (4.43 files load; unknown keys are kept).
- `layouts.user.json`: your custom layouts. Preset layouts are in
  `assets/layouts.json`.
- `images/`: PNGs added in the layout editor.

## Layout of the code

```
core/      input, M-Speed rule, settings, layouts, game definitions (no SDL, unit tested)
app/       SDL3 window, drawing, settings window, layout editor, game memory, main loop
tests/     unit tests + legacy_v443.hpp, a verbatim copy of the 4.43 math used as reference
assets/    sets/, profiles/, games/, layouts.json, fonts/
```

To add a setting, add a member to `Settings` and a row to `settingsSchema()`.
Loading, saving and the settings UI pick it up from there.

## Licenses

`THIRD-PARTY-NOTICES.txt` holds the licenses of SDL3, Dear ImGui, stb,
nlohmann/json and the M PLUS Rounded 1c font. The build copies it next to the
exe; keep it with every release. 5.0 replaced the commercial Arial Rounded font
that 4.43 used with M PLUS Rounded 1c.
