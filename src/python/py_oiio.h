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

#ifndef PYOPENIMAGEIO_PY_OIIO_H
#define PYOPENIMAGEIO_PY_OIIO_H

// Avoid a compiler warning from a duplication in tiffconf.h/pyconfig.h
#undef SIZEOF_LONG
#include <boost/python.hpp>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/deepdata.h>


#if PY_MAJOR_VERSION < 2 || (PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5)
#define Py_ssize_t int
#endif

namespace PyOpenImageIO
{

using namespace boost::python;

using namespace OIIO;

void declare_imagespec(); 
void declare_imageinput();
void declare_imageoutput();
void declare_typedesc();
void declare_roi();
void declare_deepdata();
void declare_imagecache();
void declare_imagebuf();
void declare_imagebufalgo();
void declare_paramvalue();
void declare_global();

bool PyProgressCallback(void*, float);
object C_array_to_Python_array (const char *data, TypeDesc type, size_t size);
const char * python_array_code (TypeDesc format);
TypeDesc typedesc_from_python_array_code (char code);
std::string object_classname (const object& obj);


// Given python array 'data', figure out its element type and number of
// elements, and return the memory address of its contents.  Return NULL as
// the address for an error.
const void * python_array_address (const object &data, TypeDesc &elementtype,
                                   size_t &numelements);



// Suck up one or more presumed T values into a vector<T>
template<typename T>
void py_to_stdvector (std::vector<T> &vals, const object &obj)
{
    extract<const tuple&> tup (obj);
    if (tup.check()) {
        // tuple case: recurse
        for (int i = 0, e = len(tup()); i < e; ++i)
            py_to_stdvector<T> (vals, tup()[i]);
    } else {
        // non-tuple case (presumably scalar)
        extract<T> t (obj);
        vals.push_back (t.check() ? t() : T());
    }
}


// Suck up a tuple of presumed T values into a vector<T>
template<typename T>
void py_to_stdvector (std::vector<T> &vals, const tuple &tup)
{
    for (int i = 0, e = len(tup); i < e; ++i)
        py_to_stdvector<T> (vals, tup[i]);
}



// Convert an array of T values into either tuple. FUNC is a conversion
// function such as PyInt_FromLong, PyFloat_FromDouble, or
// PyString_FromString.
template<typename T, typename FUNC>
object C_to_tuple (const T *vals, int size, FUNC f)
{
    PyObject* result = PyTuple_New (size);
    for (int i = 0;  i < size;  ++i)
        PyTuple_SetItem(result, i, f(vals[i]));
    return object(handle<>(result));
}



// Convert an array of T values (described by type) into either a simple
// Python object (if it's an int, float, or string and a SCALAR) or a
// Python tuple.  FUNC is a conversion function such as PyInt_FromLong,
// PyFloat_FromDouble, or PyString_FromString.
template<typename T, typename FUNC>
object C_to_val_or_tuple (const T *vals, TypeDesc type, FUNC f)
{
    if (type.arraylen == 0 && type.aggregate == TypeDesc::SCALAR) {
        // scalar case
        return object (vals[0]);
    }
    // Array/aggregate case -- return a tuple
    int size = type.numelements() * type.aggregate;
    return C_to_tuple (vals, size, f);
}



template<typename T>
void
attribute_typed (T &myobj, string_view name, TypeDesc type, object &dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute (name, type, &u[0]);
        }
        return;
    }
}




template<typename T>
void
attribute_tuple_typed (T &myobj, string_view name,
                       TypeDesc type, tuple &dataobj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate)
            myobj.attribute (name, type, &vals[0]);
        return;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector (vals, dataobj);
        if (vals.size() == type.numelements()*type.aggregate) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            myobj.attribute (name, type, &u[0]);
        }
        return;
    }
}



template<typename T>
object
getattribute_typed (const T& obj, string_view name, TypeDesc type)
{
    if (type == TypeDesc::UNKNOWN)
        return object();   // require a type
    char *data = OIIO_ALLOCA (char, type.size());
    bool ok = obj.getattribute (name, type, data);
    if (! ok)
        return object();   // None
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
    return object();
}




// Helper class to release the GIL, allowing other Python threads to
// proceed, then re-acquire it again when the scope ends.
class ScopedGILRelease {
public:
    ScopedGILRelease () : m_thread_state(PyEval_SaveThread()) { }
    ~ScopedGILRelease () { PyEval_RestoreThread (m_thread_state); }
private:
    PyThreadState *m_thread_state;
};



