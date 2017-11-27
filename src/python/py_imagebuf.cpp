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

#include <memory>

#include "py_oiio.h"
#include <OpenImageIO/platform.h>


namespace PyOpenImageIO
{
using namespace boost::python;



std::string
ImageBuf_name (const ImageBuf &buf)
{
    return buf.name();
}


std::string
ImageBuf_file_format_name (const ImageBuf &buf)
{
    return buf.file_format_name();
}


void
ImageBuf_reset_name (ImageBuf &buf, const std::string &name)
{
    buf.reset (name);
}

void
ImageBuf_reset_name2 (ImageBuf &buf, const std::string &name,
                      int subimage, int miplevel)
{
    buf.reset (name, subimage, miplevel);
}

void
ImageBuf_reset_name_config (ImageBuf &buf, const std::string &name,
                      int subimage, int miplevel, const ImageSpec &config)
{
    buf.reset (name, subimage, miplevel, NULL, &config);
}

void
ImageBuf_reset_spec (ImageBuf &buf, const ImageSpec &spec)
{
    buf.reset (spec);
}



bool
ImageBuf_read (ImageBuf &buf, int subimage=0, int miplevel=0,
               bool force=false, TypeDesc convert=TypeUnknown)
{
    ScopedGILRelease gil;
    return buf.read (subimage, miplevel, force, convert);
}


bool
ImageBuf_read2 (ImageBuf &buf, int subimage=0, int miplevel=0,
                bool force=false,
                TypeDesc::BASETYPE convert=TypeDesc::UNKNOWN)
{
    ScopedGILRelease gil;
    return buf.read (subimage, miplevel, force, convert);
}


BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_read_overloads,
                                ImageBuf_read, 1, 5)
BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_read2_overloads,
                                ImageBuf_read2, 1, 5)


bool
ImageBuf_write (const ImageBuf &buf, const std::string &filename,
                const std::string &fileformat="")
{
    ScopedGILRelease gil;
    return buf.write (filename, fileformat);
}


BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_write_overloads,
                                ImageBuf_write, 2, 3)


bool
ImageBuf_make_writeable (ImageBuf &buf, bool keep_cache_type)
{
    ScopedGILRelease gil;
    return buf.make_writeable (keep_cache_type);
}



void
ImageBuf_set_write_format (ImageBuf &buf, TypeDesc::BASETYPE format)
{
    buf.set_write_format (format);
}



bool
ImageBuf_copy (ImageBuf &buf, const ImageBuf &src,
               TypeDesc format = TypeUnknown)
{
    ScopedGILRelease gil;
    return buf.copy (src, format);
}


bool
ImageBuf_copy2 (ImageBuf &buf, const ImageBuf &src,
                TypeDesc::BASETYPE format = TypeDesc::UNKNOWN)
{
    ScopedGILRelease gil;
    return buf.copy (src, format);
}


BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_copy_overloads,
                                ImageBuf_copy, 2, 3)
BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_copy2_overloads,
                                ImageBuf_copy2, 2, 3)



void
ImageBuf_set_full (ImageBuf &buf, int xbegin, int xend, int ybegin, int yend,
                   int zbegin, int zend)
{
    buf.set_full (xbegin, xend, ybegin, yend, zbegin, zend);
}



float
ImageBuf_getchannel (const ImageBuf &buf, int x, int y, int z, int c,
                     ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    return buf.getchannel(x, y, z, c, wrap);
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_getchannel_overloads,
                                ImageBuf_getchannel, 5, 6)



object
ImageBuf_getpixel (const ImageBuf &buf, int x, int y, int z=0,
                   ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    int nchans = buf.nchannels();
    float *pixel = ALLOCA (float, nchans);
    buf.getpixel (x, y, z, pixel, nchans, wrap);
    PyObject *result = PyTuple_New (nchans);
    for (int i = 0;  i < nchans;  ++i)
        PyTuple_SetItem (result, i, PyFloat_FromDouble(pixel[i]));
    return object(handle<>(result));
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_getpixel_overloads,
                                ImageBuf_getpixel, 3, 5)



object
ImageBuf_interppixel (const ImageBuf &buf, float x, float y,
                      ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    int nchans = buf.nchannels();
    float *pixel = ALLOCA (float, nchans);
    buf.interppixel (x, y, pixel, wrap);
    PyObject *result = PyTuple_New (nchans);
    for (int i = 0;  i < nchans;  ++i)
        PyTuple_SetItem (result, i, PyFloat_FromDouble(pixel[i]));
    return object(handle<>(result));
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_interppixel_overloads,
                                ImageBuf_interppixel, 3, 4)



