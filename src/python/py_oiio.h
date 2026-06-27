// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#ifndef PYOPENIMAGEIO_PY_OIIO_H
#define PYOPENIMAGEIO_PY_OIIO_H

// Python.h uses the 'register' keyword, don't warn about it being
// deprecated in C++17.
#if (__cplusplus >= 201703L && defined(__GNUC__)) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wregister"
#endif

// clang-format off
// Must include Python.h first to avoid certain warnings
#ifdef _POSIX_C_SOURCE
#  error "You must include Python.h (and therefore py_oiio.h) BEFORE anything that defines _POSIX_C_SOURCE"
#endif 
#include <Python.h>
// clang-format on

#include <memory>

// Avoid a compiler warning from a duplication in tiffconf.h/pyconfig.h
#undef SIZEOF_LONG

// Avoid a problem with copysign defined in pyconfig.h on Windows.
#ifdef copysign
#    undef copysign
#endif

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/typedesc.h>

#if defined(OIIO_PY_BACKEND_NANOBIND)
#    include <cstring>
#endif

#include "py_backend.h"

#if defined(OIIO_PY_BACKEND_NANOBIND)
#    define PY_STR(x) oiio_py::str(x)

#else
#    include <pybind11/numpy.h>
#    include <pybind11/stl.h>

// Python3 is always unicode, so return a true str
#    define PY_STR py::str

#endif



#ifndef OIIO_PY_BACKEND_NANOBIND
namespace pybind11 {
namespace detail {

    // This half casting support for numpy was all derived from discussions
    // here: https://github.com/pybind/pybind11/issues/1776

    // Similar to enums in `pybind11/numpy.h`. Determined by doing:
    // python3 -c 'import numpy as np; print(np.dtype(np.float16).num)'
    constexpr int NPY_FLOAT16 = 23;

    template<> struct npy_format_descriptor<half> {
        static pybind11::dtype dtype()
        {
            handle ptr = npy_api::get().PyArray_DescrFromType_(NPY_FLOAT16);
            return reinterpret_borrow<pybind11::dtype>(ptr);
        }
        static std::string format()
        {
            // following: https://docs.python.org/3/library/struct.html#format-characters
            return "e";
        }
        static constexpr auto name = _("float16");
    };

}  // namespace detail
}  // namespace pybind11
#endif



namespace PyOpenImageIO {

using namespace OIIO;

// clang-format off

void declare_imagespec (py_module& m);
void declare_imageinput (py_module& m);
void declare_imageoutput (py_module& m);
void declare_typedesc (py_module& m);
void declare_roi (py_module& m);
void declare_deepdata (py_module& m);
void declare_colorconfig (py_module& m);
void declare_imagecache (py_module& m);
void declare_imagebuf (py_module& m);
void declare_imagebufalgo (py_module& m);
void declare_paramvalue (py_module& m);
void declare_global (py_module& m);
void declare_wrap (py_module& m);
void declare_mipmpode (py_module& m);
void declare_interpmode (py_module& m);
void declare_textureopt (py_module& m);
void declare_texturesystem (py_module& m);

// bool PyProgressCallback(void*, float);
// object C_array_to_Python_array (const char *data, TypeDesc type, size_t size);
TypeDesc typedesc_from_python_array_code (string_view code);
// const char * python_array_code (TypeDesc format);  // unused


#ifndef OIIO_PY_BACKEND_NANOBIND
inline std::string
object_classname(const py::object& obj)
{
    return obj.attr("__class__").attr("__name__").cast<py::str>();
}
#endif


template<typename T> struct PyTypeForCType { };
template<> struct PyTypeForCType<int> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned int> { typedef py::int_ type; };
template<> struct PyTypeForCType<short> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned short> { typedef py::int_ type; };
template<> struct PyTypeForCType<char> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned char> { typedef py::int_ type; };
template<> struct PyTypeForCType<int64_t> { typedef py::int_ type; };
template<> struct PyTypeForCType<uint64_t> { typedef py::int_ type; };
template<> struct PyTypeForCType<float> { typedef py::float_ type; };
template<> struct PyTypeForCType<half> { typedef py::float_ type; };
template<> struct PyTypeForCType<double> { typedef py::float_ type; };
template<> struct PyTypeForCType<const char*> { typedef py::str type; };
template<> struct PyTypeForCType<std::string> { typedef py::str type; };

// clang-format on

#ifndef OIIO_PY_BACKEND_NANOBIND
// Struct that holds OIIO style buffer info, constructed from
// py::buffer_info
struct oiio_bufinfo {
    TypeDesc format  = TypeUnknown;
    void* data       = nullptr;
    stride_t xstride = AutoStride, ystride = AutoStride, zstride = AutoStride;
    size_t size = 0;
    std::string error;

