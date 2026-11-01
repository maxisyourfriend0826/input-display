#include "draw.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace draw {

static constexpr float kPi = 3.14159265358979323846f;

static void setColor(SDL_Renderer* r, SDL_Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }

void line(SDL_Renderer* r, int x1, int y1, int x2, int y2) {
    SDL_RenderLine(r, (float)x1, (float)y1, (float)x2, (float)y2);
}

void filledCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    setColor(r, c);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)std::sqrt((float)(radius * radius - dy * dy));
        line(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void circleOutline(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    setColor(r, c);
    int x = radius, y = 0, decision = 1 - x;
    while (y <= x) {
        const SDL_FPoint pts[8] = {
            { (float)(cx + x), (float)(cy + y) }, { (float)(cx + y), (float)(cy + x) },
            { (float)(cx - x), (float)(cy + y) }, { (float)(cx - y), (float)(cy + x) },
            { (float)(cx - x), (float)(cy - y) }, { (float)(cx - y), (float)(cy - x) },
            { (float)(cx + x), (float)(cy - y) }, { (float)(cx + y), (float)(cy - x) },
        };
        SDL_RenderPoints(r, pts, 8);
        y++;
        if (decision <= 0) {
            decision += 2 * y + 1;
        } else {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
}

void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness) {
    if (thickness <= 1) {
        line(r, x1, y1, x2, y2);
        return;
    }
    float angle = std::atan2((float)(y2 - y1), (float)(x2 - x1));
    int half = thickness / 2;
    for (int offset = -half; offset <= half; offset++) {
        int ox = (int)(-offset * std::sin(angle));
        int oy = (int)(offset * std::cos(angle));
        line(r, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
    }
}

void annulusSector(SDL_Renderer* r, int cx, int cy, float rIn, float rOut, float a0, float a1, SDL_Color c) {
    setColor(r, c);
    const bool full = a1 - a0 >= 360.0f;
    const int ext = (int)std::ceil(rOut) + 1;
    std::vector<SDL_FPoint> pts;
    for (int y = -ext; y < ext; y++)
        for (int x = -ext; x < ext; x++) {
            const float px = x + 0.5f, py = y + 0.5f;  // pixel center; symmetric around (cx, cy)
            const float d = std::sqrt(px * px + py * py);
            if (d < rIn || d > rOut) continue;
            if (!full) {
                float a = std::atan2(-py, px) * 180.0f / kPi;  // y up
                while (a < a0) a += 360.0f;  // into [a0, a0 + 360)
                if (a > a1) continue;
            }
            pts.push_back({ (float)(cx + x), (float)(cy + y) });
        }
    if (!pts.empty()) SDL_RenderPoints(r, pts.data(), (int)pts.size());
}

void filledQuad(SDL_Renderer* r, const SDL_Point pts[4], SDL_Color c) {
    setColor(r, c);
    int minY = pts[0].y, maxY = pts[0].y;
    for (int i = 1; i < 4; i++) {
        minY = std::min(minY, pts[i].y);
        maxY = std::max(maxY, pts[i].y);
    }
    std::vector<float> xs;
    for (int y = minY; y <= maxY; y++) {
        xs.clear();
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            int y1 = pts[i].y, y2 = pts[j].y;
            if ((y >= y1 && y <= y2) || (y >= y2 && y <= y1)) {
                if (y1 == y2) {
                    xs.push_back((float)pts[i].x);
                    xs.push_back((float)pts[j].x);
                } else {
                    float t = (float)(y - y1) / (float)(y2 - y1);
                    xs.push_back(pts[i].x + t * (pts[j].x - pts[i].x));
                }
            }
        }
        if (xs.size() >= 2) {
            std::sort(xs.begin(), xs.end());
            line(r, (int)std::ceil(xs.front()), y, (int)std::floor(xs.back()), y);
        }
    }
}

void thickLineRect(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness, SDL_Color c) {
    float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len == 0) return;
    float pdx = -dy / len, pdy = dx / len;
    float half = thickness / 1.5f;
    SDL_Point q[4] = {
        { (int)std::round(x1 + pdx * half), (int)std::round(y1 + pdy * half) },
        { (int)std::round(x2 + pdx * half), (int)std::round(y2 + pdy * half) },
        { (int)std::round(x2 - pdx * half), (int)std::round(y2 - pdy * half) },
        { (int)std::round(x1 - pdx * half), (int)std::round(y1 - pdy * half) },
    };
    filledQuad(r, q, c);
}

static void ringBand(SDL_Renderer* r, int cx, int cy, int outerRadius, int innerRadius) {
    for (int dy = -outerRadius; dy <= outerRadius; dy++) {
        int dxOuter = (int)std::sqrt((float)(outerRadius * outerRadius - dy * dy));
        int dxInner = 0;
        if (std::abs(dy) < innerRadius) dxInner = (int)std::sqrt((float)(innerRadius * innerRadius - dy * dy));
        if (dxOuter > dxInner) {
            line(r, cx - dxOuter, cy + dy, cx - dxInner, cy + dy);
            line(r, cx + dxInner, cy + dy, cx + dxOuter, cy + dy);
        }
    }
}

void ring(SDL_Renderer* r, int cx, int cy, int outerRadius, int thickness, SDL_Color fill, SDL_Color outline) {
    int innerRadius = outerRadius - thickness - 1;
    setColor(r, outline);
    ringBand(r, cx, cy, outerRadius, innerRadius);
    setColor(r, fill);
    ringBand(r, cx, cy, outerRadius, innerRadius + 1);
    circleOutline(r, cx, cy, outerRadius, outline);
}

void octagon(SDL_Renderer* r, int cx, int cy, int outerRadius, int thickness, SDL_Color c, SDL_Color outline) {
    const int n = 8;
    SDL_Point outer[n], innerOutline[n], outerOutline[n];
    float offset = thickness / 2.0f + 2.0f;
    for (int i = 0; i < n; i++) {
        float a = i * (2.0f * kPi / n);
        outer[i] = { cx + (int)std::round(outerRadius * std::cos(a)), cy + (int)std::round(outerRadius * std::sin(a)) };
        innerOutline[i] = { cx + (int)std::round((outerRadius - 1) * std::cos(a)), cy + (int)std::round((outerRadius - 1) * std::sin(a)) };
        outerOutline[i] = { cx + (int)std::round(outerRadius * std::cos(a) + offset * std::cos(a)),
                            cy + (int)std::round(outerRadius * std::sin(a) + offset * std::sin(a)) };
    }
    setColor(r, outline);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        line(r, innerOutline[i].x, innerOutline[i].y, innerOutline[j].x, innerOutline[j].y);
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        thickLineRect(r, outer[i].x, outer[i].y, outer[j].x, outer[j].y, thickness, c);
    }
    for (int i = 0; i < n; i++) filledCircle(r, outer[i].x, outer[i].y, thickness / 2, c);
    setColor(r, outline);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        line(r, outerOutline[i].x, outerOutline[i].y, outerOutline[j].x, outerOutline[j].y);
    }
}

