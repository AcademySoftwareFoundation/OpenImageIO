/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
#include <list>

#include <OpenEXR/ImathMatrix.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/varyingref.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imagecache.h>
#include "imagecache_pvt.h"
#include "texture_pvt.h"


/*
Discussion about environment map issues and conventions:

Latlong maps (spherical parameterization) come in two varieties
that OIIO supports:

(a) Our default follows the RenderMan convention of "z is up" and
left-handed, with the north pole (t=0) pointing toward +z and the
"center" (0.5,0.5) pointing toward +y:

          --s-->         (0,0,1)
  (0,0) +---------------------------------------+ (1,0)
        |                                       |
     |  |                                       |
     t  |(-1,0,0)        (1,0,0)                |
     |  +         +         +         +         |
     V  |      (0,-1,0)            (0,1,0)      |
        |                                       |
        |                                       |
  (0,1) +---------------------------------------+ (1,1)
                         (0,0,-1)

(b) If the metadata "oiio:updirection" is "y", the map is assumed to use
the OpenEXR convention where the +y axis is "up", and the coordinate
system is right-handed, and the center pixel points toward the +x axis:

          --s-->         (0,1,0)
  (0,0) +---------------------------------------+ (1,0)
        |                                       |
     |  |                                       |
     t  |(0,0,-1)        (0,0,1)                |
     |  +         +         +         +         |
     V  |      (1,0,0)            (0,-1,0)      |
        |                                       |
        |                                       |
  (0,1) +---------------------------------------+ (1,1)
                         (0,-1,0)



By default, we assume the conversion between pixel coordinates and
texture coordinates is the same as for 2D textures; that is, pixel (i,j)
is located at s,t = ( (i+0.5)/xres, (j+0.5)/yres ).  We believe that
this is the usual interpretation of latlong maps.

However, if the metadata "oiio:sampleborder" is present and nonzero,
we assume instead that pixel (i,j) has st coordinates ( i/(xres-1),
j/(yres-1) ), in other words, that the edge texels correspond exactly to
the pole or median seam, and therefore that pixel (0,j) and (xres-1,j)
should be identical for all j, pixel (i,0) should be identical for all
i, and pixel (i,yres-1) should be identical for all i.  This latter
convention is dictated by OpenEXR.



Cubeface environment maps are composed of six orthogonal faces (that
we name px, nx, py, ny, pz, nz).  

               major   +s dir   +t dir     
       Face    axis    (right)  (down)
       ----    -----   -------  ------
        px      +x       -z       -y
        nx      -x       +z       -y
        py      +y       +x       +z
        ny      -y       +x       -z
        pz      +z       +x       -y
        nz      -z       -x       -y

The cubeface layout is easily visualized by "unwrapping":

                    +-------------+
                    |py           |
                    |             |
                    |     +y->+x  |
                    |      |      |
                    |      V      |
                    |     +z      |
      +-------------|-------------|-------------+-------------+
      |nx           |pz           |px           |nz           |
      |             |             |             |             |
      |     -x->+z  |     +z->+x  |     +x->-z  |     -z->-x  |
      |      |      |      |      |      |      |      |      |
      |      V      |      V      |      V      |      V      |
      |     -y      |     -y      |     -y      |     -y      |
      +-------------+-------------+-------------+-------------+
                    |ny           |
                    |             |
                    |    -y->+x   |
                    |     |       |
                    |     V       |
                    |    -z       |
                    +-------------+

But that's just for visualization.  The way it's actually stored in a
file varies by convention of the file format.  Here are the two
conventions that we support natively:

(a) "2x3" (a.k.a. the RenderMan/BMRT convention) wherein all six faces
are arranged within a single image:

      +-------------+-------------+-------------+
      |px           |py           |pz           |
      |             |             |             |
      |    +x->-z   |    +y->+x   |    +z->+x   |
      |     |       |     |       |     |       |
      |     V       |     V       |     V       |
      |    -y       |    +z       |    -y       |
      |-------------|-------------|-------------|
      |nx           |ny           |nz           |
      |             |             |             |
      |    -x->+z   |    -y->+x   |    -z->-x   |
      |     |       |     |       |     |       |
      |     V       |     V       |     V       |
      |    -y       |    -z       |    -y       |
      +-------------+-------------+-------------+

The space for each "face" is an integer multiple of the tile size,
padded by black pixels if necessary (i.e. if the face res is not a
full multiple of the tile size).  For example,

      +--------+--------+--------+
      |px|     |py|     |pz|     |
      |--+     |--+     |--+     |
      | (black)|        |        |
      |--------+--------+--------|
      |nx|     |ny|     |nz|     |
      |--+     |--+     |--+     |
      |        |        |        |
      +--------+--------+--------+

This might happen on low-res MIP levels if the tile size is 64x64 but
each face is only 8x8, say.

The way we signal these things is for the ImageSpec width,height to be
the true data window size (3 x res, 2 x res), but for 
full_width,full_height to be the size of the valid area of each face.

(b) "6x1" (the OpenEXR convention) wherein the six faces are arranged
in a vertical stack like this:

      +--+
      |px|
      +--+
      |nx|
      +--+
      |py|
      +--+
      |ny|
      +--+
      |pz|
      +--+
      |nz|
      +--+

Which of these conventions is being followed in a particular cubeface
environment map file should be obvious merely by looking at the aspect
ratio -- 3:2 or 1:6.

As with latlong maps, by default we assume the conversion between pixel
coordinates and texture coordinates within a face is the same as for 2D
textures; that is, pixel (i,j) is located at s,t = ( (i+0.5)/faceres,
(j+0.5)/faceres ).

However, if the metadata "oiio:sampleborder" is present and nonzero, we
assume instead that pixel (i,j) has st coordinates ( i/(faceres-1),
j/(faceres-1) ), in other words, that the edge texels correspond exactly
to the cube edge itself, and therefore that each cube face's edge texels
are identical to the bordering face, and that any corner pixel values
are identical for all three faces that share the corner.  This
convention is dictated by OpenEXR.

*/


