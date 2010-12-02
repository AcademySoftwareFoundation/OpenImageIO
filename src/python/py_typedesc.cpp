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

/// Declare the OIIO typedesc type to Python
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
        .value("INT",       TypeDesc::INT)
        .value("HALF",      TypeDesc::HALF)
        .value("FLOAT",     TypeDesc::FLOAT)
        .value("DOUBLE",    TypeDesc::DOUBLE)
        .value("STRING",    TypeDesc::STRING)
        .value("PTR",       TypeDesc::PTR)
        .value("LASTBASE",  TypeDesc::LASTBASE)
    ;

    enum_<TypeDesc::AGGREGATE>("AGGREGATE")
        .value("SCALAR",    TypeDesc::SCALAR)
        .value("VEC2",      TypeDesc::VEC2)
        .value("VEC3",      TypeDesc::VEC3)
        .value("VEC4",      TypeDesc::VEC4)
        .value("MATRIX44",  TypeDesc::MATRIX44)
    ;
    
    enum_<TypeDesc::VECSEMANTICS>("VECSEMANTICS")
        .value("NOXFORM",  TypeDesc::NOXFORM)
        .value("COLOR",    TypeDesc::COLOR)
        .value("POINT",    TypeDesc::POINT)
        .value("VECTOR",   TypeDesc::VECTOR)
        .value("NORMAL",   TypeDesc::NORMAL)
    ;

    class_<TypeDesc>("TypeDesc")
        .def_readwrite("basetype",      &TypeDesc::basetype)
        .def_readwrite("aggregate",     &TypeDesc::aggregate)
        .def_readwrite("vecsemantics",  &TypeDesc::vecsemantics)
        .def_readwrite("arraylen",      &TypeDesc::arraylen)

        .def(init<TypeDesc::BASETYPE>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS>())
        .def(init<TypeDesc::BASETYPE, int>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, int>())
        .def(init<TypeDesc::BASETYPE, TypeDesc::AGGREGATE, TypeDesc::VECSEMANTICS, int>())
        .def(init<const char *>()) //not sure how to test these inits from python
        .def("c_str",        &TypeDesc::c_str)
        .def("numelements",  &TypeDesc::numelements)
        .def("size",         &TypeDesc::size)
        .def("elementtype",  &TypeDesc::elementtype)
        .def("elementsize",  &TypeDesc::elementsize)
        .def("basesize",     &TypeDesc::basesize)
        .def("fromstring",   &TypeDesc::fromstring)   //not  sure  how  to  test  this
        //to do: operator overloads
        
        .def("unarray", &TypeDesc::unarray)

        //to do: static members        
    ;

}

} // namespace PyOpenImageIO

