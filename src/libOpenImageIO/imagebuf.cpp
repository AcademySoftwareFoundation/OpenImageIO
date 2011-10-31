/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


/// \file
/// Implementation of ImageBuf class.


#include <iostream>

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>
#include <boost/scoped_ptr.hpp>

#include "imageio.h"
#include "imagebuf.h"
#include "imagecache.h"
#include "dassert.h"
#include "strutil.h"
#include "fmath.h"

OIIO_NAMESPACE_ENTER
{

ImageBuf::ImageBuf (const std::string &filename,
                    ImageCache *imagecache)
    : m_name(filename), m_nsubimages(0),
      m_current_subimage(-1), m_current_miplevel(-1),
      m_localpixels(false), m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1), 
      m_imagecache(imagecache)
{
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec)
    : m_name(filename), m_nsubimages(0),
      m_current_subimage(-1), m_current_miplevel(-1),
      m_localpixels(true), m_spec_valid(false), m_pixels_valid(false),
      m_badfile(false), m_orientation(1), m_pixelaspect(1),
      m_imagecache(NULL)
{
    alloc (spec);
}



ImageBuf::~ImageBuf ()
{
    // Do NOT destroy m_imagecache here -- either it was created
    // externally and passed to the ImageBuf ctr or reset() method, or
    // else init_spec requested the system-wide shared cache, which
    // does not need to be destroyed.
}



void
ImageBuf::clear ()
{
    m_name.clear ();
    m_fileformat.clear ();
    m_nsubimages = 0;
    m_current_subimage = -1;
    m_current_miplevel = -1;
    m_spec = ImageSpec ();
    m_nativespec = ImageSpec ();
    {
        std::vector<char> tmp;
        std::swap (m_pixels, tmp);  // clear it with deallocation
    }
    m_localpixels = false;
    m_spec_valid = false;
    m_pixels_valid = false;
    m_badfile = false;
    m_orientation = 1;
    m_pixelaspect = 1;
}



void
ImageBuf::reset (const std::string &filename, ImageCache *imagecache)
{
    clear ();
    m_name = ustring (filename);
    if (imagecache)
        m_imagecache = imagecache;
}



void
ImageBuf::reset (const std::string &filename, const ImageSpec &spec)
{
    clear ();
    m_name = ustring (filename);
    m_current_subimage = 0;
    m_current_miplevel = 0;
    alloc (spec);
}



void
ImageBuf::realloc ()
{
    size_t newsize = spec().image_bytes ();
    if (((int)m_pixels.size() - (int)newsize) > 4*1024*1024) {
        // If we are substantially shrinking, try to actually free
        // memory, which std::vector::resize does not do!
        std::vector<char> tmp;      // vector with 0 memory
        std::swap (tmp, m_pixels);  // Now tmp holds the mem, not m_pixels
        // As tmp leaves scope, it frees m_pixels's old memory
    }
    m_pixels.resize (newsize);
    m_localpixels = true;
#if 0
    std::cerr << "ImageBuf " << m_name << " local allocation: " << newsize << "\n";
#endif
}



void
ImageBuf::alloc (const ImageSpec &spec)
{
    m_spec = spec;
    m_nativespec = spec;
    m_spec_valid = true;
    realloc ();
}



bool
ImageBuf::init_spec (const std::string &filename, int subimage, int miplevel)
{
    if (m_current_subimage >= 0 && m_current_miplevel >= 0
            && m_name == filename && m_current_subimage == subimage
            && m_current_miplevel == miplevel)
        return true;   // Already done

    if (! m_imagecache) {
        m_imagecache = ImageCache::create (true /* shared cache */);
    }

    m_name = filename;
    m_nsubimages = 0;
    m_nmiplevels = 0;
    static ustring s_subimages("subimages"), s_miplevels("miplevels");
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_subimages,
                                  TypeDesc::TypeInt, &m_nsubimages);
    m_imagecache->get_image_info (m_name, subimage, miplevel, s_miplevels,
                                  TypeDesc::TypeInt, &m_nmiplevels);
    m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel);
    m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true);
    if (m_nsubimages) {
        m_badfile = false;
        m_spec_valid = true;
        m_orientation = m_spec.get_int_attribute ("orientation", 1);
        m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);
        m_current_subimage = subimage;
        m_current_miplevel = miplevel;
    } else {
        m_badfile = true;
        m_spec_valid = false;
        m_current_subimage = -1;
        m_current_miplevel = -1;
        m_err = m_imagecache->geterror ();
        // std::cerr << "ImageBuf ERROR: " << m_err << "\n";
    }

    return !m_badfile;
}



