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
        if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stem}"):
            continue
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
        if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stem}"):
            continue
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
        if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{stem}"):
            continue
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
            if unreal.EditorAssetLibrary.does_asset_exist(f"{dest_root}/{stem}"):
                continue
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
# Material creation
# ═══════════════════════════════════════════════════════════════════════════════

def create_materials():
    """Create all materials listed in the contract (simple opaque or translucent)."""
    if not IN_EDITOR:
        return
    at = unreal.AssetToolsHelpers.get_asset_tools()
    mf = unreal.MaterialFactoryNew()

    # Environment materials
    env_mat_path = f"{UE_WIZARD}/Environment/Materials"
    _ensure_dir(env_mat_path)
    for mat_name in C.ENV_MATERIALS:
        full = f"{env_mat_path}/{mat_name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(full):
            mat = at.create_asset(mat_name, env_mat_path, unreal.Material, mf)
            unreal.EditorAssetLibrary.save_loaded_asset(mat)
            _log(f"Created material {mat_name}")

    # FX materials
    fx_mat_path = f"{UE_WIZARD}/FX/Materials"
    _ensure_dir(fx_mat_path)
    all_fx_mats = C.FX_SPRITE_MASTERS + C.FX_MESH_MATERIALS + C.FX_CHIP_MATERIALS + C.FX_DECALS
    for mat_name in all_fx_mats:
        full = f"{fx_mat_path}/{mat_name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(full):
            mat = at.create_asset(mat_name, fx_mat_path, unreal.Material, mf)
            # Set blend mode for sprite masters
            if mat_name in ("M_FX_Add",):
                mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
                mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            elif mat_name in ("M_FX_Alpha",):
                mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
                mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
            elif mat_name.startswith("M_Decal_"):
                mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
            unreal.EditorAssetLibrary.save_loaded_asset(mat)
            _log(f"Created FX material {mat_name}")

    # UI materials
    ui_mat_path = f"{UE_WIZARD}/UI/Materials"
    _ensure_dir(ui_mat_path)
    for mat_name in ["M_UI_Cooldown", "M_UI_Charge", "M_UI_Panel"]:
        full = f"{ui_mat_path}/{mat_name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(full):
            mat = at.create_asset(mat_name, ui_mat_path, unreal.Material, mf)
            mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
            unreal.EditorAssetLibrary.save_loaded_asset(mat)
            _log(f"Created UI material {mat_name}")

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

        # Bounds (compute from pieces)
        if data.get("pieces"):
            all_x = [p["transform"][0] + p["extent"][0] for p in data["pieces"]]
            all_y = [p["transform"][1] + p["extent"][1] for p in data["pieces"]]
            all_z = [p["transform"][2] + p["extent"][2] for p in data["pieces"]]
            ext = unreal.Vector(max(all_x), max(all_y), max(all_z))
            dd.set_editor_property("size", max(ext.x, ext.y, ext.z) * 2)

        unreal.EditorAssetLibrary.save_loaded_asset(dd)
        _log(f"Created/updated destructible definition: {dd_name}")

# ═══════════════════════════════════════════════════════════════════════════════
# Level assembly
# ═══════════════════════════════════════════════════════════════════════════════

