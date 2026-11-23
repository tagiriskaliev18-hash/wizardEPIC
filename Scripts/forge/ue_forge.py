#!/usr/bin/env python3
"""
ue_forge.py – Unreal Editor Python script for wizardEPIC.
Runs inside UnrealEditor-Cmd.exe via -ExecutePythonScript.

Reads Generated/ assets produced by offline_forge.py + layout.json,
imports them into /Game/Wizard/, creates materials, DataAssets,
assembles the arena level L_Arena.

Usage (via Run_Forge.bat):
    UnrealEditor-Cmd.exe wizardEPIC.uproject -ExecutePythonScript=ue_forge.py -unattended
"""

from __future__ import annotations
import json, os, sys, math
from pathlib import Path

# ── Locate generated data ────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).resolve().parent
GENERATED  = SCRIPT_DIR / "Generated"
LAYOUT_JSON = GENERATED / "layout.json"

# ── Import contract ──────────────────────────────────────────────────────────
sys.path.insert(0, str(SCRIPT_DIR))
import contract as C

# ── Unreal Python API (only available inside the editor) ──────────────────────
try:
    import unreal
    IN_EDITOR = True
except ImportError:
    IN_EDITOR = False
    print("[ue_forge] Not running inside UnrealEditor – syntax-check only.")

# ═══════════════════════════════════════════════════════════════════════════════
# Helpers
# ═══════════════════════════════════════════════════════════════════════════════

UE_WIZARD = "/Game/Wizard"

def _log(msg: str):
    if IN_EDITOR:
        unreal.log(f"[Forge] {msg}")
    else:
        print(f"[Forge] {msg}")

def _ensure_dir(ue_path: str):
    """Create a content-browser directory if it doesn't exist."""
    if IN_EDITOR:
        eal = unreal.EditorAssetLibrary
        if not eal.does_directory_exist(ue_path):
            eal.make_directory(ue_path)

# ═══════════════════════════════════════════════════════════════════════════════
# Asset import
# ═══════════════════════════════════════════════════════════════════════════════

def import_textures():
    """Import Generated/Textures/*.png into /Game/Wizard/... by category."""
    if not IN_EDITOR:
        return
    tex_dir = GENERATED / "Textures"
    tasks = []

    # Map file names to UE destination
    tex_routing = {}
    for name in C.FX_TEXTURES:
        tex_routing[name] = f"{UE_WIZARD}/FX/Textures"
    for name in ["T_DetailNoise", "T_Ground", "T_Rock", "T_Sandstone", "T_Wood", "T_Crystal"]:
        tex_routing[name] = f"{UE_WIZARD}/Environment/Textures"
    for elem in C.ELEMENTS:
        tex_routing[f"T_Icon_{elem}"] = f"{UE_WIZARD}/UI/Icons"

    for png_file in sorted(tex_dir.glob("*.png")):
        stem = png_file.stem
        dest = tex_routing.get(stem, f"{UE_WIZARD}/Environment/Textures")
        _ensure_dir(dest)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(png_file))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        _log(f"Imported {len(tasks)} textures")

def import_audio():
    """Import Generated/Audio/*.wav into /Game/Wizard/Audio/..."""
    if not IN_EDITOR:
        return
    audio_dir = GENERATED / "Audio"
    tasks = []

    for wav_file in sorted(audio_dir.glob("*.wav")):
        stem = wav_file.stem
        if "Ambient" in stem:
            dest = f"{UE_WIZARD}/Audio/Music"
        else:
            dest = f"{UE_WIZARD}/Audio/SFX"
        _ensure_dir(dest)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(wav_file))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        _log(f"Imported {len(tasks)} audio files")

def import_meshes():
    """Import Generated/Meshes/*.obj into /Game/Wizard/... by category."""
    if not IN_EDITOR:
        return
    mesh_dir = GENERATED / "Meshes"
    tasks = []

    for obj_file in sorted(mesh_dir.glob("*.obj")):
        stem = obj_file.stem
        if stem.startswith("SM_FX_"):
            dest = f"{UE_WIZARD}/FX/Meshes"
        else:
            dest = f"{UE_WIZARD}/Environment/Meshes"
        _ensure_dir(dest)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(obj_file))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        _log(f"Imported {len(tasks)} meshes")

def import_destruction_meshes():
    """Import Generated/Destruction/ piece OBJs into /Game/Wizard/Destruction/Meshes/."""
    if not IN_EDITOR:
        return
    dest_root = f"{UE_WIZARD}/Destruction/Meshes"
    _ensure_dir(dest_root)
    tasks = []

    dest_dir = GENERATED / "Destruction"
    for shape_dir in sorted(dest_dir.iterdir()):
        if not shape_dir.is_dir():
            continue
        for obj_file in sorted(shape_dir.glob("*.obj")):
            stem = obj_file.stem
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(obj_file))
            task.set_editor_property("destination_path", dest_root)
            task.set_editor_property("destination_name", stem)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        _log(f"Imported {len(tasks)} destruction meshes")

# ═══════════════════════════════════════════════════════════════════════════════
# Material creation & assignment
# ═══════════════════════════════════════════════════════════════════════════════

def _load_or_create_material(name: str, folder: str):
    at = unreal.AssetToolsHelpers.get_asset_tools()
    mf = unreal.MaterialFactoryNew()
    full = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        mat = unreal.EditorAssetLibrary.load_asset(full)
    else:
        mat = at.create_asset(name, folder, unreal.Material, mf)
    if mat:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    return mat


