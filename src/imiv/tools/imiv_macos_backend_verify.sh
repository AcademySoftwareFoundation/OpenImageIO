#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
backend="metal"
build_dir=""
out_dir=""
jobs="${IMIV_JOBS:-8}"
image_path="${repo_root}/ASWF/logos/openimageio-stacked-gradient.png"
trace=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend)
            backend="$2"
            shift 2
            ;;
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --out-dir)
            out_dir="$2"
            shift 2
            ;;
        --jobs)
            jobs="$2"
            shift 2
            ;;
        --image)
            image_path="$2"
            shift 2
            ;;
        --trace)
            trace=1
            shift
            ;;
        -h|--help)
            cat <<USAGE
Usage: src/imiv/tools/imiv_macos_backend_verify.sh [options]

Options:
  --backend metal|opengl   Backend to configure and verify (default: metal)
  --build-dir DIR          CMake build directory
  --out-dir DIR            Output/log directory
  --jobs N                 Parallel build jobs (default: IMIV_JOBS or 8)
  --image PATH             Image to open for smoke runs
  --trace                  Enable test-runner trace output
USAGE
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

case "${backend}" in
    metal|opengl)
        ;;
    *)
        echo "error: unsupported backend '${backend}'; use metal or opengl" >&2
        exit 2
        ;;
esac

if [[ -z "${build_dir}" ]]; then
    build_dir="${repo_root}/build"
fi
if [[ -z "${out_dir}" ]]; then
    out_dir="${build_dir}/imiv_captures/${backend}_verify"
fi

mkdir -p "${out_dir}"
system_info_log="${out_dir}/system_info.txt"
configure_log="${out_dir}/cmake_configure.log"
build_log="${out_dir}/cmake_build.log"
runner_log="${out_dir}/verify_runner.log"
screenshot_runner_log="${out_dir}/verify_screenshot.log"
orientation_runner_log="${out_dir}/verify_orientation.log"
ocio_missing_runner_log="${out_dir}/verify_ocio_missing.log"
ocio_config_source_runner_log="${out_dir}/verify_ocio_config_source.log"
ocio_live_runner_log="${out_dir}/verify_ocio_live.log"
ocio_live_display_runner_log="${out_dir}/verify_ocio_live_display.log"

{
    date
    echo "repo_root=${repo_root}"
    echo "backend=${backend}"
    echo "build_dir=${build_dir}"
    echo "out_dir=${out_dir}"
    echo "image_path=${image_path}"
    echo
    uname -a || true
    if command -v sw_vers >/dev/null 2>&1; then
        echo
        sw_vers || true
    fi
    if command -v sysctl >/dev/null 2>&1; then
        echo
        sysctl -n machdep.cpu.brand_string 2>/dev/null || true
    fi
    echo
    xcode-select -p 2>/dev/null || true
    echo
    cmake --version || true
    echo
    clang++ --version || true
    echo
    python3 --version || true
    echo
    ninja --version || true
    echo
    echo "OCIO=${OCIO-}"
    echo "VULKAN_SDK=${VULKAN_SDK-}"
} > "${system_info_log}"

cmake -S "${repo_root}" -B "${build_dir}" -D OIIO_IMIV_RENDERER="${backend}" \
    2>&1 | tee "${configure_log}"
build_targets=(imiv)
if [[ "${backend}" == "metal" ]]; then
    build_targets+=(oiiotool idiff)
fi
cmake --build "${build_dir}" --target "${build_targets[@]}" --parallel "${jobs}" \
    2>&1 | tee "${build_log}"

bin_path=""
for candidate in \
    "${build_dir}/bin/imiv" \
    "${build_dir}/src/imiv/imiv" \
    "${build_dir}/Release/imiv" \
    "${build_dir}/Debug/imiv"; do
    if [[ -x "${candidate}" ]]; then
        bin_path="${candidate}"
        break
    fi
done
if [[ -z "${bin_path}" ]]; then
    echo "error: could not locate built imiv binary under ${build_dir}" >&2
    exit 1
fi