    // Just raw buffer, no idea what to expect, treat like a flat array.
    // Only works for "contiguous" buffers.
    oiio_bufinfo(const py::buffer_info& pybuf);

    // Expect a certain layout, figure out how to make sense of the buffer.
    oiio_bufinfo(const py::buffer_info& pybuf, int nchans, int width,
                 int height, int depth, int pixeldims);

    // Retrieve presumed contiguous data value index i.
    template<typename T> T dataval(size_t i) { return ((const T*)data)[i]; }
};
#endif



// Suck up a tuple of presumed T values into a vector<T>. Works for T any of
// int, float, string, and works if the Python container is any of tuple,
// list.
template<typename T, typename PYT>
inline bool
py_indexable_pod_to_stdvector(std::vector<T>& vals, const PYT& obj)
{
    OIIO_ASSERT(py::isinstance<py::tuple>(obj)
                || py::isinstance<py::list>(obj));
    bool ok             = true;
    const size_t length = py::len(obj);
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (std::is_same<T, float>::value && py::isinstance<py::float_>(elem)) {
            vals.emplace_back(T(py::cast<float>(elem)));
        } else if ((std::is_same<T, float>::value || std::is_same<T, int>::value)
                   && py::isinstance<py::int_>(elem)) {
            vals.emplace_back(T(py::cast<int>(elem)));
        } else if (std::is_same<T, unsigned int>::value
                   && py::isinstance<py::int_>(elem)) {
            vals.emplace_back(T(py::cast<unsigned int>(elem)));
        } else {
            // FIXME? Other cases?
            vals.emplace_back(T(42));
            ok = false;
        }
    }
    return ok;
}


// Partial specialization for reading strings
template<typename PYT>
inline bool
py_indexable_pod_to_stdvector(std::vector<std::string>& vals, const PYT& obj)
{
    OIIO_ASSERT(py::isinstance<py::tuple>(obj)
                || py::isinstance<py::list>(obj));
    bool ok             = true;
    const size_t length = py::len(obj);
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (py::isinstance<py::str>(elem)) {
            vals.emplace_back(oiio_py::str_to_stdstring(elem));
        } else {
            // FIXME? Other cases?
            vals.emplace_back("");
            ok = false;
        }
    }
    return ok;
}


// Partial specialization for reading TypeDesc
template<typename PYT>
inline bool
py_indexable_pod_to_stdvector(std::vector<TypeDesc>& vals, const PYT& obj)
{
    OIIO_ASSERT(py::isinstance<py::tuple>(obj)
                || py::isinstance<py::list>(obj));
    bool ok             = true;
    const size_t length = py::len(obj);
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (py::isinstance<TypeDesc>(elem)) {
            vals.emplace_back(py::cast<TypeDesc>(elem));
        } else if (py::isinstance<TypeDesc::BASETYPE>(elem)) {
            vals.emplace_back(TypeDesc(py::cast<TypeDesc::BASETYPE>(elem)));
        } else if (py::isinstance<py::str>(elem)) {
            vals.emplace_back(
                TypeDesc(oiio_py::str_to_stdstring(elem)));
        } else {
            // FIXME? Other cases?
            vals.emplace_back(TypeUnknown);
            ok = false;
        }
    }
    return ok;
}



template<typename T>
inline bool
py_scalar_pod_to_stdvector(std::vector<T>& vals, const py::object& obj)
{
    using pytype = typename PyTypeForCType<T>::type;
    vals.clear();
    if (py::isinstance<pytype>(obj)) {
        vals.emplace_back(py::cast<T>(obj));
        return true;
    } else {
        return false;
    }
}