def _make_tex_param(mat, param_name: str, tex_path: str | None, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    expr.set_editor_property("parameter_name", param_name)
    if tex_path:
        tex = unreal.EditorAssetLibrary.load_asset(tex_path)
        if tex:
            expr.set_editor_property("texture", tex)
    return expr


def _make_vec_param(mat, param_name: str, rgba: tuple[float, float, float, float], x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    expr.set_editor_property("parameter_name", param_name)
    expr.set_editor_property("default_value", unreal.LinearColor(*rgba))
    return expr


def _make_scalar_param(mat, param_name: str, val: float, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    expr.set_editor_property("parameter_name", param_name)
    expr.set_editor_property("default_value", float(val))
    return expr


def _make_mul(mat, a_expr, a_out: str, b_expr, b_out: str, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a_expr, a_out, mul, "A")
    mel.connect_material_expressions(b_expr, b_out, mul, "B")
    return mul


def _make_const(mat, val: float, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", float(val))
    return c


def _finalize_material(mat):
    mel = unreal.MaterialEditingLibrary
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)


def _wire_per_instance_rgb_alpha(mat, base_rgb_expr, base_rgb_out: str, base_alpha_expr, base_alpha_out: str):
    """Multiply sprite RGB and Alpha by ISM PerInstanceCustomData (0..2 = RGB, 3 = Alpha) if available."""
    picd_cls = getattr(unreal, "MaterialExpressionPerInstanceCustomData", None)
    append_cls = getattr(unreal, "MaterialExpressionAppendVector", None)
    if not picd_cls or not append_cls:
        return (base_rgb_expr, base_rgb_out), (base_alpha_expr, base_alpha_out)

    mel = unreal.MaterialEditingLibrary
    d0 = mel.create_material_expression(mat, picd_cls, -600, 180)
    d1 = mel.create_material_expression(mat, picd_cls, -600, 260)
    d2 = mel.create_material_expression(mat, picd_cls, -600, 340)
    d3 = mel.create_material_expression(mat, picd_cls, -600, 420)
    for idx, d in enumerate((d0, d1, d2, d3)):
        d.set_editor_property("data_index", idx)
        d.set_editor_property("default_value", 1.0)

    rg = mel.create_material_expression(mat, append_cls, -440, 200)
    mel.connect_material_expressions(d0, "", rg, "A")
    mel.connect_material_expressions(d1, "", rg, "B")
    rgb = mel.create_material_expression(mat, append_cls, -300, 240)
    mel.connect_material_expressions(rg, "", rgb, "A")
    mel.connect_material_expressions(d2, "", rgb, "B")

    out_rgb = _make_mul(mat, base_rgb_expr, base_rgb_out, rgb, "", -160, 120)
    out_alpha = _make_mul(mat, base_alpha_expr, base_alpha_out, d3, "", -160, 340)
    return (out_rgb, ""), (out_alpha, "")


def create_materials():
    """Construct complete UMaterial shader graphs driven by C++ dynamic parameters."""
    if not IN_EDITOR:
        return
    mel = unreal.MaterialEditingLibrary

    # ── 1. Environment materials ──────────────────────────────────────────────
    env_mat_path = f"{UE_WIZARD}/Environment/Materials"
    _ensure_dir(env_mat_path)

    env_tex_root = f"{UE_WIZARD}/Environment/Textures"
    env_specs = {
        "M_Ground":        (f"{env_tex_root}/T_Ground",      (0.85, 0.80, 0.68, 1.0), 0.92, False, 0.0),
        "M_GrassBlade":    (f"{env_tex_root}/T_DetailNoise", (0.24, 0.48, 0.18, 1.0), 0.78, True,  0.0),
        "M_Foliage":       (f"{env_tex_root}/T_DetailNoise", (0.20, 0.44, 0.16, 1.0), 0.80, True,  0.0),
        "M_Bark":          (f"{env_tex_root}/T_Wood",        (0.45, 0.32, 0.22, 1.0), 0.90, False, 0.0),
        "M_Rock":          (f"{env_tex_root}/T_Rock",        (0.58, 0.56, 0.54, 1.0), 0.88, False, 0.0),
        "M_Sandstone":     (f"{env_tex_root}/T_Sandstone",   (0.88, 0.76, 0.58, 1.0), 0.85, False, 0.0),
        "M_RuinStone":     (f"{env_tex_root}/T_Rock",        (0.72, 0.70, 0.66, 1.0), 0.86, False, 0.0),
        "M_Wood":          (f"{env_tex_root}/T_Wood",        (0.76, 0.56, 0.36, 1.0), 0.82, False, 0.0),
        "M_Crystal":       (f"{env_tex_root}/T_Crystal",     (0.32, 0.90, 1.00, 1.0), 0.15, False, 2.5),
        "M_Piece_Stone":   (f"{env_tex_root}/T_Rock",        (0.68, 0.66, 0.62, 1.0), 0.88, False, 0.0),
        "M_Piece_Rock":    (f"{env_tex_root}/T_Rock",        (0.52, 0.50, 0.48, 1.0), 0.90, False, 0.0),
        "M_Piece_Wood":    (f"{env_tex_root}/T_Wood",        (0.72, 0.52, 0.32, 1.0), 0.84, False, 0.0),
        "M_Piece_Pottery": (f"{env_tex_root}/T_Sandstone",   (0.82, 0.46, 0.28, 1.0), 0.72, False, 0.0),
    }

    for mat_name in C.ENV_MATERIALS:
        mat = _load_or_create_material(mat_name, env_mat_path)
        if not mat:
            continue
        tex_path, tint_rgba, rough_val, two_sided, emissive_val = env_specs.get(
            mat_name, (f"{env_tex_root}/T_DetailNoise", (0.7, 0.7, 0.7, 1.0), 0.85, False, 0.0)
        )
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        mat.set_editor_property("two_sided", two_sided)

        tex = _make_tex_param(mat, "Tex", tex_path, -520, -120)
        tint = _make_vec_param(mat, "Tint", tint_rgba, -520, 80)
        base_col = _make_mul(mat, tex, "RGB", tint, "", -260, -20)
        mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

        rough = _make_scalar_param(mat, "Roughness", rough_val, -260, 140)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

        if emissive_val > 0.0:
            em_scale = _make_scalar_param(mat, "EmissiveScale", emissive_val, -260, 260)
            em_col = _make_mul(mat, base_col, "", em_scale, "", -80, 220)
            mel.connect_material_property(em_col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        _finalize_material(mat)
        _log(f"Built material {mat_name}")

    # ── 2. FX materials ───────────────────────────────────────────────────────
    fx_mat_path = f"{UE_WIZARD}/FX/Materials"
    fx_tex_root = f"{UE_WIZARD}/FX/Textures"
    _ensure_dir(fx_mat_path)

    # 2a. Sprite masters (Tex, Tint, Fade + PerInstanceCustomData)
    for mat_name in C.FX_SPRITE_MASTERS:
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue
        is_add = (mat_name == "M_FX_Add")
        mat.set_editor_property(
            "blend_mode",
            unreal.BlendMode.BLEND_ADDITIVE if is_add else unreal.BlendMode.BLEND_TRANSLUCENT,
        )
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)

        default_tex = f"{fx_tex_root}/{'T_FX_Glow' if is_add else 'T_FX_Smoke'}"
        tex = _make_tex_param(mat, "Tex", default_tex, -760, -160)
        tint = _make_vec_param(mat, "Tint", (1.0, 1.0, 1.0, 1.0), -760, 40)
        fade = _make_scalar_param(mat, "Fade", 1.0, -760, 220)

        rgb = _make_mul(mat, tex, "RGB", tint, "", -500, -80)
        alpha_tex_tint = _make_mul(mat, tex, "A", tint, "A", -500, 100)
        alpha = _make_mul(mat, alpha_tex_tint, "", fade, "", -360, 140)

        (rgb_expr, rgb_pin), (alpha_expr, alpha_pin) = _wire_per_instance_rgb_alpha(
            mat, rgb, "", alpha, ""
        )
        if is_add:
            emissive = _make_mul(mat, rgb_expr, rgb_pin, alpha_expr, alpha_pin, -40, 20)
            mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:
            mel.connect_material_property(rgb_expr, rgb_pin, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(alpha_expr, alpha_pin, unreal.MaterialProperty.MP_OPACITY)

        _finalize_material(mat)
        _log(f"Built FX sprite master {mat_name}")

    # 2b. FX mesh materials (Tint, EmissiveScale, Fade)
    for mat_name in C.FX_MESH_MATERIALS:
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue

        if mat_name == "M_FX_Core":
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            tint = _make_vec_param(mat, "Tint", (1.0, 0.5, 0.15, 1.0), -520, -80)
            em_scale = _make_scalar_param(mat, "EmissiveScale", 28.0, -520, 100)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 220)
            em = _make_mul(mat, tint, "", em_scale, "", -240, -20)
            op = _make_mul(mat, tint, "A", fade, "", -240, 180)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        elif mat_name in ("M_FX_Ring", "M_FX_Bolt"):
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("two_sided", True)
            default_em = 60.0 if mat_name == "M_FX_Bolt" else 14.0
            tint = _make_vec_param(mat, "Tint", (0.8, 0.9, 1.0, 1.0), -540, -80)
            em_scale = _make_scalar_param(mat, "EmissiveScale", default_em, -540, 100)
            fade = _make_scalar_param(mat, "Fade", 1.0, -540, 220)
            em_tint = _make_mul(mat, tint, "", em_scale, "", -300, 0)
            em = _make_mul(mat, em_tint, "", fade, "", -140, 60)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        elif mat_name == "M_FX_WindRibbon":
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("two_sided", True)
            tint = _make_vec_param(mat, "Tint", (0.72, 0.92, 0.88, 1.0), -520, -80)
            em_scale = _make_scalar_param(mat, "EmissiveScale", 2.5, -520, 80)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 200)
            em = _make_mul(mat, tint, "", em_scale, "", -260, -20)
            op_base = _make_mul(mat, tint, "A", fade, "", -260, 160)
            op = _make_mul(mat, op_base, "", _make_const(mat, 0.65, -260, 280), "", -100, 180)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        elif mat_name == "M_FX_Pillar":
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            tex = _make_tex_param(mat, "Tex", f"{env_tex_root}/T_Rock", -520, -120)
            tint = _make_vec_param(mat, "Tint", (0.75, 0.52, 0.23, 1.0), -520, 80)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 240)
            base_col = _make_mul(mat, tex, "RGB", tint, "", -240, -20)
            em_fade = _make_mul(mat, tint, "", fade, "", -240, 180)
            em = _make_mul(mat, em_fade, "", _make_const(mat, 0.25, -240, 300), "", -100, 200)
            mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(_make_const(mat, 0.85, -100, 340), "", unreal.MaterialProperty.MP_ROUGHNESS)
        else:  # M_FX_Crystal
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            tex = _make_tex_param(mat, "Tex", f"{env_tex_root}/T_Crystal", -520, -120)
            tint = _make_vec_param(mat, "Tint", (0.30, 0.90, 1.0, 1.0), -520, 80)
            em_scale = _make_scalar_param(mat, "EmissiveScale", 3.0, -520, 220)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 320)
            base_col = _make_mul(mat, tex, "RGB", tint, "", -260, -20)
            em_a = _make_mul(mat, base_col, "", em_scale, "", -120, 160)
            em = _make_mul(mat, em_a, "", fade, "", 20, 180)
            mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(_make_const(mat, 0.15, -100, 340), "", unreal.MaterialProperty.MP_ROUGHNESS)

        _finalize_material(mat)
        _log(f"Built FX mesh material {mat_name}")

    # 2c. FX debris chip materials
    chip_specs = {
        "Stone":   (f"{env_tex_root}/T_Rock",        (0.58, 0.56, 0.52, 1.0), 0.88, False),
        "Wood":    (f"{env_tex_root}/T_Wood",        (0.45, 0.30, 0.16, 1.0), 0.82, False),
        "Pottery": (f"{env_tex_root}/T_Sandstone",   (0.72, 0.42, 0.26, 1.0), 0.72, False),
        "Rock":    (f"{env_tex_root}/T_Rock",        (0.42, 0.40, 0.38, 1.0), 0.90, False),
        "Crystal": (f"{env_tex_root}/T_Crystal",     (0.28, 0.90, 1.00, 1.0), 0.15, False),
        "Foliage": (f"{env_tex_root}/T_DetailNoise", (0.24, 0.52, 0.22, 1.0), 0.80, True),
    }
    for mat_family in C.MATERIALS:
        mat_name = f"M_FX_Chip_{mat_family}"
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue
        tex_path, rgba, rough_val, two_sided = chip_specs[mat_family]
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        mat.set_editor_property("two_sided", two_sided)
        tex = _make_tex_param(mat, "Tex", tex_path, -480, -80)
        tint = _make_vec_param(mat, "Tint", rgba, -480, 100)
        base_col = _make_mul(mat, tex, "RGB", tint, "", -220, 0)
        mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(_make_scalar_param(mat, "Roughness", rough_val, -220, 140), "", unreal.MaterialProperty.MP_ROUGHNESS)
        if mat_family == "Crystal":
            mel.connect_material_property(_make_mul(mat, base_col, "", _make_const(mat, 2.0, -220, 260), "", -60, 220), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        _finalize_material(mat)
        _log(f"Built FX chip material {mat_name}")

    # 2d. Decal materials
    for mat_name in C.FX_DECALS:
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        if mat_name == "M_Decal_Scorch":
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Smoke", -480, -80)
            tint = _make_vec_param(mat, "Tint", (0.04, 0.03, 0.03, 0.9), -480, 100)
        elif mat_name == "M_Decal_Crack":
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Dust", -480, -80)
            tint = _make_vec_param(mat, "Tint", (0.12, 0.10, 0.08, 0.85), -480, 100)
        else:
            elem = mat_name.replace("M_Decal_Rune_", "")
            ec = C.ELEMENT_COLORS_F.get(elem, (1.0, 1.0, 1.0))
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Rune", -480, -80)
            tint = _make_vec_param(mat, "Tint", (ec[0], ec[1], ec[2], 1.0), -480, 100)

        fade = _make_scalar_param(mat, "Fade", 1.0, -480, 260)
        col = _make_mul(mat, tex, "RGB", tint, "", -220, -20)
        op_a = _make_mul(mat, tex, "A", tint, "A", -240, 140)
        op = _make_mul(mat, op_a, "", fade, "", -80, 160)
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        if mat_name.startswith("M_Decal_Rune_"):
            mel.connect_material_property(_make_mul(mat, col, "", _make_const(mat, 3.0, -220, -140), "", -60, -80), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        _finalize_material(mat)
        _log(f"Built decal material {mat_name}")

    # ── 3. UI materials ───────────────────────────────────────────────────────
    ui_mat_path = f"{UE_WIZARD}/UI/Materials"
    _ensure_dir(ui_mat_path)
    for mat_name in ["M_UI_Cooldown", "M_UI_Charge", "M_UI_Panel"]:
        mat = _load_or_create_material(mat_name, ui_mat_path)
        if not mat:
            continue
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

        if mat_name == "M_UI_Cooldown":
            tint = _make_vec_param(mat, "Tint", (0.02, 0.02, 0.05, 0.72), -480, -80)
            frac = _make_scalar_param(mat, "Fraction", 1.0, -480, 140)
            mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            custom_ok = False
            try:
                tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -480, 40)
                custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -220, 100)
                custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
                custom.set_editor_property(
                    "code",
                    "float2 p = UV - 0.5;\n"
                    "float a = frac(atan2(p.x, -p.y) / 6.2831853 + 1.0);\n"
                    "return (a <= Fraction) ? 0.72 : 0.0;",
                )
                inp_uv = unreal.CustomInput()
                inp_uv.set_editor_property("input_name", "UV")
                inp_frac = unreal.CustomInput()
                inp_frac.set_editor_property("input_name", "Fraction")
                custom.set_editor_property("inputs", [inp_uv, inp_frac])
                mel.connect_material_expressions(tc, "", custom, "UV")
                mel.connect_material_expressions(frac, "", custom, "Fraction")
                mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_OPACITY)
                custom_ok = True
            except Exception as exc:
                _log(f"  note: M_UI_Cooldown custom radial fallback ({exc})")
            if not custom_ok:
                op = _make_mul(mat, frac, "", _make_const(mat, 0.72, -320, 220), "", -140, 160)
                mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        elif mat_name == "M_UI_Charge":
            tint = _make_vec_param(mat, "Tint", (1.0, 0.65, 0.2, 1.0), -480, -40)
            frac = _make_scalar_param(mat, "Fraction", 1.0, -480, 120)
            em = _make_mul(mat, tint, "", _make_scalar_param(mat, "EmissiveScale", 2.0, -480, 220), "", -220, -20)
            op = _make_mul(mat, tint, "A", frac, "", -220, 120)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        else:  # M_UI_Panel
            tint = _make_vec_param(mat, "Tint", (0.035, 0.035, 0.06, 0.78), -480, -40)
            fade = _make_scalar_param(mat, "Fade", 1.0, -480, 120)
            op = _make_mul(mat, tint, "A", fade, "", -220, 80)
            mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

        _finalize_material(mat)
        _log(f"Built UI material {mat_name}")


