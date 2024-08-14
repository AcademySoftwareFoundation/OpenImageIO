// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <list>
#include <sstream>
#include <string>

#include <OpenImageIO/Imath.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>

#include "imagecache_pvt.h"
#include "texture_pvt.h"

OIIO_NAMESPACE_BEGIN
using namespace pvt;

namespace {  // anonymous

OIIO_FORCEINLINE float
ushort2float(unsigned short val)
{
    return float(val) * (1.0f / 65535.0f);
}

OIIO_FORCEINLINE float
half2float(half val)
{
    return val;
}

OIIO_FORCEINLINE float
float2float(float val)
{
    return val;
}

}  // end anonymous namespace

bool
TextureSystem::texture3d(ustring filename, TextureOpt& options, V3fParam P,
                         V3fParam dPdx, V3fParam dPdy, V3fParam dPdz,
                         int nchannels, float* result, float* dresultds,
                         float* dresultdt, float* dresultdr)
{
    return m_impl->texture3d(filename, options, P, dPdx, dPdy, dPdz, nchannels,
                             result, dresultds, dresultdt, dresultdr);
}


bool
TextureSystem::texture3d(TextureHandle* texture_handle, Perthread* thread_info,
                         TextureOpt& options, V3fParam P, V3fParam dPdx,
                         V3fParam dPdy, V3fParam dPdz, int nchannels,
                         float* result, float* dresultds, float* dresultdt,
                         float* dresultdr)
{
    return m_impl->texture3d(texture_handle, thread_info, options, P, dPdx,
                             dPdy, dPdz, nchannels, result, dresultds,
                             dresultdt, dresultdr);
}


bool
TextureSystem::texture3d(ustring filename, TextureOptBatch& options,
                         Tex::RunMask mask, const float* P, const float* dPdx,
                         const float* dPdy, const float* dPdz, int nchannels,
                         float* result, float* dresultds, float* dresultdt,
                         float* dresultdr)
{
    return m_impl->texture3d(filename, options, mask, P, dPdx, dPdy, dPdz,
                             nchannels, result, dresultds, dresultdt,
                             dresultdr);
}


bool
TextureSystem::texture3d(TextureHandle* texture_handle, Perthread* thread_info,
                         TextureOptBatch& options, Tex::RunMask mask,
                         const float* P, const float* dPdx, const float* dPdy,
                         const float* dPdz, int nchannels, float* result,
                         float* dresultds, float* dresultdt, float* dresultdr)
{
    return m_impl->texture3d(texture_handle, thread_info, options, mask, P,
                             dPdx, dPdy, dPdz, nchannels, result, dresultds,
                             dresultdt, dresultdr);
}



bool
TextureSystemImpl::texture3d(ustring filename, TextureOpt& options, V3fParam P,
                             V3fParam dPdx, V3fParam dPdy, V3fParam dPdz,
                             int nchannels, float* result, float* dresultds,
                             float* dresultdt, float* dresultdr)
{
    PerThreadInfo* thread_info = m_imagecache->get_perthread_info();
    TextureFile* texturefile   = find_texturefile(filename, thread_info);
    return texture3d((TextureHandle*)texturefile, (Perthread*)thread_info,
                     options, P, dPdx, dPdy, dPdz, nchannels, result, dresultds,
                     dresultdt, dresultdr);
}



