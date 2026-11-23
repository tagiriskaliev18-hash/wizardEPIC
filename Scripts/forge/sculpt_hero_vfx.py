#!/usr/bin/env python3
"""
sculpt_hero_vfx.py - Blender 5.3 headless procedural sculpting & export pipeline
for WizardEPIC (Crimson Desert / @BlendiByl / Black Myth: Wukong visual overhaul).

Exports Z-up cm .obj meshes into Scripts/forge/BlenderAssets/Meshes/ and renders
a Cycles-CPU studio contact sheet for visual verification without GPU contention.
"""

import math
import os
import random
from pathlib import Path

import bpy
import bmesh
from mathutils import Euler, Matrix, Vector, noise

SCRIPT_DIR = Path(__file__).resolve().parent
OUT_MESH_DIR = SCRIPT_DIR / "BlenderAssets" / "Meshes"
PREVIEW_PNG = SCRIPT_DIR / "BlenderAssets" / "hero_vfx_preview_sheet.png"
OUT_MESH_DIR.mkdir(parents=True, exist_ok=True)


# ─────────────────────────────────────────────────────────────────────────────
# Scene & Material Utilities
# ─────────────────────────────────────────────────────────────────────────────

def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_mat(name: str, base_color, roughness=0.5, metallic=0.0, emission=None, emission_strength=0.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
        if emission is not None and "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def create_mesh_object(name: str, bm: bmesh.types.BMesh, materials: list):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for m in materials:
        obj.data.materials.append(m)
    return obj


def set_smooth_shading(obj, smooth=True):
    mesh = obj.data
    mesh.polygons.foreach_set("use_smooth", [smooth] * len(mesh.polygons))
    mesh.update()


def set_box_uvs(bm: bmesh.types.BMesh, scale_cm: float = 100.0):
    uv_layer = bm.loops.layers.uv.verify()
    inv = 1.0 / max(scale_cm, 1e-4)
    for f in bm.faces:
        n = f.normal
        ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
        for loop in f.loops:
            co = loop.vert.co
            if az >= ax and az >= ay:
                u, v = co.x * inv, co.y * inv
            elif ax >= ay and ax >= az:
                u, v = co.y * inv, co.z * inv
            else:
                u, v = co.x * inv, co.z * inv
            loop[uv_layer].uv = (u, v)


def export_obj(obj, filename: str):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    out_path = str(OUT_MESH_DIR / filename)
    bpy.ops.wm.obj_export(
        filepath=out_path,
        export_selected_objects=True,
        forward_axis="Y",
        up_axis="Z",
        global_scale=1.0,
        export_uv=True,
        export_normals=True,
        export_materials=True,
        export_triangulated_mesh=False,
    )
    print(f"[Sculpt] Exported {out_path} ({len(obj.data.vertices)} verts, {len(obj.data.polygons)} polys)")


def add_lofted_tube(
    bm: bmesh.types.BMesh,
    rings: list[list[Vector]],
    mat_index: int = 0,
    closed_u: bool = True,
    cap_start: bool = False,
    cap_end: bool = False,
    u_scale: float = 1.0,
    v_range=(0.0, 1.0),
):
    """Loft a sequence of vertex rings with clean monotonic UVs."""
    uv_layer = bm.loops.layers.uv.verify()
    bm_rings = [[bm.verts.new(p) for p in ring] for ring in rings]
    n_rings = len(bm_rings)
    n_seg = len(bm_rings[0])
    v0, v1 = v_range

    faces = []
    for ri in range(n_rings - 1):
        va = v0 + (v1 - v0) * (ri / max(1, n_rings - 1))
        vb = v0 + (v1 - v0) * ((ri + 1) / max(1, n_rings - 1))
        r0 = bm_rings[ri]
        r1 = bm_rings[ri + 1]
        count = n_seg if closed_u else (n_seg - 1)
        for si in range(count):
            sn = (si + 1) % n_seg
            ua = u_scale * (si / max(1, count))
            ub = u_scale * ((si + 1) / max(1, count))
            verts = [r0[si], r0[sn], r1[sn], r1[si]]
            try:
                f = bm.faces.new(verts)
                f.material_index = mat_index
                f.smooth = True
                uvs = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                for loop, uv in zip(f.loops, uvs):
                    loop[uv_layer].uv = uv
                faces.append(f)
            except ValueError:
                pass

    if cap_start and len(bm_rings[0]) >= 3:
        center = sum((v.co for v in bm_rings[0]), Vector()) / len(bm_rings[0])
        cv = bm.verts.new(center)
        for si in range(n_seg):
            sn = (si + 1) % n_seg
            try:
                f = bm.faces.new([bm_rings[0][sn], bm_rings[0][si], cv])
                f.material_index = mat_index
                for loop in f.loops:
                    loop[uv_layer].uv = (0.5, v0)
                faces.append(f)
            except ValueError:
                pass

    if cap_end and len(bm_rings[-1]) >= 3:
        center = sum((v.co for v in bm_rings[-1]), Vector()) / len(bm_rings[-1])
        cv = bm.verts.new(center)
        for si in range(n_seg):
            sn = (si + 1) % n_seg
            try:
                f = bm.faces.new([bm_rings[-1][si], bm_rings[-1][sn], cv])
                f.material_index = mat_index
                for loop in f.loops:
                    loop[uv_layer].uv = (0.5, v1)
                faces.append(f)
            except ValueError:
                pass

    return faces


# ─────────────────────────────────────────────────────────────────────────────
# 1. SM_WizardHat — Wide-brimmed pleated wizard hat with drooping curved tip
# ─────────────────────────────────────────────────────────────────────────────

def build_wizard_hat(mat_cloth, mat_gold):
    bm = bmesh.new()
    segs = 40

    # --- A. Sweeping wavy brim (inner radius 9.8cm -> outer radius 28.5cm, ~57cm span) ---
    # Pivot (0,0,0) sits right at skull crown base; +Y is forward, +Z is up.
    brim_profile = [
        # (r_frac, z_base, thickness)
        (0.00,  0.2, 1.4),
        (0.18,  0.4, 1.3),
        (0.45, -0.3, 1.15),
        (0.75, -1.9, 1.0),
        (0.95, -3.5, 0.85),
        (1.00, -4.0, 0.45),
    ]
    top_rings = []
    bot_rings = []
    for r_frac, z_b, thick in brim_profile:
        r_top = []
        r_bot = []
        for i in range(segs):
            ang = 2.0 * math.pi * i / segs
            # Slightly elongated front/back oval & organic cloth drape waves
            oval_r = (9.6 + 19.2 * r_frac) * (1.0 + 0.08 * math.sin(ang) + 0.04 * math.cos(2.0 * ang))
            # Organic brim wave: dips slightly over brow (+Y -> sin(ang)=+1) and curves up gracefully on sides
            wave = (
                -2.6 * math.sin(ang) * (r_frac ** 1.4)
                + 1.9 * math.cos(2.0 * ang + 0.35) * (r_frac ** 1.6)
                + 0.85 * math.sin(3.0 * ang - 0.5) * (r_frac ** 1.8)
            )
            x = oval_r * math.cos(ang)
            y = oval_r * math.sin(ang)
            z = z_b + wave
            r_top.append(Vector((x, y, z + thick * 0.5)))
            r_bot.append(Vector((x, y, z - thick * 0.5)))
        top_rings.append(r_top)
        bot_rings.append(r_bot)

    # Loft upper brim surface, outer rim edge, and lower brim surface
    full_brim_rings = top_rings + list(reversed(bot_rings))
    add_lofted_tube(bm, full_brim_rings, mat_index=0, closed_u=True, u_scale=2.0, v_range=(0.0, 1.0))

    # --- B. Tall pleated cone crown with iconic S-curve kinked/drooping wizard tip (~54cm tall) ---
    crown_levels = 22
    crown_rings = []
    for li in range(crown_levels):
        t = li / (crown_levels - 1)  # 0 at base, 1 at pointed tip
        # Height profile with graceful droop at the very tip
        z_center = 58.0 * t - 13.5 * (max(0.0, t - 0.58) ** 2.0) * 5.6
        # S-curve bend sweeping distinctly backward (-Y) and asymmetrically right (+X)
        bend_y = -2.2 * math.sin(t * math.pi * 0.85) - 34.0 * (t ** 2.25)
        bend_x = 2.0 * math.sin(t * math.pi * 1.15) + 11.5 * (t ** 2.35)

        # Radius tapers from 10.6cm at hatband to a crisp 0.7cm tip, with soft horizontal cloth creases
        crease = 0.75 * math.sin(t * math.pi * 6.5) * math.sin(t * math.pi)
        base_rad = max(0.65, 10.7 * ((1.0 - t) ** 1.08) + crease * (1.0 - 0.5 * t))

        # Approximate local tangent to tilt cross-section ring cleanly along the bent spine
        dt = 0.02
        t2 = min(1.0, t + dt)
        z2 = 58.0 * t2 - 13.5 * (max(0.0, t2 - 0.58) ** 2.0) * 5.6
        y2 = -2.2 * math.sin(t2 * math.pi * 0.85) - 34.0 * (t2 ** 2.25)
        x2 = 2.0 * math.sin(t2 * math.pi * 1.15) + 11.5 * (t2 ** 2.35)
        tangent = Vector((x2 - bend_x, y2 - bend_y, z2 - z_center))
        if tangent.length < 1e-4:
            tangent = Vector((0.0, -0.5, 0.86))
        tangent.normalize()
        rot_q = Vector((0.0, 0.0, 1.0)).rotation_difference(tangent)
        center = Vector((bend_x, bend_y, z_center))

        ring = []
        for i in range(segs):
            ang = 2.0 * math.pi * i / segs
            # Subtle vertical cloth pleats/pinches near the kink
            pleat = 1.0 + 0.055 * math.sin(5.0 * ang + t * 3.0) * math.sin(t * math.pi)
            local_p = Vector((base_rad * pleat * math.cos(ang), base_rad * 1.04 * pleat * math.sin(ang), 0.0))
            ring.append(center + (rot_q @ local_p))
        crown_rings.append(ring)

    add_lofted_tube(bm, crown_rings, mat_index=0, closed_u=True, cap_end=True, u_scale=2.0, v_range=(0.0, 1.0))

    # --- C. Sculpted gold-trimmed hatband + ornate front arcane buckle (Mat_GoldTrim, slot 1) ---
    band_profile = [
        (0.5,  11.15),
        (1.0,  11.55),
        (4.8,  11.05),
        (5.4,  10.45),
    ]
    band_rings = []
    for zb, rb in band_profile:
        ring = []
        for i in range(segs):
            ang = 2.0 * math.pi * i / segs
            x = rb * math.cos(ang)
            y = rb * 1.05 * math.sin(ang) - 0.45
            ring.append(Vector((x, y, zb)))
        band_rings.append(ring)
    add_lofted_tube(bm, band_rings, mat_index=1, closed_u=True, u_scale=4.0, v_range=(0.0, 0.3))

    # Ornate beveled front buckle frame on +Y brow of hatband
    buckle_bars = [
        # (cx, cy, cz, sx, sy, sz)
        (0.0,  11.4, 5.2, 6.2, 1.5, 1.1),   # top bar
        (0.0,  11.7, 1.2, 6.2, 1.5, 1.1),   # bottom bar
        (-2.6, 11.5, 3.2, 1.2, 1.6, 4.8),   # left bar
        ( 2.6, 11.5, 3.2, 1.2, 1.6, 4.8),   # right bar
        (0.0,  11.9, 3.2, 1.0, 1.2, 3.4),   # center prong
    ]
    for cx, cy, cz, sx, sy, sz in buckle_bars:
        res = bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=Vector((sx, sy, sz)), verts=res["verts"])
        bmesh.ops.translate(bm, vec=Vector((cx, cy, cz)), verts=res["verts"])
        for v in res["verts"]:
            for f in v.link_faces:
                f.material_index = 1
        bev = bmesh.ops.bevel(bm, geom=list(set(e for v in res["verts"] for e in v.link_edges)),
                              offset=0.22, segments=1, affect="EDGES")
        for f in bev.get("faces", []):
            if f.is_valid:
                f.material_index = 1

    # Inner dark skull cap under brim so looking up never reveals hollow shell
    cap_res = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=9.5)
    for v in cap_res["verts"]:
        if v.co.z < 0.0:
            v.co.z *= 0.25
        v.co.z += 1.5
        for f in v.link_faces:
            f.material_index = 0

    obj = create_mesh_object("SM_WizardHat", bm, [mat_cloth, mat_gold])
    set_smooth_shading(obj, True)
    export_obj(obj, "SM_WizardHat.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 2. SM_WizardCloak — Draped hooded battle-mage shoulder mantle & flared cloak
# ─────────────────────────────────────────────────────────────────────────────

def build_wizard_cloak(mat_cloth, mat_gold):
    """
    Pivot (0,0,0) at spine_03 (~128cm above ground).
    +Y = character forward, -Y = character back, +Z = up.
    Total height ~138cm: hem at Z=-112cm (just above boots), shoulder/hood cowl at Z=+26cm.
    Designed to wrap 300+ degrees around Manny (open V-slit at front chest/shins for leg freedom)
    so from 3rd-person over-shoulder camera the bald robot silhouette becomes a dramatic robed archmage!
    """
    bm = bmesh.new()

    def build_draped_shell(
        z_levels,
        arc_start_deg,
        arc_end_deg,
        n_arc,
        fold_count,
        fold_amp_fn,
        rx_fn,
        ry_fn,
        y_shift_fn,
        thickness,
        mat_body=0,
        gold_hem_band=True,
    ):
        outer_rings = []
        inner_rings = []
        n_z = len(z_levels)

        for zi, z in enumerate(z_levels):
            t = zi / max(1, n_z - 1)  # 0 = top, 1 = bottom hem
            rx = rx_fn(t)
            ry = ry_fn(t)
            y_shift = y_shift_fn(t)
            f_amp = fold_amp_fn(t)

            o_ring = []
            i_ring = []
            for ai in range(n_arc):
                u = ai / max(1, n_arc - 1)
                ang = math.radians(arc_start_deg + (arc_end_deg - arc_start_deg) * u)
                # Taper folds near open front edges so lapel borders hang cleanly
                edge_env = math.sin(u * math.pi) ** 0.55
                # Deep vertical sculpted drapery folds + secondary cloth ripples
                fold = f_amp * edge_env * (
                    0.72 * math.sin(u * fold_count * math.pi * 2.0 + t * 0.45)
                    + 0.28 * math.cos(u * (fold_count * 2.0 - 1.0) * math.pi * 2.0 - t * 0.6)
                )
                # Gentle windswept hem ripple at the very bottom
                hem_wave = -3.8 * (t ** 2.2) * edge_env * math.cos(u * fold_count * math.pi * 2.0)

                r_mod_x = rx + fold
                r_mod_y = ry + fold * 1.15
                cx = r_mod_x * math.cos(ang)
                cy = r_mod_y * math.sin(ang) + y_shift
                cz = z + hem_wave

                # Outward normal approximation for solid thickness shell
                nx = math.cos(ang)
                ny = math.sin(ang)
                o_ring.append(Vector((cx + nx * thickness * 0.5, cy + ny * thickness * 0.5, cz)))
                i_ring.append(Vector((cx - nx * thickness * 0.5, cy - ny * thickness * 0.5, cz)))

            outer_rings.append(o_ring)
            inner_rings.append(i_ring)

        uv_layer = bm.loops.layers.uv.verify()

        def loft_open(rings, flip=False, v_flip=False):
            bm_r = [[bm.verts.new(p) for p in ring] for ring in rings]
            for ri in range(len(bm_r) - 1):
                va = ri / max(1, len(bm_r) - 1)
                vb = (ri + 1) / max(1, len(bm_r) - 1)
                # Bottom 2 rows on outer shell get gold trim border!
                is_hem = gold_hem_band and (not flip) and (ri >= len(bm_r) - 2)
                for si in range(n_arc - 1):
                    is_lapel = gold_hem_band and (not flip) and (si == 0 or si == n_arc - 2)
                    ua = si / max(1, n_arc - 1)
                    ub = (si + 1) / max(1, n_arc - 1)
                    if not flip:
                        verts = [bm_r[ri][si], bm_r[ri][si + 1], bm_r[ri + 1][si + 1], bm_r[ri + 1][si]]
                    else:
                        verts = [bm_r[ri][si + 1], bm_r[ri][si], bm_r[ri + 1][si], bm_r[ri + 1][si + 1]]
                    try:
                        f = bm.faces.new(verts)
                        f.material_index = 1 if (is_hem or is_lapel) else mat_body
                        f.smooth = True
                        uvs = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                        for loop, uv in zip(f.loops, uvs):
                            loop[uv_layer].uv = uv
                    except ValueError:
                        pass
            return bm_r

        o_bm = loft_open(outer_rings, flip=False)
        i_bm = loft_open(inner_rings, flip=True)

        # Stitch bottom hem & front lapel edges cleanly so cloth has true solid volume
        for si in range(n_arc - 1):
            try:
                f = bm.faces.new([o_bm[-1][si], o_bm[-1][si + 1], i_bm[-1][si + 1], i_bm[-1][si]])
                f.material_index = 1 if gold_hem_band else mat_body
                f.smooth = True
            except ValueError:
                pass
        for ri in range(len(o_bm) - 1):
            for edge_i, rev in ((0, True), (n_arc - 1, False)):
                verts = (
                    [o_bm[ri][edge_i], o_bm[ri + 1][edge_i], i_bm[ri + 1][edge_i], i_bm[ri][edge_i]]
                    if rev else
                    [o_bm[ri + 1][edge_i], o_bm[ri][edge_i], i_bm[ri][edge_i], i_bm[ri + 1][edge_i]]
                )
                try:
                    f = bm.faces.new(verts)
                    f.material_index = 1 if gold_hem_band else mat_body
                    f.smooth = True
                except ValueError:
                    pass

    # --- A. Main Long Flowing Battle-Mage Cloak (Z = +20cm down to -112cm = 132cm tall) ---
    # Angles in XY: +90 deg is +Y (front center chest), -90 deg (270 deg) is -Y (center back).
    # Arc from 106 deg (front-left chest) around the back (270 deg) to 434 deg (74 deg, front-right chest):
    # That covers 328 degrees around Manny's torso, hips, thighs, and calves with a sleek open front V-split!
    cloak_z = [20.0 - 132.0 * (i / 18.0) for i in range(19)]
    build_draped_shell(
        z_levels=cloak_z,
        arc_start_deg=110.0,
        arc_end_deg=430.0,
        n_arc=44,
        fold_count=8,
        fold_amp_fn=lambda t: 1.0 + 5.8 * (t ** 1.35),
        rx_fn=lambda t: 22.0 + 6.0 * math.sin(min(1.0, t * 3.0) * math.pi * 0.5) + 21.0 * (t ** 1.25),
        ry_fn=lambda t: 15.5 + 3.5 * math.sin(min(1.0, t * 3.0) * math.pi * 0.5) + 25.0 * (t ** 1.35),
        y_shift_fn=lambda t: -1.5 - 15.0 * (t ** 1.45),  # Dramatic windswept trailing sweep toward -Y!
        thickness=1.8,
        mat_body=0,
        gold_hem_band=True,
    )

    # --- B. Layered Battle-Mage Shoulder Mantle / Capelet (Z = +24cm down to -26cm) ---
    # Broad sculpted shoulders that immediately erase Manny's narrow metallic mannequin shoulders!
    mantle_z = [24.0 - 50.0 * (i / 10.0) for i in range(11)]
    build_draped_shell(
        z_levels=mantle_z,
        arc_start_deg=100.0,
        arc_end_deg=440.0,
        n_arc=40,
        fold_count=10,
        fold_amp_fn=lambda t: 0.7 + 3.4 * (t ** 1.2),
        rx_fn=lambda t: 14.5 + 21.5 * math.sin(min(1.0, t * 1.45) * math.pi * 0.5) + 5.5 * t,
        ry_fn=lambda t: 13.0 + 13.0 * math.sin(min(1.0, t * 1.45) * math.pi * 0.5) + 8.5 * (t ** 1.2),
        y_shift_fn=lambda t: -1.2 - 5.5 * (t ** 1.25),
        thickness=2.0,
        mat_body=0,
        gold_hem_band=True,
    )

    # --- C. Sculpted Swept-Back Hood / Layered Arcane Cowl resting at collar (Z = +12 to +32cm) ---
    hood_rings = []
    n_h_rings = 9
    n_h_seg = 28
    for hi in range(n_h_rings):
        t = hi / (n_h_rings - 1)  # 0 = collar base, 1 = swept-back cowl fold tip
        rx = 15.5 * (1.0 - 0.55 * (t ** 1.5))
        ry = 13.5 * (1.0 - 0.45 * (t ** 1.4))
        zc = 22.0 + 9.0 * math.sin(t * math.pi * 0.78) - 11.0 * (t ** 2.0)
        yc = -3.0 - 20.0 * (t ** 1.35)
        ring = []
        for si in range(n_h_seg):
            u = si / (n_h_seg - 1)
            ang = math.radians(125.0 + 290.0 * u)
            ripple = 1.1 * math.sin(u * 5.0 * math.pi) * math.sin(t * math.pi)
            ring.append(Vector(((rx + ripple) * math.cos(ang), (ry + ripple) * math.sin(ang) + yc, zc + 1.5 * math.cos(ang * 2.0) * t)))
        hood_rings.append(ring)
    add_lofted_tube(bm, hood_rings, mat_index=0, closed_u=False, u_scale=1.5, v_range=(0.0, 1.0))

    # --- D. Ornate Gilded Mantle Clasps & Chest Chain (Mat_GoldTrim, slot 1) ---
    for side in (-1.0, 1.0):
        res = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=3.4)
        bmesh.ops.scale(bm, vec=Vector((1.05, 0.55, 1.25)), verts=res["verts"])
        bmesh.ops.translate(bm, vec=Vector((side * 12.5, 13.8, 17.5)), verts=res["verts"])
        for v in res["verts"]:
            for f in v.link_faces:
                f.material_index = 1

    # Draped gilded collar clasp chain connecting left & right brooches
    chain_rings = []
    for ci in range(13):
        u = ci / 12.0
        ang_x = -12.0 + 24.0 * u
        sag_z = 17.2 - 3.6 * math.sin(u * math.pi)
        fwd_y = 14.5 + 1.2 * math.sin(u * math.pi)
        ring = []
        for si in range(8):
            a = 2.0 * math.pi * si / 8
            ring.append(Vector((ang_x, fwd_y + 0.8 * math.cos(a), sag_z + 0.8 * math.sin(a))))
        chain_rings.append(ring)
    add_lofted_tube(bm, chain_rings, mat_index=1, closed_u=True, u_scale=1.0, v_range=(0.0, 1.0))

    obj = create_mesh_object("SM_WizardCloak", bm, [mat_cloth, mat_gold])
    set_smooth_shading(obj, True)
    export_obj(obj, "SM_WizardCloak.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 3. SM_WizardStaff — @BlendiByl gnarled curved oak forked staff + crystal
# ─────────────────────────────────────────────────────────────────────────────

def build_wizard_staff(mat_wood, mat_crystal):
    """
    Pivot (0,0,0) at hand grip!
    Butt at Z=-80cm, upper crescent fork cradling faceted crystal at Z=+106cm, fork tips Z=+126cm.
    Total height ~206cm.
    Slot 0 = Mat_Wood, Slot 1 = Mat_StaffCrystal.
    """
    bm = bmesh.new()

    def build_gnarled_branch(pts, radii, n_seg=14, twist_turns=1.4, ridge_amp=0.14, mat_idx=0, cap_start=True, cap_end=True):
        rings = []
        n_pts = len(pts)
        for pi, (pt, rad) in enumerate(zip(pts, radii)):
            t = pi / max(1, n_pts - 1)
            if pi == 0:
                tang = (pts[1] - pts[0]).normalized()
            elif pi == n_pts - 1:
                tang = (pts[-1] - pts[-2]).normalized()
            else:
                tang = (pts[pi + 1] - pts[pi - 1]).normalized()
            rot_q = Vector((0.0, 0.0, 1.0)).rotation_difference(tang)
            ring = []
            for si in range(n_seg):
                ang = 2.0 * math.pi * si / n_seg
                # Twisted oak bark flutes & organic gnarl
                flute = 1.0 + ridge_amp * math.sin(4.0 * ang + t * twist_turns * math.pi * 2.0)
                flute += 0.06 * math.cos(7.0 * ang - t * 5.0)
                local = Vector((rad * flute * math.cos(ang), rad * flute * math.sin(ang), 0.0))
                ring.append(pt + (rot_q @ local))
            rings.append(ring)
        add_lofted_tube(bm, rings, mat_index=mat_idx, closed_u=True, cap_start=cap_start, cap_end=cap_end, u_scale=1.0, v_range=(0.0, 3.0))

    # --- A. Main gnarled S-curved oak shaft (Z = -80cm -> +86cm) ---
    shaft_pts = []
    shaft_rads = []
    for i in range(28):
        t = i / 27.0
        z = -80.0 + 166.0 * t
        # Subtle organic S-bend (hand grip at Z=0 stays centered at X=0, Y=0!)
        grip_env = math.sin(min(1.0, abs(z) / 55.0) * math.pi * 0.5)
        x = grip_env * (3.2 * math.sin(t * math.pi * 1.85) + 1.4 * math.sin(t * math.pi * 3.7))
        y = grip_env * (2.4 * math.cos(t * math.pi * 1.45 - 0.6))
        # Taper from gnarled butt (2.4cm) -> sturdy grip (3.1cm) -> flared fork neck (4.2cm)
        rad = 2.4 + 0.9 * math.sin(t * math.pi * 0.7) + 1.5 * (t ** 2.6)
        # Subtle wood burls
        if abs(z - 38.0) < 9.0 or abs(z + 36.0) < 8.0:
            rad *= 1.16
        shaft_pts.append(Vector((x, y, z)))
        shaft_rads.append(rad)

    build_gnarled_branch(shaft_pts, shaft_rads, n_seg=16, twist_turns=1.8, ridge_amp=0.14, mat_idx=0)
    fork_base = shaft_pts[-1]

    # --- B. Crescent Fork Prong 1 (dominant sweeping left-outer crescent horn cradling crystal) ---
    prong1_pts = []
    prong1_rads = []
    for i in range(18):
        t = i / 17.0
        # Sweep outward to -X (-16cm) then curve dramatically inward above the crystal (+4cm, Z=+126cm)
        ang = -0.35 + t * 2.55  # radians along crescent arc
        x = fork_base.x - 16.5 * math.sin(t * math.pi * 0.88) + 4.5 * (t ** 2.2)
        y = fork_base.y + 3.2 * math.sin(t * math.pi * 1.3)
        z = fork_base.z + 41.0 * math.sin(t * math.pi * 0.58) + 4.0 * t
        rad = max(0.75, 3.1 * ((1.0 - t) ** 0.72))
        prong1_pts.append(Vector((x, y, z)))
        prong1_rads.append(rad)
    build_gnarled_branch(prong1_pts, prong1_rads, n_seg=12, twist_turns=1.2, ridge_amp=0.13, mat_idx=0)

    # --- C. Crescent Fork Prong 2 (counter-curving right-inner horn cradling crystal) ---
    prong2_pts = []
    prong2_rads = []
    for i in range(16):
        t = i / 15.0
        x = fork_base.x + 14.5 * math.sin(t * math.pi * 0.82) - 2.0 * (t ** 2.0)
        y = fork_base.y - 3.6 * math.sin(t * math.pi * 1.1)
        z = fork_base.z + 32.0 * math.sin(t * math.pi * 0.60) + 2.5 * t
        rad = max(0.7, 2.7 * ((1.0 - t) ** 0.75))
        prong2_pts.append(Vector((x, y, z)))
        prong2_rads.append(rad)
    build_gnarled_branch(prong2_pts, prong2_rads, n_seg=12, twist_turns=1.0, ridge_amp=0.13, mat_idx=0)

    # --- D. Small twisted spur twig jutting from fork collar for wild organic silhouette ---
    twig_pts = [
        fork_base + Vector((1.0, -1.5, -6.0)),
        fork_base + Vector((6.5, -5.5, 1.5)),
        fork_base + Vector((10.5, -7.8, 9.5)),
        fork_base + Vector((12.0, -7.2, 16.5)),
    ]
    build_gnarled_branch(twig_pts, [1.8, 1.3, 0.85, 0.45], n_seg=8, twist_turns=0.6, ridge_amp=0.1, mat_idx=0)

    # --- E. Faceted Floating Elemental Crystal nestled in crescent fork (Mat_StaffCrystal, slot 1) ---
    crystal_center = Vector((0.0, 0.0, 106.0))
    uv_layer = bm.loops.layers.uv.verify()

    def add_faceted_crystal(center: Vector, radius: float, half_h: float, rot_euler=(0, 0, 0), sides=6):
        rot_m = Euler(rot_euler, "XYZ").to_matrix()
        v_top = bm.verts.new(center + rot_m @ Vector((0.0, 0.0, half_h)))
        v_bot = bm.verts.new(center + rot_m @ Vector((0.0, 0.0, -half_h * 0.92)))

        ring_upper = []
        ring_lower = []
        for s in range(sides):
            a = 2.0 * math.pi * s / sides
            a2 = a + (math.pi / sides) * 0.35
            r_mod = radius * (1.0 + 0.14 * math.sin(2.0 * a + 0.4))
            ring_upper.append(bm.verts.new(center + rot_m @ Vector((r_mod * math.cos(a), r_mod * math.sin(a), half_h * 0.30))))
            ring_lower.append(bm.verts.new(center + rot_m @ Vector((r_mod * 0.92 * math.cos(a2), r_mod * 0.92 * math.sin(a2), -half_h * 0.28))))

        for s in range(sides):
            sn = (s + 1) % sides
            tri_faces = [
                [v_top, ring_upper[s], ring_upper[sn]],
                [ring_upper[s], ring_lower[s], ring_lower[sn], ring_upper[sn]],
                [ring_lower[s], v_bot, ring_lower[sn]],
            ]
            for fverts in tri_faces:
                try:
                    f = bm.faces.new(fverts)
                    f.material_index = 1
                    f.smooth = False  # Crisp sparkling gemstone facets!
                    for li, loop in enumerate(f.loops):
                        loop[uv_layer].uv = (s / sides, li / max(1, len(fverts)))
                except ValueError:
                    pass

    # Main hero faceted prism + 2 orbiting crystal shards
    add_faceted_crystal(crystal_center, radius=6.4, half_h=12.8, rot_euler=(0.08, -0.06, 0.3), sides=6)
    add_faceted_crystal(crystal_center + Vector((-8.2, 2.5, 5.5)), radius=2.1, half_h=5.2, rot_euler=(0.35, 0.4, 0.8), sides=5)
    add_faceted_crystal(crystal_center + Vector((7.6, -2.2, -3.5)), radius=1.9, half_h=4.6, rot_euler=(-0.3, -0.35, 1.2), sides=5)

    obj = create_mesh_object("SM_WizardStaff", bm, [mat_wood, mat_crystal])
    # Preserve smooth wood & crisp flat-faceted crystal normals!
    for p in obj.data.polygons:
        p.use_smooth = (p.material_index == 0)
    obj.data.update()
    export_obj(obj, "SM_WizardStaff.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 4. SM_RuneCircle — Crisp concentric geometric arcane spell ring (~110cm dia)
# ─────────────────────────────────────────────────────────────────────────────

def build_rune_circle(mat_rune):
    """
    Pivot (0,0,0) at center, lies in XY plane facing +Z.
    Outer diameter = 110cm (radius 55cm). Crisp 3D geometric mandala:
    outer double rings, 12 carved arcane rune glyphs, inscribed hexagram, inner sigil ring.
    """
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()

    def add_flat_ring(r_in: float, r_out: float, segs: int = 64, z_thick: float = 0.7):
        top_in, top_out = [], []
        bot_in, bot_out = [], []
        hz = z_thick * 0.5
        for i in range(segs):
            a = 2.0 * math.pi * i / segs
            ca, sa = math.cos(a), math.sin(a)
            top_in.append(bm.verts.new(Vector((r_in * ca, r_in * sa, hz))))
            top_out.append(bm.verts.new(Vector((r_out * ca, r_out * sa, hz))))
            bot_in.append(bm.verts.new(Vector((r_in * ca, r_in * sa, -hz))))
            bot_out.append(bm.verts.new(Vector((r_out * ca, r_out * sa, -hz))))
        for i in range(segs):
            inext = (i + 1) % segs
            u0, u1 = i / segs, (i + 1) / segs
            quads = [
                ([top_in[i], top_out[i], top_out[inext], top_in[inext]], [(u0, 0), (u0, 1), (u1, 1), (u1, 0)]),
                ([bot_out[i], bot_in[i], bot_in[inext], bot_out[inext]], [(u0, 1), (u0, 0), (u1, 0), (u1, 1)]),
                ([top_out[i], bot_out[i], bot_out[inext], top_out[inext]], [(u0, 1), (u0, 1), (u1, 1), (u1, 1)]),
                ([bot_in[i], top_in[i], top_in[inext], bot_in[inext]], [(u0, 0), (u0, 0), (u1, 0), (u1, 0)]),
            ]
            for verts, uvs in quads:
                try:
                    f = bm.faces.new(verts)
                    f.material_index = 0
                    for loop, uv in zip(f.loops, uvs):
                        loop[uv_layer].uv = uv
                except ValueError:
                    pass

    def add_bar(p0: Vector, p1: Vector, width: float = 1.6, z_thick: float = 0.75):
        d = p1 - p0
        length = d.length
        if length < 1e-3:
            return
        dn = d / length
        perp = Vector((-dn.y, dn.x, 0.0)) * (width * 0.5)
        hz = Vector((0.0, 0.0, z_thick * 0.5))
        pts = [
            p0 - perp + hz, p0 + perp + hz, p1 + perp + hz, p1 - perp + hz,
            p0 - perp - hz, p0 + perp - hz, p1 + perp - hz, p1 - perp - hz,
        ]
        vs = [bm.verts.new(p) for p in pts]
        faces_idx = [
            (0, 1, 2, 3), (7, 6, 5, 4),
            (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0),
        ]
        for fi in faces_idx:
            try:
                f = bm.faces.new([vs[k] for k in fi])
                f.material_index = 0
                for loop in f.loops:
                    c = loop.vert.co
                    loop[uv_layer].uv = (c.x / 110.0 + 0.5, c.y / 110.0 + 0.5)
            except ValueError:
                pass

    # Concentric crisp arcane rings (outer dia = 110cm)
    add_flat_ring(52.4, 55.0, segs=72, z_thick=0.85)
    add_flat_ring(48.8, 50.1, segs=72, z_thick=0.70)
    add_flat_ring(38.0, 40.0, segs=64, z_thick=0.80)
    add_flat_ring(19.5, 21.5, segs=48, z_thick=0.75)

    # Inscribed hexagram (two opposing equilateral triangles connecting r = 39cm)
    r_hex = 38.8
    for offset in (0.0, math.pi / 3.0):
        corners = [
            Vector((r_hex * math.cos(offset + k * 2.0 * math.pi / 3.0),
                    r_hex * math.sin(offset + k * 2.0 * math.pi / 3.0), 0.0))
            for k in range(3)
        ]
        for k in range(3):
            add_bar(corners[k], corners[(k + 1) % 3], width=1.65, z_thick=0.75)

    # 6 radial star spokes & diamond sigil nodes between inner & outer rings
    for k in range(6):
        a = k * math.pi / 3.0
        ca, sa = math.cos(a), math.sin(a)
        add_bar(Vector((20.5 * ca, 20.5 * sa, 0.0)), Vector((53.5 * ca, 53.5 * sa, 0.0)), width=1.2, z_thick=0.7)
        # Small glowing diamond at each hexagram vertex
        da = 0.075
        p_out = Vector((47.5 * ca, 47.5 * sa, 0.0))
        p_in = Vector((41.0 * ca, 41.0 * sa, 0.0))
        p_l = Vector((44.2 * math.cos(a - da), 44.2 * math.sin(a - da), 0.0))
        p_r = Vector((44.2 * math.cos(a + da), 44.2 * math.sin(a + da), 0.0))
        for q0, q1 in ((p_out, p_l), (p_l, p_in), (p_in, p_r), (p_r, p_out)):
            add_bar(q0, q1, width=1.1, z_thick=0.8)

    # 12 crisp geometric arcane rune glyphs inside the outer rune track (r = 40.5..48.2)
    for gi in range(12):
        a = (gi + 0.5) * (2.0 * math.pi / 12.0)
        rad_dir = Vector((math.cos(a), math.sin(a), 0.0))
        tan_dir = Vector((-math.sin(a), math.cos(a), 0.0))
        c = rad_dir * 44.4
        glyph_type = gi % 4
        if glyph_type == 0:
            # Arcane chevron + center staff
            add_bar(c - rad_dir * 3.0, c + rad_dir * 3.0, width=1.05)
            add_bar(c - tan_dir * 2.8 - rad_dir * 0.5, c + rad_dir * 2.8, width=1.0)
            add_bar(c + tan_dir * 2.8 - rad_dir * 0.5, c + rad_dir * 2.8, width=1.0)
        elif glyph_type == 1:
            # Eldritch Z-lightning rune
            add_bar(c - tan_dir * 2.5 + rad_dir * 2.6, c + tan_dir * 2.5 + rad_dir * 2.6, width=1.0)
            add_bar(c + tan_dir * 2.5 + rad_dir * 2.6, c - tan_dir * 2.5 - rad_dir * 2.6, width=1.0)
            add_bar(c - tan_dir * 2.5 - rad_dir * 2.6, c + tan_dir * 2.5 - rad_dir * 2.6, width=1.0)
        elif glyph_type == 2:
            # Twin hourglass / diamond sigil
            add_bar(c - tan_dir * 2.4 + rad_dir * 2.6, c + tan_dir * 2.4 - rad_dir * 2.6, width=1.0)
            add_bar(c + tan_dir * 2.4 + rad_dir * 2.6, c - tan_dir * 2.4 - rad_dir * 2.6, width=1.0)
            add_bar(c - tan_dir * 2.6, c + tan_dir * 2.6, width=0.95)
        else:
            # Forked trident rune
            add_bar(c - rad_dir * 3.0, c + rad_dir * 3.0, width=1.05)
            add_bar(c - tan_dir * 2.5 - rad_dir * 2.0, c - tan_dir * 2.5 + rad_dir * 2.4, width=0.95)
            add_bar(c + tan_dir * 2.5 - rad_dir * 2.0, c + tan_dir * 2.5 + rad_dir * 2.4, width=0.95)
            add_bar(c - tan_dir * 2.5, c + tan_dir * 2.5, width=0.95)

    obj = create_mesh_object("SM_RuneCircle", bm, [mat_rune])
    set_smooth_shading(obj, False)
    export_obj(obj, "SM_RuneCircle.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 5. SM_FX_FireTornado — Towering @BlendiByl ribbed multi-spiral Fire Tornado
# ─────────────────────────────────────────────────────────────────────────────

def build_fire_tornado(mat_fire):
    """
    Pivot (0,0,0) at ground base, +Z up.
    Height = 950cm, narrow base dia ~120cm (r=60cm) -> swirling flared crown dia ~520cm (r=260cm).
    8 helical sculpted flame ridges + 3 outer detached spiral wind/flame ribbons.
    Continuous monotonic upward spiral UVs (V = 0 at base -> 1 at crown) for blazing shader panner!
    """
    bm = bmesh.new()
    height = 950.0
    n_rings = 34
    n_segs = 64  # high azimuthal density for crisp 8-ridge helical ribbing
    twist_revs = 2.35  # 2.35 full spiral revolutions from base to crown

    # --- A. Main 8-ridge sculpted helical flame funnel ---
    funnel_rings = []
    for ri in range(n_rings):
        v = ri / (n_rings - 1)  # 0 at ground -> 1 at crown
        z = v * height

        # Organic S-curve tornado lean/sway so silhouette looks alive & cinematic!
        sway_x = 42.0 * math.sin(v * math.pi * 1.45) * (0.25 + 0.75 * v)
        sway_y = 32.0 * math.cos(v * math.pi * 1.15 - 0.5) * (v ** 1.15)

        # Classic funnel profile: tight base (60cm) -> graceful bell flare -> dramatic wide crown (260cm)
        base_r = 60.0 + 95.0 * (v ** 1.25) + 108.0 * (v ** 2.8)
        # Helical ridge amplitude grows strongly through mid-upper funnel
        ridge_amp = (12.0 + 34.0 * math.sin(v * math.pi * 0.88)) * (0.45 + 0.55 * v)

        ring = []
        for si in range(n_segs):
            u = si / n_segs
            ang = 2.0 * math.pi * u
            # 8 sculpted spiraling ridges coiling upward!
            helix_phase = 8.0 * (ang - v * twist_revs * 2.0 * math.pi)
            # Sharpened flame fin ridge shape (peaks sharper than valleys)
            ridge = ((0.5 + 0.5 * math.cos(helix_phase)) ** 1.65)
            # Secondary turbulent lick
            lick = 0.22 * math.sin(16.0 * (ang - v * twist_revs * 2.0 * math.pi) + v * 9.0)
            r = base_r + ridge_amp * (ridge + lick)
            ring.append(Vector((sway_x + r * math.cos(ang), sway_y + r * math.sin(ang), z)))
        funnel_rings.append(ring)

    # Custom spiral UV mapping: U follows helical angle so shader streaks track the sculpted ridges!
    uv_layer = bm.loops.layers.uv.verify()
    bm_rings = [[bm.verts.new(p) for p in ring] for ring in funnel_rings]
    for ri in range(n_rings - 1):
        va = ri / (n_rings - 1)
        vb = (ri + 1) / (n_rings - 1)
        r0 = bm_rings[ri]
        r1 = bm_rings[ri + 1]
        for si in range(n_segs):
            sn = (si + 1) % n_segs
            # Spiral UV shift matches the physical ridge twist!
            ua0 = 4.0 * (si / n_segs) - va * twist_revs * 2.0
            ub0 = 4.0 * ((si + 1) / n_segs) - va * twist_revs * 2.0
            ua1 = 4.0 * (si / n_segs) - vb * twist_revs * 2.0
            ub1 = 4.0 * ((si + 1) / n_segs) - vb * twist_revs * 2.0
            try:
                f = bm.faces.new([r0[si], r0[sn], r1[sn], r1[si]])
                f.material_index = 0
                f.smooth = True
                for loop, uv in zip(f.loops, [(ua0, va), (ub0, va), (ub1, vb), (ua1, vb)]):
                    loop[uv_layer].uv = uv
            except ValueError:
                pass

    # --- B. 3 Outer detached soaring helical flame/wind tendrils for volumetric parallax ---
    for ti in range(3):
        phase_off = ti * (2.0 * math.pi / 3.0)
        n_steps = 36
        ribbon_l = []
        ribbon_r = []
        for si in range(n_steps):
            v = si / (n_steps - 1)
            z = 25.0 + v * (height - 40.0)
            sway_x = 42.0 * math.sin(v * math.pi * 1.45) * (0.25 + 0.75 * v)
            sway_y = 32.0 * math.cos(v * math.pi * 1.15 - 0.5) * (v ** 1.15)
            r_outer = (88.0 + 118.0 * (v ** 1.2) + 125.0 * (v ** 2.6)) * (1.0 + 0.08 * math.sin(v * 7.0 + ti))
            ang = phase_off + v * (twist_revs + 0.4) * 2.0 * math.pi
            ribbon_w = (24.0 + 48.0 * math.sin(v * math.pi)) * (0.65 + 0.35 * v)
            cx = sway_x + r_outer * math.cos(ang)
            cy = sway_y + r_outer * math.sin(ang)
            # Tilt ribbon slightly outward like roaring vortex sheet
            rad_v = Vector((math.cos(ang), math.sin(ang), 0.0))
            ribbon_l.append(bm.verts.new(Vector((cx, cy, z - ribbon_w * 0.45)) - rad_v * (ribbon_w * 0.18)))
            ribbon_r.append(bm.verts.new(Vector((cx, cy, z + ribbon_w * 0.45)) + rad_v * (ribbon_w * 0.18)))

        for si in range(n_steps - 1):
            va = si / (n_steps - 1)
            vb = (si + 1) / (n_steps - 1)
            try:
                f = bm.faces.new([ribbon_l[si], ribbon_r[si], ribbon_r[si + 1], ribbon_l[si + 1]])
                f.material_index = 0
                f.smooth = True
                for loop, uv in zip(f.loops, [(0.0, va), (1.0, va), (1.0, vb), (0.0, vb)]):
                    loop[uv_layer].uv = uv
            except ValueError:
                pass

    obj = create_mesh_object("SM_FX_FireTornado", bm, [mat_fire])
    set_smooth_shading(obj, True)
    export_obj(obj, "SM_FX_FireTornado.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 6. SM_RuinArch — Sculpted beveled ancient mossy stone archway (~480x520cm)
# ─────────────────────────────────────────────────────────────────────────────

def build_ruin_arch(mat_stone):
    """
    Pivot (0,0,0) at bottom center; +Z up; width ~480cm (-240..+240), height ~520cm, depth ~104cm.
    Wide open gothic-fantasy central portal (~250cm wide x ~380cm tall) with stepped plinths,
    weathered ashlar masonry piers, carved capitals, 11 beveled voussoir arch stones,
    prominent sculpted keystone, and broken weathered cornice.
    """
    bm = bmesh.new()
    rng = random.Random(42)

    def add_chipped_block(center: Vector, size: Vector, rot_euler=(0.0, 0.0, 0.0), bevel_amt=4.0, chip_amp=2.6):
        res = bmesh.ops.create_cube(bm, size=1.0)
        verts = res["verts"]
        bmesh.ops.scale(bm, vec=size, verts=verts)
        edges = list(set(e for v in verts for e in v.link_edges))
        # Bevel edges for crisp ancient architectural chamfers
        bev = bmesh.ops.bevel(bm, geom=edges, offset=bevel_amt, segments=2, affect="EDGES")
        all_v = set(v for v in verts if v.is_valid)
        for v in bev.get("verts", []):
            if v.is_valid:
                all_v.add(v)
        for f in bev.get("faces", []):
            if f.is_valid:
                for fv in f.verts:
                    all_v.add(fv)
        rot_m = Euler(rot_euler, "XYZ").to_matrix()
        for v in all_v:
            p = rot_m @ v.co + center
            # Subtle deterministic stone erosion noise (keep ground contact Z<=2 flat!)
            if p.z > 6.0 and chip_amp > 0.0:
                nv = noise.noise_vector(p * 0.022 + Vector((1.7, 3.1, 0.9)))
                p += nv * chip_amp
            v.co = p
            for f in v.link_faces:
                f.material_index = 0

    # --- A. Twin Stepped Plinth Bases (left at X=-180, right at X=+180) ---
    for side in (-1.0, 1.0):
        px = side * 182.0
        # Bottom wide foundation plinth (Z = -10 to 26cm, embedded slightly to avoid floating on slopes)
        add_chipped_block(Vector((px, 0.0, 8.0)), Vector((126.0, 116.0, 36.0)), bevel_amt=5.0, chip_amp=2.2)
        # Upper stepped plinth collar (Z = 26 to 56cm)
        add_chipped_block(Vector((px, 0.0, 41.0)), Vector((110.0, 102.0, 30.0)), bevel_amt=4.5, chip_amp=2.0)

        # --- B. Ashlar Masonry Pier Courses (5 distinct weathered stone courses, Z = 56..276cm) ---
        course_heights = [46.0, 42.0, 46.0, 44.0, 42.0]
        cur_z = 56.0
        for ci, ch in enumerate(course_heights):
            # Subtle organic stagger & weathering variance per block
            w_var = 94.0 + rng.uniform(-3.0, 3.0)
            d_var = 86.0 + rng.uniform(-2.5, 2.5)
            x_jit = rng.uniform(-1.8, 1.8)
            y_jit = rng.uniform(-1.5, 1.5)
            add_chipped_block(
                Vector((px + x_jit, y_jit, cur_z + ch * 0.5)),
                Vector((w_var, d_var, ch - 1.5)),
                rot_euler=(0.0, 0.0, rng.uniform(-0.018, 0.018)),
                bevel_amt=4.2,
                chip_amp=2.8,
            )
            cur_z += ch

        # --- C. Carved Flared Impost Capital (Z = 276..306cm) ---
        add_chipped_block(Vector((px, 0.0, 291.0)), Vector((116.0, 104.0, 30.0)), bevel_amt=4.5, chip_amp=2.2)
        # Outer decorative weathered buttress shoulder block on each pier
        add_chipped_block(Vector((px + side * 46.0, 0.0, 130.0)), Vector((38.0, 68.0, 195.0)), bevel_amt=4.5, chip_amp=3.2)

    # --- D. 11 Sculpted Beveled Voussoir Arch Stones + Prominent Keystone ---
    # Semicircular arch centered at (0, 0, 295cm), center-line radius R = 182cm
    # Inner portal radius ~136cm (272cm clear width!), top of inner portal ~431cm!
    arch_center = Vector((0.0, 0.0, 295.0))
    arch_r = 182.0
    n_voussoirs = 11
    for vi in range(n_voussoirs):
        frac = vi / (n_voussoirs - 1)  # 0 (left springer) -> 0.5 (keystone) -> 1 (right springer)
        ang = math.pi * (1.0 - frac)  # pi -> 0
        is_keystone = (vi == n_voussoirs // 2)

        r_cur = arch_r + (9.0 if is_keystone else 0.0)
        cx = r_cur * math.cos(ang)
        cz = arch_center.z + r_cur * math.sin(ang)

        # Tangent & radial dimensions for wedge block
        radial_h = 84.0 if is_keystone else (64.0 + rng.uniform(-2.5, 2.5))
        tang_w = 56.0 if is_keystone else 52.0
        depth = 104.0 if is_keystone else (88.0 + rng.uniform(-2.0, 2.0))

        # Rotate around Y axis so block's Z aligns radially outward along arch!
        # In Blender, rot_y rotates +Z toward +X: angle = (pi/2 - ang)
        rot_y = (math.pi * 0.5) - ang
        add_chipped_block(
            Vector((cx, 0.0, cz)),
            Vector((tang_w, depth, radial_h)),
            rot_euler=(0.0, rot_y, 0.0),
            bevel_amt=4.5 if is_keystone else 3.8,
            chip_amp=2.6,
        )

    # --- E. Weathered Broken Spandrel & Cornice Ruin Crown (asymmetric ancient decay, Z up to 522cm) ---
    spandrel_blocks = [
        # Left upper spandrel & weathered cornice (taller intact side)
        (Vector((-186.0, 0.0, 355.0)), Vector((96.0, 84.0, 95.0)), (0.0, 0.0, 0.01)),
        (Vector((-162.0, 0.0, 442.0)), Vector((120.0, 86.0, 76.0)), (0.0, 0.0, -0.01)),
        (Vector((-140.0, 0.0, 496.0)), Vector((155.0, 98.0, 42.0)), (0.0, 0.02, 0.0)),
        # Right upper spandrel (slightly more eroded/broken for authentic ancient silhouette!)
        (Vector((186.0, 0.0, 350.0)), Vector((94.0, 84.0, 88.0)), (0.0, 0.0, -0.015)),
        (Vector((158.0, 0.0, 428.0)), Vector((106.0, 82.0, 62.0)), (0.0, -0.03, 0.01)),
        (Vector((118.0, 0.0, 476.0)), Vector((92.0, 88.0, 38.0)), (0.0, -0.04, 0.0)),
    ]
    for pos, sz, rot in spandrel_blocks:
        add_chipped_block(pos, sz, rot_euler=rot, bevel_amt=4.2, chip_amp=3.4)

    set_box_uvs(bm, scale_cm=220.0)
    obj = create_mesh_object("SM_RuinArch", bm, [mat_stone])
    set_smooth_shading(obj, False)  # Crisp beveled ashlar masonry planes!
    export_obj(obj, "SM_RuinArch.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 7. SM_FX_Pillar — Grounded jagged basalt/moss earth-eruption spire (Z=-40..400)
# ─────────────────────────────────────────────────────────────────────────────

def build_fx_pillar(mat_basalt):
    """
    Fixes floating Earth pillar bug! Base anchored deep at Z = -40cm up to Z = +400cm.
    Clustered hexagonal volcanic basalt spires with sharp faceted ridges & strata steps.
    """
    bm = bmesh.new()
    rng = random.Random(108)

    # Central hero spire + 4 clustered flanking jagged basalt shards
    spires = [
        # (ox, oy, z_bot, z_top, r_bot, r_top, sides, tilt_x, tilt_y)
        (  0.0,   0.0, -40.0, 400.0, 52.0, 12.0, 7,  0.04, -0.03),
        (-30.0,  18.0, -40.0, 295.0, 34.0,  7.0, 6, -0.10,  0.07),
        ( 32.0, -14.0, -40.0, 265.0, 32.0,  6.5, 6,  0.11, -0.06),
        ( 12.0,  34.0, -40.0, 210.0, 28.0,  6.0, 5,  0.04,  0.12),
        (-18.0, -32.0, -40.0, 185.0, 26.0,  5.5, 5, -0.08, -0.11),
    ]

    for ox, oy, z_bot, z_top, r_bot, r_top, sides, tx, ty in spires:
        n_lvl = 9
        rings = []
        for li in range(n_lvl):
            t = li / (n_lvl - 1)
            z = z_bot + (z_top - z_bot) * t
            # Stepped rocky strata bulge
            strata = 1.0 + 0.12 * math.sin(t * math.pi * 4.5) * (1.0 - t)
            r = (r_bot * (1.0 - t) + r_top * t) * strata
            cx = ox + tx * (z - z_bot)
            cy = oy + ty * (z - z_bot)
            ring = []
            for s in range(sides):
                a = 2.0 * math.pi * s / sides + li * 0.08
                rj = r * rng.uniform(0.84, 1.16)
                ring.append(Vector((cx + rj * math.cos(a), cy + rj * math.sin(a), z + rng.uniform(-6.0, 6.0) * t)))
            rings.append(ring)
        # Jagged oblique chisel peak at top
        peak_z = z_top + rng.uniform(16.0, 32.0)
        peak_ring = [Vector((ox + tx * (z_top - z_bot) + rng.uniform(-5, 5),
                             oy + ty * (z_top - z_bot) + rng.uniform(-5, 5), peak_z)) for _ in range(sides)]
        rings.append(peak_ring)
        add_lofted_tube(bm, rings, mat_index=0, closed_u=True, cap_start=True, cap_end=False, u_scale=1.5, v_range=(0.0, 2.5))

    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1.0)
    set_box_uvs(bm, scale_cm=140.0)
    obj = create_mesh_object("SM_FX_Pillar", bm, [mat_basalt])
    set_smooth_shading(obj, False)  # Crisp volcanic basalt rock facets!
    export_obj(obj, "SM_FX_Pillar.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 8. SM_GrassBlade — Lush multi-blade arching meadow grass tuft
# ─────────────────────────────────────────────────────────────────────────────

def build_grass_tuft(mat_grass):
    """
    Replaces sparse single-blade toothpick with a lush 13-blade arching meadow grass tuft
    (radius ~28cm, height 34..56cm, Z=0 at root, V=0 root -> V=1 golden tip).
    """
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    rng = random.Random(303)

    n_blades = 13
    for bi in range(n_blades):
        ang = (2.0 * math.pi * bi / n_blades) + rng.uniform(-0.22, 0.22)
        dist = rng.uniform(2.0, 15.0)
        bx = dist * math.cos(ang)
        by = dist * math.sin(ang)
        h = rng.uniform(34.0, 58.0)
        bend = rng.uniform(10.0, 24.0)
        w0 = rng.uniform(3.4, 5.2)

        dir_out = Vector((math.cos(ang), math.sin(ang), 0.0))
        dir_side = Vector((-math.sin(ang), math.cos(ang), 0.0))

        # 4 height segments -> smooth arching tapered grass blade
        levels = [0.0, 0.35, 0.72, 1.0]
        left_v, right_v = [], []
        tip_v = None
        for lv in levels:
            w = w0 * (1.0 - lv * 0.88)
            p = Vector((bx, by, -2.0)) + dir_out * (bend * (lv ** 1.65)) + Vector((0.0, 0.0, h * lv))
            if lv >= 0.99:
                tip_v = bm.verts.new(p)
            else:
                left_v.append(bm.verts.new(p - dir_side * (w * 0.5)))
                right_v.append(bm.verts.new(p + dir_side * (w * 0.5)))

        for si in range(len(left_v) - 1):
            va, vb = levels[si], levels[si + 1]
            f = bm.faces.new([left_v[si], right_v[si], right_v[si + 1], left_v[si + 1]])
            f.material_index = 0
            f.smooth = True
            for loop, uv in zip(f.loops, [(0.0, va), (1.0, va), (1.0, vb), (0.0, vb)]):
                loop[uv_layer].uv = uv
        ft = bm.faces.new([left_v[-1], right_v[-1], tip_v])
        ft.material_index = 0
        ft.smooth = True
        for loop, uv in zip(ft.loops, [(0.0, 0.72), (1.0, 0.72), (0.5, 1.0)]):
            loop[uv_layer].uv = uv

    obj = create_mesh_object("SM_GrassBlade", bm, [mat_grass])
    set_smooth_shading(obj, True)
    export_obj(obj, "SM_GrassBlade.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# 9. SM_Flower — Colorful multi-stem stylized meadow wildflower cluster
# ─────────────────────────────────────────────────────────────────────────────

def build_wildflower_cluster(mat_stem, mat_petal):
    """
    Slot 0 = Mat_Stem (emerald stems & basal leaves)
    Slot 1 = Mat_Petal (vivid customizable wildflower blossom petals — overridden per ISM layer in UE!)
    """
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    rng = random.Random(505)

    n_stems = 7
    for si in range(n_stems):
        ang = (2.0 * math.pi * si / n_stems) + rng.uniform(-0.22, 0.22)
        r_base = rng.uniform(2.0, 14.0)
        h = rng.uniform(30.0, 52.0)
        lean = rng.uniform(7.0, 17.0)
        dir_out = Vector((math.cos(ang), math.sin(ang), 0.0))
        dir_side = Vector((-math.sin(ang), math.cos(ang), 0.0))

        # Slender stem ribbon (Mat_Stem, slot 0)
        base_p = Vector((r_base * math.cos(ang), r_base * math.sin(ang), -2.0))
        mid_p = base_p + dir_out * (lean * 0.45) + Vector((0.0, 0.0, h * 0.55))
        top_p = base_p + dir_out * lean + Vector((0.0, 0.0, h))
        sw = 1.15
        v0l, v0r = bm.verts.new(base_p - dir_side * sw), bm.verts.new(base_p + dir_side * sw)
        v1l, v1r = bm.verts.new(mid_p - dir_side * (sw * 0.8)), bm.verts.new(mid_p + dir_side * (sw * 0.8))
        v2l, v2r = bm.verts.new(top_p - dir_side * (sw * 0.6)), bm.verts.new(top_p + dir_side * (sw * 0.6))
        for quad in ([v0l, v0r, v1r, v1l], [v1l, v1r, v2r, v2l]):
            f = bm.faces.new(quad)
            f.material_index = 0
            for loop in f.loops:
                loop[uv_layer].uv = (0.5, 0.5)

        # Basal leaves (Mat_Stem, slot 0)
        for lside in (-1.0, 1.0):
            la = ang + lside * 1.1
            ld = Vector((math.cos(la), math.sin(la), 0.0))
            ls = Vector((-math.sin(la), math.cos(la), 0.0))
            lp0 = base_p + Vector((0.0, 0.0, 4.0))
            lp1 = lp0 + ld * 8.0 + Vector((0.0, 0.0, 7.0))
            lp2 = lp0 + ld * 15.0 + Vector((0.0, 0.0, 4.0))
            vl0 = bm.verts.new(lp0)
            vl1a = bm.verts.new(lp1 - ls * 3.2)
            vl1b = bm.verts.new(lp1 + ls * 3.2)
            vl2 = bm.verts.new(lp2)
            for tri in ([vl0, vl1a, vl1b], [vl1a, vl2, vl1b]):
                f = bm.faces.new(tri)
                f.material_index = 0
                for loop in f.loops:
                    loop[uv_layer].uv = (0.5, 0.5)

        # Open 6-petal stylized meadow blossom tilted slightly outward (Mat_Petal, slot 1)
        petal_len = rng.uniform(9.0, 13.0)
        petal_w = petal_len * 0.48
        tilt_q = Vector((0.0, 0.0, 1.0)).rotation_difference((Vector((0.0, 0.0, 1.0)) + dir_out * 0.30).normalized())
        vc = bm.verts.new(top_p + tilt_q @ Vector((0.0, 0.0, 0.4)))
        for pi in range(6):
            pa = 2.0 * math.pi * pi / 6.0 + rng.uniform(-0.1, 0.1)
            pd = Vector((math.cos(pa), math.sin(pa), 0.0))
            ps = Vector((-math.sin(pa), math.cos(pa), 0.0))
            p_mid_l = top_p + tilt_q @ (pd * (petal_len * 0.55) - ps * petal_w + Vector((0.0, 0.0, 2.2)))
            p_mid_r = top_p + tilt_q @ (pd * (petal_len * 0.55) + ps * petal_w + Vector((0.0, 0.0, 2.2)))
            p_tip = top_p + tilt_q @ (pd * petal_len + Vector((0.0, 0.0, 1.8)))
            vml = bm.verts.new(p_mid_l)
            vmr = bm.verts.new(p_mid_r)
            vt = bm.verts.new(p_tip)
            f = bm.faces.new([vc, vml, vt, vmr])
            f.material_index = 1
            f.smooth = True
            for loop, uv in zip(f.loops, [(0.5, 0.0), (0.0, 0.5), (0.5, 1.0), (1.0, 0.5)]):
                loop[uv_layer].uv = uv

    obj = create_mesh_object("SM_Flower", bm, [mat_stem, mat_petal])
    set_smooth_shading(obj, True)
    export_obj(obj, "SM_Flower.obj")
    return obj


# ─────────────────────────────────────────────────────────────────────────────
# Studio Preview Contact Sheet (Cycles CPU — ZERO GPU Contention!)
# ─────────────────────────────────────────────────────────────────────────────

def render_preview_sheet(objects_info: list[tuple[str, bpy.types.Object]]):
    """Arrange normalized copies of all 9 sculpted assets in a 3x3 studio grid & render via Cycles CPU."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW_PNG)

    # Dark moody warm-rim studio world
    world = bpy.data.worlds.new("StudioWorld")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.045, 0.055, 0.085, 1.0)
        bg.inputs["Strength"].default_value = 0.85

    # Arrange all 9 assets cleanly in a 3 columns x 3 rows grid on XZ plane facing -Y camera
    cols, rows = 3, 3
    cell_x = 340.0
    cell_z = 215.0

    for idx, (label, obj) in enumerate(objects_info):
        c = idx % cols
        r = idx // cols
        # Compute world bbox
        coords = [Vector(co) for co in obj.bound_box]
        min_v = Vector((min(p.x for p in coords), min(p.y for p in coords), min(p.z for p in coords)))
        max_v = Vector((max(p.x for p in coords), max(p.y for p in coords), max(p.z for p in coords)))
        size = max_v - min_v
        max_dim = max(size.x, size.y, size.z, 1.0)
        target_dim = 160.0
        scale = target_dim / max_dim
        center = (min_v + max_v) * 0.5

        grid_x = (c - 1) * cell_x
        grid_z = (1 - r) * cell_z

        obj.scale = (scale, scale, scale)
        # Tilt rune circle toward camera so concentric geometry reads clearly on contact sheet!
        if label == "SM_RuneCircle":
            obj.rotation_euler = Euler((math.radians(65.0), 0.0, math.radians(15.0)), "XYZ")
        elif label == "SM_WizardCloak":
            obj.rotation_euler = Euler((0.0, 0.0, math.radians(25.0)), "XYZ")  # 3/4 hero back-side showcase
        elif label == "SM_WizardHat":
            obj.rotation_euler = Euler((0.0, 0.0, math.radians(-145.0)), "XYZ")  # front-3/4 buckle + bent tip showcase
        elif label == "SM_WizardStaff":
            obj.rotation_euler = Euler((0.0, math.radians(-22.0), math.radians(-12.0)), "XYZ")
        elif label == "SM_RuinArch":
            obj.rotation_euler = Euler((0.0, 0.0, math.radians(18.0)), "XYZ")
        else:
            obj.rotation_euler = Euler((0.0, 0.0, math.radians(20.0)), "XYZ")

        obj.location = Vector((grid_x, 0.0, grid_z)) - (obj.rotation_euler.to_matrix() @ (center * scale))

    # Orthographic camera framing entire 3x3 showcase cleanly with zero clipping
    cam_data = bpy.data.cameras.new("PreviewCam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 1240.0
    cam_data.clip_start = 10.0
    cam_data.clip_end = 5000.0
    cam = bpy.data.objects.new("PreviewCam", cam_data)
    bpy.context.collection.objects.link(cam)
    cam.location = Vector((0.0, -900.0, 0.0))
    cam.rotation_euler = Euler((math.radians(90.0), 0.0, 0.0), "XYZ")
    scene.camera = cam

    # Warm golden-hour key light + cool sky fill + crisp rim light
    def add_sun(name, rot_deg, energy, color):
        ld = bpy.data.lights.new(name, type="SUN")
        ld.energy = energy
        ld.color = color
        lo = bpy.data.objects.new(name, ld)
        bpy.context.collection.objects.link(lo)
        lo.rotation_euler = Euler([math.radians(a) for a in rot_deg], "XYZ")

    add_sun("KeyGold", (52, 0, -38), 3.6, (1.0, 0.86, 0.66))
    add_sun("FillBlue", (35, 0, 145), 1.3, (0.62, 0.78, 1.0))
    add_sun("RimWarm", (110, 0, 180), 2.2, (1.0, 0.72, 0.42))

    bpy.ops.render.render(write_still=True)
    print(f"[Sculpt] Rendered CPU studio contact sheet -> {PREVIEW_PNG}")


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main():
    clear_scene()

    mat_cloth   = make_mat("Mat_WizardCloth",  (0.055, 0.065, 0.145), roughness=0.82, metallic=0.04)
    mat_gold    = make_mat("Mat_GoldTrim",     (0.82, 0.62, 0.22),    roughness=0.28, metallic=0.88)
    mat_wood    = make_mat("Mat_Wood",         (0.24, 0.14, 0.075),   roughness=0.78, metallic=0.0)
    mat_crystal = make_mat("Mat_StaffCrystal", (0.20, 0.82, 1.00),    roughness=0.08, metallic=0.15,
                           emission=(0.25, 0.85, 1.0), emission_strength=6.0)
    mat_rune    = make_mat("Mat_RuneGlow",     (0.15, 0.95, 0.55),    roughness=0.2,  metallic=0.0,
                           emission=(0.15, 1.0, 0.62), emission_strength=7.5)
    mat_fire    = make_mat("Mat_FireTornado",  (1.00, 0.36, 0.05),    roughness=0.35, metallic=0.0,
                           emission=(1.0, 0.42, 0.06), emission_strength=4.8)
    mat_stone   = make_mat("Mat_RuinStone",    (0.42, 0.41, 0.36),    roughness=0.88, metallic=0.0)
    mat_basalt  = make_mat("Mat_BasaltRock",   (0.20, 0.21, 0.19),    roughness=0.90, metallic=0.0)
    mat_grass   = make_mat("Mat_Grass",        (0.16, 0.46, 0.12),    roughness=0.68, metallic=0.0)
    mat_stem    = make_mat("Mat_Stem",         (0.14, 0.42, 0.11),    roughness=0.72, metallic=0.0)
    mat_petal   = make_mat("Mat_Petal",        (0.95, 0.72, 0.12),    roughness=0.52, metallic=0.0)

    built = [
        ("SM_WizardHat",      build_wizard_hat(mat_cloth, mat_gold)),
        ("SM_WizardCloak",    build_wizard_cloak(mat_cloth, mat_gold)),
        ("SM_WizardStaff",    build_wizard_staff(mat_wood, mat_crystal)),
        ("SM_RuneCircle",     build_rune_circle(mat_rune)),
        ("SM_FX_FireTornado", build_fire_tornado(mat_fire)),
        ("SM_RuinArch",       build_ruin_arch(mat_stone)),
        ("SM_FX_Pillar",      build_fx_pillar(mat_basalt)),
        ("SM_GrassBlade",     build_grass_tuft(mat_grass)),
        ("SM_Flower",         build_wildflower_cluster(mat_stem, mat_petal)),
    ]

    render_preview_sheet(built)
    print("[Sculpt] ALL 9 MESHES EXPORTED & PREVIEW RENDERED SUCCESSFULLY.")


if __name__ == "__main__":
    main()
