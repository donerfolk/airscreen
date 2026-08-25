"""Generate AirScreen app.ico — Continuity glass: plate body + pale frosted screen."""
from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "src" / "app"

DEG90 = math.radians(90)
DIR_V = (math.sin(DEG90), -math.cos(DEG90))


def sd_round_rect(px: float, py: float, x: float, y: float, w: float, h: float, r: float) -> float:
    r = min(r, w * 0.5, h * 0.5)
    cx, cy = x + w * 0.5, y + h * 0.5
    dx = abs(px - cx) - (w * 0.5 - r)
    dy = abs(py - cy) - (h * 0.5 - r)
    ox, oy = max(dx, 0.0), max(dy, 0.0)
    return math.hypot(ox, oy) + min(max(dx, dy), 0.0) - r


def cover(d: float) -> float:
    return 0.0 if d >= 0.5 else 1.0 if d <= -0.5 else 0.5 - d


def blend(dst: list[float], r: float, g: float, b: float, a: float, t: float) -> None:
    a = a / 255.0 * t
    if a <= 0:
        return
    out_a = a + dst[3] * (1 - a)
    if out_a <= 0:
        return
    dst[0] = (r * a + dst[0] * dst[3] * (1 - a)) / out_a
    dst[1] = (g * a + dst[1] * dst[3] * (1 - a)) / out_a
    dst[2] = (b * a + dst[2] * dst[3] * (1 - a)) / out_a
    dst[3] = out_a


def lerp(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[float, float, float]:
    t = 0.0 if t < 0 else 1.0 if t > 1 else t
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def axis_t(px: float, py: float, x: float, y: float, w: float, h: float, direction: tuple[float, float]) -> float:
    dx, dy = direction
    corners = ((x, y), (x + w, y), (x, y + h), (x + w, y + h))
    projs = [cx * dx + cy * dy for cx, cy in corners]
    lo, hi = min(projs), max(projs)
    if hi <= lo:
        return 0.0
    return (px * dx + py * dy - lo) / (hi - lo)


def render(size: int) -> bytes:
    px_aa = 4 if size <= 48 else 2
    super_s = size * px_aa
    buf = [[0.0, 0.0, 0.0, 0.0] for _ in range(super_s * super_s)]
    s = float(super_s)

    body_r = 0.22 * s
    inset = (0.16 if size <= 16 else 0.14) * s
    screen = (inset, inset * 1.05, s - inset * 2.0, s - inset * 2.15, min(s - inset * 2.0, s - inset * 2.15) * 0.18)

    for iy in range(super_s):
        py = iy + 0.5
        for ix in range(super_s):
            px = ix + 0.5
            pix = buf[iy * super_s + ix]

            d_body = sd_round_rect(px, py, 0, 0, s, s, body_r)
            t = axis_t(px, py, 0, 0, s, s, DIR_V)
            br, bg, bb = lerp((58, 58, 60), (36, 36, 38), t)
            blend(pix, br, bg, bb, 255, cover(d_body))

            # Hairline rim
            rim = abs(d_body + 0.35) 
            blend(pix, 96, 96, 100, 180, cover(rim - 0.55) * cover(d_body))

            d_scr = sd_round_rect(px, py, *screen)
            st = axis_t(px, py, *screen[:4], DIR_V)
            sr, sg, sb = lerp((236, 236, 240), (196, 196, 204), st)
            blend(pix, sr, sg, sb, 255, cover(d_scr))
            if d_scr < 0.5 and py < screen[1] + screen[3] * 0.45:
                wash = 1.0 - (py - screen[1]) / max(1.0, screen[3] * 0.45)
                blend(pix, 255, 255, 255, 90 * wash, cover(d_scr))

    out = bytearray(size * size * 4)
    inv = 1.0 / (px_aa * px_aa)
    for y in range(size):
        for x in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for oy in range(px_aa):
                for ox in range(px_aa):
                    p = buf[(y * px_aa + oy) * super_s + (x * px_aa + ox)]
                    acc[0] += p[0] * p[3]
                    acc[1] += p[1] * p[3]
                    acc[2] += p[2] * p[3]
                    acc[3] += p[3]
            a = acc[3] * inv
            i = (y * size + x) * 4
            if a > 1e-6:
                out[i] = int(min(255, acc[0] * inv / a + 0.5))
                out[i + 1] = int(min(255, acc[1] * inv / a + 0.5))
                out[i + 2] = int(min(255, acc[2] * inv / a + 0.5))
                out[i + 3] = int(min(255, a * 255 + 0.5))
    return bytes(out)


def png_bytes(w: int, h: int, rgba: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + rgba[y * w * 4 : (y + 1) * w * 4] for y in range(h))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def write_ico(path: Path, sizes: list[int]) -> None:
    images = [png_bytes(s, s, render(s)) for s in sizes]
    count = len(images)
    offset = 6 + 16 * count
    entries = bytearray()
    payload = bytearray()
    for s, data in zip(sizes, images):
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
        payload += data
        offset += len(data)
    path.write_bytes(struct.pack("<HHH", 0, 1, count) + entries + payload)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_ico(OUT_DIR / "app.ico", [16, 24, 32, 48, 64, 128, 256])
    (OUT_DIR / "logo.png").write_bytes(png_bytes(256, 256, render(256)))
    print("wrote", OUT_DIR / "app.ico")


if __name__ == "__main__":
    main()