bool
TextureSystemImpl::texture3d(TextureHandle* texture_handle_,
                             Perthread* thread_info_, TextureOpt& options,
                             V3fParam P, V3fParam dPdx, V3fParam dPdy,
                             V3fParam dPdz, int nchannels, float* result,
                             float* dresultds, float* dresultdt,
                             float* dresultdr)
{
#if 0
    // Handle >4 channel lookups by recursion.
    if (nchannels > 4) {
        int save_firstchannel = options.firstchannel;
        while (nchannels) {
            int n   = std::min(nchannels, 4);
            bool ok = texture3d(texture_handle_, thread_info_, options, P, dPdx,
                                dPdy, dPdz, n, result, dresultds, dresultdt,
                                dresultdr);
            if (!ok)
                return false;
            result += n;
            if (dresultds)
                dresultds += n;
            if (dresultdt)
                dresultdt += n;
            if (dresultdr)
                dresultdr += n;
            options.firstchannel += n;
            nchannels -= n;
        }
        options.firstchannel = save_firstchannel;  // restore what we changed
        return true;
    }
#endif

#if 0
    // FIXME: currently, no support of actual MIPmapping.
    static const texture3d_lookup_prototype lookup_functions[] = {
        // Must be in the same order as Mipmode enum
        &TextureSystemImpl::texture3d_lookup,
        &TextureSystemImpl::texture3d_lookup_nomip,
        &TextureSystemImpl::texture3d_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture3d_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture3d_lookup,
        &TextureSystemImpl::texture3d_lookup_trilinear_mipmap,
        &TextureSystemImpl::texture3d_lookup
    };
    texture3d_lookup_prototype lookup = lookup_functions[(int)options.mipmode];
#else
    texture3d_lookup_prototype lookup
        = &TextureSystemImpl::texture3d_lookup_nomip;
#endif

    PerThreadInfo* thread_info = m_imagecache->get_perthread_info(
        (PerThreadInfo*)thread_info_);
    TextureFile* texturefile = verify_texturefile((TextureFile*)texture_handle_,
                                                  thread_info);
    ImageCacheStatistics& stats(thread_info->m_stats);
    ++stats.texture3d_batches;
    ++stats.texture3d_queries;

    if (!texturefile || texturefile->broken())
        return missing_texture(options, nchannels, result, dresultds, dresultdt,
                               dresultdr);

    if (!options.subimagename.empty()) {
        // If subimage was specified by name, figure out its index.
        int s = m_imagecache->subimage_from_name(texturefile,
                                                 options.subimagename);
        if (s < 0) {
            error("Unknown subimage \"{}\" in texture \"{}\"",
                  options.subimagename, texturefile->filename());
            return missing_texture(options, nchannels, result, dresultds,
                                   dresultdt, dresultdr);
        }
        options.subimage = s;
        options.subimagename.clear();
    }
    if (options.subimage < 0 || options.subimage >= texturefile->subimages()) {
        error("Unknown subimage \"{}\" in texture \"{}\"", options.subimagename,
              texturefile->filename());
        return missing_texture(options, nchannels, result, dresultds, dresultdt,
                               dresultdr);
    }

    const ImageSpec& spec(texturefile->spec(options.subimage, 0));

    // Figure out the wrap functions
    if (options.swrap == TextureOpt::WrapDefault)
        options.swrap = (TextureOpt::Wrap)texturefile->swrap();
    if (options.swrap == TextureOpt::WrapPeriodic && ispow2(spec.width))
        options.swrap = TextureOpt::WrapPeriodicPow2;
    if (options.twrap == TextureOpt::WrapDefault)
        options.twrap = (TextureOpt::Wrap)texturefile->twrap();
    if (options.twrap == TextureOpt::WrapPeriodic && ispow2(spec.height))
        options.twrap = TextureOpt::WrapPeriodicPow2;
    if (options.rwrap == TextureOpt::WrapDefault)
        options.rwrap = (TextureOpt::Wrap)texturefile->rwrap();
    if (options.rwrap == TextureOpt::WrapPeriodic && ispow2(spec.depth))
        options.rwrap = TextureOpt::WrapPeriodicPow2;

    int actualchannels = OIIO::clamp(spec.nchannels - options.firstchannel, 0,
                                     nchannels);

    // Do the volume lookup in local space.
    Imath::V3f Plocal;
    const auto& si(texturefile->subimageinfo(options.subimage));
    if (si.Mlocal) {
        // See if there is a world-to-local transform stored in the cache
        // entry. If so, use it to transform the input point.
        si.Mlocal->multVecMatrix(P.cast<Imath::V3f>(), Plocal);
    } else {
        // If no world-to-local matrix could be discerned, just use the
        // input point directly.
        Plocal = P.cast<Imath::V3f>();
    }

    // FIXME: we don't bother with this for dPdx, dPdy, and dPdz only
    // because we know that we don't currently filter volume lookups and
    // therefore don't actually use the derivs.  If/when we do, we'll
    // need to transform them into local space as well.

    bool ok = (this->*lookup)(*texturefile, thread_info, options, nchannels,
                              actualchannels, Plocal, dPdx, dPdy, dPdz, result,
                              dresultds, dresultdt, dresultdr);

    if (actualchannels < nchannels && options.firstchannel == 0
        && m_gray_to_rgb)
        fill_gray_channels(spec, nchannels, result, dresultds, dresultdt,
                           dresultdr);
    return ok;
}



