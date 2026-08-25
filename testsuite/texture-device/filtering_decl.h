// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/Imath.h>

#include <cstdint>

#include "vector_lite.h"

namespace texture_device {

using Vec2 = Imath::V2f;

struct TileCoords {
    uint64_t texture_hash = 0;
    uint16_t x            = 0;
    uint16_t y            = 0;
    uint16_t mip          = 0;
    uint16_t pad          = 0;  // zero padded for the hash to be deterministic

    bool operator==(const TileCoords& other) const
    {
        return texture_hash == other.texture_hash && x == other.x
               && y == other.y && mip == other.mip;
    }
};

struct TileRecord;
struct TextureRecord;
struct FilteringUnitTestsAccess;

struct Filter {
    struct Sample {
        float weight;
        TileCoords tcoords;
        int local_x;
        int local_y;
        const TileRecord* tile;
    };
};

struct MipAnisoFilter : public Filter {
    static constexpr uint32_t kMaxSamples  = 8;
    static constexpr uint32_t kMaxMipLevel = 4;

    struct MipSelection {
        unsigned mip_levels[2] = { 0, 0 };
        float mip_blend        = 0.0f;
    };

    struct EllipseSampling {
        Vec2 axis_uv      = Vec2(1.0f, 0.0f);
        unsigned nsamples = 1;
        float span_uv     = 0.0f;
    };

    struct EllipseAxes {
        Vec2 major_uv   = Vec2(1.0f, 0.0f);
        float major_rho = 1.0f;
        float minor_rho = 1.0f;
    };

    const TextureRecord& texture;
    EllipseAxes axes;
    MipSelection mips;
    unsigned num_mips;

    MipAnisoFilter(const TextureRecord& texture, Vec2 du, Vec2 dv);
    vector_lite<Sample, kMaxSamples * 4>
    generate_samples(unsigned mip_i, float u, float v) const;

private:
    friend struct FilteringUnitTestsAccess;

    static EllipseAxes ellipse_axes(const TextureRecord& texture, Vec2 du,
                                    Vec2 dv);
    static MipSelection compute_miplevels(const TextureRecord& texture,
                                          const EllipseAxes& axes);
    static EllipseSampling
    compute_ellipse_sampling(const TextureRecord& texture,
                             const EllipseAxes& axes, int mip);


    static int floor_div(int n, int d);
    static float anisotropic_aspect(float major_rho, float minor_rho,
                                    int max_aniso)
    {
        const float safe_minor = std::max(1.0f, minor_rho);
        return std::clamp(major_rho / safe_minor, 1.0f, float(max_aniso));
    }
};

struct FilteringUnitTestsAccess {
    static MipAnisoFilter::EllipseAxes
    ellipse_axes(const TextureRecord& texture, Vec2 du, Vec2 dv)
    {
        return MipAnisoFilter::ellipse_axes(texture, du, dv);
    }

    static MipAnisoFilter::MipSelection
    compute_miplevels(const TextureRecord& texture,
                      const MipAnisoFilter::EllipseAxes& axes)
    {
        return MipAnisoFilter::compute_miplevels(texture, axes);
    }

    static MipAnisoFilter::EllipseSampling
    compute_ellipse_sampling(const TextureRecord& texture,
                             const MipAnisoFilter::EllipseAxes& axes, int mip)
    {
        return MipAnisoFilter::compute_ellipse_sampling(texture, axes, mip);
    }
};

}  // namespace texture_device