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

// constructors
ImageBufWrap::ImageBufWrap (const std::string &name, 
                            const ImageSpec &spec)
{
    m_buf = new ImageBuf(name, spec);
}

ImageBufWrap::ImageBufWrap (const std::string &name,
                            ImageCacheWrap *icw) {
    // TODO: this isn't done properly. It should not take NULL as 
    // 2nd argument, the proper way would be icw->Cache
    // It's like this just for the initial tests.
    m_buf = new ImageBuf(name, NULL);
}

void ImageBufWrap::clear () {
    m_buf->clear();
}
void 
ImageBufWrap::reset_to_new_image (const std::string &name = std::string(), 
            ImageCache *imagecache = NULL) {
    m_buf->reset(name, imagecache);
}
void ImageBufWrap:: reset_to_blank_image (const std::string &name, 
                                            const ImageSpec &spec) {
    m_buf->reset(name, spec);
}


void ImageBufWrap::alloc (const ImageSpec &spec) {
    m_buf->alloc(spec);
}

// TODO: How to wrap ProgressCallback?
/*
bool ImageBufWrap::read (int subimage=0, bool force=false, 
            TypeDesc convert=TypeDesc::UNKNOWN,
            ProgressCallback progress_callback=NULL,
            void *progress_callback_data=NULL) {
    return m_buf->read(subimage, force, convert, progress_callback, 
                    progress_callback_data);        
}    

bool ImageBufWrap::save (const std::string &filename = std::string(),
            const std::string &fileformat = std::string(),
            ProgressCallback progress_callback=NULL,
            void *progress_callback_data=NULL) const {

    return m_buf->save(filename, fileformat, progress_callback,
                    progress_callback_data);
}

//TODO: besides PC, check if IOW* works ok  
bool ImageBufWrap::write (ImageOutputWrap *out,
            ProgressCallback progress_callback=NULL,
            void *progress_callback_data=NULL) const {
    
    return m_buf->write(out->Output, progress_callback,
                        progress_callback_data);        
}
*/

bool ImageBufWrap::init_spec (const std::string &filename,
                              int subimage, int miplevel)
{
    return m_buf->init_spec(filename, subimage, miplevel);
}
const ImageSpec& ImageBufWrap::spec() const {
    return m_buf->spec();
}
const std::string& ImageBufWrap::name() const {
    return m_buf->name();
}
const std::string& ImageBufWrap::file_format_name() const {
    return m_buf->file_format_name();
}
int ImageBufWrap::subimage() const {
    return m_buf->subimage();
}
int ImageBufWrap::nsubimages() const {
    return m_buf->nsubimages();
}
int ImageBufWrap::nchannels() const {
    return m_buf->nchannels();
}

float ImageBufWrap::getchannel (int x, int y, int z) const {
    return m_buf->getchannel(x, y, z);
}
void ImageBufWrap::getpixel (int x, int y, float *pixel,
                            int maxchannels=1000) const {
    m_buf->getpixel(x, y, pixel, maxchannels);
}
void ImageBufWrap::interppixel (float x, float y, float *pixel) const {
    m_buf->interppixel(x, y, pixel);
}    
void ImageBufWrap::interppixel_NDC (float x, float y, float *pixel) const {
    m_buf->interppixel_NDC(x, y, pixel);
}
void ImageBufWrap::setpixel_xy (int x, int y, const float *pixel, 
                                int maxchannels=1000) {
    m_buf->setpixel(x, y, pixel, maxchannels);
}
void ImageBufWrap::setpixel_i (int i, const float *pixel, 
                                int maxchannels=1000) {
    m_buf->setpixel(i, pixel, maxchannels);
}

// These copy_pixel methods require the user to send an appropriate
// area of memory ("*result"), which would make little sense from Python.
// The user *could* create an appropriately sized array in Python by
// filling it with the correct amount of dummy data, but would
// this defeat the purpose of Python? Instead, the wrapper could
// allocate that array, fill it, and return it to Python. This is the way
// ImageInput.read_image() was wrapped.
bool ImageBufWrap::copy_pixels (int xbegin, int xend, int ybegin, 
                    int yend, TypeDesc format, void *result) const {
    return m_buf->copy_pixels(xbegin, xend, ybegin, yend, format, result);
}

// TODO: handle T and <T>. Don't know how to handle this with B.P, 
// but haven't given it much thought yet.
/*
bool copy_pixels_convert (int xbegin, int xend, int ybegin, int yend,
                            T *result) const {
    return m_buf->copy_pixels (xbegin, xend, ybegin, yend, result);
} 
bool copy_pixels_convert_safer (int xbegin, int xend, int ybegin,
                            int yend, std::vector<T> &result) const {
    return m_buf->copy_pixels(xbegin, xend, ybegin, yend, result);
}
*/

