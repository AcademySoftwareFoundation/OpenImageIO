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


#include <string>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <half.h>

#include "dassert.h"
#include "typedesc.h"
#include "varyingref.h"
#include "ustring.h"
#include "strutil.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
#include "filter.h"
#include "imageio.h"
using namespace OpenImageIO;

#define DLL_EXPORT_PUBLIC /* Because we are implementing TextureSystem */
#include "texture.h"
#undef DLL_EXPORT_PUBLIC

#include "texture_pvt.h"
using namespace OpenImageIO::pvt;


namespace OpenImageIO {


TextureSystem *
TextureSystem::create ()
{
    return new TextureSystemImpl;
}



void
TextureSystem::destroy (TextureSystem * &x)
{
    delete x;
    x = NULL;
}



namespace pvt {   // namespace OpenImageIO::pvt



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



Tile::Tile (const TileID &id)
    : m_id (id), m_valid(true), m_used(true)
{
    TextureFile &texfile = m_id.texfile ();
    m_texels.resize (memsize());
    if (! texfile.read_tile (m_id.level(), m_id.x(), m_id.y(), m_id.z(),
                             texfile.datatype(), &m_texels[0])) {
        std::cerr << "(1) error reading tile " << m_id.x() << ' ' << m_id.y() << "\n";
    }
    // FIXME -- for shadow, fill in mindepth, maxdepth
}



const void *
Tile::data (int x, int y, int z) const
{
    const ImageSpec &spec = m_id.texfile().spec (m_id.level());
    size_t w = spec.tile_width;
    size_t h = spec.tile_height;
    size_t d = std::max (1, spec.tile_depth);
    x -= m_id.x();
    y -= m_id.y();
    z -= m_id.z();
    if (x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d)
        return NULL;
    size_t pixelsize = spec.nchannels * m_id.texfile().datatype().size();
    size_t offset = ((z * h + y) * w + x) * pixelsize;
    return (const void *)&m_texels[offset];
}



TextureSystemImpl::TextureSystemImpl ()
    : m_open_files(0), m_file_sweep(m_texturefiles.end()),
      m_tile_sweep(m_tilecache.end()), hq_filter(NULL)
{
    init ();
}



TextureSystemImpl::~TextureSystemImpl ()
{
    delete hq_filter;
}



void
TextureSystemImpl::init ()
{
    max_open_files (100);
    max_memory_MB (50);
    m_Mw2c.makeIdentity();
    delete hq_filter;
    hq_filter = Filter1D::create ("b-spline", 4);
}



TileRef
TextureSystemImpl::find_tile (const TileID &id)
{
    DASSERT (id.texfile() != NULL);
    lock_guard guard (m_texturefiles_mutex);
    TileCache::iterator found = m_tilecache.find (id);
    TileRef tile;
    if (found != m_tilecache.end()) {
        tile = found->second;
    } else {
        check_max_mem ();
        tile.reset (new Tile (id));
        m_tilecache[id] = tile;
        m_mem_used += tile->memsize();
    }
    DASSERT (id == tile->id() && !memcmp(&id, &tile->id(), sizeof(TileID)));
    tile->used ();
    return tile;
}



void
TextureSystemImpl::check_max_mem ()
{
#ifdef DEBUG
    std::cerr << "mem used: " << m_mem_used << ", max = " << m_max_memory_bytes << "\n";
#endif
    while (m_mem_used >= m_max_memory_bytes) {
        if (m_tile_sweep == m_tilecache.end())
            m_tile_sweep = m_tilecache.begin();
        ASSERT (m_tile_sweep != m_tilecache.end());
        if (! m_tile_sweep->second->used (false)) {
            TileCache::iterator todelete = m_tile_sweep;
            ++m_tile_sweep;
            ASSERT (m_mem_used > todelete->second->memsize ());
            m_mem_used -= todelete->second->memsize ();
#ifdef DEBUG
            std::cerr << "  Freeing tile, recovering " 
                      << todelete->second->memsize() << "\n";
#endif
            m_tilecache.erase (todelete);
        } else {
            ++m_tile_sweep;
        }
    }
}



bool
TextureSystemImpl::get_texture_info (ustring filename, ustring dataname,
                                     TypeDesc datatype, void *data)
{
    TextureFileRef texfile = find_texturefile (filename);
    if (! texfile) {
        error ("Texture file \"%s\" not found", filename.c_str());
        return false;
    }
    if (texfile->broken()) {
        error ("Invalid texture file \"%s\"", filename.c_str());
        return false;
    }
    const ImageSpec &spec (texfile->spec());
    if (dataname == "resolution" && datatype==TypeDesc(TypeDesc::INT,2)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        return true;
    }
    if (dataname == "texturetype" && datatype == TypeDesc::TypeString) {
        ustring s (texture_type_name (texfile->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "textureformat" && datatype == TypeDesc::TypeString) {
        ustring s (texture_format_name (texfile->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "channels" && datatype == TypeDesc::TypeInt) {
        *(int *)data = spec.nchannels;
        return true;
    }
    if (dataname == "channels" && datatype == TypeDesc::TypeFloat) {
        *(float *)data = spec.nchannels;
        return true;
    }
    // FIXME - "viewingmatrix"
    // FIXME - "projectionmatrix"

    // general case
    const ImageIOParameter *p = spec.find_attribute (dataname.string());
    if (p && p->type().arraylen == datatype.arraylen) {
        // First test for exact type match
        if (p->type() == datatype) {
            memcpy (data, p->data(), datatype.size());
            return true;
        }
        // If the real data is int but user asks for float, translate it
        if (p->type().basetype == TypeDesc::FLOAT &&
                datatype.basetype == TypeDesc::INT) {
            for (int i = 0;  i < p->type().arraylen;  ++i)
                ((float *)data)[i] = ((int *)p->data())[i];
            return true;
        }
    }

    return false;
}



bool
TextureSystemImpl::get_imagespec (ustring filename, ImageSpec &spec)
{
    TextureFileRef texfile = find_texturefile (filename);
    if (! texfile) {
        error ("Texture file \"%s\" not found", filename.c_str());
        return false;
    }
    if (texfile->broken()) {
        error ("Invalid texture file \"%s\"", filename.c_str());
        return false;
    }
    spec = texfile->spec();
    return true;
}



bool
TextureSystemImpl::get_texels (ustring filename, TextureOptions &options,
                               int xmin, int xmax, int ymin, int ymax,
                               int zmin, int zmax, int level,
                               TypeDesc format, void *result)
{
    TextureFileRef texfile = find_texturefile (filename);
    if (! texfile) {
        error ("Texture file \"%s\" not found", filename.c_str());
        return false;
    }
    if (texfile->broken()) {
        error ("Invalid texture file \"%s\"", filename.c_str());
        return false;
    }
    if (level < 0 || level >= texfile->levels()) {
        error ("get_texel asked for nonexistant level %d of \"%s\"",
               level, filename.c_str());
        return false;
    }
    const ImageSpec &spec (texfile->spec());

    // FIXME -- this could be WAY more efficient than starting from
    // scratch for each pixel within the rectangle.  Instead, we should
    // grab a whole tile at a time and memcpy it rapidly.  But no point
    // doing anything more complicated (not to mention bug-prone) until
    // somebody reports this routine as being a bottleneck.
    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel, 0, options.nchannels);
    TileRef tile, lasttile;
    int nc = texfile->spec().nchannels;
    size_t formatpixelsize = nc * format.size();
    size_t tilepixelsize = nc * texfile->datatype().size();
    ASSERT (texfile->datatype() == TypeDesc::FLOAT);  // won't work otherwise
    float *texel = (float *) alloca (nc * sizeof(float));
    for (int z = zmin;  z <= zmax;  ++z) {
        int tz = z - (z % spec.tile_depth);
        for (int y = ymin;  y <= ymax;  ++y) {
            int ty = y - (y % spec.tile_height);
            for (int x = xmin;  x <= xmax;  ++x) {
                int tx = x - (x % spec.tile_width);
                TileID tileid (*texfile, level, tx, ty, tz);
                find_tile (tileid, tile, lasttile);
                const void *data;
                texel = (float *)result;
                if (tile && (data = tile->data (x, y, z))) {
                    for (int c = 0;  c < actualchannels;  ++c)
                        texel[c] = ((float *)data)[options.firstchannel + c];
                    for (int c = actualchannels;  c < options.nchannels;  ++c)
                        texel[c] = options.fill[0];
                    convert_types (texfile->datatype(), texel, format, result, nc);
                } else {
                    memset (texel, 0, formatpixelsize);
                }
                result = (void *) ((char *) result + formatpixelsize);
            }
        }
    }
    return false;
}



std::string
TextureSystemImpl::geterror () const
{
    lock_guard lock (m_errmutex);
    std::string e = m_errormessage;
    m_errormessage.clear();
    return e;
}



void
TextureSystemImpl::error (const char *message, ...)
{
    lock_guard lock (m_errmutex);
    va_list ap;
    va_start (ap, message);
    m_errormessage = Strutil::vformat (message, ap);
    va_end (ap);
}



// Wrap functions wrap 'coord' around 'width', and return true if the
// result is a valid pixel coordinate, false if black should be used
// instead.

typedef bool (*wrap_impl) (int &coord, int width);

static bool wrap_black (int &coord, int width)
{
    return (coord >= 0 && coord < width);
}


static bool wrap_clamp (int &coord, int width)
{
    if (coord < 0)
        coord = 0;
    else if (coord >= width)
        coord = width-1;
    return true;
}


static bool wrap_periodic (int &coord, int width)
{
    coord %= width;
    if (coord < 0)       // Fix negative values
        coord += width;
    return true;
}


static bool wrap_periodic2 (int &coord, int width)
{
    coord &= (width - 1);  // Shortcut periodic if we're sure it's a pow of 2
    return true;
}


static bool wrap_mirror (int &coord, int width)
{
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
    return true;
}



static const wrap_impl wrap_functions[] = {
    // Must be in same order as Wrap enum
    wrap_black, wrap_black, wrap_clamp, wrap_periodic, wrap_mirror
};



bool
TextureSystemImpl::texture (ustring filename, TextureOptions &options,
                            Runflag *runflags, int firstactive, int lastactive,
                            VaryingRef<float> s, VaryingRef<float> t,
                            VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                            VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                            float *result)
{
    static const texture_lookup_prototype lookup_functions[] = {
        // Must be in the same order as LookupMode enum
        &TextureSystemImpl::texture_lookup,
        &TextureSystemImpl::texture_lookup_closest,
        &TextureSystemImpl::texture_lookup_bilinear,
        &TextureSystemImpl::texture_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture_lookup
    };
    texture_lookup_prototype lookup = lookup_functions[(int)options.lookupmode];

    // FIXME - should we be keeping stats, times?

    TextureFileRef texturefile = find_texturefile (filename);
    if (! texturefile  ||  texturefile->broken()) {
        for (int i = firstactive;  i <= lastactive;  ++i) {
            if (runflags[i]) {
                for (int c = 0;  c < options.nchannels;  ++c)
                    result[c] = options.fill;
                if (options.alpha)
                    options.alpha[i] = options.fill;
            }
            result += options.nchannels;
        }
        error ("Texture file \"%s\" not found", filename.c_str());
        return false;
    }

    // If options indicate default wrap modes, use the ones in the file
    if (options.swrap == TextureOptions::WrapDefault)
        options.swrap = texturefile->swrap();
    if (options.twrap == TextureOptions::WrapDefault)
        options.twrap = texturefile->twrap();

    options.swrap_func = wrap_functions[(int)options.swrap];
    options.twrap_func = wrap_functions[(int)options.twrap];

    int actualchannels = Imath::clamp (texturefile->spec().nchannels - options.firstchannel, 0, options.nchannels);
    options.actualchannels = actualchannels;

    // Fill channels requested but not in the file
    if (options.actualchannels < options.nchannels) {
        for (int i = firstactive;  i <= lastactive;  ++i) {
            if (runflags[i]) {
                float fill = options.fill[i];
                for (int c = options.actualchannels; c < options.nchannels; ++c)
                    result[i*options.nchannels+c] = fill;
            }
        }
    }
    // Fill alpha if requested and it's not in the file
    if (options.alpha && options.actualchannels+1 < options.nchannels) {
        for (int i = firstactive;  i <= lastactive;  ++i)
            options.alpha[i] = options.fill[i];
        options.alpha.init (NULL);  // No need for texture_lookup to care
    }
    // Early out if all channels were beyond the highest in the file
    if (options.actualchannels < 1)
        return true;

    // FIXME - allow multiple filtered texture implementations

    // Loop over all the points that are active (as given in the
    // runflags), and for each, call texture_lookup.  The separation of
    // power here is that all possible work that can be done for all
    // "grid points" at once should be done in this function, outside
    // the loop, and all the work inside texture_lookup should be work
    // that MUST be redone for each individual texture lookup point.
    TileRef tilecache0, tilecache1;
    for (int i = firstactive;  i <= lastactive;  ++i) {
        if (runflags[i]) {
            (this->*lookup) (*texturefile, options, i,
                             s, t, dsdx, dtdx, dsdy, dtdy,
                             tilecache0, tilecache1,
                             result + i * options.nchannels);
        }
    }
    return true;
}



void
TextureSystemImpl::texture_lookup_closest (TextureFile &texturefile,
                            TextureOptions &options, int index,
                            VaryingRef<float> _s, VaryingRef<float> _t,
                            VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                            VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                            TileRef &tilecache0, TileRef &tilecache1,
                            float *result)
{
    // N.B. If any computations within this function are identical for
    // all texture lookups in this batch, those computations should be
    // hoisted up to the calling function, texture().

    // FIXME - handle mipmap levels, at least for lod bias

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    const ImageSpec &spec (texturefile.spec (0));
    float s = (floorf (_s[index] * spec.full_width)  + 0.5f) / spec.full_width;
    float t = (floorf (_t[index] * spec.full_height) + 0.5f) / spec.full_height;
    accum_sample_closest (s, t, 0, texturefile,
                          options, index, tilecache0, tilecache1,
                          1.0f, result);
}



void
TextureSystemImpl::texture_lookup_bilinear (TextureFile &texturefile,
                            TextureOptions &options, int index,
                            VaryingRef<float> _s, VaryingRef<float> _t,
                            VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                            VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                            TileRef &tilecache0, TileRef &tilecache1,
                            float *result)
{
    // N.B. If any computations within this function are identical for
    // all texture lookups in this batch, those computations should be
    // hoisted up to the calling function, texture().

    // FIXME - handle mipmap levels and lod bias

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    accum_sample_bilinear (_s[index], _t[index], 0, texturefile,
                           options, index, tilecache0, tilecache1,
                           1.0f, result);
}



void
TextureSystemImpl::texture_lookup_trilinear_mipmap (TextureFile &texturefile,
                            TextureOptions &options, int index,
                            VaryingRef<float> _s, VaryingRef<float> _t,
                            VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                            VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                            TileRef &tilecache0, TileRef &tilecache1,
                            float *result)
{
    // N.B. If any computations within this function are identical for
    // all texture lookups in this batch, those computations should be
    // hoisted up to the calling function, texture().

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    // Use the differentials to figure out which MIP-map levels to use.
    float dsdx = _dsdx ? _dsdx[index] : 0;
    dsdx = dsdx * options.swidth[index] + options.sblur[index];
    float dtdx = _dtdx ? _dtdx[index] : 0;
    dtdx = dtdx * options.twidth[index] + options.tblur[index];
    float dsdy = _dsdy ? _dsdy[index] : 0;
    dsdy = dsdy * options.swidth[index] + options.sblur[index];
    float dtdy = _dtdy ? _dtdy[index] : 0;
    dtdy = dtdy * options.twidth[index] + options.tblur[index];

    // FIXME - support lod bias

    // Determine the MIP-map level(s) we need: we will blend 
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    int miplevel[2] = { -1, -1 };
    float levelblend = 0;
    float sfilt = std::max (hypotf (dsdx, dtdx), (float)1.0e-8);
    float tfilt = std::max (hypotf (dsdy, dtdy), (float)1.0e-8);
    float filtwidth = std::max (sfilt, tfilt);
    for (int i = 0;  i < texturefile.levels();  ++i) {
        // Compute the filter size in raster space at this MIP level
        float filtwidth_ras = texturefile.spec(i).full_width * filtwidth;
        // Once the filter width is smaller than one texel at this level,
        // we've gone too far, so we know that we want to interpolate the
        // previous level and the current level.  Note that filtwidth_ras
        // is expected to be >= 0.5, or would have stopped one level ago.
        if (filtwidth_ras < 1) {
            miplevel[0] = i-1;
            miplevel[1] = i;
            levelblend = Imath::clamp (2.0f - 1.0f/filtwidth_ras, 0.0f, 1.0f);
            break;
        }
    } 
    if (miplevel[0] < 0) {
        // We wish we had even more resolution than the finest MIP level,
        // but tough for us.
        miplevel[0] = 0;
        miplevel[1] = 0;
        levelblend = 0;
        // FIXME -- we should use bicubic filtering here
    } else if (miplevel[1] < 0) {
        // We'd like to blur even more, but make due with the coarsest
        // MIP level.
        miplevel[0] = texturefile.levels() - 1;
        miplevel[1] = miplevel[0];
        levelblend = 0;
    }
    float levelweight[2] = { 1.0f - levelblend, levelblend };
//    levelblend = 0;
//    std::cerr << "Levels " << miplevel[0] << ' ' << miplevel[1] << ' ' << levelblend << "\n";

    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        accum_sample_bilinear (_s[index], _t[index], miplevel[level], texturefile,
                               options, index, tilecache0, tilecache1,
                               levelweight[level], result);
    }
}



void
TextureSystemImpl::texture_lookup (TextureFile &texturefile,
                            TextureOptions &options, int index,
                            VaryingRef<float> _s, VaryingRef<float> _t,
                            VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                            VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                            TileRef &tilecache0, TileRef &tilecache1,
                            float *result)
{
    // N.B. If any computations within this function are identical for
    // all texture lookups in this batch, those computations should be
    // hoisted up to the calling function, texture().

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    // Find the differentials, handle the case where user passed NULL
    // to indicate no derivs were available.
    float dsdx = _dsdx ? _dsdx[index] : 0;
    float dtdx = _dtdx ? _dtdx[index] : 0;
    float dsdy = _dsdy ? _dsdy[index] : 0;
    float dtdy = _dtdy ? _dtdy[index] : 0;
    // Compute the natural resolution we want for the bare derivs, this
    // will be the threshold for knowing we're maxifying (and therefore
    // wanting cubic interpolation).
    float sfilt_noblur = std::max (hypotf (dsdx, dtdx), (float)1.0e-8);
    float tfilt_noblur = std::max (hypotf (dsdy, dtdy), (float)1.0e-8);
    int naturalres = (int) (1.0f / std::min (sfilt_noblur, tfilt_noblur));
    // Scale by 'width' and 'blur'
    dsdx = dsdx * options.swidth[index] + options.sblur[index];
    dtdx = dtdx * options.twidth[index] + options.tblur[index];
    dsdy = dsdy * options.swidth[index] + options.sblur[index];
    dtdy = dtdy * options.twidth[index] + options.tblur[index];

    // FIXME - support lod bias

    // Determine the MIP-map level(s) we need: we will blend 
    //    data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
    int miplevel[2] = { -1, -1 };
    float levelblend = 0;
    float sfilt = std::max (hypotf (dsdx, dtdx), (float)1.0e-8);
    float tfilt = std::max (hypotf (dsdy, dtdy), (float)1.0e-8);
    float smajor, tmajor;
    float *majorlength, *minorlength;
    if (sfilt > tfilt) {
        majorlength = &sfilt;
        minorlength = &tfilt;
        smajor = dsdx;
        tmajor = dtdx;
    } else {
        majorlength = &tfilt;
        minorlength = &sfilt;
        smajor = dsdy;
        tmajor = dtdy;
    }
    float aspect = (*majorlength) / (*minorlength);
    const float max_aspect = 16;
    if (aspect > max_aspect) {
        aspect = max_aspect;
        *minorlength = (*majorlength) / max_aspect;
    }

    float filtwidth = (*minorlength);
    for (int i = 0;  i < texturefile.levels();  ++i) {
        // Compute the filter size in raster space at this MIP level
        float filtwidth_ras = texturefile.spec(i).full_width * filtwidth;
        // Once the filter width is smaller than one texel at this level,
        // we've gone too far, so we know that we want to interpolate the
        // previous level and the current level.  Note that filtwidth_ras
        // is expected to be >= 0.5, or would have stopped one level ago.
        if (filtwidth_ras < 1) {
            miplevel[0] = i-1;
            miplevel[1] = i;
            levelblend = Imath::clamp (2.0f - 1.0f/filtwidth_ras, 0.0f, 1.0f);
            break;
        }
    }
    if (miplevel[0] < 0) {
        // We wish we had even more resolution than the finest MIP level,
        // but tough for us.
        miplevel[0] = 0;
        miplevel[1] = 0;
        levelblend = 0;
    } else if (miplevel[1] < 0) {
        // We'd like to blur even more, but make due with the coarsest
        // MIP level.
        miplevel[0] = texturefile.levels() - 1;
        miplevel[1] = miplevel[0];
        levelblend = 0;
    }
    float levelweight[2] = { 1.0f - levelblend, levelblend };

    int nsamples = std::max (1, (int) ceilf (aspect - 0.25f));
    float invsamples = 1.0f / nsamples;

    float s = _s[index], t = _t[index];
    for (int level = 0;  level < 2;  ++level) {
        if (! levelweight[level])  // No contribution from this level, skip it
            continue;
        int lev = miplevel[level];
        float w = invsamples * levelweight[level];
        accum_prototype accumer = &TextureSystemImpl::accum_sample_bilinear;
        if (level == 0 || texturefile.spec(lev).full_height < naturalres/2)
            accumer = &TextureSystemImpl::accum_sample_bicubic;
        for (int sample = 0;  sample < nsamples;  ++sample) {
            float pos = (sample + 0.5f) * invsamples - 0.5f;
            (this->*accumer) (s + pos * smajor, t + pos * tmajor, lev, texturefile,
                        options, index, tilecache0, tilecache1, w, result);
        }
    }
}



void
TextureSystemImpl::accum_sample_closest (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 TextureOptions &options, int index,
                                 TileRef &tilecache0, TileRef &tilecache1,
                                 float weight, float *accum)
{
    const ImageSpec &spec (texturefile.spec (miplevel));
    // As passed in, (s,t) map the texture to (0,1).  Remap to [0,res]
    // and subtract 0.5 because samples are at texel centers.
    s = s * spec.width;
    t = t * spec.height;
    int stex, ttex;    // Texel coordintes
    float sfrac = floorfrac (s, &stex);
    float tfrac = floorfrac (t, &ttex);
    
    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    bool svalid, tvalid;  // Valid texels?  false means black border
    svalid = options.swrap_func (stex, spec.full_width);
    tvalid = options.twrap_func (ttex, spec.full_height);
    if (! (svalid | tvalid)) {
        // All texels we need were out of range and using 'black' wrap.
        return;
    }

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int invtilewidthmask  = ~tilewidthmask;     // e.g. 64+128+...
    int invtileheightmask = ~tileheightmask;
    const float *texel = NULL;
    static float black[4] = { 0, 0, 0, 0 };
    int tile_s = stex & tilewidthmask;
    int tile_t = ttex & tileheightmask;
    TileID id (texturefile, miplevel, stex - tile_s, ttex - tile_t, 0);
    find_tile (id, tilecache0, tilecache1);
    if (! tilecache0)
        return;
    int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s);
    texel = tilecache0->data() + offset + options.firstchannel;
    DASSERT (offset < spec.tile_pixels());
    for (int c = 0;  c < options.actualchannels;  ++c)
        accum[c] += weight * texel[c];
    if (options.alpha) {
        int c = options.actualchannels;
        options.alpha[index] += weight * texel[c];
    }
}



void
TextureSystemImpl::accum_sample_bilinear (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 TextureOptions &options, int index,
                                 TileRef &tilecache0, TileRef &tilecache1,
                                 float weight, float *accum)
{
    const ImageSpec &spec (texturefile.spec (miplevel));
    // As passed in, (s,t) map the texture to (0,1).  Remap to [0,res]
    // and subtract 0.5 because samples are at texel centers.
    s = s * spec.width  - 0.5f;
    t = t * spec.height - 0.5f;
    int sint, tint;
    float sfrac = floorfrac (s, &sint);
    float tfrac = floorfrac (t, &tint);
    // Now (xint,yint) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (xfrac,yfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).
    
    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    int stex[2], ttex[2];       // Texel coords
    stex[0] = sint;  stex[1] = sint+1;
    ttex[0] = tint;  ttex[1] = tint+1;
    bool svalid[2], tvalid[2];  // Valid texels?  false means black border
    svalid[0] = options.swrap_func (stex[0], spec.full_width);
    svalid[1] = options.swrap_func (stex[1], spec.full_width);
    tvalid[0] = options.twrap_func (ttex[0], spec.full_height);
    tvalid[1] = options.twrap_func (ttex[1], spec.full_height);
    if (! (svalid[0] | svalid[1] | tvalid[0] | tvalid[1])) {
        // All texels we need were out of range and using 'black' wrap.
        return;
    }

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int invtilewidthmask  = ~tilewidthmask;     // e.g. 64+128+...
    int invtileheightmask = ~tileheightmask;
    const float *texel[2][2] = { NULL, NULL, NULL, NULL };
    TileRef tile[2][2];
    static float black[4] = { 0, 0, 0, 0 };
    int tile_s = stex[0] & tilewidthmask;
    int tile_t = ttex[0] & tileheightmask;
    bool s_onetile = (tile_s != tilewidthmask) & (stex[0]+1 == stex[1]);
    bool t_onetile = (tile_t != tileheightmask) & (ttex[0]+1 == ttex[1]);
    bool onetile = (s_onetile & t_onetile);
    if (onetile && (svalid[0] & svalid[1] & tvalid[0] & tvalid[1])) {
        // Shortcut if all the texels we need are on the same tile
        TileID id (texturefile, miplevel,
                   stex[0] - tile_s, ttex[0] - tile_t, 0);
        find_tile (id, tilecache0, tilecache1);
        if (! tilecache0) {
            return;
        }
        int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s);
        texel[0][0] = tilecache0->data() + offset + options.firstchannel;
        texel[0][1] = texel[0][0] + spec.nchannels;
        texel[1][0] = texel[0][0] + spec.nchannels * spec.tile_width;
        texel[1][1] = texel[1][0] + spec.nchannels;
    } else {
        for (int j = 0, initialized = 0;  j < 2;  ++j) {
            for (int i = 0;  i < 2;  ++i) {
                if (! (svalid[i] && tvalid[j])) {
                    texel[j][i] = black;
                    continue;
                }
                tile_s = stex[i] & tilewidthmask;
                tile_t = ttex[j] & tileheightmask;
                TileID id (texturefile, miplevel,
                           stex[i] - tile_s, ttex[j] - tile_t, 0);
                if (0 && initialized)   // Why isn't it faster to do this?
                    find_tile_same_level (id, tilecache0, tilecache1);
                else {
                    find_tile (id, tilecache0, tilecache1);
                    initialized = true;
                }
                tile[j][i] = tilecache0;
                if (! tile[j][i]) {
                    return;
                }
                int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s);
                texel[j][i] = tile[j][i]->data() + offset + options.firstchannel;
                DASSERT (tile->id() == id);
            }
        }
    }
    // FIXME -- optimize the above loop by unrolling

