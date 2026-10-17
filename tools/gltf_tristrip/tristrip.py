"""
glTF triangle list -> triangle strip converter for the AthenaEnv renderer.

The PS2 renderer de-indexes every mesh and runs the VU1 microprogram once per
emitted vertex, so a triangle list costs 3 vertices per triangle. A strip costs
~1 vertex per triangle plus 2 per strip, which is where the savings come from.

What it does, per triangle-list primitive (mode 4):
  1. welds bit-identical vertices (all attributes equal) so shared edges become
     visible -- de-indexed exports would otherwise have no adjacency at all;
  2. builds strips greedily (fewest-neighbour start triangle, best of the three
     rotations, extended forwards and backwards), honouring the winding;
  3. joins every strip of the primitive into ONE strip (see below);
  4. writes the primitive back as mode 5 (TRIANGLE_STRIP), indexed.

Joining strips -- two modes:
  * restart (default): strips are separated by a primitive restart index (the
    maximum value of the index type). The engine marks the first two vertices
    after it as "no kick" (GS ADC bit), so joining costs no vertex at all. The
    renderer tracks winding by position, so every strip has to keep its parity:
    even-length strips go first (positions stay even), then odd-length ones
    alternate as-is / reversed -- an odd-length strip reversed is wound the other
    way, which is exactly what an odd start position needs. No padding ever.
    glTF forbids restart values in indices, so the file declares the
    ATHENA_strip_restart extension (required): only AthenaEnv loads it.
  * --no-restart: plain glTF, strips stitched with degenerate triangles
    (2-3 extra vertices per strip). Loads anywhere.

The renderer restarts the strip at every primitive (material group), so strips
are never stitched across primitives. Primitives that already are strips, or are
something else (points, lines, fans) or have morph targets, are copied as-is.

The whole binary payload is repacked into a single .bin next to the output
.gltf, dropping the old geometry. Images referenced by URI are left alone.

Wavefront .obj input is accepted too and written out as glTF (the engine loads
either). V is flipped exactly like loadOBJ does, faces are fan-triangulated,
and every usemtl run becomes its own primitive. Materials come from the mtllib
(Kd, d, map_Kd); an OBJ without one gets no glTF material at all, so the
engine keeps using the texture passed to `new RenderData(path, texture)`.

Usage:
    python tristrip.py input.gltf|input.obj output.gltf [--no-restart] [--no-weld] [--quiet]
"""

import argparse
import base64
import heapq
import json
import os
import struct
import sys

import numpy as np

MODE_TRIANGLES = 4
MODE_TRIANGLE_STRIP = 5

RESTART = -1                      # restart marker inside a strip list
RESTART_EXT = "ATHENA_strip_restart"

COMPONENT_DTYPE = {
    5120: np.int8,
    5121: np.uint8,
    5122: np.int16,
    5123: np.uint16,
    5125: np.uint32,
    5126: np.float32,
}

TYPE_COMPONENTS = {
    "SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
    "MAT2": 4, "MAT3": 9, "MAT4": 16,
}

ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963


# ---------------------------------------------------------------------------
# glTF I/O
# ---------------------------------------------------------------------------

def load_buffers(gltf, base_dir):
    buffers = []
    for buf in gltf.get("buffers", []):
        uri = buf.get("uri")
        if uri is None:
            sys.exit("error: GLB / buffer without uri is not supported, export as .gltf + .bin")
        if uri.startswith("data:"):
            buffers.append(base64.b64decode(uri.split(",", 1)[1]))
        else:
            with open(os.path.join(base_dir, uri), "rb") as f:
                buffers.append(f.read())
    return buffers


def read_accessor(gltf, buffers, index):
    """Returns (count, ncomp) array of the accessor's raw components."""
    acc = gltf["accessors"][index]
    if "sparse" in acc:
        sys.exit("error: sparse accessors are not supported (accessor %d)" % index)

    dtype = np.dtype(COMPONENT_DTYPE[acc["componentType"]])
    ncomp = TYPE_COMPONENTS[acc["type"]]
    count = acc["count"]

    if "bufferView" not in acc:
        return np.zeros((count, ncomp), dtype=dtype)

    view = gltf["bufferViews"][acc["bufferView"]]
    data = buffers[view["buffer"]]
    offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    elem_size = dtype.itemsize * ncomp
    stride = view.get("byteStride", elem_size)

    if stride == elem_size:
        arr = np.frombuffer(data, dtype=dtype, count=count * ncomp, offset=offset)
        return arr.reshape(count, ncomp).copy()

    out = np.empty((count, ncomp), dtype=dtype)
    for i in range(count):
        out[i] = np.frombuffer(data, dtype=dtype, count=ncomp, offset=offset + i * stride)
    return out


