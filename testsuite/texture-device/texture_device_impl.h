// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "texture_device_decl.h"

#include <OpenImageIO/dassert.h>

#include <algorithm>
#include <cstring>

// Just to reduce verbosity
#define DTS DTextureSystem<Arena, ManagedArena>
#define OPT_FUNCT_IMPL template<bool __C, typename>

namespace texture_device {

inline size_t
TileCoordsHash::operator()(const TileCoords& c) const
{
    uint64_t h
        = OIIO::farmhash::inlined::Hash64(reinterpret_cast<const char*>(&c),
                                          sizeof(TileCoords));
    return static_cast<size_t>(h);
}

inline bool
Request::operator==(const Request& other) const
{
    return type == other.type && tile == other.tile;
}

inline size_t
RequestHash::operator()(const Request& req) const
{
    uint64_t h = 1469598103934665603ull;
    h          = hash_mix_u64(h, static_cast<uint64_t>(req.type));
    h          = hash_mix_u64(h, req.tile.texture_hash);
    h          = hash_mix_u64(h,
                              static_cast<uint64_t>(static_cast<uint32_t>(req.tile.x)));
    h          = hash_mix_u64(h,
                              static_cast<uint64_t>(static_cast<uint32_t>(req.tile.y)));
    h          = hash_mix_u64(h, static_cast<uint64_t>(
                            static_cast<uint32_t>(req.tile.mip)));
    return static_cast<size_t>(h);
}

inline uint64_t
RequestHash::hash_mix_u64(uint64_t h, uint64_t v)
{
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

template<typename Arena, typename ManagedArena>
OPT_FUNCT_IMPL inline bool
DTS::set_texture_ready(OIIO::ustringhash name, uint32_t width, uint32_t height)
{
    uint32_t index = 0;
    if (!find_or_add_texture(name, index))
        return false;
    if (width == 0 || height == 0)
        return false;
    m_textures[index].width  = width;
    m_textures[index].height = height;
    m_textures[index].swrap  = WrapMode::Periodic;
    m_textures[index].twrap  = WrapMode::Periodic;
    m_textures[index].ready  = true;
    return true;
}

template<typename Arena, typename ManagedArena>
OPT_FUNCT_IMPL inline bool
DTS::set_tile_payload(OIIO::ustringhash name, TileCoords tile, int width,
                      int height, const std::vector<RGBA>& pixels)
{
    if (width != int(kTileWidth) || height != int(kTileHeight))
        return false;
    if (pixels.size() != size_t(width) * size_t(height))
        return false;

    uint32_t index = 0;
    if (!find_texture(name, index))
        return false;
    TextureRecord& texture = m_textures[index];
    if (!texture.ready)
        return false;

    uint32_t tile_pool_index = 0;
    if (m_tile_index.find(tile, tile_pool_index)) {
        if (tile_pool_index >= m_tile_count)
            return false;
        return true;
    }

    if (m_tile_count >= kMaxResidentTiles)
        return false;

    if (m_tile_index.size() >= kMaxTilesPerTexture)
        return false;

    tile_pool_index = static_cast<uint16_t>(m_tile_count);
    if (!m_tile_index.insert(tile, tile_pool_index))
        return false;

    TileRecord record;
    record.tile = tile;
    copy_tile_pixels(record, pixels);

    m_tile_pool.push_back(record);
    ++m_tile_count;

    return true;
}

template<typename Arena, typename ManagedArena>
OPT_FUNCT_IMPL inline bool
DTextureSystem<Arena, ManagedArena>::find_or_add_texture(OIIO::ustringhash name,
                                                         uint32_t& index)
{
    if (find_texture(name, index))
        return true;

    if (m_texture_count >= kMaxResidentTextures)
        return false;

    const uint32_t new_index = m_texture_count++;
    m_textures[new_index].reset(name);
    if (!m_texture_lookup.insert(name.hash(), new_index))
        return false;
    index = new_index;
    return true;
}

template<typename Arena, typename ManagedArena>
OPT_FUNCT_IMPL inline void
DTextureSystem<Arena, ManagedArena>::sync_to_managed()
{
    m_queue.sync_to_managed();
    m_texture_lookup.sync_to_managed();
    m_tile_pool.sync_to_managed();
    m_tile_index.sync_to_managed();
    m_managed.m_failed = m_failed.load();
    memcpy(m_managed.m_textures, m_textures, sizeof(m_textures));
    m_managed.m_texture_count = m_texture_count;
    m_managed.m_tile_count    = m_tile_count;
}

template<typename Arena, typename ManagedArena>
OPT_FUNCT_IMPL inline void
DTextureSystem<Arena, ManagedArena>::sync_from_managed()
{
    m_queue.sync_from_managed();
    m_texture_lookup.sync_from_managed();
    m_tile_pool.sync_from_managed();
    m_tile_index.sync_from_managed();
    m_failed = m_managed.m_failed.load();
    memcpy(m_textures, m_managed.m_textures, sizeof(m_textures));
    m_texture_count = m_managed.m_texture_count;
    m_tile_count    = m_managed.m_tile_count;
}

template<typename Arena, typename ManagedArena>
inline bool
DTS::find_texture(OIIO::ustringhash name, uint32_t& index) const
{
    uint32_t out = 0;
    if (!m_texture_lookup.find(name.hash(), out))
        return false;
    if (out >= m_texture_count)
        return false;
    if (m_textures[out].name != name)
        return false;
    index = out;
    return true;
}

template<typename Arena, typename ManagedArena>
inline void
DTS::copy_tile_pixels(TileRecord& dst, const std::vector<RGBA>& pixels)
{
    std::copy(pixels.begin(), pixels.end(), dst.pixels.begin());
}

inline int
wrap_coord(int coord, int size, WrapMode mode, bool& in_range)
{
    in_range = true;
    if (size <= 0) {
        in_range = false;
        return 0;
    }
    if (mode == WrapMode::Clamp)
        return std::clamp(coord, 0, size - 1);
    if (mode == WrapMode::Periodic) {
        const int m = coord % size;
        return (m < 0) ? (m + size) : m;
    }
    if (coord < 0 || coord >= size)
        in_range = false;
    return coord;
}

template<typename Arena, typename ManagedArena>
template<class SampleArray>
inline bool
DTS::load_tiles(SampleArray& samples)
{
    bool missing_any    = false;
    RequestQueue& queue = request_queue();
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].tile || samples[i].weight == 0)
            continue;

