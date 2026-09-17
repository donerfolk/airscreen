/* THESIS: No plate, no card. The window is an ambient color field the iPhone's picture will replace, with the
 * receiver name set huge on it; it refuses the card-in-a-window settings panel.
 * OWN-WORLD: Muted dusk-toned state fields (blue waiting, amber pairing, green live, graphite failed) lit by two
 * slow blooms; white Segoe UI Semibold lockup lifted by soft tinted shadows; flat tiles that flood white when on;
 * a dark mode that drops every field toward midnight.
 * STORY: This PC is ready, this is the name iPhone lists, this is the route to it. A PIN turns the field amber,
 * freezes it, and takes the lockup.
 * FIRST VIEWPORT: Open field with the state sentence top-left. Bottom-left lockup: receiver name at ~11% of width,
 * route beneath. Rename plus three toggles along the floor.
 * FORM: Ambient Field, grounded candidate 3 of 7; seed aba659fb.
 * FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, and
 * DESIGN.md */

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyleRegular;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LineCapRound;
using Gdiplus::LineJoinRound;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::RectF;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::StringFormat;
using Gdiplus::UnitPixel;

namespace airscreen {
namespace {

enum class Mood { Wait, Pair, Live, Fail };

// One field per connection state: the ground itself says what is happening.
struct Field {
    Color top;
    Color bottom;
    Color bloom_a;
    Color bloom_b;
    Color ink; // label ink on a control flooded white
};

// Dusk tones: saturated enough to name the state, soft enough to sit beside a desk lamp all evening.
Field field_for(Mood mood) {
    switch (mood) {
    case Mood::Pair:
        return {Color(255, 110, 68, 24), Color(255, 42, 24, 10), Color(153, 217, 151, 74), Color(115, 184, 106, 90),
                Color(255, 62, 36, 14)};
    case Mood::Live:
        return {Color(255, 26, 80, 64), Color(255, 8, 32, 26), Color(133, 84, 178, 126), Color(92, 74, 134, 216),
                Color(255, 12, 52, 40)};
    case Mood::Fail:
        return {Color(255, 48, 48, 54), Color(255, 21, 21, 24), Color(82, 192, 90, 80), Color(128, 78, 78, 86),
                Color(255, 30, 30, 34)};
    default:
        return {Color(255, 28, 50, 114), Color(255, 11, 21, 54), Color(158, 74, 134, 216), Color(128, 109, 91, 196),
                Color(255, 19, 36, 90)};
    }
}

const Color c_label(255, 255, 255, 255);
const Color c_soft(214, 255, 255, 255); // secondary text is the field's own light, never gray

const wchar_t *pick_face(std::initializer_list<const wchar_t *> faces) {
    for (auto *name : faces) {
        FontFamily fam(name);
        if (fam.GetLastStatus() == Gdiplus::Ok) {
            return name;
        }
    }
    return L"Segoe UI";
}

const wchar_t *face_text() {
    static const wchar_t *face = pick_face({L"Segoe UI Variable Text", L"Segoe UI"});
    return face;
}

const wchar_t *face_strong() {
    static const wchar_t *face = pick_face({L"Segoe UI Variable Text Semibold", L"Segoe UI Semibold"});
    return face;
}

const wchar_t *face_display() {
    static const wchar_t *face = pick_face({L"Segoe UI Variable Display Semibold", L"Segoe UI Semibold"});
    return face;
}

StringFormat &line_format() {
    // Typographic metrics so text starts exactly at x; an ellipsis when a line meets its width. LineLimit is
    // cleared because it silently drops a display-size line whose layout box rounds a hair short.
    static StringFormat *fmt = [] {
        StringFormat *f = StringFormat::GenericTypographic()->Clone();
        f->SetFormatFlags((f->GetFormatFlags() & ~Gdiplus::StringFormatFlagsLineLimit) |
                          Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
        f->SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        return f;
    }();
    return *fmt;
}

float ascent(const Font &f) {
    FontFamily fam;
    f.GetFamily(&fam);
    return f.GetSize() * (float) fam.GetCellAscent(f.GetStyle()) / (float) fam.GetEmHeight(f.GetStyle());
}

float width_of(Graphics &g, const std::wstring &s, const Font &f) {
    RectF box;
    g.MeasureString(s.c_str(), (INT) s.size(), &f, PointF(0, 0), &line_format(), &box);
    return box.Width;
}

// One line with its baseline at `baseline`, trimmed to max_w. Returns the width used.
float draw_line(Graphics &g, const std::wstring &s, const Font &f, float x, float baseline, float max_w,
                const Color &c) {
    if (s.empty() || max_w <= 0) {
        return 0;
    }
    SolidBrush brush(c);
    RectF rc(x, baseline - ascent(f), max_w, f.GetHeight(&g) * 1.5f);
    g.DrawString(s.c_str(), (INT) s.size(), &f, rc, &line_format(), &brush);
    return (std::min)(width_of(g, s, f), max_w);
}

void add_round_rect(GraphicsPath &p, RectF r, float rad) {
    float d = rad * 2.f;
    if (d > r.Width) {
        d = r.Width;
    }
    if (d > r.Height) {
        d = r.Height;
    }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

void fill_round(Graphics &g, const RectF &r, float rad, const Color &c) {
    GraphicsPath p;
    add_round_rect(p, r, rad);
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

void draw_round(Graphics &g, const RectF &r, float rad, const Color &c, float w) {
    GraphicsPath p;
    add_round_rect(p, r, rad);
    Pen pen(c, w);
    g.DrawPath(&pen, &p);
}

bool drift_allowed() {
    BOOL on = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0);
    return on != FALSE;
}

// Written straight into the DIB: two soft blooms over a diagonal wash. Per-pixel math is several times faster
// than GDI+ gradient brushes plus a stretch, and lets the long gradients carry dither so they never band.
void fill_field(uint32_t *out, int w, int h, const Field &f, double t) {
    struct Bloom {
        float x, y, inv_r2, a, r, g, b;
    };
    auto bloom = [&](const Color &c, float u, float v, float radius) {
        float r = radius * (float) w;
        return Bloom{u * (float) w,         v * (float) h,         1.f / (r * r),        (float) c.GetA() / 255.f,
                     (float) c.GetR(), (float) c.GetG(), (float) c.GetB()};
    };
    const Bloom blooms[2] = {
        bloom(f.bloom_a, 0.80f + 0.07f * (float) sin(t * 0.11), 0.16f + 0.08f * (float) cos(t * 0.09), 0.62f),
        bloom(f.bloom_b, 0.28f + 0.08f * (float) cos(t * 0.07), 0.02f + 0.07f * (float) sin(t * 0.13), 0.46f),
    };
    // A 70-degree wash from top to bottom, as a projection onto that direction.
    float span = 0.342f * (float) w + 0.940f * (float) h;
    float kx = 0.342f / span;
    float ky = 0.940f / span;
    float r0 = f.top.GetR(), g0 = f.top.GetG(), b0 = f.top.GetB();
    float dr = f.bottom.GetR() - r0, dg = f.bottom.GetG() - g0, db = f.bottom.GetB() - b0;
    uint32_t noise = 0x9e3779b9u;
    for (int y = 0; y < h; ++y) {
        float dy2[2];
        for (int i = 0; i < 2; ++i) {
            float dy = (float) y - blooms[i].y;
            dy2[i] = dy * dy;
        }
        uint32_t *row = out + (size_t) y * (size_t) w;
        for (int x = 0; x < w; ++x) {
            float k = (float) x * kx + (float) y * ky;
            float r = r0 + dr * k, g = g0 + dg * k, b = b0 + db * k;
            for (int i = 0; i < 2; ++i) {
                const Bloom &bl = blooms[i];
                float dx = (float) x - bl.x;
                float d2 = (dx * dx + dy2[i]) * bl.inv_r2;
                if (d2 < 1.f) {
                    float q = (1.f - d2) * (1.f - d2) * bl.a;
                    r += (bl.r - r) * q;
                    g += (bl.g - g) * q;
                    b += (bl.b - b) * q;
                }
            }
            noise = noise * 1664525u + 1013904223u;
            float n = (float) (noise >> 24) / 127.5f - 0.5f; // about one level of dither
            auto to8 = [n](float v) { return (uint32_t) std::clamp((int) (v + n), 0, 255); };
            row[x] = 0xff000000u | (to8(r) << 16) | (to8(g) << 8) | to8(b);
        }
    }
}

// A soft drop shadow for whatever `draw` paints: rendered at quarter scale into an alpha mask (a blurred shadow
// has no fine detail to lose), box-blurred twice each way (close to a Gaussian), then laid into the DIB in `tint`.
template <class Draw>
void cast_shadow(uint32_t *out, int w, int h, const Color &tint, float offset, float blur, float strength,
                 Draw &&draw) {
    const int sw = (std::max)(1, w / 4);
    const int sh = (std::max)(1, h / 4);
    const float sx = (float) sw / (float) w;
    const float sy = (float) sh / (float) h;
    Bitmap mask(sw, sh, PixelFormat32bppPARGB);
    {
        Graphics mg(&mask);
        mg.Clear(Color(0, 0, 0, 0));
        mg.SetSmoothingMode(SmoothingModeAntiAlias);
        mg.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        mg.ScaleTransform(sx, sy);
        mg.TranslateTransform(0, offset);
        draw(mg);
    }

    std::vector<float> a((size_t) sw * (size_t) sh);
    Gdiplus::Rect all(0, 0, sw, sh);
    Gdiplus::BitmapData bd;
    if (mask.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Gdiplus::Ok) {
        return;
    }
    for (int y = 0; y < sh; ++y) {
        const BYTE *src = (const BYTE *) bd.Scan0 + (ptrdiff_t) y * bd.Stride;
        for (int x = 0; x < sw; ++x) {
            a[(size_t) y * sw + x] = (float) src[x * 4 + 3] / 255.f;
        }
    }
    mask.UnlockBits(&bd);

    const int r = (std::max)(1, (int) std::lround(blur * sx));
    const float norm = 1.f / (float) (2 * r + 1);
    std::vector<float> tmp(a.size());
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < sh; ++y) {
            const float *src = &a[(size_t) y * sw];
            float *dst = &tmp[(size_t) y * sw];
            float sum = 0;
            for (int i = -r; i <= r; ++i) {
                sum += src[std::clamp(i, 0, sw - 1)];
            }
            for (int x = 0; x < sw; ++x) {
                dst[x] = sum * norm;
                sum += src[(std::min)(x + r + 1, sw - 1)] - src[(std::max)(x - r, 0)];
            }
        }
        for (int x = 0; x < sw; ++x) {
            float sum = 0;
            for (int i = -r; i <= r; ++i) {
                sum += tmp[(size_t) std::clamp(i, 0, sh - 1) * sw + x];
            }
            for (int y = 0; y < sh; ++y) {
                a[(size_t) y * sw + x] = sum * norm;
                sum += tmp[(size_t) (std::min)(y + r + 1, sh - 1) * sw + x] -
                       tmp[(size_t) (std::max)(y - r, 0) * sw + x];
            }
        }
    }

    // Skip the empty field: only rows that hold shadow, only the columns any shadow reaches.
    const float eps = 0.004f;
    std::vector<char> row_live(sh, 0);
    int x0 = sw, x1 = -1;
    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            if (a[(size_t) y * sw + x] > eps) {
                row_live[y] = 1;
                x0 = (std::min)(x0, x);
                x1 = (std::max)(x1, x);
            }
        }
    }
    if (x1 < 0) {
        return;
    }
    const int X0 = (std::max)(0, (int) ((float) (x0 - 1) / sx));
    const int X1 = (std::min)(w, (int) ((float) (x1 + 2) / sx));
    const float tr = tint.GetR(), tg = tint.GetG(), tb = tint.GetB();
    for (int Y = 0; Y < h; ++Y) {
        float v = ((float) Y + 0.5f) * sy - 0.5f;
        int v0 = std::clamp((int) std::floor(v), 0, sh - 1);
        int v1 = (std::min)(v0 + 1, sh - 1);
        if (!row_live[v0] && !row_live[v1]) {
            continue;
        }
        float fv = std::clamp(v - (float) v0, 0.f, 1.f);
        const float *ra = &a[(size_t) v0 * sw];
        const float *rb = &a[(size_t) v1 * sw];
        uint32_t *row = out + (size_t) Y * (size_t) w;
        for (int X = X0; X < X1; ++X) {
            float u = ((float) X + 0.5f) * sx - 0.5f;
            int u0 = std::clamp((int) std::floor(u), 0, sw - 1);
            int u1 = (std::min)(u0 + 1, sw - 1);
            float fu = std::clamp(u - (float) u0, 0.f, 1.f);
            float s = (ra[u0] + (ra[u1] - ra[u0]) * fu) * (1.f - fv) + (rb[u0] + (rb[u1] - rb[u0]) * fu) * fv;
            if (s <= eps) {
                continue;
            }
            // Thin text blurs out to a faint mask; the gain keeps small lines shadowed without choking big ones.
            float k = (std::min)(1.f, s * 1.6f) * strength;
            uint32_t p = row[X];
            float pr = (float) ((p >> 16) & 255), pg = (float) ((p >> 8) & 255), pb = (float) (p & 255);
            pr += (tr - pr) * k;
            pg += (tg - pg) * k;
            pb += (tb - pb) * k;
            row[X] = 0xff000000u | ((uint32_t) pr << 16) | ((uint32_t) pg << 8) | (uint32_t) pb;
        }
    }
}

