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
#include "py_oiio.h"

namespace PyOpenImageIO
{


bool ImageOutput_open_specs (ImageOutput &self, const std::string &name,
                             py::tuple &specs)
{
    const size_t length = len(specs);
    if (length == 0)
        return false;
    std::vector<ImageSpec> Cspecs (length);
    for (size_t i = 0; i < length; ++i) {
        auto s = specs[i];
        if (py::isinstance<ImageSpec>(s))
            Cspecs[i] = s.cast<ImageSpec>();
        else
            return false; // Tuple item was not an ImageSpec
    }
    return self.open (name, int(length), &Cspecs[0]);
}



bool
ImageOutput_write_scanline (ImageOutput &self, int y, int z, py::buffer &buffer)
{
    const ImageSpec& spec (self.spec());
    oiio_bufinfo buf (buffer.request(), spec.nchannels, spec.width, 1, 1, 1);
    if (! buf.data) {
        self.error ("Could not decode python buffer");
        return false;  // failed sanity checks
    }
    if (static_cast<int>(buf.size) < self.spec().width*self.spec().nchannels) {
        self.error ("write_scanlines was not passed a long enough array");
        return false;
    }
    py::gil_scoped_release gil;
    return self.write_scanline (y, z, buf.format, buf.data, buf.xstride);
}



bool
ImageOutput_write_scanlines (ImageOutput &self, int ybegin, int yend, int z,
                             py::buffer &buffer)
{
    const ImageSpec& spec (self.spec());
    oiio_bufinfo buf (buffer.request(), spec.nchannels, spec.width,
                      yend-ybegin, 1, 2);
    if (! buf.data) {
        self.error ("Could not decode python buffer");
        return false;  // failed sanity checks
    }
    if (static_cast<int>(buf.size) < self.spec().width*self.spec().nchannels*(yend-ybegin)) {
        self.error ("write_scanlines was not passed a long enough array");
        return false;
    }
    py::gil_scoped_release gil;
    return self.write_scanlines (ybegin, yend, z, buf.format, buf.data,
                                 buf.xstride, buf.ystride);
}



bool
ImageOutput_write_tile (ImageOutput &self, int x, int y, int z, py::buffer &buffer)
{
    const ImageSpec& spec (self.spec());
    oiio_bufinfo buf (buffer.request(), spec.nchannels, spec.tile_width,
                      spec.tile_height, spec.tile_depth,
                      spec.tile_depth > 1 ? 3 : 2);
    if (! buf.data) {
        self.error ("Could not decode python buffer");
        return false;  // failed sanity checks
    }
    if (buf.size < self.spec().tile_pixels()*self.spec().nchannels) {
        self.error ("write_tile was not passed a long enough array");
        return false;
    }
    py::gil_scoped_release gil;
    return self.write_tile (x, y, z, buf.format, buf.data,
                            buf.xstride, buf.ystride, buf.zstride);
}



bool
ImageOutput_write_tiles (ImageOutput &self, int xbegin, int xend,
                         int ybegin, int yend, int zbegin, int zend,
                         py::buffer &buffer)
{
    const ImageSpec& spec (self.spec());
    oiio_bufinfo buf (buffer.request(), spec.nchannels, xend-xbegin,
                      yend-ybegin, zend-zbegin, spec.tile_depth > 1 ? 3 : 2);
    if (! buf.data) {
        self.error ("Could not decode python buffer");
        return false;  // failed sanity checks
    }
    if (static_cast<int>(buf.size) < (xend-xbegin)*(yend-ybegin)*(zend-zbegin)*self.spec().nchannels) {
        self.error ("write_tiles was not passed a long enough array");
        return false;
    }
    py::gil_scoped_release gil;
    return self.write_tiles (xbegin, xend, ybegin, yend, zbegin, zend,
                             buf.format, buf.data,
                             buf.xstride, buf.ystride, buf.zstride);
}



bool
ImageOutput_write_image (ImageOutput &self, py::buffer &buffer)
{
    const ImageSpec& spec (self.spec());
    oiio_bufinfo buf (buffer.request(), spec.nchannels, spec.width,
                      spec.height, spec.depth, spec.depth > 1 ? 3 : 2);
    if (! buf.data || buf.size < spec.image_pixels()*spec.nchannels)
        return false;  // failed sanity checks
    py::gil_scoped_release gil;
    return self.write_image (buf.format, buf.data,
                             buf.xstride, buf.ystride, buf.zstride);
}



bool
ImageOutput_write_deep_scanlines (ImageOutput &self, int ybegin, int yend, int z,
                                  const DeepData &deepdata)
{
    py::gil_scoped_release gil;
    return self.write_deep_scanlines (ybegin, yend, z, deepdata);
}


bool
ImageOutput_write_deep_tiles (ImageOutput &self, int xbegin, int xend,
                              int ybegin, int yend, int zbegin, int zend,
                              const DeepData &deepdata)
{
    py::gil_scoped_release gil;
    return self.write_deep_tiles (xbegin, xend, ybegin, yend,
                                  zbegin, zend, deepdata);
}


bool
ImageOutput_write_deep_image (ImageOutput &self, const DeepData &deepdata)
{
    py::gil_scoped_release gil;
    return self.write_deep_image (deepdata);
}





void declare_imageoutput (py::module &m)
{
    using namespace pybind11::literals;

    py::class_<ImageOutput>(m, "ImageOutput")
        .def_static("create", [](const std::string& filename, const std::string& searchpath) ->py::object {
                ImageOutput *out (ImageOutput::create(filename, searchpath));
                return out ? py::cast(out) : py::none();
            },
            "filename"_a, "plugin_searchpath"_a="")
        .def("format_name",     &ImageOutput::format_name)
        .def("supports",    [](const ImageOutput &self, const std::string& feature){
                return self.supports(feature); })
        .def("spec",            &ImageOutput::spec)
        .def("open", [](ImageOutput &self, const std::string &name, const ImageSpec &newspec,
                        const std::string &modestr) {
                ImageOutput::OpenMode mode = ImageOutput::Create;
                if (Strutil::iequals(modestr, "AppendSubimage"))
                    mode = ImageOutput::AppendSubimage;
                else if (Strutil::iequals(modestr, "AppendMIPLevel"))
                    mode = ImageOutput::AppendMIPLevel;
                else if (! Strutil::iequals(modestr, "Create"))
                    throw std::invalid_argument (Strutil::format("Unknown open mode '%s'", modestr));
                return self.open (name, newspec, mode);
            },
            "filename"_a, "spec"_a, "mode"_a="Create")
        .def("open",            &ImageOutput_open_specs)
        .def("close", [](ImageOutput &self){
                return self.close();
            })
        .def("write_image",      &ImageOutput_write_image)
        .def("write_scanline",   &ImageOutput_write_scanline,
             "y"_a, "z"_a, "pixels"_a)
        .def("write_scanlines",  &ImageOutput_write_scanlines,
             "ybegin"_a, "yend"_a, "z"_a, "pixels"_a)
        .def("write_tile",       &ImageOutput_write_tile,
             "x"_a, "y"_a, "z"_a, "pixels"_a)
        .def("write_tiles",      &ImageOutput_write_tiles,
             "xbegin"_a, "xend"_a, "ybegin"_a, "yend"_a,
             "zbegin"_a, "zend"_a, "pixels"_a)
        .def("write_deep_scanlines", &ImageOutput_write_deep_scanlines,
             "ybegin"_a, "yend"_a, "z"_a, "deepdata"_a)
        .def("write_deep_tiles", &ImageOutput_write_deep_tiles,
             "xbegin"_a, "xend"_a, "ybegin"_a, "yend"_a,
             "zbegin"_a, "zend"_a, "deepdata"_a)
        .def("write_deep_image", &ImageOutput_write_deep_image)
        .def("copy_image", [](ImageOutput &self, ImageInput &in){
                return self.copy_image (&in);
            })
        .def("geterror", [](ImageOutput &self){
                return PY_STR(self.geterror());
            })
    ;

}

} // namespace PyOpenImageIO

