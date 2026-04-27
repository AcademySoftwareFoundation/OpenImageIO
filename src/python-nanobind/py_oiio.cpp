// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

using namespace OIIO;

nb::object
oiio_getattribute_typed(const std::string& name, TypeDesc type = TypeUnknown)
{
    if (type == TypeUnknown)
        return nb::none();
    char* data = OIIO_ALLOCA(char, type.size());
    if (!OIIO::getattribute(name, type, data))
        return nb::none();
    return PyOpenImageIO::make_pyobject(data, type);
}


struct oiio_global_attrib_wrapper {
    bool attribute(string_view name, TypeDesc type, const void* data)
    {
        return OIIO::attribute(name, type, data);
    }
    bool attribute(string_view name, int val)
    {
        return OIIO::attribute(name, val);
    }
    bool attribute(string_view name, float val)
    {
        return OIIO::attribute(name, val);
    }
    bool attribute(string_view name, const std::string& val)
    {
        return OIIO::attribute(name, val);
    }
};

}  // namespace


namespace PyOpenImageIO {

TypeDesc
typedesc_from_python_array_code(string_view code)
{
    TypeDesc t(code);
    if (!t.is_unknown())
        return t;

    if (code == "b" || code == "c")
        return TypeDesc::INT8;
    if (code == "B")
        return TypeDesc::UINT8;
    if (code == "h")
        return TypeDesc::INT16;
    if (code == "H")
        return TypeDesc::UINT16;
    if (code == "i")
        return TypeDesc::INT;
    if (code == "I")
        return TypeDesc::UINT;
    if (code == "l")
        return TypeDesc::INT64;
    if (code == "L")
        return TypeDesc::UINT64;
    if (code == "q")
        return TypeDesc::INT64;
    if (code == "Q")
        return TypeDesc::UINT64;
    if (code == "f")
        return TypeDesc::FLOAT;
    if (code == "d")
        return TypeDesc::DOUBLE;
    if (code == "float16" || code == "e")
        return TypeDesc::HALF;
    return TypeDesc::UNKNOWN;
}


nb::object
make_pyobject(const void* data, TypeDesc type, int nvalues,
              nb::handle defaultvalue)
{
    if (!data || !nvalues)
        return nb::borrow(defaultvalue);
    if (type.basetype == TypeDesc::INT32)
        return C_to_val_or_tuple(static_cast<const int*>(data), type, nvalues);
    if (type.basetype == TypeDesc::FLOAT)
        return C_to_val_or_tuple(static_cast<const float*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::STRING)
        return C_to_val_or_tuple(static_cast<const char* const*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::UINT32)
        return C_to_val_or_tuple(static_cast<const unsigned int*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::INT16)
        return C_to_val_or_tuple(static_cast<const short*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::UINT16)
        return C_to_val_or_tuple(static_cast<const unsigned short*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::INT64)
        return C_to_val_or_tuple(static_cast<const int64_t*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::UINT64)
        return C_to_val_or_tuple(static_cast<const uint64_t*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::DOUBLE)
        return C_to_val_or_tuple(static_cast<const double*>(data), type,
                                 nvalues);
    if (type.basetype == TypeDesc::HALF)
        return C_to_val_or_tuple(static_cast<const half*>(data), type, nvalues);
    if (type.basetype == TypeDesc::UINT8 && type.arraylen > 0) {
        int n = type.arraylen * nvalues;
        if (n <= 0)
            return nb::borrow(defaultvalue);
        auto* copy = new uint8_t[n];
        std::memcpy(copy, data, static_cast<size_t>(n));
        return make_numpy_array(copy, static_cast<size_t>(n));
    }
    if (type.basetype == TypeDesc::UINT8) {
        return C_to_val_or_tuple(static_cast<const unsigned char*>(data), type,
                                 nvalues);
    }
    return nb::borrow(defaultvalue);
}

}  // namespace PyOpenImageIO


NB_MODULE(_OpenImageIO, m)
{
    m.doc() = "OpenImageIO nanobind bindings.";

    PyOpenImageIO::declare_typedesc(m);
    PyOpenImageIO::declare_paramvalue(m);
    PyOpenImageIO::declare_roi(m);
    PyOpenImageIO::declare_imagespec(m);

    m.def("attribute", [](const std::string& name, nb::handle obj) {
        oiio_global_attrib_wrapper wrapper;
        PyOpenImageIO::attribute_onearg(wrapper, name, obj);
    });
    m.def("attribute",
          [](const std::string& name, TypeDesc type, nb::handle obj) {
              oiio_global_attrib_wrapper wrapper;
              PyOpenImageIO::attribute_typed(wrapper, name, type, obj);
          });
    m.def(
        "get_int_attribute",
        [](const std::string& name, int def) {
            return OIIO::get_int_attribute(name, def);
        },
        "name"_a, "defaultval"_a = 0);
    m.def(
        "get_float_attribute",
        [](const std::string& name, float def) {
            return OIIO::get_float_attribute(name, def);
        },
        "name"_a, "defaultval"_a = 0.0f);
    m.def(
        "get_string_attribute",
        [](const std::string& name, const std::string& def) {
            return std::string(OIIO::get_string_attribute(name, def));
        },
        "name"_a, "defaultval"_a = "");
    m.def("getattribute", &oiio_getattribute_typed, "name"_a,
          "type"_a = TypeUnknown);
    m.attr("__version__") = OIIO_VERSION_STRING;
}