def assign_mesh_materials():
    """Assign ENV_MATERIALS to imported environment and destructible StaticMeshes."""
    if not IN_EDITOR:
        return

    env_mat_path = f"{UE_WIZARD}/Environment/Materials"
    env_mesh_path = f"{UE_WIZARD}/Environment/Meshes"
    dest_mesh_path = f"{UE_WIZARD}/Destruction/Meshes"

    mat_cache = {}
    def _get_env_mat(name: str):
        if name not in mat_cache:
            mat_cache[name] = unreal.EditorAssetLibrary.load_asset(f"{env_mat_path}/{name}")
        return mat_cache[name]

    env_mesh_mats = {
        "SM_Terrain":    "M_Ground",
        "SM_CliffRing":  "M_Rock",
        "SM_GrassBlade": "M_GrassBlade",
        "SM_Bush":       "M_Foliage",
        "SM_Flower":     "M_Foliage",
        "SM_SmallStone": "M_Rock",
        "SM_RuinColumn": "M_RuinStone",
        "SM_RuinWall":   "M_RuinStone",
    }
    for mesh_name, mat_name in env_mesh_mats.items():
        mesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/{mesh_name}")
        mat = _get_env_mat(mat_name)
        if mesh and mat:
            mesh.set_material(0, mat)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    family_mats = {
        "Stone":   ("M_RuinStone", "M_Piece_Stone"),
        "Rock":    ("M_Rock",      "M_Piece_Rock"),
        "Wood":    ("M_Wood",      "M_Piece_Wood"),
        "Pottery": ("M_Piece_Pottery", "M_Piece_Pottery"),
        "Crystal": ("M_Crystal",   "M_Crystal"),
        "Foliage": ("M_Foliage",   "M_Bark"),
    }
    assigned = 0
    for shape_name, material, _half_ext, n_pieces, _hp in C.DESTRUCTIBLE_SHAPES:
        whole_mat_name, piece_mat_name = family_mats.get(material, ("M_RuinStone", "M_Piece_Stone"))
        whole_mat = _get_env_mat(whole_mat_name)
        piece_mat = _get_env_mat(piece_mat_name)

        whole_mesh = unreal.EditorAssetLibrary.load_asset(f"{dest_mesh_path}/SM_{shape_name}_Whole")
        if whole_mesh and whole_mat:
            whole_mesh.set_material(0, whole_mat)
            unreal.EditorAssetLibrary.save_loaded_asset(whole_mesh)
            assigned += 1

        for idx in range(n_pieces):
            pm = unreal.EditorAssetLibrary.load_asset(f"{dest_mesh_path}/SM_{shape_name}_P{idx:03d}")
            if pm and piece_mat:
                pm.set_material(0, piece_mat)
                unreal.EditorAssetLibrary.save_loaded_asset(pm)
                assigned += 1

    _log(f"Assigned materials to environment meshes and {assigned} destructible meshes")

