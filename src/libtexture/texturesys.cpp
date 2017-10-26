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


#include <cmath>
#include <string>
#include <sstream>
#include <cstring>
#include <list>

#include <OpenEXR/half.h>
#include <OpenEXR/ImathMatrix.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/varyingref.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/optparser.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <boost/random.hpp>
#include "imagecache_pvt.h"
#include "texture_pvt.h"

#define TEX_FAST_MATH 1


OIIO_NAMESPACE_BEGIN
    using namespace pvt;
    using namespace simd;

namespace {  // anonymous

// We would like shared_texturesys to be a shared_ptr so that it is
// automatically deleted when the app exists, but because it contains a
// reference to an ImageCache, we get into the "destruction order fiasco."
// The only easy way to fix this is to make shared_texturesys be an ordinary
// pointer and just let it leak (who cares? the app is done, and it only
// contains a few hundred bytes).
static TextureSystemImpl *shared_texturesys = NULL;
static spin_mutex shared_texturesys_mutex;

static EightBitConverter<float> uchar2float;
static vfloat4 u8scale (1.0f/255.0f);
static vfloat4 u16scale (1.0f/65535.0f);

OIIO_FORCEINLINE vfloat4 uchar2float4 (const unsigned char *c) {
    return vfloat4(c) * u8scale;
}


OIIO_FORCEINLINE vfloat4 ushort2float4 (const unsigned short *s) {
    return vfloat4(s) * u16scale;
}


OIIO_FORCEINLINE vfloat4 half2float4 (const half *h) {
    return vfloat4(h);
}


static const OIIO_SIMD4_ALIGN vbool4 channel_masks[5] = {
    vbool4(false, false, false, false),
    vbool4(true,  false, false, false),
    vbool4(true,  true,  false, false),
    vbool4(true,  true,  true,  false),
    vbool4(true,  true,  true,  true),
};

}  // end anonymous namespace


TextureSystem *
TextureSystem::create (bool shared)
{
    if (shared) {
        // They requested a shared texture system.  If a shared one already
        // exists, just return it, otherwise record the new texture system.
        spin_lock guard (shared_texturesys_mutex);
        if (! shared_texturesys)
            shared_texturesys = new TextureSystemImpl(ImageCache::create(true));
#if 0
        std::cerr << " shared TextureSystem is "
                  << (void *)shared_texturesys << "\n";
#endif
        return shared_texturesys;
    }

    // Doesn't need a shared cache
    TextureSystemImpl *ts = new TextureSystemImpl (ImageCache::create(false));
#if 0
    std::cerr << "creating new ImageCache " << (void *)ts << "\n";
#endif
    return ts;
}



void
TextureSystem::destroy (TextureSystem *x, bool teardown_imagecache)
{
    // std::cerr << "Destroying TS " << (void *)x << "\n";
    if (! x)
        return;
    TextureSystemImpl *impl = (TextureSystemImpl *) x;
    if (teardown_imagecache) {
        ImageCache::destroy (impl->m_imagecache, true);
        impl->m_imagecache = NULL;
    }

    spin_lock guard (shared_texturesys_mutex);
    if (impl == shared_texturesys) {
        // This is the shared TS, so don't really delete it.
    } else {
        // Not a shared cache, we are the only owner, so truly destroy it.
        delete x;
    }
}



void
TextureSystem::destroy (TextureSystem *x)
{
    destroy (x, false);
}



