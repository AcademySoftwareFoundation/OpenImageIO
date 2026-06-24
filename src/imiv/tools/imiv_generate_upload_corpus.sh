#!/usr/bin/env bash
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
out_dir="${1:-$repo_root/build_u/testsuite/imiv/upload_corpus/images}"
manifest_csv="${2:-$repo_root/build_u/testsuite/imiv/upload_corpus/corpus_manifest.csv}"
oiiotool_bin="${OIIOTOOL_BIN:-$repo_root/build_u/bin/oiiotool}"

if [[ ! -x "$oiiotool_bin" ]]; then
    echo "error: oiiotool not found or not executable: $oiiotool_bin" >&2
    exit 2
fi

mkdir -p "$out_dir"
mkdir -p "$(dirname "$manifest_csv")"
rm -f "$manifest_csv"

cat > "$manifest_csv" <<'EOF'
path,width,height,channels,depth,format
EOF

dimensions=(
    "1x1"
    "2x3"
    "3x5"
    "4x7"
    "6x10"
    "9x13"
    "17x17"
    "31x17"
)

channels=(
    "rgb:3:0.85,0.35,0.15"
    "rgba:4:0.10,0.80,0.95,0.75"
)

depths=(
    "u8:uint8:tif"
    "u16:uint16:tif"
    "u32:uint32:tif"
    "f16:half:exr"
    "f32:float:exr"
    "f64:double:tif"
)

count=0
for dim in "${dimensions[@]}"; do
    width="${dim%x*}"
    height="${dim#*x}"
    for ch_entry in "${channels[@]}"; do
        ch_name="${ch_entry%%:*}"
        rest="${ch_entry#*:}"
        ch_count="${rest%%:*}"
        color="${rest#*:}"
        for depth_entry in "${depths[@]}"; do
            depth_tag="${depth_entry%%:*}"
            rest="${depth_entry#*:}"
            depth_name="${rest%%:*}"
            extension="${rest#*:}"

            file_name="${ch_name}_${depth_tag}_${width}x${height}.${extension}"
            file_path="${out_dir}/${file_name}"

            "$oiiotool_bin" \
                --pattern "constant:color=${color}" "${width}x${height}" \
                "${ch_count}" -d "${depth_name}" -o "${file_path}"

            printf '%s,%s,%s,%s,%s,%s\n' \
                "$file_path" "$width" "$height" "$ch_count" "$depth_name" \
                "$extension" >> "$manifest_csv"
            count=$((count + 1))
        done
    done
done

echo "generated ${count} images"
echo "corpus dir: ${out_dir}"
echo "manifest: ${manifest_csv}"
