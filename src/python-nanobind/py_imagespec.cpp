// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

OIIO_NAMESPACE_USING

nb::tuple
imagespec_get_channelformats(const ImageSpec& spec, bool allow_empty = true)
{
    std::vector<TypeDesc> formats;
    if (spec.channelformats.size() || !allow_empty)
        spec.get_channelformats(formats);
    return PyOpenImageIO::C_to_tuple(cspan<TypeDesc>(formats));
}


void
imagespec_set_channelformats(ImageSpec& spec, nb::handle py_channelformats)
{
    spec.channelformats.clear();
    PyOpenImageIO::py_to_stdvector(spec.channelformats, py_channelformats);
}


nb::tuple
imagespec_get_channelnames(const ImageSpec& spec)
{
    return PyOpenImageIO::C_to_tuple(cspan<std::string>(spec.channelnames));
}


void
imagespec_set_channelnames(ImageSpec& spec, nb::handle py_channelnames)
{
    spec.channelnames.clear();
    PyOpenImageIO::py_to_stdvector(spec.channelnames, py_channelnames);
}


nb::object
imagespec_getattribute_typed(const ImageSpec& spec, const std::string& name,
                             TypeDesc type = TypeUnknown)
{
    ParamValue tmpparam;
    const ParamValue* p = spec.find_attribute(name, tmpparam, type);
    if (!p)
        return nb::none();
    return PyOpenImageIO::make_pyobject(p->data(), p->type(), p->nvalues());
}


ROI
imagespec_get_roi(const ImageSpec& spec)
{
    return get_roi(spec);
}


ROI
imagespec_get_roi_full(const ImageSpec& spec)
{
    return get_roi_full(spec);
}


void
imagespec_set_roi(ImageSpec& spec, const ROI& roi)
{
    set_roi(spec, roi);
}


void
imagespec_set_roi_full(ImageSpec& spec, const ROI& roi)
{
    set_roi_full(spec, roi);
}

}  // namespace


