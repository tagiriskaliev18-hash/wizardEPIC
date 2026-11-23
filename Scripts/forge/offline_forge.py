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


def _box_project_uv(v, normal, scale=160.0):
    """Triplanar/box UV projection from vertex position and face normal."""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if az >= ax and az >= ay:
        return (v[0] / scale, v[1] / scale)
    if ax >= ay:
        return (v[1] / scale, v[2] / scale)
    return (v[0] / scale, v[2] / scale)


def compute_box_uvs_and_normals(verts, faces, tile_cm=160.0, crease_deg=45.0):
    """
    Compute outward crease-aware vertex-corner normals and per-corner box-projected UVs.
    Returns (corner_normals, corner_uvs), each a list aligned with `valid_faces`,
    where entry[fi][ci] is the normal/UV for corner `ci` of face `fi`.
    """
    valid_faces = [fc for fc in faces if len(fc) >= 3]
    face_normals = [_face_normal(verts, fc) for fc in valid_faces]

    vert_adj = [[] for _ in verts]
    for fi, fc in enumerate(valid_faces):
        for vi in fc:
            vert_adj[vi - 1].append(fi)

    cos_thresh = math.cos(math.radians(crease_deg))
    corner_normals = []
    corner_uvs = []

    for fi, fc in enumerate(valid_faces):
        fn = face_normals[fi]
        fn_list = []
        uv_list = []
        for vi in fc:
            # Blend adjacent face normals within crease angle so curved lathes stay smooth
            # while architectural corners stay crisp.
            sx = sy = sz = 0.0
            for fj in vert_adj[vi - 1]:
                nj = face_normals[fj]
                if fn[0] * nj[0] + fn[1] * nj[1] + fn[2] * nj[2] >= cos_thresh:
                    sx += nj[0]
                    sy += nj[1]
                    sz += nj[2]
            n_corner = _normalize_v3((sx, sy, sz)) if (sx or sy or sz) else fn
            fn_list.append(n_corner)
            uv_list.append(_box_project_uv(verts[vi - 1], fn, scale=tile_cm))
        corner_normals.append(fn_list)
        corner_uvs.append(uv_list)

    return corner_normals, corner_uvs


def write_obj(
    path: Path,
    verts: list[tuple],
    faces: list[tuple],
    normals=None,
    uvs=None,
    tile_cm: float = 160.0,
):
    """Write an OBJ with guaranteed valid vertex normals (vn), UVs (vt), and v/vt/vn faces."""
    path.parent.mkdir(parents=True, exist_ok=True)
    valid_faces = [fc for fc in faces if len(fc) >= 3]

    per_vertex_normals = (
        normals is not None
        and len(normals) == len(verts)
        and len(normals) > 0
        and isinstance(normals[0][0], (int, float))
    )
    per_corner_normals = (
        normals is not None
        and len(normals) == len(valid_faces)
        and len(normals) > 0
        and isinstance(normals[0][0], (list, tuple))
    )
    per_face_normals = (
        normals is not None
        and not per_vertex_normals
        and not per_corner_normals
        and len(normals) == len(valid_faces)
    )

    per_vertex_uvs = (
        uvs is not None
        and len(uvs) == len(verts)
        and len(uvs) > 0
        and isinstance(uvs[0][0], (int, float))
    )
    per_corner_uvs = (
        uvs is not None
        and len(uvs) == len(valid_faces)
        and len(uvs) > 0
        and isinstance(uvs[0][0], (list, tuple))
    )

    if not (per_vertex_normals or per_corner_normals or per_face_normals) or not (
        per_vertex_uvs or per_corner_uvs
    ):
        auto_normals, auto_uvs = compute_box_uvs_and_normals(
            verts, valid_faces, tile_cm=tile_cm
        )
    else:
        auto_normals, auto_uvs = None, None

    out_normals = list(normals) if per_vertex_normals else []
    norm_dedup: dict[tuple[float, float, float], int] = {}
    out_uvs = list(uvs) if per_vertex_uvs else []
    uv_dedup: dict[tuple[float, float], int] = {}
    face_records = []

    def _intern_normal(n: tuple[float, float, float]) -> int:
        key = (round(n[0], 5), round(n[1], 5), round(n[2], 5))
        idx = norm_dedup.get(key)
        if idx is None:
            out_normals.append(n)
            idx = len(out_normals)
            norm_dedup[key] = idx
        return idx

    def _intern_uv(uv: tuple[float, float]) -> int:
        key = (round(uv[0], 5), round(uv[1], 5))
        idx = uv_dedup.get(key)
        if idx is None:
            out_uvs.append(uv)
            idx = len(out_uvs)
            uv_dedup[key] = idx
        return idx

    for fi, face in enumerate(valid_faces):
        corners = []
        for ci, vi in enumerate(face):
            if per_vertex_normals:
                n_idx = vi
            elif per_corner_normals:
                n_idx = _intern_normal(normals[fi][ci])
            elif per_face_normals:
                n_idx = _intern_normal(normals[fi])
            else:
                n_idx = _intern_normal(auto_normals[fi][ci])

            if per_vertex_uvs:
                t_idx = vi
            elif per_corner_uvs:
                t_idx = _intern_uv(uvs[fi][ci])
            else:
                t_idx = _intern_uv(auto_uvs[fi][ci])

            corners.append(f"{vi}/{t_idx}/{n_idx}")
        face_records.append("f " + " ".join(corners) + "\n")

    with open(path, "w", encoding="utf-8") as f:
        f.write("# wizardEPIC offline_forge\n")
        for v in verts:
            f.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
        for uv in out_uvs:
            f.write(f"vt {uv[0]:.5f} {uv[1]:.5f}\n")
        for n in out_normals:
            f.write(f"vn {n[0]:.5f} {n[1]:.5f} {n[2]:.5f}\n")
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
    """Rock pillar with seamless cylindrical UVs (ix/segments*2, t*3) and smooth unit normals."""
    rng = random.Random(777)
    verts = []
    uvs = []
    faces = []
    rings = 6
    stride = segments + 1

    for iy in range(rings + 1):
        t = iy / rings
        z = t * height
        r = base_w / 2.0 * (1.0 - t * 0.15)  # slight taper
        ring_noises = [rng.uniform(0.85, 1.15) for _ in range(segments)]
        for ix in range(stride):
            a = 2.0 * math.pi * (ix % segments) / segments
            noise = ring_noises[ix % segments]
            verts.append((r * math.cos(a) * noise, r * math.sin(a) * noise, z))
            uvs.append(((ix / segments) * 2.0, t * 3.0))

    # Side faces
    for iy in range(rings):
        for ix in range(segments):
            a = iy * stride + ix + 1
            b = iy * stride + (ix + 1) + 1
            c = (iy + 1) * stride + (ix + 1) + 1
            d = (iy + 1) * stride + ix + 1
            faces.append((a, b, c, d))

    # Top cap (+Z outward)
    top_start = rings * stride
    top_center = len(verts) + 1
    cx = sum(verts[top_start + ix][0] for ix in range(segments)) / segments
    cy = sum(verts[top_start + ix][1] for ix in range(segments)) / segments
    verts.append((cx, cy, height))
    uvs.append((1.0, 3.0))
    for ix in range(segments):
        faces.append((top_start + ix + 1, top_start + (ix + 1) + 1, top_center))

    # Bottom cap (-Z outward)
    bot_center = len(verts) + 1
    bx = sum(verts[ix][0] for ix in range(segments)) / segments
    by = sum(verts[ix][1] for ix in range(segments)) / segments
    verts.append((bx, by, 0.0))
    uvs.append((1.0, 0.0))
    for ix in range(segments):
        faces.append(((ix + 1) + 1, ix + 1, bot_center))

    # Smooth unit normals with welded seam across ix=0 and ix=segments
    normals = _compute_smooth_normals(verts, faces)
    for iy in range(rings + 1):
        i0 = iy * stride
        i_end = i0 + segments
        avg = _normalize_v3((
            normals[i0][0] + normals[i_end][0],
            normals[i0][1] + normals[i_end][1],
            normals[i0][2] + normals[i_end][2],
        ))
        normals[i0] = avg
        normals[i_end] = avg
    normals[top_center - 1] = (0.0, 0.0, 1.0)
    normals[bot_center - 1] = (0.0, 0.0, -1.0)
    return verts, faces, normals, uvs

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
    """Return list of (x, y, pad_radius, pad_z) for courtyard pads and every placed structure."""
    shape_map = {name: half_ext for name, _mat, half_ext, _np, _hp in C.DESTRUCTIBLE_SHAPES}
    z_courtyard = terrain_bowl_raw_z(0.0, 0.0)
    pads = [
        # Flat circular courtyard pads around player spawn (0, -900) and central plaza (0, 0)
        (0.0, -900.0, 650.0, z_courtyard),
        (0.0, 0.0, 650.0, z_courtyard),
    ]
    for name, (sx, sy, _sz, _yaw) in C.STRUCTURE_PLACEMENTS:
        hx, hy, _hz = shape_map.get(name, (100.0, 100.0, 100.0))
        if name == "Arch_A" and abs(sx) < 1e-3 and abs(sy - 850.0) < 1e-3:
            # Keep hero arch aligned with the ceremonial courtyard elevation without touching r=580 dais
            pads.append((sx, sy, 260.0, z_courtyard))
        else:
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
    """Bowl elevation with smooth weighted flat pads under courtyard and structure footprints."""
    z = terrain_bowl_raw_z(x, y, radius)
    if pads is None:
        pads = _build_structure_pads()
    blend_margin = 180.0
    w_sum = 0.0
    wz_sum = 0.0
    max_w = 0.0
    for sx, sy, r_pad, z_pad in pads:
        dist = math.hypot(x - sx, y - sy)
        if dist <= r_pad:
            w = 1.0
        elif dist < r_pad + blend_margin:
            w = 1.0 - _smoothstep((dist - r_pad) / blend_margin)
        else:
            w = 0.0
        if w > 0.0:
            w_sum += w
            wz_sum += w * z_pad
            if w > max_w:
                max_w = w
    if w_sum > 0.0:
        z_pad_avg = wz_sum / w_sum
        z = _lerp(z, z_pad_avg, max_w)
    return z