def build_arena_level(layout: dict):
    """Create and populate L_Arena."""
    if not IN_EDITOR:
        return

    lvl_path = f"{UE_WIZARD}/Levels"
    _ensure_dir(lvl_path)

    # Create or open level
    arena_path = f"{lvl_path}/L_Arena"
    if unreal.EditorAssetLibrary.does_asset_exist(arena_path):
        unreal.EditorLevelLibrary.load_level(arena_path)
    else:
        unreal.EditorLevelLibrary.new_level(arena_path)

    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    # ── Lighting & atmosphere ─────────────────────────────────────────────────
    _log("Placing lighting & atmosphere...")

    # Directional light (sun from south-west)
    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 0))
    sun.set_actor_label("Sun")
    sun.set_actor_rotation(unreal.Rotator(-45, -135, 0), False)
    light_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if light_comp:
        light_comp.set_editor_property("intensity", 8.0)
        light_comp.set_editor_property("light_color", unreal.Color(255, 245, 230))

    # Sky light
    sky_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, 500))
    sky_light.set_actor_label("SkyLight")

    # Sky atmosphere
    sky_atmo = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky_atmo.set_actor_label("SkyAtmosphere")

    # Volumetric cloud
    vol_cloud = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.VolumetricCloud, unreal.Vector(0, 0, 0))
    vol_cloud.set_actor_label("VolumetricCloud")

    # Exponential height fog
    fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.ExponentialHeightFog, unreal.Vector(0, 0, 100))
    fog.set_actor_label("HeightFog")

    # Post process volume (unbound)
    ppv = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    ppv.set_actor_label("PostProcess")
    ppv.set_editor_property("unbound", True)

    # Player start (spawn plaza)
    ps = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0, 0, 50))
    ps.set_actor_label("PlayerStart")

    # ── Terrain ───────────────────────────────────────────────────────────────
    _log("Placing terrain & cliffs...")
    terrain_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_Terrain")
    cliff_mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/SM_CliffRing")

    if terrain_mesh:
        terrain_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
        terrain_actor.set_actor_label("Terrain")
        terrain_actor.static_mesh_component.set_static_mesh(terrain_mesh)

    if cliff_mesh:
        cliff_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
        cliff_actor.set_actor_label("CliffRing")
        cliff_actor.static_mesh_component.set_static_mesh(cliff_mesh)

    # ── Ruin props (StaticMeshActor) ──────────────────────────────────────────
    _log("Placing ruin props...")
    for ruin_name in ["SM_RuinColumn", "SM_RuinWall"]:
        mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{ruin_name}")
        if not mesh:
            continue
        # Place a few instances
        for i, pos in enumerate(C._ring(4, 2900, 0, math.pi / 4)):
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(pos[0], pos[1], pos[2]))
            actor.set_actor_label(f"{ruin_name}_{i}")
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.set_actor_rotation(unreal.Rotator(0, pos[3], 0), False)

    # ── Destructible structures ───────────────────────────────────────────────
    _log("Placing destructible structures...")
    struct_class = unreal.load_class(None, "/Script/WizardEPIC.DestructibleStructure")
    for struct_entry in layout.get("structures", []):
        def_name = struct_entry["definition"]
        dd_path = f"{UE_WIZARD}/Core/DD_{def_name}"
        dd = unreal.EditorAssetLibrary.load_asset(dd_path)
        pos = unreal.Vector(struct_entry["x"], struct_entry["y"], struct_entry["z"])
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(struct_class, pos)
        if actor and dd:
            actor.set_actor_label(f"Struct_{def_name}")
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

        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(breakable_class, unreal.Vector(0, 0, 0))
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
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(crystal_class, pos)
        if actor:
            actor.set_actor_label(f"Crystal_{i:02d}")
            if crystal_dd:
                actor.set_editor_property("definition", crystal_dd)
            actor.set_actor_rotation(unreal.Rotator(0, cp["yaw"], 0), False)

    # ── Scatter sets ──────────────────────────────────────────────────────────
    _log("Placing scatter vegetation...")
    scatter_class = unreal.load_class(None, "/Script/WizardEPIC.WizardScatterSet")
    scatter_data = layout.get("scatter", {})
    scatter_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(scatter_class, unreal.Vector(0, 0, 0))
    if scatter_actor:
        scatter_actor.set_actor_label("ScatterVegetation")
        layer_idx = 0
        rng = __import__("random").Random(12345)
        for mesh_name, cfg in scatter_data.items():
            mesh = unreal.EditorAssetLibrary.load_asset(f"{UE_WIZARD}/Environment/Meshes/{mesh_name}")
            if not mesh:
                continue
            # Use pre-created ISMC layers on AWizardScatterSet
            layers = scatter_actor.get_editor_property("instance_layers")
            if layer_idx >= len(layers):
                break
            layers[layer_idx].set_static_mesh(mesh)

            count = cfg.get("count", 100)
            r_min, r_max = cfg.get("radius_range", (100, 3400))
            z = cfg.get("z", 0)
            transforms = []
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
            _log(f"  Scatter {mesh_name}: {count} instances on layer {layer_idx}")
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

    # Step 2: Create materials
    _log("--- Step 5: Create materials ---")
    create_materials()

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
