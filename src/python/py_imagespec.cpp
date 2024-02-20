// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


// Accessor for channelformats, converts a vector<TypeDesc> to a tuple.
static py::tuple
ImageSpec_get_channelformats(const ImageSpec& spec, bool allow_empty = true)
{
    std::vector<TypeDesc> formats;
    if (spec.channelformats.size() || !allow_empty)
        spec.get_channelformats(formats);
    return C_to_tuple(cspan<TypeDesc>(formats));
}

// Mutator for channelformats, initialized using a tuple or list whose int
// entries give the BASETYPE values.
static void
ImageSpec_set_channelformats(ImageSpec& spec,
                             const py::object& py_channelformats)
{
    spec.channelformats.clear();
    py_to_stdvector(spec.channelformats, py_channelformats);
}


static py::tuple
ImageSpec_get_channelnames(const ImageSpec& spec)
{
    return C_to_tuple(cspan<std::string>(spec.channelnames));
}

static void
ImageSpec_set_channelnames(ImageSpec& spec, const py::object& py_channelnames)
{
    spec.channelnames.clear();
    py_to_stdvector(spec.channelnames, py_channelnames);
}


static py::object
ImageSpec_getattribute_typed(const ImageSpec& spec, const std::string& name,
                             TypeDesc type = TypeUnknown)
{
    ParamValue tmpparam;
    const ParamValue* p = spec.find_attribute(name, tmpparam, type);
    if (!p)
        return py::none();
    return make_pyobject(p->data(), p->type(), p->nvalues());
}