def make_terrain_sampler(verts, radius=3600, subdivs=64):
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


def gen_terrain_bowl(radius=3600, subdivs=64):
    """Concave bowl terrain mesh with flat courtyard/structure pads, smooth normals, and tiled UVs."""
    verts = []; faces = []; uvs = []
    pads = _build_structure_pads()
    for iy in range(subdivs + 1):
        for ix in range(subdivs + 1):
            u = ix / subdivs; v_ = iy / subdivs
            x = (u - 0.5) * 2 * radius
            y = (v_ - 0.5) * 2 * radius
            z = terrain_bowl_z(x, y, radius, pads)
            verts.append((x, y, z))
            uvs.append((u * 22.0, v_ * 22.0))
    for iy in range(subdivs):
        for ix in range(subdivs):
            a = iy * (subdivs + 1) + ix + 1
            b = a + 1
            c = b + subdivs + 1
            d = a + subdivs + 1
            faces.append((a, b, c, d))
    normals = _compute_smooth_normals(verts, faces)
    return verts, faces, normals, uvs


# ── Shared procedural mesh builder (guaranteed outward winding) ───────────────

def _rotate_euler(pt, yaw_deg=0.0, pitch_deg=0.0, roll_deg=0.0):
    x, y, z = pt
    if roll_deg:
        r = math.radians(roll_deg)
        cr, sr = math.cos(r), math.sin(r)
        y, z = y * cr - z * sr, y * sr + z * cr
    if pitch_deg:
        p = math.radians(pitch_deg)
        cp, sp = math.cos(p), math.sin(p)
        x, z = x * cp + z * sp, -x * sp + z * cp
    if yaw_deg:
        yw = math.radians(yaw_deg)
        cy, sy = math.cos(yw), math.sin(yw)
        x, y = x * cy - y * sy, x * sy + y * cy
    return (x, y, z)


