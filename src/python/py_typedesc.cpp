// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {



// Declare the OIIO TypeDesc type to Python
void
declare_typedesc(py::module& m)
{
    py::enum_<TypeDesc::BASETYPE>(m, "BASETYPE")
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

    py::enum_<TypeDesc::AGGREGATE>(m, "AGGREGATE")
        .value("SCALAR", TypeDesc::SCALAR)
        .value("VEC2", TypeDesc::VEC2)
        .value("VEC3", TypeDesc::VEC3)
        .value("VEC4", TypeDesc::VEC4)
        .value("MATRIX33", TypeDesc::MATRIX33)
        .value("MATRIX44", TypeDesc::MATRIX44)
        .export_values();

    py::enum_<TypeDesc::VECSEMANTICS>(m, "VECSEMANTICS")
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

    py::class_<TypeDesc>(m, "TypeDesc")
        // basetype, aggregate, and vecsemantics should look like BASETYPE,
        // AGGREGATE, VECSEMANTICS, but since they are stored as unsigned
        // char, def_readwrite() doesn't do the right thing. Instead, we
        // use set_foo/get_foo wrappers, but from Python it looks like
        // regular member access.
        .def_property(
            "basetype",
            [](TypeDesc t) { return TypeDesc::BASETYPE(t.basetype); },
            [](TypeDesc& t, TypeDesc::BASETYPE b) { return t.basetype = b; })
        .def_property(
            "aggregate",
            [](TypeDesc t) { return TypeDesc::AGGREGATE(t.aggregate); },
            [](TypeDesc& t, TypeDesc::AGGREGATE b) { return t.aggregate = b; })
        .def_property(
            "vecsemantics",
            [](TypeDesc t) { return TypeDesc::VECSEMANTICS(t.vecsemantics); },
            [](TypeDesc& t, TypeDesc::VECSEMANTICS b) {
                return t.vecsemantics = b;
            })
        .def_readwrite("arraylen", &TypeDesc::arraylen)
        // Constructors: () [defined implicitly], (base), (base, agg),
        // (base,agg,vecsem), (base,agg,vecsem,arraylen), string.
        .def(py::init<>())
        .def(py::init<const TypeDesc&>())
        .def(py::init<TypeDesc::BASETYPE>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE,
                      TypeDesc::VECSEMANTICS>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE,
                      TypeDesc::VECSEMANTICS, int>())
        .def(py::init<const char*>())
        // Unfortunately, overloading the int varieties, as we do in C++,
        // doesn't seem to work properly, it can't distinguish between an
        // int and an AGGREGATE, for example. Maybe in C++11 with strong
        // enum typing, it will work. But for now, we must forego these
        // variants of the constructors:
        //   .def(init<TypeDesc::BASETYPE, int>())
        //   .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, int>())
        // FIXME -- I bet this works with Pybind11
        .def("c_str", [](const TypeDesc& self) { return PY_STR(self.c_str()); })
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
        .def("is_vec2", &TypeDesc::is_vec2)
        .def("is_vec3", &TypeDesc::is_vec3)
        .def("is_vec4", &TypeDesc::is_vec4)
        .def("is_box2", &TypeDesc::is_box2)
        .def("is_box3", &TypeDesc::is_box3)

        // overloaded operators
        .def(py::self == py::self)  // operator==   //NOSONAR
        .def(py::self != py::self)  // operator!=   //NOSONAR

        // Conversion to string
        .def("__str__", [](TypeDesc t) { return PY_STR(t.c_str()); })
        .def("__repr__", [](TypeDesc t) {
            return PY_STR("<TypeDesc '" + std::string(t.c_str()) + "'>");
        });

    // Declare that a BASETYPE is implicitly convertible to a TypeDesc.
    // This keeps us from having to separately declare func(TypeDesc)
    // and func(TypeDesc::BASETYPE) everywhere.
    py::implicitly_convertible<TypeDesc::BASETYPE, TypeDesc>();

    // Declare that a Python str is implicitly convertible to a TypeDesc.
    // This let you call foo("uint8") anyplace it would normally expect
    // foo(TypeUInt8).
    py::implicitly_convertible<py::str, TypeDesc>();

    // Global constants of common TypeDescs
    m.attr("TypeUnknown")  = TypeUnknown;
    m.attr("TypeFloat")    = TypeFloat;
    m.attr("TypeColor")    = TypeColor;
    m.attr("TypePoint")    = TypePoint;
    m.attr("TypeVector")   = TypeVector;
    m.attr("TypeNormal")   = TypeNormal;
    m.attr("TypeString")   = TypeString;
    m.attr("TypeInt")      = TypeInt;
    m.attr("TypeUInt")     = TypeUInt;
    m.attr("TypeInt64")    = TypeInt64;
    m.attr("TypeUInt64")   = TypeUInt64;
    m.attr("TypeInt32")    = TypeInt32;
    m.attr("TypeUInt32")   = TypeUInt32;
    m.attr("TypeInt16")    = TypeInt16;
    m.attr("TypeUInt16")   = TypeUInt16;
    m.attr("TypeInt8")     = TypeInt8;
    m.attr("TypeUInt8")    = TypeUInt8;
    m.attr("TypeHalf")     = TypeHalf;
    m.attr("TypeMatrix")   = TypeMatrix;
    m.attr("TypeMatrix33") = TypeMatrix33;
    m.attr("TypeMatrix44") = TypeMatrix44;
    m.attr("TypeTimeCode") = TypeTimeCode;
    m.attr("TypeKeyCode")  = TypeKeyCode;
    m.attr("TypeFloat2")   = TypeFloat2;
    m.attr("TypeVector2")  = TypeVector2;
    m.attr("TypeFloat4")   = TypeFloat4;
    m.attr("TypeVector4")  = TypeVector4;
    m.attr("TypeVector2i") = TypeVector2i;
    m.attr("TypeVector3i") = TypeVector3i;
    m.attr("TypeBox2")     = TypeBox2;
    m.attr("TypeBox3")     = TypeBox3;
    m.attr("TypeBox2i")    = TypeBox2i;
    m.attr("TypeBox3i")    = TypeBox3i;
    m.attr("TypeRational") = TypeRational;
    m.attr("TypePointer")  = TypePointer;
}

}  // namespace PyOpenImageIO