void round_pen(Pen &p) {
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
}

void icon_pencil(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    auto P = [&](float u, float v) { return PointF(b.X + u * b.Width, b.Y + v * b.Height); };
    PointF body[] = {P(0.16f, 0.84f), P(0.38f, 0.78f), P(0.91f, 0.25f), P(0.75f, 0.09f), P(0.22f, 0.62f)};
    g.DrawPolygon(&p, body, 5);
    g.DrawLine(&p, P(0.78f, 0.37f), P(0.63f, 0.22f));
}

void icon_expand(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    auto P = [&](float u, float v) { return PointF(b.X + u * b.Width, b.Y + v * b.Height); };
    PointF corners[4][3] = {{P(0.14f, 0.40f), P(0.14f, 0.14f), P(0.40f, 0.14f)},
                            {P(0.60f, 0.14f), P(0.86f, 0.14f), P(0.86f, 0.40f)},
                            {P(0.86f, 0.60f), P(0.86f, 0.86f), P(0.60f, 0.86f)},
                            {P(0.40f, 0.86f), P(0.14f, 0.86f), P(0.14f, 0.60f)}};
    for (auto &corner : corners) {
        g.DrawLines(&p, corner, 3);
    }
}

void icon_pin(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    auto P = [&](float u, float v) { return PointF(b.X + u * b.Width, b.Y + v * b.Height); };
    PointF body[] = {P(0.40f, 0.12f), P(0.40f, 0.44f), P(0.24f, 0.60f), P(0.76f, 0.60f), P(0.60f, 0.44f),
                     P(0.60f, 0.12f)};
    g.DrawLine(&p, P(0.34f, 0.12f), P(0.66f, 0.12f));
    g.DrawLines(&p, body, 6);
    g.DrawLine(&p, P(0.5f, 0.60f), P(0.5f, 0.90f));
}

