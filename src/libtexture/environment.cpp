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

OIIO_NAMESPACE_ENTER
{
    using namespace pvt;

namespace {  // anonymous

static EightBitConverter<float> uchar2float;

};  // end anonymous namespace

namespace pvt {   // namespace pvt



bool
TextureSystemImpl::environment (ustring filename, TextureOptions &options,
                                Runflag *runflags,
                                int beginactive, int endactive,
                                VaryingRef<Imath::V3f> R,
                                VaryingRef<Imath::V3f> dRdx,
                                VaryingRef<Imath::V3f> dRdy,
                                float *result)
{
    bool ok = true;
    for (int i = beginactive;  i < endactive;  ++i) {
        if (runflags[i]) {
            TextureOpt opt (options, i);
            ok &= environment (filename, opt, R[i], dRdx[i], dRdy[i], result);
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
                                const Imath::V3f &_R,
                                const Imath::V3f &_dRdx, const Imath::V3f &_dRdy,
                                float *result)
{
    // Per-thread microcache that prevents locking of this mutex
    PerThreadInfo *thread_info = m_imagecache->get_perthread_info ();
    ImageCacheStatistics &stats (thread_info->m_stats);
    ++stats.environment_batches;
    TextureFile *texturefile = thread_info->find_file (filename);
    if (! texturefile) {
        // Fall back on the master cache
        texturefile = find_texturefile (filename, thread_info);
        thread_info->filename (filename, texturefile);
    }

    if (! texturefile  ||  texturefile->broken()) {
        for (int c = 0;  c < options.nchannels;  ++c) {
            if (options.missingcolor)
                result[c] = options.missingcolor[c];
            else
                result[c] = options.fill;
            if (options.dresultds) options.dresultds[c] = 0;
            if (options.dresultdt) options.dresultdt[c] = 0;
        }
//        error ("Texture file \"%s\" not found", filename.c_str());
        ++stats.environment_queries;
        if (options.missingcolor) {
            (void) geterror ();   // eat the error
            return true;
        } else {
            return false;
        }
    }

    const ImageSpec &spec (texturefile->spec(options.subimage, 0));

    options.swrap_func = wrap_periodic;
    options.twrap_func = wrap_clamp;
    options.envlayout = LayoutLatLong;
    int actualchannels = Imath::clamp (spec.nchannels - options.firstchannel,
                                       0, options.nchannels);
    options.actualchannels = actualchannels;

    // Fill channels requested but not in the file
    if (options.actualchannels < options.nchannels) {
        for (int c = options.actualchannels; c < options.nchannels; ++c) {
            result[c] = options.fill;
            if (options.dresultds) options.dresultds[c] = 0;
            if (options.dresultdt) options.dresultdt[c] = 0;
        }
    }
    // Early out if all channels were beyond the highest in the file
    if (options.actualchannels < 1) {
        return true;
    }

    ++stats.environment_queries;

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

    // Calculate unit-length vectors in the direction of R, R+dRdx, R+dRdy.
    // These define the ellipse we're filtering over.
    Imath::V3f R  = _R;  R.normalize();       // center
    Imath::V3f Rx = _R + _dRdx;  Rx.normalize();  // x axis of the ellipse
    Imath::V3f Ry = _R + _dRdy;  Ry.normalize();  // y axis of the ellipse
    // angles formed by the ellipse axes.
    float xfilt_noblur = std::max (safe_acosf(R.dot(Rx)), 1e-8f);
    float yfilt_noblur = std::max (safe_acosf(R.dot(Ry)), 1e-8f);
    int naturalres = int((float)M_PI / std::min (xfilt_noblur, yfilt_noblur));
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

    accum_prototype accumer;
    long long *probecount;
    switch (options.interpmode) {
    case TextureOpt::InterpClosest :
        accumer = &TextureSystemImpl::accum_sample_closest;
        probecount = &stats.closest_interps;
        break;
    case TextureOpt::InterpBilinear :
        accumer = &TextureSystemImpl::accum_sample_bilinear;
        probecount = &stats.bilinear_interps;
        break;
    case TextureOpt::InterpBicubic :
        accumer = &TextureSystemImpl::accum_sample_bicubic;
        probecount = &stats.cubic_interps;
        break;
    default:
        accumer = NULL;
        probecount = NULL;
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
                    accumer = &TextureSystemImpl::accum_sample_bicubic;
                    ++stats.cubic_interps;
                } else {
                    accumer = &TextureSystemImpl::accum_sample_bilinear;
                    ++stats.bilinear_interps;
                }
            } else {
                *probecount += 1;
            }

            ok &= (this->*accumer) (s, t, miplevel[level], *texturefile,
                                    thread_info, options,
                                    levelweight[level]*invsamples, result,
                                    dresultds, dresultdt);
        }
    }
    stats.aniso_probes += nsamples;
    ++stats.aniso_queries;

    return ok;
}



};  // end namespace pvt

}
OIIO_NAMESPACE_EXIT
