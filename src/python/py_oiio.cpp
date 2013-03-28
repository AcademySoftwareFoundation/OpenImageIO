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

// This our stand-in ProgressCallback which allows oiio to call a Python function.
bool PythonProgressCallback(void* opaque_data, float portion_done)
{
	// When oiio calls our ProgressCallback, it will give back the opaque_data which
	// we can turn back into a Python object, which will turn out to be the
	// function we are supposed to call.
	boost::python::object* obj = reinterpret_cast<boost::python::object*>(opaque_data);

	// One-liner to call the function and return a result as a bool
	return boost::python::extract<bool>((*obj)(portion_done));
}

// This is standing in for a method from oiio which takes a ProgressCallback.
void progress_callback_example_original(ProgressCallback pc, void* opaque_data)
{
	for (float f = 0.0; f < 10.0; ++f)
	{
		bool result = pc(opaque_data, f);
		if (!result)
		{
			std::cout << "Callback example terminated at " << f << std::endl;
			return;
		}
	}
}

// This is a wrapper for the above stand-in oiio function.
void progress_callback_wrapper(object progress_callback)
{
	// Casting the object to a pointer is kind of dangerious, but safe in this case
	// because we know its lifetime will be tied to this function.
	progress_callback_example_original(&PythonProgressCallback, &progress_callback);
}


object create_array(int length)
{
	// Make some dummy data - this would normally be coming
	// from oiio, and wouldn't necessarily be int. In fact,
	// it would usually be void - we'd have to determine the
	// desired type from the TypeDesc.
	int* test = new int[length];
	for (int i = 0; i < length; ++i) test[i] = i;

	// Import the Python array module and instantly wrap it
	// in a Boost.Python handle. This means BP will take care
	// of reference-counting Python objects for us.
	object module(handle<>(PyImport_ImportModule("array")));
	
	// This is Boost.Python's way of finding and calling a
	// function within the module. It's a bit like doing
	// >>> import array as module
	// >>> array = getattr(module, "array")("i")
	object array = module.attr("array")("i");

	// Now for something ugly. The array module doesn't
	// provide a convenient way to construct an array
	// from C, so we will abuse the fromstring method.
	// First we create a Python string using the most
	// direct method available to us (i.e. avoiding boost)
	// and then we pass that into the fromstring method.
	// It only actually results in one extra copy so it's
	// not all that bad.
	object data(
		handle<>(
#if PY_MAJOR_VERSION >= 3
			PyBytes_FromStringAndSize(
#else
			PyString_FromStringAndSize(
#endif
					reinterpret_cast<const char*>(test),
					sizeof(int) * length)));
#if (PY_MAJOR_VERSION < 3) || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 2)
	array.attr("fromstring")(data);
#else
	array.attr("frombytes")(data);
#endif

	// Tidy up and prove to ourselves that the returned
	// array really stands on its own.
	delete[] test;

	return array;
}

// OIIO often expects the user to allocate an array, pass in
// the pointer and have that array filled. Python provides
// the buffer interface for that. The array module is one way
// of creating a buffer. If an array is passed into this
// function, it will be treated as an int array and filled
// with int data. So it's up to the user to pass the right
// kind of array, of the right size - just like the OIIO C++
// interface.
void fill_array(const object& buffer)
{
	// We'll pretend it's an int array but we don't actually
	// know - it's just an area of memory.
	int* array;
	Py_ssize_t length;
	int success = PyObject_AsWriteBuffer(buffer.ptr(), reinterpret_cast<void**>(&array), &length);

	// throw_error_already_set is Boost.Python's way of
	// throwing Python exceptions from within C++.
	if (success != 0) throw_error_already_set();

	// Fill the buffer with dummy data.
	for (int i = 0; i < length / static_cast<int>(sizeof(int)); ++i)
	{
		array[i] = i;
	}
}

void print_array(const object& buffer)
{
	using namespace std;

	object module(handle<>(PyImport_ImportModule("array")));
	
	int isArray = PyObject_IsInstance(buffer.ptr(), object(module.attr("array")).ptr());
	if (isArray == -1) throw_error_already_set();

	char type = 'i';
	int size = sizeof(int);

	if (isArray) {
		type = extract<string>(buffer.attr("typecode"))()[0];
		size = extract<int>(buffer.attr("itemsize"));
	}

	const void* array;
	Py_ssize_t length;
	int success = PyObject_AsReadBuffer(buffer.ptr(), &array, &length);

    if (success != 0) boost::python::throw_error_already_set();

	switch (type)
	{
	case 'i':
		for (int i = 0; i < length / size; ++i) cout << ((int*)array)[i] << endl;
		break;
	case 'f':
		for (int i = 0; i < length / size; ++i) cout << ((float*)array)[i] << endl;
		break;
	case 'c':
		for (int i = 0; i < length / size; ++i) cout << ((char*)array)[i] << endl;
		break;
	default:
		throw std::runtime_error("Can't print this array type");
	}
}

bool PyProgressCallback(void *function, float data) {
    boost::python::object *func = reinterpret_cast<boost::python::object*>(function);
    return boost::python::extract<bool>((*func)(data));
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

// This OIIO_DECLARE_PYMODULE mojo is necessary if we want to pass in the
// MODULE name as a #define. Google for Argument-Prescan for additional
// info on why this is necessary

#define OIIO_DECLARE_PYMODULE(x) BOOST_PYTHON_MODULE(x) 

OIIO_DECLARE_PYMODULE(OIIO_PYMODULE_NAME) {
    boost::python::to_python_converter<
        ustring,
        ustring_to_python_str>();

    ustring_from_python_str();

    declare_imageinput();
    declare_imagespec();
    declare_imageoutput();
    declare_typedesc();
    declare_imagecache();
    declare_imagebuf();
    declare_paramvalue();
	def("progress_callback_example", &progress_callback_wrapper);   
	def("create_array", &create_array);
	def("fill_array",   &fill_array);
	def("print_array",  &print_array);
}

} // namespace PyOpenImageIO