bool
ImageBuf::read (int subimage, int miplevel, bool force, TypeDesc convert,
               ProgressCallback progress_callback,
               void *progress_callback_data)
{
    if (pixels_valid() && !force &&
            subimage == this->subimage() && miplevel == this->miplevel())
        return true;

    if (! init_spec (m_name.string(), subimage, miplevel)) {
        m_badfile = true;
        m_spec_valid = false;
        return false;
    }

    // Set our current spec to the requested subimage
    if (! m_imagecache->get_imagespec (m_name, m_spec, subimage, miplevel) ||
        ! m_imagecache->get_imagespec (m_name, m_nativespec, subimage, miplevel, true)) {
        m_err = m_imagecache->geterror ();
        return false;
    }
    m_current_subimage = subimage;
    m_current_miplevel = miplevel;

#if 1
    // If we don't already have "local" pixels, and we aren't asking to
    // convert the pixels to a specific (and different) type, then take an
    // early out by relying on the cache.
    int peltype = TypeDesc::UNKNOWN;
    m_imagecache->get_image_info (m_name, subimage, miplevel,
                                  ustring("cachedpixeltype"),
                                  TypeDesc::TypeInt, &peltype);
    m_cachedpixeltype = TypeDesc ((TypeDesc::BASETYPE)peltype);
    if (! m_localpixels && ! force &&
        (convert == m_cachedpixeltype || convert == TypeDesc::UNKNOWN)) {
        m_spec.format = m_cachedpixeltype;
#ifdef DEBUG
        std::cerr << "read was not necessary -- using cache\n";
#endif
        return true;
    } else {
#ifdef DEBUG
        std::cerr << "going to have to read " << m_name << ": "
                  << m_spec.format.c_str() << " vs " << convert.c_str() << "\n";
#endif
    }        
#endif

    if (convert != TypeDesc::UNKNOWN)
        m_spec.format = convert;
    m_orientation = m_spec.get_int_attribute ("orientation", 1);
    m_pixelaspect = m_spec.get_float_attribute ("pixelaspectratio", 1.0f);

    realloc ();
    if (m_imagecache->get_pixels (m_name, subimage, miplevel,
                                  m_spec.x, m_spec.x+m_spec.width,
                                  m_spec.y, m_spec.y+m_spec.height,
                                  m_spec.z, m_spec.z+m_spec.depth,
                                  m_spec.format, &m_pixels[0])) {
        m_pixels_valid = true;
        m_localpixels = true;
    } else {
        m_pixels_valid = false;
        m_err = m_imagecache->geterror ();
    }

    return m_pixels_valid;
}



bool
ImageBuf::write (ImageOutput *out,
                 ProgressCallback progress_callback,
                 void *progress_callback_data) const
{
    stride_t as = AutoStride;
    bool ok = true;
    if (m_localpixels) {
        ok = out->write_image (m_spec.format, &m_pixels[0], as, as, as,
                               progress_callback, progress_callback_data);
    } else {
        std::vector<char> tmp (m_spec.image_bytes());
        copy_pixels (xbegin(), xend(), ybegin(), yend(), m_spec.format,
                     &tmp[0]);
        ok = out->write_image (m_spec.format, &tmp[0], as, as, as,
                               progress_callback, progress_callback_data);
        // FIXME -- not good for huge images.  Instead, we should read
        // little bits at a time (scanline or tile blocks).
    }
    if (! ok)
        m_err = out->geterror ();
    return ok;
}



bool
ImageBuf::save (const std::string &_filename, const std::string &_fileformat,
                ProgressCallback progress_callback,
                void *progress_callback_data) const
{
    std::string filename = _filename.size() ? _filename : name();
    std::string fileformat = _fileformat.size() ? _fileformat : filename;
    boost::scoped_ptr<ImageOutput> out (ImageOutput::create (fileformat.c_str(), "" /* searchpath */));
    if (! out) {
        m_err = geterror();
        return false;
    }
    if (! out->open (filename.c_str(), m_spec)) {
        m_err = out->geterror ();
        return false;
    }
    if (! write (out.get(), progress_callback, progress_callback_data))
        return false;
    out->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return true;
}



template<typename T>
static inline float getchannel_ (const ImageBuf &buf, int x, int y, int c)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y);
    return pixel[c];
}



