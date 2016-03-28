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

// Avoid a compiler warning from a duplication in tiffconf.h/pyconfig.h
#undef SIZEOF_LONG
#include <boost/python.hpp>
#include "py_oiio.h"

namespace PyOpenImageIO
{

using namespace boost::python;

object ImageOutputWrap::create (const std::string &filename, 
                                const std::string& plugin_searchpath="")
{
    ImageOutputWrap *iow = new ImageOutputWrap;
    iow->m_output = ImageOutput::create(filename, plugin_searchpath);
    if (iow->m_output == NULL) {
        delete iow;
        return object(handle<>(Py_None));
    }
    else {
        return object(iow);
    }
}


ImageOutputWrap::~ImageOutputWrap()
{
    delete m_output;
}


const ImageSpec& ImageOutputWrap::spec () const
{
    return m_output->spec();
}


bool ImageOutputWrap::open (const std::string &name, const ImageSpec &newspec,
                            ImageOutput::OpenMode mode=ImageOutput::Create)
{
    return m_output->open(name, newspec, mode);
}


bool ImageOutputWrap::open_specs (const std::string &name, tuple &specs)
{
    const size_t length = len(specs);
    if (length == 0)
        return false;
    std::vector<ImageSpec> Cspecs (length);
    for (size_t i = 0; i < length; ++i) {
        extract<ImageSpec> s (specs[i]);
        if (! s.check()) {
            // Tuple item was not an ImageSpec
            return false;
        }
        Cspecs[i] = s();
    }
    return m_output->open (name, int(length), &Cspecs[0]);
}



bool ImageOutputWrap::close()
{
    return m_output->close();
}
    
// this function creates a read buffer from PyObject which will be used
// for all write_<something> functions.
const void *
ImageOutputWrap::make_read_buffer (object &buffer, imagesize_t size)
{
    const void *buf = NULL; 
    Py_ssize_t len = 0;
    int success = PyObject_AsReadBuffer(buffer.ptr(), &buf, &len);
    if (success != 0 || imagesize_t(len) < size) {
        throw_error_already_set();
    }
    return buf;
}


bool
ImageOutputWrap::write_scanline_array (int y, int z, numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (static_cast<int>(numelements) < spec().width*spec().nchannels) {
        m_output->error ("write_scanline was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_scanline (y, z, format, array);
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_scanline (int y, int z, TypeDesc format, object &buffer,
                                 stride_t xstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t size = native ? m_output->spec().scanline_bytes (native)
                                  : format.size() * m_output->spec().nchannels * m_output->spec().width;
    const void *array = make_read_buffer (buffer, size);
    ScopedGILRelease gil;
    return m_output->write_scanline(y, z, format, array, xstride);
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_scanline_bt (int y, int z, TypeDesc::BASETYPE format,
                                    object &buffer, stride_t xstride)
{
    return write_scanline (y, z, format, buffer, xstride);
}


bool
ImageOutputWrap::write_scanlines_array (int ybegin, int yend, int z,
                                        numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (static_cast<int>(numelements) < spec().width*spec().nchannels*(yend-ybegin)) {
        m_output->error ("write_scanlines was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_scanlines (ybegin, yend, z, format, array);
}


bool
ImageOutputWrap::write_scanlines_random_array (int ybegin, int yend, int z,
                                               int subimage, int miplevel,
                                               numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (static_cast<int>(numelements) < spec().width*spec().nchannels*(yend-ybegin)) {
        m_output->error ("write_scanlines was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_scanlines (ybegin, yend, z, subimage, miplevel,
                                      format, array);
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_scanlines (int ybegin, int yend, int z,
                                  TypeDesc format, object &buffer,
                                  stride_t xstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t size = native ? m_output->spec().scanline_bytes (native)
                                  : format.size() * m_output->spec().nchannels * m_output->spec().width;
    const void *array = make_read_buffer (buffer, size);
    ScopedGILRelease gil;
    return m_output->write_scanlines(ybegin, yend, z, format, array, xstride);
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_scanlines_bt (int ybegin, int yend, int z,
                                     TypeDesc::BASETYPE format,
                                     object &buffer, stride_t xstride)
{
    return write_scanlines (ybegin, yend, z, format, buffer, xstride);
}



bool
ImageOutputWrap::write_tile_array (int x, int y, int z,
                                   numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (numelements < spec().tile_pixels()*spec().nchannels) {
        m_output->error ("write_tile was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_tile (x, y, z, format, array);
}

// DEPRECATED (1.6)
bool
ImageOutputWrap::write_tile (int x, int y, int z, TypeDesc format,
                             object &buffer, stride_t xstride,
                             stride_t ystride, stride_t zstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t size = native ? m_output->spec().tile_bytes (native)
                                  : format.size() * m_output->spec().nchannels * m_output->spec().tile_pixels();
    const void *array = make_read_buffer(buffer, size);
    ScopedGILRelease gil;
    return m_output->write_tile(x, y, z, format, array, xstride, ystride, zstride);    
}

// DEPRECATED (1.6)
bool
ImageOutputWrap::write_tile_bt (int x, int y, int z, TypeDesc::BASETYPE format,
                                object &buffer, stride_t xstride,
                                stride_t ystride, stride_t zstride)
{
    return write_tile(x, y, z, format, buffer, xstride, ystride, zstride);    
}



bool
ImageOutputWrap::write_tiles_array (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend,
                                    numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (static_cast<int>(numelements) < (xend-xbegin)*(yend-ybegin)*(zend-zbegin)*spec().nchannels) {
        m_output->error ("write_tiles was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                  format, array);
}


bool
ImageOutputWrap::write_tiles_random_array (int xbegin, int xend, int ybegin, int yend,
                                    int zbegin, int zend, int subimage, int miplevel,
                                    numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (static_cast<int>(numelements) < (xend-xbegin)*(yend-ybegin)*(zend-zbegin)*spec().nchannels) {
        m_output->error ("write_tiles was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                  subimage, miplevel, format, array);
}

// DEPRECATED (1.6)
bool
ImageOutputWrap::write_tiles (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              object &buffer, stride_t xstride,
                              stride_t ystride, stride_t zstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t size = native ? m_output->spec().tile_bytes (native)
                                  : format.size() * m_output->spec().nchannels * m_output->spec().tile_pixels();
    const void *array = make_read_buffer(buffer, size);
    ScopedGILRelease gil;
    return m_output->write_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                                  format, array, xstride, ystride, zstride);    
}

// DEPRECATED (1.6)
bool
ImageOutputWrap::write_tiles_bt (int xbegin, int xend, int ybegin, int yend,
                                 int zbegin, int zend, TypeDesc::BASETYPE format,
                                 object &buffer, stride_t xstride,
                                 stride_t ystride, stride_t zstride)
{
    return write_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                        format, buffer, xstride, ystride, zstride);    
}



bool
ImageOutputWrap::write_image_array (numeric::array &buffer)
{
    TypeDesc format;
    size_t numelements = 0;
    const void *array = python_array_address (buffer, format, numelements);
    if (numelements < spec().image_pixels()*spec().nchannels) {
        m_output->error ("write_image was not passed a long enough array");
        return false;
    }
    if (! array) {
        return false;
    }
    ScopedGILRelease gil;
    return m_output->write_image (format, array);
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_image (TypeDesc format, object &buffer,
                              stride_t xstride, stride_t ystride,
                              stride_t zstride)
{
    bool native = (format == TypeDesc::UNKNOWN);
    imagesize_t size = native ? m_output->spec().image_bytes (native)
                                  : format.size() * m_output->spec().nchannels * m_output->spec().image_pixels();
    const void *array = make_read_buffer (buffer, size);
    ScopedGILRelease gil;
    if (array)
        return m_output->write_image (format, array, xstride, ystride, zstride);
    return false;
}


// DEPRECATED (1.6)
bool
ImageOutputWrap::write_image_bt (TypeDesc::BASETYPE format, object &data,
                                 stride_t xstride, stride_t ystride,
                                 stride_t zstride)
{
    return write_image (format, data, xstride, ystride, zstride);
}



BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_image_overloads,
                                       write_image, 2, 5)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_image_bt_overloads,
                                       write_image_bt, 2, 5)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_scanline_overloads,
                                       write_scanline, 4, 5)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_scanline_bt_overloads,
                                       write_scanline_bt, 4, 5)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_scanlines_overloads,
                                       write_scanlines, 5, 6)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_scanlines_bt_overloads,
                                       write_scanlines_bt, 5, 6)

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_tile_overloads,
                                       write_tile, 5, 8)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_tile_bt_overloads,
                                       write_tile_bt, 5, 8)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_tiles_overloads,
                                       write_tiles, 8, 11)
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ImageOutputWrap_write_tiles_bt_overloads,
                                       write_tiles_bt, 8, 11)


bool
ImageOutputWrap::write_deep_scanlines (int ybegin, int yend, int z,
                                       const DeepData &deepdata)
{
    ScopedGILRelease gil;
    return m_output->write_deep_scanlines (ybegin, yend, z, deepdata);
}


bool
ImageOutputWrap::write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                   int zbegin, int zend, const DeepData &deepdata)
{
    ScopedGILRelease gil;
    return m_output->write_deep_tiles (xbegin, xend, ybegin, yend,
                                       zbegin, zend, deepdata);
}


bool
ImageOutputWrap::write_deep_image (const DeepData &deepdata)
{
    ScopedGILRelease gil;
    return m_output->write_deep_image (deepdata);
}



bool ImageOutputWrap::copy_image (ImageInputWrap *iiw)
{
    return m_output->copy_image(iiw->m_input);
}


const char* ImageOutputWrap::format_name (void) const
{
    return m_output->format_name();
}


int ImageOutputWrap::supports (const std::string &feature) const
{
    return m_output->supports(feature);
}


std::string ImageOutputWrap::geterror()const  {
    return m_output->geterror();
}



void declare_imageoutput()
{
    class_<ImageOutputWrap>("ImageOutput", no_init)
        .def("create",          &ImageOutputWrap::create,
             (arg("filename"), arg("plugin_searchpath")=""))
        .staticmethod("create")
        .def("format_name",     &ImageOutputWrap::format_name)
        .def("supports",        &ImageOutputWrap::supports)
        .def("spec",            &ImageOutputWrap::spec, 
              return_value_policy<copy_const_reference>())
        .def("open",            &ImageOutputWrap::open)
        .def("open",            &ImageOutputWrap::open_specs)
        .def("close",           &ImageOutputWrap::close)
        .def("write_image",     &ImageOutputWrap::write_image,
             ImageOutputWrap_write_image_overloads())
        .def("write_image",     &ImageOutputWrap::write_image_bt,
             ImageOutputWrap_write_image_bt_overloads())
        .def("write_image",     &ImageOutputWrap::write_image_array)
        .def("write_scanline",  &ImageOutputWrap::write_scanline,
             ImageOutputWrap_write_scanline_overloads())
        .def("write_scanline",  &ImageOutputWrap::write_scanline_bt,
             ImageOutputWrap_write_scanline_bt_overloads())
        .def("write_scanline",  &ImageOutputWrap::write_scanline_array)
        .def("write_scanlines",  &ImageOutputWrap::write_scanlines,
             ImageOutputWrap_write_scanlines_overloads())
        .def("write_scanlines",  &ImageOutputWrap::write_scanlines_bt,
             ImageOutputWrap_write_scanlines_bt_overloads())
        .def("write_scanlines",  &ImageOutputWrap::write_scanlines_array)
        .def("write_scanlines",  &ImageOutputWrap::write_scanlines_random_array,
             (arg("ybegin"), arg("yend"), arg("z"),
              arg("subimage"), arg("miplevel"), arg("data")))
        .def("write_tile",      &ImageOutputWrap::write_tile,
             ImageOutputWrap_write_tile_overloads())
        .def("write_tile",      &ImageOutputWrap::write_tile_bt,
             ImageOutputWrap_write_tile_bt_overloads())
        .def("write_tile",       &ImageOutputWrap::write_tile_array)
        .def("write_tiles",      &ImageOutputWrap::write_tiles,
             ImageOutputWrap_write_tiles_overloads())
        .def("write_tiles",      &ImageOutputWrap::write_tiles_bt,
             ImageOutputWrap_write_tiles_bt_overloads())
        .def("write_tiles",      &ImageOutputWrap::write_tiles_array)
        .def("write_tiles",      &ImageOutputWrap::write_tiles_random_array,
             (arg("xbegin"), arg("xend"), arg("ybegin"), arg("yend"),
              arg("zbegin"), arg("zend"), arg("subimage"), arg("miplevel"),
              arg("data")))
        .def("write_deep_scanlines", &ImageOutputWrap::write_deep_scanlines)
        .def("write_deep_tiles", &ImageOutputWrap::write_deep_tiles)
        .def("write_deep_image", &ImageOutputWrap::write_deep_image)
// FIXME - write_deep_{image,scanlines,tiles}
        .def("copy_image",      &ImageOutputWrap::copy_image)
        .def("geterror",        &ImageOutputWrap::geterror)
    ;
    enum_<ImageOutput::OpenMode>("ImageOutputOpenMode")
        .value("Create", ImageOutput::Create )
		.value("AppendSubimage", ImageOutput::AppendSubimage)
        .value("AppendMIPLevel", ImageOutput::AppendMIPLevel)
		.export_values();
}

} // namespace PyOpenImageIO