void annulusCross(SDL_Renderer* r, int cx, int cy, int ringOuterRadius, int ringThickness,
                  int thickness, SDL_Color c, float angleDegrees) {
    float theta = angleDegrees * (kPi / 180.0f);
    float inner = (float)(ringOuterRadius - ringThickness), outer = (float)ringOuterRadius;
    auto ray = [&](float a) {
        int xs = (int)std::round(inner * std::cos(a)), ys = (int)std::round(inner * std::sin(a));
        int xe = (int)std::round(outer * std::cos(a)), ye = (int)std::round(outer * std::sin(a));
        thickLineRect(r, cx + xs, cy + ys, cx + xe, cy + ye, thickness, c);
        thickLineRect(r, cx - xs, cy - ys, cx - xe, cy - ye, thickness, c);
    };
    ray(theta);
    ray(-theta);
}

void plusCross(SDL_Renderer* r, int cx, int cy, int length, int thickness, SDL_Color c) {
    // Black outline, square end caps, then the plus on top (v4.43 "second crosshair").
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    thickLine(r, cx - length, cy, cx + length, cy, thickness + 2);
    thickLine(r, cx, cy - length, cx, cy + length, thickness + 2);
    float cap = (float)(thickness + 2);
    const SDL_FRect caps[4] = {
        { (float)(cx - length - 1), cy - (float)((thickness + 2) / 2), 2, cap },
        { (float)(cx + length), cy - (float)((thickness + 2) / 2), 2, cap },
        { cx - (float)((thickness + 2) / 2), (float)(cy - length - 1), cap, 2 },
        { cx - (float)((thickness + 2) / 2), (float)(cy + length), cap, 2 },
    };
    SDL_RenderFillRects(r, caps, 4);
    setColor(r, c);
    thickLine(r, cx - length, cy, cx + length, cy, thickness);
    thickLine(r, cx, cy - length, cx, cy + length, thickness);
}

} // namespace draw
