// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

namespace PyOpenImageIO {

using namespace OIIO;

TypeDesc
typedesc_from_python_array_code(string_view code);

void
declare_roi(nb::module_& m);
void
declare_imagespec(nb::module_& m);
void
declare_typedesc(nb::module_& m);
void
declare_paramvalue(nb::module_& m);

template<typename T> struct PyTypeForCType {};
template<> struct PyTypeForCType<int> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<unsigned int> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<short> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<unsigned short> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<char> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<unsigned char> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<int64_t> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<uint64_t> {
    using type = nb::int_;
};
template<> struct PyTypeForCType<float> {
    using type = nb::float_;
};
template<> struct PyTypeForCType<half> {
    using type = nb::float_;
};
template<> struct PyTypeForCType<double> {
    using type = nb::float_;
};
template<> struct PyTypeForCType<const char*> {
    using type = nb::str;
};
template<> struct PyTypeForCType<std::string> {
    using type = nb::str;
};

template<typename T, typename Obj>
inline bool
py_indexable_pod_to_stdvector(std::vector<T>& vals, const Obj& obj)
{
    bool ok             = true;
    const size_t length = obj.size();
    vals.clear();
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        nb::handle elem = obj[i];
        if constexpr (std::is_same_v<T, float>) {
            if (nb::isinstance<nb::float_>(elem)) {
                vals.emplace_back(nb::cast<float>(elem));
            } else if (nb::isinstance<nb::int_>(elem)) {
                vals.emplace_back(static_cast<float>(nb::cast<int>(elem)));
            } else {
                ok = false;
            }
        } else if constexpr (std::is_same_v<T, int>) {
            if (nb::isinstance<nb::int_>(elem)) {
                vals.emplace_back(nb::cast<int>(elem));
            } else {
                ok = false;
            }
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            if (nb::isinstance<nb::int_>(elem)) {
                vals.emplace_back(nb::cast<unsigned int>(elem));
            } else {
                ok = false;
            }
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            if (nb::isinstance<nb::int_>(elem)) {
                vals.emplace_back(nb::cast<unsigned char>(elem));
            } else {
                ok = false;
            }
        } else {
            ok = false;
        }
        if (!ok)
            break;
    }
    return ok;
}

template<typename Obj>
inline bool
py_indexable_pod_to_stdvector(std::vector<std::string>& vals, const Obj& obj)
{
    bool ok             = true;
    const size_t length = obj.size();
    vals.clear();
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        nb::handle elem = obj[i];
        if (nb::isinstance<nb::str>(elem)) {
            vals.emplace_back(std::string(nb::cast<nb::str>(elem).c_str()));
        } else {
            ok = false;
            break;
        }
    }
    return ok;
}

template<typename Obj>
inline bool
py_indexable_pod_to_stdvector(std::vector<TypeDesc>& vals, const Obj& obj)
{
    bool ok             = true;
    const size_t length = obj.size();
    vals.clear();
    vals.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        nb::handle elem = obj[i];
        if (nb::isinstance<TypeDesc>(elem)) {
            vals.emplace_back(nb::cast<TypeDesc>(elem));
        } else if (nb::isinstance<TypeDesc::BASETYPE>(elem)) {
            vals.emplace_back(TypeDesc(nb::cast<TypeDesc::BASETYPE>(elem)));
        } else if (nb::isinstance<nb::str>(elem)) {
            vals.emplace_back(TypeDesc(nb::cast<nb::str>(elem).c_str()));
        } else {
            ok = false;
            break;
        }
    }
    return ok;
}

template<typename T>
inline bool
py_scalar_pod_to_stdvector(std::vector<T>& vals, const nb::handle& obj)
{
    using pytype = typename PyTypeForCType<T>::type;
    vals.clear();
    if (nb::isinstance<pytype>(obj)) {
        vals.emplace_back(nb::cast<T>(obj));
        return true;
    }
    return false;
}

template<>
inline bool
py_scalar_pod_to_stdvector(std::vector<float>& vals, const nb::handle& obj)
{
    vals.clear();
    if (nb::isinstance<nb::float_>(obj)) {
        vals.emplace_back(nb::cast<float>(obj));
        return true;
    }
    if (nb::isinstance<nb::int_>(obj)) {
        vals.emplace_back(static_cast<float>(nb::cast<int>(obj)));
        return true;
    }
    return false;
}

template<>
inline bool
py_scalar_pod_to_stdvector(std::vector<std::string>& vals,
                           const nb::handle& obj)
{
    vals.clear();
    if (nb::isinstance<nb::str>(obj)) {
        vals.emplace_back(std::string(nb::cast<nb::str>(obj).c_str()));
        return true;
    }
    return false;
}

template<>
inline bool
py_scalar_pod_to_stdvector(std::vector<TypeDesc>& vals, const nb::handle& obj)
{
    vals.clear();
    if (nb::isinstance<TypeDesc>(obj)) {
        vals.emplace_back(nb::cast<TypeDesc>(obj));
        return true;
    }
    if (nb::isinstance<TypeDesc::BASETYPE>(obj)) {
        vals.emplace_back(TypeDesc(nb::cast<TypeDesc::BASETYPE>(obj)));
        return true;
    }
    if (nb::isinstance<nb::str>(obj)) {
        vals.emplace_back(TypeDesc(nb::cast<nb::str>(obj).c_str()));
        return true;
    }
    return false;
}

template<typename T>
inline bool
py_buffer_to_stdvector(std::vector<T>& vals, const nb::handle& obj)
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
    if (view.format && view.format[0])
        format = typedesc_from_python_array_code(view.format);

    const size_t count = static_cast<size_t>(view.len) / view.itemsize;
    vals.clear();
    vals.reserve(count);
    const unsigned char* data = static_cast<const unsigned char*>(view.buf);

    for (size_t i = 0; ok && i < count; ++i) {
        if constexpr (std::is_same_v<T, float>) {
            if (format.basetype == TypeDesc::FLOAT)
                vals.emplace_back(reinterpret_cast<const float*>(data)[i]);
            else if (format.basetype == TypeDesc::INT)
                vals.emplace_back(
                    static_cast<float>(reinterpret_cast<const int*>(data)[i]));
            else
                ok = false;
        } else if constexpr (std::is_same_v<T, int>) {
            if (format.basetype == TypeDesc::INT)
                vals.emplace_back(reinterpret_cast<const int*>(data)[i]);
            else
                ok = false;
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            if (format.basetype == TypeDesc::UINT)
                vals.emplace_back(
                    reinterpret_cast<const unsigned int*>(data)[i]);
            else
                ok = false;
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            if (format.basetype == TypeDesc::UINT8)
                vals.emplace_back(
                    reinterpret_cast<const unsigned char*>(data)[i]);
            else
                ok = false;
        } else {
            ok = false;
        }
    }

    PyBuffer_Release(&view);
    return ok;
}

template<>
inline bool
py_buffer_to_stdvector(std::vector<std::string>&, const nb::handle&)
{
    return false;
}

template<>
inline bool
py_buffer_to_stdvector(std::vector<TypeDesc>&, const nb::handle&)
{
    return false;
}

template<typename T>
inline bool
py_to_stdvector(std::vector<T>& vals, const nb::handle& obj)
{
    if (PyTuple_Check(obj.ptr()))
        return py_indexable_pod_to_stdvector(vals, nb::borrow<nb::tuple>(obj));
    if (PyList_Check(obj.ptr()))
        return py_indexable_pod_to_stdvector(vals, nb::borrow<nb::list>(obj));
    if (PyObject_CheckBuffer(obj.ptr()) && !PyUnicode_Check(obj.ptr()))
        return py_buffer_to_stdvector(vals, obj);
    return py_scalar_pod_to_stdvector(vals, obj);
}

template<typename T>
inline nb::tuple
C_to_tuple(cspan<T> vals)
{
    nb::list list;
    for (size_t i = 0; i < vals.size(); ++i)
        list.append(nb::cast(vals[i]));
    return nb::steal<nb::tuple>(PyList_AsTuple(list.ptr()));
}

