/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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
#include "imageio.h"

#include "texture.h"

#include "imagecache.h"
#include "imagecache_pvt.h"
#include "texture_pvt.h"
#include "../field3d.imageio/field3d_pvt.h"

OIIO_NAMESPACE_ENTER
{
    using namespace pvt;

namespace {  // anonymous

static EightBitConverter<float> uchar2float;
static ustring s_field3d ("field3d");


};  // end anonymous namespace

namespace pvt {   // namespace pvt



bool
TextureSystemImpl::texture3d (ustring filename, TextureOpt &options,
                              const Imath::V3f &P,
                              const Imath::V3f &dPdx,
                              const Imath::V3f &dPdy,
                              const Imath::V3f &dPdz,
                              float *result)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texturefile = find_texturefile (filename, thread_info);
    return texture3d ((TextureHandle *)texturefile, (Perthread *)thread_info,
                      options, P, dPdx, dPdy, dPdz, result);
}



bool
TextureSystemImpl::texture3d (TextureHandle *texture_handle_,
                              Perthread *thread_info_,
                              TextureOpt &options,
                              const Imath::V3f &P,
                              const Imath::V3f &dPdx,
                              const Imath::V3f &dPdy,
                              const Imath::V3f &dPdz,
                              float *result)
{
#if 0
    // FIXME: currently, no support of actual MIPmapping.  No rush,
    // since the only volume format we currently support, Field3D,
    // doens't support MIPmapping.
    static const texture3d_lookup_prototype lookup_functions[] = {
        // Must be in the same order as Mipmode enum
        &TextureSystemImpl::texture3d_lookup,
        &TextureSystemImpl::texture3d_lookup_nomip,
        &TextureSystemImpl::texture3d_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture3d_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture3d_lookup
    };
    texture3d_lookup_prototype lookup = lookup_functions[(int)options.mipmode];
#else
    texture3d_lookup_prototype lookup = &TextureSystemImpl::texture3d_lookup_nomip;
#endif

    PerThreadInfo *thread_info = (PerThreadInfo *)thread_info_;
    TextureFile *texturefile = (TextureFile *)texture_handle_;
    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.texture3d_batches;
    ++stats.texture3d_queries;

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

    const ImageSpec &spec (texturefile->spec(options.subimage, 0));

    // Figure out the wrap functions
    if (options.swrap == TextureOpt::WrapDefault)
        options.swrap = texturefile->swrap();
    if (options.swrap == TextureOpt::WrapPeriodic && ispow2(spec.full_width))
        options.swrap_func = wrap_periodic2;
    else
        options.swrap_func = wrap_functions[(int)options.swrap];
    if (options.twrap == TextureOpt::WrapDefault)
        options.twrap = texturefile->twrap();
    if (options.twrap == TextureOpt::WrapPeriodic && ispow2(spec.full_height))
        options.twrap_func = wrap_periodic2;
    else
        options.twrap_func = wrap_functions[(int)options.twrap];
    if (options.rwrap == TextureOpt::WrapDefault)
        options.rwrap = texturefile->rwrap();
    if (options.rwrap == TextureOpt::WrapPeriodic && ispow2(spec.full_depth))
        options.rwrap_func = wrap_periodic2;
    else
        options.rwrap_func = wrap_functions[(int)options.rwrap];

    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel,
                                       0, options.nchannels);
    options.actualchannels = actualchannels;

    // Do the volume lookup in local space.  There's not actually a way
    // to ask for point transforms via the ImageInput interface, so use
    // knowledge of the few volume reader internals to the back doors.
    Imath::V3f Plocal;
    if (texturefile->fileformat() == s_field3d) {
        if (! texturefile->opened()) {
            // We need a valid ImageInput pointer below.  If the handle
            // has been invalidated, force it open again.
            texturefile->forceopen (thread_info);
        }
        Field3DInput_Interface *f3di = (Field3DInput_Interface *)texturefile->imageinput();
        ASSERT (f3di);
        f3di->worldToLocal (P, Plocal, options.time);
    } else {
        Plocal = P;
    }

    // FIXME: we don't bother with this for dPdx, dPdy, and dPdz only
    // because we know that we don't currently filter volume lookups and
    // therefore don't actually use the derivs.  If/when we do, we'll
    // need to transform them into local space as well.

    bool ok = (this->*lookup) (*texturefile, thread_info, options,
                               Plocal, dPdx, dPdy, dPdz, result);
    if (actualchannels < options.nchannels)
        fill_channels (spec.nchannels, options, result);
    return ok;


    return ok;
}



