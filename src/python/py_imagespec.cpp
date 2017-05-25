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


// Accessor for channelnames, converts a vector<string> to a tuple
static object
ImageSpec_get_channelnames(const ImageSpec& spec)
{
    size_t size = spec.channelnames.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i) {
#if PY_MAJOR_VERSION >= 3
        PyObject* name = PyUnicode_FromString(spec.channelnames[i].c_str());
#else
        PyObject* name = PyString_FromString(spec.channelnames[i].c_str());
#endif
        PyTuple_SetItem(result, i, name);
    }
    return object(handle<>(result));
}

// Mutator for channelnames, sets a vector<string> using a tuple
static void
ImageSpec_set_channelnames(ImageSpec& spec, const tuple& channelnames)
{
    const size_t length = len(channelnames);
    spec.channelnames.resize(length);
    for (size_t i = 0; i < length; ++i) {
        extract<std::string> e (channelnames[i]);
        spec.channelnames[i] = e.check() ? e() : std::string();
    }
}


// Accessor for channelformats, converts a vector<TypeDesc> to a tuple
// of ints holding the BASETYPE.
static object
ImageSpec_get_channelformats(const ImageSpec& spec)
{
    size_t size = spec.channelformats.size();
    PyObject* result = PyTuple_New(size);
    for (size_t i = 0; i < size; ++i)
#if PY_MAJOR_VERSION >= 3
        PyTuple_SetItem(result, i, PyLong_FromLong((long)spec.channelformats[i].basetype));
#else
        PyTuple_SetItem(result, i, PyInt_FromLong((long)spec.channelformats[i].basetype));
#endif
    return object(handle<>(result));
}

// Mutator for channelformats, initialized using a tuple whose int entries
// give the BASETYPE values.
static void
ImageSpec_set_channelformats(ImageSpec& spec, const tuple& channelformats)
{
    const size_t length = len(channelformats);
    spec.channelformats.resize(length, spec.format);
    for (size_t i = 0; i < length; ++i) {
        extract<int> base (channelformats[i]);
        if (base.check()) {
            spec.channelformats[i] = (TypeDesc::BASETYPE)base();
            continue;
        }
        extract<TypeDesc> type (channelformats[i]);
        if (type.check()) {
            spec.channelformats[i] = type();
            continue;
        }
    }
}


#if 0
// In this version we lose some functionality - all the inputs are
// assumed to have been autostride.
static object
ImageSpec_auto_stride_1(const TypeDesc& format, int nchannels,
                        int width, int height)
{
    stride_t x = AutoStride, y = AutoStride, z = AutoStride;
    ImageSpec::auto_stride(x, y, z, format, nchannels, width, height);
    return object(handle<>(Py_BuildValue("(iii)", x, y, z)));
}

// xstride is assumed to have been AutoStride.
static stride_t ImageSpec_auto_stride_2(const TypeDesc& format, int nchannels)
{
    stride_t x = AutoStride;
    ImageSpec::auto_stride(x, format, nchannels);
    return x;
}
#endif


static stride_t ImageSpec_channel_bytes_1(ImageSpec& spec)
{
    return spec.channel_bytes ();
}

static stride_t ImageSpec_channel_bytes_2(ImageSpec& spec, int chan)
{
    return spec.channel_bytes (chan);
}

static stride_t ImageSpec_channel_bytes_3(ImageSpec& spec, int chan, bool native)
{
    return spec.channel_bytes (chan, native);
}



static stride_t ImageSpec_pixel_bytes_0(ImageSpec& spec)
{
    return spec.pixel_bytes ();
}

static stride_t ImageSpec_pixel_bytes_1(ImageSpec& spec, bool native)
{
    return spec.pixel_bytes (native);
}

static stride_t ImageSpec_pixel_bytes_2(ImageSpec& spec, int chbegin, int chend)
{
    return spec.pixel_bytes (chbegin, chend);
}

static stride_t ImageSpec_pixel_bytes_3(ImageSpec& spec, int chbegin, int chend, bool native)
{
    return spec.pixel_bytes (chbegin, chend, native);
}


BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageSpec_scanline_bytes_overloads,
                                       scanline_bytes, 0, 1)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageSpec_tile_bytes_overloads,
                                       tile_bytes, 0, 1)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageSpec_image_bytes_overloads,
                                       image_bytes, 0, 1)


static void
ImageSpec_set_format_2(ImageSpec& spec, TypeDesc::BASETYPE basetype)
{
    spec.set_format (basetype);
}


static void
ImageSpec_erase_attribute (ImageSpec& spec, const std::string &name,
                           TypeDesc searchtype=TypeDesc::UNKNOWN,
                           bool casesensitive=false)
{
    spec.erase_attribute (name, searchtype, casesensitive);
}


static void
ImageSpec_attribute_int (ImageSpec& spec, const std::string &name, int val)
{
    spec.attribute (name, val);
}


static void
ImageSpec_attribute_float (ImageSpec& spec, const std::string &name, float val)
{
    spec.attribute (name, val);
}


static void
ImageSpec_attribute_string (ImageSpec& spec, const std::string &name,
                            const std::string &val)
{
    spec.attribute (name, val);
}



static void
ImageSpec_attribute_typed (ImageSpec& spec, const std::string &name,
                           TypeDesc type, object &obj)
{
    attribute_typed (spec, name, type, obj);
}



static void
ImageSpec_attribute_tuple_typed (ImageSpec& spec, const std::string &name,
                           TypeDesc type, tuple &obj)
{
    attribute_tuple_typed (spec, name, type, obj);
}



static object
ImageSpec_get_attribute_typed (const ImageSpec& spec,
                               const std::string &name, TypeDesc type)
{
    ParamValue tmpparam;
    const ParamValue *p = spec.find_attribute (name, tmpparam, type);
    if (!p)
        return object();   // None
    type = p->type();
    if (type.basetype == TypeDesc::INT) {
#if PY_MAJOR_VERSION >= 3
        return C_to_val_or_tuple ((const int *)p->data(), type, PyLong_FromLong);
#else
        return C_to_val_or_tuple ((const int *)p->data(), type, PyInt_FromLong);
#endif
    }
    if (type.basetype == TypeDesc::FLOAT) {
        return C_to_val_or_tuple ((const float *)p->data(), type, PyFloat_FromDouble);
    }
    if (type.basetype == TypeDesc::STRING) {
#if PY_MAJOR_VERSION >= 3
        return C_to_val_or_tuple ((const char **)p->data(), type, PyUnicode_FromString);
#else
        return C_to_val_or_tuple ((const char **)p->data(), type, PyString_FromString);
#endif
    }
    return object();
}


static object
ImageSpec_get_attribute_untyped (const ImageSpec& spec,
                                 const std::string &name)
{
    return ImageSpec_get_attribute_typed (spec, name, TypeDesc::UNKNOWN);
}



static int
ImageSpec_get_int_attribute (const ImageSpec& spec, const char *name)
{
    return spec.get_int_attribute (name);
}


static int
ImageSpec_get_int_attribute_d (const ImageSpec& spec, const char *name,
                               int defaultval)
{
    return spec.get_int_attribute (name, defaultval);
}


static float
ImageSpec_get_float_attribute (const ImageSpec& spec, const char *name)
{
    return spec.get_float_attribute (name);
}


static float
ImageSpec_get_float_attribute_d (const ImageSpec& spec, const char *name,
                                 float defaultval)
{
    return spec.get_float_attribute (name, defaultval);
}


static std::string
ImageSpec_get_string_attribute (const ImageSpec& spec, const char *name)
{
    return spec.get_string_attribute (name);
}


static std::string
ImageSpec_get_string_attribute_d (const ImageSpec& spec, const char *name,
                                  const char *defaultval)
{
    return spec.get_string_attribute (name, defaultval);
}



static int
ImageSpec_channelindex (const ImageSpec& spec, const std::string &name)
{
    return spec.channelindex (name);
}



