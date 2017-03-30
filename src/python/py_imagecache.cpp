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

std::string ImageCacheWrap::resolve_filename (const std::string &val)
{
    ScopedGILRelease gil;
    return m_cache->resolve_filename(val);
}


#if 0
object ImageCacheWrap::get_image_info (ustring filename, int subimage,
                                       int miplevel, ustring dataname,
                                       TypeDesc datatype)
{
    ScopedGILRelease gil;
    return m_cache->get_image_info(filename, subimage, miplevel,
                                   dataname, datatype, data);
}

object ImageCacheWrap::get_imagespec(ustring filename, int subimage=0)
{
    ScopedGILRelease gil;
    return m_cache->get_imagespec(filename, spec, subimage);
}    
#endif


object ImageCacheWrap::get_pixels (const std::string &filename_,
                       int subimage, int miplevel, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       TypeDesc datatype)
{ 
    ScopedGILRelease gil;
    ustring filename (filename_);
    int chbegin = 0, chend = 0;
    if (! m_cache->get_image_info (filename, subimage, miplevel,
                                   ustring("channels"), TypeDesc::INT, &chend))
        return object(handle<>(Py_None));  // couldn't open file

    size_t size = size_t ((xend-xbegin) * (yend-ybegin) * (zend-zbegin) *
                          (chend-chbegin) * datatype.size());
    std::unique_ptr<char[]> data (new char [size]);
    if (! m_cache->get_pixels (filename, subimage, miplevel, xbegin, xend,
                               ybegin, yend, zbegin, zend, datatype, &data[0]))
        return object(handle<>(Py_None));   // get_pixels failed;

    return C_array_to_Python_array (data.get(), datatype, size);
}


//Not sure how to expose this to Python. 
/*
Tile *get_tile (ImageCache &ic, ustring filename, int subimage,
                int x, int y, int z) {
    return ic.get_tile(filename, subimage, x, y, z);
}
void release_tile (ImageCache &ic, Tile *tile) const {
    ic.release-tile(tile);
}
const void *tile_pixels (ImageCache &ic, Tile *tile, TypeDesc &format) const {
    ic.tile_pixels(tile, format);
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



void
ImageCacheWrap::attribute_int (const std::string &name, int val)
{
    m_cache->attribute (name, val);
}


void
ImageCacheWrap::attribute_float (const std::string &name, float val)
{
    m_cache->attribute (name, val);
}


void
ImageCacheWrap::attribute_string (const std::string &name,
                                  const std::string &val)
{
    m_cache->attribute (name, val);
}


void
ImageCacheWrap::attribute_typed (const std::string &name,
                                 TypeDesc type, object &obj)
{
    ::PyOpenImageIO::attribute_typed (*m_cache, name, type, obj);
}


void
ImageCacheWrap::attribute_tuple_typed (const std::string &name,
                                       TypeDesc type, tuple &obj)
{
    ::PyOpenImageIO::attribute_tuple_typed (*m_cache, name, type, obj);
}



object
ImageCacheWrap::getattribute_typed (const std::string &name, TypeDesc type)
{
    return ::PyOpenImageIO::getattribute_typed (*m_cache, name, type);
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
        .def("attribute", &ImageCacheWrap::attribute_float)
        .def("attribute", &ImageCacheWrap::attribute_int)
        .def("attribute", &ImageCacheWrap::attribute_string)
        .def("attribute", &ImageCacheWrap::attribute_typed)
        .def("attribute", &ImageCacheWrap::attribute_tuple_typed)
        .def("getattribute",  &ImageCacheWrap::getattribute_typed)
        // .def("getattribute",  &ImageCacheWrap::get_attribute_untyped)

        .def("resolve_filename", &ImageCacheWrap::resolve_filename)
        // .def("get_image_info", &ImageCacheWrap::get_image_info)
        // .def("get_imagespec", &ImageCacheWrap::get_imagespec,
        //      (arg("subimage")=0)),
        .def("get_pixels", &ImageCacheWrap::get_pixels)
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