bool
TextureSystemImpl::texture3d (ustring filename, TextureOptions &options,
                              Runflag *runflags, int beginactive, int endactive,
                              VaryingRef<Imath::V3f> P,
                              VaryingRef<Imath::V3f> dPdx,
                              VaryingRef<Imath::V3f> dPdy,
                              VaryingRef<Imath::V3f> dPdz,
                              float *result)
{
    bool ok = true;
    for (int i = beginactive;  i < endactive;  ++i) {
        if (runflags[i]) {
            TextureOpt opt (options, i);
            ok &= texture3d (filename, opt, P[i], dPdx[i], dPdy[i], dPdz[i],
                             result + i*options.nchannels);
        }
    }
    return ok;
}



bool
TextureSystemImpl::texture3d_lookup_nomip (TextureFile &texturefile,
                            PerThreadInfo *thread_info, 
                            TextureOpt &options,
                            const Imath::V3f &_P, const Imath::V3f &_dPdx,
                            const Imath::V3f &_dPdy, const Imath::V3f &_dPdz,
                            float *result)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    float* dresultds = options.dresultds;
    float* dresultdt = options.dresultdt;
    float* dresultdr = options.dresultdr;
    for (int c = 0;  c < options.actualchannels;  ++c) {
        result[c] = 0;
        if (dresultds) dresultds[c] = 0;
        if (dresultdt) dresultdt[c] = 0;
        if (dresultdr) dresultdr[c] = 0;
    }
    // If the user only provided us with one pointer, clear all to simplify
    // the rest of the code, but only after we zero out the data for them so
    // they know something went wrong.
    if (!(dresultds && dresultdt && dresultdr))
        dresultds = dresultdt = dresultdr = NULL;

    static const accum3d_prototype accum_functions[] = {
        // Must be in the same order as InterpMode enum
        &TextureSystemImpl::accum3d_sample_closest,
        &TextureSystemImpl::accum3d_sample_bilinear,
        &TextureSystemImpl::accum3d_sample_bilinear, // FIXME: bicubic,
        &TextureSystemImpl::accum3d_sample_bilinear,
    };
    accum3d_prototype accumer = accum_functions[(int)options.interpmode];
    bool ok = (this->*accumer) (_P, 0, texturefile, thread_info, options,
                                1.0f, result, dresultds, dresultdt, dresultdr);

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



