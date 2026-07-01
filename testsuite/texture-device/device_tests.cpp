// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "filtering_impl.h"  // IWYU pragma: keep
#include "texture_device_decl.h"
#include "texture_device_impl.h"  // IWYU pragma: keep

#include <array>

namespace texture_device {

template<typename Arena, typename ManagedArena>
struct DTextureSystemTestAccess {
    using System = DTextureSystem<Arena, ManagedArena>;

    static auto ellipse_axes(System& system, const TextureRecord& texture,
                             Vec2 du, Vec2 dv)
    {
        (void)system;
        return FilteringUnitTestsAccess::ellipse_axes(texture, du, dv);
    }

    static auto compute_miplevels(System& system, const TextureRecord& texture,
                                  const MipAnisoFilter::EllipseAxes& axes)
    {
        (void)system;
        return FilteringUnitTestsAccess::compute_miplevels(texture, axes);
    }

    static auto
    compute_ellipse_sampling(System& system, const TextureRecord& texture,
                             const MipAnisoFilter::EllipseAxes& axes, int mip)
    {
        (void)system;
        return FilteringUnitTestsAccess::compute_ellipse_sampling(texture, axes,
                                                                  mip);
    }
};

namespace {

    struct ConstantHash {
        size_t operator()(uint64_t) const { return 1; }
    };

}  // namespace

using UnitRequestQueue = DTextureSystem<Host>::RequestQueue;
bool
run_device_unit_tests()
{
    constexpr uint64_t kGridHash    = 0x3f6b4e91u;
    constexpr uint64_t kCheckerHash = 0x13a56d2bu;
    constexpr uint64_t kBulkHash    = 0x67d9aa11u;

    Host host;

    using Map = ClosedHashMap<uint64_t, int, std::hash<uint64_t>, Host>;
    using CollisionMap = ClosedHashMap<uint64_t, int, ConstantHash, Host>;

    Map map(host, 16);
    map.clear();

    CollisionMap collision_map(host, 8);
    collision_map.clear();

    // Basic closed-hash insert/find path.
    if (!map.insert(10, 100))
        return false;
    if (!map.insert(26, 260))
        return false;

    int value = 0;
    if (!map.find(10, value) || value != 100)
        return false;
    if (!map.find(26, value) || value != 260)
        return false;
    // Missing key must not report a hit.
    if (map.find(42, value))
        return false;

    // Force collisions into one probe chain and validate retrieval order
    // independence.
    if (!collision_map.insert(1, 11) || !collision_map.insert(9, 99)
        || !collision_map.insert(17, 171))
        return false;
    if (!collision_map.find(17, value) || value != 171)
        return false;
    if (!collision_map.find(9, value) || value != 99)
        return false;

    UnitRequestQueue queue(host, DTextureSystem<Host>::kMaxRequests);
    queue.clear();

    Request req_a;
    req_a.type              = RequestType::MissingTexture;
    req_a.tile.texture_hash = kGridHash;

    Request req_b;
    req_b.type              = RequestType::MissingTexture;
    req_b.tile.texture_hash = kCheckerHash;

    // Request queue is deduplicating: repeated inserts of the same request
    // should succeed but not grow queue size.
    if (!queue.insert(req_a, true))
        return false;
    if (!queue.insert(req_a, true))
        return false;
    if (!queue.insert(req_b, true))
        return false;

    std::array<Request, DTextureSystem<Host>::kMaxRequests> bulk;
    for (uint32_t i = 0; i < DTextureSystem<Host>::kMaxRequests; ++i) {
        bulk[i].type = RequestType::MissingTile;
        bulk[i].tile = TileCoords { kBulkHash, uint16_t(i), uint16_t(i / 64),
                                    uint16_t(i % 4), 0 };
    }

    UnitRequestQueue full_queue(host, DTextureSystem<Host>::kMaxRequests);
    full_queue.clear();
    // Fill to capacity to exercise overflow signaling behavior.
    for (uint32_t i = 0; i < DTextureSystem<Host>::kMaxRequests; ++i) {
        if (!full_queue.insert(bulk[i], true))
            return false;
    }
    // Existing key still succeeds when table is full.
    if (full_queue.insert(bulk[0], true) != true)
        return false;
    // New key must fail once no slot remains.
    if (full_queue.insert(Request { RequestType::MissingTile,
                                    TileCoords { kBulkHash, 12345, 0, 0, 0 } },
                          true))
        return false;
    if (!full_queue.overflowed())
        return false;

    {
        DTextureSystem<Host> system(host);
        using Access = DTextureSystemTestAccess<Host, NullArena>;
        TextureRecord tex;
        tex.width  = 1024;
        tex.height = 1024;

        // Mip transition: small derivatives should stay near base level.
        auto axes_lo = Access::ellipse_axes(system, tex,
                                            Vec2 { 1.0f / 1024.0f, 0.0f },
                                            Vec2 { 0.0f, 1.0f / 1024.0f });
        auto mips_lo = Access::compute_miplevels(system, tex, axes_lo);
        if (mips_lo.mip_levels[0] != 0)
            return false;

        // Mip transition: larger derivatives should move to coarser levels.
        auto axes_hi = Access::ellipse_axes(system, tex,
                                            Vec2 { 8.0f / 1024.0f, 0.0f },
                                            Vec2 { 0.0f, 8.0f / 1024.0f });
        auto mips_hi = Access::compute_miplevels(system, tex, axes_hi);
        if (mips_hi.mip_levels[0] <= mips_lo.mip_levels[0])
            return false;

        // Anisotropy extremes should increase sample count, capped by max.
        auto axes_aniso = Access::ellipse_axes(system, tex,
                                               Vec2 { 8.0f / 1024.0f, 0.0f },
                                               Vec2 { 0.0f, 1.0f / 1024.0f });
        auto ellipse = Access::compute_ellipse_sampling(system, tex, axes_aniso,
                                                        0);
        if (ellipse.nsamples < 2
            || ellipse.nsamples > MipAnisoFilter::kMaxSamples)
            return false;

        // Wrap behavior regression checks.
        bool in_range = true;
        if (wrap_coord(-1, 8, WrapMode::Clamp, in_range) != 0 || !in_range)
            return false;
        if (wrap_coord(-1, 8, WrapMode::Periodic, in_range) != 7 || !in_range)
            return false;
        (void)wrap_coord(-1, 8, WrapMode::Black, in_range);
        if (in_range)
            return false;
    }

    // Dedup result for req_a + req_b should keep queue size at 2.
    return queue.size() == 2;
}

}  // namespace texture_device