class _MeshBuilder:
    """Clean solid/manifold mesh accumulator with outward winding."""

    def __init__(self):
        self.verts: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, ...]] = []

    def add_box(self, cx, cy, cz, hx, hy, hz, yaw=0.0, pitch=0.0, roll=0.0):
        base = len(self.verts) + 1
        local = [
            (-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
            (-hx, -hy,  hz), (hx, -hy,  hz), (hx, hy,  hz), (-hx, hy,  hz),
        ]
        for pt in local:
            rx, ry, rz = _rotate_euler(pt, yaw, pitch, roll)
            self.verts.append((cx + rx, cy + ry, cz + rz))
        self.faces.extend([
            (base + 0, base + 3, base + 2, base + 1),  # -Z
            (base + 4, base + 5, base + 6, base + 7),  # +Z
            (base + 0, base + 1, base + 5, base + 4),  # -Y
            (base + 1, base + 2, base + 6, base + 5),  # +X
            (base + 2, base + 3, base + 7, base + 6),  # +Y
            (base + 3, base + 0, base + 4, base + 7),  # -X
        ])

    def add_ring_stack(self, rings_xy, rings_z, cap_bottom=True, cap_top=True):
        """Extrude stacked CCW 2D rings ordered bottom-to-top (increasing Z)."""
        n = len(rings_xy[0])
        ring_starts = []
        for pts, z in zip(rings_xy, rings_z):
            ring_starts.append(len(self.verts) + 1)
            for x, y in pts:
                self.verts.append((x, y, z))

        for k in range(len(rings_xy) - 1):
            r0 = ring_starts[k]
            r1 = ring_starts[k + 1]
            for ix in range(n):
                jx = (ix + 1) % n
                self.faces.append((r0 + ix, r0 + jx, r1 + jx, r1 + ix))

        if cap_bottom:
            bx = sum(p[0] for p in rings_xy[0]) / n
            by = sum(p[1] for p in rings_xy[0]) / n
            bc = len(self.verts) + 1
            self.verts.append((bx, by, rings_z[0]))
            r0 = ring_starts[0]
            for ix in range(n):
                jx = (ix + 1) % n
                self.faces.append((r0 + jx, r0 + ix, bc))

        if cap_top:
            tx = sum(p[0] for p in rings_xy[-1]) / n
            ty = sum(p[1] for p in rings_xy[-1]) / n
            tc = len(self.verts) + 1
            self.verts.append((tx, ty, rings_z[-1]))
            rl = ring_starts[-1]
            for ix in range(n):
                jx = (ix + 1) % n
                self.faces.append((rl + ix, rl + jx, tc))

    def add_lathe(self, profile_rz, segments=14, cx=0.0, cy=0.0, z_offset=0.0):
        """
        Revolve (r, z) profile CCW around Z.
        Supports r==0 start/end pole caps and hollow interior mouths without inverted normals.
        """
        if len(profile_rz) < 2:
            return
        start_pole = profile_rz[0][0] <= 1e-6
        end_pole = profile_rz[-1][0] <= 1e-6
        mid = profile_rz[1:-1] if (start_pole and end_pole) else (
            profile_rz[1:] if start_pole else (profile_rz[:-1] if end_pole else profile_rz)
        )
        if not mid:
            return

        ring_starts = []
        for r, z in mid:
            ring_starts.append(len(self.verts) + 1)
            for ix in range(segments):
                a = 2.0 * math.pi * ix / segments
                self.verts.append((cx + r * math.cos(a), cy + r * math.sin(a), z_offset + z))

        for k in range(len(ring_starts) - 1):
            r0 = ring_starts[k]
            r1 = ring_starts[k + 1]
            for ix in range(segments):
                jx = (ix + 1) % segments
                self.faces.append((r0 + ix, r0 + jx, r1 + jx, r1 + ix))

        if start_pole:
            c_bot = len(self.verts) + 1
            self.verts.append((cx, cy, z_offset + profile_rz[0][1]))
            r0 = ring_starts[0]
            for ix in range(segments):
                jx = (ix + 1) % segments
                self.faces.append((r0 + jx, r0 + ix, c_bot))

        if end_pole:
            c_top = len(self.verts) + 1
            self.verts.append((cx, cy, z_offset + profile_rz[-1][1]))
            rl = ring_starts[-1]
            for ix in range(segments):
                jx = (ix + 1) % segments
                self.faces.append((rl + ix, rl + jx, c_top))

    def add_tube_ring(self, loop_rz, segments=16, cx=0.0, cy=0.0, z_offset=0.0):
        """Revolve a closed CCW cross-section loop [(r,z), ...] around Z."""
        m = len(loop_rz)
        ring_starts = []
        for r, z in loop_rz:
            ring_starts.append(len(self.verts) + 1)
            for ix in range(segments):
                a = 2.0 * math.pi * ix / segments
                self.verts.append((cx + r * math.cos(a), cy + r * math.sin(a), z_offset + z))
        for k in range(m):
            k2 = (k + 1) % m
            r0 = ring_starts[k]
            r1 = ring_starts[k2]
            for ix in range(segments):
                jx = (ix + 1) % segments
                self.faces.append((r0 + ix, r0 + jx, r1 + jx, r1 + ix))

    def add_deformed_sphere(
        self, cx, cy, cz, rx, ry, rz, subdivisions=2, noise_fn=None, flatten_z_min=None
    ):
        unit_v, unit_f, _, _ = gen_icosphere(1.0, subdivisions)
        base = len(self.verts)
        for nx, ny, nz in unit_v:
            scale = noise_fn(nx, ny, nz) if noise_fn else 1.0
            vx = cx + nx * rx * scale
            vy = cy + ny * ry * scale
            vz = cz + nz * rz * scale
            if flatten_z_min is not None and vz < flatten_z_min:
                vz = flatten_z_min
            self.verts.append((vx, vy, vz))
        for a, b, c in unit_f:
            self.faces.append((base + a, base + b, base + c))

    def add_hex_crystal(
        self, cx, cy, z0, radius, body_h, tip_h, tilt_deg=0.0, azimuth_deg=0.0, yaw_deg=0.0
    ):
        segs = 6
        local_verts = []
        for z in (0.0, body_h):
            r_cur = radius * (0.86 if z > 0 else 1.0)
            for ix in range(segs):
                a = 2.0 * math.pi * ix / segs
                local_verts.append((r_cur * math.cos(a), r_cur * math.sin(a), z))
        local_verts.append((0.0, 0.0, body_h + tip_h))  # apex (index 12)
        local_verts.append((0.0, 0.0, 0.0))             # bot center (index 13)

        base = len(self.verts) + 1
        for pt in local_verts:
            p1 = _rotate_euler(pt, yaw_deg, tilt_deg, 0.0)
            p2 = _rotate_euler(p1, azimuth_deg, 0.0, 0.0)
            self.verts.append((cx + p2[0], cy + p2[1], z0 + p2[2]))

        r0 = base
        r1 = base + segs
        apex = base + 2 * segs
        bot_c = base + 2 * segs + 1
        for ix in range(segs):
            jx = (ix + 1) % segs
            self.faces.append((r0 + ix, r0 + jx, r1 + jx, r1 + ix))
            self.faces.append((r1 + ix, r1 + jx, apex))
            self.faces.append((r0 + jx, r0 + ix, bot_c))

    def add_arch_wedge(self, r_in, r_out, t0, t1, y_half, center_z):
        """Closed 8-vertex voussoir wedge sweeping XZ angle [t0..t1] around (0, 0, center_z)."""
        c0, s0 = math.cos(t0), math.sin(t0)
        c1, s1 = math.cos(t1), math.sin(t1)
        xz = [
            (-r_in * c0, center_z + r_in * s0),
            (-r_in * c1, center_z + r_in * s1),
            (-r_out * c1, center_z + r_out * s1),
            (-r_out * c0, center_z + r_out * s0),
        ]
        base = len(self.verts) + 1
        for x, z in xz:
            self.verts.append((x, -y_half, z))
        for x, z in xz:
            self.verts.append((x, y_half, z))
        # front (-Y), back (+Y), inner arc, t1 cap, outer arc, t0 cap
        self.faces.extend([
            (base + 0, base + 1, base + 2, base + 3),
            (base + 4, base + 7, base + 6, base + 5),
            (base + 0, base + 4, base + 5, base + 1),
            (base + 1, base + 5, base + 6, base + 2),
            (base + 2, base + 6, base + 7, base + 3),
            (base + 3, base + 7, base + 4, base + 0),
        ])


def gen_cliff_ring(radius=3700, segments=24, height=600):
    """Closed craggy cliff rim encircling the arena (inward cliff faces visible from inside)."""
    rng = random.Random(999)
    b = _MeshBuilder()
    # Closed cross-section profile around arena perimeter:
    # inner toe -> craggy inner cliffs -> jagged crest -> outer slope -> outer toe
    r_base_in = radius - 140.0
    r_base_out = radius + 420.0
    rings_xy = []
    # Build per-segment perturbed rings for a solid closed cliff wall
    profile_specs = [
        (0.0, 0.0, 60.0),      # inner toe
        (0.38, 0.35, 90.0),    # inner cliff mid
        (0.85, 0.92, 110.0),   # inner crest lip
        (1.30, 1.00, 115.0),   # ridge crest
        (2.10, 0.55, 80.0),    # outer slope
        (2.40, 0.0, 40.0),     # outer toe
    ]
    # Emit loop of rings in (r, z) per angle ix so inward face points toward (0,0)
    m = len(profile_specs)
    ring_starts = []
    # Perturb per-angle height and radius deterministically
    ang_h = [rng.uniform(0.80, 1.18) for _ in range(segments)]
    ang_r = [rng.uniform(-70.0, 70.0) for _ in range(segments)]
    for k, (r_frac, z_frac, jitter) in enumerate(profile_specs):
        ring_starts.append(len(b.verts) + 1)
        for ix in range(segments):
            a = 2.0 * math.pi * ix / segments
            r = _lerp(r_base_in, r_base_out, r_frac / 2.40) + ang_r[ix] + rng.uniform(-jitter * 0.3, jitter * 0.3)
            z = z_frac * height * ang_h[ix] - (20.0 if z_frac == 0.0 else 0.0)
            b.verts.append((r * math.cos(a), r * math.sin(a), z))

    # Going CCW in (r, z) starting inner-toe -> inner-crest -> outer-crest -> outer-toe -> inner-toe
    # Note: inner face goes UP (dz>0), so with outward radial=(+r), inner face needs (-r) -> reverse quad order!
    for k in range(m):
        k2 = (k + 1) % m
        r0 = ring_starts[k]
        r1 = ring_starts[k2]
        for ix in range(segments):
            jx = (ix + 1) % segments
            b.faces.append((r0 + jx, r0 + ix, r1 + ix, r1 + jx))
    return b.verts, b.faces, None, None


def gen_arena_plaza(radius=580):
    """Stepped circular stone dais (r=580cm, two broad circular steps + inner carved ring)."""
    b = _MeshBuilder()
    segs = 32
    # Step 1: r=580, z=0..14
    b.add_lathe([(0.0, 0.0), (radius, 0.0), (radius, 14.0), (0.0, 14.0)], segments=segs)
    # Step 2: r=450, z=14..28
    r2 = radius * 0.776
    b.add_lathe([(0.0, 14.0), (r2, 14.0), (r2, 28.0), (0.0, 28.0)], segments=segs)
    # Central medallion slab: r=145, z=28..31
    b.add_lathe([(0.0, 28.0), (145.0, 28.0), (145.0, 31.0), (0.0, 31.0)], segments=segs)
    # Inner carved ring: r=195..250, z=28..34 (beveled top)
    b.add_tube_ring(
        [(195.0, 28.0), (250.0, 28.0), (244.0, 34.0), (201.0, 34.0)],
        segments=segs,
    )
    # 8 carved radial spokes bridging inner ring to outer step
    for i in range(8):
        yaw = i * 45.0
        rad = math.radians(yaw)
        cx = 345.0 * math.cos(rad)
        cy = 345.0 * math.sin(rad)
        b.add_box(cx, cy, 30.0, 92.0, 18.0, 2.0, yaw=yaw)
    return b.verts, b.faces, None, None


def gen_wizard_staff(height=155):
    """~155cm carved wooden staff centered at Z=0..155cm with flared fork head cradling StaffOrbMesh."""
    b = _MeshBuilder()
    segs = 12
    # Bottom metal/wood ferrule (z=0..9)
    b.add_lathe(
        [(0.0, 0.0), (1.7, 0.0), (2.2, 4.0), (2.4, 9.0), (0.0, 9.0)],
        segments=segs,
    )
    # Carved wooden shaft with subtle organic curve & collar bands (z=8..126)
    n_rings = 14
    rings_xy = []
    rings_z = []
    for iy in range(n_rings + 1):
        t = iy / n_rings
        z = 8.0 + t * (126.0 - 8.0)
        bend_x = 1.4 * math.sin(t * math.pi)
        r = 2.1 + 0.45 * t
        # Decorative carved swells at grip and collars
        if abs(t - 0.30) < 0.04 or abs(t - 0.68) < 0.04 or t > 0.94:
            r += 0.45
        pts = []
        for ix in range(segs):
            a = 2.0 * math.pi * ix / segs
            pts.append((bend_x + r * math.cos(a), r * math.sin(a)))
        rings_xy.append(pts)
        rings_z.append(z)
    b.add_ring_stack(rings_xy, rings_z, cap_bottom=True, cap_top=True)

    # Flared neck collar & central orb seat (z=124..140)
    b.add_lathe(
        [(0.0, 124.0), (2.8, 124.0), (4.2, 131.0), (4.6, 136.0), (3.0, 140.0), (0.0, 138.0)],
        segments=segs,
    )

    # Twin flared fork prongs around +-X cradling the orb at (0, 0, height=155)
    for sign in (-1.0, 1.0):
        prong_pts = [
            (sign * 2.5, 132.0, 2.2),
            (sign * 6.5, 138.0, 2.0),
            (sign * 9.5, 146.0, 1.8),
            (sign * 9.0, 153.5, 1.5),
            (sign * 6.4, height + 4.0, 1.0),
        ]
        p_rings = []
        p_zs = []
        for px, pz, pr in prong_pts:
            ring = []
            for ix in range(8):
                a = 2.0 * math.pi * ix / 8
                ring.append((px + pr * math.cos(a), pr * 0.78 * math.sin(a)))
            p_rings.append(ring)
            p_zs.append(pz)
        b.add_ring_stack(p_rings, p_zs, cap_bottom=True, cap_top=True)
    return b.verts, b.faces, None, None


def gen_grass_blade(height=30, width=4):
    """3-blade grass tuft with outward triangles and base-to-tip UV coords."""
    b = _MeshBuilder()
    blade_specs = [
        (0.0, 0.0, 0.0, 1.0),
        (1.8, -1.2, 115.0, 0.85),
        (-1.6, 1.4, 235.0, 0.92),
    ]
    uvs = []
    for ox, oy, yaw, sc in blade_specs:
        h = height * sc
        w = width * 0.5
        pts = [
            (-w, 0.0, 0.0), (w, 0.0, 0.0),
            (w * 0.65, 2.0, h * 0.55), (-w * 0.65, 2.0, h * 0.55),
            (0.0, 5.5, h),
        ]
        base = len(b.verts) + 1
        for pt in pts:
            rx, ry, rz = _rotate_euler(pt, yaw_deg=yaw)
            b.verts.append((ox + rx, oy + ry, rz))
        b.faces.extend([
            (base + 0, base + 1, base + 2, base + 3),
            (base + 3, base + 2, base + 4),
        ])
        uvs.extend([(0.0, 0.0), (1.0, 0.0), (0.85, 0.55), (0.15, 0.55), (0.5, 1.0)])
    return b.verts, b.faces, None, uvs


def gen_bush(radius=40, height=50):
    """Multi-cluster organic bush with guaranteed outward winding."""
    b = _MeshBuilder()
    rng = random.Random(1234)
    clusters = [
        (0.0, 0.0, height * 0.48, radius * 0.85, radius * 0.85, height * 0.52),
        (radius * 0.35, radius * 0.2, height * 0.36, radius * 0.58, radius * 0.55, height * 0.40),
        (-radius * 0.32, -radius * 0.22, height * 0.38, radius * 0.56, radius * 0.54, height * 0.42),
    ]
    for idx, (cx, cy, cz, rx, ry, rz) in enumerate(clusters):
        b.add_deformed_sphere(
            cx, cy, cz, rx, ry, rz, subdivisions=1,
            noise_fn=lambda x, y, z, s=idx: 0.80 + 0.35 * fbm_2d(x * 1.5 + s, y * 1.5 - z, 3, 2.2, seed=1234 + s),
            flatten_z_min=0.0,
        )
    return b.verts, b.faces, None, None


def gen_flower(height=25):
    """3D flower stem + 5 angled petals (outward normals)."""
    b = _MeshBuilder()
    # Tapered 4-sided stem
    b.add_ring_stack(
        [[(-0.8, -0.8), (0.8, -0.8), (0.8, 0.8), (-0.8, 0.8)],
         [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]],
        [0.0, height],
    )
    # 5 angled petal quads around top
    n_pet = 5
    pr = 8.0
    for i in range(n_pet):
        a = 2.0 * math.pi * i / n_pet
        cx = pr * 0.5 * math.cos(a)
        cy = pr * 0.5 * math.sin(a)
        b.add_box(cx, cy, height + 1.5, 4.2, 2.2, 0.6, yaw=math.degrees(a), pitch=-22.0)
    return b.verts, b.faces, None, None


def gen_small_stone(radius=12):
    """Faceted small boulder stone with guaranteed outward winding and flat bottom."""
    b = _MeshBuilder()
    b.add_deformed_sphere(
        0.0, 0.0, radius * 0.45, radius, radius * 0.88, radius * 0.68, subdivisions=1,
        noise_fn=lambda x, y, z: 0.76 + 0.40 * fbm_2d(x * 1.7 + 0.5, y * 1.7 + z, 3, 2.5, seed=5678),
        flatten_z_min=0.0,
    )
    return b.verts, b.faces, None, None


def gen_ruin_column(radius=35, height=250):
    """Fluted ruined stone column with square plinth and closed ends."""
    b = _MeshBuilder()
    b.add_box(0.0, 0.0, 14.0, radius * 1.15, radius * 1.15, 14.0)
    segs = 14
    rings_xy = []
    rings_z = []
    for iy in range(7):
        t = iy / 6.0
        z = 28.0 + t * (height - 28.0)
        r = radius * (1.0 - 0.10 * t)
        pts = []
        for ix in range(segs):
            a = 2.0 * math.pi * ix / segs
            flute = 1.0 - 0.06 * (1.0 - math.cos(7.0 * a))
            pts.append((r * flute * math.cos(a), r * flute * math.sin(a)))
        rings_xy.append(pts)
        rings_z.append(z)
    b.add_ring_stack(rings_xy, rings_z, cap_bottom=True, cap_top=True)
    return b.verts, b.faces, None, None


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

    for ix in range(sx):
        faces.append((f_idx(0, ix), b_idx(0, ix), b_idx(0, ix + 1), f_idx(0, ix + 1)))
        faces.append((f_idx(sy, ix), f_idx(sy, ix + 1), b_idx(sy, ix + 1), b_idx(sy, ix)))

    for iy in range(sy):
        faces.append((f_idx(iy, 0), f_idx(iy + 1, 0), b_idx(iy + 1, 0), b_idx(iy, 0)))
        faces.append((f_idx(iy, sx), b_idx(iy, sx), b_idx(iy + 1, sx), f_idx(iy + 1, sx)))

    return verts, faces, None, None


ENV_MESH_GENERATORS = {
    "terrain_bowl": gen_terrain_bowl,
    "cliff_ring":   gen_cliff_ring,
    "arena_plaza":  gen_arena_plaza,
    "wizard_staff": gen_wizard_staff,
    "grass_blade":  gen_grass_blade,
    "bush":         gen_bush,
    "flower":       gen_flower,
    "small_stone":  gen_small_stone,
    "ruin_column":  gen_ruin_column,
    "ruin_wall":    gen_ruin_wall,
}

# ── Sculpted Whole Destructible Meshes (base at Z=0 matching bbox bounds) ─────

def gen_whole_pot(half_ext, wide=True):
    """Lathe-turned amphora/urn (14-seg ring, footed base, curved belly, neck, rim)."""
    hx, hy, hz = half_ext
    rmax = min(hx, hy)
    h = 2.0 * hz
    if wide:
        profile = [
            (0.0, 0.0), (0.52 * rmax, 0.0), (0.58 * rmax, 0.08 * h), (0.48 * rmax, 0.16 * h),
            (0.88 * rmax, 0.40 * h), (1.00 * rmax, 0.58 * h), (0.80 * rmax, 0.74 * h),
            (0.46 * rmax, 0.85 * h), (0.60 * rmax, 0.94 * h), (0.62 * rmax, 1.00 * h),
            (0.48 * rmax, 1.00 * h), (0.38 * rmax, 0.84 * h), (0.55 * rmax, 0.62 * h),
            (0.0, 0.62 * h),
        ]
    else:
        profile = [
            (0.0, 0.0), (0.55 * rmax, 0.0), (0.62 * rmax, 0.08 * h), (0.46 * rmax, 0.18 * h),
            (0.92 * rmax, 0.45 * h), (1.00 * rmax, 0.64 * h), (0.74 * rmax, 0.79 * h),
            (0.46 * rmax, 0.88 * h), (0.64 * rmax, 0.95 * h), (0.66 * rmax, 1.00 * h),
            (0.50 * rmax, 1.00 * h), (0.38 * rmax, 0.87 * h), (0.55 * rmax, 0.66 * h),
            (0.0, 0.66 * h),
        ]
    b = _MeshBuilder()
    b.add_lathe(profile, segments=14)
    return b.verts, b.faces


def gen_whole_barrel(half_ext):
    """Bulged 14-seg wooden barrel with recessed lids and 4 raised hoop bands."""
    hx, hy, hz = half_ext
    rmax = min(hx, hy)
    h = 2.0 * hz
    r_end = rmax * 0.78
    r_belly = rmax * 0.95
    b = _MeshBuilder()
    profile = [
        (0.0, 0.04 * h), (r_end * 0.92, 0.04 * h), (r_end * 0.96, 0.0), (r_end, 0.0),
        (0.88 * rmax, 0.20 * h), (r_belly, 0.50 * h), (0.88 * rmax, 0.80 * h),
        (r_end, h), (r_end * 0.96, h), (r_end * 0.92, 0.96 * h), (0.0, 0.96 * h),
    ]
    b.add_lathe(profile, segments=14)
    # 4 raised iron/wood hoop bands
    for t_lo, t_hi, r_frac in [
        (0.10, 0.15, 0.86),
        (0.30, 0.35, 0.94),
        (0.65, 0.70, 0.94),
        (0.85, 0.90, 0.86),
    ]:
        r_in = rmax * (r_frac - 0.02)
        r_out = rmax * (r_frac + 0.05)
        b.add_tube_ring(
            [(r_in, t_lo * h), (r_out, t_lo * h), (r_out, t_hi * h), (r_in, t_hi * h)],
            segments=14,
        )
    return b.verts, b.faces


def gen_whole_crate(half_ext):
    """Wooden frame crate with recessed side panels, corner posts, and diagonal braces."""
    hx, hy, hz = half_ext
    h = 2.0 * hz
    b = _MeshBuilder()
    # Recessed inner wooden box
    b.add_box(0.0, 0.0, hz, hx * 0.87, hy * 0.87, hz * 0.92)
    # 4 corner posts
    pw = max(4.0, min(hx, hy) * 0.125)
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            b.add_box(sx * (hx - pw), sy * (hy - pw), hz, pw, pw, hz)
    # Top & bottom horizontal perimeter rails
    rail_h = pw
    for zc in (rail_h, h - rail_h):
        for sy in (-1.0, 1.0):
            b.add_box(0.0, sy * (hy - pw * 0.7), zc, hx - 2 * pw, pw * 0.7, rail_h)
        for sx in (-1.0, 1.0):
            b.add_box(sx * (hx - pw * 0.7), 0.0, zc, pw * 0.7, hy - 2 * pw, rail_h)
    # Diagonal cross braces on the 4 faces
    diag_len = math.hypot(hx - 2 * pw, hz - 2 * rail_h) * 0.92
    ang = math.degrees(math.atan2(hz - 2 * rail_h, hx - 2 * pw))
    for sy in (-1.0, 1.0):
        b.add_box(0.0, sy * (hy - pw * 0.8), hz, diag_len, pw * 0.35, pw * 0.7, pitch=ang)
    for sx in (-1.0, 1.0):
        b.add_box(sx * (hx - pw * 0.8), 0.0, hz, pw * 0.35, diag_len, pw * 0.7, roll=ang)
    return b.verts, b.faces


def gen_whole_pillar(half_ext):
    """Fluted temple column with square plinth, torus base, fluted shaft, and stepped capital."""
    hx, hy, hz = half_ext
    r = min(hx, hy)
    h = 2.0 * hz
    b = _MeshBuilder()
    # Square plinth base
    plinth_h = h * 0.08
    b.add_box(0.0, 0.0, plinth_h * 0.5, hx, hy, plinth_h * 0.5)
    # Molded torus base
    b.add_lathe(
        [(0.0, plinth_h), (r * 0.92, plinth_h), (r * 0.92, plinth_h + h * 0.025),
         (r * 0.78, plinth_h + h * 0.05), (0.0, plinth_h + h * 0.05)],
        segments=20,
    )
    # Fluted cylindrical shaft
    z_bot = plinth_h + h * 0.045
    z_top = h * 0.88
    segs = 24
    rings_xy = []
    rings_z = []
    for iy in range(9):
        t = iy / 8.0
        z = _lerp(z_bot, z_top, t)
        r_shaft = r * (0.76 - 0.10 * t)
        pts = []
        for ix in range(segs):
            a = 2.0 * math.pi * ix / segs
            flute = 1.0 - 0.055 * (1.0 - math.cos(12.0 * a))
            pts.append((r_shaft * flute * math.cos(a), r_shaft * flute * math.sin(a)))
        rings_xy.append(pts)
        rings_z.append(z)
    b.add_ring_stack(rings_xy, rings_z, cap_bottom=True, cap_top=True)
    # Stepped echinus capital
    b.add_lathe(
        [(0.0, z_top), (r * 0.68, z_top), (r * 0.84, z_top + h * 0.025),
         (r * 0.94, z_top + h * 0.055), (0.0, z_top + h * 0.055)],
        segments=20,
    )
    # Square abacus cap
    cap_z0 = z_top + h * 0.055
    cap_hz = (h - cap_z0) * 0.5
    b.add_box(0.0, 0.0, cap_z0 + cap_hz, hx * 0.96, hy * 0.96, cap_hz)
    return b.verts, b.faces


def gen_whole_arch(half_ext):
    """True open stone archway: left & right piers + semicircular arch voussoirs + top entablature."""
    hx, hy, hz = half_ext
    h = 2.0 * hz
    b = _MeshBuilder()
    r_in = hx * 0.56          # 140 cm clear half-width -> 280 cm open passage
    pier_z_top = h * 0.567    # 340 cm impost height
    pier_cx = 0.5 * (r_in + hx)
    pier_hx = 0.5 * (hx - r_in)

    # Left & right piers (stepped plinth, shaft, impost capital)
    for sign in (-1.0, 1.0):
        px = sign * pier_cx
        b.add_box(px, 0.0, 22.0, pier_hx, hy, 22.0)
        b.add_box(px, 0.0, 177.0, pier_hx * 0.88, hy * 0.88, 133.0)
        b.add_box(px, 0.0, pier_z_top - 15.0, pier_hx * 1.02, hy, 15.0)

    # Semicircular stone voussoir arch (12 stone wedges)
    r_out = hx * 0.84         # 210 cm outer radius (340 + 210 = 550 cm)
    n_wedges = 12
    for i in range(n_wedges):
        t0 = math.pi * i / n_wedges
        t1 = math.pi * (i + 1) / n_wedges
        b.add_arch_wedge(r_in, r_out, t0, t1, hy * 0.88, pier_z_top)

    # Keystone crown accent
    b.add_box(0.0, 0.0, pier_z_top + 0.5 * (r_in + r_out), 24.0, hy, 0.5 * (r_out - r_in) + 12.0)

    # Spandrel masonry blocks linking outer piers to entablature
    entab_z0 = pier_z_top + r_out - 12.0  # ~538 cm
    for sign in (-1.0, 1.0):
        b.add_box(sign * (hx * 0.86), 0.0, 0.5 * (pier_z_top + entab_z0), hx * 0.13, hy * 0.80, 0.5 * (entab_z0 - pier_z_top))
        b.add_box(sign * (hx * 0.62), 0.0, entab_z0 - 24.0, hx * 0.16, hy * 0.80, 24.0)

    # Top entablature (architrave + projecting cornice up to 2*hz=600)
    arch_mid = 0.5 * (entab_z0 + (h - 24.0))
    b.add_box(0.0, 0.0, arch_mid, hx * 0.98, hy * 0.92, (h - 24.0 - entab_z0) * 0.5)
    b.add_box(0.0, 0.0, h - 12.0, hx, hy, 12.0)
    return b.verts, b.faces


def gen_whole_tower(half_ext):
    """Multi-tier watchtower: battered plinth + octagonal shaft + corbel collar + crenellated battlements."""
    hx, hy, hz = half_ext
    r = min(hx, hy)
    h = 2.0 * hz
    b = _MeshBuilder()
    segs = 8

    def _oct_ring(rad):
        return [(rad * math.cos(2 * math.pi * i / segs + math.pi / segs),
                 rad * math.sin(2 * math.pi * i / segs + math.pi / segs)) for i in range(segs)]

    # Battered plinth + lower & upper octagonal shaft + mid-belt + corbel collar
    tier_specs = [
        (r * 1.02, 0.0),
        (r * 0.82, 0.16 * h),
        (r * 0.78, 0.46 * h),
        (r * 0.84, 0.47 * h),
        (r * 0.84, 0.50 * h),
        (r * 0.78, 0.51 * h),
        (r * 0.73, 0.76 * h),
        (r * 0.84, 0.80 * h),
        (r * 0.96, 0.84 * h),
    ]
    b.add_ring_stack([_oct_ring(rad) for rad, _ in tier_specs], [z for _, z in tier_specs], cap_bottom=True, cap_top=True)

    # Battlement parapet ring (hollow octagonal collar z=0.84*h .. 0.92*h)
    parapet_z0 = 0.84 * h
    parapet_z1 = 0.92 * h
    b.add_tube_ring(
        [(r * 0.70, parapet_z0), (r * 0.96, parapet_z0), (r * 0.96, parapet_z1), (r * 0.70, parapet_z1)],
        segments=segs,
    )

    # 8 crenellated merlons around the top rim (z=0.92*h .. 1.00*h)
    merlon_h = (h - parapet_z1) * 0.5
    merlon_cz = parapet_z1 + merlon_h
    for i in range(segs):
        yaw = i * (360.0 / segs) + (180.0 / segs)
        rad_a = math.radians(yaw)
        mr = r * 0.83
        b.add_box(mr * math.cos(rad_a), mr * math.sin(rad_a), merlon_cz, r * 0.13, r * 0.24, merlon_h, yaw=yaw)
    return b.verts, b.faces


def gen_whole_wall(half_ext, seed=101):
    """Castle stone wall with stepped base, main ashlar face, cornice, and broken crenellations."""
    hx, hy, hz = half_ext
    h = 2.0 * hz
    rng = random.Random(seed)
    b = _MeshBuilder()
    # Stepped plinth base
    b.add_box(0.0, 0.0, 0.07 * h, hx, hy, 0.07 * h)
    # Main ashlar body
    b.add_box(0.0, 0.0, 0.47 * h, hx * 0.97, hy * 0.80, 0.33 * h)
    # Projecting cornice
    b.add_box(0.0, 0.0, 0.825 * h, hx * 0.99, hy * 0.94, 0.025 * h)
    # Broken crenellated merlons across top
    n_merlons = 4 if hx <= 220 else 6
    span = hx * 1.90
    step = span / n_merlons
    merlon_w = step * 0.34
    base_z = 0.85 * h
    for i in range(n_merlons):
        cx = -0.95 * hx + step * (i + 0.5)
        # Deterministically chip one or two merlons for ruined silhouette
        scale_h = 0.45 if (i == 1 or (n_merlons > 4 and i == 4)) else rng.uniform(0.85, 1.0)
        mh = (h - base_z) * 0.5 * scale_h
        b.add_box(cx, 0.0, base_z + mh, merlon_w, hy * 0.82, mh)
    return b.verts, b.faces


def gen_whole_rock(half_ext, seed=202, subdivisions=2):
    """Faceted multi-octave displaced boulder with flattened seat at Z=0."""
    hx, hy, hz = half_ext
    b = _MeshBuilder()
    b.add_deformed_sphere(
        0.0, 0.0, hz * 0.85, hx * 0.92, hy * 0.92, hz * 0.96,
        subdivisions=subdivisions,
        noise_fn=lambda x, y, z: min(
            1.05,
            0.70
            + 0.30 * fbm_2d(x * 1.6 + 1.3, y * 1.6 + z * 1.1, 4, 2.4, seed=seed)
            + 0.12 * abs(math.sin(x * 3.1 + z * 2.7)),
        ),
        flatten_z_min=0.0,
    )
    return b.verts, b.faces


def gen_whole_tree(half_ext):
    """Tapered trunk with root flare + 3 layered irregular canopy clusters."""
    hx, hy, hz = half_ext
    h = 2.0 * hz
    b = _MeshBuilder()
    # Tapered trunk (z=0..0.60*h)
    segs = 8
    rings_xy = []
    rings_z = []
    trunk_specs = [
        (0.0, 22.0, 0.0, 0.0),
        (0.08 * h, 14.5, 1.5, -1.0),
        (0.32 * h, 11.5, -2.5, 2.0),
        (0.58 * h, 7.5, 1.0, 1.5),
    ]
    for z, rad, ox, oy in trunk_specs:
        pts = [(ox + rad * math.cos(2 * math.pi * i / segs),
                oy + rad * math.sin(2 * math.pi * i / segs)) for i in range(segs)]
        rings_xy.append(pts)
        rings_z.append(z)
    b.add_ring_stack(rings_xy, rings_z, cap_bottom=True, cap_top=True)

    # 3 layered irregular foliage canopy clusters
    canopy_tiers = [
        (8.0, -6.0, 0.54 * h, hx * 0.96, hy * 0.92, 0.15 * h, 901),
        (-9.0, 8.0, 0.72 * h, hx * 0.84, hy * 0.82, 0.14 * h, 902),
        (3.0, 2.0, 0.88 * h, hx * 0.64, hy * 0.62, 0.12 * h, 903),
    ]
    for cx, cy, cz, rx, ry, rz, s in canopy_tiers:
        b.add_deformed_sphere(
            cx, cy, cz, rx, ry, rz, subdivisions=2,
            noise_fn=lambda x, y, z, sd=s: 0.78 + 0.34 * fbm_2d(x * 1.4 + sd, y * 1.4 - z, 3, 2.2, seed=sd),
        )
    return b.verts, b.faces


def gen_whole_crystal(half_ext):
    """Central tall hexagonal crystal spire + 3 angled side crystal shards."""
    hx, hy, hz = half_ext
    r = min(hx, hy)
    h = 2.0 * hz
    b = _MeshBuilder()
    # Central spire
    b.add_hex_crystal(0.0, 0.0, 0.0, radius=r * 0.52, body_h=h * 0.75, tip_h=h * 0.25)
    # 3 angled side crystal shards (seated slightly above Z=0 so tilted bases stay >= 0)
    shards = [
        (15.0,  22.0, r * 0.32, h * 0.45, h * 0.16),
        (135.0, 26.0, r * 0.28, h * 0.38, h * 0.15),
        (255.0, 20.0, r * 0.34, h * 0.48, h * 0.17),
    ]
    for az, tilt, sr, bh, th in shards:
        rad_a = math.radians(az)
        b.add_hex_crystal(
            r * 0.24 * math.cos(rad_a),
            r * 0.24 * math.sin(rad_a),
            3.5,
            radius=sr,
            body_h=bh,
            tip_h=th,
            tilt_deg=tilt,
            azimuth_deg=az,
            yaw_deg=az * 0.5,
        )
    return b.verts, b.faces


def gen_whole_mesh(shape_name: str, half_ext: tuple):
    """Dispatch sculpted Whole mesh matching shape bbox with base at Z=0."""
    if shape_name == "Pot_A":
        return gen_whole_pot(half_ext, wide=True)
    if shape_name == "Pot_B":
        return gen_whole_pot(half_ext, wide=False)
    if shape_name == "Barrel_A":
        return gen_whole_barrel(half_ext)
    if shape_name == "Crate_A":
        return gen_whole_crate(half_ext)
    if shape_name == "Pillar_A":
        return gen_whole_pillar(half_ext)
    if shape_name == "Arch_A":
        return gen_whole_arch(half_ext)
    if shape_name == "Tower_A":
        return gen_whole_tower(half_ext)
    if shape_name == "Wall_A":
        return gen_whole_wall(half_ext, seed=101)
    if shape_name == "Wall_B":
        return gen_whole_wall(half_ext, seed=102)
    if shape_name == "BigRock_A":
        return gen_whole_rock(half_ext, seed=201, subdivisions=2)
    if shape_name == "BigRock_B":
        return gen_whole_rock(half_ext, seed=202, subdivisions=2)
    if shape_name == "SmRock_A":
        return gen_whole_rock(half_ext, seed=203, subdivisions=1)
    if shape_name == "SmRock_B":
        return gen_whole_rock(half_ext, seed=204, subdivisions=1)
    if shape_name == "Tree_A":
        return gen_whole_tree(half_ext)
    if shape_name == "Crystal_A":
        return gen_whole_crystal(half_ext)

    b = _MeshBuilder()
    b.add_box(0.0, 0.0, half_ext[2], *half_ext)
    return b.verts, b.faces

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
                subdivs=params.get("subdivs", 64),
            )
        write_obj(path, verts, faces, normals, uvs)
        print(f"  {name}: {len(verts)} verts, {len(faces)} faces -> {path.name}")
        total += 1

    def _ground_z(x, y, base_z=0.0):
        return round(terrain_sampler(x, y) + base_z, 3) if terrain_sampler else base_z

    def _plaza_surface_z(x, y, base_z=0.0):
        """Return terrain Z + stepped SM_ArenaPlaza top elevation when standing on the dais."""
        r = math.hypot(x, y)
        dais_h = 0.0
        if r <= 145.0:
            dais_h = 31.0
        elif r <= 195.0:
            dais_h = 28.0
        elif r <= 250.0:
            dais_h = 34.0
        elif r <= 450.0:
            dais_h = 28.0
        elif r <= 580.0:
            dais_h = 14.0
        return _ground_z(x, y, base_z + dais_h)

    # 6. Destructible fracture pieces
    print("=== Destructible Fracture ===")
    destruction_data = {}
    for shape_name, material, half_ext, n_pieces, health in C.DESTRUCTIBLE_SHAPES:
        shape_dir = dest_dir / shape_name
        shape_dir.mkdir(parents=True, exist_ok=True)

        # Generate sculpted Whole mesh with base at Z=0 matching bbox bounds
        whole_verts, whole_faces = gen_whole_mesh(shape_name, half_ext)
        write_obj(shape_dir / f"SM_{shape_name}_Whole.obj", whole_verts, whole_faces)

        seed = zlib.crc32(shape_name.encode("utf-8")) & 0xFFFF
        pieces = voronoi_fracture_box(shape_name, half_ext, n_pieces, seed=seed)

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
        print(f"  {shape_name}: {len(pieces)} pieces ({material}, whole={len(whole_verts)}v/{len(whole_faces)}f)")
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
        attempts = 0
        while len(instances) < count and attempts < count * 8:
            attempts += 1
            angle = scatter_rng.uniform(0, 2 * math.pi)
            radius = scatter_rng.uniform(r_min, r_max)
            sx = radius * math.cos(angle)
            sy = radius * math.sin(angle)
            # Keep paved dais, player spawn circle, and tower interior clear of vegetation
            if math.hypot(sx, sy) < 610.0:
                continue
            if math.hypot(sx, sy + 900.0) < 220.0:
                continue
            if math.hypot(sx, sy - 2200.0) < 150.0:
                continue
            sz = _ground_z(sx, sy, z_off)
            yaw = scatter_rng.uniform(0, 360)
            scale = scatter_rng.uniform(0.7, 1.3)
            instances.append([round(sx, 2), round(sy, 2), sz, round(yaw, 2), round(scale, 3)])
        scatter_layout[mesh_name] = {**cfg, "instances": instances}

    ruin_props = []
    ruin_rings = {
        "SM_RuinColumn": C._ring(4, 2900, 0, math.pi / 4),
        "SM_RuinWall":   C._ring(4, 2950, 0, math.pi / 4 + math.pi / 6),
    }
    for ruin_name, ring_pts in ruin_rings.items():
        for i, p in enumerate(ring_pts):
            ruin_props.append({
                "mesh": ruin_name,
                "label": f"{ruin_name}_{i}",
                "x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3],
            })

    layout = {
        "player_start": {
            "x": 0.0,
            "y": -900.0,
            "z": _ground_z(0.0, -900.0, 125.0),
            "yaw": 90.0,
        },
        "arena_plaza": {
            "x": 0.0,
            "y": 0.0,
            "z": _ground_z(0.0, 0.0, 0.0),
            "yaw": 0.0,
        },
        "structures": [
            {"definition": name, "x": p[0], "y": p[1], "z": _ground_z(p[0], p[1], p[2]), "yaw": p[3]}
            for name, p in C.STRUCTURE_PLACEMENTS
        ],
        "breakables": {
            def_name: [
                {"x": p[0], "y": p[1], "z": _plaza_surface_z(p[0], p[1], p[2]), "yaw": p[3]}
                for p in placements
            ]
            for def_name, placements in C.BREAKABLE_PLACEMENTS.items()
        },
        "crystals": [
            {"x": p[0], "y": p[1], "z": _plaza_surface_z(p[0], p[1], p[2]), "yaw": p[3]}
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