# ═══════════════════════════════════════════════════════════════════════════════
# DataAsset creation
# ═══════════════════════════════════════════════════════════════════════════════

def create_spell_data_assets(layout: dict):
    """Create DA_Spell_{Element} USpellDataAsset instances."""
    if not IN_EDITOR:
        return
    core_path = f"{UE_WIZARD}/Core"
    _ensure_dir(core_path)
    at = unreal.AssetToolsHelpers.get_asset_tools()

    spells = layout.get("spells", {})
    for elem in C.ELEMENTS:
        da_name = f"DA_Spell_{elem}"
        full = f"{core_path}/{da_name}"

        # Create or load
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            da = unreal.EditorAssetLibrary.load_asset(full)
        else:
            da = at.create_asset(da_name, core_path, unreal.load_class(None, "/Script/WizardEPIC.SpellDataAsset"), None)

        if da is None:
            _log(f"WARNING: Could not create {da_name}")
            continue

        sd = spells.get(elem, {})

        # Set properties (UE 5.8 Python expects typed enum instances)
        elem_enum_cls = getattr(unreal, "SpellElement", None)
        elem_enum_val = getattr(elem_enum_cls, elem.upper()) if elem_enum_cls else 0
        da.set_editor_property("element", elem_enum_val)
        da.set_editor_property("display_name", unreal.Text(elem))
        da.set_editor_property("cooldown", sd.get("cooldown", 1.2))
        da.set_editor_property("cast_delay", sd.get("cast_delay", 0.1))
        da.set_editor_property("charge_time", sd.get("charge_time", 1.0))
        da.set_editor_property("charge_scale_max", sd.get("charge_scale_max", 2.2))
        da.set_editor_property("min_charge_time", sd.get("min_charge", 0.22))
        da.set_editor_property("damage", sd.get("damage", 100.0))
        da.set_editor_property("damage_radius", sd.get("damage_radius", 600.0))
        da.set_editor_property("launch_speed", sd.get("launch_speed", 1400.0))
        da.set_editor_property("up_bias", sd.get("up_bias", 0.25))
        da.set_editor_property("spin", sd.get("spin", 420.0))
        da.set_editor_property("fov_kick", sd.get("fov_kick", 3.5))
        da.set_editor_property("hit_stop", sd.get("hit_stop", 0.045))
        da.set_editor_property("light_intensity", sd.get("light_intensity", 6e6))
        da.set_editor_property("light_radius", sd.get("light_radius", 2200.0))
        da.set_editor_property("light_duration", sd.get("light_dur", 0.55))

        # Delivery-specific
        delivery = sd.get("delivery", "Projectile")
        deliv_cls = getattr(unreal, "SpellDelivery", None)
        deliv_attr = {"Projectile": "PROJECTILE", "GroundLine": "GROUND_LINE", "Cone": "CONE", "Strike": "STRIKE"}.get(delivery, "PROJECTILE")
        deliv_val = getattr(deliv_cls, deliv_attr, getattr(deliv_cls, delivery.upper(), None)) if deliv_cls else 0
        da.set_editor_property("delivery", deliv_val)

        if delivery == "Projectile":
            da.set_editor_property("projectile_speed", sd.get("proj_speed", 4200.0))
            da.set_editor_property("projectile_radius", sd.get("proj_radius", 28.0))
            da.set_editor_property("projectile_gravity_scale", sd.get("proj_gravity", 0.05))
            da.set_editor_property("projectile_life", sd.get("proj_life", 3.0))
        elif delivery == "GroundLine":
            da.set_editor_property("pillar_count", sd.get("pillar_count", 6))
            da.set_editor_property("pillar_spacing", sd.get("pillar_spacing", 230.0))
            da.set_editor_property("pillar_height", sd.get("pillar_height", 520.0))
            da.set_editor_property("pillar_radius", sd.get("pillar_radius", 120.0))
            da.set_editor_property("pillar_rise_time", sd.get("pillar_rise", 0.16))
            da.set_editor_property("pillar_step_delay", sd.get("pillar_step", 0.055))
            da.set_editor_property("pillar_lifetime", sd.get("pillar_life", 4.0))
            da.set_editor_property("pillar_sink_time", sd.get("pillar_sink", 0.5))
            da.set_editor_property("pillar_start_distance", sd.get("pillar_start", 250.0))
        elif delivery == "Cone":
            da.set_editor_property("cone_range", sd.get("cone_range", 1600.0))
            da.set_editor_property("cone_half_angle", sd.get("cone_half_angle", 22.0))
            da.set_editor_property("cone_duration", sd.get("cone_duration", 0.55))
        elif delivery == "Strike":
            da.set_editor_property("strike_count", sd.get("strike_count", 3))
            da.set_editor_property("strike_interval", sd.get("strike_interval", 0.09))
            da.set_editor_property("strike_shatter_bonus", sd.get("strike_shatter", 3.0))

        # Color
        col = sd.get("color", (1, 0.45, 0.12))
        da.set_editor_property("color", unreal.LinearColor(col[0], col[1], col[2], 1.0))

        # Sound names
        da.set_editor_property("cast_sound", unreal.Name(sd.get("cast_sound", "")))
        da.set_editor_property("charge_sound", unreal.Name(sd.get("charge_sound", "")))
        da.set_editor_property("impact_sound", unreal.Name(sd.get("impact_sound", "")))

        # Icon (SoftObjectProperty expects loaded Texture2D in UE 5.8 Python)
        icon_path = f"{UE_WIZARD}/UI/Icons/T_Icon_{elem}"
        icon_tex = unreal.EditorAssetLibrary.load_asset(icon_path)
        if icon_tex:
            da.set_editor_property("icon", icon_tex)

        # Damage multipliers (BlueprintReadOnly struct fields use set_editor_property)
        dmg = sd.get("dmg_table", {})
        try:
            dt = da.get_editor_property("damage_multipliers")
            for mat in C.MATERIALS:
                prop = mat.lower()
                try:
                    dt.set_editor_property(prop, float(dmg.get(mat, 1.0)))
                except Exception:
                    pass
            da.set_editor_property("damage_multipliers", dt)
        except Exception as exc:
            _log(f"  note: damage_multipliers skipped ({exc})")

        unreal.EditorAssetLibrary.save_loaded_asset(da)
        _log(f"Created/updated spell data asset: {da_name}")

def create_destructible_definitions(layout: dict):
    """Create DD_<Name> UDestructibleDefinition DataAssets."""
    if not IN_EDITOR:
        return
    core_path = f"{UE_WIZARD}/Core"
    _ensure_dir(core_path)
    at = unreal.AssetToolsHelpers.get_asset_tools()

    dest_data = layout.get("destruction", {})
    for shape_name, data in dest_data.items():
        dd_name = f"DD_{shape_name}"
        full = f"{core_path}/{dd_name}"

        if unreal.EditorAssetLibrary.does_asset_exist(full):
            dd = unreal.EditorAssetLibrary.load_asset(full)
        else:
            dd = at.create_asset(dd_name, core_path,
                                 unreal.load_class(None, "/Script/WizardEPIC.DestructibleDefinition"), None)

        if dd is None:
            _log(f"WARNING: Could not create {dd_name}")
            continue

        # Material enum
        mat_cls = getattr(unreal, "DestructibleMaterial", None)
        mat_name_raw = data["material"]
        mat_val = getattr(mat_cls, mat_name_raw.upper()) if mat_cls else 0
        dd.set_editor_property("material", mat_val)
        dd.set_editor_property("health", data.get("health", 100.0))
        dd.set_editor_property("score_per_piece", data.get("score_per_piece", 10))
        dd.set_editor_property("collapse_bonus", data.get("collapse_bonus", 100))

        # Whole mesh
        whole_mesh_path = f"{UE_WIZARD}/Destruction/Meshes/{data['whole_mesh']}"
        whole_mesh = unreal.EditorAssetLibrary.load_asset(whole_mesh_path)
        if whole_mesh:
            dd.set_editor_property("whole_mesh", whole_mesh)

        # Break sound
        mat_name = data["material"]
        dd.set_editor_property("break_sound", unreal.Name(f"Break_{mat_name}"))

        # Pieces
        pieces_arr = []
        for pd in data.get("pieces", []):
            piece_def = unreal.PieceDefinition()
            piece_mesh_path = f"{UE_WIZARD}/Destruction/Meshes/{pd['mesh']}"
            piece_mesh = unreal.EditorAssetLibrary.load_asset(piece_mesh_path)
            if piece_mesh:
                piece_def.set_editor_property("mesh", piece_mesh)
            tx = pd["transform"]
            piece_def.set_editor_property("local_transform",
                unreal.Transform(
                    location=unreal.Vector(tx[0], tx[1], tx[2]),
                    rotation=unreal.Rotator(tx[4], tx[5], tx[3])
                ))
            piece_def.set_editor_property("mass", pd.get("mass", 25.0))
            piece_def.set_editor_property("health", pd.get("health", 100.0))
            piece_def.set_editor_property("extent", unreal.Vector(*pd.get("extent", (25, 25, 25))))
            try:
                piece_def.set_editor_property("grounded", pd.get("grounded", False))
            except Exception:
                piece_def.set_editor_property("b_grounded", pd.get("grounded", False))
            piece_def.set_editor_property("supports", pd.get("supports", []))
            piece_def.set_editor_property("laterals", pd.get("laterals", []))
            pieces_arr.append(piece_def)

        dd.set_editor_property("pieces", pieces_arr)

        # Bounds (compute true AABB & sphere radius from pieces for broad-phase culling)
        if data.get("pieces"):
            min_x = min(p["transform"][0] - p["extent"][0] for p in data["pieces"])
            max_x = max(p["transform"][0] + p["extent"][0] for p in data["pieces"])
            min_y = min(p["transform"][1] - p["extent"][1] for p in data["pieces"])
            max_y = max(p["transform"][1] + p["extent"][1] for p in data["pieces"])
            min_z = min(p["transform"][2] - p["extent"][2] for p in data["pieces"])
            max_z = max(p["transform"][2] + p["extent"][2] for p in data["pieces"])
            origin = unreal.Vector((min_x + max_x) * 0.5, (min_y + max_y) * 0.5, (min_z + max_z) * 0.5)
            ext = unreal.Vector((max_x - min_x) * 0.5, (max_y - min_y) * 0.5, (max_z - min_z) * 0.5)
            sphere_r = math.sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z)
            bounds = unreal.BoxSphereBounds(origin=origin, box_extent=ext, sphere_radius=sphere_r)
            dd.set_editor_property("bounds", bounds)
            dd.set_editor_property("size", max(ext.x, ext.y, ext.z) * 2.0)

        unreal.EditorAssetLibrary.save_loaded_asset(dd)
        _log(f"Created/updated destructible definition: {dd_name}")