float
ImageBuf::getchannel (int x, int y, int c) const
{
    if (c < 0 || c >= spec().nchannels)
        return 0.0f;
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : return getchannel_<float> (*this, x, y, c);
    case TypeDesc::UINT8 : return getchannel_<unsigned char> (*this, x, y, c);
    case TypeDesc::INT8  : return getchannel_<char> (*this, x, y, c);
    case TypeDesc::UINT16: return getchannel_<unsigned short> (*this, x, y, c);
    case TypeDesc::INT16 : return getchannel_<short> (*this, x, y, c);
    case TypeDesc::UINT  : return getchannel_<unsigned int> (*this, x, y, c);
    case TypeDesc::INT   : return getchannel_<int> (*this, x, y, c);
    case TypeDesc::HALF  : return getchannel_<half> (*this, x, y, c);
    case TypeDesc::DOUBLE: return getchannel_<double> (*this, x, y, c);
    case TypeDesc::UINT64: return getchannel_<unsigned long long> (*this, x, y, c);
    case TypeDesc::INT64 : return getchannel_<long long> (*this, x, y, c);
    default:
        ASSERT (0);
        return 0.0f;
    }
}



template<typename T>
static inline void
getpixel_ (const ImageBuf &buf, int x, int y, int z, float *result, int chans)
{
    ImageBuf::ConstIterator<T> pixel (buf, x, y, z);
    if (pixel.valid()) {
        for (int i = 0;  i < chans;  ++i)
            result[i] = pixel[i];
    } else {
        for (int i = 0;  i < chans;  ++i)
            result[i] = 0.0f;
    }
}



void
ImageBuf::getpixel (int x, int y, int z, float *pixel, int maxchannels) const
{
    int n = std::min (spec().nchannels, maxchannels);
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : getpixel_<float> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8 : getpixel_<unsigned char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT8  : getpixel_<char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16: getpixel_<unsigned short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT16 : getpixel_<short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT  : getpixel_<unsigned int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT   : getpixel_<int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF  : getpixel_<half> (*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: getpixel_<double> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64: getpixel_<unsigned long long> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT64 : getpixel_<long long> (*this, x, y, z, pixel, n); break;
    default:
        ASSERT (0);
    }
}



void
ImageBuf::interppixel (float x, float y, float *pixel) const
{
    const int maxchannels = 64;  // Reasonable guess
    float p[4][maxchannels];
    DASSERT (spec().nchannels <= maxchannels && 
             "You need to increase maxchannels in ImageBuf::interppixel");
    int n = std::min (spec().nchannels, maxchannels);
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    getpixel (xtexel, ytexel, p[0], n);
    getpixel (xtexel+1, ytexel, p[1], n);
    getpixel (xtexel, ytexel+1, p[2], n);
    getpixel (xtexel+1, ytexel+1, p[3], n);
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
}



template<typename T>
static inline void
setpixel_ (ImageBuf &buf, int x, int y, int z, const float *data, int chans)
{
    ImageBuf::Iterator<T> pixel (buf, x, y, z);
    if (pixel.valid()) {
        for (int i = 0;  i < chans;  ++i)
            pixel[i] = data[i];
    }
}



void
ImageBuf::setpixel (int x, int y, int z, const float *pixel, int maxchannels)
{
    int n = std::min (spec().nchannels, maxchannels);
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : setpixel_<float> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT8 : setpixel_<unsigned char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT8  : setpixel_<char> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT16: setpixel_<unsigned short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT16 : setpixel_<short> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT  : setpixel_<unsigned int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT   : setpixel_<int> (*this, x, y, z, pixel, n); break;
    case TypeDesc::HALF  : setpixel_<half> (*this, x, y, z, pixel, n); break;
    case TypeDesc::DOUBLE: setpixel_<double> (*this, x, y, z, pixel, n); break;
    case TypeDesc::UINT64: setpixel_<unsigned long long> (*this, x, y, z, pixel, n); break;
    case TypeDesc::INT64 : setpixel_<long long> (*this, x, y, z, pixel, n); break;
    default:
        ASSERT (0);
    }
}



void
ImageBuf::setpixel (int i, const float *pixel, int maxchannels)
{
    setpixel (spec().x + (i % spec().width), spec().y + (i / spec().width),
              pixel, maxchannels);
}



template<typename S, typename D>
static inline void 
copy_pixels_ (const ImageBuf &buf, int xbegin, int xend,
              int ybegin, int yend, D *r)
{
    int w = (xend-xbegin);
    for (ImageBuf::ConstIterator<S,D> p (buf, xbegin, xend, ybegin, yend);
         p.valid(); ++p) { 
        imagesize_t offset = ((p.y()-ybegin)*w + (p.x()-xbegin)) * buf.nchannels();
        for (int c = 0;  c < buf.nchannels();  ++c)
            r[offset+c] = p[c];
    }
}



