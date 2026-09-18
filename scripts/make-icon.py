"""Generate AirScreen app.ico: a monitor on a stand casting Continuity-blue AirPlay arcs."""
from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "src" / "app"

BLUE = (10, 132, 255)  # Continuity blue


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


def render(size: int) -> bytes:
    """Draw in the design's 256-unit space, scaled to the supersampled buffer."""
    px_aa = 4 if size <= 48 else 2
    super_s = size * px_aa
    buf = [[0.0, 0.0, 0.0, 0.0] for _ in range(super_s * super_s)]
    k = super_s / 256.0  # design-units -> supersampled pixels

    body = (6 * k, 28 * k, 244 * k, 172 * k, 22 * k)
    screen = (14 * k, 36 * k, 228 * k, 156 * k, 14 * k)
    neck = (118 * k, 200 * k, 20 * k, 14 * k, 2 * k)
    base = (102 * k, 216 * k, 52 * k, 5 * k, 2.5 * k)
    cx, cy = 128 * k, 134 * k
    rings = ((25 * k, 1.75 * k, 0.60), (47 * k, 1.5 * k, 0.35), (67 * k, 1.5 * k, 0.18))

    for iy in range(super_s):
        py = iy + 0.5
        for ix in range(super_s):
            px = ix + 0.5
            pix = buf[iy * super_s + ix]

            # Body plate + hairline rim + top sheen.
            d_body = sd_round_rect(px, py, *body)
            br, bg, bb = lerp((64, 64, 68), (40, 40, 42), (py - body[1]) / body[3])
            blend(pix, br, bg, bb, 255, cover(d_body))
            blend(pix, 106, 106, 112, 255, cover(abs(d_body) - 0.625 * k))
            sheen = 1.0 - (py - body[1]) / (56 * k)
            if sheen > 0:
                blend(pix, 255, 255, 255, 0.08 * 255 * sheen, cover(d_body))

            # Dark screen.
            d_scr = sd_round_rect(px, py, *screen)
            sr, sg, sb = lerp((28, 28, 30), (14, 14, 16), (py - screen[1]) / screen[3])
            blend(pix, sr, sg, sb, 255, cover(d_scr))
            on_screen = cover(d_scr)

            # Soft glow behind the arcs.
            e = math.hypot((px - cx) / (48 * k), (py - 120 * k) / (32 * k))
            if e < 1.0:
                blend(pix, *BLUE, 0.06 * 255 * (1 - e), on_screen)

            # Concentric signal arcs (top caps, like AirPlay waves).
            dist = math.hypot(px - cx, py - cy)
            for rr, hw, op in rings:
                clip = cover(py - (cy - 0.4 * rr))  # keep the upper cap only
                blend(pix, *BLUE, op * 255, cover(abs(dist - rr) - hw) * on_screen * clip)
            blend(pix, *BLUE, 0.9 * 255, cover(dist - 6 * k) * on_screen)

            # Stand neck + base.
            blend(pix, 58, 58, 60, 255, cover(sd_round_rect(px, py, *neck)))
            blend(pix, 72, 72, 74, 255, cover(sd_round_rect(px, py, *base)))

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