class BufferWriter:
    """Accumulates bufferViews into one binary blob."""

    def __init__(self, gltf):
        self.gltf = gltf
        self.blob = bytearray()
        self.views = []

    def add_view(self, data, target=None, stride=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        view = {"buffer": 0, "byteOffset": len(self.blob), "byteLength": len(data)}
        if stride is not None:
            view["byteStride"] = stride
        if target is not None:
            view["target"] = target
        self.blob.extend(data)
        self.views.append(view)
        return len(self.views) - 1


# ---------------------------------------------------------------------------
# OBJ input
# ---------------------------------------------------------------------------

def parse_mtl(path):
    materials = {}
    cur = None
    if not os.path.exists(path):
        print("warning: mtllib %s not found, materials dropped" % path)
        return materials
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "newmtl":
                cur = materials.setdefault(" ".join(parts[1:]), {"Kd": [1.0, 1.0, 1.0], "d": 1.0})
            elif cur is None:
                continue
            elif parts[0] == "Kd":
                cur["Kd"] = [float(v) for v in parts[1:4]]
            elif parts[0] == "d":
                cur["d"] = float(parts[1])
            elif parts[0] == "map_Kd":
                cur["map_Kd"] = parts[-1]
    return materials


def obj_to_gltf(path):
    """Reads an OBJ into an in-memory (gltf dict, [buffer]) pair."""
    base_dir = os.path.dirname(os.path.abspath(path))
    v, vt, vn = [], [], []
    groups = []          # [material name, [ (vi, ti, ni) corners of triangles ]]
    mtl = {}
    cur_mat = None

    def corner(tok):
        f = tok.split("/")
        def idx(i, n):
            if i >= len(f) or f[i] == "":
                return -1
            k = int(f[i])
            return k - 1 if k > 0 else n + k
        return (idx(0, len(v)), idx(1, len(vt)), idx(2, len(vn)))

    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            if not parts:
                continue
            tag = parts[0]
            if tag == "v":
                v.append([float(x) for x in parts[1:4]])
            elif tag == "vt":
                vt.append([float(parts[1]), float(parts[2]) if len(parts) > 2 else 0.0])
            elif tag == "vn":
                vn.append([float(x) for x in parts[1:4]])
            elif tag == "mtllib":
                mtl.update(parse_mtl(os.path.join(base_dir, " ".join(parts[1:]))))
            elif tag == "usemtl":
                cur_mat = " ".join(parts[1:])
            elif tag == "f":
                cs = [corner(t) for t in parts[1:]]
                if not groups or groups[-1][0] != cur_mat:
                    groups.append([cur_mat, []])
                for i in range(1, len(cs) - 1):
                    groups[-1][1].extend((cs[0], cs[i], cs[i + 1]))

    has_uv = len(vt) > 0
    has_n = len(vn) > 0
    gltf = {"asset": {"version": "2.0", "generator": "AthenaEnv tristrip.py (from OBJ)"},
            "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": os.path.splitext(os.path.basename(path))[0]}],
            "meshes": [{"primitives": []}], "accessors": [], "bufferViews": []}
    blob = bytearray()

    def add(arr, gltf_type):
        arr = np.ascontiguousarray(arr, dtype=np.float32)
        gltf["bufferViews"].append({"buffer": 0, "byteOffset": len(blob), "byteLength": arr.nbytes})
        blob.extend(arr.tobytes())
        gltf["accessors"].append({"bufferView": len(gltf["bufferViews"]) - 1, "componentType": 5126,
                                  "count": int(arr.shape[0]), "type": gltf_type})
        return len(gltf["accessors"]) - 1

    use_materials = bool(mtl)
    mat_index = {}
    if use_materials:
        gltf["materials"] = []

    pos = np.array(v, dtype=np.float32)
    uv = np.array(vt, dtype=np.float32) if has_uv else None
    nrm = np.array(vn, dtype=np.float32) if has_n else None

    for name, corners in groups:
        c = np.array(corners, dtype=np.int64)
        attrs = {"POSITION": add(pos[c[:, 0]], "VEC3")}
        if has_n:
            attrs["NORMAL"] = add(nrm[np.maximum(c[:, 2], 0)], "VEC3")
        if has_uv:
            t = uv[np.maximum(c[:, 1], 0)].copy()
            t[:, 1] = 1.0 - t[:, 1]  # same flip as obj_transfer_vertex
            attrs["TEXCOORD_0"] = add(t, "VEC2")
        prim = {"attributes": attrs, "mode": MODE_TRIANGLES}

        if use_materials:
            if name not in mat_index:
                m = mtl.get(name, {"Kd": [1.0, 1.0, 1.0], "d": 1.0})
                gm = {"name": name or "default",
                      "pbrMetallicRoughness": {"baseColorFactor": m["Kd"] + [m["d"]],
                                               "metallicFactor": 0.0}}
                if "map_Kd" in m:
                    gltf.setdefault("images", []).append({"uri": m["map_Kd"]})
                    gltf.setdefault("textures", []).append({"source": len(gltf["images"]) - 1})
                    gm["pbrMetallicRoughness"]["baseColorTexture"] = {"index": len(gltf["textures"]) - 1}
                gltf["materials"].append(gm)
                mat_index[name] = len(gltf["materials"]) - 1
            prim["material"] = mat_index[name]

        gltf["meshes"][0]["primitives"].append(prim)

    gltf["buffers"] = [{"byteLength": len(blob)}]
    return gltf, [bytes(blob)]