template<typename D>
bool
ImageBuf::copy_pixels (int xbegin, int xend, int ybegin, int yend, D *r) const
{
    // Caveat: serious hack here.  To avoid duplicating code, use a
    // #define.  Furthermore, exploit the CType<> template to construct
    // the right C data type for the given BASETYPE.
#define TYPECASE(B)                                                     \
    case B : copy_pixels_<CType<B>::type,D>(*this, xbegin, xend, ybegin, yend, (D *)r); return true
    
    switch (spec().format.basetype) {
        TYPECASE (TypeDesc::UINT8);
        TYPECASE (TypeDesc::INT8);
        TYPECASE (TypeDesc::UINT16);
        TYPECASE (TypeDesc::INT16);
        TYPECASE (TypeDesc::UINT);
        TYPECASE (TypeDesc::INT);
        TYPECASE (TypeDesc::HALF);
        TYPECASE (TypeDesc::FLOAT);
        TYPECASE (TypeDesc::DOUBLE);
        TYPECASE (TypeDesc::UINT64);
        TYPECASE (TypeDesc::INT64);
    }
    return false;
#undef TYPECASE
}



bool
ImageBuf::copy_pixels (int xbegin, int xend, int ybegin, int yend,
                       TypeDesc format, void *result) const
{
#if 1
    // Fancy method -- for each possible base type that the user
    // wants for a destination type, call a template specialization.
    switch (format.basetype) {
    case TypeDesc::UINT8 :
        copy_pixels<unsigned char> (xbegin, xend, ybegin, yend, (unsigned char *)result);
        break;
    case TypeDesc::INT8:
        copy_pixels<char> (xbegin, xend, ybegin, yend, (char *)result);
        break;
    case TypeDesc::UINT16 :
        copy_pixels<unsigned short> (xbegin, xend, ybegin, yend, (unsigned short *)result);
        break;
    case TypeDesc::INT16 :
        copy_pixels<short> (xbegin, xend, ybegin, yend, (short *)result);
        break;
    case TypeDesc::UINT :
        copy_pixels<unsigned int> (xbegin, xend, ybegin, yend, (unsigned int *)result);
        break;
    case TypeDesc::INT :
        copy_pixels<int> (xbegin, xend, ybegin, yend, (int *)result);
        break;
    case TypeDesc::HALF :
        copy_pixels<half> (xbegin, xend, ybegin, yend, (half *)result);
        break;
    case TypeDesc::FLOAT :
        copy_pixels<float> (xbegin, xend, ybegin, yend, (float *)result);
        break;
    case TypeDesc::DOUBLE :
        copy_pixels<double> (xbegin, xend, ybegin, yend, (double *)result);
        break;
    case TypeDesc::UINT64 :
        copy_pixels<unsigned long long> (xbegin, xend, ybegin, yend, (unsigned long long *)result);
        break;
    case TypeDesc::INT64 :
        copy_pixels<long long> (xbegin, xend, ybegin, yend, (long long *)result);
        break;
    default:
        return false;
    }
#else
    // Naive method -- loop over pixels, calling getpixel()
    size_t usersize = format.size() * nchannels();
    float *pel = (float *) alloca (nchannels() * sizeof(float));
    for (int y = ybegin;  y < yend;  ++y)
        for (int x = xbegin;  x < xend;  ++x) {
            getpixel (x, y, pel);
            convert_types (TypeDesc::TypeFloat, pel,
                           format, result, nchannels());
            result = (void *) ((char *)result + usersize);
        }
#endif
    return true;
}



template<typename T>
static inline void
transfer_pixels_ (ImageBuf &buf, ColorTransfer *tfunc)
{
    for (ImageBuf::Iterator<T> pixel (buf); pixel.valid(); ++pixel) {
        convert_types (buf.spec().format, pixel.rawptr(),
                       buf.spec().format, pixel.rawptr(),
                       buf.nchannels(), tfunc,
                       buf.spec().alpha_channel, buf.spec().z_channel);
    }
}



