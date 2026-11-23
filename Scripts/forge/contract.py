"""
contract.py – Python mirror of CONTENT_CONTRACT.md / WizardTypes.h / WizardPaths.
Single source of truth for every asset name shared between offline_forge and ue_forge.
"""

# ── Dimensions ────────────────────────────────────────────────────────────────
# 1 UE unit = 1 cm.  Character ~180 cm.  Bowl radius 3600 cm.
UNIT = 1  # cm
BOWL_RADIUS = 3600  # cm
CHARACTER_HEIGHT = 180

# ── Elements ──────────────────────────────────────────────────────────────────
ELEMENTS = ["Fire", "Earth", "Wind", "Lightning"]

ELEMENT_COLORS = {
    "Fire":      (0xFF, 0x7A, 0x1A),
    "Earth":     (0xC9, 0x8A, 0x3D),
    "Wind":      (0xBF, 0xE8, 0xE2),
    "Lightning": (0xCD, 0xEB, 0xFF),
}

ELEMENT_COLORS_F = {
    k: (r / 255.0, g / 255.0, b / 255.0) for k, (r, g, b) in ELEMENT_COLORS.items()
}

# ── Spell delivery ────────────────────────────────────────────────────────────
SPELL_DELIVERY = {
    "Fire":      "Projectile",
    "Earth":     "GroundLine",
    "Wind":      "Cone",
    "Lightning": "Strike",
}

# ── Destructible material families ────────────────────────────────────────────
MATERIALS = ["Stone", "Wood", "Pottery", "Rock", "Crystal", "Foliage"]

# ── UE content paths (/Game/Wizard/...) ───────────────────────────────────────
UE_ROOT = "/Game/Wizard"

PATHS = {
    "arena":        f"{UE_ROOT}/Levels/L_Arena",
    "spells":       [f"{UE_ROOT}/Core/DA_Spell_{e}" for e in ELEMENTS],
    "spell_icons":  [f"{UE_ROOT}/UI/Icons/T_Icon_{e}" for e in ELEMENTS],
    "fx_materials": f"{UE_ROOT}/FX/Materials",
    "fx_meshes":    f"{UE_ROOT}/FX/Meshes",
    "fx_textures":  f"{UE_ROOT}/FX/Textures",
    "sfx":          f"{UE_ROOT}/Audio/SFX",
    "music":        f"{UE_ROOT}/Audio/Music",
    "env_mats":     f"{UE_ROOT}/Environment/Materials",
    "env_meshes":   f"{UE_ROOT}/Environment/Meshes",
    "env_textures": f"{UE_ROOT}/Environment/Textures",
    "dest_meshes":  f"{UE_ROOT}/Destruction/Meshes",
    "ui_fonts":     f"{UE_ROOT}/UI/Fonts",
    "ui_mats":      f"{UE_ROOT}/UI/Materials",
}

# ── Environment materials ─────────────────────────────────────────────────────
ENV_MATERIALS = [
    "M_Ground", "M_ArenaPlaza", "M_GrassBlade", "M_Foliage", "M_Bark", "M_Rock",
    "M_Sandstone", "M_RuinStone", "M_Wood", "M_Crystal", "M_WizardStaff",
    "M_Piece_Stone", "M_Piece_Rock", "M_Piece_Wood", "M_Piece_Pottery",
]

# ── FX materials ──────────────────────────────────────────────────────────────
FX_SPRITE_MASTERS = ["M_FX_Add", "M_FX_Alpha"]
FX_MESH_MATERIALS = [
    "M_FX_Core", "M_FX_Ring", "M_FX_Pillar", "M_FX_WindRibbon",
    "M_FX_Bolt", "M_FX_Crystal", "M_FX_FireTornado", "M_FX_RuneCircle",
    "M_StaffCrystal",
]
FX_CHIP_MATERIALS = [f"M_FX_Chip_{m}" for m in MATERIALS]
FX_DECALS = [
    "M_Decal_Scorch", "M_Decal_Crack",
    *[f"M_Decal_Rune_{e}" for e in ELEMENTS],
]

