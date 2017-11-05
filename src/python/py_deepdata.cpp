/*
  Copyright 2015 Larry Gritz and the other authors and contributors.
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

#include "py_oiio.h"

namespace PyOpenImageIO
{


void
DeepData_init (DeepData &dd, int npix, int nchan, py::object py_channeltypes,
               py::object py_channelnames)
{
    std::vector<TypeDesc> chantypes;
    py_to_stdvector (chantypes, py_channeltypes);
    std::vector<std::string> channames;
    py_to_stdvector (channames, py_channelnames);
    py::gil_scoped_release gil;
    dd.init (npix, nchan, chantypes, channames);
}



void
DeepData_init_spec (DeepData &dd, const ImageSpec &spec)
{
    py::gil_scoped_release gil;
    dd.init (spec);
}



void
DeepData_set_nsamples (DeepData &dd, int pixel, int nsamples)
{
    dd.set_samples (pixel, uint32_t(nsamples));
}



void
DeepData_set_deep_value_float (DeepData &dd, int pixel,
                               int channel, int sample, float value)
{
    dd.set_deep_value (pixel, channel, sample, value);
}



void
DeepData_set_deep_value_uint (DeepData &dd, int pixel,
                              int channel, int sample, uint32_t value)
{
    dd.set_deep_value (pixel, channel, sample, value);
}





// Declare the OIIO DeepData class to Python
void
declare_deepdata (py::module& m)
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
                      [](const DeepData& d) { return d.Zback_channel(); })

        .def(py::init<>())
        .def("init",  &DeepData_init,
             "npixels"_a, "nchannels"_a, "channeltypes"_a, "channelnames"_a)
        .def("init",  &DeepData_init_spec)
        .def("clear", &DeepData::clear)
        .def("free",  &DeepData::free)
        .def("initialized",     &DeepData::initialized)
        .def("allocated",       &DeepData::allocated)

        .def("samples", [](const DeepData& dd, int pixel){
                return (int)dd.samples(pixel); },
                "pixel"_a)
        .def("set_samples",     &DeepData::set_samples,
             "pixel"_a, "nsamples"_a)
        .def("capacity", [](const DeepData& dd, int pixel){
                return (int)dd.capacity(pixel); },
                "pixel"_a)
        .def("set_capacity",    &DeepData::set_capacity,
             "pixel"_a, "nsamples"_a)
        .def("insert_samples",  &DeepData::insert_samples,
             "pixel"_a, "samplepos"_a, "nsamples"_a=1)
        .def("erase_samples",   &DeepData::erase_samples,
             "pixel"_a, "samplepos"_a, "nsamples"_a=1)
        .def("channelname", [](const DeepData& dd, int c){
                return (std::string)dd.channelname(c); })
        .def("channeltype",     &DeepData::channeltype)
        .def("channelsize", [](const DeepData& dd, int c){
                return (int)dd.channelsize(c); })
        .def("samplesize",      &DeepData::samplesize)
        .def("deep_value",      &DeepData::deep_value,
             "pixel"_a, "channel"_a, "sample"_a)
        .def("deep_value_uint", &DeepData::deep_value_uint,
             "pixel"_a, "channel"_a, "sample"_a)
        .def("set_deep_value",  &DeepData_set_deep_value_float,
             "pixel"_a, "channel"_a, "sample"_a, "value"_a)
        .def("set_deep_value_uint", &DeepData_set_deep_value_uint,
             "pixel"_a, "channel"_a, "sample"_a, "value"_a)
        .def("copy_deep_sample", &DeepData::copy_deep_sample,
             "pixel"_a, "sample"_a, "src"_a, "srcpixel"_a, "srcsample"_a)
        .def("copy_deep_pixel", &DeepData::copy_deep_pixel,
             "pixel"_a, "src"_a, "srcpixel"_a)
        .def("split", &DeepData::split, "pixel"_a, "depth"_a)
        .def("sort", &DeepData::sort, "pixel"_a)
        .def("merge_overlaps", &DeepData::merge_overlaps, "pixel"_a)
        .def("merge_deep_pixels", &DeepData::merge_deep_pixels,
             "pixel"_a, "src"_a, "srcpixel"_a)
        .def("occlusion_cull", &DeepData::occlusion_cull, "pixel"_a)
        .def("opaque_z", &DeepData::opaque_z, "pixel"_a)
    ;
}

} // namespace PyOpenImageIO

