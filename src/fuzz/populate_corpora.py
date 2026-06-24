#!/usr/bin/env python3
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
"""
Populate src/fuzz/corpora/ with seed files for each image format.

Run from the repository root. Companion image repos are expected as siblings
of the repo root (e.g. ../oiio-images, ../fits-images, etc.).

Usage:
    python src/fuzz/populate_corpora.py [--format <name>] [--dry-run]
"""

import argparse
import os
import shutil
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CORPUS_ROOT = REPO_ROOT / "src" / "fuzz" / "corpora"
MAX_FILES = 5
MAX_BYTES = 100 * 1024  # 100 KB per file

# Companion image repos may live:
#   (a) as siblings of the repo root (local dev or fuzz CI checkout)
#   (b) under build/testsuite/ (fetched by oiio_setup_test_data during ctest)
# Walk candidates in priority order; first hit wins.
def _find_images_root() -> Path:
    # CI checks out oiio-images inside the workspace (path: oiio-images)
    if (REPO_ROOT / "oiio-images").is_dir():
        return REPO_ROOT
    # Local dev: companion repos are siblings of the repo root (or worktree parent)
    for candidate in (REPO_ROOT.parent, REPO_ROOT.parent.parent):
        if (candidate / "oiio-images").is_dir():
            return candidate
    # Regular cmake build: oiio_setup_test_data fetches here
    build_ts = REPO_ROOT / "build" / "testsuite"
    if (build_ts / "oiio-images").is_dir():
        return build_ts
    return REPO_ROOT.parent  # fallback; companion repos may just be absent

IMAGES_ROOT = _find_images_root()


# Source map: format -> list of (path_relative_to_repo_root, [extensions])
# '../foo' means a sibling of the repo root.
# Crash files from testsuite are included — they exercise edge-case parser
# paths and are ideal fuzz seeds.
FORMAT_SOURCES = {
    "bmp": [
        ("testsuite/bmp/src",        ["bmp", "BMP"]),
        ("../oiio-images/bmp",       ["bmp", "BMP"]),
        ("../bmpsuite",              ["bmp", "BMP"]),
    ],
    "cineon": [
        ("../oiio-images/cineon",    ["cin"]),
        ("../oiio-images",           ["cin"]),
    ],
    "dds": [
        ("testsuite/dds/src",        ["dds"]),
        ("../oiio-images/dds",       ["dds"]),
    ],
    "dicom": [
        ("../dicom-images-pvt",      ["dcm"]),
        ("testsuite/dicom/src",      ["dcm"]),
    ],
    "dpx": [
        ("../oiio-images/dpx",       ["dpx"]),
        ("../dpx-images-spi",        ["dpx"]),
    ],
    "exr": [
        ("../oiio-images",           ["exr"]),
        ("testsuite/oiio-images",    ["exr"]),
    ],
    "ffmpeg": [
        ("testsuite/ffmpeg/ref",     ["mkv", "mov", "mp4", "avi"]),
    ],
    "fits": [
        ("../fits-images/ftt4b",     ["fits", "fit"]),
        ("../fits-images/pg93",      ["fits", "fit"]),
    ],
    "gif": [
        ("../oiio-images/gif",       ["gif"]),
    ],
    # hdr: synthetic seed committed; no further sources needed
    "hdr": [],
    "heif": [
        ("../oiio-images/heif",      ["heif", "heic", "avif"]),
        ("../heif-images",           ["heif", "heic", "avif"]),
    ],
    "ico": [
        ("testsuite/ico/src",        ["ico"]),
        ("../oiio-images/ico",       ["ico"]),
    ],
    # iff: synthetic seed committed; no further sources needed
    "iff": [],
    "jpeg": [
        ("../oiio-images/jpeg",      ["jpg", "jpeg"]),
        ("testsuite/jpeg/src",       ["jpg", "jpeg"]),
        ("../oiio-images",           ["jpg", "jpeg"]),
    ],
    "jpeg2000": [
        ("../oiio-images/jpeg2000",  ["jp2", "j2k"]),
        ("../j2kp4files_v1_5/codestreams_profile0", ["j2k"]),
    ],
    # jpegxl: synthetic seed committed; no further sources needed
    "jpegxl": [],
    "openvdb": [
        ("testsuite/openvdb/src",    ["vdb"]),
    ],
    "png": [
        ("../oiio-images/png",       ["png"]),
    ],
    "pnm": [
        ("testsuite/pnm/src",        ["ppm", "pgm", "pbm", "pnm"]),
        ("../oiio-images/pnm",       ["ppm", "pgm", "pbm", "pnm"]),
    ],
    "psd": [
        ("testsuite/psd/src",        ["psd"]),
        ("../oiio-images/psd",       ["psd"]),
    ],
    "ptex": [
        ("testsuite/ptex/src",       ["ptx", "ptex"]),
    ],
    "raw": [
        ("../oiio-images/raw",       ["cr2", "nef", "arw", "raf", "rw2", "orf",
                                      "CR2", "NEF", "ARW", "RAF", "RW2", "ORF"]),
    ],
    "rla": [
        ("testsuite/rla/src",        ["rla"]),
        ("../oiio-images/rla",       ["rla"]),
    ],
    # sgi: synthetic seed committed; no further sources needed
    "sgi": [],
    "softimage": [
        ("testsuite/softimage/src",  ["pic"]),
        ("../oiio-images/softimage", ["pic"]),
    ],
    "targa": [
        ("testsuite/targa/src",      ["tga", "TGA"]),
        ("../oiio-images/targa",     ["tga", "TGA"]),
    ],
    "tiff": [
        ("../oiio-images",           ["tif", "tiff"]),
        ("../oiio-images/libtiffpic",["tif", "tiff"]),
        ("testsuite/tiff-suite/src", ["tif", "tiff"]),
    ],
    "webp": [
        ("../oiio-images/webp",      ["webp"]),
    ],
    # zfile: use the reference output produced by the testsuite (a valid zfile)
    "zfile": [
        ("testsuite/zfile/ref",      ["zfile"]),
    ],
}


