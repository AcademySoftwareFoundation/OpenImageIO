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

namespace PyOpenImageIO {



// Declare the OIIO TypeDesc type to Python
void declare_typedesc(py::module& m) {

    py::enum_<TypeDesc::BASETYPE>(m, "BASETYPE")
        .value("UNKNOWN",   TypeDesc::UNKNOWN)
        .value("NONE",      TypeDesc::NONE)
        .value("UCHAR",     TypeDesc::UCHAR)
        .value("UINT8",     TypeDesc::UINT8)
        .value("CHAR",      TypeDesc::CHAR)
        .value("INT8",      TypeDesc::INT8)
        .value("USHORT",    TypeDesc::USHORT)
        .value("UINT16",    TypeDesc::UINT16)
        .value("SHORT",     TypeDesc::SHORT)
        .value("INT16",     TypeDesc::INT16)
        .value("UINT",      TypeDesc::UINT)
        .value("UINT32",    TypeDesc::UINT32)
        .value("INT",       TypeDesc::INT)
        .value("INT32",     TypeDesc::INT32)
        .value("ULONGLONG", TypeDesc::ULONGLONG)
        .value("UINT64",    TypeDesc::UINT64)
        .value("LONGLONG",  TypeDesc::LONGLONG)
        .value("INT64",     TypeDesc::INT64)
        .value("HALF",      TypeDesc::HALF)
        .value("FLOAT",     TypeDesc::FLOAT)
        .value("DOUBLE",    TypeDesc::DOUBLE)
        .value("STRING",    TypeDesc::STRING)
        .value("PTR",       TypeDesc::PTR)
        .value("LASTBASE",  TypeDesc::LASTBASE)
        .export_values()
    ;

    py::enum_<TypeDesc::AGGREGATE>(m, "AGGREGATE")
        .value("SCALAR",    TypeDesc::SCALAR)
        .value("VEC2",      TypeDesc::VEC2)
        .value("VEC3",      TypeDesc::VEC3)
        .value("VEC4",      TypeDesc::VEC4)
        .value("MATRIX33",  TypeDesc::MATRIX33)
        .value("MATRIX44",  TypeDesc::MATRIX44)
        .export_values()
    ;

    py::enum_<TypeDesc::VECSEMANTICS>(m, "VECSEMANTICS")
        .value("NOXFORM",  TypeDesc::NOXFORM)
        .value("NOSEMANTICS", TypeDesc::NOSEMANTICS)
        .value("COLOR",    TypeDesc::COLOR)
        .value("POINT",    TypeDesc::POINT)
        .value("VECTOR",   TypeDesc::VECTOR)
        .value("NORMAL",   TypeDesc::NORMAL)
        .value("TIMECODE", TypeDesc::TIMECODE)
        .value("KEYCODE",  TypeDesc::KEYCODE)
        .value("RATIONAL", TypeDesc::RATIONAL)
        .export_values()
    ;

    py::class_<TypeDesc>(m, "TypeDesc")
        // basetype, aggregate, and vecsemantics should look like BASETYPE,
        // AGGREGATE, VECSEMANTICS, but since they are stored as unsigned
        // char, def_readwrite() doesn't do the right thing. Instead, we
        // use set_foo/get_foo wrappers, but from Python it looks like
        // regular member access.
        .def_property("basetype",
            [](TypeDesc t){ return TypeDesc::BASETYPE(t.basetype); },
            [](TypeDesc &t, TypeDesc::BASETYPE b){ return t.basetype = b; })
        .def_property("aggregate",
            [](TypeDesc t){ return TypeDesc::AGGREGATE(t.aggregate); },
            [](TypeDesc &t, TypeDesc::AGGREGATE b){ return t.aggregate = b; })
        .def_property("vecsemantics",
            [](TypeDesc t){ return TypeDesc::VECSEMANTICS(t.vecsemantics); },
            [](TypeDesc &t, TypeDesc::VECSEMANTICS b){ return t.vecsemantics = b; })
        .def_readwrite("arraylen",      &TypeDesc::arraylen)
        // Constructors: () [defined implicitly], (base), (base, agg),
        // (base,agg,vecsem), (base,agg,vecsem,arraylen), string.
        .def(py::init<>())
        .def(py::init<const TypeDesc&>())
        .def(py::init<TypeDesc::BASETYPE>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS>())
        .def(py::init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS, int>())
        .def(py::init<const char *>())
        // Unfortunately, overloading the int varieties, as we do in C++,
        // doesn't seem to work properly, it can't distinguish between an
        // int and an AGGREGATE, for example. Maybe in C++11 with strong
        // enum typing, it will work. But for now, we must forego these
        // variants of the constructors:
        //   .def(init<TypeDesc::BASETYPE, int>())
        //   .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, int>())
        // FIXME -- I bet this works with Pybind11
        .def("c_str",            &TypeDesc::c_str)
        .def("numelements",      &TypeDesc::numelements)
        .def("basevalues",       &TypeDesc::basevalues)
        .def("size",             &TypeDesc::size)
        .def("elementtype",      &TypeDesc::elementtype)
        .def("elementsize",      &TypeDesc::elementsize)
        .def("basesize",         &TypeDesc::basesize)
        .def("fromstring",       [](TypeDesc &t, const char* typestring){
            t.fromstring (typestring); })
        .def("equivalent",       &TypeDesc::equivalent)
        .def("unarray",          &TypeDesc::unarray)
        .def("is_vec3",          &TypeDesc::is_vec3)
        .def("is_vec4",          &TypeDesc::is_vec4)

        // overloaded operators
        .def(py::self == py::self)    // operator==
        .def(py::self != py::self)    // operator!=

        // Conversion to string
        .def("__str__", [](TypeDesc t){ return PY_STR(t.c_str()); })
        .def("__repr__", [](TypeDesc t){
                return PY_STR("<TypeDesc '" + std::string(t.c_str()) + "'>");
            })

        // Static members of pre-constructed types
        // DEPRECATED(1.8)
        .def_readonly_static ("TypeFloat",    &TypeFloat)
        .def_readonly_static ("TypeColor",    &TypeColor)
        .def_readonly_static ("TypeString",   &TypeString)
        .def_readonly_static ("TypeInt",      &TypeInt)
        .def_readonly_static ("TypeHalf",     &TypeHalf)
        .def_readonly_static ("TypePoint",    &TypePoint)
        .def_readonly_static ("TypeVector",   &TypeVector)
        .def_readonly_static ("TypeNormal",   &TypeNormal)
        .def_readonly_static ("TypeMatrix",   &TypeMatrix)
        .def_readonly_static ("TypeMatrix33", &TypeMatrix33)
        .def_readonly_static ("TypeMatrix44", &TypeMatrix44)
        .def_readonly_static ("TypeTimeCode", &TypeTimeCode)
        .def_readonly_static ("TypeKeyCode",  &TypeKeyCode)
        .def_readonly_static ("TypeRational", &TypeRational)
        .def_readonly_static ("TypeFloat4",   &TypeFloat4)
    ;

    // Declare that a BASETYPE is implicitly convertible to a TypeDesc.
    // This keeps us from having to separately declare func(TypeDesc)
    // and func(TypeDesc::BASETYPE) everywhere.
    py::implicitly_convertible<TypeDesc::BASETYPE, TypeDesc>();

    // Declare that a Python str is implicitly convertible to a TypeDesc.
    // This let you call foo("uint8") anyplace it would normally expect
    // foo(TypeUInt8).
    py::implicitly_convertible<py::str, TypeDesc>();

#if 1
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
    m.attr("TypeFloat4")   = TypeFloat4;
    m.attr("TypeRational") = TypeRational;
#endif
}

} // namespace PyOpenImageIO

