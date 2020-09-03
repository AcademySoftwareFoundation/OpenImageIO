// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#ifndef PYOPENIMAGEIO_PY_OIIO_H
#define PYOPENIMAGEIO_PY_OIIO_H

// Python.h uses the 'register' keyword, don't warn about it being
// deprecated in C++17.
#if (__cplusplus >= 201703L && defined(__GNUC__))
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

#include <OpenEXR/half.h>

#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/typedesc.h>

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;


#if PY_MAJOR_VERSION == 2
// Preferred Python string caster for Python2 is py::bytes, so it's a byte
// string (not unicode).
#    define PY_STR py::bytes
#else
// Python3 is always unicode, so return a true str
#    define PY_STR py::str
#endif


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



namespace PyOpenImageIO {

//using namespace boost::python;

using namespace OIIO;

// clang-format off

void declare_imagespec (py::module& m);
void declare_imageinput (py::module& m);
void declare_imageoutput (py::module& m);
void declare_typedesc (py::module& m);
void declare_roi (py::module& m);
void declare_deepdata (py::module& m);
void declare_colorconfig (py::module& m);
void declare_imagecache (py::module& m);
void declare_imagebuf (py::module& m);
void declare_imagebufalgo (py::module& m);
void declare_paramvalue (py::module& m);
void declare_global (py::module& m);

// bool PyProgressCallback(void*, float);
// object C_array_to_Python_array (const char *data, TypeDesc type, size_t size);
const char * python_array_code (TypeDesc format);
TypeDesc typedesc_from_python_array_code (string_view code);


inline std::string
object_classname(const py::object& obj)
{
    return obj.attr("__class__").attr("__name__").cast<py::str>();
}



template<typename T> struct PyTypeForCType { };
template<> struct PyTypeForCType<int> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned int> { typedef py::int_ type; };
template<> struct PyTypeForCType<short> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned short> { typedef py::int_ type; };
template<> struct PyTypeForCType<int64_t> { typedef py::int_ type; };
template<> struct PyTypeForCType<float> { typedef py::float_ type; };
template<> struct PyTypeForCType<half> { typedef py::float_ type; };
template<> struct PyTypeForCType<double> { typedef py::float_ type; };
template<> struct PyTypeForCType<const char*> { typedef PY_STR type; };
template<> struct PyTypeForCType<std::string> { typedef PY_STR type; };

// clang-format on


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
            vals.emplace_back(T(elem.template cast<float>()));
        } else if ((std::is_same<T, float>::value || std::is_same<T, int>::value)
                   && py::isinstance<py::int_>(elem)) {
            vals.emplace_back(T(elem.template cast<int>()));
        } else if (std::is_same<T, unsigned int>::value
                   && py::isinstance<py::int_>(elem)) {
            vals.emplace_back(T(elem.template cast<unsigned int>()));
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
            vals.emplace_back(elem.template cast<py::str>());
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
            vals.emplace_back(*(elem.template cast<TypeDesc*>()));
        } else if (py::isinstance<TypeDesc::BASETYPE>(elem)) {
            vals.emplace_back(*(elem.template cast<TypeDesc::BASETYPE*>()));
        } else if (py::isinstance<py::str>(elem)) {
            vals.emplace_back(
                TypeDesc(std::string(elem.template cast<py::str>())));
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
        vals.emplace_back(obj.template cast<pytype>());
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
        vals.emplace_back(obj.template cast<py::float_>());
        return true;
    } else if (py::isinstance<py::int_>(obj)) {
        int i = obj.template cast<py::int_>();
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
        vals.emplace_back(*(obj.template cast<TypeDesc*>()));
        return true;
    } else if (py::isinstance<TypeDesc::BASETYPE>(obj)) {
        vals.emplace_back(*(obj.template cast<TypeDesc::BASETYPE*>()));
        return true;
    } else if (py::isinstance<py::str>(obj)) {
        vals.emplace_back(TypeDesc(std::string(obj.template cast<py::str>())));
        return true;
    } else {
        return false;
    }
}



