#include "receiver.h"
#include "settings.h"
#include "ui.h"
#include "app/resource.h"
#include "raop.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objbase.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <dbghelp.h>
#include <timeapi.h>
#include <string>
#include <cmath>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dbghelp.lib")

namespace {

constexpr UINT WM_AIR_EVENT = WM_APP + 1;
constexpr UINT WM_TRAY = WM_APP + 2;
constexpr UINT kTrayId = 1;
constexpr UINT kTimerOverlay = 1;
constexpr UINT kTimerDisconnect = 2;
constexpr UINT kTimerDrift = 3;
constexpr UINT kOverlayHideMs = 2200;
constexpr UINT kDisconnectMs = 400;
UINT g_taskbar_created = 0;

struct NameDlgState {
    std::wstring initial;
    std::wstring result;
    bool ok = false;
};

struct App {
    HWND hwnd = nullptr;
    HWND video_hwnd = nullptr;
    HWND settings_hwnd = nullptr;
    HINSTANCE inst = nullptr;
    NOTIFYICONDATAW nid = {};
    airscreen::Settings settings;
    airscreen::Receiver receiver;
    airscreen::ShellUi shell;
    bool fullscreen = false;
    bool running = true;
    bool settings_hot = false;
    bool mirroring = false;
    bool start_failed = false;
    int hover = 0;
    LONG windowed_style = 0;
    LONG windowed_ex = 0;
    RECT windowed_rc = {};
    std::wstring overlay_status = L"Waiting for iPhone";
    std::wstring overlay_pin;
    std::wstring overlay_detail;
    bool connected = false;
    HICON icon = nullptr;
    HICON icon_sm = nullptr;

    void post_ui(airscreen::UiEvent ev, const std::string &msg) {
        auto *s = new std::string(msg);
        PostMessageW(hwnd, WM_AIR_EVENT, (WPARAM) ev, (LPARAM) s);
    }

    std::wstring utf16(const std::string &s) {
        if (s.empty()) {
            return {};
        }
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
        std::wstring w(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int) s.size(), w.data(), n);
        return w;
    }
};

App *g_app = nullptr;

UINT window_dpi(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    return dpi ? dpi : 96;
}

