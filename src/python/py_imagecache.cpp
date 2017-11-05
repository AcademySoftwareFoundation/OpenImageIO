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

// Make a special wrapper to help with the weirdo way we use create/destroy.
class ImageCacheWrap {
public:
    struct ICDeleter {
        void operator()(ImageCache* p) const { ImageCache::destroy(p); }
    };
    std::unique_ptr<ImageCache,ICDeleter> m_cache;

    ImageCacheWrap (bool shared=true) : m_cache(ImageCache::create(shared)) {}
    ImageCacheWrap(const ImageCacheWrap&) = delete;
    ImageCacheWrap(ImageCacheWrap&&) = delete;
    ~ImageCacheWrap () { }  // will call the deleter on the IC
    static void destroy (ImageCacheWrap *x, bool teardown=false) {
        ImageCache::destroy (x->m_cache.release(), teardown);
    }
    py::object get_pixels (const std::string &filename,
                       int subimage, int miplevel, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       TypeDesc datatype);
};



py::object ImageCacheWrap::get_pixels (const std::string &filename_,
                       int subimage, int miplevel, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       TypeDesc datatype)
{
    ustring filename (filename_);
    if (datatype == TypeUnknown)
        datatype = TypeFloat;
    int chbegin = 0, chend = 0;
    if (! m_cache->get_image_info (filename, subimage, miplevel,
                                   ustring("channels"), TypeDesc::INT, &chend))
        return py::none();  // couldn't open file

    size_t size = size_t ((xend-xbegin) * (yend-ybegin) * (zend-zbegin) *
                          (chend-chbegin) * datatype.size());
    std::unique_ptr<char[]> data (new char [size]);
    bool ok;
    {
        py::gil_scoped_release gil;
        ok = m_cache->get_pixels (filename, subimage, miplevel, xbegin, xend,
                                  ybegin, yend, zbegin, zend, datatype, data.get());
    }
    if (ok)
        return make_numpy_array (datatype, data.release(),
                                 (zend-zbegin)>1 ? 4 : 3, chend-chbegin,
                                 xend-xbegin, yend-ybegin, zend-zbegin);
    else
        return py::none();
}





void declare_imagecache (py::module &m)
{
    using namespace pybind11::literals;

    py::class_<ImageCacheWrap>(m, "ImageCache")
        .def(py::init<bool>(),
            "shared"_a=true)
        // .def_static("create", &ImageCacheWrap::create,
        //     "shared"_a=true)
        .def_static("destroy", &ImageCacheWrap::destroy,
            "cache"_a, "teardown"_a=false)

        .def("attribute", [](ImageCacheWrap &ic, const std::string &name, float val){
                if (ic.m_cache)
                    ic.m_cache->attribute(name,val);
            })
        .def("attribute", [](ImageCacheWrap &ic, const std::string &name, int val){
                if (ic.m_cache)
                    ic.m_cache->attribute(name,val);
            })
        .def("attribute", [](ImageCacheWrap &ic, const std::string &name, const std::string &val){
                if (ic.m_cache)
                    ic.m_cache->attribute(name,val);
            })
        .def("attribute", [](ImageCacheWrap &ic, const std::string &name, TypeDesc type, const py::object &obj) {
                if (ic.m_cache)
                    attribute_typed (*ic.m_cache, name, type, obj);
            })
        .def("getattribute",  [](const ImageCacheWrap &ic, const std::string &name, TypeDesc type){
                return getattribute_typed (*ic.m_cache, name, type);
            },
            "name"_a, "type"_a=TypeUnknown)
        .def("resolve_filename", [](ImageCacheWrap &ic, const std::string &filename){
                py::gil_scoped_release gil;
                return PY_STR(ic.m_cache->resolve_filename(filename));
            })
        // .def("get_image_info", &ImageCacheWrap::get_image_info)
        // .def("get_imagespec", &ImageCacheWrap::get_imagespec,
        //      "subimage"_a=0),
        .def("get_pixels", &ImageCacheWrap::get_pixels)
        // .def("get_tile", &ImageCacheWrap::get_tile)
        // .def("release_tile", &ImageCacheWrap::release_tile)
        // .def("tile_pixels", &ImageCacheWrap::tile_pixels)

        .def("geterror", [](ImageCacheWrap &ic){
                return PY_STR(ic.m_cache->geterror());
            })
        .def("getstats", [](ImageCacheWrap &ic, int level){
                py::gil_scoped_release gil;
                return PY_STR(ic.m_cache->getstats(level));
            },
            "level"_a=1)
        .def("invalidate", [](ImageCacheWrap &ic, const std::string &filename){
                py::gil_scoped_release gil;
                ic.m_cache->invalidate(ustring(filename));
            },
            "filename"_a)
        .def("invalidate_all", [](ImageCacheWrap &ic, bool force){
                py::gil_scoped_release gil;
                ic.m_cache->invalidate_all(force);
            },
            "force"_a=false)
    ;
}

} // namespace PyOpenImageIO