# ── FX meshes ─────────────────────────────────────────────────────────────────
FX_MESHES = [
    "SM_FX_Quad", "SM_FX_Sphere", "SM_FX_Ring",
    "SM_FX_Pillar", "SM_FX_Bolt", "SM_FX_Chip", "SM_FX_Shard",
    "SM_FX_FireTornado", "SM_RuneCircle",
]

# ── FX sprite textures ────────────────────────────────────────────────────────
FX_SPRITE_KINDS = [
    "Smoke", "Dust", "Leaf", "Splinter", "Shard",
    "Spark", "Ember", "Flash", "Glow", "Sparkle", "Rune",
]
FX_TEXTURES = [f"T_FX_{k}" for k in FX_SPRITE_KINDS]

# ── Audio cue names ───────────────────────────────────────────────────────────
SFX_CUES = [
    "Explosion_Fire", "Cast_Fire", "Charge", "Whoosh",
    "Cast_Earth", "Erupt_Earth",
    "Cast_Wind", "Gust_Wind",
    "Cast_Lightning", "Thunder",
    "Break_Stone", "Break_Wood", "Break_Pottery",
    "Break_Rock", "Break_Crystal", "Break_Foliage",
    "Dash", "Reset", "Combo", "Crystal", "Collapse",
]

# ── Spell data defaults (tap cast, charge-scale 1) ───────────────────────────
SPELL_DEFAULTS = {
    "Fire": dict(
        delivery="Projectile", cooldown=1.2, cast_delay=0.1,
        charge_time=1.0, charge_scale_max=2.2, min_charge=0.22,
        damage=100, damage_radius=600, launch_speed=1400, up_bias=0.25, spin=420,
        proj_speed=4200, proj_radius=28, proj_gravity=0.05, proj_life=3.0,
        color=(1.0, 0.45, 0.12), light_intensity=6e6, light_radius=2200, light_dur=0.55,
        fov_kick=3.5, hit_stop=0.045,
        cast_sound="Cast_Fire", charge_sound="Charge", impact_sound="Explosion_Fire",
        dmg_table=dict(Stone=1.0, Wood=1.6, Pottery=1.8, Rock=0.7, Crystal=1.0, Foliage=2.0),
    ),
    "Earth": dict(
        delivery="GroundLine", cooldown=1.8, cast_delay=0.15,
        charge_time=1.2, charge_scale_max=2.0, min_charge=0.25,
        damage=130, damage_radius=400, launch_speed=1800, up_bias=0.6, spin=300,
        pillar_count=6, pillar_spacing=230, pillar_height=520, pillar_radius=120,
        pillar_rise=0.16, pillar_step=0.055, pillar_life=4.0, pillar_sink=0.5, pillar_start=250,
        color=(0.75, 0.52, 0.23), light_intensity=3e6, light_radius=1600, light_dur=0.4,
        fov_kick=4.0, hit_stop=0.06,
        cast_sound="Cast_Earth", charge_sound="Charge", impact_sound="Erupt_Earth",
        dmg_table=dict(Stone=1.2, Wood=1.0, Pottery=1.5, Rock=1.4, Crystal=0.8, Foliage=0.6),
    ),
    "Wind": dict(
        delivery="Cone", cooldown=0.9, cast_delay=0.05,
        charge_time=0.8, charge_scale_max=1.8, min_charge=0.18,
        damage=60, damage_radius=500, launch_speed=2200, up_bias=0.1, spin=600,
        cone_range=1600, cone_half_angle=22, cone_duration=0.55,
        color=(0.65, 0.88, 0.85), light_intensity=2e6, light_radius=1400, light_dur=0.3,
        fov_kick=2.5, hit_stop=0.0,
        cast_sound="Cast_Wind", charge_sound="Charge", impact_sound="Gust_Wind",
        dmg_table=dict(Stone=0.5, Wood=1.4, Pottery=1.2, Rock=0.4, Crystal=0.6, Foliage=3.0),
    ),
    "Lightning": dict(
        delivery="Strike", cooldown=1.5, cast_delay=0.0,
        charge_time=1.1, charge_scale_max=2.5, min_charge=0.2,
        damage=150, damage_radius=350, launch_speed=1600, up_bias=0.3, spin=500,
        strike_count=3, strike_interval=0.09, strike_shatter=3.0,
        color=(0.78, 0.9, 1.0), light_intensity=12e6, light_radius=3000, light_dur=0.15,
        fov_kick=5.0, hit_stop=0.055,
        cast_sound="Cast_Lightning", charge_sound="Charge", impact_sound="Thunder",
        dmg_table=dict(Stone=1.5, Wood=0.8, Pottery=1.0, Rock=1.8, Crystal=2.0, Foliage=0.5),
    ),
}

