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

object ImageInputWrap::open_static_regular (const std::string &filename)
{
    ImageInputWrap* iiw = new ImageInputWrap;
    {
        ScopedGILRelease gil;
        iiw->m_input = ImageInput::open(filename);
    }
    if (iiw->m_input == NULL) {
        delete iiw;
        return object(handle<>(Py_None));
    } else {
        return object(iiw);
    }
}

object ImageInputWrap::open_static_with_config (const std::string &filename,
                                                const ImageSpec &config)
{
    ImageInputWrap* iiw = new ImageInputWrap;
    {
        ScopedGILRelease gil;
        iiw->m_input = ImageInput::open(filename, &config);
    }
    if (iiw->m_input == NULL) {
        delete iiw;
        return object(handle<>(Py_None));
    } else {
        return object(iiw);
    }
}

object ImageInputWrap::create(const std::string &filename, 
                              const std::string &plugin_searchpath) 
{
    ImageInputWrap* iiw = new ImageInputWrap;
    {
        ScopedGILRelease gil;
        iiw->m_input = ImageInput::create(filename, plugin_searchpath);
    }
    if (iiw->m_input == NULL) {
        delete iiw;
        return object(handle<>(Py_None));
    }
    else {
         return object(iiw);
    }
}

ImageInputWrap::~ImageInputWrap()
{
    delete m_input;
}

const char* ImageInputWrap::format_name() const {
    return m_input->format_name();
}

bool ImageInputWrap::valid_file (const std::string &filename) const
{
    ScopedGILRelease gil;
    return m_input->valid_file (filename);
}

bool ImageInputWrap::open_regular (const std::string &name)
{
    ScopedGILRelease gil;
    ImageSpec newspec;
    return m_input->open(name, newspec);
}

bool ImageInputWrap::open_with_config (const std::string &name, 
                                       const ImageSpec &config)
{
    ScopedGILRelease gil;
    ImageSpec newspec;
    return m_input->open(name, newspec, config);
}

const ImageSpec& ImageInputWrap::spec() const
{
    return m_input->spec();
}

int ImageInputWrap::supports (const std::string &feature) const
{
    return m_input->supports (feature);
}

bool ImageInputWrap::close()
{
    return m_input->close();
}

int ImageInputWrap::current_subimage() const
{
    return m_input->current_subimage();
}

int ImageInputWrap::current_miplevel() const
{
    return m_input->current_miplevel();
}

bool ImageInputWrap::seek_subimage(int subimage, int miplevel)
{
    ScopedGILRelease gil;
    ImageSpec dummyspec;
    return m_input->seek_subimage (subimage, miplevel, dummyspec);
}



// The read_image method is a bit different from the c++ interface. 
// "function" is a function which takes a float, and the 
// PyProgressCallback function is called automatically.
object
ImageInputWrap::read_image (TypeDesc format)
{
    // Allocate our own temp buffer and try to read the image into it.
    // If the read fails, return None.
    const ImageSpec &spec = m_input->spec();
    if (format.basetype == TypeDesc::UNKNOWN)
        format = spec.format;
    size_t size = (size_t) spec.image_pixels() * spec.nchannels * format.size();
    char *data = new char[size];
    bool ok;
    {
        ScopedGILRelease gil;
        ok = m_input->read_image(format, data);
    }
    if (! ok) {
        delete [] data;  // never mind
        return object(handle<>(Py_None));
    }

    object array = C_array_to_Python_array (data, format, size);

    // clean up and return the array handle
    delete [] data;
    return array;
}


object
ImageInputWrap_read_image_bt (ImageInputWrap& in, TypeDesc::BASETYPE format)
{
    return in.read_image (format);
}


object
ImageInputWrap_read_image_default (ImageInputWrap& in)
{
    return in.read_image (TypeDesc::UNKNOWN);
}



object
ImageInputWrap::read_scanline (int y, int z, TypeDesc format)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = m_input->spec();
    if (format.basetype == TypeDesc::UNKNOWN)
        format = spec.format;
    size_t size = (size_t) spec.width * spec.nchannels * format.size();
    char *data = new char[size];
    bool ok;
    {
        ScopedGILRelease gil;
        ok = m_input->read_scanline (y, z, format, data);
    }
    if (!ok) {
        delete [] data;  // never mind
        return object(handle<>(Py_None));
    }

    object array = C_array_to_Python_array (data, format, size);

    // clean up and return the array handle
    delete [] data;
    return array;
}