namespace PyOpenImageIO {

void
declare_imagespec(nb::module_& m)
{
    nb::class_<ImageSpec>(m, "ImageSpec")
        .def_rw("x", &ImageSpec::x)
        .def_rw("y", &ImageSpec::y)
        .def_rw("z", &ImageSpec::z)
        .def_rw("width", &ImageSpec::width)
        .def_rw("height", &ImageSpec::height)
        .def_rw("depth", &ImageSpec::depth)
        .def_rw("full_x", &ImageSpec::full_x)
        .def_rw("full_y", &ImageSpec::full_y)
        .def_rw("full_z", &ImageSpec::full_z)
        .def_rw("full_width", &ImageSpec::full_width)
        .def_rw("full_height", &ImageSpec::full_height)
        .def_rw("full_depth", &ImageSpec::full_depth)
        .def_rw("tile_width", &ImageSpec::tile_width)
        .def_rw("tile_height", &ImageSpec::tile_height)
        .def_rw("tile_depth", &ImageSpec::tile_depth)
        .def_rw("nchannels", &ImageSpec::nchannels)
        .def_rw("format", &ImageSpec::format)
        .def_prop_rw(
            "channelformats",
            [](const ImageSpec& spec) {
                return imagespec_get_channelformats(spec);
            },
            &imagespec_set_channelformats)
        .def_prop_rw("channelnames", &imagespec_get_channelnames,
                     &imagespec_set_channelnames)
        .def_rw("alpha_channel", &ImageSpec::alpha_channel)
        .def_rw("z_channel", &ImageSpec::z_channel)
        .def_rw("deep", &ImageSpec::deep)
        .def_prop_ro("extra_attribs",
                     [](const ImageSpec& spec) { return spec.extra_attribs; })
        .def_prop_rw("roi", &imagespec_get_roi, &imagespec_set_roi)
        .def_prop_rw("roi_full", &imagespec_get_roi_full,
                     &imagespec_set_roi_full)
        .def(nb::init<>())
        .def(nb::init<int, int, int, TypeDesc>())
        .def(nb::init<const ROI&, TypeDesc>())
        .def(nb::init<TypeDesc>())
        .def(nb::init<const ImageSpec&>())
        .def("copy", [](const ImageSpec& self) { return ImageSpec(self); })
        .def("set_format",
             [](ImageSpec& self, TypeDesc t) { self.set_format(t); })
        .def("default_channel_names", &ImageSpec::default_channel_names)
        .def("channel_bytes",
             [](const ImageSpec& spec) { return spec.channel_bytes(); })
        .def(
            "channel_bytes",
            [](const ImageSpec& spec, int chan, bool native) {
                return spec.channel_bytes(chan, native);
            },
            "channel"_a, "native"_a = false)
        .def(
            "pixel_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.pixel_bytes(native);
            },
            "native"_a = false)
        .def(
            "pixel_bytes",
            [](const ImageSpec& spec, int chbegin, int chend, bool native) {
                return spec.pixel_bytes(chbegin, chend, native);
            },
            "chbegin"_a, "chend"_a, "native"_a = false)
        .def(
            "scanline_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.scanline_bytes(native);
            },
            "native"_a = false)
        .def("scanline_bytes",
             [](const ImageSpec& spec, TypeDesc type) {
                 return spec.scanline_bytes(type);
             })
        .def(
            "tile_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.tile_bytes(native);
            },
            "native"_a = false)
        .def("tile_bytes", [](const ImageSpec& spec,
                              TypeDesc type) { return spec.tile_bytes(type); })
        .def(
            "image_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.image_bytes(native);
            },
            "native"_a = false)
        .def("image_bytes",
             [](const ImageSpec& spec, TypeDesc type) {
                 return spec.image_bytes(type);
             })
        .def("tile_pixels", &ImageSpec::tile_pixels)
        .def("image_pixels", &ImageSpec::image_pixels)
        .def("size_t_safe", &ImageSpec::size_t_safe)
        .def("channelformat", [](const ImageSpec& spec,
                                 int chan) { return spec.channelformat(chan); })
        .def("channel_name",
             [](const ImageSpec& spec, int chan) {
                 return std::string(spec.channel_name(chan));
             })
        .def("channelindex",
             [](const ImageSpec& spec, const std::string& name) {
                 return spec.channelindex(name);
             })
        .def("get_channelformats",
             [](const ImageSpec& spec) {
                 return imagespec_get_channelformats(spec, false);
             })
        .def("attribute",
             [](ImageSpec& spec, const std::string& name, nb::handle obj) {
                 attribute_onearg(spec, name, obj);
             })
        .def("attribute",
             [](ImageSpec& spec, const std::string& name, TypeDesc type,
                nb::handle obj) { attribute_typed(spec, name, type, obj); })
        .def(
            "get_int_attribute",
            [](const ImageSpec& spec, const std::string& name, int def) {
                return spec.get_int_attribute(name, def);
            },
            "name"_a, "defaultval"_a = 0)
        .def(
            "get_float_attribute",
            [](const ImageSpec& spec, const std::string& name, float def) {
                return spec.get_float_attribute(name, def);
            },
            "name"_a, "defaultval"_a = 0.0f)
        .def(
            "get_string_attribute",
            [](const ImageSpec& spec, const std::string& name,
               const std::string& def) {
                return std::string(spec.get_string_attribute(name, def));
            },
            "name"_a, "defaultval"_a = "")
        .def("getattribute", &imagespec_getattribute_typed, "name"_a,
             "type"_a = TypeUnknown)
        .def(
            "get",
            [](const ImageSpec& self, const std::string& key, nb::handle def) {
                ParamValue tmpparam;
                auto p = self.find_attribute(key, tmpparam);
                if (!p)
                    return nb::borrow(def);
                return make_pyobject(p->data(), p->type(), 1, def);
            },
            "key"_a, "default"_a = nb::none())
        .def(
            "erase_attribute",
            [](ImageSpec& spec, const std::string& name, TypeDesc type,
               bool casesensitive) {
                return spec.erase_attribute(name, type, casesensitive);
            },
            "name"_a = "", "type"_a = TypeUnknown, "casesensitive"_a = false)
        .def_static(
            "metadata_val",
            [](const ParamValue& p, bool human) {
                return std::string(ImageSpec::metadata_val(p, human));
            },
            "param"_a, "human"_a = false)
        .def(
            "serialize",
            [](const ImageSpec& spec, const std::string& format,
               const std::string& verbose) {
                ImageSpec::SerialFormat fmt = ImageSpec::SerialText;
                if (Strutil::iequals(format, "xml"))
                    fmt = ImageSpec::SerialXML;
                ImageSpec::SerialVerbose verb = ImageSpec::SerialDetailed;
                if (Strutil::iequals(verbose, "brief"))
                    verb = ImageSpec::SerialBrief;
                else if (Strutil::iequals(verbose, "detailed"))
                    verb = ImageSpec::SerialDetailed;
                else if (Strutil::iequals(verbose, "detailedhuman"))
                    verb = ImageSpec::SerialDetailedHuman;
                return std::string(spec.serialize(fmt, verb));
            },
            "format"_a = "text", "verbose"_a = "detailed")
        .def(
            "set_colorspace",
            [](ImageSpec& self, const std::string& cs) {
                self.set_colorspace(cs);
            },
            "name"_a)
        .def("__getitem__",
             [](const ImageSpec& self, const std::string& key) {
                 ParamValue tmpparam;
                 auto p = self.find_attribute(key, tmpparam);
                 if (p == nullptr) {
                     std::string message = "key '" + key + "' does not exist";
                     throw nb::key_error(message.c_str());
                 }
                 return make_pyobject(p->data(), p->type());
             })
        .def("__setitem__",
             [](ImageSpec& self, const std::string& key, nb::handle val) {
                 delegate_setitem(self, key, val);
             })
        .def("__delitem__",
             [](ImageSpec& self, const std::string& key) {
                 self.erase_attribute(key);
             })
        .def("__contains__", [](const ImageSpec& self, const std::string& key) {
            return self.extra_attribs.contains(key);
        });

    m.def("get_roi", &imagespec_get_roi);
    m.def("get_roi_full", &imagespec_get_roi_full);
    m.def("set_roi", &imagespec_set_roi);
    m.def("set_roi_full", &imagespec_set_roi_full);
}

}  // namespace PyOpenImageIO
