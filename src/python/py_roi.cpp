// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


static ROI ROI_All;


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
declare_roi(py::module& m)
{
    using namespace py;

    py::class_<ROI>(m, "ROI")
        .def_readwrite("xbegin", &ROI::xbegin)
        .def_readwrite("xend", &ROI::xend)
        .def_readwrite("ybegin", &ROI::ybegin)
        .def_readwrite("yend", &ROI::yend)
        .def_readwrite("zbegin", &ROI::zbegin)
        .def_readwrite("zend", &ROI::zend)
        .def_readwrite("chbegin", &ROI::chbegin)
        .def_readwrite("chend", &ROI::chend)

        .def(py::init<>())
        .def(py::init<int, int, int, int>())
        .def(py::init<int, int, int, int, int, int>())
        .def(py::init<int, int, int, int, int, int, int, int>())
        .def(py::init<const ROI&>())

        // .def("defined",   [](const ROI& roi) { return (int)roi.defined(); })
        .def_property_readonly("defined", &ROI::defined)
        .def_property_readonly("width", &ROI::width)
        .def_property_readonly("height", &ROI::height)
        .def_property_readonly("depth", &ROI::depth)
        .def_property_readonly("nchannels", &ROI::nchannels)
        .def_property_readonly("npixels", &ROI::npixels)
        .def("contains", &roi_contains_coord, "x"_a, "y"_a, "z"_a = 0,
             "ch"_a = 0)
        .def("contains", &roi_contains_roi, "other"_a)

        .def_readonly_static("All", &ROI_All)

        // Conversion to string
        .def("__str__",
             [](const ROI& roi) {
                 return PY_STR(Strutil::fmt::format("{}", roi));
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
