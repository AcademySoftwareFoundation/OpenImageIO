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
using namespace boost::python;
using self_ns::str;



static TypeDesc::BASETYPE TypeDesc_get_basetype (const TypeDesc &t) {
    return (TypeDesc::BASETYPE)t.basetype;
}
static TypeDesc::AGGREGATE TypeDesc_get_aggregate (const TypeDesc &t) {
    return (TypeDesc::AGGREGATE)t.aggregate;
}
static TypeDesc::VECSEMANTICS TypeDesc_get_vecsemantics (const TypeDesc &t) {
    return (TypeDesc::VECSEMANTICS)t.vecsemantics;
}
static void TypeDesc_set_basetype (TypeDesc &t, TypeDesc::BASETYPE val) {
    t.basetype = val;
}
static void TypeDesc_set_aggregate (TypeDesc &t, TypeDesc::AGGREGATE val) {
    t.aggregate = val;
}
static void TypeDesc_set_vecsemantics (TypeDesc &t, TypeDesc::VECSEMANTICS val) {
    t.vecsemantics = val;
}

static void TypeDesc_fromstring (TypeDesc &t, const char* typestring)
{
    t.fromstring (typestring);
}



// Declare the OIIO TypeDesc type to Python
void declare_typedesc() {

    enum_<TypeDesc::BASETYPE>("BASETYPE")
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

    enum_<TypeDesc::AGGREGATE>("AGGREGATE")
        .value("SCALAR",    TypeDesc::SCALAR)
        .value("VEC2",      TypeDesc::VEC2)
        .value("VEC3",      TypeDesc::VEC3)
        .value("VEC4",      TypeDesc::VEC4)
        .value("MATRIX33",  TypeDesc::MATRIX33)
        .value("MATRIX44",  TypeDesc::MATRIX44)
        .export_values()
    ;
    
    enum_<TypeDesc::VECSEMANTICS>("VECSEMANTICS")
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

    class_<TypeDesc>("TypeDesc")
        // basetype, aggregate, and vecsemantics should look like BASETYPE,
        // AGGREGATE, VECSEMANTICS, but since they are stored as unsigned
        // char, def_readwrite() doesn't do the right thing. Instead, we
        // use set_foo/get_foo wrappers, but from Python it looks like
        // regular member access.
        .add_property("basetype", &TypeDesc_get_basetype, &TypeDesc_set_basetype)
        .add_property("aggregate", &TypeDesc_get_aggregate, &TypeDesc_set_aggregate)
        .add_property("vecsemantics", &TypeDesc_get_vecsemantics, &TypeDesc_set_vecsemantics)
        .def_readwrite("arraylen",      &TypeDesc::arraylen)
        // Constructors: () [defined implicitly], (base), (base, agg), 
        // (base,agg,vecsem), (base,agg,vecsem,arraylen), string.
        .def(init<TypeDesc::BASETYPE>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS, int>())
        .def(init<const char *>())
        // Unfortunately, overloading the int varieties, as we do in C++,
        // doesn't seem to work properly, it can't distinguish between an
        // int and an AGGREGATE, for example. Maybe in C++11 with strong
        // enum typing, it will work. But for now, we must forego these
        // variants of the constructors:
        //   .def(init<TypeDesc::BASETYPE, int>())
        //   .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, int>())
        .def("c_str",            &TypeDesc::c_str)
        .def("numelements",      &TypeDesc::numelements)
        .def("basevalues",       &TypeDesc::basevalues)
        .def("size",             &TypeDesc::size)
        .def("elementtype",      &TypeDesc::elementtype)
        .def("elementsize",      &TypeDesc::elementsize)
        .def("basesize",         &TypeDesc::basesize)
        .def("fromstring",       &TypeDesc_fromstring)
        .def("equivalent",       &TypeDesc::equivalent)
        .def("unarray",          &TypeDesc::unarray)
        .def("is_vec3",          &TypeDesc::is_vec3)
        .def("is_vec4",          &TypeDesc::is_vec4)

        // overloaded operators
        .def(self == other<TypeDesc>())    // operator==
        .def(self != other<TypeDesc>())    // operator!=

        // Define Python str(TypeDesc), it automatically uses '<<'
        .def(str(self))    // __str__

        // Static members of pre-constructed types
        // DEPRECATED(1.8)
        .def_readonly("TypeFloat",    &TypeFloat)
        .def_readonly("TypeColor",    &TypeColor)
        .def_readonly("TypeString",   &TypeString)
        .def_readonly("TypeInt",      &TypeInt)
        .def_readonly("TypeHalf",     &TypeHalf)
        .def_readonly("TypePoint",    &TypePoint)
        .def_readonly("TypeVector",   &TypeVector)
        .def_readonly("TypeNormal",   &TypeNormal)
        .def_readonly("TypeMatrix",   &TypeMatrix)
        .def_readonly("TypeMatrix33", &TypeMatrix33)
        .def_readonly("TypeMatrix44", &TypeMatrix44)
        .def_readonly("TypeTimeCode", &TypeTimeCode)
        .def_readonly("TypeKeyCode",  &TypeKeyCode)
        .def_readonly("TypeFloat4",   &TypeFloat4)
    ;

    // Global constants of common TypeDescs
    scope().attr("TypeUnknown")  = TypeUnknown;
    scope().attr("TypeFloat")    = TypeFloat;
    scope().attr("TypeColor")    = TypeColor;
    scope().attr("TypePoint")    = TypePoint;
    scope().attr("TypeVector")   = TypeVector;
    scope().attr("TypeNormal")   = TypeNormal;
    scope().attr("TypeString")   = TypeString;
    scope().attr("TypeInt")      = TypeInt;
    scope().attr("TypeUInt")     = TypeUInt;
    scope().attr("TypeHalf")     = TypeHalf;
    scope().attr("TypeMatrix")   = TypeMatrix;
    scope().attr("TypeMatrix33") = TypeMatrix33;
    scope().attr("TypeMatrix44") = TypeMatrix44;
    scope().attr("TypeTimeCode") = TypeTimeCode;
    scope().attr("TypeKeyCode")  = TypeKeyCode;
    scope().attr("TypeFloat4")   = TypeFloat4;
    scope().attr("TypeRational") = TypeRational;

}

} // namespace PyOpenImageIO