bool
TextureSystemImpl::texture3d_lookup_nomip(
    TextureFile& texturefile, PerThreadInfo* thread_info, TextureOpt& options,
    int nchannels_result, int actualchannels, const Imath::V3f& P,
    const Imath::V3f& /*dPdx*/, const Imath::V3f& /*dPdy*/,
    const Imath::V3f& /*dPdz*/, float* result, float* dresultds,
    float* dresultdt, float* dresultdr)
{
    // Initialize results to 0.  We'll add from here on as we sample.
    for (int c = 0; c < nchannels_result; ++c)
        result[c] = 0;
    if (dresultds) {
        OIIO_DASSERT(dresultdt && dresultdr);
        for (int c = 0; c < nchannels_result; ++c)
            dresultds[c] = 0;
        for (int c = 0; c < nchannels_result; ++c)
            dresultdt[c] = 0;
        for (int c = 0; c < nchannels_result; ++c)
            dresultdr[c] = 0;
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
        &TextureSystemImpl::accum3d_sample_bilinear,  // FIXME: bicubic,
        &TextureSystemImpl::accum3d_sample_bilinear,
    };
    accum3d_prototype accumer = accum_functions[(int)options.interpmode];
    bool ok = (this->*accumer)(P, 0, texturefile, thread_info, options,
                               nchannels_result, actualchannels, 1.0f, result,
                               dresultds, dresultdt, dresultdr);

    // Update stats
    ImageCacheStatistics& stats(thread_info->m_stats);
    ++stats.aniso_queries;
    ++stats.aniso_probes;
    switch (options.interpmode) {
    case TextureOpt::InterpClosest: ++stats.closest_interps; break;
    case TextureOpt::InterpBilinear: ++stats.bilinear_interps; break;
    case TextureOpt::InterpBicubic: ++stats.cubic_interps; break;
    case TextureOpt::InterpSmartBicubic: ++stats.bilinear_interps; break;
    }
    return ok;
}