object
ImageBuf_interppixel_NDC (const ImageBuf &buf, float x, float y,
                          ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    int nchans = buf.nchannels();
    float *pixel = ALLOCA (float, nchans);
    buf.interppixel_NDC (x, y, pixel, wrap);
    return C_to_val_or_tuple (pixel, TypeDesc(TypeDesc::FLOAT,nchans),
                              PyFloat_FromDouble);
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_interppixel_NDC_overloads,
                                ImageBuf_interppixel_NDC, 3, 4)



object
ImageBuf_interppixel_bicubic (const ImageBuf &buf, float x, float y,
                              ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    int nchans = buf.nchannels();
    float *pixel = ALLOCA (float, nchans);
    buf.interppixel_bicubic (x, y, pixel, wrap);
    PyObject *result = PyTuple_New (nchans);
    for (int i = 0;  i < nchans;  ++i)
        PyTuple_SetItem (result, i, PyFloat_FromDouble(pixel[i]));
    return object(handle<>(result));
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_interppixel_bicubic_overloads,
                                ImageBuf_interppixel_bicubic, 3, 4)


object
ImageBuf_interppixel_bicubic_NDC (const ImageBuf &buf, float x, float y,
                              ImageBuf::WrapMode wrap = ImageBuf::WrapBlack)
{
    int nchans = buf.nchannels();
    float *pixel = ALLOCA (float, nchans);
    buf.interppixel_bicubic_NDC (x, y, pixel, wrap);
    PyObject *result = PyTuple_New (nchans);
    for (int i = 0;  i < nchans;  ++i)
        PyTuple_SetItem (result, i, PyFloat_FromDouble(pixel[i]));
    return object(handle<>(result));
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_interppixel_bicubic_NDC_overloads,
                                ImageBuf_interppixel_bicubic_NDC, 3, 4)



void
ImageBuf_setpixel (ImageBuf &buf, int x, int y, int z, tuple p)
{
    std::vector<float> pixel;
    py_to_stdvector (pixel, p);
    if (pixel.size())
        buf.setpixel (x, y, z, &pixel[0], pixel.size());
}

void
ImageBuf_setpixel2 (ImageBuf &buf, int x, int y, tuple p)
{
    ImageBuf_setpixel (buf, x, y, 0, p);
}

void
ImageBuf_setpixel1 (ImageBuf &buf, int i, tuple p)
{
    std::vector<float> pixel;
    py_to_stdvector (pixel, p);
    if (pixel.size())
        buf.setpixel (i, &pixel[0], pixel.size());
}



object
ImageBuf_get_pixels (const ImageBuf &buf, TypeDesc format, ROI roi=ROI::All())
{
    // Allocate our own temp buffer and try to read the image into it.
    // If the read fails, return None.
    if (! roi.defined())
        roi = buf.roi();
    roi.chend = std::min (roi.chend, buf.nchannels());

    size_t size = (size_t) roi.npixels() * roi.nchannels() * format.size();
    std::unique_ptr<char[]> data (new char [size]);
    if (! buf.get_pixels (roi, format, &data[0])) {
        return object(handle<>(Py_None));
    }

    return C_array_to_Python_array (data.get(), format, size);
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_get_pixels_overloads,
                                ImageBuf_get_pixels, 2, 3)

object
ImageBuf_get_pixels_bt (const ImageBuf &buf, TypeDesc::BASETYPE format,
                        ROI roi=ROI::All())
{
    return ImageBuf_get_pixels (buf, TypeDesc(format), roi);
}

BOOST_PYTHON_FUNCTION_OVERLOADS(ImageBuf_get_pixels_bt_overloads,
                                ImageBuf_get_pixels_bt, 2, 3)



void
ImageBuf_set_deep_value (ImageBuf &buf, int x, int y, int z,
                         int c, int s, float value)
{
    buf.set_deep_value (x, y, z, c, s, value);
}

void
ImageBuf_set_deep_value_uint (ImageBuf &buf, int x, int y, int z,
                         int c, int s, uint32_t value)
{
    buf.set_deep_value (x, y, z, c, s, value);
}



bool
ImageBuf_set_pixels_tuple (ImageBuf &buf, ROI roi, const tuple& data)
{
    if (! roi.defined())
        roi = buf.roi();
    roi.chend = std::min (roi.chend, buf.nchannels());
    size_t size = (size_t) roi.npixels() * roi.nchannels();
    if (size == 0)
        return true;   // done
    std::vector<float> vals;
    py_to_stdvector (vals, data);
    if (size > vals.size())
        return false;   // Not enough data to fill our ROI
    buf.set_pixels (roi, TypeFloat, &vals[0]);
    return true;
}


