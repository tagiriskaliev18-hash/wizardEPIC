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
import json, os, sys, math, shutil
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

def sync_blender_assets():
    """Copy staged BlenderAssets meshes/textures into Generated/ without touching Piece_* destructibles."""
    blender_dir = SCRIPT_DIR / "BlenderAssets"
    if not blender_dir.exists():
        _log(f"WARNING: BlenderAssets directory not found: {blender_dir}")
        return

    blender_tex = blender_dir / "Textures"
    gen_tex = GENERATED / "Textures"
    tex_copied = 0
    if blender_tex.exists():
        gen_tex.mkdir(parents=True, exist_ok=True)
        for png_file in sorted(blender_tex.glob("*.png")):
            shutil.copy2(png_file, gen_tex / png_file.name)
            tex_copied += 1

    blender_mesh = blender_dir / "Meshes"
    gen_mesh = GENERATED / "Meshes"
    gen_dest = GENERATED / "Destruction"
    mesh_copied = 0
    dest_whole_copied = 0
    if blender_mesh.exists():
        gen_mesh.mkdir(parents=True, exist_ok=True)
        for obj_file in sorted(blender_mesh.glob("*.obj")):
            stem = obj_file.stem
            # Never overwrite fractured Voronoi Piece_* assets
            if "_P0" in stem or stem.startswith("Piece_"):
                continue
            shutil.copy2(obj_file, gen_mesh / obj_file.name)
            mesh_copied += 1
            if stem.startswith("SM_") and stem.endswith("_Whole"):
                shape_name = stem[3:-6]
                dest_whole = gen_dest / shape_name / obj_file.name
                if dest_whole.parent.exists():
                    shutil.copy2(obj_file, dest_whole)
                    dest_whole_copied += 1

    _log(
        f"Synced BlenderAssets: {tex_copied} textures -> Generated/Textures, "
        f"{mesh_copied} meshes -> Generated/Meshes, {dest_whole_copied} *_Whole -> Generated/Destruction"
    )


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
    tex_routing["T_FX_WindWisp"] = f"{UE_WIZARD}/FX/Textures"
    for name in ["T_DetailNoise", "T_Ground", "T_Rock", "T_Sandstone", "T_Wood", "T_Crystal"]:
        tex_routing[name] = f"{UE_WIZARD}/Environment/Textures"
    for elem in C.ELEMENTS:
        tex_routing[f"T_Icon_{elem}"] = f"{UE_WIZARD}/UI/Icons"

    for png_file in sorted(tex_dir.glob("*.png")):
        stem = png_file.stem
        if stem.startswith("T_FX_"):
            dest = f"{UE_WIZARD}/FX/Textures"
        else:
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


def import_pbr_textures():
    """Import Generated/PBR/* into /Game/Wizard/Environment/Textures/PBR/ with proper compression & sRGB."""
    if not IN_EDITOR:
        return
    pbr_dir = GENERATED / "PBR"
    if not pbr_dir.exists():
        _log(f"WARNING: PBR directory not found: {pbr_dir}")
        return

    dest = f"{UE_WIZARD}/Environment/Textures/PBR"
    _ensure_dir(dest)

    files = sorted(
        p for p in pbr_dir.iterdir()
        if p.is_file() and p.suffix.lower() in (".jpg", ".jpeg", ".png")
    )
    if not files:
        _log(f"WARNING: No PBR texture files found in {pbr_dir}")
        return

    tasks = []
    for tex_file in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(tex_file))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", tex_file.stem)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    configured = 0
    for tex_file in files:
        stem = tex_file.stem
        asset_path = f"{dest}/{stem}"
        tex = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not tex:
            _log(f"ERROR: Failed to load imported PBR texture {asset_path}")
            continue
        try:
            if stem.endswith("_nor"):
                tex.set_editor_property(
                    "compression_settings",
                    unreal.TextureCompressionSettings.TC_NORMALMAP,
                )
                tex.set_editor_property("srgb", False)
            elif stem.endswith("_rough"):
                tex.set_editor_property(
                    "compression_settings",
                    unreal.TextureCompressionSettings.TC_DEFAULT,
                )
                tex.set_editor_property("srgb", False)
            else:
                tex.set_editor_property("srgb", True)
            if hasattr(tex, "post_edit_change"):
                tex.post_edit_change()
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            configured += 1
        except Exception as exc:
            _log(f"ERROR: Failed configuring PBR texture {stem}: {exc}")

    _log(f"Imported and configured {configured}/{len(files)} PBR textures in {dest}")

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

def _set_complex_collision(asset_paths: list[str]):
    """Set CTF_USE_COMPLEX_AS_SIMPLE on StaticMesh body_setup so concave/arch meshes don't block as solid hulls."""
    trace_flag = getattr(
        getattr(unreal, "CollisionTraceFlag", None),
        "CTF_USE_COMPLEX_AS_SIMPLE",
        None,
    )
    if trace_flag is None:
        _log("WARNING: CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE not available")
        return
    updated = 0
    for asset_path in asset_paths:
        sm = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not sm:
            _log(f"WARNING: StaticMesh not found for collision update: {asset_path}")
            continue
        try:
            bs = sm.get_editor_property("body_setup")
            if bs:
                bs.set_editor_property("collision_trace_flag", trace_flag)
                if hasattr(sm, "post_edit_change"):
                    sm.post_edit_change()
                unreal.EditorAssetLibrary.save_loaded_asset(sm)
                updated += 1
            else:
                _log(f"WARNING: No body_setup on {asset_path}")
        except Exception as exc:
            _log(f"ERROR: Failed setting complex collision on {asset_path}: {exc}")
    _log(f"Set CTF_USE_COMPLEX_AS_SIMPLE on {updated}/{len(asset_paths)} meshes")


def import_meshes():
    """Import Generated/Meshes/*.obj into /Game/Wizard/... by category."""
    if not IN_EDITOR:
        return
    mesh_dir = GENERATED / "Meshes"
    tasks = []

    for obj_file in sorted(mesh_dir.glob("*.obj")):
        stem = obj_file.stem
        if stem.startswith("SM_FX_") or stem == "SM_RuneCircle":
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

    env_mesh_root = f"{UE_WIZARD}/Environment/Meshes"
    complex_env = [
        f"{env_mesh_root}/{name}"
        for name in ("SM_Terrain", "SM_CliffRing", "SM_ArenaPlaza", "SM_RuinColumn", "SM_RuinWall", "SM_RuinArch")
    ]
    _set_complex_collision(complex_env)


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

    complex_dest = [
        f"{dest_root}/{name}"
        for name in (
            "SM_Arch_A_Whole",
            "SM_Wall_A_Whole",
            "SM_Wall_B_Whole",
            "SM_Pillar_A_Whole",
            "SM_Tower_A_Whole",
        )
    ]
    _set_complex_collision(complex_dest)

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


def _make_tex_param(
    mat,
    param_name: str,
    tex_path: str | None,
    x: int,
    y: int,
    sampler_type: str | None = None,
):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    expr.set_editor_property("parameter_name", param_name)

    sampler_map = {
        "color": "SAMPLERTYPE_COLOR",
        "normal": "SAMPLERTYPE_NORMAL",
        "linear": "SAMPLERTYPE_LINEAR_COLOR",
    }
    st_enum = None
    if sampler_type and hasattr(unreal, "MaterialSamplerType"):
        st_name = sampler_map.get(sampler_type)
        if st_name:
            st_enum = getattr(unreal.MaterialSamplerType, st_name, None)

    if st_enum is not None:
        try:
            expr.set_editor_property("sampler_type", st_enum)
        except Exception as exc:
            _log(f"ERROR: Failed pre-setting sampler_type={sampler_type} on {param_name}: {exc}")

    if tex_path:
        tex = unreal.EditorAssetLibrary.load_asset(tex_path)
        if tex:
            expr.set_editor_property("texture", tex)
            # Safety guard: align sampler_type with texture's actual sRGB flag so SM6 compile never fails
            if st_enum is not None and sampler_type == "linear":
                try:
                    if tex.get_editor_property("srgb"):
                        st_enum = getattr(unreal.MaterialSamplerType, "SAMPLERTYPE_COLOR", st_enum)
                except Exception:
                    pass
        else:
            _log(f"WARNING: Texture not found for {param_name}: {tex_path}")

    if st_enum is not None:
        try:
            expr.set_editor_property("sampler_type", st_enum)
        except Exception as exc:
            _log(f"ERROR: Failed setting sampler_type={sampler_type} on {param_name}: {exc}")

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


def _make_add(mat, a_expr, a_out: str, b_expr, b_out: str, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, x, y)
    mel.connect_material_expressions(a_expr, a_out, expr, "A")
    mel.connect_material_expressions(b_expr, b_out, expr, "B")
    return expr


def _make_lerp(mat, a_expr, a_out: str, b_expr, b_out: str, alpha_expr, alpha_out: str, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a_expr, a_out, expr, "A")
    mel.connect_material_expressions(b_expr, b_out, expr, "B")
    mel.connect_material_expressions(alpha_expr, alpha_out, expr, "Alpha")
    return expr


def _make_const3(mat, r: float, g: float, b: float, x: int, y: int):
    mel = unreal.MaterialEditingLibrary
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(float(r), float(g), float(b), 1.0))
    return c


def _finalize_material(mat, use_ism: bool = True):
    if use_ism:
        try:
            mat.set_editor_property("used_with_instanced_static_meshes", True)
        except Exception as exc:
            _log(f"ERROR: Failed setting used_with_instanced_static_meshes on {mat.get_name()}: {exc}")
    mel = unreal.MaterialEditingLibrary
    try:
        mel.layout_material_expressions(mat)
    except Exception as exc:
        _log(f"WARNING: layout_material_expressions failed on {mat.get_name()}: {exc}")
    try:
        mel.recompile_material(mat)
    except Exception as exc:
        _log(f"ERROR: recompile_material failed on {mat.get_name()}: {exc}")
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
        try:
            d.set_editor_property("const_default_value", 1.0)
        except Exception:
            pass

    rg = mel.create_material_expression(mat, append_cls, -440, 200)
    mel.connect_material_expressions(d0, "", rg, "A")
    mel.connect_material_expressions(d1, "", rg, "B")
    rgb = mel.create_material_expression(mat, append_cls, -300, 240)
    mel.connect_material_expressions(rg, "", rgb, "A")
    mel.connect_material_expressions(d2, "", rgb, "B")

    out_rgb = _make_mul(mat, base_rgb_expr, base_rgb_out, rgb, "", -160, 120)
    out_alpha = _make_mul(mat, base_alpha_expr, base_alpha_out, d3, "", -160, 340)
    return (out_rgb, ""), (out_alpha, "")


def _connect_uv_coord(uv_expr, tex_expr, label: str):
    """Connect UV coordinate expression to a TextureSample input (Coordinates in UE5, UVs fallback)."""
    mel = unreal.MaterialEditingLibrary
    for pin in ("Coordinates", "UVs"):
        try:
            if mel.connect_material_expressions(uv_expr, "", tex_expr, pin):
                return True
        except Exception:
            pass
    _log(f"ERROR: Could not connect UV coordinate to {label} (tried Coordinates, UVs)")
    return False


