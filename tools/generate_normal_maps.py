"""Generate the normal mapping demo's brick and stone slab maps (numpy + Pillow).

Run from any directory: python tools/generate_normal_maps.py
Writes bricks_* and tiles_* (diffuse, 8-bit paletted; normal, RGBA) to examples/shared/render.
256x256 and tileable; tangent-space normals in the glTF convention, green = image up.
"""

from pathlib import Path
import numpy as np
from PIL import Image

N = 256
rng = np.random.default_rng(7)
out_dir = Path(__file__).resolve().parent.parent / "examples" / "shared" / "render"


def tile_noise(n, cells, octaves=4, seed=0):
    """Tileable value noise in [0, 1]: bilinear on a wrapped random grid."""
    r = np.random.default_rng(seed)
    acc = np.zeros((n, n))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        c = cells * (2 ** o)
        grid = r.random((c, c))
        y, x = np.mgrid[0:n, 0:n] * (c / n)
        x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
        fx, fy = x - x0, y - y0
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        x1, y1 = (x0 + 1) % c, (y0 + 1) % c
        x0, y0 = x0 % c, y0 % c
        v = (grid[y0, x0] * (1 - fx) * (1 - fy) + grid[y0, x1] * fx * (1 - fy)
             + grid[y1, x0] * (1 - fx) * fy + grid[y1, x1] * fx * fy)
        acc += v * amp
        total += amp
        amp *= 0.5
    return acc / total


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def normals_from_height(h, strength):
    # Central differences, wrapped. Row r grows downward; image up is -r.
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dr = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -dx * strength, dr * strength, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / l, ny / l, nz / l], -1)
    rgb = np.clip(np.round((n * 0.5 + 0.5) * 255), 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, np.full(h.shape, 255, np.uint8)]), "RGBA")


def save_diffuse(rgb, name):
    img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")
    img.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(f"{out_dir}/{name}")


# ----------------------------------------------------------------- bricks --
def bricks():
    rows, per_row, mortar, bevel = 8, 4, 3.0, 7.0
    bh, bw = N / rows, N / per_row
    y, x = np.mgrid[0:N, 0:N].astype(float)
    row = np.floor(y / bh).astype(int)
    xs = (x + (row % 2) * bw * 0.5) % N
    col = np.floor(xs / bw).astype(int)
    lx, ly = xs - col * bw, y - row * bh
    # Distance to the brick's own edge, mortar eating into it.
    edge = np.minimum(np.minimum(lx, bw - lx), np.minimum(ly, bh - ly)) - mortar * 0.5
    brick_id = row * per_row + col
    rnd = rng.random((rows * per_row, 4))
    # Chipped edges: the bevel starts at a noisy distance.
    chip = tile_noise(N, 16, 3, seed=11)
    edge = edge - (chip - 0.5) * 5.0
    profile = smoothstep(0.0, bevel, edge)
    profile = np.sqrt(profile)  # rounded shoulder
    tilt = (rnd[brick_id, 0] - 0.5) * 0.25 * (lx / bw - 0.5) + (rnd[brick_id, 1] - 0.5) * 0.25 * (ly / bh - 0.5)
    rough = tile_noise(N, 8, 5, seed=3)
    pits = smoothstep(0.72, 0.85, tile_noise(N, 32, 2, seed=5))
    h = profile * (1.0 + tilt + (rough - 0.5) * 0.35 - pits * 0.35)
    mortar_h = (tile_noise(N, 64, 2, seed=9) - 0.5) * 0.12
    h = np.where(edge > 0, h, mortar_h)
    h = h * 6.0

    tone = rnd[brick_id, 2]
    base = np.stack([150 + 50 * tone, 70 + 25 * tone, 50 + 15 * tone], -1)
    dirt = tile_noise(N, 8, 4, seed=21)[..., None]
    rgb = base * (0.75 + 0.45 * dirt)
    mortar_rgb = np.array([150, 145, 135]) * (0.8 + 0.3 * tile_noise(N, 32, 3, seed=4))[..., None]
    rgb = np.where((edge > 0)[..., None], rgb, mortar_rgb)
    save_diffuse(rgb, "bricks_diffuse.png")
    normals_from_height(h, 1.6).save(f"{out_dir}/bricks_normal.png")


# ------------------------------------------------------------------ tiles --
def tiles():
    per_row, gap, bevel = 2, 5.0, 10.0
    ts = N / per_row
    y, x = np.mgrid[0:N, 0:N].astype(float)
    tx, ty = np.floor(x / ts).astype(int), np.floor(y / ts).astype(int)
    lx, ly = x - tx * ts, y - ty * ts
    edge = np.minimum(np.minimum(lx, ts - lx), np.minimum(ly, ts - ly)) - gap * 0.5
    edge = edge - (tile_noise(N, 12, 3, seed=31) - 0.5) * 6.0
    tid = ty * per_row + tx
    rnd = rng.random((per_row * per_row, 4))
    profile = np.sqrt(smoothstep(0.0, bevel, edge))
    # Worn slabs: broad dents, fine grain and a few cracks.
    worn = tile_noise(N, 4, 4, seed=33)
    grain = tile_noise(N, 32, 3, seed=35)
    crack_n = tile_noise(N, 6, 5, seed=37)
    crack = 1.0 - smoothstep(0.0, 0.012, np.abs(crack_n - 0.5))
    h = profile * (1.0 + (worn - 0.5) * 0.6 + (grain - 0.5) * 0.25 - crack * 0.5)
    h = np.where(edge > 0, h, (grain - 0.5) * 0.1 - 0.1)
    h = h * 7.0

    tone = rnd[tid, 0]
    base = np.stack([125 + 30 * tone, 120 + 25 * tone, 110 + 20 * tone], -1)
    rgb = base * (0.8 + 0.35 * tile_noise(N, 8, 4, seed=39))[..., None]
    rgb = rgb * (1.0 - crack * 0.35)[..., None]
    gap_rgb = np.array([70, 65, 60]) * (0.8 + 0.4 * grain)[..., None]
    rgb = np.where((edge > 0)[..., None], rgb, gap_rgb)
    save_diffuse(rgb, "tiles_diffuse.png")
    normals_from_height(h, 1.4).save(f"{out_dir}/tiles_normal.png")


bricks()
tiles()