bool
ImageBuf_set_pixels_array (ImageBuf &buf, ROI roi, const object& data)
{
    // If it's a tuple, we handle that with the other function
    extract<tuple> tup (data);
    if (tup.check())
        return ImageBuf_set_pixels_tuple (buf, roi, tup());

    if (! roi.defined())
        roi = buf.roi();
    roi.chend = std::min (roi.chend, buf.nchannels());
    size_t size = (size_t) roi.npixels() * roi.nchannels();
    if (size == 0)
        return true;   // done

    TypeDesc elementtype;
    size_t numelements;
    const void* addr = python_array_address (data, elementtype, numelements);
    if (!addr || size > numelements)
        return false;   // Not enough data to fill our ROI
    std::vector<float> vals (numelements);
    convert_types (elementtype, addr, TypeFloat, vals.data(),
                   int(numelements));
    buf.set_pixels (roi, TypeFloat, &vals[0]);
    return true;
}



DeepData&
ImageBuf_deepdataref (ImageBuf *ib)
{
    return *ib->deepdata();
}



static void
ImageBuf_deep_alloc_dummy ()
{
}



void declare_imagebuf()
{
    enum_<ImageBuf::WrapMode>("WrapMode")
        .value("WrapDefault",  ImageBuf::WrapDefault )
        .value("WrapBlack",    ImageBuf::WrapBlack )
        .value("WrapClamp",    ImageBuf::WrapClamp )
        .value("WrapPeriodic", ImageBuf::WrapPeriodic )
        .value("WrapMirror",   ImageBuf::WrapMirror )
        .export_values();

    class_<ImageBuf, boost::noncopyable> ("ImageBuf")
        .def(init<const std::string&>())
        .def(init<const std::string&, int, int>())
        .def(init<const ImageSpec&>())

        .def("clear", &ImageBuf::clear)
        .def("reset", &ImageBuf_reset_name,
             (arg("name")))
        .def("reset", &ImageBuf_reset_name2,
             (arg("name"), arg("subimage")=0, arg("miplevel")=0))
        .def("reset", &ImageBuf_reset_name_config,
             (arg("name"), arg("subimage")=0, arg("miplevel")=0,
              arg("config")=ImageSpec()))
        .def("reset", &ImageBuf_reset_spec)
        .add_property ("initialized", &ImageBuf::initialized)
        .def("init_spec", &ImageBuf::init_spec)
        .def("read",  &ImageBuf_read,
             ImageBuf_read_overloads())
        .def("read",  &ImageBuf_read2,
             ImageBuf_read2_overloads())
        .def("write", &ImageBuf_write,
             ImageBuf_write_overloads())
        // FIXME -- write(ImageOut&)
        .def("make_writeable", &ImageBuf_make_writeable,
             (arg("keep_cache_type")=false))
        .def("set_write_format", &ImageBuf_set_write_format)
        .def("set_write_tiles", &ImageBuf::set_write_tiles,
             (arg("width")=0, arg("height")=0, arg("depth")=0))

        .def("spec", &ImageBuf::spec,
                return_value_policy<copy_const_reference>())
        .def("nativespec", &ImageBuf::nativespec,
                return_value_policy<copy_const_reference>())
        .def("specmod", &ImageBuf::specmod,
             return_value_policy<reference_existing_object>())
        .add_property("name", &ImageBuf_name)
        .add_property("file_format_name", &ImageBuf_file_format_name)
        .add_property("subimage", &ImageBuf::subimage)
        .add_property("nsubimages", &ImageBuf::nsubimages)
        .add_property("miplevel", &ImageBuf::miplevel)
        .add_property("nmiplevels", &ImageBuf::nmiplevels)
        .add_property("nchannels", &ImageBuf::nchannels)
        .add_property("orientation", &ImageBuf::orientation,
                      &ImageBuf::set_orientation)
        .add_property("oriented_width", &ImageBuf::oriented_width)
        .add_property("oriented_height", &ImageBuf::oriented_height)
        .add_property("oriented_x", &ImageBuf::oriented_x)
        .add_property("oriented_y", &ImageBuf::oriented_y)
        .add_property("oriented_full_width", &ImageBuf::oriented_full_width)
        .add_property("oriented_full_height", &ImageBuf::oriented_full_height)
        .add_property("oriented_full_x", &ImageBuf::oriented_full_x)
        .add_property("oriented_full_y", &ImageBuf::oriented_full_y)
        .add_property("xbegin", &ImageBuf::xbegin)
        .add_property("xend", &ImageBuf::xend)
        .add_property("ybegin", &ImageBuf::ybegin)
        .add_property("yend", &ImageBuf::yend)
        .add_property("zbegin", &ImageBuf::zbegin)
        .add_property("zend", &ImageBuf::zend)
        .add_property("xmin", &ImageBuf::xmin)
        .add_property("xmax", &ImageBuf::xmax)
        .add_property("ymin", &ImageBuf::ymin)
        .add_property("ymax", &ImageBuf::ymax)
        .add_property("zmin", &ImageBuf::zmin)
        .add_property("zmax", &ImageBuf::zmax)
        .add_property("roi", &ImageBuf::roi)
        .add_property("roi_full",
                      &ImageBuf::roi_full, &ImageBuf::set_roi_full)
        .def("set_full", &ImageBuf_set_full)

        .add_property("pixels_valid", &ImageBuf::pixels_valid)
        .add_property("pixeltype", &ImageBuf::pixeltype)
        .add_property("has_error",   &ImageBuf::has_error)
        .def("geterror",    &ImageBuf::geterror)

        .def("pixelindex", &ImageBuf::pixelindex,
             (arg("x"), arg("y"), arg("z"), arg("check_range")=false))
        .def("copy_metadata", &ImageBuf::copy_metadata)
        .def("copy_pixels", &ImageBuf::copy_pixels)
        .def("copy",  &ImageBuf_copy,
             ImageBuf_copy_overloads())
        .def("copy",  &ImageBuf_copy2,
             ImageBuf_copy2_overloads())
        .def("swap", &ImageBuf::swap)

        .def("getchannel", &ImageBuf_getchannel,
             ImageBuf_getchannel_overloads())
        .def("getpixel", &ImageBuf_getpixel,
             ImageBuf_getpixel_overloads())
        .def("interppixel", &ImageBuf_interppixel,
             ImageBuf_interppixel_overloads())
        .def("interppixel_NDC", &ImageBuf_interppixel_NDC,
             ImageBuf_interppixel_NDC_overloads())
        .def("interppixel_NDC_full", &ImageBuf_interppixel_NDC,
             ImageBuf_interppixel_NDC_overloads())
        .def("interppixel_bicubic", &ImageBuf_interppixel_bicubic,
             ImageBuf_interppixel_bicubic_overloads())
        .def("interppixel_bicubic_NDC", &ImageBuf_interppixel_bicubic_NDC,
             ImageBuf_interppixel_bicubic_NDC_overloads())
        .def("setpixel", &ImageBuf_setpixel)
        .def("setpixel", &ImageBuf_setpixel2)
        .def("setpixel", &ImageBuf_setpixel1)
        .def("get_pixels", &ImageBuf_get_pixels, ImageBuf_get_pixels_overloads())
        .def("get_pixels", &ImageBuf_get_pixels_bt, ImageBuf_get_pixels_bt_overloads())
        .def("set_pixels", &ImageBuf_set_pixels_tuple)
        .def("set_pixels", &ImageBuf_set_pixels_array)

        .add_property("deep", &ImageBuf::deep)
        .def("deep_samples", &ImageBuf::deep_samples,
             (arg("x"), arg("y"), arg("z")=0))
        .def("set_deep_samples", &ImageBuf::set_deep_samples,
             (arg("x"), arg("y"), arg("z")=0, arg("nsamples")=1))
        .def("deep_insert_samples", &ImageBuf::deep_insert_samples,
             (arg("x"), arg("y"), arg("z")=0, arg("samplepos"), arg("nsamples")=1))
        .def("deep_erase_samples", &ImageBuf::deep_erase_samples,
             (arg("x"), arg("y"), arg("z")=0, arg("samplepos"), arg("nsamples")=1))
        .def("deep_value", &ImageBuf::deep_value,
             (arg("x"), arg("y"), arg("z")=0, arg("channel"), arg("sample")))
        .def("deep_value_uint", &ImageBuf::deep_value_uint,
             (arg("x"), arg("y"), arg("z")=0, arg("channel"), arg("sample")))
        .def("set_deep_value", &ImageBuf_set_deep_value,
             (arg("x"), arg("y"), arg("z")=0, arg("channel"),
              arg("sample"), arg("value")=0.0f))
        .def("set_deep_value_uint", &ImageBuf_set_deep_value_uint,
             (arg("x"), arg("y"), arg("z")=0, arg("channel"),
              arg("sample"), arg("value")=0))
        .def("deep_alloc", &ImageBuf_deep_alloc_dummy)  // DEPRECATED(1.7)
        .def("deepdata", &ImageBuf_deepdataref,
             return_value_policy<reference_existing_object>())

        // FIXME -- do we want to provide pixel iterators?
    ;

}

} // namespace PyOpenImageIO