# ---------------------------------------------------------------------------
# Welding
# ---------------------------------------------------------------------------

def weld(attr_arrays, indices, do_weld):
    """Merges vertices whose every attribute is bit-identical.

    Returns (unique attribute arrays, remapped triangle indices).
    """
    count = next(iter(attr_arrays.values())).shape[0]

    if not do_weld:
        return attr_arrays, indices

    # One row of raw bytes per vertex across all attributes.
    rows = np.concatenate(
        [np.ascontiguousarray(a).view(np.uint8).reshape(count, -1) for a in attr_arrays.values()],
        axis=1)
    rows = np.ascontiguousarray(rows)
    keys = rows.view(np.dtype((np.void, rows.shape[1]))).ravel()
    _, first, inverse = np.unique(keys, return_index=True, return_inverse=True)

    # Keep first-seen order so output stays roughly in input order.
    order = np.argsort(first)
    rank = np.empty_like(order)
    rank[order] = np.arange(order.size)

    remap = rank[inverse.ravel()]
    keep = first[order]

    welded = {name: a[keep] for name, a in attr_arrays.items()}
    return welded, remap[indices]


# ---------------------------------------------------------------------------
# Stripification
# ---------------------------------------------------------------------------

def tri_edge(strip):
    """Directed edge the next triangle of `strip` has to contain.

    Triangle j of a strip is (s[j], s[j+1], s[j+2]) when j is even and
    (s[j+1], s[j], s[j+2]) when odd. The next triangle (j = len-2) therefore
    shares the last two vertices, and -- given a consistently wound mesh --
    contains them in the direction opposite to how the current triangle does.
    """
    a, b = strip[-2], strip[-1]
    j = len(strip) - 2
    return (a, b) if j % 2 == 0 else (b, a)