template<typename T>
inline bool
py_buffer_to_stdvector(std::vector<T>& vals, const py::buffer& obj)
{
    OIIO_DASSERT(py::isinstance<py::buffer>(obj));
    oiio_bufinfo binfo(obj.request());
    bool ok = true;
    vals.reserve(binfo.size);
    for (size_t i = 0; i < binfo.size; ++i) {
        if (std::is_same<T, float>::value
            && binfo.format.basetype == TypeDesc::FLOAT) {
            vals.emplace_back(binfo.dataval<float>(i));
        } else if ((std::is_same<T, float>::value || std::is_same<T, int>::value)
                   && binfo.format.basetype == TypeDesc::INT) {
            vals.emplace_back(T(binfo.dataval<int>(i)));
        } else if (std::is_same<T, unsigned int>::value
                   && binfo.format.basetype == TypeDesc::UINT) {
            vals.emplace_back(T(binfo.dataval<unsigned int>(i)));
        } else {
            // FIXME? Other cases?
            vals.emplace_back(T(42));
            ok = false;
        }
    }
    return ok;
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



// Suck up a tuple (or list, or single instance) of presumed T values into a
// vector<T>. Works for T any of int, float, string, and works if the Python
// container is any of tuple, list.
template<typename T>
inline bool
py_to_stdvector(std::vector<T>& vals, const py::object& obj)
{
    if (py::isinstance<py::tuple>(obj)) {  // if it's a Python tuple
        return py_indexable_pod_to_stdvector(vals, obj.cast<py::tuple>());
    }
    if (py::isinstance<py::list>(obj)) {  // if it's a Python list
        return py_indexable_pod_to_stdvector(vals, obj.cast<py::list>());
    }
    // Apparently a str can masquerade as a buffer object, so make sure to
    // exclude that from teh buffer case.
    if (py::isinstance<py::buffer>(obj) && !py::isinstance<py::str>(obj)) {
        return py_buffer_to_stdvector(vals, obj.cast<py::buffer>());
    }

    // handle scalar case or bust
    return py_scalar_pod_to_stdvector(vals, obj);
}



template<typename T>
inline py::tuple
C_to_tuple(cspan<T> vals)
{
    size_t size = vals.size();
    py::tuple result(size);
    for (size_t i = 0; i < size; ++i)
        result[i] = typename PyTypeForCType<T>::type(vals[i]);
    return result;
}


template<typename T>
inline py::tuple
C_to_tuple(const T* vals, size_t size)
{
    py::tuple result(size);
    for (size_t i = 0; i < size; ++i)
        result[i] = typename PyTypeForCType<T>::type(vals[i]);
    return result;
}


// Special case for TypeDesc
template<>
inline py::tuple
C_to_tuple<TypeDesc>(cspan<TypeDesc> vals)
{
    size_t size = vals.size();
    py::tuple result(size);
    for (size_t i = 0; i < size; ++i)
        result[i] = py::cast(vals[i]);
    return result;
}



// Convert an array of T values (described by type) into either a simple
// Python object (if it's an int, float, or string and a SCALAR) or a
// Python tuple.
template<typename T>
inline py::object
C_to_val_or_tuple(const T* vals, TypeDesc type, int nvalues = 1)
{
    size_t n = type.numelements() * type.aggregate * nvalues;
    if (n == 1 && !type.arraylen)
        return typename PyTypeForCType<T>::type(vals[0]);
    else
        return C_to_tuple(cspan<T>(vals, n));
}



template<typename T, typename POBJ>
bool
attribute_typed(T& myobj, string_view name, TypeDesc type, const POBJ& dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute(name, type, &u[0]);
        }
        return ok;
    }
    return false;
}



