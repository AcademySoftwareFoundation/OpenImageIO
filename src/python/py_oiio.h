/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#ifndef PYOPENIMAGEIO_PY_OIIO_H
#define PYOPENIMAGEIO_PY_OIIO_H

#include <memory>

// Avoid a compiler warning from a duplication in tiffconf.h/pyconfig.h
#undef SIZEOF_LONG

#include <OpenEXR/half.h>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/deepdata.h>
#include <OpenImageIO/array_view.h>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
namespace py = pybind11;


#if PY_MAJOR_VERSION == 2
// Preferred Python string caster for Python2 is py::bytes, so it's a byte
// string (not unicode).
#define PY_STR py::bytes
#else
// Python3 is always unicode, so return a true str
#define PY_STR py::str
#endif


namespace PyOpenImageIO
{

//using namespace boost::python;

using namespace OIIO;

void declare_imagespec (py::module& m);
void declare_imageinput (py::module& m);
void declare_imageoutput (py::module& m);
void declare_typedesc (py::module& m);
void declare_roi (py::module& m);
void declare_deepdata (py::module& m);
void declare_imagecache (py::module& m);
void declare_imagebuf (py::module& m);
void declare_imagebufalgo (py::module& m);
void declare_paramvalue (py::module& m);
void declare_global (py::module& m);

// bool PyProgressCallback(void*, float);
// object C_array_to_Python_array (const char *data, TypeDesc type, size_t size);
const char * python_array_code (TypeDesc format);
TypeDesc typedesc_from_python_array_code (char code);
std::string object_classname (const py::object& obj);


template<typename T> struct PyTypeForCType { };
template<> struct PyTypeForCType<int> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned int> { typedef py::int_ type; };
template<> struct PyTypeForCType<short> { typedef py::int_ type; };
template<> struct PyTypeForCType<unsigned short> { typedef py::int_ type; };
template<> struct PyTypeForCType<float> { typedef py::float_ type; };
template<> struct PyTypeForCType<half> { typedef py::float_ type; };
template<> struct PyTypeForCType<double> { typedef py::float_ type; };
template<> struct PyTypeForCType<const char*> { typedef PY_STR type; };
template<> struct PyTypeForCType<std::string> { typedef PY_STR type; };



// Suck up a tuple of presumed T values into a vector<T>. Works for T any of
// int, float, string, and works if the Python container is any of tuple,
// list.
template<typename T, typename PYT>
inline bool py_indexable_pod_to_stdvector (std::vector<T> &vals, const PYT &obj)
{
    ASSERT (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj));
    bool ok = true;
    const size_t length = py::len(obj);
    vals.reserve (length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (std::is_same<T,float>::value && py::isinstance<py::float_>(elem)) {
            vals.emplace_back (elem.template cast<float>());
        } else if ((std::is_same<T,float>::value || std::is_same<T,int>::value)
                   && py::isinstance<py::int_>(elem)) {
            vals.emplace_back (elem.template cast<int>());
        } else {
            // FIXME? Other cases?
            vals.emplace_back (T(0));
            ok = false;
        }
    }
    return ok;
}


// Partial specialization for reading strings
template<typename PYT>
inline bool py_indexable_pod_to_stdvector (std::vector<std::string> &vals, const PYT &obj)
{
    ASSERT (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj));
    bool ok = true;
    const size_t length = py::len(obj);
    vals.reserve (length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (py::isinstance<py::str>(elem)) {
            vals.emplace_back (elem.template cast<py::str>());
        } else {
            // FIXME? Other cases?
            vals.emplace_back ("");
            ok = false;
        }
    }
    return ok;
}


// Partial specialization for reading TypeDesc
template<typename PYT>
inline bool py_indexable_pod_to_stdvector (std::vector<TypeDesc> &vals, const PYT &obj)
{
    ASSERT (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj));
    bool ok = true;
    const size_t length = py::len(obj);
    vals.reserve (length);
    for (size_t i = 0; i < length; ++i) {
        auto elem = obj[i];
        if (py::isinstance<TypeDesc>(elem)) {
            vals.emplace_back (*(elem.template cast<TypeDesc *>()));
        } else if (py::isinstance<TypeDesc::BASETYPE>(elem)) {
            vals.emplace_back (*(elem.template cast<TypeDesc::BASETYPE *>()));
        } else if (py::isinstance<py::str>(elem)) {
            vals.emplace_back (TypeDesc(std::string(elem.template cast<py::str>())));
        } else {
            // FIXME? Other cases?
            vals.emplace_back (TypeUnknown);
            ok = false;
        }
    }
    return ok;
}


// Suck up a tuple of presumed T values into a vector<T>. Works for T any of
// int, float, string, and works if the Python container is any of tuple,
// list.
template<typename T>
inline bool py_to_stdvector (std::vector<T> &vals, const py::object &obj)
{
    bool ok = true;
    if (py::isinstance<py::tuple>(obj)) {
        return py_indexable_pod_to_stdvector (vals, obj.cast<py::tuple>());
    }
    if (py::isinstance<py::list>(obj)) {
        return py_indexable_pod_to_stdvector (vals, obj.cast<py::list>());
    }
    ok = false;
    return ok;
}



template<typename T>
inline py::tuple C_to_tuple (array_view<const T> vals)
{
    size_t size = vals.size();
    py::tuple result (size);
    for (size_t i = 0;  i < size;  ++i)
        result[i] = typename PyTypeForCType<T>::type (vals[i]);
    return result;
}