// float specialization -- accept ints, too
template<>
inline bool
py_scalar_pod_to_stdvector(std::vector<float>& vals, const py::object& obj)
{
    vals.clear();
    if (py::isinstance<py::float_>(obj)) {
        vals.emplace_back(py::cast<float>(obj));
        return true;
    } else if (py::isinstance<py::int_>(obj)) {
        int i = py::cast<int>(obj);
        vals.emplace_back((float)i);
        return true;
    } else {
        return false;
    }
}

// TypeDesc specialization
template<>
inline bool
py_scalar_pod_to_stdvector(std::vector<TypeDesc>& vals, const py::object& obj)
{
    vals.clear();
    if (py::isinstance<TypeDesc>(obj)) {
        vals.emplace_back(py::cast<TypeDesc>(obj));
        return true;
    } else if (py::isinstance<TypeDesc::BASETYPE>(obj)) {
        vals.emplace_back(TypeDesc(py::cast<TypeDesc::BASETYPE>(obj)));
        return true;
    } else if (py::isinstance<py::str>(obj)) {
        vals.emplace_back(TypeDesc(oiio_py::str_to_stdstring(obj)));
        return true;
    } else {
        return false;
    }
}



// Shared: read `count` contiguous elements of `format` into vector<T>.
// Matches the element-dispatch logic from the original pybind11 implementation.
template<typename T>
inline bool
buffer_format_to_stdvector(std::vector<T>& vals, TypeDesc format,
                           const void* data, size_t count)
{
    bool ok = true;
    vals.reserve(count);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < count; ++i) {
        if (std::is_same<T, float>::value
            && format.basetype == TypeDesc::FLOAT) {
            vals.push_back(reinterpret_cast<const float*>(bytes)[i]);
        } else if ((std::is_same<T, float>::value || std::is_same<T, int>::value)
                   && format.basetype == TypeDesc::INT) {
            vals.push_back(T(reinterpret_cast<const int*>(bytes)[i]));
        } else if (std::is_same<T, unsigned int>::value
                   && format.basetype == TypeDesc::UINT) {
            vals.push_back(T(reinterpret_cast<const unsigned int*>(bytes)[i]));
        } else if (std::is_same<T, unsigned char>::value
                   && format.basetype == TypeDesc::UINT8) {
            vals.push_back(T(reinterpret_cast<const unsigned char*>(bytes)[i]));
        } else if (std::is_same<T, unsigned short>::value
                   && format.basetype == TypeDesc::UINT16) {
            vals.push_back(
                T(reinterpret_cast<const unsigned short*>(bytes)[i]));
        } else {
            // FIXME? Other cases?
            vals.push_back(T(42));
            ok = false;
        }
    }
    return ok;
}



#if defined(OIIO_PY_BACKEND_NANOBIND)
template<typename T>
inline bool
py_buffer_to_stdvector(std::vector<T>& vals, const py::object& obj)
{
    Py_buffer view;
    if (PyObject_GetBuffer(obj.ptr(), &view, PyBUF_FORMAT | PyBUF_C_CONTIGUOUS)
        != 0) {
        PyErr_Clear();
        return false;
    }

    bool ok = view.itemsize > 0 && view.len % view.itemsize == 0;
    if (!ok) {
        PyBuffer_Release(&view);
        return false;
    }

    TypeDesc format = TypeUnknown;
    if (view.format && view.format[0]) {
        format = typedesc_from_python_array_code(view.format);
    }

    const size_t count = static_cast<size_t>(view.len) / view.itemsize;
    ok                 = buffer_format_to_stdvector(vals, format, view.buf, count);
    PyBuffer_Release(&view);
    return ok;
}



template<>
inline bool
py_buffer_to_stdvector(std::vector<std::string>&, const py::object&)
{
    return false;
}



template<>
inline bool
py_buffer_to_stdvector(std::vector<TypeDesc>&, const py::object&)
{
    return false;
}

#else

template<typename T>
inline bool
py_buffer_to_stdvector(std::vector<T>& vals, const py::buffer& obj)
{
    OIIO_DASSERT(py::isinstance<py::buffer>(obj));
    oiio_bufinfo binfo(obj.request());
    return buffer_format_to_stdvector(vals, binfo.format, binfo.data,
                                      binfo.size);
}