namespace pvt {   // namespace pvt



// Wrap functions wrap 'coord' around 'width', and return true if the
// result is a valid pixel coordinate, false if black should be used
// instead.


bool
TextureSystemImpl::wrap_periodic_sharedborder (int &coord, int origin, int width)
{
    // Like periodic, but knowing that the first column and last are
    // actually the same position, so we essentially skip the last
    // column in the next cycle.
    if (width <= 2) {
        coord = origin;  // special case -- just 1 pixel wide
    } else {
        coord -= origin;
        coord %= (width-1);
        if (coord < 0)       // Fix negative values
            coord += width;
        coord += origin;
    }
    return true;
}


const TextureSystemImpl::wrap_impl TextureSystemImpl::wrap_functions[] = {
    // Must be in same order as Wrap enum
    wrap_black, wrap_black, wrap_clamp, wrap_periodic, wrap_mirror,
    wrap_periodic_pow2, wrap_periodic_sharedborder
};



simd::vbool4
wrap_black_simd (simd::vint4 &coord_, const simd::vint4& origin,
                 const simd::vint4& width)
{
    simd::vint4 coord (coord_);
    return (coord >= origin) & (coord < (width+origin));
}


simd::vbool4
wrap_clamp_simd (simd::vint4 &coord_, const simd::vint4& origin,
                 const simd::vint4& width)
{
    simd::vint4 coord (coord_);
    coord = simd::blend (coord, origin, coord < origin);
    coord = simd::blend (coord, (origin+width-1), coord >= (origin+width));
    coord_ = coord;
    return simd::vbool4::True();
}


simd::vbool4
wrap_periodic_simd (simd::vint4 &coord_, const simd::vint4& origin,
                    const simd::vint4& width)
{
    simd::vint4 coord (coord_);
    coord = coord - origin;
    coord = coord % width;
    coord = simd::blend (coord, coord+width, coord < 0);
    coord = coord + origin;
    coord_ = coord;
    return simd::vbool4::True();
}


simd::vbool4
wrap_periodic_pow2_simd (simd::vint4 &coord_, const simd::vint4& origin,
                         const simd::vint4& width)
{
    simd::vint4 coord (coord_);
//    DASSERT (ispow2(width));
    coord = coord - origin;
    coord = coord & (width - 1); // Shortcut periodic if we're sure it's a pow of 2
    coord = coord + origin;
    coord_ = coord;
    return simd::vbool4::True();
}


simd::vbool4
wrap_mirror_simd (simd::vint4 &coord_, const simd::vint4& origin,
                  const simd::vint4& width)
{
    simd::vint4 coord (coord_);
    coord = coord - origin;
    coord = simd::blend (coord, -1 - coord, coord < 0);
    simd::vint4 iter = coord / width;    // Which iteration of the pattern?
    coord -= iter * width;
    // Odd iterations -- flip the sense
    coord = blend (coord, (width - 1) - coord, (iter & 1) != 0);
    // DASSERT_MSG (coord >= 0 && coord < width,
    //              "width=%d, origin=%d, result=%d", width, origin, coord);
    coord += origin;
    coord_ = coord;
    return simd::vbool4::True();
}


simd::vbool4
wrap_periodic_sharedborder_simd (simd::vint4 &coord_, const simd::vint4& origin,
                                 const simd::vint4& width)
{
    // Like periodic, but knowing that the first column and last are
    // actually the same position, so we essentially skip the last
    // column in the next cycle.
    simd::vint4 coord (coord_);
    coord = coord - origin;
    coord = coord % (width-1);
    coord += blend (simd::vint4(origin), width+origin, coord < 0); // Fix negative values
    coord = blend (coord, origin, width <= 2);  // special case -- just 1 pixel wide
    coord_ = coord;
    return true;
}



typedef simd::vbool4 (*wrap_impl_simd) (simd::vint4 &coord, const simd::vint4& origin,
                                       const simd::vint4& width);


const wrap_impl_simd wrap_functions_simd[] = {
    // Must be in same order as Wrap enum
    wrap_black_simd, wrap_black_simd, wrap_clamp_simd,
    wrap_periodic_simd, wrap_mirror_simd, wrap_periodic_pow2_simd,
    wrap_periodic_sharedborder_simd
};




const char *
texture_format_name (TexFormat f)
{
    static const char * texture_format_names[] = {
        // MUST match the order of TexFormat
        "unknown", "Plain Texture", "Volume Texture",
        "Shadow", "CubeFace Shadow", "Volume Shadow",
        "LatLong Environment", "CubeFace Environment",
        ""
    };
    return texture_format_names[(int)f];
}



const char *
texture_type_name (TexFormat f)
{
    static const char * texture_type_names[] = {
        // MUST match the order of TexFormat
        "unknown", "Plain Texture", "Volume Texture",
        "Shadow", "Shadow", "Shadow",
        "Environment", "Environment",
        ""
    };
    return texture_type_names[(int)f];
}



TextureSystemImpl::TextureSystemImpl (ImageCache *imagecache)
    : hq_filter(NULL)
{
    m_imagecache = (ImageCacheImpl *) imagecache;
    init ();
}



void
TextureSystemImpl::init ()
{
    m_Mw2c.makeIdentity();
    m_gray_to_rgb = false;
    m_flip_t = false;
    m_max_tile_channels = 5;
    delete hq_filter;
    hq_filter = Filter1D::create ("b-spline", 4);
    m_statslevel = 0;

    // Allow environment variable to override default options
    const char *options = getenv ("OPENIMAGEIO_TEXTURE_OPTIONS");
    if (options)
        attribute ("options", options);

#if 0
    unit_test_texture ();
#endif
}



TextureSystemImpl::~TextureSystemImpl ()
{
    printstats ();
    ImageCache::destroy (m_imagecache);
    m_imagecache = NULL;
    delete hq_filter;
}



std::string
TextureSystemImpl::getstats (int level, bool icstats) const
{
    // Merge all the threads
    ImageCacheStatistics stats;
    m_imagecache->mergestats (stats);

    std::ostringstream out;
    out.imbue (std::locale::classic());  // Force "C" locale with '.' decimal
    bool anytexture = (stats.texture_queries + stats.texture3d_queries +
                       stats.shadow_queries + stats.environment_queries);
    if (level > 0 && anytexture) {
        out << "OpenImageIO Texture statistics\n";

        std::string opt;
#define BOOLOPT(name) if (m_##name) opt += #name " "
#define INTOPT(name) opt += Strutil::format(#name "=%d ", m_##name)
#define STROPT(name) if (m_##name.size()) opt += Strutil::format(#name "=\"%s\" ", m_##name)
        INTOPT(gray_to_rgb);
        INTOPT(flip_t);
        INTOPT(max_tile_channels);
#undef BOOLOPT
#undef INTOPT
#undef STROPT
        out << "  Options:  " << Strutil::wordwrap(opt,75,12) << "\n";

        out << "  Queries/batches : \n";
        out << "    texture     :  " << stats.texture_queries
            << " queries in " << stats.texture_batches << " batches\n";
        out << "    texture 3d  :  " << stats.texture3d_queries
            << " queries in " << stats.texture3d_batches << " batches\n";
        out << "    shadow      :  " << stats.shadow_queries
            << " queries in " << stats.shadow_batches << " batches\n";
        out << "    environment :  " << stats.environment_queries
            << " queries in " << stats.environment_batches << " batches\n";
        out << "  Interpolations :\n";
        out << "    closest  : " << stats.closest_interps << "\n";
        out << "    bilinear : " << stats.bilinear_interps << "\n";
        out << "    bicubic  : " << stats.cubic_interps << "\n";
        if (stats.aniso_queries)
            out << Strutil::format ("  Average anisotropic probes : %.3g\n",
                                    (double)stats.aniso_probes/(double)stats.aniso_queries);
        else
            out << Strutil::format ("  Average anisotropic probes : 0\n");
        out << Strutil::format ("  Max anisotropy in the wild : %.3g\n",
                                stats.max_aniso);
        if (icstats)
            out << "\n";
    }
    if (icstats)
        out << m_imagecache->getstats (level);
    return out.str();
}



void
TextureSystemImpl::printstats () const
{
    if (m_statslevel == 0)
        return;
    std::cout << getstats (m_statslevel, false) << "\n\n";
}



void
TextureSystemImpl::reset_stats ()
{
    ASSERT (m_imagecache);
    m_imagecache->reset_stats ();
}



bool
TextureSystemImpl::attribute (string_view name, TypeDesc type,
                              const void *val)
{
    if (name == "options" && type == TypeDesc::STRING) {
        return optparser (*this, *(const char **)val);
    }
    if (name == "worldtocommon" && (type == TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        m_Mw2c = *(const Imath::M44f *)val;
        m_Mc2w = m_Mw2c.inverse();
        return true;
    }
    if (name == "commontoworld" && (type == TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        m_Mc2w = *(const Imath::M44f *)val;
        m_Mw2c = m_Mc2w.inverse();
        return true;
    }
    if ((name == "gray_to_rgb" || name == "grey_to_rgb") &&
        (type == TypeInt)) {
        m_gray_to_rgb = *(const int *)val;
        return true;
    }
    if (name == "flip_t" && type == TypeInt) {
        m_flip_t = *(const int *)val;
        return true;
    }
    if (name == "m_max_tile_channels" && type == TypeInt) {
        m_max_tile_channels = *(const int *)val;
        return true;
    }
    if (name == "statistics:level" && type == TypeInt) {
        m_statslevel = *(const int *)val;
        // DO NOT RETURN! pass the same message to the image cache
    }

    // Maybe it's meant for the cache?
    return m_imagecache->attribute (name, type, val);
}



bool
TextureSystemImpl::getattribute (string_view name, TypeDesc type,
                                 void *val) const
{
    if (name == "worldtocommon" && (type == TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mw2c;
        return true;
    }
    if (name == "commontoworld" && (type == TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mc2w;
        return true;
    }
    if ((name == "gray_to_rgb" || name == "grey_to_rgb") &&
        (type == TypeInt)) {
        *(int *)val = m_gray_to_rgb;
        return true;
    }
    if (name == "flip_t" && type == TypeInt) {
        *(int *)val = m_flip_t;
        return true;
    }
    if (name == "m_max_tile_channels" && type == TypeInt) {
        *(int *)val = m_max_tile_channels;
        return true;
    }

    // If not one of these, maybe it's an attribute meant for the image cache?
    return m_imagecache->getattribute (name, type, val);

    return false;
}



std::string
TextureSystemImpl::resolve_filename (const std::string &filename) const
{
    return m_imagecache->resolve_filename (filename);
}



bool
TextureSystemImpl::get_texture_info (ustring filename, int subimage,
                             ustring dataname, TypeDesc datatype, void *data)
{
    bool ok = m_imagecache->get_image_info (filename, subimage, 0,
                                            dataname, datatype, data);
    if (! ok) {
        std::string err = m_imagecache->geterror();
        if (!err.empty())
            error ("%s", err);
    }
    return ok;
}



bool
TextureSystemImpl::get_texture_info (TextureHandle *texture_handle,
                             Perthread *thread_info, int subimage,
                             ustring dataname, TypeDesc datatype, void *data)
{
    bool ok = m_imagecache->get_image_info ((ImageCache::ImageHandle *)texture_handle,
                                            (ImageCache::Perthread *)thread_info,
                                            subimage, 0,
                                            dataname, datatype, data);
    if (! ok) {
        std::string err = m_imagecache->geterror();
        if (!err.empty())
            error ("%s", err);
    }
    return ok;
}



bool
TextureSystemImpl::get_imagespec (ustring filename, int subimage,
                                  ImageSpec &spec)
{
    bool ok = m_imagecache->get_imagespec (filename, spec, subimage);
    if (! ok) {
        std::string err = m_imagecache->geterror();
        if (!err.empty())
            error ("%s", err);
    }
    return ok;
}



bool
TextureSystemImpl::get_imagespec (TextureHandle *texture_handle,
                                  Perthread *thread_info, int subimage,
                                  ImageSpec &spec)
{
    bool ok = m_imagecache->get_imagespec ((ImageCache::ImageHandle *)texture_handle,
                                           (ImageCache::Perthread *)thread_info,
                                           spec, subimage);
    if (! ok) {
        std::string err = m_imagecache->geterror();
        if (!err.empty())
            error ("%s", err);
    }
    return ok;
}



const ImageSpec *
TextureSystemImpl::imagespec (ustring filename, int subimage)
{
    const ImageSpec *spec = m_imagecache->imagespec (filename, subimage);
    if (! spec)
        error ("%s", m_imagecache->geterror());
    return spec;
}



const ImageSpec *
TextureSystemImpl::imagespec (TextureHandle *texture_handle,
                              Perthread *thread_info, int subimage)
{
    const ImageSpec *spec =
        m_imagecache->imagespec ((ImageCache::ImageHandle *)texture_handle,
                                 (ImageCache::Perthread *)thread_info, subimage);
    if (! spec) {
        std::string err = m_imagecache->geterror();
        if (!err.empty())
            error ("%s", err);
    }
    return spec;
}



bool
TextureSystemImpl::get_texels (ustring filename, TextureOpt &options,
                               int miplevel, int xbegin, int xend,
                               int ybegin, int yend, int zbegin, int zend,
                               int chbegin, int chend,
                               TypeDesc format, void *result)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texfile = find_texturefile (filename, thread_info);
    if (! texfile) {
        error ("Texture file \"%s\" not found", filename);
        return false;
    }
    return get_texels ((TextureHandle *)texfile, (Perthread *)thread_info,
                       options, miplevel, xbegin, xend,
                       ybegin, yend, zbegin, zend, chbegin, chend,
                       format, result);
}



bool
TextureSystemImpl::get_texels (TextureHandle *texture_handle_,
                               Perthread *thread_info_, TextureOpt &options,
                               int miplevel, int xbegin, int xend,
                               int ybegin, int yend, int zbegin, int zend,
                               int chbegin, int chend,
                               TypeDesc format, void *result)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info((PerThreadInfo *)thread_info_);
    TextureFile *texfile = verify_texturefile ((TextureFile *)texture_handle_, thread_info);
    if (! texfile) {
        error ("Invalid texture handle NULL");
        return false;
    }

    if (texfile->broken()) {
        if (texfile->errors_should_issue())
            error ("Invalid texture file \"%s\"", texfile->filename());
        return false;
    }
    int subimage = options.subimage;
    if (subimage < 0 || subimage >= texfile->subimages()) {
        error ("get_texel asked for nonexistant subimage %d of \"%s\"",
               subimage, texfile->filename());
        return false;
    }
    if (miplevel < 0 || miplevel >= texfile->miplevels(subimage)) {
        if (texfile->errors_should_issue())
            error ("get_texel asked for nonexistant MIP level %d of \"%s\"",
                   miplevel, texfile->filename());
        return false;
    }
    const ImageSpec &spec (texfile->spec(subimage, miplevel));

    // FIXME -- this could be WAY more efficient than starting from
    // scratch for each pixel within the rectangle.  Instead, we should
    // grab a whole tile at a time and memcpy it rapidly.  But no point
    // doing anything more complicated (not to mention bug-prone) until
    // somebody reports this routine as being a bottleneck.
    int nchannels = chend - chbegin;
    int actualchannels = Imath::clamp (spec.nchannels - chbegin, 0, nchannels);
    int tile_chbegin = 0, tile_chend = spec.nchannels;
    if (spec.nchannels > m_max_tile_channels) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = chbegin;
        tile_chend = chbegin+actualchannels;
    }
    TileID tileid (*texfile, subimage, miplevel, 0, 0, 0,
                   tile_chbegin, tile_chend);
    size_t formatchannelsize = format.size();
    size_t formatpixelsize = nchannels * formatchannelsize;
    size_t scanlinesize = (xend-xbegin) * formatpixelsize;
    size_t zplanesize = (yend-ybegin) * scanlinesize;
    bool ok = true;
    for (int z = zbegin;  z < zend;  ++z) {
        if (z < spec.z || z >= (spec.z+std::max(spec.depth,1))) {
            // nonexistant planes
            memset (result, 0, zplanesize);
            result = (void *) ((char *) result + zplanesize);
            continue;
        }
        tileid.z (z - ((z - spec.z) % std::max (1, spec.tile_depth)));
        for (int y = ybegin;  y < yend;  ++y) {
            if (y < spec.y || y >= (spec.y+spec.height)) {
                // nonexistant scanlines
                memset (result, 0, scanlinesize);
                result = (void *) ((char *) result + scanlinesize);
                continue;
            }
            tileid.y (y - ((y - spec.y) % spec.tile_height));
            for (int x = xbegin;  x < xend;  ++x) {
                if (x < spec.x || x >= (spec.x+spec.width)) {
                    // nonexistant columns
                    memset (result, 0, formatpixelsize);
                    result = (void *) ((char *) result + formatpixelsize);
                    continue;
                }
                tileid.x (x - ((x - spec.x) % spec.tile_width));
                ok &= find_tile (tileid, thread_info);
                TileRef &tile (thread_info->tile);
                const char *data;
                if (tile && (data = (const char *)tile->data (x, y, z, chbegin))) {
                    convert_types (texfile->datatype(subimage), data,
                                   format, result, actualchannels);
                    for (int c = actualchannels;  c < nchannels;  ++c)
                        convert_types (TypeDesc::FLOAT, &options.fill, format,
                                       (char *)result+c*formatchannelsize, 1);
                } else {
                    memset (result, 0, formatpixelsize);
                }
                result = (void *) ((char *) result + formatpixelsize);
            }
        }
    }
    if (! ok) {
        std::string err = m_imagecache->geterror();
        if (! err.empty())
            error ("%s", err);
    }
    return ok;
}



std::string
TextureSystemImpl::geterror () const
{
    std::string e;
    std::string *errptr = m_errormessage.get ();
    if (errptr) {
        e = *errptr;
        errptr->clear ();
    }
    return e;
}



void
TextureSystemImpl::append_error (const std::string& message) const
{
    std::string *errptr = m_errormessage.get ();
    if (! errptr) {
        errptr = new std::string;
        m_errormessage.reset (errptr);
    }
    ASSERT (errptr != NULL);
    ASSERT (errptr->size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (errptr->size())
        *errptr += '\n';
    *errptr += message;
}



// Implementation of invalidate -- just invalidate the image cache.
void
TextureSystemImpl::invalidate (ustring filename)
{
    m_imagecache->invalidate (filename);
}



// Implementation of invalidate -- just invalidate the image cache.
void
TextureSystemImpl::invalidate_all (bool force)
{
    m_imagecache->invalidate_all (force);
}



bool
TextureSystemImpl::missing_texture (TextureOpt &options, int nchannels,
                                    float *result, float *dresultds,
                                    float *dresultdt, float *dresultdr)
{
    for (int c = 0;  c < nchannels;  ++c) {
        if (options.missingcolor)
            result[c] = options.missingcolor[c];
        else
            result[c] = options.fill;
        if (dresultds) dresultds[c] = 0;
        if (dresultdt) dresultdt[c] = 0;
        if (dresultdr) dresultdr[c] = 0;
    }
    if (options.missingcolor) {
        // don't treat it as an error if missingcolor was supplied
        (void) geterror ();   // eat the error
        return true;
    } else {
        return false;
    }
}



void
TextureSystemImpl::fill_gray_channels (const ImageSpec &spec,
                                       int nchannels, float *result,
                                       float *dresultds, float *dresultdt,
                                       float *dresultdr)
{
    int specchans = spec.nchannels;
    if (specchans == 1 && nchannels >= 3) {
        // Asked for RGB or RGBA, texture was just R...
        // copy the one channel to G and B
        *(simd::vfloat4 *)result = simd::shuffle<0,0,0,3>(*(simd::vfloat4 *)result);
        if (dresultds) {
            *(simd::vfloat4 *)dresultds = simd::shuffle<0,0,0,3>(*(simd::vfloat4 *)dresultds);
            *(simd::vfloat4 *)dresultdt = simd::shuffle<0,0,0,3>(*(simd::vfloat4 *)dresultdt);
            if (dresultdr)
                *(simd::vfloat4 *)dresultdr = simd::shuffle<0,0,0,3>(*(simd::vfloat4 *)dresultdr);
        }
    } else if (specchans == 2 && nchannels == 4 && spec.alpha_channel == 1) {
        // Asked for RGBA, texture was RA
        // Shuffle into RRRA
        *(simd::vfloat4 *)result = simd::shuffle<0,0,0,1>(*(simd::vfloat4 *)result);
        if (dresultds) {
            *(simd::vfloat4 *)dresultds = simd::shuffle<0,0,0,1>(*(simd::vfloat4 *)dresultds);
            *(simd::vfloat4 *)dresultdt = simd::shuffle<0,0,0,1>(*(simd::vfloat4 *)dresultdt);
            if (dresultdr)
                *(simd::vfloat4 *)dresultdr = simd::shuffle<0,0,0,1>(*(simd::vfloat4 *)dresultdr);
        }

    }
}



bool
TextureSystemImpl::texture (ustring filename, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<float> s, VaryingRef<float> t,
                            VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                            VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    Perthread *thread_info = get_perthread_info();
    TextureHandle *texture_handle = get_texture_handle (filename, thread_info);
    return texture (texture_handle, thread_info, options,
                    runflags, beginactive, endactive,
                    s, t, dsdx, dtdx, dsdy, dtdy,
                    nchannels, result, dresultds, dresultdt);
}



bool
TextureSystemImpl::texture (TextureHandle *texture_handle,
                            Perthread *thread_info, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<float> s, VaryingRef<float> t,
                            VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                            VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    if (! texture_handle)
        return false;
    bool ok = true;
    result += beginactive*nchannels;
    if (dresultds) {
        dresultds += beginactive*nchannels;
        dresultdt += beginactive*nchannels;
    }
    for (int i = beginactive;  i < endactive;  ++i) {
        if (runflags[i]) {
            TextureOpt opt (options, i);
            ok &= texture (texture_handle, thread_info,
                           opt, s[i], t[i], dsdx[i], dtdx[i],
                           dsdy[i], dtdy[i], nchannels,
                           result, dresultds, dresultdt);
        }
        result += nchannels;
        if (dresultds) {
            dresultds += nchannels;
            dresultdt += nchannels;
        }
    }
    return ok;
}



bool
TextureSystemImpl::texture (ustring filename, TextureOpt &options,
                            float s, float t, float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texturefile = find_texturefile (filename, thread_info);
    return texture ((TextureHandle *)texturefile, (Perthread *)thread_info,
                    options, s, t, dsdx, dtdx, dsdy, dtdy,
                    nchannels, result, dresultds, dresultdt);
}



bool
TextureSystemImpl::texture (TextureHandle *texture_handle_,
                            Perthread *thread_info_, TextureOpt &options,
                            float s, float t, float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    // Handle >4 channel lookups by recursion.
    if (nchannels > 4) {
        int save_firstchannel = options.firstchannel;
        while (nchannels) {
            int n = std::min (nchannels, 4);
            bool ok = texture (texture_handle_, thread_info_, options,
                               s, t, dsdx, dtdx, dsdy, dtdy, n /* chans */,
                               result, dresultds, dresultdt);
            if (! ok)
                return false;
            result += n;
            if (dresultds) dresultds += n;
            if (dresultdt) dresultdt += n;
            options.firstchannel += n;
            nchannels -= n;
        }
        options.firstchannel = save_firstchannel; // restore what we changed
        return true;
    }

    static const texture_lookup_prototype lookup_functions[] = {
        // Must be in the same order as Mipmode enum
        &TextureSystemImpl::texture_lookup,
        &TextureSystemImpl::texture_lookup_nomip,
        &TextureSystemImpl::texture_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture_lookup
    };
    texture_lookup_prototype lookup = lookup_functions[(int)options.mipmode];

    PerThreadInfo *thread_info = m_imagecache->get_perthread_info((PerThreadInfo *)thread_info_);
    TextureFile *texturefile = (TextureFile *)texture_handle_;
    if (texturefile->is_udim())
        texturefile = m_imagecache->resolve_udim (texturefile, s, t);

    texturefile = verify_texturefile (texturefile, thread_info);

    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.texture_batches;
    ++stats.texture_queries;

    if (! texturefile  ||  texturefile->broken())
        return missing_texture (options, nchannels, result, dresultds, dresultdt);

    if (options.subimagename) {
        // If subimage was specified by name, figure out its index.
        int s = m_imagecache->subimage_from_name (texturefile, options.subimagename);
        if (s < 0) {
            error ("Unknown subimage \"%s\" in texture \"%s\"",
                   options.subimagename, texturefile->filename());
            return false;
        }
        options.subimage = s;
        options.subimagename.clear();
    }

    const ImageCacheFile::SubimageInfo &subinfo (texturefile->subimageinfo(options.subimage));
    const ImageSpec &spec (texturefile->spec(options.subimage, 0));

    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel, 0, nchannels);

    // Figure out the wrap functions
    if (options.swrap == TextureOpt::WrapDefault)
        options.swrap = (TextureOpt::Wrap)texturefile->swrap();
    if (options.swrap == TextureOpt::WrapPeriodic && ispow2(spec.width))
        options.swrap = TextureOpt::WrapPeriodicPow2;
    if (options.twrap == TextureOpt::WrapDefault)
        options.twrap = (TextureOpt::Wrap)texturefile->twrap();
    if (options.twrap == TextureOpt::WrapPeriodic && ispow2(spec.height))
        options.twrap = TextureOpt::WrapPeriodicPow2;

    if (subinfo.is_constant_image && options.swrap != TextureOpt::WrapBlack &&
          options.twrap != TextureOpt::WrapBlack) {
        // Lookup of constant color texture, non-black wrap -- skip all the
        // hard stuff.
        for (int c = 0; c < actualchannels; ++c)
            result[c] = subinfo.average_color[c+options.firstchannel];
        for (int c = actualchannels; c < nchannels; ++c)
            result[c] = options.fill;
        if (dresultds) {
            // Derivs are always 0 from a constant texture lookup
            for (int c = 0; c < nchannels; ++c) {
                dresultds[c] = 0.0f;
                dresultdt[c] = 0.0f;
            }
        }
        if (actualchannels < nchannels && options.firstchannel == 0 && m_gray_to_rgb)
            fill_gray_channels (spec, nchannels, result,
                                dresultds, dresultdt);
        return true;
    }

    if (m_flip_t) {
        t = 1.0f - t;
        dtdx *= -1.0f;
        dtdy *= -1.0f;
    }

    if (! subinfo.full_pixel_range) {  // remap st for overscan or crop
        s = s * subinfo.sscale + subinfo.soffset;
        dsdx *= subinfo.sscale;
        dsdy *= subinfo.sscale;
        t = t * subinfo.tscale + subinfo.toffset;
        dtdx *= subinfo.tscale;
        dtdy *= subinfo.tscale;
    }

    bool ok;
    // Everything from the lookup function on down will assume that there
    // is space for a vfloat4 in all of the result locations, so if that's
    // not the case (or it's not properly aligned), make a local copy and
    // then copy back when we're done.
    // FIXME -- is this necessary at all? Can we eliminate the conditional
    // and the duplicated code by always doing the simd copy thing? Come
    // back here and time whether for 4-channnel textures it really matters.
    bool simd_copy = (nchannels != 4 || ((size_t)result&0x0f) ||
                      ((size_t)dresultds & 0x0f) || /* FIXME -- necessary? */
                      ((size_t)dresultdt & 0x0f));
    if (simd_copy) {
        simd::vfloat4 result_simd, dresultds_simd, dresultdt_simd;
        float * OIIO_RESTRICT saved_dresultds = dresultds;
        float * OIIO_RESTRICT saved_dresultdt = dresultdt;
        if (saved_dresultds) {
            dresultds = (float *)&dresultds_simd;
            dresultdt = (float *)&dresultdt_simd;
        }
        ok = (this->*lookup) (*texturefile, thread_info, options,
                              nchannels, actualchannels,
                              s, t, dsdx, dtdx, dsdy, dtdy, (float *)&result_simd,
                              dresultds, dresultdt);
        if (actualchannels < nchannels && options.firstchannel == 0 && m_gray_to_rgb)
            fill_gray_channels (spec, nchannels, (float *)&result_simd,
                                dresultds, dresultdt);
        result_simd.store (result, nchannels);
        if (saved_dresultds) {
            if (m_flip_t)
                dresultdt_simd = -dresultdt_simd;
            dresultds_simd.store (saved_dresultds, nchannels);
            dresultdt_simd.store (saved_dresultdt, nchannels);
        }
    } else {
        // All provided output slots are aligned 4-floats, use them directly
        ok = (this->*lookup) (*texturefile, thread_info, options,
                              nchannels, actualchannels,
                              s, t, dsdx, dtdx, dsdy, dtdy, result,
                              dresultds, dresultdt);
        if (actualchannels < nchannels && options.firstchannel == 0 && m_gray_to_rgb)
            fill_gray_channels (spec, nchannels, result,
                                dresultds, dresultdt);
        if (m_flip_t && dresultdt)
            *(vfloat4 *)dresultdt = -(*(vfloat4 *)dresultdt);
    }

    return ok;
}



bool
TextureSystemImpl::texture (ustring filename, TextureOptBatch &options,
                            Tex::RunMask mask, const float *s, const float *t,
                            const float *dsdx, const float *dtdx,
                            const float *dsdy, const float *dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    Perthread *thread_info = get_perthread_info();
    TextureHandle *texture_handle = get_texture_handle (filename, thread_info);
    return texture (texture_handle, thread_info, options, mask,
                    s, t, dsdx, dtdx, dsdy, dtdy,
                    nchannels, result, dresultds, dresultdt);
}


bool
TextureSystemImpl::texture (TextureHandle *texture_handle,
                            Perthread *thread_info, TextureOptBatch &options,
                            Tex::RunMask mask, const float *s, const float *t,
                            const float *dsdx, const float *dtdx,
                            const float *dsdy, const float *dtdy,
                            int nchannels, float *result,
                            float *dresultds, float *dresultdt)
{
    // (FIXME) CHEAT! Texture points individually
    TextureOpt opt;
    opt.firstchannel = options.firstchannel;
    opt.subimage = options.subimage;
    opt.subimagename = options.subimagename;
    opt.swrap = (TextureOpt::Wrap) options.swrap;
    opt.twrap = (TextureOpt::Wrap) options.twrap;
    opt.mipmode = (TextureOpt::MipMode) options.mipmode;
    opt.interpmode = (TextureOpt::InterpMode) options.interpmode;
    opt.anisotropic = options.anisotropic;
    opt.conservative_filter = options.conservative_filter;
    opt.fill = options.fill;
    opt.missingcolor = options.missingcolor;
    // rwrap not needed for 2D texture

    bool ok = true;
    Tex::RunMask bit = 1;
    for (int i = 0; i < Tex::BatchWidth;  ++i, bit <<= 1) {
        float r[4], drds[4], drdt[4];  // temp result
        if (mask & bit) {
            opt.sblur = options.sblur[i];
            opt.tblur = options.tblur[i];
            opt.swidth = options.swidth[i];
            opt.twidth = options.twidth[i];
            // rblur, rwidth not needed for 2D texture
            if (dresultds) {
                ok &= texture (texture_handle, thread_info,
                               opt, s[i], t[i], dsdx[i], dtdx[i],
                               dsdy[i], dtdy[i], nchannels, r, drds, drdt);
                for (int c = 0; c < nchannels; ++c) {
                    result[c*Tex::BatchWidth+i] = r[c];
                    dresultds[c*Tex::BatchWidth+i] = drds[c];
                    dresultdt[c*Tex::BatchWidth+i] = drdt[c];
                }
            } else {
                ok &= texture (texture_handle, thread_info,
                               opt, s[i], t[i], dsdx[i], dtdx[i],
                               dsdy[i], dtdy[i], nchannels, r);
                for (int c = 0; c < nchannels; ++c) {
                    result[c*Tex::BatchWidth+i] = r[c];
                }
            }
        }
    }
    return ok;
}



bool
TextureSystemImpl::texture_lookup_nomip (TextureFile &texturefile,
                            PerThreadInfo *thread_info, 
                            TextureOpt &options,
                            int nchannels_result, int actualchannels,
                            float s, float t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result, float *dresultds, float *dresultdt)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    DASSERT ((dresultds == NULL) == (dresultdt == NULL));
    ((simd::vfloat4 *)result)->clear();
    if (dresultds) {
        ((simd::vfloat4 *)dresultds)->clear();
        ((simd::vfloat4 *)dresultdt)->clear();
    }

    static const sampler_prototype sample_functions[] = {
        // Must be in the same order as InterpMode enum
        &TextureSystemImpl::sample_closest,
        &TextureSystemImpl::sample_bilinear,
        &TextureSystemImpl::sample_bicubic,
        &TextureSystemImpl::sample_bilinear,
    };
    sampler_prototype sampler = sample_functions[(int)options.interpmode];
    OIIO_SIMD4_ALIGN float sval[4] = { s, 0.0f, 0.0f, 0.0f };
    OIIO_SIMD4_ALIGN float tval[4] = { t, 0.0f, 0.0f, 0.0f };
    static OIIO_SIMD4_ALIGN float weight[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    bool ok = (this->*sampler) (1, sval, tval, 0 /*miplevel*/,
                                texturefile, thread_info, options,
                                nchannels_result, actualchannels, weight,
                                (vfloat4 *)result, (vfloat4 *)dresultds, (vfloat4 *)dresultdt);

    // Update stats
    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.aniso_queries;
    ++stats.aniso_probes;
    switch (options.interpmode) {
        case TextureOpt::InterpClosest :  ++stats.closest_interps;  break;
        case TextureOpt::InterpBilinear : ++stats.bilinear_interps; break;
        case TextureOpt::InterpBicubic :  ++stats.cubic_interps;  break;
        case TextureOpt::InterpSmartBicubic : ++stats.bilinear_interps; break;
    }
    return ok;
}



// Scale the derivs as dictated by 'width' and 'blur', and also make sure
// they are all some minimum value to make the subsequent math clean.
inline void
adjust_width (float &dsdx, float &dtdx, float &dsdy, float &dtdy,
              float swidth, float twidth)
{
    // Trust user not to use nonsensical width<0
    dsdx *= swidth;
    dtdx *= twidth;
    dsdy *= swidth;
    dtdy *= twidth;

    // Clamp degenerate derivatives so they don't cause mathematical problems
    static const float eps = 1.0e-8f, eps2 = eps*eps;
    float dxlen2 = dsdx*dsdx + dtdx*dtdx;
    float dylen2 = dsdy*dsdy + dtdy*dtdy;
    if (dxlen2 < eps2) {   // Tiny dx
        if (dylen2 < eps2) {
            // Tiny dx and dy: Essentially point sampling.  Substitute a
            // tiny but finite filter.
            dsdx = eps; dsdy = 0;
            dtdx = 0;   dtdy = eps;
            dxlen2 = dylen2 = eps2;
        } else {
            // Tiny dx, sane dy -- pick a small dx orthogonal to dy, but
            // of length eps.
            float scale = eps / sqrtf(dylen2);
            dsdx = dtdy * scale;
            dtdx = -dsdy * scale;
            dxlen2 = eps2;
        }
    } else if (dylen2 < eps2) {
        // Tiny dy, sane dx -- pick a small dy orthogonal to dx, but of
        // length eps.
        float scale = eps / sqrtf(dxlen2);
        dsdy = -dtdx * scale;
        dtdy = dsdx * scale;
        dylen2 = eps2;
    }
}



// Adjust the ellipse major and minor axes based on the blur, if nonzero.
// Trust user not to use nonsensical blur<0
inline void
adjust_blur (float &majorlength, float &minorlength, float theta,
             float sblur, float tblur)
{
    if (sblur+tblur != 0.0f /* avoid the work when blur is zero */) {
        // Carefully add blur to the right derivative components in the
        // right proportions -- merely adding the same amount of blur
        // to all four derivatives blurs too much at some angles.
        // FIXME -- we should benchmark whether a fast approximate rsqrt
        // here could have any detectable performance improvement.
        DASSERT (majorlength > 0.0f && minorlength > 0.0f);
        float sintheta, costheta;
#ifdef TEX_FAST_MATH
        fast_sincos (theta, &sintheta, &costheta);
#else
        sincos (theta, &sintheta, &costheta);
#endif
        sintheta = fabsf(sintheta);
        costheta = fabsf(costheta);
        majorlength += sblur * costheta + tblur * sintheta;
        minorlength += sblur * sintheta + tblur * costheta;
    }
}



// For the given texture file, options, and ellipse major and minor
// lengths and aspect ratio, compute the two MIPmap levels and
// respective weights to use for a texture lookup.  The general strategy
// is that we choose the MIPmap level so that the minor axis length is
// pixel-sized (and then we will sample several times along the major
// axis in order to handle anisotropy), but we make adjustments in
// corner cases where the ideal sampling is too high or too low resolution
// given the MIPmap levels we have available.
inline void
compute_miplevels (TextureSystemImpl::TextureFile &texturefile,
                   TextureOpt &options,
                   float majorlength, float minorlength, float &aspect,
                   int *miplevel, float *levelweight)
{
    ImageCacheFile::SubimageInfo &subinfo (texturefile.subimageinfo(options.subimage));
    float levelblend = 0.0f;
    int nmiplevels = (int)subinfo.levels.size();
    for (int m = 0;  m < nmiplevels;  ++m) {
        // Compute the filter size (minor axis) in raster space at this
        // MIP level.  We use the smaller of the two texture resolutions,
        // which is better than just using one, but a more principled
        // approach is desired but remains elusive.  FIXME.
        float filtwidth_ras = minorlength * std::min (subinfo.spec(m).width, subinfo.spec(m).height);
        // Once the filter width is smaller than one texel at this level,
        // we've gone too far, so we know that we want to interpolate the
        // previous level and the current level.  Note that filtwidth_ras
        // is expected to be >= 0.5, or would have stopped one level ago.
        if (filtwidth_ras <= 1.0f) {
            miplevel[0] = m-1;
            miplevel[1] = m;
            levelblend = Imath::clamp (2.0f*filtwidth_ras - 1.0f, 0.0f, 1.0f);
            break;
        }
    }

    if (miplevel[1] < 0) {
        // We'd like to blur even more, but make due with the coarsest
        // MIP level.
        miplevel[0] = nmiplevels - 1;
        miplevel[1] = miplevel[0];
        levelblend = 0;
    } else if (miplevel[0] < 0) {
        // We wish we had even more resolution than the finest MIP level,
        // but tough for us.
        miplevel[0] = 0;
        miplevel[1] = 0;
        levelblend = 0;
        // It's possible that minorlength is degenerate, giving an aspect
        // ratio that implies a huge nsamples, which is pointless if those
        // samples are too close.  So if minorlength is less than 1/2 texel
        // at the finest resolution, clamp it and recalculate aspect.
        int r = std::max (subinfo.spec(0).full_width, subinfo.spec(0).full_height);
        if (minorlength*r < 0.5f) {
            aspect = Imath::clamp (majorlength * r * 2.0f, 1.0f, float(options.anisotropic));
        }
    }
    if (options.mipmode == TextureOpt::MipModeOneLevel) {
        miplevel[0] = miplevel[1];
        levelblend = 0;
    }
    levelweight[0] = 1.0f - levelblend;
    levelweight[1] = levelblend;
}



bool
TextureSystemImpl::texture_lookup_trilinear_mipmap (TextureFile &texturefile,
                            PerThreadInfo *thread_info,
                            TextureOpt &options,
                            int nchannels_result, int actualchannels,
                            float s, float t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result, float *dresultds, float *dresultdt)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    DASSERT ((dresultds == NULL) == (dresultdt == NULL));
    ((simd::vfloat4 *)result)->clear();
    if (dresultds) {
        ((simd::vfloat4 *)dresultds)->clear();
        ((simd::vfloat4 *)dresultdt)->clear();
    }

    adjust_width (dsdx, dtdx, dsdy, dtdy, options.swidth, options.twidth);

    // Determine the MIP-map level(s) we need: we will blend
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    int miplevel[2] = { -1, -1 };
    float levelweight[2] = { 0, 0 };
    float sfilt = std::max (fabsf(dsdx), fabsf(dsdy));
    float tfilt = std::max (fabsf(dtdx), fabsf(dtdy));
    float filtwidth = options.conservative_filter ? std::max (sfilt, tfilt)
                                                  : std::min (sfilt, tfilt);
    // account for blur
    filtwidth += std::max (options.sblur, options.tblur);
    float aspect = 1.0f;
    compute_miplevels (texturefile, options, filtwidth, filtwidth, aspect,
                       miplevel, levelweight);

    static const sampler_prototype sample_functions[] = {
        // Must be in the same order as InterpMode enum
        &TextureSystemImpl::sample_closest,
        &TextureSystemImpl::sample_bilinear,
        &TextureSystemImpl::sample_bicubic,
        &TextureSystemImpl::sample_bilinear,
    };
    sampler_prototype sampler = sample_functions[(int)options.interpmode];

    // FIXME -- support for smart cubic?

    OIIO_SIMD4_ALIGN float sval[4] = { s, 0.0f, 0.0f, 0.0f };
    OIIO_SIMD4_ALIGN float tval[4] = { t, 0.0f, 0.0f, 0.0f };
    OIIO_SIMD4_ALIGN float weight[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    bool ok = true;
    int npointson = 0;
    vfloat4 r_sum, drds_sum, drdt_sum;
    r_sum.clear();
    if (dresultds) {
        drds_sum.clear(); drdt_sum.clear();
    }
    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        vfloat4 r, drds, drdt;
        ok &= (this->*sampler) (1, sval, tval, miplevel[level],
                                texturefile, thread_info, options,
                                nchannels_result, actualchannels, weight,
                                &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
        ++npointson;
        vfloat4 lw = levelweight[level];
        r_sum += lw * r;
        if (dresultds) {
            drds_sum += lw * drds;
            drdt_sum += lw * drdt;
        }
    }

    *(simd::vfloat4 *)(result) = r_sum;
    if (dresultds) {
        *(simd::vfloat4 *)(dresultds) = drds_sum;
        *(simd::vfloat4 *)(dresultdt) = drdt_sum;
    }

    // Update stats
    ImageCacheStatistics &stats (thread_info->m_stats);
    stats.aniso_queries += npointson;
    stats.aniso_probes += npointson;
    switch (options.interpmode) {
        case TextureOpt::InterpClosest :  stats.closest_interps += npointson;  break;
        case TextureOpt::InterpBilinear : stats.bilinear_interps += npointson; break;
        case TextureOpt::InterpBicubic :  stats.cubic_interps += npointson;  break;
        case TextureOpt::InterpSmartBicubic : stats.bilinear_interps += npointson; break;
    }
    return ok;
}



// Given pixel derivatives, calculate major and minor axis lengths and
// major axis orientation angle of the ellipse.  See Greene's EWA paper
// or Mavridis (2011).  If ABCF is non-NULL, it should point to space
// for 4 floats, which will be used to store the ellipse parameters A,
// B, C, F.
inline void
ellipse_axes (float dsdx, float dtdx, float dsdy, float dtdy,
              float &majorlength, float &minorlength, float &theta,
              float *ABCF = NULL)
{
    float dsdx2 = dsdx*dsdx;
    float dtdx2 = dtdx*dtdx;
    float dsdy2 = dsdy*dsdy;
    float dtdy2 = dtdy*dtdy;
    double A = dtdx2 + dtdy2;
    double B = -2.0 * (dsdx * dtdx + dsdy * dtdy);
    double C = dsdx2 + dsdy2;
    double root = hypot (A-C, B);  // equivalent: sqrt (A*A - 2AC + C*C + B*B)
    double Aprime = (A + C - root) * 0.5;
    double Cprime = (A + C + root) * 0.5;
#if 0
    double F = A*C - B*B*0.25;
    majorlength = std::min(safe_sqrt (float(F / Aprime)), 1000.0f);
    minorlength = std::min(safe_sqrt (float(F / Cprime)), 1000.0f);
#else
    // Wolfram says that this is equivalent to:
    majorlength = std::min (safe_sqrt(float(Cprime)), 1000.0f);
    minorlength = std::min (safe_sqrt(float(Aprime)), 1000.0f);
#endif
#ifdef TEX_FAST_MATH
    theta = fast_atan2 (B, A-C) * 0.5f + float(M_PI_2);
#else
    theta = atan2 (B, A-C) * 0.5 + M_PI_2;
#endif
    if (ABCF) {
        // Optionally store the ellipse equation parameters, the ellipse
        // is given by: A*u^2 + B*u*v + C*v^2 < 1
        double F = A*C - B*B*0.25;
        double Finv = 1.0f / F;
        ABCF[0] = A * Finv;
        ABCF[1] = B * Finv;
        ABCF[2] = C * Finv;
        ABCF[3] = F;
    }

    // N.B. If the derivs passed in are the full pixel-to-pixel
    // derivatives, then majorlength/minorlength are the (diameter) axes
    // of the ellipse; if the derivs are the "to edge of pixel" 1/2
    // length derivs, then majorlength/minorlength are the major and
    // minur radii of the elipse.  We do the former!  So it's important
    // to consider that factor of 2 in compute_ellipse_sampling.
}



// Given the aspect ratio, major axis orientation angle, and axis lengths,
// calculate the smajor & tmajor values that give the orientation of the
// line on which samples should be distributed.  If there are n samples,
// they should be positioned as:
//     p_i = 2*(i+0.5)/n - 1.0;
//     sample_i = (s + p_i*smajor, t + p_i*tmajor)
// If a weights ptr is supplied, it will be filled in [0..nsamples-1] with
// normalized weights for each sample.
inline int
compute_ellipse_sampling (float aspect, float theta,
                          float majorlength, float minorlength,
                          float &smajor, float &tmajor,
                          float &invsamples,
                          float *weights=NULL)
{
    // Compute the sin and cos of the sampling direction, given major
    // axis angle
    sincos (theta, &tmajor, &smajor);
    float L = 2.0f * (majorlength - minorlength);
    smajor *= L;
    tmajor *= L;
#if 1
    // This is the theoretically correct number of samples.
    int nsamples = std::max (1, int(2.0f*aspect-1.0f));
#else
    int nsamples = std::max (1, (int) ceilf (aspect - 0.3f));
    // This approach does fewer samples for high aspect ratios, but I see
    // artifacts.
#endif
    invsamples = 1.0f / nsamples;
    if (weights) {
        if (nsamples == 1) {
            weights[0] = 1.0f;
        } else if (nsamples == 2) {
            weights[0] = 0.5f;  weights[1] = 0.5f;
        } else {
            float scale = majorlength / L;  // 1/(L/major)
            for (int i = 0, e = (nsamples+1)/2;  i < e;  ++i) {
                float x = (2.0f*(i+0.5f)*invsamples - 1.0f) * scale;
#ifdef TEX_FAST_MATH
                float w = fast_exp(-2.0f*x*x);
#else
                float w = expf(-2.0f*x*x);
#endif
                weights[nsamples-i-1] = weights[i] = w;
            }
            float sumw = 0.0f;
            for (int i = 0; i < nsamples; ++i)
                sumw += weights[i];
            for (int i = 0; i < nsamples; ++i)
                weights[i] /= sumw;
        }
    }
    return nsamples;
}



bool
TextureSystemImpl::texture_lookup (TextureFile &texturefile,
                            PerThreadInfo *thread_info,
                            TextureOpt &options,
                            int nchannels_result, int actualchannels,
                            float s, float t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result, float *dresultds, float *dresultdt)
{
    DASSERT ((dresultds == NULL) == (dresultdt == NULL));

    // Compute the natural resolution we want for the bare derivs, this
    // will be the threshold for knowing we're maxifying (and therefore
    // wanting cubic interpolation).
    float sfilt_noblur = std::max (std::max (fabsf(dsdx), fabsf(dsdy)), 1e-8f);
    float tfilt_noblur = std::max (std::max (fabsf(dtdx), fabsf(dtdy)), 1e-8f);
    int naturalsres = (int) (1.0f / sfilt_noblur);
    int naturaltres = (int) (1.0f / tfilt_noblur);

    // Scale by 'width'
    adjust_width (dsdx, dtdx, dsdy, dtdy, options.swidth, options.twidth);

    // Determine the MIP-map level(s) we need: we will blend
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    float smajor, tmajor;
    float majorlength, minorlength;
    float theta;

    // Do a bit more math and get the exact ellipse axis lengths, and
    // therefore a more accurate aspect ratio as well.  Looks much MUCH
    // better, but for scenes with lots of grazing angles, it can greatly
    // increase the average anisotropy, therefore the number of bilinear
    // or bicubic texture probes, and therefore runtime!
    ellipse_axes (dsdx, dtdx, dsdy, dtdy, majorlength, minorlength, theta);

    adjust_blur (majorlength, minorlength, theta, options.sblur, options.tblur);

    float aspect, trueaspect;
    aspect = anisotropic_aspect (majorlength, minorlength, options, trueaspect);

    int miplevel[2] = { -1, -1 };
    float levelweight[2] = { 0, 0 };
    compute_miplevels (texturefile, options, majorlength, minorlength, aspect,
                       miplevel, levelweight);

    float *lineweight = ALLOCA (float, round_to_multiple_of_pow2(2*options.anisotropic, 4));
    float invsamples;
    int nsamples = compute_ellipse_sampling (aspect, theta, majorlength,
                                             minorlength, smajor, tmajor,
                                             invsamples, lineweight);
    // All the computations were done assuming full diametric axes of
    // the ellipse, but our derivatives are pixel-to-pixel, yielding
    // semi-major and semi-minor lengths, so we need to scale everything
    // by 1/2.
    smajor *= 0.5f;
    tmajor *= 0.5f;

    bool ok = true;
    int npointson = 0;
    int closestprobes = 0, bilinearprobes = 0, bicubicprobes = 0;
    int nsamples_padded = round_to_multiple_of_pow2 (nsamples, 4);
    float *sval = OIIO_ALLOCA (float, nsamples_padded);
    float *tval = OIIO_ALLOCA (float, nsamples_padded);

    // Compute the s and t positions of the samples along the major axis.
#if OIIO_SIMD
    // Do the computations in batches of 4, with SIMD ops.
    static OIIO_SIMD4_ALIGN float iota_start[4] = { 0.5f, 1.5f, 2.5f, 3.5f };
    vfloat4 iota = *(const vfloat4 *)iota_start;
    for (int sample = 0;  sample < nsamples;  sample += 4) {
        vfloat4 pos = 2.0f * (iota * invsamples - 0.5f);
        vfloat4 ss = s + pos * smajor;
        vfloat4 tt = t + pos * tmajor;
        ss.store (sval+sample);
        tt.store (tval+sample);
        iota += 4.0f;
    }
#else
    // Non-SIMD, reference code
    for (int sample = 0;  sample < nsamples;  ++sample) {
        float pos = 2.0f * ((sample + 0.5f) * invsamples - 0.5f);
        sval[sample] = s + pos * smajor;
        tval[sample] = t + pos * tmajor;
    }
#endif

    vfloat4 r_sum, drds_sum, drdt_sum;
    r_sum.clear();
    if (dresultds) {
        drds_sum.clear(); drdt_sum.clear();
    }
    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        ++npointson;
        vfloat4 r, drds, drdt;
        int lev = miplevel[level];
        switch (options.interpmode) {
        case TextureOpt::InterpClosest :
            ok &= sample_closest (nsamples, sval, tval,
                                  lev, texturefile, thread_info, options,
                                  nchannels_result, actualchannels, lineweight,
                                  &r, NULL, NULL);
            ++closestprobes;
            break;
        case TextureOpt::InterpBilinear :
            ok &= sample_bilinear (nsamples, sval, tval,
                                   lev, texturefile, thread_info, options,
                                   nchannels_result, actualchannels, lineweight,
                                   &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
            ++bilinearprobes;
            break;
        case TextureOpt::InterpBicubic :
            ok &= sample_bicubic (nsamples, sval, tval,
                                  lev, texturefile, thread_info, options,
                                  nchannels_result, actualchannels, lineweight,
                                  &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
            ++bicubicprobes;
            break;
        case TextureOpt::InterpSmartBicubic :
            if (lev == 0 || 
                (texturefile.spec(options.subimage,lev).width < naturalsres/2) ||
                (texturefile.spec(options.subimage,lev).height < naturaltres/2)) {
                ok &= sample_bicubic (nsamples, sval, tval,
                                      lev, texturefile, thread_info, options,
                                      nchannels_result, actualchannels, lineweight,
                                      &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
                ++bicubicprobes;
            } else {
                ok &= sample_bilinear (nsamples, sval, tval,
                                       lev, texturefile, thread_info, options,
                                       nchannels_result, actualchannels, lineweight,
                                       &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
                ++bilinearprobes;
            }
            break;
        }

        vfloat4 lw = levelweight[level];
        r_sum += lw * r;
        if (dresultds) {
            drds_sum += lw * drds;
            drdt_sum += lw * drdt;
        }
    }

    *(simd::vfloat4 *)(result) = r_sum;
    if (dresultds) {
        *(simd::vfloat4 *)(dresultds) = drds_sum;
        *(simd::vfloat4 *)(dresultdt) = drdt_sum;
    }

    // Update stats
    ImageCacheStatistics &stats (thread_info->m_stats);
    stats.aniso_queries += npointson;
    stats.aniso_probes += npointson * nsamples;
    if (trueaspect > stats.max_aniso)
        stats.max_aniso = trueaspect;   // FIXME?
    stats.closest_interps += closestprobes * nsamples;
    stats.bilinear_interps += bilinearprobes * nsamples;
    stats.cubic_interps += bicubicprobes * nsamples;

    return ok;
}



const float *
TextureSystemImpl::pole_color (TextureFile &texturefile,
                               PerThreadInfo *thread_info,
                               const ImageCacheFile::LevelInfo &levelinfo,
                               TileRef &tile,
                               int subimage, int miplevel, int pole)
{
    if (! levelinfo.onetile)
        return NULL;   // Only compute color for one-tile MIP levels
    const ImageSpec &spec (levelinfo.spec);
    if (! levelinfo.polecolorcomputed) {
        static spin_mutex mutex;   // Protect everybody's polecolor
        spin_lock lock (mutex);
        if (! levelinfo.polecolorcomputed) {
            DASSERT (levelinfo.polecolor.size() == 0);
            levelinfo.polecolor.resize (2*spec.nchannels);
            ASSERT (tile->id().nchannels() == spec.nchannels &&
                    "pole_color doesn't work for channel subsets");
            int pixelsize = tile->pixelsize();
            TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(subimage);
            // We store north and south poles adjacently in polecolor
            float *p = &(levelinfo.polecolor[0]);
            int width = spec.width;
            float scale = 1.0f / width;
            for (int pole = 0;  pole <= 1;  ++pole, p += spec.nchannels) {
                int y = pole * (spec.height-1);   // 0 or height-1
                for (int c = 0;  c < spec.nchannels;  ++c)
                    p[c] = 0.0f;
                const unsigned char *texel = tile->bytedata() + y*spec.tile_width*pixelsize;
                for (int i = 0;  i < width;  ++i, texel += pixelsize)
                    for (int c = 0;  c < spec.nchannels;  ++c) {
                        if (pixeltype == TypeDesc::UINT8)
                            p[c] += uchar2float(texel[c]);
                        else if (pixeltype == TypeDesc::UINT16)
                            p[c] += convert_type<uint16_t,float> (((const uint16_t *)texel)[c]);
                        else if (pixeltype == TypeDesc::HALF)
                            p[c] += ((const half *)texel)[c];
                        else {
                            DASSERT (pixeltype == TypeDesc::FLOAT);
                            p[c] += ((const float *)texel)[c];
                        }
                    }
                for (int c = 0;  c < spec.nchannels;  ++c)
                    p[c] *= scale;
            }            
            levelinfo.polecolorcomputed = true;
        }
    }
    return &(levelinfo.polecolor[pole*spec.nchannels]);
}



void
TextureSystemImpl::fade_to_pole (float t, float *accum, float &weight,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 const ImageCacheFile::LevelInfo &levelinfo,
                                 TextureOpt &options,  int miplevel,
                                 int nchannels)
{
    // N.B. We want to fade to pole colors right at texture
    // boundaries t==0 and t==height, but at the very top of this
    // function, we subtracted another 0.5 from t, so we need to
    // undo that here.
    float pole;
    const float *polecolor;
    if (t < 1.0f) {
        pole = (1.0f - t);
        polecolor = pole_color (texturefile, thread_info, levelinfo,
                                thread_info->tile, options.subimage,
                                miplevel, 0);
    } else {
        pole = t - floorf(t);
        polecolor = pole_color (texturefile, thread_info, levelinfo,
                                thread_info->tile, options.subimage,
                                miplevel, 1);
    }
    pole = Imath::clamp (pole, 0.0f, 1.0f);
    pole *= pole;  // squaring makes more pleasing appearance
    polecolor += options.firstchannel;
    for (int c = 0;  c < nchannels;  ++c)
        accum[c] += weight * pole * polecolor[c];
    weight *= 1.0f - pole;
}



bool
TextureSystemImpl::sample_closest (int nsamples, const float *s_,
                                   const float *t_, int miplevel,
                                   TextureFile &texturefile,
                                   PerThreadInfo *thread_info,
                                   TextureOpt &options,
                                   int nchannels_result, int actualchannels,
                                   const float *weight_,
                                   vfloat4 *accum_, vfloat4 *daccumds_, vfloat4 *daccumdt_)
{
    bool allok = true;
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(options.subimage);
    wrap_impl swrap_func = wrap_functions[(int)options.swrap];
    wrap_impl twrap_func = wrap_functions[(int)options.twrap];
    vfloat4 accum;
    accum.clear();
    float nonfill = 0.0f;
    int firstchannel = options.firstchannel;
    int tile_chbegin = 0, tile_chend = spec.nchannels;
    if (spec.nchannels > m_max_tile_channels) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = options.firstchannel;
        tile_chend = options.firstchannel+actualchannels;
    }
    TileID id (texturefile, options.subimage, miplevel, 0, 0, 0,
               tile_chbegin, tile_chend);
    for (int sample = 0;  sample < nsamples;  ++sample) {
        float s = s_[sample], t = t_[sample];
        float weight = weight_[sample];

        int stex, ttex;    // Texel coordintes
        float sfrac, tfrac;
        st_to_texel (s, t, texturefile, spec, stex, ttex, sfrac, tfrac);
    
        if (sfrac > 0.5f)
            ++stex;
        if (tfrac > 0.5f)
            ++ttex;
        
        // Wrap
        bool svalid, tvalid;  // Valid texels?  false means black border
        svalid = swrap_func (stex, spec.x, spec.width);
        tvalid = twrap_func (ttex, spec.y, spec.height);
        if (! levelinfo.full_pixel_range) {
            svalid &= (stex >= spec.x && stex < (spec.x+spec.width)); // data window
            tvalid &= (ttex >= spec.y && ttex < (spec.y+spec.height));
        }
        if (! (svalid & tvalid)) {
            // All texels we need were out of range and using 'black' wrap.
            nonfill += weight;
            continue;
        }
    
        int tile_s = (stex - spec.x) % spec.tile_width;
        int tile_t = (ttex - spec.y) % spec.tile_height;
        id.xy (stex - tile_s, ttex - tile_t);
        bool ok = find_tile (id, thread_info);
        if (! ok)
            error ("%s", m_imagecache->geterror());
        TileRef &tile (thread_info->tile);
        if (! tile  ||  ! ok) {
            allok = false;
            continue;
        }
        int offset = id.nchannels() * (tile_t * spec.tile_width + tile_s)
                        + (firstchannel - id.chbegin());
        DASSERT ((size_t)offset < spec.nchannels*spec.tile_pixels());
        simd::vfloat4 texel_simd;
        if (pixeltype == TypeDesc::UINT8) {
            // special case for 8-bit tiles
            texel_simd = uchar2float4 (tile->bytedata() + offset);
        } else if (pixeltype == TypeDesc::UINT16) {
            texel_simd = ushort2float4 (tile->ushortdata() + offset);
        } else if (pixeltype == TypeDesc::HALF) {
            texel_simd = half2float4 (tile->halfdata() + offset);
        } else {
            DASSERT (pixeltype == TypeDesc::FLOAT);
            texel_simd.load (tile->floatdata() + offset);
        }

        accum += weight * texel_simd;
    }
    simd::vbool4 channel_mask = channel_masks[actualchannels];
    accum = blend0(accum, channel_mask);
    if (nonfill < 1.0f && nchannels_result > actualchannels && options.fill) {
        // Add the weighted fill color
        accum += blend0not(vfloat4((1.0f - nonfill) * options.fill), channel_mask);
    }

    *accum_ = accum;
    if (daccumds_) {
        daccumds_->clear();  // constant interp has 0 derivatives
        daccumdt_->clear();
    }
    return allok;
}



/// Convert texture coordinates (s,t), which range on 0-1 for the "full"
/// image boundary, to texel coordinates (i+ifrac,j+jfrac) where (i,j) is
/// the texel to the immediate upper left of the sample position, and ifrac
/// and jfrac are the fractional (0-1) portion of the way to the next texel
/// to the right or down, respectively.  Do this for 4 s,t values at a time.
inline void
st_to_texel_simd (const vfloat4& s_, const vfloat4& t_, TextureSystemImpl::TextureFile &texturefile,
                  const ImageSpec &spec, vint4 &i, vint4 &j,
                  vfloat4 &ifrac, vfloat4 &jfrac)
{
	vfloat4 s,t;
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords.
    // Note that we have two modes, depending on the m_sample_border.
    if (texturefile.sample_border() == 0) {
        // texel samples are at 0.5/res, 1.5/res, ..., (res-0.5)/res,
        s = s_ * float(spec.width)  + (spec.x - 0.5f);
        t = t_ * float(spec.height) + (spec.y - 0.5f);
    } else {
        // first and last rows/columns are *exactly* on the boundary,
        // so samples are at 0, 1/(res-1), ..., 1.
        s = s_ * float(spec.width-1)  + float(spec.x);
        t = t_ * float(spec.height-1) + float(spec.y);
    }
    ifrac = floorfrac (s, &i);
    jfrac = floorfrac (t, &j);
    // Now (i,j) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (ifrac,jfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).
}



bool
TextureSystemImpl::sample_bilinear (int nsamples, const float *s_,
                                    const float *t_, int miplevel,
                                    TextureFile &texturefile,
                                    PerThreadInfo *thread_info,
                                    TextureOpt &options,
                                    int nchannels_result, int actualchannels,
                                    const float *weight_,
                                    vfloat4 *accum_, vfloat4 *daccumds_, vfloat4 *daccumdt_)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(options.subimage);
    wrap_impl swrap_func = wrap_functions[(int)options.swrap];
    wrap_impl twrap_func = wrap_functions[(int)options.twrap];
    wrap_impl_simd wrap_func = (swrap_func == twrap_func) ? wrap_functions_simd[(int)options.swrap] : NULL;
    simd::vint4 xy (spec.x, spec.y);
    simd::vint4 widthheight (spec.width, spec.height);
    simd::vint4 tilewh (spec.tile_width, spec.tile_height);
    simd::vint4 tilewhmask = tilewh - 1;
    bool use_fill = (nchannels_result > actualchannels && options.fill);
    bool tilepow2 = ispow2(spec.tile_width) && ispow2(spec.tile_height);
    size_t channelsize = texturefile.channelsize(options.subimage);
    int firstchannel = options.firstchannel;
    int tile_chbegin = 0, tile_chend = spec.nchannels;
    // need_pole: do we potentially need to fade to special pole color?
    // If we do, can't restrict channel range or fade_to_pole won't work.
    bool need_pole = (options.envlayout == LayoutLatLong && levelinfo.onetile);
    if (spec.nchannels > m_max_tile_channels && !need_pole) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = options.firstchannel;
        tile_chend = options.firstchannel+actualchannels;
    }
    TileID id (texturefile, options.subimage, miplevel, 0, 0, 0,
               tile_chbegin, tile_chend);
    float nonfill = 0.0f;  // The degree to which we DON'T need fill
    // N.B. What's up with "nofill"? We need to consider fill only when we
    // are inside the valid texture region. Outside, i.e. in the black wrap
    // region, black takes precedence over fill. By keeping track of when
    // we don't need to worry about fill, which is the comparitively rare
    // case, we do a lot less math and have fewer rounding errors.

    vfloat4 accum, daccumds, daccumdt;
    accum.clear();
    if (daccumds_) {
        daccumds.clear();
        daccumdt.clear();
    }
    vfloat4 s_simd, t_simd;
    vint4 sint_simd, tint_simd;
    vfloat4 sfrac_simd, tfrac_simd;
    for (int sample = 0;  sample < nsamples;  ++sample) {
        // To utilize SIMD ops in an inherently scalar loop, every fourth
        // step, we compute the st_to_texel for the next four samples.
        int sample4 = sample & 3;
        if (sample4 == 0) {
            s_simd.load (s_ + sample);
            t_simd.load (t_ + sample);
            st_to_texel_simd (s_simd, t_simd, texturefile, spec,
                         sint_simd, tint_simd, sfrac_simd, tfrac_simd);
        }
        int sint = sint_simd[sample4], tint = tint_simd[sample4];
        float sfrac = sfrac_simd[sample4], tfrac = tfrac_simd[sample4];
        float weight = weight_[sample];

        // SIMD-ize the indices. We have four texels, fit them into one SIMD
        // 4-vector as S0,S1,T0,T1.
        enum { S0=0, S1=1, T0=2, T1=3 };
    
        simd::vint4 sttex (sint, sint+1, tint, tint+1); // Texel coords: s0,s1,t0,t1
        simd::vbool4 stvalid;
        if (wrap_func) {
            // Both directions use the same wrap function, call in parallel.
            stvalid = wrap_func (sttex, xy, widthheight);
        } else {
            stvalid.load (swrap_func (sttex[S0], spec.x, spec.width),
                          swrap_func (sttex[S1], spec.x, spec.width),
                          twrap_func (sttex[T0], spec.y, spec.height),
                          twrap_func (sttex[T1], spec.y, spec.height));
        }
    
        // Account for crop windows
        if (! levelinfo.full_pixel_range) {
            stvalid &= (sttex >= xy) & (sttex < (xy + widthheight));
        }
        if (none (stvalid)) {
            nonfill += weight;
            continue; // All texels we need were out of range and using 'black' wrap
        }
    
        simd::vfloat4 texel_simd[2][2];
        simd::vint4 tile_st = (simd::vint4(simd::shuffle<S0,S0,T0,T0>(sttex)) - xy);
        if (tilepow2)
            tile_st &= tilewhmask;
        else
            tile_st %= tilewh;
        bool s_onetile = (tile_st[S0] != tilewhmask[S0]) & (sttex[S0]+1 == sttex[S1]);
        bool t_onetile = (tile_st[T0] != tilewhmask[T0]) & (sttex[T0]+1 == sttex[T1]);
        bool onetile = (s_onetile & t_onetile);
        if (onetile && all(stvalid)) {
            // Shortcut if all the texels we need are on the same tile
            id.xy (sttex[S0] - tile_st[S0], sttex[T0] - tile_st[T0]);
            bool ok = find_tile (id, thread_info);
            if (! ok)
                error ("%s", m_imagecache->geterror());
            TileRef &tile (thread_info->tile);
            if (! tile->valid())
                return false;
            int pixelsize = tile->pixelsize();
            int offset = pixelsize * (tile_st[T0] * spec.tile_width + tile_st[S0]);
            const unsigned char *p = tile->bytedata() + offset 
                                   + channelsize * (firstchannel - id.chbegin());
            if (pixeltype == TypeDesc::UINT8) {
                texel_simd[0][0] = uchar2float4 (p);
                texel_simd[0][1] = uchar2float4 (p+pixelsize);
                p += pixelsize * spec.tile_width;
                texel_simd[1][0] = uchar2float4 (p);
                texel_simd[1][1] = uchar2float4 (p+pixelsize);
            } else if (pixeltype == TypeDesc::UINT16) {
                texel_simd[0][0] = ushort2float4 ((uint16_t *)p);
                texel_simd[0][1] = ushort2float4 ((uint16_t *)(p+pixelsize));
                p += pixelsize * spec.tile_width;
                texel_simd[1][0] = ushort2float4 ((uint16_t *)p);
                texel_simd[1][1] = ushort2float4 ((uint16_t *)(p+pixelsize));
            } else if (pixeltype == TypeDesc::HALF) {
                texel_simd[0][0] = half2float4 ((half *)p);
                texel_simd[0][1] = half2float4 ((half *)(p+pixelsize));
                p += pixelsize * spec.tile_width;
                texel_simd[1][0] = half2float4 ((half *)p);
                texel_simd[1][1] = half2float4 ((half *)(p+pixelsize));
            } else {
                DASSERT (pixeltype == TypeDesc::FLOAT);
                texel_simd[0][0].load ((const float *)p);
                texel_simd[0][1].load ((const float *)(p+pixelsize));
                p += pixelsize * spec.tile_width;
                texel_simd[1][0].load ((const float *)p);
                texel_simd[1][1].load ((const float *)(p+pixelsize));
            }
        } else {
            bool noreusetile = (options.swrap == TextureOpt::WrapMirror);
            simd::vint4 tile_st = (sttex - xy) % tilewh;
            simd::vint4 tile_edge = sttex - tile_st;
            for (int j = 0;  j < 2;  ++j) {
                if (! stvalid[T0+j]) {
                    texel_simd[j][0].clear();
                    texel_simd[j][1].clear();
                    continue;
                }
                int tile_t = tile_st[T0+j];
                for (int i = 0;  i < 2;  ++i) {
                    if (! stvalid[S0+i]) {
                        texel_simd[j][i].clear();
                        continue;
                    }
                    int tile_s = tile_st[S0+i];
                    // Trick: we only need to find a tile if i == 0 or if we
                    // just crossed a tile bouncary (if tile_s == 0).
                    // Otherwise, we are still on the same tile as the last
                    // iteration, as long as we aren't using mirror wrap mode!
                    if (i == 0 || tile_s == 0 || noreusetile) {
                        id.xy (tile_edge[S0+i], tile_edge[T0+j]);
                        bool ok = find_tile (id, thread_info);
                        if (! ok)
                            error ("%s", m_imagecache->geterror());
                        if (! thread_info->tile->valid()) {
                            return false;
                        }
                        DASSERT (thread_info->tile->id() == id);
                    }
                    TileRef &tile (thread_info->tile);
                    int pixelsize = tile->pixelsize();
                    int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
                    offset += (firstchannel - id.chbegin()) * channelsize;
                    DASSERT (offset < spec.tile_width*spec.tile_height*spec.tile_depth*pixelsize);
                    if (pixeltype == TypeDesc::UINT8)
                        texel_simd[j][i] = uchar2float4 ((const unsigned char *)(tile->bytedata() + offset));
                    else if (pixeltype == TypeDesc::UINT16)
                        texel_simd[j][i] = ushort2float4 ((const unsigned short *)(tile->bytedata() + offset));
                    else if (pixeltype == TypeDesc::HALF)
                        texel_simd[j][i] = half2float4 ((const half *)(tile->bytedata() + offset));
                    else {
                        DASSERT (pixeltype == TypeDesc::FLOAT);
                        texel_simd[j][i].load ((const float *)(tile->bytedata() + offset));
                    }
                }
            }
        }
    
        // When we're on the lowest res mipmap levels, it's more pleasing if
        // we converge to a single pole color right at the pole.  Fade to
        // the average color over the texel height right next to the pole.
        if (need_pole) {
            float height = spec.height;
            if (texturefile.m_sample_border)
                height -= 1.0f;
            float tt = t_[sample] * height;
            if (tt < 1.0f || tt > (height-1.0f))
                fade_to_pole (tt, (float *)&accum, weight, texturefile, thread_info,
                              levelinfo, options, miplevel, actualchannels);
        }
    
        simd::vfloat4 weight_simd = weight;
        accum += weight_simd * bilerp(texel_simd[0][0], texel_simd[0][1],
                                       texel_simd[1][0], texel_simd[1][1],
                                       sfrac, tfrac);
        if (daccumds_) {
            simd::vfloat4 scalex = weight_simd * float(spec.width);
            simd::vfloat4 scaley = weight_simd * float(spec.height);
            daccumds += scalex * lerp (texel_simd[0][1] - texel_simd[0][0],
                                       texel_simd[1][1] - texel_simd[1][0],
                                       tfrac);
            daccumdt += scaley * lerp (texel_simd[1][0] - texel_simd[0][0],
                                       texel_simd[1][1] - texel_simd[0][1],
                                       sfrac);
        }
        if (use_fill && ! all (stvalid)) {
            // Compute appropriate amount of "fill" color to extra channels in
            // non-"black"-wrapped regions.
            float f = bilerp (float(stvalid[S0]*stvalid[T0]), float(stvalid[S1]*stvalid[T0]),
                              float(stvalid[S0]*stvalid[T1]), float(stvalid[S1]*stvalid[T1]),
                              sfrac, tfrac);
            nonfill += (1.0f - f) * weight;
        }
    }

    simd::vbool4 channel_mask = channel_masks[actualchannels];
    accum = blend0(accum, channel_mask);
    if (use_fill) {
        // Add the weighted fill color
        accum += blend0not(vfloat4((1.0f - nonfill) * options.fill), channel_mask);
    }

    *accum_ = accum;
    if (daccumds_) {
        *daccumds_ = blend0(daccumds, channel_mask);
        *daccumdt_ = blend0(daccumdt, channel_mask);
    }
    return true;
}


namespace {

// Evaluate Bspline weights for both value and derivatives (if dw is not
// NULL) into w[0..3] and dw[0..3]. This is the canonical version for
// reference, but we don't actually call it, instead favoring the much
// harder to read SIMD versions below.
template <typename T>
inline void evalBSplineWeights_and_derivs (T *w, T fraction,
                                           T *dw = NULL)
{
    T one_frac = 1.0 - fraction;
    w[0] = T(1.0 / 6.0) * one_frac * one_frac * one_frac;
    w[1] = T(2.0 / 3.0) - T(0.5) * fraction * fraction * (T(2.0) - fraction);
    w[2] = T(2.0 / 3.0) - T(0.5) * one_frac * one_frac * (T(2.0) - one_frac);
    w[3] = T(1.0 / 6.0) * fraction * fraction * fraction;
    if (dw) {
        dw[0] = T(-0.5) * one_frac * one_frac;
        dw[1] = T( 0.5) * fraction * (T(3.0) * fraction - T(4.0));
        dw[2] = T(-0.5) * one_frac * (T(3.0) * one_frac - T(4.0));
        dw[3] = T( 0.5) * fraction * fraction;
    }
}

// Evaluate the 4 Bspline weights (no derivs), returing them as a vfloat4.
// The fraction also comes in as a vfloat4 (assuming the same value in all 4
// slots).
inline vfloat4 evalBSplineWeights (const vfloat4& fraction)
{
#if 0
    // Version that's easy to read and understand:
    float one_frac = 1.0f - fraction;
    vfloat4 w;
    w[0] = 0.0f          + (1.0f / 6.0f) * one_frac * one_frac * one_frac;
    w[1] = (2.0f / 3.0f) + (-0.5f)       * fraction * fraction * (2.0f - fraction);
    w[2] = (2.0f / 3.0f) + (-0.5f)       * one_frac * one_frac * (2.0f - one_frac);
    w[3] = 0.0f          + (1.0f / 6.0f) * fraction * fraction * fraction;
    return w;
#else
    // Not as clear, but fastest version I've been able to achieve:
    OIIO_SIMD_FLOAT4_CONST4 (A, 0.0f, 2.0f/3.0f, 2.0f/3.0f, 0.0f);
    OIIO_SIMD_FLOAT4_CONST4 (B, 1.0f / 6.0f, -0.5f, -0.5f, 1.0f / 6.0f);
    OIIO_SIMD_FLOAT4_CONST4 (om1m1o, 1.0f, -1.0f, -1.0f, 1.0f);
    OIIO_SIMD_FLOAT4_CONST4 (z22z, 0.0f, 2.0f, 2.0f, 0.0f);
    simd::vfloat4 one_frac = vfloat4::One() - fraction;
    simd::vfloat4 ofof = AxBxAyBy (one_frac, fraction); // 1-frac, frac, 1-frac, frac
    simd::vfloat4 C = (*(vfloat4*)&om1m1o) * ofof + (*(vfloat4*)&z22z);
    return (*(vfloat4*)&A) + (*(vfloat4*)&B) * ofof * ofof * C;
#endif
}

// Evaluate Bspline weights for both value and derivatives (if dw is not
// NULL), returning the 4 coefficients for each as vfloat4's.
inline void evalBSplineWeights_and_derivs (simd::vfloat4 *w, float fraction,
                                           simd::vfloat4 *dw = NULL)
{
#if 0
    // Version that's easy to read and understand:
    float one_frac = 1.0f - fraction;
    (*w)[0] = 0.0f          + (1.0f / 6.0f) * one_frac * one_frac * one_frac;
    (*w)[1] = (2.0f / 3.0f) + (-0.5f)       * fraction * fraction * (2.0f - fraction);
    (*w)[2] = (2.0f / 3.0f) + (-0.5f)       * one_frac * one_frac * (2.0f - one_frac);
    (*w)[3] = 0.0f          + (1.0f / 6.0f) * fraction * fraction * fraction;
    if (dw) {
        (*dw)[0] = -0.5f * one_frac * (1.0f * one_frac - 0.0f);
        (*dw)[1] =  0.5f * fraction * (3.0f * fraction - 4.0f);
        (*dw)[2] = -0.5f * one_frac * (3.0f * one_frac - 4.0f);
        (*dw)[3] =  0.5f * fraction * (1.0f * fraction - 0.0f);
    }
#else
    // Not as clear, but fastest version I've been able to achieve:
    OIIO_SIMD_FLOAT4_CONST4 (A, 0.0f, 2.0f/3.0f, 2.0f/3.0f, 0.0f);
    OIIO_SIMD_FLOAT4_CONST4 (B, 1.0f / 6.0f, -0.5f, -0.5f, 1.0f / 6.0f);
    float one_frac = 1.0f - fraction;
    simd::vfloat4 ofof (one_frac, fraction, one_frac, fraction);
    simd::vfloat4 C (one_frac, 2.0f-fraction, 2.0f-one_frac, fraction);
    *w = (*(vfloat4*)&A) + (*(vfloat4*)&B) * ofof * ofof * C;
    if (dw) {
        const simd::vfloat4 D (-0.5f, 0.5f, -0.5f, 0.5f);
        const simd::vfloat4 E (1.0f, 3.0f, 3.0f, 1.0f);
        const simd::vfloat4 F (0.0f, 4.0f, 4.0f, 0.0f);
        *dw = D * ofof * (E * ofof - F);
    }
#endif
}

} // anonymous namespace



bool
TextureSystemImpl::sample_bicubic (int nsamples, const float *s_,
                                   const float *t_, int miplevel,
                                   TextureFile &texturefile,
                                   PerThreadInfo *thread_info,
                                   TextureOpt &options,
                                   int nchannels_result, int actualchannels,
                                   const float *weight_,
                                   vfloat4 *accum_, vfloat4 *daccumds_, vfloat4 *daccumdt_)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(options.subimage);
    wrap_impl_simd swrap_func_simd = wrap_functions_simd[(int)options.swrap];
    wrap_impl_simd twrap_func_simd = wrap_functions_simd[(int)options.twrap];

    vint4 spec_x_simd (spec.x);
    vint4 spec_y_simd (spec.y);
    vint4 spec_width_simd (spec.width);
    vint4 spec_height_simd (spec.height);
    vint4 spec_x_plus_width_simd = spec_x_simd + spec_width_simd;
    vint4 spec_y_plus_height_simd = spec_y_simd + spec_height_simd;
    bool use_fill = (nchannels_result > actualchannels && options.fill);
    bool tilepow2 = ispow2(spec.tile_width) && ispow2(spec.tile_height);
    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    size_t channelsize = texturefile.channelsize(options.subimage);
    int firstchannel = options.firstchannel;
    float nonfill = 0.0f;  // The degree to which we DON'T need fill
    // N.B. What's up with "nofill"? We need to consider fill only when we
    // are inside the valid texture region. Outside, i.e. in the black wrap
    // region, black takes precedence over fill. By keeping track of when
    // we don't need to worry about fill, which is the comparitively rare
    // case, we do a lot less math and have fewer rounding errors.

    // need_pole: do we potentially need to fade to special pole color?
    // If we do, can't restrict channel range or fade_to_pole won't work.
    bool need_pole = (options.envlayout == LayoutLatLong && levelinfo.onetile);
    int tile_chbegin = 0, tile_chend = spec.nchannels;
    if (spec.nchannels > m_max_tile_channels) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = options.firstchannel;
        tile_chend = options.firstchannel+actualchannels;
    }
    TileID id (texturefile, options.subimage, miplevel, 0, 0, 0,
               tile_chbegin, tile_chend);
    size_t pixelsize = channelsize * id.nchannels();
    size_t firstchannel_offset_bytes = channelsize * (firstchannel - id.chbegin());
    vfloat4 accum, daccumds, daccumdt;
    accum.clear();
    if (daccumds_) {
        daccumds.clear();
        daccumdt.clear();
    }

