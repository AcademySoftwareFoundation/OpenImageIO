// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace {

using namespace OIIO;

template<class Enum>
void
typedesc_property(TypeDesc& t, Enum value);

template<>
void
typedesc_property<TypeDesc::BASETYPE>(TypeDesc& t, TypeDesc::BASETYPE value)
{
    t.basetype = value;
}

template<>
void
typedesc_property<TypeDesc::AGGREGATE>(TypeDesc& t, TypeDesc::AGGREGATE value)
{
    t.aggregate = value;
}

template<>
void
typedesc_property<TypeDesc::VECSEMANTICS>(TypeDesc& t,
                                          TypeDesc::VECSEMANTICS value)
{
    t.vecsemantics = value;
}

}  // namespace


namespace PyOpenImageIO {

void
declare_typedesc(nb::module_& m)
{
    using BASETYPE     = TypeDesc::BASETYPE;
    using AGGREGATE    = TypeDesc::AGGREGATE;
    using VECSEMANTICS = TypeDesc::VECSEMANTICS;

    nb::enum_<BASETYPE>(m, "BASETYPE")
        .value("UNKNOWN", TypeDesc::UNKNOWN)
        .value("NONE", TypeDesc::NONE)
        .value("UCHAR", TypeDesc::UCHAR)
        .value("UINT8", TypeDesc::UINT8)
        .value("CHAR", TypeDesc::CHAR)
        .value("INT8", TypeDesc::INT8)
        .value("UINT16", TypeDesc::UINT16)
        .value("USHORT", TypeDesc::USHORT)
        .value("SHORT", TypeDesc::SHORT)
        .value("INT16", TypeDesc::INT16)
        .value("UINT", TypeDesc::UINT)
        .value("UINT32", TypeDesc::UINT32)
        .value("INT", TypeDesc::INT)
        .value("INT32", TypeDesc::INT32)
        .value("ULONGLONG", TypeDesc::ULONGLONG)
        .value("UINT64", TypeDesc::UINT64)
        .value("LONGLONG", TypeDesc::LONGLONG)
        .value("INT64", TypeDesc::INT64)
        .value("HALF", TypeDesc::HALF)
        .value("FLOAT", TypeDesc::FLOAT)
        .value("DOUBLE", TypeDesc::DOUBLE)
        .value("STRING", TypeDesc::STRING)
        .value("PTR", TypeDesc::PTR)
        .value("LASTBASE", TypeDesc::LASTBASE)
        .export_values();

    nb::enum_<AGGREGATE>(m, "AGGREGATE")
        .value("SCALAR", TypeDesc::SCALAR)
        .value("VEC2", TypeDesc::VEC2)
        .value("VEC3", TypeDesc::VEC3)
        .value("VEC4", TypeDesc::VEC4)
        .value("MATRIX33", TypeDesc::MATRIX33)
        .value("MATRIX44", TypeDesc::MATRIX44)
        .export_values();

    nb::enum_<VECSEMANTICS>(m, "VECSEMANTICS")
        .value("NOXFORM", TypeDesc::NOXFORM)
        .value("NOSEMANTICS", TypeDesc::NOSEMANTICS)
        .value("COLOR", TypeDesc::COLOR)
        .value("POINT", TypeDesc::POINT)
        .value("VECTOR", TypeDesc::VECTOR)
        .value("NORMAL", TypeDesc::NORMAL)
        .value("TIMECODE", TypeDesc::TIMECODE)
        .value("KEYCODE", TypeDesc::KEYCODE)
        .value("RATIONAL", TypeDesc::RATIONAL)
        .value("BOX", TypeDesc::BOX)
        .export_values();

    nb::class_<TypeDesc>(m, "TypeDesc")
        .def_prop_rw(
            "basetype", [](TypeDesc t) { return BASETYPE(t.basetype); },
            [](TypeDesc& t, BASETYPE b) { typedesc_property(t, b); })
        .def_prop_rw(
            "aggregate", [](TypeDesc t) { return AGGREGATE(t.aggregate); },
            [](TypeDesc& t, AGGREGATE b) { typedesc_property(t, b); })
        .def_prop_rw(
            "vecsemantics",
            [](TypeDesc t) { return VECSEMANTICS(t.vecsemantics); },
            [](TypeDesc& t, VECSEMANTICS b) { typedesc_property(t, b); })
        .def_rw("arraylen", &TypeDesc::arraylen)
        .def(nb::init<>())
        .def(nb::init<const TypeDesc&>())
        .def(nb::init<BASETYPE>())
        .def(nb::init<BASETYPE, AGGREGATE>())
        .def(nb::init<BASETYPE, AGGREGATE, VECSEMANTICS>())
        .def(nb::init<BASETYPE, AGGREGATE, VECSEMANTICS, int>())
        .def(nb::init<const char*>())
        .def("c_str",
             [](const TypeDesc& self) { return std::string(self.c_str()); })
        .def("numelements", &TypeDesc::numelements)
        .def("basevalues", &TypeDesc::basevalues)
        .def("size", &TypeDesc::size)
        .def("elementtype", &TypeDesc::elementtype)
        .def("elementsize", &TypeDesc::elementsize)
        .def("basesize", &TypeDesc::basesize)
        .def("fromstring",
             [](TypeDesc& t, const char* typestring) {
                 t.fromstring(typestring);
             })
        .def("equivalent", &TypeDesc::equivalent)
        .def("unarray", &TypeDesc::unarray)
        .def("is_vec2",
             [](const TypeDesc& t, BASETYPE b = TypeDesc::FLOAT) {
                 return t.is_vec2(b);
             })
        .def("is_vec3",
             [](const TypeDesc& t, BASETYPE b = TypeDesc::FLOAT) {
                 return t.is_vec3(b);
             })
        .def("is_vec4",
             [](const TypeDesc& t, BASETYPE b = TypeDesc::FLOAT) {
                 return t.is_vec4(b);
             })
        .def("is_box2",
             [](const TypeDesc& t, BASETYPE b = TypeDesc::FLOAT) {
                 return t.is_box2(b);
             })
        .def("is_box3",
             [](const TypeDesc& t, BASETYPE b = TypeDesc::FLOAT) {
                 return t.is_box3(b);
             })
        .def_static("all_types_equal",
                    [](const std::vector<TypeDesc>& types) {
                        return TypeDesc::all_types_equal(types);
                    })
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__str__", [](TypeDesc t) { return std::string(t.c_str()); })
        .def("__repr__", [](TypeDesc t) {
            return Strutil::fmt::format("<TypeDesc '{}'>", t.c_str());
        });

    nb::implicitly_convertible<BASETYPE, TypeDesc>();
    nb::implicitly_convertible<nb::str, TypeDesc>();

    m.attr("UNKNOWN")   = nb::cast(TypeDesc::UNKNOWN);
    m.attr("NONE")      = nb::cast(TypeDesc::NONE);
    m.attr("UCHAR")     = nb::cast(TypeDesc::UCHAR);
    m.attr("UINT8")     = nb::cast(TypeDesc::UINT8);
    m.attr("CHAR")      = nb::cast(TypeDesc::CHAR);
    m.attr("INT8")      = nb::cast(TypeDesc::INT8);
    m.attr("UINT16")    = nb::cast(TypeDesc::UINT16);
    m.attr("USHORT")    = nb::cast(TypeDesc::USHORT);
    m.attr("SHORT")     = nb::cast(TypeDesc::SHORT);
    m.attr("INT16")     = nb::cast(TypeDesc::INT16);
    m.attr("UINT")      = nb::cast(TypeDesc::UINT);
    m.attr("UINT32")    = nb::cast(TypeDesc::UINT32);
    m.attr("INT")       = nb::cast(TypeDesc::INT);
    m.attr("INT32")     = nb::cast(TypeDesc::INT32);
    m.attr("ULONGLONG") = nb::cast(TypeDesc::ULONGLONG);
    m.attr("UINT64")    = nb::cast(TypeDesc::UINT64);
    m.attr("LONGLONG")  = nb::cast(TypeDesc::LONGLONG);
    m.attr("INT64")     = nb::cast(TypeDesc::INT64);
    m.attr("HALF")      = nb::cast(TypeDesc::HALF);
    m.attr("FLOAT")     = nb::cast(TypeDesc::FLOAT);
    m.attr("DOUBLE")    = nb::cast(TypeDesc::DOUBLE);
    m.attr("STRING")    = nb::cast(TypeDesc::STRING);
    m.attr("PTR")       = nb::cast(TypeDesc::PTR);
    m.attr("LASTBASE")  = nb::cast(TypeDesc::LASTBASE);

    m.attr("SCALAR")   = nb::cast(TypeDesc::SCALAR);
    m.attr("VEC2")     = nb::cast(TypeDesc::VEC2);
    m.attr("VEC3")     = nb::cast(TypeDesc::VEC3);
    m.attr("VEC4")     = nb::cast(TypeDesc::VEC4);
    m.attr("MATRIX33") = nb::cast(TypeDesc::MATRIX33);
    m.attr("MATRIX44") = nb::cast(TypeDesc::MATRIX44);

    m.attr("NOXFORM")     = nb::cast(TypeDesc::NOXFORM);
    m.attr("NOSEMANTICS") = nb::cast(TypeDesc::NOSEMANTICS);
    m.attr("COLOR")       = nb::cast(TypeDesc::COLOR);
    m.attr("POINT")       = nb::cast(TypeDesc::POINT);
    m.attr("VECTOR")      = nb::cast(TypeDesc::VECTOR);
    m.attr("NORMAL")      = nb::cast(TypeDesc::NORMAL);
    m.attr("TIMECODE")    = nb::cast(TypeDesc::TIMECODE);
    m.attr("KEYCODE")     = nb::cast(TypeDesc::KEYCODE);
    m.attr("RATIONAL")    = nb::cast(TypeDesc::RATIONAL);
    m.attr("BOX")         = nb::cast(TypeDesc::BOX);

    m.attr("TypeUnknown")   = TypeUnknown;
    m.attr("TypeFloat")     = TypeFloat;
    m.attr("TypeColor")     = TypeColor;
    m.attr("TypePoint")     = TypePoint;
    m.attr("TypeVector")    = TypeVector;
    m.attr("TypeNormal")    = TypeNormal;
    m.attr("TypeString")    = TypeString;
    m.attr("TypeInt")       = TypeInt;
    m.attr("TypeUInt")      = TypeUInt;
    m.attr("TypeInt64")     = TypeInt64;
    m.attr("TypeUInt64")    = TypeUInt64;
    m.attr("TypeInt32")     = TypeInt32;
    m.attr("TypeUInt32")    = TypeUInt32;
    m.attr("TypeInt16")     = TypeInt16;
    m.attr("TypeUInt16")    = TypeUInt16;
    m.attr("TypeInt8")      = TypeInt8;
    m.attr("TypeUInt8")     = TypeUInt8;
    m.attr("TypeHalf")      = TypeHalf;
    m.attr("TypeMatrix")    = TypeMatrix;
    m.attr("TypeMatrix33")  = TypeMatrix33;
    m.attr("TypeMatrix44")  = TypeMatrix44;
    m.attr("TypeTimeCode")  = TypeTimeCode;
    m.attr("TypeKeyCode")   = TypeKeyCode;
    m.attr("TypeFloat2")    = TypeFloat2;
    m.attr("TypeVector2")   = TypeVector2;
    m.attr("TypeFloat4")    = TypeFloat4;
    m.attr("TypeVector4")   = TypeVector4;
    m.attr("TypeVector2i")  = TypeVector2i;
    m.attr("TypeVector3i")  = TypeVector3i;
    m.attr("TypeBox2")      = TypeBox2;
    m.attr("TypeBox3")      = TypeBox3;
    m.attr("TypeBox2i")     = TypeBox2i;
    m.attr("TypeBox3i")     = TypeBox3i;
    m.attr("TypeRational")  = TypeRational;
    m.attr("TypeURational") = TypeURational;
    m.attr("TypePointer")   = TypePointer;
}

}  // namespace PyOpenImageIO