bool
TextureSystemImpl::accum3d_sample_closest(
    const Imath::V3f& P, int miplevel, TextureFile& texturefile,
    PerThreadInfo* thread_info, TextureOpt& options, int nchannels_result,
    int actualchannels, float weight, float* accum, float* daccumds,
    float* daccumdt, float* daccumdr)
{
    const ImageSpec& spec(texturefile.spec(options.subimage, miplevel));
    const ImageCacheFile::LevelInfo& levelinfo(
        texturefile.levelinfo(options.subimage, miplevel));
    TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(options.subimage);
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords.
    float s = P.x * spec.full_width + spec.full_x;
    float t = P.y * spec.full_height + spec.full_y;
    float r = P.z * spec.full_depth + spec.full_z;
    int stex, ttex, rtex;       // Texel coordinates
    (void)floorfrac(s, &stex);  // don't need fractional result
    (void)floorfrac(t, &ttex);
    (void)floorfrac(r, &rtex);

    wrap_impl swrap_func = wrap_functions[(int)options.swrap];
    wrap_impl twrap_func = wrap_functions[(int)options.twrap];
    wrap_impl rwrap_func = wrap_functions[(int)options.rwrap];
    bool svalid, tvalid, rvalid;  // Valid texels?  false means black border
    svalid = swrap_func(stex, spec.x, spec.width);
    tvalid = twrap_func(ttex, spec.y, spec.height);
    rvalid = rwrap_func(rtex, spec.z, spec.depth);
    if (!levelinfo.full_pixel_range) {
        svalid &= (stex >= spec.x
                   && stex < (spec.x + spec.width));  // data window
        tvalid &= (ttex >= spec.y && ttex < (spec.y + spec.height));
        rvalid &= (rtex >= spec.z && rtex < (spec.z + spec.depth));
    }
    if (!(svalid & tvalid & rvalid)) {
        // All texels we need were out of range and using 'black' wrap.
        return true;
    }

    int tile_chbegin = 0, tile_chend = spec.nchannels;
    if (spec.nchannels > m_max_tile_channels) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = options.firstchannel;
        tile_chend   = options.firstchannel + actualchannels;
    }
    int tile_s = (stex - spec.x) % spec.tile_width;
    int tile_t = (ttex - spec.y) % spec.tile_height;
    int tile_r = (rtex - spec.z) % spec.tile_depth;
    TileID id(texturefile, options.subimage, miplevel, stex - tile_s,
              ttex - tile_t, rtex - tile_r, tile_chbegin, tile_chend,
              options.colortransformid);
    bool ok = find_tile(id, thread_info, true);
    if (!ok)
        error("{}", m_imagecache->geterror());
    TileRef& tile(thread_info->tile);
    if (!tile || !ok)
        return false;
    imagesize_t tilepel = (tile_r * spec.tile_height + imagesize_t(tile_t))
                              * spec.tile_width
                          + tile_s;
    int startchan_in_tile = options.firstchannel - id.chbegin();
    imagesize_t offset    = spec.nchannels * tilepel + startchan_in_tile;
    OIIO_DASSERT((size_t)offset < spec.nchannels * spec.tile_pixels());
    if (pixeltype == TypeDesc::UINT8) {
        const unsigned char* texel = tile->bytedata() + offset;
        for (int c = 0; c < actualchannels; ++c)
            accum[c] += weight * uchar2float(texel[c]);
    } else if (pixeltype == TypeDesc::UINT16) {
        const unsigned short* texel = tile->ushortdata() + offset;
        for (int c = 0; c < actualchannels; ++c)
            accum[c] += weight * ushort2float(texel[c]);
    } else if (pixeltype == TypeDesc::HALF) {
        const half* texel = tile->halfdata() + offset;
        for (int c = 0; c < actualchannels; ++c)
            accum[c] += weight * float(texel[c]);
    } else {
        OIIO_DASSERT(pixeltype == TypeDesc::FLOAT);
        const float* texel = tile->floatdata() + offset;
        for (int c = 0; c < actualchannels; ++c)
            accum[c] += weight * texel[c];
    }

    // Add appropriate amount of "fill" color to extra channels in
    // non-"black"-wrapped regions.
    if (nchannels_result > actualchannels && options.fill) {
        float f = weight * options.fill;
        for (int c = actualchannels; c < nchannels_result; ++c)
            accum[c] += f;
        if (OIIO_UNLIKELY(daccumds)) {
            OIIO_DASSERT(daccumdt && daccumdr);
            for (int c = actualchannels; c < nchannels_result; ++c) {
                daccumds[c] = 0.0f;
                daccumdt[c] = 0.0f;
                daccumdr[c] = 0.0f;
            }
        }
    }
    return true;
}



template<class T, class Converter = void>
void
trilerp_accum(float* accum, float* daccumds, float* daccumdt, float* daccumdr,
              const unsigned char* texel[2][2][2], float sfrac, float tfrac,
              float rfrac, int actualchannels, float weight,
              const ImageSpec& spec, const Converter& convert)
{
    for (int c = 0; c < actualchannels; ++c) {
        accum[c] += weight
                    * trilerp(convert(((const T*)texel[0][0][0])[c]),
                              convert(((const T*)texel[0][0][1])[c]),
                              convert(((const T*)texel[0][1][0])[c]),
                              convert(((const T*)texel[0][1][1])[c]),
                              convert(((const T*)texel[1][0][0])[c]),
                              convert(((const T*)texel[1][0][1])[c]),
                              convert(((const T*)texel[1][1][0])[c]),
                              convert(((const T*)texel[1][1][1])[c]), sfrac,
                              tfrac, rfrac);
    }
    if (daccumds) {
        float scalex = weight * spec.full_width;
        float scaley = weight * spec.full_height;
        float scalez = weight * spec.full_depth;
        for (int c = 0; c < actualchannels; ++c) {
            daccumds[c]
                += scalex
                   * bilerp(convert(((const T*)texel[0][0][1])[c])
                                - convert(((const T*)texel[0][0][0])[c]),
                            convert(((const T*)texel[0][1][1])[c])
                                - convert(((const T*)texel[0][1][0])[c]),
                            convert(((const T*)texel[1][0][1])[c])
                                - convert(((const T*)texel[1][0][0])[c]),
                            convert(((const T*)texel[1][1][1])[c])
                                - convert(((const T*)texel[1][1][0])[c]),
                            tfrac, rfrac);
            daccumdt[c]
                += scaley
                   * bilerp(convert(((const T*)texel[0][1][0])[c])
                                - convert(((const T*)texel[0][0][0])[c]),
                            convert(((const T*)texel[0][1][1])[c])
                                - convert(((const T*)texel[0][0][1])[c]),
                            convert(((const T*)texel[1][1][0])[c])
                                - convert(((const T*)texel[1][0][0])[c]),
                            convert(((const T*)texel[1][1][1])[c])
                                - convert(((const T*)texel[1][0][1])[c]),
                            sfrac, rfrac);
            daccumdr[c]
                += scalez
                   * bilerp(convert(((const T*)texel[0][1][0])[c])
                                - convert(((const T*)texel[1][1][0])[c]),
                            convert(((const T*)texel[0][1][1])[c])
                                - convert(((const T*)texel[1][1][1])[c]),
                            convert(((const T*)texel[0][0][1])[c])
                                - convert(((const T*)texel[1][0][0])[c]),
                            convert(((const T*)texel[0][1][1])[c])
                                - convert(((const T*)texel[1][1][1])[c]),
                            sfrac, tfrac);
        }
    }
}



