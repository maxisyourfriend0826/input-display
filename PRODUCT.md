# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop (native window, Dear ImGui on SDL3; Windows first, Linux/macOS planned). Not web.

## Users

- **Runners setting up:** Sonic speedrunners (Sonic X Shadow Generations first) who capture the overlay in OBS. They pick a skin, layout and colors once, then leave it running beside the game.
- **The developer (the user, leaderboard moderator):** uses the game probe, deadzone and ring numbers for M-speed research.
- Moderators are not a primary audience of the settings window (confirmed 2026-10-31).

## Product Purpose

A controller input overlay that shows stick, buttons and triggers live, and lights up exactly when the stick is in the M-speed band, so viewers and runners can see M-speed happen. Success: pixel-accurate stick, a correct M-speed light, and a setup a runner finishes in minutes.

## Positioning

Reads the game's own stick value from memory and checks M-speed with the rule the game uses (one ring in game space after a 0.265 per-axis deadzone), instead of hand-tuned pad boxes. Based on TurtleMan64's Sonic Input Display v2.2 / v4.43.

## Operating Context

- Two windows: the overlay (captured in OBS, transparent background) and the settings window (opened with M).
- Hotkeys: F4 d-pad mode, F5 shape, F7 layout, F8 game/pad source.
- Used beside a running game, often while streaming; settings changes must show on the overlay immediately.

## Capabilities and Constraints

- M-speed: one ring, game value distance 0.03..0.077; the pad goes through g = (|p| - d)/(1 - d) per axis first. The v4.43 boxes are removed from the app (kept only in tests).
- Cut in v5.1: rule profile picker, pulse rate/amplitude, stick smoothing.
- Planned v5.1: resizable scaling overlay; layout editor on the overlay itself (edit mode, drag buttons, snapping, show/hide, 5 presets, saved layouts); toggleable "game not found" note.
- Read-only process memory access (the game uses Denuvo; never attach a debugger).
- ARLRDBD.TTF is a commercial font: do not ship it as a new dependency.

## Brand Commitments

Product name: Gens Input Display. Button sets and toon art in assets/ belong to the existing skins.

## Evidence on Hand

Probe logs (build/Release/probe-*.csv) and E:/.inputdisplay/mspeed-research/NOTES.md. No testimonials or user counts exist; never invent them.

## Product Principles

1. Accuracy first: what's drawn and lit matches the game.
2. Set up once, then get out of the way.
3. Changes show live on the overlay.
4. Research tools stay available but out of a runner's path.
