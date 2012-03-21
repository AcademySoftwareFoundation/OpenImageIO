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
    iiw->m_input = ImageInput::open(filename);
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
    iiw->m_input = ImageInput::open(filename, &config);
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
    iiw->m_input = ImageInput::create(filename, plugin_searchpath);
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

const char* ImageInputWrap::format_name()const {
    return m_input->format_name();
}

bool ImageInputWrap::open_regular (const std::string &name, 
                                ImageSpec &newspec)
{
    return m_input->open(name, newspec);
}

bool ImageInputWrap::open_with_config (const std::string &name, 
                        ImageSpec &newspec, const ImageSpec &config)
{
    return m_input->open(name, newspec, config);
}

const ImageSpec&  ImageInputWrap::spec() const
{
    return m_input->spec();
}

bool ImageInputWrap::close()
{
    return m_input->close();
}

int ImageInputWrap::current_subimage() const
{
    return m_input->current_subimage();
}

bool ImageInputWrap::seek_subimage(int subimage, int miplevel, ImageSpec &newspec)
{
    return m_input->seek_subimage (subimage, miplevel, newspec);
}

/*      **deprecated**
// This is a bit different from the C++ interface.
// The function is called with no arguments, and returns a 
// python array. 
object ImageInputWrap::read_image_simple () {
    ImageSpec spec = m_input->spec();
    //std::cout << "c_str: " << spec.format.c_str() << std::endl;
    int size = spec.image_bytes();
    float *data = new float[size];
    // FIXME: should raise an exception instead of returning Py_None
    if (!m_input->read_image(data)) return object(handle<>(Py_None));

    object arr_module(handle<>(PyImport_ImportModule("array")));
   
    object array = arr_module.attr("array")("f");
    object string_py(
        handle<>(
            PyString_FromStringAndSize(
                    reinterpret_cast<const char*>(data),
                    sizeof(float) * size)));

    array.attr("fromstring")(string_py);
    return array;
}

// Again, different from the C++ interface. User doesn't pass
// an array - it is generated here and returned to Python.
// other arguments not yet exposed to Python.
object ImageInputWrap::read_image (TypeDesc format)  {
    ImageSpec spec = m_input->spec();
    int size = spec.image_bytes();
    char *data = new char[size];
    //FIXME: expose arguments to python (as default arguments)
    if (!m_input->read_image(format, data, AutoStride, AutoStride, 
        AutoStride, NULL, NULL )) return object(handle<>(Py_None));

    object arr_module(handle<>(PyImport_ImportModule("array")));
    object array = arr_module.attr("array")("B");
    object string_py(
        handle<>(
            PyString_FromStringAndSize(
                    reinterpret_cast<const char*>(data),
                    sizeof(char) * size)));

    array.attr("fromstring")(string_py);
    return array;
}
*/

// This function prepares a buffer for writing to it from c++
void* ImageInputWrap::make_write_buffer (object &buffer, Py_ssize_t expected_len)
{
    void *array;
    Py_ssize_t len;
    int success;
    success = PyObject_AsWriteBuffer(buffer.ptr(), &array, &len);
    if (success != 0) throw_error_already_set();

    if (len < expected_len) {
        PyErr_SetString(PyExc_IndexError, "Buffer size is smaller than data size");
        throw_error_already_set();
    }
    return array;
}

// The read_image method is a bit different from the c++ interface. 
// "function" is a function which takes a float, and the 
// PyProgressCallback function is called automatically.
bool ImageInputWrap::read_image(TypeDesc format, object &buffer,
                                    stride_t xstride=AutoStride, 
                                    stride_t ystride=AutoStride, 
                                    stride_t zstride=AutoStride,
                                    object function=object(handle<>(Py_None)))
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().image_pixels() * m_input->spec().nchannels * format.basesize());
    if (function==handle<>(Py_None)) {
        return m_input->read_image(format, write_buf, xstride, ystride, zstride,
                                    NULL, NULL);
    }
    else {
        return m_input->read_image(format, write_buf, xstride, ystride, zstride,
                                    &PyProgressCallback, &function);
    }
}

bool ImageInputWrap::read_scanline(int y, int z, TypeDesc format, object &buffer,
                stride_t xstride=AutoStride)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().width * m_input->spec().nchannels * format.basesize());
    return m_input->read_scanline(y, z, format, write_buf, xstride);
}

// TESTME
bool ImageInputWrap::read_scanline_simple(int y, int z, object &buffer)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().width * m_input->spec().nchannels * sizeof(float));
    return m_input->read_scanline(y, z, reinterpret_cast<float*>(write_buf));    
}

// TESTME
bool ImageInputWrap::read_tile(int x, int y, int z, TypeDesc format, 
                                object &buffer, stride_t xstride=AutoStride, 
                                stride_t ystride=AutoStride,
                                stride_t zstride=AutoStride)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().tile_pixels() * m_input->spec().nchannels * format.basesize());
    return m_input->read_tile(x, y, z, format, write_buf, 
                                xstride, ystride, zstride);
}

// TESTME
bool ImageInputWrap::read_tile_simple(int x, int y, int z, object &buffer)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().tile_pixels() * m_input->spec().nchannels * sizeof(float));
    return m_input->read_tile(x, y, z, reinterpret_cast<float*>(write_buf));
}

// TESTME
bool ImageInputWrap::read_native_scanline(int y, int z, object &buffer)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().scanline_bytes());
    return m_input->read_native_scanline(y, z, write_buf);
}

// TESTME
bool ImageInputWrap::read_native_tile(int x, int y, int z, object &buffer)
{
    void *write_buf = make_write_buffer(buffer, m_input->spec().tile_bytes());
    return m_input->read_native_tile(x, y, z, write_buf);
}

std::string ImageInputWrap::geterror()const  {
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
        .staticmethod("open")
        .def("format_name",      &ImageInputWrap::format_name)
//        .def("open",             &ImageInputWrap::open_regular)
//        .def("open",             &ImageInputWrap::open_with_config)
        .def("spec",             &ImageInputWrap::spec, 
             return_value_policy<copy_const_reference>())
        .def("close",            &ImageInputWrap::close)
        .def("current_subimage", &ImageInputWrap::current_subimage)
        .def("seek_subimage",    &ImageInputWrap::seek_subimage)
        .def("read_image",       &ImageInputWrap::read_image,
             (arg("xstride")=AutoStride, arg("ystride")=AutoStride,
              arg("zstride")=AutoStride, arg("function")=object(handle<>(Py_None))))
        .def("read_scanline",    &ImageInputWrap::read_scanline,
             arg("xstride")=AutoStride)
        .def("read_scanline",    &ImageInputWrap::read_scanline_simple)
        .def("read_tile",        &ImageInputWrap::read_tile)
        .def("read_tile",        &ImageInputWrap::read_tile_simple)
        .def("read_native_scanline", &ImageInputWrap::read_native_scanline)
        .def("read_native_tile", &ImageInputWrap::read_native_tile)
        .def("geterror",         &ImageInputWrap::geterror)
    ;
}

} // namespace PyOpenImageIO

