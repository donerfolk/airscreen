#pragma once

#include <windows.h>
#include <string>

namespace airscreen {

enum class UiHit {
    None = 0,
    Name,
    Rename,
    Fullscreen,
    Topmost,
    Pin,
    Dark,
};

struct UiView {
    std::wstring name;
    std::wstring status;
    std::wstring pin;
    std::wstring detail;
    bool connected = false;
    bool always_on_top = false;
    bool require_pin = false;
    bool fullscreen = false;
    bool dark = false;
    int inset_right = 0;
    bool failed = false;
};

class ShellUi {
public:
    void startup();
    void shutdown();

    HICON icon32() const { return icon32_; }
    HICON icon16() const { return icon16_; }

    void paint(HWND hwnd, HDC hdc, RECT client, const UiView &view, int hover);
    UiHit hit_test(POINT pt) const;
    RECT name_rect() const { return name_rc_; }
    RECT edit_rect() const { return edit_rc_; }

    bool cursor_hand(UiHit hit) const;

private:
    void ensure_icons();
    void destroy_icons();

    HICON icon32_ = nullptr;
    HICON icon16_ = nullptr;
    RECT screen_rc_{};
    RECT name_rc_{};
    RECT edit_rc_{};
    RECT btn_rc_[4]{};
    RECT dark_rc_{};
    RECT client_{};
    int dpi_ = 96;
};

ULONG_PTR gdiplus_startup();
void gdiplus_shutdown(ULONG_PTR token);

} // namespace airscreen
