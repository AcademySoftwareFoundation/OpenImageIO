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
#include <boost/python/enum.hpp>
#include "imageio.h"
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

bool ImageOutputWrap::close()
{
    return m_output->close();
}
    
// this function creates a read buffer from PyObject which will be used
// for all write_<something> functions.
const void* ImageOutputWrap::make_read_buffer(object &buffer)
{
    const void *buf; 
    Py_ssize_t len;
    int success = PyObject_AsReadBuffer(buffer.ptr(), &buf, &len);
    if (success != 0) throw_error_already_set();
    return buf;
}

// TESTME
bool ImageOutputWrap::write_scanline(int y, int z, TypeDesc format, object &buffer,
                                        stride_t xstride=AutoStride)
{
    const void *array = make_read_buffer(buffer);
    return m_output->write_scanline(y, z, format, array, xstride);
}

// TESTME
bool ImageOutputWrap::write_tile(int x, int y, int z, TypeDesc format, object &buffer,
                                stride_t xstride=AutoStride,
                                stride_t ystride=AutoStride,
                                stride_t zstride=AutoStride)
{
    const void *array = make_read_buffer(buffer);
    return m_output->write_tile(x, y, z, format, array, xstride, ystride, zstride);    
}

// TESTME
bool ImageOutputWrap::write_rectangle(int xbegin, int xend, int ybegin, int yend,
                                     int zbegin, int zend, TypeDesc format, 
                                    object &buffer, stride_t xstride=AutoStride,
                                    stride_t ystride=AutoStride,
                                    stride_t zstride=AutoStride)
{
    const void *array = make_read_buffer(buffer);
    return m_output->write_rectangle(xbegin, xend, ybegin, yend, zbegin, zend,
                                     format, array, xstride, ystride, zstride);
}

// The write_image method is a bit different from the c++ interface. 
// "function" is a function which takes a float, and the 
// PyProgressCallback function is called automatically.
bool ImageOutputWrap::write_image (TypeDesc format, object &buffer,
                                    stride_t xstride=AutoStride,
                                    stride_t ystride=AutoStride,
                                    stride_t zstride=AutoStride,
                                object function=object(handle<>(Py_None)))
{

    const void *array = make_read_buffer(buffer);
    if (function==handle<>(Py_None)) {
        return m_output->write_image(format, array, xstride, ystride, 
                                zstride, NULL, NULL);
    }
    else {
        return m_output->write_image(format, array, xstride, ystride, zstride, 
                            &PyProgressCallback, &function);
    }
    
}

//for testing if m_output looks ok
void ImageOutputWrap::print_pointer()
{
    std::cout << m_output << std::endl;
}

bool ImageOutputWrap::copy_image (ImageInputWrap *iiw)
{
    return m_output->copy_image(iiw->m_input);
}

const char* ImageOutputWrap::format_name (void) const
{
    return m_output->format_name();
}

bool ImageOutputWrap::supports (const std::string &feature) const
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
        .def("close",           &ImageOutputWrap::close)
        .def("write_tile",      &ImageOutputWrap::write_tile,
             (arg("xstride")=AutoStride, arg("ystride")=AutoStride,
              arg("zstride")=AutoStride))
        .def("write_scanline",  &ImageOutputWrap::write_scanline,
              arg("xstride")=AutoStride)
        .def("write_rectangle", &ImageOutputWrap::write_rectangle, 
             (arg("xstride")=AutoStride, arg("ystride")=AutoStride,
              arg("zstride")=AutoStride))
        .def("write_image",     &ImageOutputWrap::write_image, 
             (arg("xstride")=AutoStride, arg("ystride")=AutoStride,
              arg("zstride")=AutoStride, arg("function")=object(handle<>(Py_None))))
        .def("print_pointer",   &ImageOutputWrap::print_pointer)//for testing
        .def("copy_image",      &ImageOutputWrap::copy_image)
        .def("geterror",        &ImageOutputWrap::geterror)
    ;
    enum_<ImageOutput::OpenMode>("ImageOutputOpenMode")
                .value("Create", ImageOutput::Create )
		.value("AppendSubimage", ImageOutput::AppendSubimage)
                .value("AppendMIPLevel", ImageOutput::AppendMIPLevel)
		.export_values();
    scope().attr("AutoStride") = AutoStride;
}

} // namespace PyOpenImageIO

