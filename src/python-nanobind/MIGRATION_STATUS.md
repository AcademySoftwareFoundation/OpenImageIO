# Nanobind migration status (vs `src/python` pybind11)

Generated from the binding sources. The nanobind extension is `PyOpenImageIONanobind` / module `_OpenImageIO` (see `CMakeLists.txt`).

## Migrated — full parity with pybind (no known gaps for this surface)

| Source file | Python / C++ API |
| --- | --- |
| `py_roi.cpp` | <code>ROI</code><br>Free functions:<br><ul><li><code>union</code></li><li><code>intersection</code></li><li><code>get_roi</code></li><li><code>get_roi_full</code></li><li><code>set_roi</code></li><li><code>set_roi_full</code></li></ul> |
| `py_typedesc.cpp` | <code>TypeDesc</code><br>Enums: <code>BASETYPE</code>, <code>AGGREGATE</code>, <code>VECSEMANTICS</code><br>Module <code>Type*</code> constants |
| `py_imagespec.cpp` | <code>ImageSpec</code> (bound methods/properties, typed <code>attribute</code> / buffer paths via shared helpers). |
| `py_paramvalue.cpp` | <code>ParamValue</code>, <code>ParamValueList</code><br>Enum: <code>Interp</code> |

## Migrated — partial (gaps or intentional deltas vs pybind)

| Source file | Migrated (vs pybind) | Missing or divergent (vs pybind) |
| --- | --- | --- |
| `py_oiio.cpp` (`_OpenImageIO` module) | <ul><li><code>attribute</code> (one-arg and typed)</li><li><code>get_int_attribute</code></li><li><code>get_float_attribute</code></li><li><code>get_string_attribute</code></li><li><code>getattribute</code></li><li><code>__version__</code></li></ul> | <ul><li><code>geterror</code></li><li><code>get_bytes_attribute</code></li><li>Module <code>set_colorspace</code> (helper taking <code>ImageSpec</code> — the instance method is on <code>ImageSpec</code> in nanobind)</li><li><code>set_colorspace_rec709_gamma</code></li><li><code>equivalent_colorspace</code></li><li><code>is_imageio_format_name</code></li><li><code>AutoStride</code></li><li><code>openimageio_version</code>, <code>VERSION</code>, <code>VERSION_STRING</code>, <code>VERSION_MAJOR</code>, <code>VERSION_MINOR</code>, <code>VERSION_PATCH</code>, <code>INTRO_STRING</code></li><li>Optional: stack traces when <code>OPENIMAGEIO_DEBUG_PYTHON</code> is set (<code>Sysutil</code>)</li><li><code>make_pyobject</code>: no pybind-style <code>debugfmt</code> when the type is unhandled (returns default quietly)</li></ul> |
| `__init__.py` (package) | Env / DLL path setup, <code>from ._OpenImageIO import *</code>, version docstring. | <strong>TODO:</strong> Python CLI entry-point trampolines when the install layout matches the full wheel. |

---

## Not migrated — entire pybind modules

These exist only under `src/python/` today; there are **no** corresponding `py_*.cpp` files in `src/python-nanobind/`.

| Source file | Python / C++ API |
| --- | --- |
| `py_imageinput.cpp` | <ul><li><code>ImageInput</code></li><li>open, read, formats, …</li></ul> |
| `py_imageoutput.cpp` | <code>ImageOutput</code> |
| `py_imagebuf.cpp` | <code>ImageBuf</code> |
| `py_imagebufalgo.cpp` | <ul><li><code>ImageBufAlgo</code> (namespace)</li><li><code>PixelStats</code></li><li><code>CompareResults</code></li><li>Exposed <code>IBA_*</code> helpers</li></ul> |
| `py_texturesys.cpp` | <ul><li><code>Wrap</code></li><li><code>MipMode</code></li><li><code>InterpMode</code></li><li><code>TextureOpt</code></li><li><code>TextureSystem</code></li></ul> |
| `py_imagecache.cpp` | <code>ImageCache</code> (wrapped) |
| `py_colorconfig.cpp` | <code>ColorConfig</code> |
| `py_deepdata.cpp` | <code>DeepData</code> |

---

## Conventions

When adding coverage, prefer mirroring the existing `declare_*` split in `src/python/` unless a file becomes too large.

Extend **testsuite** coverage for any migrated code that is not already covered, so **parity with pybind11** is demonstrated rather than only claimed. Follow the existing `testsuite/python-*` scripts and `ref/out.txt` pattern where applicable.