    vfloat4 s_simd, t_simd;
    vint4 sint_simd, tint_simd;
    vfloat4 sfrac_simd, tfrac_simd;
    for (int sample = 0;  sample < nsamples;  ++sample) {
        // To utilize SIMD ops in an inherently scalar loop, every fourth
        // step, we compute the st_to_texel for the next four samples.
        int sample4 = sample & 3;
        if (sample4 == 0) {
            s_simd.load (s_ + sample);
            t_simd.load (t_ + sample);
            st_to_texel_simd (s_simd, t_simd, texturefile, spec,
                         sint_simd, tint_simd, sfrac_simd, tfrac_simd);
        }
        int sint = sint_simd[sample4], tint = tint_simd[sample4];
        float sfrac = sfrac_simd[sample4], tfrac = tfrac_simd[sample4];
        float weight = weight_[sample];
    
        // We're gathering 4x4 samples and 4x weights.  Indices: texels 0,
        // 1, 2, 3.  The sample lies between samples 1 and 2.
    
        static const OIIO_SIMD4_ALIGN int iota[4] = { 0, 1, 2, 3 };
        static const OIIO_SIMD4_ALIGN int iota_1[4] = { -1, 0, 1, 2 };
        simd::vint4 stex, ttex;       // Texel coords for each row and column
        stex = sint + (*(vint4 *)iota_1);
        ttex = tint + (*(vint4 *)iota_1);
        simd::vbool4 svalid = swrap_func_simd (stex, spec_x_simd, spec_width_simd);
        simd::vbool4 tvalid = twrap_func_simd (ttex, spec_y_simd, spec_height_simd);
        bool allvalid = reduce_and(svalid & tvalid);
        bool anyvalid = reduce_or (svalid | tvalid);
        if (! levelinfo.full_pixel_range && anyvalid) {
            // Handle case of crop windows or overscan
            svalid &= (stex >= spec_x_simd) & (stex < spec_x_plus_width_simd);
            tvalid &= (ttex >= spec_y_simd) & (ttex < spec_y_plus_height_simd);
            allvalid = reduce_and(svalid & tvalid);
            anyvalid = reduce_or (svalid | tvalid);
        }
        if (! anyvalid) {
            // All texels we need were out of range and using 'black' wrap.
            nonfill += weight;
            continue;
        }
    
        simd::vfloat4 texel_simd[4][4];
        // int tile_s = (stex[0] - spec.x) % spec.tile_width;
        // int tile_t = (ttex[0] - spec.y) % spec.tile_height;
        int tile_s = (stex[0] - spec.x);
        int tile_t = (ttex[0] - spec.y);
        if (tilepow2) {
            tile_s &= tilewidthmask;
            tile_t &= tileheightmask;
        } else {
            tile_s %= spec.tile_width;
            tile_t %= spec.tile_height;
        }
        bool s_onetile = (tile_s <= tilewidthmask-3);
        bool t_onetile = (tile_t <= tileheightmask-3);
        if (s_onetile & t_onetile) {
            // If we thought it was one tile, realize that it isn't unless
            // it's ascending.
            s_onetile &= all (stex == (simd::shuffle<0>(stex)+(*(vint4 *)iota)));
            t_onetile &= all (ttex == (simd::shuffle<0>(ttex)+(*(vint4 *)iota)));
        }
        bool onetile = (s_onetile & t_onetile);
        if (onetile & allvalid) {
            // Shortcut if all the texels we need are on the same tile
            id.xy (stex[0] - tile_s, ttex[0] - tile_t);
            bool ok = find_tile (id, thread_info);
            if (! ok)
                error ("%s", m_imagecache->geterror());
            TileRef &tile (thread_info->tile);
            if (! tile) {
                return false;
            }
            // N.B. thread_info->tile will keep holding a ref-counted pointer
            // to the tile for the duration that we're using the tile data.
            int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
            const unsigned char *base = tile->bytedata() + offset + firstchannel_offset_bytes;
            DASSERT (tile->data());
            if (pixeltype == TypeDesc::UINT8) {
                for (int j = 0, j_offset = 0;  j < 4;  ++j, j_offset += pixelsize*spec.tile_width)
                    for (int i = 0, i_offset = j_offset;  i < 4;  ++i, i_offset += pixelsize)
                        texel_simd[j][i] = uchar2float4 (base + i_offset);
            } else if (pixeltype == TypeDesc::UINT16) {
                for (int j = 0, j_offset = 0;  j < 4;  ++j, j_offset += pixelsize*spec.tile_width)
                    for (int i = 0, i_offset = j_offset;  i < 4;  ++i, i_offset += pixelsize)
                        texel_simd[j][i] = ushort2float4 ((const uint16_t *)(base + i_offset));
            } else if (pixeltype == TypeDesc::HALF) {
                for (int j = 0, j_offset = 0;  j < 4;  ++j, j_offset += pixelsize*spec.tile_width)
                    for (int i = 0, i_offset = j_offset;  i < 4;  ++i, i_offset += pixelsize)
                        texel_simd[j][i] = half2float4 ((const half *)(base + i_offset));
            } else {
                for (int j = 0, j_offset = 0;  j < 4;  ++j, j_offset += pixelsize*spec.tile_width)
                    for (int i = 0, i_offset = j_offset;  i < 4;  ++i, i_offset += pixelsize)
                        texel_simd[j][i].load ((const float *)(base + i_offset));
            }
        } else {
            simd::vint4 tile_s, tile_t;   // texel offset WITHIN its tile
            simd::vint4 tile_s_edge, tile_t_edge;  // coordinate of the tile edge
            tile_s = (stex - spec_x_simd) % spec.tile_width;
            tile_t = (ttex - spec_y_simd) % spec.tile_height;
            tile_s_edge = stex - tile_s;
            tile_t_edge = ttex - tile_t;
            simd::vint4 column_offset_bytes = tile_s * pixelsize + firstchannel_offset_bytes;
            for (int j = 0;  j < 4;  ++j) {
                if (! tvalid[j]) {
                    for (int i = 0;  i < 4;  ++i)
                        texel_simd[j][i].clear();
                    continue;
                }
                int row_offset_bytes = tile_t[j] * (spec.tile_width * pixelsize);
                for (int i = 0;  i < 4;  ++i) {
                    if (! svalid[i]) {
                        texel_simd[j][i].clear();
                        continue;
                    }
                    // Trick: we only need to find a tile if i == 0 or if we
                    // just crossed a tile boundary (if tile_s[i] == 0).
                    // Otherwise, we are still on the same tile as the last
                    // iteration, as long as we aren't using mirror wrap mode!
                    if (i == 0 || tile_s[i] == 0 || options.swrap == TextureOpt::WrapMirror) {
                        id.xy (tile_s_edge[i], tile_t_edge[j]);
                        bool ok = find_tile (id, thread_info);
                        if (! ok)
                            error ("%s", m_imagecache->geterror());
                        DASSERT (thread_info->tile->id() == id);
                        if (! thread_info->tile->valid())
                            return false;
                    }
                    TileRef &tile (thread_info->tile);
                    DASSERT (tile->data());
                    int offset = row_offset_bytes + column_offset_bytes[i];
                    // const unsigned char *pixelptr = tile->bytedata() + offset[i];
                    if (pixeltype == TypeDesc::UINT8)
                        texel_simd[j][i] = uchar2float4 (tile->bytedata() + offset);
                    else if (pixeltype == TypeDesc::UINT16)
                        texel_simd[j][i] = ushort2float4 ((const uint16_t *)(tile->bytedata() + offset));
                    else if (pixeltype == TypeDesc::HALF)
                        texel_simd[j][i] = half2float4 ((const half *)(tile->bytedata() + offset));
                    else
                        texel_simd[j][i].load ((const float *)(tile->bytedata() + offset));
                }
            }
        }
    
        // When we're on the lowest res mipmap levels, it's more pleasing if
        // we converge to a single pole color right at the pole.  Fade to
        // the average color over the texel height right next to the pole.
        if (need_pole) {
            float height = spec.height;
            if (texturefile.m_sample_border)
                height -= 1.0f;
            float tt = t_[sample] * height;
            if (tt < 1.0f || tt > (height-1.0f))
                fade_to_pole (tt, (float *)&accum, weight, texturefile, thread_info,
                              levelinfo, options, miplevel, actualchannels);
        }
    
        // We use a formulation of cubic B-spline evaluation that reduces to
        // lerps.  It's tricky to follow, but the references are:
        //   * Ruijters, Daniel et al, "Efficient GPU-Based Texture
        //     Interpolation using Uniform B-Splines", Journal of Graphics
        //     Tools 13(4), pp. 61-69, 2008.
        //     http://jgt.akpeters.com/papers/RuijtersEtAl08/
        //   * Sigg, Christian and Markus Hadwiger, "Fast Third-Order Texture 
        //     Filtering", in GPU Gems 2 (Chapter 20), Pharr and Fernando, ed.
        //     http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter20.html
        // We like this formulation because it's slightly faster than any of
        // the other B-spline evaluation routines we tried, and also the lerp
        // guarantees that the filtered results will be non-negative for
        // non-negative texel values (which we had trouble with before due to
        // numerical imprecision).
        vfloat4 wx, dwx;
        vfloat4 wy, dwy;
        if (daccumds_) {
            evalBSplineWeights_and_derivs (&wx, sfrac, &dwx);
            evalBSplineWeights_and_derivs (&wy, tfrac, &dwy);
        } else {
            wx = evalBSplineWeights (vfloat4(sfrac));
            wy = evalBSplineWeights (vfloat4(tfrac));
#if (defined(__i386__) && !defined(__x86_64__)) || defined(__aarch64__)
            // Some platforms complain here about these being uninitialized,
            // so initialize them. Don't waste the cycles for platforms that
            // don't seem to have that error. It's a false positive -- this
            // code really is safe without the initialization, but that
            // doesn't seem to get figured out on some platforms (even when
            // running the same gcc version).
            dwx = vfloat4::Zero();
            dwy = vfloat4::Zero();
#endif
        }

        // figure out lerp weights so we can turn the filter into a sequence of lerp's
        // Here's the obvious scalar code:
        //   float g0x = wx[0] + wx[1]; float h0x = (wx[1] / g0x);
        //   float g1x = wx[2] + wx[3]; float h1x = (wx[3] / g1x);
        //   float g0y = wy[0] + wy[1]; float h0y = (wy[1] / g0y);
        //   float g1y = wy[2] + wy[3]; float h1y = (wy[3] / g1y);
        // But instead, we convolutedly (but quickly!) compute the four g
        // and four h values using SIMD ops:
        vfloat4 wx_0213 = simd::shuffle<0,2,1,3>(wx);
        vfloat4 wx_1302 = simd::shuffle<1,3,0,2>(wx);
        vfloat4 wx_01_23_01_23 = wx_0213 + wx_1302;  // wx[0]+wx[1] wx[2]+wx[3] ..
        vfloat4 wy_0213 = simd::shuffle<0,2,1,3>(wy);
        vfloat4 wy_1302 = simd::shuffle<1,3,0,2>(wy);
        vfloat4 wy_01_23_01_23 = wy_0213 + wy_1302;  // wy[0]+wy[1] wy[2]+wy[3] ..
        vfloat4 g = AxyBxy (wx_01_23_01_23, wy_01_23_01_23); // wx[0]+wx[1] wx[2]+wx[3] wy[0]+wy[1] wy[2]+wy[3]
        vfloat4 wx13_wy13 = AxyBxy (wx_1302, wy_1302);
        vfloat4 h = wx13_wy13 / g;  // [ h0x h1x h0y h1y ]
    
        simd::vfloat4 col[4];
        for (int j = 0;  j < 4; ++j) {
            simd::vfloat4 lx = lerp (texel_simd[j][0], texel_simd[j][1], shuffle<0>(h) /*h0x*/);
            simd::vfloat4 rx = lerp (texel_simd[j][2], texel_simd[j][3], shuffle<1>(h) /*h1x*/);
            col[j] = lerp (lx, rx, shuffle<1>(g) /*g1x*/);
        }
        simd::vfloat4 ly = lerp (col[0], col[1], shuffle<2>(h) /*h0y*/);
        simd::vfloat4 ry = lerp (col[2], col[3], shuffle<3>(h) /*h1y*/);
        simd::vfloat4 weight_simd = weight;
        accum += weight_simd * lerp (ly, ry, shuffle<3>(g) /*g1y*/);
        if (daccumds_) {
            simd::vfloat4 scalex = weight_simd * float(spec.width);
            simd::vfloat4 scaley = weight_simd * float(spec.height);
            daccumds += scalex * (
                dwx[0] * (wy[0] * texel_simd[0][0] + wy[1] * texel_simd[1][0] +
                          wy[2] * texel_simd[2][0] + wy[3] * texel_simd[3][0]) +
                dwx[1] * (wy[0] * texel_simd[0][1] + wy[1] * texel_simd[1][1] +
                          wy[2] * texel_simd[2][1] + wy[3] * texel_simd[3][1]) +
                dwx[2] * (wy[0] * texel_simd[0][2] + wy[1] * texel_simd[1][2] +
                          wy[2] * texel_simd[2][2] + wy[3] * texel_simd[3][2]) +
                dwx[3] * (wy[0] * texel_simd[0][3] + wy[1] * texel_simd[1][3] +
                          wy[2] * texel_simd[2][3] + wy[3] * texel_simd[3][3])
            );
            daccumdt += scaley * (
                dwy[0] * (wx[0] * texel_simd[0][0] + wx[1] * texel_simd[0][1] +
                          wx[2] * texel_simd[0][2] + wx[3] * texel_simd[0][3]) +
                dwy[1] * (wx[0] * texel_simd[1][0] + wx[1] * texel_simd[1][1] +
                          wx[2] * texel_simd[1][2] + wx[3] * texel_simd[1][3]) +
                dwy[2] * (wx[0] * texel_simd[2][0] + wx[1] * texel_simd[2][1] +
                          wx[2] * texel_simd[2][2] + wx[3] * texel_simd[2][3]) +
                dwy[3] * (wx[0] * texel_simd[3][0] + wx[1] * texel_simd[3][1] +
                          wx[2] * texel_simd[3][2] + wx[3] * texel_simd[3][3])
            );
        }
    
        // Compute appropriate amount of "fill" color to extra channels in
        // non-"black"-wrapped regions.
        if (!allvalid && use_fill) {
            float col[4];
            for (int j = 0;  j < 4; ++j) {
                float lx = lerp (1.0f*tvalid[j]*svalid[0], 1.0f*tvalid[j]*svalid[1], extract<0>(h) /*h0x*/);
                float rx = lerp (1.0f*tvalid[j]*svalid[2], 1.0f*tvalid[j]*svalid[3], extract<1>(h) /*h1x*/);
                col[j]   = lerp (lx, rx, extract<1>(g) /*g1x*/);
            }
            float ly = lerp (col[0], col[1], extract<2>(h) /*h0y*/);
            float ry = lerp (col[2], col[3], extract<3>(h) /*h1y*/);
            nonfill += weight * (1.0f - lerp (ly, ry, extract<3>(g) /*g1y*/));
        }
    }