def build_strips(tris, quiet):
    ntri = len(tris)
    if ntri == 0:
        return []

    # directed edge -> triangles containing it with that orientation
    edge_map = {}
    for t, (a, b, c) in enumerate(tris):
        for e in ((a, b), (b, c), (c, a)):
            edge_map.setdefault(e, []).append(t)

    def third(t, u, v):
        a, b, c = tris[t]
        for x in (a, b, c):
            if x != u and x != v:
                return x
        return None

    def neighbours(t):
        a, b, c = tris[t]
        out = []
        for u, v in ((a, b), (b, c), (c, a)):
            for n in edge_map.get((v, u), ()):
                if n != t:
                    out.append(n)
        return out

    used = bytearray(ntri)

    def degree(t):
        return sum(1 for n in neighbours(t) if not used[n])

    def extend(strip, taken):
        while True:
            e = tri_edge(strip)
            nxt = None
            best = None
            for t in edge_map.get(e, ()):
                if used[t] or t in taken:
                    continue
                d = third(t, e[0], e[1])
                if d is None:
                    continue
                # Prefer the candidate with fewer free neighbours: leaves the
                # well-connected ones for later strips to pass through.
                dg = degree(t)
                if best is None or dg < best:
                    best, nxt = dg, (t, d)
            if nxt is None:
                return
            taken.add(nxt[0])
            strip.append(nxt[1])

    def grow(t, rot):
        a, b, c = tris[t]
        strip = [a, b, c][rot:] + [a, b, c][:rot]
        taken = {t}
        extend(strip, taken)

        # Backwards: reverse and keep extending from the original start.
        # Reversing keeps the winding only when the vertex count is even; an odd
        # strip gets a leading duplicate so every triangle shifts one parity.
        rev = strip[::-1]
        if len(rev) % 2:
            rev = [rev[0]] + rev
        before = len(rev)
        extend(rev, taken)
        if len(rev) > before:
            strip = rev
        return strip, taken

    heap = [(degree(t), t) for t in range(ntri)]
    heapq.heapify(heap)

    strips = []
    remaining = ntri
    while heap:
        dg, t = heapq.heappop(heap)
        if used[t]:
            continue
        cur = degree(t)
        if cur != dg:
            heapq.heappush(heap, (cur, t))
            continue

        best = None
        for rot in range(3):
            strip, taken = grow(t, rot)
            if best is None or len(taken) > len(best[1]) or \
               (len(taken) == len(best[1]) and len(strip) < len(best[0])):
                best = (strip, taken)

        strip, taken = best
        touched = set()
        for x in taken:
            used[x] = 1
            touched.update(neighbours(x))
        remaining -= len(taken)
        for n in touched:
            if not used[n]:
                heapq.heappush(heap, (degree(n), n))
        strips.append(strip)

    return strips


def stitch(strips):
    """Joins strips into one with degenerate triangles, preserving winding.

    A strip only keeps its winding when it starts at an even position of the
    combined strip, so odd-length prefixes get one extra duplicate. Strips of
    even length are emitted first, then the odd ones in pairs, which keeps the
    number of parity fixes down.
    """
    even = [s for s in strips if len(s) % 2 == 0]
    odd = [s for s in strips if len(s) % 2 == 1]
    ordered = even + odd

    out = []
    for s in ordered:
        if out:
            out.append(out[-1])
            if len(out) % 2 == 0:
                # out had odd length before the bridge vertex: one more
                # duplicate so s starts on an even index.
                out.append(out[-1])
            out.append(s[0])
        out.extend(s)
    return out


def stitch_restart(strips):
    """Joins strips with RESTART markers, no extra vertices.

    Positions only count real vertices (the marker takes no slot on the
    engine side). Even-length strips keep the running position even; after
    that, an odd-length strip landing on an odd position is reversed, which
    flips its winding back to what the position parity expects.
    """
    even = [s for s in strips if len(s) % 2 == 0]
    odd = [s for s in strips if len(s) % 2 == 1]

    out = []
    pos = 0
    for s in even + odd:
        if out:
            out.append(RESTART)
        out.extend(s if pos % 2 == 0 else s[::-1])
        pos += len(s)
    return out


def strip_triangles(strip):
    """Non-degenerate triangles of a strip, in list winding (for validation).

    Decodes exactly like the engine: RESTART takes no position and suppresses
    the triangles completed by the next two vertices; winding alternates by
    position.
    """
    tris = []
    verts = []
    nokick = []
    pending = 0
    for v in strip:
        if v == RESTART:
            pending = 2
            continue
        verts.append(v)
        nokick.append(pending > 0)
        pending = max(0, pending - 1)

    for j in range(len(verts) - 2):
        if nokick[j + 2]:
            continue
        a, b, c = verts[j], verts[j + 1], verts[j + 2]
        if a == b or b == c or a == c:
            continue
        tris.append((a, b, c) if j % 2 == 0 else (b, a, c))
    return tris