    // FIXME -- handle 8 bit textures? float only for now
    DASSERT (offset < spec.tile_pixels());
    bilerp_mad (texel[0][0], texel[0][1], texel[1][0], texel[1][1],
                sfrac, tfrac, weight, options.actualchannels, accum);
    if (options.alpha) {
        int c = options.actualchannels;
        options.alpha[index] += weight * bilerp (texel[0][0][c], texel[0][1][c],
                                                 texel[1][0][c], texel[1][1][c],
                                                 sfrac, tfrac);
    }
}



void
TextureSystemImpl::accum_sample_bicubic (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 TextureOptions &options, int index,
                                 TileRef &tilecache0, TileRef &tilecache1,
                                 float weight, float *accum)
{
    const ImageSpec &spec (texturefile.spec (miplevel));
    // As passed in, (s,t) map the texture to (0,1).  Remap to [0,res]
    // and subtract 0.5 because samples are at texel centers.
    s = s * spec.width  - 0.5f;
    t = t * spec.height - 0.5f;
    int sint, tint;
    float sfrac = floorfrac (s, &sint);
    float tfrac = floorfrac (t, &tint);
    // Now (xint,yint) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (xfrac,yfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).
    
    // We're gathering 4x4 samples and 4x weights.  Indices: texels 0,
    // 1, 2, 3.  The sample lies between samples 1 and 2.

    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL);
    bool svalid[4], tvalid[4];  // Valid texels?  false means black border
    int stex[4], ttex[4];       // Texel coords
    bool allsvalid = true, alltvalid = true;
    bool anyvalid = false;
    for (int i = 0; i < 4;  ++i) {
        bool v;
        stex[i] = sint + i - 1;
        v = options.swrap_func (stex[i], spec.full_width);
        svalid[i] = v;
        allsvalid &= v;
        anyvalid |= v;
        ttex[i] = tint + i - 1;
        v = options.twrap_func (ttex[i], spec.full_height);
        tvalid[i] = v;
        alltvalid &= v;
        anyvalid |= v;
    }
    if (! anyvalid) {
        // All texels we need were out of range and using 'black' wrap.
        return;
    }

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int invtilewidthmask  = ~tilewidthmask;     // e.g. 64+128+...
    int invtileheightmask = ~tileheightmask;
    const float *texel[4][4] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    TileRef tile[4][4];
    static float black[4] = { 0, 0, 0, 0 };
    int tile_s = stex[0] & tilewidthmask;
    int tile_t = ttex[0] & tileheightmask;
    bool s_onetile = (tile_s <= tilewidthmask-3);
    bool t_onetile = (tile_t <= tileheightmask-3);
    if (s_onetile && t_onetile) {
        for (int i = 1; i < 4;  ++i) {
            s_onetile &= (stex[i] == stex[0]);
            t_onetile &= (ttex[i] == ttex[0]);
        }
    }
    bool onetile = (s_onetile & t_onetile);
    if (onetile & allsvalid & alltvalid) {
        // Shortcut if all the texels we need are on the same tile
        TileID id (texturefile, miplevel,
                   stex[0] - tile_s, ttex[0] - tile_t, 0);
        find_tile (id, tilecache0, tilecache1);
        if (! tilecache0) {
            return;
        }
        int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s);
        const float *base = tilecache0->data() + offset + options.firstchannel;
        for (int j = 0;  j < 4;  ++j)
            for (int i = 0;  i < 4;  ++i)
                texel[j][i] = base + spec.nchannels * (i + j*spec.tile_width);
    } else {
        for (int j = 0, initialized = 0;  j < 4;  ++j) {
            for (int i = 0;  i < 4;  ++i) {
                if (! (svalid[i] && tvalid[j])) {
                    texel[j][i] = black;
                    continue;
                }
                tile_s = stex[i] & tilewidthmask;
                tile_t = ttex[j] & tileheightmask;
                TileID id (texturefile, miplevel,
                           stex[i] - tile_s, ttex[j] - tile_t, 0);
                if (0 && initialized)   // Why isn't it faster to do this?
                    find_tile_same_level (id, tilecache0, tilecache1);
                else {
                    find_tile (id, tilecache0, tilecache1);
                    initialized = true;
                }
                tile[j][i] = tilecache0;
                if (! tile[j][i]) {
                    return;
                }
                DASSERT (tile->id() == id);
                int offset = spec.nchannels * (tile_t * spec.tile_width + tile_s);
                DASSERT (offset < spec.tile_pixels());
                texel[j][i] = tile[j][i]->data() + offset + options.firstchannel;
            }
        }
    }

    // Weights in x and y
    DASSERT (hq_filter);
    float wx[4] = { (*hq_filter)(-1.0f-sfrac), (*hq_filter)(-sfrac),
                    (*hq_filter)(1.0f-sfrac),  (*hq_filter)(2.0f-sfrac) };
    float wy[4] = { (*hq_filter)(-1.0f-tfrac), (*hq_filter)(-tfrac),
                    (*hq_filter)(1.0f-tfrac),  (*hq_filter)(2.0f-tfrac) };
    float w[4][4];  // 2D filter weights
    float totalw = 0;  // total filter weight
    for (int j = 0;  j < 4;  ++j) {
        for (int i = 0;  i < 4;  ++i) {
            w[j][i] = wy[j] * wx[i];
            totalw += w[j][i];
        }
    }

    // FIXME -- handle 8 bit textures? float only for now
    weight /= totalw;
    for (int j = 0;  j < 4;  ++j)
        for (int i = 0;  i < 4;  ++i) {
            for (int c = 0;  c < options.actualchannels;  ++c)
                accum[c] += (w[j][i] * weight) * texel[j][i][c];
        }
    if (options.alpha) {
        for (int j = 0;  j < 4;  ++j)
            for (int i = 0;  i < 4;  ++i) {
                int c = options.actualchannels;
                accum[c] += (w[j][i] * weight) * texel[j][i][c];
            }
    }
}


};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO
