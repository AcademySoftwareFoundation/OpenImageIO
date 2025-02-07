// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

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



oiio_bufinfo::oiio_bufinfo(const py::buffer_info& pybuf)
{
    if (pybuf.format.size())
        format = typedesc_from_python_array_code(pybuf.format);
    if (format != TypeUnknown) {
        data    = pybuf.ptr;
        xstride = format.size();
        size    = 1;
        for (int i = pybuf.ndim - 1; i >= 0; --i) {
            if (pybuf.strides[i] != py::ssize_t(size * xstride)) {
                // Just can't handle non-contiguous strides
                format = TypeUnknown;
                size   = 0;
                break;
            }
            size *= pybuf.shape[i];
        }
    }
}



oiio_bufinfo::oiio_bufinfo(const py::buffer_info& pybuf, int nchans, int width,
                           int height, int depth, int pixeldims)
{
    if (pybuf.format.size())
        format = typedesc_from_python_array_code(pybuf.format);
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
                && pybuf.shape[1] == nchans)
                xstride = pybuf.strides[0];
            else if (pybuf.shape[0] == height
                     && pybuf.shape[1] == int64_t(width) * int64_t(nchans)) {
                ystride = pybuf.strides[0];
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
                cspan<py::ssize_t>(pybuf.shape), height, width, nchans);
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

    if (nchans > 1 && format.size()
        && size_t(pybuf.strides.back()) != format.size()) {
        format = TypeUnknown;  // can't handle noncontig channels
        error  = "Can't handle numpy array with noncontiguous channels";
    }
    if (format != TypeUnknown)
        data = pybuf.ptr;
}



py::object
make_pyobject(const void* data, TypeDesc type, int nvalues,
              py::object defaultvalue)
{
    if (!data || !nvalues)
        return defaultvalue;
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
            return defaultvalue;
        uint8_t* ucdata(new uint8_t[n]);
        std::memcpy(ucdata, data, n);
        return make_numpy_array(ucdata, 1, 1, size_t(type.arraylen),
                                size_t(nvalues));
    }
    if (type.basetype == TypeDesc::UINT8)
        return C_to_val_or_tuple((const unsigned char*)data, type, nvalues);
    debugfmt("Don't know how to handle type {}\n", type);
    return defaultvalue;
}



static py::object
oiio_getattribute_typed(const std::string& name, TypeDesc type = TypeUnknown)
{
    if (type == TypeDesc::UNKNOWN)
        return py::none();
    char* data = OIIO_ALLOCA(char, type.size());
    if (!OIIO::getattribute(name, type, data))
        return py::none();
    return make_pyobject(data, type);
}


// Wrapper to let attribute_typed work for global attributes.
struct oiio_global_attrib_wrapper {
    bool attribute(string_view name, TypeDesc type, const void* data)
    {
        return OIIO::attribute(name, type, data);
    }
};



// This OIIO_DECLARE_PYMODULE mojo is necessary if we want to pass in the
// MODULE name as a #define. Google for Argument-Prescan for additional
// info on why this is necessary

#define OIIO_DECLARE_PYMODULE(x) PYBIND11_MODULE(x, m)

OIIO_DECLARE_PYMODULE(PYMODULE_NAME)
{
    using namespace pybind11::literals;

    if (Sysutil::getenv("OPENIMAGEIO_DEBUG_PYTHON") != "")
        Sysutil::setup_crash_stacktrace("stdout");

    // Basic helper classes
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

    // Global (OpenImageIO scope) functions and symbols
    m.def("geterror", &OIIO::geterror, "clear"_a = true);
    m.def("attribute", [](const std::string& name, float val) {
        OIIO::attribute(name, val);
    });
    m.def("attribute",
          [](const std::string& name, int val) { OIIO::attribute(name, val); });
    m.def("attribute", [](const std::string& name, const std::string& val) {
        OIIO::attribute(name, val);
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
        py::arg("name"), py::arg("defaultval") = 0);
    m.def(
        "get_float_attribute",
        [](const std::string& name, float def) {
            return OIIO::get_float_attribute(name, def);
        },
        py::arg("name"), py::arg("defaultval") = 0.0f);
    m.def(
        "get_string_attribute",
        [](const std::string& name, const std::string& def) {
            return PY_STR(std::string(OIIO::get_string_attribute(name, def)));
        },
        py::arg("name"), py::arg("defaultval") = "");
    m.def(
        "get_bytes_attribute",
        [](const std::string& name, const std::string& def) {
            return py::bytes(
                std::string(OIIO::get_string_attribute(name, def)));
        },
        py::arg("name"), py::arg("defaultval") = "");
    m.def("getattribute", &oiio_getattribute_typed);
    m.def(
        "set_colorspace",
        [](ImageSpec& spec, const std::string& name) {
            set_colorspace(spec, name);
        },
        py::arg("spec"), py::arg("name"));
    m.def("set_colorspace_rec709_gamma", [](ImageSpec& spec, float gamma) {
        set_colorspace_rec709_gamma(spec, gamma);
    });
    m.def("equivalent_colorspace",
          [](const std::string& a, const std::string& b) {
              return equivalent_colorspace(a, b);
          });
    m.def(
        "is_imageio_format_name",
        [](const std::string& name) {
            return OIIO::is_imageio_format_name(name);
        },
        py::arg("name"));
    m.attr("AutoStride")          = AutoStride;
    m.attr("openimageio_version") = OIIO_VERSION;
    m.attr("VERSION")             = OIIO_VERSION;
    m.attr("VERSION_STRING")      = PY_STR(OIIO_VERSION_STRING);
    m.attr("VERSION_MAJOR")       = OIIO_VERSION_MAJOR;
    m.attr("VERSION_MINOR")       = OIIO_VERSION_MINOR;
    m.attr("VERSION_PATCH")       = OIIO_VERSION_PATCH;
    m.attr("INTRO_STRING")        = PY_STR(OIIO_INTRO_STRING);
    m.attr("__version__")         = PY_STR(OIIO_VERSION_STRING);
}

}  // namespace PyOpenImageIO
