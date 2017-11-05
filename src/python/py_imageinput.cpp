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

static py::object
ImageInput_read_image (ImageInput &self, int chbegin, int chend,
                       TypeDesc format, int dims)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = self.spec();
    if (format == TypeUnknown)
        format = spec.format;
    chend = clamp (chend, chbegin+1, spec.nchannels);
    size_t nchans = size_t(chend - chbegin);
    size_t pixelsize = size_t(nchans * format.size());
    size_t size = spec.image_pixels() * pixelsize;
    std::unique_ptr<char[]> data (new char[size]);
    bool ok;
    {
        py::gil_scoped_release gil;
        ok = self.read_image (chbegin, chend, format, data.get());
    }
    if (ok)
        return make_numpy_array (format, data.release(), dims, nchans, spec.width,
                                 spec.height, spec.depth);
    else
        return py::none();
}



static py::object
ImageInput_read_scanlines (ImageInput &self, int ybegin, int yend, int z,
                           int chbegin, int chend, TypeDesc format,
                           int dims)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = self.spec();
    if (format == TypeUnknown)
        format = spec.format;
    chend = clamp (chend, chbegin+1, spec.nchannels);
    size_t nchans = size_t(chend - chbegin);
    size_t pixelsize = size_t(nchans * format.size());
    size_t size = size_t((yend-ybegin) * spec.width) * pixelsize;
    std::unique_ptr<char[]> data (new char[size]);
    bool ok;
    {
        py::gil_scoped_release gil;
        ok = self.read_scanlines (ybegin, yend, z, chbegin, chend,
                                  format, data.get());
    }
    if (ok)
        return make_numpy_array (format, data.release(), dims, nchans, spec.width,
                                 yend-ybegin, 1);
    else
        return py::none();
}



static py::object
ImageInput_read_tiles (ImageInput &self, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       int chbegin, int chend, TypeDesc format, int dims)
{
    // Allocate our own temp buffer and try to read the scanline into it.
    // If the read fails, return None.
    const ImageSpec &spec = self.spec();
    if (format == TypeUnknown)
        format = spec.format;
    chend = clamp (chend, chbegin+1, spec.nchannels);
    size_t nchans = size_t(chend - chbegin);
    size_t pixelsize = size_t(nchans * format.size());
    size_t size = size_t((xend-xbegin) * (yend-ybegin) * (zend-zbegin)) * pixelsize;
    std::unique_ptr<char[]> data (new char[size]);
    bool ok;
    {
        py::gil_scoped_release gil;
        ok = self.read_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                              chbegin, chend, format, data.get());
    }
    if (ok)
        return make_numpy_array (format, data.release(), dims, nchans,
                                 xend-xbegin, yend-ybegin, zend-zbegin);
    else
        return py::none();
}



py::object
ImageInput_read_native_deep_scanlines (ImageInput& self, int ybegin, int yend,
                                       int z, int chbegin, int chend)
{
    std::unique_ptr<DeepData> dd;
    bool ok = true;
    {
        py::gil_scoped_release gil;
        dd.reset (new DeepData);
        ok = self.read_native_deep_scanlines (ybegin, yend, z,
                                              chbegin, chend, *dd);
    }
    return ok ? py::cast(dd.release()) : py::none();
}



py::object
ImageInput_read_native_deep_tiles (ImageInput& self, int xbegin, int xend,
                                   int ybegin, int yend, int zbegin, int zend,
                                   int chbegin, int chend)
{
    std::unique_ptr<DeepData> dd;
    bool ok = true;
    {
        py::gil_scoped_release gil;
        dd.reset (new DeepData);
        ok = self.read_native_deep_tiles (xbegin, xend, ybegin, yend,
                                          zbegin, zend, chbegin, chend, *dd);
    }
    return ok ? py::cast(dd.release()) : py::none();
}



py::object
ImageInput_read_native_deep_image (ImageInput& self)
{
    std::unique_ptr<DeepData> dd;
    bool ok = true;
    {
        py::gil_scoped_release gil;
        dd.reset (new DeepData);
        ok = self.read_native_deep_image (*dd);
    }
    return ok ? py::cast(dd.release()) : py::none();
}




