#pragma once

#include <windows.h>
#include <algorithm>
#include <cmath>

namespace airscreen {

inline SIZE scaled_video_size(SIZE source, double scale) {
    return {std::max(1L, std::lround(source.cx * scale)),
            std::max(1L, std::lround(source.cy * scale))};
}

inline RECT sized_window_rect(RECT current, SIZE size, RECT work) {
    LONG x = current.left + (current.right - current.left - size.cx) / 2;
    LONG y = current.top + (current.bottom - current.top - size.cy) / 2;
    x = std::clamp(x, work.left, std::max(work.left, work.right - size.cx));
    y = std::clamp(y, work.top, std::max(work.top, work.bottom - size.cy));
    return {x, y, x + size.cx, y + size.cy};
}

inline RECT fixed_ratio_rect(RECT requested, RECT initial, UINT edge, SIZE source, SIZE frame) {
    LONG width = std::max(1L, requested.right - requested.left - frame.cx);
    LONG height = std::max(1L, requested.bottom - requested.top - frame.cy);
    bool horizontal = edge == WMSZ_LEFT || edge == WMSZ_RIGHT;
    if (edge != WMSZ_TOP && edge != WMSZ_BOTTOM && !horizontal) {
        LONG dx = (requested.right - requested.left) - (initial.right - initial.left);
        LONG dy = (requested.bottom - requested.top) - (initial.bottom - initial.top);
        horizontal = std::abs(dx) * (double) source.cy >= std::abs(dy) * (double) source.cx;
    }
    double scale = horizontal ? (double) width / source.cx : (double) height / source.cy;
    scale = std::max(scale, std::max(64.0 / source.cx, 64.0 / source.cy));
    SIZE size = scaled_video_size(source, scale);
    size.cx += frame.cx;
    size.cy += frame.cy;
    if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT) {
        requested.left = requested.right - size.cx;
    } else {
        requested.right = requested.left + size.cx;
    }
    if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT) {
        requested.top = requested.bottom - size.cy;
    } else {
        requested.bottom = requested.top + size.cy;
    }
    return requested;
}

} // namespace airscreen
