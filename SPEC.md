# Input Display: spec

Updated: 2026-11-01

## Purpose

An input display for speedrunners that shows **exactly** what they are doing,
both while they play and in their recordings: stick, buttons, triggers, and
game-specific zones like M-speed. It should work for any game on Windows,
macOS and Linux.

Success means the Sonic Generations community (2011 and 2024 versions) can
trust it, both for practice and as supporting evidence in leaderboard
submissions.

## Decisions so far

- First game: **Sonic X Shadow Generations (2024)**. Sonic Generations (2011)
  and other games must be addable without changing code.
- Two stick values where the game allows it: **controller** (what the pad
  sends, read through SDL) and **game** (what the game itself stored, read from
  its memory). The user can display either one. M-speed must work on both;
  testing will show which one matches what happens in-game.
- Memory reading is read-only, the same technique as LiveSplit autosplitters.
  DLL hooking is a separate experimental project (2011 version first).
- Recordings carry a hash chain for tamper evidence (see below).

## Adding a game: game definition files

Each game is one JSON file in `assets/games/`. Contributors find the values
with Cheat Engine and submit the file. No code changes.

```json
{
  "id": "sonic-x-shadow-generations",
  "name": "Sonic X Shadow Generations",
  "process": "SonicXShadowGenerations.exe",
  "profile": "sxsg-default",
  "versions": [
    {
      "label": "Steam 2024-10 (example)",
      "match": { "timeDateStamp": "0x00000000", "sizeOfImage": "0x00000000" },
      "values": {
        "gameStickX": { "path": ["SonicXShadowGenerations.exe+0x0", "0x0"], "type": "float", "scale": 128 },
        "gameStickY": { "path": ["SonicXShadowGenerations.exe+0x0", "0x0"], "type": "float", "scale": 128 },
        "frame":      { "path": ["SonicXShadowGenerations.exe+0x0"], "type": "u32" }
      }
    }
  ]
}
```

- **process**: the executable to look for.
- **versions[].match**: identifies the exact game build from two fields in the
  exe header. Each game update gets its own entry, so an old definition never
  silently reads garbage from a new build.
- **values**: pointer paths in Cheat Engine's notation (module + offset, then
  offsets to follow). `type` is the stored type; `scale` converts the value to
  stick units (-128..128). Optional `frame` is the game's frame counter, used
  to record one row per game frame.
- **profile**: the default rule profile (M-speed regions) for this game.
- Later: byte-pattern scanning ("AOB") as an alternative to fixed offsets, so a
  definition can survive small game updates.

**Contributor workflow:**
1. Find the stick value in Cheat Engine: move the stick, scan for a changing
   float; then find a stable pointer path.
2. Open the display's **Game probe** panel, paste the path, and watch the live
   game value next to the controller value.
3. When they track, save, and submit the JSON (pull request or Discord).

## Recording format

One `.idrec` file per run: a JSON header line, then one CSV row per game frame
(or per controller report if the game has no frame counter).

```
{"app":"5.1.0","game":"sonic-x-shadow-generations","version":"Steam 2024-10","profile":"sxsg-default","profileSha256":"…","start":"2026-11-02T19:04:11Z"}
frame,ctrlX,ctrlY,gameX,gameY,buttons,rules,chain
1042,0,0,0.00,0.00,0x0000,0,3f9a…
1043,9984,-9984,39.00,39.00,0x0001,1,b21c…
```

- `chain` = SHA-256(previous chain + this row). Changing, removing or
  reordering any row breaks every chain value after it.
- **The overlay shows the first 6 characters of the current chain value** in a
  corner, updated every second. They appear in the run video, so a moderator
  can check that the file matches the video at any timestamp. The file can't
  be edited after the fact without the codes in the video no longer matching.
- **Limit (stated to the community):** someone who modifies the display program
  itself could fake both the file and the on-screen codes live. The chain makes
  after-the-fact edits detectable and ties the file to the video. It is strong
  supporting evidence, not proof on its own.
- A small verifier (in-app and command line) recomputes the chain and lists the
  codes at video timestamps.

## Increments (re-plan after each)

1. ✅ v5 core rebuild: SDL3, exact v4.43 M-speed, data-driven layouts and profiles.
2. Real-pad and OBS check by the user; push and CI for Linux/macOS.
3. ✅ (2026-10-31) Windows game-memory reader; game definition loader with
   exact-build match; Game probe panel with CSV logging; F8 draws the game
   value. SXSG stick X/Y found and pinned. Not done: frame counter, a
   generic `InputSource` abstraction (not needed yet).
4. Display choice (controller / game value); verify M-speed against both in
   SXSG; SXSG rule profile.
5. Recording with hash chain plus on-screen code; verifier.
6. Sonic Generations (2011) definition; contributor guide.
7. Later: AOB scanning, more games, the hooking experiment (separate project).

## Next: v5.1 "clean up + display studio" (requested 2026-10-31)

User testing (2026-10-31) found the game-space ring M-speed rule matches the
game. Requirements from the user, to be refined in a grill + Impeccable
session before building:

**M-speed and sensitivity**
- The ring becomes the default and only M-speed rule. Remove the rule-region
  boxes and their UI. Keep the ring min/max configurable (advanced).
- For the pad source the ring needs the deadzone: g = (|p| - d) / (1 - d) per
  axis, then the ring check. (Open: keep the v4.43 boxes anywhere, e.g. as a
  hidden legacy option or only in tests?)
