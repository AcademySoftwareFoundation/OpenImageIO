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
using boost::python::make_tuple;
using namespace std;

template <typename BaseType>
object ParamValue_convert(const TypeDesc& t, int n, const BaseType* data) {

    switch (t.aggregate) {
    case TypeDesc::SCALAR:   return object(data[n]);
    case TypeDesc::VEC2:     return make_tuple(data[n*2], data[n*2+1]);
    case TypeDesc::VEC3:     return make_tuple(data[n*3], data[n*3+1],
                                               data[n*3+2]);
    case TypeDesc::VEC4:     return make_tuple(data[n*4], data[n*4+1],
                                               data[n*4+2], data[n*4+3]);
    // Bypass the make_tuple argument list size limit by making two
    // tuples and adding them. Inefficient, but not likely to be a bottleneck.
    // If it turns out we need efficient access to this stuff we should look
    // at an array/ctypes interface.
    case TypeDesc::MATRIX44: return make_tuple(
            data[n*16+0],  data[n*16+1],  data[n*16+2],  data[n*16+3],
            data[n*16+4],  data[n*16+5],  data[n*16+6],  data[n*16+7]) +
    make_tuple(data[n*16+8],  data[n*16+9],  data[n*16+10], data[n*16+11],
               data[n*16+12], data[n*16+13], data[n*16+14], data[n*16+15]);
    default:
        PyErr_SetString(PyExc_TypeError,
            "Unable to convert ParamValue with unknown TypeDesc");
        throw_error_already_set();
    }
    return object();

}

object ParamValue_getitem(const ParamValue& self, int n) {
    if (n >= self.nvalues()) {
        PyErr_SetString(PyExc_IndexError, "ParamValue index out of range");
        throw_error_already_set();
    }

    TypeDesc t = self.type();

#define ParamValue_convert_dispatch(TYPE) \
    case TypeDesc::TYPE: \
    return ParamValue_convert(t,n,(CType<TypeDesc::TYPE>::type*)self.data());

    switch (t.basetype) {
    ParamValue_convert_dispatch(UCHAR)
    ParamValue_convert_dispatch(CHAR)
    ParamValue_convert_dispatch(USHORT)
    ParamValue_convert_dispatch(SHORT)
    ParamValue_convert_dispatch(UINT)
    ParamValue_convert_dispatch(INT)
    ParamValue_convert_dispatch(ULONGLONG)
    ParamValue_convert_dispatch(LONGLONG)
#ifdef _HALF_H_
    ParamValue_convert_dispatch(HALF)
#endif
    ParamValue_convert_dispatch(FLOAT)
    ParamValue_convert_dispatch(DOUBLE)
    case TypeDesc::STRING:
        return ParamValue_convert(t, n, (ustring*)self.data());
    default: return object();
    }

#undef ParamValue_convert_dispatch
}


static std::string
ParamValue_name(const ParamValue& self)
{
    return self.name().string();
}


static object
ParamValue_value (const ParamValue& self)
{
    return ParamValue_getitem (self, 0);
}


ParamValue& ParamValueList_getitem(ParamValueList& self, int i)
{
    return self[i];
}


void ParamValueList_push_back (ParamValueList& self, const ParamValue& p)
{
    self.push_back (p);
}


void ParamValueList_clear (ParamValueList& self)
{
    self.clear();
}


void ParamValueList_resize (ParamValueList& self, size_t s)
{
    self.resize (s);
}


size_t ParamValueList_size (ParamValueList& self)
{
    return self.size ();
}



void declare_paramvalue()
{

   enum_<ParamValue::Interp>("Interp")
       .value("INTERP_CONSTANT", ParamValue::INTERP_CONSTANT)
       .value("INTERP_PERPIECE", ParamValue::INTERP_PERPIECE)
       .value("INTERP_LINEAR",   ParamValue::INTERP_LINEAR)
       .value("INTERP_VERTEX",   ParamValue::INTERP_VERTEX)
   ;

   class_<ParamValue>("ParamValue")
       .add_property("name",     &ParamValue_name)
       .add_property("type",     &ParamValue::type)
       .add_property("value",    &ParamValue_value)
       .def("__getitem__",       &ParamValue_getitem)
       .def("__len__",           &ParamValue::nvalues)
       .def(init<const std::string&, int>())
       .def(init<const std::string&, float>())
       .def(init<const std::string&, const std::string&>())
   ;

    class_<ParamValueList>("ParamValueList")
        .def("__getitem__", &ParamValueList_getitem,
            return_internal_reference<>())
        .def("__iter__", boost::python::iterator<ParamValueList>())
        .def("__len__",     &ParamValueList_size)
        .def("grow",        &ParamValueList::grow,
            return_internal_reference<>())
        .def("append",      &ParamValueList_push_back)
        .def("clear",       &ParamValueList_clear)
        .def("free",        &ParamValueList::free)
        .def("resize",      &ParamValueList_resize)
    ;
}

}
