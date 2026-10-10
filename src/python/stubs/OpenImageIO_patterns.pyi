# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# OIIO pattern file for nanobind stubgen (#5489).
# Ported from the OIIOSignatureGenerator overrides in the former
# mypy-stubgen-based generate_stubs.py that this replaces.
#
# A query is a regex matched against the fully qualified member name; when it
# matches, the indented lines below take over the member's rendering entirely.
# Queries must be end-anchored ($) so e.g. read_tile does not also match
# read_tiles: stubgen applies the first matching pattern in file order.
#
# The mypy matcher expressed the TypeDesc-implicit-conversion rule cross-cuttingly
# (by argument type); here it is spelled out per member. Cosmetic differences from
# the mypy-based stubs remain (literal defaults instead of "...", positional-only
# markers, mypy's SupportsInt/SupportsFloat canonicalization).

# --- signature_overrides: __eq__/__ne__ should accept any object ---
OpenImageIO\.TypeDesc\.__eq__$:
    def __eq__(self, arg: object, /) -> bool: ...

OpenImageIO\.TypeDesc\.__ne__$:
    def __ne__(self, arg: object, /) -> bool: ...

OpenImageIO\.ROI\.__eq__$:
    def __eq__(self, arg: object, /) -> bool: ...

OpenImageIO\.ROI\.__ne__$:
    def __ne__(self, arg: object, /) -> bool: ...

# --- result_type_overrides: create/open return None on failure ---
OpenImageIO\.ImageInput\.create$:
    @staticmethod
    def create(filename: str, plugin_searchpath: str = '') -> ImageInput | None: ...

OpenImageIO\.ImageInput\.open$:
    @overload
    @staticmethod
    def open(filename: str) -> ImageInput | None: ...
    @overload
    @staticmethod
    def open(filename: str, config: ImageSpec) -> ImageInput | None: ...

OpenImageIO\.ImageOutput\.create$:
    @staticmethod
    def create(filename: str, plugin_searchpath: str = '') -> ImageOutput | None: ...

