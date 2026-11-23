#!/usr/bin/env python3
"""
fetch_pbr.py – Download 1K PBR texture sets (Diffuse, nor_gl, Rough) from Poly Haven
into Generated/PBR/ using Python stdlib only.

Usage:
    python fetch_pbr.py [--out DIR] [--force]
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

API_BASE = "https://api.polyhaven.com"
USER_AGENT = "wizardEPIC-forge/1.0 (+https://polyhaven.com)"
MIN_EXISTING_BYTES = 10 * 1024  # 10 KB

CATEGORIES = [
    {
        "key": "ground",
        "primary": "mossy_cobblestone",
        "keywords": ["cobblestone", "mossy", "courtyard", "pavement", "cobble"],
        "desc": "cobblestone / mossy stone courtyard",
    },
    {
        "key": "ruinstone",
        "primary": "castle_brick_01",
        "keywords": ["castle", "ashlar", "stone_wall", "medieval", "brick"],
        "desc": "castle brick / ashlar stone wall",
    },
    {
        "key": "rock",
        "primary": "dark_rock_02",
        "keywords": ["dark_rock", "cliff", "rock_face", "rock", "craggy"],
        "desc": "craggy dark cliff rock",
    },
    {
        "key": "wood",
        "primary": "weathered_planks",
        "keywords": ["weathered_planks", "planks", "weathered", "plank", "wood"],
        "desc": "weathered planks",
    },
    {
        "key": "bark",
        "primary": "bark_brown_02",
        "keywords": ["bark", "tree", "timber"],
        "desc": "rough tree bark",
    },
    {
        "key": "sandstone",
        "primary": "large_sandstone_blocks_01",
        "keywords": ["sandstone", "terracotta", "blocks", "clay"],
        "desc": "sandstone / terracotta",
    },
]

MAPS = [
    ("Diffuse", "diff"),
    ("nor_gl", "nor"),
    ("Rough", "rough"),
]


def _http_get_bytes(url: str, timeout: int = 30, retries: int = 2) -> bytes:
    last_err: Exception | None = None
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.read()
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                raise
            last_err = exc
        except Exception as exc:
            last_err = exc
        if attempt < retries:
            time.sleep(1.0 * (attempt + 1))
    assert last_err is not None
    raise last_err


def _fetch_json(url: str) -> dict:
    raw = _http_get_bytes(url)
    return json.loads(raw.decode("utf-8"))


def _pick_map_entry(files_json: dict, map_key: str) -> tuple[str, str] | None:
    """Return (url, ext) for 1k jpg/png of map_key, or None if unavailable."""
    map_block = files_json.get(map_key)
    if not isinstance(map_block, dict):
        return None
    res_block = map_block.get("1k") or map_block.get("2k")
    if not isinstance(res_block, dict):
        return None
    for ext in ("jpg", "png"):
        fmt = res_block.get(ext)
        if isinstance(fmt, dict) and fmt.get("url"):
            return fmt["url"], ext
    return None


def _find_fallback_candidates(
    catalog: dict, keywords: list[str], exclude_id: str, limit: int = 8
) -> list[str]:
    """Score Poly Haven texture assets against keywords and return top IDs."""
    scored: list[tuple[int, str]] = []
    for asset_id, meta in catalog.items():
        if asset_id == exclude_id or not isinstance(meta, dict):
            continue
        haystack = " ".join(
            [
                asset_id.lower(),
                str(meta.get("name", "")).lower(),
                " ".join(str(c).lower() for c in meta.get("categories", [])),
                " ".join(str(t).lower() for t in meta.get("tags", [])),
            ]
        )
        score = 0
        for rank, kw in enumerate(keywords):
            weight = max(1, len(keywords) - rank) * 3
            if kw.lower() in asset_id.lower():
                score += weight * 2
            elif kw.lower() in haystack:
                score += weight
        if score > 0:
            scored.append((score, asset_id))
    scored.sort(key=lambda item: (-item[0], item[1]))
    return [aid for _, aid in scored[:limit]]


def _existing_map_file(out_dir: Path, key: str, suffix: str) -> Path | None:
    for ext in ("jpg", "png"):
        cand = out_dir / f"PBR_{key}_{suffix}.{ext}"
        if cand.exists() and cand.stat().st_size > MIN_EXISTING_BYTES:
            return cand
    return None


def _download_file(url: str, dest: Path) -> int:
    data = _http_get_bytes(url, timeout=45, retries=2)
    tmp = dest.with_suffix(dest.suffix + ".part")
    tmp.write_bytes(data)
    tmp.replace(dest)
    return len(data)


def fetch_category(
    cat: dict,
    out_dir: Path,
    catalog_cache: list[dict | None],
    force: bool = False,
) -> bool:
    key = cat["key"]
    primary = cat["primary"]

    missing_maps: list[tuple[str, str]] = []
    for map_key, suffix in MAPS:
        existing = None if force else _existing_map_file(out_dir, key, suffix)
        if existing is not None:
            kb = existing.stat().st_size / 1024.0
            print(f"  [skip] {existing.name} ({kb:.1f} KB)")
        else:
            missing_maps.append((map_key, suffix))

    if not missing_maps:
        return True

    def _try_asset(asset_id: str) -> dict | None:
        try:
            files = _fetch_json(f"{API_BASE}/files/{asset_id}")
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                print(f"  [warn] Poly Haven asset '{asset_id}' returned 404")
                return None
            print(f"  [warn] HTTP {exc.code} querying '{asset_id}': {exc}")
            return None
        except Exception as exc:
            print(f"  [warn] Failed querying '{asset_id}': {exc}")
            return None

        resolved = {}
        for map_key, suffix in missing_maps:
            entry = _pick_map_entry(files, map_key)
            if entry is None:
                print(f"  [warn] '{asset_id}' missing 1k '{map_key}' map")
                return None
            resolved[suffix] = entry
        return resolved

    chosen_id = primary
    resolved_urls = _try_asset(primary)

    if resolved_urls is None:
        print(f"  [fallback] Querying {API_BASE}/assets?t=textures for '{key}'...")
        if catalog_cache[0] is None:
            try:
                catalog_cache[0] = _fetch_json(f"{API_BASE}/assets?t=textures")
            except Exception as exc:
                print(f"  [error] Failed fetching texture catalog: {exc}")
                return False
        candidates = _find_fallback_candidates(catalog_cache[0], cat["keywords"], primary)
        for cand_id in candidates:
            resolved_urls = _try_asset(cand_id)
            if resolved_urls is not None:
                chosen_id = cand_id
                print(f"  [fallback] Selected '{chosen_id}' for '{key}'")
                break

    if resolved_urls is None:
        print(f"  [error] Could not resolve PBR maps for category '{key}'")
        return False

    ok = True
    for _map_key, suffix in missing_maps:
        url, ext = resolved_urls[suffix]
        dest = out_dir / f"PBR_{key}_{suffix}.{ext}"
        try:
            nbytes = _download_file(url, dest)
            print(f"  [ok]   {dest.name} <- {chosen_id} ({nbytes / 1024.0:.1f} KB)")
        except Exception as exc:
            print(f"  [error] Download failed for {dest.name} ({url}): {exc}")
            ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch 1K PBR textures from Poly Haven")
    parser.add_argument(
        "--out",
        type=str,
        default=None,
        help="Output PBR directory (default: Generated/PBR next to this script)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-download even if files > 10 KB already exist",
    )
    args = parser.parse_args()

    out_dir = (
        Path(args.out)
        if args.out
        else Path(__file__).resolve().parent / "Generated" / "PBR"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"=== Fetching Poly Haven 1K PBR Maps -> {out_dir} ===")
    catalog_cache: list[dict | None] = [None]
    all_ok = True

    for cat in CATEGORIES:
        print(f"[{cat['key']}] ({cat['desc']})")
        if not fetch_category(cat, out_dir, catalog_cache, force=args.force):
            all_ok = False

    if not all_ok:
        print("\nWARNING: One or more PBR textures failed to download.")
        return 1

    print("\n=== All 18 PBR texture maps ready ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