# ── Destructible shape definitions (for offline_forge Voronoi fracture) ───────
# Each entry:  name, material, bbox (half-extents in cm), piece_count, health_per_piece
DESTRUCTIBLE_SHAPES = [
    # Structures (placed as DestructibleStructure actors)
    ("Wall_A",    "Stone",   (200, 30, 250), 12, 100),
    ("Wall_B",    "Stone",   (300, 30, 200), 15, 100),
    ("Pillar_A",  "Stone",   (50,  50, 350), 8,  120),
    ("Arch_A",    "Stone",   (250, 40, 300), 14, 100),
    ("Tower_A",   "Stone",   (120, 120, 500), 20, 80),
    ("BigRock_A", "Rock",    (150, 130, 100), 10, 150),
    ("BigRock_B", "Rock",    (200, 160, 120), 12, 150),
    # Breakable props (placed as WizardBreakableSet instances)
    ("Pot_A",     "Pottery", (25,  25,  30),  5, 40),
    ("Pot_B",     "Pottery", (20,  20,  35),  5, 40),
    ("Crate_A",   "Wood",    (40,  40,  40),  6, 60),
    ("Barrel_A",  "Wood",    (30,  30,  50),  6, 60),
    ("SmRock_A",  "Rock",    (35,  30,  25),  4, 80),
    ("SmRock_B",  "Rock",    (45,  35,  30),  5, 80),
    ("Tree_A",    "Foliage", (60,  60, 300),  8, 50),
    ("Crystal_A", "Crystal", (20,  20,  60),  4, 30),
]

# ── Environment mesh catalog (for offline_forge OBJ generation) ───────────────
# (name, generator_func_key, params)
ENV_MESHES = [
    # terrain bowl
    ("SM_Terrain",    "terrain_bowl",  dict(radius=3600, subdivs=64)),
    # cliff ring
    ("SM_CliffRing",  "cliff_ring",    dict(radius=3700, segments=24, height=600)),
    # stepped circular central stone dais
    ("SM_ArenaPlaza", "arena_plaza",   dict(radius=580)),
    # carved wizard staff (~155 cm, fork cradle at Z=155 cm)
    ("SM_WizardStaff","wizard_staff",  dict(height=155)),
    # grass blade (single instance mesh, scattered many times)
    ("SM_GrassBlade", "grass_blade",   dict(height=30, width=4)),
    # bush
    ("SM_Bush",       "bush",          dict(radius=40, height=50)),
    # flower
    ("SM_Flower",     "flower",        dict(height=25)),
    # small stone (scatter)
    ("SM_SmallStone", "small_stone",   dict(radius=12)),
    # column ruin
    ("SM_RuinColumn", "ruin_column",   dict(radius=35, height=250)),
    # broken wall section
    ("SM_RuinWall",   "ruin_wall",     dict(width=200, height=180, depth=30)),
]

