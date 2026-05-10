// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "texture_loader.h"

#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace texture_device {

namespace {

    bool loader_error(const std::string& msg)
    {
        std::cerr << "texture-device: " << msg << "\n";
        return false;
    }

}  // namespace

void
TextureLoader::add_texture_path(std::string path)
{
    if (!path.empty())
        m_texture_paths.emplace_back(std::move(path));
}

bool
TextureLoader::resolve_texture(uint64_t texture_hash,
                               OIIO::ustringhash& texture_name,
                               std::string& filename) const
{
    texture_name         = OIIO::ustringhash::from_hash(texture_hash);
    const char* basename = texture_name.c_str();
    if (!basename || !basename[0])
        return false;

    for (const std::string& path : m_texture_paths) {
        const std::filesystem::path fullpath = std::filesystem::path(path)
                                               / basename;
        if (!std::filesystem::exists(fullpath))
            continue;
        filename = fullpath.string();
        return true;
    }

    return false;
}

bool
TextureLoader::query_texture_info(const std::string& filename, int& width,
                                  int& height)
{
    // Cache dimensions/validation for repeated requests to the same file.
    auto cache_it = m_metadata_cache.find(filename);
    if (cache_it != m_metadata_cache.end()) {
        width  = cache_it->second.width;
        height = cache_it->second.height;
        return true;
    }

    auto in = OIIO::ImageInput::open(filename);
    if (!in)
        return false;

    const OIIO::ImageSpec& spec = in->spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
        in->close();
        return false;
    }
    if (spec.nchannels != 3 && spec.nchannels != 4) {
        in->close();
        return loader_error("unsupported channel count for " + filename
                            + " got " + std::to_string(spec.nchannels)
                            + " expected 3 or 4");
    }

    if (spec.tile_width != int(TileRecord::kTileWidth)
        || spec.tile_height != int(TileRecord::kTileHeight)) {
        in->close();
        return loader_error("unsupported tile size for " + filename + " got "
                            + std::to_string(spec.tile_width) + "x"
                            + std::to_string(spec.tile_height) + " expected "
                            + std::to_string(TileRecord::kTileWidth) + "x"
                            + std::to_string(TileRecord::kTileHeight));
    }

    width  = spec.width;
    height = spec.height;
    in->close();
    m_metadata_cache[filename] = TextureMetadata { width, height };
    return true;
}

bool
TextureLoader::load_tile_payload(const std::string& filename, TileCoords tile,
                                 std::vector<RGBA>& tile_pixels)
{
    int width  = 0;
    int height = 0;
    if (!query_texture_info(filename, width, height))
        return false;

    auto in = OIIO::ImageInput::open(filename);
    if (!in)
        return false;

    constexpr int tile_w = int(TileRecord::kTileWidth);
    constexpr int tile_h = int(TileRecord::kTileHeight);
    const int mip        = std::max(0u, unsigned(tile.mip));
    if (!in->seek_subimage(0, mip)) {
        in->close();
        return loader_error("failed to seek mip " + std::to_string(mip)
                            + " for " + filename);
    }
    const OIIO::ImageSpec mipspec = in->spec();

    if (mipspec.tile_width != tile_w || mipspec.tile_height != tile_h) {
        in->close();
        return loader_error(
            "unsupported mip tile size for " + filename + " mip "
            + std::to_string(mip) + " got " + std::to_string(mipspec.tile_width)
            + "x" + std::to_string(mipspec.tile_height) + " expected "
            + std::to_string(tile_w) + "x" + std::to_string(tile_h));
    }

    const int tile_x    = mipspec.x + tile.x * tile_w;
    const int tile_y    = mipspec.y + tile.y * tile_h;
    const int nchannels = std::max(1, mipspec.nchannels);
    if (nchannels != 3 && nchannels != 4) {
        in->close();
        return loader_error("unsupported channel count for " + filename
                            + " mip " + std::to_string(mip) + " got "
                            + std::to_string(nchannels) + " expected 3 or 4");
    }

    std::vector<float> raw(size_t(tile_w) * size_t(tile_h) * size_t(nchannels),
                           0.0f);
    // Read one tile in float and normalize payload to RGBA for device lookup.
    const bool ok = in->read_tile(tile_x, tile_y, 0, OIIO::TypeDesc::FLOAT,
                                  raw.data());
    in->close();
    if (!ok)
        return false;

    tile_pixels.assign(size_t(tile_w * tile_h), RGBA(0.0f, 0.0f, 0.0f, 0.0f));
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            const size_t p = size_t(y) * size_t(tile_w) + size_t(x);
            const size_t o = p * size_t(nchannels);
            const float r  = raw[o + 0];
            const float g  = raw[o + 1];
            const float b  = raw[o + 2];
            const float a  = (nchannels == 4) ? raw[o + 3] : 1.0f;
            tile_pixels[p] = RGBA(r, g, b, a);
        }
    }
    return true;
}

}  // namespace texture_device
