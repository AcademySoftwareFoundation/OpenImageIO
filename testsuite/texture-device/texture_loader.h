// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "texture_device_decl.h"

#include <OpenImageIO/ustring.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace texture_device {

class TextureLoader {
public:
    using RGBA = texture_device::RGBA;

    void add_texture_path(std::string path);

    bool resolve_texture(uint64_t texture_hash, OIIO::ustringhash& texture_name,
                         std::string& filename) const;

    bool query_texture_info(const std::string& filename, int& width,
                            int& height);

    bool load_tile_payload(const std::string& filename, TileCoords tile,
                           std::vector<RGBA>& tile_pixels);

    template<typename TextureSystem>
    bool process_request(const Request& req, TextureSystem* ts)
    {
        const uint64_t key = req.tile.texture_hash;
        OIIO::ustringhash texture_name;
        std::string filename;
        if (!resolve_texture(key, texture_name, filename))
            return false;

        switch (req.type) {
        case RequestType::MissingTexture: {
            int tex_w = 0;
            int tex_h = 0;
            if (!query_texture_info(filename, tex_w, tex_h))
                return false;
            return ts->set_texture_ready(texture_name, uint32_t(tex_w),
                                         uint32_t(tex_h));
        }
        case RequestType::MissingTile: {
            std::vector<RGBA> tile_pixels;
            if (!load_tile_payload(filename, req.tile, tile_pixels))
                return false;
            return ts->set_tile_payload(texture_name, req.tile,
                                        int(TextureSystem::kTileWidth),
                                        int(TextureSystem::kTileHeight),
                                        tile_pixels);
        }
        default: return false;
        }
    }

private:
    struct TextureMetadata {
        int width  = 0;
        int height = 0;
    };

    std::vector<std::string> m_texture_paths;
    std::unordered_map<std::string, TextureMetadata> m_metadata_cache;
};

}  // namespace texture_device