# ── Level layout: arena actors ────────────────────────────────────────────────
# (actor_type, name_or_def, transform: (x,y,z, yaw))
import math as _math

def _ring(count, radius, z=0, start_angle=0):
    """Generate evenly spaced (x,y,z,yaw) around a ring."""
    out = []
    for i in range(count):
        a = start_angle + 2 * _math.pi * i / count
        out.append((radius * _math.cos(a), radius * _math.sin(a), z, _math.degrees(a) + 90))
    return out

# Structures
STRUCTURE_PLACEMENTS = [
    # outer walls
    *[("Wall_A", p) for p in _ring(6, 2800, 0, 0)],
    *[("Wall_B", p) for p in _ring(4, 2600, 0, _math.pi / 6)],
    # pillars
    *[("Pillar_A", p) for p in _ring(8, 2200, 0, _math.pi / 8)],
    # grand hero arch on sightline + flanking side arches
    ("Arch_A", (0, 850, 0, 0)),
    ("Arch_A", (1800, 0, 0, 90)),
    ("Arch_A", (-1800, 0, 0, 90)),
    # hero watchtower framed through the grand arch
    ("Tower_A", (0, 2200, 0, 0)),
    # big rocks
    ("BigRock_A", (1500, -800, 0, 30)),
    ("BigRock_A", (-1200, 1000, 0, 120)),
    ("BigRock_B", (800, 1600, 0, 210)),
    ("BigRock_B", (-1800, -600, 0, 65)),
]

# Breakable props
BREAKABLE_PLACEMENTS = {
    # def_name: list of (x,y,z,yaw)
    "Pot_A": (
        [(-260, -160, 0, 15), (260, -160, 0, 165), (-180, 240, 0, 40), (180, 240, 0, 140),
         (-110, 520, 0, 75), (110, 520, 0, 105)]
        + _ring(10, 1600, 0, _math.pi / 10)
    ),
    "Pot_B": (
        [(-310, -110, 0, 30), (310, -110, 0, 150), (-220, 300, 0, 60), (220, 300, 0, 120),
         (0, 480, 0, 90)]
        + _ring(8, 1400, 0, _math.pi / 8)
    ),
    "Crate_A": (
        [(-340, 80, 0, 25), (340, 80, 0, 65), (-260, 440, 0, 15), (260, 440, 0, 110)]
        + _ring(6, 2000, 0, _math.pi / 12)
    ),
    "Barrel_A": (
        [(-280, 140, 0, 10), (280, 140, 0, 80), (-160, 380, 0, 45), (160, 380, 0, 135)]
        + _ring(5, 1200, 0, 0)
    ),
    "SmRock_A": _ring(8, 2500, 0, 0),
    "SmRock_B": _ring(6, 1800, 0, _math.pi / 6),
    "Tree_A":   _ring(8, 3000, 0, _math.pi / 8) + _ring(5, 2600, 0, 0),
}

# Crystal targets: 6 plaza sightline crystals + 9 outer ring crystals = 15 crystals
CRYSTAL_PLACEMENTS = (
    [(-440, -200, 0, 15), (440, -200, 0, 165),
     (-440,  220, 0, 35), (440,  220, 0, 145),
     (-320,  600, 0, 50), (320,  600, 0, 130)]
    + _ring(9, 2000, 0, _math.pi / 9)
)

# Scatter layers: (mesh_name, placements_func)
SCATTER_CONFIG = {
    "SM_GrassBlade": dict(count=800, radius_range=(200, 3400), z=0),
    "SM_Bush":       dict(count=120, radius_range=(400, 3200), z=0),
    "SM_Flower":     dict(count=200, radius_range=(300, 3000), z=0),
    "SM_SmallStone": dict(count=150, radius_range=(100, 3500), z=0),
}

# ── UI icon specs ─────────────────────────────────────────────────────────────
ICON_SIZE = 256  # px