void icon_lock(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    auto P = [&](float u, float v) { return PointF(b.X + u * b.Width, b.Y + v * b.Height); };
    draw_round(g, RectF(b.X + 0.20f * b.Width, b.Y + 0.44f * b.Height, 0.60f * b.Width, 0.46f * b.Height),
               0.12f * b.Width, c, w);
    g.DrawArc(&p, b.X + 0.32f * b.Width, b.Y + 0.12f * b.Height, 0.36f * b.Width, 0.40f * b.Height, 180, 180);
    g.DrawLine(&p, P(0.32f, 0.32f), P(0.32f, 0.44f));
    g.DrawLine(&p, P(0.68f, 0.32f), P(0.68f, 0.44f));
}

void icon_moon(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    // Crescent: the outer circle's arc outside a smaller offset circle, closed back along that circle's arc.
    GraphicsPath path;
    path.AddArc(b.X + 0.14f * b.Width, b.Y + 0.16f * b.Height, 0.72f * b.Width, 0.72f * b.Height, 14.2f, 248.5f);
    path.AddArc(b.X + 0.38f * b.Width, b.Y + 0.06f * b.Height, 0.60f * b.Width, 0.60f * b.Height, 221.1f, -165.4f);
    path.CloseFigure();
    g.DrawPath(&p, &path);
}