bool
TextureSystemImpl::accum3d_sample_bilinear(
    const Imath::V3f& P, int miplevel, TextureFile& texturefile,
    PerThreadInfo* thread_info, TextureOpt& options, int nchannels_result,
    int actualchannels, float weight, float* accum, float* daccumds,
    float* daccumdt, float* daccumdr)
{
    const ImageSpec& spec(texturefile.spec(options.subimage, miplevel));
    const ImageCacheFile::LevelInfo& levelinfo(
        texturefile.levelinfo(options.subimage, miplevel));
    TypeDesc::BASETYPE pixeltype = texturefile.pixeltype(options.subimage);
    // As passed in, (s,t) map the texture to (0,1).  Remap to texel coords
    // and subtract 0.5 because samples are at texel centers.
    float s = P.x * spec.full_width + spec.full_x - 0.5f;
    float t = P.y * spec.full_height + spec.full_y - 0.5f;
    float r = P.z * spec.full_depth + spec.full_z - 0.5f;
    int sint, tint, rint;
    float sfrac = floorfrac(s, &sint);
    float tfrac = floorfrac(t, &tint);
    float rfrac = floorfrac(r, &rint);
    // Now (sint,tint,rint) are the integer coordinates of the texel to the
    // immediate "upper left" of the lookup point, and (sfrac,tfrac,rfrac) are
    // the amount that the lookup point is actually offset from the
    // texel center (with (1,1) being all the way to the next texel down
    // and to the right).

    // Wrap
    wrap_impl swrap_func = wrap_functions[(int)options.swrap];
    wrap_impl twrap_func = wrap_functions[(int)options.twrap];
    wrap_impl rwrap_func = wrap_functions[(int)options.rwrap];

    int stex[2], ttex[2], rtex[2];  // Texel coords
    stex[0] = sint;
    stex[1] = sint + 1;
    ttex[0] = tint;
    ttex[1] = tint + 1;
    rtex[0] = rint;
    rtex[1] = rint + 1;
    //    bool svalid[2], tvalid[2], rvalid[2];  // Valid texels?  false means black border
    union {
        bool bvalid[6];
        unsigned long long ivalid;
    } valid_storage;
    valid_storage.ivalid = 0;
    OIIO_DASSERT(sizeof(valid_storage) == 8);
    const unsigned long long none_valid = 0;
    const unsigned long long all_valid  = littleendian() ? 0x010101010101LL
                                                         : 0x01010101010100LL;

    bool* svalid = valid_storage.bvalid;
    bool* tvalid = valid_storage.bvalid + 2;
    bool* rvalid = valid_storage.bvalid + 4;

    svalid[0] = swrap_func(stex[0], spec.x, spec.width);
    svalid[1] = swrap_func(stex[1], spec.x, spec.width);
    tvalid[0] = twrap_func(ttex[0], spec.y, spec.height);
    tvalid[1] = twrap_func(ttex[1], spec.y, spec.height);
    rvalid[0] = rwrap_func(rtex[0], spec.z, spec.depth);
    rvalid[1] = rwrap_func(rtex[1], spec.z, spec.depth);
    // Account for crop windows
    if (!levelinfo.full_pixel_range) {
        svalid[0] &= (stex[0] >= spec.x && stex[0] < spec.x + spec.width);
        svalid[1] &= (stex[1] >= spec.x && stex[1] < spec.x + spec.width);
        tvalid[0] &= (ttex[0] >= spec.y && ttex[0] < spec.y + spec.height);
        tvalid[1] &= (ttex[1] >= spec.y && ttex[1] < spec.y + spec.height);
        rvalid[0] &= (rtex[0] >= spec.z && rtex[0] < spec.z + spec.depth);
        rvalid[1] &= (rtex[1] >= spec.z && rtex[1] < spec.z + spec.depth);
    }
    //    if (! (svalid[0] | svalid[1] | tvalid[0] | tvalid[1] | rvalid[0] | rvalid[1]))
    if (valid_storage.ivalid == none_valid)
        return true;  // All texels we need were out of range and using 'black' wrap

    if (nchannels_result > actualchannels && options.fill) {
        // Add appropriate amount of "fill" color to extra channels in
        // non-"black"-wrapped regions.
        float f = trilerp(1.0f * (rvalid[0] * tvalid[0] * svalid[0]),
                          1.0f * (rvalid[0] * tvalid[0] * svalid[1]),
                          1.0f * (rvalid[0] * tvalid[1] * svalid[0]),
                          1.0f * (rvalid[0] * tvalid[1] * svalid[1]),
                          1.0f * (rvalid[1] * tvalid[0] * svalid[0]),
                          1.0f * (rvalid[1] * tvalid[0] * svalid[1]),
                          1.0f * (rvalid[1] * tvalid[1] * svalid[0]),
                          1.0f * (rvalid[1] * tvalid[1] * svalid[1]), sfrac,
                          tfrac, rfrac);
        f *= weight * options.fill;
        for (int c = actualchannels; c < nchannels_result; ++c)
            accum[c] += f;
    }
    if (actualchannels < 0) {
        // nothing more to do here
        return true;
    }

    int tilewidthmask  = spec.tile_width - 1;  // e.g. 63
    int tileheightmask = spec.tile_height - 1;
    int tiledepthmask  = spec.tile_depth - 1;
    const unsigned char* texel[2][2][2];
    TileRef savetile[2][2][2];
    static float black[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int tile_s            = (stex[0] - spec.x) % spec.tile_width;
    int tile_t            = (ttex[0] - spec.y) % spec.tile_height;
    int tile_r            = (rtex[0] - spec.z) % spec.tile_depth;
    bool s_onetile     = (tile_s != tilewidthmask) & (stex[0] + 1 == stex[1]);
    bool t_onetile     = (tile_t != tileheightmask) & (ttex[0] + 1 == ttex[1]);
    bool r_onetile     = (tile_r != tiledepthmask) & (rtex[0] + 1 == rtex[1]);
    bool onetile       = (s_onetile & t_onetile & r_onetile);
    size_t channelsize = texturefile.channelsize(options.subimage);
    size_t pixelsize   = texturefile.pixelsize(options.subimage);
    int tile_chbegin = 0, tile_chend = spec.nchannels;
    if (spec.nchannels > m_max_tile_channels) {
        // For files with many channels, narrow the range we cache
        tile_chbegin = options.firstchannel;
        tile_chend   = options.firstchannel + actualchannels;
    }
    TileID id(texturefile, options.subimage, miplevel, 0, 0, 0, tile_chbegin,
              tile_chend, options.colortransformid);
    int startchan_in_tile = options.firstchannel - id.chbegin();

    if (onetile && valid_storage.ivalid == all_valid) {
        // Shortcut if all the texels we need are on the same tile
        id.xyz(stex[0] - tile_s, ttex[0] - tile_t, rtex[0] - tile_r);
        bool ok = find_tile(id, thread_info, true);
        if (!ok)
            error("{}", m_imagecache->geterror());
        TileRef& tile(thread_info->tile);
        if (!tile->valid())
            return false;
        imagesize_t tilepel = (tile_r * spec.tile_height + imagesize_t(tile_t))
                                  * spec.tile_width
                              + tile_s;
        imagesize_t offset = (spec.nchannels * tilepel + startchan_in_tile)
                             * channelsize;
        OIIO_DASSERT(offset < spec.tile_bytes());

        const unsigned char* b = tile->bytedata() + offset;
        texel[0][0][0]         = b;
        texel[0][0][1]         = b + pixelsize;
        texel[0][1][0]         = b + pixelsize * spec.tile_width;
        texel[0][1][1]         = b + pixelsize * spec.tile_width + pixelsize;
        b += pixelsize * spec.tile_width * spec.tile_height;
        texel[1][0][0] = b;
        texel[1][0][1] = b + pixelsize;
        texel[1][1][0] = b + pixelsize * spec.tile_width;
        texel[1][1][1] = b + pixelsize * spec.tile_width + pixelsize;
    } else {
        bool firstsample = true;
        for (int k = 0; k < 2; ++k) {
            for (int j = 0; j < 2; ++j) {
                for (int i = 0; i < 2; ++i) {
                    if (!(svalid[i] && tvalid[j] && rvalid[k])) {
                        texel[k][j][i] = (unsigned char*)black;
                        continue;
                    }
                    tile_s = (stex[i] - spec.x) % spec.tile_width;
                    tile_t = (ttex[j] - spec.y) % spec.tile_height;
                    tile_r = (rtex[k] - spec.z) % spec.tile_depth;
                    id.xyz(stex[i] - tile_s, ttex[j] - tile_t,
                           rtex[k] - tile_r);
                    bool ok = find_tile(id, thread_info, firstsample);
                    if (!ok)
                        error("{}", m_imagecache->geterror());
                    firstsample = false;
                    TileRef& tile(thread_info->tile);
                    if (!tile->valid())
                        return false;
                    savetile[k][j][i]   = tile;
                    imagesize_t tilepel = (tile_r * spec.tile_height
                                           + imagesize_t(tile_t))
                                              * spec.tile_width
                                          + tile_s;
                    imagesize_t offset = (spec.nchannels * tilepel
                                          + startchan_in_tile)
                                         * channelsize;
#ifndef NDEBUG
                    if (offset >= spec.tile_bytes())
                        std::cerr << "offset=" << offset << ", whd "
                                  << spec.tile_width << ' ' << spec.tile_height
                                  << ' ' << spec.tile_depth << " pixsize "
                                  << pixelsize << "\n";
#endif
                    OIIO_DASSERT((size_t)offset < spec.tile_bytes());
                    texel[k][j][i] = tile->bytedata() + offset;
                    OIIO_DASSERT(tile->id() == id);
                }
            }
        }
    }
    // FIXME -- optimize the above loop by unrolling

    if (pixeltype == TypeDesc::UINT8) {
        trilerp_accum<uint8_t>(accum, daccumds, daccumdt, daccumdr, texel,
                               sfrac, tfrac, rfrac, actualchannels, weight,
                               spec, uchar2float);
    } else if (pixeltype == TypeDesc::UINT16) {
        trilerp_accum<uint16_t>(accum, daccumds, daccumdt, daccumdr, texel,
                                sfrac, tfrac, rfrac, actualchannels, weight,
                                spec, ushort2float);
    } else if (pixeltype == TypeDesc::HALF) {
        trilerp_accum<half>(accum, daccumds, daccumdt, daccumdr, texel, sfrac,
                            tfrac, rfrac, actualchannels, weight, spec,
                            half2float);
    } else {
        // General case for float tiles
        trilerp_accum<float>(accum, daccumds, daccumdt, daccumdr, texel, sfrac,
                             tfrac, rfrac, actualchannels, weight, spec,
                             float2float);
    }

    return true;
}



bool
TextureSystemImpl::texture3d(TextureHandle* texture_handle,
                             Perthread* thread_info, TextureOptBatch& options,
                             Tex::RunMask mask, const float* P,
                             const float* dPdx, const float* dPdy,
                             const float* dPdz, int nchannels, float* result,
                             float* dresultds, float* dresultdt,
                             float* dresultdr)
{
    // (FIXME) CHEAT! Texture points individually
    TextureOpt opt;
    opt.firstchannel        = options.firstchannel;
    opt.subimage            = options.subimage;
    opt.subimagename        = options.subimagename;
    opt.swrap               = (TextureOpt::Wrap)options.swrap;
    opt.twrap               = (TextureOpt::Wrap)options.twrap;
    opt.mipmode             = (TextureOpt::MipMode)options.mipmode;
    opt.interpmode          = (TextureOpt::InterpMode)options.interpmode;
    opt.anisotropic         = options.anisotropic;
    opt.conservative_filter = options.conservative_filter;
    opt.fill                = options.fill;
    opt.missingcolor        = options.missingcolor;
    opt.rwrap               = (TextureOpt::Wrap)options.rwrap;

    bool ok          = true;
    Tex::RunMask bit = 1;
    float* r         = OIIO_ALLOCA(float, 4 * nchannels * Tex::BatchWidth);
    float* drds      = r + 1 * nchannels * Tex::BatchWidth;
    float* drdt      = r + 2 * nchannels * Tex::BatchWidth;
    float* drdr      = r + 3 * nchannels * Tex::BatchWidth;
    for (int i = 0; i < Tex::BatchWidth; ++i, bit <<= 1) {
        if (mask & bit) {
            opt.sblur  = options.sblur[i];
            opt.tblur  = options.tblur[i];
            opt.rblur  = options.rblur[i];
            opt.swidth = options.swidth[i];
            opt.twidth = options.twidth[i];
            opt.rwidth = options.rwidth[i];
            Imath::V3f P_(P[i], P[i + Tex::BatchWidth],
                          P[i + 2 * Tex::BatchWidth]);
            Imath::V3f dPdx_(dPdx[i], dPdx[i + Tex::BatchWidth],
                             dPdx[i + 2 * Tex::BatchWidth]);
            Imath::V3f dPdy_(dPdy[i], dPdy[i + Tex::BatchWidth],
                             dPdy[i + 2 * Tex::BatchWidth]);
            Imath::V3f dPdz_(dPdz[i], dPdz[i + Tex::BatchWidth],
                             dPdz[i + 2 * Tex::BatchWidth]);
            if (dresultds) {
                ok &= texture3d(texture_handle, thread_info, opt, P_, dPdx_,
                                dPdy_, dPdz_, nchannels, r, drds, drdt, drdr);
                for (int c = 0; c < nchannels; ++c) {
                    result[c * Tex::BatchWidth + i]    = r[c];
                    dresultds[c * Tex::BatchWidth + i] = drds[c];
                    dresultdt[c * Tex::BatchWidth + i] = drdt[c];
                    dresultdr[c * Tex::BatchWidth + i] = drdr[c];
                }
            } else {
                ok &= texture3d(texture_handle, thread_info, opt, P_, dPdx_,
                                dPdy_, dPdz_, nchannels, r);
                for (int c = 0; c < nchannels; ++c) {
                    result[c * Tex::BatchWidth + i] = r[c];
                }
            }
        }
    }
    return ok;
}



bool
TextureSystemImpl::texture3d(ustring filename, TextureOptBatch& options,
                             Tex::RunMask mask, const float* P,
                             const float* dPdx, const float* dPdy,
                             const float* dPdz, int nchannels, float* result,
                             float* dresultds, float* dresultdt,
                             float* dresultdr)
{
    Perthread* thread_info        = get_perthread_info();
    TextureHandle* texture_handle = get_texture_handle(filename, thread_info);
    return texture3d(texture_handle, thread_info, options, mask, P, dPdx, dPdy,
                     dPdz, nchannels, result, dresultds, dresultdt, dresultdr);
}


OIIO_NAMESPACE_END
