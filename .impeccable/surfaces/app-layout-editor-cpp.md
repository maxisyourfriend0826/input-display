---
version: 1
slug: "app-layout-editor-cpp"
primary_target: "app/layout_editor.cpp"
related_targets: []
---

# Layout editor (app/layout_editor.cpp)

Mode: Operate. Users: a runner arranging their overlay once (sometimes again for a new scene). Task: move, resize, show/hide the overlay's elements on a fixed-size canvas, snap them, save as a custom layout. Answers (2026-11-01): the real overlay updates live while editing (Cancel restores); drag plus X/Y/W/H fields; Ctrl+Z / Ctrl+Y undo. Constraints: separate native window, Dear ImGui on SDL3, OFL fonts only, presets read-only (Duplicate makes an editable copy), custom layouts in layouts.user.json.

## Direction contract

FORM: Layers rail + inspector by the selection (dealt lead, position 3 of 7 on the grounded list). seed:15f91642b2bf3fdd. Sketch: .impeccable/mocks/decision/editor/editor-layers.html (1180x760).

THESIS: The editor is the settings window's sibling: the same pale console menu, with the canvas as the one dark object on it. It refuses the dark ImGui dock with property grids.

OWN-WORLD: Inherited from the settings window: ground #e9f0f7, white 12px panels, navy #1d3557 text, chosen states and primary pill, muted #4f6a8a, M PLUS Rounded 1c 800 headers / 400-500 body, pill buttons and pill toggles. The canvas is the overlay's own navy #0a205b on a dot-grid white stage. Orange stays M-speed only (the ring lights orange when M-speed is on, nowhere else). Snap guides are light blue dashes.

STORY: You see your overlay at 150% with every element outlined; pick one in the list or on the canvas, drag it, read its numbers beside it, Save.

FIRST VIEWPORT: Top bar (60px): "Layout editor", name tag "<name> · W × H", Undo, Redo, Snap pill, Canvas size, Cancel, Save layout (primary). Left rail 250px: groups Stick / Face buttons / Shoulders and triggers / D-pad / Extras, rows with an icon chip, name, a live-press dot and a show/hide pill; the selected row is navy-filled; hidden rows struck. Stage: the canvas centered, selection outline with corner handles, a white floating card beside the selection with X Y W H fields and Show, placed on the free side.

SIGNATURE: The canvas is driven live: buttons fill when pressed, the stick dot moves, the ring lights orange in M-speed, while you edit.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance
