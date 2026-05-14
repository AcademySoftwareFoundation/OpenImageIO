// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

using namespace OIIO;

bool
roi_contains_coord(const ROI& roi, int x, int y, int z, int ch)
{
    return roi.contains(x, y, z, ch);
}


bool
roi_contains_roi(const ROI& roi, const ROI& other)
{
    return roi.contains(other);
}

}  // namespace


namespace PyOpenImageIO {

void
declare_roi(nb::module_& m)
{
    nb::class_<ROI> roi(m, "ROI");
    roi.def_rw("xbegin", &ROI::xbegin)
        .def_rw("xend", &ROI::xend)
        .def_rw("ybegin", &ROI::ybegin)
        .def_rw("yend", &ROI::yend)
        .def_rw("zbegin", &ROI::zbegin)
        .def_rw("zend", &ROI::zend)
        .def_rw("chbegin", &ROI::chbegin)
        .def_rw("chend", &ROI::chend)
        .def(nb::init<>())
        .def(nb::init<int, int, int, int>())
        .def(nb::init<int, int, int, int, int, int>())
        .def(nb::init<int, int, int, int, int, int, int, int>())
        .def(nb::init<const ROI&>())
        .def_prop_ro("defined", &ROI::defined)
        .def_prop_ro("width", &ROI::width)
        .def_prop_ro("height", &ROI::height)
        .def_prop_ro("depth", &ROI::depth)
        .def_prop_ro("nchannels", &ROI::nchannels)
        .def_prop_ro("npixels", &ROI::npixels)
        .def("contains", &roi_contains_coord, "x"_a, "y"_a, "z"_a = 0,
             "ch"_a = 0)
        .def("contains", &roi_contains_roi, "other"_a)
        .def_prop_ro_static("All", [](nb::handle) { return ROI::All(); })
        .def("__str__",
             [](const ROI& roi_) { return Strutil::fmt::format("{}", roi_); })
        .def("copy", [](const ROI& self) { return self; })
        .def(nb::self == nb::self)
        .def(nb::self != nb::self);

    m.def("union", &roi_union);
    m.def("intersection", &roi_intersection);
    m.def("get_roi", &get_roi);
    m.def("get_roi_full", &get_roi_full);
    m.def("set_roi", &set_roi);
    m.def("set_roi_full", &set_roi_full);
}

}  // namespace PyOpenImageIO
