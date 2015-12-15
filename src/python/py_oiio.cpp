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


struct ustring_to_python_str {
    static PyObject* convert(ustring const& s) {
        return boost::python::incref(boost::python::object(s.string()).ptr());
    }
};

struct ustring_from_python_str
{
    ustring_from_python_str() {
        boost::python::converter::registry::push_back(
                &convertible,
                &construct,
                boost::python::type_id<ustring>());
    }

    static void* convertible(PyObject* obj_ptr) {
#if PY_MAJOR_VERSION >= 3
        if (!PyUnicode_Check(obj_ptr)) return 0;
#else
        if (!PyString_Check(obj_ptr)) return 0;
#endif
        return obj_ptr;
    }

    static void construct(
            PyObject* obj_ptr,
            boost::python::converter::rvalue_from_python_stage1_data* data) {
#if PY_MAJOR_VERSION >= 3
        PyObject* pyStr = PyUnicode_AsUTF8String(obj_ptr);
        const char* value = PyBytes_AsString(pyStr);
#else
        const char* value = PyString_AsString(obj_ptr);
#endif
        if (value == 0) boost::python::throw_error_already_set();
        void* storage = (
                (boost::python::converter::rvalue_from_python_storage<ustring>*)
                data)->storage.bytes;
        new (storage) ustring(value);
        data->convertible = storage;
    }
};



bool
oiio_attribute_int (const std::string &name, int val)
{
    return OIIO::attribute (name, val);
}


bool
oiio_attribute_float (const std::string &name, float val)
{
    return OIIO::attribute (name, val);
}


bool
oiio_attribute_string (const std::string &name, const std::string &val)
{
    return OIIO::attribute (name, val);
}



bool
oiio_attribute_typed (const std::string &name, TypeDesc type, object &obj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate)
            return OIIO::attribute (name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate)
            return OIIO::attribute (name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate) {
            std::vector<ustring> u;
            for (size_t i = 0, e = vals.size(); i < e; ++i)
                u.push_back (ustring(vals[i]));
            return OIIO::attribute (name, type, &u[0]);
        }
        return false;
    }
    return false;
}



bool
oiio_attribute_tuple_typed (const std::string &name,
                            TypeDesc type, tuple &obj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate)
            return OIIO::attribute (name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate)
            return OIIO::attribute (name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector (vals, obj);
        if (vals.size() == type.numelements()*type.aggregate) {
            std::vector<ustring> u;
            for (size_t i = 0, e = vals.size(); i < e; ++i)
                u.push_back (ustring(vals[i]));
            return OIIO::attribute (name, type, &u[0]);
        }
        return false;
    }
    return false;
}



static object
oiio_getattribute_typed (const std::string &name, TypeDesc type)
{
    if (type == TypeDesc::UNKNOWN)
        return object();
    char *data = OIIO_ALLOCA(char, type.size());
    if (OIIO::getattribute (name, type, data)) {
        if (type.basetype == TypeDesc::INT) {
#if PY_MAJOR_VERSION >= 3
            return C_to_val_or_tuple ((const int *)data, type, PyLong_FromLong);
#else
            return C_to_val_or_tuple ((const int *)data, type, PyInt_FromLong);
#endif
        }
        if (type.basetype == TypeDesc::FLOAT) {
            return C_to_val_or_tuple ((const float *)data, type, PyFloat_FromDouble);
        }
        if (type.basetype == TypeDesc::STRING) {
#if PY_MAJOR_VERSION >= 3
            return C_to_val_or_tuple ((const char **)data, type, PyUnicode_FromString);
#else
            return C_to_val_or_tuple ((const char **)data, type, PyString_FromString);
#endif
        }
    }
    return object();
}




// This OIIO_DECLARE_PYMODULE mojo is necessary if we want to pass in the
// MODULE name as a #define. Google for Argument-Prescan for additional
// info on why this is necessary

#define OIIO_DECLARE_PYMODULE(x) BOOST_PYTHON_MODULE(x) 

OIIO_DECLARE_PYMODULE(OIIO_PYMODULE_NAME) {
    // Conversion back and forth to ustring
    boost::python::to_python_converter<
        ustring,
        ustring_to_python_str>();
    ustring_from_python_str();

    // Basic helper classes
    declare_typedesc();
    declare_paramvalue();
    declare_imagespec();
    declare_roi();
    declare_deepdata();

    // Main OIIO I/O classes
    declare_imageinput();
    declare_imageoutput();
    declare_imagebuf();
    declare_imagecache();

    declare_imagebufalgo();
    
    // Global (OpenImageIO scope) functiona and symbols
    def("geterror",     &OIIO::geterror);
    def("attribute",    &oiio_attribute_float);
    def("attribute",    &oiio_attribute_int);
    def("attribute",    &oiio_attribute_string);
    def("attribute",    &oiio_attribute_typed);
    def("attribute",    &oiio_attribute_tuple_typed);
    def("getattribute",         &oiio_getattribute_typed);
    scope().attr("AutoStride") = AutoStride;
    scope().attr("openimageio_version") = OIIO_VERSION;
    scope().attr("VERSION") = OIIO_VERSION;
    scope().attr("VERSION_STRING") = OIIO_VERSION_STRING;
    scope().attr("VERSION_MAJOR") = OIIO_VERSION_MAJOR;
    scope().attr("VERSION_MINOR") = OIIO_VERSION_MINOR;
    scope().attr("VERSION_PATCH") = OIIO_VERSION_PATCH;
    scope().attr("INTRO_STRING") = OIIO_INTRO_STRING;

    boost::python::numeric::array::set_module_and_type("array", "array");
}

} // namespace PyOpenImageIO

