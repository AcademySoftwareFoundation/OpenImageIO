# imiv Verification

Canonical cross-platform verifier:

```bash
python src/imiv/tools/imiv_backend_verify.py ...
```

Use the same runner everywhere. It selects the right regression set for the
requested backend.

Linux / WSL Vulkan:

```bash
python src/imiv/tools/imiv_backend_verify.py \
  --backend vulkan \
  --build-dir build_u \
  --out-dir verify_vulkan \
  --trace
```

Linux / WSL OpenGL:

```bash
python src/imiv/tools/imiv_backend_verify.py \
  --backend opengl \
  --build-dir build_u \
  --out-dir verify_opengl \
  --trace
```

macOS Metal:

```bash
python3 src/imiv/tools/imiv_backend_verify.py \
  --backend metal \
  --build-dir build \
  --out-dir verify_metal \
  --trace
```

macOS OpenGL:

```bash
python3 src/imiv/tools/imiv_backend_verify.py \
  --backend opengl \
  --build-dir build \
  --out-dir verify_opengl \
  --trace
```

macOS Vulkan, if MoltenVK is available:

```bash
python3 src/imiv/tools/imiv_backend_verify.py \
  --backend vulkan \
  --build-dir build \
  --out-dir verify_vulkan \
  --trace
```

Windows Vulkan:

```bat
python src\imiv\tools\imiv_backend_verify.py ^
  --backend vulkan ^
  --build-dir build ^
  --config Debug ^
  --out-dir verify_vulkan ^
  --trace
```

Windows OpenGL:

```bat
python src\imiv\tools\imiv_backend_verify.py ^
  --backend opengl ^
  --build-dir build ^
  --config Debug ^
  --out-dir verify_opengl ^
  --trace
```

If the build is already up to date, add:

```text
--skip-configure --skip-build
```

If you run through `uv` from the repo root, use:

```bash
uv run --no-project python src/imiv/tools/imiv_backend_verify.py ...
```

Optional backend-wide `ctest` entries from one multi-backend build:

```bash
cmake -S . -B build_u \
  -D OIIO_IMIV_ENABLE_VULKAN=AUTO \
  -D OIIO_IMIV_ENABLE_OPENGL=AUTO \
  -D OIIO_IMIV_ENABLE_METAL=OFF \
  -D OIIO_IMIV_DEFAULT_RENDERER=vulkan \
  -D OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON
```

```bash
ctest --test-dir build_u -N | rg imiv_backend_verify
```

Embedded binary assets:

- `OIIO_IMIV_EMBED_FONTS=ON` is the default and embeds `DroidSans.ttf` and
  `DroidSansMono.ttf` into the `imiv` binary.
- `OIIO_IMIV_EMBED_FONTS=OFF` keeps the older runtime `fonts/` directory
  behavior, with fallback to Dear ImGui's default font if those files are not
  present.
- Vulkan static upload and preview shaders are embedded into the binary at
  build time from `src/imiv/shaders/`.
- Vulkan OCIO preview shaders are still generated at runtime from the active
  OCIO configuration.
- OpenGL and Metal continue to compile their native shader source at runtime;
  they do not use embedded SPIR-V blobs.

Focused backend-selector regression from a multi-backend build:

```bash
ctest --test-dir build_u -V -R '^imiv_backend_preferences_regression$'
```

Focused multi-view regression:

```bash
ctest --test-dir build_u -V -R '^imiv_multiview_regression$'
```

Focused multi-file Image List regression:

```bash
ctest --test-dir build_u -V -R '^imiv_image_list_regression$'
```

Focused Image List interaction regression:

```bash
ctest --test-dir build_u -V -R '^imiv_image_list_interaction_regression$'
```

This covers:

- Image List single-click into the active view
- open-in-new-view
- close-in-active-view
- remove-from-session, including the one-image-remaining case

Focused Image List centering regression:

```bash
python3 src/imiv/tools/imiv_image_list_center_regression.py \
  --bin build/bin/imiv \
  --cwd build/bin \
  --backend opengl \
  --oiiotool build/bin/oiiotool \
  --env-script build/imiv_env.sh \
  --out-dir build/imiv_captures/image_list_center_regression
```

Focused OpenGL multi-open OCIO regression:

```bash
python3 src/imiv/tools/imiv_opengl_multiopen_ocio_regression.py \
  --bin build/bin/imiv \
  --cwd build/bin \
  --backend opengl \
  --env-script build/imiv_env.sh \
  --out-dir build/imiv_captures/opengl_multiopen_ocio_regression
```

Focused folder-open regression:

```bash
ctest --test-dir build_u -V -R '^imiv_open_folder_regression$'
```

Focused drag/drop regression:

```bash
ctest --test-dir build_u -V -R '^imiv_drag_drop_regression$'
```

Focused per-view recipe regression:

```bash
ctest --test-dir build_u -V -R '^imiv_view_recipe_regression$'
```

Focused Vulkan/OpenGL/Metal large-image switch regressions:

```bash
ctest --test-dir build_u -V -R '^imiv_large_image_switch_regression_(vulkan|opengl|metal)$'
```

Focused Save Selection export regression:

```bash
ctest --test-dir build_u -V -R '^imiv_save_selection_regression$'
```

Focused Export Selection As regression:

```bash
ctest --test-dir build_u -V -R '^imiv_export_selection_regression$'
```

Focused Export As regression:

```bash
ctest --test-dir build_u -V -R '^imiv_save_window_regression$'
```

Focused Export As OCIO regression:

```bash
ctest --test-dir build_u -V -R '^imiv_save_window_ocio_regression$'
```

Focused developer-menu regression:

```bash
ctest --test-dir build -V -R '^imiv_developer_menu_regression$'
```

Runtime developer mode controls:

- `--devmode` enables the `Developer` menu for the current launch
- `OIIO_DEVMODE=1|0|true|false|on|off|yes|no` overrides the default
- Debug builds default to developer mode on

Main output logs:

- `verify_smoke.log`
- `verify_rgb.log`
- `verify_ux.log`
- `verify_sampling.log`
- `verify_ocio_missing.log`
- `verify_ocio_config_source.log`
- `verify_ocio_live.log`
- `verify_ocio_live_display.log`

Backend-specific specialized regressions such as Metal orientation still write
their own dedicated logs outside the shared verifier.
