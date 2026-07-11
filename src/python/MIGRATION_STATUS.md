# Nanobind migration status

Nanobind shares binding sources with pybind11 under `src/python/` (see
`py_backend.h` and `python_dual_backend_srcs` in `CMakeLists.txt`). Configure
with `-DOIIO_PYTHON_BINDINGS_BACKEND=nanobind` for nanobind-only (`PyOpenImageIO`
/ module `OpenImageIO` in site-packages), or `both` to also build
`PyOpenImageIONanobind` (`_OpenImageIO` under `lib/python/nanobind/OpenImageIO`).
Nanobind-only code paths live in binding `.cpp` files and `py_oiio.cpp` behind
`OIIO_PY_BACKEND_NANOBIND`. Shared Python↔C++ conversion helpers live in
`py_oiio.h` / `py_backend.h` for both backends.

## Migrated — full dual-backend sources

All modules below compile for both pybind11 and nanobind from `src/python/`:

| Source file | Python / C++ API |
| --- | --- |
| `py_roi.cpp` | `ROI`, free functions (`union`, `intersection`, `get_roi`, …) |
| `py_typedesc.cpp` | `TypeDesc`, enums, module `Type*` constants |
| `py_imagespec.cpp` | `ImageSpec` |
| `py_paramvalue.cpp` | `ParamValue`, `ParamValueList`, `Interp` |
| `py_deepdata.cpp` | `DeepData` |
| `py_colorconfig.cpp` | `ColorConfig`, module color constants |
| `py_imageinput.cpp` | `ImageInput` |
| `py_imageoutput.cpp` | `ImageOutput` |
| `py_imagebuf.cpp` | `ImageBuf` |
| `py_imagecache.cpp` | `ImageCache` (wrapped) |
| `py_texturesys.cpp` | `Wrap`, `MipMode`, `InterpMode`, `TextureOpt`, `TextureSystem` |
| `py_imagebufalgo.cpp` | `ImageBufAlgo`, `PixelStats`, `CompareResults`, `IBA_*` |
| `py_oiio.cpp` | Module-level attributes and global helpers |

## Consumer-visible differences (pybind11 vs nanobind)

**None intended.** Callers of the pybind11 module should see the same Python API
and behavior with the nanobind build. Binding-side `#if` / caster / constructor
differences exist only to keep that parity.

If you find a behavioral difference, treat it as a bug and add a regression test.

## Packaging / install layout

| Item | Notes |
| --- | --- |
| `__init__.py` | Shared env setup; CLI entry-point trampolines still TODO for full wheel layout. |
| `both` layout | With `OIIO_PYTHON_BINDINGS_BACKEND=both`, nanobind installs under `lib/python/nanobind/` via `src/python-nanobind/` (`_OpenImageIO` + package `__init__.py`). Default pybind11 install path is unchanged. |

## Conventions (maintainers)

- Binding macros: `.OIIO_PY_RW`, `.OIIO_PY_PROP_RO`, `.OIIO_PY_PROP_RW`, `.OIIO_PY_PROP_RW_NONE`, `.OIIO_PY_RO`, `.OIIO_PY_RO_STATIC` (see `py_backend.h`).
- Declare functions use `py_module&`, not `py::module&`.
- Buffer I/O: `oiio_py_request_buffer()` / `oiio_bufinfo_from_object()` (both backends).
- `#if defined(OIIO_PY_BACKEND_NANOBIND)` only where backends genuinely differ.

Extend **testsuite** coverage when adding behavior; run both pybind and `*.nanobind` ctest variants when `OIIO_PYTHON_BINDINGS_BACKEND=both`.
