---
version: 1
slug: "app-settings-window-cpp"
primary_target: "app/settings_window.cpp"
related_targets: []
---

# Settings window (app/settings_window.cpp)

Mode: Operate. Users: runners setting up once; the developer's research. Task: pick skin, layout, colors; see M-speed state; research tools out of the way. Constraints: Dear ImGui on SDL3, native window, OFL fonts only, live overlay updates.

## Direction contract

FORM: Console System Menu (assigned, chosen by the user 2026-10-31). seed:b75ac4b2083d23db. Sketch: .impeccable/mocks/decision/assigned.html (560x820).

THESIS: The settings window is a console's own system menu: pale, friendly, chunky rounded type, readable at a glance beside a running game. It refuses the default dark ImGui dock with dropdowns everywhere.

OWN-WORLD: Pale blue ground #e9f0f7, white rounded panels (12px), navy #1d3557 for text, open headers and chosen states. Orange #f08a24 is reserved for M-speed (ring, state pill); text on orange uses #b35500. Muted text #4f6a8a. M PLUS Rounded 1c (OFL), 800 for headers, 400/500 body. Segmented pills and cell grids instead of combos.

STORY: The top tile shows the stick live, so you see M-speed working before touching anything; Display studio is open first because that's the setup job; Input, M-speed, Research stay collapsed with one-line summaries.

SIGNATURE: A memory-card style LCD tile (#b9c9a6, inset #8fa37c) drawing the live stick dot, deadzone square and the M-speed ring lighting orange.

RAISES: orange for one job only; button sets and toons picked as a whole-cell grid; values in one fixed right column; states as distinct marks (outlined rest, filled chosen, struck unavailable).

RISK: Light window next to dark OBS; keep the LCD tile functional, not a costume. No Sega logos or swirl.