object
ImageInputWrap_read_scanline_bt (ImageInputWrap& in, int y, int z,
                                 TypeDesc::BASETYPE format)
{
    return in.read_scanline (y, z, format);
}


object
ImageInputWrap_read_scanline_default (ImageInputWrap& in, int y, int z)
{
    return in.read_scanline (y, z, TypeDesc::UNKNOWN);
}



object
ImageInputWrap::read_scanlines (int ybegin, int yend, int z,
                                int chbegin, int chend, TypeDesc format)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    ASSERT (m_input);
    const ImageSpec &spec = m_input->spec();
    if (format.basetype == TypeDesc::UNKNOWN)
        format = spec.format;
    chend = clamp (chend, chbegin+1, spec.nchannels);
    int nchans = chend - chbegin;
    size_t size = (size_t) spec.width * (yend-ybegin) * nchans * format.size();
    char *data = new char[size];
    bool ok;
    {
        ScopedGILRelease gil;
        ok = m_input->read_scanlines (ybegin, yend, z, chbegin, chend, format, data);
    }
    if (! ok) {
        delete [] data;  // never mind
        return object(handle<>(Py_None));
    }

    object array = C_array_to_Python_array (data, format, size);

    // clean up and return the array handle
    delete [] data;
    return array;
}


object
ImageInputWrap_read_scanlines_bt (ImageInputWrap& in, int ybegin, int yend,
                                  int z, int chbegin, int chend,
                                  TypeDesc::BASETYPE format)
{
    return in.read_scanlines (ybegin, yend, z, chbegin, chend, format);
}


object
ImageInputWrap_read_scanlines_default (ImageInputWrap& in, int ybegin, int yend,
                                       int z, int chbegin, int chend)
{
    return in.read_scanlines (ybegin, yend, z, chbegin, chend, TypeDesc::UNKNOWN);
}



object
ImageInputWrap::read_tile (int x, int y, int z, TypeDesc format)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = m_input->spec();
    if (format.basetype == TypeDesc::UNKNOWN)
        format = spec.format;
    size_t size = (size_t) spec.tile_pixels() * spec.nchannels * format.size();
    char *data = new char[size];
    bool ok;
    {
        ScopedGILRelease gil;
        ok = m_input->read_tile (x, y, z, format, data);
    }
    if (! ok) {
        delete [] data;  // never mind
        return object(handle<>(Py_None));
    }

    object array = C_array_to_Python_array (data, format, size);

    // clean up and return the array handle
    delete [] data;
    return array;
}


object
ImageInputWrap_read_tile_bt (ImageInputWrap& in,
                             int x, int y, int z, TypeDesc::BASETYPE format)
{
    return in.read_tile (x, y, z, format);
}


object
ImageInputWrap_read_tile_default (ImageInputWrap& in,
                                  int x, int y, int z)
{
    return in.read_tile (x, y, z, TypeDesc::UNKNOWN);
}



object
ImageInputWrap::read_tiles (int xbegin, int xend, int ybegin, int yend,
                            int zbegin, int zend, int chbegin, int chend,
                            TypeDesc format)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = m_input->spec();
    if (format.basetype == TypeDesc::UNKNOWN)
        format = spec.format;
    chend = clamp (chend, chbegin+1, spec.nchannels);
    int nchans = chend - chbegin;
    size_t size = (size_t) ((xend-xbegin) * (yend-ybegin) * 
                            (zend-zbegin) * nchans * format.size());
    char *data = new char[size];
    bool ok;
    {
        ScopedGILRelease gil;
        ok = m_input->read_tiles (xbegin, xend, ybegin, yend,
                                  zbegin, zend, chbegin, chend, format, data);
    }
    if (! ok) {
        delete [] data;  // never mind
        return object(handle<>(Py_None));
    }

    object array = C_array_to_Python_array (data, format, size);

    // clean up and return the array handle
    delete [] data;
    return array;
}


object
ImageInputWrap_read_tiles_bt (ImageInputWrap& in,
                              int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, int chbegin, int chend,
                              TypeDesc::BASETYPE format)
{
    return in.read_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                          chbegin, chend, format);
}