void chevron(Graphics &g, float x, float mid, float s, const Color &c, float w) {
    Pen p(c, w);
    round_pen(p);
    PointF pts[] = {PointF(x, mid - s), PointF(x + s * 0.6f, mid), PointF(x, mid + s)};
    g.DrawLines(&p, pts, 3);
}

void fill_round_grad(Graphics &g, const RectF &r, float rad, const Color &a, const Color &b, float angle) {
    GraphicsPath p;
    add_round_rect(p, r, rad);
    LinearGradientBrush br(r, a, b, angle);
    g.FillPath(&br, &p);
}

void draw_mark(Graphics &g, RectF box) {
    // Fallback window icon only; the shipped icon is src/app/app.ico.
    float s = box.Width;
    float body_r = s * 0.22f;
    fill_round_grad(g, box, body_r, Color(255, 58, 58, 60), Color(255, 36, 36, 38), 90.f);
    draw_round(g, box, body_r, Color(180, 96, 96, 100), (std::max)(1.f, s * 0.04f));

    float inset = s <= 18.f ? s * 0.16f : s * 0.14f;
    RectF screen(box.X + inset, box.Y + inset * 1.05f, s - inset * 2.f, s - inset * 2.15f);
    float scr_r = (std::min)(screen.Width, screen.Height) * 0.18f;
    fill_round_grad(g, screen, scr_r, Color(255, 236, 236, 240), Color(255, 196, 196, 204), 72.f);
}

