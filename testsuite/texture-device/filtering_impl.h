// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "filtering_decl.h"
#include "texture_device_decl.h"
#include <cmath>

namespace texture_device {

inline MipAnisoFilter::MipAnisoFilter(const TextureRecord& texture, Vec2 du,
                                      Vec2 dv)
    : texture(texture)
{
    axes     = ellipse_axes(texture, du, dv);
    mips     = compute_miplevels(texture, axes);
    num_mips = mips.mip_levels[1] == mips.mip_levels[0] ? 1 : 2;
}

inline int
MipAnisoFilter::floor_div(int n, int d)
{
    int q = n / d;
    int r = n % d;
    if (r != 0 && ((r < 0) != (d < 0)))
        --q;
    return q;
}

inline vector_lite<Filter::Sample, MipAnisoFilter::kMaxSamples * 4>
MipAnisoFilter::generate_samples(unsigned mip_i, float u, float v) const
{
    EllipseSampling e = compute_ellipse_sampling(texture, axes,
                                                 mips.mip_levels[mip_i]);


    // Precompute normalized per-sample gaussian weights along the major axis.
    vector_lite<float, kMaxSamples> gaussian;
    float gaussian_weight_sum = 0;
    for (unsigned s = 0; s < e.nsamples; ++s) {
        const float t = (s + 0.5f) / e.nsamples - 0.5f;
        const float w = e.nsamples > 1 ? std::exp(-2.0f * t * t) : 1;
        gaussian.push_back(w);
        gaussian_weight_sum += w;
    }
    vector_lite<Sample, kMaxSamples * 4> samples;

    const uint32_t mip     = mips.mip_levels[mip_i];
    const uint32_t base_w  = std::max(1u, texture.width);
    const uint32_t base_h  = std::max(1u, texture.height);
    const uint32_t level_w = std::max(1u, base_w >> mip);
    const uint32_t level_h = std::max(1u, base_h >> mip);

    if (gaussian_weight_sum <= 0.0f)
        return samples;

    unsigned s = 0;
    for (const float gaussian_sample : gaussian) {
        const float t = (e.nsamples > 1)
                            ? ((float(s) + 0.5f) / float(e.nsamples) - 0.5f)
                            : 0.0f;
        const Vec2 uv = Vec2(u, v) + (2.0f * t * e.span_uv) * e.axis_uv;
        const float gaussian_w = gaussian_sample / gaussian_weight_sum;

        const float fu = uv.x * float(level_w);
        const float fv = uv.y * float(level_h);
        const int x0   = int(std::floor(fu));
        const int y0   = int(std::floor(fv));
        const int x1   = x0 + 1;
        const int y1   = y0 + 1;
        const float tx = fu - float(x0);
        const float ty = fv - float(y0);

        const int tap_x[4]        = { x0, x1, x0, x1 };
        const int tap_y[4]        = { y0, y0, y1, y1 };
        const float bilinear_w[4] = { (1.0f - tx) * (1.0f - ty),
                                      tx * (1.0f - ty), (1.0f - tx) * ty,
                                      tx * ty };

        // Expand each anisotropic sample position into 4 bilinear taps.
        for (int tap = 0; tap < 4; ++tap) {
            const float w = gaussian_w * bilinear_w[tap];
            if (w <= 0.0f)
                continue;

            bool x_ok           = true;
            bool y_ok           = true;
            const int wrapped_x = wrap_coord(tap_x[tap], int(level_w),
                                             texture.swrap, x_ok);
            const int wrapped_y = wrap_coord(tap_y[tap], int(level_h),
                                             texture.twrap, y_ok);
            if (!x_ok || !y_ok)
                continue;

            const uint16_t tile_x = floor_div(wrapped_x,
                                              int(TileRecord::kTileWidth));
            const uint16_t tile_y = floor_div(wrapped_y,
                                              int(TileRecord::kTileHeight));
            const int local_x     = wrapped_x
                                - tile_x * int(TileRecord::kTileWidth);
            const int local_y = wrapped_y
                                - tile_y * int(TileRecord::kTileHeight);

            if (samples.size() >= samples.capacity())
                continue;

            samples.push_back({
                w,
                TileCoords { texture.name.hash(), tile_x, tile_y, uint16_t(mip),
                             0 },
                local_x,
                local_y,
                nullptr,
            });
        }
        ++s;
    }
    return samples;
}

inline MipAnisoFilter::EllipseSampling
MipAnisoFilter::compute_ellipse_sampling(const TextureRecord& texture,
                                         const EllipseAxes& axes, int mip)
{
    (void)texture;
    const float mip_scale   = 1.0f / float(1u << uint32_t(std::max(0, mip)));
    const float aspect      = anisotropic_aspect(axes.major_rho * mip_scale,
                                                 axes.minor_rho * mip_scale,
                                                 MipAnisoFilter::kMaxSamples);
    const unsigned nsamples = std::clamp(unsigned(std::ceil(aspect)), 1u,
                                         MipAnisoFilter::kMaxSamples);

    const float span_uv = 0.5f * axes.major_uv.length();
    return EllipseSampling { axes.major_uv, nsamples, span_uv };
}

inline MipAnisoFilter::EllipseAxes
MipAnisoFilter::ellipse_axes(const TextureRecord& texture, Vec2 du, Vec2 dv)
{
    const float tex_w = float(std::max(1u, texture.width));
    const float tex_h = float(std::max(1u, texture.height));
    const float rho_u = std::sqrt((du.x * tex_w) * (du.x * tex_w)
                                  + (du.y * tex_h) * (du.y * tex_h));
    const float rho_v = std::sqrt((dv.x * tex_w) * (dv.x * tex_w)
                                  + (dv.y * tex_h) * (dv.y * tex_h));

    const bool major_is_u = rho_u >= rho_v;
    const Vec2 major_uv   = major_is_u ? du : dv;
    const float major_rho = std::max(rho_u, rho_v);
    const float minor_rho = std::max(1.0f, std::min(rho_u, rho_v));

    const float major_uv_len = major_uv.length();
    Vec2 axis_uv             = Vec2(1.0f, 0.0f);
    if (major_uv_len > 0.0f)
        axis_uv = major_uv / major_uv_len;

    return EllipseAxes { axis_uv, major_rho, minor_rho };
}

inline MipAnisoFilter::MipSelection
MipAnisoFilter::compute_miplevels(const TextureRecord& texture,
                                  const EllipseAxes& axes)
{
    (void)texture;
    const float rho = std::max(1.0f, axes.minor_rho);

    const float lod     = std::max(0.0f, std::log2(rho));
    const uint32_t mip0 = std::clamp(uint32_t(std::floor(lod)), 0u,
                                     kMaxMipLevel);
    const uint32_t mip1 = std::min(kMaxMipLevel, mip0 + 1);
    const float blend   = std::clamp(lod - float(mip0), 0.0f, 1.0f);
    return MipSelection { { mip0, mip1 }, blend };
}


}  // namespace texture_device