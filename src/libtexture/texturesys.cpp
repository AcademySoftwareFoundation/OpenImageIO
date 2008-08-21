/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <string>
#include <boost/scoped_ptr.hpp>
#include <boost/tr1/memory.hpp>
using namespace std::tr1;

#include <ImathVec.h>
#include <ImathMatrix.h>
#include <half.h>

#include "dassert.h"
#include "paramtype.h"
#include "varyingref.h"
#include "ustring.h"
#include "hash.h"
#include "thread.h"
#include "fmath.h"
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
    const ImageIOFormatSpec &spec (texfile.spec());
    ParamBaseType peltype = PT_FLOAT;
    // FIXME -- read 8-bit directly if that's native
    m_texels.resize (spec.tile_pixels () * spec.nchannels * typesize(peltype));
    if (! texfile.read_tile (m_id.level(), m_id.x(), m_id.y(), m_id.z(),
                             peltype, &m_texels[0])) {
        std::cerr << "(1) error reading tile " << m_id.x() << ' ' << m_id.y() << "\n";
    }
    // FIXME -- for shadow, fill in mindepth, maxdepth
}



TextureSystemImpl::TextureSystemImpl ()
    : m_open_files(0)
{
    init ();
}



TextureSystemImpl::~TextureSystemImpl ()
{
}



void
TextureSystemImpl::init ()
{
    max_open_files (100);
    max_memory_MB (50);
    m_Mw2c.makeIdentity();
}



TileRef
TextureSystemImpl::find_tile (const TileID &id)
{
    DASSERT (id.texfile() != NULL);
    lock_guard guard (m_texturefiles_mutex);
    TileCache::iterator found = tilecache.find (id);
    TileRef tile;
    if (found != tilecache.end()) {
        tile = found->second;
    } else {
        tile.reset (new Tile (id));
        tilecache[id] = tile;
    }
    DASSERT (id == tile->id() && !memcmp(&id, &tile->id(), sizeof(TileID)));
    return tile;
}



bool
TextureSystemImpl::gettextureinfo (ustring filename, ustring dataname,
                                   ParamType datatype, void *data)
{
    TextureFileRef texfile = find_texturefile (filename);
    if (! texfile) {
        std::cerr << "   NOT FOUND\n";
        return false;
    }
    if (texfile->broken()) {
        std::cerr << "    Invalid file\n";
        return false;
    }
    const ImageIOFormatSpec &spec (texfile->spec());
    if (dataname == "resolution" && datatype==ParamType(PT_INT,2)) {
        int *d = (int *)data;
        d[0] = spec.width;
        d[1] = spec.height;
        return true;
    }
    if (dataname == "texturetype" && datatype==PT_STRING) {
        ustring s (texture_type_name (texfile->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "textureformat" && datatype==PT_STRING) {
        ustring s (texture_format_name (texfile->textureformat()));
        *(const char **)data = s.c_str();
        return true;
    }
    if (dataname == "channels" && datatype==PT_INT) {
        *(int *)data = spec.nchannels;
        return true;
    }
    if (dataname == "channels" && datatype==PT_FLOAT) {
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
            memcpy (data, p->data(), datatype.datasize());
            return true;
        }
        // If the real data is int but user asks for float, translate it
        if (p->type().basetype == PT_FLOAT && datatype.basetype == PT_INT) {
            for (int i = 0;  i < p->type().arraylen;  ++i)
                ((float *)data)[i] = ((int *)p->data())[i];
            return true;
        }
    }

    return false;
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



void
TextureSystemImpl::texture (ustring filename, TextureOptions &options,
                            Runflag *runflags, int firstactive, int lastactive,
                            VaryingRef<float> s, VaryingRef<float> t,
                            VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                            VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                            float *result)
{
    // FIXME - should we be keeping stats, times?

    TextureFileRef texturefile = find_texturefile (filename);
    if (! texturefile  ||  texturefile->broken()) {
        std::cerr << "   TEXTURE NOT FOUND " << filename << "\n";
        for (int i = firstactive;  i <= lastactive;  ++i) {
            if (runflags[i]) {
                for (int c = 0;  c < options.nchannels;  ++c)
                    result[c] = options.fill;
                if (options.alpha)
                    options.alpha[i] = options.fill;
            }
            result += options.nchannels;
        }
        return ;
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
        return;

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
            texture_lookup (*texturefile, options, i,
                            s, t, dsdx, dtdx, dsdy, dtdy,
                            tilecache0, tilecache1,
                            result + i * options.nchannels);
        }
    }
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

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    const ImageIOFormatSpec &spec (texturefile.spec (0));
    float s = (floorf (_s[index] * spec.full_width)  + 0.5f) / spec.full_width;
    float t = (floorf (_t[index] * spec.full_height) + 0.5f) / spec.full_height;
    accum_sample (s, t, 0, texturefile,
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

    // Initialize results to 0.  We'll add from here on as we sample.
    result += index * options.nchannels;
    for (int c = 0;  c < options.actualchannels;  ++c)
        result[c] = 0;
    if (options.alpha)
        options.alpha[index] = 0;

    accum_sample (_s[index], _t[index], 0, texturefile,
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
    float dsdx = _dsdx ? (_dsdx[index] * options.swidth[index] + options.sblur[index]) : 0;
    float dtdx = _dtdx ? (_dtdx[index] * options.twidth[index] + options.tblur[index]) : 0;
    float dsdy = _dsdy ? (_dsdy[index] * options.swidth[index] + options.sblur[index]) : 0;
    float dtdy = _dtdy ? (_dtdy[index] * options.twidth[index] + options.tblur[index]) : 0;

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
        accum_sample (_s[index], _t[index], miplevel[level], texturefile,
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

    // Use the differentials to figure out which MIP-map levels to use.
    float dsdx = _dsdx ? (_dsdx[index] * options.swidth[index] + options.sblur[index]) : 0;
    float dtdx = _dtdx ? (_dtdx[index] * options.twidth[index] + options.tblur[index]) : 0;
    float dsdy = _dsdy ? (_dsdy[index] * options.swidth[index] + options.sblur[index]) : 0;
    float dtdy = _dtdy ? (_dtdy[index] * options.twidth[index] + options.tblur[index]) : 0;

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
        accum_sample (_s[index], _t[index], miplevel[level], texturefile,
                      options, index, tilecache0, tilecache1,
                      levelweight[level], result);
    }
}



void
TextureSystemImpl::accum_sample (float s, float t, int miplevel,
                                 TextureFile &texturefile,
                                 TextureOptions &options, int index,
                                 TileRef &tilecache0, TileRef &tilecache1,
                                 float weight, float *accum)
{
    const ImageIOFormatSpec &spec (texturefile.spec (miplevel));
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

    // Indices: texels 0, 1, 2, 3.  The sample lies between samples 1 and 2.
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
//& invtilewidthmask) == (stex[1] & invtilewidthmask));
//        bool onetilet = ((ttex[0] & invtileheightmask) == (ttex[1] & invtileheightmask));
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


};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO
