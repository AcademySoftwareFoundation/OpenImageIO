// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

#include <cstring>
#include <type_traits>

#if defined(OIIO_PY_BACKEND_NANOBIND)
#    include <OpenImageIO/oiioversion.h>
#endif
#include <OpenImageIO/sysutil.h>

namespace PyOpenImageIO {


#if 0 /* unused */
const char*
python_array_code(TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::UINT8: return "uint8";
    case TypeDesc::INT8: return "int8";
    case TypeDesc::UINT16: return "uint16";
    case TypeDesc::INT16: return "int16";
    case TypeDesc::UINT32: return "uint32";
    case TypeDesc::INT32: return "int32";
    case TypeDesc::FLOAT: return "float";
    case TypeDesc::DOUBLE: return "double";
    case TypeDesc::HALF: return "half";
    default:
        // For any other type, including UNKNOWN, pack it into an
        // unsigned byte array.
        return "B";
    }
}
#endif


TypeDesc
typedesc_from_python_array_code(string_view code)
{
    TypeDesc t(code);
    if (!t.is_unknown())
        return t;

    if (code == "b" || code == "c")
        return TypeDesc::INT8;
    if (code == "B")
        return TypeDesc::UINT8;
    if (code == "h")
        return TypeDesc::INT16;
    if (code == "H")
        return TypeDesc::UINT16;
    if (code == "i")
        return TypeDesc::INT;
    if (code == "I")
        return TypeDesc::UINT;
    if (code == "l")
        return TypeDesc::INT64;
    if (code == "L")
        return TypeDesc::UINT64;
    if (code == "f")
        return TypeDesc::FLOAT;
    if (code == "d")
        return TypeDesc::DOUBLE;
    if (code == "float16" || code == "e")
        return TypeDesc::HALF;
    return TypeDesc::UNKNOWN;
}



oiio_py_buffer_view
oiio_py_request_buffer(const py::object& obj)
{
    oiio_py_buffer_view info;
#if defined(OIIO_PY_BACKEND_NANOBIND)
    Py_buffer view;
    if (PyObject_GetBuffer(obj.ptr(), &view, PyBUF_STRIDES | PyBUF_FORMAT)
        != 0) {
        PyErr_Clear();
        return info;
    }
    info.ptr      = view.buf;
    info.itemsize = view.itemsize;
    info.ndim     = view.ndim;
    if (view.ndim > 0) {
        info.shape.assign(view.shape, view.shape + view.ndim);
        info.strides.assign(view.strides, view.strides + view.ndim);
        info.size = 1;
        for (int i = 0; i < view.ndim; ++i) {
            info.size *= view.shape[i];
        }
    } else {
        info.size = view.len / std::max<Py_ssize_t>(view.itemsize, 1);
    }
    if (view.format && view.format[0]) {
        info.format = view.format;
    }
    PyBuffer_Release(&view);
#else
    const py::buffer_info req = py::cast<py::buffer>(obj).request();
    info.ptr                  = req.ptr;
    info.itemsize             = req.itemsize;
    info.size                 = req.size;
    info.ndim                 = req.ndim;
    info.shape.assign(req.shape.begin(), req.shape.end());
    info.strides.assign(req.strides.begin(), req.strides.end());
    info.format = req.format;
#endif
    return info;
}



oiio_bufinfo::oiio_bufinfo(const oiio_py_buffer_view& pybuf)
{
    if (pybuf.format.size()) {
        format = typedesc_from_python_array_code(pybuf.format);
    }
    if (format != TypeUnknown) {
        data    = pybuf.ptr;
        xstride = format.size();
        size    = 1;
        for (int i = pybuf.ndim - 1; i >= 0; --i) {
            if (pybuf.strides[i] != Py_ssize_t(size * xstride)) {
                // Just can't handle non-contiguous strides
                format = TypeUnknown;
                size   = 0;
                break;
            }
            size *= pybuf.shape[i];
        }
    }
}



oiio_bufinfo::oiio_bufinfo(const oiio_py_buffer_view& pybuf, int nchans,
                           int width, int height, int depth, int pixeldims)
{
    if (pybuf.format.size()) {
        format = typedesc_from_python_array_code(pybuf.format);
    }
    if (size_t(pybuf.itemsize) != format.size()
        || pybuf.size
               != int64_t(width) * int64_t(height) * int64_t(depth * nchans)) {
        format = TypeUnknown;  // Something went wrong
        error  = Strutil::fmt::format(
            "buffer is wrong size (expected {}x{}x{}x{}, got total {})", depth,
            height, width, nchans, pybuf.size);
        return;
    }
    size = pybuf.size;
    if (pixeldims == 3) {
        // Reading a 3D volumetric cube
        if (pybuf.ndim == 4 && pybuf.shape[0] == depth
            && pybuf.shape[1] == height && pybuf.shape[2] == width
            && pybuf.shape[3] == nchans) {
            // passed from python as [z][y][x][c]
            xstride = pybuf.strides[2];
            ystride = pybuf.strides[1];
            zstride = pybuf.strides[0];
        } else if (pybuf.ndim == 3 && pybuf.shape[0] == depth
                   && pybuf.shape[1] == height
                   && pybuf.shape[2] == int64_t(width) * int64_t(nchans)) {
            // passed from python as [z][y][xpixel] -- chans mushed together
            xstride = pybuf.strides[2];
            ystride = pybuf.strides[1];
            zstride = pybuf.strides[0];
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = "Bad dimensions of pixel data";
        }
    } else if (pixeldims == 2) {
        // Reading an 2D image rectangle
        if (pybuf.ndim == 3 && pybuf.shape[0] == height
            && pybuf.shape[1] == width && pybuf.shape[2] == nchans) {
            // passed from python as [y][x][c]
            xstride = pybuf.strides[1];
            ystride = pybuf.strides[0];
        } else if (pybuf.ndim == 2) {
            // Somebody collapsed a dimension. Is it [pixel][c] with x&y
            // combined, or is it [y][xpixel] with channels mushed together?
            if (pybuf.shape[0] == int64_t(width) * int64_t(height)
                && pybuf.shape[1] == nchans) {
                xstride = pybuf.strides[0];
            } else if (pybuf.shape[0] == height
                       && pybuf.shape[1] == int64_t(width) * int64_t(nchans)) {
                ystride = pybuf.strides[1];
                xstride = pybuf.strides[0] * nchans;
            } else {
                format = TypeUnknown;  // error
                error  = Strutil::fmt::format(
                    "Can't figure out array shape (pixeldims={}, pydim={})",
                    pixeldims, pybuf.ndim);
            }
        } else if (pybuf.ndim == 1
                   && pybuf.shape[0]
                          == int64_t(width) * int64_t(height)
                                 * int64_t(nchans)) {
            // all pixels & channels smushed together
            // just rely on autostride
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = Strutil::fmt::format(
                "Python array shape is [{:,}] but expecting h={}, w={}, ch={}",
                cspan<const Py_ssize_t>(pybuf.shape.data(), pybuf.shape.size()),
                height, width, nchans);
        }
    } else if (pixeldims == 1) {
        // Reading a 1D scanline span
        if (pybuf.ndim == 2 && pybuf.shape[0] == width
            && pybuf.shape[1] == nchans) {
            // passed from python as [x][c]
            xstride = pybuf.strides[0];
        } else if (pybuf.ndim == 1
                   && pybuf.shape[0] == int64_t(width) * int64_t(nchans)) {
            // all pixels & channels smushed together
            xstride = pybuf.strides[0] * nchans;
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = Strutil::fmt::format(
                "Can't figure out array shape (pixeldims={}, pydim={})",
                pixeldims, pybuf.ndim);
        }
    } else {
        error = Strutil::fmt::format(
            "Can't figure out array shape (pixeldims={}, pydim={})", pixeldims,
            pybuf.ndim);
    }

    if (nchans > 1 && format.size() && pybuf.ndim > 0
        && size_t(pybuf.strides.back()) != format.size()) {
        format = TypeUnknown;  // can't handle noncontig channels
        error  = "Can't handle numpy array with noncontiguous channels";
    }
    if (format != TypeUnknown) {
        data = pybuf.ptr;
    }
}



namespace {

    // Shared image layout → NumPy shape (C-contiguous pixel data).
    void numpy_image_shape(std::vector<size_t>& shape, int dims, size_t chans,
                           size_t width, size_t height, size_t depth,
                           size_t size)
    {
        if (dims == 4) {  // volumetric
            shape.assign({ depth, height, width, chans });
        } else if (dims == 3 && depth == 1) {  // 2D+channels
            shape.assign({ height, width, chans });
        } else if (dims == 2 && depth == 1
                   && height == 1) {  // 1D (scanline) + channels
            shape.assign({ width, chans });
        } else {  // punt -- make it a 1D array
            shape.assign({ size });
        }
    }



    template<class T>
    py::object make_numpy_array_t(T* data, int dims, size_t chans, size_t width,
                                  size_t height, size_t depth)
    {
        const size_t size = chans * width * height * depth;
        T* mem            = data ? data : new T[size];
        std::vector<size_t> shape;
        numpy_image_shape(shape, dims, chans, width, height, depth, size);
#if defined(OIIO_PY_BACKEND_NANOBIND)
        py::capsule owner(mem, [](void* p) noexcept {
            delete[] reinterpret_cast<T*>(p);
        });
        // nullptr strides → C-contiguous (element strides).
        return py::cast(py::ndarray<py::numpy, T>(mem, shape.size(),
                                                  shape.data(), owner),
                        py::rv_policy::move);
#else
        // Create a Python object that will free the allocated memory when
        // destroyed. Shape-only ctor assumes C-contiguous byte layout.
        py::capsule free_when_done(mem, [](void* f) {
            delete[] (reinterpret_cast<T*>(f));
        });
        return py::array_t<T>(shape, mem, free_when_done);
#endif
    }



#if defined(OIIO_PY_BACKEND_NANOBIND)
    // Build a real numpy float16 array (nanobind has no half dtype).
    template<>
    py::object make_numpy_array_t<half>(half* data, int dims, size_t chans,
                                        size_t width, size_t height,
                                        size_t depth)
    {
        const size_t size = chans * width * height * depth;
        half* mem         = data ? data : new half[size];
        std::vector<size_t> shape;
        numpy_image_shape(shape, dims, chans, width, height, depth, size);
        py::object np = py::module_::import_("numpy");
        py::list shape_list;
        for (size_t d : shape)
            shape_list.append(d);
        py::object arr = np.attr("empty")(py::tuple(shape_list), "float16");
        // Write through a uint16 view of the same buffer.
        auto u16 = py::cast<py::ndarray<py::numpy, uint16_t>>(
            arr.attr("view")(np.attr("uint16")));
        std::memcpy(u16.data(), mem, size * sizeof(half));
        delete[] mem;
        return arr;
    }
#endif

}  // namespace



template<class T>
py::object
make_numpy_array(T* data, int dims, size_t chans, size_t width, size_t height,
                 size_t depth)
{
    return make_numpy_array_t(data, dims, chans, width, height, depth);
}



py::object
make_numpy_array(TypeDesc format, void* data, int dims, size_t chans,
                 size_t width, size_t height, size_t depth)
{
    if (format == TypeDesc::FLOAT) {
        return make_numpy_array((float*)data, dims, chans, width, height,
                                depth);
    }
    if (format == TypeDesc::UINT8) {
        return make_numpy_array((unsigned char*)data, dims, chans, width,
                                height, depth);
    }
    if (format == TypeDesc::UINT16) {
        return make_numpy_array((unsigned short*)data, dims, chans, width,
                                height, depth);
    }
    if (format == TypeDesc::INT8) {
        return make_numpy_array((char*)data, dims, chans, width, height, depth);
    }
    if (format == TypeDesc::INT16) {
        return make_numpy_array((short*)data, dims, chans, width, height,
                                depth);
    }
    if (format == TypeDesc::DOUBLE) {
        return make_numpy_array((double*)data, dims, chans, width, height,
                                depth);
    }
    if (format == TypeDesc::HALF) {
        return make_numpy_array((half*)data, dims, chans, width, height, depth);
    }
    if (format == TypeDesc::UINT) {
        return make_numpy_array((unsigned int*)data, dims, chans, width, height,
                                depth);
    }
    if (format == TypeDesc::INT) {
        return make_numpy_array((int*)data, dims, chans, width, height, depth);
    }
    delete[] (char*)data;
    return py::none();
}



