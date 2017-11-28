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


py::object
ParamValue_getitem (const ParamValue& self, int n)
{
    if (n < 0 || n >= self.nvalues()) {
        throw std::out_of_range (Strutil::format("ParamValue index out of range %d", n));
    }

    TypeDesc t = self.type();

#define ParamValue_convert_dispatch(TYPE) \
    case TypeDesc::TYPE: \
    return C_to_val_or_tuple ((CType<TypeDesc::TYPE>::type*)self.data(), t)

    switch (t.basetype) {
    // ParamValue_convert_dispatch(UCHAR);
    // ParamValue_convert_dispatch(CHAR);
    ParamValue_convert_dispatch(USHORT);
    ParamValue_convert_dispatch(SHORT);
    ParamValue_convert_dispatch(UINT);
    ParamValue_convert_dispatch(INT);
    // ParamValue_convert_dispatch(ULONGLONG);
    // ParamValue_convert_dispatch(LONGLONG);
#ifdef _HALF_H_
    ParamValue_convert_dispatch(HALF);
#endif
    ParamValue_convert_dispatch(FLOAT);
    ParamValue_convert_dispatch(DOUBLE);
    case TypeDesc::STRING:
        return C_to_val_or_tuple ((const char **)self.data(), t);
    default: return py::none();
    }

#undef ParamValue_convert_dispatch
}




void
declare_paramvalue (py::module& m)
{
    using namespace pybind11::literals;

    py::enum_<ParamValue::Interp>(m, "Interp")
        .value("INTERP_CONSTANT", ParamValue::INTERP_CONSTANT)
        .value("INTERP_PERPIECE", ParamValue::INTERP_PERPIECE)
        .value("INTERP_LINEAR",   ParamValue::INTERP_LINEAR)
        .value("INTERP_VERTEX",   ParamValue::INTERP_VERTEX)
    ;

    py::class_<ParamValue>(m, "ParamValue")
        .def_property_readonly("name",
                      [](const ParamValue& p) { return p.name().string(); })
        .def_property_readonly("type",
                      [](const ParamValue& p) { return p.type().c_str(); })
        .def_property_readonly("value",
                      [](const ParamValue& p) { return ParamValue_getitem (p, 0); })
        // .def("__getitem__",       &ParamValue_getitem)
        .def_property_readonly("__len__", &ParamValue::nvalues)
        .def(py::init<const std::string&, int>())
        .def(py::init<const std::string&, float>())
        .def(py::init<const std::string&, const std::string&>())
    ;

    py::class_<ParamValueList>(m, "ParamValueList")
        .def(py::init<>())
        .def("__getitem__", [](ParamValueList& self, int i){ return self[i]; },
            py::return_value_policy::reference_internal)

        // .def("__iter__", boost::python::iterator<ParamValueList>())
        .def("__len__", [](const ParamValueList& p){ return p.size(); })
        // .def("grow",        &ParamValueList::grow,
        //     py::return_value_policy::reference_internal)
        .def("append", [](ParamValueList& p, const ParamValue &v){ return p.push_back(v); })
        .def("clear",       &ParamValueList::clear)
        .def("free",        &ParamValueList::free)
        .def("resize", [](ParamValueList& p, size_t s){ return p.resize(s); })
        .def("remove", [](ParamValueList& p, const std::string& name,
                          TypeDesc type, bool casesensitive) {
                p.remove (name, type, casesensitive);
            },
            "name"_a, "type"_a=TypeUnknown, "casesensitive"_a=true)
        .def("contains", [](ParamValueList& p, const std::string& name,
                          TypeDesc type, bool casesensitive) {
                return p.contains (name, type, casesensitive);
            },
            "name"_a, "type"_a=TypeUnknown, "casesensitive"_a=true)
        .def("add_or_replace", [](ParamValueList& p, const ParamValue& pv,
                                  bool casesensitive) {
                return p.add_or_replace (pv, casesensitive);
            },
            "value"_a, "casesensitive"_a=true)
    ;
}

}
