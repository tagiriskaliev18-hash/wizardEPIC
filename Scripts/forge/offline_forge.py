#!/usr/bin/env python3
"""
offline_forge.py – Procedural asset generator for wizardEPIC.
Runs on stock Python 3.10+ (no numpy / Pillow).
Outputs:
  Generated/Textures/*.png   – FX sprites, environment detail, UI icons
  Generated/Audio/*.wav       – spell SFX, break SFX, ambient
  Generated/Meshes/*.obj      – environment and FX meshes
  Generated/Destruction/*.obj – fracture pieces per destructible shape
  Generated/layout.json       – arena placement data for ue_forge.py

Usage:  python offline_forge.py [--out DIR]   (default DIR = Generated/)
"""

from __future__ import annotations
import argparse, json, math, os, random, struct, sys, time, zlib
from pathlib import Path
from typing import List, Tuple

# ── Import contract ───────────────────────────────────────────────────────────
sys.path.insert(0, str(Path(__file__).resolve().parent))
import contract as C

# ═══════════════════════════════════════════════════════════════════════════════
# PNG writer (stdlib only)
# ═══════════════════════════════════════════════════════════════════════════════

def _png_chunk(tag: bytes, data: bytes) -> bytes:
    chunk = tag + data
    return struct.pack(">I", len(data)) + chunk + struct.pack(">I", zlib.crc32(chunk) & 0xFFFFFFFF)

def write_png(path: Path, width: int, height: int, pixels: list[list[tuple[int, ...]]]):
    """Write an RGBA PNG from pixels[row][col] = (R,G,B,A)."""
    has_alpha = len(pixels[0][0]) == 4
    color_type = 6 if has_alpha else 2  # RGBA or RGB
    bpp = 4 if has_alpha else 3
    raw = bytearray()
    for row in pixels:
        raw.append(0)  # filter: none
        for px in row:
            raw.extend(px[:bpp])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_png_chunk(b"IHDR", ihdr))
        f.write(_png_chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
        f.write(_png_chunk(b"IEND", b""))

# ═══════════════════════════════════════════════════════════════════════════════
# WAV writer
# ═══════════════════════════════════════════════════════════════════════════════

def write_wav(path: Path, samples: list[float], sr: int = 48000, channels: int = 1):
    """Write 16-bit WAV from float samples in [-1, 1]."""
    import wave as _wave
    path.parent.mkdir(parents=True, exist_ok=True)
    with _wave.open(str(path), "w") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        buf = bytearray(len(samples) * 2)
        for i, s in enumerate(samples):
            v = max(-32768, min(32767, int(s * 32767)))
            struct.pack_into("<h", buf, i * 2, v)
        wf.writeframes(bytes(buf))

# ═══════════════════════════════════════════════════════════════════════════════
# Noise & helpers
# ═══════════════════════════════════════════════════════════════════════════════

def _lerp(a, b, t):
    return a + (b - a) * t

def _smoothstep(t):
    return t * t * (3.0 - 2.0 * t)

def _hash2d(ix, iy, seed=42):
    """Simple integer hash → float in [0,1)."""
    h = (ix * 374761393 + iy * 668265263 + seed) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h & 0x7FFFFFFF) / 0x7FFFFFFF

def value_noise_2d(x, y, freq=1.0, seed=42):
    """Value noise in [0,1]."""
    x *= freq; y *= freq
    ix = int(math.floor(x)); iy = int(math.floor(y))
    fx = x - ix; fy = y - iy
    fx = _smoothstep(fx); fy = _smoothstep(fy)
    a = _lerp(_hash2d(ix, iy, seed), _hash2d(ix+1, iy, seed), fx)
    b = _lerp(_hash2d(ix, iy+1, seed), _hash2d(ix+1, iy+1, seed), fx)
    return _lerp(a, b, fy)

def fbm_2d(x, y, octaves=4, freq=1.0, lacunarity=2.0, gain=0.5, seed=42):
    """Fractal Brownian Motion."""
    total = 0.0; amp = 1.0; f = freq
    for o in range(octaves):
        total += value_noise_2d(x, y, f, seed + o * 7) * amp
        f *= lacunarity; amp *= gain
    return total / (2.0 - 2.0 ** (1 - octaves))

# ═══════════════════════════════════════════════════════════════════════════════
# Texture generators
# ═══════════════════════════════════════════════════════════════════════════════

def _make_solid(w, h, r, g, b, a=255):
    return [[(r, g, b, a)] * w for _ in range(h)]

def _radial_gradient(w, h, inner_rgb, outer_rgb, falloff=2.0, alpha_edge=0):
    """Radial gradient with alpha falloff."""
    cx, cy = w / 2.0, h / 2.0
    maxr = math.hypot(cx, cy)
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            d = math.hypot(x - cx, y - cy) / maxr
            d = min(1.0, d)
            t = d ** falloff
            r = int(_lerp(inner_rgb[0], outer_rgb[0], t))
            g = int(_lerp(inner_rgb[1], outer_rgb[1], t))
            b = int(_lerp(inner_rgb[2], outer_rgb[2], t))
            a = int(_lerp(255, alpha_edge, t))
            row.append((r, g, b, a))
        px.append(row)
    return px

def gen_fx_smoke(w=128, h=128):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 5, 4.0, seed=11)
            d = math.hypot(x - w/2, y - h/2) / (w / 2)
            a_falloff = max(0, 1.0 - d * 1.3)
            v = int(n * 200 * a_falloff)
            a = int(a_falloff * n * 255)
            row.append((v, v, v, min(255, a)))
        px.append(row)
    return px

def gen_fx_dust(w=128, h=128):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 3, 6.0, seed=22)
            d = math.hypot(x - w/2, y - h/2) / (w / 2)
            a_f = max(0, 1.0 - d * 1.5)
            v = int(180 + n * 60)
            row.append((v, int(v * 0.9), int(v * 0.75), int(a_f * n * 220)))
        px.append(row)
    return px

def gen_fx_leaf(w=64, h=64):
    px = _make_solid(w, h, 0, 0, 0, 0)
    cx, cy = w // 2, h // 2
    for y in range(h):
        for x in range(w):
            dx = (x - cx) / (w * 0.35)
            dy = (y - cy) / (h * 0.5)
            d = dx * dx + dy * dy
            if d < 1.0:
                n = fbm_2d(x / w, y / h, 2, 8.0, seed=33)
                g = int(60 + n * 120)
                px[y][x] = (int(30 + n * 40), g, int(20 + n * 30), int((1.0 - d) * 255))
    return px

def gen_fx_splinter(w=64, h=64):
    px = _make_solid(w, h, 0, 0, 0, 0)
    cx, cy = w // 2, h // 2
    for y in range(h):
        for x in range(w):
            dx = abs(x - cx) / (w * 0.08)
            dy = abs(y - cy) / (h * 0.45)
            if dx < 1.0 and dy < 1.0:
                n = fbm_2d(x / w, y / h, 2, 10.0, seed=44)
                v = int(140 + n * 80)
                a = int((1.0 - max(dx, dy)) * 255)
                px[y][x] = (v, int(v * 0.8), int(v * 0.55), a)
    return px

