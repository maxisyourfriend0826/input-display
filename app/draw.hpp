// Pixel drawing helpers, ported from v4.43 so the overlay looks the same.
#pragma once
#include <SDL3/SDL.h>

namespace draw {

void line(SDL_Renderer* r, int x1, int y1, int x2, int y2);
void filledCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
void circleOutline(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
// Uses the renderer's current draw color.
void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness);
void filledQuad(SDL_Renderer* r, const SDL_Point pts[4], SDL_Color c);
void thickLineRect(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness, SDL_Color c);
void ring(SDL_Renderer* r, int cx, int cy, int outerRadius, int thickness, SDL_Color fill, SDL_Color outline);
void octagon(SDL_Renderer* r, int cx, int cy, int outerRadius, int thickness, SDL_Color c, SDL_Color outline);
void annulusCross(SDL_Renderer* r, int cx, int cy, int ringOuterRadius, int ringThickness,
                  int thickness, SDL_Color c, float angleDegrees);
// Pixels whose centers lie between rIn and rOut from (cx, cy) and, measured
// counterclockwise from +x with y up, between a0 and a1 degrees (a0 < a1;
// 0..360 is a full ring). Crisp edges, like the other shapes.
void annulusSector(SDL_Renderer* r, int cx, int cy, float rIn, float rOut, float a0, float a1, SDL_Color c);
void plusCross(SDL_Renderer* r, int cx, int cy, int length, int thickness, SDL_Color c);

} // namespace draw
