#include "window_size.h"
#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

bool same_size(SIZE size, LONG width, LONG height) {
    return size.cx == width && size.cy == height;
}
}

int main() {
    SIZE landscape = {1920, 1080};
    SIZE portrait = {1080, 1920};
    const double scales[] = {0.5, 1, 1.5, 2};
    const LONG widths[] = {960, 1920, 2880, 3840};
    const LONG heights[] = {540, 1080, 1620, 2160};
    for (int i = 0; i < 4; ++i) {
        check(same_size(airscreen::scaled_video_size(landscape, scales[i]), widths[i], heights[i]),
              "landscape native resolution");
        check(same_size(airscreen::scaled_video_size(portrait, scales[i]), heights[i], widths[i]),
              "portrait native resolution");
    }

    SIZE frame = {16, 16};
    RECT current = {100, 100, 1076, 656};
    for (UINT edge = WMSZ_LEFT; edge <= WMSZ_BOTTOMRIGHT; ++edge) {
        bool left = edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT;
        bool right = edge == WMSZ_RIGHT || edge == WMSZ_TOPRIGHT || edge == WMSZ_BOTTOMRIGHT;
        bool top = edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT;
        bool bottom = edge == WMSZ_BOTTOM || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT;
        RECT requested = current;
        if (left) requested.left -= 192;
        if (right) requested.right += 192;
        if (top) requested.top -= 108;
        if (bottom) requested.bottom += 108;
        RECT actual = airscreen::fixed_ratio_rect(requested, current, edge, landscape, frame);
        check(actual.right - actual.left - frame.cx == 1152 &&
              actual.bottom - actual.top - frame.cy == 648, "all edges keep client aspect ratio");
        check(left ? actual.right == current.right : actual.left == current.left,
              "horizontal opposite edge stays anchored");
        check(top ? actual.bottom == current.bottom : actual.top == current.top,
              "vertical opposite edge stays anchored");
    }

    RECT vertical_drag = current;
    vertical_drag.bottom += 108;
    RECT actual = airscreen::fixed_ratio_rect(vertical_drag, current, WMSZ_BOTTOMRIGHT, landscape, frame);
    check(actual.right - actual.left == 1168 && actual.bottom - actual.top == 664,
          "corner follows vertical dragging");

    RECT portrait_rect = {100, 100, 656, 1076};
    vertical_drag = portrait_rect;
    vertical_drag.bottom += 192;
    actual = airscreen::fixed_ratio_rect(vertical_drag, portrait_rect, WMSZ_BOTTOM, portrait, frame);
    check(actual.right - actual.left == 664 && actual.bottom - actual.top == 1168,
          "portrait resizing uses original ratio");

    RECT tiny = {100, 100, 117, 117};
    actual = airscreen::fixed_ratio_rect(tiny, current, WMSZ_RIGHT, landscape, frame);
    check(actual.right - actual.left == 130 && actual.bottom - actual.top == 80,
          "minimum size preserves ratio");

    RECT work = {-1920, 40, 0, 1080};
    RECT old = {-1200, 200, -224, 756};
    RECT rotated = airscreen::sized_window_rect(old, {556, 976}, work);
    check(rotated.left == -990 && rotated.top == 40 && rotated.right == -434 && rotated.bottom == 1016,
          "rotation preserves center and stays in the monitor work area");
    RECT large = airscreen::sized_window_rect(old, {3856, 2176}, work);
    check(large.left == -1920 && large.top == 40 && large.right - large.left == 3856 &&
          large.bottom - large.top == 2176, "large explicit sizes remain exact");

    std::printf("Window sizing: %s\n", failures ? "FAILED" : "PASS");
    return failures ? 1 : 0;
}