void declare_imagespec()
{
    class_<ImageSpec>("ImageSpec")
        .def_readwrite("x",             &ImageSpec::x)
        .def_readwrite("y",             &ImageSpec::y)
        .def_readwrite("z",             &ImageSpec::z)
        .def_readwrite("width",         &ImageSpec::width)
        .def_readwrite("height",        &ImageSpec::height)
        .def_readwrite("depth",         &ImageSpec::depth)
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
        .add_property("channelformats", &ImageSpec_get_channelformats,
                                        &ImageSpec_set_channelformats)
        .def_readwrite("alpha_channel", &ImageSpec::alpha_channel)
        .def_readwrite("z_channel",     &ImageSpec::z_channel)
        .def_readwrite("deep",          &ImageSpec::deep)
        .add_property("extra_attribs", 
            make_getter(&ImageSpec::extra_attribs))//ParamValueList
        
        .def(init<int, int, int, TypeDesc>())
        .def(init<int, int, int, TypeDesc::BASETYPE>())
        .def(init<const ROI&, TypeDesc>())
        .def(init<const ROI&, TypeDesc::BASETYPE>())
        .def(init<TypeDesc>())
        .def(init<TypeDesc::BASETYPE>())
        .def(init<const ImageSpec&>())
        .def("set_format",              &ImageSpec::set_format)
        .def("set_format",              &ImageSpec_set_format_2)
        .def("default_channel_names",   &ImageSpec::default_channel_names)
        .def("channel_bytes",           &ImageSpec_channel_bytes_1)
        .def("channel_bytes",           &ImageSpec_channel_bytes_2)
        .def("channel_bytes",           &ImageSpec_channel_bytes_3)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_0)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_1)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_2)
        .def("pixel_bytes",             &ImageSpec_pixel_bytes_3)
        .def("scanline_bytes",          &ImageSpec::scanline_bytes,
             ImageSpec_scanline_bytes_overloads(args("native")))
        .def("tile_bytes",          &ImageSpec::tile_bytes,
             ImageSpec_tile_bytes_overloads(args("native")))
        .def("image_bytes",          &ImageSpec::image_bytes,
             ImageSpec_image_bytes_overloads(args("native")))
        .def("tile_pixels",             &ImageSpec::tile_pixels)
        .def("image_pixels",            &ImageSpec::image_pixels)
        .def("size_t_safe",             &ImageSpec::size_t_safe)
        .def("channelindex",            &ImageSpec_channelindex)

        // For now, do not expose auto_stride.  It's not obvious that
        // anybody will want to do pointer work and strides from Python.
        //
        // auto_stride is overloaded so needs explicit function casts
        // .def("auto_stride",             &ImageSpec_auto_stride_1)
        // .def("auto_stride",             &ImageSpec_auto_stride_2)
        // .staticmethod("auto_stride")
 
        .def("attribute", &ImageSpec_attribute_float)
        .def("attribute", &ImageSpec_attribute_int)
        .def("attribute", &ImageSpec_attribute_string)
        .def("attribute", &ImageSpec_attribute_typed)
        .def("attribute", &ImageSpec_attribute_tuple_typed)
        .def("get_int_attribute", &ImageSpec_get_int_attribute)
        .def("get_int_attribute", &ImageSpec_get_int_attribute_d)
        .def("get_float_attribute", &ImageSpec_get_float_attribute)
        .def("get_float_attribute", &ImageSpec_get_float_attribute_d)
        .def("get_string_attribute", &ImageSpec_get_string_attribute)
        .def("get_string_attribute", &ImageSpec_get_string_attribute_d)
        .def("getattribute",  &ImageSpec_get_attribute_typed)
        .def("getattribute",  &ImageSpec_get_attribute_untyped)
        .def("get_attribute", &ImageSpec_get_attribute_typed) // DEPRECATED(1.7)
        .def("get_attribute", &ImageSpec_get_attribute_untyped) // DEPRECATED(1.7)
        .def("erase_attribute", &ImageSpec_erase_attribute,
             (arg("name")="", arg("type")=TypeDesc(TypeDesc::UNKNOWN),
              arg("casesensitive")=false))

        .def("metadata_val", &ImageSpec::metadata_val)
        .staticmethod("metadata_val")
    ;          
}

} // namespace PyOpenImageIO

