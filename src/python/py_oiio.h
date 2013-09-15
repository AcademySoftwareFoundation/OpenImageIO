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

#include "imageio.h"
#include "typedesc.h"
#include "imagecache.h"
#include "imagebuf.h"


#if PY_MAJOR_VERSION < 2 || (PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5)
#define Py_ssize_t int
#endif

namespace PyOpenImageIO
{

using namespace boost::python;

OIIO_NAMESPACE_USING

void declare_imagespec(); 
void declare_imageinput();
void declare_imageoutput();
void declare_typedesc();
void declare_imagecache();
void declare_imagebuf();
void declare_paramvalue();
void declare_global();

bool PyProgressCallback(void*, float);


// Suck up one or more presumed T values into a vector<T>
template<typename T>
static void py_to_stdvector (std::vector<T> &vals, const object &obj)
{
    extract<const tuple&> tup (obj);
    if (tup.check()) {
        // tuple case: recurse
        for (int i = 0, e = len(tup()); i < e; ++i)
            py_to_stdvector<T> (vals, tup()[i]);
    } else {
        // non-tuple case
        extract<T> t (obj);
        vals.push_back (t.check() ? t() : T());
    }
}


// Suck up a tuple of presumed T values into a vector<T>
template<typename T>
static void py_to_stdvector (std::vector<T> &vals, const tuple &tup)
{
    for (int i = 0, e = len(tup); i < e; ++i)
        py_to_stdvector<T> (vals, tup[i]);
}



// Convert an array of T values (described by type) into either a simple
// Python object (if it's an int, float, or string and a SCALAR) or a
// Python tuple.  FUNC is a conversion function such as PyInt_FromLong,
// PyFloat_FromDouble, or PyString_FromString.
template<typename T, typename FUNC>
static object C_to_val_or_tuple (const T *vals, TypeDesc type, FUNC f)
{
    if (type.arraylen == 0 && type.aggregate == TypeDesc::SCALAR) {
        // scalar case
        return object (vals[0]);
    }
    // Array/aggregate case -- return a tuple
    int size = type.numelements() * type.aggregate;
    PyObject* result = PyTuple_New (size);
    for (int i = 0;  i < size;  ++i)
        PyTuple_SetItem(result, i, f(vals[i]));
    return object(handle<>(result));
}



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
    bool supports (const std::string &feature) const;
    bool close();
    int current_subimage() const;
    int current_miplevel() const;
    bool seek_subimage (int, int);
    object read_image (TypeDesc);
    object read_scanline (int y, int z, TypeDesc format);
    object read_scanlines (int ybegin, int yend, int z,
                           int chbegin, int chend, TypeDesc format);
    object read_tile (int x, int y, int z, TypeDesc format);
    object read_tiles (int xbegin, int xend, int ybegin, int yend,
                       int zbegin, int zend, int chbegin, int chend,
                       TypeDesc format);
    std::string geterror() const;
};


class ImageOutputWrap {
private:
    friend class ImageBufWrap;
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
    bool write_scanlines (int, int, int, TypeDesc, boost::python::object&,
                         stride_t xstride=AutoStride);
    bool write_scanlines_bt (int, int, int, TypeDesc::BASETYPE,
                            boost::python::object&, stride_t xstride=AutoStride);
    bool write_tile (int, int, int, TypeDesc, boost::python::object&,
                     stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride);
    bool write_tile_bt (int, int, int, TypeDesc::BASETYPE,
                        boost::python::object&, stride_t xstride=AutoStride,
                        stride_t ystride=AutoStride,
                        stride_t zstride=AutoStride);
    bool write_tiles (int, int, int, int, int, int,
                      TypeDesc, boost::python::object&,
                      stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                      stride_t zstride=AutoStride);
    bool write_tiles_bt (int, int, int, int, int, int,
                         TypeDesc::BASETYPE, boost::python::object&,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride,
                         stride_t zstride=AutoStride);
    bool write_image (TypeDesc format, object &buffer,
                      stride_t xstride=AutoStride,
                      stride_t ystride=AutoStride,
                      stride_t zstride=AutoStride);
    bool write_image_bt (TypeDesc::BASETYPE basetype, object &buffer,
                         stride_t xstride=AutoStride,
                         stride_t ystride=AutoStride,
                         stride_t zstride=AutoStride);
    bool copy_image (ImageInputWrap *iiw);
    const char *format_name () const;
    bool supports (const std::string&) const;
    std::string geterror()const;
};


