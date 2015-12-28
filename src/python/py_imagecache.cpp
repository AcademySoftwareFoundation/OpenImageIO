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
#include "OpenImageIO/ustring.h"

namespace PyOpenImageIO
{
using namespace boost::python;

ImageCacheWrap* ImageCacheWrap::create (bool shared=true)
{
    ImageCacheWrap *icw = new ImageCacheWrap;
    icw->m_cache = ImageCache::create(shared);
    return icw;
}    

void ImageCacheWrap::destroy (ImageCacheWrap *x)
{
    ImageCache::destroy(x->m_cache);
}

void ImageCacheWrap::clear ()
{
    m_cache->clear();
}

bool ImageCacheWrap::attribute (const std::string &name, TypeDesc type,
                const void *val)
{
    return m_cache->attribute(name, type, val);
}

//Shortcuts for common types
bool ImageCacheWrap::attribute_int (const std::string &name, int val)
{
    return m_cache->attribute(name, val);
}

bool ImageCacheWrap::attribute_float (const std::string &name, float val)
{
    return m_cache->attribute(name, val);
}

bool ImageCacheWrap::attribute_double (const std::string &name, double val)
{
    return m_cache->attribute(name, val);
}

bool ImageCacheWrap::attribute_char (const std::string &name, const char *val)
{
    return m_cache->attribute(name, val);
}

bool ImageCacheWrap::attribute_string (const std::string &name, const std::string &val)
{
    return m_cache->attribute(name, val);
}

bool ImageCacheWrap::getattribute(const std::string &name, TypeDesc type,
                    void *val)
{
    return m_cache->getattribute(name, type, val);
}

//Shortcuts for common types
bool ImageCacheWrap::getattribute_int(const std::string &name, int &val)
{
    return m_cache->getattribute(name, val);
}

bool ImageCacheWrap::getattribute_float(const std::string &name, float &val)
{
    return m_cache->getattribute(name, val);
}

bool ImageCacheWrap::getattribute_double(const std::string &name, double &val)
{
    return m_cache->getattribute(name, val);
}

bool ImageCacheWrap::getattribute_char(const std::string &name, char **val)
{
    return m_cache->getattribute(name, val);
}

bool ImageCacheWrap::getattribute_string(const std::string &name, std::string &val)
{
    return m_cache->getattribute(name, val);
}

std::string ImageCacheWrap::resolve_filename (const std::string &val) {
    ScopedGILRelease gil;
    return m_cache->resolve_filename(val);
}

bool ImageCacheWrap::get_image_info (ustring filename, int subimage,
                                     int miplevel, ustring dataname,
                                     TypeDesc datatype, void *data)
{
    ScopedGILRelease gil;
    return m_cache->get_image_info(filename, subimage, miplevel,
                                   dataname, datatype, data);
}

bool ImageCacheWrap::get_image_info_old (ustring filename, ustring dataname,
                        TypeDesc datatype, void *data)
{
    ScopedGILRelease gil;
    return m_cache->get_image_info(filename, 0, 0, dataname, datatype, data);
}   

bool ImageCacheWrap::get_imagespec(ustring filename, ImageSpec &spec, int subimage=0)
{
    ScopedGILRelease gil;
    return m_cache->get_imagespec(filename, spec, subimage);
}    

bool ImageCacheWrap::get_pixels (ustring filename, int subimage, int miplevel,
                int xbegin, int xend, int ybegin, int yend, int zbegin, 
                int zend, TypeDesc format, void *result)
{ 
    ScopedGILRelease gil;
    return m_cache->get_pixels(filename, subimage, miplevel, xbegin, xend,
                               ybegin, yend, zbegin, zend, format, result);
}

//Not sure how to expose this to Python. 
/*
Tile *get_tile (ustring filename, int subimage,
                int x, int y, int z) {
    return m_cache->get_tile(filename, subimage, x, y, z);
}
void release_tile (Tile *tile) const {
    m_cache->release-tile(tile);
}
const void *tile_pixels (Tile *tile, TypeDesc &format) const {
    m_cache->tile_pixels(tile, format);
}        
*/

std::string ImageCacheWrap::geterror () const
{
    return m_cache->geterror();
}

std::string ImageCacheWrap::getstats (int level=1) const
{
    ScopedGILRelease gil;
    return m_cache->getstats(level);
}

void ImageCacheWrap::invalidate (ustring filename)
{
    ScopedGILRelease gil;
    return m_cache->invalidate(filename);
}

void ImageCacheWrap::invalidate_all (bool force=false)
{
    ScopedGILRelease gil;
    return m_cache->invalidate_all(force);
}           

void declare_imagecache()
{
    class_<ImageCacheWrap, boost::noncopyable>("ImageCache", no_init)
        .def("create", &ImageCacheWrap::create,
                 (arg("shared")),         
                 return_value_policy<manage_new_object>())
        .staticmethod("create")
        .def("destroy", &ImageCacheWrap::destroy)
        .staticmethod("destroy")
        .def("clear", &ImageCacheWrap::clear)
        // .def("attribute", &ImageCacheWrap::attribute)
        .def("attribute", &ImageCacheWrap::attribute_int)
        .def("attribute", &ImageCacheWrap::attribute_float)
        // .def("attribute", &ImageCacheWrap::attribute_double)
        // .def("attribute", &ImageCacheWrap::attribute_char)
        .def("attribute", &ImageCacheWrap::attribute_string)
        // .def("getattribute", &ImageCacheWrap::attribute)
        .def("getattribute", &ImageCacheWrap::getattribute_int)
        .def("getattribute", &ImageCacheWrap::getattribute_float)
        // .def("getattribute", &ImageCacheWrap::getattribute_double)
        // .def("getattribute", &ImageCacheWrap::getattribute_char)
        .def("getattribute", &ImageCacheWrap::getattribute_string)
        // .def("get_image_info", &ImageCacheWrap::get_image_info)
        // .def("get_image_info", &ImageCacheWrap::get_image_info_old)
        .def("get_imagespec", &ImageCacheWrap::get_imagespec)
        // .def("get_pixels", &ImageCacheWrap::get_pixels)
        .def("resolve_filename", &ImageCacheWrap::resolve_filename)

//      .def("get_tile", &ImageCacheWrap::get_tile)
//      .def("release_tile", &ImageCacheWrap::release_tile)
//      .def("tile_pixels", &ImageCacheWrap::tile_pixels)

//added _ to the method names for consistency
        .def("geterror",       &ImageCacheWrap::geterror)
        .def("getstats",       &ImageCacheWrap::getstats)
        .def("invalidate",     &ImageCacheWrap::invalidate)
        .def("invalidate_all", &ImageCacheWrap::invalidate_all)
    ;

}

} // namespace PyOpenImageIO