def canonical(tri):
    a, b, c = tri
    m = min(range(3), key=lambda i: tri[i])
    return tuple(tri[m:] + tri[:m]) if isinstance(tri, list) else (tri[m:] + tri[:m])


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def convert(in_path, out_path, do_weld=True, quiet=False, restart=True):
    if in_path.lower().endswith(".obj"):
        gltf, buffers = obj_to_gltf(in_path)
    else:
        with open(in_path, "r", encoding="utf-8") as f:
            gltf = json.load(f)
        buffers = load_buffers(gltf, os.path.dirname(os.path.abspath(in_path)))
    writer = BufferWriter(gltf)

    old_accessors = gltf.get("accessors", [])
    new_accessors = []
    accessor_remap = {}  # old accessor -> new accessor (for untouched references)

    def copy_accessor(old_index, target=None):
        if old_index in accessor_remap:
            return accessor_remap[old_index]
        acc = dict(old_accessors[old_index])
        arr = read_accessor(gltf, buffers, old_index)
        new_index = add_accessor(arr, acc, target)
        accessor_remap[old_index] = new_index
        return new_index

    def add_accessor(arr, template, target):
        acc = {k: v for k, v in template.items() if k not in ("bufferView", "byteOffset", "sparse")}
        arr = np.ascontiguousarray(arr)
        count, ncomp = arr.shape
        elem = arr.dtype.itemsize * ncomp
        stride = None
        data = arr.tobytes()
        if target == ARRAY_BUFFER and elem % 4:
            # Vertex attributes need a 4-byte aligned stride.
            stride = (elem + 3) & ~3
            padded = np.zeros((count, stride), dtype=np.uint8)
            padded[:, :elem] = arr.view(np.uint8).reshape(count, elem)
            data = padded.tobytes()
        acc["bufferView"] = writer.add_view(data, target, stride)
        acc["count"] = int(count)
        new_accessors.append(acc)
        return len(new_accessors) - 1

    used_restart = False
    total_before = 0
    total_after = 0
    total_tris = 0

    for mi, mesh in enumerate(gltf.get("meshes", [])):
        for pi, prim in enumerate(mesh["primitives"]):
            mode = prim.get("mode", MODE_TRIANGLES)
            attrs = prim["attributes"]

            if mode != MODE_TRIANGLES or "targets" in prim:
                for name, idx in attrs.items():
                    attrs[name] = copy_accessor(idx, ARRAY_BUFFER)
                if "indices" in prim:
                    prim["indices"] = copy_accessor(prim["indices"], ELEMENT_ARRAY_BUFFER)
                if "targets" in prim:
                    for tgt in prim["targets"]:
                        for name, idx in tgt.items():
                            tgt[name] = copy_accessor(idx, ARRAY_BUFFER)
                if not quiet and mode == MODE_TRIANGLES:
                    print("  mesh %d prim %d: morph targets, left as a list" % (mi, pi))
                continue

            arrays = {name: read_accessor(gltf, buffers, idx) for name, idx in attrs.items()}
            vcount = next(iter(arrays.values())).shape[0]

            if "indices" in prim:
                indices = read_accessor(gltf, buffers, prim["indices"]).ravel().astype(np.int64)
            else:
                indices = np.arange(vcount, dtype=np.int64)

            indices = indices[: (len(indices) // 3) * 3]
            welded, idx = weld(arrays, indices, do_weld)

            tris_arr = idx.reshape(-1, 3)
            # Degenerate input triangles draw nothing; drop them.
            keep = (tris_arr[:, 0] != tris_arr[:, 1]) & (tris_arr[:, 1] != tris_arr[:, 2]) & \
                   (tris_arr[:, 0] != tris_arr[:, 2])
            tris = [tuple(int(v) for v in t) for t in tris_arr[keep]]

            strips = build_strips(tris, quiet)
            strip = stitch_restart(strips) if restart else stitch(strips)

            # Validation: the strip has to draw exactly the input triangles,
            # with the same winding.
            want = sorted(canonical(t) for t in tris)
            got = sorted(canonical(t) for t in strip_triangles(strip))
            if want != got:
                sys.exit("error: strip validation failed on mesh %d prim %d" % (mi, pi))

            before = len(indices)
            after = sum(1 for v in strip if v != RESTART)
            total_before += before
            total_after += after
            total_tris += len(tris)

            if not quiet:
                print("  mesh %d prim %d (%s): %d tris, %d -> %d verts (%d strips, %.2f verts/tri)" % (
                    mi, pi, mesh.get("name", ""), len(tris), before, after, len(strips),
                    after / max(1, len(tris))))

            for name in list(attrs.keys()):
                template = dict(old_accessors[attrs[name]])
                arr = welded[name]
                if name == "POSITION" and arr.dtype == np.float32:
                    template["min"] = [float(v) for v in arr.min(axis=0)]
                    template["max"] = [float(v) for v in arr.max(axis=0)]
                attrs[name] = add_accessor(arr, template, ARRAY_BUFFER)

            nverts = next(iter(welded.values())).shape[0]
            # The restart value is the type's maximum, so it must never be a
            # real vertex index.
            if nverts < 65535:
                dtype, ctype = np.uint16, 5123
            else:
                dtype, ctype = np.uint32, 5125
            restart_value = int(np.iinfo(dtype).max)
            idx_arr = np.array([restart_value if v == RESTART else v for v in strip], dtype=dtype)
            prim["indices"] = add_accessor(idx_arr.reshape(-1, 1),
                                           {"componentType": ctype, "type": "SCALAR"},
                                           ELEMENT_ARRAY_BUFFER)
            if restart:
                prim.setdefault("extensions", {})[RESTART_EXT] = {}
                used_restart = True
            prim["mode"] = MODE_TRIANGLE_STRIP

    # Everything else that points at accessors.
    for skin in gltf.get("skins", []):
        if "inverseBindMatrices" in skin:
            skin["inverseBindMatrices"] = copy_accessor(skin["inverseBindMatrices"])
    for anim in gltf.get("animations", []):
        for sampler in anim.get("samplers", []):
            sampler["input"] = copy_accessor(sampler["input"])
            sampler["output"] = copy_accessor(sampler["output"])

    # Images embedded in bufferViews.
    for image in gltf.get("images", []):
        if "bufferView" in image:
            view = gltf["bufferViews"][image["bufferView"]]
            data = buffers[view["buffer"]]
            off = view.get("byteOffset", 0)
            image["bufferView"] = writer.add_view(data[off: off + view["byteLength"]])

    if used_restart:
        for key in ("extensionsUsed", "extensionsRequired"):
            exts = gltf.setdefault(key, [])
            if RESTART_EXT not in exts:
                exts.append(RESTART_EXT)

    bin_name = os.path.splitext(os.path.basename(out_path))[0] + ".bin"
    gltf["accessors"] = new_accessors
    gltf["bufferViews"] = writer.views
    gltf["buffers"] = [{"uri": bin_name, "byteLength": len(writer.blob)}]

    out_dir = os.path.dirname(os.path.abspath(out_path))
    with open(os.path.join(out_dir, bin_name), "wb") as f:
        f.write(writer.blob)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(gltf, f, indent=1)

    if total_tris:
        print("total: %d tris, %d -> %d verts (%.1f%%), %.2f verts/tri (list = 3.00)" % (
            total_tris, total_before, total_after, 100.0 * total_after / total_before,
            total_after / total_tris))


def main():
    ap = argparse.ArgumentParser(description="Convert glTF/OBJ triangle lists to glTF triangle strips.")
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--no-weld", action="store_true",
                    help="do not merge identical vertices (only useful for already indexed meshes)")
    ap.add_argument("--no-restart", action="store_true",
                    help="stitch with degenerate triangles instead of restart indices (portable glTF)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if os.path.abspath(args.input) == os.path.abspath(args.output):
        sys.exit("error: output must be a different file than input")

    convert(args.input, args.output, do_weld=not args.no_weld, quiet=args.quiet,
            restart=not args.no_restart)


if __name__ == "__main__":
    main()