# ═══════════════════════════════════════════════════════════════════════════════
# Level assembly
# ═══════════════════════════════════════════════════════════════════════════════

FORGE_TAG = unreal.Name("WizardForge") if IN_EDITOR else "WizardForge"

_FORGE_EXACT_LABELS = {
    "Sun", "SkyLight", "SkyAtmosphere", "VolumetricCloud", "HeightFog",
    "PostProcess", "PlayerStart", "Terrain", "CliffRing", "ScatterVegetation",
}
_FORGE_LABEL_PREFIXES = (
    "SM_RuinColumn_", "SM_RuinWall_", "Struct_", "Breakables_", "Crystal_",
)


def _tag_forge_actor(actor):
    if actor:
        tags = list(actor.get_editor_property("tags"))
        if FORGE_TAG not in tags:
            tags.append(FORGE_TAG)
            actor.set_editor_property("tags", tags)
    return actor


def _clear_previous_forge_actors():
    """Remove any previously spawned forge actors so build_arena_level() is idempotent."""
    removed = 0
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if not actor:
            continue
        tags = actor.get_editor_property("tags")
        label = actor.get_actor_label()
        is_forge = (
            (FORGE_TAG in tags)
            or (label in _FORGE_EXACT_LABELS)
            or any(label.startswith(pref) for pref in _FORGE_LABEL_PREFIXES)
        )
        if is_forge:
            unreal.EditorLevelLibrary.destroy_actor(actor)
            removed += 1
    if removed:
        _log(f"Cleared {removed} previous forge actors from L_Arena")