    simd::vbool4 channel_mask = channel_masks[actualchannels];
    accum = blend0(accum, channel_mask);
    if (use_fill) {
        // Add the weighted fill color
        accum += blend0not(vfloat4((1.0f - nonfill) * options.fill), channel_mask);
    }

    *accum_ = accum;
    if (daccumds_) {
        *daccumds_ = blend0(daccumds, channel_mask);
        *daccumdt_ = blend0(daccumdt, channel_mask);
     }
    return true;
}



void
TextureSystemImpl::visualize_ellipse (const std::string &name,
                                      float dsdx, float dtdx,
                                      float dsdy, float dtdy,
                                      float sblur, float tblur)
{
    std::cout << name << " derivs dx " << dsdx << ' ' << dtdx << ", dt " << dtdx << ' ' << dtdy << "\n";
    adjust_width (dsdx, dtdx, dsdy, dtdy, 1.0f, 1.0f);
    float majorlength, minorlength, theta;
    float ABCF[4];
    ellipse_axes (dsdx, dtdx, dsdy, dtdy, majorlength, minorlength, theta, ABCF);
    std::cout << "  ellipse major " << majorlength << ", minor " 
              << minorlength << ", theta " << theta << "\n";
    adjust_blur (majorlength, minorlength, theta, sblur, tblur);
    std::cout << "  post " << sblur << ' ' << tblur << " blur: major " 
              << majorlength << ", minor " << minorlength << "\n\n";

    TextureOpt options;
    float trueaspect;
    float aspect = TextureSystemImpl::anisotropic_aspect (majorlength, minorlength, options, trueaspect);
    float *lineweight = ALLOCA (float, 2*options.anisotropic);
    float smajor, tmajor, invsamples;
    int nsamples = compute_ellipse_sampling (aspect, theta, majorlength,
                                             minorlength, smajor, tmajor,
                                             invsamples, lineweight);

    // Make an ImageBuf to hold our visualization image, set it to grey
    float scale = 100;
    int w = 256, h = 256;
    ImageSpec spec (w, h, 3);
    ImageBuf ib (spec);
    static float dark[3] = { 0.2f, 0.2f, 0.2f };
    static float white[3] = { 1, 1, 1 };
    static float grey[3] = { 0.5, 0.5, 0.5 };
    static float red[3] = { 1, 0, 0 };
    static float green[3] = { 0, 1, 0 };
    ImageBufAlgo::fill (ib, grey);

    // scan all the pixels, darken the ellipse interior
    for (int j = 0;  j < h;  ++j) {
        float y = (j-h/2)/scale;
        for (int i = 0;  i < w;  ++i) {
            float x = (i-w/2)/scale;
            float d2 = ABCF[0]*x*x + ABCF[1]*x*y + ABCF[2]*y*y;
            if (d2 < 1.0f)
                ib.setpixel (i, h-1-j, dark);
        }
    }

    // Draw red and green axes for the dx and dy derivatives, respectively
    for (int i = 0, e = std::max(fabsf(dsdx),fabsf(dtdx))*scale;  i < e;  ++i)
        ib.setpixel (w/2+int(float(i)/e*dsdx*scale), h/2-int(float(i)/e*dtdx*scale), red);
    for (int i = 0, e = std::max(fabsf(dsdy),fabsf(dtdy))*scale;  i < e;  ++i)
        ib.setpixel (w/2+int(float(i)/e*dsdy*scale), h/2-int(float(i)/e*dtdy*scale), green);

    float bigweight = 0;
    for (int i = 0;  i < nsamples;  ++i)
        bigweight = std::max(lineweight[i],bigweight);

    // Plop white dots at the sample positions
    for (int sample = 0;  sample < nsamples;  ++sample) {
        float pos = 1.0f * (sample + 0.5f) * invsamples - 0.5f;
        float x = pos*smajor, y = pos*tmajor;
        int xx = w/2+int(x*scale), yy = h/2-int(y*scale);
        int size = int (5 * lineweight[sample]/bigweight);
        ImageBufAlgo::fill (ib, white, ROI(xx-size/2, xx+size/2+1, 
                                           yy-size/2, yy+size/2+1));
    }

    ib.write (name);
}