def gen_fx_shard(w=64, h=64):
    px = _make_solid(w, h, 0, 0, 0, 0)
    cx, cy = w // 2, h // 2
    for y in range(h):
        for x in range(w):
            dx = abs(x - cx) / (w * 0.12)
            dy = abs(y - cy) / (h * 0.42)
            if dx < 1.0 and dy < 1.0:
                d = max(dx, dy)
                edge = 1.0 - d
                n = fbm_2d(x / w, y / h, 2, 12.0, seed=55)
                v = int(180 + n * 70)
                px[y][x] = (v, v, int(v * 1.1) & 0xFF, int(edge * 255))
    return px

def gen_fx_spark(w=64, h=64):
    return _radial_gradient(w, h, (255, 240, 200), (255, 120, 20), 3.0, 0)

def gen_fx_ember(w=64, h=64):
    return _radial_gradient(w, h, (255, 200, 60), (255, 80, 0), 2.5, 0)

def gen_fx_flash(w=128, h=128):
    return _radial_gradient(w, h, (255, 255, 255), (255, 220, 160), 1.5, 0)

def gen_fx_glow(w=128, h=128):
    return _radial_gradient(w, h, (255, 255, 255), (200, 200, 255), 2.0, 0)

def gen_fx_sparkle(w=64, h=64):
    px = _make_solid(w, h, 0, 0, 0, 0)
    cx, cy = w // 2, h // 2
    for y in range(h):
        for x in range(w):
            dx = abs(x - cx); dy = abs(y - cy)
            # cross shape
            if dx < 2 or dy < 2:
                d = (dx + dy) / (w * 0.5)
                a = max(0, int((1.0 - d) * 255))
                px[y][x] = (255, 255, 255, a)
            else:
                # diagonal sparkle
                dd = abs(dx - dy)
                if dd < 2:
                    d = (dx + dy) / (w * 0.4)
                    a = max(0, int((1.0 - d) * 180))
                    px[y][x] = (255, 255, 240, a)
    return px

def gen_fx_rune(w=128, h=128):
    """Magic rune circle with inscribed geometry."""
    px = _make_solid(w, h, 0, 0, 0, 0)
    cx, cy = w / 2.0, h / 2.0
    r1 = w * 0.42; r2 = w * 0.35; r3 = w * 0.15
    for y in range(h):
        for x in range(w):
            d = math.hypot(x - cx, y - cy)
            # outer ring
            ring1 = abs(d - r1) < 2.0
            ring2 = abs(d - r2) < 1.5
            ring3 = abs(d - r3) < 1.5
            # inner triangle
            angle = math.atan2(y - cy, x - cx)
            tri = False
            for k in range(3):
                a0 = k * 2 * math.pi / 3
                a1 = (k + 1) * 2 * math.pi / 3
                x0 = cx + r2 * math.cos(a0); y0 = cy + r2 * math.sin(a0)
                x1 = cx + r2 * math.cos(a1); y1 = cy + r2 * math.sin(a1)
                # line distance
                dx = x1 - x0; dy_ = y1 - y0
                ln = math.hypot(dx, dy_)
                if ln > 0:
                    dist = abs((x - x0) * dy_ - (y - y0) * dx) / ln
                    if dist < 1.5:
                        t = ((x - x0) * dx + (y - y0) * dy_) / (ln * ln)
                        if 0 <= t <= 1:
                            tri = True
            if ring1 or ring2 or ring3 or tri:
                fade = max(0, 1.0 - d / (w * 0.48))
                px[y][x] = (255, 255, 255, int(fade * 255))
    return px

FX_TEX_GENERATORS = {
    "T_FX_Smoke": gen_fx_smoke,
    "T_FX_Dust": gen_fx_dust,
    "T_FX_Leaf": gen_fx_leaf,
    "T_FX_Splinter": gen_fx_splinter,
    "T_FX_Shard": gen_fx_shard,
    "T_FX_Spark": gen_fx_spark,
    "T_FX_Ember": gen_fx_ember,
    "T_FX_Flash": gen_fx_flash,
    "T_FX_Glow": gen_fx_glow,
    "T_FX_Sparkle": gen_fx_sparkle,
    "T_FX_Rune": gen_fx_rune,
}

# ── Environment textures ─────────────────────────────────────────────────────

def gen_env_detail_noise(w=256, h=256, seed=100):
    """Generic tileable noise for environment materials."""
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 5, 4.0, seed=seed)
            v = int(n * 255)
            row.append((v, v, v, 255))
        px.append(row)
    return px

def gen_ground_tex(w=256, h=256):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 5, 3.0, seed=200)
            r = int(100 + n * 80)
            g = int(90 + n * 70)
            b = int(60 + n * 50)
            row.append((r, g, b, 255))
        px.append(row)
    return px

def gen_rock_tex(w=256, h=256):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 6, 5.0, seed=300)
            v = int(80 + n * 100)
            row.append((v, int(v * 0.95), int(v * 0.9), 255))
        px.append(row)
    return px

def gen_sandstone_tex(w=256, h=256):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 4, 4.0, seed=400)
            r = int(180 + n * 60)
            g = int(150 + n * 50)
            b = int(100 + n * 40)
            row.append((r, g, b, 255))
        px.append(row)
    return px

def gen_wood_tex(w=256, h=256):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            grain = math.sin(x / w * 20.0 + fbm_2d(x / w, y / h, 3, 2.0, seed=500) * 4) * 0.5 + 0.5
            r = int(120 + grain * 80)
            g = int(80 + grain * 50)
            b = int(40 + grain * 30)
            row.append((r, g, b, 255))
        px.append(row)
    return px

def gen_crystal_tex(w=256, h=256):
    px = []
    for y in range(h):
        row = []
        for x in range(w):
            n = fbm_2d(x / w, y / h, 3, 6.0, seed=600)
            r = int(100 + n * 100)
            g = int(200 + n * 55)
            b = int(230 + n * 25)
            row.append((min(255, r), min(255, g), min(255, b), 255))
        px.append(row)
    return px

ENV_TEX_GENERATORS = {
    "T_DetailNoise":  gen_env_detail_noise,
    "T_Ground":       gen_ground_tex,
    "T_Rock":         gen_rock_tex,
    "T_Sandstone":    gen_sandstone_tex,
    "T_Wood":         gen_wood_tex,
    "T_Crystal":      gen_crystal_tex,
}

# ── UI icon generators ────────────────────────────────────────────────────────

