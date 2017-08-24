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



const char *
python_array_code (TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::UINT8 :  return "B";
    case TypeDesc::INT8 :   return "b";
    case TypeDesc::UINT16 : return "H";
    case TypeDesc::INT16 :  return "h";
    case TypeDesc::UINT32 : return "I";
    case TypeDesc::INT32 :  return "i";
    case TypeDesc::FLOAT :  return "f";
    case TypeDesc::DOUBLE : return "d";
    case TypeDesc::HALF :   return "H";  // Return half in uint16
    default :
        // For any other type, including UNKNOWN, pack it into an
        // unsigned byte array.
        return "B";
    }
}



TypeDesc
typedesc_from_python_array_code (char code)
{
    switch (code) {
    case 'b' :
    case 'c' : return TypeDesc::INT8;
    case 'B' : return TypeDesc::UINT8;
    case 'h' : return TypeDesc::INT16;
    case 'H' : return TypeDesc::UINT16;
    case 'i' : return TypeDesc::INT;
    case 'I' : return TypeDesc::UINT;
    case 'l' : return TypeDesc::INT;
    case 'L' : return TypeDesc::UINT;
    case 'f' : return TypeDesc::FLOAT;
    case 'd' : return TypeDesc::DOUBLE;
    }
    return TypeDesc::UNKNOWN;
}



std::string
object_classname (const object& obj)
{
    return extract<std::string>(obj.attr("__class__").attr("__name__"));
}



object
C_array_to_Python_array (const char *data, TypeDesc type, size_t size)
{
    // Figure out what kind of array to return and create it
    object arr_module(handle<>(PyImport_ImportModule("array")));
    object array = arr_module.attr("array")(python_array_code(type));

    // Create a Python byte array (or string for Python2) to hold the
    // data.
#if PY_MAJOR_VERSION >= 3
    object string_py(handle<>(PyBytes_FromStringAndSize(data, size)));
#else
    object string_py(handle<>(PyString_FromStringAndSize(data, size)));
#endif

    // Copy the data from the string to the array, then return the array.
#if (PY_MAJOR_VERSION < 3) || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 2)
    array.attr("fromstring")(string_py);
#else
    array.attr("frombytes")(string_py);
#endif
    return array;
}



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
            for (auto& val : vals)
                u.emplace_back(val);
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
            for (auto& val : vals)
                u.emplace_back(val);
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


static int
oiio_get_int_attribute (const char *name)
{
    return OIIO::get_int_attribute (name);
}


static int
oiio_get_int_attribute_d (const char *name, int defaultval)
{
    return OIIO::get_int_attribute (name, defaultval);
}


static float
oiio_get_float_attribute (const char *name)
{
    return OIIO::get_float_attribute (name);
}


static float
oiio_get_float_attribute_d (const char *name, float defaultval)
{
    return OIIO::get_float_attribute (name, defaultval);
}


static std::string
oiio_get_string_attribute (const char *name)
{
    return OIIO::get_string_attribute (name);
}


static std::string
oiio_get_string_attribute_d (const char *name, const char *defaultval)
{
    return OIIO::get_string_attribute (name, defaultval);
}





const void *
python_array_address (const object &data, TypeDesc &elementtype,
                      size_t &numelements)
{
    // Figure out the type of the array
    object tcobj;
    try {
        tcobj = data.attr("typecode");
    } catch(...) {
        return NULL;
    }
    if (! tcobj)
        return NULL;
    extract<char> tce (tcobj);
    char typecode = tce.check() ? (char)tce : 0;
    elementtype = typedesc_from_python_array_code (typecode);
    if (elementtype == TypeDesc::UNKNOWN)
        return NULL;

    // TODO: The PyObject_AsReadBuffer is a deprecated API dating from
    // Python 1.6 (see https://docs.python.org/2/c-api/objbuffer.html). It
    // still works in 2.x, but for future-proofing, we should switch to the
    // memory buffer interface:
    // https://docs.python.org/2/c-api/buffer.html#bufferobjects
    // https://docs.python.org/3/c-api/buffer.html
    const void *addr = NULL;
    Py_ssize_t pylen = 0;
    int success = PyObject_AsReadBuffer(data.ptr(), &addr, &pylen);
    if (success != 0) {
        throw_error_already_set();
        return NULL;
    }

    numelements = size_t(pylen) / elementtype.size();
    return addr;
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
    
    // Global (OpenImageIO scope) functions and symbols
    def("geterror",     &OIIO::geterror);
    def("attribute",    &oiio_attribute_float);
    def("attribute",    &oiio_attribute_int);
    def("attribute",    &oiio_attribute_string);
    def("attribute",    &oiio_attribute_typed);
    def("attribute",    &oiio_attribute_tuple_typed);
    def("get_int_attribute",    &oiio_get_int_attribute);
    def("get_int_attribute",    &oiio_get_int_attribute_d);
    def("get_float_attribute",  &oiio_get_float_attribute);
    def("get_float_attribute",  &oiio_get_float_attribute_d);
    def("get_string_attribute", &oiio_get_string_attribute);
    def("get_string_attribute", &oiio_get_string_attribute_d);
    def("getattribute",         &oiio_getattribute_typed);
    scope().attr("AutoStride") = AutoStride;
    scope().attr("openimageio_version") = OIIO_VERSION;
    scope().attr("VERSION") = OIIO_VERSION;
    scope().attr("VERSION_STRING") = OIIO_VERSION_STRING;
    scope().attr("VERSION_MAJOR") = OIIO_VERSION_MAJOR;
    scope().attr("VERSION_MINOR") = OIIO_VERSION_MINOR;
    scope().attr("VERSION_PATCH") = OIIO_VERSION_PATCH;
    scope().attr("INTRO_STRING") = OIIO_INTRO_STRING;

#if BOOST_VERSION < 106500
    boost::python::numeric::array::set_module_and_type("array", "array");
#endif
}

} // namespace PyOpenImageIO