object
ImageInputWrap_read_tiles_default (ImageInputWrap& in,
                                   int xbegin, int xend, int ybegin, int yend,
                                   int zbegin, int zend, int chbegin, int chend)
{
    return in.read_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                          chbegin, chend, TypeDesc::UNKNOWN);
}




object
ImageInputWrap::read_native_deep_scanlines (int ybegin, int yend, int z,
                                            int chbegin, int chend)
{
    DeepData* dd = NULL;
    bool ok = true;
    {
        ScopedGILRelease gil;
        dd = new DeepData;
        ok = m_input->read_native_deep_scanlines (ybegin, yend, z,
                                                  chbegin, chend, *dd);
    }
    if (ok)
        return object(dd);
    delete dd;
    return object(handle<>(Py_None));
}


object
ImageInputWrap::read_native_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                        int zbegin, int zend, int chbegin, int chend)
{
    DeepData* dd = NULL;
    bool ok = true;
    {
        ScopedGILRelease gil;
        dd = new DeepData;
        ok = m_input->read_native_deep_tiles (xbegin, xend, ybegin, yend,
                                              zbegin, zend, chbegin, chend, *dd);
    }
    if (ok)
        return object(dd);
    delete dd;
    return object(handle<>(Py_None));
}


object
ImageInputWrap::read_native_deep_image ()
{
    DeepData* dd = NULL;
    bool ok = true;
    {
        ScopedGILRelease gil;
        dd = new DeepData;
        ok = m_input->read_native_deep_image (*dd);
    }
    if (ok)
        return object(dd);
    delete dd;
    return object(handle<>(Py_None));
}



std::string
ImageInputWrap::geterror() const
{
    return m_input->geterror();
}



void declare_imageinput()
{
    class_<ImageInputWrap>("ImageInput", no_init)
        .def("create", &ImageInputWrap::create,
             (arg("filename"), arg("plugin_searchpath")=""))
        .staticmethod("create")
        .def("open", &ImageInputWrap::open_static_regular,
             (arg("filename")))
        .def("open", &ImageInputWrap::open_static_with_config,
             (arg("filename")))
        .staticmethod("open")
        .def("format_name",      &ImageInputWrap::format_name)
        .def("valid_file",       &ImageInputWrap::valid_file)
        // .def("open",             &ImageInputWrap::open_regular)
        // .def("open",             &ImageInputWrap::open_with_config)
        .def("spec",             &ImageInputWrap::spec, 
             return_value_policy<copy_const_reference>())
        .def("supports",         &ImageInputWrap::supports)
        .def("close",            &ImageInputWrap::close)
        .def("current_subimage", &ImageInputWrap::current_subimage)
        .def("current_miplevel", &ImageInputWrap::current_miplevel)
        .def("seek_subimage",    &ImageInputWrap::seek_subimage)
        .def("read_scanline",    &ImageInputWrap::read_scanline)
        .def("read_scanline",    &ImageInputWrap_read_scanline_bt)
        .def("read_scanline",    &ImageInputWrap_read_scanline_default)
        .def("read_scanlines",   &ImageInputWrap::read_scanlines)
        .def("read_scanlines",   &ImageInputWrap_read_scanlines_bt)
        .def("read_scanlines",   &ImageInputWrap_read_scanlines_default)
        .def("read_tile",        &ImageInputWrap::read_tile)
        .def("read_tile",        &ImageInputWrap_read_tile_bt)
        .def("read_tile",        &ImageInputWrap_read_tile_default)
        .def("read_tiles",       &ImageInputWrap::read_tiles)
        .def("read_tiles",       &ImageInputWrap_read_tiles_bt)
        .def("read_tiles",       &ImageInputWrap_read_tiles_default)
        .def("read_image",       &ImageInputWrap::read_image)
        .def("read_image",       &ImageInputWrap_read_image_bt)
        .def("read_image",       &ImageInputWrap_read_image_default)
        .def("read_native_deep_scanlines", &ImageInputWrap::read_native_deep_scanlines)
        .def("read_native_deep_tiles",     &ImageInputWrap::read_native_deep_tiles)
        .def("read_native_deep_image",     &ImageInputWrap::read_native_deep_image)
        .def("geterror",         &ImageInputWrap::geterror)
    ;
}

} // namespace PyOpenImageIO

