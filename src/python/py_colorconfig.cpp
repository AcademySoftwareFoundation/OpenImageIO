// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"
#include <OpenImageIO/color.h>
#include <OpenImageIO/color_interop_ids.h>
#include <cctype>
#include <optional>
#include <utility>

#include <pybind11/eval.h>

namespace PyOpenImageIO {


// Declare the OIIO ColorConfig class to Python
void
declare_colorconfig(py::module& m)
{
    using namespace pybind11::literals;

    py::class_<ColorConfig>(m, "ColorConfig")

        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("geterror",
             [](ColorConfig& self) { return PY_STR(self.geterror()); })

        .def("getNumColorSpaces", &ColorConfig::getNumColorSpaces)
        .def("getColorSpaceNames", &ColorConfig::getColorSpaceNames)
        .def("getColorSpaceNameByIndex", &ColorConfig::getColorSpaceNameByIndex)
        .def(
            "getColorSpaceIndex",
            [](const ColorConfig& self, const std::string& name) {
                return self.getColorSpaceIndex(name);
            },
            "name"_a)
        .def(
            "getColorSpaceNameByRole",
            [](const ColorConfig& self, const std::string& role) {
                return self.getColorSpaceNameByRole(role);
            },
            "role"_a)
        .def("getNumRoles", &ColorConfig::getNumRoles)
        .def("getRoleByIndex", &ColorConfig::getRoleByIndex)
        .def("getRoles", &ColorConfig::getRoles)
        .def(
            "getColorSpaceDataType",
            [](const ColorConfig& self, const std::string& name) {
                int bits      = 0;
                TypeDesc type = self.getColorSpaceDataType(name, &bits);
                return std::make_pair(type, bits);
            },
            "name"_a)
        .def(
            "getColorSpaceFamilyByName",
            [](const ColorConfig& self, const std::string& name) {
                return self.getColorSpaceFamilyByName(name);
            },
            "name"_a)

        .def("getNumLooks", &ColorConfig::getNumLooks)
        .def("getLookNameByIndex", &ColorConfig::getLookNameByIndex)
        .def("getLookNames", &ColorConfig::getLookNames)

        .def("getNumDisplays", &ColorConfig::getNumDisplays)
        .def("getDisplayNameByIndex", &ColorConfig::getDisplayNameByIndex)
        .def("getDisplayNames", &ColorConfig::getDisplayNames)
        .def("getDefaultDisplayName", &ColorConfig::getDefaultDisplayName)

        .def(
            "getNumViews",
            [](const ColorConfig& self, const std::string& display) {
                return self.getNumViews(display);
            },
            "display"_a = "")
        .def(
            "getViewNameByIndex",
            [](const ColorConfig& self, const std::string& display, int index) {
                return self.getViewNameByIndex(display, index);
            },
            "display"_a = "", "index"_a)
        .def(
            "getViewNames",
            [](const ColorConfig& self, const std::string& display) {
                return self.getViewNames(display);
            },
            "display"_a = "")
        .def(
            "getDefaultViewName",
            [](const ColorConfig& self, const std::string& display) {
                return self.getDefaultViewName(display);
            },
            "display"_a = "")
        .def(
            "getDefaultViewName",
            [](const ColorConfig& self, const std::string& display,
               const std::string& input_color_space) {
                return self.getDefaultViewName(display, input_color_space);
            },
            "display"_a = "", "input_color_space"_a)
        .def(
            "getDisplayViewColorSpaceName",
            [](const ColorConfig& self, const std::string& display,
               const std::string& view) {
                return self.getDisplayViewColorSpaceName(display, view);
            },
            "display"_a, "view"_a)
        .def(
            "getDisplayViewLooks",
            [](const ColorConfig& self, const std::string& display,
               const std::string& view) {
                return self.getDisplayViewLooks(display, view);
            },
            "display"_a, "view"_a)

        .def("getAliases",
             [](const ColorConfig& self, const std::string& color_space) {
                 return self.getAliases(color_space);
             })
        .def("getNumNamedTransforms", &ColorConfig::getNumNamedTransforms)
        .def("getNamedTransformNameByIndex",
             &ColorConfig::getNamedTransformNameByIndex)
        .def("getNamedTransformNames", &ColorConfig::getNamedTransformNames)
        .def("getNamedTransformAliases",
             [](const ColorConfig& self, const std::string& named_transform) {
                 return self.getNamedTransformAliases(named_transform);
             })
        .def(
            "isColorSpaceLinear",
            [](const ColorConfig& self, const std::string& name) {
                return self.isColorSpaceLinear(name);
            },
            "name"_a)
        .def(
            "isData",
            [](const ColorConfig& self, const std::string& name) {
                return self.isData(name);
            },
            "name"_a)
        .def(
            "getColorSpaceFromFilepath",
            [](const ColorConfig& self, const std::string& filepath) {
                return std::string(self.getColorSpaceFromFilepath(filepath));
            },
            "filepath"_a)
        .def(
            "getColorSpaceFromFilepath",
            [](const ColorConfig& self, const std::string& filepath,
               const std::string& default_cs, const bool& cs_name_match) {
                return std::string(
                    self.getColorSpaceFromFilepath(filepath, default_cs,
                                                   cs_name_match));
            },
            "filepath"_a, "default_cs"_a, "cs_name_match"_a = false)
        .def(
            "filepathOnlyMatchesDefaultRule",
            [](const ColorConfig& self, const std::string& filepath) {
                return self.filepathOnlyMatchesDefaultRule(filepath);
            },
            "filepath"_a)
        .def("parseColorSpaceFromString",
             [](const ColorConfig& self, const std::string& str) {
                 return std::string(self.parseColorSpaceFromString(str));
             })
        .def(
            "resolve",
            [](const ColorConfig& self, const std::string& name) {
                return std::string(self.resolve(name));
            },
            "name"_a)
        .def(
            "equivalent",
            [](const ColorConfig& self, const std::string& color_space,
               const std::string& other_color_space) {
                return self.equivalent(color_space, other_color_space);
            },
            "color_space"_a, "other_color_space"_a)
        .def("get_color_interop_id",
             [](const ColorConfig& self, const std::string& colorspace) {
                 return std::string(self.get_color_interop_id(colorspace));
             })
        .def("get_color_interop_id",
             [](const ColorConfig& self, const std::array<int, 4> cicp) {
                 return std::string(self.get_color_interop_id(cicp.data()));
             })
        .def("get_cicp",
             [](const ColorConfig& self, const std::string& colorspace)
                 -> std::optional<std::array<int, 4>> {
                 cspan<int> cicp = self.get_cicp(colorspace);
                 if (!cicp.empty()) {
                     return std::array<int, 4>(
                         { cicp[0], cicp[1], cicp[2], cicp[3] });
                 }
                 return std::nullopt;
             })
        .def("configname", &ColorConfig::configname)
        .def_static("default_colorconfig", []() -> const ColorConfig& {
            return ColorConfig::default_colorconfig();
        });

    m.attr("supportsOpenColorIO")     = ColorConfig::supportsOpenColorIO();
    m.attr("OpenColorIO_version_hex") = ColorConfig::OpenColorIO_version_hex();

    // ColorInteropID: a Python str enum (ADR-0017 in the color-interop hub)
    // of every canonical Color Interop Forum id OIIO's built-in interop
    // identities registry declares -- the same set as the generated C++
    // OIIO::ColorInteropIDs::* constants (color_interop_ids.h), one member
    // per id. Defined via a `class ColorInteropID(str, enum.Enum)` source
    // string rather than enum's functional API so it can override __str__ to
    // return the plain value (matching stdlib enum.StrEnum's behavior, which
    // isn't available before Python 3.11 -- this project's floor is 3.9):
    // members compare equal to, print as, and are accepted anywhere as plain
    // str, so every existing string-taking API above (including
    // get_color_interop_id) takes a member unchanged.
    {
        std::string src = "import enum\n"
                          "class ColorInteropID(str, enum.Enum):\n";
        for (string_view id : ColorInteropIDs::all) {
            std::string name(id);
            for (char& c : name)
                c = (c == ':') ? '_' : char(std::toupper((unsigned char)c));
            src += "    " + name + " = \"" + std::string(id) + "\"\n";
        }
        src += "    def __str__(self):\n"
               "        return self.value\n";
        py::dict ns;
        py::exec(src, py::globals(), ns);
        m.attr("ColorInteropID") = ns["ColorInteropID"];
    }
}

}  // namespace PyOpenImageIO