void
declare_imagespec(py::module& m)
{
    using namespace pybind11::literals;

    py::class_<ImageSpec>(m, "ImageSpec")
        .def_readwrite("x", &ImageSpec::x)
        .def_readwrite("y", &ImageSpec::y)
        .def_readwrite("z", &ImageSpec::z)
        .def_readwrite("width", &ImageSpec::width)
        .def_readwrite("height", &ImageSpec::height)
        .def_readwrite("depth", &ImageSpec::depth)
        .def_readwrite("full_x", &ImageSpec::full_x)
        .def_readwrite("full_y", &ImageSpec::full_y)
        .def_readwrite("full_z", &ImageSpec::full_z)
        .def_readwrite("full_width", &ImageSpec::full_width)
        .def_readwrite("full_height", &ImageSpec::full_height)
        .def_readwrite("full_depth", &ImageSpec::full_depth)
        .def_readwrite("tile_width", &ImageSpec::tile_width)
        .def_readwrite("tile_height", &ImageSpec::tile_height)
        .def_readwrite("tile_depth", &ImageSpec::tile_depth)
        .def_readwrite("nchannels", &ImageSpec::nchannels)
        .def_readwrite("format", &ImageSpec::format)
        .def_property(
            "channelformats",
            [](const ImageSpec& spec) {
                return ImageSpec_get_channelformats(spec);
            },
            &ImageSpec_set_channelformats)
        .def_property("channelnames", &ImageSpec_get_channelnames,
                      &ImageSpec_set_channelnames)
        .def_readwrite("alpha_channel", &ImageSpec::alpha_channel)
        .def_readwrite("z_channel", &ImageSpec::z_channel)
        .def_readwrite("deep", &ImageSpec::deep)
        .def_readwrite("extra_attribs", &ImageSpec::extra_attribs)

        .def_property("roi", &ImageSpec::roi, &ImageSpec::set_roi)
        .def_property("roi_full", &ImageSpec::roi_full,
                      &ImageSpec::set_roi_full)

        .def(py::init<>())
        .def(py::init<int, int, int, TypeDesc>())
        .def(py::init<const ROI&, TypeDesc>())
        .def(py::init<TypeDesc>())
        .def(py::init<const ImageSpec&>())
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
        // .def("pixel_bytes",
        //      [](const ImageSpec &spec){ return spec.pixel_bytes(); })
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
        // .def("scanline_bytes",
        //      [](const ImageSpec &spec){ return spec.scanline_bytes(); })
        .def(
            "scanline_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.scanline_bytes(native);
            },
            "native"_a = false)
        // .def("tile_bytes",
        //      [](const ImageSpec &spec){ return spec.tile_bytes(); })
        .def(
            "tile_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.tile_bytes(native);
            },
            "native"_a = false)
        // .def("image_bytes",
        //      [](const ImageSpec &spec){ return spec.image_bytes(); })
        .def(
            "image_bytes",
            [](const ImageSpec& spec, bool native) {
                return spec.image_bytes(native);
            },
            "native"_a = false)
        .def("tile_pixels", &ImageSpec::tile_pixels)
        .def("image_pixels", &ImageSpec::image_pixels)
        .def("size_t_safe", &ImageSpec::size_t_safe)
        .def("channelformat", [](const ImageSpec& spec,
                                 int chan) { return spec.channelformat(chan); })
        .def("channel_name",
             [](const ImageSpec& spec, int chan) {
                 return PY_STR(std::string(spec.channel_name(chan)));
             })
        .def("channelindex",
             [](const ImageSpec& spec, const std::string& name) {
                 return spec.channelindex(name);
             })
        .def("get_channelformats",
             [](const ImageSpec& spec) {
                 return ImageSpec_get_channelformats(spec, false);
             })

        // For now, do not expose auto_stride.  It's not obvious that
        // anybody will want to do pointer work and strides from Python.

        .def("attribute", [](ImageSpec& spec, const std::string& name,
                             float val) { spec.attribute(name, val); })
        .def("attribute", [](ImageSpec& spec, const std::string& name,
                             int val) { spec.attribute(name, val); })
        .def("attribute",
             [](ImageSpec& spec, const std::string& name,
                const std::string& val) { spec.attribute(name, val); })
        .def("attribute",
             [](ImageSpec& spec, const std::string& name, TypeDesc type,
                const py::object& obj) {
                 attribute_typed(spec, name, type, obj);
             })
        // .def("attribute", [](ImageSpec &spec, const std::string &name, TypeDesc type, const py::list &obj) {
        //         attribute_typed (spec, name, type, obj);
        //     })
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
                return PY_STR(
                    std::string(spec.get_string_attribute(name, def)));
            },
            "name"_a, "defaultval"_a = "")
        .def(
            "get_bytes_attribute",
            [](const ImageSpec& spec, const std::string& name,
               const std::string& def) {
                return py::bytes(
                    std::string(spec.get_string_attribute(name, def)));
            },
            "name"_a, "defaultval"_a = "")
        .def("getattribute", &ImageSpec_getattribute_typed, "name"_a,
             "type"_a = TypeUnknown)
        .def(
            "get",
            [](const ImageSpec& self, const std::string& key, py::object def) {
                ParamValue tmpparam;
                auto p = self.find_attribute(key, tmpparam);
                return p ? make_pyobject(p->data(), p->type(), 1, def) : def;
            },
            "key"_a, "default"_a = py::none())
        .def(
            "erase_attribute",
            [](ImageSpec& spec, const std::string& name, TypeDesc type,
               bool casesensitive = false) {
                return spec.erase_attribute(name, type, casesensitive);
            },
            "name"_a = "", "type"_a = TypeUnknown, "casesensitive"_a = false)

        .def_static(
            "metadata_val",
            [](const ParamValue& p, bool human) {
                return PY_STR(ImageSpec::metadata_val(p, human));
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
                return PY_STR(spec.serialize(fmt, verb));
            },
            "format"_a = "text", "verbose"_a = "detailed")
        .def("to_xml",
             [](const ImageSpec& spec) { return PY_STR(spec.to_xml()); })
        .def("from_xml", &ImageSpec::from_xml)
        .def("valid_tile_range", &ImageSpec::valid_tile_range, "xbegin"_a,
             "xend"_a, "ybegin"_a, "yend"_a, "zbegin"_a, "zend"_a)
        .def("copy_dimensions", &ImageSpec::copy_dimensions, "other"_a)
        .def(
            "set_colorspace",
            [](ImageSpec& self, const std::string& cs) {
                self.set_colorspace(cs);
            },
            "name"_a)
        // __getitem__ is the dict-like `ImageSpec[key]` lookup
        .def("__getitem__",
             [](const ImageSpec& self, const std::string& key) {
                 ParamValue tmpparam;
                 auto p = self.find_attribute(key, tmpparam);
                 if (p == nullptr)
                     throw py::key_error("key '" + key + "' does not exist");
                 return make_pyobject(p->data(), p->type());
             })
        // __setitem__ is the dict-like `ImageSpec[key] = value` assignment
        .def("__setitem__",
             [](ImageSpec& self, const std::string& key, py::object val) {
                 delegate_setitem(self, key, val);
             })
        // __delitem__ is the dict-like `del ImageSpec[key]`
        .def("__delitem__",
             [](ImageSpec& self, const std::string& key) {
                 self.erase_attribute(key);
             })
        // __contains__ is the dict-like `key in ImageSpec`
        .def("__contains__", [](const ImageSpec& self, const std::string& key) {
            return self.extra_attribs.contains(key);
        });
}

}  // namespace PyOpenImageIO