void set_dark_title(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

void apply_topmost(HWND hwnd, bool on) {
    SetWindowPos(hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void update_title(App *app) {
    std::wstring t = L"AirScreen - " + app->utf16(app->settings.name);
    if (app->connected) {
        t += L" - Live";
    } else {
        t += L" - Waiting";
    }
    SetWindowTextW(app->hwnd, t.c_str());
}

void apply_video_fill(App *app) {
    app->receiver.video().set_cover(app->fullscreen && app->settings.fill_screen);
}

void layout_chrome(App *app) {
    if (!app->hwnd) {
        return;
    }
    RECT rc = {};
    GetClientRect(app->hwnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (app->video_hwnd) {
        SetWindowPos(app->video_hwnd, HWND_BOTTOM, 0, 0, cw, ch,
                     SWP_NOACTIVATE);
    }
    UINT dpi = window_dpi(app->hwnd);
    int size = MulDiv(40, dpi, 96);
    int margin = MulDiv(12, dpi, 96);
    int x = cw - margin - size;
    int y = margin;
    if (x < margin) {
        x = margin;
    }
    if (app->settings_hwnd) {
        SetWindowPos(app->settings_hwnd, HWND_TOP, x, y, size, size, SWP_NOACTIVATE);
        HRGN rgn = CreateRoundRectRgn(0, 0, size + 1, size + 1, size, size);
        SetWindowRgn(app->settings_hwnd, rgn, TRUE);
    }
}

void show_settings_button(App *app, bool show) {
    if (!app->settings_hwnd) {
        return;
    }
    ShowWindow(app->settings_hwnd, show ? SW_SHOWNA : SW_HIDE);
    if (show) {
        SetWindowPos(app->settings_hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void note_activity(App *app) {
    if (!app->mirroring) {
        show_settings_button(app, false);
        KillTimer(app->hwnd, kTimerOverlay);
        return;
    }
    show_settings_button(app, true);
    SetTimer(app->hwnd, kTimerOverlay, kOverlayHideMs, nullptr);
}

void set_fullscreen(App *app, bool on) {
    if (!app->hwnd || app->fullscreen == on) {
        if (on) {
            apply_video_fill(app);
        }
        return;
    }
    if (on) {
        app->windowed_style = GetWindowLongW(app->hwnd, GWL_STYLE);
        app->windowed_ex = GetWindowLongW(app->hwnd, GWL_EXSTYLE);
        GetWindowRect(app->hwnd, &app->windowed_rc);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(app->hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongW(app->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowLongW(app->hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
        SetWindowPos(app->hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        app->fullscreen = true;
    } else {
        SetWindowLongW(app->hwnd, GWL_STYLE, app->windowed_style | WS_CLIPCHILDREN);
        SetWindowLongW(app->hwnd, GWL_EXSTYLE, app->windowed_ex);
        SetWindowPos(app->hwnd, HWND_NOTOPMOST, app->windowed_rc.left, app->windowed_rc.top,
                     app->windowed_rc.right - app->windowed_rc.left,
                     app->windowed_rc.bottom - app->windowed_rc.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        apply_topmost(app->hwnd, app->settings.always_on_top);
        app->fullscreen = false;
    }
    apply_video_fill(app);
    layout_chrome(app);
    app->receiver.video().resize();
    note_activity(app);
}

void toggle_fullscreen(App *app) {
    set_fullscreen(app, !app->fullscreen);
}

void set_boost(bool on) {
    static bool boosted = false;
    if (on == boosted) {
        return;
    }
    boosted = on;
    SetPriorityClass(GetCurrentProcess(), on ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
    on ? timeBeginPeriod(1) : timeEndPeriod(1);
}

void show_video_surface(App *app) {
    set_boost(true);
    app->mirroring = true;
    if (!IsWindowVisible(app->hwnd)) {
        ShowWindow(app->hwnd, SW_SHOWNORMAL); // someone started mirroring while we sat in the tray
    }
    if (app->video_hwnd) {
        ShowWindow(app->video_hwnd, SW_SHOW);
    }
    layout_chrome(app);
    note_activity(app);
}

void return_to_menu(App *app) {
    set_boost(false);
    app->connected = false;
    app->mirroring = false;
    app->overlay_status = L"Waiting for iPhone";
    app->overlay_pin.clear();
    if (app->video_hwnd) {
        ShowWindow(app->video_hwnd, SW_HIDE);
    }
    if (app->fullscreen) {
        set_fullscreen(app, false);
    }
    KillTimer(app->hwnd, kTimerOverlay);
    show_settings_button(app, false);
    layout_chrome(app);
    update_title(app);
    InvalidateRect(app->hwnd, nullptr, FALSE);
    UpdateWindow(app->hwnd);
}

INT_PTR CALLBACK rename_dlg(HWND h, UINT msg, WPARAM w, LPARAM l) {
    auto *st = (NameDlgState *) GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_INITDIALOG: {
        st = (NameDlgState *) l;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR) st);
        SetDlgItemTextW(h, IDC_NAME, st->initial.c_str());
        SendDlgItemMessageW(h, IDC_NAME, EM_SETSEL, 0, -1);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) {
            wchar_t buf[128] = {};
            GetDlgItemTextW(h, IDC_NAME, buf, 128);
            st->result = buf;
            st->ok = !st->result.empty();
            EndDialog(h, IDOK);
            return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void do_rename(App *app) {
    NameDlgState st;
    st.initial = app->utf16(app->settings.name);
    if (DialogBoxParamW(app->inst, MAKEINTRESOURCEW(IDD_RENAME), app->hwnd, rename_dlg, (LPARAM) &st) == IDOK &&
        st.ok) {
        int n = WideCharToMultiByte(CP_UTF8, 0, st.result.c_str(), (int) st.result.size(), nullptr, 0, nullptr, nullptr);
        std::string name(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, st.result.c_str(), (int) st.result.size(), name.data(), n, nullptr, nullptr);
        if (app->receiver.rename(name)) {
            app->settings.name = name;
            airscreen::save_settings(app->settings);
            update_title(app);
        }
    }
}

void popup_settings_menu(App *app, POINT screen_pt, bool tray) {
    HMENU m = CreatePopupMenu();
    if (tray) {
        AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"Show AirScreen");
    }
    AppendMenuW(m, MF_STRING, IDM_TRAY_RENAME, L"Rename receiver…");
    AppendMenuW(m, MF_STRING | (app->settings.always_on_top ? MF_CHECKED : 0), IDM_TRAY_TOP, L"Always on top");
    AppendMenuW(m, MF_STRING | (app->settings.require_pin ? MF_CHECKED : 0), IDM_TRAY_PIN, L"Require pairing PIN");
    AppendMenuW(m, MF_STRING | (app->fullscreen ? MF_CHECKED : 0), IDM_TRAY_FULL, L"Fullscreen");
    AppendMenuW(m, MF_STRING | (app->settings.fill_screen ? MF_CHECKED : 0), IDM_TRAY_FILL,
                L"Fill screen (no black bars)");
    AppendMenuW(m, MF_STRING | (airscreen::start_with_windows() ? MF_CHECKED : 0), IDM_TRAY_STARTUP,
                L"Start with Windows");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_TRAY_FIREWALL, L"Allow through firewall");
    AppendMenuW(m, MF_STRING, IDM_TRAY_LOG, L"Open log file");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"Exit");
    SetForegroundWindow(app->hwnd);
    KillTimer(app->hwnd, kTimerOverlay);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, screen_pt.x, screen_pt.y, 0, app->hwnd, nullptr);
    DestroyMenu(m);
    note_activity(app);
}

void show_tray_menu(App *app) {
    POINT pt;
    GetCursorPos(&pt);
    popup_settings_menu(app, pt, true);
}

void show_corner_menu(App *app) {
    RECT rc = {};
    GetWindowRect(app->settings_hwnd, &rc);
    POINT pt = {rc.left, rc.bottom + MulDiv(4, window_dpi(app->hwnd), 96)};
    popup_settings_menu(app, pt, false);
}

void paint_idle(App *app, HDC hdc, RECT rc) {
    airscreen::UiView v;
    v.name = app->utf16(app->settings.name);
    v.status = app->overlay_status;
    v.pin = app->overlay_pin;
    v.connected = app->connected;
    v.always_on_top = app->settings.always_on_top;
    v.require_pin = app->settings.require_pin;
    v.fullscreen = app->fullscreen;
    v.dark = app->settings.dark_mode;
    v.failed = app->start_failed;
    v.inset_right = 0;
    app->shell.paint(app->hwnd, hdc, rc, v, app->hover);
}

void handle_tray_cmd(App *app, UINT id) {
    switch (id) {
    case IDM_TRAY_SHOW:
        ShowWindow(app->hwnd, SW_RESTORE);
        SetForegroundWindow(app->hwnd);
        break;
    case IDM_TRAY_RENAME:
        do_rename(app);
        break;
    case IDM_TRAY_TOP:
        app->settings.always_on_top = !app->settings.always_on_top;
        airscreen::save_settings(app->settings);
        if (!app->fullscreen) {
            apply_topmost(app->hwnd, app->settings.always_on_top);
        }
        break;
    case IDM_TRAY_PIN:
        app->settings.require_pin = !app->settings.require_pin;
        app->receiver.set_require_pin(app->settings.require_pin);
        airscreen::save_settings(app->settings);
        break;
    case IDM_TRAY_FULL:
        toggle_fullscreen(app);
        break;
    case IDM_TRAY_FILL:
        app->settings.fill_screen = !app->settings.fill_screen;
        airscreen::save_settings(app->settings);
        apply_video_fill(app);
        break;
    case IDM_TRAY_STARTUP:
        airscreen::set_start_with_windows(!airscreen::start_with_windows());
        break;
    case IDM_TRAY_FIREWALL:
        if (!airscreen::ensure_firewall_rule()) {
            MessageBoxW(app->hwnd, L"Could not add a firewall rule. Allow AirScreen when Windows asks, or run as administrator.",
                        L"AirScreen", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(app->hwnd, L"Firewall rule added for AirScreen.", L"AirScreen", MB_OK);
        }
        break;
    case IDM_TRAY_LOG: {
        auto p = app->utf16(airscreen::log_path());
        ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    }
    case IDM_TRAY_EXIT:
        DestroyWindow(app->hwnd);
        return;
    }
    if (app->hwnd) {
        InvalidateRect(app->hwnd, nullptr, FALSE);
    }
}

void apply_hit(App *app, airscreen::UiHit hit) {
    if (app->mirroring) {
        return;
    }
    switch (hit) {
    case airscreen::UiHit::Name:
    case airscreen::UiHit::Rename:
        do_rename(app);
        break;
    case airscreen::UiHit::Fullscreen:
        toggle_fullscreen(app);
        break;
    case airscreen::UiHit::Topmost:
        handle_tray_cmd(app, IDM_TRAY_TOP);
        break;
    case airscreen::UiHit::Pin:
        handle_tray_cmd(app, IDM_TRAY_PIN);
        break;
    case airscreen::UiHit::Dark:
        app->settings.dark_mode = !app->settings.dark_mode;
        airscreen::save_settings(app->settings);
        InvalidateRect(app->hwnd, nullptr, FALSE);
        break;
    default:
        break;
    }
}

void paint_gear(HWND hwnd, HDC hdc, bool hot) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ old_bmp = SelectObject(mem, bmp);

    // Continuity plate body at rest; plate-hi on hover.
    COLORREF fill = hot ? RGB(58, 58, 60) : RGB(44, 44, 46);
    HBRUSH b = CreateSolidBrush(fill);
    FillRect(mem, &rc, b);
    DeleteObject(b);

    // Hairline rim: glass edge, brighter on hover.
    HPEN rim = CreatePen(PS_SOLID, 1, hot ? RGB(120, 120, 124) : RGB(96, 96, 100));
    HGDIOBJ old_pen = SelectObject(mem, rim);
    HGDIOBJ old_br = SelectObject(mem, GetStockObject(NULL_BRUSH));
    Ellipse(mem, 0, 0, w - 1, h - 1);
    SelectObject(mem, old_br);
    SelectObject(mem, old_pen);
    DeleteObject(rim);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(245, 245, 247));
    // Optical: MDL2 gear sits heavy; size a hair under the circle for balance.
    int px = MulDiv(17, window_dpi(hwnd), 96);
    HFONT font = CreateFontW(px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    HGDIOBJ old_font = SelectObject(mem, font);
    RECT text_rc = rc;
    text_rc.top -= 1; // optical vertical center
    DrawTextW(mem, L"\uE713", -1, &text_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, old_font);
    DeleteObject(font);
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

LRESULT CALLBACK video_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    HWND parent = GetParent(hwnd);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        SetFocus(parent);
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONUP: {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        MapWindowPoints(hwnd, parent, &pt, 1);
        return SendMessageW(parent, msg, wparam, MAKELPARAM(pt.x, pt.y));
    }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK gear_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    App *app = g_app;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_gear(hwnd, hdc, app && app->settings_hot);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        if (app && !app->settings_hot) {
            app->settings_hot = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (app) {
            note_activity(app);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (app) {
            app->settings_hot = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (app) {
            show_corner_menu(app);
        }
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    App *app = g_app;
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        paint_idle(app, hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (app) {
            layout_chrome(app);
            app->receiver.video().resize();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        RECT *sugg = (RECT *) lparam;
        SetWindowPos(hwnd, nullptr, sugg->left, sugg->top, sugg->right - sugg->left, sugg->bottom - sugg->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (app) {
            layout_chrome(app);
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto *mm = (MINMAXINFO *) lparam;
        mm->ptMinTrackSize.x = 720;
        mm->ptMinTrackSize.y = 480;
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (app) {
            note_activity(app);
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int hov = (int) app->shell.hit_test(pt);
            if (hov != app->hover) {
                app->hover = hov;
                if (!app->mirroring) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (app && app->hover) {
            app->hover = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (app && !app->mirroring && LOWORD(lparam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (app->shell.cursor_hand(app->shell.hit_test(pt))) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_LBUTTONUP:
        if (app && !app->mirroring) {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            apply_hit(app, app->shell.hit_test(pt));
        }
        return 0;
    case WM_TIMER:
        if (!app) {
            return 0;
        }
        if (wparam == kTimerOverlay && app->mirroring) {
            POINT pt = {};
            GetCursorPos(&pt);
            if (WindowFromPoint(pt) != app->settings_hwnd) {
                show_settings_button(app, false);
            }
        } else if (wparam == kTimerDisconnect) {
            KillTimer(hwnd, kTimerDisconnect);
            return_to_menu(app);
        } else if (wparam == kTimerDrift && !app->mirroring) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDBLCLK:
        if (app && !app->mirroring) {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            auto hit = app->shell.hit_test(pt);
            if (hit == airscreen::UiHit::Name || hit == airscreen::UiHit::Rename) {
                do_rename(app);
                return 0;
            }
            if (hit != airscreen::UiHit::None) {
                return 0; // a double-click on a tile is just two clicks; only the open field goes full screen
            }
        }
        toggle_fullscreen(app);
        return 0;
    case WM_RBUTTONUP:
        if (app) {
            POINT pt = {};
            GetCursorPos(&pt);
            popup_settings_menu(app, pt, false);
        }
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_F11) {
            toggle_fullscreen(app);
        } else if (wparam == VK_F2) {
            do_rename(app);
        } else if (wparam == VK_ESCAPE && app->fullscreen) {
            set_fullscreen(app, false);
        }
        return 0;
    case WM_AIR_EVENT: {
        auto ev = (airscreen::UiEvent) wparam;
        auto *s = (std::string *) lparam;
        std::string msg = s ? *s : "";
        delete s;
        switch (ev) {
        case airscreen::UiEvent::Connected:
            KillTimer(hwnd, kTimerDisconnect);
            set_boost(true);
            app->connected = true;
            app->overlay_pin.clear();
            app->overlay_status = L"Connected";
            if (!msg.empty()) {
                app->overlay_status = L"Connected: " + app->utf16(msg);
            }
            update_title(app);
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case airscreen::UiEvent::VideoReady:
            KillTimer(hwnd, kTimerDisconnect);
            app->connected = true;
            show_video_surface(app);
            break;
        case airscreen::UiEvent::Disconnected:
            SetTimer(hwnd, kTimerDisconnect, kDisconnectMs, nullptr);
            break;
        case airscreen::UiEvent::Pin:
            app->overlay_pin = app->utf16(msg);
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case airscreen::UiEvent::Log:
            app->overlay_detail = app->utf16(msg);
            break;
        case airscreen::UiEvent::VideoSize:
        case airscreen::UiEvent::ClientName:
            if (ev == airscreen::UiEvent::ClientName && !msg.empty()) {
                app->overlay_status = L"Connected: " + app->utf16(msg);
                update_title(app);
            }
            break;
        }
        return 0;
    }
    case WM_TRAY:
        if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
            show_tray_menu(app);
        } else if (LOWORD(lparam) == WM_LBUTTONDBLCLK || LOWORD(lparam) == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;
    case WM_COMMAND:
        handle_tray_cmd(app, LOWORD(wparam));
        return 0;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        if (app && !app->settings.tray_tip_shown) {
            app->settings.tray_tip_shown = true;
            airscreen::save_settings(app->settings);
            NOTIFYICONDATAW tip = app->nid;
            tip.uFlags = NIF_INFO;
            tip.dwInfoFlags = NIIF_NONE;
            wcscpy_s(tip.szInfoTitle, L"AirScreen is still running");
            wcscpy_s(tip.szInfo, L"Your iPhone can still mirror here. Right-click the tray icon to exit.");
            Shell_NotifyIconW(NIM_MODIFY, &tip);
        }
        return 0;
    case WM_DESTROY: {
        if (app->fullscreen) {
            set_fullscreen(app, false);
        }
        WINDOWPLACEMENT wp = {sizeof(wp)};
        if (GetWindowPlacement(hwnd, &wp)) {
            RECT r = wp.rcNormalPosition;
            app->settings.win_x = r.left;
            app->settings.win_y = r.top;
            app->settings.win_w = r.right - r.left;
            app->settings.win_h = r.bottom - r.top;
            airscreen::save_settings(app->settings);
        }
        KillTimer(hwnd, kTimerDrift);
        KillTimer(hwnd, kTimerOverlay);
        KillTimer(hwnd, kTimerDisconnect);
        Shell_NotifyIconW(NIM_DELETE, &app->nid);
        HICON shell32 = app->shell.icon32();
        HICON shell16 = app->shell.icon16();
        if (app->icon_sm && app->icon_sm != app->icon && app->icon_sm != shell16) {
            DestroyIcon(app->icon_sm);
        }
        app->icon_sm = nullptr;
        if (app->icon && app->icon != shell32) {
            DestroyIcon(app->icon);
        }
        app->icon = nullptr;
        app->receiver.stop();
        app->shell.shutdown();
        PostQuitMessage(0);
        return 0;
    }
    }
    if (msg == g_taskbar_created && app) {
        Shell_NotifyIconW(NIM_ADD, &app->nid);
        Shell_NotifyIconW(NIM_SETVERSION, &app->nid);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep) {
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1) != 0) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "CRASH code=0x%08X addr=%p",
             (unsigned) ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);
    airscreen::append_log(buf);

    std::wstring dump = airscreen::app_data_dir() + L"\\crash.dmp";
    HANDLE file = CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info = {};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = ep;
        info.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithDataSegs, &info, nullptr,
                          nullptr);
        CloseHandle(file);
        airscreen::append_log("wrote crash.dmp");
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

bool activate_existing() {
    HWND w = FindWindowW(L"AirScreenWindow", nullptr);
    if (!w) {
        return false;
    }
    ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdline, int show) {
    SetUnhandledExceptionFilter(crash_filter);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE instance = CreateMutexW(nullptr, TRUE, L"Local\\AirScreen.Receiver.1");
    if (instance && GetLastError() == ERROR_ALREADY_EXISTS) {
        activate_existing();
        CloseHandle(instance);
        return 0;
    }

    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    ULONG_PTR gdip = airscreen::gdiplus_startup();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void) hr;
    airscreen::rotate_log();
    airscreen::append_log("AirScreen starting");
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    bool start_in_tray = wcsstr(cmdline, L"--tray") != nullptr;

    App app;
    g_app = &app;
    app.inst = inst;
    app.settings = airscreen::load_settings();
    app.shell.startup();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (FAILED(LoadIconMetric(inst, MAKEINTRESOURCEW(IDI_APP), LIM_LARGE, &app.icon)) || !app.icon) {
        app.icon = app.shell.icon32();
    }
    if (FAILED(LoadIconMetric(inst, MAKEINTRESOURCEW(IDI_APP), LIM_SMALL, &app.icon_sm)) || !app.icon_sm) {
        app.icon_sm = app.shell.icon16() ? app.shell.icon16() : app.icon;
    }
    wc.hIcon = app.icon;
    wc.hIconSm = app.icon_sm;
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"AirScreenWindow";
    if (!RegisterClassExW(&wc)) {
        airscreen::append_log("RegisterClassEx failed");
        MessageBoxW(nullptr, L"Could not register the AirScreen window class.", L"AirScreen", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW vc = wc;
    vc.style = CS_DBLCLKS;
    vc.lpfnWndProc = video_proc;
    vc.lpszClassName = L"AirScreenVideo";
    vc.hIcon = nullptr;
    if (!RegisterClassExW(&vc)) {
        airscreen::append_log("RegisterClassEx video failed");
        return 1;
    }

    WNDCLASSEXW gc = wc;
    gc.style = 0;
    gc.lpfnWndProc = gear_proc;
    gc.lpszClassName = L"AirScreenGear";
    gc.hCursor = LoadCursor(nullptr, IDC_HAND);
    gc.hIcon = nullptr;
    gc.hbrBackground = nullptr;
    if (!RegisterClassExW(&gc)) {
        airscreen::append_log("RegisterClassEx gear failed");
        return 1;
    }

    app.hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"AirScreen",
                               WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720,
                               nullptr, nullptr, inst, nullptr);
    if (!app.hwnd) {
        airscreen::append_log("CreateWindowEx failed");
        MessageBoxW(nullptr, L"Could not create the AirScreen window.", L"AirScreen", MB_OK | MB_ICONERROR);
        return 1;
    }
    app.video_hwnd = CreateWindowExW(0, L"AirScreenVideo", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, app.hwnd,
                                     nullptr, inst, nullptr);
    app.settings_hwnd = CreateWindowExW(0, L"AirScreenGear", L"Settings", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 40, 40,
                                        app.hwnd, (HMENU)(INT_PTR) IDC_SETTINGS, inst, nullptr);
    RECT saved = {app.settings.win_x, app.settings.win_y, app.settings.win_x + app.settings.win_w,
                  app.settings.win_y + app.settings.win_h};
    if (app.settings.win_w > 0 && app.settings.win_h > 0 && MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
        WINDOWPLACEMENT wp = {sizeof(wp)};
        GetWindowPlacement(app.hwnd, &wp);
        wp.rcNormalPosition = saved;
        wp.showCmd = SW_HIDE;
        SetWindowPlacement(app.hwnd, &wp);
    }
    set_dark_title(app.hwnd);
    apply_topmost(app.hwnd, app.settings.always_on_top);
    update_title(&app);
    layout_chrome(&app);

    (void) show;
    if (!start_in_tray) {
        ShowWindow(app.hwnd, SW_SHOWNORMAL);
        UpdateWindow(app.hwnd);
        SetForegroundWindow(app.hwnd);
    }

    app.nid.cbSize = sizeof(app.nid);
    app.nid.hWnd = app.hwnd;
    app.nid.uID = kTrayId;
    app.nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    app.nid.uCallbackMessage = WM_TRAY;
    app.nid.hIcon = app.icon_sm ? app.icon_sm : app.icon;
    wcscpy_s(app.nid.szTip, L"AirScreen");
    Shell_NotifyIconW(NIM_ADD, &app.nid);
    app.nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &app.nid);
    SetTimer(app.hwnd, kTimerDrift, 100, nullptr); // the field drifts over minutes; 10 fps is plenty

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#ifdef PROCESS_POWER_THROTTLING_CURRENT_VERSION
    PROCESS_POWER_THROTTLING_STATE pt = {};
    pt.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    pt.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof(pt));
#endif

    MSG pump;
    while (PeekMessageW(&pump, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&pump);
        DispatchMessageW(&pump);
    }

    auto ui = [&app](airscreen::UiEvent ev, const std::string &msg) { app.post_ui(ev, msg); };
    ntp_global_init();
    app.overlay_status = L"Starting AirPlay…";
    InvalidateRect(app.hwnd, nullptr, FALSE);
    if (!app.receiver.start(app.video_hwnd, app.settings, ui)) {
        app.start_failed = true;
        app.overlay_status = L"Could not start. Check the log from Settings";
        InvalidateRect(app.hwnd, nullptr, FALSE);
        MessageBoxW(app.hwnd,
                    L"Could not start the AirPlay receiver.\n\nAllow AirScreen through the firewall (tray menu) and check %APPDATA%\\AirScreen\\airscreen.log",
                    L"AirScreen", MB_OK | MB_ICONERROR);
    } else {
        app.overlay_status = L"Waiting for iPhone";
        apply_video_fill(&app);
        InvalidateRect(app.hwnd, nullptr, FALSE);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    airscreen::gdiplus_shutdown(gdip);
    set_boost(false);
    if (instance) {
        CloseHandle(instance);
    }
    return 0;
}
