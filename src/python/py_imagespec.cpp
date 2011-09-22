/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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
using namespace boost::python;
using namespace std;

/// Accessor for channelnames, converts a vector<string> to a tuple
object ImageSpec_get_channelnames(const ImageSpec& imageSpec)
{
    PyObject* result = PyTuple_New(imageSpec.channelnames.size());

    for (unsigned int i = 0; i < imageSpec.channelnames.size(); ++i) {
        PyTuple_SetItem(result, i, PyString_FromString(imageSpec.channelnames[i].c_str()));
    }

    return object(handle<>(result));
}

/// Mutator for channelnames, sets a vector<string> using a tuple
void ImageSpec_set_channelnames(ImageSpec& imageSpec, const tuple& channelnames)
{
    const unsigned int length = len(channelnames);
    imageSpec.channelnames.resize(length);

    for (unsigned i = 0; i < length; ++i) {
        imageSpec.channelnames[i] = extract<string>(channelnames[i]);
    }
}

/// In this version we lose some functionality - all the inputs are
/// assumed to have been autostride.
object ImageSpec_auto_stride_1(const TypeDesc& format, int nchannels,
                              int width, int height)
{
    stride_t x = AutoStride, y = AutoStride, z = AutoStride;
    ImageSpec::auto_stride(x, y, z, format, nchannels, width, height);
    return object(handle<>(Py_BuildValue("(iii)", x, y, z)));
}

/// xstride is assumed to have been AutoStride.
stride_t ImageSpec_auto_stride_2(const TypeDesc& format, int nchannels)
{
    stride_t x = AutoStride;
    ImageSpec::auto_stride(x, format, nchannels);
    return x;
}


stride_t ImageSpec_channel_bytes_1(ImageSpec& spec)
{
    return spec.channel_bytes ();
}

stride_t ImageSpec_channel_bytes_2(ImageSpec& spec, int chan, bool native)
{
    return spec.channel_bytes (chan, native);
}



stride_t ImageSpec_pixel_bytes_1(ImageSpec& spec, bool native)
{
    return spec.pixel_bytes (native);
}

stride_t ImageSpec_pixel_bytes_2(ImageSpec& spec, int firstchan, int nchans, bool native)
{
    return spec.pixel_bytes (firstchan, nchans, native);
}



void declare_imagespec()
{
    class_<ImageSpec>("ImageSpec")
        .def_readwrite("x",             &ImageSpec::x)
        .def_readwrite("y",             &ImageSpec::y)
        .def_readwrite("z",             &ImageSpec::z)
        .def_readwrite("width",         &ImageSpec::width)
        .def_readwrite("height",        &ImageSpec::height)
        .def_readwrite("full_x",        &ImageSpec::full_x)
        .def_readwrite("full_y",        &ImageSpec::full_y)
        .def_readwrite("full_z",        &ImageSpec::full_z)
        .def_readwrite("full_width",    &ImageSpec::full_width)
        .def_readwrite("full_height",   &ImageSpec::full_height)
        .def_readwrite("full_depth",    &ImageSpec::full_depth)
        .def_readwrite("tile_width",    &ImageSpec::tile_width)
        .def_readwrite("tile_height",   &ImageSpec::tile_height)
        .def_readwrite("tile_depth",    &ImageSpec::tile_depth)
        .def_readwrite("format",        &ImageSpec::format) //TypeDesc
        .def_readwrite("nchannels",     &ImageSpec::nchannels)
        .add_property("channelnames",   &ImageSpec_get_channelnames,
                                        &ImageSpec_set_channelnames)
        .def_readwrite("alpha_channel", &ImageSpec::alpha_channel)
        .def_readwrite("z_channel",     &ImageSpec::z_channel)
        .def_readwrite("quant_black",   &ImageSpec::quant_black)
        .def_readwrite("quant_white",   &ImageSpec::quant_white)
        .def_readwrite("quant_min",     &ImageSpec::quant_min)
        .def_readwrite("quant_max",     &ImageSpec::quant_max)
        .add_property("extra_attribs", 
            make_getter(&ImageSpec::extra_attribs))//ImageIOParameterList
        .def(init<int, int, int, TypeDesc>())
        .def(init<TypeDesc>())
        .def(init<const ImageSpec&>())
        .def("set_format",              &ImageSpec::set_format)
        .def("default_channel_names",   &ImageSpec::default_channel_names)
        .def("format_from_quantize",    &ImageSpec::format_from_quantize)
        .staticmethod("format_from_quantize")
        .def("channel_bytes",           &ImageSpec_channel_bytes_1)
        .def("channel_bytes",           &ImageSpec_channel_bytes_2)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_1)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_2)
        .def("scanline_bytes",          &ImageSpec::scanline_bytes)
        .def("tile_pixels",             &ImageSpec::tile_pixels)
        .def("tile_bytes",              &ImageSpec::tile_bytes)
        .def("image_pixels",            &ImageSpec::image_pixels)
        .def("image_bytes",             &ImageSpec::image_bytes)
        .def("size_t_safe",             &ImageSpec::size_t_safe) 
         // auto_stride is overloaded so needs explicit function casts
        .def("auto_stride",     &ImageSpec_auto_stride_1)
        .def("auto_stride",     &ImageSpec_auto_stride_2)
        .staticmethod("auto_stride")
 
        //to do: everything from here is yet to be tested from python
        .def("attribute", (void (ImageSpec::*)(const std::string&,
                                TypeDesc, const void*)) 
                                &ImageSpec::attribute)

        .def("attribute", (void (ImageSpec::*)(const std::string&, unsigned int))
                                &ImageSpec::attribute)

        .def("attribute", (void (ImageSpec::*)(const std::string&, int)) 
                                &ImageSpec::attribute)

        .def("attribute", (void (ImageSpec::*)(const std::string&, float ))
                                &ImageSpec::attribute)

        .def("attribute", (void (ImageSpec::*)(const std::string&, const char*))
                                &ImageSpec::attribute)

        .def("attribute", (void (ImageSpec::*)(const std::string&, const std::string&))
                                &ImageSpec::attribute)


        //to add: ImageIoParameter * find_attribute() (both)
        
        // TODO: Default arguments of these 3 functions not exposed properly
        .def("get_int_attribute", &ImageSpec::get_int_attribute) 
        .def("get_float_attribute", &ImageSpec::get_float_attribute)
        .def("get_string_attribute", &ImageSpec::get_string_attribute)

        .def("metadata_val", &ImageSpec::metadata_val)
    ;          
}

} // namespace PyOpenImageIO