void
ImageBuf::transfer_pixels (ColorTransfer *tfunc)
{
    if (! tfunc)
        return;
    switch (spec().format.basetype) {
    case TypeDesc::FLOAT : transfer_pixels_<float> (*this, tfunc); break;
    case TypeDesc::UINT8 : transfer_pixels_<unsigned char> (*this, tfunc); break;
    case TypeDesc::INT8  : transfer_pixels_<char> (*this, tfunc); break;
    case TypeDesc::UINT16: transfer_pixels_<unsigned short> (*this, tfunc); break;
    case TypeDesc::INT16 : transfer_pixels_<short> (*this, tfunc); break;
    case TypeDesc::UINT  : transfer_pixels_<unsigned int> (*this, tfunc); break;
    case TypeDesc::INT   : transfer_pixels_<int> (*this, tfunc); break;
    case TypeDesc::UINT64: transfer_pixels_<unsigned long long> (*this, tfunc); break;
    case TypeDesc::INT64 : transfer_pixels_<long long> (*this, tfunc); break;
    case TypeDesc::HALF  : transfer_pixels_<half> (*this, tfunc); break;
    case TypeDesc::DOUBLE: transfer_pixels_<double> (*this, tfunc); break;
    default:
        ASSERT (0);
    }
}



int
ImageBuf::oriented_width () const
{
    return m_orientation <= 4 ? m_spec.width : m_spec.height;
}



int
ImageBuf::oriented_height () const
{
    return m_orientation <= 4 ? m_spec.height : m_spec.width;
}



int
ImageBuf::oriented_x () const
{
    return m_orientation <= 4 ? m_spec.x : m_spec.y;
}



int
ImageBuf::oriented_y () const
{
    return m_orientation <= 4 ? m_spec.y : m_spec.x;
}



int
ImageBuf::oriented_full_width () const
{
    return m_orientation <= 4 ? m_spec.full_width : m_spec.full_height;
}



int
ImageBuf::oriented_full_height () const
{
    return m_orientation <= 4 ? m_spec.full_height : m_spec.full_width;
}



int
ImageBuf::oriented_full_x () const
{
    return m_orientation <= 4 ? m_spec.full_x : m_spec.full_y;
}



int
ImageBuf::oriented_full_y () const
{
    return m_orientation <= 4 ? m_spec.full_y : m_spec.full_x;
}



void
ImageBuf::set_full (int xbegin, int xend, int ybegin, int yend,
                    int zbegin, int zend, const float *bordercolor)
{
    m_spec.full_x = xbegin;
    m_spec.full_y = ybegin;
    m_spec.full_z = zbegin;
    m_spec.full_width  = xend - xbegin;
    m_spec.full_height = yend - ybegin;
    m_spec.full_depth  = zend - zbegin;
    if (bordercolor)
        m_spec.attribute ("oiio:bordercolor",
                          TypeDesc(TypeDesc::FLOAT,m_spec.nchannels),
                          bordercolor);
}



const void *
ImageBuf::pixeladdr (int x, int y, int z) const
{
    if (! m_localpixels)
        return NULL;
    x -= spec().x;
    y -= spec().y;
    z -= spec().z;
    size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
    if (z)
        p += z * clamped_mult64 (m_spec.scanline_bytes(), (imagesize_t)spec().height);
    return &(m_pixels[p]);
}



void *
ImageBuf::pixeladdr (int x, int y, int z)
{
    if (! m_localpixels)
        return NULL;
    x -= spec().x;
    y -= spec().y;
    z -= spec().z;
    size_t p = y * m_spec.scanline_bytes() + x * m_spec.pixel_bytes();
    if (z)
        p += z * m_spec.scanline_bytes() * spec().height;
    return &(m_pixels[p]);
}



const void *
ImageBuf::retile (int x, int y, int z, ImageCache::Tile* &tile,
                  int &tilexbegin, int &tileybegin, int &tilezbegin) const
{
    int tw = spec().tile_width, th = spec().tile_height;
    int td = std::max (1, spec().tile_depth);
    if (tile == NULL || x < tilexbegin || x >= (tilexbegin+tw) ||
                        y < tileybegin || y >= (tileybegin+th) ||
                        z < tilezbegin || z >= (tilezbegin+td)) {
        // not the same tile as before
        if (tile)
            m_imagecache->release_tile (tile);
        int xtile = (x-spec().x) / tw;
        int ytile = (y-spec().y) / th;
        int ztile = (z-spec().z) / td;
        tilexbegin = spec().x + xtile*tw;
        tileybegin = spec().y + ytile*th;
        tilezbegin = spec().z + ztile*td;
        tile = m_imagecache->get_tile (m_name, subimage(), miplevel(), x, y, z);
    }

    size_t offset = ((y - tileybegin) * tw) + (x - tilexbegin);
    offset += ((z - tilezbegin) * tw * th);
    offset *= spec().pixel_bytes();
    TypeDesc format;
    return (const char *)m_imagecache->tile_pixels (tile, format) + offset;
}

}
OIIO_NAMESPACE_EXIT