def build_arena_level(layout: dict):
    """Create and populate L_Arena (idempotent on re-run)."""
    if not IN_EDITOR:
        return

    lvl_path = f"{UE_WIZARD}/Levels"
    _ensure_dir(lvl_path)

    # Create or open level
    arena_path = f"{lvl_path}/L_Arena"
    if unreal.EditorAssetLibrary.does_asset_exist(arena_path):
        unreal.EditorLevelLibrary.load_level(arena_path)
        _clear_previous_forge_actors()
    else:
        unreal.EditorLevelLibrary.new_level(arena_path)

    # ── Lighting & atmosphere ─────────────────────────────────────────────────
    _log("Placing lighting & atmosphere...")

    # Directional light (sun from south-west)
    sun = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 0)))
    sun.set_actor_label("Sun")
    sun.set_actor_rotation(unreal.Rotator(-45, -135, 0), False)
    light_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if light_comp:
        light_comp.set_editor_property("intensity", 8.0)
        light_comp.set_editor_property("light_color", unreal.Color(255, 245, 230))

    # Sky light
    sky_light = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, 500)))
    sky_light.set_actor_label("SkyLight")

    # Sky atmosphere
    sky_atmo = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)))
    sky_atmo.set_actor_label("SkyAtmosphere")

    # Volumetric cloud
    vol_cloud = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.VolumetricCloud, unreal.Vector(0, 0, 0)))
    vol_cloud.set_actor_label("VolumetricCloud")

    # Exponential height fog
    fog = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.ExponentialHeightFog, unreal.Vector(0, 0, 100)))
    fog.set_actor_label("HeightFog")

    # Post process volume (unbound)
    ppv = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0, 0, 0)))
    ppv.set_actor_label("PostProcess")
    ppv.set_editor_property("unbound", True)

    # Player start (spawn plaza, aligned to terrain)
    ps_cfg = layout.get("player_start", {})
    ps = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(ps_cfg.get("x", 0.0), ps_cfg.get("y", 0.0), ps_cfg.get("z", 95.0)),
    ))
    ps.set_actor_label("PlayerStart")

    # ── Terrain ───────────────────────────────────────────────────────────────
    _log("Placing terrain & cliffs...")
    terrain_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_Terrain")
    cliff_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_CliffRing")

    if terrain_mesh:
        terrain_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0)))
        terrain_actor.set_actor_label("Terrain")
        terrain_actor.static_mesh_component.set_static_mesh(terrain_mesh)

    if cliff_mesh:
        cliff_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0)))
        cliff_actor.set_actor_label("CliffRing")
        cliff_actor.static_mesh_component.set_static_mesh(cliff_mesh)

    # ── Ruin props (StaticMeshActor) ──────────────────────────────────────────
    _log("Placing ruin props...")
    ruin_entries = layout.get("ruin_props")
    if ruin_entries:
        for entry in ruin_entries:
            mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{entry['mesh']}")
            if not mesh:
                continue
            actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(entry["x"], entry["y"], entry["z"])))
            actor.set_actor_label(entry["label"])
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.set_actor_rotation(unreal.Rotator(0, entry["yaw"], 0), False)
    else:
        for ruin_name in ["SM_RuinColumn", "SM_RuinWall"]:
            mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{ruin_name}")
            if not mesh:
                continue
            for i, pos in enumerate(C._ring(4, 2900, 0, math.pi / 4)):
                actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
                    unreal.StaticMeshActor, unreal.Vector(pos[0], pos[1], pos[2])))
                actor.set_actor_label(f"{ruin_name}_{i}")
                actor.static_mesh_component.set_static_mesh(mesh)
                actor.set_actor_rotation(unreal.Rotator(0, pos[3], 0), False)

    # ── Destructible structures ───────────────────────────────────────────────
    _log("Placing destructible structures...")
    struct_class = unreal.load_class(None, "/Script/WizardEPIC.DestructibleStructure")
    for idx, struct_entry in enumerate(layout.get("structures", [])):
        def_name = struct_entry["definition"]
        dd_path = f"{UE_WIZARD}/Core/DD_{def_name}"
        dd = unreal.EditorAssetLibrary.load_asset(dd_path)
        pos = unreal.Vector(struct_entry["x"], struct_entry["y"], struct_entry["z"])
        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(struct_class, pos))
        if actor and dd:
            actor.set_actor_label(f"Struct_{def_name}_{idx:02d}")
            actor.set_editor_property("definition", dd)
            actor.set_actor_rotation(unreal.Rotator(0, struct_entry["yaw"], 0), False)

    # ── Breakable sets ────────────────────────────────────────────────────────
    _log("Placing breakable sets...")
    breakable_class = unreal.load_class(None, "/Script/WizardEPIC.WizardBreakableSet")
    for def_name, placements in layout.get("breakables", {}).items():
        dd_path = f"{UE_WIZARD}/Core/DD_{def_name}"
        dd = unreal.EditorAssetLibrary.load_asset(dd_path)
        whole_mesh_name = f"SM_{def_name}_Whole"
        whole_mesh = unreal.EditorAssetLibrary.load_asset(
            f"{UE_WIZARD}/Destruction/Meshes/{whole_mesh_name}")

        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            breakable_class, unreal.Vector(0, 0, 0)))
        if not actor:
            continue
        actor.set_actor_label(f"Breakables_{def_name}")
        if dd:
            actor.set_editor_property("definition", dd)

        # Set the mesh on the Instances ISMC
        instances_comp = actor.get_editor_property("instances")
        if instances_comp and whole_mesh:
            instances_comp.set_static_mesh(whole_mesh)

        # Add instances
        for p in placements:
            transform = unreal.Transform(
                location=unreal.Vector(p["x"], p["y"], p["z"]),
                rotation=unreal.Rotator(0, p["yaw"], 0),
                scale=unreal.Vector(1, 1, 1)
            )
            actor.add_instance(transform)

    # ── Crystal targets ───────────────────────────────────────────────────────
    _log("Placing crystal targets...")
    crystal_class = unreal.load_class(None, "/Script/WizardEPIC.CrystalTarget")
    crystal_dd = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Core/DD_Crystal_A")
    for i, cp in enumerate(layout.get("crystals", [])):
        pos = unreal.Vector(cp["x"], cp["y"], cp["z"])
        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(crystal_class, pos))
        if actor:
            actor.set_actor_label(f"Crystal_{i:02d}")
            if crystal_dd:
                actor.set_editor_property("definition", crystal_dd)
            actor.set_actor_rotation(unreal.Rotator(0, cp["yaw"], 0), False)

    # ── Scatter sets ──────────────────────────────────────────────────────────
    _log("Placing scatter vegetation...")
    scatter_class = unreal.load_class(None, "/Script/WizardEPIC.WizardScatterSet")
    scatter_data = layout.get("scatter", {})
    scatter_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        scatter_class, unreal.Vector(0, 0, 0)))
    if scatter_actor:
        scatter_actor.set_actor_label("ScatterVegetation")
        layer_idx = 0
        rng = __import__("random").Random(12345)
        for mesh_name, cfg in scatter_data.items():
            mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{mesh_name}")
            if not mesh:
                continue
            layers = scatter_actor.get_editor_property("instance_layers")
            if layer_idx >= len(layers):
                break
            layers[layer_idx].set_static_mesh(mesh)

            transforms = []
            if cfg.get("instances"):
                for x, y, z, yaw, scale in cfg["instances"]:
                    transforms.append(unreal.Transform(
                        location=unreal.Vector(x, y, z),
                        rotation=unreal.Rotator(0, yaw, 0),
                        scale=unreal.Vector(scale, scale, scale)
                    ))
            else:
                count = cfg.get("count", 100)
                r_min, r_max = cfg.get("radius_range", (100, 3400))
                z = cfg.get("z", 0)
                for _ in range(count):
                    angle = rng.uniform(0, 2 * math.pi)
                    radius = rng.uniform(r_min, r_max)
                    x = radius * math.cos(angle)
                    y = radius * math.sin(angle)
                    yaw = rng.uniform(0, 360)
                    scale = rng.uniform(0.7, 1.3)
                    transforms.append(unreal.Transform(
                        location=unreal.Vector(x, y, z),
                        rotation=unreal.Rotator(0, yaw, 0),
                        scale=unreal.Vector(scale, scale, scale)
                    ))
            scatter_actor.add_instances(layer_idx, transforms)
            _log(f"  Scatter {mesh_name}: {len(transforms)} instances on layer {layer_idx}")
            layer_idx += 1

    # ── Save level ────────────────────────────────────────────────────────────
    _log("Saving L_Arena...")
    unreal.EditorLevelLibrary.save_current_level()
    _log("Level assembly complete!")

# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    _log("=== wizardEPIC UE Forge ===")

    if not LAYOUT_JSON.exists():
        _log(f"ERROR: {LAYOUT_JSON} not found. Run offline_forge.py first.")
        return

    with open(LAYOUT_JSON, "r", encoding="utf-8") as f:
        layout = json.load(f)

    if not IN_EDITOR:
        _log("Syntax check passed. Run inside UnrealEditor to import assets.")
        return

    # Step 1: Import all generated assets
    _log("--- Step 1: Import textures ---")
    import_textures()

    _log("--- Step 2: Import audio ---")
    import_audio()

    _log("--- Step 3: Import meshes ---")
    import_meshes()

    _log("--- Step 4: Import destruction meshes ---")
    import_destruction_meshes()

    # Step 2: Create materials & assign to meshes
    _log("--- Step 5: Create materials ---")
    create_materials()

    _log("--- Step 5b: Assign mesh materials ---")
    assign_mesh_materials()

    # Step 3: Create DataAssets
    _log("--- Step 6: Create spell DataAssets ---")
    create_spell_data_assets(layout)

    _log("--- Step 7: Create destructible definitions ---")
    create_destructible_definitions(layout)

    # Step 4: Build level
    _log("--- Step 8: Assemble L_Arena ---")
    build_arena_level(layout)

    _log("=== Forge complete! ===")

main()