runner_py=""
screenshot_runner_py=""
orientation_runner_py=""
ocio_missing_runner_py=""
ocio_config_source_runner_py=""
ocio_live_runner_py=""
case "${backend}" in
    metal)
        runner_py="${repo_root}/src/imiv/tools/imiv_metal_smoke_regression.py"
        screenshot_runner_py="${repo_root}/src/imiv/tools/imiv_metal_screenshot_regression.py"
        orientation_runner_py="${repo_root}/src/imiv/tools/imiv_metal_orientation_regression.py"
        ocio_missing_runner_py="${repo_root}/src/imiv/tools/imiv_ocio_missing_fallback_regression.py"
        ocio_config_source_runner_py="${repo_root}/src/imiv/tools/imiv_ocio_config_source_regression.py"
        ocio_live_runner_py="${repo_root}/src/imiv/tools/imiv_ocio_live_update_regression.py"
        ;;
    opengl)
        runner_py="${repo_root}/src/imiv/tools/imiv_opengl_smoke_regression.py"
        ;;
esac

cmd=(python3 "${runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --out-dir "${out_dir}/runtime" --open "${image_path}")
if [[ -f "${build_dir}/imiv_env.sh" ]]; then
    cmd+=(--env-script "${build_dir}/imiv_env.sh")
fi
if [[ ${trace} -ne 0 ]]; then
    cmd+=(--trace)
fi
"${cmd[@]}" 2>&1 | tee "${runner_log}"

verify_failed=0

if [[ -n "${screenshot_runner_py}" ]]; then
    screenshot_cmd=(python3 "${screenshot_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --out-dir "${out_dir}/runtime_screenshot" --open "${image_path}")
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        screenshot_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        screenshot_cmd+=(--trace)
    fi
    if ! "${screenshot_cmd[@]}" 2>&1 | tee "${screenshot_runner_log}"; then
        verify_failed=1
    fi
fi

if [[ -n "${orientation_runner_py}" ]]; then
    orientation_cmd=(python3 "${orientation_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --out-dir "${out_dir}/runtime_orientation" --open "${image_path}")
    if [[ -x "${build_dir}/bin/oiiotool" ]]; then
        orientation_cmd+=(--oiiotool "${build_dir}/bin/oiiotool")
    elif [[ -x "${build_dir}/src/oiiotool/oiiotool" ]]; then
        orientation_cmd+=(--oiiotool "${build_dir}/src/oiiotool/oiiotool")
    fi
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        orientation_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        orientation_cmd+=(--trace)
    fi
    if ! "${orientation_cmd[@]}" 2>&1 | tee "${orientation_runner_log}"; then
        verify_failed=1
    fi
fi

oiiotool_path=""
for candidate in \
    "${build_dir}/bin/oiiotool" \
    "${build_dir}/src/oiiotool/oiiotool" \
    "${build_dir}/Release/oiiotool" \
    "${build_dir}/Debug/oiiotool"; do
    if [[ -x "${candidate}" ]]; then
        oiiotool_path="${candidate}"
        break
    fi
done

idiff_path=""
for candidate in \
    "${build_dir}/bin/idiff" \
    "${build_dir}/src/idiff/idiff" \
    "${build_dir}/Release/idiff" \
    "${build_dir}/Debug/idiff"; do
    if [[ -x "${candidate}" ]]; then
        idiff_path="${candidate}"
        break
    fi
done

ocio_config_path="${repo_root}/temp/studio-config-all-views-v4.0.0_aces-v2.0_ocio-v2.5.ocio"
if [[ ! -f "${ocio_config_path}" ]]; then
    ocio_config_path=""
fi

if [[ -n "${ocio_missing_runner_py}" ]]; then
    ocio_missing_cmd=(python3 "${ocio_missing_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --out-dir "${out_dir}/runtime_ocio_missing" --open "${image_path}")
    if [[ -n "${oiiotool_path}" ]]; then
        ocio_missing_cmd+=(--oiiotool "${oiiotool_path}")
    fi
    if [[ -n "${idiff_path}" ]]; then
        ocio_missing_cmd+=(--idiff "${idiff_path}")
    fi
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        ocio_missing_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        ocio_missing_cmd+=(--trace)
    fi
    if ! "${ocio_missing_cmd[@]}" 2>&1 | tee "${ocio_missing_runner_log}"; then
        verify_failed=1
    fi
fi

if [[ -n "${ocio_config_source_runner_py}" && -n "${ocio_config_path}" ]]; then
    ocio_config_cmd=(python3 "${ocio_config_source_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --out-dir "${out_dir}/runtime_ocio_config_source" --ocio-config "${ocio_config_path}")
    if [[ -n "${oiiotool_path}" ]]; then
        ocio_config_cmd+=(--oiiotool "${oiiotool_path}")
    fi
    if [[ -n "${idiff_path}" ]]; then
        ocio_config_cmd+=(--idiff "${idiff_path}")
    fi
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        ocio_config_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        ocio_config_cmd+=(--trace)
    fi
    if ! "${ocio_config_cmd[@]}" 2>&1 | tee "${ocio_config_source_runner_log}"; then
        verify_failed=1
    fi
elif [[ -n "${ocio_config_source_runner_py}" ]]; then
    echo "skip: OCIO config-source regression not run because temp OCIO config was not found" \
        | tee "${ocio_config_source_runner_log}"
fi

if [[ -n "${ocio_live_runner_py}" && -n "${ocio_config_path}" && -n "${oiiotool_path}" && -n "${idiff_path}" ]]; then
    ocio_live_image="${out_dir}/runtime_ocio_live/ocio_live_input.exr"
    mkdir -p "${out_dir}/runtime_ocio_live" "${out_dir}/runtime_ocio_live_display"

    ocio_live_cmd=(python3 "${ocio_live_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --oiiotool "${oiiotool_path}" --idiff "${idiff_path}" --out-dir "${out_dir}/runtime_ocio_live" --image "${ocio_live_image}" --ocio-config "${ocio_config_path}")
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        ocio_live_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        ocio_live_cmd+=(--trace)
    fi
    if ! "${ocio_live_cmd[@]}" 2>&1 | tee "${ocio_live_runner_log}"; then
        verify_failed=1
    fi

    ocio_live_display_cmd=(python3 "${ocio_live_runner_py}" --bin "${bin_path}" --cwd "$(dirname "${bin_path}")" --oiiotool "${oiiotool_path}" --idiff "${idiff_path}" --out-dir "${out_dir}/runtime_ocio_live_display" --image "${out_dir}/runtime_ocio_live_display/ocio_live_input.exr" --ocio-config "${ocio_config_path}" --display "sRGB - Display" --target-display "Display P3 - Display" --raw-view "Un-tone-mapped" --target-view "Un-tone-mapped")
    if [[ -f "${build_dir}/imiv_env.sh" ]]; then
        ocio_live_display_cmd+=(--env-script "${build_dir}/imiv_env.sh")
    fi
    if [[ ${trace} -ne 0 ]]; then
        ocio_live_display_cmd+=(--trace)
    fi
    if ! "${ocio_live_display_cmd[@]}" 2>&1 | tee "${ocio_live_display_runner_log}"; then
        verify_failed=1
    fi
elif [[ -n "${ocio_live_runner_py}" ]]; then
    {
        if [[ -z "${ocio_config_path}" ]]; then
            echo "skip: Metal OCIO live regressions not run because temp OCIO config was not found"
        elif [[ -z "${oiiotool_path}" ]]; then
            echo "skip: Metal OCIO live regressions not run because oiiotool was not found in ${build_dir}"
        elif [[ -z "${idiff_path}" ]]; then
            echo "skip: Metal OCIO live regressions not run because idiff was not found in ${build_dir}"
        fi
    } | tee "${ocio_live_runner_log}"
fi

echo
echo "Verification logs written to: ${out_dir}"
echo "  system:    ${system_info_log}"
echo "  configure: ${configure_log}"
echo "  build:     ${build_log}"
echo "  runner:    ${runner_log}"
if [[ -n "${screenshot_runner_py}" ]]; then
echo "  screenshot:${screenshot_runner_log}"
echo "  runtime+ss:${out_dir}/runtime_screenshot"
fi
if [[ -n "${orientation_runner_py}" ]]; then
echo "  orient:    ${orientation_runner_log}"
echo "  runtime+or:${out_dir}/runtime_orientation"
fi
if [[ -n "${ocio_missing_runner_py}" ]]; then
echo "  ocio-miss: ${ocio_missing_runner_log}"
echo "  runtime+om:${out_dir}/runtime_ocio_missing"
fi
if [[ -n "${ocio_config_source_runner_py}" && -n "${ocio_config_path}" ]]; then
echo "  ocio-src:  ${ocio_config_source_runner_log}"
echo "  runtime+os:${out_dir}/runtime_ocio_config_source"
fi
if [[ -n "${ocio_live_runner_py}" && -n "${ocio_config_path}" && -n "${oiiotool_path}" && -n "${idiff_path}" ]]; then
echo "  ocio-live: ${ocio_live_runner_log}"
echo "  runtime+ol:${out_dir}/runtime_ocio_live"
echo "  ocio-disp: ${ocio_live_display_runner_log}"
echo "  runtime+od:${out_dir}/runtime_ocio_live_display"
fi
echo "  runtime:   ${out_dir}/runtime"

exit "${verify_failed}"