class ImageCacheWrap {
private:
    friend class ImageBufWrap;
    ImageCache *m_cache;
public:
    static ImageCacheWrap *create (bool);
    static void destroy (ImageCacheWrap*);
    void clear ();     
    bool attribute (const std::string&, TypeDesc, const void*);    
    bool attribute_int    (const std::string&, int );
    bool attribute_float  (const std::string&, float);
    bool attribute_double (const std::string&, double);
    bool attribute_char   (const std::string&, const char*);
    bool attribute_string (const std::string&, const std::string&);
    bool getattribute(const std::string&, TypeDesc, void*);
    bool getattribute_int (const std::string&, int&);
    bool getattribute_float(const std::string&, float&);
    bool getattribute_double(const std::string&, double&);
    bool getattribute_char(const std::string&, char**);    
    bool getattribute_string(const std::string&, std::string&);
    std::string resolve_filename (const std::string&);
    bool get_image_info (ustring, ustring, TypeDesc, void*);
    bool get_imagespec(ustring, ImageSpec&, int);
    bool get_pixels (ustring, int, int, int, int, int, int, 
                     int, int, TypeDesc, void*);

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


class ImageBufWrap {
private:
    ImageBuf *m_buf;
public:

    ImageBufWrap (const std::string&name = std::string(),
                  ImageCacheWrap*icw = NULL);
    ImageBufWrap (const std::string&, const ImageSpec&) ;  
    void clear ();
    void reset_to_new_image (const std::string&, ImageCache*);
    void reset_to_blank_image (const std::string&, const ImageSpec&);
    void alloc (const ImageSpec&);
    // TODO: How to wrap ProgressCallback?
    /*
    bool read (int subimage=0, bool force=false, 
                TypeDesc convert=TypeDesc::UNKNOWN,
                ProgressCallback progress_callback=NULL,
                void *progress_callback_data=NULL);        
       
    bool save (const std::string &filename = std::string(),
                const std::string &fileformat = std::string(),
                ProgressCallback progress_callback=NULL,
                void *progress_callback_data=NULL) const;

    bool write (ImageOutputWrap *out,
                ProgressCallback progress_callback=NULL,
                void *progress_callback_data=NULL) const;  
    */

    bool init_spec (const std::string&, int, int);
    const ImageSpec &spec() const;
    const std::string &name() const;
    const std::string &file_format_name() const;
    int subimage() const;
    int nsubimages() const;
    int nchannels() const;
    float getchannel (int, int, int, int) const;
    void getpixel (int, int, float*, int) const;
    void interppixel (float, float, float*) const;
    void interppixel_NDC (float, float, float*) const;
    void setpixel_xy (int, int, const float*, int);
    void setpixel_i (int, const float*, int);

    // These copy_pixel methods require the user to send an appropriate
    // area of memory ("*result"), which would make little sense from Python.
    // The user *could* create an appropriately sized array in Python by
    // filling it with the correct amount of dummy data, but would
    // this defeat the purpose of Python? Instead, the wrapper could
    // allocate that array, fill it, and return it to Python. This is the way
    // ImageInput.read_image() was wrapped.
    bool get_pixels (int, int, int, int, int, int, TypeDesc, void*) const;

    // TODO: handle T and <T>. Don't know how to handle this with B.P, 
    // though haven't given it much thought yet.
    /*
    bool get_pixels_convert (int xbegin, int xend, int ybegin, int yend,
                                T *result) const; 
    bool get_pixels_convert_safer (int xbegin, int xend, int ybegin,
                                int yend, std::vector<T> &result) const;
    */
    
    int orientation() const; 
    int oriented_width() const;
    int oriented_height() const;
    int oriented_x() const;
    int oriented_y() const;
    int oriented_full_width() const;
    int oriented_full_height() const;
    int oriented_full_x() const;
    int oriented_full_y() const;
    int xbegin() const;
    int xend() const;
    int ybegin() const;
    int yend() const;
    int xmin();
    int xmax();
    int ymin();
    int ymax();
    void zero();
    bool pixels_valid () const;
    bool localpixels () const;
    //TODO: class Iterator and ConstIterator

    std::string geterror()const;
};


} // namespace PyOpenImageIO

#endif // PYOPENIMAGEIO_PY_OIIO_H
