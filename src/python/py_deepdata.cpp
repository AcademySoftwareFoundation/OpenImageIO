// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


void
DeepData_init(DeepData& dd, int npix, int nchan, py::object py_channeltypes,
              py::object py_channelnames)
{
    std::vector<TypeDesc> chantypes;
    py_to_stdvector(chantypes, py_channeltypes);
    std::vector<std::string> channames;
    py_to_stdvector(channames, py_channelnames);
    py::gil_scoped_release gil;
    dd.init(npix, nchan, chantypes, channames);
}



void
DeepData_init_spec(DeepData& dd, const ImageSpec& spec)
{
    py::gil_scoped_release gil;
    dd.init(spec);
}



void
DeepData_set_nsamples(DeepData& dd, int pixel, int nsamples)
{
    dd.set_samples(pixel, uint32_t(nsamples));
}



void
DeepData_set_deep_value_float(DeepData& dd, int pixel, int channel, int sample,
                              float value)
{
    dd.set_deep_value(pixel, channel, sample, value);
}



void
DeepData_set_deep_value_uint(DeepData& dd, int pixel, int channel, int sample,
                             uint32_t value)
{
    dd.set_deep_value(pixel, channel, sample, value);
}



// Declare the OIIO DeepData class to Python
void
declare_deepdata(py::module& m)
{
    using namespace pybind11::literals;

    py::class_<DeepData>(m, "DeepData")
        .def_property_readonly("pixels",
                               [](const DeepData& d) { return d.pixels(); })
        .def_property_readonly("channels",
                               [](const DeepData& d) { return d.channels(); })
        .def_property_readonly("A_channel",
                               [](const DeepData& d) { return d.A_channel(); })
        .def_property_readonly("AR_channel",
                               [](const DeepData& d) { return d.AR_channel(); })
        .def_property_readonly("AG_channel",
                               [](const DeepData& d) { return d.AG_channel(); })
        .def_property_readonly("AB_channel",
                               [](const DeepData& d) { return d.AB_channel(); })
        .def_property_readonly("Z_channel",
                               [](const DeepData& d) { return d.Z_channel(); })
        .def_property_readonly("Zback_channel",
                               [](const DeepData& d) {
                                   return d.Zback_channel();
                               })

        .def(py::init<>())
        .def("init", &DeepData_init, "npixels"_a, "nchannels"_a,
             "channeltypes"_a, "channelnames"_a)
        .def("init", &DeepData_init_spec)
        .def("clear", &DeepData::clear)
        .def("free", &DeepData::free)
        .def("initialized", &DeepData::initialized)
        .def("allocated", &DeepData::allocated)

        .def(
            "samples",
            [](const DeepData& dd, int pixel) { return (int)dd.samples(pixel); },
            "pixel"_a)
        .def("set_samples", &DeepData::set_samples, "pixel"_a, "nsamples"_a)
        .def(
            "capacity",
            [](const DeepData& dd, int pixel) {
                return (int)dd.capacity(pixel);
            },
            "pixel"_a)
        .def("set_capacity", &DeepData::set_capacity, "pixel"_a, "nsamples"_a)
        .def("insert_samples", &DeepData::insert_samples, "pixel"_a,
             "samplepos"_a, "nsamples"_a = 1)
        .def("erase_samples", &DeepData::erase_samples, "pixel"_a,
             "samplepos"_a, "nsamples"_a = 1)
        .def("channelname", [](const DeepData& dd,
                               int c) { return PY_STR(dd.channelname(c)); })
        .def("channeltype", &DeepData::channeltype)
        .def("channelsize",
             [](const DeepData& dd, int c) { return (int)dd.channelsize(c); })
        .def("same_channeltypes", &DeepData::same_channeltypes)
        .def("samplesize", &DeepData::samplesize)
        .def("deep_value", &DeepData::deep_value, "pixel"_a, "channel"_a,
             "sample"_a)
        .def("deep_value_uint", &DeepData::deep_value_uint, "pixel"_a,
             "channel"_a, "sample"_a)
        .def("set_deep_value", &DeepData_set_deep_value_float, "pixel"_a,
             "channel"_a, "sample"_a, "value"_a)
        .def("set_deep_value_uint", &DeepData_set_deep_value_uint, "pixel"_a,
             "channel"_a, "sample"_a, "value"_a)
        .def("copy_deep_sample", &DeepData::copy_deep_sample, "pixel"_a,
             "sample"_a, "src"_a, "srcpixel"_a, "srcsample"_a)
        .def("copy_deep_pixel", &DeepData::copy_deep_pixel, "pixel"_a, "src"_a,
             "srcpixel"_a)
        .def("split", &DeepData::split, "pixel"_a, "depth"_a)
        .def("sort", &DeepData::sort, "pixel"_a)
        .def("merge_overlaps", &DeepData::merge_overlaps, "pixel"_a)
        .def("merge_deep_pixels", &DeepData::merge_deep_pixels, "pixel"_a,
             "src"_a, "srcpixel"_a)
        .def("occlusion_cull", &DeepData::occlusion_cull, "pixel"_a)
        .def("opaque_z", &DeepData::opaque_z, "pixel"_a);
}

}  // namespace PyOpenImageIO