class ImageInputWrap {
private:
    /// Friend declaration for ImageOutputWrap::copy_image
    friend class ImageOutputWrap;
    ImageInput *m_input;
public:
	virtual ~ImageInputWrap();
    static object create(const std::string&, const std::string&);
    static object open_static_regular(const std::string&);
    static object open_static_with_config(const std::string&,const ImageSpec&);
    const char *format_name () const;
    bool valid_file (const std::string &name) const;
    bool open_regular (const std::string &name);
    bool open_with_config(const std::string &name, const ImageSpec &config);
    const ImageSpec &spec() const;
    int supports (const std::string &feature) const;
    bool close();
    int current_subimage() const;
    int current_miplevel() const;
    bool seek_subimage (int, int);
    object read_image (int chbegin, int chend, TypeDesc);
    object read_scanline (int y, int z, TypeDesc format);
    object read_scanlines (int ybegin, int yend, int z,
                           int chbegin, int chend, TypeDesc format);
    object read_tile (int x, int y, int z, TypeDesc format);
    object read_tiles (int xbegin, int xend, int ybegin, int yend,
                       int zbegin, int zend, int chbegin, int chend,
                       TypeDesc format);
    object read_native_deep_scanlines (int ybegin, int yend, int z,
                                       int chbegin, int chend);
    object read_native_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                                   int zbegin, int zend, int chbegin, int chend);
    object read_native_deep_image ();
    std::string geterror() const;
};


class ImageOutputWrap {
private:
    ImageOutput *m_output;
    const void *make_read_buffer (object &buffer, imagesize_t size);
public:
    virtual ~ImageOutputWrap();
    static boost::python::object create(const std::string&, const std::string&);
    const ImageSpec &spec() const;
    bool open (const std::string&, const ImageSpec&, ImageOutput::OpenMode);
    bool open_specs (const std::string&, tuple &specs);
    bool close();
    bool write_scanline (int, int, TypeDesc, boost::python::object&,
                         stride_t xstride=AutoStride);
    bool write_scanline_bt (int, int, TypeDesc::BASETYPE,
                            boost::python::object&, stride_t xstride=AutoStride);
    bool write_scanline_array (int, int, object&);
    bool write_scanlines (int, int, int, TypeDesc, boost::python::object&,
                         stride_t xstride=AutoStride);
    bool write_scanlines_bt (int, int, int, TypeDesc::BASETYPE,
                            boost::python::object&, stride_t xstride=AutoStride);
    bool write_scanlines_array (int, int, int, object&);
    bool write_tile (int, int, int, TypeDesc, boost::python::object&,
                     stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride);
    bool write_tile_bt (int, int, int, TypeDesc::BASETYPE,
                        boost::python::object&, stride_t xstride=AutoStride,
                        stride_t ystride=AutoStride,
                        stride_t zstride=AutoStride);
    bool write_tile_array (int, int, int, object&);
    bool write_tiles (int, int, int, int, int, int,
                      TypeDesc, boost::python::object&,
                      stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                      stride_t zstride=AutoStride);
    bool write_tiles_bt (int, int, int, int, int, int,
                         TypeDesc::BASETYPE, boost::python::object&,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride,
                         stride_t zstride=AutoStride);
    bool write_tiles_array (int, int, int, int, int, int, object&);
    bool write_image (TypeDesc format, object &buffer,
                      stride_t xstride=AutoStride,
                      stride_t ystride=AutoStride,
                      stride_t zstride=AutoStride);
    bool write_image_bt (TypeDesc::BASETYPE basetype, object &buffer,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride,
                         stride_t zstride=AutoStride);
    bool write_image_array (object &buffer);
    bool write_deep_scanlines (int ybegin, int yend, int z,
                               const DeepData &deepdata);
    bool write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend, const DeepData &deepdata);
    bool write_deep_image (const DeepData &deepdata);
    bool copy_image (ImageInputWrap *iiw);
    const char *format_name () const;
    int supports (const std::string&) const;
    std::string geterror()const;
};


class ImageCacheWrap {
private:
    ImageCache *m_cache;
public:
    static ImageCacheWrap *create (bool);
    static void destroy (ImageCacheWrap*);
    void attribute_int    (const std::string&, int );
    void attribute_float  (const std::string&, float);
    void attribute_string (const std::string&, const std::string&);
    void attribute_typed  (const std::string&, TypeDesc, object &obj);
    void attribute_tuple_typed (const std::string&, TypeDesc, tuple &obj);
    object getattribute_typed (const std::string&, TypeDesc);
    std::string resolve_filename (const std::string& filename);
    // object get_image_info (const std::string &filename, int subimage,
    //                        int miplevel, const std::string &dataname,
    //                        TypeDesc datatype);
    // object get_imagespec (const std::string &filename, int subimage=0,
    //                       int miplevel=0, bool native=false);
    object get_pixels (const std::string &filename,
                       int subimage, int miplevel, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend,
                       TypeDesc datatype);

    //First needs to be exposed to python in imagecache.cpp
    /*
    Tile *get_tile (ustring filename, int subimage,
                    int x, int y, int z) {
        return m_cache->get_tile(filename, subimage, x, y, z);
    }
    void release_tile (Tile *tile) const {
        m_cache->release-tile(tile);
    }
    const void *tile_pixels (Tile *tile, TypeDesc &format) const {
        m_cache->tile_pixels(tile, format);
    }        
    */

    std::string geterror () const;
    std::string getstats (int) const;
    void invalidate (ustring);
    void invalidate_all (bool);
};



} // namespace PyOpenImageIO

#endif // PYOPENIMAGEIO_PY_OIIO_H
