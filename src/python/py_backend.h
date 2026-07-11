// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#if defined(OIIO_PY_BACKEND_NANOBIND)

#    include <nanobind/make_iterator.h>
#    include <nanobind/nanobind.h>
#    include <nanobind/ndarray.h>
#    include <nanobind/operators.h>
#    include <nanobind/stl/array.h>
#    include <nanobind/stl/optional.h>
#    include <nanobind/stl/string.h>
#    include <nanobind/stl/string_view.h>
#    include <nanobind/stl/unique_ptr.h>
#    include <nanobind/stl/vector.h>

namespace py    = nanobind;
using py_module = nanobind::module_;
using namespace py::literals;

#    define OIIO_PY_RW def_rw
#    define OIIO_PY_RO def_ro
#    define OIIO_PY_PROP_RO def_prop_ro
#    define OIIO_PY_PROP_RW def_prop_rw
// Allow assigning None to a property (nanobind rejects it unless annotated).
#    define OIIO_PY_PROP_RW_NONE(name, getter, setter) \
        def_prop_rw(name, getter, setter, py::for_setter(py::arg().none()))
#    define OIIO_PY_RO_STATIC def_prop_ro_static
#    define OIIO_PY_RO def_ro

namespace oiio_py {

inline std::string
str_to_stdstring(const py::handle& handle)
{
    return std::string(py::cast<py::str>(handle).c_str());
}

template<typename F>
inline py::tuple
make_tuple(size_t size, F&& fill)
{
    py::list list;
    for (size_t i = 0; i < size; ++i) {
        list.append(fill(i));
    }
    return py::steal<py::tuple>(PyList_AsTuple(list.ptr()));
}

inline constexpr auto ref          = py::rv_policy::reference;
inline constexpr auto ref_internal = py::rv_policy::reference_internal;

// Copy into std::string so temporary string_view contents stay valid;
// nanobind's string caster turns this into a Python str.
template<typename T>
inline std::string
str(T&& x)
{
    return std::string(std::forward<T>(x));
}

inline void
throw_key_error(const std::string& msg)
{
    throw py::key_error(msg.c_str());
}

inline std::string
bytes_to_stdstring(const py::bytes& b)
{
    return std::string(b.c_str(), b.size());
}

template<typename Container>
inline auto
make_iterator(Container& container)
{
    return py::make_iterator(py::type<Container>(), "iterator",
                             container.begin(), container.end());
}

// Return a py::object from a binding lambda (nanobind needs borrow).
inline py::object
return_object(py::object obj)
{
    return py::borrow(obj);
}

// TRANSFERS ownership of data; builds a 1D numpy array of length size.
template<class T>
inline py::object
make_numpy_array(T* data, size_t size)
{
    py::capsule owner(data, [](void* p) noexcept {
        delete[] reinterpret_cast<T*>(p);
    });
    py::ndarray<py::numpy, T, py::shape<-1>> array(data, { size }, owner);
    return py::cast(std::move(array), py::rv_policy::move);
}

}  // namespace oiio_py

#else  // pybind11

#    include <pybind11/numpy.h>
#    include <pybind11/operators.h>
#    include <pybind11/pybind11.h>

namespace py    = pybind11;
using py_module = pybind11::module;
using namespace py::literals;

#    define OIIO_PY_RW def_readwrite
#    define OIIO_PY_RO def_readonly
#    define OIIO_PY_PROP_RO def_property_readonly
#    define OIIO_PY_PROP_RW def_property
#    define OIIO_PY_PROP_RW_NONE def_property
#    define OIIO_PY_RO_STATIC def_property_readonly_static
#    define OIIO_PY_RO def_readonly

namespace oiio_py {

inline std::string
str_to_stdstring(const py::handle& handle)
{
    return std::string(py::cast<py::str>(handle));
}

template<typename F>
inline py::tuple
make_tuple(size_t size, F&& fill)
{
    py::tuple result(size);
    for (size_t i = 0; i < size; ++i) {
        result[i] = fill(i);
    }
    return result;
}

inline constexpr auto ref = py::return_value_policy::reference;
inline constexpr auto ref_internal = py::return_value_policy::reference_internal;

template<typename T>
inline auto
str(T&& x)
{
    return py::str(std::forward<T>(x));
}

inline void
throw_key_error(const std::string& msg)
{
    throw py::key_error(msg);
}

inline std::string
bytes_to_stdstring(const py::bytes& b)
{
    return std::string(b);
}

template<typename Container>
inline auto
make_iterator(Container& container)
{
    return py::make_iterator(container.begin(), container.end());
}

inline py::object
return_object(py::object obj)
{
    return obj;
}

// TRANSFERS ownership of data; builds a 1D numpy array of length size.
template<class T>
inline py::object
make_numpy_array(T* data, size_t size)
{
    py::capsule free_when_done(data, [](void* f) {
        delete[] (reinterpret_cast<T*>(f));
    });
    return py::array_t<T>({ size }, { sizeof(T) }, data, free_when_done);
}

}  // namespace oiio_py

#endif