py::object
make_pyobject(const void* data, TypeDesc type, int nvalues,
              py::object defaultvalue)
{
    if (!data || !nvalues) {
        return oiio_py::return_object(defaultvalue);
    }
    if (type.basetype == TypeDesc::INT32)
        return C_to_val_or_tuple((const int*)data, type, nvalues);
    if (type.basetype == TypeDesc::FLOAT)
        return C_to_val_or_tuple((const float*)data, type, nvalues);
    if (type.basetype == TypeDesc::STRING)
        return C_to_val_or_tuple((const char**)data, type, nvalues);
    if (type.basetype == TypeDesc::UINT32)
        return C_to_val_or_tuple((const unsigned int*)data, type, nvalues);
    if (type.basetype == TypeDesc::INT16)
        return C_to_val_or_tuple((const short*)data, type, nvalues);
    if (type.basetype == TypeDesc::UINT16)
        return C_to_val_or_tuple((const unsigned short*)data, type, nvalues);
    if (type.basetype == TypeDesc::INT64)
        return C_to_val_or_tuple((const int64_t*)data, type, nvalues);
    if (type.basetype == TypeDesc::UINT64)
        return C_to_val_or_tuple((const uint64_t*)data, type, nvalues);
    if (type.basetype == TypeDesc::DOUBLE)
        return C_to_val_or_tuple((const double*)data, type, nvalues);
    if (type.basetype == TypeDesc::HALF)
        return C_to_val_or_tuple((const half*)data, type, nvalues);
    if (type.basetype == TypeDesc::UINT8 && type.arraylen > 0) {
        // Array of uint8 bytes
        // Have to make a copy of the data, because make_numpy_array will
        // take possession of it.
        int n = type.arraylen * nvalues;
        if (n <= 0)
            return oiio_py::return_object(defaultvalue);
        auto* copy = new uint8_t[n];
        std::memcpy(copy, data, static_cast<size_t>(n));
        return oiio_py::make_numpy_array(copy, static_cast<size_t>(n));
    }
    if (type.basetype == TypeDesc::UINT8)
        return C_to_val_or_tuple((const unsigned char*)data, type, nvalues);
    debugfmt("Don't know how to handle type {}\n", type);
    return oiio_py::return_object(defaultvalue);
}



static py::object
oiio_getattribute_typed(const std::string& name, TypeDesc type = TypeUnknown)
{
    if (type == TypeUnknown)
        return py::none();
    char* data = OIIO_ALLOCA(char, type.size());
    if (!OIIO::getattribute(name, type, data))
        return py::none();
    return make_pyobject(data, type);
}


// Wrapper to let attribute_typed/attribute_onearg work for global attributes.
struct oiio_global_attrib_wrapper {
    bool attribute(string_view name, TypeDesc type, const void* data)
    {
        return OIIO::attribute(name, type, data);
    }
    bool attribute(string_view name, int val)
    {
        return OIIO::attribute(name, val);
    }
    bool attribute(string_view name, float val)
    {
        return OIIO::attribute(name, val);
    }
    bool attribute(string_view name, const std::string& val)
    {
        return OIIO::attribute(name, val);
    }
};


static void
declare_global_bindings(py_module& m)
{
    declare_typedesc(m);
    declare_paramvalue(m);
    declare_imagespec(m);
    declare_roi(m);
    declare_deepdata(m);
    declare_colorconfig(m);

    // Main OIIO I/O classes
    declare_imageinput(m);
    declare_imageoutput(m);
    declare_imagebuf(m);
    declare_imagecache(m);

    // TextureSys classes
    declare_wrap(m);
    declare_mipmpode(m);
    declare_interpmode(m);
    declare_textureopt(m);
    declare_texturesystem(m);

    declare_imagebufalgo(m);
}