def resolve(rel: str) -> Path:
    """Resolve a path; '../x' resolves relative to IMAGES_ROOT (companion repos)."""
    if rel.startswith("../"):
        return (IMAGES_ROOT / rel[3:]).resolve()
    return (REPO_ROOT / rel).resolve()


def candidate_files(sources: list) -> list[Path]:
    """Collect all candidate files from all sources, sorted by size ascending."""
    candidates = []
    for rel_dir, exts in sources:
        d = resolve(rel_dir)
        if not d.is_dir():
            continue
        for f in d.iterdir():
            if f.suffix.lstrip(".").lower() in [e.lower() for e in exts]:
                try:
                    sz = f.stat().st_size
                    if sz > 0:
                        candidates.append((sz, f))
                except OSError:
                    pass
    candidates.sort()  # ascending by size — smallest first
    return [f for _, f in candidates]


def populate_format(fmt: str, dry_run: bool, dest: Path | None = None) -> tuple[int, list[str]]:
    """
    Copy up to MAX_FILES files ≤ MAX_BYTES into dest (default: src/fuzz/corpora/<fmt>/).
    Returns (files_copied, missing_reasons).
    """
    dest = dest if dest is not None else CORPUS_ROOT / fmt
    sources = FORMAT_SOURCES.get(fmt, [])
    missing = []

    if not sources:
        missing.append("no source configured — add a synthetic seed manually")
        return 0, missing

    files = candidate_files(sources)
    small = [f for f in files if f.stat().st_size <= MAX_BYTES]

    if not small:
        if files:
            missing.append(
                f"found {len(files)} file(s) but all exceed {MAX_BYTES // 1024} KB"
            )
        else:
            dirs_checked = [resolve(r) for r, _ in sources]
            missing.append(
                "no source files found; checked: "
                + ", ".join(str(d) for d in dirs_checked if not d.exists())
                + " (dirs missing)"
                if any(not resolve(r).exists() for r, _ in sources)
                else "no source files found (dirs present but empty or wrong exts)"
            )
        return 0, missing

    chosen = small[:MAX_FILES]
    if not dry_run:
        dest.mkdir(parents=True, exist_ok=True)
    copied = 0
    for src in chosen:
        dst = dest / src.name
        if dst.exists():
            continue  # idempotent — skip existing
        if not dry_run:
            shutil.copy2(src, dst)
        copied += 1

    return copied, missing


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--format", metavar="NAME",
                        help="Populate only this format (default: all)")
    parser.add_argument("--dest", metavar="DIR", type=Path,
                        help="Write seeds into DIR/<format>/ instead of src/fuzz/corpora/<format>/")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show what would be copied without copying")
    args = parser.parse_args()

    formats = [args.format] if args.format else sorted(FORMAT_SOURCES)

    total_copied = 0
    needs_synthetic = []
    ok = []
    warnings = []

    for fmt in formats:
        dest_dir = (args.dest / fmt) if args.dest else None
        corpus_dir = dest_dir if dest_dir is not None else (CORPUS_ROOT / fmt)
        if not corpus_dir.exists() and not args.dry_run:
            corpus_dir.mkdir(parents=True, exist_ok=True)

        copied, missing = populate_format(fmt, args.dry_run, dest_dir)
        total_copied += copied
        prefix = "[dry-run] " if args.dry_run else ""

        existing = (len(list(corpus_dir.glob("*")))
                    - (1 if (corpus_dir / ".gitkeep").exists() else 0)
                    ) if corpus_dir.exists() else 0

        if missing:
            needs_synthetic.append((fmt, missing))
            total_in_dir = existing
            print(f"  {prefix}{fmt}: NEEDS MANUAL SEED — {'; '.join(missing)}"
                  + (f" ({total_in_dir} existing)" if total_in_dir else ""))
        else:
            ok.append(fmt)
            print(f"  {prefix}{fmt}: +{copied} copied"
                  + (f" ({existing} already present)" if existing > 0 else ""))

    print()
    print(f"Done. {total_copied} file(s) {'would be ' if args.dry_run else ''}copied.")
    if needs_synthetic:
        print(f"\nFormats needing manual seeds ({len(needs_synthetic)}):")
        for fmt, reasons in needs_synthetic:
            print(f"  {fmt}: {'; '.join(reasons)}")
        print("\nGenerate with: oiiotool --create 64x64 3 --ch R,G,B -o src/fuzz/corpora/<fmt>/seed.<ext>")
    if warnings:
        print(f"\nMissing corpus dirs (run T002 first): {', '.join(warnings)}")


if __name__ == "__main__":
    main()
