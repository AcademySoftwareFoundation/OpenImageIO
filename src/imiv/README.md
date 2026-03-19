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

Main output logs:

- `verify_smoke.log`
- `verify_ux.log`
- `verify_screenshot.log`
- `verify_sampling.log`
- `verify_orientation.log`
- `verify_ocio_missing.log`
- `verify_ocio_config_source.log`
- `verify_ocio_live.log`
- `verify_ocio_live_display.log`