template<typename T>
inline py::tuple C_to_tuple (const T* vals, size_t size)
{
    py::tuple result (size);
    for (size_t i = 0;  i < size;  ++i)
        result[i] = typename PyTypeForCType<T>::type (vals[i]);
    return result;
}


// Special case for TypeDesc
template<>
inline py::tuple C_to_tuple<TypeDesc> (array_view<const TypeDesc> vals)
{
    size_t size = vals.size();
    py::tuple result (size);
    for (size_t i = 0;  i < size;  ++i)
        result[i] = py::cast<TypeDesc>(vals[i]);
    return result;
}



// Convert an array of T values (described by type) into either a simple
// Python object (if it's an int, float, or string and a SCALAR) or a
// Python tuple.
template<typename T>
inline py::object C_to_val_or_tuple (const T *vals, TypeDesc type)
{
    size_t n = type.numelements() * type.aggregate;
    if (n == 1 && !type.arraylen)
        return typename PyTypeForCType<T>::type (vals[0]);
    else
        return C_to_tuple (array_view<const T>(vals, n));
}




template<typename T, typename POBJ>
void
attribute_typed (T &myobj, string_view name,
                 TypeDesc type, const POBJ &dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute (name, type, &u[0]);
        }
        return;
    }
}



template<typename T>
py::object
getattribute_typed (const T& obj,
                    const std::string &name, TypeDesc type=TypeUnknown)
{
    if (type == TypeUnknown)
        return py::none();
    char *data = OIIO_ALLOCA (char, type.size());
    bool ok = obj.getattribute (name, type, data);
    if (! ok)
        return py::none();   // None
    if (type.basetype == TypeDesc::INT)
        return C_to_val_or_tuple ((const int *)data, type);
    if (type.basetype == TypeDesc::UINT)
        return C_to_val_or_tuple ((const unsigned int *)data, type);
    if (type.basetype == TypeDesc::INT16)
        return C_to_val_or_tuple ((const short *)data, type);
    if (type.basetype == TypeDesc::UINT16)
        return C_to_val_or_tuple ((const unsigned short *)data, type);
    if (type.basetype == TypeDesc::FLOAT)
        return C_to_val_or_tuple ((const float *)data, type);
    if (type.basetype == TypeDesc::DOUBLE)
        return C_to_val_or_tuple ((const double *)data, type);
    if (type.basetype == TypeDesc::HALF)
        return C_to_val_or_tuple ((const half *)data, type);
    if (type.basetype == TypeDesc::STRING)
        return C_to_val_or_tuple ((const char **)data, type);
    return py::none();
}



// Struct that holds OIIO style buffer info, constructed from
// py::buffer_info
struct oiio_bufinfo {
    TypeDesc format = TypeUnknown;
    void* data = nullptr;
    stride_t xstride = AutoStride, ystride = AutoStride, zstride = AutoStride;
    size_t size = 0;

    oiio_bufinfo (const py::buffer_info &pybuf, int nchans, int width,
                  int height, int depth, int pixeldims);
};



// TRANSFERS ownership of the data pointer!
template<class T>
inline py::array_t<T>
make_numpy_array (T *data, int dims, size_t chans, size_t width,
                  size_t height, size_t depth=1)
{
    const size_t size = chans*width*height*depth;
    T *mem = data ? data : new T[size];

    // Create a Python object that will free the allocated memory when
    // destroyed:
    py::capsule free_when_done(mem, [](void *f) {
        delete[] (reinterpret_cast<T *>(f));
    });

    std::vector<size_t> shape, strides;
    if (dims == 4) { // volumetric
        shape.assign ({ depth, height, width, chans });
        strides.assign ({ height*width*chans*sizeof(T), width*chans*sizeof(T),
                          chans*sizeof(T), sizeof(T) });
    }
    else if (dims == 3 && depth == 1) { // 2D+channels
        shape.assign ({ height, width, chans });
        strides.assign ({ width*chans*sizeof(T), chans*sizeof(T), sizeof(T) });
    }
    else if (depth == 2 && depth == 1 && height == 1) { // 1D (scanline) + channels
        shape.assign ({ width, chans });
        strides.assign ({ chans*sizeof(T), sizeof(T) });
    }
    else {   // punt -- make it a 1D array
        shape.assign ({ size });
        strides.assign ({ sizeof(T) });
    }
    return py::array_t<T>(shape, strides, mem, free_when_done);
}



inline py::object
make_numpy_array (TypeDesc format, void *data, int dims,
                  size_t chans, size_t width, size_t height, size_t depth=1)
{
    if (format == TypeDesc::FLOAT)
        return make_numpy_array ((float *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::UINT8)
        return make_numpy_array ((unsigned char *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::UINT16)
        return make_numpy_array ((unsigned short *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::INT8)
        return make_numpy_array ((char *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::INT16)
        return make_numpy_array ((short *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::DOUBLE)
        return make_numpy_array ((double *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::HALF)
        return make_numpy_array ((unsigned short *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::UINT)
        return make_numpy_array ((unsigned int *)data, dims, chans, width, height, depth);
    if (format == TypeDesc::INT)
        return make_numpy_array ((int *)data, dims, chans, width, height, depth);
    delete [] (char *)data;
    return py::none ();
}


} // namespace PyOpenImageIO

#endif // PYOPENIMAGEIO_PY_OIIO_H
