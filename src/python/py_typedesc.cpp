// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"

namespace PyOpenImageIO {



// Declare the OIIO TypeDesc type to Python
void
declare_typedesc(py_module& m)
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
        .OIIO_PY_PROP_RW(
            "basetype",
            [](TypeDesc t) { return TypeDesc::BASETYPE(t.basetype); },
            [](TypeDesc& t, TypeDesc::BASETYPE b) { return t.basetype = b; })
        .OIIO_PY_PROP_RW(
            "aggregate",
            [](TypeDesc t) { return TypeDesc::AGGREGATE(t.aggregate); },
            [](TypeDesc& t, TypeDesc::AGGREGATE b) { return t.aggregate = b; })
        .OIIO_PY_PROP_RW(
            "vecsemantics",
            [](TypeDesc t) { return TypeDesc::VECSEMANTICS(t.vecsemantics); },
            [](TypeDesc& t, TypeDesc::VECSEMANTICS b) {
                return t.vecsemantics = b;
            })
        .OIIO_PY_RW("arraylen", &TypeDesc::arraylen)
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
        .def("c_str",
             [](const TypeDesc& self) { return oiio_py::str(self.c_str()); })
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
        .def(
            "is_vec2",
            [](const TypeDesc& t, TypeDesc::BASETYPE b = TypeDesc::FLOAT) {
                return t.is_vec2(b);
            },
            "b"_a = TypeDesc::FLOAT)
        .def(
            "is_vec3",
            [](const TypeDesc& t, TypeDesc::BASETYPE b = TypeDesc::FLOAT) {
                return t.is_vec3(b);
            },
            "b"_a = TypeDesc::FLOAT)
        .def(
            "is_vec4",
            [](const TypeDesc& t, TypeDesc::BASETYPE b = TypeDesc::FLOAT) {
                return t.is_vec4(b);
            },
            "b"_a = TypeDesc::FLOAT)
        .def(
            "is_box2",
            [](const TypeDesc& t, TypeDesc::BASETYPE b = TypeDesc::FLOAT) {
                return t.is_box2(b);
            },
            "b"_a = TypeDesc::FLOAT)
        .def(
            "is_box3",
            [](const TypeDesc& t, TypeDesc::BASETYPE b = TypeDesc::FLOAT) {
                return t.is_box3(b);
            },
            "b"_a = TypeDesc::FLOAT)
        .def_static("all_types_equal",
                    [](const std::vector<TypeDesc>& types) {
                        return TypeDesc::all_types_equal(types);
                    })

        // overloaded operators
        .def(py::self == py::self)  // operator==   //NOSONAR
        .def(py::self != py::self)  // operator!=   //NOSONAR

        // Conversion to string
        .def("__str__", [](TypeDesc t) { return oiio_py::str(t.c_str()); })
        .def("__repr__", [](TypeDesc t) {
            return oiio_py::str("<TypeDesc '" + std::string(t.c_str()) + "'>");
        });


    // Declare that a BASETYPE is implicitly convertible to a TypeDesc.
    // This keeps us from having to separately declare func(TypeDesc)
    // and func(TypeDesc::BASETYPE) everywhere.
    py::implicitly_convertible<TypeDesc::BASETYPE, TypeDesc>();

    // Declare that a Python str is implicitly convertible to a TypeDesc.
    // This let you call foo("uint8") anyplace it would normally expect
    // foo(TypeUInt8).
    py::implicitly_convertible<py::str, TypeDesc>();

#if defined(OIIO_PY_BACKEND_NANOBIND)
    // Pybind11's .export_values() above copies enum members onto the module
    // (oiio.FLOAT, oiio.VEC3, ...). Nanobind has no equivalent here, so set
    // them explicitly to keep the same public API.
    m.attr("UNKNOWN")   = py::cast(TypeDesc::UNKNOWN);
    m.attr("NONE")      = py::cast(TypeDesc::NONE);
    m.attr("UCHAR")     = py::cast(TypeDesc::UCHAR);
    m.attr("UINT8")     = py::cast(TypeDesc::UINT8);
    m.attr("CHAR")      = py::cast(TypeDesc::CHAR);
    m.attr("INT8")      = py::cast(TypeDesc::INT8);
    m.attr("UINT16")    = py::cast(TypeDesc::UINT16);
    m.attr("USHORT")    = py::cast(TypeDesc::USHORT);
    m.attr("SHORT")     = py::cast(TypeDesc::SHORT);
    m.attr("INT16")     = py::cast(TypeDesc::INT16);
    m.attr("UINT")      = py::cast(TypeDesc::UINT);
    m.attr("UINT32")    = py::cast(TypeDesc::UINT32);
    m.attr("INT")       = py::cast(TypeDesc::INT);
    m.attr("INT32")     = py::cast(TypeDesc::INT32);
    m.attr("ULONGLONG") = py::cast(TypeDesc::ULONGLONG);
    m.attr("UINT64")    = py::cast(TypeDesc::UINT64);
    m.attr("LONGLONG")  = py::cast(TypeDesc::LONGLONG);
    m.attr("INT64")     = py::cast(TypeDesc::INT64);
    m.attr("HALF")      = py::cast(TypeDesc::HALF);
    m.attr("FLOAT")     = py::cast(TypeDesc::FLOAT);
    m.attr("DOUBLE")    = py::cast(TypeDesc::DOUBLE);
    m.attr("STRING")    = py::cast(TypeDesc::STRING);
    m.attr("PTR")       = py::cast(TypeDesc::PTR);
    m.attr("LASTBASE")  = py::cast(TypeDesc::LASTBASE);

    m.attr("SCALAR")   = py::cast(TypeDesc::SCALAR);
    m.attr("VEC2")     = py::cast(TypeDesc::VEC2);
    m.attr("VEC3")     = py::cast(TypeDesc::VEC3);
    m.attr("VEC4")     = py::cast(TypeDesc::VEC4);
    m.attr("MATRIX33") = py::cast(TypeDesc::MATRIX33);
    m.attr("MATRIX44") = py::cast(TypeDesc::MATRIX44);

    m.attr("NOXFORM")     = py::cast(TypeDesc::NOXFORM);
    m.attr("NOSEMANTICS") = py::cast(TypeDesc::NOSEMANTICS);
    m.attr("COLOR")       = py::cast(TypeDesc::COLOR);
    m.attr("POINT")       = py::cast(TypeDesc::POINT);
    m.attr("VECTOR")      = py::cast(TypeDesc::VECTOR);
    m.attr("NORMAL")      = py::cast(TypeDesc::NORMAL);
    m.attr("TIMECODE")    = py::cast(TypeDesc::TIMECODE);
    m.attr("KEYCODE")     = py::cast(TypeDesc::KEYCODE);
    m.attr("RATIONAL")    = py::cast(TypeDesc::RATIONAL);
    m.attr("BOX")         = py::cast(TypeDesc::BOX);
#endif

    // Global constants of common TypeDescs
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