def gen_spell_icon(element: str, w=256, h=256):
    """Generates a stylised elemental glyph icon."""
    ec = C.ELEMENT_COLORS[element]
    base = _radial_gradient(w, h, ec, (ec[0]//3, ec[1]//3, ec[2]//3), 1.8, 0)
    # Draw simple glyph overlay
    cx, cy = w // 2, h // 2
    r = w * 0.3
    if element == "Fire":
        # flame shape
        for y in range(h):
            for x in range(w):
                dy = (cy - y) / r
                dx_ = abs(x - cx) / (r * (0.3 + 0.7 * max(0, 1.0 - dy * 0.8)))
                if dy > -0.5 and dy < 1.5 and dx_ < 1.0:
                    t = 1.0 - max(dx_, max(0, -dy))
                    if t > 0.1:
                        base[y][x] = (255, min(255, 200 + int(t * 55)), int(t * 100), int(t * 255))
    elif element == "Earth":
        # triangle/mountain
        for y in range(h):
            for x in range(w):
                dy = (y - cy) / r
                dx_ = abs(x - cx) / r
                if dy > -0.6 and dx_ < (0.8 - dy * 0.5) and dy < 0.8:
                    t = 1.0 - max(dx_, abs(dy)) * 0.8
                    if t > 0:
                        base[y][x] = (min(255, ec[0] + int(t * 60)), min(255, ec[1] + int(t * 40)), ec[2], int(t * 255))
    elif element == "Wind":
        # swirl lines
        for y in range(h):
            for x in range(w):
                dx_ = x - cx; dy_ = y - cy
                d = math.hypot(dx_, dy_)
                if 0.2 * r < d < 1.2 * r:
                    angle = math.atan2(dy_, dx_) + d / r * 2
                    v = (math.sin(angle * 3) * 0.5 + 0.5)
                    if v > 0.6:
                        t = (v - 0.6) / 0.4
                        base[y][x] = (min(255, ec[0] + int(t * 40)), min(255, ec[1] + int(t * 20)), min(255, ec[2] + int(t * 20)), int(t * 240))
    else:  # Lightning
        # bolt
        for y in range(h):
            for x in range(w):
                dy = (y - cy + r * 0.4) / (r * 1.6)
                if 0 <= dy <= 1.0:
                    zag = cx + math.sin(dy * 3.5) * r * 0.25
                    dx_ = abs(x - zag)
                    if dx_ < r * 0.08:
                        t = 1.0 - dx_ / (r * 0.08)
                        base[y][x] = (255, 255, min(255, 220 + int(t * 35)), int(t * 255))
    return base

# ═══════════════════════════════════════════════════════════════════════════════
# OBJ mesh writers
# ═══════════════════════════════════════════════════════════════════════════════

def _sub_v3(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross_v3(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _face_normal(verts, face):
    """Compute unit normal for a polygon face (1-indexed) using Newell's method."""
    nx = ny = nz = 0.0
    m = len(face)
    for k in range(m):
        v0 = verts[face[k] - 1]
        v1 = verts[face[(k + 1) % m] - 1]
        nx += (v0[1] - v1[1]) * (v0[2] + v1[2])
        ny += (v0[2] - v1[2]) * (v0[0] + v1[0])
        nz += (v0[0] - v1[0]) * (v0[1] + v1[1])
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length > 1e-9:
        return (nx / length, ny / length, nz / length)
    return (0.0, 0.0, 1.0)


def _compute_smooth_normals(verts, faces):
    """Compute area-weighted smooth per-vertex normals for 1-indexed faces."""
    accum = [[0.0, 0.0, 0.0] for _ in verts]
    for face in faces:
        if len(face) < 3:
            continue
        v0 = verts[face[0] - 1]
        for k in range(1, len(face) - 1):
            v1 = verts[face[k] - 1]
            v2 = verts[face[k + 1] - 1]
            c = _cross_v3(_sub_v3(v1, v0), _sub_v3(v2, v0))
            for idx in (face[0] - 1, face[k] - 1, face[k + 1] - 1):
                accum[idx][0] += c[0]
                accum[idx][1] += c[1]
                accum[idx][2] += c[2]
    return [_normalize_v3(tuple(a)) for a in accum]


def _box_project_uv(v, normal, scale=100.0):
    """Triplanar/box UV projection from vertex position and face normal."""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if az >= ax and az >= ay:
        return (v[0] / scale, v[1] / scale)
    if ax >= ay:
        return (v[1] / scale, v[2] / scale)
    return (v[0] / scale, v[2] / scale)


def write_obj(path: Path, verts: list[tuple], faces: list[tuple], normals=None, uvs=None):
    """Write an OBJ with guaranteed valid vertex normals (vn), UVs (vt), and v/vt/vn faces."""
    path.parent.mkdir(parents=True, exist_ok=True)
    valid_faces = [fc for fc in faces if len(fc) >= 3]

    use_vertex_normals = normals is not None and len(normals) == len(verts)
    use_vertex_uvs = uvs is not None and len(uvs) == len(verts)

    out_normals = list(normals) if use_vertex_normals else []
    out_uvs = list(uvs) if use_vertex_uvs else []
    face_records = []

    for face in valid_faces:
        fn = _face_normal(verts, face)
        if not use_vertex_normals:
            vn_idx = len(out_normals) + 1
            out_normals.append(fn)
        else:
            vn_idx = None

        corners = []
        for vi in face:
            v_pos = verts[vi - 1]
            n_idx = vi if use_vertex_normals else vn_idx
            if use_vertex_uvs:
                t_idx = vi
            else:
                n_for_uv = out_normals[n_idx - 1]
                out_uvs.append(_box_project_uv(v_pos, n_for_uv))
                t_idx = len(out_uvs)
            corners.append(f"{vi}/{t_idx}/{n_idx}")
        face_records.append("f " + " ".join(corners) + "\n")

    with open(path, "w", encoding="utf-8") as f:
        f.write("# wizardEPIC offline_forge\n")
        for v in verts:
            f.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
        for uv in out_uvs:
            f.write(f"vt {uv[0]:.4f} {uv[1]:.4f}\n")
        for n in out_normals:
            f.write(f"vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}\n")
        for line in face_records:
            f.write(line)

# ── FX mesh generators ───────────────────────────────────────────────────────

def gen_quad():
    """100x100 plane in XY, centred, UV 0..1."""
    verts = [(-50, -50, 0), (50, -50, 0), (50, 50, 0), (-50, 50, 0)]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    normals = [(0, 0, 1)] * 4
    faces = [(1, 2, 3, 4)]
    return verts, faces, normals, uvs

def gen_icosphere(radius=50, subdivisions=2):
    """Icosphere with smooth normals."""
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [
        (-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
        (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
        (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1),
    ]
    # Normalize
    verts = [_normalize_v3(v) for v in verts]
    faces = [
        (0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),
        (1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),
        (3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),
        (4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1),
    ]
    for _ in range(subdivisions):
        mid_cache = {}
        new_faces = []
        for f in faces:
            a, b, c = f
            ab = _midpoint(a, b, verts, mid_cache)
            bc = _midpoint(b, c, verts, mid_cache)
            ca = _midpoint(c, a, verts, mid_cache)
            new_faces.extend([(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)])
        faces = new_faces
    # Scale and 1-index
    scaled = [(v[0] * radius, v[1] * radius, v[2] * radius) for v in verts]
    normals = list(verts)  # unit sphere = normals
    faces_1 = [tuple(i + 1 for i in f) for f in faces]
    return scaled, faces_1, normals, None

def _normalize_v3(v):
    l = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    return (v[0]/l, v[1]/l, v[2]/l) if l > 0 else (0, 0, 1)

def _midpoint(a, b, verts, cache):
    key = (min(a, b), max(a, b))
    if key in cache:
        return cache[key]
    va = verts[a]; vb = verts[b]
    mid = _normalize_v3(((va[0]+vb[0])/2, (va[1]+vb[1])/2, (va[2]+vb[2])/2))
    idx = len(verts)
    verts.append(mid)
    cache[key] = idx
    return idx

def gen_ring(outer=100, inner=75, segments=32):
    """Flat annulus in XY."""
    verts = []
    faces = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        ca, sa = math.cos(a), math.sin(a)
        verts.append((outer * ca, outer * sa, 0))
        verts.append((inner * ca, inner * sa, 0))
    for i in range(segments):
        j = (i + 1) % segments
        o1 = i * 2 + 1; i1 = i * 2 + 2
        o2 = j * 2 + 1; i2 = j * 2 + 2
        faces.append((o1, o2, i2, i1))
    return verts, faces, None, None

def gen_pillar(base_w=110, height=400, segments=8):
    """Rock pillar, base centred at origin."""
    rng = random.Random(777)
    verts = []
    faces = []
    rings = 6
    for iy in range(rings + 1):
        t = iy / rings
        y = t * height
        r = base_w / 2 * (1.0 - t * 0.15)  # slight taper
        for ix in range(segments):
            a = 2 * math.pi * ix / segments
            noise = rng.uniform(0.85, 1.15)
            verts.append((r * math.cos(a) * noise, r * math.sin(a) * noise, y))
    # Faces
    for iy in range(rings):
        for ix in range(segments):
            jx = (ix + 1) % segments
            a = iy * segments + ix + 1
            b = iy * segments + jx + 1
            c = (iy + 1) * segments + jx + 1
            d = (iy + 1) * segments + ix + 1
            faces.append((a, b, c, d))
    # top cap
    top_start = rings * segments
    top_center = len(verts) + 1
    cx, cy = 0, 0
    for ix in range(segments):
        v = verts[top_start + ix]
        cx += v[0]; cy += v[1]
    verts.append((cx / segments, cy / segments, height))
    for ix in range(segments):
        jx = (ix + 1) % segments
        faces.append((top_start + ix + 1, top_start + jx + 1, top_center))
    return verts, faces, None, None

def gen_bolt():
    """Unit box 1x1x1 centred at origin with outward winding."""
    s = 0.5
    verts = [
        (-s, -s, -s), (s, -s, -s), (s, s, -s), (-s, s, -s),
        (-s, -s, s),  (s, -s, s),  (s, s, s),  (-s, s, s),
    ]
    faces = [
        (1, 4, 3, 2),  # bottom (-Z)
        (5, 6, 7, 8),  # top (+Z)
        (1, 2, 6, 5),  # -Y
        (2, 3, 7, 6),  # +X
        (3, 4, 8, 7),  # +Y
        (4, 1, 5, 8),  # -X
    ]
    return verts, faces, None, None

def gen_chip():
    """Irregular flat chip ~20 cm."""
    rng = random.Random(888)
    verts = []
    n = 7
    for i in range(n):
        a = 2 * math.pi * i / n
        r = 10 * rng.uniform(0.6, 1.0)
        verts.append((r * math.cos(a), r * math.sin(a), rng.uniform(-1, 1)))
    # top center
    verts.append((0, 0, 1))
    # bottom center
    verts.append((0, 0, -1))
    faces = []
    tc = n + 1; bc = n + 2
    for i in range(n):
        j = (i + 1) % n
        faces.append((i + 1, j + 1, tc))
        faces.append((j + 1, i + 1, bc))
    return verts, faces, None, None

def gen_shard():
    """Elongated crystal shard ~20x20x60."""
    verts = [
        (0, 0, 60),   # top point
        (-10, -7, 0), (10, -7, 0), (8, 10, 0), (-8, 10, 0),  # base ring
        (0, 0, -5),   # bottom point
    ]
    faces = [
        (1, 2, 3), (1, 3, 4), (1, 4, 5), (1, 5, 2),
        (6, 3, 2), (6, 4, 3), (6, 5, 4), (6, 2, 5),
    ]
    return verts, faces, None, None

FX_MESH_GENERATORS = {
    "SM_FX_Quad":   gen_quad,
    "SM_FX_Sphere": lambda: gen_icosphere(50, 2),
    "SM_FX_Ring":   gen_ring,
    "SM_FX_Pillar": gen_pillar,
    "SM_FX_Bolt":   gen_bolt,
    "SM_FX_Chip":   gen_chip,
    "SM_FX_Shard":  gen_shard,
}

# ── Environment mesh generators & terrain elevation ──────────────────────────

def _build_structure_pads():
    """Return list of (x, y, pad_radius, pad_z) for every placed structure."""
    shape_map = {name: half_ext for name, _mat, half_ext, _np, _hp in C.DESTRUCTIBLE_SHAPES}
    pads = []
    for name, (sx, sy, _sz, _yaw) in C.STRUCTURE_PLACEMENTS:
        hx, hy, _hz = shape_map.get(name, (100.0, 100.0, 100.0))
        r_pad = math.hypot(hx, hy) + 60.0
        pads.append((sx, sy, r_pad, terrain_bowl_raw_z(sx, sy)))
    return pads


def terrain_bowl_raw_z(x, y, radius=C.BOWL_RADIUS):
    """Raw concave bowl + FBM noise elevation before structure pad flattening."""
    u = x / (2.0 * radius) + 0.5
    v_ = y / (2.0 * radius) + 0.5
    d = math.hypot(x, y) / radius
    z = -60.0 * d * d
    z += fbm_2d(u, v_, 3, 4.0, seed=700) * 40.0 - 20.0
    return z


def terrain_bowl_z(x, y, radius=C.BOWL_RADIUS, pads=None):
    """Bowl elevation with smooth flat pads under structure footprints."""
    z = terrain_bowl_raw_z(x, y, radius)
    if pads is None:
        pads = _build_structure_pads()
    blend_margin = 180.0
    for sx, sy, r_pad, z_pad in pads:
        dist = math.hypot(x - sx, y - sy)
        if dist <= r_pad:
            return z_pad
        if dist < r_pad + blend_margin:
            t = _smoothstep((dist - r_pad) / blend_margin)
            z = _lerp(z_pad, z, t)
    return z


def make_terrain_sampler(verts, radius=3600, subdivs=32):
    """Build exact bilinear/triangle sampler matching the generated SM_Terrain mesh."""
    stride = subdivs + 1
    cell = (2.0 * radius) / subdivs

    def sample(x, y):
        fx = max(0.0, min(float(subdivs) - 1e-6, (x + radius) / cell))
        fy = max(0.0, min(float(subdivs) - 1e-6, (y + radius) / cell))
        ix = int(fx)
        iy = int(fy)
        tx = fx - ix
        ty = fy - iy
        z00 = verts[iy * stride + ix][2]
        z10 = verts[iy * stride + ix + 1][2]
        z01 = verts[(iy + 1) * stride + ix][2]
        z11 = verts[(iy + 1) * stride + ix + 1][2]
        return _lerp(_lerp(z00, z10, tx), _lerp(z01, z11, tx), ty)

    return sample


def gen_terrain_bowl(radius=3600, subdivs=32):
    """Concave bowl terrain mesh with flat structure pads and smooth normals."""
    verts = []; faces = []; uvs = []
    pads = _build_structure_pads()
    for iy in range(subdivs + 1):
        for ix in range(subdivs + 1):
            u = ix / subdivs; v_ = iy / subdivs
            x = (u - 0.5) * 2 * radius
            y = (v_ - 0.5) * 2 * radius
            z = terrain_bowl_z(x, y, radius, pads)
            verts.append((x, y, z))
            uvs.append((u, v_))
    for iy in range(subdivs):
        for ix in range(subdivs):
            a = iy * (subdivs + 1) + ix + 1
            b = a + 1
            c = b + subdivs + 1
            d = a + subdivs + 1
            faces.append((a, b, c, d))
    normals = _compute_smooth_normals(verts, faces)
    return verts, faces, normals, uvs

def gen_cliff_ring(radius=3700, segments=24, height=600):
    """Ring of cliff geometry around the arena."""
    rng = random.Random(999)
    verts = []; faces = []
    rings = 4
    for iy in range(rings + 1):
        t = iy / rings
        z = t * height
        for ix in range(segments):
            a = 2 * math.pi * ix / segments
            r = radius + rng.uniform(-80, 80) + t * 150
            verts.append((r * math.cos(a), r * math.sin(a), z))
    for iy in range(rings):
        for ix in range(segments):
            jx = (ix + 1) % segments
            a = iy * segments + ix + 1
            b = iy * segments + jx + 1
            c = (iy+1) * segments + jx + 1
            d = (iy+1) * segments + ix + 1
            faces.append((a, b, c, d))
    return verts, faces, None, None

def gen_grass_blade(height=30, width=4):
    verts = [(0, 0, 0), (width, 0, 0), (width*0.6, 0, height*0.6), (width*0.2, 0, height)]
    faces = [(1, 2, 3), (1, 3, 4)]
    return verts, faces, None, None

def gen_bush(radius=40, height=50):
    """Simple bush as deformed sphere."""
    verts = []; faces = []
    rng = random.Random(1234)
    segs = 8; rings_ = 5
    for iy in range(rings_ + 1):
        phi = math.pi * iy / rings_
        for ix in range(segs):
            theta = 2 * math.pi * ix / segs
            r = radius * rng.uniform(0.7, 1.0)
            x = r * math.sin(phi) * math.cos(theta)
            y = r * math.sin(phi) * math.sin(theta)
            z = height * math.cos(phi) * 0.5 + height * 0.5
            verts.append((x, y, max(0, z)))
    for iy in range(rings_):
        for ix in range(segs):
            jx = (ix + 1) % segs
            a = iy * segs + ix + 1
            b = iy * segs + jx + 1
            c = (iy+1) * segs + jx + 1
            d = (iy+1) * segs + ix + 1
            faces.append((a, b, c, d))
    return verts, faces, None, None

def gen_flower(height=25):
    """Simple flower: stem + 5 petals."""
    verts = [(0, 0, 0), (0, 0, height)]  # stem
    # petals
    n_pet = 5; pr = 8
    for i in range(n_pet):
        a = 2 * math.pi * i / n_pet
        verts.append((pr * math.cos(a), pr * math.sin(a), height))
        verts.append((pr * 0.5 * math.cos(a + 0.3), pr * 0.5 * math.sin(a + 0.3), height + 5))
    faces = [(1, 2)]  # degenerate line for stem – skip
    faces = []
    for i in range(n_pet):
        base = 3 + i * 2
        faces.append((2, base, base + 1))
    return verts, faces, None, None

def gen_small_stone(radius=12):
    rng = random.Random(5678)
    verts = []; faces = []
    segs = 6; rings_ = 3
    for iy in range(rings_ + 1):
        phi = math.pi * iy / rings_
        for ix in range(segs):
            theta = 2 * math.pi * ix / segs
            r = radius * rng.uniform(0.65, 1.0)
            x = r * math.sin(phi) * math.cos(theta)
            y = r * math.sin(phi) * math.sin(theta)
            z = r * math.cos(phi) * 0.6
            verts.append((x, y, z + radius * 0.3))
    for iy in range(rings_):
        for ix in range(segs):
            jx = (ix + 1) % segs
            a = iy * segs + ix + 1
            b = iy * segs + jx + 1
            c = (iy+1) * segs + jx + 1
            d = (iy+1) * segs + ix + 1
            faces.append((a, b, c, d))
    return verts, faces, None, None

def gen_ruin_column(radius=35, height=250):
    verts = []; faces = []
    segs = 10; rings_ = 5
    rng = random.Random(321)
    for iy in range(rings_ + 1):
        t = iy / rings_
        z = t * height
        r = radius * (1.0 + 0.1 * math.cos(t * 6))
        for ix in range(segs):
            a = 2 * math.pi * ix / segs
            # fluted column
            flute = 1.0 + 0.08 * math.cos(ix * 2 * math.pi / segs * 3)
            verts.append((r * flute * math.cos(a), r * flute * math.sin(a), z))
    for iy in range(rings_):
        for ix in range(segs):
            jx = (ix + 1) % segs
            a = iy * segs + ix + 1
            b = iy * segs + jx + 1
            c = (iy+1) * segs + jx + 1
            d = (iy+1) * segs + ix + 1
            faces.append((a, b, c, d))
    return verts, faces, None, None

def gen_ruin_wall(width=200, height=180, depth=30):
    """Closed broken wall section with front, back, side, top, and bottom faces."""
    rng = random.Random(654)
    verts = []; faces = []
    sx, sy = 6, 4
    stride = sx + 1
    for iy in range(sy + 1):
        for ix in range(sx + 1):
            u = ix / sx; v_ = iy / sy
            x = (u - 0.5) * width
            z = v_ * height
            # broken top edge
            if iy == sy:
                z *= rng.uniform(0.6, 1.0)
            y = -depth * 0.5 + (rng.uniform(-0.5, 0.5)) * depth * 0.1
            verts.append((x, y, z))
    for iy in range(sy):
        for ix in range(sx):
            a = iy * stride + ix + 1
            b = a + 1
            c = b + stride
            d = a + stride
            faces.append((a, b, c, d))  # front (-Y)
    # back face (offset by depth)
    offset = len(verts)
    for v in verts[:offset]:
        verts.append((v[0], v[1] + depth, v[2]))
    for iy in range(sy):
        for ix in range(sx):
            a = offset + iy * stride + ix + 1
            b = a + 1
            c = b + stride
            d = a + stride
            faces.append((a, d, c, b))  # back (+Y)

    def f_idx(iy, ix):
        return iy * stride + ix + 1

    def b_idx(iy, ix):
        return offset + iy * stride + ix + 1

    # bottom (-Z) and top (+Z) faces
    for ix in range(sx):
        faces.append((f_idx(0, ix), b_idx(0, ix), b_idx(0, ix + 1), f_idx(0, ix + 1)))
        faces.append((f_idx(sy, ix), f_idx(sy, ix + 1), b_idx(sy, ix + 1), b_idx(sy, ix)))

    # left (-X) and right (+X) side faces
    for iy in range(sy):
        faces.append((f_idx(iy, 0), f_idx(iy + 1, 0), b_idx(iy + 1, 0), b_idx(iy, 0)))
        faces.append((f_idx(iy, sx), b_idx(iy, sx), b_idx(iy + 1, sx), f_idx(iy + 1, sx)))

    return verts, faces, None, None

ENV_MESH_GENERATORS = {
    "terrain_bowl": gen_terrain_bowl,
    "cliff_ring":   gen_cliff_ring,
    "grass_blade":  gen_grass_blade,
    "bush":         gen_bush,
    "flower":       gen_flower,
    "small_stone":  gen_small_stone,
    "ruin_column":  gen_ruin_column,
    "ruin_wall":    gen_ruin_wall,
}

# ═══════════════════════════════════════════════════════════════════════════════
# Voronoi fracture (simple convex decomposition)
# ═══════════════════════════════════════════════════════════════════════════════

def voronoi_fracture_box(name: str, half_extents: tuple, n_pieces: int, seed: int = 42):
    """
    Generates convex Voronoi-like pieces from a box with base at Z=0 (Z in [0, 2*hz]).
    Each piece's OBJ vertices are centered in its own local space around (0, 0, 0),
    while (cx_p, cy_p, cz_p) is stored in `transform` so UE does not double-offset.
    """
    rng = random.Random(seed)
    hx, hy, hz = half_extents
    pieces = []

    # Strategy: divide into layers (vertical slices) then each layer into columns
    n_layers = max(1, int(math.ceil(math.sqrt(n_pieces * hz / max(hx, hy, 1)))))
    pieces_per_layer = max(1, n_pieces // n_layers)
    remainder = n_pieces - n_layers * pieces_per_layer

    piece_idx = 0
    for layer in range(n_layers):
        z_lo = 2.0 * hz * layer / n_layers
        z_hi = 2.0 * hz * (layer + 1) / n_layers
        n_cols = pieces_per_layer + (1 if layer < remainder else 0)

        for col in range(n_cols):
            # Divide the XY rectangle into exact convex sector polygons
            a0 = 2.0 * math.pi * col / n_cols
            a1 = 2.0 * math.pi * (col + 1) / n_cols
            pts_2d = _sector_box_intersect(a0, a1, hx, hy)
            if len(pts_2d) < 3:
                pts_2d = [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]

            # Piece center in structure-local space (base at Z=0)
            cx_p = sum(p[0] for p in pts_2d) / len(pts_2d)
            cy_p = sum(p[1] for p in pts_2d) / len(pts_2d)
            cz_p = 0.5 * (z_lo + z_hi)

            # Extrude to 3D in piece-local space centered around (0, 0, 0)
            dz_half = 0.5 * (z_hi - z_lo)
            verts_3d = []
            for p in pts_2d:
                verts_3d.append((p[0] - cx_p, p[1] - cy_p, -dz_half))
            for p in pts_2d:
                verts_3d.append((p[0] - cx_p, p[1] - cy_p, dz_half))

            n_pts = len(pts_2d)
            faces_3d = []
            # bottom face (-Z outward winding)
            faces_3d.append(tuple([1] + list(range(n_pts, 1, -1))))
            # top face (+Z outward winding)
            faces_3d.append(tuple(range(n_pts + 1, n_pts * 2 + 1)))
            # sides (CCW outward)
            for i in range(n_pts):
                j = (i + 1) % n_pts
                faces_3d.append((i + 1, j + 1, j + n_pts + 1, i + n_pts + 1))

            # Half-extents around piece local (0, 0, 0)
            ext_x = max(abs(v[0]) for v in verts_3d)
            ext_y = max(abs(v[1]) for v in verts_3d)
            ext_z = dz_half

            # Mass proportional to volume
            vol = ext_x * ext_y * ext_z * 8.0
            mass = max(5.0, vol / 1e4 * 25.0)

            is_grounded = (layer == 0)

            # Supports: pieces in layer below
            supports = []
            if layer > 0:
                prev_start = sum(
                    pieces_per_layer + (1 if l < remainder else 0) for l in range(layer - 1)
                )
                prev_count = pieces_per_layer + (1 if (layer - 1) < remainder else 0)
                for s in range(prev_count):
                    supports.append(prev_start + s)

            # Laterals: adjacent in same layer
            laterals = []
            layer_start = sum(
                pieces_per_layer + (1 if l < remainder else 0) for l in range(layer)
            )
            for c2 in range(n_cols):
                if c2 != col:
                    laterals.append(layer_start + c2)

            pieces.append({
                "verts": verts_3d,
                "faces": faces_3d,
                "transform": (round(cx_p, 4), round(cy_p, 4), round(cz_p, 4), 0.0, 0.0, 0.0),
                "mass": round(mass, 1),
                "health": 100.0,
                "extent": (round(ext_x, 1), round(ext_y, 1), round(ext_z, 1)),
                "grounded": is_grounded,
                "supports": supports,
                "laterals": laterals,
            })
            piece_idx += 1

    return pieces


def _ray_box_hit(angle: float, hx: float, hy: float):
    dx, dy = math.cos(angle), math.sin(angle)
    t = float("inf")
    if abs(dx) > 1e-9:
        t = min(t, abs(hx / dx))
    if abs(dy) > 1e-9:
        t = min(t, abs(hy / dy))
    return (dx * t, dy * t)


def _sector_box_intersect(a0: float, a1: float, hx: float, hy: float):
    """Exact convex CCW polygon of sector [a0..a1] intersected with [-hx..hx, -hy..hy]."""
    span = a1 - a0
    if span >= 2.0 * math.pi - 1e-5:
        return [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]

    p_start = _ray_box_hit(a0, hx, hy)
    p_end = _ray_box_hit(a1, hx, hy)

    # Box corners strictly inside (a0, a1), sorted by angle
    corners = [(hx, hy), (-hx, hy), (-hx, -hy), (hx, -hy)]
    in_corners = []
    for cx, cy in corners:
        ang = math.atan2(cy, cx)
        while ang < a0 - 1e-7:
            ang += 2.0 * math.pi
        while ang >= a0 + 2.0 * math.pi - 1e-7:
            ang -= 2.0 * math.pi
        if a0 + 1e-5 < ang < a1 - 1e-5:
            in_corners.append((ang, (cx, cy)))
    in_corners.sort(key=lambda item: item[0])

    raw_pts = []
    if span < math.pi - 1e-4:
        raw_pts.append((0.0, 0.0))
    raw_pts.append(p_start)
    for _, c_pt in in_corners:
        raw_pts.append(c_pt)
    raw_pts.append(p_end)

    # Deduplicate near-coincident consecutive vertices
    pts = []
    for pt in raw_pts:
        if not pts or math.hypot(pt[0] - pts[-1][0], pt[1] - pts[-1][1]) > 1e-4:
            pts.append(pt)
    if len(pts) >= 2 and math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1]) <= 1e-4:
        pts.pop()
    return pts

# ═══════════════════════════════════════════════════════════════════════════════
# Audio generators
# ═══════════════════════════════════════════════════════════════════════════════

def _envelope(samples, attack, decay, sustain_level, release, sr):
    """ADSR envelope."""
    n = len(samples)
    a_s = int(attack * sr); d_s = int(decay * sr); r_s = int(release * sr)
    s_s = max(0, n - a_s - d_s - r_s)
    for i in range(n):
        if i < a_s:
            env = i / max(1, a_s)
        elif i < a_s + d_s:
            t = (i - a_s) / max(1, d_s)
            env = 1.0 - t * (1.0 - sustain_level)
        elif i < a_s + d_s + s_s:
            env = sustain_level
        else:
            t = (i - a_s - d_s - s_s) / max(1, r_s)
            env = sustain_level * (1.0 - t)
        samples[i] *= env
    return samples

def gen_explosion(dur=0.8, sr=48000):
    rng = random.Random(100)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        noise = rng.uniform(-1, 1)
        # low rumble + noise
        rumble = math.sin(2 * math.pi * 55 * t + 3 * math.sin(2 * math.pi * 15 * t))
        samples[i] = (noise * 0.6 + rumble * 0.4) * math.exp(-t * 4)
    return samples

def gen_cast(freq=440, dur=0.3, sr=48000):
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        sweep = freq * (1.0 + t * 2)  # rising pitch
        samples[i] = math.sin(2 * math.pi * sweep * t) * 0.6
    return _envelope(samples, 0.01, 0.05, 0.6, 0.15, sr)

def gen_charge(dur=1.2, sr=48000):
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        freq = 120 + 300 * (t / dur)
        samples[i] = math.sin(2 * math.pi * freq * t) * 0.3 + math.sin(2 * math.pi * freq * 1.5 * t) * 0.15
    return _envelope(samples, 0.1, 0.2, 0.8, 0.1, sr)

def gen_whoosh(dur=0.4, sr=48000):
    rng = random.Random(200)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        noise = rng.uniform(-1, 1)
        # bandpass-ish
        freq = 800 + 1200 * math.sin(math.pi * t / dur)
        bp = math.sin(2 * math.pi * freq * t)
        samples[i] = noise * 0.5 * bp * math.sin(math.pi * t / dur)
    return samples

def gen_erupt(dur=0.6, sr=48000):
    rng = random.Random(300)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        noise = rng.uniform(-1, 1)
        thud = math.sin(2 * math.pi * 40 * t) * math.exp(-t * 8)
        crack = noise * math.exp(-t * 12)
        samples[i] = thud * 0.5 + crack * 0.5
    return _envelope(samples, 0.005, 0.1, 0.4, 0.3, sr)

def gen_gust(dur=0.6, sr=48000):
    rng = random.Random(400)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        noise = rng.uniform(-1, 1)
        env = math.sin(math.pi * t / dur)
        samples[i] = noise * 0.4 * env
    return samples

def gen_thunder(dur=1.0, sr=48000):
    rng = random.Random(500)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        crack = rng.uniform(-1, 1) * math.exp(-t * 20) * 0.8
        rumble = math.sin(2 * math.pi * 30 * t + rng.uniform(0, 1)) * math.exp(-t * 2) * 0.6
        samples[i] = crack + rumble
    return samples

def gen_break(material: str, dur=0.3, sr=48000):
    rng = random.Random(hash(material))
    n = int(dur * sr)
    samples = [0.0] * n
    # Frequency characteristics by material
    freqs = {
        "Stone": (200, 600), "Wood": (300, 900), "Pottery": (800, 2000),
        "Rock": (150, 500), "Crystal": (1200, 3000), "Foliage": (100, 400),
    }
    lo, hi = freqs.get(material, (200, 800))
    for i in range(n):
        t = i / sr
        noise = rng.uniform(-1, 1)
        freq = lo + (hi - lo) * rng.random()
        tone = math.sin(2 * math.pi * freq * t)
        samples[i] = (noise * 0.5 + tone * 0.3) * math.exp(-t * 10)
    return samples

def gen_ui_sound(freq=600, dur=0.15, sr=48000):
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        samples[i] = math.sin(2 * math.pi * freq * t) * 0.4
    return _envelope(samples, 0.005, 0.03, 0.4, 0.08, sr)

def gen_ambient_loop(dur=8.0, sr=48000):
    """Ambient nature/wind loop."""
    rng = random.Random(900)
    n = int(dur * sr)
    samples = [0.0] * n
    for i in range(n):
        t = i / sr
        wind = rng.uniform(-1, 1) * 0.1
        # subtle tonal drone
        drone = math.sin(2 * math.pi * 85 * t) * 0.05
        birds = math.sin(2 * math.pi * (1200 + 300 * math.sin(2 * math.pi * 0.3 * t)) * t) * 0.02
        # crossfade for seamless loop
        fade = 1.0
        if t < 0.1:
            fade = t / 0.1
        elif t > dur - 0.1:
            fade = (dur - t) / 0.1
        samples[i] = (wind + drone + birds) * fade
    return samples

SFX_GENERATORS = {
    "Explosion_Fire": lambda: gen_explosion(0.8),
    "Cast_Fire":      lambda: gen_cast(440, 0.3),
    "Charge":         lambda: gen_charge(1.2),
    "Whoosh":         lambda: gen_whoosh(0.4),
    "Cast_Earth":     lambda: gen_cast(220, 0.35),
    "Erupt_Earth":    lambda: gen_erupt(0.6),
    "Cast_Wind":      lambda: gen_cast(660, 0.25),
    "Gust_Wind":      lambda: gen_gust(0.6),
    "Cast_Lightning": lambda: gen_cast(880, 0.2),
    "Thunder":        lambda: gen_thunder(1.0),
    "Break_Stone":    lambda: gen_break("Stone"),
    "Break_Wood":     lambda: gen_break("Wood"),
    "Break_Pottery":  lambda: gen_break("Pottery"),
    "Break_Rock":     lambda: gen_break("Rock"),
    "Break_Crystal":  lambda: gen_break("Crystal"),
    "Break_Foliage":  lambda: gen_break("Foliage"),
    "Dash":           lambda: gen_whoosh(0.25),
    "Reset":          lambda: gen_ui_sound(400, 0.3),
    "Combo":          lambda: gen_ui_sound(800, 0.2),
    "Crystal":        lambda: gen_cast(1600, 0.4),
    "Collapse":       lambda: gen_explosion(1.2),
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main forge pipeline
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="wizardEPIC offline asset forge")
    parser.add_argument("--out", type=str, default=None,
                        help="Output directory (default: Generated/ next to this script)")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    out_dir = Path(args.out) if args.out else script_dir / "Generated"

    tex_dir = out_dir / "Textures"
    audio_dir = out_dir / "Audio"
    mesh_dir = out_dir / "Meshes"
    dest_dir = out_dir / "Destruction"
    tex_dir.mkdir(parents=True, exist_ok=True)
    audio_dir.mkdir(parents=True, exist_ok=True)
    mesh_dir.mkdir(parents=True, exist_ok=True)
    dest_dir.mkdir(parents=True, exist_ok=True)

    t0 = time.time()
    total = 0

    # 1. FX sprite textures
    print("=== FX Textures ===")
    for name, gen in FX_TEX_GENERATORS.items():
        path = tex_dir / f"{name}.png"
        px = gen()
        write_png(path, len(px[0]), len(px), px)
        print(f"  {name}: {len(px[0])}x{len(px)} -> {path.name}")
        total += 1

    # 2. Environment textures
    print("=== Environment Textures ===")
    for name, gen in ENV_TEX_GENERATORS.items():
        path = tex_dir / f"{name}.png"
        px = gen()
        write_png(path, len(px[0]), len(px), px)
        print(f"  {name}: {len(px[0])}x{len(px)} -> {path.name}")
        total += 1

    # 3. UI spell icons
    print("=== UI Icons ===")
    for elem in C.ELEMENTS:
        name = f"T_Icon_{elem}"
        path = tex_dir / f"{name}.png"
        px = gen_spell_icon(elem, C.ICON_SIZE, C.ICON_SIZE)
        write_png(path, C.ICON_SIZE, C.ICON_SIZE, px)
        print(f"  {name}: {C.ICON_SIZE}x{C.ICON_SIZE} -> {path.name}")
        total += 1

    # 4. FX meshes
    print("=== FX Meshes ===")
    for name, gen in FX_MESH_GENERATORS.items():
        path = mesh_dir / f"{name}.obj"
        verts, faces, normals, uvs = gen()
        write_obj(path, verts, faces, normals, uvs)
        print(f"  {name}: {len(verts)} verts, {len(faces)} faces -> {path.name}")
        total += 1

    # 5. Environment meshes
    print("=== Environment Meshes ===")
    terrain_sampler = None
    for name, key, params in C.ENV_MESHES:
        gen = ENV_MESH_GENERATORS[key]
        path = mesh_dir / f"{name}.obj"
        verts, faces, normals, uvs = gen(**params)
        if name == "SM_Terrain":
            terrain_sampler = make_terrain_sampler(
                verts,
                radius=params.get("radius", C.BOWL_RADIUS),
                subdivs=params.get("subdivs", 32),
            )
        write_obj(path, verts, faces, normals, uvs)
        print(f"  {name}: {len(verts)} verts, {len(faces)} faces -> {path.name}")
        total += 1

    def _ground_z(x, y, base_z=0.0):
        return round(terrain_sampler(x, y) + base_z, 3) if terrain_sampler else base_z

    # 6. Destructible fracture pieces
    print("=== Destructible Fracture ===")
    destruction_data = {}
    for shape_name, material, half_ext, n_pieces, health in C.DESTRUCTIBLE_SHAPES:
        shape_dir = dest_dir / shape_name
        shape_dir.mkdir(parents=True, exist_ok=True)

        # Generate whole mesh with base at Z=0 (Z in [0, 2*hz]) and outward winding
        hx, hy, hz = half_ext
        whole_verts = [
            (-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0),
            (-hx, -hy, 2*hz), (hx, -hy, 2*hz), (hx, hy, 2*hz), (-hx, hy, 2*hz),
        ]
        whole_faces = [
            (1, 4, 3, 2), (5, 6, 7, 8), (1, 2, 6, 5), (2, 3, 7, 6), (3, 4, 8, 7), (4, 1, 5, 8),
        ]
        write_obj(shape_dir / f"SM_{shape_name}_Whole.obj", whole_verts, whole_faces)

        pieces = voronoi_fracture_box(shape_name, half_ext, n_pieces, seed=hash(shape_name) & 0xFFFF)

        piece_data = []
        for idx, piece in enumerate(pieces):
            piece_name = f"SM_{shape_name}_P{idx:03d}"
            write_obj(shape_dir / f"{piece_name}.obj", piece["verts"], piece["faces"])
            piece_data.append({
                "mesh": piece_name,
                "transform": piece["transform"],
                "mass": piece["mass"],
                "health": health,
                "extent": piece["extent"],
                "grounded": piece["grounded"],
                "supports": piece["supports"],
                "laterals": piece["laterals"],
            })
            total += 1

        destruction_data[shape_name] = {
            "material": material,
            "whole_mesh": f"SM_{shape_name}_Whole",
            "pieces": piece_data,
            "health": health * 2,  # whole prop health (sum-ish)
            "score_per_piece": 10,
            "collapse_bonus": 100 if n_pieces >= 8 else 50,
        }
        print(f"  {shape_name}: {len(pieces)} pieces ({material})")
        total += 1  # whole mesh

    # 7. Audio SFX
    print("=== Audio SFX ===")
    for name, gen in SFX_GENERATORS.items():
        path = audio_dir / f"S_{name}.wav"
        samples = gen()
        write_wav(path, samples)
        print(f"  S_{name}: {len(samples)} samples -> {path.name}")
        total += 1

    # 7b. Ambient loop
    print("=== Ambient Loop ===")
    path = audio_dir / "S_AmbientLoop.wav"
    samples = gen_ambient_loop(8.0)
    write_wav(path, samples)
    print(f"  S_AmbientLoop: {len(samples)} samples -> {path.name}")
    total += 1

    # 8. Layout JSON (for ue_forge.py) with terrain-aligned Z coordinates
    print("=== Layout Data ===")
    scatter_rng = random.Random(12345)
    scatter_layout = {}
    for mesh_name, cfg in C.SCATTER_CONFIG.items():
        count = cfg.get("count", 100)
        r_min, r_max = cfg.get("radius_range", (100, 3400))
        z_off = cfg.get("z", 0)
        instances = []
        for _ in range(count):
            angle = scatter_rng.uniform(0, 2 * math.pi)
            radius = scatter_rng.uniform(r_min, r_max)
            sx = radius * math.cos(angle)
            sy = radius * math.sin(angle)
            sz = _ground_z(sx, sy, z_off)
            yaw = scatter_rng.uniform(0, 360)
            scale = scatter_rng.uniform(0.7, 1.3)
            instances.append([round(sx, 2), round(sy, 2), sz, round(yaw, 2), round(scale, 3)])
        scatter_layout[mesh_name] = {**cfg, "instances": instances}

    ruin_props = []
    for ruin_name in ["SM_RuinColumn", "SM_RuinWall"]:
        for i, p in enumerate(C._ring(4, 2900, 0, math.pi / 4)):
            ruin_props.append({
                "mesh": ruin_name,
                "label": f"{ruin_name}_{i}",
                "x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3],
            })

    layout = {
        "player_start": {"x": 0.0, "y": 0.0, "z": _ground_z(0.0, 0.0, 95.0)},
        "structures": [
            {"definition": name, "x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3]}
            for name, p in C.STRUCTURE_PLACEMENTS
        ],
        "breakables": {
            def_name: [
                {"x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3]}
                for p in placements
            ]
            for def_name, placements in C.BREAKABLE_PLACEMENTS.items()
        },
        "crystals": [
            {"x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3]}
            for p in C.CRYSTAL_PLACEMENTS
        ],
        "ruin_props": ruin_props,
        "scatter": scatter_layout,
        "destruction": destruction_data,
        "spells": C.SPELL_DEFAULTS,
    }

    layout_path = out_dir / "layout.json"
    with open(layout_path, "w", encoding="utf-8") as f:
        json.dump(layout, f, indent=2, default=str)
    print(f"  layout.json -> {layout_path.name}")
    total += 1

    dt = time.time() - t0
    print(f"\n✅ Done: {total} assets generated in {dt:.1f}s -> {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
