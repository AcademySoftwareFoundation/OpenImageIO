// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/ustring.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <vector>

#include "arena.h"
#include "closed_hashmap.h"
#include "filtering_decl.h"
#include "stream.h"

namespace texture_device {

#define OPT_FUNCT(condition, return_type) \
    template<typename __Q = return_type> std::enable_if_t<condition, __Q>

#define OPT_CONSTRUCT(condition) \
    template<bool __C = condition, typename = std::enable_if_t<__C>>

#define OPT_FIELD(condition, field_type) \
    std::conditional_t<condition, field_type, std::monostate>

#define OPT_FUNCT_DECL(condition) \
    template<bool __C = condition, typename = std::enable_if_t<__C>>

using RGBA = Imath::C4f;
using Vec2 = Imath::V2f;

struct TileCoordsHash {
    size_t operator()(const TileCoords& c) const;
};

enum class RequestType : uint8_t { MissingTexture, MissingTile };

struct Request {
    RequestType type = RequestType::MissingTexture;
    TileCoords tile;

    bool operator==(const Request& other) const;
};

struct RequestHash {
    size_t operator()(const Request& req) const;
    static uint64_t hash_mix_u64(uint64_t h, uint64_t v);
};

enum class WrapMode : uint8_t { Clamp, Periodic, Black };

int
wrap_coord(int coord, int size, WrapMode mode, bool& in_range);

struct TileRecord {
    static constexpr uint32_t kTileWidth  = 64;
    static constexpr uint32_t kTileHeight = 64;
    TileCoords tile;
    std::array<RGBA, kTileWidth * kTileHeight> pixels {};
};

struct TextureRecord {
    OIIO::ustringhash name;
    bool ready      = false;
    uint32_t width  = 0;
    uint32_t height = 0;
    WrapMode swrap  = WrapMode::Clamp;
    WrapMode twrap  = WrapMode::Clamp;

    void reset(OIIO::ustringhash texture_name)
    {
        name   = texture_name;
        ready  = false;
        width  = 0;
        height = 0;
        swrap  = WrapMode::Periodic;
        twrap  = WrapMode::Periodic;
    }
};

template<typename Arena, typename ManagedArena> struct DTextureSystemTestAccess;

template<typename Arena, typename ManagedArena = NullArena>
class DTextureSystem {
public:
    template<typename, typename> friend class DTextureSystem;
    template<typename, typename> friend struct DTextureSystemTestAccess;

    template<typename T> using Atomic = typename Arena::template Atomic<T>;
    using Managed                     = DTextureSystem<ManagedArena>;
    static constexpr bool IsManager
        = !std::is_same<ManagedArena, NullArena>::value;

    static constexpr uint32_t kMaxResidentTextures = 32;
    static constexpr uint32_t kMaxResidentTiles    = 2048;
    static constexpr uint32_t kMaxTilesPerTexture  = 256;
    static constexpr uint32_t kMaxRequests         = 1024;
    static constexpr uint32_t kTileWidth           = TileRecord::kTileWidth;
    static constexpr uint32_t kTileHeight          = TileRecord::kTileHeight;

    using RequestQueue
        = ClosedHashMap<Request, bool, RequestHash, Arena, ManagedArena>;
    using TextureMap = ClosedHashMap<uint64_t, uint32_t, std::hash<uint64_t>,
                                     Arena, ManagedArena>;

    // Managed only functionality

    OPT_CONSTRUCT(!IsManager)
    DTextureSystem()
        : m_failed(false)
        , m_texture_count(0)
        , m_tile_count(0)
    {
    }

    OPT_CONSTRUCT(!IsManager)
    DTextureSystem(Arena& arena)
        : m_queue(arena, kMaxRequests)
        , m_failed(false)
        , m_texture_count(0)
        , m_texture_lookup(arena, kMaxResidentTextures)
        , m_tile_pool(arena)
        , m_tile_index(arena, kMaxResidentTiles)
        , m_tile_count(0)
    {
        std::fill(std::begin(m_textures), std::end(m_textures),
                  TextureRecord {});
    }

    OPT_CONSTRUCT(!IsManager)
    DTextureSystem(const DTextureSystem& o)
    {
        m_queue  = o.m_queue;
        m_failed = o.m_failed.load();
        memcpy(m_textures, o.m_textures, sizeof(m_textures));
        m_texture_count  = o.m_texture_count;
        m_texture_lookup = o.m_texture_lookup;
        m_tile_pool      = o.m_tile_pool;
        m_tile_index     = o.m_tile_index;
        m_tile_count     = o.m_tile_count;
    }


