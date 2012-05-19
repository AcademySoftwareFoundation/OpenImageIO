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


#include <math.h>
#include <string>
#include <sstream>
#include <list>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <OpenEXR/ImathMatrix.h>

#include "dassert.h"
#include "typedesc.h"
#include "varyingref.h"
#include "ustring.h"
#include "strutil.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
#include "filter.h"
#include "optparser.h"
#include "imageio.h"

#include "texture.h"

#include "imagecache.h"
#include "imagecache_pvt.h"
#include "texture_pvt.h"

OIIO_NAMESPACE_ENTER
{
    using namespace pvt;


namespace {  // anonymous

static shared_ptr<TextureSystemImpl> shared_texsys;
static mutex shared_texsys_mutex;
static EightBitConverter<float> uchar2float;

};  // end anonymous namespace


TextureSystem *
TextureSystem::create (bool shared)
{
    ImageCache *ic = ImageCache::create (shared);
#if 0
    if (shared) {
        // They requested a shared texsys.  If a shared texsys already
        // exists, just return it, otherwise record the new cache.
        lock_guard guard (shared_texsys_mutex);
        if (! shared_texsys.get())
            shared_texsys.reset (new TextureSystemImpl (ic));
#ifdef DEBUG
        std::cerr << " shared TextureSystem is "
                  << (void *)shared_texsys.get() << "\n";
#endif
        return shared_texsys.get ();
    }
#endif
    return new TextureSystemImpl (ic);
}



void
TextureSystem::destroy (TextureSystem *x)
{
    // Delete only if it's a private one
//    lock_guard guard (shared_texsys_mutex);
//    if (x != shared_texsys.get())
        delete (TextureSystemImpl *) x;
}



namespace pvt {   // namespace pvt



// Wrap functions wrap 'coord' around 'width', and return true if the
// result is a valid pixel coordinate, false if black should be used
// instead.


bool
TextureSystemImpl::wrap_black (int &coord, int origin, int width)
{
    return (coord >= origin && coord < (width+origin));
}


bool
TextureSystemImpl::wrap_clamp (int &coord, int origin, int width)
{
    if (coord < origin)
        coord = origin;
    else if (coord >= origin+width)
        coord = origin+width-1;
    return true;
}


bool
TextureSystemImpl::wrap_periodic (int &coord, int origin, int width)
{
    coord -= origin;
    coord %= width;
    if (coord < 0)       // Fix negative values
        coord += width;
    coord += origin;
    return true;
}


bool
TextureSystemImpl::wrap_periodic2 (int &coord, int origin, int width)
{
    coord -= origin;
    coord &= (width - 1); // Shortcut periodic if we're sure it's a pow of 2
    coord += origin;
    return true;
}


bool
TextureSystemImpl::wrap_periodic_sharedborder (int &coord, int origin, int width)
{
    // Like periodic, but knowing that the first column and last are
    // actually the same position, so we essentially skip the first
    // column in the next cycle.  We only need this to work for one wrap
    // in each direction since it's only used for latlong maps.
    coord -= origin;
    if (coord >= width) {
        coord = coord - width + 1;
    } else if (coord < 0) {
        coord = coord + width - 1;
    }
    coord += origin;
    return true;
}


bool
TextureSystemImpl::wrap_mirror (int &coord, int origin, int width)
{
    coord -= origin;
    bool negative = (coord < 0);
    int iter = coord / width;    // Which iteration of the pattern?
    coord -= iter * width;
    bool flip = (iter & 1);
    if (negative) {
        coord += width - 1;
        flip = !flip;
    }
    if (flip)
        coord = width - 1 - coord;
    DASSERT (coord >= 0 && coord < width);
    coord += origin;
    return true;
}



const TextureSystemImpl::wrap_impl TextureSystemImpl::wrap_functions[] = {
    // Must be in same order as Wrap enum
    wrap_black, wrap_black, wrap_clamp, wrap_periodic, wrap_mirror
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
    delete hq_filter;
    hq_filter = Filter1D::create ("b-spline", 4);
    m_statslevel = 0;

    // Allow environment variable to override default options
    const char *options = getenv ("OPENIMAGEIO_TEXTURE_OPTIONS");
    if (options)
        attribute ("options", options);
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
    bool anytexture = (stats.texture_queries + stats.texture3d_queries +
                       stats.shadow_queries + stats.environment_queries);
    if (level > 0 && anytexture) {
        out << "OpenImageIO Texture statistics (" << (void*)this
            << ", cache = " << (void *)m_imagecache << ")\n";
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
            out << Strutil::format ("  Average anisotropy : %.3g\n",
                                    (double)stats.aniso_probes/(double)stats.aniso_queries);
        else
            out << Strutil::format ("  Average anisotropy : 0\n");
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
TextureSystemImpl::attribute (const std::string &name, TypeDesc type,
                              const void *val)
{
    if (name == "options" && type == TypeDesc::STRING) {
        return optparser (*this, *(const char **)val);
    }
    if (name == "worldtocommon" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        m_Mw2c = *(const Imath::M44f *)val;
        m_Mc2w = m_Mw2c.inverse();
        return true;
    }
    if (name == "commontoworld" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        m_Mc2w = *(const Imath::M44f *)val;
        m_Mw2c = m_Mc2w.inverse();
        return true;
    }
    if ((name == "gray_to_rgb" || name == "grey_to_rgb") &&
        (type == TypeDesc::TypeInt)) {
        m_gray_to_rgb = *(const int *)val;
        return true;
    }
    if (name == "statistics:level" && type == TypeDesc::TypeInt) {
        m_statslevel = *(const int *)val;
        // DO NOT RETURN! pass the same message to the image cache
    }

    // Maybe it's meant for the cache?
    return m_imagecache->attribute (name, type, val);
}



bool
TextureSystemImpl::getattribute (const std::string &name, TypeDesc type,
                                 void *val)
{
    if (name == "worldtocommon" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mw2c;
        return true;
    }
    if (name == "commontoworld" && (type == TypeDesc::TypeMatrix ||
                                    type == TypeDesc(TypeDesc::FLOAT,16))) {
        *(Imath::M44f *)val = m_Mc2w;
        return true;
    }
    if ((name == "gray_to_rgb" || name == "grey_to_rgb") &&
        (type == TypeDesc::TypeInt)) {
        *(int *)val = m_gray_to_rgb;
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
    if (! ok)
        error ("%s", m_imagecache->geterror().c_str());
    return ok;
}



bool
TextureSystemImpl::get_imagespec (ustring filename, int subimage,
                                  ImageSpec &spec)
{
    bool ok = m_imagecache->get_imagespec (filename, spec, subimage);
    if (! ok)
        error ("%s", m_imagecache->geterror().c_str());
    return ok;
}



const ImageSpec *
TextureSystemImpl::imagespec (ustring filename, int subimage)
{
    const ImageSpec *spec = m_imagecache->imagespec (filename, subimage);
    if (! spec)
        error ("%s", m_imagecache->geterror().c_str());
    return spec;
}



bool
TextureSystemImpl::get_texels (ustring filename, TextureOpt &options,
                               int miplevel, int xbegin, int xend,
                               int ybegin, int yend, int zbegin, int zend,
                               TypeDesc format, void *result)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texfile = find_texturefile (filename, thread_info);
    if (! texfile) {
        error ("Texture file \"%s\" not found", filename.c_str());
        return false;
    }
    if (texfile->broken()) {
        error ("Invalid texture file \"%s\"", filename.c_str());
        return false;
    }
    int subimage = options.subimage;
    if (subimage < 0 || subimage >= texfile->subimages()) {
        error ("get_texel asked for nonexistant subimage %d of \"%s\"",
               subimage, filename.c_str());
        return false;
    }
    if (miplevel < 0 || miplevel >= texfile->miplevels(subimage)) {
        error ("get_texel asked for nonexistant MIP level %d of \"%s\"",
               miplevel, filename.c_str());
        return false;
    }
    const ImageSpec &spec (texfile->spec(subimage, miplevel));

    // FIXME -- this could be WAY more efficient than starting from
    // scratch for each pixel within the rectangle.  Instead, we should
    // grab a whole tile at a time and memcpy it rapidly.  But no point
    // doing anything more complicated (not to mention bug-prone) until
    // somebody reports this routine as being a bottleneck.
    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel, 0, options.nchannels);
    int nc = spec.nchannels;
    size_t formatpixelsize = nc * format.size();
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
        int tz = z - ((z - spec.z) % std::max (1, spec.tile_depth));
        for (int y = ybegin;  y < yend;  ++y) {
            if (y < spec.y || y >= (spec.y+spec.height)) {
                // nonexistant scanlines
                memset (result, 0, scanlinesize);
                result = (void *) ((char *) result + scanlinesize);
                continue;
            }
            int ty = y - ((y - spec.y) % spec.tile_height);
            for (int x = xbegin;  x < xend;  ++x) {
                if (x < spec.x || x >= (spec.x+spec.width)) {
                    // nonexistant columns
                    memset (result, 0, formatpixelsize);
                    result = (void *) ((char *) result + formatpixelsize);
                    continue;
                }
                int tx = x - ((x - spec.x) % spec.tile_width);
                TileID tileid (*texfile, subimage, miplevel, tx, ty, tz);
                ok &= find_tile (tileid, thread_info);
                TileRef &tile (thread_info->tile);
                const char *data;
                if (tile && (data = (const char *)tile->data (x, y, z))) {
                    data += options.firstchannel * texfile->datatype().size();
                    convert_types (texfile->datatype(), data,
                                   format, result, actualchannels);
                    for (int c = actualchannels;  c < options.nchannels;  ++c)
                        convert_types (TypeDesc::FLOAT, &options.fill,
                                       format, result, 1);
                } else {
                    memset (result, 0, formatpixelsize);
                }
                result = (void *) ((char *) result + formatpixelsize);
            }
        }
    }
    if (! ok)
        error ("%s", m_imagecache->geterror().c_str());
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
TextureSystemImpl::missing_texture (TextureOpt &options, float *result)
{
    for (int c = 0;  c < options.nchannels;  ++c) {
        if (options.missingcolor)
            result[c] = options.missingcolor[c];
        else
            result[c] = options.fill;
        if (options.dresultds) options.dresultds[c] = 0;
        if (options.dresultdt) options.dresultdt[c] = 0;
        if (options.dresultdr) options.dresultdr[c] = 0;
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
TextureSystemImpl::fill_channels (int nfilechannels, TextureOpt &options,
                                  float *result)
{
    // Starting channel to deal with is the first beyond what we actually
    // got from the texture lookup.
    int c = options.actualchannels;

    // Special case: multi-channel texture reads from single channel
    // files propagate their grayscale value to G and B.
    if (nfilechannels == 1 && m_gray_to_rgb && 
            options.firstchannel == 0 && options.nchannels >= 3) {
        result[1] = result[0];
        result[2] = result[0];
        if (options.dresultds) {
            options.dresultds[1] = options.dresultds[0];
            options.dresultds[2] = options.dresultds[0];
        }
        if (options.dresultdt) {
            options.dresultdt[1] = options.dresultdt[0];
            options.dresultdt[2] = options.dresultdt[0];
        }
        if (options.dresultdr) {
            options.dresultdr[1] = options.dresultdr[0];
            options.dresultdr[2] = options.dresultdr[0];
        }
        c = 3; // we're done with channels 0-2, work on 3 next
    }

    // Fill in the remaining files with the fill color
    for ( ; c < options.nchannels; ++c) {
        result[c] = options.fill;
        if (options.dresultds) options.dresultds[c] = 0;
        if (options.dresultdt) options.dresultdt[c] = 0;
        if (options.dresultdr) options.dresultdr[c] = 0;
    }
}



bool
TextureSystemImpl::texture (ustring filename, TextureOpt &options,
                            float s, float t,
                            float dsdx, float dtdx, float dsdy, float dtdy,
                            float *result)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texturefile = find_texturefile (filename, thread_info);
    return texture ((TextureHandle *)texturefile, (Perthread *)thread_info,
                    options, s, t, dsdx, dtdx, dsdy, dtdy, result);
}



bool
TextureSystemImpl::texture (TextureHandle *texture_handle_,
                            Perthread *thread_info_,
                            TextureOpt &options,
                            float s, float t,
                            float dsdx, float dtdx, float dsdy, float dtdy,
                            float *result)
{
    static const texture_lookup_prototype lookup_functions[] = {
        // Must be in the same order as Mipmode enum
        &TextureSystemImpl::texture_lookup,
        &TextureSystemImpl::texture_lookup_nomip,
        &TextureSystemImpl::texture_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture_lookup
    };
    texture_lookup_prototype lookup = lookup_functions[(int)options.mipmode];

    PerThreadInfo *thread_info = (PerThreadInfo *)thread_info_;
    TextureFile *texturefile = (TextureFile *)texture_handle_;
    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.texture_batches;
    ++stats.texture_queries;

    if (! texturefile  ||  texturefile->broken())
        return missing_texture (options, result);

    if (options.subimagename) {
        // If subimage was specified by name, figure out its index.
        int s = m_imagecache->subimage_from_name (texturefile, options.subimagename);
        if (s < 0) {
            error ("Unknown subimage \"%s\" in texture \"%s\"",
                   options.subimagename.c_str(), texturefile->filename().c_str());
            return false;
        }
        options.subimage = s;
        options.subimagename.clear();
    }

    const ImageCacheFile::SubimageInfo &subinfo (texturefile->subimageinfo(options.subimage));
    const ImageSpec &spec (texturefile->spec(options.subimage, 0));

    if (! subinfo.full_pixel_range) {  // remap st for overscan or crop
        s = s * subinfo.sscale + subinfo.soffset;
        dsdx *= subinfo.sscale;
        dsdy *= subinfo.sscale;
        t = t * subinfo.tscale + subinfo.toffset;
        dtdx *= subinfo.tscale;
        dtdy *= subinfo.tscale;
    }

    // Figure out the wrap functions
    if (options.swrap == TextureOpt::WrapDefault)
        options.swrap = (TextureOpt::Wrap)texturefile->swrap();
    if (options.swrap == TextureOpt::WrapPeriodic && ispow2(spec.width))
        options.swrap_func = wrap_periodic2;
    else
        options.swrap_func = wrap_functions[(int)options.swrap];
    if (options.twrap == TextureOpt::WrapDefault)
        options.twrap = (TextureOpt::Wrap)texturefile->twrap();
    if (options.twrap == TextureOpt::WrapPeriodic && ispow2(spec.height))
        options.twrap_func = wrap_periodic2;
    else
        options.twrap_func = wrap_functions[(int)options.twrap];

    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel, 0, options.nchannels);
    options.actualchannels = actualchannels;

    bool ok = (this->*lookup) (*texturefile, thread_info, options,
                               s, t, dsdx, dtdx, dsdy, dtdy, result);
    if (actualchannels < options.nchannels)
        fill_channels (spec.nchannels, options, result);
    return ok;
}



bool
TextureSystemImpl::texture (ustring filename, TextureOptions &options,
                            Runflag *runflags, int beginactive, int endactive,
                            VaryingRef<float> s, VaryingRef<float> t,
                            VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                            VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                            float *result)
{
    bool ok = true;
    for (int i = beginactive;  i < endactive;  ++i) {
        if (runflags[i]) {
            TextureOpt opt (options, i);
            ok &= texture (filename, opt, s[i], t[i], dsdx[i], dtdx[i],
                           dsdy[i], dtdy[i], result+i*options.nchannels);
        }
    }
    return ok;
}



bool
TextureSystemImpl::texture_lookup_nomip (TextureFile &texturefile,
                            PerThreadInfo *thread_info, 
                            TextureOpt &options,
                            float s, float t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    float* dresultds = options.dresultds;
    float* dresultdt = options.dresultdt;
    for (int c = 0;  c < options.actualchannels;  ++c) {
        result[c] = 0;
        if (dresultds) dresultds[c] = 0;
        if (dresultdt) dresultdt[c] = 0;
    }
    // If the user only provided us with one pointer, clear both to simplify
    // the rest of the code, but only after we zero out the data for them so
    // they know something went wrong.
    if (!(dresultds && dresultdt))
        dresultds = dresultdt = NULL;

    static const accum_prototype accum_functions[] = {
        // Must be in the same order as InterpMode enum
        &TextureSystemImpl::accum_sample_closest,
        &TextureSystemImpl::accum_sample_bilinear,
        &TextureSystemImpl::accum_sample_bicubic,
        &TextureSystemImpl::accum_sample_bilinear,
    };
    accum_prototype accumer = accum_functions[(int)options.interpmode];
    bool ok = (this->*accumer) (s, t, 0, texturefile,
                                thread_info, options,
                                1.0f, result, dresultds, dresultdt);

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
adjust_width_blur (float &dsdx, float &dtdx, float &dsdy, float &dtdy,
                   float swidth, float twidth, float sblur, float tblur)
{
    // Trust user not to use nonsensical width<0 or blur<0
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

    if (sblur+tblur != 0.0f /* avoid the work when blur is zero */) {
        // Carefully add blur to the right derivative components in the
        // right proportions -- meerely adding the same amount of blur
        // to all four derivatives blurs too much at some angles.
        // FIXME -- we should benchmark whether a fast approximate rsqrt
        // here could have any detectable performance improvement.
        float dxlen_inv = 1.0f / sqrtf (dxlen2);
        float dylen_inv = 1.0f / sqrtf (dylen2);
        dsdx += sblur * dsdx * dxlen_inv;
        dtdx += tblur * dtdx * dxlen_inv;
        dsdy += sblur * dsdy * dylen_inv;
        dtdy += tblur * dtdy * dylen_inv;
    }
}



bool
TextureSystemImpl::texture_lookup_trilinear_mipmap (TextureFile &texturefile,
                            PerThreadInfo *thread_info,
                            TextureOpt &options,
                            float _s, float _t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    float* dresultds = options.dresultds;
    float* dresultdt = options.dresultdt;
    for (int c = 0;  c < options.actualchannels;  ++c) {
        result[c] = 0;
        if (dresultds) dresultds[c] = 0;
        if (dresultdt) dresultdt[c] = 0;
    }
    // If the user only provided us with one pointer, clear both to simplify
    // the rest of the code, but only after we zero out the data for them so
    // they know something went wrong.
    if (!(dresultds && dresultdt))
        dresultds = dresultdt = NULL;

    // Use the differentials to figure out which MIP-map levels to use.
    adjust_width_blur (dsdx, dtdx, dsdy, dtdy, options.swidth, options.twidth,
                       options.sblur, options.tblur);

    // Determine the MIP-map level(s) we need: we will blend
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    int miplevel[2] = { -1, -1 };
    float levelblend = 0;
    float sfilt = std::max (fabsf(dsdx), fabsf(dsdy));
    float tfilt = std::max (fabsf(dtdx), fabsf(dtdy));
    float filtwidth = options.conservative_filter ? std::max (sfilt, tfilt)
                                                  : std::min (sfilt, tfilt);
    ImageCacheFile::SubimageInfo &subinfo (texturefile.subimageinfo(options.subimage));
    int nmiplevels = (int)subinfo.levels.size();
    for (int m = 0;  m < nmiplevels;  ++m) {
        // Compute the filter size in raster space at this MIP level
        float filtwidth_ras = subinfo.spec(m).width * filtwidth;
        // Once the filter width is smaller than one texel at this level,
        // we've gone too far, so we know that we want to interpolate the
        // previous level and the current level.  Note that filtwidth_ras
        // is expected to be >= 0.5, or would have stopped one level ago.
        if (filtwidth_ras <= 1) {
            miplevel[0] = m-1;
            miplevel[1] = m;
            levelblend = Imath::clamp (2.0f - 1.0f/filtwidth_ras, 0.0f, 1.0f);
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
    }
    if (options.mipmode == TextureOpt::MipModeOneLevel) {
        // Force use of just one mipmap level
        miplevel[0] = miplevel[1];
        levelblend = 0;
    }
    float levelweight[2] = { 1.0f - levelblend, levelblend };
//    std::cerr << "Levels " << miplevel[0] << ' ' << miplevel[1] << ' ' << levelblend << "\n";

    static const accum_prototype accum_functions[] = {
        // Must be in the same order as InterpMode enum
        &TextureSystemImpl::accum_sample_closest,
        &TextureSystemImpl::accum_sample_bilinear,
        &TextureSystemImpl::accum_sample_bicubic,
        &TextureSystemImpl::accum_sample_bilinear,
    };
    accum_prototype accumer = accum_functions[(int)options.interpmode];
//    if (level == 0 || texturefile.spec(lev).height < naturalres/2)
//        accumer = &TextureSystemImpl::accum_sample_bicubic;

    // FIXME -- support for smart cubic?

    bool ok = true;
    int npointson = 0;
    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        ok &= (this->*accumer) (_s, _t, miplevel[level], texturefile,
                          thread_info, options,
                          levelweight[level], result, dresultds, dresultdt);
        ++npointson;
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



// Calculate major and minor axis lengths of the ellipse specified by the
// s and t derivatives.  See Greene's EWA paper.  Return value is 0 if
// the 'x' axis was the major axis, 1 if the 'y' axis was major.
inline int
ellipse_axes (float dsdx, float dtdx, float dsdy, float dtdy,
              float &majorlength, float &minorlength)
{
    float dsdx2 = dsdx*dsdx;
    float dtdx2 = dtdx*dtdx;
    float dsdy2 = dsdy*dsdy;
    float dtdy2 = dtdy*dtdy;
    double A = dtdx2 + dtdy2;
    double B = -2.0 * (dsdx * dtdx + dsdy * dtdy);
    double C = dsdx2 + dsdy2;
    double F = A*C - B*B*0.25;
    double root = hypot (A-C, B);
    double Aprime = (A + C - root) * 0.5;
    double Cprime = (A + C + root) * 0.5;
    majorlength = A > 0 ? sqrtf (F / Aprime) : 0;
    minorlength = C > 0 ? sqrtf (F / Cprime) : 0;
    // N.B. Various papers (including the FELINE ones, imply that the
    // above calculations is the major and minor radii, but we treat
    // them as the diameter.  Tests indicate that we are filtering just
    // right, so I suspect that we are off by a factor of 2 elsewhere as
    // well, compensating perfectly for this error.  Or maybe I just
    // don't understand the situation at all.
    if ((dsdy2+dtdy2) > (dsdx2+dtdx2))
        return 1;
    else
        return 0;
}



bool
TextureSystemImpl::texture_lookup (TextureFile &texturefile,
                            PerThreadInfo *thread_info,
                            TextureOpt &options,
                            float s, float t,
                            float dsdx, float dtdx,
                            float dsdy, float dtdy,
                            float *result)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    float* dresultds = options.dresultds;
    float* dresultdt = options.dresultdt;
    for (int c = 0;  c < options.actualchannels;  ++c) {
        result[c] = 0;
        if (dresultds) dresultds[c] = 0;
        if (dresultdt) dresultdt[c] = 0;
    }
    // If the user only provided us with one pointer, clear both to simplify
    // the rest of the code, but only after we zero out the data for them so
    // they know something went wrong.
    if (!(dresultds && dresultdt))
        dresultds = dresultdt = NULL;

    // Compute the natural resolution we want for the bare derivs, this
    // will be the threshold for knowing we're maxifying (and therefore
    // wanting cubic interpolation).
    float sfilt_noblur = std::max (std::max (fabsf(dsdx), fabsf(dsdy)), 1e-8f);
    float tfilt_noblur = std::max (std::max (fabsf(dtdx), fabsf(dtdy)), 1e-8f);
    int naturalsres = (int) (1.0f / sfilt_noblur);
    int naturaltres = (int) (1.0f / tfilt_noblur);

    // Scale by 'width' and 'blur'
    adjust_width_blur (dsdx, dtdx, dsdy, dtdy, options.swidth, options.twidth,
                       options.sblur, options.tblur);

    // Determine the MIP-map level(s) we need: we will blend
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    int miplevel[2] = { -1, -1 };
    float levelblend = 0;
    // The ellipse is made up of two axes which correspond to the x and y pixel
    // directions. Pick the longest one and take several samples along it.
    float smajor, tmajor, sminor, tminor;
    float majorlength, minorlength;

#if 0
    // OLD code -- I thought this approximation was a good try, but it
    // severely underestimates anisotropy for grazing angles, and
    // therefore overblurs the texture lookups compared to the
    // ellipse_axis code below...
    float xfilt = std::max (fabsf(dsdx), fabsf(dtdx));
    float yfilt = std::max (fabsf(dsdy), fabsf(dtdy));
    if (xfilt > yfilt) {
        majorlength = xfilt;
        minorlength = yfilt;
        smajor = dsdx;
        tmajor = dtdx;
        sminor = dsdy;
        tminor = dtdy;
    } else {
        majorlength = yfilt;
        minorlength = xfilt;
        smajor = dsdy;
        tmajor = dtdy;
        sminor = dsdx;
        tminor = dtdx;
    }
#else
    // Do a bit more math and get the exact ellipse axis lengths, and
    // therefore a more accurate aspect ratio as well.  Looks much MUCH
    // better, but for scenes with lots of grazing angles, it can greatly
    // increase the average anisotropy, therefore the number of bilinear
    // or bicubic texture probes, and therefore runtime!
    // FIXME -- come back and improve performance to compensate.
    int axis = ellipse_axes (dsdx, dtdx, dsdy, dtdy, majorlength, minorlength);
    if (axis) {
        smajor = dsdy;
        tmajor = dtdy;
        sminor = dsdx;
        tminor = dtdx;
    } else {
        smajor = dsdx;
        tmajor = dtdx;
        sminor = dsdy;
        tminor = dtdy;
    }
#endif

    float aspect, trueaspect;
    aspect = anisotropic_aspect (majorlength, minorlength, options, trueaspect);

    ImageCacheFile::SubimageInfo &subinfo (texturefile.subimageinfo(options.subimage));
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
            levelblend = Imath::clamp (2.0f - 1.0f/filtwidth_ras, 0.0f, 1.0f);
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
        if (minorlength*r < 0.5f)
            aspect = Imath::clamp (majorlength * r * 2.0f, 1.0f, float(options.anisotropic));
    }
    if (options.mipmode == TextureOpt::MipModeOneLevel) {
        miplevel[0] = miplevel[1];
        levelblend = 0;
    }
    float levelweight[2] = { 1.0f - levelblend, levelblend };

    int nsamples = std::max (1, (int) ceilf (aspect - 0.25f));
    float invsamples = 1.0f / nsamples;

    bool ok = true;
    int npointson = 0;
    int closestprobes = 0, bilinearprobes = 0, bicubicprobes = 0;
    float sumw = 0;
    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        ++npointson;
        int lev = miplevel[level];
        accum_prototype accumer = &TextureSystemImpl::accum_sample_bilinear;
        switch (options.interpmode) {
        case TextureOpt::InterpClosest :
            accumer = &TextureSystemImpl::accum_sample_closest;
            ++closestprobes;
            break;
        case TextureOpt::InterpBilinear :
            accumer = &TextureSystemImpl::accum_sample_bilinear;
            ++bilinearprobes;
            break;
        case TextureOpt::InterpBicubic :
            accumer = &TextureSystemImpl::accum_sample_bicubic;
            ++bicubicprobes;
            break;
        case TextureOpt::InterpSmartBicubic :
            if (lev == 0 || 
                (texturefile.spec(options.subimage,lev).width < naturalsres/2) ||
                (texturefile.spec(options.subimage,lev).height < naturaltres/2)) {
                accumer = &TextureSystemImpl::accum_sample_bicubic;
                ++bicubicprobes;
            } else {
                accumer = &TextureSystemImpl::accum_sample_bilinear;
                ++bilinearprobes;
            }
            break;
        }
        for (int sample = 0;  sample < nsamples;  ++sample) {
            float pos = (sample + 0.5f) * invsamples - 0.5f;
            ok &= (this->*accumer) (s + pos * smajor, t + pos * tmajor, lev, texturefile,
                                    thread_info, options, levelweight[level],
                                    result, dresultds, dresultdt);
            sumw += levelweight[level];
        }
    }

    if (sumw > 0) {
       float invw = 1 / sumw;
       for (int c = 0;  c < options.actualchannels;  ++c) {
          result[c] *= invw;
          if (dresultds) dresultds[c] *= invw;
          if (dresultdt) dresultdt[c] *= invw;
       }
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
    size_t pixelsize = texturefile.pixelsize();
    if (! levelinfo.polecolorcomputed) {
        static spin_mutex mutex;   // Protect everybody's polecolor
        spin_lock lock (mutex);
        if (! levelinfo.polecolorcomputed) {
            DASSERT (levelinfo.polecolor.size() == 0);
            levelinfo.polecolor.resize (2*spec.nchannels);
            // We store north and south poles adjacently in polecolor
            float *p = &(levelinfo.polecolor[0]);
            size_t width = spec.width;
            float scale = 1.0f / width;
            for (int pole = 0;  pole <= 1;  ++pole, p += spec.nchannels) {
                int y = pole * (spec.height-1);   // 0 or height-1
                for (int c = 0;  c < spec.nchannels;  ++c)
                    p[c] = 0.0f;
                const unsigned char *texel = tile->bytedata() + y*spec.tile_width*pixelsize;
                for (size_t i = 0;  i < width;  ++i, texel += pixelsize)
                    for (int c = 0;  c < spec.nchannels;  ++c)
                        if (texturefile.eightbit())
                            p[c] += uchar2float(texel[c]);
                        else
                            p[c] += ((const float *)texel)[c];
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
TextureSystemImpl::accum_sample_closest (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 TextureOpt &options,
                                 float weight, float *accum, float *daccumds, float *daccumdt)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    int stex, ttex;    // Texel coordintes
    float sfrac, tfrac;
    st_to_texel (s, t, texturefile, spec, stex, ttex, sfrac, tfrac);

    if (sfrac > 0.5f)
        ++stex;
    if (tfrac > 0.5f)
        ++ttex;
    
    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    bool svalid, tvalid;  // Valid texels?  false means black border
    svalid = options.swrap_func (stex, spec.x, spec.width);
    tvalid = options.twrap_func (ttex, spec.y, spec.height);
    if (! levelinfo.full_pixel_range) {
        svalid &= (stex >= spec.x && stex < (spec.x+spec.width)); // data window
        tvalid &= (ttex >= spec.y && ttex < (spec.y+spec.height));
    }
    if (! (svalid & tvalid)) {
        // All texels we need were out of range and using 'black' wrap.
        return true;
    }

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int tile_s = (stex - spec.x) & tilewidthmask;
    int tile_t = (ttex - spec.y) & tileheightmask;
    TileID id (texturefile, options.subimage, miplevel,
               stex - tile_s, ttex - tile_t, 0);
    bool ok = find_tile (id, thread_info);
    if (! ok)
        error ("%s", m_imagecache->geterror().c_str());
    TileRef &tile (thread_info->tile);
    if (! tile  ||  ! ok)
        return false;
    size_t channelsize = texturefile.channelsize();
    int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s) + options.firstchannel;
    DASSERT ((size_t)offset < spec.nchannels*spec.tile_pixels());
    if (channelsize == 1) {
        // special case for 8-bit tiles
        const unsigned char *texel = tile->bytedata() + offset;
        for (int c = 0;  c < options.actualchannels;  ++c)
            accum[c] += weight * uchar2float(texel[c]);
    } else {
        // General case for float tiles
        const float *texel = tile->data() + offset;
        for (int c = 0;  c < options.actualchannels;  ++c)
            accum[c] += weight * texel[c];
    }
    return true;
}



bool
TextureSystemImpl::accum_sample_bilinear (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 TextureOpt &options,
                                 float weight, float *accum, float *daccumds, float *daccumdt)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    int sint, tint;
    float sfrac, tfrac;
    st_to_texel (s, t, texturefile, spec, sint, tint, sfrac, tfrac);

    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    int stex[2], ttex[2];       // Texel coords
    stex[0] = sint;  stex[1] = sint+1;
    ttex[0] = tint;  ttex[1] = tint+1;
//    bool svalid[2], tvalid[2];  // Valid texels?  false means black border
    union { bool bvalid[4]; unsigned int ivalid; } valid_storage;
    valid_storage.ivalid = 0;
    DASSERT (sizeof(valid_storage) == 4*sizeof(bool));
    const unsigned int none_valid = 0;
    const unsigned int all_valid = 0x01010101;
    bool *svalid = valid_storage.bvalid;
    bool *tvalid = valid_storage.bvalid + 2;
    svalid[0] = options.swrap_func (stex[0], spec.x, spec.width);
    svalid[1] = options.swrap_func (stex[1], spec.x, spec.width);
    tvalid[0] = options.twrap_func (ttex[0], spec.y, spec.height);
    tvalid[1] = options.twrap_func (ttex[1], spec.y, spec.height);

    // FIXME -- we've got crop windows all wrong

    // Account for crop windows
    if (! levelinfo.full_pixel_range) {
        svalid[0] &= (stex[0] >= spec.x && stex[0] < spec.x+spec.width);
        svalid[1] &= (stex[1] >= spec.x && stex[1] < spec.x+spec.width);
        tvalid[0] &= (ttex[0] >= spec.y && ttex[0] < spec.y+spec.height);
        tvalid[1] &= (ttex[1] >= spec.y && ttex[1] < spec.y+spec.height);
    }
//    if (! (svalid[0] | svalid[1] | tvalid[0] | tvalid[1]))
    if (valid_storage.ivalid == none_valid)
        return true; // All texels we need were out of range and using 'black' wrap

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    const unsigned char *texel[2][2];
    TileRef savetile[2][2];
    static float black[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int tile_s = (stex[0] - spec.x) % spec.tile_width;
    int tile_t = (ttex[0] - spec.y) % spec.tile_height;
    bool s_onetile = (tile_s != tilewidthmask) & (stex[0]+1 == stex[1]);
    bool t_onetile = (tile_t != tileheightmask) & (ttex[0]+1 == ttex[1]);
    bool onetile = (s_onetile & t_onetile);
    size_t channelsize = texturefile.channelsize();
    size_t pixelsize = texturefile.pixelsize();
    if (onetile &&
//        (svalid[0] & svalid[1] & tvalid[0] & tvalid[1])) {
        valid_storage.ivalid == all_valid) {
        // Shortcut if all the texels we need are on the same tile
        TileID id (texturefile, options.subimage, miplevel,
                   stex[0] - tile_s, ttex[0] - tile_t, 0);
        bool ok = find_tile (id, thread_info);
        if (! ok)
            error ("%s", m_imagecache->geterror().c_str());
        TileRef &tile (thread_info->tile);
        if (! tile->valid()) {
#if 0
            std::cerr << "found it\n";
            std::cerr << "\trequested " << miplevel << ' ' << id.x() << ' ' << id.y() << ", res is " << texturefile.spec(miplevel).width << ' ' << texturefile.spec(miplevel).height << "\n";
            std::cerr << "\tstex = " << stex[0] << ", tile_s = " << tile_s << ", ttex = " << ttex[0] << ", tile_t = " << tile_t << "\n";
            std::cerr << "\torig " << orig_s << ' ' << orig_t << "\n";
            std::cerr << "\ts,t = " << s << ' ' << t << "\n";
            std::cerr << "\tsint,tint = " << sint << ' ' << tint << "\n";
            std::cerr << "\tstfrac = " << sfrac << ' ' << tfrac << "\n";
            std::cerr << "\tspec full size = " << texturefile.spec(miplevel).full_width << ' ' << texturefile.spec(miplevel).full_height << "\n";
#endif
            return false;
        }
        // N.B. thread_info->tile will keep holding a ref-counted pointer
        // to the tile for the duration that we're using the tile data.
        int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
        texel[0][0] = tile->bytedata() + offset + channelsize * options.firstchannel;
        texel[0][1] = texel[0][0] + pixelsize;
        texel[1][0] = texel[0][0] + pixelsize * spec.tile_width;
        texel[1][1] = texel[1][0] + pixelsize;
    } else {
        for (int j = 0;  j < 2;  ++j) {
            for (int i = 0;  i < 2;  ++i) {
                if (! (svalid[i] && tvalid[j])) {
                    texel[j][i] = (unsigned char *)black;
                    continue;
                }
                tile_s = (stex[i] - spec.x) % spec.tile_width;
                tile_t = (ttex[j] - spec.y) % spec.tile_height;
                TileID id (texturefile, options.subimage, miplevel,
                           stex[i] - tile_s, ttex[j] - tile_t, 0);
                bool ok = find_tile (id, thread_info);
                if (! ok)
                    error ("%s", m_imagecache->geterror().c_str());
                TileRef &tile (thread_info->tile);
                if (! tile->valid())
                    return false;
                savetile[j][i] = tile;
                int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
                DASSERT ((size_t)offset < spec.tile_width*spec.tile_height*spec.tile_depth*pixelsize);
                texel[j][i] = tile->bytedata() + offset + channelsize * options.firstchannel;
                DASSERT (tile->id() == id);
            }
        }
    }
    // FIXME -- optimize the above loop by unrolling

    int nc = options.actualchannels;

    // When we're on the lowest res mipmap levels, it's more pleasing if
    // we converge to a single pole color right at the pole.  Fade to
    // the average color over the texel height right next to the pole.
    if (options.envlayout == LayoutLatLong && levelinfo.onetile) {
        float height = spec.height;
        if (texturefile.m_sample_border)
            height -= 1.0f;
        float tt = t * height;
        if (tt < 1.0f || tt > (height-1.0f))
            fade_to_pole (tt, accum, weight, texturefile, thread_info,
                          levelinfo, options, miplevel, nc);
    }

    if (channelsize == 1) {
        // special case for 8-bit tiles
        int c;
        for (c = 0;  c < nc;  ++c)
            accum[c] += weight * bilerp (uchar2float(texel[0][0][c]), uchar2float(texel[0][1][c]),
                                         uchar2float(texel[1][0][c]), uchar2float(texel[1][1][c]),
                                         sfrac, tfrac);
        if (daccumds) {
            float scalex = weight * spec.width;
            float scaley = weight * spec.height;
            for (c = 0;  c < nc;  ++c) {
                daccumds[c] += scalex * Imath::lerp(
                    uchar2float(texel[0][1][c]) - uchar2float(texel[0][0][c]),
                    uchar2float(texel[1][1][c]) - uchar2float(texel[1][0][c]),
                    tfrac
                );
                daccumdt[c] += scaley * Imath::lerp(
                    uchar2float(texel[1][0][c]) - uchar2float(texel[0][0][c]),
                    uchar2float(texel[1][1][c]) - uchar2float(texel[0][1][c]),
                    sfrac
                );
            }
        }
    } else {
        // General case for float tiles
        bilerp_mad ((const float *)texel[0][0], (const float *)texel[0][1],
                    (const float *)texel[1][0], (const float *)texel[1][1],
                    sfrac, tfrac, weight, nc, accum);
        if (daccumds) {
            float scalex = weight * spec.width;
            float scaley = weight * spec.height;
            for (int c = 0;  c < nc;  ++c) {
                daccumds[c] += scalex * Imath::lerp(
                    ((const float *) texel[0][1])[c] - ((const float *) texel[0][0])[c],
                    ((const float *) texel[1][1])[c] - ((const float *) texel[1][0])[c],
                    tfrac
                );
                daccumdt[c] += scaley * Imath::lerp(
                    ((const float *) texel[1][0])[c] - ((const float *) texel[0][0])[c],
                    ((const float *) texel[1][1])[c] - ((const float *) texel[0][1])[c],
                    sfrac
                );
            }
        }
    }

    return true;
}

namespace {

template <typename T>
inline void evalBSplineWeights (T w[4], T fraction)
{
    T one_frac = 1 - fraction;
    w[0] = T(1.0 / 6.0) * one_frac * one_frac * one_frac;
    w[1] = T(2.0 / 3.0) - T(0.5) * fraction * fraction * (2 - fraction);
    w[2] = T(2.0 / 3.0) - T(0.5) * one_frac * one_frac * (2 - one_frac);
    w[3] = T(1.0 / 6.0) * fraction * fraction * fraction;
}

template <typename T>
inline void evalBSplineWeightDerivs (T dw[4], T fraction)
{
    T one_frac = 1 - fraction;
    dw[0] = -T(0.5) * one_frac * one_frac;
    dw[1] =  T(0.5) * fraction * (3 * fraction - 4);
    dw[2] = -T(0.5) * one_frac * (3 * one_frac - 4);
    dw[3] =  T(0.5) * fraction * fraction;
}

} // anonymous namesace

bool
TextureSystemImpl::accum_sample_bicubic (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 TextureOpt &options,
                                 float weight, float *accum, float *daccumds, float *daccumdt)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    int sint, tint;
    float sfrac, tfrac;
    st_to_texel (s, t, texturefile, spec, sint, tint, sfrac, tfrac);

    // We're gathering 4x4 samples and 4x weights.  Indices: texels 0,
    // 1, 2, 3.  The sample lies between samples 1 and 2.

    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    bool svalid[4], tvalid[4];  // Valid texels?  false means black border
    int stex[4], ttex[4];       // Texel coords
    bool allvalid = true;
    bool anyvalid = false;
    for (int i = 0; i < 4;  ++i) {
        bool v;
        stex[i] = sint + i - 1;
        v = options.swrap_func (stex[i], spec.x, spec.width);
        svalid[i] = v;
        allvalid &= v;
        anyvalid |= v;
        ttex[i] = tint + i - 1;
        v = options.twrap_func (ttex[i], spec.y, spec.height);
        tvalid[i] = v;
        allvalid &= v;
        anyvalid |= v;
    }
    if (anyvalid && ! levelinfo.full_pixel_range) {
        // Handle case of crop windows or overscan
        anyvalid = false;
        for (int i = 0; i < 4;  ++i) {
            bool v;
            v = (stex[i] >= spec.x && stex[i] < (spec.x+spec.width));
            svalid[i] &= v;
            allvalid &= v;
            anyvalid |= v;
            v = (ttex[i] >= spec.y && ttex[i] < (spec.y+spec.height));
            tvalid[i] &= v;
            allvalid &= v;
            anyvalid |= v;
        }
    }
    if (! anyvalid) {
        // All texels we need were out of range and using 'black' wrap.
        return true;
    }

    const unsigned char *texel[4][4] = { {NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL},
                                         {NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL} };
    TileRef savetile[4][4];
    static float black[4] = { 0, 0, 0, 0 };
    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int tile_s = (stex[0] - spec.x) % spec.tile_width;
    int tile_t = (ttex[0] - spec.y) % spec.tile_height;
    bool s_onetile = (tile_s <= tilewidthmask-3);
    bool t_onetile = (tile_t <= tileheightmask-3);
    if (s_onetile && t_onetile) {
        for (int i = 1; i < 4;  ++i) {
            s_onetile &= (stex[i] == stex[0]+i);
            t_onetile &= (ttex[i] == ttex[0]+i);
        }
    }
    bool onetile = (s_onetile & t_onetile);
    size_t channelsize = texturefile.channelsize();
    size_t pixelsize = texturefile.pixelsize();
    if (onetile & allvalid) {
        // Shortcut if all the texels we need are on the same tile
        TileID id (texturefile, options.subimage, miplevel,
                   stex[0] - tile_s, ttex[0] - tile_t, 0);
        bool ok = find_tile (id, thread_info);
        if (! ok)
            error ("%s", m_imagecache->geterror().c_str());
        TileRef &tile (thread_info->tile);
        if (! tile) {
            return false;
        }
        // N.B. thread_info->tile will keep holding a ref-counted pointer
        // to the tile for the duration that we're using the tile data.
        int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
        const unsigned char *base = tile->bytedata() + offset + channelsize * options.firstchannel;
        DASSERT (tile->data());
        for (int j = 0;  j < 4;  ++j)
            for (int i = 0;  i < 4;  ++i)
                texel[j][i] = base + pixelsize * (i + j*spec.tile_width);
    } else {
        for (int j = 0;  j < 4;  ++j) {
            for (int i = 0;  i < 4;  ++i) {
                if (! (svalid[i] && tvalid[j])) {
                    texel[j][i] = (unsigned char *) black;
                    continue;
                }
                int stex_i = stex[i];
                tile_s = (stex_i - spec.x) % spec.tile_width;
                tile_t = (ttex[j] - spec.y) % spec.tile_height;
                TileID id (texturefile, options.subimage, miplevel,
                           stex_i - tile_s, ttex[j] - tile_t, 0);
                bool ok = find_tile (id, thread_info);
                if (! ok)
                    error ("%s", m_imagecache->geterror().c_str());
                TileRef &tile (thread_info->tile);
                if (! tile->valid())
                    return false;
                savetile[j][i] = tile;
                DASSERT (tile->id() == id);
                int offset = pixelsize * (tile_t * spec.tile_width + tile_s);
                DASSERT (tile->data());
                texel[j][i] = tile->bytedata() + offset + channelsize * options.firstchannel;
            }
        }
    }

    int nc = options.actualchannels;

    // When we're on the lowest res mipmap levels, it's more pleasing if
    // we converge to a single pole color right at the pole.  Fade to
    // the average color over the texel height right next to the pole.
    if (options.envlayout == LayoutLatLong && levelinfo.onetile) {
        float height = spec.height;
        if (texturefile.m_sample_border)
            height -= 1.0f;
        float tt = t * height;
        if (tt < 1.0f || tt > (height-1.0f))
            fade_to_pole (tt, accum, weight, texturefile, thread_info,
                          levelinfo, options, miplevel, nc);
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
    float wx[4]; evalBSplineWeights (wx, sfrac);
    float wy[4]; evalBSplineWeights (wy, tfrac);
    // figure out lerp weights so we can turn the filter into a sequence of lerp's
    float g0x = wx[0] + wx[1]; float h0x = (wx[1] / g0x); 
    float g1x = wx[2] + wx[3]; float h1x = (wx[3] / g1x); 
    float g0y = wy[0] + wy[1]; float h0y = (wy[1] / g0y);
    float g1y = wy[2] + wy[3]; float h1y = (wy[3] / g1y);

    if (texturefile.eightbit()) {
        for (int c = 0;  c < nc; ++c) {
            float col[4];
            for (int j = 0;  j < 4; ++j) {
                float lx = Imath::lerp (uchar2float(texel[j][0][c]), uchar2float(texel[j][1][c]), h0x);
                float rx = Imath::lerp (uchar2float(texel[j][2][c]), uchar2float(texel[j][3][c]), h1x);
                col[j]   = Imath::lerp (lx, rx, g1x);
            }
            float ly = Imath::lerp (col[0], col[1], h0y);
            float ry = Imath::lerp (col[2], col[3], h1y);
            accum[c] += weight * Imath::lerp (ly, ry, g1y);
        }
        if (daccumds) {
            float dwx[4]; evalBSplineWeightDerivs (dwx, sfrac);
            float dwy[4]; evalBSplineWeightDerivs (dwy, tfrac);
            float scalex = weight * spec.width;
            float scaley = weight * spec.height;
            for (int c = 0;  c < nc; ++c) {
                daccumds[c] += scalex * (
                    dwx[0] * (wy[0] * uchar2float(texel[0][0][c]) +
                              wy[1] * uchar2float(texel[1][0][c]) +
                              wy[2] * uchar2float(texel[2][0][c]) +
                              wy[3] * uchar2float(texel[3][0][c])) +
                    dwx[1] * (wy[0] * uchar2float(texel[0][1][c]) +
                              wy[1] * uchar2float(texel[1][1][c]) +
                              wy[2] * uchar2float(texel[2][1][c]) +
                              wy[3] * uchar2float(texel[3][1][c])) +
                    dwx[2] * (wy[0] * uchar2float(texel[0][2][c]) +
                              wy[1] * uchar2float(texel[1][2][c]) +
                              wy[2] * uchar2float(texel[2][2][c]) +
                              wy[3] * uchar2float(texel[3][2][c])) +
                    dwx[3] * (wy[0] * uchar2float(texel[0][3][c]) +
                              wy[1] * uchar2float(texel[1][3][c]) +
                              wy[2] * uchar2float(texel[2][3][c]) +
                              wy[3] * uchar2float(texel[3][3][c]))
                );
                daccumdt[c] += scaley * (
                    dwy[0] * (wx[0] * uchar2float(texel[0][0][c]) +
                              wx[1] * uchar2float(texel[0][1][c]) +
                              wx[2] * uchar2float(texel[0][2][c]) +
                              wx[3] * uchar2float(texel[0][3][c])) +
                    dwy[1] * (wx[0] * uchar2float(texel[1][0][c]) +
                              wx[1] * uchar2float(texel[1][1][c]) +
                              wx[2] * uchar2float(texel[1][2][c]) +
                              wx[3] * uchar2float(texel[1][3][c])) +
                    dwy[2] * (wx[0] * uchar2float(texel[2][0][c]) +
                              wx[1] * uchar2float(texel[2][1][c]) +
                              wx[2] * uchar2float(texel[2][2][c]) +
                              wx[3] * uchar2float(texel[2][3][c])) +
                    dwy[3] * (wx[0] * uchar2float(texel[3][0][c]) +
                              wx[1] * uchar2float(texel[3][1][c]) +
                              wx[2] * uchar2float(texel[3][2][c]) +
                              wx[3] * uchar2float(texel[3][3][c]))
                );
            }
        }
    } else {
        // float texels
        for (int c = 0;  c < nc; ++c) {
           float col[4];
           for (int j = 0;  j < 4; ++j) {
               float lx = Imath::lerp (((const float*)(texel[j][0]))[c], ((const float*)(texel[j][1]))[c], h0x);
               float rx = Imath::lerp (((const float*)(texel[j][2]))[c], ((const float*)(texel[j][3]))[c], h1x);
               col[j]   = Imath::lerp (lx, rx, g1x);
           }
           float ly = Imath::lerp (col[0], col[1], h0y);
           float ry = Imath::lerp (col[2], col[3], h1y);
           accum[c] += weight * Imath::lerp (ly, ry, g1y);
        }
        if (daccumds) {
            float dwx[4]; evalBSplineWeightDerivs (dwx, sfrac);
            float dwy[4]; evalBSplineWeightDerivs (dwy, tfrac);
            float scalex = weight * spec.width;
            float scaley = weight * spec.height;
            for (int c = 0;  c < nc; ++c) {
                daccumds[c] += scalex * (
                    dwx[0] * (wy[0] * ((const float*)(texel[0][0]))[c] +
                              wy[1] * ((const float*)(texel[1][0]))[c] +
                              wy[2] * ((const float*)(texel[2][0]))[c] +
                              wy[3] * ((const float*)(texel[3][0]))[c]) +
                    dwx[1] * (wy[0] * ((const float*)(texel[0][1]))[c] +
                              wy[1] * ((const float*)(texel[1][1]))[c] +
                              wy[2] * ((const float*)(texel[2][1]))[c] +
                              wy[3] * ((const float*)(texel[3][1]))[c]) +
                    dwx[2] * (wy[0] * ((const float*)(texel[0][2]))[c] +
                              wy[1] * ((const float*)(texel[1][2]))[c] +
                              wy[2] * ((const float*)(texel[2][2]))[c] +
                              wy[3] * ((const float*)(texel[3][2]))[c]) +
                    dwx[3] * (wy[0] * ((const float*)(texel[0][3]))[c] +
                              wy[1] * ((const float*)(texel[1][3]))[c] +
                              wy[2] * ((const float*)(texel[2][3]))[c] +
                              wy[3] * ((const float*)(texel[3][3]))[c])
                );
                daccumdt[c] += scaley * (
                    dwy[0] * (wx[0] * ((const float*)(texel[0][0]))[c] +
                              wx[1] * ((const float*)(texel[0][1]))[c] +
                              wx[2] * ((const float*)(texel[0][2]))[c] +
                              wx[3] * ((const float*)(texel[0][3]))[c]) +
                    dwy[1] * (wx[0] * ((const float*)(texel[1][0]))[c] +
                              wx[1] * ((const float*)(texel[1][1]))[c] +
                              wx[2] * ((const float*)(texel[1][2]))[c] +
                              wx[3] * ((const float*)(texel[1][3]))[c]) +
                    dwy[2] * (wx[0] * ((const float*)(texel[2][0]))[c] +
                              wx[1] * ((const float*)(texel[2][1]))[c] +
                              wx[2] * ((const float*)(texel[2][2]))[c] +
                              wx[3] * ((const float*)(texel[2][3]))[c]) +
                    dwy[3] * (wx[0] * ((const float*)(texel[3][0]))[c] +
                              wx[1] * ((const float*)(texel[3][1]))[c] +
                              wx[2] * ((const float*)(texel[3][2]))[c] +
                              wx[3] * ((const float*)(texel[3][3]))[c])
                );
            }
        }
    }

    return true;
}


};  // end namespace pvt

}
OIIO_NAMESPACE_EXIT