OIIO_NAMESPACE_BEGIN
    using namespace pvt;
    using namespace simd;

namespace {  // anonymous

static EightBitConverter<float> uchar2float;

}  // end anonymous namespace

namespace pvt {   // namespace pvt



bool
TextureSystemImpl::environment (ustring filename, TextureOptions &options,
                                Runflag *runflags,
                                int beginactive, int endactive,
                                VaryingRef<Imath::V3f> R,
                                VaryingRef<Imath::V3f> dRdx,
                                VaryingRef<Imath::V3f> dRdy,
                                int nchannels, float *result,
                                float *dresultds, float *dresultdt)
{
    Perthread *thread_info = get_perthread_info();
    TextureHandle *texture_handle = get_texture_handle (filename, thread_info);
    return environment (texture_handle, thread_info, options,
                        runflags, beginactive, endactive,
                        R, dRdx, dRdy, nchannels, result, dresultds, dresultdt);
}



bool
TextureSystemImpl::environment (TextureHandle *texture_handle,
                                Perthread *thread_info, TextureOptions &options,
                                Runflag *runflags,
                                int beginactive, int endactive,
                                VaryingRef<Imath::V3f> R,
                                VaryingRef<Imath::V3f> dRdx,
                                VaryingRef<Imath::V3f> dRdy,
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
            ok &= environment (texture_handle, thread_info,
                               opt, R[i], dRdx[i], dRdy[i],
                               nchannels, result, dresultds, dresultdt);
        }
        result += nchannels;
        if (dresultds) {
            dresultds += nchannels;
            dresultdt += nchannels;
        }
    }
    return ok;
}



/// Convert a direction vector to latlong st coordinates
///
inline void
vector_to_latlong (const Imath::V3f& R, bool y_is_up, float &s, float &t)
{
    if (y_is_up) {
        s = atan2f (-R[0], R[2]) / (2.0f*(float)M_PI) + 0.5f;
        t = 0.5f - atan2f(R[1], hypotf(R[2],-R[0])) / (float)M_PI;
    } else {
        s = atan2f (R[1], R[0]) / (2.0f*(float)M_PI) + 0.5f;
        t = 0.5f - atan2f(R[2], hypotf(R[0],R[1])) / (float)M_PI;
    }
    // learned from experience, beware NaNs
    if (isnan(s))
	s = 0.0f;
    if (isnan(t))
	t = 0.0f;
}



bool
TextureSystemImpl::environment (ustring filename, TextureOpt &options,
                                const Imath::V3f &R,
                                const Imath::V3f &dRdx, const Imath::V3f &dRdy,
                                int nchannels, float *result,
                                float *dresultds, float *dresultdt)
{
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    TextureFile *texturefile = find_texturefile (filename, thread_info);
    return environment ((TextureHandle *)texturefile, (Perthread *)thread_info,
                        options, R, dRdx, dRdy,
                        nchannels, result, dresultds, dresultdt);
}



bool
TextureSystemImpl::environment (TextureHandle *texture_handle_,
                                Perthread *thread_info_,
                                TextureOpt &options, const Imath::V3f &_R,
                                const Imath::V3f &_dRdx, const Imath::V3f &_dRdy,
                                int nchannels, float *result,
                                float *dresultds, float *dresultdt)
{
    // Handle >4 channel lookups by recursion.
    if (nchannels > 4) {
        int save_firstchannel = options.firstchannel;
        while (nchannels) {
            int n = std::min (nchannels, 4);
            bool ok = environment (texture_handle_, thread_info_, options,
                                   _R, _dRdx, _dRdy,
                                   n, result, dresultds, dresultdt);
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

    PerThreadInfo *thread_info = m_imagecache->get_perthread_info((PerThreadInfo *)thread_info_);
    TextureFile *texturefile = verify_texturefile ((TextureFile *)texture_handle_, thread_info);
    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.environment_batches;
    ++stats.environment_queries;

    if (! texturefile  ||  texturefile->broken())
        return missing_texture (options, nchannels, result,
                                dresultds, dresultdt);

    const ImageSpec &spec (texturefile->spec(options.subimage, 0));

    // Environment maps dictate particular wrap modes
    options.swrap = texturefile->m_sample_border ?
        TextureOpt::WrapPeriodicSharedBorder : TextureOpt::WrapPeriodic;
    options.twrap = TextureOpt::WrapClamp;

    options.envlayout = LayoutLatLong;
    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel,
                                       0, nchannels);

    // Initialize results to 0.  We'll add from here on as we sample.
    for (int c = 0;  c < nchannels;  ++c)
        result[c] = 0;
    if (dresultds) {
        for (int c = 0;  c < nchannels;  ++c)
            dresultds[c] = 0;
        for (int c = 0;  c < nchannels;  ++c)
            dresultdt[c] = 0;
    }
    // If the user only provided us with one pointer, clear both to simplify
    // the rest of the code, but only after we zero out the data for them so
    // they know something went wrong.
    if (!(dresultds && dresultdt))
        dresultds = dresultdt = NULL;

    // Calculate unit-length vectors in the direction of R, R+dRdx, R+dRdy.
    // These define the ellipse we're filtering over.
    Imath::V3f R  = _R;  R.normalize();       // center
    Imath::V3f Rx = _R + _dRdx;  Rx.normalize();  // x axis of the ellipse
    Imath::V3f Ry = _R + _dRdy;  Ry.normalize();  // y axis of the ellipse
    // angles formed by the ellipse axes.
    float xfilt_noblur = std::max (safe_acos(R.dot(Rx)), 1e-8f);
    float yfilt_noblur = std::max (safe_acos(R.dot(Ry)), 1e-8f);
    int naturalres = int((float)M_PI / std::min (xfilt_noblur, yfilt_noblur));
    // FIXME -- figure naturalres sepearately for s and t
    // FIXME -- ick, why is it x and y at all, shouldn't it be s and t?
    // N.B. naturalres formulated for latlong

    // Account for width and blur
    float xfilt = xfilt_noblur * options.swidth + options.sblur;
    float yfilt = yfilt_noblur * options.twidth + options.tblur;

    // Figure out major versus minor, and aspect ratio
    Imath::V3f Rmajor;   // major axis
    float majorlength, minorlength;
    bool x_is_majoraxis = (xfilt >= yfilt);
    if (x_is_majoraxis) {
        Rmajor = Rx;
        majorlength = xfilt;
        minorlength = yfilt;
    } else {
        Rmajor = Ry;
        majorlength = yfilt;
        minorlength = xfilt;
    }

    sampler_prototype sampler;
    long long *probecount;
    switch (options.interpmode) {
    case TextureOpt::InterpClosest :
        sampler = &TextureSystemImpl::sample_closest;
        probecount = &stats.closest_interps;
        break;
    case TextureOpt::InterpBilinear :
        sampler = &TextureSystemImpl::sample_bilinear;
        probecount = &stats.bilinear_interps;
        break;
    case TextureOpt::InterpBicubic :
        sampler = &TextureSystemImpl::sample_bicubic;
        probecount = &stats.cubic_interps;
        break;
    default:
        sampler = &TextureSystemImpl::sample_bilinear;
        probecount = &stats.bilinear_interps;
        break;
    }

    TextureOpt::MipMode mipmode = options.mipmode;
    bool aniso = (mipmode == TextureOpt::MipModeDefault ||
                  mipmode == TextureOpt::MipModeAniso);

    float aspect, trueaspect, filtwidth;
    int nsamples;
    float invsamples;
    if (aniso) {
        aspect = anisotropic_aspect (majorlength, minorlength, options, trueaspect);
        filtwidth = minorlength;
        if (trueaspect > stats.max_aniso)
            stats.max_aniso = trueaspect;
        nsamples = std::max (1, (int) ceilf (aspect - 0.25f));
        invsamples = 1.0f / nsamples;
    } else {
        filtwidth = options.conservative_filter ? majorlength : minorlength;
        nsamples = 1;
        invsamples = 1.0f;
    }

    ImageCacheFile::SubimageInfo &subinfo (texturefile->subimageinfo(options.subimage));

    // FIXME -- assuming latlong
    bool ok = true;
    float pos = -0.5f + 0.5f * invsamples;
    for (int sample = 0;  sample < nsamples;  ++sample, pos += invsamples) {
        Imath::V3f Rsamp = R + pos*Rmajor;
        float s, t;
        vector_to_latlong (Rsamp, texturefile->m_y_up, s, t);

        // Determine the MIP-map level(s) we need: we will blend
        //  data(miplevel[0]) * (1-levelblend) + data(miplevel[1]) * levelblend
        int miplevel[2] = { -1, -1 };
        float levelblend = 0;

        int nmiplevels = (int)subinfo.levels.size();
        for (int m = 0;  m < nmiplevels;  ++m) {
            // Compute the filter size in raster space at this MIP level.
            // Filters are in radians, and the vertical resolution of a
            // latlong map is PI radians.  So to compute the raster size of
            // our filter width...
            float filtwidth_ras = subinfo.spec(m).full_height * filtwidth * M_1_PI;
            // Once the filter width is smaller than one texel at this level,
            // we've gone too far, so we know that we want to interpolate the
            // previous level and the current level.  Note that filtwidth_ras
            // is expected to be >= 0.5, or would have stopped one level ago.
            if (filtwidth_ras <= 1) {
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
        }
        if (options.mipmode == TextureOpt::MipModeOneLevel) {
            // Force use of just one mipmap level
            miplevel[1] = miplevel[0];
            levelblend = 0;
        } else if (mipmode == TextureOpt::MipModeNoMIP) {
            // Just sample from lowest level
            miplevel[0] = 0;
            miplevel[1] = 0;
            levelblend = 0;
        }

        float levelweight[2] = { 1.0f - levelblend, levelblend };

        int npointson = 0;
        for (int level = 0;  level < 2;  ++level) {
            if (! levelweight[level])
                continue;
            ++npointson;
            int lev = miplevel[level];
            if (options.interpmode == TextureOpt::InterpSmartBicubic) {
                if (lev == 0 ||
                    (texturefile->spec(options.subimage,lev).full_height < naturalres/2)) {
                    sampler = &TextureSystemImpl::sample_bicubic;
                    ++stats.cubic_interps;
                } else {
                    sampler = &TextureSystemImpl::sample_bilinear;
                    ++stats.bilinear_interps;
                }
            } else {
                *probecount += 1;
            }

            OIIO_SIMD4_ALIGN float sval[4] = { s, 0.0f, 0.0f, 0.0f };
            OIIO_SIMD4_ALIGN float tval[4] = { t, 0.0f, 0.0f, 0.0f };
            OIIO_SIMD4_ALIGN float weight[4] = { levelweight[level]*invsamples,
                                                 0.0f, 0.0f, 0.0f };
            vfloat4 r, drds, drdt;
            ok &= (this->*sampler) (1, sval, tval, miplevel[level],
                                    *texturefile, thread_info, options,
                                    nchannels, actualchannels, weight,
                                    &r, dresultds ? &drds : NULL, dresultds ? &drdt : NULL);
            for (int c = 0; c < nchannels; ++c)
                result[c] += r[c];
            if (dresultds) {
                for (int c = 0; c < nchannels; ++c) {
                    dresultds[c] += drds[c];
                    dresultdt[c] += drdt[c];
                }
            }
        }
    }
    stats.aniso_probes += nsamples;
    ++stats.aniso_queries;

    if (actualchannels < nchannels && options.firstchannel == 0 && m_gray_to_rgb)
        fill_gray_channels (spec, nchannels, result, dresultds, dresultdt);

    return ok;
}



bool
TextureSystemImpl::environment (TextureHandle *texture_handle, Perthread *thread_info,
                                TextureOptBatch &options, Tex::RunMask mask,
                                const float *R, const float *dRdx, const float *dRdy,
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

    bool ok = true;
    Tex::RunMask bit = 1;
    for (int i = 0; i < Tex::BatchWidth;  ++i, bit <<= 1) {
        float r[4], drds[4], drdt[4];  // temp result
        if (mask & bit) {
            opt.sblur = options.sblur[i];
            opt.tblur = options.tblur[i];
            opt.swidth = options.swidth[i];
            opt.twidth = options.twidth[i];
            Imath::V3f R_ (R[i], R[i+Tex::BatchWidth], R[i+2*Tex::BatchWidth]);
            Imath::V3f dRdx_ (dRdx[i], dRdx[i+Tex::BatchWidth], dRdx[i+2*Tex::BatchWidth]);
            Imath::V3f dRdy_ (dRdy[i], dRdy[i+Tex::BatchWidth], dRdy[i+2*Tex::BatchWidth]);
            if (dresultds) {
                ok &= environment (texture_handle, thread_info, opt,
                                   R_, dRdx_, dRdy_, nchannels, r, drds, drdt);
                for (int c = 0; c < nchannels; ++c) {
                    result[c*Tex::BatchWidth+i] = r[c];
                    dresultds[c*Tex::BatchWidth+i] = drds[c];
                    dresultdt[c*Tex::BatchWidth+i] = drdt[c];
                }
            } else {
                ok &= environment (texture_handle, thread_info, opt,
                                   R_, dRdx_, dRdy_, nchannels, r);
                for (int c = 0; c < nchannels; ++c) {
                    result[c*Tex::BatchWidth+i] = r[c];
                }
            }
        }
    }
    return ok;
}



bool
TextureSystemImpl::environment (ustring filename,
                                TextureOptBatch &options, Tex::RunMask mask,
                                const float *R, const float *dRdx, const float *dRdy,
                                int nchannels, float *result,
                                float *dresultds, float *dresultdt)
{
    Perthread *thread_info = get_perthread_info();
    TextureHandle *texture_handle = get_texture_handle (filename, thread_info);
    return environment (texture_handle, thread_info, options, mask,
                        R, dRdx, dRdy, nchannels, result, dresultds, dresultdt);
}


}  // end namespace pvt

OIIO_NAMESPACE_END
