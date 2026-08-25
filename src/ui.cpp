/* Idle window: a full-window color field that shows receiver state (waiting, pairing, connected, failed),
 * the receiver name, and the toolbar. Mirrored video replaces it once the first frame arrives. */

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "ui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

using Gdiplus::Bitmap;
using Gdiplus::Brush;
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleBold;
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
using Gdiplus::StringAlignment;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringAlignmentFar;
using Gdiplus::StringAlignmentNear;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsNoWrap;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

namespace airscreen {
namespace {

Color c_field(255, 10, 10, 12);      // #0a0a0c — near-black so the plate floats
Color c_plate(255, 44, 44, 46);      // #2c2c2e
Color c_plate_hi(255, 58, 58, 60);   // #3a3a3c
Color c_capsule(255, 28, 28, 30);     // #1c1c1e — recessed into the plate
Color c_rim(255, 142, 142, 147);      // #8e8e93 — readable glass lip
Color c_line(255, 72, 72, 74);        // #48484a
Color c_text(255, 245, 245, 247);     // #f5f5f7
Color c_muted(255, 134, 134, 139);    // #86868b — quieter secondary
Color c_on_ink(255, 255, 255, 255);
Color c_blue(255, 10, 132, 255);      // #0a84ff
Color c_live(255, 48, 209, 88);
Color c_wait(255, 142, 142, 147);
Color c_pair(255, 255, 159, 10);
Color c_fail(255, 255, 69, 58);

int dip(int v, int dpi) {
    return MulDiv(v, dpi, 96);
}

const wchar_t *ui_display() {
    static const wchar_t *face = nullptr;
    if (!face) {
        const wchar_t *try_faces[] = {L"SF Pro Display", L"SF Pro", L"Segoe UI Variable Display", L"Segoe UI"};
        for (auto *name : try_faces) {
            Font probe(name, 12.f, FontStyleRegular, UnitPixel);
            if (probe.GetLastStatus() == Gdiplus::Ok) {
                face = name;
                break;
            }
        }
        if (!face) {
            face = L"Segoe UI";
        }
    }
    return face;
}

const wchar_t *ui_display_medium() {
    static const wchar_t *face = nullptr;
    if (!face) {
        const wchar_t *try_faces[] = {L"SF Pro Display Medium", L"SF Pro Display Semibold", L"SF Pro Display",
                                      L"Segoe UI Variable Display Semibold", L"Segoe UI Variable Display", L"Segoe UI"};
        for (auto *name : try_faces) {
            Font probe(name, 12.f, FontStyleRegular, UnitPixel);
            if (probe.GetLastStatus() == Gdiplus::Ok) {
                face = name;
                break;
            }
        }
        if (!face) {
            face = L"Segoe UI";
        }
    }
    return face;
}

const wchar_t *ui_text() {
    static const wchar_t *face = nullptr;
    if (!face) {
        const wchar_t *try_faces[] = {L"SF Pro Text", L"SF Pro", L"Segoe UI Variable Text", L"Segoe UI Variable",
                                      L"Segoe UI"};
        for (auto *name : try_faces) {
            Font probe(name, 12.f, FontStyleRegular, UnitPixel);
            if (probe.GetLastStatus() == Gdiplus::Ok) {
                face = name;
                break;
            }
        }
        if (!face) {
            face = L"Segoe UI";
        }
    }
    return face;
}

void add_round_rect(GraphicsPath &p, RectF r, float rad) {
    float d = rad * 2.f;
    if (d > r.Width) {
        d = r.Width;
    }
    if (d > r.Height) {
        d = r.Height;
    }
    rad = d / 2.f;
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

void draw_shadow(Graphics &g, RectF r, float rad) {
    // Visible Continuity float: stacked umbra, readable on near-black field.
    for (int i = 14; i >= 1; --i) {
        RectF s = r;
        float t = (float) i;
        s.Y += t * 1.6f;
        s.X -= t * 0.35f;
        s.Width += t * 0.7f;
        s.Height += t * 0.95f;
        BYTE a = (BYTE) (std::min)(52, 8 + i * 3); // ~11..50
        fill_round(g, s, rad + t * 0.7f, Color(a, 0, 0, 0));
    }
}

void fill_round_grad(Graphics &g, const RectF &r, float rad, const Color &a, const Color &b, float angle) {
    GraphicsPath p;
    add_round_rect(p, r, rad);
    LinearGradientBrush br(r, a, b, angle);
    g.FillPath(&br, &p);
}

void draw_mark(Graphics &g, RectF box) {
    // Continuity glass: plate-color body, pale frosted screen, hairline rim.
    float s = box.Width;
    float body_r = s * 0.22f;
    fill_round_grad(g, box, body_r, Color(255, 58, 58, 60), Color(255, 36, 36, 38), 90.f);
    draw_round(g, box, body_r, Color(180, 96, 96, 100), (std::max)(1.f, s * 0.04f));

    float inset = s <= 18.f ? s * 0.16f : s * 0.14f;
    RectF screen(box.X + inset, box.Y + inset * 1.05f, s - inset * 2.f, s - inset * 2.15f);
    float scr_r = (std::min)(screen.Width, screen.Height) * 0.18f;
    fill_round_grad(g, screen, scr_r, Color(255, 236, 236, 240), Color(255, 196, 196, 204), 72.f);
    // Top glass catch
    GraphicsPath scr;
    add_round_rect(scr, screen, scr_r);
    g.SetClip(&scr);
    LinearGradientBrush wash(RectF(screen.X, screen.Y, screen.Width, screen.Height * 0.45f),
                             Color(90, 255, 255, 255), Color(0, 255, 255, 255),
                             Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&wash, RectF(screen.X, screen.Y, screen.Width, screen.Height * 0.45f));
    g.ResetClip();
}

void icon_pencil(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    float x = b.X, y = b.Y, s = b.Width;
    g.DrawLine(&p, x + 0.18f * s, y + 0.82f * s, x + 0.72f * s, y + 0.28f * s);
    PointF tip[3] = {PointF(x + 0.72f * s, y + 0.28f * s), PointF(x + 0.86f * s, y + 0.14f * s),
                     PointF(x + 0.78f * s, y + 0.42f * s)};
    g.DrawLines(&p, tip, 3);
    g.DrawLine(&p, x + 0.14f * s, y + 0.70f * s, x + 0.30f * s, y + 0.86f * s);
}

void icon_display(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    RectF screen(b.X + b.Width * 0.12f, b.Y + b.Height * 0.10f, b.Width * 0.76f, b.Height * 0.58f);
    draw_round(g, screen, b.Width * 0.12f, c, w);
    float cx = b.X + b.Width * 0.5f;
    g.DrawLine(&p, cx, b.Y + b.Height * 0.70f, cx, b.Y + b.Height * 0.82f);
    g.DrawLine(&p, b.X + b.Width * 0.32f, b.Y + b.Height * 0.86f, b.X + b.Width * 0.68f, b.Y + b.Height * 0.86f);
}

void icon_pin(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    float cx = b.X + b.Width * 0.5f;
    float head = b.Width * 0.34f;
    g.DrawEllipse(&p, cx - head * 0.5f, b.Y + b.Height * 0.10f, head, head);
    g.DrawLine(&p, cx, b.Y + b.Height * 0.44f, cx, b.Y + b.Height * 0.90f);
    g.DrawLine(&p, cx - b.Width * 0.18f, b.Y + b.Height * 0.58f, cx + b.Width * 0.18f, b.Y + b.Height * 0.58f);
}

void icon_lock(Graphics &g, RectF b, const Color &c, float w) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    RectF body(b.X + b.Width * 0.22f, b.Y + b.Height * 0.42f, b.Width * 0.56f, b.Height * 0.46f);
    draw_round(g, body, b.Width * 0.10f, c, w);
    g.DrawArc(&p, b.X + b.Width * 0.30f, b.Y + b.Height * 0.12f, b.Width * 0.40f, b.Height * 0.48f, 180, 180);
}

void draw_tracked_center(Graphics &g, const std::wstring &s, const Font &f, RectF rc, Brush &br, float track) {
    if (s.empty()) {
        return;
    }
    const StringFormat *typo = StringFormat::GenericTypographic();
    std::vector<float> adv(s.size());
    float total = 0.f;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t ch[2] = {s[i], 0};
        RectF box;
        g.MeasureString(ch, 1, &f, PointF(0, 0), typo, &box);
        adv[i] = box.Width * track;
        total += adv[i];
    }
    float x = rc.X + (rc.Width - total) * 0.5f;
    float y = rc.Y + (rc.Height - f.GetHeight(&g)) * 0.5f;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t ch[2] = {s[i], 0};
        g.DrawString(ch, 1, &f, PointF(x, y), typo, &br);
        x += adv[i];
    }
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

void ShellUi::paint(HWND hwnd, HDC hdc, RECT client, const UiView &view, int hover, float pulse) {
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

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bm = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ old = SelectObject(mem, bm);

    Graphics g(mem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.Clear(c_field);

    float margin = (float) dip(24, dpi_);
    RectF plate(margin, margin, (float) w - margin * 2.f, (float) h - margin * 2.f);
    if (plate.Width < 200) {
        plate.Width = (float) w * 0.92f;
        plate.X = ((float) w - plate.Width) * 0.5f;
    }
    if (plate.Height < 160) {
        plate.Height = (float) h * 0.88f;
        plate.Y = ((float) h - plate.Height) * 0.5f;
    }
    screen_rc_ = {(LONG) plate.X, (LONG) plate.Y, (LONG) (plate.X + plate.Width), (LONG) (plate.Y + plate.Height)};

    float rad = (float) dip(28, dpi_);
    draw_shadow(g, plate, rad);
    // Plate body: slight vertical lift so frost has a base tone to sit on.
    fill_round_grad(g, plate, rad, Color(255, 52, 52, 54), Color(255, 38, 38, 40), 90.f);

    GraphicsPath clip;
    add_round_rect(clip, plate, rad);
    g.SetClip(&clip);
    // Frost: full-height fade so glass reads without a hard top band.
    LinearGradientBrush sheen(plate, Color(95, 255, 255, 255), Color(0, 255, 255, 255),
                              Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&sheen, plate);
    float edge_h = (float) dip(28, dpi_);
    LinearGradientBrush edge(RectF(plate.X, plate.Y, plate.Width, edge_h), Color(140, 255, 255, 255),
                             Color(0, 255, 255, 255), Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&edge, RectF(plate.X, plate.Y, plate.Width, edge_h));
    Pen lip(Color(80, 255, 255, 255), 1.25f);
    g.DrawLine(&lip, plate.X + rad * 0.45f, plate.Y + 1.25f, plate.X + plate.Width - rad * 0.45f,
               plate.Y + 1.25f);
    g.ResetClip();
    draw_round(g, plate, rad, c_rim, 1.35f);

    float pad = (float) dip(36, dpi_);
    float inner_l = plate.X + pad;
    float inner_r = plate.X + plate.Width - pad;
    float inner_t = plate.Y + (float) dip(22, dpi_);
    float inner_b = plate.Y + plate.Height - pad;
    float inner_w = inner_r - inner_l;

    StringFormat near_f;
    near_f.SetAlignment(StringAlignmentNear);
    near_f.SetLineAlignment(StringAlignmentCenter);
    near_f.SetFormatFlags(StringFormatFlagsNoWrap);
    StringFormat far_f;
    far_f.SetAlignment(StringAlignmentFar);
    far_f.SetLineAlignment(StringAlignmentCenter);
    far_f.SetFormatFlags(StringFormatFlagsNoWrap);
    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);
    center.SetLineAlignment(StringAlignmentCenter);
    center.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush text_b(c_text);
    SolidBrush muted_b(c_muted);

    Font mark_f(ui_text(), (float) dip(13, dpi_), FontStyleRegular, UnitPixel);
    RectF mark_rc(inner_l, inner_t, inner_w * 0.45f, (float) dip(28, dpi_));
    g.DrawString(L"AirScreen", -1, &mark_f, mark_rc, &near_f, &muted_b);

    const wchar_t *pill_label = L"Waiting";
    Color pill_dot = c_wait;
    if (view.failed) {
        pill_label = L"Failed";
        pill_dot = c_fail;
    } else if (!view.pin.empty()) {
        pill_label = L"Pairing";
        pill_dot = c_pair;
    } else if (view.connected) {
        pill_label = L"Live";
        pill_dot = c_live;
    }
    float pill_h = (float) dip(28, dpi_);
    float pill_w = (float) dip(108, dpi_);
    RectF pill(inner_r - pill_w, inner_t + (mark_rc.Height - pill_h) * 0.5f, pill_w, pill_h);
    fill_round(g, pill, pill_h * 0.5f, Color(255, 36, 36, 38));
    draw_round(g, pill, pill_h * 0.5f, Color(160, 72, 72, 74), 1.f);
    float dot = (float) dip(7, dpi_);
    float dx = pill.X + (float) dip(12, dpi_);
    float dy = pill.Y + (pill.Height - dot) * 0.5f;
    BYTE a = (BYTE) (160 + 95 * pulse);
    SolidBrush glow(Color((BYTE) (40 + 50 * pulse), pill_dot.GetR(), pill_dot.GetG(), pill_dot.GetB()));
    g.FillEllipse(&glow, dx - 3.5f, dy - 3.5f, dot + 7.f, dot + 7.f);
    SolidBrush dot_b(Color(a, pill_dot.GetR(), pill_dot.GetG(), pill_dot.GetB()));
    g.FillEllipse(&dot_b, dx, dy, dot, dot);
    Font pill_f(ui_text(), (float) dip(12, dpi_), FontStyleRegular, UnitPixel);
    RectF pill_tx(dx + dot + (float) dip(6, dpi_), pill.Y, pill.GetRight() - (dx + dot + dip(10, dpi_)), pill.Height);
    g.DrawString(pill_label, -1, &pill_f, pill_tx, &near_f, &text_b);

    bool show_capsules = !view.fullscreen && view.pin.empty();
    float cap_h = show_capsules ? (float) dip(44, dpi_) : 0.f;
    float cap_gap = (float) dip(10, dpi_);
    float cap_y = inner_b - cap_h;

    if (!view.pin.empty()) {
        Font pin_f(ui_display(), (float) (std::max)(dip(64, dpi_), (int) (inner_w / 5.6f)), FontStyleRegular, UnitPixel);
        RectF pin_rc(inner_l, plate.Y + plate.Height * 0.24f, inner_w, (float) dip(100, dpi_));
        g.DrawString(view.pin.c_str(), -1, &pin_f, pin_rc, &center, &text_b);
        Font hint(ui_text(), (float) dip(17, dpi_), FontStyleRegular, UnitPixel);
        RectF hint_rc(inner_l, pin_rc.GetBottom() + (float) dip(14, dpi_), inner_w, (float) dip(28, dpi_));
        g.DrawString(L"Enter this PIN on your iPhone", -1, &hint, hint_rc, &center, &muted_b);
        name_rc_ = {};
        edit_rc_ = {(LONG) pin_rc.X, (LONG) pin_rc.Y, (LONG) pin_rc.GetRight(), (LONG) hint_rc.GetBottom()};
    } else {
        float name_px = (float) (std::clamp)((int) (inner_w / 7.0f), dip(40, dpi_), dip(84, dpi_));
        FontStyle name_style = FontStyleRegular;
        const wchar_t *name_face = ui_display_medium();
        if (wcsstr(name_face, L"Medium") == nullptr && wcsstr(name_face, L"Semibold") == nullptr) {
            name_style = FontStyleBold;
        }
        Font name_f(name_face, name_px, name_style, UnitPixel);
        float name_h = name_px * 1.28f;
        float name_y = plate.Y + (plate.Height - name_h) * 0.38f;
        RectF name_area(inner_l, name_y, inner_w, name_h);
        Color name_c = (hover == (int) UiHit::Name) ? c_on_ink : c_text;
        SolidBrush name_b(name_c);
        draw_tracked_center(g, view.name, name_f, name_area, name_b, 0.90f);
        name_rc_ = {(LONG) name_area.X, (LONG) name_area.Y, (LONG) name_area.GetRight(),
                    (LONG) name_area.GetBottom()};
        edit_rc_ = name_rc_;

        Font sub(ui_text(), (float) dip(17, dpi_), FontStyleRegular, UnitPixel);
        RectF sub_rc(inner_l, name_area.GetBottom() + (float) dip(12, dpi_), inner_w, (float) dip(26, dpi_));
        g.DrawString(view.status.c_str(), -1, &sub, sub_rc, &center, &muted_b);

        std::wstring how = L"On iPhone: Control Center  →  Screen Mirroring  →  " + view.name;
        Font step(ui_text(), (float) dip(13, dpi_), FontStyleRegular, UnitPixel);
        RectF step_rc(inner_l, sub_rc.GetBottom() + (float) dip(10, dpi_), inner_w, (float) dip(22, dpi_));
        if (show_capsules && step_rc.GetBottom() > cap_y - dip(8, dpi_)) {
            step_rc.Y = cap_y - (float) dip(30, dpi_);
        }
        g.DrawString(how.c_str(), -1, &step, step_rc, &center, &muted_b);
    }

    if (show_capsules) {
        const wchar_t *labels[4] = {L"Rename", L"Full Screen", L"Always on Top", L"Require PIN"};
        bool on[4] = {false, view.fullscreen, view.always_on_top, view.require_pin};
        float bw = (inner_w - cap_gap * 3.f) / 4.f;
        if (bw < (float) dip(96, dpi_)) {
            bw = (float) dip(96, dpi_);
        }
        Font btn_f(ui_text(), (float) dip(12, dpi_), FontStyleRegular, UnitPixel);
        float icon_s = (float) dip(14, dpi_);
        float sw = (std::max)(1.15f, (float) dip(1, dpi_) + 0.25f);
        for (int i = 0; i < 4; ++i) {
            RectF br(inner_l + (float) i * (bw + cap_gap), cap_y, bw, cap_h);
            if (br.GetRight() > inner_r + 2) {
                br.Width = inner_r - br.X;
            }
            btn_rc_[i] = {(LONG) br.X, (LONG) br.Y, (LONG) br.GetRight(), (LONG) br.GetBottom()};
            bool hot = hover == (int) UiHit::Rename + i;
            float cr = cap_h * 0.5f;
            Color ink = on[i] ? c_on_ink : (hot ? c_text : c_muted);
            if (on[i]) {
                fill_round(g, br, cr, c_blue);
                GraphicsPath on_clip;
                add_round_rect(on_clip, br, cr);
                g.SetClip(&on_clip);
                LinearGradientBrush on_sheen(br, Color(55, 255, 255, 255), Color(0, 255, 255, 255),
                                             Gdiplus::LinearGradientModeVertical);
                g.FillRectangle(&on_sheen, RectF(br.X, br.Y, br.Width, br.Height * 0.45f));
                g.ResetClip();
            } else {
                Color fill = hot ? c_plate_hi : c_capsule;
                fill_round(g, br, cr, fill);
                draw_round(g, br, cr, hot ? c_rim : Color(255, 90, 90, 94), 1.15f);
            }
            // Optical: stroke icons sit a hair high in stadiums.
            RectF icon(br.X + (float) dip(12, dpi_), br.Y + (cap_h - icon_s) * 0.5f - 0.5f, icon_s, icon_s);
            if (i == 0) {
                icon_pencil(g, icon, ink, sw);
            } else if (i == 1) {
                icon_display(g, icon, ink, sw);
            } else if (i == 2) {
                icon_pin(g, icon, ink, sw);
            } else {
                icon_lock(g, icon, ink, sw);
            }
            RectF lab(icon.GetRight() + (float) dip(6, dpi_), br.Y, br.GetRight() - icon.GetRight() - dip(10, dpi_),
                      cap_h);
            SolidBrush tb(ink);
            StringFormat lab_f;
            lab_f.SetAlignment(StringAlignmentNear);
            lab_f.SetLineAlignment(StringAlignmentCenter);
            lab_f.SetFormatFlags(StringFormatFlagsNoWrap);
            g.DrawString(labels[i], -1, &btn_f, lab, &lab_f, &tb);
        }
    } else {
        for (auto &r : btn_rc_) {
            r = {};
        }
    }

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteObject(mem);
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
    return UiHit::None;
}

bool ShellUi::cursor_hand(UiHit hit) const {
    return hit != UiHit::None;
}

} // namespace airscreen