def _wire_pbr_graph(
    mat,
    pbr_key: str,
    fallback_tex_path: str,
    tint_rgba: tuple[float, float, float, float],
    rough_scale: float = 1.0,
    uv_tile: float = 1.0,
    macro_anti_tiling: bool = False,
):
    """Wire TextureCoordinate * UVTile -> PBR Albedo/Normal/Roughness graph with optional 0.18x macro anti-tiling."""
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    pbr_root = f"{UE_WIZARD}/Environment/Textures/PBR"
    detail_noise_path = f"{UE_WIZARD}/Environment/Textures/T_DetailNoise"

    diff_path = f"{pbr_root}/PBR_{pbr_key}_diff"
    nor_path = f"{pbr_root}/PBR_{pbr_key}_nor"
    rough_path = f"{pbr_root}/PBR_{pbr_key}_rough"

    has_diff = eal.does_asset_exist(diff_path)
    has_nor = eal.does_asset_exist(nor_path)
    has_rough = eal.does_asset_exist(rough_path)

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -920, 0)
    uv_scale = _make_scalar_param(mat, "UVTile", uv_tile, -920, 120)
    uv_coord = _make_mul(mat, tc, "", uv_scale, "", -720, 40)

    albedo = _make_tex_param(
        mat,
        "Tex",
        diff_path if has_diff else fallback_tex_path,
        -500,
        -160,
        sampler_type="color",
    )
    _connect_uv_coord(uv_coord, albedo, f"{mat.get_name()}.Tex")
    tint = _make_vec_param(mat, "Tint", tint_rgba, -500, 40)
    base_col = _make_mul(mat, albedo, "RGB", tint, "", -240, -60)

    macro_noise = None
    if macro_anti_tiling and eal.does_asset_exist(detail_noise_path):
        # Secondary ~0.18x macro-scale noise modulating albedo (0.78..1.18) and roughness (+-0.12)
        macro_uv = _make_mul(mat, uv_coord, "", _make_const(mat, 0.18, -720, -240), "", -620, -260)
        macro_noise = _make_tex_param(mat, "MacroNoise", detail_noise_path, -460, -340, sampler_type="color")
        _connect_uv_coord(macro_uv, macro_noise, f"{mat.get_name()}.MacroNoise")
        albedo_mod = _make_lerp(
            mat,
            _make_const(mat, 0.78, -340, -400), "",
            _make_const(mat, 1.18, -340, -340), "",
            macro_noise, "R",
            -200, -300,
        )
        base_col = _make_mul(mat, base_col, "", albedo_mod, "", -60, -120)

    mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if has_nor:
        nor = _make_tex_param(mat, "NormalTex", nor_path, -500, 220, sampler_type="normal")
        _connect_uv_coord(uv_coord, nor, f"{mat.get_name()}.NormalTex")
        mel.connect_material_property(nor, "RGB", unreal.MaterialProperty.MP_NORMAL)
    else:
        _log(f"WARNING: Missing PBR normal map {nor_path}; skipping MP_NORMAL")

    if has_rough:
        rough_tex = _make_tex_param(mat, "RoughTex", rough_path, -500, 420, sampler_type="linear")
        _connect_uv_coord(uv_coord, rough_tex, f"{mat.get_name()}.RoughTex")
        rough_scalar = _make_scalar_param(mat, "Roughness", rough_scale, -500, 600)
        rough_val = _make_mul(mat, rough_tex, "R", rough_scalar, "", -240, 460)
    else:
        rough_val = _make_scalar_param(mat, "Roughness", rough_scale, -240, 460)

    if macro_noise is not None:
        rough_delta = _make_lerp(
            mat,
            _make_const(mat, -0.10, -260, 620), "",
            _make_const(mat, 0.12, -260, 680), "",
            macro_noise, "G",
            -120, 620,
        )
        rough_sum = _make_add(mat, rough_val, "", rough_delta, "", 20, 520)
        rough_clamped, rough_pin = _make_clamp01(mat, rough_sum, "", 160, 520, f"{mat.get_name()}.RoughClamp")
        mel.connect_material_property(rough_clamped, rough_pin, unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        mel.connect_material_property(rough_val, "", unreal.MaterialProperty.MP_ROUGHNESS)

    return base_col


def _wire_crystal_emissive(mat, base_col, emissive_val: float, fade_expr=None):
    """Connect glowing turquoise crystalline emissive with subtle Fresnel rim boost."""
    mel = unreal.MaterialEditingLibrary
    em_scale = _make_scalar_param(mat, "EmissiveScale", emissive_val, -260, 240)
    em_base = _make_mul(mat, base_col, "", em_scale, "", -100, 200)
    fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
    add_cls = getattr(unreal, "MaterialExpressionAdd", None)
    if fresnel_cls and add_cls:
        try:
            fresnel = mel.create_material_expression(mat, fresnel_cls, -360, 340)
            fresnel.set_editor_property("exponent", 2.2)
            fresnel.set_editor_property("base_reflect_fraction", 0.25)
            add_expr = mel.create_material_expression(mat, add_cls, -140, 340)
            mel.connect_material_expressions(_make_const(mat, 0.65, -360, 440), "", add_expr, "A")
            mel.connect_material_expressions(fresnel, "", add_expr, "B")
            em_base = _make_mul(mat, em_base, "", add_expr, "", 20, 240)
        except Exception as exc:
            _log(f"WARNING: Crystal Fresnel setup skipped ({exc})")
    if fade_expr is not None:
        em_base = _make_mul(mat, em_base, "", fade_expr, "", 140, 240)
    mel.connect_material_property(em_base, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _connect_unary(src_expr, src_out: str, dst_expr, label: str) -> bool:
    """Connect a single-input material expression trying common UE5 pin names."""
    mel = unreal.MaterialEditingLibrary
    for pin in ("Input", "", "X", "Value"):
        try:
            if mel.connect_material_expressions(src_expr, src_out, dst_expr, pin):
                return True
        except Exception:
            pass
    _log(f"ERROR: Could not connect unary pin on {label}")
    return False


def _make_clamp01(mat, src_expr, src_out: str, x: int, y: int, label: str):
    """Create a 0..1 Clamp expression (preferred over Saturate which can leave Input unbound)."""
    mel = unreal.MaterialEditingLibrary
    clamp_cls = getattr(unreal, "MaterialExpressionClamp", None)
    if clamp_cls:
        c = mel.create_material_expression(mat, clamp_cls, x, y)
        try:
            c.set_editor_property("min_default", 0.0)
            c.set_editor_property("max_default", 1.0)
        except Exception:
            pass
        if _connect_unary(src_expr, src_out, c, f"{label}.Clamp"):
            return c, ""
    sat_cls = getattr(unreal, "MaterialExpressionSaturate", None)
    if sat_cls:
        s = mel.create_material_expression(mat, sat_cls, x, y)
        if _connect_unary(src_expr, src_out, s, f"{label}.Saturate"):
            return s, ""
    return src_expr, src_out


def _wire_foliage_graph(
    mat,
    tint_rgba: tuple[float, float, float, float],
    subsurface_rgba: tuple[float, float, float, float],
    rough_val: float = 0.72,
):
    """Wire PBR_foliage_rgba (BaseColor + OpacityMask) + PBR_foliage_nor + TwoSidedFoliage subsurface."""
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    pbr_root = f"{UE_WIZARD}/Environment/Textures/PBR"
    rgba_path = f"{pbr_root}/PBR_foliage_rgba"
    nor_path = f"{pbr_root}/PBR_foliage_nor"
    fallback_tex = f"{UE_WIZARD}/Environment/Textures/T_DetailNoise"

    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    mat.set_editor_property("two_sided", True)
    try:
        mat.set_editor_property("opacity_mask_clip_value", 0.38)
    except Exception as exc:
        _log(f"WARNING: opacity_mask_clip_value failed on {mat.get_name()}: {exc}")

    has_rgba = eal.does_asset_exist(rgba_path)
    tex = _make_tex_param(
        mat,
        "Tex",
        rgba_path if has_rgba else fallback_tex,
        -540,
        -160,
        sampler_type="color",
    )
    tint = _make_vec_param(mat, "Tint", tint_rgba, -540, 40)
    base_col = _make_mul(mat, tex, "RGB", tint, "", -240, -60)
    mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Mask out white cutout patches inside PBR_foliage_rgba where R*B > ~0.24 linear (sRGB ~190+)
    mask_expr, mask_pin = tex, "A"
    add_cls = getattr(unreal, "MaterialExpressionAdd", None)
    if has_rgba and add_cls:
        rb_prod = _make_mul(mat, tex, "R", tex, "B", -400, -280)
        neg_rb = _make_mul(mat, rb_prod, "", _make_const(mat, -1.0, -400, -190), "", -280, -280)
        thresh = mel.create_material_expression(mat, add_cls, -180, -280)
        mel.connect_material_expressions(neg_rb, "", thresh, "A")
        mel.connect_material_expressions(_make_const(mat, 0.24, -280, -200), "", thresh, "B")
        scaled = _make_mul(mat, thresh, "", _make_const(mat, 12.0, -180, -200), "", -60, -280)
        clip_expr, clip_pin = _make_clamp01(mat, scaled, "", 60, -280, f"{mat.get_name()}.WhiteClip")
        masked_alpha = _make_mul(mat, tex, "A", clip_expr, clip_pin, 180, -220)
        mask_expr, mask_pin = masked_alpha, ""
    mel.connect_material_property(mask_expr, mask_pin, unreal.MaterialProperty.MP_OPACITY_MASK)

    if eal.does_asset_exist(nor_path):
        nor = _make_tex_param(mat, "NormalTex", nor_path, -540, 220, sampler_type="normal")
        mel.connect_material_property(nor, "RGB", unreal.MaterialProperty.MP_NORMAL)
    else:
        _log(f"WARNING: Missing foliage normal map {nor_path}; skipping MP_NORMAL")

    rough = _make_scalar_param(mat, "Roughness", rough_val, -240, 360)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    subsurf_vec = _make_vec_param(mat, "SubsurfaceColor", subsurface_rgba, -540, 480)
    subsurf_col = _make_mul(mat, tex, "RGB", subsurf_vec, "", -240, 480)
    mel.connect_material_property(subsurf_col, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    return base_col


def _wire_soft_radial_mask(mat, x: int = -760, y: int = 360):
    """Build smooth radial UV mask (1 at center -> 0 at edge radius) to prevent card corners."""
    mel = unreal.MaterialEditingLibrary
    add_cls = getattr(unreal, "MaterialExpressionAdd", None)
    dot_cls = getattr(unreal, "MaterialExpressionDotProduct", None)
    one_minus_cls = getattr(unreal, "MaterialExpressionOneMinus", None)
    if not (add_cls and dot_cls and one_minus_cls):
        return None

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
    sub_half = mel.create_material_expression(mat, add_cls, x + 160, y)
    mel.connect_material_expressions(tc, "", sub_half, "A")
    mel.connect_material_expressions(_make_const(mat, -0.5, x, y + 90), "", sub_half, "B")

    dot_expr = mel.create_material_expression(mat, dot_cls, x + 300, y)
    mel.connect_material_expressions(sub_half, "", dot_expr, "A")
    mel.connect_material_expressions(sub_half, "", dot_expr, "B")

    r2 = _make_mul(mat, dot_expr, "", _make_const(mat, 4.0, x + 300, y + 90), "", x + 440, y)
    r4 = _make_mul(mat, r2, "", r2, "", x + 560, y)
    inv = mel.create_material_expression(mat, one_minus_cls, x + 680, y)
    _connect_unary(r4, "", inv, f"{mat.get_name()}.RadialOneMinus")
    clamped_expr, _ = _make_clamp01(mat, inv, "", x + 800, y, f"{mat.get_name()}.Radial")
    return clamped_expr


def _wire_wind_wisp_fx(
    mat,
    wisp_tex_path: str,
    default_tint: tuple[float, float, float, float],
    default_em: float,
    pan_x: float,
    pan_y: float,
    fade_dist: float = 80.0,
    ribbon_v_mask: bool = False,
    is_additive: bool = False,
    opacity_scale: float = 0.72,
    square_wisp_alpha: bool = False,
):
    """Wire panning T_FX_WindWisp + DepthFade + soft emissive edge mask for WindRibbon/Ring."""
    mel = unreal.MaterialEditingLibrary
    mat.set_editor_property(
        "blend_mode",
        unreal.BlendMode.BLEND_ADDITIVE if is_additive else unreal.BlendMode.BLEND_TRANSLUCENT,
    )
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, -120)
    uv_expr = tc
    panner_cls = getattr(unreal, "MaterialExpressionPanner", None)
    if panner_cls:
        panner = mel.create_material_expression(mat, panner_cls, -900, -120)
        try:
            panner.set_editor_property("speed_x", float(pan_x))
            panner.set_editor_property("speed_y", float(pan_y))
        except Exception as exc:
            _log(f"WARNING: Panner speed props failed on {mat.get_name()}: {exc}")
        mel.connect_material_expressions(tc, "", panner, "Coordinate")
        uv_expr = panner

    wisp = _make_tex_param(mat, "Tex", wisp_tex_path, -680, -180, sampler_type="color")
    _connect_uv_coord(uv_expr, wisp, f"{mat.get_name()}.Tex")

    tint = _make_vec_param(mat, "Tint", default_tint, -680, 40)
    em_scale = _make_scalar_param(mat, "EmissiveScale", default_em, -680, 180)
    fade = _make_scalar_param(mat, "Fade", 1.0, -680, 300)

    mask_expr, mask_pin = wisp, "A"
    if square_wisp_alpha:
        a_sq = _make_mul(mat, wisp, "A", wisp, "A", -500, 40)
        mask_expr = _make_mul(mat, a_sq, "", wisp, "A", -400, 40)
        mask_pin = ""

    # Soft V edge mask: 4 * V * (1 - V) clamped 0..1 -> 0 at V=0 and V=1, 1 at ribbon center
    if ribbon_v_mask:
        cm_cls = getattr(unreal, "MaterialExpressionComponentMask", None)
        om_cls = getattr(unreal, "MaterialExpressionOneMinus", None)
        if cm_cls and om_cls:
            v_mask = mel.create_material_expression(mat, cm_cls, -900, 120)
            v_mask.set_editor_property("r", False)
            v_mask.set_editor_property("g", True)
            v_mask.set_editor_property("b", False)
            v_mask.set_editor_property("a", False)
            _connect_unary(tc, "", v_mask, f"{mat.get_name()}.VMask")

            one_minus_v = mel.create_material_expression(mat, om_cls, -760, 180)
            _connect_unary(v_mask, "", one_minus_v, f"{mat.get_name()}.OneMinusV")
            v_prod = _make_mul(mat, v_mask, "", one_minus_v, "", -620, 140)
            v_bell = _make_mul(mat, v_prod, "", _make_const(mat, 4.0, -620, 240), "", -500, 160)
            v_clamp, v_clamp_pin = _make_clamp01(mat, v_bell, "", -380, 160, f"{mat.get_name()}.VBell")

            combined = _make_mul(mat, mask_expr, mask_pin, v_clamp, v_clamp_pin, -260, 80)
            mask_expr, mask_pin = combined, ""

    # DepthFade so wind wisps & shockwave rings blend cleanly into terrain/ruins
    depth_expr, depth_pin = mask_expr, mask_pin
    df_cls = getattr(unreal, "MaterialExpressionDepthFade", None)
    if df_cls:
        df = mel.create_material_expression(mat, df_cls, -140, 140)
        for prop_name in ("fade_distance_default", "fade_distance"):
            try:
                df.set_editor_property(prop_name, float(fade_dist))
                break
            except Exception:
                pass
        mel.connect_material_expressions(mask_expr, mask_pin, df, "Opacity")
        mel.connect_material_expressions(_make_const(mat, fade_dist, -300, 260), "", df, "FadeDistance")
        depth_expr, depth_pin = df, ""

    alpha_tint = _make_mul(mat, depth_expr, depth_pin, tint, "A", 0, 180)
    alpha_fade = _make_mul(mat, alpha_tint, "", fade, "", 120, 200)

    # Use linear wisp.A * Tint so grayscale sRGB attenuation doesn't crush wind ribbon visibility
    em_gain = 0.35 if is_additive else 2.2
    wisp_tint = _make_mul(mat, tint, "", wisp, "A", -300, -120)
    em_scaled = _make_mul(mat, wisp_tint, "", em_scale, "", -140, -100)
    em_gain_mul = _make_mul(mat, em_scaled, "", _make_const(mat, em_gain, -140, -20), "", 20, -100)
    em_masked = _make_mul(mat, em_gain_mul, "", alpha_fade, "", 180, -40)
    mel.connect_material_property(em_masked, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if not is_additive:
        op = _make_mul(mat, alpha_fade, "", _make_const(mat, opacity_scale, 120, 320), "", 260, 220)
        mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)


def _wire_meadow_ground_graph(mat):
    """Lush golden-hour wildflower meadow ground: emerald grass + worn earth variation + 0.18x macro anti-tiling."""
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    pbr_root = f"{UE_WIZARD}/Environment/Textures/PBR"
    env_tex_root = f"{UE_WIZARD}/Environment/Textures"

    diff_path = f"{pbr_root}/PBR_ground_diff"
    nor_path = f"{pbr_root}/PBR_ground_nor"
    rough_path = f"{pbr_root}/PBR_ground_rough"
    noise_path = f"{env_tex_root}/T_DetailNoise"
    fallback_tex = f"{env_tex_root}/T_Ground"

    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    uv_detail = _make_mul(mat, tc, "", _make_scalar_param(mat, "UVTile", 18.0, -1100, 120), "", -900, 40)
    uv_macro = _make_mul(mat, uv_detail, "", _make_const(mat, 0.18, -900, -140), "", -740, -140)
    uv_mega = _make_mul(mat, uv_detail, "", _make_const(mat, 0.065, -900, -280), "", -740, -280)

    detail_tex = _make_tex_param(
        mat, "Tex", diff_path if eal.does_asset_exist(diff_path) else fallback_tex,
        -600, 40, sampler_type="color",
    )
    _connect_uv_coord(uv_detail, detail_tex, "M_Ground.Tex")

    macro_noise = _make_tex_param(
        mat, "MacroNoise", noise_path if eal.does_asset_exist(noise_path) else fallback_tex,
        -560, -180, sampler_type="color",
    )
    _connect_uv_coord(uv_macro, macro_noise, "M_Ground.MacroNoise")

    mega_noise = _make_tex_param(
        mat, "MegaNoise", noise_path if eal.does_asset_exist(noise_path) else fallback_tex,
        -560, -340, sampler_type="color",
    )
    _connect_uv_coord(uv_mega, mega_noise, "M_Ground.MegaNoise")

    # Blend deep highland emerald meadow <-> sunlit golden-olive grass <-> subtle worn earth/stone path
    deep_emerald = _make_vec_param(mat, "MeadowDeep", (0.075, 0.215, 0.045, 1.0), -420, -480)
    sunlit_meadow = _make_vec_param(mat, "MeadowSunlit", (0.195, 0.305, 0.068, 1.0), -420, -380)
    meadow_mix = _make_lerp(mat, deep_emerald, "", sunlit_meadow, "", macro_noise, "R", -220, -400)

    earth_tint = _make_vec_param(mat, "EarthTint", (0.28, 0.22, 0.14, 1.0), -420, -20)
    earth_col = _make_mul(mat, detail_tex, "RGB", earth_tint, "", -240, 20)

    # Organic worn earth patches driven by macro/mega breakup (keeps meadow ~78% rich emerald-gold!)
    earth_raw = _make_mul(mat, macro_noise, "G", mega_noise, "R", -340, -180)
    earth_shifted = _make_add(mat, earth_raw, "", _make_const(mat, -0.22, -340, -100), "", -220, -160)
    earth_gain = _make_mul(mat, earth_shifted, "", _make_const(mat, 1.45, -220, -80), "", -100, -160)
    earth_mask, earth_pin = _make_clamp01(mat, earth_gain, "", 20, -160, "M_Ground.EarthMask")
    earth_soft = _make_mul(mat, earth_mask, earth_pin, _make_const(mat, 0.55, 20, -80), "", 140, -140)

    blended_ground = _make_lerp(mat, meadow_mix, "", earth_col, "", earth_soft, "", 240, -240)

    # Micro surface contrast from detail_tex + Wukong 0.78..1.18 macro anti-tiling modulation
    micro_contrast = _make_lerp(
        mat,
        _make_const(mat, 0.76, -160, 120), "",
        _make_const(mat, 1.18, -160, 180), "",
        detail_tex, "G",
        0, 140,
    )
    macro_mod = _make_lerp(
        mat,
        _make_const(mat, 0.78, -160, -280), "",
        _make_const(mat, 1.18, -160, -220), "",
        mega_noise, "G",
        40, -260,
    )
    lit_micro = _make_mul(mat, blended_ground, "", micro_contrast, "", 380, -140)
    final_col = _make_mul(mat, lit_micro, "", macro_mod, "", 520, -160)
    mel.connect_material_property(final_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if eal.does_asset_exist(nor_path):
        nor = _make_tex_param(mat, "NormalTex", nor_path, -600, 260, sampler_type="normal")
        _connect_uv_coord(uv_detail, nor, "M_Ground.NormalTex")
        mel.connect_material_property(nor, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # Velvety matte roughness 0.78..0.96 (eliminates glossy white specular wash on meadow!)
    rough_base = _make_lerp(
        mat,
        _make_const(mat, 0.80, -260, 440), "",
        _make_const(mat, 0.96, -260, 500), "",
        macro_noise, "B",
        -80, 460,
    )
    if eal.does_asset_exist(rough_path):
        rtex = _make_tex_param(mat, "RoughTex", rough_path, -600, 460, sampler_type="linear")
        _connect_uv_coord(uv_detail, rtex, "M_Ground.RoughTex")
        r_mod = _make_lerp(mat, _make_const(mat, 0.90, -260, 580), "", _make_const(mat, 1.04, -260, 640), "", rtex, "R", -80, 580)
        rough_val = _make_mul(mat, rough_base, "", r_mod, "", 100, 500)
        rough_c, rough_p = _make_clamp01(mat, rough_val, "", 240, 500, "M_Ground.RoughClamp")
        mel.connect_material_property(rough_c, rough_p, unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        mel.connect_material_property(rough_base, "", unreal.MaterialProperty.MP_ROUGHNESS)


def _wire_grass_tuft_graph(
    mat,
    root_rgba: tuple[float, float, float, float],
    tip_rgba: tuple[float, float, float, float],
    subsurf_rgba: tuple[float, float, float, float],
    wind_amp: float = 5.5,
    rough_val: float = 0.66,
):
    """TwoSidedFoliage grass/petal material with root->tip UV gradient, warm subsurface, and gentle breeze WPO."""
    mel = unreal.MaterialEditingLibrary
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    mat.set_editor_property("two_sided", True)

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -760, -80)
    cm_cls = getattr(unreal, "MaterialExpressionComponentMask", None)
    v_expr = None
    if cm_cls:
        v_expr = mel.create_material_expression(mat, cm_cls, -580, -80)
        v_expr.set_editor_property("r", False)
        v_expr.set_editor_property("g", True)
        v_expr.set_editor_property("b", False)
        v_expr.set_editor_property("a", False)
        _connect_unary(tc, "", v_expr, f"{mat.get_name()}.V")

    root_col = _make_vec_param(mat, "RootTint", root_rgba, -580, -240)
    tip_col = _make_vec_param(mat, "TipTint", tip_rgba, -580, 40)
    if v_expr is not None:
        base_col = _make_lerp(mat, root_col, "", tip_col, "", v_expr, "", -240, -100)
    else:
        base_col = tip_col
    mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_make_scalar_param(mat, "Roughness", rough_val, -240, 120), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_make_vec_param(mat, "SubsurfaceColor", subsurf_rgba, -240, 260), "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    # Gentle golden-hour meadow breeze World Position Offset (anchored at root V=0, swaying at tip V=1)
    time_cls = getattr(unreal, "MaterialExpressionTime", None)
    sine_cls = getattr(unreal, "MaterialExpressionSine", None)
    wpos_cls = getattr(unreal, "MaterialExpressionWorldPosition", None)
    append_cls = getattr(unreal, "MaterialExpressionAppendVector", None)
    if v_expr is not None and time_cls and sine_cls and append_cls:
        try:
            t_expr = mel.create_material_expression(mat, time_cls, -760, 460)
            phase = _make_mul(mat, t_expr, "", _make_const(mat, 0.42, -760, 560), "", -600, 480)
            if wpos_cls and cm_cls:
                wp = mel.create_material_expression(mat, wpos_cls, -920, 620)
                wp_x = mel.create_material_expression(mat, cm_cls, -760, 620)
                wp_x.set_editor_property("r", True)
                wp_x.set_editor_property("g", False)
                wp_x.set_editor_property("b", False)
                wp_x.set_editor_property("a", False)
                _connect_unary(wp, "", wp_x, f"{mat.get_name()}.WorldX")
                wp_phase = _make_mul(mat, wp_x, "", _make_const(mat, 0.0025, -760, 720), "", -600, 620)
                phase = _make_add(mat, phase, "", wp_phase, "", -460, 520)
            sway_sin = mel.create_material_expression(mat, sine_cls, -320, 520)
            _connect_unary(phase, "", sway_sin, f"{mat.get_name()}.WindSine")
            v_sq = _make_mul(mat, v_expr, "", v_expr, "", -440, 360)
            amp = _make_mul(mat, sway_sin, "", v_sq, "", -180, 480)
            dx = _make_mul(mat, amp, "", _make_const(mat, wind_amp, -180, 580), "", -40, 480)
            dy = _make_mul(mat, amp, "", _make_const(mat, wind_amp * 0.55, -180, 660), "", -40, 580)
            xy = mel.create_material_expression(mat, append_cls, 100, 500)
            mel.connect_material_expressions(dx, "", xy, "A")
            mel.connect_material_expressions(dy, "", xy, "B")
            xyz = mel.create_material_expression(mat, append_cls, 240, 520)
            mel.connect_material_expressions(xy, "", xyz, "A")
            mel.connect_material_expressions(_make_const(mat, 0.0, 100, 620), "", xyz, "B")
            mel.connect_material_property(xyz, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        except Exception as exc:
            _log(f"WARNING: Wind WPO skipped on {mat.get_name()} ({exc})")


def _wire_fire_tornado_fx(mat, wisp_tex_path: str, noise_tex_path: str):
    """Blazing panning molten orange-gold spiral flame shader (also tintable to icy-cyan for Wind!)."""
    mel = unreal.MaterialEditingLibrary
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1240, -120)
    panner_cls = getattr(unreal, "MaterialExpressionPanner", None)
    uv1 = tc
    uv2 = _make_mul(mat, tc, "", _make_const(mat, 1.65, -1240, 20), "", -1060, 20)
    if panner_cls:
        p1 = mel.create_material_expression(mat, panner_cls, -980, -180)
        p1.set_editor_property("speed_x", 0.55)
        p1.set_editor_property("speed_y", -1.65)
        mel.connect_material_expressions(tc, "", p1, "Coordinate")
        uv1 = p1

        p2 = mel.create_material_expression(mat, panner_cls, -880, 20)
        p2.set_editor_property("speed_x", -0.32)
        p2.set_editor_property("speed_y", -0.95)
        mel.connect_material_expressions(uv2, "", p2, "Coordinate")
        uv2 = p2

    wisp = _make_tex_param(mat, "Tex", wisp_tex_path, -720, -220, sampler_type="color")
    _connect_uv_coord(uv1, wisp, "M_FX_FireTornado.Tex")

    turb = _make_tex_param(mat, "NoiseTex", noise_tex_path, -720, 20, sampler_type="color")
    _connect_uv_coord(uv2, turb, "M_FX_FireTornado.NoiseTex")

    # Combine streaming wisp ribbons + turbulent fire noise
    turb_bias = _make_add(mat, turb, "R", _make_const(mat, 0.35, -540, 120), "", -460, 20)
    flame_raw = _make_mul(mat, wisp, "A", turb_bias, "", -340, -100)
    flame_mask, flame_pin = _make_clamp01(mat, _make_mul(mat, flame_raw, "", _make_const(mat, 1.85, -340, 0), "", -200, -100), "", -80, -100, "M_FX_FireTornado.Flame")

    # Dynamic color ramp: EdgeColor (crimson-orange) -> CoreColor (molten gold-white), tinted by Tint
    edge_col = _make_vec_param(mat, "EdgeColor", (1.0, 0.22, 0.02, 1.0), -540, -440)
    core_col = _make_vec_param(mat, "CoreColor", (1.0, 0.85, 0.30, 1.0), -540, -320)
    tint = _make_vec_param(mat, "Tint", (1.0, 1.0, 1.0, 1.0), -200, -360)
    em_scale = _make_scalar_param(mat, "EmissiveScale", 16.0, -200, -220)
    fade = _make_scalar_param(mat, "Fade", 1.0, -200, 260)

    ramp_col = _make_lerp(mat, edge_col, "", core_col, "", flame_mask, flame_pin, -40, -380)
    tinted_col = _make_mul(mat, ramp_col, "", tint, "", 100, -320)

    # Vertical top & bottom smooth V fade: clamp(V*5) * clamp((1-V)*3.5)
    v_fade_expr, v_fade_pin = flame_mask, flame_pin
    cm_cls = getattr(unreal, "MaterialExpressionComponentMask", None)
    om_cls = getattr(unreal, "MaterialExpressionOneMinus", None)
    if cm_cls and om_cls:
        v_ch = mel.create_material_expression(mat, cm_cls, -980, 240)
        v_ch.set_editor_property("r", False)
        v_ch.set_editor_property("g", True)
        v_ch.set_editor_property("b", False)
        v_ch.set_editor_property("a", False)
        _connect_unary(tc, "", v_ch, "M_FX_FireTornado.V")

        bot_fade, bot_pin = _make_clamp01(mat, _make_mul(mat, v_ch, "", _make_const(mat, 5.0, -840, 320), "", -720, 240), "", -580, 240, "M_FX_FireTornado.BotFade")
        inv_v = mel.create_material_expression(mat, om_cls, -840, 400)
        _connect_unary(v_ch, "", inv_v, "M_FX_FireTornado.OneMinusV")
        top_fade, top_pin = _make_clamp01(mat, _make_mul(mat, inv_v, "", _make_const(mat, 3.2, -700, 480), "", -580, 400), "", -440, 400, "M_FX_FireTornado.TopFade")
        v_env = _make_mul(mat, bot_fade, bot_pin, top_fade, top_pin, -300, 300)
        v_fade_expr = _make_mul(mat, flame_mask, flame_pin, v_env, "", -120, 120)
        v_fade_pin = ""

    # Fresnel glowing rim boost for volumetric tornado silhouette
    fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
    if fresnel_cls:
        try:
            fr = mel.create_material_expression(mat, fresnel_cls, -300, 460)
            fr.set_editor_property("exponent", 2.0)
            fr.set_editor_property("base_reflect_fraction", 0.15)
            fr_mul = _make_mul(mat, fr, "", _make_const(mat, 1.15, -180, 440), "", -80, 440)
            fr_boost = _make_add(mat, _make_const(mat, 0.55, -180, 520), "", fr_mul, "", 40, 440)
            v_fade_expr = _make_mul(mat, v_fade_expr, v_fade_pin, fr_boost, "", 160, 180)
            v_fade_pin = ""
        except Exception:
            pass

    # DepthFade so base merges cleanly into meadow ground
    df_cls = getattr(unreal, "MaterialExpressionDepthFade", None)
    if df_cls:
        df = mel.create_material_expression(mat, df_cls, 280, 180)
        for prop_name in ("fade_distance_default", "fade_distance"):
            try:
                df.set_editor_property(prop_name, 85.0)
                break
            except Exception:
                pass
        mel.connect_material_expressions(v_fade_expr, v_fade_pin, df, "Opacity")
        mel.connect_material_expressions(_make_const(mat, 85.0, 120, 300), "", df, "FadeDistance")
        v_fade_expr, v_fade_pin = df, ""

    mask_faded = _make_mul(mat, v_fade_expr, v_fade_pin, fade, "", 420, 160)
    em_col = _make_mul(mat, tinted_col, "", em_scale, "", 280, -220)
    em_final = _make_mul(mat, em_col, "", mask_faded, "", 540, -60)
    mel.connect_material_property(em_final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _wire_rune_circle_fx(mat):
    """Crisp glowing arcane rune circle driven by dynamic Color, EmissiveScale, and Fade."""
    mel = unreal.MaterialEditingLibrary
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    color = _make_vec_param(mat, "Color", (0.18, 1.0, 0.55, 1.0), -520, -120)
    em_scale = _make_scalar_param(mat, "EmissiveScale", 12.0, -520, 60)
    fade = _make_scalar_param(mat, "Fade", 1.0, -520, 180)

    em = _make_mul(mat, color, "", em_scale, "", -260, -40)
    # Subtle animated arcane pulse
    time_cls = getattr(unreal, "MaterialExpressionTime", None)
    sine_cls = getattr(unreal, "MaterialExpressionSine", None)
    if time_cls and sine_cls:
        try:
            t_e = mel.create_material_expression(mat, time_cls, -520, 320)
            s_e = mel.create_material_expression(mat, sine_cls, -360, 320)
            _connect_unary(_make_mul(mat, t_e, "", _make_const(mat, 0.72, -520, 400), "", -440, 320), "", s_e, "M_FX_RuneCircle.Pulse")
            pulse_amp = _make_mul(mat, s_e, "", _make_const(mat, 0.18, -240, 300), "", -180, 300)
            pulse = _make_add(mat, _make_const(mat, 0.88, -240, 400), "", pulse_amp, "", -60, 320)
            em = _make_mul(mat, em, "", pulse, "", -40, 20)
        except Exception:
            pass

    em_final = _make_mul(mat, em, "", fade, "", 140, 20)
    mel.connect_material_property(em_final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _wire_staff_crystal_fx(mat):
    """Faceted iridescent glowing staff crystal driven by dynamic ElementColor & EmissiveScale."""
    mel = unreal.MaterialEditingLibrary
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", False)

    elem_col = _make_vec_param(mat, "ElementColor", (1.0, 0.42, 0.08, 1.0), -620, -120)
    em_scale = _make_scalar_param(mat, "EmissiveScale", 9.0, -620, 80)

    mel.connect_material_property(elem_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_make_scalar_param(mat, "Roughness", 0.08, -320, -220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_make_scalar_param(mat, "Metallic", 0.22, -320, -120), "", unreal.MaterialProperty.MP_METALLIC)

    em_base = _make_mul(mat, elem_col, "", em_scale, "", -320, 40)
    fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
    if fresnel_cls:
        try:
            fr = mel.create_material_expression(mat, fresnel_cls, -560, 240)
            fr.set_editor_property("exponent", 2.4)
            fr.set_editor_property("base_reflect_fraction", 0.20)
            rim_mul = _make_mul(mat, fr, "", _make_const(mat, 1.55, -380, 220), "", -260, 240)
            rim_boost = _make_add(mat, _make_const(mat, 0.65, -380, 320), "", rim_mul, "", -120, 240)
            em_base = _make_mul(mat, em_base, "", rim_boost, "", 40, 100)
        except Exception:
            pass

    time_cls = getattr(unreal, "MaterialExpressionTime", None)
    sine_cls = getattr(unreal, "MaterialExpressionSine", None)
    if time_cls and sine_cls:
        try:
            t_e = mel.create_material_expression(mat, time_cls, -560, 440)
            s_e = mel.create_material_expression(mat, sine_cls, -380, 440)
            _connect_unary(_make_mul(mat, t_e, "", _make_const(mat, 0.55, -560, 520), "", -460, 440), "", s_e, "M_StaffCrystal.Pulse")
            pulse_mul = _make_mul(mat, s_e, "", _make_const(mat, 0.18, -220, 420), "", -120, 440)
            pulse = _make_add(mat, _make_const(mat, 0.88, -220, 520), "", pulse_mul, "", 40, 440)
            em_base = _make_mul(mat, em_base, "", pulse, "", 200, 180)
        except Exception:
            pass

    mel.connect_material_property(em_base, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def create_materials():
    """Construct complete UMaterial shader graphs driven by C++ dynamic parameters."""
    if not IN_EDITOR:
        return
    mel = unreal.MaterialEditingLibrary

    # ── 1. Environment materials ──────────────────────────────────────────────
    env_mat_path = f"{UE_WIZARD}/Environment/Materials"
    _ensure_dir(env_mat_path)

    env_tex_root = f"{UE_WIZARD}/Environment/Textures"
    pbr_env_specs = {
        "M_ArenaPlaza":    ("ruinstone", f"{env_tex_root}/T_Rock",      (0.84, 0.80, 0.72, 1.0), 0.90, 1.6, True),
        "M_Rock":          ("rock",      f"{env_tex_root}/T_Rock",      (0.56, 0.60, 0.52, 1.0), 0.96, 2.2, True),
        "M_RuinStone":     ("ruinstone", f"{env_tex_root}/T_Rock",      (0.76, 0.78, 0.70, 1.0), 0.94, 1.4, True),
        "M_Sandstone":     ("sandstone", f"{env_tex_root}/T_Sandstone", (0.94, 0.84, 0.72, 1.0), 0.92, 1.0, False),
        "M_Wood":          ("wood",      f"{env_tex_root}/T_Wood",      (0.88, 0.78, 0.68, 1.0), 0.90, 1.0, False),
        "M_Bark":          ("bark",      f"{env_tex_root}/T_Wood",      (0.72, 0.65, 0.56, 1.0), 0.98, 1.0, False),
        "M_WizardStaff":   ("bark",      f"{env_tex_root}/T_Wood",      (0.48, 0.34, 0.22, 1.0), 0.80, 1.6, False),
        "M_Piece_Stone":   ("ruinstone", f"{env_tex_root}/T_Rock",      (0.74, 0.76, 0.68, 1.0), 0.95, 1.2, True),
        "M_Piece_Rock":    ("rock",      f"{env_tex_root}/T_Rock",      (0.62, 0.64, 0.60, 1.0), 0.98, 1.0, False),
        "M_Piece_Wood":    ("wood",      f"{env_tex_root}/T_Wood",      (0.84, 0.74, 0.64, 1.0), 0.92, 1.0, False),
        "M_Piece_Pottery": ("sandstone", f"{env_tex_root}/T_Sandstone", (0.86, 0.50, 0.32, 1.0), 0.76, 1.0, False),
    }
    non_pbr_specs = {
        "M_Foliage":    (f"{env_tex_root}/T_DetailNoise", (0.18, 0.40, 0.14, 1.0), 0.68, True,  0.0, 0.0),
        "M_Crystal":    (f"{env_tex_root}/T_Crystal",     (0.22, 0.92, 1.00, 1.0), 0.12, False, 2.2, 0.15),
    }

    for mat_name in C.ENV_MATERIALS:
        mat = _load_or_create_material(mat_name, env_mat_path)
        if not mat:
            continue

        if mat_name == "M_Ground":
            _wire_meadow_ground_graph(mat)
        elif mat_name == "M_GrassBlade":
            _wire_grass_tuft_graph(
                mat,
                root_rgba=(0.045, 0.145, 0.030, 1.0),
                tip_rgba=(0.245, 0.465, 0.105, 1.0),
                subsurf_rgba=(0.24, 0.48, 0.09, 1.0),
                wind_amp=6.5,
                rough_val=0.66,
            )
        elif mat_name == "M_Foliage":
            _wire_foliage_graph(mat, (0.84, 0.94, 0.78, 1.0), (0.12, 0.38, 0.10, 1.0), rough_val=0.72)
        elif mat_name in pbr_env_specs:
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            pbr_key, fallback_tex, tint_rgba, rough_scale, uv_tile, use_macro = pbr_env_specs[mat_name]
            mat.set_editor_property("two_sided", False)
            _wire_pbr_graph(
                mat, pbr_key, fallback_tex, tint_rgba,
                rough_scale=rough_scale, uv_tile=uv_tile, macro_anti_tiling=use_macro,
            )
        else:
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            tex_path, tint_rgba, rough_val, two_sided, emissive_val, metallic_val = non_pbr_specs.get(
                mat_name, (f"{env_tex_root}/T_DetailNoise", (0.7, 0.7, 0.7, 1.0), 0.80, False, 0.0, 0.0)
            )
            mat.set_editor_property("two_sided", two_sided)
            tex = _make_tex_param(mat, "Tex", tex_path, -520, -120, sampler_type="color")
            tint = _make_vec_param(mat, "Tint", tint_rgba, -520, 80)
            base_col = _make_mul(mat, tex, "RGB", tint, "", -260, -20)
            mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

            rough = _make_scalar_param(mat, "Roughness", rough_val, -260, 140)
            mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

            if metallic_val > 0.0:
                metal = _make_scalar_param(mat, "Metallic", metallic_val, -260, 220)
                mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

            if mat_name == "M_Crystal":
                _wire_crystal_emissive(mat, base_col, emissive_val)
            elif emissive_val > 0.0:
                em_scale = _make_scalar_param(mat, "EmissiveScale", emissive_val, -260, 260)
                em_col = _make_mul(mat, base_col, "", em_scale, "", -80, 220)
                mel.connect_material_property(em_col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        _finalize_material(mat, use_ism=True)
        _log(f"Built material {mat_name}")

    # 1a. Colorful stylized wildflower petal materials (Gold, Blue, Pink, White)
    flower_petal_specs = {
        "M_Flower_Gold":  ((0.72, 0.44, 0.04, 1.0), (1.00, 0.80, 0.14, 1.0), (0.85, 0.62, 0.10, 1.0)),
        "M_Flower_Blue":  ((0.12, 0.28, 0.78, 1.0), (0.34, 0.66, 1.00, 1.0), (0.24, 0.52, 0.92, 1.0)),
        "M_Flower_Pink":  ((0.68, 0.14, 0.30, 1.0), (1.00, 0.40, 0.58, 1.0), (0.88, 0.28, 0.48, 1.0)),
        "M_Flower_White": ((0.76, 0.74, 0.62, 1.0), (1.00, 0.98, 0.90, 1.0), (0.90, 0.88, 0.76, 1.0)),
    }
    for fmat_name, (root_c, tip_c, sub_c) in flower_petal_specs.items():
        fmat = _load_or_create_material(fmat_name, env_mat_path)
        if fmat:
            _wire_grass_tuft_graph(fmat, root_c, tip_c, sub_c, wind_amp=5.2, rough_val=0.52)
            _finalize_material(fmat, use_ism=True)
            _log(f"Built wildflower petal material {fmat_name}")

    # 1b. Stylized wizard robe & trim materials (M_WizardRobe_Obsidian & M_WizardRobe_Gold)
    robe_obsidian = _load_or_create_material("M_WizardRobe_Obsidian", env_mat_path)
    if robe_obsidian:
        robe_obsidian.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        robe_obsidian.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        robe_obsidian.set_editor_property("two_sided", True)
        robe_obsidian.set_editor_property("used_with_skeletal_mesh", True)
        obs_tint = _make_vec_param(robe_obsidian, "Tint", (0.028, 0.036, 0.088, 1.0), -540, -100)
        noise_path = f"{env_tex_root}/T_DetailNoise"
        if unreal.EditorAssetLibrary.does_asset_exist(noise_path):
            tc_r = mel.create_material_expression(robe_obsidian, unreal.MaterialExpressionTextureCoordinate, -900, -160)
            uv_r = _make_mul(robe_obsidian, tc_r, "", _make_const(robe_obsidian, 6.0, -900, -60), "", -740, -160)
            weave = _make_tex_param(robe_obsidian, "WeaveNoise", noise_path, -560, -240, sampler_type="color")
            _connect_uv_coord(uv_r, weave, "M_WizardRobe_Obsidian.WeaveNoise")
            weave_mod = _make_lerp(robe_obsidian, _make_const(robe_obsidian, 0.82, -400, -320), "", _make_const(robe_obsidian, 1.18, -400, -240), "", weave, "R", -240, -240)
            base_c = _make_mul(robe_obsidian, obs_tint, "", weave_mod, "", -80, -140)
            mel.connect_material_property(base_c, "", unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            mel.connect_material_property(obs_tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(_make_scalar_param(robe_obsidian, "Roughness", 0.86, -460, 80), "", unreal.MaterialProperty.MP_ROUGHNESS)
        fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
        if fresnel_cls:
            try:
                fresnel = mel.create_material_expression(robe_obsidian, fresnel_cls, -460, 220)
                fresnel.set_editor_property("exponent", 3.0)
                fresnel.set_editor_property("base_reflect_fraction", 0.05)
                rim_col = _make_vec_param(robe_obsidian, "VelvetRimTint", (0.07, 0.09, 0.22, 1.0), -460, 360)
                rim_em = _make_mul(robe_obsidian, fresnel, "", rim_col, "", -200, 280)
                mel.connect_material_property(rim_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            except Exception as exc:
                _log(f"WARNING: Robe velvet rim skipped ({exc})")
        _finalize_material(robe_obsidian, use_ism=False)
        _log("Built material M_WizardRobe_Obsidian")

    robe_gold = _load_or_create_material("M_WizardRobe_Gold", env_mat_path)
    if robe_gold:
        robe_gold.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        robe_gold.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        robe_gold.set_editor_property("two_sided", True)
        robe_gold.set_editor_property("used_with_skeletal_mesh", True)
        gold_tint = _make_vec_param(robe_gold, "Tint", (0.76, 0.54, 0.16, 1.0), -460, -80)
        mel.connect_material_property(gold_tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(_make_scalar_param(robe_gold, "Metallic", 0.88, -460, 80), "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(_make_scalar_param(robe_gold, "Roughness", 0.30, -460, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
        _finalize_material(robe_gold, use_ism=False)
        _log("Built material M_WizardRobe_Gold")

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
        if is_add:
            radial_mask = _wire_soft_radial_mask(mat, -1100, 360)
            if radial_mask is not None:
                alpha = _make_mul(mat, alpha, "", radial_mask, "", -240, 220)

        (rgb_expr, rgb_pin), (alpha_expr, alpha_pin) = _wire_per_instance_rgb_alpha(
            mat, rgb, "", alpha, ""
        )
        if is_add:
            emissive = _make_mul(mat, rgb_expr, rgb_pin, alpha_expr, alpha_pin, -40, 20)
            mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:
            mel.connect_material_property(rgb_expr, rgb_pin, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(alpha_expr, alpha_pin, unreal.MaterialProperty.MP_OPACITY)

        _finalize_material(mat, use_ism=True)
        _log(f"Built FX sprite master {mat_name}")

    # 2b. FX mesh materials (Tint, EmissiveScale, Fade, ElementColor, Color, CoreColor, EdgeColor)
    for mat_name in C.FX_MESH_MATERIALS:
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue

        if mat_name == "M_FX_Core":
            # Soft center-hot additive starburst plasma (ZERO hard white plastic dome silhouette!)
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("two_sided", False)
            tint = _make_vec_param(mat, "Tint", (1.0, 0.55, 0.18, 1.0), -620, -100)
            em_scale = _make_scalar_param(mat, "EmissiveScale", 8.0, -620, 60)
            fade = _make_scalar_param(mat, "Fade", 1.0, -620, 180)
            core_em = _make_mul(mat, tint, "", em_scale, "", -340, -20)
            fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
            om_cls = getattr(unreal, "MaterialExpressionOneMinus", None)
            if fresnel_cls and om_cls:
                try:
                    fr = mel.create_material_expression(mat, fresnel_cls, -620, 320)
                    fr.set_editor_property("exponent", 1.6)
                    fr.set_editor_property("base_reflect_fraction", 0.0)
                    inv_fr = mel.create_material_expression(mat, om_cls, -440, 320)
                    _connect_unary(fr, "", inv_fr, "M_FX_Core.OneMinusFresnel")
                    soft_sq = _make_mul(mat, inv_fr, "", inv_fr, "", -280, 320)
                    soft_cube = _make_mul(mat, soft_sq, "", inv_fr, "", -140, 320)
                    core_em = _make_mul(mat, core_em, "", soft_cube, "", 20, 60)
                except Exception:
                    pass
            em_final = _make_mul(mat, core_em, "", fade, "", 180, 100)
            mel.connect_material_property(em_final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        elif mat_name == "M_FX_Bolt":
            # Crimson Desert blinding electric white-blue core + vivid cyan-indigo Fresnel outer plasma halo!
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("two_sided", True)
            tint = _make_vec_param(mat, "Tint", (0.28, 0.62, 1.0, 1.0), -640, -120)
            em_scale = _make_scalar_param(mat, "EmissiveScale", 14.0, -640, 60)
            fade = _make_scalar_param(mat, "Fade", 1.0, -640, 180)
            white_core = _make_const3(mat, 1.4, 1.65, 2.0, -640, -260)
            halo_col = _make_mul(mat, tint, "", _make_const3(mat, 0.35, 0.68, 1.45, -640, -380), "", -420, -200)
            fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
            if fresnel_cls:
                try:
                    fr = mel.create_material_expression(mat, fresnel_cls, -480, 280)
                    fr.set_editor_property("exponent", 1.8)
                    fr.set_editor_property("base_reflect_fraction", 0.04)
                    bolt_col = _make_lerp(mat, white_core, "", halo_col, "", fr, "", -220, -120)
                except Exception:
                    bolt_col = _make_add(mat, white_core, "", halo_col, "", -220, -120)
            else:
                bolt_col = _make_add(mat, white_core, "", halo_col, "", -220, -120)
            em_scaled = _make_mul(mat, bolt_col, "", em_scale, "", -40, -40)
            em = _make_mul(mat, em_scaled, "", fade, "", 120, 40)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        elif mat_name == "M_FX_Ring":
            _wire_wind_wisp_fx(
                mat,
                f"{fx_tex_root}/T_FX_WindWisp",
                (0.36, 0.78, 1.0, 1.0),
                default_em=7.5,
                pan_x=0.65,
                pan_y=0.18,
                fade_dist=70.0,
                ribbon_v_mask=False,
                is_additive=True,
                square_wisp_alpha=True,
            )
        elif mat_name == "M_FX_WindRibbon":
            _wire_wind_wisp_fx(
                mat,
                f"{fx_tex_root}/T_FX_WindWisp",
                (0.75, 0.96, 1.0, 1.0),
                default_em=4.8,
                pan_x=1.25,
                pan_y=0.0,
                fade_dist=75.0,
                ribbon_v_mask=True,
                is_additive=True,
                opacity_scale=0.82,
            )
        elif mat_name == "M_FX_FireTornado":
            _wire_fire_tornado_fx(
                mat,
                f"{fx_tex_root}/T_FX_WindWisp",
                f"{env_tex_root}/T_DetailNoise",
            )
        elif mat_name == "M_FX_RuneCircle":
            _wire_rune_circle_fx(mat)
        elif mat_name == "M_StaffCrystal":
            _wire_staff_crystal_fx(mat)
        elif mat_name == "M_FX_Pillar":
            # Dark volcanic basalt & ancient emerald moss with subtle glowing fracture rim
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            pbr_tint = (0.34, 0.36, 0.32, 1.0)
            _wire_pbr_graph(mat, "rock", f"{env_tex_root}/T_Rock", pbr_tint, rough_scale=0.92, uv_tile=1.8, macro_anti_tiling=True)
            em_tint = _make_vec_param(mat, "EmissiveTint", (0.28, 0.72, 0.34, 1.0), -520, 740)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 860)
            em_fade = _make_mul(mat, em_tint, "", fade, "", -240, 780)
            fresnel_cls = getattr(unreal, "MaterialExpressionFresnel", None)
            if fresnel_cls:
                try:
                    fr = mel.create_material_expression(mat, fresnel_cls, -360, 960)
                    fr.set_editor_property("exponent", 3.5)
                    fr.set_editor_property("base_reflect_fraction", 0.02)
                    em = _make_mul(mat, em_fade, "", _make_mul(mat, fr, "", _make_const(mat, 0.85, -220, 1020), "", -120, 960), "", 40, 840)
                except Exception:
                    em = _make_mul(mat, em_fade, "", _make_const(mat, 0.18, -240, 900), "", -100, 800)
            else:
                em = _make_mul(mat, em_fade, "", _make_const(mat, 0.18, -240, 900), "", -100, 800)
            mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:  # M_FX_Crystal
            mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
            tex = _make_tex_param(mat, "Tex", f"{env_tex_root}/T_Crystal", -520, -120, sampler_type="color")
            tint = _make_vec_param(mat, "Tint", (0.22, 0.92, 1.0, 1.0), -520, 80)
            fade = _make_scalar_param(mat, "Fade", 1.0, -520, 320)
            base_col = _make_mul(mat, tex, "RGB", tint, "", -260, -20)
            mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(_make_scalar_param(mat, "Roughness", 0.12, -260, 120), "", unreal.MaterialProperty.MP_ROUGHNESS)
            mel.connect_material_property(_make_scalar_param(mat, "Metallic", 0.15, -260, 200), "", unreal.MaterialProperty.MP_METALLIC)
            _wire_crystal_emissive(mat, base_col, 4.5, fade_expr=fade)

        _finalize_material(mat, use_ism=(mat_name != "M_StaffCrystal"))
        _log(f"Built FX mesh material {mat_name}")

    # 2c. FX debris chip materials
    chip_specs = {
        "Stone":   ("ruinstone", f"{env_tex_root}/T_Rock",        (0.76, 0.74, 0.70, 1.0), 0.94, False),
        "Wood":    ("wood",      f"{env_tex_root}/T_Wood",        (0.72, 0.56, 0.40, 1.0), 0.90, False),
        "Pottery": ("sandstone", f"{env_tex_root}/T_Sandstone",   (0.86, 0.52, 0.34, 1.0), 0.76, False),
        "Rock":    ("rock",      f"{env_tex_root}/T_Rock",        (0.68, 0.68, 0.70, 1.0), 0.96, False),
        "Crystal": (None,        f"{env_tex_root}/T_Crystal",     (0.24, 0.92, 1.00, 1.0), 0.12, False),
        "Foliage": (None,        f"{env_tex_root}/T_DetailNoise", (0.20, 0.44, 0.16, 1.0), 0.68, True),
    }
    for mat_family in C.MATERIALS:
        mat_name = f"M_FX_Chip_{mat_family}"
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue
        pbr_key, tex_path, rgba, rough_val, two_sided = chip_specs[mat_family]
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        mat.set_editor_property("two_sided", two_sided)
        if pbr_key:
            _wire_pbr_graph(mat, pbr_key, tex_path, rgba, rough_scale=rough_val, uv_tile=1.0)
        else:
            tex = _make_tex_param(mat, "Tex", tex_path, -480, -80, sampler_type="color")
            tint = _make_vec_param(mat, "Tint", rgba, -480, 100)
            base_col = _make_mul(mat, tex, "RGB", tint, "", -220, 0)
            mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(_make_scalar_param(mat, "Roughness", rough_val, -220, 140), "", unreal.MaterialProperty.MP_ROUGHNESS)
            if mat_family == "Crystal":
                mel.connect_material_property(_make_scalar_param(mat, "Metallic", 0.15, -220, 220), "", unreal.MaterialProperty.MP_METALLIC)
                mel.connect_material_property(_make_mul(mat, base_col, "", _make_const(mat, 3.0, -220, 300), "", -60, 240), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        _finalize_material(mat, use_ism=True)
        _log(f"Built FX chip material {mat_name}")

    # 2d. Decal materials (use_ism=False for Deferred Decal domain)
    for mat_name in C.FX_DECALS:
        mat = _load_or_create_material(mat_name, fx_mat_path)
        if not mat:
            continue
        mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        fade = _make_scalar_param(mat, "Fade", 1.0, -520, 320)

        if mat_name == "M_Decal_Scorch":
            # Feathered circular charred crater + glowing molten ember center (ZERO hard black square!)
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Smoke", -660, -140)
            tint = _make_vec_param(mat, "Tint", (0.032, 0.024, 0.020, 0.88), -660, 60)
            radial = _wire_soft_radial_mask(mat, -1080, 260)
            char_col = _make_mul(mat, tex, "RGB", tint, "", -260, -100)
            mel.connect_material_property(char_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(_make_const(mat, 0.94, -260, 0), "", unreal.MaterialProperty.MP_ROUGHNESS)

            if radial is not None:
                # Feather alpha smoothly to 0.0 well inside the decal bounds & break up edge with smoke noise
                rad_sq = _make_mul(mat, radial, "", radial, "", -240, 260)
                noise_mod = _make_lerp(mat, _make_const(mat, 0.55, -420, 140), "", _make_const(mat, 1.15, -420, 200), "", tex, "R", -260, 140)
                op_raw = _make_mul(mat, rad_sq, "", noise_mod, "", -80, 220)
                op_c, op_p = _make_clamp01(mat, op_raw, "", 60, 220, "M_Decal_Scorch.OpClamp")
                op_tint = _make_mul(mat, op_c, op_p, tint, "A", 180, 220)
                op_final = _make_mul(mat, op_tint, "", fade, "", 300, 220)
                mel.connect_material_property(op_final, "", unreal.MaterialProperty.MP_OPACITY)

                # Glowing molten orange-red ember crater core at center (pow(radial, 4))
                rad_4 = _make_mul(mat, rad_sq, "", rad_sq, "", -60, 380)
                ember_mask = _make_mul(mat, rad_4, "", tex, "R", 80, 380)
                ember_col = _make_const3(mat, 1.85, 0.40, 0.04, -60, 480)
                ember_em = _make_mul(mat, ember_col, "", _make_mul(mat, ember_mask, "", _make_const(mat, 5.5, 80, 480), "", 200, 400), "", 340, 420)
                ember_faded = _make_mul(mat, ember_em, "", fade, "", 480, 420)
                mel.connect_material_property(ember_faded, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            else:
                op_a = _make_mul(mat, tex, "A", tint, "A", -240, 140)
                mel.connect_material_property(_make_mul(mat, op_a, "", fade, "", -80, 160), "", unreal.MaterialProperty.MP_OPACITY)
        elif mat_name == "M_Decal_Crack":
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Dust", -620, -80)
            tint = _make_vec_param(mat, "Tint", (0.10, 0.09, 0.07, 0.85), -620, 100)
            radial = _wire_soft_radial_mask(mat, -1040, 260)
            col = _make_mul(mat, tex, "RGB", tint, "", -220, -20)
            op_a = _make_mul(mat, tex, "A", tint, "A", -240, 140)
            if radial is not None:
                op_a = _make_mul(mat, op_a, "", radial, "", -120, 180)
            op = _make_mul(mat, op_a, "", fade, "", 40, 180)
            mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        else:
            elem = mat_name.replace("M_Decal_Rune_", "")
            ec = C.ELEMENT_COLORS_F.get(elem, (1.0, 1.0, 1.0))
            tex = _make_tex_param(mat, "Tex", f"{fx_tex_root}/T_FX_Rune", -480, -80)
            tint = _make_vec_param(mat, "Tint", (ec[0], ec[1], ec[2], 1.0), -480, 100)
            col = _make_mul(mat, tex, "RGB", tint, "", -220, -20)
            op_a = _make_mul(mat, tex, "A", tint, "A", -240, 140)
            op = _make_mul(mat, op_a, "", fade, "", -80, 160)
            mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
            mel.connect_material_property(_make_mul(mat, col, "", _make_const(mat, 4.0, -220, -140), "", -60, -80), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

        _finalize_material(mat, use_ism=False)
        _log(f"Built decal material {mat_name}")

    # ── 3. UI materials (use_ism=False for UI domain) ─────────────────────────
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

        _finalize_material(mat, use_ism=False)
        _log(f"Built UI material {mat_name}")


def assign_mesh_materials():
    """Assign ENV_MATERIALS to imported environment and destructible StaticMeshes."""
    if not IN_EDITOR:
        return

    env_mat_path = f"{UE_WIZARD}/Environment/Materials"
    fx_mat_path = f"{UE_WIZARD}/FX/Materials"
    env_mesh_path = f"{UE_WIZARD}/Environment/Meshes"
    fx_mesh_path = f"{UE_WIZARD}/FX/Meshes"
    dest_mesh_path = f"{UE_WIZARD}/Destruction/Meshes"

    mat_cache = {}
    def _get_env_mat(name: str):
        key = f"env:{name}"
        if key not in mat_cache:
            mat_cache[key] = unreal.EditorAssetLibrary.load_asset(f"{env_mat_path}/{name}")
        return mat_cache[key]

    def _get_fx_mat(name: str):
        key = f"fx:{name}"
        if key not in mat_cache:
            mat_cache[key] = unreal.EditorAssetLibrary.load_asset(f"{fx_mat_path}/{name}")
        return mat_cache[key]

    def _assign_named_slots(mesh, rules: list[tuple[tuple[str, ...], str, object]], default_pair):
        """Map StaticMesh material slots by imported slot name with deterministic fallback."""
        if not mesh:
            return False
        try:
            static_mats = list(mesh.get_editor_property("static_materials"))
        except Exception:
            static_mats = []
        if not static_mats:
            if default_pair[1]:
                mesh.set_material(0, default_pair[1])
                unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            return True
        for slot_idx, smat in enumerate(static_mats):
            slot_names = []
            for prop in ("material_slot_name", "imported_material_slot_name"):
                try:
                    slot_names.append(str(smat.get_editor_property(prop)).lower())
                except Exception:
                    pass
            joined = " ".join(slot_names)
            chosen_name, chosen_mat = default_pair
            matched = False
            for kws, mname, mobj in rules:
                if any(kw in joined for kw in kws):
                    chosen_name, chosen_mat = mname, mobj
                    matched = True
                    break
            if not matched and slot_idx < len(rules):
                _, chosen_name, chosen_mat = rules[slot_idx]
            if chosen_mat:
                mesh.set_material(slot_idx, chosen_mat)
                _log(f"  Slot map {mesh.get_name()}[{slot_idx}] ({joined.strip()}) -> {chosen_name}")
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        return True

    def _assign_foliage_mesh_slots(mesh):
        bark_mat = _get_env_mat("M_Bark")
        foliage_mat = _get_env_mat("M_Foliage")
        if not mesh or not (bark_mat or foliage_mat):
            return False
        return _assign_named_slots(
            mesh,
            [
                (("bark", "trunk", "wood"), "M_Bark", bark_mat),
                (("foliage", "leaf", "leaves"), "M_Foliage", foliage_mat),
            ],
            ("M_Bark", bark_mat),
        )

    env_mesh_mats = {
        "SM_Terrain":         "M_Ground",
        "SM_ArenaPlaza":      "M_ArenaPlaza",
        "SM_CliffRing":       "M_Rock",
        "SM_GrassBlade":      "M_GrassBlade",
        "SM_SmallStone":      "M_Rock",
        "SM_BigRock_A_Whole": "M_Rock",
        "SM_BigRock_B_Whole": "M_Rock",
        "SM_SmRock_A_Whole":  "M_Rock",
        "SM_SmRock_B_Whole":  "M_Rock",
        "SM_RuinColumn":      "M_RuinStone",
        "SM_RuinWall":        "M_RuinStone",
        "SM_RuinArch":        "M_RuinStone",
    }
    for mesh_name, mat_name in env_mesh_mats.items():
        mesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/{mesh_name}")
        mat = _get_env_mat(mat_name)
        if mesh and mat:
            mesh.set_material(0, mat)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    # Multi-material hero & meadow assets
    robe_obs = _get_env_mat("M_WizardRobe_Obsidian")
    robe_gld = _get_env_mat("M_WizardRobe_Gold")
    for wizard_mesh_name in ("SM_WizardHat", "SM_WizardCloak"):
        wmesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/{wizard_mesh_name}")
        if wmesh:
            _assign_named_slots(
                wmesh,
                [
                    (("cloth", "robe", "fabric"), "M_WizardRobe_Obsidian", robe_obs),
                    (("gold", "trim", "buckle", "metal"), "M_WizardRobe_Gold", robe_gld),
                ],
                ("M_WizardRobe_Obsidian", robe_obs),
            )

    staff_mesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/SM_WizardStaff")
    if staff_mesh:
        _assign_named_slots(
            staff_mesh,
            [
                (("wood", "bark", "shaft"), "M_WizardStaff", _get_env_mat("M_WizardStaff")),
                (("crystal", "gem", "orb"), "M_StaffCrystal", _get_fx_mat("M_StaffCrystal")),
            ],
            ("M_WizardStaff", _get_env_mat("M_WizardStaff")),
        )

    flower_mesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/SM_Flower")
    if flower_mesh:
        _assign_named_slots(
            flower_mesh,
            [
                (("stem", "leaf", "grass"), "M_GrassBlade", _get_env_mat("M_GrassBlade")),
                (("petal", "flower", "blossom"), "M_Flower_Gold", _get_env_mat("M_Flower_Gold")),
            ],
            ("M_GrassBlade", _get_env_mat("M_GrassBlade")),
        )

    # Sculpted FX meshes default materials
    fx_mesh_mats = {
        "SM_FX_FireTornado": "M_FX_FireTornado",
        "SM_RuneCircle":     "M_FX_RuneCircle",
        "SM_FX_Pillar":      "M_FX_Pillar",
    }
    for fx_mname, fx_matname in fx_mesh_mats.items():
        fxm = unreal.EditorAssetLibrary.load_asset(f"{fx_mesh_path}/{fx_mname}")
        fmat = _get_fx_mat(fx_matname)
        if fxm and fmat:
            fxm.set_material(0, fmat)
            unreal.EditorAssetLibrary.save_loaded_asset(fxm)

    for foliage_env_name in ("SM_Bush", "SM_Tree_A_Whole"):
        fmesh = unreal.EditorAssetLibrary.load_asset(f"{env_mesh_path}/{foliage_env_name}")
        if fmesh:
            _assign_foliage_mesh_slots(fmesh)

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
        if whole_mesh:
            if material == "Foliage":
                if _assign_foliage_mesh_slots(whole_mesh):
                    assigned += 1
            elif whole_mat:
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
                    rotation=unreal.Rotator(roll=float(tx[3]), pitch=float(tx[4]), yaw=float(tx[5]))
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
    "PostProcess", "PlayerStart", "Terrain", "CliffRing", "ArenaPlaza", "ScatterVegetation",
}
_FORGE_LABEL_PREFIXES = (
    "SM_RuinColumn_", "SM_RuinWall_", "SM_RuinArch_", "Struct_", "Breakables_", "Crystal_",
    "PlazaCrystalLight_", "PerimeterTorch_",
)


def _get_terrain_z_fn():
    """Return exact terrain bowl height function matching SM_Terrain (with smooth fallback)."""
    try:
        import offline_forge as _of
        pads = _of._build_structure_pads()
        return lambda x, y: float(_of.terrain_bowl_z(x, y, pads=pads))
    except Exception:
        return lambda x, y: -60.0 * ((math.hypot(x, y) / float(C.BOWL_RADIUS)) ** 2)


def _find_mesh_slot_index(mesh, keywords: tuple[str, ...], fallback_idx: int = 1) -> int:
    try:
        static_mats = list(mesh.get_editor_property("static_materials"))
        for idx, smat in enumerate(static_mats):
            names = []
            for prop in ("material_slot_name", "imported_material_slot_name"):
                try:
                    names.append(str(smat.get_editor_property(prop)).lower())
                except Exception:
                    pass
            joined = " ".join(names)
            if any(kw in joined for kw in keywords):
                return idx
    except Exception:
        pass
    return fallback_idx


def _try_set_prop(obj, prop_names, value, label: str):
    """Set first supported property name on UE object and log loudly if none succeed."""
    if isinstance(prop_names, str):
        prop_names = [prop_names]
    last_exc = None
    for pname in prop_names:
        try:
            obj.set_editor_property(pname, value)
            return True
        except Exception as exc:
            last_exc = exc
    _log(f"ERROR: Failed setting {prop_names} on {label}: {last_exc}")
    return False


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

    # ── Lighting & atmosphere (Black Myth: Wukong / Crimson Desert Golden Hour) ─
    _log("Placing Wukong golden-hour volumetric lighting & atmosphere...")
    terrain_z = _get_terrain_z_fn()

    # Low late-afternoon warm golden DirectionalLight with dramatic sun shafts & crisp shadows
    sun = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 0)))
    sun.set_actor_label("Sun")
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-16.5, yaw=-132.0), False)
    light_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if light_comp:
        _try_set_prop(light_comp, "intensity", 10.5, "Sun.intensity")
        _try_set_prop(light_comp, "light_color", unreal.Color(255, 208, 145), "Sun.light_color")
        _try_set_prop(
            light_comp,
            ["atmosphere_sun_light", "used_as_atmosphere_sun_light"],
            True,
            "Sun.atmosphere_sun_light",
        )
        _try_set_prop(light_comp, "volumetric_scattering_intensity", 3.2, "Sun.volumetric_scattering")
        _try_set_prop(light_comp, ["enable_light_shaft_occlusion", "b_enable_light_shaft_occlusion"], True, "Sun.shaft_occ")
        _try_set_prop(light_comp, ["enable_light_shaft_bloom", "b_enable_light_shaft_bloom"], True, "Sun.shaft_bloom")
        _try_set_prop(light_comp, "bloom_scale", 0.28, "Sun.bloom_scale")
        _try_set_prop(light_comp, "bloom_threshold", 1.15, "Sun.bloom_threshold")
        _try_set_prop(light_comp, "bloom_tint", unreal.Color(255, 214, 152), "Sun.bloom_tint")
        _try_set_prop(light_comp, "dynamic_shadow_distance_movable_light", 12000.0, "Sun.csm_dist")

    # Sky light (real-time capture + warm sunlit bounce to preserve architectural shadow detail)
    sky_light = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, 500)))
    sky_light.set_actor_label("SkyLight")
    sky_comp = sky_light.get_component_by_class(unreal.SkyLightComponent)
    if sky_comp:
        _try_set_prop(sky_comp, "real_time_capture", True, "SkyLight.real_time_capture")
        _try_set_prop(sky_comp, "intensity", 1.35, "SkyLight.intensity")
        _try_set_prop(
            sky_comp,
            ["lower_hemisphere_is_black", "b_lower_hemisphere_is_black"],
            False,
            "SkyLight.lower_hemisphere_is_black",
        )
        _try_set_prop(
            sky_comp,
            "lower_hemisphere_color",
            unreal.LinearColor(0.30, 0.24, 0.16, 1.0),
            "SkyLight.lower_hemisphere_color",
        )

    # Sky atmosphere
    sky_atmo = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)))
    sky_atmo.set_actor_label("SkyAtmosphere")

    # Volumetric cloud
    vol_cloud = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.VolumetricCloud, unreal.Vector(0, 0, 0)))
    vol_cloud.set_actor_label("VolumetricCloud")

    # Exponential height fog with volumetric scattering & soft golden-blue distant valley haze
    fog = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.ExponentialHeightFog, unreal.Vector(0, 0, 120)))
    fog.set_actor_label("HeightFog")
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fog_comp:
        _try_set_prop(fog_comp, "fog_density", 0.0085, "HeightFog.fog_density")
        _try_set_prop(fog_comp, "fog_height_falloff", 0.22, "HeightFog.fog_height_falloff")
        _try_set_prop(
            fog_comp,
            "fog_inscattering_luminance",
            unreal.LinearColor(0.56, 0.66, 0.84, 1.0),
            "HeightFog.fog_inscattering_luminance",
        )
        _try_set_prop(
            fog_comp,
            "directional_inscattering_luminance",
            unreal.LinearColor(1.0, 0.72, 0.36, 1.0),
            "HeightFog.directional_inscattering_luminance",
        )
        _try_set_prop(fog_comp, "directional_inscattering_exponent", 4.0, "HeightFog.dir_inscattering_exp")
        if hasattr(fog_comp, "set_volumetric_fog"):
            try:
                fog_comp.set_volumetric_fog(True)
            except Exception:
                pass
        else:
            for vf_prop in ("enable_volumetric_fog", "b_enable_volumetric_fog"):
                try:
                    fog_comp.set_editor_property(vf_prop, True)
                    break
                except Exception:
                    pass
        _try_set_prop(fog_comp, "volumetric_fog_scattering_distribution", 0.72, "HeightFog.vf_dist")
        _try_set_prop(fog_comp, "volumetric_fog_extinction_scale", 1.15, "HeightFog.vf_ext")
        _try_set_prop(fog_comp, "volumetric_fog_distance", 6000.0, "HeightFog.vf_range")

    # Post process volume (unbound): Lumen GI, Wukong ACES filmic contrast & glowing magic bloom
    ppv = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0, 0, 0)))
    ppv.set_actor_label("PostProcess")
    ppv.set_editor_property("unbound", True)
    try:
        pp_settings = ppv.get_editor_property("settings")
        gi_enum = getattr(unreal, "DynamicGlobalIlluminationMethod", None)
        if gi_enum and hasattr(gi_enum, "LUMEN"):
            _try_set_prop(pp_settings, "override_dynamic_global_illumination_method", True, "PPV.override_gi")
            _try_set_prop(pp_settings, "dynamic_global_illumination_method", gi_enum.LUMEN, "PPV.gi_lumen")
        _try_set_prop(pp_settings, "override_auto_exposure_min_brightness", True, "PPV.override_ae_min")
        _try_set_prop(pp_settings, "auto_exposure_min_brightness", 0.28, "PPV.ae_min")
        _try_set_prop(pp_settings, "override_auto_exposure_max_brightness", True, "PPV.override_ae_max")
        _try_set_prop(pp_settings, "auto_exposure_max_brightness", 2.0, "PPV.ae_max")
        _try_set_prop(pp_settings, "override_auto_exposure_bias", True, "PPV.override_ae_bias")
        _try_set_prop(pp_settings, "auto_exposure_bias", 0.22, "PPV.ae_bias")
        _try_set_prop(pp_settings, "override_bloom_intensity", True, "PPV.override_bloom_intensity")
        _try_set_prop(pp_settings, "bloom_intensity", 0.52, "PPV.bloom_intensity")
        _try_set_prop(pp_settings, "override_bloom_threshold", True, "PPV.override_bloom_threshold")
        _try_set_prop(pp_settings, "bloom_threshold", 0.95, "PPV.bloom_threshold")
        _try_set_prop(pp_settings, "override_film_slope", True, "PPV.override_film_slope")
        _try_set_prop(pp_settings, "film_slope", 0.88, "PPV.film_slope")
        _try_set_prop(pp_settings, "override_film_toe", True, "PPV.override_film_toe")
        _try_set_prop(pp_settings, "film_toe", 0.48, "PPV.film_toe")
        _try_set_prop(pp_settings, "override_vignette_intensity", True, "PPV.override_vignette")
        _try_set_prop(pp_settings, "vignette_intensity", 0.28, "PPV.vignette")
        ppv.set_editor_property("settings", pp_settings)
    except Exception as exc:
        _log(f"ERROR: Failed configuring PostProcessVolume settings: {exc}")

    # Warm amber perimeter torch/brazier fill lights for CliffRing & outer ruins backdrop
    for torch_idx in range(8):
        ang = (2.0 * math.pi * torch_idx / 8.0) + (math.pi / 8.0)
        tx = 3050.0 * math.cos(ang)
        ty = 3050.0 * math.sin(ang)
        pl = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PointLight, unreal.Vector(tx, ty, 340.0)))
        if pl:
            pl.set_actor_label(f"PerimeterTorch_{torch_idx:02d}")
            plc = pl.get_component_by_class(unreal.PointLightComponent)
            if plc:
                if hasattr(unreal, "LightUnits"):
                    _try_set_prop(plc, "intensity_units", unreal.LightUnits.LUMENS, f"PerimeterTorch_{torch_idx:02d}.units")
                _try_set_prop(plc, "intensity", 2400.0, f"PerimeterTorch_{torch_idx:02d}.intensity")
                _try_set_prop(plc, "attenuation_radius", 950.0, f"PerimeterTorch_{torch_idx:02d}.radius")
                _try_set_prop(plc, "source_radius", 28.0, f"PerimeterTorch_{torch_idx:02d}.source_radius")
                _try_set_prop(plc, "light_color", unreal.Color(255, 175, 85), f"PerimeterTorch_{torch_idx:02d}.color")
                _try_set_prop(plc, "cast_shadows", False, f"PerimeterTorch_{torch_idx:02d}.cast_shadows")

    # Player start (hero framing at south courtyard facing +Y across plaza)
    ps_cfg = layout.get("player_start", {})
    ps = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(ps_cfg.get("x", 0.0), ps_cfg.get("y", -900.0), ps_cfg.get("z", 125.0)),
    ))
    ps.set_actor_label("PlayerStart")
    ps.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(ps_cfg.get("yaw", 90.0))), False)

    # Set WorldSettings.default_game_mode = AWizardGameMode safely
    try:
        ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem) if hasattr(unreal, "get_editor_subsystem") else None
        world = ues.get_editor_world() if ues else unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings() if world else None
        gm_cls = unreal.load_class(None, "/Script/WizardEPIC.WizardGameMode")
        if ws and gm_cls:
            _try_set_prop(ws, "default_game_mode", gm_cls, "WorldSettings.default_game_mode")
            _log("Configured WorldSettings.default_game_mode = AWizardGameMode")
        else:
            _log(f"WARNING: WorldSettings or WizardGameMode unavailable (ws={ws}, gm={gm_cls})")
    except Exception as exc:
        _log(f"WARNING: Could not set WorldSettings.default_game_mode: {exc}")

    # ── Terrain & Plaza ───────────────────────────────────────────────────────
    _log("Placing terrain, cliffs & stepped arena plaza...")
    terrain_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_Terrain")
    cliff_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_CliffRing")
    plaza_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_ArenaPlaza")

    if terrain_mesh:
        terrain_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0)))
        terrain_actor.set_actor_label("Terrain")
        terrain_actor.static_mesh_component.set_static_mesh(terrain_mesh)

    if cliff_mesh:
        # Sink cliff skirt below terrain bowl rim (-129cm) to eliminate any horizon sky seam!
        cliff_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, -145.0)))
        cliff_actor.set_actor_label("CliffRing")
        cliff_actor.static_mesh_component.set_static_mesh(cliff_mesh)

    plaza_cfg = layout.get("arena_plaza", {"x": 0.0, "y": 0.0, "z": 0.0, "yaw": 0.0})
    plaza_z = float(plaza_cfg.get("z", 0.0))
    if plaza_mesh:
        plaza_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(plaza_cfg.get("x", 0.0), plaza_cfg.get("y", 0.0), plaza_z),
        ))
        plaza_actor.set_actor_label("ArenaPlaza")
        plaza_actor.static_mesh_component.set_static_mesh(plaza_mesh)
        plaza_actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(plaza_cfg.get("yaw", 0.0))), False)

    # ── Ruin props (Sculpted SM_RuinArch + mossy SM_RuinColumn replacing flat walls!) ─
    _log("Placing sculpted mossy ruin archways & columns...")
    arch_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_RuinArch")
    col_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_RuinColumn")

    # Hero ceremonial sightline archway + dramatic flanking meadow ruins
    hero_arches = [
        ("SM_RuinArch_HeroNorth",   0.0,     860.0,   plaza_z,                    0.0,   1.05),
        ("SM_RuinArch_FlankNW",  -1180.0,    680.0,   terrain_z(-1180.0, 680.0),  32.0,  0.96),
        ("SM_RuinArch_FlankNE",   1180.0,    680.0,   terrain_z(1180.0, 680.0),  -32.0,  0.96),
        ("SM_RuinArch_WestGate", -1950.0,   -420.0,   terrain_z(-1950.0, -420.0), 78.0,  1.00),
        ("SM_RuinArch_EastGate",  1950.0,   -420.0,   terrain_z(1950.0, -420.0), -78.0,  1.00),
    ]
    if arch_mesh:
        for label, ax, ay, az, ayaw, ascale in hero_arches:
            actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(ax, ay, az)))
            actor.set_actor_label(label)
            actor.static_mesh_component.set_static_mesh(arch_mesh)
            actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=ayaw), False)
            actor.set_actor_scale3d(unreal.Vector(ascale, ascale, ascale))

    # Replace layout SM_RuinWall blocks with sculpted SM_RuinArch + mossy columns
    ruin_entries = layout.get("ruin_props") or []
    arch_extra_idx = 0
    for entry in ruin_entries:
        mname = entry["mesh"]
        if mname == "SM_RuinWall" and arch_mesh:
            mesh = arch_mesh
            lbl = f"SM_RuinArch_Outer_{arch_extra_idx}"
            arch_extra_idx += 1
        else:
            mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{mname}")
            lbl = entry["label"]
        if not mesh:
            continue
        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(entry["x"], entry["y"], entry["z"])))
        actor.set_actor_label(lbl)
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(entry["yaw"])), False)

    # Extra mossy weathered columns framing the hero sightline courtyard
    if col_mesh:
        courtyard_cols = [
            (-520.0, 820.0, 15.0, 1.05),
            ( 520.0, 820.0, -25.0, 1.00),
            (-760.0, 260.0, 40.0, 0.92),
            ( 760.0, 260.0, -50.0, 0.95),
            (-640.0, -480.0, 10.0, 0.88),
            ( 640.0, -480.0, 80.0, 0.90),
        ]
        for ci, (cx, cy, cyaw, csc) in enumerate(courtyard_cols):
            cz = terrain_z(cx, cy) - 6.0
            actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(cx, cy, cz)))
            actor.set_actor_label(f"SM_RuinColumn_Meadow_{ci}")
            actor.static_mesh_component.set_static_mesh(col_mesh)
            actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=cyaw), False)
            actor.set_actor_scale3d(unreal.Vector(csc, csc, csc))

    # ── Destructible structures ───────────────────────────────────────────────
    _log("Placing destructible structures...")
    struct_class = unreal.load_class(None, "/Script/WizardEPIC.DestructibleStructure")
    for idx, struct_entry in enumerate(layout.get("structures", [])):
        def_name = struct_entry["definition"]
        dd_path = f"{UE_WIZARD}/Core/DD_{def_name}"
        dd = unreal.EditorAssetLibrary.load_asset(dd_path)
        sx, sy, sz, syaw = struct_entry["x"], struct_entry["y"], struct_entry["z"], float(struct_entry["yaw"])
        # Shift center-sightline blocky Arch_A aside so the sculpted SM_RuinArch frames the vista cleanly!
        if def_name == "Arch_A" and abs(sx) < 50.0 and abs(sy - 850.0) < 100.0:
            sx, sy, syaw = -920.0, 1480.0, 28.0
            sz = terrain_z(sx, sy)
        pos = unreal.Vector(sx, sy, sz)
        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(struct_class, pos))
        if actor and dd:
            actor.set_actor_label(f"Struct_{def_name}_{idx:02d}")
            actor.set_editor_property("definition", dd)
            actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=syaw), False)

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
                rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(p["yaw"])),
                scale=unreal.Vector(1, 1, 1)
            )
            actor.add_instance(transform)

    # ── Crystal targets & plaza crystal/arch accent lights ────────────────────
    _log("Placing crystal targets & accent lights...")
    crystal_class = unreal.load_class(None, "/Script/WizardEPIC.CrystalTarget")
    crystal_dd = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Core/DD_Crystal_A")
    for i, cp in enumerate(layout.get("crystals", [])):
        pos = unreal.Vector(cp["x"], cp["y"], cp["z"])
        actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(crystal_class, pos))
        if actor:
            actor.set_actor_label(f"Crystal_{i:02d}")
            if crystal_dd:
                actor.set_editor_property("definition", crystal_dd)
            actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(cp["yaw"])), False)
        if math.hypot(cp["x"], cp["y"]) <= 850.0:
            pl = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.PointLight, unreal.Vector(cp["x"], cp["y"], cp["z"] + 95.0)))
            if pl:
                pl.set_actor_label(f"PlazaCrystalLight_{i:02d}")
                plc = pl.get_component_by_class(unreal.PointLightComponent)
                if plc:
                    if hasattr(unreal, "LightUnits"):
                        _try_set_prop(plc, "intensity_units", unreal.LightUnits.LUMENS, f"PlazaCrystalLight_{i:02d}.units")
                    _try_set_prop(plc, "intensity", 35.0, f"PlazaCrystalLight_{i:02d}.intensity")
                    _try_set_prop(plc, "attenuation_radius", 360.0, f"PlazaCrystalLight_{i:02d}.radius")
                    _try_set_prop(plc, "light_color", unreal.Color(90, 230, 255), f"PlazaCrystalLight_{i:02d}.color")
                    _try_set_prop(plc, "cast_shadows", False, f"PlazaCrystalLight_{i:02d}.cast_shadows")

    for side_label, sx in (("ArchWarmL", -210.0), ("ArchWarmR", 210.0)):
        pl = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PointLight, unreal.Vector(sx, 850.0, plaza_z + 320.0)))
        if pl:
            pl.set_actor_label(f"PlazaCrystalLight_{side_label}")
            plc = pl.get_component_by_class(unreal.PointLightComponent)
            if plc:
                if hasattr(unreal, "LightUnits"):
                    _try_set_prop(plc, "intensity_units", unreal.LightUnits.LUMENS, f"{side_label}.units")
                _try_set_prop(plc, "intensity", 65.0, f"{side_label}.intensity")
                _try_set_prop(plc, "attenuation_radius", 520.0, f"{side_label}.radius")
                _try_set_prop(plc, "light_color", unreal.Color(255, 195, 120), f"{side_label}.color")
                _try_set_prop(plc, "cast_shadows", False, f"{side_label}.cast_shadows")

    # ── Scatter sets (Lush 5,700+ Wildflower Meadow across 7 ISM layers!) ─────
    _log("Placing lush golden-hour wildflower meadow vegetation...")
    scatter_class = unreal.load_class(None, "/Script/WizardEPIC.WizardScatterSet")
    scatter_data = layout.get("scatter", {})
    scatter_actor = _tag_forge_actor(unreal.EditorLevelLibrary.spawn_actor_from_class(
        scatter_class, unreal.Vector(0, 0, 0)))
    if scatter_actor:
        scatter_actor.set_actor_label("ScatterVegetation")
        layers = scatter_actor.get_editor_property("instance_layers")
        rng = __import__("random").Random(12345)

        def _scatter_meadow_ring(count: int, r_min: float, r_max: float, scale_min: float, scale_max: float,
                                 avoid_plaza: bool = True, cluster_prob: float = 0.0):
            pts = []
            # Optional organic wildflower meadow drift clusters
            centers = []
            if cluster_prob > 0.0:
                for _ in range(max(8, count // 28)):
                    ang = rng.uniform(0.0, 2.0 * math.pi)
                    rad = math.sqrt(rng.uniform(r_min * r_min, r_max * r_max))
                    centers.append((rad * math.cos(ang), rad * math.sin(ang)))
            for _ in range(count):
                if centers and rng.random() < cluster_prob:
                    cx, cy = rng.choice(centers)
                    ca = rng.uniform(0.0, 2.0 * math.pi)
                    cd = rng.uniform(25.0, 260.0)
                    x, y = cx + cd * math.cos(ca), cy + cd * math.sin(ca)
                    rad = math.hypot(x, y)
                    if rad > r_max or (avoid_plaza and rad < 610.0):
                        ang = rng.uniform(0.0, 2.0 * math.pi)
                        rad = math.sqrt(rng.uniform(r_min * r_min, r_max * r_max))
                        x, y = rad * math.cos(ang), rad * math.sin(ang)
                else:
                    ang = rng.uniform(0.0, 2.0 * math.pi)
                    rad = math.sqrt(rng.uniform(r_min * r_min, r_max * r_max))
                    x, y = rad * math.cos(ang), rad * math.sin(ang)
                if avoid_plaza and math.hypot(x, y) < 605.0:
                    continue
                z = terrain_z(x, y) - 2.0
                yaw = rng.uniform(0.0, 360.0)
                pitch = rng.uniform(-5.0, 5.0)
                roll = rng.uniform(-5.0, 5.0)
                sc = rng.uniform(scale_min, scale_max)
                sc_z = sc * rng.uniform(0.85, 1.25)
                pts.append(unreal.Transform(
                    location=unreal.Vector(x, y, z),
                    rotation=unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                    scale=unreal.Vector(sc, sc, sc_z),
                ))
            return pts

        grass_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_GrassBlade")
        bush_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_Bush")
        flower_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_Flower")
        stone_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_SmallStone")

        petal_slot = _find_mesh_slot_index(flower_mesh, ("petal", "flower", "blossom"), 1) if flower_mesh else 1
        stem_slot = 0 if petal_slot != 0 else 1
        stem_mat = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Materials/M_GrassBlade")

        meadow_layers = [
            # (layer_idx, mesh, label, petal_mat_name, transforms)
            (0, grass_mesh,  "SM_GrassBlade (Lush Tufts)", None,
             _scatter_meadow_ring(3200, 610.0, 3350.0, 0.85, 1.65, avoid_plaza=True, cluster_prob=0.35)),
            (1, bush_mesh,   "SM_Bush", None,
             _scatter_meadow_ring(130,  760.0, 3150.0, 0.78, 1.32, avoid_plaza=True, cluster_prob=0.25)),
            (2, flower_mesh, "SM_Flower_Gold",  "M_Flower_Gold",
             _scatter_meadow_ring(700,  620.0, 3200.0, 0.85, 1.45, avoid_plaza=True, cluster_prob=0.65)),
            (3, stone_mesh,  "SM_SmallStone", None,
             _scatter_meadow_ring(180,  640.0, 3400.0, 0.80, 1.45, avoid_plaza=True, cluster_prob=0.20)),
            (4, flower_mesh, "SM_Flower_Blue",  "M_Flower_Blue",
             _scatter_meadow_ring(650,  620.0, 3200.0, 0.85, 1.45, avoid_plaza=True, cluster_prob=0.70)),
            (5, flower_mesh, "SM_Flower_Pink",  "M_Flower_Pink",
             _scatter_meadow_ring(550,  640.0, 3150.0, 0.85, 1.40, avoid_plaza=True, cluster_prob=0.70)),
            (6, flower_mesh, "SM_Flower_White", "M_Flower_White",
             _scatter_meadow_ring(600,  620.0, 3250.0, 0.80, 1.38, avoid_plaza=True, cluster_prob=0.65)),
        ]

        for l_idx, mesh, label, petal_mat_name, transforms in meadow_layers:
            if not mesh or l_idx >= len(layers) or not transforms:
                continue
            comp = layers[l_idx]
            comp.set_static_mesh(mesh)
            if petal_mat_name:
                pmat = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Materials/{petal_mat_name}")
                if stem_mat:
                    comp.set_material(stem_slot, stem_mat)
                if pmat:
                    comp.set_material(petal_slot, pmat)
            # Disable grass/flower shadow spam & collision for crisp 60fps sunlit meadow
            if mesh in (grass_mesh, flower_mesh):
                try:
                    comp.set_editor_property("cast_shadow", False)
                except Exception:
                    pass
                try:
                    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                except Exception:
                    pass
            scatter_actor.add_instances(l_idx, transforms)
            _log(f"  Scatter {label}: {len(transforms)} instances on layer {l_idx}")

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

    # Step 0: Stage BlenderAssets meshes & textures into Generated/ (preserving Piece_* destructibles)
    _log("--- Step 0: Sync BlenderAssets ---")
    sync_blender_assets()

    # Step 1: Import all generated assets
    _log("--- Step 1: Import textures ---")
    import_textures()

    _log("--- Step 1b: Import PBR textures ---")
    import_pbr_textures()

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
