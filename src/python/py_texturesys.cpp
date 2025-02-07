// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


/// TextureOptWrap is wrapper for TextureOpt, which exists to
/// to hold the values of missingcolor, which in C++ is
/// is a user managed pointer which doesn't make sense in Python.
class TextureOptWrap : public TextureOpt {
public:
    using TextureOpt::TextureOpt;


    py::tuple get_missingcolor() const
    {
        return C_to_tuple(m_missingcolor_data.data(),
                          m_missingcolor_data.size());
    }

    void set_missingcolor(const py::object& py_m_missingcolor_data)
    {
        m_missingcolor_data.clear();
        if (py_m_missingcolor_data.is_none()) {
            this->missingcolor = nullptr;
        } else {
            py_to_stdvector(m_missingcolor_data, py_m_missingcolor_data);
            if (m_missingcolor_data.empty()) {
                this->missingcolor = nullptr;
            } else {
                this->missingcolor = m_missingcolor_data.data();
            }
        }
    }

    std::vector<float> m_missingcolor_data;
};



// Make a special wrapper to help with the weirdo way we use create/destroy.
class TextureSystemWrap {
public:
    std::shared_ptr<TextureSystem> m_texsys;


    TextureSystemWrap(bool shared = true)
        : m_texsys(TextureSystem::create(shared))
    {
    }
    TextureSystemWrap(const TextureSystemWrap&) = delete;
    TextureSystemWrap(TextureSystemWrap&&)      = delete;
    ~TextureSystemWrap() {}  // will call the deleter on the m_texsys
    static void destroy(TextureSystemWrap* x)
    {
        TextureSystem::destroy(x->m_texsys);
    }
};



void
declare_wrap(py::module& m)
{
    py::enum_<Tex::Wrap>(m, "Wrap")
        .value("Default", Tex::Wrap::Default)
        .value("Black", Tex::Wrap::Black)
        .value("Clamp", Tex::Wrap::Clamp)
        .value("Periodic", Tex::Wrap::Periodic)
        .value("Mirror", Tex::Wrap::Mirror)
        .value("PeriodicPow2", Tex::Wrap::PeriodicPow2)
        .value("PeriodicSharedBorder", Tex::Wrap::PeriodicSharedBorder)
        .value("Last", Tex::Wrap::Last);
}



void
declare_mipmpode(py::module& m)
{
    py::enum_<Tex::MipMode>(m, "MipMode")
        .value("Default", Tex::MipMode::Default)
        .value("NoMIP", Tex::MipMode::NoMIP)
        .value("OneLevel", Tex::MipMode::OneLevel)
        .value("Trilinear", Tex::MipMode::Trilinear)
        .value("Aniso", Tex::MipMode::Aniso);
}



void
declare_interpmode(py::module& m)
{
    py::enum_<Tex::InterpMode>(m, "InterpMode")
        .value("Closest", Tex::InterpMode::Closest)
        .value("Bilinear", Tex::InterpMode::Bilinear)
        .value("Bicubic", Tex::InterpMode::Bicubic)
        .value("SmartBicubic", Tex::InterpMode::SmartBicubic);
}



void
declare_textureopt(py::module& m)
{
    py::class_<TextureOptWrap>(m, "TextureOpt")
        .def(py::init<>())
        .def_readwrite("firstchannel", &TextureOptWrap::firstchannel)
        .def_readwrite("subimage", &TextureOptWrap::subimage)
        .def_property(
            "subimagename",
            [](const TextureOptWrap& texopt) {
                return std::string(texopt.subimagename);
            },
            [](TextureOptWrap& texopt, const std::string& subimagename) {
                texopt.subimagename = subimagename;
            })
        .def_property(
            "swrap",
            [](const TextureOptWrap& texopt) { return (Tex::Wrap)texopt.swrap; },
            [](TextureOptWrap& texopt, const Tex::Wrap wrap) {
                texopt.swrap = (TextureOpt::Wrap)wrap;
            })
        .def_property(
            "twrap",
            [](const TextureOptWrap& texopt) { return (Tex::Wrap)texopt.twrap; },
            [](TextureOptWrap& texopt, const Tex::Wrap wrap) {
                texopt.twrap = (TextureOpt::Wrap)wrap;
            })
        .def_property(
            "mipmode",
            [](const TextureOptWrap& texopt) {
                return (Tex::MipMode)texopt.mipmode;
            },
            [](TextureOptWrap& texopt, const Tex::MipMode mipmode) {
                texopt.mipmode = (TextureOpt::MipMode)mipmode;
            })
        .def_property(
            "interpmode",
            [](const TextureOptWrap& texopt) {
                return (Tex::InterpMode)texopt.interpmode;
            },
            [](TextureOptWrap& texopt, const Tex::InterpMode interp) {
                texopt.interpmode = (TextureOpt::InterpMode)interp;
            })
        .def_readwrite("anisotropic", &TextureOptWrap::anisotropic)
        .def_readwrite("conservative_filter",
                       &TextureOptWrap::conservative_filter)
        .def_readwrite("sblur", &TextureOptWrap::sblur)
        .def_readwrite("tblur", &TextureOptWrap::tblur)
        .def_readwrite("swidth", &TextureOptWrap::swidth)
        .def_readwrite("twidth", &TextureOptWrap::twidth)
        .def_readwrite("fill", &TextureOptWrap::fill)
        .def_property("missingcolor", &TextureOptWrap::get_missingcolor,
                      &TextureOptWrap::set_missingcolor)
        .def_readwrite("rnd", &TextureOptWrap::rnd)
        .def_property(
            "rwrap",
            [](const TextureOptWrap& texopt) { return (Tex::Wrap)texopt.rwrap; },
            [](TextureOptWrap& texopt, const Tex::Wrap wrap) {
                texopt.rwrap = (TextureOpt::Wrap)wrap;
            })
        .def_readwrite("rwidth", &TextureOptWrap::rwidth);
}