void
TextureSystemImpl::unit_test_texture ()
{
    float blur = 0;
    float dsdx, dtdx, dsdy, dtdy;

    dsdx = 0.4; dtdx = 0.0; dsdy = 0.0; dtdy = 0.2;
    visualize_ellipse ("0.tif", dsdx, dtdx, dsdy, dtdy, blur, blur);

    dsdx = 0.2; dtdx = 0.0; dsdy = 0.0; dtdy = 0.4;
    visualize_ellipse ("1.tif", dsdx, dtdx, dsdy, dtdy, blur, blur);

    dsdx = 0.2; dtdx = 0.2; dsdy = -0.2; dtdy = 0.2;
    visualize_ellipse ("2.tif", dsdx, dtdx, dsdy, dtdy, blur, blur);

    dsdx = 0.35; dtdx = 0.27; dsdy = 0.1; dtdy = 0.35;
    visualize_ellipse ("3.tif", dsdx, dtdx, dsdy, dtdy, blur, blur);

    dsdx = 0.35; dtdx = 0.27; dsdy = 0.1; dtdy = -0.35;
    visualize_ellipse ("4.tif", dsdx, dtdx, dsdy, dtdy, blur, blur);

    boost::mt19937 rndgen;
    boost::uniform_01<boost::mt19937, float> rnd(rndgen);
    for (int i = 0;  i < 100;  ++i) {
        dsdx = 1.5f*(rnd() - 0.5f);
        dtdx = 1.5f*(rnd() - 0.5f);
        dsdy = 1.5f*(rnd() - 0.5f);
        dtdy = 1.5f*(rnd() - 0.5f);
        visualize_ellipse (Strutil::format("%d.tif", 100+i),
                           dsdx, dtdx, dsdy, dtdy, blur, blur);
    }
}



}  // end namespace pvt

OIIO_NAMESPACE_END
