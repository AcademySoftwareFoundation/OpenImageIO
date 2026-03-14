// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

OIIO_NAMESPACE_USING

ROI
imagespec_get_roi(const OpenImageIO::v3_1::ImageSpec& spec)
{
    return OpenImageIO::v3_1::get_roi(spec);
}


ROI
imagespec_get_roi_full(const OpenImageIO::v3_1::ImageSpec& spec)
{
    return OpenImageIO::v3_1::get_roi_full(spec);
}


void
imagespec_set_roi(OpenImageIO::v3_1::ImageSpec& spec, const ROI& roi)
{
    OpenImageIO::v3_1::set_roi(spec, roi);
}


void
imagespec_set_roi_full(OpenImageIO::v3_1::ImageSpec& spec, const ROI& roi)
{
    OpenImageIO::v3_1::set_roi_full(spec, roi);
}

}  // namespace


namespace PyOpenImageIO {

void
declare_imagespec(nb::module_& m)
{
    // This is intentionally not a full ImageSpec port yet. It only exists
    // to support ROI parity until py_imagespec.cpp grows into the real
    // binding.
    nb::class_<ImageSpec>(m, "ImageSpec")
        .def(nb::init<>())
        .def("__init__",
             [](ImageSpec* self, int xres, int yres, int nchans, int format) {
                 new (self)
                     ImageSpec(xres, yres, nchans,
                               TypeDesc(static_cast<TypeDesc::BASETYPE>(
                                   format)));
             })
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

    m.def("get_roi", &imagespec_get_roi);
    m.def("get_roi_full", &imagespec_get_roi_full);
    m.def("set_roi", &imagespec_set_roi);
    m.def("set_roi_full", &imagespec_set_roi_full);
    m.attr("UINT8") = static_cast<int>(TypeDesc::UINT8);
}

}  // namespace PyOpenImageIO