int ImageBufWrap::orientation() const { 
    return m_buf->orientation(); 
}
int ImageBufWrap::oriented_width() const {
    return m_buf->oriented_width();
}
int ImageBufWrap::oriented_height() const {
    return m_buf->oriented_height();
}
int ImageBufWrap::oriented_x() const {
    return m_buf->oriented_x();
}
int ImageBufWrap::oriented_y() const {
    return m_buf->oriented_y();
}
int ImageBufWrap::oriented_full_width() const {
    return m_buf->oriented_full_width();
}
int ImageBufWrap::oriented_full_height() const {
    return m_buf->oriented_full_height();
}
int ImageBufWrap::oriented_full_x() const {
    return m_buf->oriented_full_x();
}
int ImageBufWrap::oriented_full_y() const {
    return m_buf->oriented_full_y();
}    
int ImageBufWrap::xbegin() const {
    return m_buf->xbegin();
}
int ImageBufWrap::xend() const {
    return m_buf->xend();
}
int ImageBufWrap::ybegin() const {
    return m_buf->ybegin();
}
int ImageBufWrap::yend() const {
    return m_buf->yend();
}
int ImageBufWrap::xmin() {
    return m_buf->xmin();
}
int ImageBufWrap::xmax() {
    return m_buf->xmax();
}
int ImageBufWrap::ymin() {
    return m_buf->ymin();
}
int ImageBufWrap::ymax() {
    return m_buf->ymax();
}
bool ImageBufWrap::pixels_valid () const {
    return m_buf->pixels_valid();
}
bool ImageBufWrap::localpixels () const {
    return m_buf->localpixels();
}
//TODO: class Iterator and ConstIterator

std::string ImageBufWrap::geterror()const  {
    return m_buf->geterror();
}

void declare_imagebuf()
{
    class_<ImageBufWrap, boost::noncopyable> cls ("ImageBuf",
            init<const std::string&, const ImageSpec&>());
    cls.def(init<const std::string&, ImageCacheWrap*>());
    
    cls.def("clear", &ImageBufWrap::clear);
    cls.def("reset", &ImageBufWrap::reset_to_new_image);
    cls.def("reset", &ImageBufWrap::reset_to_blank_image);    
    cls.def("alloc", &ImageBufWrap::alloc);
    //cls.def("read", &ImageBufWrap::read);    
    //cls.def("save", &ImageBufWrap::save);
    //cls.def("write", &ImageBufWrap::write);
    cls.def("init_spec", &ImageBufWrap::init_spec);

    cls.def("spec", &ImageBufWrap::spec, 
                return_value_policy<copy_const_reference>());

    cls.def("name", &ImageBufWrap::name, 
                return_value_policy<copy_const_reference>());

    cls.def("file_format_name", &ImageBufWrap::file_format_name, 
                return_value_policy<copy_const_reference>());

    cls.def("subimage", &ImageBufWrap::subimage);
    cls.def("nsubimages", &ImageBufWrap::nsubimages);
    cls.def("nchannels", &ImageBufWrap::nchannels);
    cls.def("getchannel", &ImageBufWrap::getchannel);
    cls.def("getpixel", &ImageBufWrap::getpixel);
    cls.def("interppixel", &ImageBufWrap::interppixel);
    cls.def("interppixel_NDC", &ImageBufWrap::interppixel_NDC);
    cls.def("setpixel", &ImageBufWrap::setpixel_xy);
    cls.def("setpixel", &ImageBufWrap::setpixel_i);
    cls.def("copy_pixels", &ImageBufWrap::copy_pixels);
    //cls.def("copy_pixels", &ImageBufWrap::copy_pixels_convert);
    //cls.def("copy_pixels", &ImageBufWrap::copy_pixels_convert_safer);
    cls.def("orientation", &ImageBufWrap::orientation);
    cls.def("oriented_width", &ImageBufWrap::oriented_width);
    cls.def("oriented_height", &ImageBufWrap::oriented_height);
    cls.def("oriented_x", &ImageBufWrap::oriented_x);
    cls.def("oriented_y", &ImageBufWrap::oriented_y);
    cls.def("oriented_full_width", &ImageBufWrap::oriented_full_width);
    cls.def("oriented_full_height", &ImageBufWrap::oriented_full_height);
    cls.def("oriented_full_x", &ImageBufWrap::oriented_full_x);
    cls.def("oriented_full_y", &ImageBufWrap::oriented_full_y);
    cls.def("xbegin", &ImageBufWrap::xbegin);
    cls.def("xend", &ImageBufWrap::xend);
    cls.def("ybegin", &ImageBufWrap::ybegin);
    cls.def("yend", &ImageBufWrap::yend);
    cls.def("xmin", &ImageBufWrap::xmin);
    cls.def("xmax", &ImageBufWrap::xmax);
    cls.def("ymin", &ImageBufWrap::ymin);
    cls.def("ymax", &ImageBufWrap::ymax);
    cls.def("pixels_valid", &ImageBufWrap::pixels_valid);
    cls.def("localpixels", &ImageBufWrap::localpixels);
    cls.def("geterror",    &ImageBufWrap::geterror);
    
}

} // namespace PyOpenImageIO