// Specialization for reading strings
template<>
inline bool
py_buffer_to_stdvector(std::vector<std::string>& /*vals*/,
                       const py::buffer& /*obj*/)
{
    return false;  // not supported
}


// Specialization for reading TypeDesc
template<>
inline bool
py_buffer_to_stdvector(std::vector<TypeDesc>& /*vals*/,
                       const py::buffer& /*obj*/)
{
    return false;  // not supported
}

#endif



// Suck up a tuple (or list, or single instance) of presumed T values into a
// vector<T>. Works for T any of int, float, string, and works if the Python
// container is any of tuple, list.
template<typename T>
inline bool
py_to_stdvector(std::vector<T>& vals, const py::object& obj)
{
    if (py::isinstance<py::tuple>(obj)) {  // if it's a Python tuple
        return py_indexable_pod_to_stdvector(vals, py::cast<py::tuple>(obj));
    }
    if (py::isinstance<py::list>(obj)) {  // if it's a Python list
        return py_indexable_pod_to_stdvector(vals, py::cast<py::list>(obj));
    }
    // Apparently a str can masquerade as a buffer object, so make sure to
    // exclude that from the buffer case.
#if defined(OIIO_PY_BACKEND_NANOBIND)
    if (PyObject_CheckBuffer(obj.ptr()) && !PyUnicode_Check(obj.ptr())) {
        return py_buffer_to_stdvector(vals, obj);
    }
#else
    if (py::isinstance<py::buffer>(obj) && !py::isinstance<py::str>(obj)) {
        return py_buffer_to_stdvector(vals, py::cast<py::buffer>(obj));
    }
#endif

    // handle scalar case or bust
    return py_scalar_pod_to_stdvector(vals, obj);
}



template<typename T>
inline py::tuple
C_to_tuple(cspan<T> vals)
{
    return oiio_py::make_tuple(vals.size(), [&](size_t i) {
        if constexpr (std::is_same_v<T, half>) {
            return py::cast(static_cast<float>(vals[i]));
        } else {
            return py::cast(vals[i]);
        }
    });
}



template<typename T>
inline py::tuple
C_to_tuple(span<T> vals)
{
    return C_to_tuple(cspan<T>(vals));
}


template<typename T>
inline py::tuple
C_to_tuple(const T* vals, size_t size)
{
    return oiio_py::make_tuple(size, [&](size_t i) {
        if constexpr (std::is_same_v<T, half>) {
            return py::cast(static_cast<float>(vals[i]));
        } else {
            return py::cast(vals[i]);
        }
    });
}


template<>
inline py::tuple
C_to_tuple(cspan<unsigned char> vals)
{
    return oiio_py::make_tuple(vals.size(), [&](size_t i) {
        return static_cast<unsigned char>(vals[i]);
    });
}


// Special case for TypeDesc
template<>
inline py::tuple
C_to_tuple<TypeDesc>(cspan<TypeDesc> vals)
{
    return oiio_py::make_tuple(vals.size(), [&](size_t i) {
        return py::cast(vals[i]);
    });
}



// Convert an array of T values (described by type) into either a simple
// Python object (if it's an int, float, or string and a SCALAR) or a
// Python tuple.
template<typename T>
inline py::object
C_to_val_or_tuple(const T* vals, TypeDesc type, int nvalues = 1)
{
    OIIO_DASSERT(vals && nvalues);
    size_t n = type.numelements() * type.aggregate * nvalues;
    if (n == 1 && !type.arraylen) {
        if constexpr (std::is_same_v<T, half>) {
            return py::cast(static_cast<float>(vals[0]));
        } else {
            return py::cast(vals[0]);
        }
    } else {
        return C_to_tuple(cspan<T>(vals, n));
    }
}



template<typename T, typename POBJ>
bool
attribute_typed(T& myobj, string_view name, TypeDesc type, const POBJ& dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            myobj.attribute(name, type, &vals[0]);
        }
        return ok;
    }
    if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            myobj.attribute(name, type, &vals[0]);
        }
        return ok;
    }
    if (type.basetype == TypeDesc::UINT8) {
        std::vector<unsigned char> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            myobj.attribute(name, type, &vals[0]);
        }
        return ok;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            myobj.attribute(name, type, &vals[0]);
        }
        return ok;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            std::vector<ustring> u;
            for (auto& val : vals) {
                u.emplace_back(val);
            }
            myobj.attribute(name, type, &u[0]);
        }
        return ok;
    }
    return false;
}