void declare_imageinput (py::module &m)
{
    using namespace pybind11::literals;

    py::class_<ImageInput>(m, "ImageInput")
        .def_static("create", [](const std::string& filename, const std::string& searchpath) ->py::object {
                ImageInput *in (ImageInput::create(filename, searchpath));
                return in ? py::cast(in) : py::none();
            },
            "filename"_a, "plugin_searchpath"_a="")
        .def_static("open", [](const std::string& filename) ->py::object {
                ImageInput *in (ImageInput::open(filename));
                return in ? py::cast(in) : py::none();
                },
            "filename"_a)
        .def_static("open", [](const std::string& filename, const ImageSpec& config) ->py::object {
                ImageInput *in (ImageInput::open(filename, &config));
                return in ? py::cast(in) : py::none();
            },
            "filename"_a, "config"_a)
        .def("format_name",      &ImageInput::format_name)
        .def("valid_file",       &ImageInput::valid_file)
        .def("spec",             &ImageInput::spec)
        .def("supports",    [](const ImageInput &self, const std::string& feature){
                return self.supports(feature); })
        .def("close",            &ImageInput::close)
        .def("current_subimage", &ImageInput::current_subimage)
        .def("current_miplevel", &ImageInput::current_miplevel)
        .def("seek_subimage",    [](ImageInput &self, int subimage, int miplevel){
                py::gil_scoped_release gil;
                ImageSpec dummyspec;
                return self.seek_subimage (subimage, miplevel, dummyspec);
            })
        .def("read_image", [](ImageInput &self, TypeDesc format)->py::object{
                const ImageSpec &spec (self.spec());
                return ImageInput_read_image (self, 0, spec.nchannels, format,
                                              spec.depth > 1 ? 4 : 3);
            },
            "format"_a=TypeFloat)
        .def("read_scanline", [](ImageInput &self, int y, int z,
                                 TypeDesc format)->py::object{
                const ImageSpec &spec (self.spec());
                return ImageInput_read_scanlines (self, y, y+1, z,
                                                  0, spec.nchannels, format, 2);
            },
            "y"_a, "z"_a=0, "format"_a=TypeFloat)
        .def("read_scanlines", [](ImageInput &self, int ybegin, int yend, int z,
                                  int chbegin, int chend, TypeDesc format)->py::object{
                return ImageInput_read_scanlines (self, ybegin, yend, z,
                                                  chbegin, chend, format, 3);
            },
            "ybegin"_a, "yend"_a, "z"_a, "chbegin"_a, "chend"_a,
            "format"_a=TypeFloat)
        .def("read_tiles", [](ImageInput &self, int xbegin, int xend,
                              int ybegin, int yend, int zbegin, int zend,
                              int chbegin, int chend, TypeDesc format)->py::object{
                const ImageSpec &spec (self.spec());
                return ImageInput_read_tiles (self, xbegin, xend, ybegin, yend,
                                              zbegin, zend, chbegin, chend, format,
                                              spec.tile_depth > 1 ? 4 : 3);
            },
            "xbegin"_a, "xend"_a, "ybegin"_a, "yend"_a, "zbegin"_a, "zend"_a,
            "chbegin"_a, "chend"_a, "format"_a=TypeFloat)
        .def("read_tile", [](ImageInput &self, int x, int y, int z,
                             TypeDesc format)->py::object{
                const ImageSpec &spec (self.spec());
                return ImageInput_read_tiles (self, x, x+spec.tile_width,
                                              y, y+spec.tile_height,
                                              z, z+std::min(1,spec.tile_depth),
                                              0, spec.nchannels, format,
                                              spec.tile_depth > 1 ? 4 : 3);
            },
            "x"_a, "y"_a, "z"_a, "format"_a=TypeFloat)
        .def("read_native_deep_scanlines", &ImageInput_read_native_deep_scanlines,
             "ybegin"_a, "yend"_a, "z"_a, "chbegin"_a, "chend"_a)
        .def("read_native_deep_tiles", &ImageInput_read_native_deep_tiles,
             "xbegin"_a, "xend"_a, "ybegin"_a, "yend"_a, "zbegin"_a, "zend"_a,
             "chbegin"_a, "chend"_a)
        .def("read_native_deep_image", &ImageInput_read_native_deep_image)
        .def("geterror", [](ImageInput &self){
                return PY_STR(self.geterror());
            })
    ;
}

} // namespace PyOpenImageIO