bool
TextureSystemImpl::accum3d_sample_closest (const Imath::V3f &P, int miplevel,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 TextureOpt &options,
                                 float weight, float *accum, float *daccumds,
                                 float *daccumdt, float *daccumdr)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords.
    float s = P[0] * spec.full_width  + spec.full_x;
    float t = P[1] * spec.full_height + spec.full_y;
    float r = P[2] * spec.full_depth + spec.full_z;
    int stex, ttex, rtex;    // Texel coordintes
    (void) floorfrac (s, &stex);   // don't need fractional result
    (void) floorfrac (t, &ttex);
    (void) floorfrac (r, &rtex);

    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL &&
             options.rwrap_func != NULL);
    bool svalid, tvalid, rvalid;  // Valid texels?  false means black border
    svalid = options.swrap_func (stex, spec.x, spec.width);
    tvalid = options.twrap_func (ttex, spec.y, spec.height);
    rvalid = options.rwrap_func (rtex, spec.z, spec.depth);
    if (! levelinfo.full_pixel_range) {
        svalid &= (stex >= spec.x && stex < (spec.x+spec.width)); // data window
        tvalid &= (ttex >= spec.y && ttex < (spec.y+spec.height));
        rvalid &= (rtex >= spec.z && rtex < (spec.z+spec.depth));
    }
    if (! (svalid & tvalid & rvalid)) {
        // All texels we need were out of range and using 'black' wrap.
        return true;
    }

    int tile_s = (stex - spec.x) % spec.tile_width;
    int tile_t = (ttex - spec.y) % spec.tile_height;
    int tile_r = (rtex - spec.z) % spec.tile_depth;
    TileID id (texturefile, options.subimage, miplevel,
               stex - tile_s, ttex - tile_t, rtex - tile_r);
    bool ok = find_tile (id, thread_info);
    if (! ok)
        error ("%s", m_imagecache->geterror().c_str());
    TileRef &tile (thread_info->tile);
    if (! tile  ||  ! ok)
        return false;
    size_t channelsize = texturefile.channelsize();
    int tilepel = (tile_r * spec.tile_height + tile_t) * spec.tile_width + tile_s;
    int offset = spec.nchannels * tilepel + options.firstchannel;
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
TextureSystemImpl::accum3d_sample_bilinear (const Imath::V3f &P, int miplevel,
                                 TextureFile &texturefile,
                                 PerThreadInfo *thread_info,
                                 TextureOpt &options,
                                 float weight, float *accum, float *daccumds,
                                 float *daccumdt, float *daccumdr)
{
    const ImageSpec &spec (texturefile.spec (options.subimage, miplevel));
    const ImageCacheFile::LevelInfo &levelinfo (texturefile.levelinfo(options.subimage,miplevel));
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords
    // and subtract 0.5 because samples are at texel centers.
    float s = P[0] * spec.full_width  + spec.full_x - 0.5f;
    float t = P[1] * spec.full_height + spec.full_y - 0.5f;
    float r = P[2] * spec.full_depth  + spec.full_z - 0.5f;
    int sint, tint, rint;
    float sfrac = floorfrac (s, &sint);
    float tfrac = floorfrac (t, &tint);
    float rfrac = floorfrac (r, &rint);
    // Now (sint,tint,rint) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (sfrac,tfrac,rfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).

    // Wrap
    DASSERT (options.swrap_func != NULL && options.twrap_func != NULL &&
             options.rwrap_func != NULL);
    int stex[2], ttex[2], rtex[2];       // Texel coords
    stex[0] = sint;  stex[1] = sint+1;
    ttex[0] = tint;  ttex[1] = tint+1;
    rtex[0] = rint;  rtex[1] = rint+1;
//    bool svalid[2], tvalid[2], rvalid[2];  // Valid texels?  false means black border
    union { bool bvalid[6]; unsigned long long ivalid; } valid_storage;
    valid_storage.ivalid = 0;
    DASSERT (sizeof(valid_storage) >= 6*sizeof(bool));
    const unsigned long long none_valid = 0;
    const unsigned long long all_valid = 0x010101010101LL;
    DASSERT (__LITTLE_ENDIAN__ && "this trick won't work with big endian");
    bool *svalid = valid_storage.bvalid;
    bool *tvalid = valid_storage.bvalid + 2;
    bool *rvalid = valid_storage.bvalid + 4;

    svalid[0] = options.swrap_func (stex[0], spec.x, spec.width);
    svalid[1] = options.swrap_func (stex[1], spec.x, spec.width);
    tvalid[0] = options.twrap_func (ttex[0], spec.y, spec.height);
    tvalid[1] = options.twrap_func (ttex[1], spec.y, spec.height);
    rvalid[0] = options.rwrap_func (rtex[0], spec.z, spec.depth);
    rvalid[1] = options.rwrap_func (rtex[1], spec.z, spec.depth);
    // Account for crop windows
    if (! levelinfo.full_pixel_range) {
        svalid[0] &= (stex[0] >= spec.x && stex[0] < spec.x+spec.width);
        svalid[1] &= (stex[1] >= spec.x && stex[1] < spec.x+spec.width);
        tvalid[0] &= (ttex[0] >= spec.y && ttex[0] < spec.y+spec.height);
        tvalid[1] &= (ttex[1] >= spec.y && ttex[1] < spec.y+spec.height);
        rvalid[0] &= (rtex[0] >= spec.z && rtex[0] < spec.z+spec.depth);
        rvalid[1] &= (rtex[1] >= spec.z && rtex[1] < spec.z+spec.depth);
    }
//    if (! (svalid[0] | svalid[1] | tvalid[0] | tvalid[1] | rvalid[0] | rvalid[1]))
    if (valid_storage.ivalid == none_valid)
        return true; // All texels we need were out of range and using 'black' wrap

    int tilewidthmask  = spec.tile_width  - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int tiledepthmask = spec.tile_depth - 1;
    const unsigned char *texel[2][2][2];
    TileRef savetile[2][2][2];
    static float black[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int tile_s = (stex[0] - spec.x) % spec.tile_width;
    int tile_t = (ttex[0] - spec.y) % spec.tile_height;
    int tile_r = (rtex[0] - spec.z) % spec.tile_depth;
    bool s_onetile = (tile_s != tilewidthmask) & (stex[0]+1 == stex[1]);
    bool t_onetile = (tile_t != tileheightmask) & (ttex[0]+1 == ttex[1]);
    bool r_onetile = (tile_r != tiledepthmask) & (rtex[0]+1 == rtex[1]);
    bool onetile = (s_onetile & t_onetile & r_onetile);
    size_t channelsize = texturefile.channelsize();
    size_t pixelsize = texturefile.pixelsize();
    if (onetile &&
        valid_storage.ivalid == all_valid) {
        // Shortcut if all the texels we need are on the same tile
        TileID id (texturefile, options.subimage, miplevel,
                   stex[0] - tile_s, ttex[0] - tile_t, rtex[0] - tile_r);
        bool ok = find_tile (id, thread_info);
        if (! ok)
            error ("%s", m_imagecache->geterror().c_str());
        TileRef &tile (thread_info->tile);
        if (! tile->valid())
            return false;
        size_t tilepel = (tile_r * spec.tile_height + tile_t) * spec.tile_width + tile_s;
        size_t offset = (spec.nchannels * tilepel + options.firstchannel) * channelsize;
        DASSERT ((size_t)offset < spec.tile_width*spec.tile_height*spec.tile_depth*pixelsize);

        const unsigned char *b = tile->bytedata() + offset;
        texel[0][0][0] = b;
        texel[0][0][1] = b + pixelsize;
        texel[0][1][0] = b + pixelsize * spec.tile_width;
        texel[0][1][1] = b + pixelsize * spec.tile_width + pixelsize;
        b += pixelsize * spec.tile_width * spec.tile_height;
        texel[1][0][0] = b;
        texel[1][0][1] = b + pixelsize;
        texel[1][1][0] = b + pixelsize * spec.tile_width;
        texel[1][1][1] = b + pixelsize * spec.tile_width + pixelsize;
    } else {
        for (int k = 0;  k < 2;  ++k) {
            for (int j = 0;  j < 2;  ++j) {
                for (int i = 0;  i < 2;  ++i) {
                    if (! (svalid[i] && tvalid[j] && rvalid[k])) {
                        texel[k][j][i] = (unsigned char *)black;
                        continue;
                    }
                    tile_s = (stex[i] - spec.x) % spec.tile_width;
                    tile_t = (ttex[j] - spec.y) % spec.tile_height;
                    tile_r = (rtex[k] - spec.z) % spec.tile_depth;
                    TileID id (texturefile, options.subimage, miplevel,
                               stex[i] - tile_s, ttex[j] - tile_t,
                               rtex[k] - tile_r);
                    bool ok = find_tile (id, thread_info);
                    if (! ok)
                        error ("%s", m_imagecache->geterror().c_str());
                    TileRef &tile (thread_info->tile);
                    if (! tile->valid())
                        return false;
                    savetile[k][j][i] = tile;
                    size_t tilepel = (tile_r * spec.tile_height + tile_t) * spec.tile_width + tile_s;
                    size_t offset = (spec.nchannels * tilepel + options.firstchannel) * channelsize;
#if DEBUG
                    if ((size_t)offset >= spec.tile_width*spec.tile_height*spec.tile_depth*pixelsize)
                        std::cerr << "offset=" << offset << ", whd " << spec.tile_width << ' ' << spec.tile_height << ' ' << spec.tile_depth << " pixsize " << pixelsize << "\n";
#endif
                    DASSERT ((size_t)offset < spec.tile_width*spec.tile_height*spec.tile_depth*pixelsize);
                    texel[k][j][i] = tile->bytedata() + offset;
                    DASSERT (tile->id() == id);
                }
            }
        }
    }
    // FIXME -- optimize the above loop by unrolling

    if (channelsize == 1) {
        // special case for 8-bit tiles
        int c;
        for (c = 0;  c < options.actualchannels;  ++c)
            accum[c] += weight * trilerp (uchar2float(texel[0][0][0][c]), uchar2float(texel[0][0][1][c]),
                                          uchar2float(texel[0][1][0][c]), uchar2float(texel[0][1][1][c]),
                                          uchar2float(texel[1][0][0][c]), uchar2float(texel[1][0][1][c]),
                                          uchar2float(texel[1][1][0][c]), uchar2float(texel[1][1][1][c]),
                                          sfrac, tfrac, rfrac);
        if (daccumds) {
            float scalex = weight * spec.full_width;
            float scaley = weight * spec.full_height;
            float scalez = weight * spec.full_depth;
            for (c = 0;  c < options.actualchannels;  ++c) {
                daccumds[c] += scalex * bilerp(
                    uchar2float(texel[0][0][1][c]) - uchar2float(texel[0][0][0][c]),
                    uchar2float(texel[0][1][1][c]) - uchar2float(texel[0][1][0][c]),

                    uchar2float(texel[1][0][1][c]) - uchar2float(texel[1][0][0][c]),
                    uchar2float(texel[1][1][1][c]) - uchar2float(texel[1][1][0][c]),
                    tfrac, rfrac
                );
                daccumdt[c] += scaley * bilerp(
                    uchar2float(texel[0][1][0][c]) - uchar2float(texel[0][0][0][c]),
                    uchar2float(texel[0][1][1][c]) - uchar2float(texel[0][0][1][c]),
                    uchar2float(texel[1][1][0][c]) - uchar2float(texel[1][0][0][c]),
                    uchar2float(texel[1][1][1][c]) - uchar2float(texel[1][0][1][c]),
                    sfrac, rfrac
                );
                daccumdr[c] += scalez * bilerp(
                    uchar2float(texel[0][1][0][c]) - uchar2float(texel[1][1][0][c]),
                    uchar2float(texel[0][1][1][c]) - uchar2float(texel[1][1][1][c]),
                    uchar2float(texel[0][0][1][c]) - uchar2float(texel[1][0][0][c]),
                    uchar2float(texel[0][1][1][c]) - uchar2float(texel[1][1][1][c]),
                    sfrac, tfrac
                );
            }
        }
    } else {
        // General case for float tiles
        trilerp_mad ((const float *)texel[0][0][0], (const float *)texel[0][0][1],
                     (const float *)texel[0][1][0], (const float *)texel[0][1][1],
                     (const float *)texel[1][0][0], (const float *)texel[1][0][1],
                     (const float *)texel[1][1][0], (const float *)texel[1][1][1],
                     sfrac, tfrac, rfrac, weight, options.actualchannels, accum);
        if (daccumds) {
            float scalex = weight * spec.full_width;
            float scaley = weight * spec.full_height;
            float scalez = weight * spec.full_depth;
            for (int c = 0;  c < options.actualchannels;  ++c) {
                daccumds[c] += scalex * bilerp(
                    ((const float *) texel[0][0][1])[c] - ((const float *) texel[0][0][0])[c],
                    ((const float *) texel[0][1][1])[c] - ((const float *) texel[0][1][0])[c],
                    ((const float *) texel[1][0][1])[c] - ((const float *) texel[1][0][0])[c],
                    ((const float *) texel[1][1][1])[c] - ((const float *) texel[1][1][0])[c],
                    tfrac, rfrac
                );
                daccumdt[c] += scaley * bilerp(
                    ((const float *) texel[0][1][0])[c] - ((const float *) texel[0][0][0])[c],
                    ((const float *) texel[0][1][1])[c] - ((const float *) texel[0][0][1])[c],
                    ((const float *) texel[1][1][0])[c] - ((const float *) texel[1][0][0])[c],
                    ((const float *) texel[1][1][1])[c] - ((const float *) texel[1][0][1])[c],
                    sfrac, rfrac
                );
                daccumdr[c] += scalez * bilerp(
                    ((const float *) texel[0][1][0])[c] - ((const float *) texel[1][1][0])[c],
                    ((const float *) texel[0][1][1])[c] - ((const float *) texel[1][1][1])[c],
                    ((const float *) texel[0][0][1])[c] - ((const float *) texel[1][0][0])[c],
                    ((const float *) texel[0][1][1])[c] - ((const float *) texel[1][1][1])[c],
                    sfrac, tfrac
                );
            }
        }
    }

    return true;
}



};  // end namespace pvt

}
OIIO_NAMESPACE_EXIT
