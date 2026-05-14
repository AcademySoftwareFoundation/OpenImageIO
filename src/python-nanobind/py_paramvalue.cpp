// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

using namespace OIIO;

template<typename T, typename Obj>
bool
attribute_typed_nvalues(T& myobj, string_view name, TypeDesc type, int nvalues,
                        const Obj& dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        bool ok = PyOpenImageIO::py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        bool ok = PyOpenImageIO::py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        bool ok = PyOpenImageIO::py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, vals.data());
        return ok;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        bool ok = PyOpenImageIO::py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok) {
            std::vector<ustring> converted;
            converted.reserve(vals.size());
            for (auto& val : vals)
                converted.emplace_back(val);
            myobj.attribute(name, type, nvalues, converted.data());
        }
        return ok;
    }
    if (type.basetype == TypeDesc::UINT8) {
        std::vector<uint8_t> vals;
        bool ok = PyOpenImageIO::py_to_stdvector(vals, dataobj);
        ok &= (vals.size() == type.numelements() * type.aggregate * nvalues);
        if (ok)
            myobj.attribute(name, type, nvalues, vals.data());
        return ok;
    }
    return false;
}

ParamValue
paramvalue_from_pyobject(string_view name, TypeDesc type, int nvalues,
                         ParamValue::Interp interp, nb::handle obj)
{
    ParamValue pv;
    // Unsized uint8[] + bytes infers arraylen — must run before numelements().
    if (type.basetype == TypeDesc::UINT8 && type.arraylen
        && nb::isinstance<nb::bytes>(obj)) {
        TypeDesc t  = type;
        nb::bytes b = nb::cast<nb::bytes>(obj);
        const std::string s(b.c_str(), b.size());
        if (t.arraylen < 0)
            t.arraylen = static_cast<int>(s.size()) / nvalues;
        if (t.arraylen * nvalues == static_cast<int>(s.size())) {
            std::vector<uint8_t> vals(reinterpret_cast<const uint8_t*>(s.data()),
                                      reinterpret_cast<const uint8_t*>(s.data())
                                          + s.size());
            pv.init(name, t, nvalues, interp, vals.data());
        }
        return pv;
    }

    const size_t expected_size = static_cast<size_t>(
        type.numelements() * type.aggregate * nvalues);
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        if (PyOpenImageIO::py_to_stdvector(vals, obj)
            && vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, vals.data());
        }
    } else if (type.basetype == TypeDesc::UINT) {
        std::vector<unsigned int> vals;
        if (PyOpenImageIO::py_to_stdvector(vals, obj)
            && vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, vals.data());
        }
    } else if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        if (PyOpenImageIO::py_to_stdvector(vals, obj)
            && vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, vals.data());
        }
    } else if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        if (PyOpenImageIO::py_to_stdvector(vals, obj)
            && vals.size() >= expected_size) {
            std::vector<ustring> converted;
            converted.reserve(vals.size());
            for (auto& val : vals)
                converted.emplace_back(val);
            pv.init(name, type, nvalues, interp, converted.data());
        }
    } else if (type.basetype == TypeDesc::UINT8) {
        std::vector<uint8_t> vals;
        if (PyOpenImageIO::py_to_stdvector(vals, obj)
            && vals.size() >= expected_size) {
            pv.init(name, type, nvalues, interp, vals.data());
        }
    }
    return pv;
}

}  // namespace


namespace PyOpenImageIO {

void
declare_paramvalue(nb::module_& m)
{
    nb::enum_<ParamValue::Interp>(m, "Interp")
        .value("CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("LINEAR", ParamValue::INTERP_LINEAR)
        .value("VERTEX", ParamValue::INTERP_VERTEX)
        .value("INTERP_CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("INTERP_PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("INTERP_LINEAR", ParamValue::INTERP_LINEAR)
        .value("INTERP_VERTEX", ParamValue::INTERP_VERTEX);

    nb::class_<ParamValue>(m, "ParamValue")
        .def_prop_ro("name",
                     [](const ParamValue& self) {
                         return std::string(self.name().string());
                     })
        .def_prop_ro("type", [](const ParamValue& self) { return self.type(); })
        .def_prop_ro("value",
                     [](const ParamValue& self) {
                         return make_pyobject(self.data(), self.type(),
                                              self.nvalues());
                     })
        .def("__len__", &ParamValue::nvalues)
        .def(nb::init<const std::string&, int>())
        .def(nb::init<const std::string&, float>())
        .def(nb::init<const std::string&, const std::string&>())
        .def("__init__",
             [](ParamValue* self, const std::string& name, TypeDesc type,
                nb::handle obj) {
                 new (self) ParamValue(paramvalue_from_pyobject(
                     name, type, 1, ParamValue::INTERP_CONSTANT, obj));
             })
        .def("__init__", [](ParamValue* self, const std::string& name,
                            TypeDesc type, int nvalues,
                            ParamValue::Interp interp, nb::handle obj) {
            new (self) ParamValue(
                paramvalue_from_pyobject(name, type, nvalues, interp, obj));
        });

    nb::class_<ParamValueList>(m, "ParamValueList")
        .def(nb::init<>())
        .def(
            "__getitem__",
            [](const ParamValueList& self, size_t i) {
                if (i >= self.size())
                    throw nb::index_error();
                return self[i];
            },
            nb::rv_policy::reference_internal)
        .def("__getitem__",
             [](const ParamValueList& self, const std::string& key) {
                 auto p = self.find(key);
                 if (p == self.end()) {
                     std::string message = "key '" + key + "' does not exist";
                     throw nb::key_error(message.c_str());
                 }
                 return make_pyobject(p->data(), p->type());
             })
        .def("__setitem__",
             [](ParamValueList& self, const std::string& key, nb::handle val) {
                 delegate_setitem(self, key, val);
             })
        .def("__delitem__", [](ParamValueList& self,
                               const std::string& key) { self.remove(key); })
        .def("__contains__",
             [](const ParamValueList& self, const std::string& key) {
                 return self.contains(key);
             })
        .def("__len__", [](const ParamValueList& self) { return self.size(); })
        .def(
            "__iter__",
            [](const ParamValueList& self) {
                return nb::make_iterator(nb::type<ParamValueList>(), "iterator",
                                         self.begin(), self.end());
            },
            nb::keep_alive<0, 1>())
        .def("append", [](ParamValueList& self,
                          const ParamValue& value) { self.push_back(value); })
        .def("clear", &ParamValueList::clear)
        .def("free", &ParamValueList::free)
        .def("resize",
             [](ParamValueList& self, size_t size) { self.resize(size); })
        .def(
            "remove",
            [](ParamValueList& self, const std::string& name, TypeDesc type,
               bool casesensitive) { self.remove(name, type, casesensitive); },
            "name"_a, "type"_a = TypeUnknown, "casesensitive"_a = true)
        .def(
            "contains",
            [](const ParamValueList& self, const std::string& name,
               TypeDesc type, bool casesensitive) {
                return self.contains(name, type, casesensitive);
            },
            "name"_a, "type"_a = TypeUnknown, "casesensitive"_a = true)
        .def(
            "add_or_replace",
            [](ParamValueList& self, const ParamValue& pv, bool casesensitive) {
                return self.add_or_replace(pv, casesensitive);
            },
            "value"_a, "casesensitive"_a = true)
        .def("sort", &ParamValueList::sort, "casesensitive"_a = true)
        .def("merge", &ParamValueList::merge, "other"_a, "override"_a = false)
        .def("attribute",
             [](ParamValueList& self, const std::string& name, nb::handle val) {
                 attribute_onearg(self, name, val);
             })
        .def("attribute",
             [](ParamValueList& self, const std::string& name, TypeDesc type,
                nb::handle obj) { attribute_typed(self, name, type, obj); })
        .def("attribute", [](ParamValueList& self, const std::string& name,
                             TypeDesc type, int nvalues, nb::handle obj) {
            attribute_typed_nvalues(self, name, type, nvalues, obj);
        });
}

}  // namespace PyOpenImageIO