static void
declare_global_attribute_functions(py_module& m)
{
    m.def("attribute", [](const std::string& name, const py::object& obj) {
        oiio_global_attrib_wrapper wrapper;
        attribute_onearg(wrapper, name, obj);
    });
    m.def("attribute",
          [](const std::string& name, TypeDesc type, const py::object& obj) {
              oiio_global_attrib_wrapper wrapper;
              attribute_typed(wrapper, name, type, obj);
          });

    m.def(
        "get_int_attribute",
        [](const std::string& name, int def) {
            return OIIO::get_int_attribute(name, def);
        },
        "name"_a, "defaultval"_a = 0);
    m.def(
        "get_float_attribute",
        [](const std::string& name, float def) {
            return OIIO::get_float_attribute(name, def);
        },
        "name"_a, "defaultval"_a = 0.0f);
    m.def(
        "get_string_attribute",
        [](const std::string& name, const std::string& def) {
            return oiio_py::str(OIIO::get_string_attribute(name, def));
        },
        "name"_a, "defaultval"_a = "");
    m.def("getattribute", &oiio_getattribute_typed, "name"_a,
          "type"_a = TypeUnknown);
    m.def("geterror", &OIIO::geterror, "clear"_a = true);
    m.def(
        "get_bytes_attribute",
        [](const std::string& name, const py::object& def) {
            // Accept str, bytes, or None (None → empty default).
            std::string defstr;
            if (!def.is_none()) {
                if (PyBytes_Check(def.ptr())) {
                    defstr = oiio_py::bytes_to_stdstring(
                        py::cast<py::bytes>(def));
                } else {
                    defstr = oiio_py::str_to_stdstring(def);
                }
            }
            std::string s(OIIO::get_string_attribute(name, defstr));
            return py::bytes(s.data(), s.size());
        },
        "name"_a, "defaultval"_a.none() = "");
    m.def(
        "set_colorspace",
        [](ImageSpec& spec, const std::string& name) {
            set_colorspace(spec, name);
        },
        "spec"_a, "name"_a);
    m.def(
        "set_colorspace_rec709_gamma",
        [](ImageSpec& spec, float gamma) {
            set_colorspace_rec709_gamma(spec, gamma);
        },
        "spec"_a, "gamma"_a);
    m.def(
        "equivalent_colorspace",
        [](const std::string& a, const std::string& b) {
            return equivalent_colorspace(a, b);
        },
        "a"_a, "b"_a);
    m.def(
        "is_imageio_format_name",
        [](const std::string& name) {
            return OIIO::is_imageio_format_name(name);
        },
        "name"_a);
}



static void
declare_module_attributes(py_module& m)
{
    m.attr("AutoStride")          = AutoStride;
    m.attr("openimageio_version") = OIIO_VERSION;
    m.attr("VERSION")             = OIIO_VERSION;
    m.attr("VERSION_STRING")      = oiio_py::str(OIIO_VERSION_STRING);
    m.attr("VERSION_MAJOR")       = OIIO_VERSION_MAJOR;
    m.attr("VERSION_MINOR")       = OIIO_VERSION_MINOR;
    m.attr("VERSION_PATCH")       = OIIO_VERSION_PATCH;
    m.attr("INTRO_STRING")        = oiio_py::str(OIIO_INTRO_STRING);
    m.attr("__version__")         = oiio_py::str(OIIO_VERSION_STRING);
}



#if defined(OIIO_PY_BACKEND_NANOBIND)

}  // namespace PyOpenImageIO

#    define OIIO_DECLARE_NB_MODULE(x) NB_MODULE(x, m)

#    if defined(OIIO_PY_NANOBIND_ISOLATED_PACKAGE)
OIIO_DECLARE_NB_MODULE(_OpenImageIO)
#    else
OIIO_DECLARE_NB_MODULE(OpenImageIO)
#    endif
{
    m.doc() = "OpenImageIO nanobind bindings.";

    PyOpenImageIO::declare_global_bindings(m);
    PyOpenImageIO::declare_global_attribute_functions(m);
    PyOpenImageIO::declare_module_attributes(m);
}

#else  // pybind11

// This OIIO_DECLARE_PYMODULE mojo is necessary if we want to pass in the
// MODULE name as a #define. Google for Argument-Prescan for additional
// info on why this is necessary

#    define OIIO_DECLARE_PYMODULE(x) PYBIND11_MODULE(x, m)

OIIO_DECLARE_PYMODULE(PYMODULE_NAME)
{
    if (Sysutil::getenv("OPENIMAGEIO_DEBUG_PYTHON") != "")
        Sysutil::setup_crash_stacktrace("stdout");

    declare_global_bindings(m);
    declare_global_attribute_functions(m);
    declare_module_attributes(m);
}

}  // namespace PyOpenImageIO

#endif  // OIIO_PY_BACKEND_NANOBIND