- New setting: in-game **stick sensitivity** (default 50). It sets the
  deadzone. The deadzone slider stays, with a "link to stick sensitivity"
  button that sets it automatically. If the sensitivity can be found in game
  memory, read it automatically (slider still overrides).
- Sonic Generations 2011 and Sonic Unleashed use the same idea: a later
  version (v2), with testing.

**Overlay**
- "pad (game not found)" becomes a toggle in the menu.
- Every settings change shows live on the overlay as you change it (colors,
  ring sizes, and so on).
- The overlay window can be resized; buttons, rings and text scale with it.
- Layouts: keep the 5 built-in ones (diamond, split, left, right, ...) as
  presets. Users can drag buttons anywhere, save their layout, and choose
  which buttons are shown. Optional snapping with even spacing.

**Settings window**
- Redesign it with the Impeccable process. A collapsible **Display** "studio"
  section holds the display toggles, assets (button set, toon), window
  settings and the layout editor. The window may be bigger.
- Drop settings that the ring rule makes unnecessary.

## v5.2 "display studio + layout editor" (agreed 2026-10-31, grill round 1)

Status of v5.1: the ring rule is the default (user-confirmed in game), the
rule regions are gone, and settings already update the overlay live.

**Decisions**
- **Sensitivity (Q1):** no slider or link button for now. The user tests
  sensitivity 0 and 100: does the overlay light still match real M-speed? Only
  if the ring moves do we add a "Stick sensitivity" setting (plus a memory read
  if an address is found).
- **M-speed panel (Q2):** "Light up the overlay" (F3), and under Advanced the
  ring min/max and the deadzone (measured, 0.265). Glow colors and "keep lit
  after a hit" move to Display studio. "Add the deadzone back to the game
  value" moves to Research.
- **Menu (Q3):** Display studio (open by default; everything visual: shape,
  colors, sizes, button set, toon, layouts, overlay window, overlay text
  toggles), Input, M-speed, Research (collapsed).
- **Layout editor (Q4):** a separate window. The Layout section shows the 5
  preset cards, custom cards and a "+ Custom layout" card. The editor draws a
  simplified pad driven by the live stick and buttons.
- **Storage (Q5):** presets are read-only; "Duplicate" makes an editable copy.
  Custom layouts go in a user file next to settings.ini (`layouts.user.json`);
  rename and delete are supported.
- **Resizing (Q6):** each layout has its own canvas size (presets 420x300),
  set in the editor. The overlay scales the layout uniformly to its window
  size, keeping proportions; the leftover space is transparent.
- **Elements (Q7):** the stick (ring or custom image, dot, line); A, B, X, Y,
  L, R, LT, RT, Start, Select, L3, R3, Up, Down, Left, Right; the toon; the
  coordinates text. Each has a show/hide toggle and can be moved and resized.
  A button the icon set lacks isn't drawn.
- **Snapping (Q8):** snap to grid (spacing, default 8 px) and align to other
  elements with guides; Alt moves freely; arrows nudge 1 px, Shift+arrow by
  the grid step.
- **Custom shape (Q9), revised 2026-11-01:** not a Shape option. Instead
  each layout can hold any number of PNG **images** (copied into `images/`
  next to settings.ini), placed, resized and layered in the editor; plus a
  **"Dot only"** shape (no ring, just the dot and line) so an image can be
  the ring.
- **Layers (2026-11-01):** every element sits in one layer order (default,
  bottom first: images, buttons, right stick, left stick, coordinates, toon).
  The card has Bottom / Down / Up / Top. Setting "Draw stick dots above
  images and the toon" (on by default) keeps dots and lines above all layers;
  off, they draw with their ring.
- **Right stick (2026-11-01):** a second stick element, hidden in presets.
  Each stick has "Show M-speed" (the right one runs the ring rule on its own
  pad value) and a Shape override ("Match settings" or any shape).
- **Target shape (2026-11-01):** 8th shape from a community reference: blue
  ring with a black inner edge, offset quarter arcs, black cross, red plus.
  Same diameter as the regular ring; fixed colors.
- **Game-not-found note (Q10):** a Display studio toggle, off by default.
- **Codex (Q12):** a review after slices 3 and 4.

**Slices (Q11, re-plan after each)**
1. Menu cleanup: Q2, Q3, Q10.
2. Resizable overlay (presets only).
3. Layout editor, minimum version: move, resize, show/hide, snap, save
   custom layouts.
4. ✅ (2026-11-01) Images + layers + right stick + "Dot only" (replaced the custom shape idea).
5. Sensitivity, only if the Q1 test shows it matters.
The parked settings-window polish items (LCD zoom etc.) fit after slice 1.
The editor's look goes through the Impeccable choice step before slice 3.

**Update 2026-10-31 (probe data):** the in-game stick sensitivity does not
change the deadzone; it is 0.2648 per axis at 0, 25, 50 and 100. The
"link to stick sensitivity" idea is on hold until we know whether
sensitivity moves the M-speed ring instead (see
../mspeed-research/NOTES.md).

**Update 2026-11-01:** the user confirmed in game that sensitivity changes
whether M-speed happens, so slice 5 is on. It waits for the user's
measurements (ring min/max at several sensitivities) to find the formula.
Slice 2 (resizable overlay) is done: the 420x300 canvas scales uniformly to
the window, centered, with background-colored (alpha 0) space around it;
the window size is saved (`overlayWidth`/`overlayHeight`, 0 = canvas size),
and Display studio > Window shows the size with "Reset to 100%".
