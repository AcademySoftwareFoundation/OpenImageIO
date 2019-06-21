// OpenImageIO Copyright 2008- Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include "py_oiio.h"

namespace PyOpenImageIO {


void
declare_paramvalue(py::module& m)
{
    using namespace pybind11::literals;

    py::enum_<ParamValue::Interp>(m, "Interp")
        .value("INTERP_CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("INTERP_PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("INTERP_LINEAR", ParamValue::INTERP_LINEAR)
        .value("INTERP_VERTEX", ParamValue::INTERP_VERTEX);

    py::class_<ParamValue>(m, "ParamValue")
        .def_property_readonly("name",
                               [](const ParamValue& p) {
                                   return PY_STR(p.name().string());
                               })
        .def_property_readonly("type",
                               [](const ParamValue& p) {
                                   return PY_STR(p.type().c_str());
                               })
        .def_property_readonly("value",
                               [](const ParamValue& p) {
                                   return ParamValue_getitem(p, 0);
                               })
        // .def("__getitem__",       &ParamValue_getitem)
        .def_property_readonly("__len__", &ParamValue::nvalues)
        .def(py::init<const std::string&, int>())
        .def(py::init<const std::string&, float>())
        .def(py::init<const std::string&, const std::string&>());

    py::class_<ParamValueList>(m, "ParamValueList")
        .def(py::init<>())
        .def(
            "__getitem__",
            [](const ParamValueList& self, size_t i) {
                if (i >= self.size())
                    throw py::index_error();
                return self[i];
            },
            py::return_value_policy::reference_internal)
        .def("__getitem__",
             [](const ParamValueList& self, const std::string& key) {
                 auto p = self.find(key);
                 if (p == self.end())
                     throw py::key_error("key '" + key + "' does not exist");
                 return ParamValue_getitem(*p);
             })
        .def("__setitem__",
             [](ParamValueList& self, const std::string& key, py::object val) {
                 delegate_setitem(self, key, val);
             })
        .def("__len__", [](const ParamValueList& p) { return p.size(); })
        .def(
            "__iter__",
            [](const ParamValueList& self) {
                return py::make_iterator(self.begin(), self.end());
            },
            py::keep_alive<0, 1>())
        .def("append", [](ParamValueList& p,
                          const ParamValue& v) { return p.push_back(v); })
        .def("clear", &ParamValueList::clear)
        .def("free", &ParamValueList::free)
        .def("resize", [](ParamValueList& p, size_t s) { return p.resize(s); })
        .def(
            "remove",
            [](ParamValueList& p, const std::string& name, TypeDesc type,
               bool casesensitive) { p.remove(name, type, casesensitive); },
            "name"_a, "type"_a = TypeUnknown, "casesensitive"_a = true)
        .def(
            "contains",
            [](ParamValueList& p, const std::string& name, TypeDesc type,
               bool casesensitive) {
                return p.contains(name, type, casesensitive);
            },
            "name"_a, "type"_a = TypeUnknown, "casesensitive"_a = true)
        .def(
            "add_or_replace",
            [](ParamValueList& p, const ParamValue& pv, bool casesensitive) {
                return p.add_or_replace(pv, casesensitive);
            },
            "value"_a, "casesensitive"_a = true)
        .def("sort", &ParamValueList::sort, "casesensitive"_a = true)
        .def("attribute",
             [](ParamValueList& self, const std::string& name, float val) {
                 self.attribute(name, TypeFloat, &val);
             })
        .def("attribute", [](ParamValueList& self, const std::string& name,
                             int val) { self.attribute(name, TypeInt, &val); })
        .def("attribute",
             [](ParamValueList& self, const std::string& name,
                const std::string& val) {
                 const char* s = val.c_str();
                 self.attribute(name, TypeString, &s);
             })
        .def("attribute", [](ParamValueList& self, const std::string& name,
                             TypeDesc type, const py::tuple& obj) {
            attribute_typed(self, name, type, obj);
        });
}

}  // namespace PyOpenImageIO