void
declare_texturesystem(py::module& m)
{
    using namespace pybind11::literals;

    py::class_<TextureSystemWrap>(m, "TextureSystem")
        .def(py::init<bool>(), "shared"_a = true)
        .def_static("destroy", &TextureSystemWrap::destroy)

        .def("attribute",
             [](TextureSystemWrap& ts, const std::string& name, float val) {
                 if (ts.m_texsys)
                     ts.m_texsys->attribute(name, val);
             })
        .def("attribute",
             [](TextureSystemWrap& ts, const std::string& name, int val) {
                 if (ts.m_texsys)
                     ts.m_texsys->attribute(name, val);
             })
        .def("attribute",
             [](TextureSystemWrap& ts, const std::string& name,
                const std::string& val) {
                 if (ts.m_texsys)
                     ts.m_texsys->attribute(name, val);
             })
        .def("attribute",
             [](TextureSystemWrap& ts, const std::string& name, TypeDesc type,
                const py::object& obj) {
                 if (ts.m_texsys)
                     attribute_typed(*ts.m_texsys, name, type, obj);
             })
        .def(
            "getattributetype",
            [](const TextureSystemWrap& ts, const std::string& name) {
                return ts.m_texsys->getattributetype(name);
            },
            "name"_a)
        .def(
            "getattribute",
            [](const TextureSystemWrap& ts, const std::string& name,
               TypeDesc type) {
                if (type == TypeUnknown)
                    type = ts.m_texsys->getattributetype(name);
                return getattribute_typed(*ts.m_texsys, name, type);
            },
            "name"_a, "type"_a = TypeUnknown)

        .def(
            "texture",
            [](const TextureSystemWrap& ts, const std::string& filename,
               TextureOptWrap& options, float s, float t, float dsdx,
               float dtdx, float dsdy, float dtdy, int nchannels) {
                if (!ts.m_texsys || nchannels < 1) {
                    return py::tuple();
                }
                float* result = OIIO_ALLOCA(float, nchannels);
                {
                    py::gil_scoped_release gil;
                    ts.m_texsys->texture(ustring(filename), options, s, t, dsdx,
                                         dtdx, dsdy, dtdy, nchannels, result);
                }
                return C_to_tuple(result, nchannels);
            },
            "filename"_a, "options"_a, "s"_a, "t"_a, "dsdx"_a, "dtdx"_a,
            "dsdy"_a, "dtdy"_a, "nchannels"_a)


        .def(
            "texture3d",
            [](const TextureSystemWrap& ts, const std::string& filename,
               TextureOptWrap& options, const std::array<float, 3> P,
               const std::array<float, 3> dPdx, const std::array<float, 3> dPdy,
               const std::array<float, 3> dPdz, int nchannels) {
                if (!ts.m_texsys || nchannels < 1) {
                    return py::tuple();
                }
                float* result = OIIO_ALLOCA(float, nchannels);
                {
                    py::gil_scoped_release gil;
                    ts.m_texsys->texture3d(
                        ustring(filename), options,
                        Imath::V3f(P[0], P[1], P[2]),
                        Imath::V3f(dPdx[0], dPdx[1], dPdx[2]),
                        Imath::V3f(dPdy[0], dPdy[1], dPdy[2]),
                        Imath::V3f(dPdz[0], dPdz[1], dPdz[2]), nchannels,
                        result);
                }
                return C_to_tuple(result, nchannels);
            },
            "filename"_a, "options"_a, "P"_a, "dPdx"_a, "dPdy"_a, "dPdz"_a,
            "nchannels"_a)

        .def(
            "environment",
            [](const TextureSystemWrap& ts, const std::string& filename,
               TextureOptWrap& options, const std::array<float, 3> R,
               const std::array<float, 3> dRdx, const std::array<float, 3> dRdy,
               int nchannels) {
                if (!ts.m_texsys || nchannels < 1) {
                    return py::tuple();
                }

                float* result = OIIO_ALLOCA(float, nchannels);
                {
                    py::gil_scoped_release gil;
                    ts.m_texsys->environment(ustring(filename), options,
                                             Imath::V3f(R[0], R[1], R[2]),
                                             Imath::V3f(dRdx[0], dRdx[1],
                                                        dRdx[2]),
                                             Imath::V3f(dRdy[0], dRdy[1],
                                                        dRdy[2]),
                                             nchannels, result);
                }
                return C_to_tuple(result, nchannels);
            },
            "filename"_a, "options"_a, "R"_a, "dRdx"_a, "dRdy"_a, "nchannels"_a)

        .def(
            "resolve_filename",
            [](TextureSystemWrap& ts, const std::string& filename) {
                py::gil_scoped_release gil;
                return ts.m_texsys->resolve_filename(filename);
            },
            "filename"_a)
        .def(
            "imagespec",
            [](TextureSystemWrap& ts, const std::string& filename,
               int subimage) -> py::object {
                py::gil_scoped_release gil;
                const ImageSpec* spec
                    = ts.m_texsys->imagespec(ustring(filename), subimage);
                if (!spec) {
                    return py::none();
                }
                return py::object(py::cast(*spec));
            },
            "filename"_a, "subimage"_a = 0)
        .def(
            "is_udim",
            [](TextureSystemWrap& ts, const std::string& filename) {
                return ts.m_texsys->is_udim(ustring(filename));
            },
            "filename"_a)
        .def(
            "resolve_udim",
            [](TextureSystemWrap& ts, const std::string& filename, float s,
               float t) -> std::string {
                auto th = ts.m_texsys->resolve_udim(ustring(filename), s, t);
                return th ? ts.m_texsys->filename_from_handle(th).string()
                          : std::string();
            },
            "filename"_a, "s"_a, "t"_a)
        .def(
            "inventory_udim",
            [](TextureSystemWrap& ts, const std::string& filename) {
                // Return a tuple containing (nutiles, nvtiles, filenames)
                int nutiles = 0, nvtiles = 0;
                std::vector<ustring> filenames;
                ts.m_texsys->inventory_udim(ustring(filename), filenames,
                                            nutiles, nvtiles);
                std::vector<PY_STR> strs;
                for (auto f : filenames)
                    strs.emplace_back(f.string());
                py::tuple ret = py::make_tuple(nutiles, nvtiles, strs);
                return ret;
            },
            "filename"_a)
        .def(
            "invalidate",
            [](TextureSystemWrap& ts, const std::string& filename, bool force) {
                py::gil_scoped_release gil;
                ts.m_texsys->invalidate(ustring(filename), force);
            },
            "filename"_a, "force"_a = true)
        .def(
            "invalidate_all",
            [](TextureSystemWrap& ts, bool force) {
                py::gil_scoped_release gil;
                ts.m_texsys->invalidate_all(force);
            },
            "force"_a = true)
        .def(
            "close",
            [](TextureSystemWrap& ts, const std::string& filename) {
                py::gil_scoped_release gil;
                ts.m_texsys->close(ustring(filename));
            },
            "filename"_a)
        .def("close_all",
             [](TextureSystemWrap& ts) {
                 py::gil_scoped_release gil;
                 ts.m_texsys->close_all();
             })
        .def("has_error",
             [](TextureSystemWrap& ts) { return ts.m_texsys->has_error(); })
        .def(
            "geterror",
            [](TextureSystemWrap& ts, bool clear) {
                return ts.m_texsys->geterror(clear);
            },
            "clear"_a = true)
        .def(
            "getstats",
            [](TextureSystemWrap& ts, int level, bool icstats) {
                return ts.m_texsys->getstats(level, icstats);
            },
            "level"_a = 1, "icstats"_a = true)
        .def("reset_stats",
             [](TextureSystemWrap& ts) { return ts.m_texsys->reset_stats(); })

        ;
}

}  // namespace PyOpenImageIO
