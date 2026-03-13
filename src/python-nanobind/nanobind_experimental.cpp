// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/strutil.h>

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;
using namespace nb::literals;
OIIO_NAMESPACE_USING

namespace {

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


ROI
imagespec_get_roi(const ImageSpec& spec)
{
    return get_roi(spec);
}


ROI
imagespec_get_roi_full(const ImageSpec& spec)
{
    return get_roi_full(spec);
}


void
imagespec_set_roi(ImageSpec& spec, const ROI& roi)
{
    set_roi(spec, roi);
}


void
imagespec_set_roi_full(ImageSpec& spec, const ROI& roi)
{
    set_roi_full(spec, roi);
}

}  // namespace


NB_MODULE(_nanobind_experimental, m)
{
    m.doc() = "Experimental OpenImageIO nanobind bindings.";

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

    nb::class_<ImageSpec>(m, "ImageSpec")
        .def(nb::init<>())
        .def_rw("x", &ImageSpec::x)
        .def_rw("y", &ImageSpec::y)
        .def_rw("z", &ImageSpec::z)
        .def_rw("width", &ImageSpec::width)
        .def_rw("height", &ImageSpec::height)
        .def_rw("depth", &ImageSpec::depth)
        .def_rw("full_x", &ImageSpec::full_x)
        .def_rw("full_y", &ImageSpec::full_y)
        .def_rw("full_z", &ImageSpec::full_z)
        .def_rw("full_width", &ImageSpec::full_width)
        .def_rw("full_height", &ImageSpec::full_height)
        .def_rw("full_depth", &ImageSpec::full_depth)
        .def_rw("nchannels", &ImageSpec::nchannels)
        .def_prop_ro("roi", &imagespec_get_roi)
        .def_prop_ro("roi_full", &imagespec_get_roi_full);

    m.def("union", &roi_union);
    m.def("intersection", &roi_intersection);
    m.def("get_roi", &get_roi);
    m.def("get_roi_full", &get_roi_full);
    m.def("set_roi", &imagespec_set_roi);
    m.def("set_roi_full", &imagespec_set_roi_full);
    m.attr("__version__") = OIIO_VERSION_STRING;
}