template<typename T>
inline nb::tuple
C_to_tuple(const T* vals, size_t size)
{
    nb::list list;
    for (size_t i = 0; i < size; ++i)
        list.append(nb::cast(vals[i]));
    return nb::steal<nb::tuple>(PyList_AsTuple(list.ptr()));
}

template<typename T>
inline nb::object
C_to_val_or_tuple(const T* vals, TypeDesc type, int nvalues = 1)
{
    OIIO_DASSERT(vals && nvalues);
    const size_t n = type.numelements() * type.aggregate * nvalues;
    if (n == 1 && !type.arraylen)
        return nb::cast(vals[0]);
    return C_to_tuple(vals, n);
}

template<>
inline nb::tuple
C_to_tuple(cspan<half> vals)
{
    nb::list list;
    for (size_t i = 0; i < vals.size(); ++i)
        list.append(static_cast<float>(vals[i]));
    return nb::steal<nb::tuple>(PyList_AsTuple(list.ptr()));
}

template<>
inline nb::tuple
C_to_tuple(const half* vals, size_t size)
{
    nb::list list;
    for (size_t i = 0; i < size; ++i)
        list.append(static_cast<float>(vals[i]));
    return nb::steal<nb::tuple>(PyList_AsTuple(list.ptr()));
}

template<>
inline nb::object
C_to_val_or_tuple(const half* vals, TypeDesc type, int nvalues)
{
    OIIO_DASSERT(vals && nvalues);
    const size_t n = type.numelements() * type.aggregate * nvalues;
    if (n == 1 && !type.arraylen)
        return nb::cast(static_cast<float>(vals[0]));
    return C_to_tuple(vals, n);
}

template<typename T, typename Obj>
bool
attribute_typed(T& myobj, string_view name, TypeDesc type, const Obj& dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::UINT8) {
        std::vector<unsigned char> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok)
            myobj.attribute(name, type, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate);
        if (ok) {
            std::vector<ustring> u;
            u.reserve(vals.size());
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute(name, type, u.data());
        }
        return ok;
    }
    return false;
}

template<typename T>
inline void
attribute_onearg(T& myobj, string_view name, const nb::handle& obj)
{
    if (nb::isinstance<nb::float_>(obj))
        myobj.attribute(name, nb::cast<float>(obj));
    else if (nb::isinstance<nb::int_>(obj))
        myobj.attribute(name, nb::cast<int>(obj));
    else if (nb::isinstance<nb::str>(obj))
        myobj.attribute(name, std::string(nb::cast<nb::str>(obj).c_str()));
    else if (nb::isinstance<nb::bytes>(obj)) {
        nb::bytes bytes = nb::cast<nb::bytes>(obj);
        myobj.attribute(name, std::string(bytes.c_str(), bytes.size()));
    } else
        throw nb::type_error("attribute() value must be int, float, or str");
}

template<class T>
inline nb::object
make_numpy_array(T* data, size_t size)
{
    nb::capsule owner(data, [](void* p) noexcept {
        delete[] reinterpret_cast<T*>(p);
    });
    nb::ndarray<nb::numpy, T, nb::shape<-1>> array(data, { size }, owner);
    return nb::cast(std::move(array), nb::rv_policy::move);
}

nb::object
make_pyobject(const void* data, TypeDesc type, int nvalues = 1,
              nb::handle defaultvalue = nb::none());

template<typename C>
inline void
delegate_setitem(C& self, const std::string& key, const nb::handle& obj)
{
    if (nb::isinstance<nb::float_>(obj))
        self[key] = nb::cast<float>(obj);
    else if (nb::isinstance<nb::int_>(obj))
        self[key] = nb::cast<int>(obj);
    else if (nb::isinstance<nb::str>(obj))
        self[key] = std::string(nb::cast<nb::str>(obj).c_str());
    else if (nb::isinstance<nb::bytes>(obj)) {
        nb::bytes bytes = nb::cast<nb::bytes>(obj);
        self[key]       = std::string(bytes.c_str(), bytes.size());
    } else
        throw std::invalid_argument("Bad type for __setitem__");
}

}  // namespace PyOpenImageIO