HICON icon_from_size(int size) {
    Bitmap bmp(size, size, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    draw_mark(g, RectF(0, 0, (float) size, (float) size));
    HICON icon = nullptr;
    bmp.GetHICON(&icon);
    return icon;
}

} // namespace

ULONG_PTR gdiplus_startup() {
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &in, nullptr);
    return token;
}

void gdiplus_shutdown(ULONG_PTR token) {
    if (token) {
        Gdiplus::GdiplusShutdown(token);
    }
}

void ShellUi::startup() {
    ensure_icons();
}

void ShellUi::shutdown() {
    destroy_icons();
}

void ShellUi::ensure_icons() {
    if (!icon32_) {
        icon32_ = icon_from_size(32);
    }
    if (!icon16_) {
        icon16_ = icon_from_size(16);
    }
}

void ShellUi::destroy_icons() {
    if (icon32_) {
        DestroyIcon(icon32_);
        icon32_ = nullptr;
    }
    if (icon16_) {
        DestroyIcon(icon16_);
        icon16_ = nullptr;
    }
}

void ShellUi::paint(HWND hwnd, HDC hdc, RECT client, const UiView &view, int hover) {
    client_ = client;
    int w = client.right - client.left;
    int h = client.bottom - client.top;
    if (w < 2 || h < 2) {
        return;
    }
    dpi_ = hwnd ? (int) GetDpiForWindow(hwnd) : 96;
    if (dpi_ < 96) {
        dpi_ = 96;
    }
    auto D = [this](float v) { return v * (float) dpi_ / 96.f; };

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void *bits = nullptr;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bm) {
        DeleteDC(mem);
        return;
    }
    HGDIOBJ old = SelectObject(mem, bm);

    Mood mood = view.failed         ? Mood::Fail
                : !view.pin.empty() ? Mood::Pair
                : view.connected    ? Mood::Live
                                    : Mood::Wait;
    Field f = field_for(mood);
    if (view.dark) {
        // Dark mode: the same fields at about a third of their brightness with faint blooms, so waiting becomes a
        // deep midnight blue and the other states still read by hue.
        auto dim = [](Color &c, float k, float alpha) {
            c = Color((BYTE) (c.GetA() * alpha), (BYTE) (c.GetR() * k), (BYTE) (c.GetG() * k), (BYTE) (c.GetB() * k));
        };
        dim(f.top, 0.35f, 1.f);
        dim(f.bottom, 0.35f, 1.f);
        dim(f.bloom_a, 0.6f, 0.45f);
        dim(f.bloom_b, 0.6f, 0.45f);
    }
    // Waiting drifts free; a PIN or a failure holds the field still so the state reads at once.
    bool drift = (mood == Mood::Wait || mood == Mood::Live) && drift_allowed();
    fill_field((uint32_t *) bits, w, h, f, drift ? (double) GetTickCount64() / 1000.0 : 0.0);

    float m = std::clamp((float) w * 0.06f, D(32), D(72));
    float x = m;
    float avail = (float) w - m * 2.f;
    bool controls = !view.fullscreen && view.pin.empty();
    float row_h = D(40);
    float row_y = (float) h - m - row_h;
    float bottom = controls ? row_y - D(48) : (float) h - m; // the lockup stacks upward from this line
    name_rc_ = {};
    edit_rc_ = {};
    dark_rc_ = {};
    for (auto &r : btn_rc_) {
        r = {};
    }

    // Everything set straight on the field: the state sentence and the name (or PIN) lockup. It runs twice, once
    // into the shadow mask and once for real, so the shadow always sits under exactly what is drawn.
    auto field_text = [&](Graphics &g, bool real) {
        // The state sentence owns the top-left corner in every mood, so nothing ever sits above the name or PIN.
        if (real) {
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        }
        Font state_f(face_strong(), D(17), FontStyleRegular, UnitPixel);
        draw_line(g, view.pin.empty() ? view.status : L"Enter this PIN on your iPhone", state_f, x,
                  m + D(17) * 0.72f, controls ? avail - D(56) : avail, mood == Mood::Wait ? c_soft : c_label);

        if (real) {
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        }
        if (!view.pin.empty()) {
            // The PIN takes the lockup: digits sit on the floor line, spaced to be read back one at a time.
            const float track = 0.06f;
            auto spaced_width = [&](const Font &font, float size) {
                float t = 0;
                for (wchar_t ch : view.pin) {
                    t += width_of(g, std::wstring(1, ch), font) + size * track;
                }
                return t - size * track;
            };
            float px = std::clamp((std::min)((float) w * 0.16f, (float) h * 0.30f), D(64), D(220));
            {
                Font probe(face_display(), px, FontStyleRegular, UnitPixel);
                float tw = spaced_width(probe, px);
                if (tw > avail) {
                    px *= avail / tw;
                }
            }
            Font pin_f(face_display(), px, FontStyleRegular, UnitPixel);
            float cx = x;
            for (wchar_t ch : view.pin) {
                cx += draw_line(g, std::wstring(1, ch), pin_f, cx, bottom, avail, c_label) + px * track;
            }
            if (real) {
                edit_rc_ = {(LONG) x, (LONG) (bottom - px * 0.74f), (LONG) cx, (LONG) bottom};
            }
            return;
        }

        bool route = mood == Mood::Wait;
        float px = std::clamp((std::min)((float) w * 0.105f, (float) h * 0.20f), D(44), D(168));
        {
            Font probe(face_display(), px, FontStyleRegular, UnitPixel);
            float nw = width_of(g, view.name, probe);
            if (nw > avail) {
                px = (std::max)(D(36), px * avail / nw);
            }
        }
        Font name_f(face_display(), px, FontStyleRegular, UnitPixel);
        // Baselines stack upward: route on the line, then the name clearing its descenders.
        float name_base = route ? bottom - D(15) * 0.72f - D(22) - px * 0.22f : bottom - px * 0.22f;
        float name_right = x + draw_line(g, view.name, name_f, x, name_base, avail, c_label);
        if (real) {
            float pencil_s = std::clamp(px * 0.26f, D(18), D(34));
            if (hover == (int) UiHit::Name) {
                RectF ib(name_right + px * 0.16f, name_base - px * 0.36f - pencil_s * 0.5f, pencil_s, pencil_s);
                icon_pencil(g, ib, c_soft, (std::max)(D(1.6f), pencil_s * 0.09f));
            }
            name_rc_ = {(LONG) x, (LONG) (name_base - px * 0.74f), (LONG) (name_right + px * 0.16f + pencil_s),
                        (LONG) (name_base + px * 0.22f)};
            edit_rc_ = name_rc_;
        }

        if (route) {
            if (real) {
                g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            }
            Font route_f(face_text(), D(15), FontStyleRegular, UnitPixel);
            Font route_name_f(face_strong(), D(15), FontStyleRegular, UnitPixel);
            float right = x + avail;
            float mid = bottom - D(15) * 0.36f;
            float cx = x + draw_line(g, L"Control Center", route_f, x, bottom, avail, c_soft);
            auto step = [&] {
                cx += D(10);
                chevron(g, cx, mid, D(3.5f), c_soft, D(1.5f));
                cx += D(3.5f) * 0.6f + D(10);
            };
            step();
            cx += draw_line(g, L"Screen Mirroring", route_f, cx, bottom, right - cx, c_soft);
            step();
            draw_line(g, view.name, route_name_f, cx, bottom, right - cx, c_label);
        }
    };

    // Shadow in the field's own deep tone, dropped a few pixels: lift for the type, not a dark halo.
    Color shade((BYTE) (f.bottom.GetR() / 3), (BYTE) (f.bottom.GetG() / 3), (BYTE) (f.bottom.GetB() / 3));
    cast_shadow((uint32_t *) bits, w, h, shade, D(3), D(12), 0.55f, [&](Graphics &mg) { field_text(mg, false); });

    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        if (controls) {
            const wchar_t *labels[4] = {L"Rename", L"Full Screen", L"Always on Top", L"Require PIN"};
            bool on[4] = {false, view.fullscreen, view.always_on_top, view.require_pin};
            Font btn_f(face_strong(), D(13), FontStyleRegular, UnitPixel);
            float gap = D(8);
            float split = D(20); // Rename is an action; the toggles group apart from it
            float icon_s = D(16);
            float icon_gap = D(8);
            float pad = D(14);
            float label_w[4];
            for (int i = 0; i < 4; ++i) {
                label_w[i] = width_of(g, labels[i], btn_f);
            }
            bool icons = true;
            auto row_width = [&] {
                float t = gap * 2 + split;
                for (float lw : label_w) {
                    t += pad * 2 + (icons ? icon_s + icon_gap : 0) + lw;
                }
                return t;
            };
            if (row_width() > avail) {
                icons = false;
                pad = D(10);
            }

            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            float bx = x;
            for (int i = 0; i < 4; ++i) {
                float bw = pad * 2 + (icons ? icon_s + icon_gap : 0) + label_w[i];
                RectF br(bx, row_y, bw, row_h);
                bool hot = hover == (int) UiHit::Rename + i;
                float rad = D(12);
                Color ink = c_label;
                if (on[i]) {
                    // On floods the whole control: readable as filled, not by hue.
                    fill_round(g, br, rad, hot ? Color(232, 255, 255, 255) : c_label);
                    ink = f.ink;
                } else if (i == 0) {
                    if (hot) {
                        fill_round(g, br, rad, Color(36, 255, 255, 255));
                    }
                    draw_round(g, br, rad, Color(120, 255, 255, 255), D(1.25f));
                } else {
                    fill_round(g, br, rad, Color(hot ? 70 : 40, 255, 255, 255));
                }
                float cx = br.X + pad;
                if (icons) {
                    RectF ib(cx, row_y + (row_h - icon_s) * 0.5f, icon_s, icon_s);
                    float sw = D(1.6f);
                    if (i == 0) {
                        icon_pencil(g, ib, ink, sw);
                    } else if (i == 1) {
                        icon_expand(g, ib, ink, sw);
                    } else if (i == 2) {
                        icon_pin(g, ib, ink, sw);
                    } else {
                        icon_lock(g, ib, ink, sw);
                    }
                    cx += icon_s + icon_gap;
                }
                draw_line(g, labels[i], btn_f, cx, row_y + row_h * 0.5f + D(13) * 0.35f, label_w[i] + 2, ink);
                btn_rc_[i] = {(LONG) br.X, (LONG) br.Y, (LONG) br.GetRight(), (LONG) br.GetBottom()};
                bx += bw + (i == 0 ? split : gap);
            }

            // Dark mode: a square tile in the top-right corner, centred on the state sentence, flooded when on.
            RectF dr((float) w - m - row_h, m + D(17) * 0.36f - row_h * 0.5f, row_h, row_h);
            bool dark_hot = hover == (int) UiHit::Dark;
            if (view.dark) {
                fill_round(g, dr, D(12), dark_hot ? Color(232, 255, 255, 255) : c_label);
            } else {
                fill_round(g, dr, D(12), Color(dark_hot ? 70 : 40, 255, 255, 255));
            }
            icon_moon(g, RectF(dr.X + (row_h - icon_s) * 0.5f, dr.Y + (row_h - icon_s) * 0.5f, icon_s, icon_s),
                      view.dark ? f.ink : c_label, D(1.6f));
            dark_rc_ = {(LONG) dr.X, (LONG) dr.Y, (LONG) dr.GetRight(), (LONG) dr.GetBottom()};
        }

        field_text(g, true);
    }

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
}

UiHit ShellUi::hit_test(POINT pt) const {
    if (PtInRect(&name_rc_, pt)) {
        return UiHit::Name;
    }
    if (PtInRect(&btn_rc_[0], pt)) {
        return UiHit::Rename;
    }
    if (PtInRect(&btn_rc_[1], pt)) {
        return UiHit::Fullscreen;
    }
    if (PtInRect(&btn_rc_[2], pt)) {
        return UiHit::Topmost;
    }
    if (PtInRect(&btn_rc_[3], pt)) {
        return UiHit::Pin;
    }
    if (PtInRect(&dark_rc_, pt)) {
        return UiHit::Dark;
    }
    return UiHit::None;
}

bool ShellUi::cursor_hand(UiHit hit) const {
    return hit != UiHit::None;
}

} // namespace airscreen