    OPT_FUNCT(!IsManager, DTextureSystem&)
    operator=(const DTextureSystem & o)
    {
        m_queue  = o.m_queue;
        m_failed = o.m_failed.load();
        memcpy(m_textures, o.m_textures, sizeof(m_textures));
        m_texture_count  = o.m_texture_count;
        m_texture_lookup = o.m_texture_lookup;
        m_tile_pool      = o.m_tile_pool;
        m_tile_index     = o.m_tile_index;
        m_tile_count     = o.m_tile_count;
        return *this;
    }

    // Manager only functionality

    OPT_CONSTRUCT(IsManager)
    DTextureSystem(Arena& arena, ManagedArena& marena, Managed& managed)
        : m_managed(managed)
        , m_queue(arena, marena, kMaxRequests, m_managed.m_queue)
        , m_failed(false)
        , m_texture_count(0)
        , m_texture_lookup(arena, marena, kMaxResidentTextures,
                           m_managed.m_texture_lookup)
        , m_tile_pool(arena, marena, m_managed.m_tile_pool)
        , m_tile_index(arena, marena, kMaxResidentTiles, m_managed.m_tile_index)
        , m_tile_count(0)
    {
        std::fill(std::begin(m_textures), std::end(m_textures),
                  TextureRecord {});
    }

    OPT_FUNCT(IsManager, void) begin_launch() { m_failed.store(false); }
    OPT_FUNCT_DECL(IsManager)
    bool set_texture_ready(OIIO::ustringhash name, uint32_t width,
                           uint32_t height);
    OPT_FUNCT_DECL(IsManager)
    bool set_tile_payload(OIIO::ustringhash name, TileCoords tile, int width,
                          int height, const std::vector<RGBA>& pixels);
    OPT_FUNCT(IsManager, bool)
    needs_retry()
    {
        if (!m_queue.overflowed())
            return false;
        if (!m_queue.grow())
            return false;

        // Overflow indicates dropped requests; clear and re-run to recollect.
        m_queue.clear();
        m_failed.store(false);
        return true;
    }
    OPT_FUNCT_DECL(IsManager)
    bool find_or_add_texture(OIIO::ustringhash name, uint32_t& index);
    OPT_FUNCT_DECL(IsManager)
    void sync_to_managed();
    OPT_FUNCT_DECL(IsManager)
    void sync_from_managed();

    template<typename Func, bool __C = IsManager,
             typename = std::enable_if_t<__C>>
    bool process_requests(Func&& fn)
    {
        RequestQueue& queue = request_queue();
        for (const Request& req : queue) {
            if (!fn(req, this))
                return false;
        }
        queue.clear();
        return true;
    }

    // Both managed and manager

    RequestQueue& request_queue() { return m_queue; }
    const RequestQueue& request_queue() const { return m_queue; }

    bool failures() const
    {
        const RequestQueue& queue = request_queue();
        return m_failed.load() || queue.failed();
    }
    RGBA lookup(OIIO::ustringhash name, float u, float v, Vec2 du, Vec2 dv,
                float rnd = -1);

    using TilePool     = Stream<TileRecord, Arena, ManagedArena>;
    using TileIndexMap = ClosedHashMap<TileCoords, uint32_t, TileCoordsHash,
                                       Arena, ManagedArena>;

private:
    bool find_texture(OIIO::ustringhash name, uint32_t& index) const;

    static void copy_tile_pixels(TileRecord& dst,
                                 const std::vector<RGBA>& pixels);

    template<class SampleArray> bool load_tiles(SampleArray& samples);

    // Manager only
    OPT_FIELD(IsManager, Managed&) m_managed;
    // Both
    RequestQueue m_queue;
    Atomic<bool> m_failed;
    TextureRecord m_textures[kMaxResidentTextures];
    uint32_t m_texture_count;
    TextureMap m_texture_lookup;
    TilePool m_tile_pool;
    TileIndexMap m_tile_index;
    uint32_t m_tile_count;
};

bool
run_device_unit_tests();

}  // namespace texture_device

#undef OPT_FIELD
#undef OPT_CONSTRUCT
#undef OPT_FUNCT_DECL
#undef OPT_FUNCT
