"""Generate the physics demo's dust and debris sprite atlas (stdlib only).

Run from any directory: python tools/generate_fx_atlas.py
Cells: soft dust puff, irregular solid fragment. Transparent gutters prevent bleed.
"""

import math
from pathlib import Path
import struct
import zlib


def chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


def generate():
    rows = bytearray()
    for y in range(32):
        rows.append(0)  # PNG filter: none
        for x in range(64):
            cell = x // 32
            u, v = ((x % 32) - 15.5) / 14, (y - 15.5) / 14
            radius = math.hypot(u, v)
            if cell == 0:
                cloud = 1 + 0.14 * math.sin(u * 13 + v * 7) * math.cos(v * 11)
                alpha = max(0, 1 - radius * cloud) ** 1.4 * 0.65
            else:
                edge = max(abs(u + v * 0.3), abs(v - u * 0.2), abs(u - v) * 0.7)
                alpha = min(1, max(0, (0.75 - edge) * 12))
            if x % 32 < 2 or x % 32 > 29 or y < 2 or y > 29:
                alpha = 0
            rows.extend((255, 255, 255, round(alpha * 255)))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 32, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    target = Path(__file__).resolve().parents[1] / "examples/shared/render/fx_atlas.png"
    target.write_bytes(generate())
    print(f"Generated {target.name}: 64x32 RGBA, {target.stat().st_size} bytes")