        uint32_t tile_pool_index = 0;
        const bool found         = m_tile_index.find(samples[i].tcoords,
                                                     tile_pool_index)
                           && tile_pool_index < m_tile_count;

        // Apply the same tile-resolution result to all duplicate tap entries
        // with identical tile coordinates in this sample batch.
        for (size_t j = i; j < samples.size(); ++j) {
            if (!(samples[j].tcoords == samples[i].tcoords))
                continue;
            if (found) {
                samples[j].tile = &m_tile_pool[tile_pool_index];
            } else {
                samples[j].weight = 0;
            }
        }

        if (!found) {
            missing_any = true;
            // RequestQueue deduplicates repeated misses across pixels/taps.
            if (!queue.insert(Request { RequestType::MissingTile,
                                        samples[i].tcoords },
                              true)) {
                m_failed.store(true);
            }
        }
    }
    return !missing_any;
}

template<typename Arena, typename ManagedArena>
inline RGBA
DTS::lookup(OIIO::ustringhash name, float u, float v, Vec2 du, Vec2 dv,
            float rnd)
{
    uint32_t texture_index = 0;
    if (find_texture(name, texture_index)) {
        const TextureRecord& texture = m_textures[texture_index];
        if (texture.ready) {
            RGBA accum(0.0f, 0.0f, 0.0f, 0.0f);
            bool failure = false;
            MipAnisoFilter filter(texture, du, dv);
            if (rnd >= 0.0f && filter.num_mips > 1) {
                // Experimental path: stochastic mip choice collapses trilinear
                // blending to one selected mip for this lookup.
                const float blend = filter.mips.mip_blend;
                OIIO_CONTRACT_ASSERT(0 <= rnd && rnd < 1);
                unsigned selected = rnd < blend ? 0 : 1;
                rnd = selected ? (rnd - blend) / (1 - blend) : rnd / blend;
                filter.mips.mip_levels[0] = filter.mips.mip_levels[selected];
                filter.num_mips           = 1;  // This will make the weight 1.0
            }
            for (unsigned mip_i = 0; mip_i < filter.num_mips; ++mip_i) {
                const float mip_weight
                    = filter.num_mips == 1
                          ? 1
                          : (mip_i ? filter.mips.mip_blend
                                   : 1 - filter.mips.mip_blend);
                auto samples = filter.generate_samples(mip_i, u, v);

                if (!load_tiles(samples)) {
                    failure = true;
                    continue;
                }

                if (rnd >= 0) {
                    // Experimental path: select one tap by cumulative
                    // distribution instead of deterministic weighted sum.
                    float total_weight = 0;
                    for (size_t i = 0; i < samples.size(); ++i)
                        total_weight += samples[i].weight;
                    float sum = 0;
                    for (size_t i = 0; i < samples.size(); ++i) {
                        sum += samples[i].weight;
                        if (rnd < sum / total_weight
                            || i == samples.size() - 1) {
                            const size_t idx = size_t(samples[i].local_y)
                                                   * size_t(kTileWidth)
                                               + size_t(samples[i].local_x);
                            accum = samples[i].tile->pixels[idx];
                            break;
                        }
                    }
                } else {
                    for (size_t i = 0; i < samples.size(); ++i) {
                        if (!samples[i].tile)
                            continue;
                        const size_t idx = size_t(samples[i].local_y)
                                               * size_t(kTileWidth)
                                           + size_t(samples[i].local_x);
                        accum += mip_weight * samples[i].weight
                                 * samples[i].tile->pixels[idx];
                    }
                }
            }

            return failure ? RGBA(1.0f, 0.0f, 1.0f, 1.0f) : accum;
        }
    }

    Request req;
    req.type = (find_texture(name, texture_index)
                && m_textures[texture_index].ready)
                   ? RequestType::MissingTile
                   : RequestType::MissingTexture;
    req.tile = TileCoords { name.hash() };

    RequestQueue& queue = request_queue();
    if (!queue.insert(req, true))
        m_failed.store(true);

    return RGBA(1.0f, 0.0f, 1.0f, 1.0f);
}

#undef DTS
#undef OPT_FUNCT_IMPL

}  // namespace texture_device