template<typename T>
py::object
getattribute_typed(const T& obj, const std::string& name,
                   TypeDesc type = TypeUnknown)
{
    if (type == TypeUnknown)
        return py::none();
    char* data = OIIO_ALLOCA(char, type.size());
    bool ok    = obj.getattribute(name, type, data);
    if (!ok)
        return py::none();  // None
    if (type.basetype == TypeDesc::INT)
        return C_to_val_or_tuple((const int*)data, type);
    if (type.basetype == TypeDesc::UINT)
        return C_to_val_or_tuple((const unsigned int*)data, type);
    if (type.basetype == TypeDesc::INT16)
        return C_to_val_or_tuple((const short*)data, type);
    if (type.basetype == TypeDesc::UINT16)
        return C_to_val_or_tuple((const unsigned short*)data, type);
    if (type.basetype == TypeDesc::FLOAT)
        return C_to_val_or_tuple((const float*)data, type);
    if (type.basetype == TypeDesc::DOUBLE)
        return C_to_val_or_tuple((const double*)data, type);
    if (type.basetype == TypeDesc::HALF)
        return C_to_val_or_tuple((const half*)data, type);
    if (type.basetype == TypeDesc::STRING)
        return C_to_val_or_tuple((const char**)data, type);
    return py::none();
}



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
        delete[](reinterpret_cast<T*>(f));
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
    if (format == TypeDesc::FLOAT)
        return make_numpy_array((float*)data, dims, chans, width, height,
                                depth);
    if (format == TypeDesc::UINT8)
        return make_numpy_array((unsigned char*)data, dims, chans, width,
                                height, depth);
    if (format == TypeDesc::UINT16)
        return make_numpy_array((unsigned short*)data, dims, chans, width,
                                height, depth);
    if (format == TypeDesc::INT8)
        return make_numpy_array((char*)data, dims, chans, width, height, depth);
    if (format == TypeDesc::INT16)
        return make_numpy_array((short*)data, dims, chans, width, height,
                                depth);
    if (format == TypeDesc::DOUBLE)
        return make_numpy_array((double*)data, dims, chans, width, height,
                                depth);
    if (format == TypeDesc::HALF)
        return make_numpy_array((half*)data, dims, chans, width, height, depth);
    if (format == TypeDesc::UINT)
        return make_numpy_array((unsigned int*)data, dims, chans, width, height,
                                depth);
    if (format == TypeDesc::INT)
        return make_numpy_array((int*)data, dims, chans, width, height, depth);
    delete[](char*) data;
    return py::none();
}



inline py::object
ParamValue_getitem(const ParamValue& self, bool allitems = false)
{
    TypeDesc t = self.type();
    int nvals  = allitems ? self.nvalues() : 1;

#define ParamValue_convert_dispatch(TYPE)                                  \
case TypeDesc::TYPE:                                                       \
    return C_to_val_or_tuple((CType<TypeDesc::TYPE>::type*)self.data(), t, \
                             nvals)

    switch (t.basetype) {
        // ParamValue_convert_dispatch(UCHAR);
        // ParamValue_convert_dispatch(CHAR);
        ParamValue_convert_dispatch(USHORT);
        ParamValue_convert_dispatch(SHORT);
        ParamValue_convert_dispatch(UINT);
        ParamValue_convert_dispatch(INT);
        // ParamValue_convert_dispatch(ULONGLONG);
        // ParamValue_convert_dispatch(LONGLONG);
#ifdef _HALF_H_
        ParamValue_convert_dispatch(HALF);
#endif
        ParamValue_convert_dispatch(FLOAT);
        ParamValue_convert_dispatch(DOUBLE);
    case TypeDesc::STRING:
        return C_to_val_or_tuple((const char**)self.data(), t, nvals);
    default: return py::none();
    }

#undef ParamValue_convert_dispatch
}



template<typename C>
inline void
delegate_setitem(C& self, const std::string& key, py::object obj)
{
    if (py::isinstance<py::float_>(obj))
        self[key] = float(obj.template cast<py::float_>());
    else if (py::isinstance<py::int_>(obj))
        self[key] = int(obj.template cast<py::int_>());
    else if (py::isinstance<py::str>(obj))
        self[key] = std::string(obj.template cast<py::str>());
    else
        throw std::invalid_argument("Bad type for __setitem__");
}


}  // namespace PyOpenImageIO

#endif  // PYOPENIMAGEIO_PY_OIIO_H
