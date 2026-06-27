// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


static bool
roi_contains_coord(const ROI& roi, int x, int y, int z, int ch)
{
    return roi.contains(x, y, z, ch);
}


static bool
roi_contains_roi(const ROI& roi, const ROI& other)
{
    return roi.contains(other);
}



// Declare the OIIO ROI class to Python
void
declare_roi(py_module& m)
{
    py::class_<ROI>(m, "ROI")
        OIIO_PY_DEF_RW("xbegin", &ROI::xbegin)
        OIIO_PY_DEF_RW("xend", &ROI::xend)
        OIIO_PY_DEF_RW("ybegin", &ROI::ybegin)
        OIIO_PY_DEF_RW("yend", &ROI::yend)
        OIIO_PY_DEF_RW("zbegin", &ROI::zbegin)
        OIIO_PY_DEF_RW("zend", &ROI::zend)
        OIIO_PY_DEF_RW("chbegin", &ROI::chbegin)
        OIIO_PY_DEF_RW("chend", &ROI::chend)

        .def(py::init<>())
        .def(py::init<int, int, int, int>())
        .def(py::init<int, int, int, int, int, int>())
        .def(py::init<int, int, int, int, int, int, int, int>())
        .def(py::init<const ROI&>())

        // .def("defined",   [](const ROI& roi) { return (int)roi.defined(); })
        OIIO_PY_DEF_PROP_RO("defined", &ROI::defined)
        OIIO_PY_DEF_PROP_RO("width", &ROI::width)
        OIIO_PY_DEF_PROP_RO("height", &ROI::height)
        OIIO_PY_DEF_PROP_RO("depth", &ROI::depth)
        OIIO_PY_DEF_PROP_RO("nchannels", &ROI::nchannels)
        OIIO_PY_DEF_PROP_RO("npixels", &ROI::npixels)
        .def("contains", &roi_contains_coord, "x"_a, "y"_a, "z"_a = 0,
             "ch"_a = 0)
        .def("contains", &roi_contains_roi, "other"_a)

        OIIO_PY_DEF_READONLY_STATIC_LAMBDA(
            "All", [](const py::object&) { return ROI::All(); })

        // Conversion to string
        .def("__str__",
             [](const ROI& roi) {
                 return oiio_py::str(Strutil::fmt::format("{}", roi));
             })

        // Copy
        .def("copy", [](const ROI& self) -> ROI { return self; })

        // roi_union, roi_intersection, get_roi(spec), get_roi_full(spec)
        // set_roi(spec,newroi), set_roi_full(newroi)

        // overloaded operators
        .def(py::self == py::self)  // operator==   // NOSONAR
        .def(py::self != py::self)  // operator!=   // NOSONAR
        ;

    m.def("union", &roi_union);
    m.def("intersection", &roi_intersection);
    m.def("get_roi", &get_roi);
    m.def("get_roi_full", &get_roi_full);
    m.def("set_roi", &set_roi);
    m.def("set_roi_full", &set_roi_full);
}


}  // namespace PyOpenImageIO
