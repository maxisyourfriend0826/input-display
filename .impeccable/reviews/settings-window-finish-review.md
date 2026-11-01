# Finish review: settings window (2026-10-31)

Reviewer: impeccable-finish-reviewer. Disposition: **fix**. Evidence: r1_top, r2_input_mspeed, r3_lit, scroll1, b, p3 (all pass check 0). This is a code-led build, so the sketch is a reference, not a comp. Line numbers refer to app/settings_window.cpp as of the review.

## Fidelity
Everything matches the contract:
- **TYPE:** M PLUS Rounded 1c.
- **MATERIAL:** real LCD pixel cells.
- **GROUND:** #e9f0f7.
- **SIGNATURE:** the LCD tile lights orange.
- **M-speed state:** the pill and the 3px rule.
- **Orange:** used only for M-speed.
- **Panels:** the topology matches.
- **Controls:** segmented pills, whole-cell grids, and a fixed right column for values.

Accepted adaptations:
- The band is a rounded square, because of the per-axis deadzone rule.
- The view is zoomed onto the deadzone, and the dot is pinned at the edge when the stick goes past it.
- The Edit layout button is deferred.
- The cut settings are absent.

## Material fixes (in order)
1. **Button set order and captions.**
   - The grid reads 1, 10, 11, 12, 2… (around lines 685-699).
   - Build a display order `std::vector<int> order(nSets)`, sorted by the numeric folder name with lexical order as the fallback. Draw cell k with `i = order[k]`, and pass `selected` as the position of `s.buttonSetIndex` in `order`. On a click, set `s.buttonSetIndex = order[pick]`.
   - This keeps the persisted index meaning, so no migration is needed.
2. **Swatch rows can't be read without hovering.**
   - Split "Ring colors" (around line 663) into an "Outer ring" row (fill, outline) and an "Inner ring" row (fill, outline).
   - Show the swatch tooltip on keyboard focus as well (around line 208): `IsItemHovered() || (IsItemFocused() && io.NavVisible)`.
3. **Hotkey and "restart" hints share one mark.**
   - In label(), draw `F<n>` keys as a keycap: a 1px kMuted rounded rect, radius 4, with 4px horizontal padding.
   - Change "restart" to "applies after restart", unboxed (around lines 748 and 764).
   - Widen `keyW` by the keycap padding in rowSegmented (around line 259).
4. **Hints baked into subheads.**
   - Give sub() an optional `hint` parameter, drawn in F->small kMuted, with the keycap form for F7.
   - Call `sub("Layout", "F7")` and `sub("Values", "stick units, stored /128")`.
5. **Ghost opacity shows a raw byte.**
   - Slide a percentage instead: `pct = (v*100+127)/255` over 0-100 with format "%d%%". When it changes, write back `v = (pct*255+50)/100`.
6. **Disclosure rows look disabled.**
   - Draw "Ring sizes" and "Advanced" in kNavy at rest.
   - Show hover as a kHover row fill across the panel width, not as a colour swap.

## Ceiling (optional, not blockers)
- **Motion:** add one authored moment, a short ease when M-speed lights.
- **LCD zoom:** change `lcdView_` from 1.6 to about 1.25 so the signature reads at a glance.
- **LCD dot:** draw it as a 2px-cell block, so it matches the pixel idiom.
- **Sliders:** add a navy fill left of the grab.
- **Display studio summary:** show live state, e.g. "Clock · Set 12 · Sonic · Diamond right".

## Keep
- The true pad-space band, lighting together with the pill and the rule.
- Orange for M-speed alone; navy for every other chosen state.