// Dispatch a single-value attribute() call based on the type of the
// Python object (int, float, or string).
template<typename T>
inline void
attribute_onearg(T& myobj, string_view name, const py::object& obj)
{
    if (py::isinstance<py::float_>(obj)) {
        myobj.attribute(name, py::cast<float>(obj));
    } else if (py::isinstance<py::int_>(obj)) {
        myobj.attribute(name, py::cast<int>(obj));
    } else if (py::isinstance<py::str>(obj)) {
        myobj.attribute(name, oiio_py::str_to_stdstring(obj));
    } else if (py::isinstance<py::bytes>(obj)) {
        myobj.attribute(name,
                        oiio_py::bytes_to_stdstring(py::cast<py::bytes>(obj)));
    } else {
        throw py::type_error("attribute() value must be int, float, or str");
    }
}



// `data` points to values of `type`. Make a python object that represents
// them.
py::object
make_pyobject(const void* data, TypeDesc type, int nvalues = 1,
              py::object defaultvalue = py::none());



template<typename T>
py::object
getattribute_typed(const T& obj, const std::string& name,
                   TypeDesc type = TypeUnknown)
{
    if (type == TypeUnknown) {
        return py::none();
    }
    char* data = OIIO_ALLOCA(char, type.size());
    bool ok    = obj.getattribute(name, type, data);
    if (!ok) {
        return py::none();  // None
    }
    return make_pyobject(data, type);
}



#ifndef OIIO_PY_BACKEND_NANOBIND
// TRANSFERS ownership of the data pointer!
// N.B. There is some evidence that this doesn't work properly with
// non-float arrays. Maybe a limitation of pybind11?
template<class T>
inline py::array_t<T>
make_numpy_array(T* data, int dims, size_t chans, size_t width, size_t height,
                 size_t depth = 1)
{
    const size_t size = chans * width * height * depth;
    T* mem            = data ? data : new T[size];

    // Create a Python object that will free the allocated memory when
    // destroyed:
    py::capsule free_when_done(mem, [](void* f) {
        delete[] (reinterpret_cast<T*>(f));
    });

    std::vector<size_t> shape, strides;
    if (dims == 4) {  // volumetric
        shape.assign({ depth, height, width, chans });
        strides.assign({ height * width * chans * sizeof(T),
                         width * chans * sizeof(T), chans * sizeof(T),
                         sizeof(T) });
    } else if (dims == 3 && depth == 1) {  // 2D+channels
        shape.assign({ height, width, chans });
        strides.assign(
            { width * chans * sizeof(T), chans * sizeof(T), sizeof(T) });
    } else if (dims == 2 && depth == 1
               && height == 1) {  // 1D (scanline) + channels
        shape.assign({ width, chans });
        strides.assign({ chans * sizeof(T), sizeof(T) });
    } else {  // punt -- make it a 1D array
        shape.assign({ size });
        strides.assign({ sizeof(T) });
    }
    return py::array_t<T>(shape, strides, mem, free_when_done);
}



inline py::object
make_numpy_array(TypeDesc format, void* data, int dims, size_t chans,
                 size_t width, size_t height, size_t depth = 1)
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
#endif



template<typename C>
inline void
delegate_setitem(C& self, const std::string& key, py::object obj)
{
    if (py::isinstance<py::float_>(obj)) {
        self[key] = py::cast<float>(obj);
    } else if (py::isinstance<py::int_>(obj)) {
        self[key] = py::cast<int>(obj);
    } else if (py::isinstance<py::str>(obj)) {
        self[key] = oiio_py::str_to_stdstring(obj);
    } else if (py::isinstance<py::bytes>(obj)) {
        self[key] = oiio_py::bytes_to_stdstring(py::cast<py::bytes>(obj));
    } else {
        throw std::invalid_argument("Bad type for __setitem__");
    }
}

}  // namespace PyOpenImageIO

#endif  // PYOPENIMAGEIO_PY_OIIO_H
