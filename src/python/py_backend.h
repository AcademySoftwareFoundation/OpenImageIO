// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#if defined(OIIO_PY_BACKEND_NANOBIND)

#    include <nanobind/make_iterator.h>
#    include <nanobind/nanobind.h>
#    include <nanobind/ndarray.h>
#    include <nanobind/operators.h>
#    include <nanobind/stl/string.h>
#    include <nanobind/stl/vector.h>

namespace py = nanobind;
using py_module = nanobind::module_;
using namespace py::literals;

#    define OIIO_PY_DEF_RW(name, member) .def_rw(name, member)
#    define OIIO_PY_DEF_PROP_RO(name, member) .def_prop_ro(name, member)
#    define OIIO_PY_DEF_PROP_RW(name, get, set) .def_prop_rw(name, get, set)
#    define OIIO_PY_DEF_READONLY_STATIC_LAMBDA(name, lambda)                 \
        .def_prop_ro_static(name, lambda)

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

inline constexpr auto ref_internal = py::rv_policy::reference_internal;

template<typename T>
inline auto
str(T&& x)
{
    return std::forward<T>(x);
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

#else // pybind11

#    include <pybind11/numpy.h>
#    include <pybind11/operators.h>
#    include <pybind11/pybind11.h>

namespace py = pybind11;
using py_module = pybind11::module;
using namespace py::literals;

#    define OIIO_PY_DEF_RW(name, member) .def_readwrite(name, member)
#    define OIIO_PY_DEF_PROP_RO(name, member) .def_property_readonly(name, member)
#    define OIIO_PY_DEF_PROP_RW(name, get, set) .def_property(name, get, set)
#    define OIIO_PY_DEF_READONLY_STATIC_LAMBDA(name, lambda)                 \
        .def_property_readonly_static(name, lambda)

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

inline constexpr auto ref_internal =
    py::return_value_policy::reference_internal;

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