# --- result_type_overrides: pixel reads return numpy arrays ---
OpenImageIO\.ImageInput\.read_image$:
    \import numpy
    @overload
    def read_image(self, subimage: int, miplevel: int, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...
    @overload
    def read_image(self, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...
    @overload
    def read_image(self, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

OpenImageIO\.ImageInput\.read_scanline$:
    \import numpy
    def read_scanline(self, y: int, z: int = 0, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

OpenImageIO\.ImageInput\.read_scanlines$:
    \import numpy
    @overload
    def read_scanlines(self, subimage: int, miplevel: int, ybegin: int, yend: int, z: int, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...
    @overload
    def read_scanlines(self, ybegin: int, yend: int, z: int, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

OpenImageIO\.ImageInput\.read_tile$:
    \import numpy
    def read_tile(self, x: int, y: int, z: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

OpenImageIO\.ImageInput\.read_tiles$:
    \import numpy
    @overload
    def read_tiles(self, subimage: int, miplevel: int, xbegin: int, xend: int, ybegin: int, yend: int, zbegin: int, zend: int, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...
    @overload
    def read_tiles(self, xbegin: int, xend: int, ybegin: int, yend: int, zbegin: int, zend: int, chbegin: int, chend: int, format: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

# --- read_native_deep_* can return None (uninitialized unique_ptr, see #4685) ---
OpenImageIO\.ImageInput\.read_native_deep_scanlines$:
    def read_native_deep_scanlines(self, subimage: int, miplevel: int, ybegin: int, yend: int, z: int, chbegin: int, chend: int) -> DeepData | None: ...

OpenImageIO\.ImageInput\.read_native_deep_tiles$:
    def read_native_deep_tiles(self, subimage: int, miplevel: int, xbegin: int, xend: int, ybegin: int, yend: int, zbegin: int, zend: int, chbegin: int, chend: int) -> DeepData | None: ...

OpenImageIO\.ImageInput\.read_native_deep_image$:
    def read_native_deep_image(self, subimage: int = 0, miplevel: int = 0) -> DeepData | None: ...

# --- result_type_overrides: get_pixels ---
OpenImageIO\.ImageBuf\.get_pixels$:
    \import numpy
    def get_pixels(self, format: TypeDesc | BASETYPE | str = ..., roi: ROI = ...) -> numpy.ndarray | None: ...

OpenImageIO\.ImageCache\.get_pixels$:
    \import numpy
    @overload
    def get_pixels(self, filename: str, subimage: int, miplevel: int, xbegin: int, xend: int, ybegin: int, yend: int, zbegin: int = 0, zend: int = 1, datatype: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...
    @overload
    def get_pixels(self, filename: str, subimage: int, miplevel: int, roi: ROI, datatype: TypeDesc | BASETYPE | str = ...) -> numpy.ndarray | None: ...

# --- result_type_overrides: TextureSystem ---
OpenImageIO\.TextureSystem\.imagespec$:
    def imagespec(self, filename: str, subimage: int = 0) -> ImageSpec | None: ...

OpenImageIO\.TextureSystem\.texture$:
    def texture(self, filename: str, options: TextureOpt, s: float, t: float, dsdx: float, dtdx: float, dsdy: float, dtdy: float, nchannels: int) -> tuple[float, ...]: ...

OpenImageIO\.TextureSystem\.texture3d$:
    def texture3d(self, filename: str, options: TextureOpt, P: Sequence[float], dPdx: Sequence[float], dPdy: Sequence[float], dPdz: Sequence[float], nchannels: int) -> tuple[float, ...]: ...

OpenImageIO\.TextureSystem\.environment$:
    def environment(self, filename: str, options: TextureOpt, R: Sequence[float], dRdx: Sequence[float], dRdy: Sequence[float], nchannels: int) -> tuple[float, ...]: ...

# --- result_type_overrides: ImageBuf pixel accessors return tuple[float, ...] ---
OpenImageIO\.ImageBuf\.getpixel$:
    def getpixel(self, x: int, y: int, z: int = 0, wrap: str = 'black') -> tuple[float, ...]: ...

OpenImageIO\.ImageBuf\.interppixel$:
    def interppixel(self, x: float, y: float, wrap: str = 'black') -> tuple[float, ...]: ...

OpenImageIO\.ImageBuf\.interppixel_NDC$:
    def interppixel_NDC(self, x: float, y: float, wrap: str = 'black') -> tuple[float, ...]: ...

OpenImageIO\.ImageBuf\.interppixel_NDC_full$:
    def interppixel_NDC_full(self, x: float, y: float, wrap: str = 'black') -> tuple[float, ...]: ...

OpenImageIO\.ImageBuf\.interppixel_bicubic$:
    def interppixel_bicubic(self, x: float, y: float, wrap: str = 'black') -> tuple[float, ...]: ...

OpenImageIO\.ImageBuf\.interppixel_bicubic_NDC$:
    def interppixel_bicubic_NDC(self, x: float, y: float, wrap: str = 'black') -> tuple[float, ...]: ...

# --- result_type_overrides: ImageSpec helpers ---
OpenImageIO\.ImageSpec\.get$:
    \import typing
    def get(self, key: str, default: object | None = None) -> typing.Any: ...

OpenImageIO\.ImageSpec\.get_channelformats$:
    def get_channelformats(self) -> tuple[TypeDesc, ...]: ...

# --- result_type_overrides: getattribute returns Any; its TypeDesc arg accepts
# --- implicit conversions (BASETYPE & str) ---
OpenImageIO\.ImageSpec\.getattribute$:
    \import typing
    def getattribute(self, name: str, type: TypeDesc | BASETYPE | str = ...) -> typing.Any: ...

OpenImageIO\.ImageCache\.getattribute$:
    \import typing
    def getattribute(self, name: str, type: TypeDesc | BASETYPE | str = ...) -> typing.Any: ...

OpenImageIO\.TextureSystem\.getattribute$:
    \import typing
    def getattribute(self, name: str, type: TypeDesc | BASETYPE | str = ...) -> typing.Any: ...

OpenImageIO\.getattribute$:
    \import typing
    def getattribute(arg0: str, arg1: TypeDesc | BASETYPE | str, /) -> typing.Any: ...

# --- result_type_overrides: ImageBufAlgo returning tuples or objects ---
OpenImageIO\.ImageBufAlgo\.histogram$:
    @staticmethod
    def histogram(src: ImageBuf, channel: int = 0, bins: int = 256, min: float = 0.0, max: float = 1.0, ignore_empty: bool = False, roi: ROI = ..., nthreads: int = 0) -> tuple[int, ...]: ...

OpenImageIO\.ImageBufAlgo\.isConstantColor$:
    @staticmethod
    def isConstantColor(src: ImageBuf, threshold: float = 0.0, roi: ROI = ..., nthreads: int = 0) -> tuple[float, ...] | None: ...

OpenImageIO\.ImageBufAlgo\.color_range_check$:
    @staticmethod
    def color_range_check(src: ImageBuf, low: object, high: object, roi: ROI = ..., nthreads: int = 0) -> tuple[int, ...] | None: ...

# --- arg_type_overrides: std::vector<T> params accept scalars or iterables ---
OpenImageIO\.ImageBufAlgo\.clamp$:
    \import typing
    @overload
    @staticmethod
    def clamp(dst: ImageBuf, src: ImageBuf, min: float | typing.Iterable[float], max: float | typing.Iterable[float], clampalpha01: bool = False, roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def clamp(src: ImageBuf, min: float | typing.Iterable[float], max: float | typing.Iterable[float], clampalpha01: bool = False, roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...

OpenImageIO\.ImageBufAlgo\.contrast_remap$:
    \import typing
    @overload
    @staticmethod
    def contrast_remap(dst: ImageBuf, src: ImageBuf, black: float | typing.Iterable[float] = 0.0, white: float | typing.Iterable[float] = 1.0, min: float | typing.Iterable[float] = 0.0, max: float | typing.Iterable[float] = 1.0, scontrast: float | typing.Iterable[float] = 1.0, sthresh: float | typing.Iterable[float] = 0.5, roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def contrast_remap(src: ImageBuf, black: float | typing.Iterable[float] = 0.0, white: float | typing.Iterable[float] = 1.0, min: float | typing.Iterable[float] = 0.0, max: float | typing.Iterable[float] = 1.0, scontrast: float | typing.Iterable[float] = 1.0, sthresh: float | typing.Iterable[float] = 0.5, roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...

OpenImageIO\.ImageBufAlgo\.fill$:
    \import typing
    @overload
    @staticmethod
    def fill(dst: ImageBuf, values: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def fill(dst: ImageBuf, top: float | typing.Iterable[float], bottom: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def fill(dst: ImageBuf, topleft: float | typing.Iterable[float], topright: float | typing.Iterable[float], bottomleft: float | typing.Iterable[float], bottomright: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def fill(values: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...
    @overload
    @staticmethod
    def fill(top: float | typing.Iterable[float], bottom: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...
    @overload
    @staticmethod
    def fill(topleft: float | typing.Iterable[float], topright: float | typing.Iterable[float], bottomleft: float | typing.Iterable[float], bottomright: float | typing.Iterable[float], roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...

OpenImageIO\.ImageBufAlgo\.demosaic$:
    \import typing
    @overload
    @staticmethod
    def demosaic(dst: ImageBuf, src: ImageBuf, pattern: str = '', algorithm: str = '', layout: str = '', white_balance_mode: str = '', white_balance: float | typing.Iterable[float] = ..., roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def demosaic(src: ImageBuf, pattern: str = '', algorithm: str = '', layout: str = '', white_balance_mode: str = '', white_balance: float | typing.Iterable[float] = ..., roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...

OpenImageIO\.ImageBufAlgo\.render_point$:
    \import typing
    @staticmethod
    def render_point(dst: ImageBuf, x: int, y: int, color: float | typing.Iterable[float] = ...) -> bool: ...

OpenImageIO\.ImageBufAlgo\.render_line$:
    \import typing
    @staticmethod
    def render_line(dst: ImageBuf, x1: int, y1: int, x2: int, y2: int, color: float | typing.Iterable[float] = ..., skip_first_point: bool = False) -> bool: ...

OpenImageIO\.ImageBufAlgo\.render_box$:
    \import typing
    @staticmethod
    def render_box(dst: ImageBuf, x1: int, y1: int, x2: int, y2: int, color: float | typing.Iterable[float] = ..., fill: bool = False) -> bool: ...

# --- workaround: nanobind stubgen renders these signatures syntactically invalid
# --- (a non-default parameter follows a defaulted one)
OpenImageIO\.ColorConfig\.getViewNameByIndex$:
    def getViewNameByIndex(self, display: str = '', index: int = 0) -> str: ...

OpenImageIO\.ColorConfig\.getDefaultViewName$:
    def getDefaultViewName(self, display: str = '', input_color_space: str = '') -> str: ...

OpenImageIO\.ImageBuf\.deep_insert_samples$:
    def deep_insert_samples(self, x: int, y: int, z: int = 0, samplepos: int = 0, nsamples: int = 1) -> None: ...

OpenImageIO\.ImageBuf\.deep_erase_samples$:
    def deep_erase_samples(self, x: int, y: int, z: int = 0, samplepos: int = 0, nsamples: int = 1) -> None: ...

# --- arg_type_overrides: TypeDesc params accept implicit conversions (BASETYPE & str) ---
OpenImageIO\.TypeDesc\.__init__$:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, arg: TypeDesc | BASETYPE | str, /) -> None: ...
    @overload
    def __init__(self, arg: BASETYPE, /) -> None: ...
    @overload
    def __init__(self, arg0: BASETYPE, arg1: AGGREGATE, /) -> None: ...
    @overload
    def __init__(self, arg0: BASETYPE, arg1: AGGREGATE, arg2: VECSEMANTICS, /) -> None: ...
    @overload
    def __init__(self, arg0: BASETYPE, arg1: AGGREGATE, arg2: VECSEMANTICS, arg3: int, /) -> None: ...
    @overload
    def __init__(self, arg: str, /) -> None: ...
OpenImageIO\.TypeDesc\.equivalent$:
    def equivalent(self, arg: TypeDesc | BASETYPE | str, /) -> bool: ...

OpenImageIO\.ParamValue\.__init__$:
    @overload
    def __init__(self, arg0: str, arg1: int, /) -> None: ...
    @overload
    def __init__(self, arg0: str, arg1: float, /) -> None: ...
    @overload
    def __init__(self, arg0: str, arg1: str, /) -> None: ...
    @overload
    def __init__(self, name: str, type: TypeDesc | BASETYPE | str, value: object) -> None: ...
    @overload
    def __init__(self, name: str, type: TypeDesc | BASETYPE | str, nvalues: int, interp: Interp, value: object) -> None: ...
OpenImageIO\.ParamValueList\.attribute$:
    @overload
    def attribute(self, name: str, val: object) -> None: ...
    @overload
    def attribute(self, arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: object, /) -> None: ...
    @overload
    def attribute(self, arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: int, arg3: object, /) -> None: ...
OpenImageIO\.ParamValueList\.contains$:
    def contains(self, name: str, type: TypeDesc | BASETYPE | str = ..., casesensitive: bool = True) -> bool: ...

OpenImageIO\.ParamValueList\.remove$:
    def remove(self, name: str, type: TypeDesc | BASETYPE | str = ..., casesensitive: bool = True) -> None: ...

OpenImageIO\.ImageSpec\.attribute$:
    @overload
    def attribute(self, arg0: str, arg1: object, /) -> None: ...
    @overload
    def attribute(self, arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: object, /) -> None: ...
OpenImageIO\.ImageSpec\.erase_attribute$:
    def erase_attribute(self, name: str = '', type: TypeDesc | BASETYPE | str = ..., casesensitive: bool = False) -> None: ...

OpenImageIO\.ImageSpec\.format$:
    @overload
    def format(self) -> TypeDesc: ...
    @overload
    def format(self, arg: TypeDesc | BASETYPE | str, /) -> None: ...

OpenImageIO\.ImageSpec\.set_format$:
    def set_format(self, arg: TypeDesc | BASETYPE | str, /) -> None: ...

OpenImageIO\.ImageSpec\.scanline_bytes$:
    @overload
    def scanline_bytes(self, native: bool = False) -> int: ...
    @overload
    def scanline_bytes(self, arg: TypeDesc | BASETYPE | str, /) -> int: ...
OpenImageIO\.ImageSpec\.tile_bytes$:
    @overload
    def tile_bytes(self, native: bool = False) -> int: ...
    @overload
    def tile_bytes(self, arg: TypeDesc | BASETYPE | str, /) -> int: ...
OpenImageIO\.ImageSpec\.image_bytes$:
    @overload
    def image_bytes(self, native: bool = False) -> int: ...
    @overload
    def image_bytes(self, native: TypeDesc | BASETYPE | str = ...) -> int: ...
OpenImageIO\.ImageSpec\.__init__$:
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, arg0: int, arg1: int, arg2: int, arg3: TypeDesc | BASETYPE | str, /) -> None: ...
    @overload
    def __init__(self, arg0: ROI, arg1: TypeDesc | BASETYPE | str, /) -> None: ...
    @overload
    def __init__(self, arg: TypeDesc | BASETYPE | str, /) -> None: ...
    @overload
    def __init__(self, arg: ImageSpec) -> None: ...
OpenImageIO\.ImageCache\.attribute$:
    @overload
    def attribute(self, arg0: str, arg1: object, /) -> None: ...
    @overload
    def attribute(self, arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: object, /) -> None: ...
OpenImageIO\.TextureSystem\.attribute$:
    @overload
    def attribute(self, arg0: str, arg1: object, /) -> None: ...
    @overload
    def attribute(self, arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: object, /) -> None: ...
OpenImageIO\.ImageBufAlgo\.copy$:
    @overload
    @staticmethod
    def copy(dst: ImageBuf, src: ImageBuf, convert: TypeDesc | BASETYPE | str = ..., roi: ROI = ..., nthreads: int = 0) -> bool: ...
    @overload
    @staticmethod
    def copy(src: ImageBuf, convert: TypeDesc | BASETYPE | str = ..., roi: ROI = ..., nthreads: int = 0) -> ImageBuf: ...

OpenImageIO\.ImageBuf\.copy$:
    @overload
    def copy(self, src: ImageBuf, format: TypeDesc | BASETYPE | str = ...) -> bool: ...
    @overload
    def copy(self, format: TypeDesc | BASETYPE | str = ...) -> ImageBuf: ...

OpenImageIO\.ImageBuf\.read$:
    @overload
    def read(self, subimage: int, miplevel: int, chbegin: int, chend: int, force: bool, convert: TypeDesc | BASETYPE | str) -> bool: ...
    @overload
    def read(self, subimage: int = 0, miplevel: int = 0, force: bool = False, convert: TypeDesc | BASETYPE | str = ...) -> bool: ...

OpenImageIO\.ImageBuf\.write$:
    @overload
    def write(self, filename: str, dtype: TypeDesc | BASETYPE | str = ..., fileformat: str = '') -> bool: ...
    @overload
    def write(self, out: ImageOutput) -> bool: ...
OpenImageIO\.attribute$:
    @overload
    def attribute(arg0: str, arg1: object, /) -> None: ...
    @overload
    def attribute(arg0: str, arg1: TypeDesc | BASETYPE | str, arg2: object, /) -> None: ...

# --- fix nanobind rendering quirks (Final inside ClassVar; C++-namespace annotation) ---
OpenImageIO\.ROI\.All$:
    \from typing import ClassVar
    All: ClassVar[ROI] = ...  # read-only

OpenImageIO\.ImageBuf\.getchannel$:
    def getchannel(self, x: int, y: int, z: int, c: int, wrap: str = 'black') -> float: ...
