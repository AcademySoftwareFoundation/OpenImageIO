// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {


static ParamValue
ParamValue_from_pyobject(string_view name, TypeDesc type, int nvalues,
                         ParamValue::Interp interp, const py::object& obj)
{
    size_t expected_size = size_t(type.numelements() * type.aggregate
                                  * nvalues);
    ParamValue pv;
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, &vals[0]);
            return pv;
        }
    } else if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, &vals[0]);
            return pv;
        }
    } else if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, &vals[0]);
            return pv;
        }
    } else if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() >= expected_size) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            pv.init(name, type, nvalues, interp, &u[0]);
            return pv;
        }
    } else if (type.basetype == TypeDesc::UINT8 && type.arraylen
               && py::isinstance<py::bytes>(obj)) {
        // Special case: converting a "bytes" object to a byte array
        std::string s = obj.cast<py::bytes>();
        if (type.arraylen < 0)  // convert un-specified length to real length
            type.arraylen = int(s.size()) / nvalues;
        if (type.arraylen * nvalues == int(s.size())) {
            std::vector<uint8_t> vals((const uint8_t*)s.data(),
                                      (const uint8_t*)s.data() + s.size());
            pv.init(name, type, nvalues, interp, vals.data());
            return pv;
        }
    } else if (type.basetype == TypeDesc::UINT8) {
        std::vector<uint8_t> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, vals.data());
            return pv;
        }
    } else {
        Strutil::print("ParamValue_from_pyobject not sure how to handle {} {}\n",
                       name, type);
    }

    // I think this is what we should do here when not enough data is
    // provided, but I get crashes when I do. Maybe pybind11 bug?
    //
    // throw std::length_error("ParamValue data length mismatch");

    return pv;
}



// Based on attribute_typed in py_oiio.h, but with nvalues.
template<typename T, typename POBJ>
bool
attribute_typed(T& myobj, string_view name, TypeDesc type, int nvalues,
                const POBJ& dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, &vals[0]);
        return ok;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        bool ok = py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute(name, type, nvalues, &u[0]);
        }
        return ok;
    }
    return false;
}



void
declare_paramvalue(py::module& m)
{
    using namespace pybind11::literals;

    py::enum_<ParamValue::Interp>(m, "Interp")
        .value("CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("LINEAR", ParamValue::INTERP_LINEAR)
        .value("VERTEX", ParamValue::INTERP_VERTEX)
        // synonyms that more close to the C++ names
        .value("INTERP_CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("INTERP_PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("INTERP_LINEAR", ParamValue::INTERP_LINEAR)
        .value("INTERP_VERTEX", ParamValue::INTERP_VERTEX);

    py::class_<ParamValue>(m, "ParamValue")
        .def_property_readonly("name",
                               [](const ParamValue& self) {
                                   return PY_STR(self.name().string());
                               })
        .def_property_readonly("type",
                               [](const ParamValue& self) {
                                   return self.type();
                               })
        .def_property_readonly("value",
                               [](const ParamValue& self) {
                                   return make_pyobject(self.data(),
                                                        self.type(),
                                                        self.nvalues());
                               })
        .def_property_readonly("__len__", &ParamValue::nvalues)
        .def(py::init<const std::string&, int>())
        .def(py::init<const std::string&, float>())
        .def(py::init<const std::string&, const std::string&>())
        .def(py::init([](const std::string& name, TypeDesc type,
                         const py::object& obj) {
                 return ParamValue_from_pyobject(name, type, 1,
                                                 ParamValue::INTERP_CONSTANT,
                                                 obj);
             }),
             "name"_a, "type"_a, "value"_a)
        .def(py::init([](const std::string& name, TypeDesc type, int nvalues,
                         ParamValue::Interp interp, const py::object& obj) {
                 return ParamValue_from_pyobject(name, type, nvalues, interp,
                                                 obj);
             }),
             "name"_a, "type"_a, "nvalues"_a, "interp"_a, "value"_a);

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
        // __getitem__ is the dict-like `pvl[key]` lookup
        .def(
            "__getitem__",
            [](const ParamValueList& self, const std::string& key) {
                auto p = self.find(key);
                if (p == self.end())
                    throw py::key_error("key '" + key + "' does not exist");
                return make_pyobject(p->data(), p->type());
            },
            py::return_value_policy::reference_internal)
        // __setitem__ is the dict-like `pvl[key] = value` assignment
        .def("__setitem__",
             [](ParamValueList& self, const std::string& key, py::object val) {
                 delegate_setitem(self, key, val);
             })
        // __delitem__ is the dict-like `del pvl[key]`
        .def("__delitem__", [](ParamValueList& self,
                               const std::string& key) { self.remove(key); })
        // __contains__ is the dict-like `key in pvl`
        .def("__contains__",
             [](const ParamValueList& self, const std::string& key) {
                 return self.contains(key);
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
        .def("merge", &ParamValueList::merge, "other"_a, "override"_a = false)
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
        .def("attribute",
             [](ParamValueList& self, const std::string& name, TypeDesc type,
                const py::object& obj) {
                 attribute_typed(self, name, type, obj);
             })
        .def("attribute",
             [](ParamValueList& self, const std::string& name, TypeDesc type,
                int nvalues, const py::object& obj) {
                 attribute_typed(self, name, type, nvalues, obj);
             });
}

}  // namespace PyOpenImageIO
